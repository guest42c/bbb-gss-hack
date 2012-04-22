
#ifndef _GSS_HTML_H
#define _GSS_HTML_H

#include <glib.h>
#include "gss-server.h"
#include <libsoup/soup.h>

//#define USE_XHTML
#define USE_HTML5

void gss_html_header (GssServer *server, GString *s, const char *title);
void gss_html_footer (GssServer *server, GString *s, const char *token);
void gss_html_error_404 (SoupMessage *msg);
void gss_html_append_break (GString *s);
void gss_html_append_image (GString *s, const char *url, int width, int height,
    const char *alt_text);
void gss_html_append_image_printf (GString *s, const char *url,
    int width, int height, const char *alt_text, ...);

#endif

