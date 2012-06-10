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
#include "gss-content.h"
#include "gss-utils.h"
#include "gss-vod.h"



#define BASE "/"

#define verbose FALSE

enum
{
  PROP_PORT = 1,
  PROP_HTTPS_PORT,
  PROP_SERVER_NAME,
  PROP_TITLE,
  PROP_MAX_CONNECTIONS,
  PROP_MAX_BITRATE,
  PROP_ARCHIVE_DIR
};

#define DEFAULT_HTTP_PORT 80
#define DEFAULT_HTTPS_PORT 443
#define DEFAULT_SERVER_NAME NULL
#define DEFAULT_TITLE "GStreamer Streaming Server"
#define DEFAULT_MAX_CONNECTIONS G_MAXINT
#define DEFAULT_MAX_BITRATE G_MAXINT64
#ifdef USE_LOCAL
#define DEFAULT_ARCHIVE_DIR "."
#else
#define DEFAULT_ARCHIVE_DIR "/mnt/sdb1"
#endif

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


static gboolean periodic_timer (gpointer data);



G_DEFINE_TYPE (GssServer, gss_server, G_TYPE_OBJECT);


static const gchar *soup_method_source;
#define SOUP_METHOD_SOURCE (soup_method_source)

static GObjectClass *parent_class;

static void
gss_server_set_http_port (GssServer * server, int port)
{
  SoupAddress *if6;

  if (server->port == port)
    return;

  if (server->server) {
    g_object_unref (server->server);
  }

  server->port = port;

  g_free (server->base_url);
  if (server->port == 80) {
    server->base_url = g_strdup_printf ("http://%s", server->server_name);
  } else {
    server->base_url = g_strdup_printf ("http://%s:%d", server->server_name,
        server->port);
  }

  if6 = soup_address_new_any (SOUP_ADDRESS_FAMILY_IPV6, server->port);
  server->server = soup_server_new (SOUP_SERVER_INTERFACE, if6,
      SOUP_SERVER_PORT, server->port, NULL);

  if (server->server == NULL) {
    /* try again with just IPv4 */
    server->server = soup_server_new (SOUP_SERVER_PORT, server->port, NULL);
  }

  g_object_unref (if6);

  if (server->server) {
    soup_server_add_handler (server->server, "/", gss_server_resource_callback,
        server, NULL);
    soup_server_run_async (server->server);
  }
}

static void
gss_server_set_https_port (GssServer * server, int port)
{
  SoupAddress *if6;

  if (server->https_port == port)
    return;

  if (server->ssl_server) {
    g_object_unref (server->ssl_server);
  }

  server->https_port = port;

  if (server->https_port == 443) {
    server->base_url_https = g_strdup_printf ("https://%s",
        server->server_name);
  } else {
    server->base_url_https = g_strdup_printf ("https://%s:%d",
        server->server_name, server->port);
  }

  if6 = soup_address_new_any (SOUP_ADDRESS_FAMILY_IPV6, server->port);
  server->ssl_server = soup_server_new (SOUP_SERVER_PORT, server->https_port,
      SOUP_SERVER_INTERFACE, if6,
      SOUP_SERVER_SSL_CERT_FILE, "server.crt",
      SOUP_SERVER_SSL_KEY_FILE, "server.key", NULL);
  if (!server->ssl_server) {
    server->ssl_server = soup_server_new (SOUP_SERVER_PORT, server->https_port,
        SOUP_SERVER_SSL_CERT_FILE, "server.crt",
        SOUP_SERVER_SSL_KEY_FILE, "server.key", NULL);
  }

  g_object_unref (if6);

  if (server->ssl_server) {
    soup_server_add_handler (server->ssl_server, "/",
        gss_server_resource_callback, server, NULL);
    soup_server_run_async (server->ssl_server);
  }
}

