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

#include "gss-user.h"
#include "gss-utils.h"
#include "gss-config.h"
#include "gss-html.h"

#include <string.h>
#include <stdlib.h>

enum
{
  PROP_0,
  PROP_USERS,
  PROP_ADMIN_PASSWORD0,
  PROP_ADMIN_PASSWORD1,
  PROP_PERMANENT_SESSIONS,
  PROP_REMOVE_SESSION,
};

#define DEFAULT_USERS ""
#define DEFAULT_ADMIN_PASSWORD0 ""
#define DEFAULT_ADMIN_PASSWORD1 ""
#define DEFAULT_PERMANENT_SESSIONS ""
#define DEFAULT_REMOVE_SESSION ""

#define REALM "GStreamer Streaming Server"


/* Hide the ew-admin group */
#define GROUP_START 1
#define N_GROUPS 4
static const char *group_names[] = { "ew-admin", "admin", "producer", "user" };

static void gss_user_finalize (GObject * object);
static void gss_user_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gss_user_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gss_user_get_resource (GssTransaction * t);
static void gss_user_post_resource (GssTransaction * t);
static void gss_profile_get_resource (GssTransaction * t);
static void gss_profile_post_resource (GssTransaction * t);
static char *gss_user_get_permanent_sessions (GssUser * user);
static void gss_user_add_permanent_session (GssUser * user,
    const char *session_id);
static void gss_user_set_permanent_sessions (GssUser * user, const char *s);

static void gss_user_info_free (GssUserInfo * ui);


G_DEFINE_TYPE (GssUser, gss_user, GSS_TYPE_OBJECT);

static GObjectClass *parent_class;

static void
gss_user_init (GssUser * user)
{
  user->users = g_hash_table_new_full (g_str_hash, g_str_equal,
      NULL, (GDestroyNotify) gss_user_info_free);
  gss_user_parse_users_string (user, DEFAULT_USERS);

  user->admin_password0 = NULL;
  user->admin_password1 = NULL;
}

static void
gss_user_class_init (GssUserClass * user_class)
{
  G_OBJECT_CLASS (user_class)->set_property = gss_user_set_property;
  G_OBJECT_CLASS (user_class)->get_property = gss_user_get_property;
  G_OBJECT_CLASS (user_class)->finalize = gss_user_finalize;

  g_object_class_install_property (G_OBJECT_CLASS (user_class), PROP_USERS,
      g_param_spec_string ("users", "Users", "Users", DEFAULT_USERS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (G_OBJECT_CLASS (user_class),
      PROP_ADMIN_PASSWORD0,
      g_param_spec_string ("admin-password0", "Admin Password",
          "Admin Password", DEFAULT_ADMIN_PASSWORD0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GSS_PARAM_SECURE)));
  g_object_class_install_property (G_OBJECT_CLASS (user_class),
      PROP_ADMIN_PASSWORD1, g_param_spec_string ("admin-password1",
          "Password (repeat)", "Password (repeat)", DEFAULT_ADMIN_PASSWORD1,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GSS_PARAM_SECURE)));
  g_object_class_install_property (G_OBJECT_CLASS (user_class),
      PROP_PERMANENT_SESSIONS, g_param_spec_string ("permanent-sessions",
          "Permanent Sessions", "Permanent Sessions",
          DEFAULT_PERMANENT_SESSIONS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GSS_PARAM_HIDE | GSS_PARAM_SECURE)));

  parent_class = g_type_class_peek_parent (user_class);
}

static void
gss_user_finalize (GObject * object)
{
  GssUser *user;

  user = GSS_USER (object);

  g_hash_table_unref (user->users);

  g_free (user->admin_password0);
  g_free (user->admin_password1);

  parent_class->finalize (object);
}

static void
gss_user_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GssUser *user;

  user = GSS_USER (object);

  switch (prop_id) {
    case PROP_USERS:
      g_hash_table_remove_all (user->users);
      gss_user_parse_users_string (user, g_value_get_string (value));
      break;
    case PROP_ADMIN_PASSWORD0:
      g_free (user->admin_password0);
      user->admin_password0 = g_value_dup_string (value);
      break;
    case PROP_ADMIN_PASSWORD1:
      g_free (user->admin_password1);
      user->admin_password1 = g_value_dup_string (value);
      break;
    case PROP_PERMANENT_SESSIONS:
      gss_user_set_permanent_sessions (user, g_value_get_string (value));
      break;
    default:
      g_assert_not_reached ();
      break;
  }
}

