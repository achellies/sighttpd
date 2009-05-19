
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "params.h"
#include "shell.h"

#define UIOMUX_HEADER \
  "<html>\n" \
  "<head>\n" \
  "<title>Sighttpd/%s - UIOMux</title>\n" \
  "</head>\n" \
  "<body>\n" \
  "<h1>UIOMux status</h1>\n" \
  "<pre>"

#define UIOMUX_FOOTER \
  "</pre>\n" \
  "</body>\n" \
  "</head>\n"

params_t * uiomux_append_headers(params_t * response_headers)
{
	char length[16];

	response_headers =
	    params_append(response_headers, "Content-Type", "text/html");

	return response_headers;
}

int uiomux_stream_body(int fd)
{
	char buf[4096];
	int n, total = 0;
	int status;

	n = snprintf(buf, 4096, UIOMUX_HEADER, VERSION, VERSION);
	if (n < 0)
		return -1;
	if (n > 4096)
		n = 4096;
	total += write(fd, buf, n);

        total += shell_stream (fd, "uiomux info");
        total += shell_stream (fd, "uiomux meminfo");

	n = snprintf(buf, 4096, UIOMUX_FOOTER);
	if (n < 0)
		return -1;
	if (n > 4096)
		n = 4096;
	total += write(fd, buf, n);

        return total;
}
