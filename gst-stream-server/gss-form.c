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

#include "gss-server.h"
#include "gss-config.h"
#include "gss-form.h"
#include "gss-html.h"

#include <glib/gstdio.h>
#include <glib-object.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <string.h>
#include <glib/gstdio.h>


void
gss_config_form_add_select (GString * s, GssField * item, const char *value)
{
  int i;

  if (item->indent)
    g_string_append (s, "&nbsp;&nbsp;&nbsp;&nbsp;\n");
  g_string_append_printf (s, "%s: <select id=\"%s\" name=\"%s\">\n",
      item->long_name, item->config_name, item->config_name);
  for (i = 0; i < 10; i++) {
    if (item->options[i].long_name) {
      gboolean selected;

      selected = (value && g_str_equal (value, item->options[i].config_name));

      g_string_append_printf (s, "<option value=\"%s\" %s>%s</option>\n",
          item->options[i].config_name,
          selected ? "selected=\"selected\"" : "", item->options[i].long_name);
    }
  }
  g_string_append_printf (s, "</select>\n");
  gss_html_append_break (s);

}

void
gss_config_form_add_text_input (GString * s, GssField * item, const char *value)
{
  if (item->indent)
    g_string_append (s, "&nbsp;&nbsp;&nbsp;&nbsp;\n");
  g_string_append_printf (s,
      "%s: <input id=\"%s\" type=\"text\" name=\"%s\" size=30 value=\"%s\">\n",
      item->long_name, item->config_name, item->config_name, value);
  gss_html_append_break (s);
}

void
gss_config_form_add_password (GString * s, GssField * item, const char *value)
{
  if (item->indent)
    g_string_append (s, "&nbsp;&nbsp;&nbsp;&nbsp;\n");
  g_string_append_printf (s,
      "%s: <input id=\"%s\" type=\"password\" name=\"%s\" size=20>\n",
      item->long_name, item->config_name, item->config_name);
  gss_html_append_break (s);
}

void
gss_config_form_add_checkbox (GString * s, GssField * item, const char *value)
{
  gboolean selected;

  selected = (value && g_str_equal (value, "on"));
  if (item->indent)
    g_string_append (s, "&nbsp;&nbsp;&nbsp;&nbsp;\n");
#if 0
  g_string_append_printf (s,
      "<script type=\"text/javascript\">\n"
      "function toggle() {\n"
      "var node = document.getElementById(\"%s\");\n"
      "var e = document.getElementById(\"%s\");\n"
      "e.disabled = node.checked;\n"
      "}\n" "</script>\n", item->config_name, "admin_token0");
#endif
  g_string_append_printf (s, "%s: <input type=\"hidden\" name=\"%s\" value=off>"
      "<input id=\"%s\" type=\"checkbox\" name=\"%s\" %s onclick=\"toggle()\">\n",
      item->long_name,
      item->config_name,
      item->config_name, item->config_name, selected ? "checked=on" : "");
  gss_html_append_break (s);
}

void
gss_config_form_add_file (GString * s, GssField * item, const char *value)
{
  if (item->indent)
    g_string_append (s, "&nbsp;&nbsp;&nbsp;&nbsp;\n");
  g_string_append_printf (s,
      "%s: <input id=\"%s\" type=\"file\" name=\"%s\" value=\"%s\">\n",
      item->long_name, item->config_name, item->config_name, value);
  gss_html_append_break (s);
}

void
gss_config_form_add_radio (GString * s, GssField * item, const char *value)
{
  int i;

  if (item->indent)
    g_string_append (s, "&nbsp;&nbsp;&nbsp;&nbsp;\n");
  g_string_append_printf (s, "%s\n", item->long_name);
  gss_html_append_break (s);
  for (i = 0; i < 10; i++) {
    if (item->options[i].long_name) {
      gboolean selected;

      selected = (value && g_str_equal (value, item->options[i].config_name));
      g_string_append_printf (s,
          "<input type=\"radio\" name=\"%s\" value=\"%s\" %s>%s\n",
          item->config_name,
          item->options[i].config_name,
          selected ? " checked=checked" : "", item->options[i].long_name);
      gss_html_append_break (s);
    }
  }
}

