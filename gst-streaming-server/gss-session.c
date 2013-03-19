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
#include "gss-config.h"
#include "gss-html.h"
#include "gss-soup.h"
#include "gss-utils.h"

#include <json-glib/json-glib.h>
#include <stdlib.h>

#define GST_CAT_DEFAULT gss_debug

#define BASE "/"

static GList *sessions;

static void append_login_html_login (GssServer * server, GssTransaction * t);
static void append_login_html_browserid (GssServer * server,
    GssTransaction * t);
static void append_login_html_cas (GssServer * server, GssTransaction * t);

static GssSessionAuthorizationFunc _gss_session_authorization_func;
static gpointer _gss_session_authorization_priv;

static void session_login_post_resource (GssTransaction * t);
static void session_login_get_resource (GssTransaction * t);
static void session_logout_resource (GssTransaction * t);

void
gss_session_add_session_callbacks (GssServer * server)
{
  if (0)
    server->append_login_html = append_login_html_login;
  server->append_login_html = append_login_html_browserid;
  if (0)
    server->append_login_html = append_login_html_cas;

  gss_server_add_resource (server, "/login", 0,
      GSS_TEXT_HTML,
      session_login_get_resource, NULL, session_login_post_resource, NULL);
  gss_server_add_resource (server, "/logout", GSS_RESOURCE_HTTPS_ONLY,
      GSS_TEXT_HTML, session_logout_resource, NULL, NULL, NULL);
}

gboolean
gss_addr_address_check (GssServer * server, SoupClientContext * context)
{
  SoupAddress *addr;

  addr = soup_client_context_get_address (context);
  return gss_addr_range_list_check_address (server->admin_arl, addr);
}

gboolean
gss_addr_is_localhost (SoupClientContext * context)
{
  SoupAddress *addr;
  struct sockaddr *sa;
  int len;
  char lh[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 0x7f, 0, 0, 1 };

  addr = soup_client_context_get_address (context);
  sa = soup_address_get_sockaddr (addr, &len);

  if (sa) {
    if (sa->sa_family == AF_INET) {
      struct sockaddr_in *sin = (struct sockaddr_in *) sa;
      struct in6_addr in6a;
      guint32 addr = ntohl (sin->sin_addr.s_addr);

      memset (&in6a.s6_addr[0], 0, 10);
      in6a.s6_addr[10] = 0xff;
      in6a.s6_addr[11] = 0xff;
      in6a.s6_addr[12] = (addr >> 24) & 0xff;
      in6a.s6_addr[13] = (addr >> 16) & 0xff;
      in6a.s6_addr[14] = (addr >> 8) & 0xff;
      in6a.s6_addr[15] = addr & 0xff;

      return (memcmp (in6a.s6_addr, lh, 16) == 0);
    } else if (sa->sa_family == AF_INET6) {
      struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) sa;
      struct in6_addr *in6a = &sin6->sin6_addr;

      return (memcmp (in6a->s6_addr, lh, 16) == 0);
    }
  }
  return FALSE;
}

#if 0
static char *
password_hash (const char *username, const char *password)
{
  char *s;
  char *hash;

  s = g_strdup_printf ("su5sVybsCM1VSMSw %s %s", username, password);
  hash = g_compute_checksum_for_string (G_CHECKSUM_SHA256, s, strlen (s));
  g_free (s);

  return hash;
}
#endif

#define RANDOM_BYTES 6

char *
gss_session_create_id (void)
{
  uint8_t entropy[RANDOM_BYTES];
  int i;
  char *base64;

  gss_utils_get_random_bytes (entropy, RANDOM_BYTES);

  base64 = g_base64_encode (entropy, RANDOM_BYTES);

  /* Convert to base64url (RFC 4648), since we use it in URLs */
  for (i = 0; base64[i]; i++) {
    if (base64[i] == '+')
      base64[i] = '-';
    else if (base64[i] == '/')
      base64[i] = '_';
  }

  return base64;
}

#define SESSION_TIMEOUT 3600

static gboolean
__gss_session_is_valid (GssSession * session, time_t now)
{
  if (!session->permanent && session->last_time + SESSION_TIMEOUT < now) {
    return FALSE;
  }
  return session->valid;
}

gboolean
gss_session_is_valid (GssSession * session)
{
  return __gss_session_is_valid (session, time (NULL));
}

GList *
gss_session_get_list (void)
{
  return sessions;
}

