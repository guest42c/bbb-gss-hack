
//#include "config.h"

#include "ew-server.h"
#include "ew-html.h"
#include "ew-session.h"

#include <glib/gstdio.h>

#include <sys/ioctl.h>
#include <net/if.h>


#define BASE "/"

#define enable_video_tag TRUE
#define enable_flash TRUE
#define enable_cortado FALSE

enum {
  PROP_PORT = 1
};

char * get_time_string (void);

static void main_page_callback (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data);
static void list_callback (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data);
static void log_callback (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data);
static void file_callback (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data);
static void ew_server_handle_program (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data);
static void ew_server_handle_program_frag (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data);
static void ew_server_handle_program_list (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data);
static void ew_server_handle_program_image (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data);
static void ew_server_handle_program_jpeg (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data);
static void static_content_callback (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data);

static void ew_server_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void ew_server_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void
setup_paths (EwServer *server, SoupServer *soupserver);

static void ew_server_notify (const char *key, void *priv);

static void
client_removed (GstElement *e, int arg0, int arg1,
    gpointer user_data);
static void client_fd_removed (GstElement *e, int fd, gpointer user_data);
static void
msg_wrote_headers (SoupMessage *msg, void *user_data);
static void
ew_stream_handle (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data);
static void
ew_stream_handle_m3u8 (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data);
static gboolean periodic_timer (gpointer data);
static void jpeg_wrote_headers (SoupMessage *msg, void *user_data);

static void
handle_pipeline_message (GstBus *bus, GstMessage *message,
    gpointer user_data);

void ew_program_stop (EwProgram *program);
void ew_program_start (EwProgram *program);
void ew_stream_set_sink (EwServerStream *stream, GstElement *sink);
void ew_stream_create_follow_pipeline (EwServerStream *stream);

static SoupSession *session;

#define MAX_FDS 65536
void *fd_table[MAX_FDS];

G_DEFINE_TYPE (EwServer, ew_server, G_TYPE_OBJECT);

static void
ew_server_init (EwServer *server)
{
  if (session == NULL) {
    session = soup_session_async_new ();
  }

  if (getuid() == 0) {
    server->port = DEFAULT_PORT;
  } else {
    server->port = 8080;
  }

  server->n_programs = 0;
  server->programs = NULL;

}

void
ew_server_deinit (void)
{
  if (session) g_object_unref (session);
  session = NULL;
}

void
ew_server_log (EwServer *server, char *message)
{
  if (verbose) g_print ("%s\n", message);
  server->messages = g_list_append (server->messages, message);
  server->n_messages++;
  while (server->n_messages > 50) {
    g_free (server->messages->data);
    server->messages = g_list_delete_link (server->messages, server->messages);
    server->n_messages--;
  }

}