static void
gss_user_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GssUser *user;

  user = GSS_USER (object);

  switch (prop_id) {
    case PROP_USERS:
      g_value_take_string (value, gss_user_get_string (user));
      break;
    case PROP_ADMIN_PASSWORD0:
      g_value_set_string (value, "");
      break;
    case PROP_ADMIN_PASSWORD1:
      g_value_set_string (value, "");
      break;
    case PROP_PERMANENT_SESSIONS:
      g_value_take_string (value, gss_user_get_permanent_sessions (user));
      break;
    default:
      g_assert_not_reached ();
      break;
  }
}


GssUser *
gss_user_new (void)
{
  GssUser *user;

  user = g_object_new (GSS_TYPE_USER, "name", "admin.user", NULL);

  return user;
}

static gpointer
_gss_user_authorize (GssSession * session, gpointer priv)
{
  GssUser *user = priv;
  GssUserInfo *ui;
  static const GssUserInfo admin_user =
      { (char *) "admin", GSS_USER_GROUP_ADMIN | GSS_USER_GROUP_PRODUCER };

  if (strcmp (session->username, "admin") == 0) {
    session->is_admin = TRUE;
    return (gpointer) & admin_user;
  }

  ui = g_hash_table_lookup (user->users, session->username);
  if (ui && ui->groups & GSS_USER_GROUP_ADMIN) {
    session->is_admin = TRUE;
  }

  return ui;
}


void
gss_user_add_resources (GssUser * user, GssServer * server)
{
  GssResource *r;

  gss_session_set_authorization_function (_gss_user_authorize, user);

  r = gss_server_add_resource (server, "/admin/users",
      GSS_RESOURCE_ADMIN,
      GSS_TEXT_HTML, gss_user_get_resource, NULL, gss_user_post_resource, user);
  gss_server_add_admin_resource (server, r, "Users");

  gss_server_add_resource (server, "/profile",
      GSS_RESOURCE_UI | GSS_RESOURCE_USER,
      GSS_TEXT_HTML, gss_profile_get_resource, NULL, gss_profile_post_resource,
      user);
}

GssUserInfo *
gss_user_add_user_info (GssUser * user, const char *username, guint groups)
{
  GssUserInfo *ui;

  ui = g_new0 (GssUserInfo, 1);

  ui->username = g_strdup (username);
  ui->groups = groups;

  g_hash_table_replace (user->users, ui->username, ui);

  return ui;
}

static void
gss_user_info_free (GssUserInfo * ui)
{
  g_free (ui->username);
  g_free (ui);
}

void
gss_user_parse_users_string (GssUser * user, const char *s)
{
  char **list;
  int i;

  list = g_strsplit (s, " ", 0);
  for (i = 0; list[i]; i++) {
    char *e = strchr (list[i], ':');
    char *f;
    guint groups;

    if (e == NULL)
      continue;

    groups = strtoul (e + 1, &f, 0x10);
    if (f > e + 1) {
      e[0] = 0;
      gss_user_add_user_info (user, list[i], groups);
    }
  }

  g_strfreev (list);
}

char *
gss_user_get_string (GssUser * user)
{
  GString *s;
  GHashTableIter iter;
  gpointer key, value;

  s = g_string_new ("");
  g_hash_table_iter_init (&iter, user->users);
  while (g_hash_table_iter_next (&iter, &key, &value)) {
    GssUserInfo *userinfo = value;

    g_string_append_printf (s, "%s:%04x ", userinfo->username,
        userinfo->groups);
  }

  g_string_truncate (s, s->len - 1);

  return g_string_free (s, FALSE);
}

gboolean
gss_session_is_producer (GssSession * session)
{
  GssUserInfo *ui;

  if (session == NULL)
    return FALSE;

  if (session->is_admin)
    return TRUE;

  ui = session->priv;
  if (ui == NULL)
    return FALSE;

  if (ui->groups & (GSS_USER_GROUP_PRODUCER | GSS_USER_GROUP_ADMIN |
          GSS_USER_GROUP_GSS_ADMIN)) {
    return TRUE;
  }

  return FALSE;
}

