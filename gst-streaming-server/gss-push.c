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

#include "gss-push.h"
#include "gss-server.h"
#include "gss-html.h"
#include "gss-session.h"
#include "gss-soup.h"
#include "gss-content.h"
#include "gss-utils.h"

enum
{
  PROP_NONE,
  PROP_PUSH_URI,
  PROP_PUSH_METHOD,
  PROP_DEFAULT_TYPE
};

#define DEFAULT_PUSH_URI "http://example.com/stream.webm"
#define DEFAULT_PUSH_METHOD GSS_PUSH_METHOD_HTTP_PUT
#define DEFAULT_DEFAULT_TYPE GSS_STREAM_TYPE_WEBM


static void gss_push_finalize (GObject * object);
static void gss_push_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gss_push_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void handle_pipeline_message (GstBus * bus, GstMessage * message,
    gpointer user_data);

static void gss_stream_create_push_pipeline (GssStream * stream);
static void gss_push_add_resources (GssProgram * program);
static char *gss_push_get_push_uri (GssPush * push);
static void gss_push_start (GssProgram * program);
static void gss_push_stop (GssProgram * program);

static GssProgramClass *parent_class;

static GType
gss_push_method_get_type (void)
{
  static gsize id = 0;
  static const GEnumValue values[] = {
    {GSS_PUSH_METHOD_HTTP_PUT, "http-put", "http-put"},
    {GSS_PUSH_METHOD_TCP_SOCKET, "tcp-socket", "tcp-socket"},
    {GSS_PUSH_METHOD_ICECAST, "icecast", "icecast"},
    {0, NULL, NULL}
  };

  if (g_once_init_enter (&id)) {
    GType tmp = g_enum_register_static ("GssPushMethod", values);
    g_once_init_leave (&id, tmp);
  }

  return (GType) id;
}

#if 0
static const char *
gss_push_method_get_name (GssPushMethod method)
{
  GEnumValue *ev;

  ev = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
          (gss_push_method_get_type ())), method);
  if (ev == NULL)
    return NULL;

  return ev->value_name;
}
#endif

G_DEFINE_TYPE (GssPush, gss_push, GSS_TYPE_PROGRAM);

static void
gss_push_init (GssPush * push)
{
  push->push_uri = g_strdup (DEFAULT_PUSH_URI);
  push->push_method = DEFAULT_PUSH_METHOD;
  push->default_type = DEFAULT_DEFAULT_TYPE;
}