static void
ew_server_class_init (EwServerClass *server_class)
{
  G_OBJECT_CLASS(server_class)->set_property = ew_server_set_property;
  G_OBJECT_CLASS(server_class)->get_property = ew_server_get_property;

  g_object_class_install_property (G_OBJECT_CLASS(server_class), PROP_PORT,
      g_param_spec_int ("port", "Port",
        "Port", 0, 65535, DEFAULT_PORT,
        (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

}


static void
ew_server_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  EwServer *server;

  server = EW_SERVER (object);

  switch (prop_id) {
    case PROP_PORT:
      server->port = g_value_get_int (value);
      break;
  }
}

static void
ew_server_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  EwServer *server;

  server = EW_SERVER (object);

  switch (prop_id) {
    case PROP_PORT:
      g_value_set_int (value, server->port);
      break;
  }
}

char *
get_ip_address_string (const char *interface)
{
  int sock;
  int ret;
  struct ifreq ifr;

  sock = socket (AF_INET, SOCK_DGRAM, 0);

  memset (&ifr, 0, sizeof(ifr));
  strcpy (ifr.ifr_name, "eth0");

  ret = ioctl (sock, SIOCGIFADDR, &ifr);
  if (ret == 0) {
    struct sockaddr_in *sa = (struct sockaddr_in *)&ifr.ifr_addr;
    guint32 quad = ntohl (sa->sin_addr.s_addr);

    return g_strdup_printf ("%d.%d.%d.%d", (quad>>24)&0xff,
        (quad>>16)&0xff, (quad>>8)&0xff, (quad>>0)&0xff);
  }

  return strdup("127.0.0.1");
}

char *
gethostname_alloc (void)
{
  char *s;
  char *t;
  int ret;

  s = g_malloc (1000);
  ret = gethostname (s, 1000);
  if (ret == 0) {
    t = g_strdup (s);
  } else {
    t = get_ip_address_string ("eth0");
  }
  g_free (s);
  return t;
}

EwServer *
ew_server_new (void)
{
  EwServer *server;
  SoupAddress *if6;

  server = g_object_new (EW_TYPE_SERVER, NULL);

  server->config = ew_config_new ();

  ew_config_set_notify (server->config, "max_connections", ew_server_notify,
      server);
  ew_config_set_notify (server->config, "max_bandwidth", ew_server_notify,
      server);
  ew_config_set_notify (server->config, "server_name", ew_server_notify,
      server);
  ew_config_set_notify (server->config, "server_port", ew_server_notify,
      server);

  //server->config_filename = "/opt/entropywave/ew-oberon/config";
  server->server_name = gethostname_alloc ();

  if (server->port == 80) {
    server->base_url = g_strdup_printf("http://%s", server->server_name);
  } else {
    server->base_url = g_strdup_printf("http://%s:%d", server->server_name,
        server->port);
  }

  if6 = soup_address_new_any (SOUP_ADDRESS_FAMILY_IPV6, server->port);
  server->server = soup_server_new (SOUP_SERVER_INTERFACE, if6,
      SOUP_SERVER_PORT, server->port, NULL);
  g_object_unref (if6);

  if (server->server == NULL) {
    /* try again with just IPv4 */
    server->server = soup_server_new (SOUP_SERVER_PORT, server->port, NULL);
  }

  if (server->server == NULL) {
    g_print ("failed to obtain server port\n");
    return NULL;
  }

  setup_paths (server, server->server);

  server->ssl_server = soup_server_new (SOUP_SERVER_PORT, 443,
      "ssl-cert-file", "server.crt",
      "ssl-key-file", "server.key",
      NULL);
  if (!server->ssl_server) {
    server->ssl_server = soup_server_new (SOUP_SERVER_PORT, 8443,
        "ssl-cert-file", "server.crt",
        "ssl-key-file", "server.key",
        NULL);
  }

  if (server->ssl_server) {
    setup_paths (server, server->ssl_server);
  }

  g_timeout_add (1000, (GSourceFunc)periodic_timer, server);

  server->max_connections = INT_MAX;
  server->max_bitrate = G_MAXINT64;

  return server;
}


static void
setup_paths (EwServer *server, SoupServer *soupserver)
{
  ew_session_add_session_callbacks (soupserver, server);
  ew_server_add_admin_callbacks (server, soupserver);

  soup_server_add_handler (soupserver, "/", main_page_callback, server, NULL);
  soup_server_add_handler (soupserver, "/list", list_callback, server, NULL);
  soup_server_add_handler (soupserver, "/log", log_callback, server, NULL);

  if (enable_cortado) {
    soup_server_add_handler (soupserver, "/cortado.jar", file_callback,
        "application/java-archive", NULL);
  }

  if (enable_flash) {
    soup_server_add_handler (soupserver, "/OSplayer.swf", file_callback,
        "application/x-shockwave-flash", NULL);
    soup_server_add_handler (soupserver, "/AC_RunActiveContent.js", file_callback,
        "application/javascript", NULL);
    soup_server_add_handler (soupserver, "/test.flv", file_callback,
        "video/x-flv", NULL);
  }

#define IMAGE(image) \
  ew_server_add_static_file (soupserver, "/images/" image , "image/png")

  IMAGE("button_access.png");
  IMAGE("button_admin.png");
  IMAGE("button_edit.png");
  IMAGE("button_events.png");
  IMAGE("button_log.png");
  IMAGE("button_main.png");
  IMAGE("button_network.png");
  IMAGE("button_server.png");
  IMAGE("button_video.png");
  IMAGE("template_bodybg.png");
  IMAGE("template_c1000.png");
  IMAGE("template_e1000.png");
  IMAGE("template_footer.png");
  IMAGE("template_header.png");
  IMAGE("template_header_nologo.png");
  IMAGE("template_navadmin.png");
  IMAGE("template_navlog.png");
  IMAGE("template_navmain.png");
  IMAGE("template_navnet.png");
  IMAGE("template_s1000.png");

  soup_server_run_async (soupserver);
}

typedef struct _StaticContent StaticContent;
struct _StaticContent {
  const char *filename;
  const char *mime_type;
  char *etag;
  char *contents;
  gsize size;
};

static void
static_content_callback (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data)
{
  StaticContent *content = (StaticContent *)user_data;
  gboolean ret;
  GError *error = NULL;
  const char *inm;

  if (msg->method != SOUP_METHOD_GET) {
    soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);
    return;
  }

  if (content->contents == NULL) {
    ret = g_file_get_contents (path + 1, &content->contents, &content->size,
        &error);
    if (!ret) {
      g_error_free (error);
      ew_html_error_404 (msg);
      if (verbose) g_print("missing file %s\n", path);
      return;
    }
  }

  soup_message_headers_replace (msg->response_headers, "Keep-Alive",
      "timeout=5, max=100");
  soup_message_headers_append (msg->response_headers, "Etag",
      content->etag);

  inm = soup_message_headers_get_one (msg->request_headers,
      "If-None-Match");
  if (inm && !strcmp (inm, content->etag)) {
    soup_message_set_status (msg, SOUP_STATUS_NOT_MODIFIED);
    return;
  }

  soup_message_set_status (msg, SOUP_STATUS_OK);

  soup_message_set_response (msg, content->mime_type,
      SOUP_MEMORY_STATIC, content->contents, content->size);
}


void
ew_server_add_static_file (SoupServer *soupserver, const char *filename,
    const char *mime_type)
{
  StaticContent *content;

  content = g_malloc0 (sizeof(StaticContent));

  content->filename = filename;
  content->mime_type = mime_type;
  content->etag = g_strdup_printf("%p", content);

  soup_server_add_handler (soupserver, content->filename,
      static_content_callback, content, NULL);

}

void
ew_server_free (EwServer *server)
{
  int i;

  for(i=0;i<server->n_programs;i++){
    EwProgram *program = server->programs[i];

    ew_program_free (program);
  }

  if (server->server) g_object_unref (server->server);

  g_list_foreach (server->messages, (GFunc)g_free, NULL);
  g_list_free (server->messages);

  g_free (server->base_url);
  g_free (server->server_name);
  g_free (server->programs);
  //g_free (server);
}

static void
ew_server_notify (const char *key, void *priv)
{
  EwServer *server = (EwServer *)priv;
  const char *s;

#if 0
  s = ew_config_get (server->config, "server_port");
  server->port = strtol (s, NULL, 10);
#endif

  s = ew_config_get (server->config, "server_name");
  ew_server_set_hostname (server, s);

  s = ew_config_get (server->config, "max_connections");
  server->max_connections = strtol (s, NULL, 10);
  if (server->max_connections == 0) {
    server->max_connections = INT_MAX;
  }

  s = ew_config_get (server->config, "max_bandwidth");
  server->max_bitrate = (gint64)strtol (s, NULL, 10) * 8000;
  if (server->max_bitrate == 0) {
    server->max_bitrate = G_MAXINT64;
  }

}

void
ew_server_set_hostname (EwServer *server, const char *hostname)
{
  g_free (server->server_name);
  server->server_name = g_strdup (hostname);

  g_free (server->base_url);
  if (server->server_name[0]) {
    if (server->port == 80) {
      server->base_url = g_strdup_printf("http://%s", server->server_name);
    } else {
      server->base_url = g_strdup_printf("http://%s:%d", server->server_name,
          server->port);
    }
  } else {
    server->base_url = g_strdup ("");
  }
}

