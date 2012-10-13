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



void
gss_config_form_add_select (GString * s, GssField * item, const char *value)
{
  int i;

  GSS_A ("<div class='control-group'>\n");
  GSS_P ("<label class='control-label' for='%s'>%s</label>\n",
      item->config_name, item->long_name);
  GSS_A ("<div class='controls'>\n");

  GSS_P ("<select id='%s' name='%s'>\n", item->config_name, item->config_name);
  for (i = 0; i < GSS_FORM_NUM_OPTIONS; i++) {
    if (item->options[i].long_name) {
      gboolean selected;

      selected = (value && g_str_equal (value, item->options[i].config_name));

      GSS_P ("<option value=\"%s\" %s>%s</option>\n",
          item->options[i].config_name,
          selected ? "selected=\"selected\"" : "", item->options[i].long_name);
    }
  }
  GSS_P ("</select>\n");
  gss_html_append_break (s);
  GSS_A ("</div>\n");
  GSS_A ("</div>\n");

}

void
gss_config_form_add_text_input (GString * s, GssField * item, const char *value)
{
  GSS_A ("<div class='control-group'>\n");
  GSS_P ("<label class='control-label' for='%s'>%s</label>\n",
      item->config_name, item->long_name);
  GSS_A ("<div class='controls'>\n");
  GSS_P
      ("<input type='text' class='input-xlarge' id='%s' name='%s' value='%s'>\n",
      item->config_name, item->config_name, value);
  GSS_A ("</div>\n");
  GSS_A ("</div>\n");
}

void
gss_config_form_add_password (GString * s, GssField * item, const char *value)
{
  GSS_A ("<div class='control-group'>\n");
  GSS_P ("<label class='control-label' for='%s'>%s</label>\n",
      item->config_name, item->long_name);
  GSS_A ("<div class='controls'>\n");
  GSS_P
      ("<input type='password' class='input-xlarge' id='%s' name='%s' value='%s'>\n",
      item->config_name, item->config_name, value);
  GSS_A ("</div>\n");
  GSS_A ("</div>\n");
}

void
gss_config_form_add_checkbox (GString * s, GssField * item, const char *value)
{
  gboolean selected;

  selected = (value && g_str_equal (value, "on"));

  GSS_A ("<div class='control-group'>\n");
  GSS_P ("<label class='control-label' for='%s'>%s</label>\n",
      item->config_name, item->long_name);
  GSS_A ("<div class='controls'>\n");
  GSS_P ("<input type='hidden' name='%s' value=off>"
      "<input type='checkbox' class='input-xlarge' id='%s' name='%s' value='%s' %s>\n",
      item->config_name,
      item->config_name, item->config_name, value,
      selected ? "checked=on" : "");
  GSS_A ("</div>\n");
  GSS_A ("</div>\n");
}

void
gss_config_form_add_file (GString * s, GssField * item, const char *value)
{
  GSS_A ("<div class='control-group'>\n");
  GSS_P ("<label class='control-label' for='%s'>%s</label>\n",
      item->config_name, item->long_name);
  GSS_A ("<div class='controls'>\n");
  GSS_P
      ("<input type='file' class='input-xlarge' id='%s' name='%s' value='%s'>\n",
      item->config_name, item->config_name, value);
  GSS_A ("</div>\n");
  GSS_A ("</div>\n");
}

void
gss_config_form_add_radio (GString * s, GssField * item, const char *value)
{
  int i;

  GSS_A ("<div class='control-group'>\n");
  GSS_P ("<label class='control-label' for='%s'>%s</label>\n",
      item->config_name, item->long_name);
  GSS_A ("<div class='controls'>\n");

  for (i = 0; i < GSS_FORM_NUM_OPTIONS; i++) {
    if (item->options[i].long_name) {
      gboolean selected;

      selected = (value && g_str_equal (value, item->options[i].config_name));
      GSS_P
          ("<input class='input-xlarge' type='radio' name='%s' value='%s' %s>%s\n",
          item->config_name, item->options[i].config_name,
          selected ? " checked=checked" : "", item->options[i].long_name);
      gss_html_append_break (s);
    }
  }
  GSS_A ("</div>\n");
  GSS_A ("</div>\n");
}

