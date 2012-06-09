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

#ifdef USE_LOCAL
#define DEFAULT_ARCHIVE_DIR "."
#else
#define DEFAULT_ARCHIVE_DIR "/mnt/sdb1"
#endif

#include "config.h"

#include "gss-server.h"
#include "gss-html.h"
#include "gss-session.h"
#include "gss-soup.h"
#include "gss-rtsp.h"
#include "gss-content.h"
#include "gss-vod.h"

#include <sys/ioctl.h>
#include <net/if.h>


#define BASE "/"

#define verbose FALSE

enum
{
  PROP_PORT = 1
};

char *get_time_string (void);

/* Server Resources */
static void gss_server_resource_main_page (GssTransaction * transaction);
static void gss_server_resource_list (GssTransaction * transaction);
static void gss_server_resource_log (GssTransaction * transaction);


/* GssServer internals */
static void gss_server_resource_callback (SoupServer * soupserver,
    SoupMessage * msg, const char *path, GHashTable * query,
    SoupClientContext * client, gpointer user_data);
static void gss_server_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gss_server_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gss_server_setup_resources (GssServer * server);

static void gss_server_notify (const char *key, void *priv);


/* misc */
static char *gethostname_alloc (void);
static void
client_removed (GstElement * e, int arg0, int arg1, gpointer user_data);
static void client_fd_removed (GstElement * e, int fd, gpointer user_data);
static void msg_wrote_headers (SoupMessage * msg, void *user_data);
static void stream_resource (GssTransaction * transaction);
static void gss_stream_handle_m3u8 (GssTransaction * transaction);
static gboolean periodic_timer (gpointer data);



/* FIXME move to gss-stream.c */
void *gss_stream_fd_table[GSS_STREAM_MAX_FDS];

G_DEFINE_TYPE (GssServer, gss_server, G_TYPE_OBJECT);

#define DEFAULT_HTTP_PORT 80
#define DEFAULT_HTTPS_PORT 443

static const gchar *soup_method_source;
#define SOUP_METHOD_SOURCE (soup_method_source)

static GObjectClass *parent_class;

static void
gss_server_init (GssServer * server)
{
  server->resources = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, (GDestroyNotify) gss_resource_free);

  server->client_session = soup_session_async_new ();

  if (getuid () == 0) {
    server->port = DEFAULT_HTTP_PORT;
    server->https_port = DEFAULT_HTTPS_PORT;
  } else {
    server->port = 8000 + DEFAULT_HTTP_PORT;
    server->https_port = 8000 + DEFAULT_HTTPS_PORT;
  }

  server->programs = NULL;
  server->archive_dir = g_strdup (".");

  server->title = g_strdup ("GStreamer Streaming Server");

  if (enable_rtsp)
    gss_server_rtsp_init (server);
}

void
gss_server_deinit (void)
{
  __gss_session_deinit ();

}

