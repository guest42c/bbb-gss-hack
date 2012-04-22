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