void
ew_server_follow_all (EwProgram *program, const char *host)
{

}

EwProgram *
ew_server_add_program (EwServer *server, const char *program_name)
{
  EwProgram *program;
  char *s;

  program = g_malloc0(sizeof(EwProgram));

  server->programs = g_realloc (server->programs,
      sizeof(EwProgram *) * (server->n_programs + 1));
  server->programs[server->n_programs] = program;
  server->n_programs++;

  program->server = server;
  program->location = g_strdup (program_name);
  program->enable_streaming = TRUE;

  s = g_strdup_printf ("/%s", program_name);
  soup_server_add_handler (server->server, s,
      ew_server_handle_program, program, NULL);
  g_free (s);

  s = g_strdup_printf ("/%s.frag", program_name);
  soup_server_add_handler (server->server, s,
      ew_server_handle_program_frag, program, NULL);
  g_free (s);

  s = g_strdup_printf ("/%s.list", program_name);
  soup_server_add_handler (server->server, s,
      ew_server_handle_program_list, program, NULL);
  g_free (s);

  s = g_strdup_printf ("/%s-snapshot.png", program_name);
  soup_server_add_handler (server->server, s,
      ew_server_handle_program_image, program, NULL);
  g_free (s);

  s = g_strdup_printf ("/%s-snapshot.jpeg", program_name);
  soup_server_add_handler (server->server, s,
      ew_server_handle_program_jpeg, program, NULL);
  g_free (s);


  return program;
}

void
ew_program_set_jpegsink (EwProgram *program, GstElement *jpegsink)
{
  program->jpegsink = g_object_ref (jpegsink);

  g_signal_connect (jpegsink, "client-removed",
      G_CALLBACK (client_removed), NULL);
  g_signal_connect (jpegsink, "client-fd-removed",
      G_CALLBACK (client_fd_removed), NULL);
}

void
ew_server_remove_program (EwServer *server, EwProgram *program)
{

  int i;

  for(i=0;i<server->n_programs;i++){
    if (server->programs[i] == program) {
      if (i+1 < server->n_programs) {
        memmove (server->programs + i, server->programs + i + 1,
            server->n_programs - i - 1);
      }
      server->n_programs--;
    }
  }

  ew_program_free (program);
}

void
ew_program_free (EwProgram *program)
{
  int i;

  for(i=0;i<program->n_streams;i++){
    EwServerStream *stream = program->streams[i];

    ew_stream_free (stream);
  }

  if (program->hls.variant_buffer) {
    soup_buffer_free (program->hls.variant_buffer);
  }

  if (program->pngappsink) g_object_unref (program->pngappsink);
  if (program->jpegsink) g_object_unref (program->jpegsink);
  g_free (program->location);
  g_free (program->streams);
  g_free (program->follow_uri);
  g_free (program->follow_host);
  g_free (program);
}

char *
get_time_string (void)
{
  GDateTime *datetime;
  char *s;

  datetime = g_date_time_new_now_local ();

  /* RFC 822 */
  //strftime(thetime, 79, "%a, %d %b %y %T %z", tmp); // RFC-822
  /* RFC 2822 */
  s = g_date_time_format (datetime, "%a, %d %b %Y %H:%M:%S %z"); // RFC-2822
  if (s[27] == '-') s[27] = '0';
  /* RFC 3339, almost */
  //strftime(thetime, 79, "%Y-%m-%d %H:%M:%S%z", tmp);

  g_date_time_unref (datetime);

  return s;
}

void
ew_program_log (EwProgram *program, const char *message, ...)
{
  char *thetime = get_time_string ();
  char *s;
  va_list varargs;

  g_return_if_fail (program);
  g_return_if_fail (message);

  va_start (varargs, message);
  s = g_strdup_vprintf (message, varargs);
  va_end (varargs);

  ew_server_log (program->server, g_strdup_printf("%s: %s: %s",
        thetime, program->location, s));
  g_free (s);
  g_free (thetime);
}

void
ew_stream_free (EwServerStream *stream)
{
  int i;

  g_free (stream->name);
  g_free (stream->playlist_name);
  g_free (stream->codecs);
  g_free (stream->mime_type);
  g_free (stream->follow_url);

  for(i=0;i<N_CHUNKS;i++){
    EwHLSSegment *segment = &stream->chunks[i];

    if (segment->buffer) {
      soup_buffer_free (segment->buffer);
      g_free (segment->location);
    }
  }

  if (stream->hls.index_buffer) {
    soup_buffer_free (stream->hls.index_buffer);
  }

  ew_stream_set_sink (stream, NULL);
  if (stream->pipeline) {
    gst_element_set_state (GST_ELEMENT(stream->pipeline), GST_STATE_NULL);
    g_object_unref (stream->pipeline);
  }
  if (stream->adapter) g_object_unref (stream->adapter);

  g_free (stream);
}


const char *
ew_server_get_multifdsink_string (void)
{
  return "multifdsink "
    "sync=false "
    "time-min=200000000 "
    "recover-policy=keyframe "
    "unit-type=2 "
    "units-max=20000000000 "
    "units-soft-max=11000000000 "
    "sync-method=burst-keyframe "
    "burst-unit=2 "
    "burst-value=3000000000";
}

void
ew_program_add_stream (EwProgram *program, EwServerStream *stream)
{
  program->streams = g_realloc (program->streams,
      sizeof(EwProgram *) * (program->n_streams + 1));
  program->streams[program->n_streams] = stream;
  stream->index = program->n_streams;
  program->n_streams++;

  stream->program = program;
}

static void
client_removed (GstElement *e, int fd, int status,
    gpointer user_data)
{
  EwServerStream *stream = user_data;

  if (fd_table[fd]) {
    if (stream) {
      stream->n_clients--;
      stream->program->server->n_clients--;
    }
  }
}

static void
client_fd_removed (GstElement *e, int fd, gpointer user_data)
{
  EwServerStream *stream = user_data;
  SoupSocket *socket = fd_table[fd];

  if (socket) {
    soup_socket_disconnect (socket);
    fd_table[fd] = NULL;
  } else {
    stream->program->special_client_fd_removed (stream, fd,
        stream->program->special_user_data);
  }
}


