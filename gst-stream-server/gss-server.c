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
#include "gss-rtsp.h"

#include <glib/gstdio.h>

#include <sys/ioctl.h>
#include <net/if.h>


#define BASE "/"

#define enable_video_tag TRUE
#define enable_flash TRUE
#define enable_cortado FALSE

#define verbose FALSE

enum
{
  PROP_PORT = 1
};

char *get_time_string (void);

static void resource_callback (SoupServer * soupserver, SoupMessage * msg,
    const char *path, GHashTable * query, SoupClientContext * client,
    gpointer user_data);

static void main_page_resource (GssTransaction *transaction);
static void list_resource (GssTransaction *transaction);
static void log_resource (GssTransaction *transaction);
#if 0
static void push_resource (GssTransaction *transaction);
#endif

static void push_wrote_headers (SoupMessage * msg, void *user_data);
static void file_resource (GssTransaction *transaction);
static void program_get_resource (GssTransaction *transaction);
static void program_put_resource (GssTransaction *transaction);
static void program_frag_resource (GssTransaction *transaction);
static void program_list_resource (GssTransaction *transaction);
static void program_png_resource (GssTransaction *transaction);
static void program_jpeg_resource (GssTransaction *transaction);

static void gss_server_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gss_server_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void setup_paths (GssServer * server);

static void gss_server_notify (const char *key, void *priv);

static void
client_removed (GstElement * e, int arg0, int arg1, gpointer user_data);
static void client_fd_removed (GstElement * e, int fd, gpointer user_data);
static void msg_wrote_headers (SoupMessage * msg, void *user_data);
static void
gss_stream_handle (SoupServer * server, SoupMessage * msg,
    const char *path, GHashTable * query, SoupClientContext * client,
    gpointer user_data);
static void
gss_stream_handle_m3u8 (SoupServer * server, SoupMessage * msg,
    const char *path, GHashTable * query, SoupClientContext * client,
    gpointer user_data);
static gboolean periodic_timer (gpointer data);
static void jpeg_wrote_headers (SoupMessage * msg, void *user_data);

static void
handle_pipeline_message (GstBus * bus, GstMessage * message,
    gpointer user_data);

void gss_program_stop (GssProgram * program);
void gss_program_start (GssProgram * program);
void gss_stream_set_sink (GssServerStream * stream, GstElement * sink);
void gss_stream_create_follow_pipeline (GssServerStream * stream);
void gss_stream_create_push_pipeline (GssServerStream * stream);

#define MAX_FDS 65536
static void *fd_table[MAX_FDS];

G_DEFINE_TYPE (GssServer, gss_server, G_TYPE_OBJECT);

#define DEFAULT_HTTP_PORT 80
#define DEFAULT_HTTPS_PORT 443

static gboolean enable_rtsp = FALSE;

static const gchar *soup_method_source;
#define SOUP_METHOD_SOURCE (soup_method_source)


static void
gss_server_init (GssServer * server)
{
  server->resources = g_hash_table_new (g_str_hash, g_str_equal);

  server->client_session = soup_session_async_new ();

  if (getuid () == 0) {
    server->port = DEFAULT_HTTP_PORT;
    server->https_port = DEFAULT_HTTPS_PORT;
  } else {
    server->port = 8000 + DEFAULT_HTTP_PORT;
    server->https_port = 8000 + DEFAULT_HTTPS_PORT;
  }

  server->n_programs = 0;
  server->programs = NULL;

  if (enable_rtsp)
    gss_server_rtsp_init (server);
}

void
gss_server_deinit (void)
{

}

void
gss_server_log (GssServer * server, char *message)
{
  if (verbose)
    g_print ("%s\n", message);
  server->messages = g_list_append (server->messages, message);
  server->n_messages++;
  while (server->n_messages > 50) {
    g_free (server->messages->data);
    server->messages = g_list_delete_link (server->messages, server->messages);
    server->n_messages--;
  }

}