static void
gss_user_set_permanent_sessions (GssUser * user, const char *s)
{
  char **ids;
  GList *g;
  int i;

  for (g = gss_session_get_list (); g;) {
    GssSession *session = g->data;
    g = g_list_next (g);
    if (session->permanent) {
      gss_session_invalidate (session);
    }
  }

  ids = g_strsplit (s, " ", 0);
  for (i = 0; ids[i]; i++) {
    gss_user_add_permanent_session (user, ids[i]);
  }
  g_strfreev (ids);
}

static char *
gss_user_get_permanent_sessions (GssUser * user)
{
  GList *g;
  GString *s;

  s = g_string_new ("");
  for (g = gss_session_get_list (); g; g = g_list_next (g)) {
    GssSession *session = g->data;
    if (!session->permanent)
      continue;

    g_string_append_printf (s, "%s ", session->session_id);
  }

  g_string_truncate (s, s->len - 1);

  return g_string_free (s, FALSE);
}

static void
gss_user_add_permanent_session (GssUser * user, const char *session_id)
{
  GssSession *session;

  session = gss_session_new ("permanent");
  if (session_id) {
    g_free (session->session_id);
    session->session_id = g_strdup (session_id);
  }
  session->permanent = TRUE;
  session->is_admin = TRUE;
}