static void
ew_stream_handle (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data)
{
  EwServerStream *stream = (EwServerStream *)user_data;
  EwServer *ewserver = stream->program->server;
  EwConnection *connection;

  if (msg->method != SOUP_METHOD_GET) {
    soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);
    return;
  }
  if (!stream->program->enable_streaming) {
    soup_message_set_status (msg, SOUP_STATUS_NO_CONTENT);
    return;
  }

  if (ewserver->n_clients >= ewserver->max_connections ||
      ewserver->current_bitrate + stream->bitrate >= ewserver->max_bitrate) {
    if (verbose) g_print ("n_clients %d max_connections %d\n",
        ewserver->n_clients, ewserver->max_connections);
    if (verbose) g_print ("current bitrate %" G_GINT64_FORMAT " bitrate %d max_bitrate %" G_GINT64_FORMAT "\n",
      ewserver->current_bitrate, stream->bitrate, ewserver->max_bitrate);
    soup_message_set_status (msg, SOUP_STATUS_SERVICE_UNAVAILABLE);
    return;
  }

  connection = g_malloc0 (sizeof(EwConnection));
  connection->msg = msg;
  connection->client = client;
  connection->stream = stream;

  soup_message_set_status (msg, SOUP_STATUS_OK);

  soup_message_headers_set_encoding (msg->response_headers,
      SOUP_ENCODING_EOF);
  soup_message_headers_replace (msg->response_headers, "Content-Type",
      stream->mime_type);

  g_signal_connect (msg, "wrote-headers", G_CALLBACK(msg_wrote_headers),
      connection);
}

static void
msg_wrote_headers (SoupMessage *msg, void *user_data)
{
  EwConnection *connection = user_data;
  SoupSocket *socket;
  int fd;

  socket = soup_client_context_get_socket (connection->client);
  fd = soup_socket_get_fd (socket);

  if (connection->stream->sink) {
    g_signal_emit_by_name (connection->stream->sink, "add", fd);

    g_assert (fd < MAX_FDS);
    fd_table[fd] = socket;

    connection->stream->n_clients++;
    connection->stream->program->server->n_clients++;
  } else {
    soup_socket_disconnect (socket);
  }

  g_free (connection);
}

static void
ew_stream_handle_m3u8 (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data)
{
  char *content;
  EwServerStream *stream = (EwServerStream *)user_data;

  content = g_strdup_printf(
      "#EXTM3U\n"
      "#EXT-X-TARGETDURATION:10\n"
      "#EXTINF:10,\n"
      "%s/%s\n", stream->program->server->base_url, stream->name);

  soup_message_set_status (msg, SOUP_STATUS_OK);

  soup_message_set_response (msg, "application/x-mpegurl", SOUP_MEMORY_TAKE,
      content, strlen(content));
}

EwServerStream *
ew_stream_new (int type, int width, int height, int bitrate)
{
  EwServerStream *stream;

  stream = g_malloc0 (sizeof(EwServerStream));

  stream->type = type;
  stream->width = width;
  stream->height = height;
  stream->bitrate = bitrate;

  switch (type) {
    case EW_SERVER_STREAM_OGG:
      stream->mime_type = g_strdup ("video/ogg");
      stream->mod = "";
      stream->ext = "ogv";
      break;
    case EW_SERVER_STREAM_WEBM:
      stream->mime_type = g_strdup ("video/webm");
      stream->mod = "";
      stream->ext = "webm";
      break;
    case EW_SERVER_STREAM_TS:
      stream->mime_type = g_strdup ("video/mp2t");
      stream->mod = "";
      stream->ext = "ts";
      break;
    case EW_SERVER_STREAM_TS_MAIN:
      stream->mime_type = g_strdup ("video/mp2t");
      stream->mod = "-main";
      stream->ext = "ts";
      break;
    case EW_SERVER_STREAM_FLV:
      stream->mime_type = g_strdup ("video/x-flv");
      stream->mod = "";
      stream->ext = "flv";
      break;
  }

  return stream;
}

void
ew_program_enable_streaming (EwProgram *program)
{
  program->enable_streaming = TRUE;
}

void
ew_program_disable_streaming (EwProgram *program)
{
  int i;

  program->enable_streaming = FALSE;
  for(i=0;i<program->n_streams;i++){
    EwServerStream *stream = program->streams[i];
    g_signal_emit_by_name (stream->sink, "clear");
  }
}

EwServerStream *
ew_program_add_stream_full (EwProgram *program,
    int type, int width, int height, int bitrate, GstElement *sink)
{
  SoupServer *soupserver = program->server->server;
  EwServerStream *stream;
  char *s;

  stream = ew_stream_new (type, width, height, bitrate);
  ew_program_add_stream (program, stream);

  stream->name = g_strdup_printf ("%s-%dx%d-%dkbps%s.%s", program->location,
      stream->width, stream->height, stream->bitrate/1000,stream->mod,
      stream->ext);
  s = g_strdup_printf("/%s", stream->name);
  soup_server_add_handler (soupserver, s, ew_stream_handle,
      stream, NULL);
  g_free (s);

  stream->playlist_name = g_strdup_printf ("%s-%dx%d-%dkbps%s-%s.m3u8",
      program->location,
      stream->width, stream->height, stream->bitrate/1000, stream->mod,
      stream->ext);
  s = g_strdup_printf("/%s", stream->playlist_name);
  soup_server_add_handler (soupserver, s, ew_stream_handle_m3u8,
      stream, NULL);
  g_free (s);

  ew_stream_set_sink (stream, sink);

  return stream;
}

void
ew_stream_set_sink (EwServerStream *stream, GstElement *sink)
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
    if (stream->type == EW_SERVER_STREAM_TS ||
        stream->type == EW_SERVER_STREAM_TS_MAIN) {
      ew_server_stream_add_hls (stream);
    }
  }
}


