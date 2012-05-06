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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ew-stream-server.h"

#include <gst/gst.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>


#define GETTEXT_PACKAGE "ew-streaming-server"

//#define CONFIG_FILENAME "/opt/entropywave/ew-oberon/config"
#define CONFIG_FILENAME "config"

extern GssConfigDefault config_defaults[];

gboolean verbose = TRUE;
gboolean cl_verbose;

void ew_stream_server_notify_url (const char *s, void *priv);

static void signal_interrupt (int signum);
static void append_style_html (GssServer * server, GString * s, void *priv);

#define N_PROGRAMS 10

GssProgram *programs[N_PROGRAMS];

static GOptionEntry entries[] = {
  {"verbose", 'v', 0, G_OPTION_ARG_NONE, &cl_verbose, "Be verbose", NULL},

  {NULL}

};

#define N_ENCODERS 1
GssServer *server;
GMainLoop *main_loop;

int
main (int argc, char *argv[])
{
  GError *error = NULL;
  GOptionContext *context;
  int i;

  if (!g_thread_supported ())
    g_thread_init (NULL);

  signal (SIGPIPE, SIG_IGN);
  signal (SIGINT, signal_interrupt);

  context = g_option_context_new ("- FIXME");
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  g_option_context_add_group (context, gst_init_get_option_group ());
  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_print ("option parsing failed: %s", error->message);
    exit (1);
  }
  g_option_context_free (context);

  server = gss_server_new ();
  server->append_style_html = append_style_html;
  server->append_style_html_priv = NULL;

  ew_stream_server_add_admin_callbacks (server, server->server);
  if (server->ssl_server) {
    ew_stream_server_add_admin_callbacks (server, server->ssl_server);
  }

  gss_server_read_config (server, CONFIG_FILENAME);

  gss_config_set_config_filename (server->config, CONFIG_FILENAME);
  gss_config_load_defaults (server->config, config_defaults);
  gss_config_load_from_file (server->config);
  gss_config_load_from_file_locked (server->config, CONFIG_FILENAME ".perm");
  gss_config_load_from_file_locked (server->config, CONFIG_FILENAME ".package");

  for (i = 0; i < N_PROGRAMS; i++) {
    char *key;

    key = g_strdup_printf ("stream%d_url", i);

    gss_config_set_notify (server->config, key,
        ew_stream_server_notify_url, GINT_TO_POINTER (i));

    ew_stream_server_notify_url (key, GINT_TO_POINTER (i));

    g_free (key);
  }

  //g_timeout_add (1000, periodic_timer, NULL);

  main_loop = g_main_loop_new (NULL, TRUE);

  g_main_loop_run (main_loop);

  gss_server_free (server);
  gss_server_deinit ();
  gst_deinit ();

  exit (0);
}

static void
signal_interrupt (int signum)
{
  if (main_loop) {
    g_main_loop_quit (main_loop);
  }
}


void
ew_stream_server_notify_url (const char *s, void *priv)
{
  int i = GPOINTER_TO_INT (priv);
  const char *url;

  url = gss_config_get (server->config, s);

  if (url[0] == 0 && programs[i] == NULL)
    return;
  if (programs[i] && strcmp (url, programs[i]->follow_host) == 0)
    return;

  if (programs[i]) {
    gss_server_remove_program (server, programs[i]);
    programs[i] = NULL;
  }

  if (url[0]) {
    char *stream_name;
    const char *stream_type;
    char *key;

    key = g_strdup_printf ("stream%d_type", i);
    stream_type = gss_config_get (server->config, key);
    g_free (key);

    key = g_strdup_printf ("stream%d_name", i);
    stream_name = g_strdup (gss_config_get (server->config, key));
    g_free (key);
    if (strcmp (stream_name, "") == 0) {
      g_free (stream_name);
      if (i == 0) {
        stream_name = g_strdup ("stream");
      } else {
        stream_name = g_strdup_printf ("stream%d", i);
      }
    }

    programs[i] = gss_server_add_program (server, stream_name);
    if (strcmp (stream_type, "http-follow") == 0) {
      gss_program_http_follow (programs[i], url);
    } else if (strcmp (stream_type, "ew-contrib") == 0) {
      gss_program_ew_contrib (programs[i]);
    } else if (strcmp (stream_type, "http-put") == 0) {
      gss_program_http_put (programs[i], "stream");
    } else {
      /* ew-follow */
      gss_program_follow (programs[i], url, "stream");
    }

    g_free (stream_name);
  }

}


static void
append_style_html (GssServer * server, GString * s, void *priv)
{
  g_string_append_printf (s,
      "<style type=\"text/css\">\n"
      "body {background-color: #998276; font-family: Verdana, Geneva, sans-serif;}\n"
      "div#container {width: 812px; background-image: url(/images/template_bodybg.png); background-repeat: repeat-y;}\n"
      "div#nav {text-align: center; margin: 0 auto;}\n"
      "div#nav div {display: inline; margin: 0 -1px;}\n"
      "div#nav img {padding: 0; margin: 0;}\n"
      "div#content {margin: 0 30px;}\n"
      "form {font-size: 10pt;}\n"
      "fieldset {margin: 10px 0;}\n"
      "legend {color: #282a8c; font-weight: bold;}\n"
      "input, textarea {background: -webkit-gradient(linear, left top, left bottom, from(#edeaea), to(#fff)); background: -moz-linear-gradient(top, #edeaea, #fff);}\n"
      "table.subtab {margin-left: 15px;}\n" ".indent {margin-left: 15px;}\n"
      /* FIXME this is a hack to remove the extra space under images */
      "img {border: 0px; margin-bottom: -5px; }\n"
      "</style>\n"
      "<!--[if IE 7]>\n"
      "<link rel=\"stylesheet\" href=\"ie7.css\" type=\"text/css\"></link>\n"
      "<![endif]-->\n");
}
