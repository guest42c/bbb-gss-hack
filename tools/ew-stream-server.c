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
#include "gst-streaming-server/gss-user.h"
#include "gst-streaming-server/gss-manager.h"
#include "gst-streaming-server/gss-push.h"

#include <gst/gst.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>


#define GETTEXT_PACKAGE "ew-stream-server"

#define CONFIG_FILENAME "config"

#define LOG g_print

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
GssUser *user;
GssManager *manager;
GMainLoop *main_loop;

static void G_GNUC_NORETURN
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

#if !GLIB_CHECK_VERSION (2, 31, 0)
  if (!g_thread_supported ())
    g_thread_init (NULL);
#endif

  signal (SIGPIPE, SIG_IGN);
  signal (SIGINT, signal_interrupt);

  context = g_option_context_new ("- GStreamer Streaming Server");
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  g_option_context_add_group (context, gst_init_get_option_group ());
  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_print ("option parsing failed: %s", error->message);
    exit (1);
  }
  g_option_context_free (context);

  server = gss_server_new ();
  gst_object_set_name (GST_OBJECT (server), "admin.server");

  if (enable_daemon)
    daemonize ();

  if (server->server == NULL) {
    g_print ("failed to create HTTP server\n");
    exit (1);
  }
  if (server->ssl_server == NULL) {
    g_print ("failed to create HTTPS server\n");
    exit (1);
  }

  gss_server_set_title (server, "GStreamer Streaming Server");
  gss_server_set_footer_html (server, footer_html, NULL);

  //ew_stream_server_add_admin_callbacks (server);

  gss_config_attach (G_OBJECT (server));
  gss_config_add_server_resources (server);

  user = gss_user_new ();
  gss_config_attach (G_OBJECT (user));
  gss_user_add_resources (user, server);

  manager = gss_manager_new ();
  gss_config_attach (G_OBJECT (manager));
  gss_manager_add_resources (manager, server);

#define ENABLE_DEBUG
#ifdef ENABLE_DEBUG
#define REALM "GStreamer Streaming Server"
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

  for (i = 0; i < 1; i++) {
    char *key;

    key = g_strdup_printf ("stream%d", i);
#if 0
    if (!gss_config_exists (server->config, key))
      break;
#endif

    add_program (server, i);

    g_free (key);
  }

  gss_config_load_config_file ();

  gss_config_save_config_file ();


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
  char *stream_name;

  stream_name = g_strdup_printf ("stream%d", i);
  program = gss_push_new ();
  g_object_set (program, "name", stream_name, NULL);
  gss_server_add_program_simple (server, program);
  g_free (stream_name);
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
