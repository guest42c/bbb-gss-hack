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

#include "gst-streaming-server/gss-server.h"
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

//#define STREAM_TYPE GSS_STREAM_TYPE_OGG_THEORA_VORBIS
#define STREAM_TYPE GSS_STREAM_TYPE_WEBM


#define GETTEXT_PACKAGE "ew-stream-server"

#define CONFIG_FILENAME "config"

#define LOG g_print

gboolean verbose = TRUE;
gboolean cl_verbose;
gboolean enable_daemon = FALSE;

static GssProgram *gss_vts_new (GssServer * server, const char *name);
void ew_stream_server_notify_url (const char *s, void *priv);

static void signal_interrupt (int signum);
static void footer_html (GssServer * server, GString * s, void *priv);

static void
handle_pipeline_message (GstBus * bus, GstMessage * message,
    gpointer user_data);


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

#if !GLIB_CHECK_VERSION (2, 31, 0)
  if (!g_thread_supported ())
    g_thread_init (NULL);
#endif

  signal (SIGPIPE, SIG_IGN);
  signal (SIGINT, signal_interrupt);

  context = g_option_context_new ("- Videotestsrc Example");
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

  gss_server_set_title (server, "Videotestsrc Example");
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

  gss_vts_new (server, "vts-stream");

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




#define GSS_TYPE_VTS \
  (gss_vts_get_type())
#define GSS_VTS(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GSS_TYPE_VTS,GssVts))
#define GSS_VTS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GSS_TYPE_VTS,GssVtsClass))
#define GSS_IS_VTS(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GSS_TYPE_VTS))
#define GSS_IS_VTS_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GSS_TYPE_VTS))


typedef struct _GssVts GssVts;
typedef struct _GssVtsClass GssVtsClass;
typedef struct _GssOutput GssOutput;


struct _GssVts
{
  GssProgram program;

  /* properties */
  int pattern;

  GstElement *pipeline;
  GssStream *stream;
};

struct _GssVtsClass
{
  GssProgramClass program_class;
};

GType gss_vts_get_type (void);

enum
{
  PROP_0,
};

#define DEFAULT_PATTERN 0

static void gss_vts_finalize (GObject * object);
static void gss_vts_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gss_vts_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gss_vts_start (GssProgram * program);
static void gss_vts_stop (GssProgram * program);


static GObjectClass *parent_class;


G_DEFINE_TYPE (GssVts, gss_vts, GSS_TYPE_PROGRAM);

static void
gss_vts_init (GssVts * vts)
{

  vts->pattern = DEFAULT_PATTERN;

}

static void
gss_vts_class_init (GssVtsClass * vts_class)
{
  G_OBJECT_CLASS (vts_class)->set_property = gss_vts_set_property;
  G_OBJECT_CLASS (vts_class)->get_property = gss_vts_get_property;
  G_OBJECT_CLASS (vts_class)->finalize = gss_vts_finalize;

  GSS_PROGRAM_CLASS (vts_class)->start = gss_vts_start;
  GSS_PROGRAM_CLASS (vts_class)->stop = gss_vts_stop;

  parent_class = g_type_class_peek_parent (vts_class);

}

static GssProgram *
gss_vts_new (GssServer * server, const char *name)
{
  GssProgram *program;

  program = g_object_new (GSS_TYPE_VTS, NULL);

  g_object_set (program, "name", "vts-stream", NULL);
  gss_server_add_program_simple (server, program);
  GSS_VTS (program)->stream = gss_stream_new (STREAM_TYPE, 640, 360, 600000);
  gss_program_add_stream (program, GSS_VTS (program)->stream);
  g_object_set (program, "enabled", TRUE, NULL);

  return program;
}

static void
gss_vts_finalize (GObject * object)
{
  //GssVts *vts = GSS_VTS (object);

  parent_class->finalize (object);

}

static void
gss_vts_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GssVts *vts;

  vts = GSS_VTS (object);
  (void) vts;

  switch (prop_id) {
    default:
      g_assert_not_reached ();
      break;
  }
}

static void
gss_vts_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GssVts *vts;

  vts = GSS_VTS (object);
  (void) vts;

  switch (prop_id) {
    default:
      g_assert_not_reached ();
      break;
  }
}

