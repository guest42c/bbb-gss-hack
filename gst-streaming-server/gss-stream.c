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

#include "gss-server.h"
#include "gss-html.h"
#include "gss-session.h"
#include "gss-soup.h"
#ifdef ENABLE_RTSP
#include "gss-rtsp.h"
#endif
#include "gss-content.h"
#include "gss-utils.h"

enum
{
  PROP_TYPE = 1,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_BITRATE
};

#define DEFAULT_TYPE GSS_STREAM_TYPE_WEBM
#define DEFAULT_WIDTH 640
#define DEFAULT_HEIGHT 360
#define DEFAULT_BITRATE 600000



static void msg_wrote_headers (SoupMessage * msg, void *user_data);


static void gss_stream_finalize (GObject * object);
static void gss_stream_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gss_stream_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GObjectClass *parent_class;


G_DEFINE_TYPE (GssStream, gss_stream, GST_TYPE_OBJECT);

static void
gss_stream_init (GssStream * stream)
{

  stream->metrics = gss_metrics_new ();

  stream->type = DEFAULT_TYPE;
  gss_stream_set_type (stream, DEFAULT_TYPE);
  stream->width = DEFAULT_WIDTH;
  stream->height = DEFAULT_HEIGHT;
  stream->bitrate = DEFAULT_BITRATE;
}

GType
gss_stream_type_get_type (void)
{
  static gsize id = 0;
  static const GEnumValue values[] = {
    {GSS_STREAM_TYPE_OGG_THEORA_VORBIS, "ogg-theora-vorbis",
        "Ogg/Theora/Vorbis"},
    {GSS_STREAM_TYPE_WEBM, "webm", "WebM (Matroska/VP8/Vorbis)"},
    {GSS_STREAM_TYPE_M2TS_H264BASE_AAC, "m2ts-h264base-aac",
        "MPEG-TS/H.264 Baseline/AAC"},
    {GSS_STREAM_TYPE_M2TS_H264MAIN_AAC, "m2ts-h264main-aac",
        "MPEG-TS/H.264 Main/AAC"},
    {GSS_STREAM_TYPE_FLV_H264BASE_AAC, "flv-h264base-aac",
        "Flash/H.264 Baseline/AAC"},
    {GSS_STREAM_TYPE_OGG_THEORA_OPUS, "ogg-theora-opus", "Ogg/Theora/Opus"},
    {0, NULL, NULL}
  };

  if (g_once_init_enter (&id)) {
    GType tmp = g_enum_register_static ("GssStreamType", values);
    g_once_init_leave (&id, tmp);
  }

  return (GType) id;
}