void
gss_config_form_add_submit (GString * s, GssField * item, const char *value)
{
  gss_html_append_break (s);
  g_string_append_printf (s,
      "<input id=\"%s\" type=\"submit\" value=\"%s\">\n",
      item->config_name, item->long_name);
  gss_html_append_break (s);
}

void
gss_config_form_add_hidden (GString * s, GssField * item, const char *value)
{
  g_string_append_printf (s,
      "<input name=\"%s\" type=\"hidden\" value=\"%s\">\n",
      item->config_name, "1");
}

void
gss_config_form_add_form (GssServer * server, GString * s, const char *action,
    const char *name, GssField * fields, GssSession * session)
{
  int i;
  const char *enctype;
  gboolean in_fieldset = FALSE;

  enctype = "multipart/form-data";
  //enctype = "application/x-www-form-urlencoded";

  g_string_append (s, "<iframe name=\"hidden_frame\" src=\"about:blank\" "
      "style=\"display:none; width:0px; height:0px\"></iframe>\n");
  if (session == NULL) {
    g_string_append_printf (s,
        "<form action=\"%s\" method=\"post\" enctype=\"%s\" >\n",
        action, enctype);
  } else {
    g_string_append_printf (s,
        "<form action=\"%s?session_id=%s\" method=\"post\" enctype=\"%s\" >\n",
        action, session->session_id, enctype);
  }

  if (session) {
    g_string_append_printf (s,
        "<input name=\"session_id\" type=\"hidden\" value=\"%s\">\n",
        session->session_id);
  }
  //g_string_append (s, "<fieldset>\n");
  //g_string_append_printf (s, "<legend>%s</legend>\n", name);

  for (i = 0; fields[i].type != GSS_FIELD_NONE; i++) {
    const char *default_value = NULL;

    if (fields[i].config_name) {
      default_value = gss_config_get (server->config, fields[i].config_name);
    }
    if (default_value == NULL)
      default_value = "";
    switch (fields[i].type) {
      case GSS_FIELD_SELECT:
        gss_config_form_add_select (s, fields + i, default_value);
        break;
      case GSS_FIELD_TEXT_INPUT:
        gss_config_form_add_text_input (s, fields + i, default_value);
        break;
      case GSS_FIELD_PASSWORD:
        gss_config_form_add_password (s, fields + i, default_value);
        break;
      case GSS_FIELD_CHECKBOX:
        gss_config_form_add_checkbox (s, fields + i, default_value);
        break;
      case GSS_FIELD_FILE:
        gss_config_form_add_file (s, fields + i, default_value);
        break;
      case GSS_FIELD_RADIO:
        gss_config_form_add_radio (s, fields + i, default_value);
        break;
      case GSS_FIELD_SUBMIT:
        if (fields[i].indent) {
          gss_config_form_add_submit (s, fields + i, default_value);
          if (in_fieldset)
            g_string_append (s, "</fieldset>\n");
        } else {
          if (in_fieldset)
            g_string_append (s, "</fieldset>\n");
          gss_config_form_add_submit (s, fields + i, default_value);
        }
        in_fieldset = FALSE;
        break;
      case GSS_FIELD_VERTICAL_SPACE:
        gss_html_append_break (s);
        break;
      case GSS_FIELD_SECTION:
        if (in_fieldset)
          g_string_append (s, "</fieldset>\n");
        g_string_append_printf (s, "<fieldset><legend>%s</legend>\n",
            fields[i].long_name);
        in_fieldset = TRUE;
        break;
      case GSS_FIELD_HIDDEN:
        gss_config_form_add_hidden (s, fields + i, default_value);
        break;
      default:
        break;
    }
  }

  if (in_fieldset)
    g_string_append (s, "</fieldset>\n");
  g_string_append (s, "</form>\n");
}