static void
gss_vts_start (GssProgram * program)
{
  GssVts *vts = GSS_VTS (program);
  GstElement *pipe;
  GstElement *e;
  GstBus *bus;
  GError *error = NULL;
  char *s;

#if STREAM_TYPE == GSS_STREAM_TYPE_WEBM
  s = g_strdup_printf ("videotestsrc is-live=true pattern=%d ! "
      "video/x-raw-yuv,format=(fourcc)I420,width=640,height=360 ! "
      "timeoverlay ! "
      "vp8enc ! queue ! "
      "webmmux streamable=true name=mux ! %s name=multifdsink "
      "audiotestsrc is-live=true wave=ticks volume=0.2 ! "
      "audioconvert ! vorbisenc ! "
      "queue ! mux.audio_%%d ",
      vts->pattern, gss_server_get_multifdsink_string ());
#elif STREAM_TYPE == GSS_STREAM_TYPE_OGG_THEORA_VORBIS
  s = g_strdup_printf ("videotestsrc is-live=true pattern=%d ! "
      "video/x-raw-yuv,format=(fourcc)I420,width=640,height=360 ! "
      "timeoverlay ! "
      "theoraenc ! queue ! "
      "oggmux name=mux ! %s name=multifdsink "
      "audiotestsrc is-live=true wave=ticks volume=0.2 ! "
      "audioconvert ! vorbisenc ! "
      "queue ! mux. ", vts->pattern, gss_server_get_multifdsink_string ());
#else
#error FIXME
#endif
  pipe = gst_parse_launch (s, &error);
  if (error) {
    GST_INFO_OBJECT (vts, "pipeline parsing error: %s", error->message);
    g_error_free (error);
    if (pipe)
      g_object_unref (pipe);
    return;
  }

  vts->pipeline = pipe;

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipe));
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (handle_pipeline_message), vts);
  g_object_unref (bus);

  e = gst_bin_get_by_name (GST_BIN (pipe), "multifdsink");
  if (e) {
    gss_stream_set_sink (GSS_STREAM (vts->stream), e);
    g_object_unref (e);
  }

  /* no jpegsink for now */
  //gss_program_set_jpegsink (program, vts->jpegsink);

  gst_element_set_state (vts->pipeline, GST_STATE_PLAYING);

}

static void
gss_vts_stop (GssProgram * program)
{
  //GssVts *vts = GSS_VTS (program);

}

#if 0
static void
gss_vts_add_resources (GssVts * vts)
{

}
#endif

static void
handle_pipeline_message (GstBus * bus, GstMessage * message, gpointer user_data)
{
  GssVts *vts = user_data;
  GssProgram *program = GSS_PROGRAM (vts);

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_STATE_CHANGED:
    {
      GstState newstate;
      GstState oldstate;
      GstState pending;

      gst_message_parse_state_changed (message, &oldstate, &newstate, &pending);

      GST_DEBUG ("message: %s (%s,%s,%s) from %s",
          GST_MESSAGE_TYPE_NAME (message),
          gst_element_state_get_name (newstate),
          gst_element_state_get_name (oldstate),
          gst_element_state_get_name (pending), GST_MESSAGE_SRC_NAME (message));

      if (newstate == GST_STATE_PLAYING
          && message->src == GST_OBJECT (vts->pipeline)) {
        GST_ERROR_OBJECT (program, "vts started");
        gss_program_set_state (program, GSS_PROGRAM_STATE_RUNNING);
      }
    }
      break;
    case GST_MESSAGE_STREAM_STATUS:
    {
      GstStreamStatusType type;
      GstElement *owner;

      gst_message_parse_stream_status (message, &type, &owner);

      GST_DEBUG ("message: %s (%d) from %s", GST_MESSAGE_TYPE_NAME (message),
          type, GST_MESSAGE_SRC_NAME (message));
    }
      break;
    case GST_MESSAGE_ERROR:
    {
      GError *error;
      gchar *debug;
      char *s;

      gst_message_parse_error (message, &error, &debug);

      s = g_strdup_printf ("Internal Error: %s (%s) from %s\n",
          error->message, debug, GST_MESSAGE_SRC_NAME (message));
      GST_DEBUG_OBJECT (program, s);
      g_free (s);

      gss_program_stop (program);
    }
      break;
    case GST_MESSAGE_EOS:
      GST_DEBUG_OBJECT (program, "end of stream");
      gss_program_stop (program);
      break;
    case GST_MESSAGE_ELEMENT:
      break;
    default:
      break;
  }
}
