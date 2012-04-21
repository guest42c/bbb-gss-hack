/* ew-stream-server
 * Copyright (C) 2011 David Schleef <ds@schleef.org>
 * Copyright (C) 2010 Entropy Wave Inc
 *
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

  for(i=0;i<N_PROGRAMS;i++){
    char *key;

    key = g_strdup_printf("stream%d_url", i);

    gss_config_set_notify (server->config, key,
        ew_stream_server_notify_url, GINT_TO_POINTER (i));

    ew_stream_server_notify_url (key, GINT_TO_POINTER (i));

    g_free (key);
  }

  //g_timeout_add (1000, periodic_timer, NULL);

  main_loop = g_main_loop_new (NULL, TRUE);

  g_main_loop_run (main_loop);

  gss_server_free (server);
  gss_server_deinit();
  gst_deinit();

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

  if (url[0] == 0 && programs[i] == NULL) return;
  if (programs[i] && strcmp(url, programs[i]->follow_host) == 0) return;

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


