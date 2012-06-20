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

#include <gst-stream-server/gss-server.h>
#include <gst-stream-server/gss-config.h>
#include <gst-stream-server/gss-form.h>
#include <gst-stream-server/gss-html.h>
#include <gst-stream-server/gss-soup.h>

#include <glib/gstdio.h>
#include <glib-object.h>


static int enable_machine = TRUE;

void ew_stream_server_add_admin_callbacks (GssServer * server);

static void admin_resource_get (GssTransaction * t);

GssField control_fields[] = {
  {GSS_FIELD_SECTION, NULL, "Control"},
  {GSS_FIELD_SECTION, NULL, "Program #0"},
  {GSS_FIELD_ENABLE, "stream0", "enable"},
  {GSS_FIELD_TEXT_INPUT, "stream0_name", "Stream Name", "stream0", 0},
  {GSS_FIELD_SELECT, "stream0_type", "Connection type", "ew-follow", 0,
        {
              {"ew-follow", "E1000/S1000 follower"},
              {"http-follow", "HTTP stream follower"},
              {"ew-contrib", "Entropy Wave contributor"},
              {"icecast", "Icecast contributor"},
              {"http-put", "HTTP PUT contributor"}
            }
      },
  {GSS_FIELD_TEXT_INPUT, "stream0_url", "Stream URL or E1000 IP address",
      "10.0.2.40", 0},
  //{ GSS_FIELD_TEXT_INPUT, "stream0_width", "Width", "640", 0 },
  //{ GSS_FIELD_TEXT_INPUT, "stream0_height", "Height", "360", 0 },
  //{ GSS_FIELD_TEXT_INPUT, "stream0_bitrate", "Bitrate", "700000", 0 },
  {GSS_FIELD_SECTION, NULL, "Program #1"},
  {GSS_FIELD_ENABLE, "stream1", "enable"},
  {GSS_FIELD_TEXT_INPUT, "stream1_name", "Stream Name", "stream1", 0},
  {GSS_FIELD_SELECT, "stream1_type", "Connection type", "ew-follow", 0,
        {
              {"ew-follow", "E1000/S1000 follower"},
              {"http-follow", "HTTP stream follower"},
              {"ew-contrib", "Entropy Wave contributor"},
              {"icecast", "Icecast contributor"},
              {"http-put", "HTTP PUT contributor"}
            }
      },
  {GSS_FIELD_TEXT_INPUT, "stream1_url", "Stream URL or E1000 IP address", "",
      0},
  {GSS_FIELD_SECTION, NULL, "Program #2"},
  {GSS_FIELD_ENABLE, "stream2", "enable"},
  {GSS_FIELD_TEXT_INPUT, "stream2_name", "Stream Name", "stream2", 0},
  {GSS_FIELD_SELECT, "stream2_type", "Connection type", "ew-follow", 0,
        {
              {"ew-follow", "E1000/S1000 follower"},
              {"http-follow", "HTTP stream follower"},
              {"ew-contrib", "Entropy Wave contributor"},
              {"icecast", "Icecast contributor"},
              {"http-put", "HTTP PUT contributor"}
            }
      },
  {GSS_FIELD_TEXT_INPUT, "stream2_url", "Stream URL or E1000 IP address", "",
      0},
  {GSS_FIELD_SECTION, NULL, "Program #3"},
  {GSS_FIELD_TEXT_INPUT, "stream3_name", "Stream Name", "stream3", 0},
  {GSS_FIELD_SELECT, "stream3_type", "Connection type", "ew-follow", 0,
        {
              {"ew-follow", "E1000/S1000 follower"},
              {"http-follow", "HTTP stream follower"},
              {"ew-contrib", "Entropy Wave contributor"},
              {"icecast", "Icecast contributor"},
              {"http-put", "HTTP PUT contributor"}
            }
      },
  {GSS_FIELD_TEXT_INPUT, "stream3_url", "Stream URL or E1000 IP address", "",
      0},
  {GSS_FIELD_SECTION, NULL, "Program #4"},
  {GSS_FIELD_TEXT_INPUT, "stream4_name", "Stream Name", "stream4", 0},
  {GSS_FIELD_SELECT, "stream4_type", "Connection type", "ew-follow", 0,
        {
              {"ew-follow", "E1000/S1000 follower"},
              {"http-follow", "HTTP stream follower"},
              {"ew-contrib", "Entropy Wave contributor"},
              {"icecast", "Icecast contributor"},
              {"http-put", "HTTP PUT contributor"}
            }
      },
  {GSS_FIELD_TEXT_INPUT, "stream4_url", "Stream URL or E1000 IP address", "",
      0},
  {GSS_FIELD_SUBMIT, "submit", "Update Configuration", NULL, 0},
  {GSS_FIELD_NONE}
};

