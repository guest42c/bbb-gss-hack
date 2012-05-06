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
#include "gss-server.h"

#include <string.h>


void
gss_html_header (GssServer * server, GString * s, const char *title)
{
  g_string_append_printf (s,
#ifdef USE_XHTML
      "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
      "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n"
#else
#ifdef USE_HTML5
      "<!DOCTYPE html>\n"
#else
      "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">\n"
#endif
      "<html>\n"
#endif
      "<head>\n"
#ifdef USE_XHTML
      "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\n"
#else
      "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">\n"
#endif
      "<title>%s</title>\n", title);

  if (server->append_style_html) {
    server->append_style_html (server, s, server->append_style_html_priv);
  }

  g_string_append (s, "</head>\n" "<body>\n" "<div id=\"container\">\n");
}

void
gss_html_footer (GssServer * server, GString * s, const char *session_id)
{
  gss_html_append_break (s);
  g_string_append (s,
      "&nbsp;&nbsp;&nbsp;&nbsp;<a href=\"/\">Available Streams</a>\n");
  if (session_id) {
    g_string_append_printf (s,
        "&nbsp;&nbsp;&nbsp;&nbsp;<a href=\"/admin/admin?session_id=%s\">Admin Interface</a>\n",
        session_id);
    g_string_append_printf (s,
        "&nbsp;&nbsp;&nbsp;&nbsp;<a href=\"/logout?session_id=%s\">Log Out</a>\n",
        session_id);
  }
  gss_html_append_break (s);

  g_string_append (s, "<div id=\"footer\">\n");
  g_string_append (s, "<a href=\"http://entropywave.com/\">\n");
  gss_html_append_image (s, "/images/template_footer.png", 812, 97, NULL);
  g_string_append (s, "</a>");
  g_string_append (s, "</div><!-- end footer div -->\n");
  g_string_append (s,
      "</div><!-- end container div -->\n" "</body>\n" "</html>\n");

}

void
gss_html_error_404 (SoupMessage * msg)
{
  char *content;
  GString *s;

  s = g_string_new ("");

  g_string_append (s,
      "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
      "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n"
      "<head>\n"
      "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">\n"
      "<title>Error 404: Page not found</title>\n"
      "</head>\n"
      "<body>\n" "Error 404: Page not found\n" "</body>\n" "</html>\n");

  content = g_string_free (s, FALSE);

  soup_message_set_response (msg, "text/html", SOUP_MEMORY_TAKE,
      content, strlen (content));

  soup_message_set_status (msg, SOUP_STATUS_NOT_FOUND);
}

void
gss_html_append_break (GString * s)
{
#ifdef USE_XHTML
  g_string_append (s, "<br />");
#else
  g_string_append (s, "<br>");
#endif
}

void
gss_html_append_image (GString * s, const char *url, int width, int height,
    const char *alt_text)
{
  if (alt_text == NULL)
    alt_text = "";

  g_string_append_printf (s, "<img src='%s' alt='%s' ", url,
      alt_text ? alt_text : "");
  if (width > 0 && height > 0) {
    g_string_append_printf (s, "width='%d' height='%d' ", width, height);
  }
#ifdef USE_HTML5
  /* border is in CSS */
#else
  g_string_append (s, "border='0' ");
#endif

#ifdef USE_XHTML
  g_string_append (s, "/>");
#else
#ifdef USE_HTML5
  g_string_append (s, "/>");
#else
  g_string_append (s, ">");
#endif
#endif
}

void
gss_html_append_image_printf (GString * s, const char *url_format, int width,
    int height, const char *alt_text, ...)
{
  va_list args;
  char *url;

  va_start (args, alt_text);
  url = g_strdup_vprintf (url_format, args);
  va_end (args);

  gss_html_append_image (s, url, width, height, alt_text);

  g_free (url);
}
