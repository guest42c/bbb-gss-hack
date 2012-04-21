
#ifndef _GSS_SERVER_H
#define _GSS_SERVER_H

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <libsoup/soup.h>
#include "gss-config.h"

#define DEFAULT_PORT 80

typedef struct _EwProgram EwProgram;
typedef struct _EwServerStream EwServerStream;
typedef struct _EwServer EwServer;
typedef struct _EwServerClass EwServerClass;
typedef struct _EwConnection EwConnection;
typedef struct _EwHLSSegment EwHLSSegment;

enum {
  EW_SERVER_STREAM_UNKNOWN,
  EW_SERVER_STREAM_OGG,
  EW_SERVER_STREAM_WEBM,
  EW_SERVER_STREAM_TS,
  EW_SERVER_STREAM_TS_MAIN,
  EW_SERVER_STREAM_FLV
};

typedef enum {
  EW_PROGRAM_EW_FOLLOW,
  EW_PROGRAM_HTTP_FOLLOW,
  EW_PROGRAM_MANUAL
} EwProgramType;

struct _EwProgram {
  EwServer *server;

  EwProgramType program_type;

  char *location;
  char *follow_uri;
  char *follow_host;

  SoupClientContext *push_client;
  int push_media_type;

  gboolean enable_streaming;

  int n_streams;
  EwServerStream **streams;

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

  void (*special_client_fd_removed) (EwServerStream *stream, int fd,
      gpointer user_data);
  gpointer special_user_data;
};

struct _EwHLSSegment {
  int index;
  SoupBuffer *buffer;
  char *location;
  int duration;
};

struct _EwServerStream {
  EwProgram *program;
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

  GstElement *pipeline;
  GstElement *sink;
  int level;
  int profile;
  int program_id;
  int bandwidth;
  int width;
  int height;
  int bitrate;
  int n_clients;
  gboolean is_hls;

  GstAdapter *adapter;

  int n_chunks;
#define N_CHUNKS 20
  EwHLSSegment chunks[N_CHUNKS];
  struct {
    gboolean need_index_update;
    SoupBuffer *index_buffer; /* contents of current index file */

    gboolean at_eos; /* true if sliding window is at the end of the stream */
  } hls;
};

struct _EwConnection {
  SoupMessage *msg;
  SoupClientContext *client;
  EwServerStream *stream;
  EwProgram *program;
};


#define EW_TYPE_SERVER \
  (ew_server_get_type())
#define EW_SERVER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),EW_TYPE_SERVER,EwServer))
#define EW_SERVER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),EW_TYPE_SERVER,EwServerClass))
#define GST_IS_EW_SERVER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),EW_TYPE_SERVER))
#define GST_IS_EW_SERVER_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),EW_TYPE_SERVER))

struct _EwServer
{
  GObject object;

  EwConfig *config;
  //char * config_filename;
  char * server_name;
  int port;

  int n_programs;
  EwProgram **programs;

  SoupServer *server;
  SoupServer *ssl_server;
  char *base_url;

  //time_t config_timestamp;

  int max_connections;
  gint64 max_bitrate;
  int n_clients;
  gint64 current_bitrate;

  GList *messages;
  int n_messages;
};

struct _EwServerClass
{
  GObjectClass object_class;

};

extern int verbose;

GType ew_server_get_type (void);

EwServer * ew_server_new (void);
void ew_server_free (EwServer *server);
void ew_server_set_hostname (EwServer *server, const char *hostname);
void ew_server_read_config (EwServer *server, const char *config_filename);

EwProgram * ew_server_add_program (EwServer *server, const char *program_name);
void ew_server_remove_program (EwServer *server, EwProgram *program);
void ew_server_follow_all (EwProgram *program, const char *host);

void ew_program_follow (EwProgram *program, const char *host,
    const char *stream);
void ew_program_http_follow (EwProgram *progra, const char *uri);
void ew_program_ew_contrib (EwProgram *program);
void ew_program_http_put (EwProgram *program, const char *location);
void ew_program_follow_get_list (EwProgram *program);
EwServerStream * ew_program_add_ogv_stream (EwProgram *program);
EwServerStream * ew_program_add_webm_stream (EwProgram *program);
EwServerStream * ew_program_add_hls_stream (EwProgram *program);
void ew_program_add_stream (EwProgram *program, EwServerStream *stream);
EwServerStream * ew_program_add_stream_full (EwProgram *program,
    int type, int width, int height, int bitrate, GstElement *sink);
void ew_program_log (EwProgram *program, const char *message, ...);
void ew_program_enable_streaming (EwProgram *program);
void ew_program_disable_streaming (EwProgram *program);

void ew_server_stream_add_hls (EwServerStream *stream);

const char * ew_server_get_multifdsink_string (void);

void ew_server_deinit (void);
void ew_program_free (EwProgram *program);
void ew_stream_free (EwServerStream *stream);
void ew_program_set_jpegsink (EwProgram *program, GstElement *jpegsink);

void ew_server_add_admin_callbacks (EwServer *server, SoupServer *soupserver);

void add_video_block (EwProgram *program, GString *s, int max_width,
    const char *base_url);

void ew_server_log (EwServer *server, char *message);

void
ew_server_add_static_file (SoupServer *soupserver, const char *filename,
    const char *mime_type);

#endif