GssSession *
gss_session_lookup (const char *session_id)
{
  GList *g;
  time_t now = time (NULL);

  g = sessions;
  while (g) {
    GssSession *session = g->data;
    if (!__gss_session_is_valid (session, now)) {
      g = g_list_next (g);
      gss_session_invalidate (session);
      continue;
    }
    if (strcmp (session->session_id, session_id) == 0) {
      return gss_session_ref (session);
    }
    g = g_list_next (g);
  }
  return NULL;
}

void
gss_session_touch (GssSession * session)
{
  session->last_time = time (NULL);
}

GssSession *
gss_session_get_session (GHashTable * query)
{
  char *id;

  if (query == NULL)
    return NULL;

  id = g_hash_table_lookup (query, "session_id");
  if (id == NULL)
    return NULL;

  return gss_session_lookup (id);
}

GssSession *
gss_session_new (const char *username)
{
  GssSession *session;

  session = g_malloc0 (sizeof (GssSession));
  session->username = g_strdup (username);
  session->session_id = gss_session_create_id ();
  session->last_time = time (NULL);
  session->valid = TRUE;

  if (_gss_session_authorization_func) {
    session->priv = _gss_session_authorization_func (session,
        _gss_session_authorization_priv);
  }

  session->refcount = 1;
  sessions = g_list_prepend (sessions, session);

  return session;
}

GssSession *
gss_session_ref (GssSession * session)
{
  session->refcount++;
  return session;
}

void
gss_session_unref (GssSession * session)
{
  session->refcount--;
  if (session->refcount == 0) {
    g_free (session->username);
    g_free (session->session_id);
    g_free (session);
  }
}

void
gss_session_set_authorization_function (GssSessionAuthorizationFunc func,
    gpointer priv)
{
  _gss_session_authorization_func = func;
  _gss_session_authorization_priv = priv;
}

typedef struct _BrowserIDVerify BrowserIDVerify;
struct _BrowserIDVerify
{
  GssServer *server;
  SoupServer *soupserver;
  SoupMessage *msg;
  gchar *redirect_url;
};

static void
persona_verify_done (SoupSession * session, SoupMessage * msg,
    gpointer user_data)
{
  GssSession *login_session;
  BrowserIDVerify *v = (BrowserIDVerify *) user_data;
  JsonParser *jp = NULL;
  JsonNode *node;
  JsonNode *node2;
  JsonObject *object;
  GError *error = NULL;
  const char *s;
  gboolean ret;
  char *location;
  char *s2;

  if (msg->status_code != SOUP_STATUS_OK) {
    GST_INFO ("BrowserID verify failed: %s",
        soup_status_get_phrase (msg->status_code));
    goto err_no_msg;
  }

  jp = json_parser_new ();
  ret = json_parser_load_from_data (jp, msg->response_body->data,
      msg->response_body->length, &error);
  if (!ret) {
    g_error_free (error);
    goto err;
  }
  node = json_parser_get_root (jp);
  if (node == NULL) {
    goto err;
  }
  object = json_node_get_object (node);
  if (object == NULL) {
    goto err;
  }

  node2 = json_object_get_member (object, "status");
  if (node2 == NULL) {
    goto err;
  }
  s = json_node_get_string (node2);
  if (!s || strcmp (s, "okay") != 0) {
    GST_INFO ("BrowserID verify failed: status=%s", s);
    goto err_no_msg;
  }

  node2 = json_object_get_member (object, "email");
  if (node2 == NULL) {
    goto err;
  }
  s = json_node_get_string (node2);
  if (!s) {
    goto err;
  }

  login_session = gss_session_new (s);

  /* FIXME this can return session_id in HTTP */
  location = g_strdup_printf ("%s%s?session_id=%s",
      gss_soup_get_base_url_https (v->server, v->msg), v->redirect_url,
      login_session->session_id);

  GST_INFO ("new session for user %s", s);

  soup_message_headers_append (v->msg->response_headers, "Location", location);
  s2 = g_strdup_printf ("<html><body>Oops, you were supposed to "
      "be redirected <a href='%s'>here</a>.</body></html>\n", location);
  g_free (location);
  soup_message_set_response (v->msg, GSS_TEXT_HTML, SOUP_MEMORY_TAKE, s2,
      strlen (s2));
  soup_message_set_status (v->msg, SOUP_STATUS_SEE_OTHER);

  soup_server_unpause_message (v->soupserver, v->msg);

  g_object_unref (jp);
  g_free (v->redirect_url);
  g_free (v);
  return;
err:
  GST_INFO ("BrowserID verify failed: bad json response");
err_no_msg:
  if (jp)
    g_object_unref (jp);

  s2 = g_strdup_printf ("<html>\n"
      "<head>\n"
      "<title>Authorization Failed</title>\n"
      "</head>\n"
      "<body>\n"
      "<h1>Authorization Failed</h1>\n"
      "<p>Please go back and try again.</p>\n"
      "<br>\n"
      "<p>Response:</p>\n"
      "<pre>%s</pre>\n" "</body>\n" "</html>\n", msg->response_body->data);
  soup_message_set_response (v->msg, GSS_TEXT_HTML, SOUP_MEMORY_TAKE,
      s2, strlen (s2));
  soup_message_set_status (v->msg, SOUP_STATUS_UNAUTHORIZED);
  soup_server_unpause_message (v->soupserver, v->msg);
  g_free (v->redirect_url);
  g_free (v);
}