static void
gss_server_class_init (GssServerClass * server_class)
{
  soup_method_source = g_intern_static_string ("SOURCE");

  G_OBJECT_CLASS (server_class)->set_property = gss_server_set_property;
  G_OBJECT_CLASS (server_class)->get_property = gss_server_get_property;

  g_object_class_install_property (G_OBJECT_CLASS (server_class), PROP_PORT,
      g_param_spec_int ("port", "Port",
          "Port", 0, 65535, DEFAULT_HTTP_PORT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

}


static void
gss_server_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GssServer *server;

  server = GSS_SERVER (object);

  switch (prop_id) {
    case PROP_PORT:
      server->port = g_value_get_int (value);
      break;
  }
}

static void
gss_server_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GssServer *server;

  server = GSS_SERVER (object);

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

  memset (&ifr, 0, sizeof (ifr));
  strcpy (ifr.ifr_name, "eth0");

  ret = ioctl (sock, SIOCGIFADDR, &ifr);
  if (ret == 0) {
    struct sockaddr_in *sa = (struct sockaddr_in *) &ifr.ifr_addr;
    guint32 quad = ntohl (sa->sin_addr.s_addr);

    return g_strdup_printf ("%d.%d.%d.%d", (quad >> 24) & 0xff,
        (quad >> 16) & 0xff, (quad >> 8) & 0xff, (quad >> 0) & 0xff);
  }

  return strdup ("127.0.0.1");
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

GssServer *
gss_server_new (void)
{
  GssServer *server;
  SoupAddress *if6;

  server = g_object_new (GSS_TYPE_SERVER, NULL);

  server->config = gss_config_new ();

  gss_config_set_notify (server->config, "max_connections", gss_server_notify,
      server);
  gss_config_set_notify (server->config, "max_bandwidth", gss_server_notify,
      server);
  gss_config_set_notify (server->config, "server_name", gss_server_notify,
      server);
  gss_config_set_notify (server->config, "server_port", gss_server_notify,
      server);
  gss_config_set_notify (server->config, "enable_public_ui", gss_server_notify,
      server);

  //server->config_filename = "/opt/entropywave/ew-oberon/config";
  server->server_name = gethostname_alloc ();

  if (server->port == 80) {
    server->base_url = g_strdup_printf ("http://%s", server->server_name);
  } else {
    server->base_url = g_strdup_printf ("http://%s:%d", server->server_name,
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

  soup_server_add_handler (server->server, "/", resource_callback,
      server, NULL);

  server->ssl_server = soup_server_new (SOUP_SERVER_PORT,
      DEFAULT_HTTPS_PORT,
      SOUP_SERVER_SSL_CERT_FILE, "server.crt",
      SOUP_SERVER_SSL_KEY_FILE, "server.key", NULL);
  if (!server->ssl_server) {
    server->ssl_server = soup_server_new (SOUP_SERVER_PORT,
        8000 + DEFAULT_HTTPS_PORT,
        SOUP_SERVER_SSL_CERT_FILE, "server.crt",
        SOUP_SERVER_SSL_KEY_FILE, "server.key", NULL);
  }

  if (server->ssl_server) {
    soup_server_add_handler (server->ssl_server, "/", resource_callback,
        server, NULL);
  }

  setup_paths (server);

  soup_server_run_async (server->server);
  if (server->ssl_server) {
    soup_server_run_async (server->ssl_server);
  }

  g_timeout_add (1000, (GSourceFunc) periodic_timer, server);

  server->max_connections = INT_MAX;
  server->max_bitrate = G_MAXINT64;

  return server;
}

void
gss_server_add_resource (GssServer *server, const char *location,
    GssResourceFlags flags, GssTransactionCallback get_callback,
    GssTransactionCallback put_callback, GssTransactionCallback post_callback,
    gpointer priv)
{
  GssResource *resource;

  resource = g_new0 (GssResource, 1);
  resource->location = g_strdup (location);
  resource->flags = flags;
  resource->get_callback = get_callback;
  resource->put_callback = put_callback;
  resource->post_callback = post_callback;
  resource->priv = priv;

  g_hash_table_replace (server->resources, resource->location, resource);
}

void
gss_server_remove_resource (GssServer *server, const char *location)
{
  g_hash_table_remove (server->resources, location);
}

static void
setup_paths (GssServer * server)
{
  gss_session_add_session_callbacks (server);

  gss_server_add_resource (server, "/", GSS_RESOURCE_UI, main_page_resource,
      NULL, NULL, NULL);
  gss_server_add_resource (server, "/list", GSS_RESOURCE_UI, list_resource,
      NULL, NULL, NULL);
  gss_server_add_resource (server, "/log", GSS_RESOURCE_UI, log_resource,
      NULL, NULL, NULL);
#if 0
  gss_server_add_resource (server, "/push", GSS_RESOURCE_UI, NULL,
      push_resource, NULL, NULL);
#endif

  if (enable_cortado) {
    gss_server_add_file_resource (server, "/cortado.jar", 0,
        "application/java-archive");
  }

  if (enable_flash) {
    gss_server_add_file_resource (server, "/OSplayer.swf", 0,
        "application/x-shockwave-flash");
    gss_server_add_file_resource (server, "/AC_RunActiveContent.js", 0,
        "application/javascript");
  }
#define IMAGE(image) \
  gss_server_add_file_resource (server, "/images/" image , 0, "image/png")

  IMAGE ("button_access.png");
  IMAGE ("button_admin.png");
  IMAGE ("button_edit.png");
  IMAGE ("button_events.png");
  IMAGE ("button_log.png");
  IMAGE ("button_main.png");
  IMAGE ("button_network.png");
  IMAGE ("button_server.png");
  IMAGE ("button_video.png");
  IMAGE ("button_input.png");
  IMAGE ("button_output.png");
  IMAGE ("template_bodybg.png");
  IMAGE ("template_c1000.png");
  IMAGE ("template_e1000.png");
  IMAGE ("template_footer.png");
  IMAGE ("template_header_nologo.png");
  IMAGE ("template_navadmin.png");
  IMAGE ("template_navlog.png");
  IMAGE ("template_navmain.png");
  IMAGE ("template_navnet.png");
  IMAGE ("template_s1000.png");

  gss_server_add_string_resource (server, "/robots.txt", 0,
      "text/plain", "User-agent: *\nDisallow: /\n");
}

typedef struct _GssStaticResource GssStaticResource;
struct _GssStaticResource
{
  GssResource resource;

  const char *filename;
  const char *mime_type;
  char *contents;
  gsize size;
};

static void
generate_etag (GssStaticResource *sr)
{
  GChecksum *checksum;
  gsize n;
  guchar digest[32];

  n = g_checksum_type_get_length (G_CHECKSUM_MD5);
  g_assert (n <= 32);
  checksum = g_checksum_new (G_CHECKSUM_MD5);

  g_checksum_update (checksum, (guchar *)sr->contents, sr->size);
  g_checksum_get_digest (checksum, digest, &n);
  sr->resource.etag = g_base64_encode (digest, n);
  /* remove the trailing = (for MD5) */
  sr->resource.etag[22] = 0;
  g_checksum_free (checksum);
}

static void
file_resource (GssTransaction *t)
{
  GssStaticResource *sr = (GssStaticResource *)t->resource;

  soup_message_headers_replace (t->msg->response_headers, "Keep-Alive",
      "timeout=5, max=100");
  soup_message_headers_append (t->msg->response_headers, "Etag",
      sr->resource.etag);

  soup_message_set_status (t->msg, SOUP_STATUS_OK);

  soup_message_set_response (t->msg, sr->mime_type,
      SOUP_MEMORY_STATIC, sr->contents, sr->size);
}

void
gss_server_add_file_resource (GssServer *server,
    const char *filename, GssResourceFlags flags, const char *mime_type)
{
  GssStaticResource *sr;
  gsize size = 0;
  char *contents;
  gboolean ret;
  GError *error = NULL;

  ret = g_file_get_contents (filename + 1, &contents, &size,
      &error);
  if (!ret) {
    g_error_free (error);
    if (verbose) g_print ("missing file %s\n", filename);
    return;
  }

  sr = g_new0 (GssStaticResource, 1);

  sr->filename = filename;
  sr->mime_type = mime_type;
  sr->contents = contents;
  sr->size = size;

  sr->resource.location = g_strdup (filename);
  sr->resource.flags = flags;
  sr->resource.get_callback = file_resource;
  generate_etag (sr);

  g_hash_table_replace (server->resources, sr->resource.location,
      (GssResource *)sr);
}

void
gss_server_add_string_resource (GssServer * server, const char *filename,
    GssResourceFlags flags, const char *mime_type, const char *string)
{
  GssStaticResource *sr;

  sr = g_new0 (GssStaticResource, 1);

  sr->filename = filename;
  sr->mime_type = mime_type;
  sr->resource.etag = gss_session_create_id ();
  sr->contents = g_strdup (string);
  sr->size = strlen (string);
  generate_etag (sr);

  sr->resource.location = g_strdup (filename);
  sr->resource.flags = flags;
  sr->resource.get_callback = file_resource;

  g_hash_table_replace (server->resources, sr->resource.location,
      (GssResource *)sr);
}

void
gss_server_free (GssServer * server)
{
  int i;

  for (i = 0; i < server->n_programs; i++) {
    GssProgram *program = server->programs[i];

    gss_program_free (program);
  }

  if (server->server)
    g_object_unref (server->server);

  g_list_foreach (server->messages, (GFunc) g_free, NULL);
  g_list_free (server->messages);

  g_free (server->base_url);
  g_free (server->server_name);
  g_free (server->programs);
  /* FIXME why? */
  //g_free (server);
}

static void
gss_server_notify (const char *key, void *priv)
{
  GssServer *server = (GssServer *) priv;
  const char *s;

  s = gss_config_get (server->config, "server_name");
  gss_server_set_hostname (server, s);

  s = gss_config_get (server->config, "max_connections");
  server->max_connections = strtol (s, NULL, 10);
  if (server->max_connections == 0) {
    server->max_connections = INT_MAX;
  }

  s = gss_config_get (server->config, "max_bandwidth");
  server->max_bitrate = (gint64) strtol (s, NULL, 10) * 8000;
  if (server->max_bitrate == 0) {
    server->max_bitrate = G_MAXINT64;
  }

  server->enable_public_ui = gss_config_value_is_on (server->config,
      "enable_public_ui");
}

void
gss_server_set_hostname (GssServer * server, const char *hostname)
{
  g_free (server->server_name);
  server->server_name = g_strdup (hostname);

  g_free (server->base_url);
  if (server->server_name[0]) {
    if (server->port == 80) {
      server->base_url = g_strdup_printf ("http://%s", server->server_name);
    } else {
      server->base_url = g_strdup_printf ("http://%s:%d", server->server_name,
          server->port);
    }
  } else {
    server->base_url = g_strdup ("");
  }
}

void
gss_server_follow_all (GssProgram * program, const char *host)
{

}

GssProgram *
gss_server_add_program (GssServer * server, const char *program_name)
{
  GssProgram *program;
  char *s;

  program = g_malloc0 (sizeof (GssProgram));

  server->programs = g_realloc (server->programs,
      sizeof (GssProgram *) * (server->n_programs + 1));
  server->programs[server->n_programs] = program;
  server->n_programs++;

  program->server = server;
  program->location = g_strdup (program_name);
  program->enable_streaming = TRUE;
  program->running = FALSE;

  s = g_strdup_printf ("/%s", program_name);
  gss_server_add_resource (server, s, GSS_RESOURCE_UI, program_get_resource,
      program_put_resource, NULL, program);
  g_free (s);

  s = g_strdup_printf ("/%s.frag", program_name);
  gss_server_add_resource (server, s, GSS_RESOURCE_UI, program_frag_resource,
      NULL, NULL, program);
  g_free (s);

  s = g_strdup_printf ("/%s.list", program_name);
  gss_server_add_resource (server, s, GSS_RESOURCE_UI, program_list_resource,
      NULL, NULL, program);
  g_free (s);

  s = g_strdup_printf ("/%s-snapshot.png", program_name);
  gss_server_add_resource (server, s, GSS_RESOURCE_UI, program_png_resource,
      NULL, NULL, program);
  g_free (s);

  s = g_strdup_printf ("/%s-snapshot.jpeg", program_name);
  gss_server_add_resource (server, s, GSS_RESOURCE_HTTP_ONLY,
      program_jpeg_resource, NULL, NULL, program);
  g_free (s);

  return program;
}

void
gss_program_set_jpegsink (GssProgram * program, GstElement * jpegsink)
{
  program->jpegsink = g_object_ref (jpegsink);

  g_signal_connect (jpegsink, "client-removed",
      G_CALLBACK (client_removed), NULL);
  g_signal_connect (jpegsink, "client-fd-removed",
      G_CALLBACK (client_fd_removed), NULL);
}

void
gss_server_remove_program (GssServer * server, GssProgram * program)
{

  int i;

  for (i = 0; i < server->n_programs; i++) {
    if (server->programs[i] == program) {
      if (i + 1 < server->n_programs) {
        memmove (server->programs + i, server->programs + i + 1,
            server->n_programs - i - 1);
      }
      server->n_programs--;
    }
  }

  gss_program_free (program);
}

void
gss_program_free (GssProgram * program)
{
  int i;

  for (i = 0; i < program->n_streams; i++) {
    GssServerStream *stream = program->streams[i];

    gss_stream_free (stream);
  }

  if (program->hls.variant_buffer) {
    soup_buffer_free (program->hls.variant_buffer);
  }

  if (program->pngappsink)
    g_object_unref (program->pngappsink);
  if (program->jpegsink)
    g_object_unref (program->jpegsink);
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

#if 0
  /* RFC 822 */
  strftime (thetime, 79, "%a, %d %b %y %T %z", tmp);    // RFC-822
#endif
  /* RFC 2822 */
  s = g_date_time_format (datetime, "%a, %d %b %Y %H:%M:%S %z");        // RFC-2822
  /* Workaround for a glib bug that was fixed some time ago */
  if (s[27] == '-')
    s[27] = '0';
#if 0
  /* RFC 3339, almost */
  strftime (thetime, 79, "%Y-%m-%d %H:%M:%S%z", tmp);
#endif

  g_date_time_unref (datetime);

  return s;
}

void
gss_program_log (GssProgram * program, const char *message, ...)
{
  char *thetime = get_time_string ();
  char *s;
  va_list varargs;

  g_return_if_fail (program);
  g_return_if_fail (message);

  va_start (varargs, message);
  s = g_strdup_vprintf (message, varargs);
  va_end (varargs);

  gss_server_log (program->server, g_strdup_printf ("%s: %s: %s",
          thetime, program->location, s));
  g_free (s);
  g_free (thetime);
}

void
gss_stream_free (GssServerStream * stream)
{
  int i;

  g_free (stream->name);
  g_free (stream->playlist_name);
  g_free (stream->codecs);
  g_free (stream->mime_type);
  g_free (stream->follow_url);

  for (i = 0; i < N_CHUNKS; i++) {
    GssHLSSegment *segment = &stream->chunks[i];

    if (segment->buffer) {
      soup_buffer_free (segment->buffer);
      g_free (segment->location);
    }
  }

  if (stream->hls.index_buffer) {
    soup_buffer_free (stream->hls.index_buffer);
  }

  gss_stream_set_sink (stream, NULL);
  if (stream->pipeline) {
    gst_element_set_state (GST_ELEMENT (stream->pipeline), GST_STATE_NULL);
    g_object_unref (stream->pipeline);
  }
  if (stream->adapter)
    g_object_unref (stream->adapter);

  g_free (stream);
}

void
gss_stream_get_stats (GssServerStream * stream, guint64 * in, guint64 * out)
{
  g_object_get (stream->sink, "bytes-to-serve", in, "bytes-served", out, NULL);
}

const char *
gss_server_get_multifdsink_string (void)
{
  return "multifdsink "
      "sync=false " "time-min=200000000 " "recover-policy=keyframe "
      //"recover-policy=latest "
      "unit-type=2 "
      "units-max=20000000000 "
      "units-soft-max=11000000000 "
      "sync-method=burst-keyframe " "burst-unit=2 " "burst-value=3000000000";
}

void
gss_program_add_stream (GssProgram * program, GssServerStream * stream)
{
  program->streams = g_realloc (program->streams,
      sizeof (GssProgram *) * (program->n_streams + 1));
  program->streams[program->n_streams] = stream;
  stream->index = program->n_streams;
  program->n_streams++;

  stream->program = program;
}

static void
client_removed (GstElement * e, int fd, int status, gpointer user_data)
{
  GssServerStream *stream = user_data;

  if (fd_table[fd]) {
    if (stream) {
      stream->n_clients--;
      stream->program->server->n_clients--;
    }
  }
}

static void
client_fd_removed (GstElement * e, int fd, gpointer user_data)
{
  GssServerStream *stream = user_data;
  SoupSocket *socket = fd_table[fd];

  if (socket) {
    soup_socket_disconnect (socket);
    fd_table[fd] = NULL;
  } else {
    stream->custom_client_fd_removed (stream, fd,
        stream->custom_user_data);
  }
}


static void
gss_stream_handle (SoupServer * server, SoupMessage * msg,
    const char *path, GHashTable * query, SoupClientContext * client,
    gpointer user_data)
{
  GssServerStream *stream = (GssServerStream *) user_data;
  GssServer *ewserver = stream->program->server;
  GssConnection *connection;

  if (msg->method != SOUP_METHOD_GET) {
    soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);
    return;
  }
  if (!stream->program->enable_streaming || !stream->program->running) {
    soup_message_set_status (msg, SOUP_STATUS_NO_CONTENT);
    return;
  }

  if (ewserver->n_clients >= ewserver->max_connections ||
      ewserver->current_bitrate + stream->bitrate >= ewserver->max_bitrate) {
    if (verbose)
      g_print ("n_clients %d max_connections %d\n",
          ewserver->n_clients, ewserver->max_connections);
    if (verbose)
      g_print ("current bitrate %" G_GINT64_FORMAT " bitrate %d max_bitrate %"
          G_GINT64_FORMAT "\n", ewserver->current_bitrate, stream->bitrate,
          ewserver->max_bitrate);
    soup_message_set_status (msg, SOUP_STATUS_SERVICE_UNAVAILABLE);
    return;
  }

  connection = g_malloc0 (sizeof (GssConnection));
  connection->msg = msg;
  connection->client = client;
  connection->stream = stream;

  soup_message_set_status (msg, SOUP_STATUS_OK);

  soup_message_headers_set_encoding (msg->response_headers, SOUP_ENCODING_EOF);
  soup_message_headers_replace (msg->response_headers, "Content-Type",
      stream->mime_type);

  g_signal_connect (msg, "wrote-headers", G_CALLBACK (msg_wrote_headers),
      connection);
}

static void
msg_wrote_headers (SoupMessage * msg, void *user_data)
{
  GssConnection *connection = user_data;
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
gss_stream_handle_m3u8 (SoupServer * server, SoupMessage * msg,
    const char *path, GHashTable * query, SoupClientContext * client,
    gpointer user_data)
{
  char *content;
  GssServerStream *stream = (GssServerStream *) user_data;

  content = g_strdup_printf ("#EXTM3U\n"
      "#EXT-X-TARGETDURATION:10\n"
      "#EXTINF:10,\n"
      "%s/%s\n", stream->program->server->base_url, stream->name);

  soup_message_set_status (msg, SOUP_STATUS_OK);

  soup_message_set_response (msg, "application/x-mpegurl", SOUP_MEMORY_TAKE,
      content, strlen (content));
}

GssServerStream *
gss_stream_new (int type, int width, int height, int bitrate)
{
  GssServerStream *stream;

  stream = g_malloc0 (sizeof (GssServerStream));

  stream->type = type;
  stream->width = width;
  stream->height = height;
  stream->bitrate = bitrate;

  switch (type) {
    case GSS_SERVER_STREAM_OGG:
      stream->mime_type = g_strdup ("video/ogg");
      stream->mod = "";
      stream->ext = "ogv";
      break;
    case GSS_SERVER_STREAM_WEBM:
      stream->mime_type = g_strdup ("video/webm");
      stream->mod = "";
      stream->ext = "webm";
      break;
    case GSS_SERVER_STREAM_TS:
      stream->mime_type = g_strdup ("video/mp2t");
      stream->mod = "";
      stream->ext = "ts";
      break;
    case GSS_SERVER_STREAM_TS_MAIN:
      stream->mime_type = g_strdup ("video/mp2t");
      stream->mod = "-main";
      stream->ext = "ts";
      break;
    case GSS_SERVER_STREAM_FLV:
      stream->mime_type = g_strdup ("video/x-flv");
      stream->mod = "";
      stream->ext = "flv";
      break;
  }

  return stream;
}

void
gss_program_enable_streaming (GssProgram * program)
{
  program->enable_streaming = TRUE;
}

void
gss_program_disable_streaming (GssProgram * program)
{
  int i;

  program->enable_streaming = FALSE;
  for (i = 0; i < program->n_streams; i++) {
    GssServerStream *stream = program->streams[i];
    g_signal_emit_by_name (stream->sink, "clear");
  }
}

void
gss_program_set_running (GssProgram * program, gboolean running)
{
  program->running = running;
}

GssServerStream *
gss_program_add_stream_full (GssProgram * program,
    int type, int width, int height, int bitrate, GstElement * sink)
{
  SoupServer *soupserver = program->server->server;
  GssServerStream *stream;
  char *s;

  stream = gss_stream_new (type, width, height, bitrate);
  gss_program_add_stream (program, stream);
  if (enable_rtsp) {
    if (type == GSS_SERVER_STREAM_OGG) {
      stream->rtsp_stream = gss_rtsp_stream_new (stream);
      gss_rtsp_stream_start (stream->rtsp_stream);
    }
  }

  stream->name = g_strdup_printf ("%s-%dx%d-%dkbps%s.%s", program->location,
      stream->width, stream->height, stream->bitrate / 1000, stream->mod,
      stream->ext);
  s = g_strdup_printf ("/%s", stream->name);
  soup_server_add_handler (soupserver, s, gss_stream_handle, stream, NULL);
  g_free (s);

  stream->playlist_name = g_strdup_printf ("%s-%dx%d-%dkbps%s-%s.m3u8",
      program->location,
      stream->width, stream->height, stream->bitrate / 1000, stream->mod,
      stream->ext);
  s = g_strdup_printf ("/%s", stream->playlist_name);
  soup_server_add_handler (soupserver, s, gss_stream_handle_m3u8, stream, NULL);
  g_free (s);

  gss_stream_set_sink (stream, sink);

  return stream;
}

void
gss_stream_set_sink (GssServerStream * stream, GstElement * sink)
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
    if (stream->type == GSS_SERVER_STREAM_TS ||
        stream->type == GSS_SERVER_STREAM_TS_MAIN) {
      gss_server_stream_add_hls (stream);
    }
  }
}

static void
resource_callback (SoupServer * soupserver, SoupMessage * msg,
    const char *path, GHashTable * query, SoupClientContext * client,
    gpointer user_data)
{
  GssServer *server = (GssServer *)user_data;
  GssResource *resource;
  GssTransaction *transaction;
  GssSession *session;

  resource = g_hash_table_lookup (server->resources, path);

  if (!resource) {
    gss_html_error_404 (msg);
    return;
  }

  if (resource->flags & GSS_RESOURCE_UI) {
    if (!server->enable_public_ui && soupserver == server->server) {
      gss_html_error_404 (msg);
      return;
    }
  }

  if (resource->flags & GSS_RESOURCE_HTTPS_ONLY) {
    if (soupserver != server->ssl_server) {
      gss_html_error_404 (msg);
      return;
    }
  }

  if (resource->flags & GSS_RESOURCE_HTTP_ONLY) {
    if (soupserver != server->server) {
      gss_html_error_404 (msg);
      return;
    }
  }

  session = gss_session_message_get_session (msg, query);

  if (resource->flags & GSS_RESOURCE_ADMIN) {
    if (session == NULL) {
      gss_html_error_404 (msg);
      return;
    }
  }

  if (resource->etag) {
    const char *inm;

    inm = soup_message_headers_get_one (msg->request_headers,
        "If-None-Match");
    if (inm && !strcmp (inm, resource->etag)) {
      soup_message_set_status (msg, SOUP_STATUS_NOT_MODIFIED);
      return;
    }
  }

  transaction = g_new0 (GssTransaction, 1);
  transaction->server = server;
  transaction->soupserver = soupserver;
  transaction->msg = msg;
  transaction->path = path;
  transaction->query = query;
  transaction->client = client;
  transaction->resource = resource;
  transaction->session = session;
  transaction->done = FALSE;

  if (msg->method == SOUP_METHOD_GET && resource->get_callback) {
    resource->get_callback (transaction);
  } else if (msg->method == SOUP_METHOD_PUT && resource->put_callback) {
    resource->put_callback (transaction);
  } else if (msg->method == SOUP_METHOD_POST && resource->post_callback) {
    resource->post_callback (transaction);
  } else if (msg->method == SOUP_METHOD_SOURCE && resource->put_callback) {
    resource->put_callback (transaction);
  } else {
    gss_html_error_404 (msg);
  }

  g_free (transaction);
}


static void
main_page_resource (GssTransaction *t)
{
  const char *mime_type = "text/html";
  char *content;
  GString *s;
  char *base_url;
  int i;

  if (t->soupserver == t->server->ssl_server) {
    base_url = gss_soup_get_base_url_http (t->server, t->msg);
  } else {
    base_url = g_strdup ("");
  }

  s = g_string_new ("");

  gss_html_header (t->server, s, "Entropy Wave Live Streaming");

  g_string_append_printf (s, "<div id=\"header\">\n");
  gss_html_append_image (s,
      BASE "images/template_header_nologo.png", 812, 36, NULL);
  g_string_append_printf (s,
      "</div><!-- end header div -->\n"
      "<div id=\"content\">\n" "<h1>Available Streams</h1>\n");

  for (i = 0; i < t->server->n_programs; i++) {
    GssProgram *program = t->server->programs[i];
    gss_html_append_break (s);
    if (program->running) {
      gss_html_append_image_printf (s,
          "%s/%s-snapshot.jpeg", 0, 0, "snapshot image", base_url,
          program->location);
      g_string_append_printf (s,
          "<a href=\"%s/%s\">%s</a>\n",
          base_url, program->location, program->location);
    } else {
      g_string_append_printf (s,
          "<div style=\"background-color:#000000;color:#ffffff;width:320px;height:180px;text-align:center;\">currently unavailable</div>\n"
          "<a href=\"%s/%s\">%s</a>\n",
          base_url, program->location, program->location);
    }
  }

  g_free (base_url);

  g_string_append (s, "</div><!-- end content div -->\n");

  gss_html_footer (t->server, s, NULL);

  content = g_string_free (s, FALSE);

  soup_message_set_status (t->msg, SOUP_STATUS_OK);

  soup_message_set_response (t->msg, mime_type, SOUP_MEMORY_TAKE,
      content, strlen (content));
}

static void
list_resource (GssTransaction *t)
{
  char *content;
  GString *s = g_string_new ("");
  int i;

  for (i = 0; i < t->server->n_programs; i++) {
    GssProgram *program = t->server->programs[i];
    g_string_append_printf (s, "%s\n", program->location);
  }

  content = g_string_free (s, FALSE);

  soup_message_set_status (t->msg, SOUP_STATUS_OK);

  soup_message_set_response (t->msg, "text/plain", SOUP_MEMORY_TAKE,
      content, strlen (content));
}

static void
log_resource (GssTransaction *t)
{
  char *content;
  GString *s = g_string_new ("");
  GList *g;
  char *time_string;

  time_string = get_time_string ();
  g_string_append_printf (s, "Server time: %s\n", time_string);
  g_free (time_string);
  g_string_append_printf (s, "Recent log messages:\n");

  for (g = g_list_first (t->server->messages); g; g = g_list_next (g)) {
    g_string_append_printf (s, "%s\n", (char *) g->data);
  }

  content = g_string_free (s, FALSE);

  soup_message_set_status (t->msg, SOUP_STATUS_OK);

  soup_message_set_response (t->msg, "text/plain", SOUP_MEMORY_TAKE,
      content, strlen (content));
}

#if 0
static void
dump_header (const char *name, const char *value, gpointer user_data)
{
  g_print ("%s: %s\n", name, value);
}

static void
gss_transaction_dump (GssTransaction *t)
{
  soup_message_headers_foreach (t->msg->request_headers, dump_header, NULL);
}
#endif

#if 0
static void
push_resource (GssTransaction *t)
{
  GssProgram *program;
  const char *content_type;

  program = gss_server_add_program (t->server, "push_stream");
  program->program_type = GSS_PROGRAM_HTTP_PUT;

  content_type = soup_message_headers_get_one (t->msg->request_headers,
      "Content-Type");
  if (content_type) {
    if (strcmp (content_type, "application/ogg") == 0) {
      program->push_media_type = GSS_SERVER_STREAM_OGG;
    } else if (strcmp (content_type, "video/webm") == 0) {
      program->push_media_type = GSS_SERVER_STREAM_WEBM;
    } else if (strcmp (content_type, "video/mpeg-ts") == 0) {
      program->push_media_type = GSS_SERVER_STREAM_TS;
    } else if (strcmp (content_type, "video/mp2t") == 0) {
      program->push_media_type = GSS_SERVER_STREAM_TS;
    } else if (strcmp (content_type, "video/x-flv") == 0) {
      program->push_media_type = GSS_SERVER_STREAM_FLV;
    } else {
      program->push_media_type = GSS_SERVER_STREAM_OGG;
    }
  } else {
    program->push_media_type = GSS_SERVER_STREAM_OGG;
  }

  gss_program_start (program);

  program->push_client = t->client;

  soup_message_set_status (t->msg, SOUP_STATUS_OK);

  soup_message_headers_set_encoding (t->msg->response_headers, SOUP_ENCODING_EOF);

  g_signal_connect (t->msg, "wrote-headers", G_CALLBACK (push_wrote_headers),
      program);

}
#endif

static void
push_wrote_headers (SoupMessage * msg, void *user_data)
{
  GssProgram *program = (GssProgram *) user_data;
  SoupSocket *socket;
  GssServerStream *stream;
  int fd;

  socket = soup_client_context_get_socket (program->push_client);
  fd = soup_socket_get_fd (socket);

  stream = gss_program_add_stream_full (program, program->push_media_type,
      640, 360, 600000, NULL);
  stream->push_fd = fd;
  gss_stream_create_push_pipeline (stream);

  gst_element_set_state (stream->pipeline, GST_STATE_PLAYING);
  program->running = TRUE;
}

#if 0
static void
file_callback (SoupServer * server, SoupMessage * msg,
    const char *path, GHashTable * query, SoupClientContext * client,
    gpointer user_data)
{
  char *contents;
  gboolean ret;
  gsize size;
  GError *error = NULL;
  const char *mime_type = user_data;

  ret = g_file_get_contents (path + 1, &contents, &size, &error);
  if (!ret) {
    gss_html_error_404 (msg);
    if (verbose)
      g_print ("missing file %s\n", path);
    return;
  }

  soup_message_set_status (msg, SOUP_STATUS_OK);

  soup_message_set_response (msg, mime_type, SOUP_MEMORY_TAKE, contents, size);
}
#endif



void
add_video_block (GssProgram * program, GString * s, int max_width,
    const char *base_url)
{
  int i;
  int width = 0;
  int height = 0;
  int flash_only = TRUE;

  if (!program->running) {
    g_string_append_printf (s,
        "<div style=\"background-color:#000000;color:#ffffff;width:320px;height:180px;text-align:center;vertical-align:middle;\">currently unavailable</div>\n");
  }

  for (i = 0; i < program->n_streams; i++) {
    GssServerStream *stream = program->streams[i];
    if (stream->width > width)
      width = stream->width;
    if (stream->height > height)
      height = stream->height;
    if (stream->type != GSS_SERVER_STREAM_FLV) {
      flash_only = FALSE;
    }
  }
  if (max_width != 0 && width > max_width) {
    height = max_width * 9 / 16;
    width = max_width;
  }

  if (enable_video_tag && !flash_only) {
    g_string_append_printf (s,
        "<video controls=\"controls\" autoplay=\"autoplay\" "
        "id=video width=\"%d\" height=\"%d\">\n", width, height);

    for (i = program->n_streams - 1; i >= 0; i--) {
      GssServerStream *stream = program->streams[i];
      if (stream->type == GSS_SERVER_STREAM_WEBM) {
        g_string_append_printf (s,
            "<source src=\"%s/%s\" type='video/webm; codecs=\"vp8, vorbis\"'>\n",
            base_url, stream->name);
      }
    }

    for (i = program->n_streams - 1; i >= 0; i--) {
      GssServerStream *stream = program->streams[i];
      if (stream->type == GSS_SERVER_STREAM_OGG) {
        g_string_append_printf (s,
            "<source src=\"%s/%s\" type='video/ogg; codecs=\"theora, vorbis\"'>\n",
            base_url, stream->name);
      }
    }

    for (i = program->n_streams - 1; i >= 0; i--) {
      GssServerStream *stream = program->streams[i];
      if (stream->type == GSS_SERVER_STREAM_TS ||
          stream->type == GSS_SERVER_STREAM_TS_MAIN) {
#if 0
        g_string_append_printf (s,
            "<source src=\"%s/%s\" type='video/x-mpegURL; codecs=\"avc1.42E01E, mp4a.40.2\"' >\n",
            base_url, stream->playlist_name);
#endif
        g_string_append_printf (s,
            "<source src=\"%s/%s.m3u8\" >\n", base_url, program->location);
        break;
      }
    }

  }

  if (enable_cortado) {
    for (i = 0; i < program->n_streams; i++) {
      GssServerStream *stream = program->streams[i];
      if (stream->type == GSS_SERVER_STREAM_OGG) {
        g_string_append_printf (s,
            "<applet code=\"com.fluendo.player.Cortado.class\"\n"
            "  archive=\"%s/cortado.jar\" width=\"%d\" height=\"%d\">\n"
            "    <param name=\"url\" value=\"%s/%s\"></param>\n"
            "</applet>\n", base_url, width, height, base_url, stream->name);
        break;
      }
    }
  }

  if (enable_flash) {
    for (i = 0; i < program->n_streams; i++) {
      GssServerStream *stream = program->streams[i];
      if (stream->type == GSS_SERVER_STREAM_FLV) {
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
            "&showTitle=yes\">\n", width, height + 24, base_url, stream->name);
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
            " </embed>\n", base_url, stream->name, width, height);
#endif
        if (program->enable_snapshot) {
          gss_html_append_image_printf (s,
              "%s/%s-snapshot.png", 0, 0, "snapshot image",
              base_url, program->location);
        }
        g_string_append_printf (s, " </object>\n");
        break;
      }

    }
  } else {
    if (program->enable_snapshot) {
      gss_html_append_image_printf (s,
          "%s/%s-snapshot.png", 0, 0, "snapshot image",
          base_url, program->location);
    }
  }

  if (enable_video_tag && !flash_only) {
    g_string_append (s, "</video>\n");
  }

}

