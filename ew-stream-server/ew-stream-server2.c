/* EwEncoder
 * Copyright (C) 2011 David Schleef <ds@schleef.org>
 * Copyright (C) 2010 Entropy Wave Inc
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ew-stream-server2.h"

#include <gst/gst.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>


#define GETTEXT_PACKAGE "ew-streaming-server"

//#define CONFIG_FILENAME "/opt/entropywave/ew-oberon/config"
#define CONFIG_FILENAME "config"

extern EwConfigDefault config_defaults[];

gboolean verbose;
gboolean cl_verbose;

static void signal_interrupt (int signum);

static GOptionEntry entries[] = {
  {"verbose", 'v', 0, G_OPTION_ARG_NONE, &cl_verbose, "Be verbose", NULL},

  {NULL}

};

#define N_ENCODERS 1
EwServer *server;
GMainLoop *main_loop;

int
main (int argc, char *argv[])
{
  GError *error = NULL;
  GOptionContext *context;

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

  server = ew_server_new ();
  ew_server_read_config (server, CONFIG_FILENAME);

  ew_config_set_config_filename (server->config, CONFIG_FILENAME);
  ew_config_load_defaults (server->config, config_defaults);
  ew_config_load_from_file (server->config);
  ew_config_load_from_file_locked (server->config, CONFIG_FILENAME ".perm");
  ew_config_load_from_file_locked (server->config, CONFIG_FILENAME ".package");

  //g_timeout_add (1000, periodic_timer, NULL);

  main_loop = g_main_loop_new (NULL, TRUE);

  g_main_loop_run (main_loop);

  ew_server_free (server);
  ew_server_deinit();
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