#ifdef ENABLE_CAS
static void
cas_verify_done (SoupSession * session, SoupMessage * msg, gpointer user_data)
{
  BrowserIDVerify *v = (BrowserIDVerify *) user_data;
  char *s2;

  GST_DEBUG ("cas verify done");

  if (msg->response_body->length > 5 &&
      strncmp (msg->response_body->data, "yes\n", 4) == 0) {
    GssSession *login_session;
    char *username;
    char *location;

    username = strndup (msg->response_body->data + 4,
        msg->response_body->length - 5);
    GST_DEBUG ("username %s", username);

    login_session = gss_session_new (username);

    /* FIXME this can return session_id in HTTP */
    location = g_strdup_printf ("%s%s?session_id=%s",
        gss_soup_get_base_url_https (v->server, v->msg), v->redirect_url,
        login_session->session_id);

    GST_INFO ("new session for user %s", username);

    soup_message_headers_append (v->msg->response_headers, "Location",
        location);
    s2 = g_strdup_printf ("<html><body>Oops, you were supposed to "
        "be redirected <a href='%s'>here</a>.</body></html>\n", location);
    g_free (location);
    soup_message_set_response (v->msg, GSS_TEXT_HTML, SOUP_MEMORY_TAKE, s2,
        strlen (s2));
    soup_message_set_status (v->msg, SOUP_STATUS_SEE_OTHER);

    g_free (username);
  } else {
    s2 = g_strdup_printf ("<html>\n"
        "<head>\n"
        "<title>Authorization Failed</title>\n"
        "</head>\n"
        "<body>\n"
        "<h1>Authorization Failed</h1>\n"
        "<p>Please go back and try again.</p>\n"
        "<br>\n"
        "<p>Response:</p>\n"
        "<pre>%s</pre>\n" "</body>\n" "</html>\n", msg->response_body->data);
    soup_message_set_response (v->msg, GSS_TEXT_HTML, SOUP_MEMORY_TAKE,
        s2, strlen (s2));
    soup_message_set_status (v->msg, SOUP_STATUS_UNAUTHORIZED);
  }
  soup_server_unpause_message (v->soupserver, v->msg);
  g_free (v->redirect_url);
  g_free (v);
}
#endif

