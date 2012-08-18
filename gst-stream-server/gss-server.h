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
#include "gss-types.h"
#include "gss-session.h"
#include "gss-program.h"
#include "gss-metrics.h"
#include "gss-stream.h"
#include "gss-resource.h"
#include "gss-transaction.h"

G_BEGIN_DECLS


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
  GstObject object;

  GssConfig *config;
  //char * config_filename;
  
  /* properties */
  gboolean enable_public_interface;
  int http_port;
  int https_port;
  char *server_hostname;
  char *title;
  int max_connections;
  int max_rate;
  char *admin_hosts_allow;
  gboolean enable_html5_video;
  gboolean enable_cortado;
  gboolean enable_flash;
  gboolean enable_rtsp;
  gboolean enable_rtmp;

  gboolean enable_programs;
  GList *programs;
  GssMetrics *metrics;

  SoupServer *server;
  SoupServer *ssl_server;
  SoupSession *client_session;
  char *base_url;
  char *base_url_https;
  GHashTable *resources;

  GstRTSPServer *rtsp_server;

  //time_t config_timestamp;

  GList *messages;
  int n_messages;

  GssFooterHtml *footer_html;
  void *footer_html_priv;

  GList *admin_resources;
  GList *featured_resources;
  char *archive_dir;

  void (*add_warnings) (GssTransaction *t, void *priv);
  void *add_warnings_priv;
};

struct _GssServerClass
{
  GstObjectClass object_class;

};


GType gss_server_get_type (void);

GssServer * gss_server_new (void);
void gss_server_set_server_hostname (GssServer *server, const char *hostname);
void gss_server_read_config (GssServer *server, const char *config_filename);

GssProgram * gss_server_add_program (GssServer *server, const char *program_name);
void gss_server_remove_program (GssServer *server, GssProgram *program);
void gss_server_follow_all (GssProgram *program, const char *host);
void gss_server_set_footer_html (GssServer *server, GssFooterHtml footer_html,
    gpointer priv);
void gss_server_set_title (GssServer *server, const char *title);

const char * gss_server_get_multifdsink_string (void);

void gss_server_deinit (void);

void gss_server_add_admin_callbacks (GssServer *server, SoupServer *soupserver);
GssProgram * gss_server_get_program_by_name (GssServer *server, const char *name);

void gss_server_log (GssServer *server, char *message);

void gss_server_add_static_file (SoupServer *soupserver, const char *filename,
    const char *content_type);
void gss_server_add_static_string (SoupServer * soupserver,
    const char *filename, const char *content_type, const char *string);

GssResource *gss_server_add_resource (GssServer *server, const char *location,
    GssResourceFlags flags, const char *content_type,
    GssTransactionCallback get_callback,
    GssTransactionCallback put_callback, GssTransactionCallback post_callback,
    gpointer priv);
void gss_server_remove_resource (GssServer *server, const char *location);
void gss_server_add_file_resource (GssServer *server,
    const char *filename, GssResourceFlags flags, const char *content_type);
void gss_server_add_static_resource (GssServer * server, const char *filename,
    GssResourceFlags flags, const char *content_type, const char *string,
    int len);
void gss_server_add_string_resource (GssServer * server, const char *filename,
    GssResourceFlags flags, const char *content_type, const char *string);

void gss_server_add_admin_resource (GssServer * server, GssResource *resource,
    const char *name);
void gss_server_add_featured_resource (GssServer * server, GssResource *resource,
    const char *name);

void gss_server_add_resource_simple (GssServer * server, GssResource * r);
void gss_server_add_program_simple (GssServer * server, GssProgram * program);

void gss_server_disable_programs (GssServer *server);

void gss_server_add_warnings_callback (GssServer *server, void (*add_warnings_func)(GssTransaction *t, void *priv),
    void *priv);



G_END_DECLS

#endif