void
gss_config_form_add_submit (GString * s, GssField * item, const char *value)
{
  GSS_A ("<div class='control-group'>\n");
  GSS_A ("<div class='controls'>\n");
  GSS_P ("<input type='submit' class='input-xlarge' value='%s'>\n",
      item->long_name);
  GSS_A ("</div>\n");
  GSS_A ("</div>\n");
}

void
gss_config_form_add_hidden (GString * s, GssField * item, const char *value)
{
  GSS_P ("<input name=\"%s\" type=\"hidden\" value=\"%s\">\n",
      item->config_name, "1");
}

void
gss_config_form_add_enable (GString * s, GssField * item, const char *value)
{
  gboolean selected;

  selected = (value && g_str_equal (value, "on"));

  GSS_A ("<div class='control-group'>\n");
  GSS_P ("<label class='control-label' for='%s'>%s</label>\n",
      item->config_name, item->long_name);
  GSS_A ("<div class='controls'>\n");

  GSS_P ("<script type=\"text/javascript\">\n"
      "function toggle(node_name,e_name) {\n"
      "var node = document.getElementById(node_name);\n"
      "var e = document.getElementById(e_name);\n"
      "if (node.checked) e.style.display='block';\n"
      "else e.style.display='none';\n" "}\n" "</script>\n");

  GSS_P ("<input type=\"hidden\" name=\"%s\" value=off>"
      "<input id=\"%s\" type=\"checkbox\" name=\"%s\" %s "
      "onclick=\"toggle('%s','div_%s')\">\n", item->config_name,
      item->config_name, item->config_name, selected ? "checked=on" : "",
      item->config_name, item->config_name);
  GSS_A ("</div>\n");
  GSS_A ("</div>\n");
  GSS_P ("<div id='div_%s' %s>", item->config_name,
      selected ? "" : "style='display:none'");
}

void
gss_config_form_add_form (GssServer * server, GString * s, const char *action,
    const char *name, GssField * fields, GssSession * session)
{
  int i;
  const char *enctype;
  gboolean in_fieldset = FALSE;
  gboolean in_enable = FALSE;

  enctype = "multipart/form-data";

  GSS_A ("<iframe name=\"hidden_frame\" src=\"about:blank\" "
      "style=\"display:none; width:0px; height:0px\"></iframe>\n");
  if (session == NULL) {
    GSS_P
        ("<form class='form-horizontal' action=\"%s\" method=\"post\" enctype=\"%s\" >\n",
        action, enctype);
  } else {
    GSS_P
        ("<form class='form-horizontal' action=\"%s?session_id=%s\" method=\"post\" enctype=\"%s\" >\n",
        action, session->session_id, enctype);
  }

  if (session) {
    GSS_P ("<input name=\"session_id\" type=\"hidden\" value=\"%s\">\n",
        session->session_id);
  }

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
            GSS_A ("</fieldset>\n");
        } else {
          if (in_fieldset)
            GSS_A ("</fieldset>\n");
          gss_config_form_add_submit (s, fields + i, default_value);
        }
        in_fieldset = FALSE;
        break;
      case GSS_FIELD_VERTICAL_SPACE:
        gss_html_append_break (s);
        break;
      case GSS_FIELD_SECTION:
        if (in_fieldset) {
          if (in_enable) {
            GSS_A ("</div>\n");
          }
          in_enable = FALSE;
          GSS_A ("</fieldset>\n");
        }
        GSS_P ("<fieldset><legend>%s</legend>\n", fields[i].long_name);
        in_fieldset = TRUE;
        break;
      case GSS_FIELD_ENABLE:
        gss_config_form_add_enable (s, fields + i, default_value);
        in_enable = TRUE;
        break;
      case GSS_FIELD_HIDDEN:
        gss_config_form_add_hidden (s, fields + i, default_value);
        break;
      default:
        break;
    }
  }

  if (in_fieldset) {
    if (in_enable) {
      GSS_A ("</div>\n");
    }
    GSS_A ("</fieldset>\n");
  }
  GSS_A ("</form>\n");
}
