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


#ifndef _GSS_SERVER_H
#define _GSS_SERVER_H

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <libsoup/soup.h>
#include "gss-config.h"
#include "gss-session.h"

G_BEGIN_DECLS


typedef struct _GssProgram GssProgram;
typedef struct _GssServerStream GssServerStream;
typedef struct _GssServer GssServer;
typedef struct _GssServerClass GssServerClass;
typedef struct _GssConnection GssConnection;
typedef struct _GssHLSSegment GssHLSSegment;
typedef struct _GssRtspStream GssRtspStream;
typedef struct _GssMetrics GssMetrics;

enum {
  GSS_SERVER_STREAM_UNKNOWN,
  GSS_SERVER_STREAM_OGG,
  GSS_SERVER_STREAM_WEBM,
  GSS_SERVER_STREAM_TS,
  GSS_SERVER_STREAM_TS_MAIN,
  GSS_SERVER_STREAM_FLV
};

typedef enum {
  GSS_PROGRAM_EW_FOLLOW,
  GSS_PROGRAM_HTTP_FOLLOW,
  GSS_PROGRAM_HTTP_PUT,
  GSS_PROGRAM_EW_CONTRIB,
  GSS_PROGRAM_ICECAST,
  GSS_PROGRAM_MANUAL
} GssProgramType;

struct _GssMetrics {
  int n_clients;
  int max_clients;
  guint64 bitrate;
  guint64 max_bitrate;
};

struct _GssProgram {
  GssServer *server;

  GssProgramType program_type;

  char *location;
  char *follow_uri;
  char *follow_host;

  SoupClientContext *push_client;
  int push_media_type;

  gboolean running;
  gboolean enable_streaming;

  int n_streams;
  GssServerStream **streams;
  GssMetrics *metrics;

  gboolean enable_ogv;
  gboolean enable_webm;
  gboolean enable_hls;
  gboolean enable_snapshot;
  int restart_delay;

  GstElement *pngappsink;
  GstElement *jpegsink;

  int n_hls_chunks;
  struct {
    SoupBuffer *variant_buffer; /* contents of current variant file */

    int target_duration; /* max length of a chunk (in seconds) */
    gboolean is_encrypted;
    const char *key_uri;
    gboolean have_iv;
    guint32 init_vector[4];
  } hls;
};

struct _GssHLSSegment {
  int index;
  SoupBuffer *buffer;
  char *location;
  int duration;
};

struct _GssServerStream {
  GssProgram *program;
  int index;
  char *name;
  char *codecs;
  char *mime_type;
  char *playlist_name;
  const char *ext; /* filaname extension */
  const char *mod; /* stream modifier ('-main') */
  int type;
  char *follow_url;
  int push_fd;
  GssMetrics *metrics;

  GstElement *pipeline;
  GstElement *src;
  GstElement *sink;
  int level;
  int profile;
  int program_id;
  int bandwidth;
  int width;
  int height;
  int bitrate;
  gboolean is_hls;

  GstAdapter *adapter;

  int n_chunks;
#define N_CHUNKS 20
  GssHLSSegment chunks[N_CHUNKS];
  struct {
    gboolean need_index_update;
    SoupBuffer *index_buffer; /* contents of current index file */

    gboolean at_eos; /* true if sliding window is at the end of the stream */
  } hls;

  GssRtspStream *rtsp_stream;

  void (*custom_client_fd_removed) (GssServerStream *stream, int fd,
      gpointer user_data);
  gpointer custom_user_data;
};

struct _GssConnection {
  SoupMessage *msg;
  SoupClientContext *client;
  GssServerStream *stream;
  GssProgram *program;
};

typedef enum {
  GSS_RESOURCE_ADMIN = (1<<0),
  GSS_RESOURCE_UI = (1<<1),
  GSS_RESOURCE_HTTP_ONLY = (1<<2),
  GSS_RESOURCE_HTTPS_ONLY = (1<<3),
  GSS_RESOURCE_ONETIME = (1<<4),
} GssResourceFlags;

typedef struct _GssResource GssResource;
typedef struct _GssTransaction GssTransaction;

typedef void (GssTransactionCallback)(GssTransaction *transaction);

struct _GssResource {
  char *location;
  char *etag;
  const char *content_type;

  GssResourceFlags flags;

  GssTransactionCallback *get_callback;
  GssTransactionCallback *put_callback;
  GssTransactionCallback *post_callback;

  gpointer priv;
};

struct _GssTransaction {
  GssServer *server;
  SoupServer *soupserver;
  SoupMessage *msg;
  const char *path;
  GHashTable *query;
  SoupClientContext *client;
  GssResource *resource;
  GssSession *session;
  gboolean done;
  GString *s;
};