static void
gss_user_get_resource (GssTransaction * t)
{
  GssUser *user = GSS_USER (t->resource->priv);
  GString *s = g_string_new ("");
  GHashTableIter iter;
  gpointer key, value;
  GList *g;

  t->s = s;

  gss_html_header (t);

  g_string_append_printf (s, "<h1>Users</h1><hr>\n");

  /* Users table */
  g_string_append (s, "<table class='table table-striped table-bordered "
      "table-condensed'>\n");
  g_string_append (s, "<thead>\n");
  g_string_append (s, "<tr>\n");
  g_string_append (s, "<th>User</th>\n");
  g_string_append (s, "<th>Groups</th>\n");
  g_string_append (s, "<th>Requests</th>\n");
  g_string_append (s, "<th>Other</th>\n");
  g_string_append (s, "</tr>\n");
  g_string_append (s, "</thead>\n");
  g_string_append (s, "<tbody>\n");

  g_hash_table_iter_init (&iter, user->users);
  while (g_hash_table_iter_next (&iter, &key, &value)) {
    GssUserInfo *userinfo = value;

    g_string_append (s, "<tr>\n");
    g_string_append_printf (s, "<td>%s</td>\n", userinfo->username);
    g_string_append (s, "<td>\n");
    if (userinfo->groups == 0) {
      g_string_append_printf (s, "none");
    } else {
      int i;

      for (i = GROUP_START; i < N_GROUPS; i++) {
        gboolean in_group = (userinfo->groups >> i) & 1;

        if (in_group) {
          char *u;
          u = g_strdup_printf ("<i class='icon-remove-sign'></i>%s",
              group_names[i]);
          gss_html_append_button3 (s, u,
              "action", "remove-user-group",
              "user", userinfo->username, "group", group_names[i]);
          g_free (u);
        }
      }
    }
    g_string_append (s, "<td>\n");
    if (userinfo->requested_groups) {
      int i;

      for (i = GROUP_START; i < N_GROUPS; i++) {
        gboolean in_group = (userinfo->requested_groups >> i) & 1;

        if (in_group) {
          char *u;
          u = g_strdup_printf ("<i class='icon-plus-sign'></i>%s",
              group_names[i]);
          gss_html_append_button3 (s, u,
              "action", "add-user-group",
              "user", userinfo->username, "group", group_names[i]);
          g_free (u);
        }
      }
    }
    g_string_append (s, "</td>\n");
    g_string_append (s, "<td>\n");
    {
      int i;
      unsigned int mask;

      mask = 0xffffffff & ~(userinfo->groups | userinfo->requested_groups);

      for (i = GROUP_START; i < N_GROUPS; i++) {
        gboolean in_group = (mask >> i) & 1;

        if (in_group) {
          char *u;
          u = g_strdup_printf ("<i class='icon-plus-sign'></i>%s",
              group_names[i]);
          gss_html_append_button3 (s, u,
              "action", "add-user-group",
              "user", userinfo->username, "group", group_names[i]);
          g_free (u);
        }
      }
    }
    g_string_append (s, "</td>\n");
    g_string_append (s, "</td>\n");
    g_string_append (s, "</tr>\n");
  }

  g_string_append (s, "</tbody>\n");
  g_string_append (s, "</table>\n");


  /* Permanent sessions */
  g_string_append (s, "<h2>Permanent Sessions</h2>\n");
  g_string_append (s, "<table class='table table-striped table-bordered "
      "table-condensed'>\n");
  g_string_append (s, "<thead>\n");
  g_string_append (s, "<tr>\n");
  g_string_append (s, "<th></th>\n");
  g_string_append (s, "<th>Session ID</th>\n");
  g_string_append (s, "<th>Description</th>\n");
  g_string_append (s, "<th>URL</th>\n");
  g_string_append (s, "</tr>\n");
  g_string_append (s, "</thead>\n");
  g_string_append (s, "<tbody>\n");

  for (g = gss_session_get_list (); g; g = g_list_next (g)) {
    GssSession *session = g->data;

    if (!session->permanent)
      continue;

    g_string_append (s, "<tr>\n");
    g_string_append_printf (s, "<td>\n");
    if (session == t->session) {
      gss_html_append_button2 (s,
          "<i class='icon-minus'></i>Remove current session", "action",
          "remove-session", "remove-id", session->session_id);
    } else {
      gss_html_append_button2 (s, "<i class='icon-minus'></i>Remove",
          "action", "remove-session", "remove-id", session->session_id);
    }
    g_string_append (s, "</td>\n");
    g_string_append_printf (s, "<td>%s</td>\n", session->session_id);
    g_string_append_printf (s, "<td>%s</td>\n", session->username);
    g_string_append_printf (s,
        "<td><a href=\"https://%s:%d/?session_id=%s\">https://%s:%d/?session_id=%s</a></td>\n",
        t->server->server_hostname, t->server->https_port, session->session_id,
        t->server->server_hostname, t->server->https_port, session->session_id);
    g_string_append (s, "</tr>\n");
  }

  g_string_append (s, "<tr>\n");
  g_string_append_printf (s, "<td colspan='4'>\n");
  gss_html_append_button (s, "<i class='icon-plus'></i>Add",
      "action", "add-session");
  g_string_append (s, "</tr>\n");

  g_string_append (s, "</tbody>\n");
  g_string_append (s, "</table>\n");

  g_string_append (s, "<div class='accordion' id='accordion-config'>\n");
  g_string_append (s, "<div class='accordion-group'>\n");
  g_string_append (s, "<div class='accordion-heading'>\n");
  g_string_append (s, "<div class='accordion-toggle'>\n");
  g_string_append (s, "<button class='btn btn-mini' data-toggle='collapse' "
      "data-parent='#accordion-config' data-target='#collapse-config'>\n");
  g_string_append (s, "<b class='caret'></b> Change Administrator Password\n");
  g_string_append (s, "</button>\n");
  g_string_append (s, "</div>\n");
  g_string_append (s,
      "<div id='collapse-config' class='accordion-body collapse out'>\n");
  g_string_append (s, "<div class='accordion-inner'>\n");

  g_string_append_printf (s,
      "<form class='form-horizontal' method='post' enctype='multipart/form-data' >\n");

  if (t->session) {
    g_string_append_printf (s,
        "<input name='session_id' type='hidden' value='%s'>\n",
        t->session->session_id);
  }

  g_string_append (s, "<fieldset><legend>Administrator Password</legend>\n");
  g_string_append (s, "<div class='control-group'>\n");
  g_string_append (s, "<label class='control-label' for='admin_password0'>"
      "New Password</label>\n");
  g_string_append (s, "<div class='controls'>\n");
  g_string_append_printf (s, "<input type='password' class='input-medium' "
      "id='admin_password0' name='admin_password0' " "value=''>\n");
  g_string_append (s, "</div>\n");
  g_string_append (s, "</div>\n");


  g_string_append (s, "<div class='control-group'>\n");
  g_string_append (s, "<label class='control-label' for='admin_password1'>"
      "Repeat Password</label>\n");
  g_string_append (s, "<div class='controls'>\n");
  g_string_append_printf (s, "<input type='password' class='input-medium' "
      "id='admin_password1' name='admin_password1' " "value=''>\n");
  g_string_append (s, "</div>\n");
  g_string_append (s, "</div>\n");
  g_string_append (s, "</fieldset>\n");

  g_string_append (s, "<div class='control-group'>\n");
  g_string_append (s, "<div class='controls'>\n");
  g_string_append_printf (s,
      "<input type='submit' class='input' value='%s'>\n", "Submit");
  g_string_append (s, "</div>\n");
  g_string_append (s, "</div>\n");


  g_string_append (s, "</form>\n");
  g_string_append (s, "</div>\n");
  g_string_append (s, "</div>\n");
  g_string_append (s, "</div>\n");
  g_string_append (s, "</div>\n");
  g_string_append (s, "</div>\n");


  gss_html_footer (t);
}

