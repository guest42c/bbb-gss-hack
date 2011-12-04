
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
ew_config_form_add_select (GString *s, Field *item, const char *value)
{
  int i;

  if (item->indent) g_string_append (s, "&nbsp;&nbsp;&nbsp;&nbsp;\n");
  g_string_append_printf(s, "%s: <select id=\"%s\" name=\"%s\">\n",
      item->long_name,
      item->config_name,
      item->config_name);
  for(i=0;i<10;i++){
    if (item->options[i].long_name) {
      gboolean selected;

      selected = (value && g_str_equal (value, item->options[i].config_name));

      g_string_append_printf(s, "<option value=\"%s\" %s>%s</option>\n",
          item->options[i].config_name,
          selected ? "selected=\"selected\"" : "",
          item->options[i].long_name);
    }
  }
  g_string_append_printf(s, "</select>\n");
  g_string_append (s, "<br />\n");

}

void
ew_config_form_add_text_input (GString *s, Field *item, const char *value)
{
  if (item->indent) g_string_append (s, "&nbsp;&nbsp;&nbsp;&nbsp;\n");
  g_string_append_printf(s, "%s: <input id=\"%s\" type=\"text\" name=\"%s\" size=30 value=\"%s\">\n",
      item->long_name,
      item->config_name,
      item->config_name, value);
  g_string_append (s, "<br />\n");
}

void
ew_config_form_add_password (GString *s, Field *item, const char *value)
{
  if (item->indent) g_string_append (s, "&nbsp;&nbsp;&nbsp;&nbsp;\n");
  g_string_append_printf(s, "%s: <input id=\"%s\" type=\"password\" name=\"%s\" size=20>\n",
      item->long_name,
      item->config_name,
      item->config_name);
  g_string_append (s, "<br />\n");
}

void
ew_config_form_add_checkbox (GString *s, Field *item, const char *value)
{
  gboolean selected;

  selected = (value && g_str_equal (value, "on"));
  if (item->indent) g_string_append (s, "&nbsp;&nbsp;&nbsp;&nbsp;\n");
#if 0
  g_string_append_printf (s,
      "<script type=\"text/javascript\">\n"
      "function toggle() {\n"
      "var node = document.getElementById(\"%s\");\n"
      "var e = document.getElementById(\"%s\");\n"
      "e.disabled = node.checked;\n"
      "}\n"
      "</script>\n",
      item->config_name,
      "admin_token0");
#endif
  g_string_append_printf(s, "%s: <input type=\"hidden\" name=\"%s\" value=off>"
      "<input id=\"%s\" type=\"checkbox\" name=\"%s\" %s onclick=\"toggle()\">\n",
      item->long_name,
      item->config_name,
      item->config_name,
      item->config_name,
      selected ? "checked=on" : "");
  g_string_append (s, "<br />\n");
}

void
ew_config_form_add_file (GString *s, Field *item, const char *value)
{
  if (item->indent) g_string_append (s, "&nbsp;&nbsp;&nbsp;&nbsp;\n");
  g_string_append_printf(s, "%s: <input id=\"%s\" type=\"file\" name=\"%s\" value=\"%s\">\n",
      item->long_name,
      item->config_name,
      item->config_name, value);
  g_string_append (s, "<br />\n");
}

void
ew_config_form_add_radio (GString *s, Field *item, const char *value)
{
  int i;

  if (item->indent) g_string_append (s, "&nbsp;&nbsp;&nbsp;&nbsp;\n");
  g_string_append_printf(s, "%s<br />\n", item->long_name);
  for(i=0;i<10;i++){
    if (item->options[i].long_name) {
      gboolean selected;

      selected = (value && g_str_equal (value, item->options[i].config_name));
      g_string_append_printf(s,
          "<input id=\"%s\" type=\"radio\" name=\"%s\" value=\"%s\" %s>%s<br />\n",
          item->config_name,
          item->config_name,
          item->options[i].config_name,
          selected ? " checked=checked" : "",
          item->options[i].long_name);
    }
  }
}

void
ew_config_form_add_submit (GString *s, Field *item, const char *value)
{
  g_string_append (s, "<br />\n");
  g_string_append_printf (s,
      "<input id=\"%s\" type=\"submit\" value=\"%s\"/>\n",
      item->config_name,
      item->long_name);
  g_string_append (s, "<br />\n");
}

void
ew_config_form_add_hidden (GString *s, Field *item, const char *value)
{
  g_string_append_printf (s,
      "<input name=\"%s\" type=\"hidden\" value=\"%s\"/>\n",
      item->config_name,
      "1");
}

void
ew_config_form_add_form (EwServer *server, GString * s, const char *action,
    const char *name, Field *fields, EwSession *session)
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
        "<input name=\"session_id\" type=\"hidden\" value=\"%s\"/>\n",
        session->session_id);
  }

  //g_string_append (s, "<fieldset>\n");
  //g_string_append_printf (s, "<legend>%s</legend>\n", name);

  for (i=0; fields[i].type != FIELD_NONE; i++) {
    const char *default_value = NULL;
    
    if (fields[i].config_name) {
      default_value = ew_config_get (server->config, fields[i].config_name);
    }
    if (default_value == NULL) default_value = "";
    switch (fields[i].type) {
      case FIELD_SELECT:
        ew_config_form_add_select (s, fields + i, default_value);
        break;
      case FIELD_TEXT_INPUT:
        ew_config_form_add_text_input (s, fields + i, default_value);
        break;
      case FIELD_PASSWORD:
        ew_config_form_add_password (s, fields + i, default_value);
        break;
      case FIELD_CHECKBOX:
        ew_config_form_add_checkbox (s, fields + i, default_value);
        break;
      case FIELD_FILE:
        ew_config_form_add_file (s, fields + i, default_value);
        break;
      case FIELD_RADIO:
        ew_config_form_add_radio (s, fields + i, default_value);
        break;
      case FIELD_SUBMIT:
        if (fields[i].indent) {
          ew_config_form_add_submit (s, fields + i, default_value);
          if (in_fieldset) g_string_append (s, "</fieldset>\n");
        } else {
          if (in_fieldset) g_string_append (s, "</fieldset>\n");
          ew_config_form_add_submit (s, fields + i, default_value);
        }
        in_fieldset = FALSE;
        break;
      case FIELD_VERTICAL_SPACE:
        g_string_append (s, "<br />\n");
        break;
      case FIELD_SECTION:
        if (in_fieldset) g_string_append (s, "</fieldset>\n");
        g_string_append_printf (s, "<fieldset><legend>%s</legend>\n",
            fields[i].long_name);
        in_fieldset = TRUE;
        break;
      case FIELD_HIDDEN:
        ew_config_form_add_hidden (s, fields + i, default_value);
        break;
      default:
        break;
    }
  }

  if (in_fieldset) g_string_append (s, "</fieldset>\n");
  g_string_append (s, "</form>\n");
}


