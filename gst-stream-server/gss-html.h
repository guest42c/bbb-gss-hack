
#ifndef _GSS_HTML_H
#define _GSS_HTML_H

#include <glib.h>
#include <libsoup/soup.h>

void gss_html_header (GString *s, const char *title);
void gss_html_footer (GString *s, const char *token);
void gss_html_error_404 (SoupMessage *msg);

#endif

