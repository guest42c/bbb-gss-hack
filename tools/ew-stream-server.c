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


#define GETTEXT_PACKAGE "ew-stream-server"

#define CONFIG_FILENAME "config"

#define LOG g_print


extern GssConfigDefault config_defaults[];

gboolean verbose = TRUE;
gboolean cl_verbose;
gboolean enable_daemon = FALSE;

void ew_stream_server_notify_url (const char *s, void *priv);

static void signal_interrupt (int signum);
static void footer_html (GssServer * server, GString * s, void *priv);
static void add_program (GssServer * server, int i);


static GOptionEntry entries[] = {
  {"verbose", 'v', 0, G_OPTION_ARG_NONE, &cl_verbose, "Be verbose", NULL},
  {"daemon", 'd', 0, G_OPTION_ARG_NONE, &enable_daemon, "Daemonize", NULL},

  {NULL}

};

GssServer *server;
GMainLoop *main_loop;

static void
do_quit (int signal)
{
  LOG ("caught signal %d", signal);

  kill (0, SIGTERM);

  exit (0);
}

static void
daemonize (void)
{
  int ret;
  int fd;
  char s[20];

#if 0
  ret = chdir ("/var/log");
  if (ret < 0)
    exit (1);
#endif

  ret = fork ();
  if (ret < 0)
    exit (1);                   /* fork error */
  if (ret > 0)
    exit (0);                   /* parent */

  ret = setpgid (0, 0);
  if (ret < 0) {
    g_print ("could not set process group\n");
  }
  ret = setuid (65534);
  if (ret < 0) {
    g_print ("could not switch user to 'nobody'\n");
  }
  umask (0022);

  fd = open ("/dev/null", O_RDWR);
  dup2 (fd, 0);
  close (fd);

#if 0
  fd = open ("/tmp/ew-stream-server.log", O_RDWR | O_CREAT | O_TRUNC, 0644);
#else
  fd = open ("/dev/null", O_RDWR | O_CREAT | O_TRUNC, 0644);
#endif
  dup2 (fd, 1);
  dup2 (fd, 2);
  close (fd);

  fd = open ("/var/run/ew-stream-server.pid", O_RDWR | O_CREAT | O_TRUNC, 0644);
  sprintf (s, "%d\n", getpid ());
  ret = write (fd, s, strlen (s));
  close (fd);

  //signal (SIGCHLD, SIG_IGN);
  signal (SIGHUP, do_quit);
  signal (SIGTERM, do_quit);

}

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

  context = g_option_context_new ("- Entropy Wave Streaming Server");
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  g_option_context_add_group (context, gst_init_get_option_group ());
  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_print ("option parsing failed: %s", error->message);
    exit (1);
  }
  g_option_context_free (context);

  server = gss_server_new ();

  if (enable_daemon)
    daemonize ();

  gss_server_set_title (server, "Entropy Wave Streaming Server");
  gss_server_set_footer_html (server, footer_html, NULL);

#ifdef ENABLE_DEBUG
#define REALM "Entropy Wave E1000"
  {
    GssSession *session;
    char *s;

    s = soup_auth_domain_digest_encode_password ("admin", REALM, "admin");
    g_object_set (server, "admin-token", s, NULL);
    g_free (s);

    session = gss_session_new ("permanent");
    g_free (session->session_id);
    session->session_id = g_strdup ("00000000");
    session->permanent = TRUE;
    session->is_admin = TRUE;
  }
#endif

  ew_stream_server_add_admin_callbacks (server);

  gss_server_read_config (server, CONFIG_FILENAME);

  gss_config_set_config_filename (server->config, CONFIG_FILENAME);
  gss_config_load_defaults (server->config, config_defaults);
  gss_config_load_from_file (server->config);
  gss_config_load_from_file_locked (server->config, CONFIG_FILENAME ".perm");
  gss_config_load_from_file_locked (server->config, CONFIG_FILENAME ".package");

  for (i = 0;; i++) {
    char *key;

    key = g_strdup_printf ("stream%d_name", i);
    if (!gss_config_exists (server->config, key))
      break;

    add_program (server, i);

    g_free (key);
  }

  //g_timeout_add (1000, periodic_timer, NULL);

  main_loop = g_main_loop_new (NULL, TRUE);

  g_main_loop_run (main_loop);

  g_main_loop_unref (main_loop);
  main_loop = NULL;

  g_object_unref (server);
  server = NULL;

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


static void
add_program (GssServer * server, int i)
{
  GssProgram *program;
  const char *url;
  const char *stream_name;
  const char *stream_type;
  char *key;

  key = g_strdup_printf ("stream%d_name", i);
  stream_name = gss_config_get (server->config, key);
  g_free (key);

  key = g_strdup_printf ("stream%d_type", i);
  stream_type = gss_config_get (server->config, key);
  g_free (key);

  key = g_strdup_printf ("stream%d_url", i);
  url = gss_config_get (server->config, key);
  g_free (key);

  program = gss_server_add_program (server, stream_name);
  if (strcmp (stream_type, "http-follow") == 0) {
    gss_program_http_follow (program, url);
  } else if (strcmp (stream_type, "ew-contrib") == 0) {
    gss_program_ew_contrib (program);
  } else if (strcmp (stream_type, "http-put") == 0) {
    gss_program_http_put (program);
  } else if (strcmp (stream_type, "icecast") == 0) {
    gss_program_icecast (program);
  } else {
    /* ew-follow */
    gss_program_follow (program, url, "stream");
  }

}

static void
footer_html (GssServer * server, GString * s, void *priv)
{
  g_string_append (s,
      "        <div class='span4'>\n"
      "          <p>&copy; Entropy Wave Inc 2012</p>\n"
      "        </div>\n"
      "        <div class='span4'>\n"
      "          <a href='http://entropywave.com'>\n"
      "            <img src='/images/footer-entropywave.png'>\n"
      "          </a>\n" "        </div>\n");
}
