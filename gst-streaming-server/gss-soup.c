/* GStreamer Streaming Server
 * Copyright (C) 2009-2012 Entropy Wave Inc <info@entropywave.com>
 * Copyright (C) 2009-2012 David Schleef <ds@schleef.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */


#include "config.h"

#include "gss-html.h"
#include "gss-soup.h"

#include <string.h>



char *
gss_soup_get_request_host (SoupMessage * msg)
{
  char *host;
  char *colon;
  const char *s;

  s = soup_message_headers_get_one (msg->request_headers, "Host");
  if (s == NULL)
    return NULL;
  host = strdup (s);
  colon = strchr (host, ':');
  if (colon)
    colon[0] = 0;

  return host;
}

char *
gss_soup_get_base_url_http (GssServer * server, SoupMessage * msg)
{
  char *base_url;
  char *host;

  host = gss_soup_get_request_host (msg);
  if (host == NULL)
    host = g_strdup (server->server_hostname);

  if (server->http_port == 80) {
    base_url = g_strdup_printf ("http://%s", host);
  } else {
    base_url = g_strdup_printf ("http://%s:%d", host, server->http_port);
  }
  g_free (host);

  return base_url;
}

char *
gss_soup_get_base_url_https (GssServer * server, SoupMessage * msg)
{
  char *base_url;
  char *host;

  host = gss_soup_get_request_host (msg);
  if (host == NULL)
    host = g_strdup (server->server_hostname);

  if (server->https_port == 443) {
    base_url = g_strdup_printf ("https://%s", host);
  } else {
    base_url = g_strdup_printf ("https://%s:%d", host, server->https_port);
  }
  g_free (host);

  return base_url;
}

char *
gss_transaction_get_base_url (GssTransaction * t)
{
  if (t->soupserver == t->server->server) {
    return gss_soup_get_base_url_http (t->server, t->msg);
  } else {
    return gss_soup_get_base_url_https (t->server, t->msg);
  }
}

gboolean
gss_transaction_is_secure (GssTransaction * t)
{
  return (t->soupserver == t->server->ssl_server);
}