static void
session_login_post_resource (GssTransaction * t)
{
  GssServer *server = (GssServer *) t->server;
  char *hash;
  GHashTable *query_hash;
  const char *username = NULL;
  const char *password = NULL;
  char *s;

  query_hash = gss_config_get_post_hash (t);

  if (query_hash) {
    const char *assertion;

    assertion = g_hash_table_lookup (query_hash, "assertion");

    if (assertion) {
      SoupMessage *client_msg;
      BrowserIDVerify *v;
      char *s;
      char *request_host;

      soup_server_pause_message (t->soupserver, t->msg);

      v = g_malloc0 (sizeof (BrowserIDVerify));
      v->server = server;
      v->soupserver = t->soupserver;
      v->msg = t->msg;

      s = g_hash_table_lookup (t->query, "redirect_url");
      if (s) {
        v->redirect_url = g_uri_unescape_string (s, NULL);
      } else {
        char *base_url;
        base_url = gss_soup_get_base_url_https (t->server, t->msg);
        v->redirect_url = g_strdup_printf ("%s/", base_url);
        g_free (base_url);
      }

      /* Redirect URLs must be local references, and must point to an
       * existing resource.  Otherwise, just ignore it.  */
      if (v->redirect_url[0] != '/' ||
          g_hash_table_lookup (t->server->resources, v->redirect_url) == NULL) {
        g_free (v->redirect_url);
        v->redirect_url = g_strdup ("/");
      }


      request_host = gss_soup_get_request_host (t->msg);
      if (request_host && server->alt_hostname &&
          strcmp (request_host, server->alt_hostname) == 0) {
        s = g_strdup_printf
            ("https://persona.org/verify?assertion=%s&audience=%s",
            assertion, server->alt_hostname);
      } else {
        s = g_strdup_printf
            ("https://persona.org/verify?assertion=%s&audience=%s",
            assertion, server->server_hostname);
      }
      client_msg = soup_message_new ("POST", s);
      g_free (s);
      g_free (request_host);
      soup_message_headers_replace (client_msg->request_headers,
          "Content-Type", "application/x-www-form-urlencoded");

      soup_session_queue_message (server->client_session, client_msg,
          persona_verify_done, v);

      g_hash_table_unref (query_hash);

      return;
    }
  }

  if (t->soupserver == t->server->server) {
    /* No password logins over HTTP */
    /* FIXME */
    gss_html_error_404 (t->server, t->msg);
    return;
  }

  if (query_hash) {
    username = g_hash_table_lookup (query_hash, "username");
    password = g_hash_table_lookup (query_hash, "password");
  }

  if (username && password) {
    gboolean valid = FALSE;

#if 0
    hash = password_hash (username, password);
    valid = (strcmp (username, "admin") == 0) &&
        gss_config_value_is_equal (server->config, "admin_hash", hash);
    g_free (hash);
#endif
    hash = soup_auth_domain_digest_encode_password (username, server->realm,
        password);
    valid = (strcmp (username, "admin") == 0) &&
        (strcmp (hash, server->admin_token) == 0);
    g_free (hash);

    if (!gss_addr_range_list_check_address (server->admin_arl,
            soup_client_context_get_address (t->client))) {
      valid = FALSE;
    }

    if (valid) {
      GssSession *login_session;
      gchar *location;
      const gchar *s;
      gchar *redirect_url;

      login_session = gss_session_new (username);
      login_session->is_admin = TRUE;

      if (t->query) {
        s = g_hash_table_lookup (t->query, "redirect_url");
      } else {
        s = NULL;
      }
      if (s) {
        redirect_url = g_uri_unescape_string (s, NULL);
      } else {
        char *base_url;
        base_url = gss_soup_get_base_url_https (t->server, t->msg);
        redirect_url = g_strdup ("/");
        g_free (base_url);
      }

      /* Redirect URLs must be local references, and must point to an
       * existing resource.  Otherwise, just ignore it.  */
      if (redirect_url[0] != '/' ||
          g_hash_table_lookup (t->server->resources, redirect_url) == NULL) {
        g_free (redirect_url);
        redirect_url = g_strdup ("/");
      }

      location = g_strdup_printf ("%s?session_id=%s",
          redirect_url, login_session->session_id);
      g_free (redirect_url);

      soup_message_headers_append (t->msg->response_headers, "Location",
          location);
      s = g_strdup_printf ("<html><body>Oops, you were supposed to "
          "be redirected <a href='%s'>here</a>.</body></html>\n", location);
      soup_message_set_response (t->msg, GSS_TEXT_HTML, SOUP_MEMORY_TAKE, s,
          strlen (s));
      soup_message_set_status (t->msg, SOUP_STATUS_SEE_OTHER);
      g_free (location);

      g_hash_table_unref (query_hash);

      return;
    }
  }

  if (query_hash) {
    g_hash_table_unref (query_hash);
  }

  s = g_strdup_printf ("<html>\n"
      "<head>\n"
      "<title>Authorization Failed</title>\n"
      "</head>\n"
      "<body>\n"
      "<h1>Authorization Failed</h1>\n"
      "<p>Please go back and try again.</p>\n" "</body>\n" "</html>\n");
  soup_message_set_response (t->msg, GSS_TEXT_HTML, SOUP_MEMORY_TAKE,
      s, strlen (s));
  soup_message_set_status (t->msg, SOUP_STATUS_UNAUTHORIZED);
}