static void
main_page_callback (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data)
{
  const char *mime_type = "text/html";
  char *content;
  EwServer *ewserver = (EwServer *)user_data;
  GString *s;
  int i;

  if (msg->method != SOUP_METHOD_GET) {
    soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);
    return;
  }

  if (!g_str_equal (path, "/")) {
    ew_html_error_404 (msg);
    return;
  }

  s = g_string_new ("");

  ew_html_header (s, "Entropy Wave Live Streaming");

  g_string_append_printf(s,
      "<div id=\"header\">\n"
      "<img src=\"" BASE "images/template_header_nologo.png\" width=\"812\" height=\"36\" border=\"0\" />\n"
      "</div><!-- end header div -->\n"
      "<div id=\"content\">\n"
      "<h1>Available Streams</h1>\n");

  for(i=0;i<ewserver->n_programs;i++){
    EwProgram *program = ewserver->programs[i];
    g_string_append_printf(s,"<br/>"
        "<img src=\"/%s-snapshot.jpeg\">"
        "<a href=\"/%s\">%s</a>\n",
        program->location,
        program->location, program->location);
  }

  g_string_append (s, "</div><!-- end content div -->\n");

  ew_html_footer (s, NULL);

  content = g_string_free (s, FALSE);

  soup_message_set_status (msg, SOUP_STATUS_OK);

  soup_message_set_response (msg, mime_type, SOUP_MEMORY_TAKE,
      content, strlen(content));
}

static void
list_callback (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data)
{
  const char *mime_type = "text/plain";
  char *content;
  EwServer *ewserver = (EwServer *)user_data;
  GString *s = g_string_new ("");
  int i;

  if (msg->method != SOUP_METHOD_GET) {
    soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);
    return;
  }

  if (!g_str_equal (path, "/list")) {
    ew_html_error_404 (msg);
    return;
  }

  for(i=0;i<ewserver->n_programs;i++){
    EwProgram *program = ewserver->programs[i];
    g_string_append_printf(s, "%s\n", program->location);
  }

  content = g_string_free (s, FALSE);

  soup_message_set_status (msg, SOUP_STATUS_OK);

  soup_message_set_response (msg, mime_type, SOUP_MEMORY_TAKE,
      content, strlen(content));
}

static void
log_callback (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data)
{
  const char *mime_type = "text/plain";
  char *content;
  EwServer *ewserver = (EwServer *)user_data;
  GString *s = g_string_new ("");
  GList *g;
  char *t;

  if (msg->method != SOUP_METHOD_GET) {
    soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);
    return;
  }

  if (!g_str_equal (path, "/log")) {
    ew_html_error_404 (msg);
    return;
  }

  t = get_time_string ();
  g_string_append_printf(s, "Server time: %s\n", t);
  g_free (t);
  g_string_append_printf(s, "Recent log messages:\n");

  for(g=g_list_first(ewserver->messages);g;g=g_list_next(g)){
    g_string_append_printf(s, "%s\n", (char *)g->data);
  }

  content = g_string_free (s, FALSE);

  soup_message_set_status (msg, SOUP_STATUS_OK);

  soup_message_set_response (msg, mime_type, SOUP_MEMORY_TAKE,
      content, strlen(content));
}

static void
file_callback (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data)
{
  char *contents;
  gboolean ret;
  gsize size;
  GError *error = NULL;
  const char *mime_type = user_data;

  ret = g_file_get_contents (path + 1, &contents, &size, &error);
  if (!ret) {
    ew_html_error_404 (msg);
    if (verbose) g_print("missing file %s\n", path);
    return;
  }

  soup_message_set_status (msg, SOUP_STATUS_OK);

  soup_message_set_response (msg, mime_type,
      SOUP_MEMORY_TAKE, contents, size);
}