GssField server_fields[] = {
  {GSS_FIELD_SECTION, NULL, "HTTP Server Configuration"},
  {GSS_FIELD_CHECKBOX, "enable_streaming", "Enable Public Streaming", "on", 1},
  {GSS_FIELD_CHECKBOX, "enable_public_ui", "Enable public user interface", "on",
      0},

  {GSS_FIELD_TEXT_INPUT, "server_name", "Server Hostname", "127.0.0.1", 0},
  {GSS_FIELD_TEXT_INPUT, "max_connections", "Max Connections", "10000", 0},
  {GSS_FIELD_TEXT_INPUT, "max_bandwidth", "Max Bandwidth (kbytes/sec)",
        "100000",
      0},
  {GSS_FIELD_SUBMIT, "submit", "Update Configuration", NULL, 1},
  {GSS_FIELD_NONE}
};

GssField access_fields[] = {
  {GSS_FIELD_SECTION, NULL, "Access Restrictions"},
  {GSS_FIELD_TEXT_INPUT, "hosts_allow", "Allowed Hosts (admin)", "0.0.0.0/0",
      0},
  //{ GSS_FIELD_TEXT_INPUT, "hosts_allow_stream", "Streaming", "0.0.0.0/0", 0 },
  {GSS_FIELD_SUBMIT, "submit", "Update", NULL, 1},
  {GSS_FIELD_NONE}
};

GssField admin_password_fields[] = {
  {GSS_FIELD_SECTION, NULL, "Administrator Password"},
  {GSS_FIELD_PASSWORD, "admin_token0", "Password", "", 0},
  {GSS_FIELD_PASSWORD, "admin_token1", "Retype", "", 0},
  {GSS_FIELD_SUBMIT, "submit", "Change Password", NULL, 1},
  {GSS_FIELD_NONE}
};

GssField configuration_file_fields[] = {
  {GSS_FIELD_SECTION, NULL, "Configuration File"},
  {GSS_FIELD_FILE, "config_file", "Configuration", "config", 0},
  {GSS_FIELD_SUBMIT, "submit", "Upload File", NULL, 1},
  {GSS_FIELD_NONE}
};

GssField firmware_file_fields[] = {
  {GSS_FIELD_SECTION, NULL, "Firmware Update"},
  {GSS_FIELD_FILE, "firmware_file", "Upload File", "config", 0},
  {GSS_FIELD_SUBMIT, "submit", "Update Firmware", NULL, 1},
  {GSS_FIELD_NONE}
};

GssField certificate_file_fields[] = {
  {GSS_FIELD_SECTION, NULL, "Certificate Upload"},
  {GSS_FIELD_FILE, "cert_file", "Certificate", "server.crt", 0},
  {GSS_FIELD_FILE, "key_file", "Key", "server.key", 0},
  {GSS_FIELD_SUBMIT, "submit", "Upload Files", NULL, 1},
  {GSS_FIELD_NONE}
};

GssField reboot_fields[] = {
  {GSS_FIELD_SECTION, NULL, "Reboot Machine"},
  {GSS_FIELD_HIDDEN, "reboot", NULL, "yes", 0},
  {GSS_FIELD_SUBMIT, "submit", "Reboot", NULL, 1},
  {GSS_FIELD_NONE}
};