#ifdef ENABLE_CAS
static void
handle_cas_ticket (GssTransaction * t)
{
  const char *ticket;
  SoupMessage *client_msg;
  BrowserIDVerify *v;
  char *s;
  char *request_host;

  ticket = g_hash_table_lookup (t->query, "ticket");
  g_return_if_fail (ticket != NULL);

  soup_server_pause_message (t->soupserver, t->msg);

  v = g_malloc0 (sizeof (BrowserIDVerify));
  v->server = t->server;
  v->soupserver = t->soupserver;
  v->msg = t->msg;

  s = g_hash_table_lookup (t->query, "redirect_url");
  if (s) {
    v->redirect_url = g_uri_unescape_string (s, NULL);
  } else {
    char *base_url;
    base_url = gss_soup_get_base_url_https (t->server, t->msg);
    v->redirect_url = g_strdup_printf ("%s/", base_url);
    g_free (base_url);
  }

  /* Redirect URLs must be local references, and must point to an
   * existing resource.  Otherwise, just ignore it.  */
  if (v->redirect_url[0] != '/' ||
      g_hash_table_lookup (t->server->resources, v->redirect_url) == NULL) {
    g_free (v->redirect_url);
    v->redirect_url = g_strdup ("/");
  }

  request_host = gss_soup_get_request_host (t->msg);
  s = g_strdup_printf
      ("%s/validate?service=https://127.0.0.1:8443/login&ticket=%s",
      t->server->cas_server, ticket);
  client_msg = soup_message_new ("GET", s);
  g_free (s);
  g_free (request_host);
  soup_message_headers_replace (client_msg->request_headers,
      "Content-Type", "application/x-www-form-urlencoded");

  soup_session_queue_message (t->server->client_session, client_msg,
      cas_verify_done, v);
}
#endif

static void
session_login_get_resource (GssTransaction * t)
{
  GssServer *server = (GssServer *) t->server;
  GString *s;
  char *redirect_url;
  char *location;

  if (t->soupserver == server->server) {
    char *base_url;
    char *location;
    char *s2;

    base_url = gss_soup_get_base_url_https (t->server, t->msg);
    location = g_strdup_printf ("%s/login", base_url);

    soup_message_headers_append (t->msg->response_headers, "Location",
        location);
    s2 = g_strdup_printf ("<html><body>Oops, you were supposed to "
        "be redirected <a href='%s'>here</a>.</body></html>\n", location);
    soup_message_set_response (t->msg, GSS_TEXT_HTML, SOUP_MEMORY_TAKE, s2,
        strlen (s2));
    soup_message_set_status (t->msg, SOUP_STATUS_SEE_OTHER);

    g_free (location);
    g_free (base_url);
    return;
  }
#ifdef ENABLE_CAS
  if (t->query && g_hash_table_lookup (t->query, "ticket")) {
    handle_cas_ticket (t);
    return;
  }
#endif

  t->s = s = g_string_new ("");

  gss_html_header (t);

  redirect_url = NULL;
  if (t->query) {
    redirect_url = g_hash_table_lookup (t->query, "redirect_url");
  }
  if (redirect_url) {
    char *e;
    e = g_uri_escape_string (redirect_url, NULL, FALSE);
    location = g_strdup_printf ("/login?redirect_url=%s", e);
    g_free (e);
  } else {
    location = g_strdup ("/login");
  }

  GSS_A ("<h2>Login</h2><br>\n");
  GSS_A
      ("<form class='form-horizontal' method='post' enctype='multipart/form-data'>\n");
  GSS_A ("<div class='control-group'>\n");
  GSS_A ("<label class='control-label' for='username'>Username</label>\n");
  GSS_A ("<div class='controls'>\n");
  GSS_A ("<div class='input'>\n");
  GSS_A ("<input name='username' id='username' type='text'>");
  GSS_A ("</div>\n");
  GSS_A ("</div>\n");
  GSS_A ("</div>\n");
  GSS_A ("<div class='control-group'>\n");
  GSS_A ("<label class='control-label' for='password'>Password</label>\n");
  GSS_A ("<div class='controls'>\n");
  GSS_A ("<div class='input-append'>\n");
  GSS_A ("<input name='password' id='password' type='password'>");
  GSS_A ("</div>\n");
  GSS_A ("</div>\n");
  GSS_A ("</div>\n");
  GSS_A ("<button type='submit' class='btn'>Login</button>\n");
  GSS_A ("</form>\n");

  g_free (location);

  gss_html_footer (t);
}