#define GSS_TYPE_SERVER \
  (gss_server_get_type())
#define GSS_SERVER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GSS_TYPE_SERVER,GssServer))
#define GSS_SERVER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GSS_TYPE_SERVER,GssServerClass))
#define GSS_IS_SERVER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GSS_TYPE_SERVER))
#define GSS_IS_SERVER_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GSS_TYPE_SERVER))

typedef void (GssFooterHtml) (GssServer *server, GString *s, void *priv);

struct _GssServer
{
  GObject object;

  GssConfig *config;
  //char * config_filename;
  char * server_name;
  int port;
  int https_port;
  char *title;

  int n_programs;
  GssProgram **programs;
  GssMetrics *metrics;

  SoupServer *server;
  SoupServer *ssl_server;
  SoupSession *client_session;
  char *base_url;
  GHashTable *resources;

  gboolean enable_public_ui;

  GstRTSPServer *rtsp_server;

  //time_t config_timestamp;

  int max_connections;
  gint64 max_bitrate;

  GList *messages;
  int n_messages;

  GssFooterHtml *footer_html;
  void *footer_html_priv;

  void (*append_style_html) (GssServer *server, GString *s, void *priv);
  void *append_style_html_priv;
};

struct _GssServerClass
{
  GObjectClass object_class;

};


GType gss_server_get_type (void);

GssServer * gss_server_new (void);
void gss_server_free (GssServer *server);
void gss_server_set_hostname (GssServer *server, const char *hostname);
void gss_server_read_config (GssServer *server, const char *config_filename);

GssProgram * gss_server_add_program (GssServer *server, const char *program_name);
void gss_server_remove_program (GssServer *server, GssProgram *program);
void gss_server_follow_all (GssProgram *program, const char *host);
void gss_server_set_footer_html (GssServer *server, GssFooterHtml footer_html,
    gpointer priv);
void gss_server_set_title (GssServer *server, const char *title);

void gss_program_follow (GssProgram *program, const char *host,
    const char *stream);
void gss_program_http_follow (GssProgram *progra, const char *uri);
void gss_program_ew_contrib (GssProgram *program);
void gss_program_http_put (GssProgram *program);
void gss_program_icecast (GssProgram *program);
void gss_program_follow_get_list (GssProgram *program);
GssServerStream * gss_program_add_ogv_stream (GssProgram *program);
GssServerStream * gss_program_add_webm_stream (GssProgram *program);
GssServerStream * gss_program_add_hls_stream (GssProgram *program);
void gss_program_add_stream (GssProgram *program, GssServerStream *stream);
GssServerStream * gss_program_add_stream_full (GssProgram *program,
    int type, int width, int height, int bitrate, GstElement *sink);
void gss_program_log (GssProgram *program, const char *message, ...);
void gss_program_enable_streaming (GssProgram *program);
void gss_program_disable_streaming (GssProgram *program);
void gss_program_set_running (GssProgram *program, gboolean running);

void gss_server_stream_add_hls (GssServerStream *stream);

const char * gss_server_get_multifdsink_string (void);

void gss_server_deinit (void);
void gss_program_free (GssProgram *program);
void gss_stream_free (GssServerStream *stream);
void gss_program_set_jpegsink (GssProgram *program, GstElement *jpegsink);
void gss_stream_get_stats (GssServerStream *stream, guint64 *n_bytes_in,
    guint64 *n_bytes_out);

void gss_server_add_admin_callbacks (GssServer *server, SoupServer *soupserver);

void add_video_block (GssProgram *program, GString *s, int max_width,
    const char *base_url);

void gss_server_log (GssServer *server, char *message);

void gss_server_add_static_file (SoupServer *soupserver, const char *filename,
    const char *mime_type);
void gss_server_add_static_string (SoupServer * soupserver,
    const char *filename, const char *mime_type, const char *string);

void gss_server_add_resource (GssServer *server, const char *location,
    GssResourceFlags flags, const char *content_type,
    GssTransactionCallback get_callback,
    GssTransactionCallback put_callback, GssTransactionCallback post_callback,
    gpointer priv);
void gss_server_remove_resource (GssServer *server, const char *location);
void gss_server_add_file_resource (GssServer *server,
    const char *filename, GssResourceFlags flags, const char *mime_type);
void gss_server_add_string_resource (GssServer * server, const char *filename,
    GssResourceFlags flags, const char *mime_type, const char *string);


GssMetrics * gss_metrics_new (void);
void gss_metrics_free (GssMetrics * metrics);
void gss_metrics_add_client (GssMetrics * metrics, int bitrate);
void gss_metrics_remove_client (GssMetrics * metrics, int bitrate);

G_END_DECLS

#endif