GssField poweroff_fields[] = {
  {GSS_FIELD_SECTION, NULL, "Power Off Machine"},
  {GSS_FIELD_HIDDEN, "poweroff", NULL, "yes", 0},
  {GSS_FIELD_SUBMIT, "submit", "Power Off", NULL, 1},
  {GSS_FIELD_NONE}
};



static void
admin_resource_post (GssTransaction * t)
{

  if (t->msg->method == SOUP_METHOD_POST) {
    gss_config_handle_post (t->server->config, t->msg);
  }

  if (t->resource->get_callback) {
    t->resource->get_callback (t);
  } else {
    t->s = g_string_new ("");

    gss_html_header (t);
    g_string_append (t->s, "OK");
    gss_html_footer (t);
  }
}

static void
admin_status_resource_get (GssTransaction * t)
{
  GString *s;

  t->s = s = g_string_new ("");

  g_string_append (s, "OK\n");
}

static void
admin_config_resource_get (GssTransaction * t)
{
  GString *s;

  t->s = s = g_string_new ("");

  gss_config_hash_to_string (s, t->server->config->hash);
}

static void
admin_resource_get (GssTransaction * t)
{
  GString *s;

  t->s = s = g_string_new ("");

  gss_html_header (t);
#if 0
  if (t->msg->method == SOUP_METHOD_POST) {
    g_string_append_printf (s, "<br />Configuration Updated!<br /><br />\n");
  }
#endif
  gss_config_form_add_form (t->server, s, "/admin", "Control",
      control_fields, t->session);

  gss_html_footer (t);
}

static void
admin_server_resource_get (GssTransaction * t)
{
  GString *s;

  t->s = s = g_string_new ("");

  gss_html_header (t);
#if 0
  if (t->msg->method == SOUP_METHOD_POST) {
    g_string_append_printf (s, "<br />Configuration Updated!<br /><br />\n");
  }
#endif
  gss_config_form_add_form (t->server, s, "/admin/server",
      "HTTP Server Configuration", server_fields, t->session);
  gss_config_form_add_form (t->server, s, "/admin/access",
      "Access Restrictions", access_fields, t->session);
  gss_config_form_add_form (t->server, s, "/admin/status",
      "Upload Certificate", certificate_file_fields, t->session);

  gss_html_footer (t);
}

static void
admin_log_resource_get (GssTransaction * t)
{
  GString *s;

  t->s = s = g_string_new ("");

  gss_html_header (t);
#if 0
  if (t->msg->method == SOUP_METHOD_POST) {
    g_string_append_printf (s, "<br />Configuration Updated!<br /><br />\n");
  }
#endif


  {
    GList *g;
    g_string_append_printf (s, "<h2>Programs</h2>\n");
    g_string_append_printf (s, "<pre>\n");
    for (g = t->server->programs; g; g = g_list_next (g)) {
      GssProgram *program = g->data;
      GssStream *stream;
      guint64 n_bytes_in = 0;
      guint64 n_bytes_out = 0;
      int j;

      g_string_append_printf (s, "Program: %s\n", GST_OBJECT_NAME (program));

      for (j = 0; j < program->n_streams; j++) {
        if (program->streams == NULL)
          continue;
        stream = program->streams[j];

        gss_stream_get_stats (stream, &n_bytes_in, &n_bytes_out);

        g_string_append_printf (s, "  %s clients=%d max_clients%d "
            "bitrate=%" G_GUINT64_FORMAT " max_bitrate=%"
            G_GUINT64_FORMAT " in=%" G_GUINT64_FORMAT " out=%"
            G_GUINT64_FORMAT,
            GST_OBJECT_NAME (program),
            program->metrics->n_clients,
            program->metrics->max_clients,
            program->metrics->bitrate,
            program->metrics->max_bitrate, n_bytes_in, n_bytes_out);
        gss_html_append_break (s);
      }
    }
    g_string_append_printf (s, "</pre>\n");
  }
  {
    GList *g;
    g_string_append_printf (s, "<h2>Log</h2>\n");
    g_string_append_printf (s, "<pre>\n");
    for (g = t->server->messages; g; g = g_list_next (g)) {
      g_string_append_printf (s, "%s\n", (char *) g->data);
    }
    g_string_append_printf (s, "</pre>\n");
  }

  gss_html_footer (t);
}