static void
gss_push_class_init (GssPushClass * push_class)
{
  G_OBJECT_CLASS (push_class)->set_property = gss_push_set_property;
  G_OBJECT_CLASS (push_class)->get_property = gss_push_get_property;
  G_OBJECT_CLASS (push_class)->finalize = gss_push_finalize;

  g_object_class_install_property (G_OBJECT_CLASS (push_class),
      PROP_PUSH_METHOD, g_param_spec_enum ("push-method", "Push Method",
          "Push method.", gss_push_method_get_type (), DEFAULT_PUSH_METHOD,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (G_OBJECT_CLASS (push_class),
      PROP_PUSH_URI, g_param_spec_string ("push-uri", "Push URI",
          "URI for the stream or program to push to.", DEFAULT_PUSH_URI,
          (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (G_OBJECT_CLASS (push_class),
      PROP_DEFAULT_TYPE, g_param_spec_enum ("default-type",
          "Default Stream Format", "Default Stream Format",
          gss_stream_type_get_type (), DEFAULT_DEFAULT_TYPE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  GSS_PROGRAM_CLASS (push_class)->add_resources = gss_push_add_resources;
  GSS_PROGRAM_CLASS (push_class)->start = gss_push_start;
  GSS_PROGRAM_CLASS (push_class)->stop = gss_push_stop;

  parent_class = g_type_class_peek_parent (push_class);
}

static void
gss_push_finalize (GObject * object)
{
  GssPush *push = GSS_PUSH (object);

  g_free (push->push_uri);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gss_push_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GssPush *push;

  push = GSS_PUSH (object);

  switch (prop_id) {
    case PROP_PUSH_METHOD:
      push->push_method = g_value_get_enum (value);
      break;
    case PROP_DEFAULT_TYPE:
      push->default_type = g_value_get_enum (value);
      break;
    default:
      g_assert_not_reached ();
      break;
  }
}

static void
gss_push_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GssPush *push;

  push = GSS_PUSH (object);

  switch (prop_id) {
    case PROP_PUSH_METHOD:
      g_value_set_enum (value, push->push_method);
      break;
    case PROP_PUSH_URI:
      g_value_take_string (value, gss_push_get_push_uri (push));
      break;
    case PROP_DEFAULT_TYPE:
      g_value_set_enum (value, push->default_type);
      break;
    default:
      g_assert_not_reached ();
      break;
  }
}


GssProgram *
gss_push_new (void)
{
  return g_object_new (GSS_TYPE_PUSH, NULL);
}

static char *
gss_push_get_push_uri (GssPush * push)
{
  return g_strdup_printf ("%s/%s", push->program.server->base_url,
      GSS_OBJECT_NAME (push));
}

static void
gss_push_stop (GssProgram * program)
{
  GssPush *push = GSS_PUSH (program);
  GssStream *stream;

  push->push_client = NULL;

  if (program->streams) {
    stream = program->streams->data;

    gss_program_remove_stream (program, stream);
  }
}

void
gss_push_start (GssProgram * program)
{
  //GssPush *push = GSS_PUSH (program);

  /* do stuff */
}


static gboolean
push_data_probe_callback (GstPad * pad, GstMiniObject * mo, gpointer user_data)
{

  return TRUE;
}

static void
gss_stream_create_push_pipeline (GssStream * stream)
{
  GstElement *pipe;
  GstElement *e;
  GString *pipe_desc;
  GError *error = NULL;
  GstBus *bus;
  GssPush *push = GSS_PUSH (stream->program);

  pipe_desc = g_string_new ("");

  if (push->push_method == GSS_PUSH_METHOD_ICECAST) {
    g_string_append_printf (pipe_desc, "fdsrc name=src do-timestamp=true ! ");
  } else {
    g_string_append_printf (pipe_desc, "appsrc name=src do-timestamp=true ! ");
  }
  switch (stream->type) {
    case GSS_STREAM_TYPE_OGG_THEORA_VORBIS:
      g_string_append (pipe_desc, "oggparse name=parse ! ");
      break;
    case GSS_STREAM_TYPE_M2TS_H264BASE_AAC:
    case GSS_STREAM_TYPE_M2TS_H264MAIN_AAC:
      g_string_append (pipe_desc, "mpegtsparse name=parse ! ");
      break;
    case GSS_STREAM_TYPE_WEBM:
      g_string_append (pipe_desc, "matroskaparse name=parse ! ");
      break;
    case GSS_STREAM_TYPE_FLV_H264BASE_AAC:
      g_string_append (pipe_desc, "flvparse name=parse ! ");
      break;
    default:
      g_assert_not_reached ();
      break;
  }
  g_string_append (pipe_desc, "queue ! ");
  g_string_append_printf (pipe_desc, "%s name=sink ",
      gss_server_get_multifdsink_string ());

  GST_DEBUG ("pipeline: %s", pipe_desc->str);
  error = NULL;
  pipe = gst_parse_launch (pipe_desc->str, &error);
  if (error != NULL) {
    GST_WARNING ("pipeline parse error: %s", error->message);
    g_error_free (error);
    return;
  }
  g_string_free (pipe_desc, TRUE);

  e = gst_bin_get_by_name (GST_BIN (pipe), "src");
  g_assert (e != NULL);
  if (push->push_method == GSS_PUSH_METHOD_ICECAST) {
    g_object_set (e, "fd", stream->push_fd, NULL);
  }
  stream->src = e;
  gst_pad_add_data_probe (gst_element_get_pad (e, "src"),
      G_CALLBACK (push_data_probe_callback), stream);

  e = gst_bin_get_by_name (GST_BIN (pipe), "sink");
  g_assert (e != NULL);
  gss_stream_set_sink (stream, e);
  g_object_unref (e);
  stream->pipeline = pipe;

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipe));
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (handle_pipeline_message),
      stream);
  g_object_unref (bus);

}

static void
handle_pipeline_message (GstBus * bus, GstMessage * message, gpointer user_data)
{
  GssStream *stream = user_data;
  GssProgram *program = stream->program;

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
          && message->src == GST_OBJECT (stream->pipeline)) {
        char *s;
        s = g_strdup_printf ("stream %s started", GSS_OBJECT_NAME (stream));
        GST_DEBUG_OBJECT (program, s);
        g_free (s);
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

      program->restart_delay = 5;
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

static void
push_wrote_headers (SoupMessage * msg, void *user_data)
{
  GssStream *stream = (GssStream *) user_data;
  SoupSocket *socket;

  socket =
      soup_client_context_get_socket (GSS_PUSH (stream->program)->push_client);
  if (socket == NULL) {
    GST_WARNING_OBJECT (stream, "Push socket is NULL");
    return;
  }
  stream->push_fd = soup_socket_get_fd (socket);

  gss_stream_create_push_pipeline (stream);

  gst_element_set_state (stream->pipeline, GST_STATE_PLAYING);
}

static void
gss_push_get_resource (GssTransaction * t)
{
  GssProgram *program = (GssProgram *) t->resource->priv;
  GString *s = g_string_new ("");

  t->s = s;

  gss_html_header (t);

  GSS_P ("<h1>%s</h1>\n", GSS_OBJECT_NAME (program));

  gss_program_add_video_block (program, t, 0);

  GSS_A ("<br>");

  gss_program_add_stream_table (program, s);

  if (t->session && t->session->is_admin) {
    gss_config_append_config_block (G_OBJECT (program), t, FALSE);
  }

  gss_html_footer (t);
}

static void
gss_push_put_resource (GssTransaction * t)
{
  GssProgram *program = (GssProgram *) t->resource->priv;
  GssPush *push = GSS_PUSH (t->resource->priv);
  const char *content_type;
  GssStream *stream;
  gboolean is_icecast;

  if (push->push_client && push->push_method == GSS_PUSH_METHOD_ICECAST) {
    GST_DEBUG_OBJECT (program, "busy");
    soup_message_set_status (t->msg, SOUP_STATUS_CONFLICT);
    return;
  }

  is_icecast = FALSE;
  if (soup_message_headers_get_one (t->msg->request_headers, "ice-name")) {
    is_icecast = TRUE;
  }

  content_type = soup_message_headers_get_one (t->msg->request_headers,
      "Content-Type");
  if (content_type) {
    GST_DEBUG_OBJECT (push, "content_type %s", content_type);
    if (strcmp (content_type, "application/ogg") == 0) {
      push->push_media_type = GSS_STREAM_TYPE_OGG_THEORA_VORBIS;
    } else if (strcmp (content_type, "video/webm") == 0) {
      push->push_media_type = GSS_STREAM_TYPE_WEBM;
    } else if (strcmp (content_type, "video/mpeg-ts") == 0) {
      push->push_media_type = GSS_STREAM_TYPE_M2TS_H264BASE_AAC;
    } else if (strcmp (content_type, "video/mp2t") == 0) {
      push->push_media_type = GSS_STREAM_TYPE_M2TS_H264MAIN_AAC;
    } else if (strcmp (content_type, "video/x-flv") == 0) {
      push->push_media_type = GSS_STREAM_TYPE_FLV_H264BASE_AAC;
    } else {
      push->push_media_type = GSS_STREAM_TYPE_OGG_THEORA_VORBIS;
    }
  } else {
    push->push_media_type = push->default_type;
  }

  if (push->push_client == NULL) {
    if (is_icecast) {
      push->push_method = GSS_PUSH_METHOD_ICECAST;
    } else {
      push->push_method = GSS_PUSH_METHOD_HTTP_PUT;
    }

    if (program->streams == NULL) {
      stream = gss_program_add_stream_full (GSS_PROGRAM (push),
          push->push_media_type, 640, 360, 600000, NULL);
    }
    stream = program->streams->data;

    if (!is_icecast) {
      gss_stream_create_push_pipeline (stream);

      gst_element_set_state (stream->pipeline, GST_STATE_PLAYING);
    }

    gss_program_start (GSS_PROGRAM (push));

    push->push_client = t->client;
  }

  /* FIXME the user should specify a stream */
  stream = program->streams->data;

  if (is_icecast) {
    soup_message_headers_set_encoding (t->msg->response_headers,
        SOUP_ENCODING_EOF);

    g_signal_connect (t->msg, "wrote-headers", G_CALLBACK (push_wrote_headers),
        stream);
  } else {
    if (t->msg->request_body) {
      GstBuffer *buffer;
      GstFlowReturn flow_ret;

      buffer = gst_buffer_new_and_alloc (t->msg->request_body->length);
      memcpy (GST_BUFFER_DATA (buffer), t->msg->request_body->data,
          t->msg->request_body->length);

      g_signal_emit_by_name (stream->src, "push-buffer", buffer, &flow_ret);
      gst_buffer_unref (buffer);
    }
  }

  soup_message_set_status (t->msg, SOUP_STATUS_OK);
}

static void
gss_push_add_resources (GssProgram * program)
{
  char *s;

  parent_class->add_resources (program);

  s = g_strdup_printf ("/%s", GSS_OBJECT_NAME (program));
  program->resource =
      gss_server_add_resource (program->server, s, GSS_RESOURCE_UI, "text/html",
      gss_push_get_resource, gss_push_put_resource, gss_config_post_resource,
      program);
  g_free (s);
}