void
gss_server_log (GssServer * server, char *message)
{
  g_return_if_fail (server);
  g_return_if_fail (message);

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
gss_server_finalize (GObject * object)
{
  GssServer *server = GSS_SERVER (object);
  GList *g;

  for (g = server->programs; g; g = g_list_next (g)) {
    GssProgram *program = g->data;
    gss_program_free (program);
  }
  g_list_free (server->programs);

  if (server->server)
    g_object_unref (server->server);
  if (server->ssl_server)
    g_object_unref (server->ssl_server);

  g_list_foreach (server->messages, (GFunc) g_free, NULL);
  g_list_free (server->messages);

  g_hash_table_unref (server->resources);
  gss_metrics_free (server->metrics);
  gss_config_free (server->config);
  g_free (server->base_url);
  g_free (server->base_url_https);
  g_free (server->server_name);
  g_free (server->title);
  g_object_unref (server->client_session);

  parent_class->finalize (object);
}

static void
gss_server_class_init (GssServerClass * server_class)
{
  soup_method_source = g_intern_static_string ("SOURCE");

  G_OBJECT_CLASS (server_class)->set_property = gss_server_set_property;
  G_OBJECT_CLASS (server_class)->get_property = gss_server_get_property;
  G_OBJECT_CLASS (server_class)->finalize = gss_server_finalize;

  g_object_class_install_property (G_OBJECT_CLASS (server_class), PROP_PORT,
      g_param_spec_int ("port", "Port",
          "Port", 0, 65535, DEFAULT_HTTP_PORT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  parent_class = g_type_class_peek_parent (server_class);
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
    default:
      g_assert_not_reached ();
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
    default:
      g_assert_not_reached ();
      break;
  }
}

void
gss_server_set_footer_html (GssServer * server, GssFooterHtml footer_html,
    gpointer priv)
{
  server->footer_html = footer_html;
  server->footer_html_priv = priv;
}

void
gss_server_set_title (GssServer * server, const char *title)
{
  g_free (server->title);
  server->title = g_strdup (title);
}

static char *
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

static char *
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

static void
dump_header (const char *name, const char *value, gpointer user_data)
{
  g_print ("%s: %s\n", name, value);
}

static void
request_read (SoupServer * server, SoupMessage * msg,
    SoupClientContext * client, gpointer user_data)
{
  g_print ("request_read\n");

  soup_message_headers_foreach (msg->request_headers, dump_header, NULL);
}

GssServer *
gss_server_new (void)
{
  GssServer *server;
  SoupAddress *if6;

  server = g_object_new (GSS_TYPE_SERVER, NULL);

  server->config = gss_config_new ();
  server->metrics = gss_metrics_new ();

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

  if (server->https_port == 443) {
    server->base_url_https = g_strdup_printf ("https://%s",
        server->server_name);
  } else {
    server->base_url_https = g_strdup_printf ("https://%s:%d",
        server->server_name, server->port);
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

  soup_server_add_handler (server->server, "/", gss_server_resource_callback,
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
    soup_server_add_handler (server->ssl_server, "/",
        gss_server_resource_callback, server, NULL);
  }

  gss_server_setup_resources (server);

  soup_server_run_async (server->server);
  if (server->ssl_server) {
    soup_server_run_async (server->ssl_server);
  }

  if (0)
    g_signal_connect (server->server, "request-read", G_CALLBACK (request_read),
        server);

  g_timeout_add (1000, (GSourceFunc) periodic_timer, server);

  server->max_connections = INT_MAX;
  server->max_bitrate = G_MAXINT64;

  return server;
}

GssResource *
gss_server_add_resource (GssServer * server, const char *location,
    GssResourceFlags flags, const char *content_type,
    GssTransactionCallback get_callback,
    GssTransactionCallback put_callback, GssTransactionCallback post_callback,
    gpointer priv)
{
  GssResource *resource;

  resource = g_new0 (GssResource, 1);
  resource->location = g_strdup (location);
  resource->flags = flags;
  resource->content_type = content_type;
  resource->get_callback = get_callback;
  resource->put_callback = put_callback;
  resource->post_callback = post_callback;
  resource->priv = priv;

  g_hash_table_replace (server->resources, resource->location, resource);

  return resource;
}

void
gss_server_remove_resource (GssServer * server, const char *location)
{
  g_hash_table_remove (server->resources, location);
}

static void
gss_server_setup_resources (GssServer * server)
{
  gss_session_add_session_callbacks (server);

  gss_server_add_resource (server, "/", GSS_RESOURCE_UI, "text/html",
      gss_server_resource_main_page, NULL, NULL, NULL);
  gss_server_add_resource (server, "/list", GSS_RESOURCE_UI, "text/plain",
      gss_server_resource_list, NULL, NULL, NULL);
  gss_server_add_resource (server, "/log", GSS_RESOURCE_UI, "text/plain",
      gss_server_resource_log, NULL, NULL, NULL);

  gss_server_add_resource (server, "/about", GSS_RESOURCE_UI, "text/html",
      gss_resource_unimplemented, NULL, NULL, NULL);
  gss_server_add_resource (server, "/contact", GSS_RESOURCE_UI, "text/html",
      gss_resource_unimplemented, NULL, NULL, NULL);
  gss_server_add_resource (server, "/add_program", GSS_RESOURCE_UI, "text/html",
      gss_resource_unimplemented, NULL, NULL, NULL);
  gss_server_add_resource (server, "/dashboard", GSS_RESOURCE_UI, "text/html",
      gss_resource_unimplemented, NULL, NULL, NULL);
  gss_server_add_resource (server, "/profile", GSS_RESOURCE_UI, "text/html",
      gss_resource_unimplemented, NULL, NULL, NULL);
  gss_server_add_resource (server, "/monitor", GSS_RESOURCE_UI, "text/html",
      gss_resource_unimplemented, NULL, NULL, NULL);
  gss_server_add_resource (server, "/meep", GSS_RESOURCE_UI, "text/html",
      gss_resource_unimplemented, NULL, NULL, NULL);

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

  gss_server_add_static_resource (server, "/images/footer-entropywave.png",
      0, "image/png",
      gss_data_footer_entropywave_png, gss_data_footer_entropywave_png_len);

  gss_server_add_string_resource (server, "/robots.txt", 0,
      "text/plain", "User-agent: *\nDisallow: /\n");

  gss_server_add_static_resource (server, "/include.js", 0,
      "text/javascript", gss_data_include_js, gss_data_include_js_len);
  gss_server_add_static_resource (server,
      "/bootstrap/css/bootstrap-responsive.css", 0, "text/css",
      gss_data_bootstrap_responsive_css, gss_data_bootstrap_responsive_css_len);
  gss_server_add_static_resource (server,
      "/bootstrap/css/bootstrap.css", 0, "text/css",
      gss_data_bootstrap_css, gss_data_bootstrap_css_len);
  gss_server_add_static_resource (server,
      "/bootstrap/js/bootstrap.js", 0, "text/javascript",
      gss_data_bootstrap_js, gss_data_bootstrap_js_len);
  gss_server_add_static_resource (server,
      "/bootstrap/js/jquery.js", 0, "text/javascript",
      gss_data_jquery_js, gss_data_jquery_js_len);
  gss_server_add_static_resource (server,
      "/bootstrap/img/glyphicons-halflings.png", 0, "image/png",
      gss_data_glyphicons_halflings_png, gss_data_glyphicons_halflings_png_len);
  gss_server_add_static_resource (server,
      "/bootstrap/img/glyphicons-halflings-white.png", 0, "image/png",
      gss_data_glyphicons_halflings_white_png,
      gss_data_glyphicons_halflings_white_png_len);
  gss_server_add_static_resource (server,
      "/no-snapshot.png", 0, "image/png",
      gss_data_no_snapshot_png, gss_data_no_snapshot_png_len);
  gss_server_add_static_resource (server,
      "/offline.png", 0, "image/png",
      gss_data_offline_png, gss_data_offline_png_len);

  gss_vod_setup (server);
}

void
gss_server_add_resource_simple (GssServer * server, GssResource * r)
{
  g_hash_table_replace (server->resources, r->location, r);
}

void
gss_server_add_file_resource (GssServer * server,
    const char *filename, GssResourceFlags flags, const char *content_type)
{
  GssResource *r;

  r = gss_resource_new_file (filename, flags, content_type);
  if (r == NULL)
    return;
  gss_server_add_resource_simple (server, r);
}

void
gss_server_add_static_resource (GssServer * server, const char *filename,
    GssResourceFlags flags, const char *content_type, const char *string,
    int len)
{
  GssResource *r;

  r = gss_resource_new_static (filename, flags, content_type, string, len);
  gss_server_add_resource_simple (server, r);
}

void
gss_server_add_string_resource (GssServer * server, const char *filename,
    GssResourceFlags flags, const char *content_type, const char *string)
{
  GssResource *r;

  r = gss_resource_new_string (filename, flags, content_type, string);
  gss_server_add_resource_simple (server, r);
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

void
gss_server_add_program_simple (GssServer * server, GssProgram * program)
{
  server->programs = g_list_append (server->programs, program);
  program->server = server;
  gss_program_add_server_resources (program);
}

GssProgram *
gss_server_add_program (GssServer * server, const char *program_name)
{
  GssProgram *program;
  program = gss_program_new (program_name);
  gss_server_add_program_simple (server, program);
  return program;
}

void
gss_server_remove_program (GssServer * server, GssProgram * program)
{
  gss_program_remove_server_resources (program);
  server->programs = g_list_remove (server->programs, program);
  gss_program_free (program);
}

void
gss_server_add_admin_resource (GssServer * server, GssResource * resource,
    const char *name)
{
  resource->name = g_strdup (name);
  server->admin_resources = g_list_append (server->admin_resources, resource);
}

void
gss_server_add_featured_resource (GssServer * server, GssResource * resource,
    const char *name)
{
  resource->name = g_strdup (name);
  server->featured_resources =
      g_list_append (server->featured_resources, resource);
}

GssProgram *
gss_server_get_program_by_name (GssServer * server, const char *name)
{
  GList *g;

  for (g = server->programs; g; g = g_list_next (g)) {
    GssProgram *program = g->data;
    if (strcmp (program->location, name) == 0) {
      return program;
    }
  }
  return NULL;
}

char *
get_time_string (void)
{
  GDateTime *datetime;
  char *s;

  datetime = g_date_time_new_now_local ();

#if 0
  /* RFC 822 */
  strftime (thetime, 79, "%a, %d %b %y %T %z", tmp);
#endif
  /* RFC 2822 */
  s = g_date_time_format (datetime, "%a, %d %b %Y %H:%M:%S %z");
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
gss_stream_free (GssServerStream * stream)
{
  int i;

  g_free (stream->name);
  g_free (stream->playlist_name);
  g_free (stream->codecs);
  g_free (stream->content_type);
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
#define CLEANUP(x) do { \
  if (x) { \
    if (verbose && GST_OBJECT_REFCOUNT (x) != 1) \
      g_print( #x "refcount %d\n", GST_OBJECT_REFCOUNT (x)); \
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
  if (stream->adapter)
    g_object_unref (stream->adapter);
  gss_metrics_free (stream->metrics);

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

static void
client_removed (GstElement * e, int fd, int status, gpointer user_data)
{
  GssServerStream *stream = user_data;

  if (gss_stream_fd_table[fd]) {
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
  GssServerStream *stream = user_data;
  SoupSocket *sock = gss_stream_fd_table[fd];

  if (sock) {
    soup_socket_disconnect (sock);
    gss_stream_fd_table[fd] = NULL;
  } else {
    stream->custom_client_fd_removed (stream, fd, stream->custom_user_data);
  }
}


static void
stream_resource (GssTransaction * t)
{
  GssServerStream *stream = (GssServerStream *) t->resource->priv;
  GssConnection *connection;

  if (!stream->program->enable_streaming || !stream->program->running) {
    soup_message_set_status (t->msg, SOUP_STATUS_NO_CONTENT);
    return;
  }

  if (t->server->metrics->n_clients >= t->server->max_connections ||
      t->server->metrics->bitrate + stream->bitrate >= t->server->max_bitrate) {
    if (verbose)
      g_print ("n_clients %d max_connections %d\n",
          t->server->metrics->n_clients, t->server->max_connections);
    if (verbose)
      g_print ("current bitrate %" G_GINT64_FORMAT " bitrate %d max_bitrate %"
          G_GINT64_FORMAT "\n", t->server->metrics->bitrate, stream->bitrate,
          t->server->max_bitrate);
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
      stream->content_type);

  g_signal_connect (t->msg, "wrote-headers", G_CALLBACK (msg_wrote_headers),
      connection);
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
    GssServerStream *stream = connection->stream;

    g_signal_emit_by_name (connection->stream->sink, "add", fd);

    g_assert (fd < GSS_STREAM_MAX_FDS);
    gss_stream_fd_table[fd] = sock;

    gss_metrics_add_client (stream->metrics, stream->bitrate);
    gss_metrics_add_client (stream->program->metrics, stream->bitrate);
    gss_metrics_add_client (stream->program->server->metrics, stream->bitrate);
  } else {
    soup_socket_disconnect (sock);
  }

  g_free (connection);
}

static void
gss_stream_handle_m3u8 (GssTransaction * t)
{
  GssServerStream *stream = (GssServerStream *) t->resource->priv;
  char *content;

  content = g_strdup_printf ("#EXTM3U\n"
      "#EXT-X-TARGETDURATION:10\n"
      "#EXTINF:10,\n"
      "%s/%s\n", stream->program->server->base_url, stream->name);

  soup_message_set_status (t->msg, SOUP_STATUS_OK);

  soup_message_body_append (t->msg->response_body, SOUP_MEMORY_TAKE,
      content, strlen (content));
}

GssServerStream *
gss_stream_new (int type, int width, int height, int bitrate)
{
  GssServerStream *stream;

  stream = g_malloc0 (sizeof (GssServerStream));

  stream->metrics = gss_metrics_new ();

  stream->type = type;
  stream->width = width;
  stream->height = height;
  stream->bitrate = bitrate;

  switch (type) {
    case GSS_SERVER_STREAM_OGG:
      stream->content_type = g_strdup ("video/ogg");
      stream->mod = "";
      stream->ext = "ogv";
      break;
    case GSS_SERVER_STREAM_WEBM:
      stream->content_type = g_strdup ("video/webm");
      stream->mod = "";
      stream->ext = "webm";
      break;
    case GSS_SERVER_STREAM_TS:
      stream->content_type = g_strdup ("video/mp2t");
      stream->mod = "";
      stream->ext = "ts";
      break;
    case GSS_SERVER_STREAM_TS_MAIN:
      stream->content_type = g_strdup ("video/mp2t");
      stream->mod = "-main";
      stream->ext = "ts";
      break;
    case GSS_SERVER_STREAM_FLV:
      stream->content_type = g_strdup ("video/x-flv");
      stream->mod = "";
      stream->ext = "flv";
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  return stream;
}

GssServerStream *
gss_program_add_stream_full (GssProgram * program,
    int type, int width, int height, int bitrate, GstElement * sink)
{
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
  gss_server_add_resource (program->server, s, GSS_RESOURCE_HTTP_ONLY,
      stream->content_type, stream_resource, NULL, NULL, stream);
  g_free (s);

  stream->playlist_name = g_strdup_printf ("%s-%dx%d-%dkbps%s-%s.m3u8",
      program->location,
      stream->width, stream->height, stream->bitrate / 1000, stream->mod,
      stream->ext);
  s = g_strdup_printf ("/%s", stream->playlist_name);
  gss_server_add_resource (program->server, s, 0, "application/x-mpegurl",
      gss_stream_handle_m3u8, NULL, NULL, stream);
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
gss_server_resource_callback (SoupServer * soupserver, SoupMessage * msg,
    const char *path, GHashTable * query, SoupClientContext * client,
    gpointer user_data)
{
  GssServer *server = (GssServer *) user_data;
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

  session = gss_session_get_session (query);

  if (session && soupserver != server->ssl_server) {
    gss_session_invalidate (session);
    session = NULL;
  }

  if (resource->flags & GSS_RESOURCE_ADMIN) {
    if (session == NULL || !session->is_admin) {
      gss_html_error_404 (msg);
      return;
    }
  }

  if (resource->content_type) {
    soup_message_headers_replace (msg->response_headers, "Content-Type",
        resource->content_type);
  }

  if (resource->etag) {
    const char *inm;

    inm = soup_message_headers_get_one (msg->request_headers, "If-None-Match");
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

  if (resource->flags & GSS_RESOURCE_HTTP_ONLY) {
    if (soupserver != server->server) {
      gss_resource_onetime_redirect (transaction);
      g_free (transaction);
      return;
    }
  }

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

  if (transaction->s) {
    int len;
    gchar *content;

    len = transaction->s->len;
    content = g_string_free (transaction->s, FALSE);
    soup_message_body_append (msg->response_body, SOUP_MEMORY_TAKE,
        content, len);
    soup_message_set_status (msg, SOUP_STATUS_OK);
  }
#if 0
  soup_message_headers_replace (t->msg->response_headers, "Keep-Alive",
      "timeout=5, max=100");
#endif

  g_free (transaction);
}

static void
gss_server_resource_main_page (GssTransaction * t)
{
  GString *s;
  GList *g;

  s = t->s = g_string_new ("");

  gss_html_header (t);

  g_string_append_printf (s, "<h2>Input Media</h2>\n");

  g_string_append_printf (s, "<ul class='thumbnails'>\n");
  for (g = t->server->programs; g; g = g_list_next (g)) {
    GssProgram *program = g->data;

    if (program->is_archive)
      continue;

    g_string_append_printf (s, "<li class='span4'>\n");
    g_string_append_printf (s, "<div class='thumbnail'>\n");
    g_string_append_printf (s,
        "<a href=\"/%s%s%s\">",
        program->location,
        t->session ? "?session_id=" : "",
        t->session ? t->session->session_id : "");
    if (program->running) {
      if (program->jpegsink) {
        gss_html_append_image_printf (s,
            "/%s-snapshot.jpeg", 0, 0, "snapshot image", program->location);
      } else {
        g_string_append_printf (s, "<img src='/no-snapshot.png'>\n");
      }
    } else {
      g_string_append_printf (s, "<img src='/offline.png'>\n");
    }
    g_string_append_printf (s, "</a>\n");
    g_string_append_printf (s, "<h5>%s</h5>\n", program->location);
    g_string_append_printf (s, "</div>\n");
    g_string_append_printf (s, "</li>\n");
  }
  g_string_append_printf (s, "</ul>\n");

  g_string_append_printf (s, "<h2>Archived Media</h2>\n");

  g_string_append_printf (s, "<ul class='thumbnails'>\n");
  for (g = t->server->programs; g; g = g_list_next (g)) {
    GssProgram *program = g->data;

    if (!program->is_archive)
      continue;

    g_string_append_printf (s, "<li class='span4'>\n");
    //g_string_append_printf (s, "<div class='well' style='width:1000;'>\n");
    g_string_append_printf (s, "<div class='thumbnail'>\n");
    g_string_append_printf (s,
        "<a href=\"/%s%s%s\">",
        program->location,
        t->session ? "?session_id=" : "",
        t->session ? t->session->session_id : "");
    if (program->running) {
      if (program->jpegsink) {
        gss_html_append_image_printf (s,
            "/%s-snapshot.jpeg", 0, 0, "snapshot image", program->location);
      } else {
        g_string_append_printf (s, "<img src='/no-snapshot.png'>\n");
      }
    } else {
      g_string_append_printf (s, "<img src='/offline.png'>\n");
    }
    g_string_append_printf (s, "</a>\n");
    g_string_append_printf (s, "<h5>%s</h5>\n", program->location);
    g_string_append_printf (s, "</div>\n");
    g_string_append_printf (s, "</li>\n");
  }
  g_string_append_printf (s, "</ul>\n");

  gss_html_footer (t);
}

static void
gss_server_resource_list (GssTransaction * t)
{
  GString *s;
  GList *g;

  s = t->s = g_string_new ("");

  for (g = t->server->programs; g; g = g_list_next (g)) {
    GssProgram *program = g->data;
    g_string_append_printf (s, "%s\n", program->location);
  }
}

static void
gss_server_resource_log (GssTransaction * t)
{
  GString *s = g_string_new ("");
  GList *g;
  char *time_string;

  t->s = s;

  time_string = get_time_string ();
  g_string_append_printf (s, "Server time: %s\n", time_string);
  g_free (time_string);
  g_string_append_printf (s, "Recent log messages:\n");

  for (g = g_list_first (t->server->messages); g; g = g_list_next (g)) {
    g_string_append_printf (s, "%s\n", (char *) g->data);
  }
}

#if 0
static void
dump_header (const char *name, const char *value, gpointer user_data)
{
  g_print ("%s: %s\n", name, value);
}

static void
gss_transaction_dump (GssTransaction * t)
{
  soup_message_headers_foreach (t->msg->request_headers, dump_header, NULL);
}
#endif

#if 0
static void
push_callback (SoupServer * server, SoupMessage * msg,
    const char *path, GHashTable * query, SoupClientContext * client,
    gpointer user_data)
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

  soup_message_headers_set_encoding (t->msg->response_headers,
      SOUP_ENCODING_EOF);

  g_signal_connect (t->msg, "wrote-headers", G_CALLBACK (push_wrote_headers),
      program);

}
#endif

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
  const char *content_type = user_data;

  ret = g_file_get_contents (path + 1, &contents, &size, &error);
  if (!ret) {
    gss_html_error_404 (msg);
    if (verbose)
      g_print ("missing file %s\n", path);
    return;
  }

  soup_message_set_status (msg, SOUP_STATUS_OK);

  soup_message_set_response (msg, content_type, SOUP_MEMORY_TAKE, contents,
      size);
}
#endif


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


static gboolean
periodic_timer (gpointer data)
{
  GssServer *server = (GssServer *) data;
  GList *g;

  for (g = server->programs; g; g = g_list_next (g)) {
    GssProgram *program = g->data;

    if (program->restart_delay) {
      program->restart_delay--;
      if (program->restart_delay == 0) {
        gss_program_start (program);
      }
    }

  }

  return TRUE;
}
