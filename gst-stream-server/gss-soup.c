
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

