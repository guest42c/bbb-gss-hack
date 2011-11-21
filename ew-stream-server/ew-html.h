
#ifndef EW_HTML_H
#define EW_HTML_H

#include <glib.h>
#include <libsoup/soup.h>

void ew_html_header (GString *s, const char *title);
void ew_html_footer (GString *s, const char *token);
void ew_html_error_404 (SoupMessage *msg);

#endif