void
gss_session_invalidate (GssSession * session)
{
  session->valid = FALSE;
  sessions = g_list_remove (sessions, session);
  gss_session_unref (session);
}

static void
session_logout_resource (GssTransaction * t)
{
  if (t->session && !t->session->permanent) {
    gss_session_invalidate (t->session);
  }

  soup_message_headers_append (t->msg->response_headers, "Location", "/");
  soup_message_set_status (t->msg, SOUP_STATUS_TEMPORARY_REDIRECT);
}




static void
append_login_html_login (GssServer * server, GssTransaction * t)
{
  g_string_append (t->s, "<a href='/login' title='Login'>Login</a>\n");
}

static void
append_login_html_browserid (GssServer * server, GssTransaction * t)
{
  g_string_append (t->s,
      "<a href='#' id='browserid' title='Sign-in with Persona'>\n"
      "<img src='/sign_in_blue.png' alt='Sign in' "
      "onclick='navigator.id.get(gotAssertion);'>\n" "</a>\n");
}

static void
append_login_html_cas (GssServer * server, GssTransaction * t)
{
  char *base_url;
  base_url = gss_soup_get_base_url_https (t->server, t->msg);
  g_string_append_printf (t->s,
      "<a href='%s/login?service=%s/login' title='Login'>Login</a>\n",
      t->server->cas_server, t->server->base_url_https);

  g_free (base_url);
}


/* GssAddrRangeList */

typedef struct _GssAddrRange GssAddrRange;
struct _GssAddrRangeList
{
  int n_ranges;
  GssAddrRange *ranges;
};

struct _GssAddrRange
{
  struct in6_addr addr;
  int mask;
};



void
gss_addr_range_list_free (GssAddrRangeList * addr_range_list)
{
  g_free (addr_range_list->ranges);
  g_free (addr_range_list);
}

GssAddrRangeList *
gss_addr_range_list_new (int n_entries)
{
  GssAddrRangeList *addr_range_list;

  addr_range_list = g_malloc0 (sizeof (GssAddrRangeList));
  addr_range_list->ranges = g_malloc0 ((n_entries + 1) * sizeof (GssAddrRange));

  return addr_range_list;
}