static void
program_frag_resource (GssTransaction *t)
{
  char *content;
  GssProgram *program = (GssProgram *) t->resource->priv;
  GString *s = g_string_new ("");

  if (!program->enable_streaming) {
    soup_message_set_status (t->msg, SOUP_STATUS_NO_CONTENT);
    return;
  }

  add_video_block (program, s, 0, program->server->base_url);

  content = g_string_free (s, FALSE);

  soup_message_set_status (t->msg, SOUP_STATUS_OK);

  soup_message_set_response (t->msg, "text/plain", SOUP_MEMORY_TAKE,
      content, strlen (content));
}

static void
program_get_resource (GssTransaction *t)
{
  GssProgram *program = (GssProgram *) t->resource->priv;
  char *content;
  GString *s = g_string_new ("");
  const char *base_url = "";
  int i;

  gss_html_header (program->server, s, program->location);
  g_string_append_printf (s, "<div id=\"header\">\n");
  gss_html_append_image (s, BASE "images/template_header_nologo.png",
      812, 36, NULL);
  g_string_append_printf (s,
      "</div><!-- end header div -->\n" "<div id=\"content\">\n");

  g_string_append_printf (s, "<h1>Live Stream: %s</h1>\n", program->location);

  add_video_block (program, s, 0, "");

  gss_html_append_break (s);
  for (i = 0; i < program->n_streams; i++) {
    const char *typename = "Unknown";
    GssServerStream *stream = program->streams[i];

    switch (stream->type) {
      case GSS_SERVER_STREAM_OGG:
        typename = "Ogg/Theora";
        break;
      case GSS_SERVER_STREAM_WEBM:
        typename = "WebM";
        break;
      case GSS_SERVER_STREAM_TS:
        typename = "MPEG-TS";
        break;
      case GSS_SERVER_STREAM_TS_MAIN:
        typename = "MPEG-TS main";
        break;
      case GSS_SERVER_STREAM_FLV:
        typename = "FLV";
        break;
    }
    gss_html_append_break (s);
    g_string_append_printf (s,
        "%d: %s %dx%d %d kbps <a href=\"%s/%s\">stream</a> "
        "<a href=\"%s/%s\">playlist</a>\n", i, typename,
        stream->width, stream->height, stream->bitrate / 1000,
        base_url, stream->name, base_url, stream->playlist_name);
  }
  if (program->enable_hls) {
    gss_html_append_break (s);
    g_string_append_printf (s,
        "<a href=\"%s/%s.m3u8\">HLS</a>\n", base_url, program->location);
  }

  g_string_append (s, "</div><!-- end content div -->\n");

  gss_html_footer (program->server, s, NULL);

  content = g_string_free (s, FALSE);

  soup_message_set_status (t->msg, SOUP_STATUS_OK);

  soup_message_set_response (t->msg, "text/html", SOUP_MEMORY_TAKE,
      content, strlen (content));
}

