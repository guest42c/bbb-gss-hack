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
#include "gss-transaction.h"

#include <string.h>

void
gss_transaction_redirect (GssTransaction * t, const char *target)
{
  char *s;

  s = g_strdup_printf
      ("<html><head><meta http-equiv='refresh' content='0'></head>\n"
      "<body>Oops, you were supposed to "
      "be redirected <a href='%s'>here</a>.</body></html>\n", target);
  soup_message_set_response (t->msg, GSS_TEXT_HTML, SOUP_MEMORY_TAKE, s,
      strlen (s));
  soup_message_headers_append (t->msg->response_headers, "Location", target);
  soup_message_set_status (t->msg, SOUP_STATUS_SEE_OTHER);
}

void
gss_transaction_error (GssTransaction * t, const char *message)
{
  GString *s = g_string_new ("");

  t->s = s;

  gss_html_header (t);

  g_string_append (s, "<h1>Configuration Failed</h1><hr>\n");
  g_string_append (s, "<p>Invalid configuration options were provided.\n"
      "Please return to previous page and retry.</p>\n");

  gss_html_footer (t);
  soup_message_set_status (t->msg, SOUP_STATUS_BAD_REQUEST);
}