GssAddrRangeList *
gss_addr_range_list_new_from_string (const char *str, gboolean default_all,
    gboolean allow_localhost)
{
  char **chunks;
  char *end;
  const char *s;
  int n;
  int i;
  GssAddrRangeList *addr_range_list;
  GssAddrRange *range;

  s = g_strdup (str);
  chunks = g_strsplit (s, " ", 0);
  n = g_strv_length (chunks);

  addr_range_list = gss_addr_range_list_new (n);
  for (i = 0; i < n; i++) {
    if (strcmp (chunks[i], "all") == 0) {
      range = &addr_range_list->ranges[0];
      memset (range, 0, sizeof (*range));
      addr_range_list->n_ranges = 1;
      break;
    } else if (strcmp (chunks[i], "segment") == 0) {
      /* IPv6 link local */
      range = &addr_range_list->ranges[addr_range_list->n_ranges];
      memset (range, 0, sizeof (*range));
      range->addr.s6_addr[0] = 0xfe;
      range->addr.s6_addr[1] = 0x80;
      range->mask = 64;
      addr_range_list->n_ranges++;

    } else {
      char **d;
      int bits;

      d = g_strsplit (chunks[i], "/", 0);
      if (d[0] && d[1]) {
        bits = strtol (d[1], &end, 0);
        if (end != d[1]) {
          int len = strlen (d[0]);

          range = &addr_range_list->ranges[addr_range_list->n_ranges];

          if (inet_pton (AF_INET, d[0], &range->addr.s6_addr)) {
            range->addr.s6_addr[10] = 0xff;
            range->addr.s6_addr[11] = 0xff;
            range->addr.s6_addr[12] = range->addr.s6_addr[0];
            range->addr.s6_addr[13] = range->addr.s6_addr[1];
            range->addr.s6_addr[14] = range->addr.s6_addr[2];
            range->addr.s6_addr[15] = range->addr.s6_addr[3];
            memset (range->addr.s6_addr, 0, 10);
            range->mask = 96 + bits;
            addr_range_list->n_ranges++;
          } else if (d[0][0] == '[' && d[0][len - 1] == ']') {
            d[0][len - 1] = 0;
            if (inet_pton (AF_INET6, d[0] + 1, &range->addr.s6_addr)) {
              range->mask = bits;
              addr_range_list->n_ranges++;
            }
          }
        }
      } else if (d[0] && d[1] == NULL) {
        int len = strlen (d[0]);

        /* FIXME merge with above code */
        range = &addr_range_list->ranges[addr_range_list->n_ranges];

        if (inet_pton (AF_INET, d[0], &range->addr.s6_addr)) {
          range->addr.s6_addr[10] = 0xff;
          range->addr.s6_addr[11] = 0xff;
          range->addr.s6_addr[12] = range->addr.s6_addr[0];
          range->addr.s6_addr[13] = range->addr.s6_addr[1];
          range->addr.s6_addr[14] = range->addr.s6_addr[2];
          range->addr.s6_addr[15] = range->addr.s6_addr[3];
          memset (range->addr.s6_addr, 0, 10);
          range->mask = 96 + 32;
          addr_range_list->n_ranges++;
        } else if (d[0][0] == '[' && d[0][len - 1] == ']') {
          d[0][len - 1] = 0;
          if (inet_pton (AF_INET6, d[0] + 1, &range->addr.s6_addr)) {
            range->mask = 32;
            addr_range_list->n_ranges++;
          }
        }
      }
      g_strfreev (d);
    }
  }

  if (default_all && addr_range_list->n_ranges == 0) {
    addr_range_list->n_ranges = 1;
    addr_range_list->ranges =
        g_realloc (addr_range_list->ranges, 2 * sizeof (GssAddrRange));
    memset (&addr_range_list->ranges[0], 0, sizeof (GssAddrRange));
  }

  if (allow_localhost) {
    /* always allow localhost */
    range = &addr_range_list->ranges[addr_range_list->n_ranges];
    range->addr.s6_addr[10] = 0xff;
    range->addr.s6_addr[11] = 0xff;
    range->addr.s6_addr[12] = 127;
    range->addr.s6_addr[13] = 0;
    range->addr.s6_addr[14] = 0;
    range->addr.s6_addr[15] = 1;
    memset (range->addr.s6_addr, 0, 10);
    range->mask = 96 + 8;
    addr_range_list->n_ranges++;
  }

  g_strfreev (chunks);

  return addr_range_list;
}

static gboolean
gss_addr_range_list_check_in6 (const GssAddrRangeList * addr_range_list,
    const struct in6_addr *in6a)
{
  int i;
  int j;

  for (i = 0; i < addr_range_list->n_ranges; i++) {
    const GssAddrRange *range = addr_range_list->ranges + i;
    int mask = range->mask;

    for (j = 0; j < 16; j++) {
      if (mask >= 8) {
        if (range->addr.s6_addr[j] != in6a->s6_addr[j]) {
          break;
        }
      } else if (mask <= 0) {
        return TRUE;
      } else {
        int maskbits = 0xff & (0xff00 >> mask);
        if ((range->addr.s6_addr[j] & maskbits) !=
            (in6a->s6_addr[j] & maskbits)) {
          break;
        }
      }
      mask -= 8;
    }
    if (mask <= 0) {
      return TRUE;
    }
  }

  return FALSE;
}

gboolean
gss_addr_range_list_check_address (const GssAddrRangeList * addr_range_list,
    SoupAddress * addr)
{
  struct sockaddr *sa;
  int len;

  sa = soup_address_get_sockaddr (addr, &len);

  if (sa) {
    if (sa->sa_family == AF_INET) {
      struct sockaddr_in *sin = (struct sockaddr_in *) sa;
      struct in6_addr in6a;
      guint32 addr = ntohl (sin->sin_addr.s_addr);

      memset (&in6a.s6_addr[0], 0, 10);
      in6a.s6_addr[10] = 0xff;
      in6a.s6_addr[11] = 0xff;
      in6a.s6_addr[12] = (addr >> 24) & 0xff;
      in6a.s6_addr[13] = (addr >> 16) & 0xff;
      in6a.s6_addr[14] = (addr >> 8) & 0xff;
      in6a.s6_addr[15] = addr & 0xff;

      return gss_addr_range_list_check_in6 (addr_range_list, &in6a);
    } else if (sa->sa_family == AF_INET6) {
      struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) sa;
      struct in6_addr *in6a = &sin6->sin6_addr;

      return gss_addr_range_list_check_in6 (addr_range_list, in6a);
    }
  }

  return FALSE;
}