void
add_video_block (EwProgram *program, GString *s, int max_width,
    const char *base_url)
{
  int i;
  int width = 0;
  int height = 0;

  for(i=0;i<program->n_streams;i++){
    EwServerStream *stream = program->streams[i];
    if (stream->width > width) width = stream->width;
    if (stream->height > height) height = stream->height;
  }
  if (max_width != 0 && width > max_width) {
    height = max_width * 9 / 16;
    width = max_width;
  }

  if (enable_video_tag) {
  g_string_append_printf (s,
      "<video controls=\"true\" autoplay=\"false\" "
      "id=video width=\"%d\" height=\"%d\">\n",
      width, height);

  for(i=program->n_streams-1;i>=0;i--){
    EwServerStream *stream = program->streams[i];
    if (stream->type == EW_SERVER_STREAM_WEBM) {
      g_string_append_printf (s,
          "<source src=\"%s/%s\" type='video/webm; codecs=\"vp8, vorbis\"'>\n",
          base_url, stream->name);
    }
  }

  for(i=program->n_streams-1;i>=0;i--){
    EwServerStream *stream = program->streams[i];
    if (stream->type == EW_SERVER_STREAM_OGG) {
      g_string_append_printf (s,
          "<source src=\"%s/%s\" type='video/ogg; codecs=\"theora, vorbis\"'>\n",
          base_url, stream->name);
    }
  }

  for(i=program->n_streams-1;i>=0;i--){
    EwServerStream *stream = program->streams[i];
    if (stream->type == EW_SERVER_STREAM_TS ||
        stream->type == EW_SERVER_STREAM_TS_MAIN) {
#if 0
      g_string_append_printf (s,
          "<source src=\"%s/%s\" type='video/x-mpegURL; codecs=\"avc1.42E01E, mp4a.40.2\"' >\n",
          base_url, stream->playlist_name);
#endif
      g_string_append_printf (s,
          "<source src=\"%s/%s.m3u8\" >\n",
          base_url, program->location);
      break;
    }
  }

  }

  if (enable_cortado) {
    for(i=0;i<program->n_streams;i++){
      EwServerStream *stream = program->streams[i];
      if (stream->type == EW_SERVER_STREAM_OGG) {
        g_string_append_printf (s,
            "<applet code=\"com.fluendo.player.Cortado.class\"\n"
            "  archive=\"%s/cortado.jar\" width=\"%d\" height=\"%d\">\n"
            "    <param name=\"url\" value=\"%s/%s\"/>\n"
            "</applet>\n", base_url, width, height,
            base_url, stream->name);
        break;
      }
    }
  }

  if (enable_flash) {
    for(i=0;i<program->n_streams;i++){
      EwServerStream *stream = program->streams[i];
      if (stream->type == EW_SERVER_STREAM_FLV) {
        g_string_append_printf (s,
            " <object width='%d' height='%d' id='flvPlayer' "
              "type=\"application/x-shockwave-flash\" "
              "data=\"OSplayer.swf\">\n"
            "  <param name='allowFullScreen' value='true'>\n"
            "  <param name=\"allowScriptAccess\" value=\"always\"> \n"
            "  <param name=\"movie\" value=\"OSplayer.swf\"> \n"
            "  <param name=\"flashvars\" value=\""
              "movie=%s/%s"
              "&btncolor=0x333333"
              "&accentcolor=0x31b8e9"
              "&txtcolor=0xdddddd"
              "&volume=30"
              "&autoload=on"
              "&autoplay=off"
              "&vTitle=TITLE"
              "&showTitle=yes\">\n",
            width, height + 24,
            base_url, stream->name);
#if 0
        g_string_append_printf (s,
            "  <embed src='OSplayer.swf"
              "?movie=%s/%s"
              "&btncolor=0x333333"
              "&accentcolor=0x31b8e9"
              "&txtcolor=0xdddddd"
              "&volume=30"
              "&autoload=on"
              "&autoplay=off"
              "&vTitle=TITLE"
              "&showTitle=yes' width='%d' height='%d' "
              "allowFullScreen='true' "
              "type='application/x-shockwave-flash' "
              "allowScriptAccess='always'>\n"
            " </embed>\n",
            base_url, stream->name,
            width, height);
#endif
        if (program->enable_snapshot) {
          g_string_append_printf (s, "<img src=\"%s/%s-snapshot.png\" alt=\"\">\n",
              base_url, program->location);
        }
        g_string_append_printf (s, " </object>\n");
        break;
      }

    }
  } else {
    if (program->enable_snapshot) {
      g_string_append_printf (s, "<img src=\"%s/%s-snapshot.png\" alt=\"\">\n",
          base_url, program->location);
    }
  }

  if (enable_video_tag) {
    g_string_append (s, "</video>\n");
  }

}

static void
ew_server_handle_program_frag (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data)
{
  const char *mime_type = "text/html";
  char *content;
  EwProgram *program = (EwProgram *)user_data;
  GString *s = g_string_new ("");

  if (!program->enable_streaming) {
    soup_message_set_status (msg, SOUP_STATUS_NO_CONTENT);
    return;
  }

  add_video_block (program, s, 0, program->server->base_url);

  content = g_string_free (s, FALSE);

  soup_message_set_status (msg, SOUP_STATUS_OK);

  soup_message_set_response (msg, mime_type, SOUP_MEMORY_TAKE,
      content, strlen(content));
}

static void
ew_server_handle_program (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data)
{
  const char *mime_type = "text/html";
  char *content;
  EwProgram *program = (EwProgram *)user_data;
  GString *s = g_string_new ("");
  const char *base_url = "";
  int i;

  ew_html_header (s, program->location);
  g_string_append_printf(s,
      "<div id=\"header\">\n"
      "<img src=\"" BASE "images/template_header_nologo.png\" width=\"812\" height=\"36\" border=\"0\" alt=\"\" />\n"
      "</div><!-- end header div -->\n"
      "<div id=\"content\">\n");

  g_string_append_printf (s,
      "<h1>Live Stream: %s</h1>\n", program->location);

  add_video_block (program, s, 0, "");

  g_string_append (s, "<br/>\n");
  for(i=0;i<program->n_streams;i++){
    const char *typename = "Unknown";
    EwServerStream *stream = program->streams[i];

    switch (stream->type) {
      case EW_SERVER_STREAM_OGG:
        typename = "Ogg/Theora";
        break;
      case EW_SERVER_STREAM_WEBM:
        typename = "WebM";
        break;
      case EW_SERVER_STREAM_TS:
        typename = "MPEG-TS";
        break;
      case EW_SERVER_STREAM_TS_MAIN:
        typename = "MPEG-TS main";
        break;
      case EW_SERVER_STREAM_FLV:
        typename = "FLV";
        break;
    }
    g_string_append_printf (s,
      "<br/>%d: %s %dx%d %d kbps <a href=\"%s/%s\">stream</a> "
      "<a href=\"%s/%s\">playlist</a>\n", i, typename,
      stream->width, stream->height, stream->bitrate/1000,
      base_url, stream->name,
      base_url, stream->playlist_name);
  }
  if (program->enable_hls) {
    g_string_append_printf (s,
      "<br/><a href=\"%s/%s.m3u8\">HLS</a>\n",
      base_url, program->location);
  }

  g_string_append (s, "</div><!-- end content div -->\n");
  
  ew_html_footer (s, NULL);

  content = g_string_free (s, FALSE);

  soup_message_set_status (msg, SOUP_STATUS_OK);

  soup_message_set_response (msg, mime_type, SOUP_MEMORY_TAKE,
      content, strlen(content));
}

static void
ew_server_handle_program_list (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data)
{
  const char *mime_type = "text/plain";
  char *content;
  EwProgram *program = (EwProgram *)user_data;
  GString *s = g_string_new ("");
  int i;
  const char *base_url = "";

  for(i=0;i<program->n_streams;i++){
    EwServerStream *stream = program->streams[i];
    const char *typename = "unknown";
    switch (stream->type) {
      case EW_SERVER_STREAM_OGG:
        typename = "ogg";
        break;
      case EW_SERVER_STREAM_WEBM:
        typename = "webm";
        break;
      case EW_SERVER_STREAM_TS:
        typename = "mpeg-ts";
        break;
      case EW_SERVER_STREAM_TS_MAIN:
        typename = "mpeg-ts-main";
        break;
      case EW_SERVER_STREAM_FLV:
        typename = "flv";
        break;
    }
    g_string_append_printf (s,
        "%d %s %d %d %d %s/%s\n", i, typename,
        stream->width, stream->height, stream->bitrate,
        base_url, stream->name);
  }

  content = g_string_free (s, FALSE);

  soup_message_set_status (msg, SOUP_STATUS_OK);

  soup_message_set_response (msg, mime_type, SOUP_MEMORY_TAKE,
      content, strlen(content));
}