static void
program_put_resource (GssTransaction *t)
{
  GssProgram *program = (GssProgram *) t->resource->priv;
  const char *content_type;

  if (program->push_client) {
    gss_program_log (program, "busy");
    soup_message_set_status (t->msg, SOUP_STATUS_CONFLICT);
    return;
  }

  content_type = soup_message_headers_get_one (t->msg->request_headers,
      "Content-Type");
  if (content_type) {
    if (strcmp (content_type, "application/ogg") == 0) {
      program->push_media_type = GSS_SERVER_STREAM_OGG;
    } else if (strcmp (content_type, "video/webm") == 0) {
      program->push_media_type = GSS_SERVER_STREAM_WEBM;
    } else if (strcmp (content_type, "video/mpeg-ts") == 0) {
      program->push_media_type = GSS_SERVER_STREAM_TS;
    } else if (strcmp (content_type, "video/mp2t") == 0) {
      program->push_media_type = GSS_SERVER_STREAM_TS;
    } else if (strcmp (content_type, "video/x-flv") == 0) {
      program->push_media_type = GSS_SERVER_STREAM_FLV;
    } else {
      program->push_media_type = GSS_SERVER_STREAM_OGG;
    }
  } else {
    program->push_media_type = GSS_SERVER_STREAM_OGG;
  }

  gss_program_start (program);

  program->program_type = GSS_PROGRAM_HTTP_PUT;
  program->push_client = t->client;

  soup_message_set_status (t->msg, SOUP_STATUS_OK);

  soup_message_headers_set_encoding (t->msg->response_headers, SOUP_ENCODING_EOF);

  g_signal_connect (t->msg, "wrote-headers", G_CALLBACK (push_wrote_headers),
      program);
}

