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

#include "gss-manager.h"
#include "gss-utils.h"
#include "gss-config.h"
#include "gss-html.h"
#include "gss-push.h"
#include "gss-pull.h"

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>

enum
{
  PROP_0,
  PROP_FOLLOW_HOSTS
};

#define DEFAULT_FOLLOW_HOSTS ""

static void gss_manager_finalize (GObject * object);
static void gss_manager_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gss_manager_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gss_manager_get_resource (GssTransaction * t);
static void gss_manager_post_resource (GssTransaction * t);

G_DEFINE_TYPE (GssManager, gss_manager, GSS_TYPE_OBJECT);

static GObjectClass *parent_class;

static void
gss_manager_init (GssManager * manager)
{
  manager->follow_hosts = g_strdup (DEFAULT_FOLLOW_HOSTS);
}

static void
gss_manager_class_init (GssManagerClass * manager_class)
{
  G_OBJECT_CLASS (manager_class)->set_property = gss_manager_set_property;
  G_OBJECT_CLASS (manager_class)->get_property = gss_manager_get_property;
  G_OBJECT_CLASS (manager_class)->finalize = gss_manager_finalize;

  g_object_class_install_property (G_OBJECT_CLASS (manager_class),
      PROP_FOLLOW_HOSTS, g_param_spec_string ("follow-hosts", "Follow Hosts",
          "Hostnames or IP addresses for Entropy Wave encoders or "
          "streaming servers to follow.", DEFAULT_FOLLOW_HOSTS,
          (GParamFlags) (GSS_PARAM_MULTILINE | G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS)));

  parent_class = g_type_class_peek_parent (manager_class);
}

static void
gss_manager_finalize (GObject * object)
{
  GssManager *manager;

  manager = GSS_MANAGER (object);

  g_free (manager->follow_hosts);

  parent_class->finalize (object);
}

static void
string_replace (char **s_ptr, char *s)
{
  char *old = *s_ptr;
  *s_ptr = s;
  g_free (old);
}

static void
gss_manager_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GssManager *manager;

  manager = GSS_MANAGER (object);

  switch (prop_id) {
    case PROP_FOLLOW_HOSTS:
      string_replace (&manager->follow_hosts, g_value_dup_string (value));
      break;
    default:
      g_assert_not_reached ();
      break;
  }
}

static void
gss_manager_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GssManager *manager;

  manager = GSS_MANAGER (object);

  switch (prop_id) {
    case PROP_FOLLOW_HOSTS:
      g_value_set_string (value, manager->follow_hosts);
      break;
    default:
      g_assert_not_reached ();
      break;
  }
}


GssManager *
gss_manager_new (void)
{
  GssManager *manager;

  manager = g_object_new (GSS_TYPE_MANAGER, "name", "admin.manager", NULL);

  return manager;
}

void
gss_manager_add_resources (GssManager * manager, GssServer * server)
{
  GssResource *r;

  r = gss_server_add_resource (server, "/admin/manager", GSS_RESOURCE_ADMIN,
      "text/html", gss_manager_get_resource, NULL, gss_manager_post_resource,
      manager);
  gss_server_add_admin_resource (server, r, "Manager");

}

static void
gss_manager_get_resource (GssTransaction * t)
{
  GssManager *manager = GSS_MANAGER (t->resource->priv);
  GString *s = g_string_new ("");

  t->s = s;

  gss_html_header (t);

  GSS_A ("<h1>Channel Manager</h1>\n");

  GSS_A ("<h2>Add Push Channel</h2>\n");
  GSS_A ("<p>Push channels are used for sources that initiate a connection\n");
  GSS_A ("to this server and send the stream.</p>\n");

  GSS_A
      ("<form class='form-horizontal' method='post' enctype='multipart/form-data'>\n");
  GSS_A ("<div class='control-group'>\n");
  GSS_A ("<label class='control-label' for='name1'>Stream name</label>\n");
  GSS_A ("<div class='controls'>\n");
  GSS_A ("<div class='input'>\n");
  GSS_A ("<input name='name' id='name1' type='text'>");
  GSS_A ("</div>\n");
  GSS_A ("</div>\n");
  GSS_A ("</div>\n");
  GSS_A
      ("<input name='action' id='button1' type='hidden' value='add-push-stream'>");
  GSS_A ("<button type='submit' class='btn'>Create Push Channel</button>\n");
  GSS_A ("</form>\n");

  GSS_A ("<hr>\n");
  GSS_A ("<h2>Add Pull Channel</h2>\n");
  GSS_A ("<p>Pull channels are used for sources that are stremaing servers.\n");
  GSS_A ("This server connects to the remote server to pull the stream.</p>\n");

  GSS_A
      ("<form class='form-horizontal' method='post' enctype='multipart/form-data'>\n");
  GSS_A ("<div class='control-group'>\n");
  GSS_A ("<label class='control-label' for='name2'>Stream name</label>\n");
  GSS_A ("<div class='controls'>\n");
  GSS_A ("<div class='input'>\n");
  GSS_A ("<input name='name' id='name2' type='text'>");
  GSS_A ("</div>\n");
  GSS_A ("</div>\n");
  GSS_A ("</div>\n");
  GSS_A
      ("<input name='action' id='button2' type='hidden' value='add-pull-stream'>");
  GSS_A ("<button type='submit' class='btn'>Create Push Channel</button>\n");
  GSS_A ("</form>\n");

  GSS_A ("<hr>\n");
  gss_config_append_config_block (G_OBJECT (manager), t, TRUE);

  gss_html_footer (t);
}

static gboolean
handle_action_add_push_stream (GssManager * manager, GssTransaction * t,
    GHashTable * hash)
{
  const char *name;
  GssProgram *program;

  program = gss_push_new ();

  name = g_hash_table_lookup (hash, "name");
  if (name) {
    g_object_set (program, "name", name, NULL);
  }

  gss_server_add_program_simple (t->server, program);

  return TRUE;
}

static gboolean
handle_action_add_pull_stream (GssManager * manager, GssTransaction * t,
    GHashTable * hash)
{
  const char *name;
  GssProgram *program;

  program = gss_pull_new ();

  name = g_hash_table_lookup (hash, "name");
  if (name) {
    g_object_set (program, "name", name, NULL);
  }

  gss_server_add_program_simple (t->server, program);

  return TRUE;
}

static void
gss_manager_post_resource (GssTransaction * t)
{
  GssManager *manager = GSS_MANAGER (t->resource->priv);
  gboolean ret = FALSE;
  GHashTable *hash;

  hash = gss_config_get_post_hash (t);
  if (hash) {
    const char *value;

    value = g_hash_table_lookup (hash, "action");
    if (value) {
      if (strcmp (value, "add-push-stream") == 0) {
        ret = handle_action_add_push_stream (manager, t, hash);
      } else if (strcmp (value, "add-pull-stream") == 0) {
        ret = handle_action_add_pull_stream (manager, t, hash);
      }
    } else {
      ret = gss_config_handle_post_hash (G_OBJECT (manager), t, hash);
    }
  }

  if (ret) {
    gss_config_save_config_file ();
    gss_transaction_redirect (t, "");
  } else {
    gss_transaction_error (t, "Configuration Error");
  }

}