static void
ew_server_handle_program_image (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data)
{
  const char *mime_type = "image/png";
  EwProgram *program = (EwProgram *)user_data;
  GstBuffer *buffer = NULL;

  if (!program->enable_streaming) {
    soup_message_set_status (msg, SOUP_STATUS_NO_CONTENT);
    return;
  }

  if (program->pngappsink) {
    g_object_get (program->pngappsink, "last-buffer", &buffer, NULL);
  }

  if (buffer != NULL) {

    soup_message_set_status (msg, SOUP_STATUS_OK);

    soup_message_set_response (msg, mime_type, SOUP_MEMORY_COPY,
        (void *)GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer));

    gst_buffer_unref (buffer);
  } else {
    ew_html_error_404 (msg);
  }

}

static void
ew_server_handle_program_jpeg (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data)
{
  EwProgram *program = (EwProgram *)user_data;
  EwConnection *connection;

  if (!program->enable_streaming || program->jpegsink == NULL) {
    soup_message_set_status (msg, SOUP_STATUS_NO_CONTENT);
    return;
  }

  connection = g_malloc0 (sizeof(EwConnection));
  connection->msg = msg;
  connection->client = client;
  connection->program = program;

  soup_message_set_status (msg, SOUP_STATUS_OK);

  soup_message_headers_set_encoding (msg->response_headers,
      SOUP_ENCODING_EOF);
  soup_message_headers_replace (msg->response_headers, "Content-Type",
      "multipart/x-mixed-replace;boundary=myboundary");

  g_signal_connect (msg, "wrote-headers", G_CALLBACK(jpeg_wrote_headers),
      connection);
}

static void
jpeg_wrote_headers (SoupMessage *msg, void *user_data)
{
  EwConnection *connection = user_data;
  SoupSocket *socket;
  int fd;

  socket = soup_client_context_get_socket (connection->client);
  fd = soup_socket_get_fd (socket);

  if (connection->program->jpegsink) {
    g_signal_emit_by_name (connection->program->jpegsink, "add", fd);

    g_assert (fd < MAX_FDS);
    fd_table[fd] = socket;
  } else {
    soup_socket_disconnect (socket);
  }

  g_free (connection);
}

#if 0
static int
get_timestamp (const char *filename)
{
  struct stat statbuf;
  int ret;

  ret = g_stat (filename, &statbuf);
  if (ret == 0) {
    return statbuf.st_mtime;
  }
  return 0;
}
#endif


void
ew_server_read_config (EwServer *server, const char *config_filename)
{
  GKeyFile *kf;
  char *s;
  GError *error;

  //server->config_timestamp = get_timestamp (config_filename);
  error = NULL;
  kf = g_key_file_new ();
  g_key_file_load_from_file (kf, config_filename,
      G_KEY_FILE_KEEP_COMMENTS, &error);
  if (error) {
    g_error_free (error);
  }

  error = NULL;
  s = g_key_file_get_string (kf, "video", "eth0_name", &error);
  if (s) {
    g_free (server->server_name);
    server->server_name = s;
    g_free (server->base_url);
    if (server->port == 80) {
      server->base_url = g_strdup_printf("http://%s", server->server_name);
    } else {
      server->base_url = g_strdup_printf("http://%s:%d", server->server_name,
          server->port);
    }
  }
  if (error) {
    g_error_free (error);
  }

  g_key_file_free (kf);
}

/* set up a stream follower */

void
ew_program_add_stream_follow (EwProgram *program, int type, int width,
    int height, int bitrate, const char *url)
{
  EwServerStream *stream;

  stream = ew_program_add_stream_full (program, type, width, height,
      bitrate, NULL);
  stream->follow_url = g_strdup (url);

  ew_stream_create_follow_pipeline (stream);
 
  gst_element_set_state (stream->pipeline, GST_STATE_PLAYING);
}

void
ew_stream_create_follow_pipeline (EwServerStream *stream)
{
  GstElement *pipe;
  GstElement *e;
  GString *pipe_desc;
  GError *error = NULL;
  GstBus *bus;

  pipe_desc = g_string_new ("");

  g_string_append_printf (pipe_desc, "souphttpsrc name=src do-timestamp=true ! ");
  switch (stream->type) {
    case EW_SERVER_STREAM_OGG:
      g_string_append (pipe_desc, "oggparse name=parse ! ");
      break;
    case EW_SERVER_STREAM_TS:
    case EW_SERVER_STREAM_TS_MAIN:
      g_string_append (pipe_desc, "mpegtsparse name=parse ! ");
      break;
    case EW_SERVER_STREAM_WEBM:
      g_string_append (pipe_desc, "matroskaparse name=parse ! ");
      break;
  }
  g_string_append (pipe_desc, "queue ! ");
  g_string_append_printf (pipe_desc, "%s name=sink ",
      ew_server_get_multifdsink_string ());

  error = NULL;
  pipe = gst_parse_launch (pipe_desc->str, &error);
  if (error != NULL) {
    if (verbose) g_print("pipeline parse error: %s\n", error->message);
  }
  g_string_free (pipe_desc, TRUE);

  e = gst_bin_get_by_name (GST_BIN(pipe), "src");
  g_assert(e != NULL);
  g_object_set (e, "location", stream->follow_url, NULL);
  g_object_unref (e);

  e = gst_bin_get_by_name (GST_BIN(pipe), "sink");
  g_assert(e != NULL);
  ew_stream_set_sink (stream, e);
  g_object_unref (e);
  stream->pipeline = pipe;

  bus = gst_pipeline_get_bus (GST_PIPELINE(pipe));
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK(handle_pipeline_message),
      stream);
  g_object_unref (bus);

}

