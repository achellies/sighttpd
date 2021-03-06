/*
   Copyright (C) 2009 Conrad Parker
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "sighttpd.h"
#include "resource.h"
#include "params.h"
#include "http-reqline.h"
#include "http-status.h"
#include "list.h"
#include "log.h"

/* #define DEBUG */

static params_t *
response_headers_new (void)
{
        params_t * response_headers = NULL;
        char date[256];

        httpdate_snprint (date, 256, time(NULL));
        response_headers = params_append (response_headers, "Date", date);
        response_headers = params_append (response_headers, "Server", "Sighttpd/" VERSION);

        return response_headers;
}

static void
params_writefd (int fd, params_t * params)
{
        char headers_out[1024];

        params_snprint (headers_out, 1024, params, PARAMS_HEADERS);
        write (fd, headers_out, strlen(headers_out));
        fsync (fd);
}

static void
respond_get_head (struct sighttpd_child * schild, http_request * request, params_t * request_headers,
                  const char ** status_line, params_t ** response_headers)
{
        list_t * l, * resources;

	resources = schild->sighttpd->resources;
	for (l = resources; l; l = l->next) {
		struct resource * r = (struct resource *)l->data;

		if (r->check(request, r->data)) {
			r->head (request, request_headers, status_line, response_headers, r->data);
			return;
		}
	}

        *status_line = http_status_line (HTTP_STATUS_NOT_FOUND);
        *response_headers = http_status_append_headers (*response_headers, HTTP_STATUS_NOT_FOUND);
}

static void
respond_get_body (struct sighttpd_child * schild, http_request * request, params_t * request_headers)
{
        int fd = schild->accept_fd;
        list_t * l, * resources;

	resources = schild->sighttpd->resources;
	for (l = resources; l; l = l->next) {
		struct resource * r = (struct resource *)l->data;

		if (r->check(request, r->data)) {
			r->body (fd, request, request_headers, r->data);
			return;
		}
	}

        http_status_stream_body (fd, HTTP_STATUS_NOT_FOUND);
}

static void
respond_method_not_allowed (const char ** status_line, params_t ** response_headers)
{
        *status_line = http_status_line (HTTP_STATUS_METHOD_NOT_ALLOWED);
        *response_headers = params_append (*response_headers, "Allow", "GET");
        *response_headers = params_append (*response_headers, "Allow", "HEAD");
}

static void
respond (struct sighttpd_child * schild, http_request * request, params_t * request_headers)
{
        params_t * response_headers;
        const char * status_line;

        response_headers = response_headers_new ();

        switch (request->method) {
        case HTTP_METHOD_HEAD:
        case HTTP_METHOD_GET:
                respond_get_head (schild, request, request_headers, &status_line, &response_headers);
                break;
        default:
                respond_method_not_allowed (&status_line, &response_headers);
                break;
        }

        write (schild->accept_fd, status_line, strlen(status_line));
        params_writefd (schild->accept_fd, response_headers);

        log_access (request, request_headers, response_headers);

        if (request->method == HTTP_METHOD_GET) {
                respond_get_body (schild, request, request_headers);
        }

#ifdef DEBUG
        printf ("Finished serving / lost client\n");
#endif
}

void * http_response (struct sighttpd_child * schild)
{
        params_t * request_headers;
        http_request request;
        int fd;
	char s[8192];
        char * cur;
        size_t rem=8192, nread, n;
        int init=0;

        fd = schild->accept_fd;

        cur = s;

        setsockopt (fd, IPPROTO_TCP, TCP_NODELAY, NULL, 0);

	/* proc client's requests */
	while ((nread = read(fd, cur, rem)) != 0) {
                if (n == -1) {
                        perror ("read");
                        continue;
                } else if (nread < 8192) {
                        /* NUL-terminate input buffer */
                        cur[nread] = '\0';
                }
                if (!init && (n = http_request_parse (cur, rem, &request)) > 0) {
                        memmove (s, &s[n], nread-n);
                        init = 1;
#ifdef DEBUG
                        printf ("Got HTTP method %d, version %d for %s (consumed %d)\n", request.method,
                                request.version, request.path, n);
#endif
                }

                request_headers = params_new_parse (s, strlen (s), PARAMS_HEADERS);
                if (request_headers != NULL) {
                        respond (schild, &request, request_headers);
                        goto closeit;
                } else {
                        n = strlen (cur);
                        cur += n;
                        rem -= n;
                }
	}

closeit:
        sighttpd_child_destroy (schild);

	return 0;		/* terminate the thread */
}