static int
get_group (const char *group)
{
  int i;
  for (i = GROUP_START; i < N_GROUPS; i++) {
    if (strcmp (group, group_names[i]) == 0)
      return i;
  }
  return -1;
}

static gboolean
handle_action_add_user_group (GssUser * user, GssTransaction * t,
    GHashTable * hash)
{
  const char *username;
  const char *group;
  GssUserInfo *ui;
  int group_index = -1;

  username = g_hash_table_lookup (hash, "user");
  if (username == NULL)
    return FALSE;
  group = g_hash_table_lookup (hash, "group");
  if (group == NULL)
    return FALSE;

  ui = g_hash_table_lookup (user->users, username);
  if (ui == NULL)
    return FALSE;

  group_index = get_group (group);
  if (group_index == -1)
    return FALSE;

  ui->groups |= (1 << group_index);
  ui->requested_groups &= ~(1 << group_index);
  return TRUE;
}

static gboolean
handle_action_remove_user_group (GssUser * user, GssTransaction * t,
    GHashTable * hash)
{
  const char *username;
  const char *group;
  GssUserInfo *ui;
  int group_index = -1;

  username = g_hash_table_lookup (hash, "user");
  if (username == NULL)
    return FALSE;
  group = g_hash_table_lookup (hash, "group");
  if (group == NULL)
    return FALSE;

  ui = g_hash_table_lookup (user->users, username);
  if (ui == NULL)
    return FALSE;

  group_index = get_group (group);
  if (group_index == -1)
    return FALSE;

  ui->groups &= ~(1 << group_index);
  return TRUE;
}

static gboolean
handle_action_add_session (GssUser * user, GssTransaction * t,
    GHashTable * hash)
{
  gss_user_add_permanent_session (user, NULL);
  return TRUE;
}

static gboolean
handle_action_remove_session (GssUser * user, GssTransaction * t,
    GHashTable * hash)
{
  GssSession *session;
  const char *id;

  id = g_hash_table_lookup (hash, "remove-id");
  if (id == NULL)
    return FALSE;

  session = gss_session_lookup (id);
  if (session == NULL)
    return FALSE;

  gss_session_invalidate (session);

  return TRUE;
}

