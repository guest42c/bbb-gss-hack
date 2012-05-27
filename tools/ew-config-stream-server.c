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


static void admin_resource_get (GssTransaction * t);

GssField control_fields[] = {
  {GSS_FIELD_SECTION, NULL, "Control"},
  {GSS_FIELD_CHECKBOX, "enable_streaming", "Enable Public Streaming", "on", 1},
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

GssField editor_password_fields[] = {
  {GSS_FIELD_SECTION, NULL, "Editor Password"},
  {GSS_FIELD_PASSWORD, "editor_token0", "Password", "", 0},
  {GSS_FIELD_PASSWORD, "editor_token1", "Retype", "", 0},
  {GSS_FIELD_SUBMIT, "submit", "Change Password", NULL, 1},
  {GSS_FIELD_NONE}
};

GssField configuration_file_fields[] = {
  {GSS_FIELD_SECTION, NULL, "Configuration File"},
  {GSS_FIELD_FILE, "config_file", "Upload Config", "config", 0},
  {GSS_FIELD_SUBMIT, "submit", "Upload Configuration", NULL, 1},
  {GSS_FIELD_NONE}
};

GssField certificate_file_fields[] = {
  {GSS_FIELD_SECTION, NULL, "Certificate Upload"},
  {GSS_FIELD_FILE, "cert_file", "Upload Certificate", "server.crt", 0},
  {GSS_FIELD_FILE, "key_file", "Upload Key", "server.key", 0},
  {GSS_FIELD_SUBMIT, "submit", "Update Files", NULL, 1},
  {GSS_FIELD_NONE}
};


enum
{
  ADMIN_NONE = 0,
  ADMIN_CONTROL,
  ADMIN_SERVER,
  ADMIN_ADMIN,
  ADMIN_LOG,
  ADMIN_STATUS,
  ADMIN_CONFIG,
  ADMIN_ACCESS
};

typedef struct _AdminPage AdminPage;
struct _AdminPage
{
  const char *location;
  int type;
  gboolean admin_only;
};

AdminPage admin_pages[] = {
  {"/admin", ADMIN_CONTROL, FALSE},
  {"/admin/", ADMIN_CONTROL, FALSE},
  {"/admin/server", ADMIN_SERVER, TRUE},
  {"/admin/admin", ADMIN_ADMIN, TRUE},
  {"/admin/admin_password", ADMIN_ADMIN, TRUE},
  {"/admin/editor_password", ADMIN_ADMIN, FALSE},
  {"/admin/log", ADMIN_LOG, TRUE},
  {"/admin/status", ADMIN_STATUS, FALSE},
  {"/admin/config", ADMIN_CONFIG, TRUE},
  {"/admin/access", ADMIN_ACCESS, TRUE}
};

static void
admin_resource_post (GssTransaction * t)
{
  int type;
  GString *s;
  int i;

  type = ADMIN_NONE;
  for (i = 0; i < G_N_ELEMENTS (admin_pages); i++) {
    if (g_str_equal (admin_pages[i].location, t->path)) {
      type = admin_pages[i].type;
      break;
    }
  }
  if (type == ADMIN_NONE) {
    gss_html_error_404 (t->msg);
    return;
  }

  if (t->msg->method == SOUP_METHOD_POST) {
    gss_config_handle_post (t->server->config, t->msg);
  }

  t->s = s = g_string_new ("");

  gss_html_header (t);
  g_string_append (s, "OK");
  gss_html_footer (t);
}

static void
admin_resource_get (GssTransaction * t)
{
  GString *s;
  int type;
  int i;


  type = ADMIN_NONE;
  for (i = 0; i < G_N_ELEMENTS (admin_pages); i++) {
    if (g_str_equal (admin_pages[i].location, t->path)) {
      type = admin_pages[i].type;
      break;
    }
  }
  if (type == ADMIN_NONE) {
    gss_html_error_404 (t->msg);
    return;
  }

  t->s = s = g_string_new ("");

  if (type == ADMIN_STATUS) {
    //mime_type = "text/plain";
    g_string_append (s, "OK\n");
  } else if (type == ADMIN_CONFIG) {
    //mime_type = "text/plain";
    gss_config_hash_to_string (s, t->server->config->hash);
  } else {
    gss_html_header (t);

    if (t->msg->method == SOUP_METHOD_POST) {
      g_string_append_printf (s, "<br />Configuration Updated!<br /><br />\n");
    }

    switch (type) {
      case ADMIN_CONTROL:
#if 0
      {
        int i;
        for (i = 0; i < t->server->n_programs; i++) {
          GssProgram *program = t->server->programs[i];
          g_string_append_printf (s, "%s type=%d %s %s<br />\n",
              program->location,
              program->program_type, program->follow_uri, program->follow_host);
        }
      }
#endif
        gss_config_form_add_form (t->server, s, "/admin", "Control",
            control_fields, t->session);
        break;
      case ADMIN_SERVER:
        gss_config_form_add_form (t->server, s, "/admin/server",
            "HTTP Server Configuration", server_fields, t->session);
        break;
      case ADMIN_LOG:
      {
        int i;
        g_string_append_printf (s, "<pre>\n");
        g_string_append_printf (s, "Streams:\n");
        for (i = 0; i < t->server->n_programs; i++) {
          GssProgram *program = t->server->programs[i];
          GssServerStream *stream;
          guint64 n_bytes_in = 0;
          guint64 n_bytes_out = 0;
          int j;

          g_string_append_printf (s, "Program: %s\n", program->location);

          for (j = 0; j < program->n_streams; j++) {
            if (program->streams == NULL)
              continue;
            stream = program->streams[j];

            gss_stream_get_stats (stream, &n_bytes_in, &n_bytes_out);

            g_string_append_printf (s, "  %s clients=%d max_clients%d "
                "bitrate=%" G_GUINT64_FORMAT " max_bitrate=%"
                G_GUINT64_FORMAT " in=%" G_GUINT64_FORMAT " out=%"
                G_GUINT64_FORMAT,
                program->location,
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
          g_string_append_printf (s, "<pre>\n");
          g_string_append_printf (s, "Log:\n");
          for (g = t->server->messages; g; g = g_list_next (g)) {
            g_string_append_printf (s, "%s\n", (char *) g->data);
          }
          g_string_append_printf (s, "</pre>\n");
        }
        break;
      case ADMIN_ADMIN:
        g_string_append_printf (s, "Firmware Version: %s<br />\n",
            gss_config_get (t->server->config, "version"));
        g_string_append_printf (s,
            "Configuration File: <a href=\"/admin/config?session_id=%s\">LINK</a><br />\n",
            t->session->session_id);
        gss_config_form_add_form (t->server, s, "/admin/admin_password",
            "Admin Password", admin_password_fields, t->session);
        gss_config_form_add_form (t->server, s, "/admin/editor_password",
            "Editor Password", editor_password_fields, t->session);
        gss_config_form_add_form (t->server, s, "/admin/upload_config",
            "Configuration File", configuration_file_fields, t->session);
        gss_config_form_add_form (t->server, s, "/admin/status",
            "Upload Certificate", certificate_file_fields, t->session);
        break;
      case ADMIN_ACCESS:
        gss_config_form_add_form (t->server, s, "/admin/access",
            "Access Restrictions", access_fields, t->session);
        break;
      default:
        break;
    }

    //g_string_append (s, "</div><!-- end content div -->\n");
    gss_html_footer (t);
  }
}


void
ew_stream_server_add_admin_callbacks (GssServer * server)
{
  gss_server_add_resource (server, "/admin", 0,
      "text/html", admin_resource_get, NULL, admin_resource_post, NULL);
}


GssConfigDefault config_defaults[] = {
  {"mode", "streamer"},

  /* master enable */
  {"enable_streaming", "on"},

  /* web server config */
  {"server_name", ""},
  {"max_connections", "10000"},
  {"max_bandwidth", "100000"},

  /* Ethernet config */
  {"eth0_name", "entropywave"},
  {"eth0_config", "manual"},
  {"eth0_ipaddr", "192.168.0.10"},
  {"eth0_netmask", "255.255.255.0"},
  {"eth0_gateway", "192.168.0.1"},
  {"eth1_name", "entropywave"},
  {"eth1_config", "manual"},
  {"eth1_ipaddr", "192.168.1.10"},
  {"eth1_netmask", "255.255.255.0"},
  {"eth1_gateway", "192.168.1.1"},

  {NULL, NULL}
};
