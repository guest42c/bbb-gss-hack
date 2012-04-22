
#ifndef _GSS_SOUP_H
#define _GSS_SOUP_H

#include <glib.h>
#include <libsoup/soup.h>

char *gss_soup_get_request_host (SoupMessage *message);
char * gss_soup_get_base_url_http (GssServer *server, SoupMessage *msg);

#endif