static void
gss_user_post_resource (GssTransaction * t)
{
  GssUser *user = GSS_USER (t->resource->priv);
  GHashTable *hash;
  gboolean ret = FALSE;

  g_free (user->admin_password0);
  user->admin_password0 = NULL;
  g_free (user->admin_password1);
  user->admin_password1 = NULL;

  hash = gss_config_get_post_hash (t);
  if (hash) {
    const char *value;

    value = g_hash_table_lookup (hash, "action");
    if (value) {
      if (strcmp (value, "add-user-group") == 0) {
        ret = handle_action_add_user_group (user, t, hash);
      } else if (strcmp (value, "remove-user-group") == 0) {
        ret = handle_action_remove_user_group (user, t, hash);
      } else if (strcmp (value, "add-session") == 0) {
        ret = handle_action_add_session (user, t, hash);
      } else if (strcmp (value, "remove-session") == 0) {
        ret = handle_action_remove_session (user, t, hash);
      }
      gss_config_save_config_file ();
    } else {
      ret = gss_config_handle_post_hash (G_OBJECT (user), t, hash);

      if (user->admin_password0 && user->admin_password1) {
        if (strcmp (user->admin_password0, user->admin_password1) == 0) {
          char *s;
          s = soup_auth_domain_digest_encode_password ("admin",
              t->server->realm, user->admin_password0);
          g_object_set (t->server, "admin-token", s, NULL);
          g_free (s);

          gss_config_save_config_file ();
        }
      }

      if (user->admin_password0) {
        /* Poison string.  Just because. */
        memset (user->admin_password0, 0, strlen (user->admin_password0));
        g_free (user->admin_password0);
        user->admin_password0 = NULL;
      }
      if (user->admin_password1) {
        /* Poison string.  Just because. */
        memset (user->admin_password1, 0, strlen (user->admin_password1));
        g_free (user->admin_password1);
        user->admin_password1 = NULL;
      }
    }

    g_hash_table_unref (hash);
  }

  if (ret) {
    gss_transaction_redirect (t, "");
  } else {
    gss_transaction_error (t, "Configuration Error");
  }

}

static void
gss_profile_get_resource (GssTransaction * t)
{
  GssUserInfo *userinfo = t->session->priv;
  GString *s;
  int i;

  s = g_string_new ("");
  t->s = s;

  gss_html_header (t);

  g_string_append_printf (s, "<h1>User: %s</h1>\n", t->session->username);
  g_string_append (s, "<hr>\n");


  if (userinfo && userinfo->groups & GSS_USER_GROUP_USER) {
    g_string_append (s, "<table class='table table-striped table-bordered "
        "table-condensed'>\n");
    g_string_append (s, "<thead>\n");
    g_string_append (s, "<tr>\n");
    g_string_append (s, "<th>Group</th>\n");
    g_string_append (s, "<th></th>\n");
    g_string_append (s, "<th></th>\n");
    g_string_append (s, "</tr>\n");
    g_string_append (s, "</thead>\n");
    g_string_append (s, "<tbody>\n");

    for (i = GROUP_START; i < N_GROUPS; i++) {
      gboolean in_group = (userinfo->groups >> i) & 1;

      g_string_append (s, "<tr>\n");
      g_string_append_printf (s, "<td>%s</td>\n", group_names[i]);
      g_string_append_printf (s, "<td>%s</td>\n", in_group ? "yes" : "no");
      g_string_append_printf (s, "<td>\n");
      if (!in_group) {
        gss_html_append_button (s, "Request membership", "request-membership",
            group_names[i]);
        /* FIXME this isn't implemented */
      }
      g_string_append_printf (s, "</td>\n");
      g_string_append (s, "</tr>\n");
    }

    g_string_append (s, "</tbody>\n");
    g_string_append (s, "</table>\n");
  } else {
    g_string_append (s, "<p>You are currently logged in as a guest, which "
        "allows you to view most streams.</p>\n");
    if (userinfo) {
      g_string_append (s, "<p>A request for a user account is pending "
          "approval.</p>\n");
    } else {
      g_string_append (s, "<p>A user account is required to create or modify "
          "streams.  User accounts require administrator approval, and can "
          "be requested below:</p>\n");

      gss_html_append_button (s, "Request Account", "request-account", "user");
    }
  }

  gss_html_footer (t);
}

static void
gss_profile_post_resource (GssTransaction * t)
{
  GHashTable *hash;
  GssUser *user = GSS_USER (t->resource->priv);

  hash = gss_config_get_post_hash (t);
  if (hash) {
    const char *s;

    s = g_hash_table_lookup (hash, "request-account");
    if (s) {
      GST_ERROR ("request account %s", t->session->username);

      if (t->session->priv == NULL) {
        GssUserInfo *ui;

        ui = gss_user_add_user_info (user, t->session->username, 0);
        t->session->priv = ui;
        ui->requested_groups |= GSS_USER_GROUP_USER;
      }

      g_hash_table_remove (hash, "request-account");
    }

    s = g_hash_table_lookup (hash, "request-membership");
    if (s) {
      GST_ERROR ("request membership %s", s);

      g_hash_table_remove (hash, "request-membership");
    }

    g_hash_table_unref (hash);
  }

  gss_transaction_redirect (t, "");
}
