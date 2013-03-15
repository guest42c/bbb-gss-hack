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

#define GSS_TEXT_HTML "text/html;charset=utf-8"
#define GSS_TEXT_PLAIN "text/plain;charset=utf-8"

void gss_html_header (GssTransaction *t);
void gss_html_footer (GssTransaction *t);
void gss_html_header_bare (GssTransaction *t);
void gss_html_footer_bare (GssTransaction *t);
void gss_html_error_404 (GssServer *server, SoupMessage *msg);
void gss_html_append_image (GString *s, const char *url, int width, int height,
    const char *alt_text);
void gss_html_append_image_printf (GString *s, const char *url,
    int width, int height, const char *alt_text, ...);
void gss_html_bootstrap_doc (GssTransaction *t);
char * gss_html_sanitize_attribute (const char *s);
char * gss_html_sanitize_entity (const char *s);
char * gss_html_sanitize_url (const char *s);
gboolean gss_html_entity_is_sane (const char *s);
gboolean gss_html_attribute_is_sane (const char *s);
gboolean gss_html_url_is_sane (const char *s);
void gss_html_append_button (GString * s, const char *button_name,
    const char *key, const char *value);
void gss_html_append_button_target (GString * s, const char *button_name,
    const char *key, const char *value, const char *target);
void gss_html_append_button2 (GString * s, const char *button_name,
    const char *key0, const char *value0, const char *key1, const char *value1);
void gss_html_append_button3 (GString * s, const char *button_name,
    const char *key0, const char *value0,
    const char *key1, const char *value1, const char *key2, const char *value2);

#define GSS_A(a) g_string_append (s, a)
#define GSS_P(...) g_string_append_printf (s, __VA_ARGS__)

G_END_DECLS

#endif