static void
handle_pipeline_message (GstBus *bus, GstMessage *message,
    gpointer user_data)
{
  EwServerStream *stream = user_data;
  EwProgram *program = stream->program;

  switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_STATE_CHANGED:
      {
        GstState newstate;
        GstState oldstate;
        GstState pending;
        
        gst_message_parse_state_changed (message, &oldstate, &newstate,
            &pending);
        
        if (0 && verbose) g_print("message: %s (%s,%s,%s) from %s\n",
            GST_MESSAGE_TYPE_NAME(message),
            gst_element_state_get_name (newstate),
            gst_element_state_get_name (oldstate),
            gst_element_state_get_name (pending),
            GST_MESSAGE_SRC_NAME(message));
            
        if (newstate == GST_STATE_PLAYING && message->src == GST_OBJECT(stream->pipeline)) {
          char *s;
          s = g_strdup_printf ("stream %s started", stream->name);
          ew_program_log (program, s);
          g_free (s);
        }
        //gst_element_set_state (stream->pipeline, GST_STATE_PLAYING);
      }
      break;
    case GST_MESSAGE_STREAM_STATUS:
      {
        GstStreamStatusType type;
        GstElement *owner;
        
        gst_message_parse_stream_status (message, &type, &owner);

        if (0 && verbose) g_print("message: %s (%d) from %s\n", GST_MESSAGE_TYPE_NAME(message),
            type,
            GST_MESSAGE_SRC_NAME(message));
      }     
      break;
    case GST_MESSAGE_ERROR:
      {
        GError *error;
        gchar *debug;
        char *s;
        
        gst_message_parse_error (message, &error, &debug);
        
        s = g_strdup_printf("Internal Error: %s (%s) from %s\n",
            error->message, debug, GST_MESSAGE_SRC_NAME(message));
        ew_program_log (program, s);
        g_free (s);

        program->restart_delay = 5;
        ew_program_stop (program);
      }
      break;
    case GST_MESSAGE_EOS:
      ew_program_log (program, "end of stream");
      stream->program->restart_delay = 5;
      ew_program_stop (program);
      break;
    case GST_MESSAGE_ELEMENT:
      break;
    default:
      break;
  }
}

void
ew_program_stop (EwProgram *program)
{
  int i;
  EwServerStream *stream;

  ew_program_log (program, "stop");

  for (i=0;i<program->n_streams;i++){
    stream = program->streams[i];
    gst_element_set_state (stream->pipeline, GST_STATE_NULL);
    if (stream->follow_url) {
      ew_stream_set_sink (stream, NULL);
      g_object_unref (stream->pipeline);
      stream->pipeline = NULL;
    }
  }

  if (program->follow_uri) {
    for (i=0;i<program->n_streams;i++){
      stream = program->streams[i];
      ew_stream_free (stream);
    }
    program->n_streams = 0;
  }
}

void
ew_program_start (EwProgram *program)
{
  int i;
  EwServerStream *stream;

  ew_program_log (program, "start");
  if (program->follow_uri) {
    ew_program_follow_get_list (program);
  } else {
    for (i=0;i<program->n_streams;i++){
      stream = program->streams[i];
      if (stream->follow_url) {
        ew_stream_create_follow_pipeline (stream);
      }
      gst_element_set_state (stream->pipeline, GST_STATE_PLAYING);
    }
  }
}

static void
follow_callback (SoupSession *session, SoupMessage *message, gpointer ptr)
{
  EwProgram *program = ptr;

  if (message->status_code == SOUP_STATUS_OK) {
    SoupBuffer *buffer;
    char **lines;
    int i;

    ew_program_log (program, "got list of streams");

    buffer = soup_message_body_flatten (message->response_body);

    lines = g_strsplit (buffer->data, "\n", -1);

    for(i=0;lines[i];i++){
      int n;
      int index;
      char type_str[10];
      char url[200];
      int width;
      int height;
      int bitrate;
      int type;

      n = sscanf (lines[i], "%d %9s %d %d %d %199s\n",
          &index, type_str, &width, &height, &bitrate, url);

      if (n == 6) {
        char *full_url;

        type = EW_SERVER_STREAM_UNKNOWN;
        if (strcmp (type_str, "ogg") == 0) {
          type = EW_SERVER_STREAM_OGG;
        } else if (strcmp (type_str, "webm") == 0) {
          type = EW_SERVER_STREAM_WEBM;
        } else if (strcmp (type_str, "mpeg-ts") == 0) {
          type = EW_SERVER_STREAM_TS;
        } else if (strcmp (type_str, "mpeg-ts-main") == 0) {
          type = EW_SERVER_STREAM_TS_MAIN;
        } else if (strcmp (type_str, "flv") == 0) {
          type = EW_SERVER_STREAM_FLV;
        }

        full_url = g_strdup_printf("http://%s%s", program->follow_host, url);
        ew_program_add_stream_follow (program, type, width, height, bitrate,
            full_url);
        g_free (full_url);
      }

    }

    g_strfreev (lines);

    soup_buffer_free (buffer);
  } else {
    ew_program_log (program, "failed to get list of streams");
    program->restart_delay = 10;
    ew_program_stop (program);
  }

}

void
ew_program_follow (EwProgram *program, const char *host, const char *stream)
{
  program->follow_uri = g_strdup_printf("http://%s/%s.list", host, stream);
  program->follow_host = g_strdup (host);
  program->restart_delay = 1;
}

void
ew_program_follow_get_list (EwProgram *program)
{
  SoupMessage *message;

  message = soup_message_new ("GET", program->follow_uri);

  soup_session_queue_message (session, message, follow_callback, program);
}


static gboolean
periodic_timer (gpointer data)
{
  EwServer *server = (EwServer *) data;
  int i;

  for (i = 0; i< server->n_programs; i++) {
    EwProgram *program = server->programs[i];

    if (program->restart_delay) {
      program->restart_delay--;
      if (program->restart_delay == 0) {
        ew_program_start (program);
      }
    }

  }

  return TRUE;
}