static void
program_list_resource (GssTransaction *t)
{
  GssProgram *program = (GssProgram *) t->resource->priv;
  char *content;
  GString *s = g_string_new ("");
  int i;
  const char *base_url = "";

  for (i = 0; i < program->n_streams; i++) {
    GssServerStream *stream = program->streams[i];
    const char *typename = "unknown";
    switch (stream->type) {
      case GSS_SERVER_STREAM_OGG:
        typename = "ogg";
        break;
      case GSS_SERVER_STREAM_WEBM:
        typename = "webm";
        break;
      case GSS_SERVER_STREAM_TS:
        typename = "mpeg-ts";
        break;
      case GSS_SERVER_STREAM_TS_MAIN:
        typename = "mpeg-ts-main";
        break;
      case GSS_SERVER_STREAM_FLV:
        typename = "flv";
        break;
    }
    g_string_append_printf (s,
        "%d %s %d %d %d %s/%s\n", i, typename,
        stream->width, stream->height, stream->bitrate, base_url, stream->name);
  }

  content = g_string_free (s, FALSE);

  soup_message_set_status (t->msg, SOUP_STATUS_OK);

  soup_message_set_response (t->msg, "text/plain", SOUP_MEMORY_TAKE,
      content, strlen (content));
}

static void
program_png_resource (GssTransaction *t)
{
  GssProgram *program = (GssProgram *) t->resource->priv;
  GstBuffer *buffer = NULL;

  if (!program->enable_streaming || !program->running) {
    soup_message_set_status (t->msg, SOUP_STATUS_NO_CONTENT);
    return;
  }

  if (program->pngappsink) {
    g_object_get (program->pngappsink, "last-buffer", &buffer, NULL);
  }

  if (buffer) {
    soup_message_set_status (t->msg, SOUP_STATUS_OK);

    soup_message_set_response (t->msg, "image/png", SOUP_MEMORY_COPY,
        (void *) GST_BUFFER_DATA (buffer), GST_BUFFER_SIZE (buffer));

    gst_buffer_unref (buffer);
  } else {
    gss_html_error_404 (t->msg);
  }

}

