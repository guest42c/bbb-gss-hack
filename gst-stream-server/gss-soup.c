
#include "config.h"

#include "gss-html.h"

#include <string.h>



char *
gss_soup_get_request_host (SoupMessage *msg)
{
  char *host;
  char *colon;

  host = strdup (soup_message_headers_get (msg->request_headers, "Host"));
  colon = strchr (host, ':');
  if (colon) colon[0] = 0;

  return host;
}

char *
gss_soup_get_base_url_http (GssServer *server, SoupMessage *msg)
{
  char *base_url;
  char *host;

  host = gss_soup_get_request_host (msg);

  if (server->port == 80) {
    base_url = g_strdup_printf("http://%s", host);
  } else {
    base_url = g_strdup_printf("http://%s:%d", host, server->port);
  }
  g_free (host);

  return base_url;
}