static void
gss_server_init (GssServer * server)
{
  char *s;
  int port, https_port;

  server->config = gss_config_new ();
  server->metrics = gss_metrics_new ();

  server->resources = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, (GDestroyNotify) gss_resource_free);

  server->client_session = soup_session_async_new ();

  if (getuid () == 0) {
    port = DEFAULT_HTTP_PORT;
    https_port = DEFAULT_HTTPS_PORT;
  } else {
    port = 8000 + DEFAULT_HTTP_PORT;
    https_port = 8000 + DEFAULT_HTTPS_PORT;
  }

  gss_server_set_http_port (server, port);
  gss_server_set_https_port (server, https_port);

  server->max_connections = DEFAULT_MAX_CONNECTIONS;
  server->max_bitrate = DEFAULT_MAX_BITRATE;

  server->programs = NULL;
  server->archive_dir = g_strdup (DEFAULT_ARCHIVE_DIR);

  server->title = g_strdup (DEFAULT_TITLE);

  if (enable_rtsp)
    gss_server_rtsp_init (server);

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

  s = gss_utils_gethostname ();
  gss_server_set_server_name (server, s);
  g_free (s);
  gss_server_setup_resources (server);

  g_timeout_add (1000, (GSourceFunc) periodic_timer, server);
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
    g_assert (GSS_IS_PROGRAM (program));
    g_object_unref (program);
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
  g_object_class_install_property (G_OBJECT_CLASS (server_class),
      PROP_HTTPS_PORT, g_param_spec_int ("https-port", "HTTPS Port",
          "HTTPS Port", 0, 65535, DEFAULT_HTTPS_PORT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (G_OBJECT_CLASS (server_class),
      PROP_SERVER_NAME, g_param_spec_string ("server-name", "Server name",
          "Server name", DEFAULT_SERVER_NAME,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (G_OBJECT_CLASS (server_class), PROP_TITLE,
      g_param_spec_string ("title", "Title", "Title", DEFAULT_TITLE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (G_OBJECT_CLASS (server_class), PROP_PORT,
      g_param_spec_int ("max-connections", "Maximum number of connections",
          "Maximum number of connections", 0, G_MAXINT, 0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (G_OBJECT_CLASS (server_class), PROP_PORT,
      g_param_spec_int64 ("max-bitrate", "Maximum bitrate", "Maximum bitrate",
          0, G_MAXINT64, 0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (G_OBJECT_CLASS (server_class),
      PROP_ARCHIVE_DIR, g_param_spec_string ("archive-dir", "Archive Directory",
          "Archive Directory", DEFAULT_ARCHIVE_DIR,
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
      gss_server_set_http_port (server, g_value_get_int (value));
      break;
    case PROP_HTTPS_PORT:
      gss_server_set_https_port (server, g_value_get_int (value));
      break;
    case PROP_SERVER_NAME:
      gss_server_set_server_name (server, g_value_get_string (value));
      break;
    case PROP_TITLE:
      gss_server_set_title (server, g_value_get_string (value));
      break;
    case PROP_MAX_CONNECTIONS:
      server->max_connections = g_value_get_int (value);
      break;
    case PROP_MAX_BITRATE:
      server->max_bitrate = g_value_get_int64 (value);
      break;
    case PROP_ARCHIVE_DIR:
      g_free (server->archive_dir);
      server->archive_dir = g_value_dup_string (value);
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
    case PROP_HTTPS_PORT:
      g_value_set_int (value, server->https_port);
      break;
    case PROP_SERVER_NAME:
      g_value_set_string (value, server->server_name);
      break;
    case PROP_TITLE:
      g_value_set_string (value, server->title);
      break;
    case PROP_MAX_CONNECTIONS:
      g_value_set_int (value, server->max_connections);
      break;
    case PROP_MAX_BITRATE:
      g_value_set_int64 (value, server->max_bitrate);
      break;
    case PROP_ARCHIVE_DIR:
      g_value_set_string (value, server->archive_dir);
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

GssServer *
gss_server_new (void)
{
  GssServer *server;

  server = g_object_new (GSS_TYPE_SERVER, NULL);

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
  gss_server_set_server_name (server, s);

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
gss_server_set_server_name (GssServer * server, const char *hostname)
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
  g_return_if_fail (GSS_IS_SERVER (server));
  g_return_if_fail (GSS_IS_PROGRAM (program));

  gss_program_remove_server_resources (program);
  server->programs = g_list_remove (server->programs, program);
  g_object_unref (program);
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

const char *
gss_server_get_multifdsink_string (void)
{
  return "multifdsink "
      "sync=false " "time-min=200000000 " "recover-policy=keyframe "
      "unit-type=2 "
      "units-max=20000000000 "
      "units-soft-max=11000000000 "
      "sync-method=burst-keyframe " "burst-unit=2 " "burst-value=3000000000";
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

  time_string = gss_utils_get_time_string ();
  g_string_append_printf (s, "Server time: %s\n", time_string);
  g_free (time_string);
  g_string_append_printf (s, "Recent log messages:\n");

  for (g = g_list_first (t->server->messages); g; g = g_list_next (g)) {
    g_string_append_printf (s, "%s\n", (char *) g->data);
  }
}

void
gss_server_read_config (GssServer * server, const char *config_filename)
{
  GKeyFile *kf;
  char *s;
  GError *error;

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
