
#include "config.h"

#include <gst/gst.h>
#include <libsoup/soup.h>
#include <gst-stream-server/gss-server.h>

#if 0
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#define GETTEXT_PACKAGE NULL

gboolean verbose = FALSE;
char *hostname = NULL;

void handle_pipeline_message (GstBus *bus, GstMessage *message,
    gpointer user_data);
GstElement * create_pipeline (int i);
void init_server (void);
gboolean timeout (gpointer data);
void client_removed (GstElement *e, int arg0, int arg1,
    gpointer user_data);

GMainLoop *main_loop;

int listen_socket;

static GOptionEntry entries[] =
{
  { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Be verbose", NULL },

  { "hostname", 'h', 0, G_OPTION_ARG_STRING, &hostname, "Hostname to use in URLs", NULL },

  { NULL }
};

int
main (int argc, char *argv[])
{
  GError *error = NULL;
  GOptionContext *context;
  EwServer *server;
  EwProgram *program;
  int i;

  if (!g_thread_supported ()) g_thread_init(NULL);

  context = g_option_context_new ("[HOSTS] - stream server relay");
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  g_option_context_add_group (context, gst_init_get_option_group ());
  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_print ("option parsing failed: %s\n", error->message);
    exit (1);
  }
  g_option_context_free (context);

  if (argc < 2) {
    g_print ("expected addresses of hosts to follow\n");
    exit (1);
  }

  server = ew_server_new ();

  if (hostname) {
    ew_server_set_hostname (server, hostname);
  }

  for(i=1;i<argc;i++){
    char *stream_name;
    if (i == 1) {
      stream_name = g_strdup ("stream");
    } else {
      stream_name = g_strdup_printf ("stream%d", i-1);
    }
    program = ew_server_add_program (server, stream_name);
    ew_program_follow (program, argv[i], "stream");
    g_free (stream_name);
  }

  main_loop = g_main_loop_new(NULL, TRUE);

  g_timeout_add (1000, (GSourceFunc)timeout, server);

  g_main_loop_run(main_loop);

  ew_server_free (server);
  ew_server_deinit();
  gst_deinit();
  
  return 0;
}



gboolean
event_probe_callback (GstPad *pad, GstMiniObject *mini_obj, gpointer user_data)
{
  GstEvent *event;

  event = GST_EVENT (mini_obj);

  g_print("event %s\n", gst_event_type_get_name (event->type));

  return TRUE;
}


gboolean
timeout (gpointer data)
{
  EwServer *server = (EwServer *) data;

#if 0
  {
    static int countdown = 90;
    if (countdown-- == 0) {
      g_main_loop_quit (main_loop);
    }
  }
#endif

  if (0 && verbose) {
    int n_clients = 0;
    int i;
    EwProgram *program = server->programs[0];

    for(i=0;i<program->n_streams;i++){
      n_clients += program->streams[i]->n_clients;
    }
    g_print ("clients: %d\n", n_clients);
  }

  return TRUE;
}

void
ew_server_add_admin_callbacks (EwServer *server, SoupServer *soupserver)
{
}