static void
program_jpeg_resource (GssTransaction *t)
{
  GssProgram *program = (GssProgram *) t->resource->priv;
  GssConnection *connection;

  if (!program->enable_streaming || program->jpegsink == NULL) {
    soup_message_set_status (t->msg, SOUP_STATUS_NO_CONTENT);
    return;
  }

  connection = g_malloc0 (sizeof (GssConnection));
  connection->msg = t->msg;
  connection->client = t->client;
  connection->program = program;

  soup_message_set_status (t->msg, SOUP_STATUS_OK);

  soup_message_headers_set_encoding (t->msg->response_headers, SOUP_ENCODING_EOF);
  soup_message_headers_replace (t->msg->response_headers, "Content-Type",
      "multipart/x-mixed-replace;boundary=myboundary");

  g_signal_connect (t->msg, "wrote-headers", G_CALLBACK (jpeg_wrote_headers),
      connection);
}

static void
jpeg_wrote_headers (SoupMessage * msg, void *user_data)
{
  GssConnection *connection = user_data;
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
gss_server_read_config (GssServer * server, const char *config_filename)
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
      server->base_url = g_strdup_printf ("http://%s", server->server_name);
    } else {
      server->base_url = g_strdup_printf ("http://%s:%d", server->server_name,
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
gss_program_add_stream_follow (GssProgram * program, int type, int width,
    int height, int bitrate, const char *url)
{
  GssServerStream *stream;

  stream = gss_program_add_stream_full (program, type, width, height,
      bitrate, NULL);
  stream->follow_url = g_strdup (url);

  gss_stream_create_follow_pipeline (stream);

  gst_element_set_state (stream->pipeline, GST_STATE_PLAYING);
}

void
gss_stream_create_follow_pipeline (GssServerStream * stream)
{
  GstElement *pipe;
  GstElement *e;
  GString *pipe_desc;
  GError *error = NULL;
  GstBus *bus;

  pipe_desc = g_string_new ("");

  g_string_append_printf (pipe_desc,
      "souphttpsrc name=src do-timestamp=true ! ");
  switch (stream->type) {
    case GSS_SERVER_STREAM_OGG:
      g_string_append (pipe_desc, "oggparse name=parse ! ");
      break;
    case GSS_SERVER_STREAM_TS:
    case GSS_SERVER_STREAM_TS_MAIN:
      g_string_append (pipe_desc, "mpegtsparse name=parse ! ");
      break;
    case GSS_SERVER_STREAM_WEBM:
      g_string_append (pipe_desc, "matroskaparse name=parse ! ");
      break;
  }
  g_string_append (pipe_desc, "queue ! ");
  g_string_append_printf (pipe_desc, "%s name=sink ",
      gss_server_get_multifdsink_string ());

  if (verbose) {
    g_print ("pipeline: %s\n", pipe_desc->str);
  }
  error = NULL;
  pipe = gst_parse_launch (pipe_desc->str, &error);
  if (error != NULL) {
    if (verbose)
      g_print ("pipeline parse error: %s\n", error->message);
  }
  g_string_free (pipe_desc, TRUE);

  e = gst_bin_get_by_name (GST_BIN (pipe), "src");
  g_assert (e != NULL);
  g_object_set (e, "location", stream->follow_url, NULL);
  g_object_unref (e);

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

static gboolean
push_data_probe_callback (GstPad * pad, GstMiniObject * mo, gpointer user_data)
{
#if 0
  //GssServerStream *stream = (GssServerStream *) user_data;

  if (GST_IS_BUFFER (mo)) {
    GstBuffer *buffer = GST_BUFFER (mo);

    g_print ("push got %d bytes\n", GST_BUFFER_SIZE (buffer));
    //gst_util_dump_mem (GST_BUFFER_DATA(buffer), 16);
  }
#endif

  return TRUE;
}

void
gss_stream_create_push_pipeline (GssServerStream * stream)
{
  GstElement *pipe;
  GstElement *e;
  GString *pipe_desc;
  GError *error = NULL;
  GstBus *bus;

  pipe_desc = g_string_new ("");

  g_string_append_printf (pipe_desc, "fdsrc name=src do-timestamp=true ! ");
  switch (stream->type) {
    case GSS_SERVER_STREAM_OGG:
      g_string_append (pipe_desc, "oggparse name=parse ! ");
      break;
    case GSS_SERVER_STREAM_TS:
    case GSS_SERVER_STREAM_TS_MAIN:
      g_string_append (pipe_desc, "mpegtsparse name=parse ! ");
      break;
    case GSS_SERVER_STREAM_WEBM:
      g_string_append (pipe_desc, "matroskaparse name=parse ! ");
      break;
  }
  g_string_append (pipe_desc, "queue ! ");
  g_string_append_printf (pipe_desc, "%s name=sink ",
      gss_server_get_multifdsink_string ());

  if (verbose) {
    g_print ("pipeline: %s\n", pipe_desc->str);
  }
  error = NULL;
  pipe = gst_parse_launch (pipe_desc->str, &error);
  if (error != NULL) {
    if (verbose)
      g_print ("pipeline parse error: %s\n", error->message);
  }
  g_string_free (pipe_desc, TRUE);

  e = gst_bin_get_by_name (GST_BIN (pipe), "src");
  g_assert (e != NULL);
  g_object_set (e, "fd", stream->push_fd, NULL);
  gst_pad_add_data_probe (gst_element_get_pad (e, "src"),
      G_CALLBACK (push_data_probe_callback), stream);
  g_object_unref (e);

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
  GssServerStream *stream = user_data;
  GssProgram *program = stream->program;

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_STATE_CHANGED:
    {
      GstState newstate;
      GstState oldstate;
      GstState pending;

      gst_message_parse_state_changed (message, &oldstate, &newstate, &pending);

      if (0 && verbose)
        g_print ("message: %s (%s,%s,%s) from %s\n",
            GST_MESSAGE_TYPE_NAME (message),
            gst_element_state_get_name (newstate),
            gst_element_state_get_name (oldstate),
            gst_element_state_get_name (pending),
            GST_MESSAGE_SRC_NAME (message));

      if (newstate == GST_STATE_PLAYING
          && message->src == GST_OBJECT (stream->pipeline)) {
        char *s;
        s = g_strdup_printf ("stream %s started", stream->name);
        gss_program_log (program, s);
        g_free (s);
        program->running = TRUE;
      }
      //gst_element_set_state (stream->pipeline, GST_STATE_PLAYING);
    }
      break;
    case GST_MESSAGE_STREAM_STATUS:
    {
      GstStreamStatusType type;
      GstElement *owner;

      gst_message_parse_stream_status (message, &type, &owner);

      if (0 && verbose)
        g_print ("message: %s (%d) from %s\n", GST_MESSAGE_TYPE_NAME (message),
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
      gss_program_log (program, s);
      g_free (s);

      program->restart_delay = 5;
      gss_program_stop (program);
    }
      break;
    case GST_MESSAGE_EOS:
      gss_program_log (program, "end of stream");
      gss_program_stop (program);
      switch (program->program_type) {
        case GSS_PROGRAM_EW_FOLLOW:
        case GSS_PROGRAM_HTTP_FOLLOW:
          program->restart_delay = 5;
          break;
        case GSS_PROGRAM_HTTP_PUT:
        case GSS_PROGRAM_ICECAST:
          program->push_client = NULL;
          break;
        case GSS_PROGRAM_EW_CONTRIB:
          break;
        case GSS_PROGRAM_MANUAL:
        default:
          break;
      }
      break;
    case GST_MESSAGE_ELEMENT:
      break;
    default:
      break;
  }
}

void
gss_program_stop (GssProgram * program)
{
  int i;
  GssServerStream *stream;

  gss_program_log (program, "stop");

  for (i = 0; i < program->n_streams; i++) {
    stream = program->streams[i];

    gss_stream_set_sink (stream, NULL);
    if (stream->pipeline) {
      gst_element_set_state (stream->pipeline, GST_STATE_NULL);

      g_object_unref (stream->pipeline);
      stream->pipeline = NULL;
    }
  }

  if (program->program_type != GSS_PROGRAM_MANUAL) {
    for (i = 0; i < program->n_streams; i++) {
      stream = program->streams[i];
      gss_stream_free (stream);
    }
    program->n_streams = 0;
  }
}

void
gss_program_start (GssProgram * program)
{

  gss_program_log (program, "start");

  switch (program->program_type) {
    case GSS_PROGRAM_EW_FOLLOW:
      gss_program_follow_get_list (program);
      break;
    case GSS_PROGRAM_HTTP_FOLLOW:
      gss_program_add_stream_follow (program, GSS_SERVER_STREAM_OGG, 640, 360,
          700000, program->follow_uri);
      break;
    case GSS_PROGRAM_MANUAL:
    case GSS_PROGRAM_ICECAST:
    case GSS_PROGRAM_HTTP_PUT:
      break;
    default:
      g_warning ("not implemented");
      break;
  }
}

static void
follow_callback (SoupSession * session, SoupMessage * message, gpointer ptr)
{
  GssProgram *program = ptr;

  if (message->status_code == SOUP_STATUS_OK) {
    SoupBuffer *buffer;
    char **lines;
    int i;

    gss_program_log (program, "got list of streams");

    buffer = soup_message_body_flatten (message->response_body);

    lines = g_strsplit (buffer->data, "\n", -1);

    for (i = 0; lines[i]; i++) {
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

        type = GSS_SERVER_STREAM_UNKNOWN;
        if (strcmp (type_str, "ogg") == 0) {
          type = GSS_SERVER_STREAM_OGG;
        } else if (strcmp (type_str, "webm") == 0) {
          type = GSS_SERVER_STREAM_WEBM;
        } else if (strcmp (type_str, "mpeg-ts") == 0) {
          type = GSS_SERVER_STREAM_TS;
        } else if (strcmp (type_str, "mpeg-ts-main") == 0) {
          type = GSS_SERVER_STREAM_TS_MAIN;
        } else if (strcmp (type_str, "flv") == 0) {
          type = GSS_SERVER_STREAM_FLV;
        }

        full_url = g_strdup_printf ("http://%s%s", program->follow_host, url);
        gss_program_add_stream_follow (program, type, width, height, bitrate,
            full_url);
        g_free (full_url);
      }

    }

    g_strfreev (lines);

    soup_buffer_free (buffer);
  } else {
    gss_program_log (program, "failed to get list of streams");
    program->restart_delay = 10;
    gss_program_stop (program);
  }

}

void
gss_program_follow (GssProgram * program, const char *host, const char *stream)
{
  program->program_type = GSS_PROGRAM_EW_FOLLOW;
  program->follow_uri = g_strdup_printf ("http://%s/%s.list", host, stream);
  program->follow_host = g_strdup (host);
  program->restart_delay = 1;
}

void
gss_program_follow_get_list (GssProgram * program)
{
  SoupMessage *message;

  message = soup_message_new ("GET", program->follow_uri);

  soup_session_queue_message (program->server->client_session, message,
      follow_callback, program);
}


static gboolean
periodic_timer (gpointer data)
{
  GssServer *server = (GssServer *) data;
  int i;

  for (i = 0; i < server->n_programs; i++) {
    GssProgram *program = server->programs[i];

    if (program->restart_delay) {
      program->restart_delay--;
      if (program->restart_delay == 0) {
        gss_program_start (program);
      }
    }

  }

  return TRUE;
}

void
gss_program_http_follow (GssProgram * program, const char *uri)
{
  program->program_type = GSS_PROGRAM_HTTP_FOLLOW;
  program->follow_uri = g_strdup (uri);
  program->follow_host = g_strdup (uri);

  program->restart_delay = 1;
}

void
gss_program_ew_contrib (GssProgram * program)
{
  program->program_type = GSS_PROGRAM_EW_CONTRIB;

  program->restart_delay = 0;

}

void
gss_program_http_put (GssProgram * program)
{
  program->program_type = GSS_PROGRAM_HTTP_PUT;

  program->restart_delay = 0;

}

void
gss_program_icecast (GssProgram * program)
{
  program->program_type = GSS_PROGRAM_ICECAST;

  program->restart_delay = 0;

}
