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


#ifndef _GSS_HTML_H
#define _GSS_HTML_H

#include <glib.h>
#include "gss-server.h"
#include "gss-session.h"
#include <libsoup/soup.h>

G_BEGIN_DECLS

//#define USE_XHTML
#define USE_HTML5

void gss_html_header (GssTransaction *t);
void gss_html_footer (GssTransaction *t);
void gss_html_error_404 (GssServer *server, SoupMessage *msg);
void gss_html_append_break (GString *s);
void gss_html_append_image (GString *s, const char *url, int width, int height,
    const char *alt_text);
void gss_html_append_image_printf (GString *s, const char *url,
    int width, int height, const char *alt_text, ...);
void gss_html_bootstrap_doc (GssTransaction *t);

G_END_DECLS

#endif