static void
admin_admin_resource_get (GssTransaction * t)
{
  GString *s;

  g_return_if_fail (t);
  g_return_if_fail (t->session);

  t->s = s = g_string_new ("");

  gss_html_header (t);
#if 0
  if (t->msg->method == SOUP_METHOD_POST) {
    g_string_append_printf (s, "<br />Configuration Updated!<br /><br />\n");
  }
#endif
  g_string_append_printf (s,
      "Configuration File: <a href=\"/admin/config?session_id=%s\">LINK</a><br />\n",
      t->session->session_id);
  gss_config_form_add_form (t->server, s, "/admin/admin_password",
      "Admin Password", admin_password_fields, t->session);
  gss_config_form_add_form (t->server, s, "/admin/upload_config",
      "Configuration File", configuration_file_fields, t->session);

  gss_html_footer (t);
}

static void
admin_machine_resource_get (GssTransaction * t)
{
  GString *s;

  g_return_if_fail (t);
  g_return_if_fail (t->session);

  t->s = s = g_string_new ("");

  gss_html_header (t);
#if 0
  if (t->msg->method == SOUP_METHOD_POST) {
    g_string_append_printf (s, "<br />Configuration Updated!<br /><br />\n");
  }
#endif
  g_string_append_printf (s, "Firmware Version: %s<br />\n",
      gss_config_get (t->server->config, "version"));
  gss_config_form_add_form (t->server, s, "/admin/machine",
      "Firmware file", firmware_file_fields, t->session);
  gss_config_form_add_form (t->server, s, "/admin/machine", "Reboot",
      reboot_fields, t->session);
  gss_config_form_add_form (t->server, s, "/admin/machine", "Power Off",
      poweroff_fields, t->session);


  gss_html_footer (t);
}


void
ew_stream_server_add_admin_callbacks (GssServer * server)
{
  GssResource *r;

  r = gss_server_add_resource (server, "/admin", GSS_RESOURCE_ADMIN,
      "text/html", admin_resource_get, NULL, admin_resource_post, NULL);
  gss_server_add_admin_resource (server, r, "Programs");
  r = gss_server_add_resource (server, "/admin/server", GSS_RESOURCE_ADMIN,
      "text/html", admin_server_resource_get, NULL, admin_resource_post, NULL);
  gss_server_add_admin_resource (server, r, "Server");
  r = gss_server_add_resource (server, "/admin/admin", GSS_RESOURCE_ADMIN,
      "text/html", admin_admin_resource_get, NULL, admin_resource_post, NULL);
  gss_server_add_admin_resource (server, r, "Administration");
  r = gss_server_add_resource (server, "/admin/log", GSS_RESOURCE_ADMIN,
      "text/html", admin_log_resource_get, NULL, admin_resource_post, NULL);
  gss_server_add_admin_resource (server, r, "Log");
  r = gss_server_add_resource (server, "/admin/status", GSS_RESOURCE_ADMIN,
      "text/plain", admin_status_resource_get, NULL, admin_resource_post, NULL);
  r = gss_server_add_resource (server, "/admin/config", GSS_RESOURCE_ADMIN,
      "text/plain", admin_config_resource_get, NULL, admin_resource_post, NULL);

  /* This is for machine control, i.e., in the S1000 */
  if (enable_machine) {
    r = gss_server_add_resource (server, "/admin/machine", GSS_RESOURCE_ADMIN,
        "text/html", admin_machine_resource_get, NULL, admin_resource_post,
        NULL);
    gss_server_add_admin_resource (server, r, "Machine");
  }
}


GssConfigDefault config_defaults[] = {
  /* master enable */
  {"enable_streaming", "on"}
  ,

  /* web server config */
  {"server_name", ""}
  ,
  {"max_connections", "10000"}
  ,
  {"max_bandwidth", "100000"}
  ,

  {NULL, NULL}
};