static void
gss_stream_class_init (GssStreamClass * stream_class)
{
  G_OBJECT_CLASS (stream_class)->set_property = gss_stream_set_property;
  G_OBJECT_CLASS (stream_class)->get_property = gss_stream_get_property;
  G_OBJECT_CLASS (stream_class)->finalize = gss_stream_finalize;

  g_object_class_install_property (G_OBJECT_CLASS (stream_class),
      PROP_TYPE, g_param_spec_enum ("type", "Stream Format",
          "Stream Format", gss_stream_type_get_type (), DEFAULT_TYPE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (G_OBJECT_CLASS (stream_class),
      PROP_WIDTH, g_param_spec_int ("width", "Width",
          "Width", 0, 3840, DEFAULT_WIDTH,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (G_OBJECT_CLASS (stream_class),
      PROP_HEIGHT, g_param_spec_int ("height", "Height",
          "Height", 0, 2160, DEFAULT_HEIGHT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (G_OBJECT_CLASS (stream_class),
      PROP_BITRATE, g_param_spec_int ("bitrate", "Bit Rate",
          "[bits/sec] Bit Rate", 0, G_MAXINT, DEFAULT_BITRATE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  parent_class = g_type_class_peek_parent (stream_class);
}

static void
gss_stream_finalize (GObject * object)
{
  GssStream *stream = GSS_STREAM (object);
  int i;

  g_free (stream->playlist_location);
  g_free (stream->location);
  g_free (stream->codecs);
  g_free (stream->follow_url);

  for (i = 0; i < GSS_STREAM_HLS_CHUNKS; i++) {
    GssHLSSegment *segment = &stream->chunks[i];

    if (segment->buffer) {
      soup_buffer_free (segment->buffer);
      g_free (segment->location);
    }
  }

  if (stream->hls.index_buffer) {
    soup_buffer_free (stream->hls.index_buffer);
  }
#define CLEANUP(x) do { \
  if (x) { \
    if (GST_OBJECT_REFCOUNT (x) != 1) \
      GST_WARNING( #x " refcount %d", GST_OBJECT_REFCOUNT (x)); \
    g_object_unref (x); \
  } \
} while (0)

  gss_stream_set_sink (stream, NULL);
  CLEANUP (stream->src);
  CLEANUP (stream->sink);
  CLEANUP (stream->adapter);
  CLEANUP (stream->rtsp_stream);
  if (stream->pipeline) {
    gst_element_set_state (GST_ELEMENT (stream->pipeline), GST_STATE_NULL);
    CLEANUP (stream->pipeline);
  }
  gss_metrics_free (stream->metrics);

  parent_class->finalize (object);
}

static void
gss_stream_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GssStream *stream;

  stream = GSS_STREAM (object);

  switch (prop_id) {
    case PROP_TYPE:
      gss_stream_set_type (stream, g_value_get_enum (value));
      break;
    case PROP_WIDTH:
      stream->width = g_value_get_int (value);
      break;
    case PROP_HEIGHT:
      stream->height = g_value_get_int (value);
      break;
    case PROP_BITRATE:
      stream->bitrate = g_value_get_int (value);
      break;
    default:
      g_assert_not_reached ();
      break;
  }
}

static void
gss_stream_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GssStream *stream;

  stream = GSS_STREAM (object);

  switch (prop_id) {
    case PROP_TYPE:
      g_value_set_enum (value, stream->type);
      break;
    case PROP_WIDTH:
      g_value_set_int (value, stream->width);
      break;
    case PROP_HEIGHT:
      g_value_set_int (value, stream->height);
      break;
    case PROP_BITRATE:
      g_value_set_int (value, stream->bitrate);
      break;
    default:
      g_assert_not_reached ();
      break;
  }
}

const char *
gss_stream_type_get_name (GssStreamType type)
{
  GEnumValue *value;

  value =
      g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
          (gss_stream_type_get_type ())), type);
  g_return_val_if_fail (value != NULL, NULL);

  return value->value_name;
}

const char *
gss_stream_type_get_id (GssStreamType type)
{
  GEnumValue *value;

  value =
      g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
          (gss_stream_type_get_type ())), type);
  g_return_val_if_fail (value != NULL, NULL);

  return value->value_nick;
}

GssStreamType
gss_stream_type_from_id (const char *id)
{
  GEnumValue *value;

  value =
      g_enum_get_value_by_nick (G_ENUM_CLASS (g_type_class_peek
          (gss_stream_type_get_type ())), id);
  if (value)
    return value->value;

  /* some legacy values */
  if (strcmp (id, "ogg") == 0) {
    return GSS_STREAM_TYPE_OGG_THEORA_VORBIS;
  } else if (strcmp (id, "webm") == 0) {
    return GSS_STREAM_TYPE_WEBM;
  } else if (strcmp (id, "mpeg-ts") == 0) {
    return GSS_STREAM_TYPE_M2TS_H264BASE_AAC;
  } else if (strcmp (id, "mpeg-ts-main") == 0) {
    return GSS_STREAM_TYPE_M2TS_H264MAIN_AAC;
  } else if (strcmp (id, "flv") == 0) {
    return GSS_STREAM_TYPE_FLV_H264BASE_AAC;
  }

  return GSS_STREAM_TYPE_UNKNOWN;
}


void
gss_stream_set_type (GssStream * stream, int type)
{
  g_return_if_fail (GSS_IS_STREAM (stream));

  stream->type = type;
}

const char *
gss_stream_type_get_ext (int type)
{
  switch (type) {
    case GSS_STREAM_TYPE_UNKNOWN:
      return "";
    case GSS_STREAM_TYPE_OGG_THEORA_VORBIS:
    case GSS_STREAM_TYPE_OGG_THEORA_OPUS:
      return "ogv";
    case GSS_STREAM_TYPE_WEBM:
      return "webm";
    case GSS_STREAM_TYPE_M2TS_H264BASE_AAC:
    case GSS_STREAM_TYPE_M2TS_H264MAIN_AAC:
      return "ts";
    case GSS_STREAM_TYPE_FLV_H264BASE_AAC:
      return "flv";
    default:
      g_assert_not_reached ();
      break;
  }


  return "";
}

const char *
gss_stream_type_get_mod (int type)
{
  switch (type) {
    case GSS_STREAM_TYPE_UNKNOWN:
    case GSS_STREAM_TYPE_OGG_THEORA_VORBIS:
    case GSS_STREAM_TYPE_OGG_THEORA_OPUS:
    case GSS_STREAM_TYPE_WEBM:
    case GSS_STREAM_TYPE_M2TS_H264BASE_AAC:
    case GSS_STREAM_TYPE_FLV_H264BASE_AAC:
      return "";
    case GSS_STREAM_TYPE_M2TS_H264MAIN_AAC:
      return "-main";
    default:
      g_assert_not_reached ();
      break;
  }
}

const char *
gss_stream_type_get_content_type (int type)
{
  switch (type) {
    case GSS_STREAM_TYPE_UNKNOWN:
      return "unknown/unknown";
    case GSS_STREAM_TYPE_OGG_THEORA_VORBIS:
    case GSS_STREAM_TYPE_OGG_THEORA_OPUS:
      return "video/ogg";
    case GSS_STREAM_TYPE_WEBM:
      return "video/webm";
    case GSS_STREAM_TYPE_M2TS_H264BASE_AAC:
    case GSS_STREAM_TYPE_M2TS_H264MAIN_AAC:
      return "video/mp2t";
    case GSS_STREAM_TYPE_FLV_H264BASE_AAC:
      return "video/x-flv";
    default:
      g_assert_not_reached ();
      break;
  }

  return "";
}

void
gss_stream_get_stats (GssStream * stream, guint64 * in, guint64 * out)
{
  if (stream->sink) {
    g_object_get (stream->sink, "bytes-to-serve", in, "bytes-served", out,
        NULL);
  } else {
    *in = 0;
    *out = 0;
  }
}

static void
client_removed (GstElement * e, int fd, int status, gpointer user_data)
{
  GssStream *stream = user_data;

  if (gss_stream_fd_table[fd].callback == NULL) {
    if (stream) {
      gss_metrics_remove_client (stream->metrics, stream->bitrate);
      gss_metrics_remove_client (stream->program->metrics, stream->bitrate);
      gss_metrics_remove_client (stream->program->server->metrics,
          stream->bitrate);
    }
  }
}

static void
client_fd_removed (GstElement * e, int fd, gpointer user_data)
{
  GssStream *stream = user_data;

  if (gss_stream_fd_table[fd].callback) {
    gss_stream_fd_table[fd].callback (stream, fd, gss_stream_fd_table[fd].priv);
  } else {
    SoupSocket *sock = gss_stream_fd_table[fd].priv;
    if (sock)
      soup_socket_disconnect (sock);
  }
  gss_stream_fd_table[fd].priv = NULL;
  gss_stream_fd_table[fd].callback = NULL;
}

static void
stream_resource (GssTransaction * t)
{
  GssStream *stream = (GssStream *) t->resource->priv;
  GssConnection *connection;

  if (!stream->program->enable_streaming
      || stream->program->state != GSS_PROGRAM_STATE_RUNNING) {
    soup_message_set_status (t->msg, SOUP_STATUS_NO_CONTENT);
    return;
  }

  if (t->server->metrics->n_clients >= t->server->max_connections ||
      t->server->metrics->bitrate + stream->bitrate >=
      t->server->max_rate * 8000) {
    GST_DEBUG ("n_clients %d max_connections %d\n",
        t->server->metrics->n_clients, t->server->max_connections);
    GST_DEBUG ("current bitrate %" G_GINT64_FORMAT " bitrate %d max_bitrate %d"
        "\n", t->server->metrics->bitrate, stream->bitrate,
        t->server->max_rate * 8000);
    soup_message_set_status (t->msg, SOUP_STATUS_SERVICE_UNAVAILABLE);
    return;
  }

  connection = g_malloc0 (sizeof (GssConnection));
  connection->msg = t->msg;
  connection->client = t->client;
  connection->stream = stream;

  soup_message_set_status (t->msg, SOUP_STATUS_OK);

  soup_message_headers_set_encoding (t->msg->response_headers,
      SOUP_ENCODING_EOF);
  soup_message_headers_replace (t->msg->response_headers, "Content-Type",
      gss_stream_type_get_content_type (stream->type));

  g_signal_connect (t->msg, "wrote-headers", G_CALLBACK (msg_wrote_headers),
      connection);
}

void
gss_stream_add_fd (GssStream * stream, int fd,
    void (*callback) (GssStream * stream, int fd, void *priv), void *priv)
{
  g_return_if_fail (fd < GSS_STREAM_MAX_FDS);

  gss_stream_fd_table[fd].callback = callback;
  gss_stream_fd_table[fd].priv = priv;

  g_signal_emit_by_name (stream->sink, "add", fd);
}

static void
msg_wrote_headers (SoupMessage * msg, void *user_data)
{
  GssConnection *connection = user_data;
  SoupSocket *sock;
  int fd;

  sock = soup_client_context_get_socket (connection->client);
  fd = soup_socket_get_fd (sock);

  if (connection->stream->sink) {
    GssStream *stream = connection->stream;

    gss_stream_add_fd (stream, fd, NULL, sock);

    gss_metrics_add_client (stream->metrics, stream->bitrate);
    gss_metrics_add_client (stream->program->metrics, stream->bitrate);
    gss_metrics_add_client (stream->program->server->metrics, stream->bitrate);
  } else {
    soup_socket_disconnect (sock);
  }

  g_free (connection);
}

GssStream *
gss_stream_new (int type, int width, int height, int bitrate)
{
  return g_object_new (GSS_TYPE_STREAM, "type", type,
      "width", width, "height", height, "bitrate", bitrate, NULL);
}

GssStream *
gss_program_add_stream_full (GssProgram * program,
    int type, int width, int height, int bitrate, GstElement * sink)
{
  GssStream *stream;

  stream = gss_stream_new (type, width, height, bitrate);

  gss_program_add_stream (program, stream);

  /* FIXME this should be called before adding the stream, but it fails */
  gss_stream_set_sink (stream, sink);

  return stream;
}

void
gss_stream_add_resources (GssStream * stream)
{

#ifdef ENABLE_RTSP
  if (stream->program->server->enable_rtsp) {
    if (stream->type == GSS_STREAM_TYPE_OGG_THEORA_VORBIS) {
      stream->rtsp_stream = gss_rtsp_stream_new (stream);
      gss_rtsp_stream_start (stream->rtsp_stream);
    }
  }
#endif

  gss_stream_remove_resources (stream);

  g_free (stream->location);
  stream->location = g_strdup_printf ("/%s/streams/stream%d-%dx%d-%dkbps%s.%s",
      GST_OBJECT_NAME (stream->program),
      gss_program_get_stream_index (stream->program, stream),
      stream->width, stream->height,
      stream->bitrate / 1000,
      gss_stream_type_get_mod (stream->type),
      gss_stream_type_get_ext (stream->type));
  stream->resource = gss_server_add_resource (stream->program->server,
      stream->location, GSS_RESOURCE_HTTP_ONLY,
      gss_stream_type_get_content_type (stream->type),
      stream_resource, NULL, NULL, stream);

  g_free (stream->playlist_location);
  stream->playlist_location =
      g_strdup_printf ("/%s/streams/stream%d-%dx%d-%dkbps%s-%s.m3u8",
      GST_OBJECT_NAME (stream->program),
      gss_program_get_stream_index (stream->program, stream),
      stream->width, stream->height, stream->bitrate / 1000,
      gss_stream_type_get_mod (stream->type),
      gss_stream_type_get_ext (stream->type));
  stream->playlist_resource =
      gss_server_add_resource (stream->program->server,
      stream->playlist_location, 0, "application/x-mpegurl",
      gss_stream_handle_m3u8, NULL, NULL, stream);

  return;
}

void
gss_stream_remove_resources (GssStream * stream)
{
  if (stream->resource)
    gss_server_remove_resource (stream->program->server,
        stream->resource->location);
  if (stream->playlist_resource)
    gss_server_remove_resource (stream->program->server,
        stream->playlist_resource->location);
}

void
gss_stream_set_sink (GssStream * stream, GstElement * sink)
{
  if (stream->sink) {
    g_object_unref (stream->sink);
  }

  stream->sink = sink;
  if (stream->sink) {
    g_object_ref (stream->sink);
    g_signal_connect (stream->sink, "client-removed",
        G_CALLBACK (client_removed), stream);
    g_signal_connect (stream->sink, "client-fd-removed",
        G_CALLBACK (client_fd_removed), stream);
    if (stream->type == GSS_STREAM_TYPE_M2TS_H264BASE_AAC ||
        stream->type == GSS_STREAM_TYPE_M2TS_H264MAIN_AAC) {
      gss_stream_add_hls (stream);
    }
  }
}
