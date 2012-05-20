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


#ifndef _GSS_SOUP_H
#define _GSS_SOUP_H

#include <glib.h>
#include <libsoup/soup.h>

G_BEGIN_DECLS

char *gss_soup_get_request_host (SoupMessage *message);
char * gss_soup_get_base_url_http (GssServer *server, SoupMessage *msg);
char * gss_soup_get_base_url_https (GssServer * server, SoupMessage * msg);
GHashTable * gss_soup_parse_json (const char *s, int len);


G_END_DECLS

#endif

