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
#include "gss-form.h"
#include "gss-html.h"
#include "gss-soup.h"

#include <json-glib/json-glib.h>

#include <fcntl.h>


#define REALM "Entropy Wave E1000"
#define BASE "/"

static GList *sessions;

static GssField login_fields[] = {
  {GSS_FIELD_SECTION, NULL, "Login"},
  {GSS_FIELD_TEXT_INPUT, "username", "User", "", 0},
  {GSS_FIELD_PASSWORD, "password", "Password", "", 0},
  {GSS_FIELD_SUBMIT, "submit", "Login", NULL, 1},
  {GSS_FIELD_NONE}
};


typedef struct _AddrRange AddrRange;
struct _AddrRange
{
  struct in6_addr addr;
  int mask;
};
AddrRange *hosts_allow;
int n_hosts_allow;

static GssSessionAuthorizationFunc _ew_session_authorization_func;
static gpointer _ew_session_authorization_priv;

static void session_login_post_resource (GssTransaction * t);
static void session_login_get_resource (GssTransaction * t);
static void session_logout_resource (GssTransaction * t);

void
gss_session_add_session_callbacks (GssServer * server)
{
  gss_server_add_resource (server, "/login", 0,
      "text/html",
      session_login_get_resource, NULL, session_login_post_resource, NULL);
  gss_server_add_resource (server, "/logout", GSS_RESOURCE_HTTPS_ONLY,
      "text/html", session_logout_resource, NULL, NULL, NULL);

  gss_config_set_notify (server->config, "hosts_allow",
      gss_session_notify_hosts_allow, server);
  gss_session_notify_hosts_allow ("hosts_allow", server);
}

void
gss_session_notify_hosts_allow (const char *key, void *priv)
{
  GssServer *ewserver = (GssServer *) priv;
  char **chunks;
  char *end;
  const char *s;
  int n;
  int i;

  g_free (hosts_allow);
  n_hosts_allow = 0;

  s = gss_config_get (ewserver->config, "hosts_allow");
  chunks = g_strsplit (s, " ", 0);
  n = g_strv_length (chunks);

  hosts_allow = g_malloc0 ((n + 1) * sizeof (AddrRange));
  for (i = 0; i < n; i++) {
    if (strcmp (chunks[i], "all") == 0) {
      n_hosts_allow = 1;
      memset (&hosts_allow[0], 0, sizeof (AddrRange));
      break;
    } else if (strcmp (chunks[i], "segment") == 0) {
      /* IPv6 link local */
      memset (&hosts_allow[n_hosts_allow], 0, sizeof (struct in6_addr));
      hosts_allow[n_hosts_allow].addr.s6_addr[0] = 0xfe;
      hosts_allow[n_hosts_allow].addr.s6_addr[1] = 0x80;
      hosts_allow[n_hosts_allow].mask = 64;
      n_hosts_allow++;

    } else {
      char **d;
      int bits;

      d = g_strsplit (chunks[i], "/", 0);
      if (d[0] && d[1]) {
        bits = strtol (d[1], &end, 0);
        if (end != d[1]) {
          int len = strlen (d[0]);

          if (inet_pton (AF_INET, d[0],
                  &hosts_allow[n_hosts_allow].addr.s6_addr)) {
            hosts_allow[n_hosts_allow].addr.s6_addr[10] = 0xff;
            hosts_allow[n_hosts_allow].addr.s6_addr[11] = 0xff;
            hosts_allow[n_hosts_allow].addr.s6_addr[12] =
                hosts_allow[n_hosts_allow].addr.s6_addr[0];
            hosts_allow[n_hosts_allow].addr.s6_addr[13] =
                hosts_allow[n_hosts_allow].addr.s6_addr[1];
            hosts_allow[n_hosts_allow].addr.s6_addr[14] =
                hosts_allow[n_hosts_allow].addr.s6_addr[2];
            hosts_allow[n_hosts_allow].addr.s6_addr[15] =
                hosts_allow[n_hosts_allow].addr.s6_addr[3];
            memset (hosts_allow[n_hosts_allow].addr.s6_addr, 0, 10);
            hosts_allow[n_hosts_allow].mask = 96 + bits;
            n_hosts_allow++;
          } else if (d[0][0] == '[' && d[0][len - 1] == ']') {
            d[0][len - 1] = 0;
            if (inet_pton (AF_INET6, d[0] + 1,
                    &hosts_allow[n_hosts_allow].addr.s6_addr)) {
              hosts_allow[n_hosts_allow].mask = bits;
              n_hosts_allow++;
            }
          }
        }
      }
      g_strfreev (d);
    }
  }

  if (n_hosts_allow == 0) {
    n_hosts_allow = 1;
    hosts_allow = g_realloc (hosts_allow, 2 * sizeof (AddrRange));
    memset (&hosts_allow[0], 0, sizeof (AddrRange));
  }

  /* always allow localhost */
  hosts_allow[n_hosts_allow].addr.s6_addr[10] = 0xff;
  hosts_allow[n_hosts_allow].addr.s6_addr[11] = 0xff;
  hosts_allow[n_hosts_allow].addr.s6_addr[12] = 127;
  hosts_allow[n_hosts_allow].addr.s6_addr[13] = 0;
  hosts_allow[n_hosts_allow].addr.s6_addr[14] = 0;
  hosts_allow[n_hosts_allow].addr.s6_addr[15] = 1;
  memset (hosts_allow[n_hosts_allow].addr.s6_addr, 0, 10);
  hosts_allow[n_hosts_allow].mask = 96 + 8;
  n_hosts_allow++;

  g_strfreev (chunks);
}

static gboolean
host_validate (const struct in6_addr *in6a)
{
  int i;
  int j;

  for (i = 0; i < n_hosts_allow; i++) {
    int mask = hosts_allow[i].mask;

    for (j = 0; j < 16; j++) {
      if (mask >= 8) {
        if (hosts_allow[i].addr.s6_addr[j] != in6a->s6_addr[j]) {
          break;
        }
      } else if (mask <= 0) {
        return TRUE;
      } else {
        int maskbits = 0xff & (0xff00 >> mask);
        if ((hosts_allow[i].addr.s6_addr[j] & maskbits) !=
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
gss_addr_address_check (SoupClientContext * context)
{
  SoupAddress *addr;
  struct sockaddr *sa;
  int len;

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

      return host_validate (&in6a);
    } else if (sa->sa_family == AF_INET6) {
      struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) sa;
      struct in6_addr *in6a = &sin6->sin6_addr;

      return host_validate (in6a);
    }
  }

  return FALSE;
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

static int random_fd = -1;

void
__gss_session_deinit (void)
{
  if (random_fd != -1) {
    close (random_fd);
  }
}

char *
gss_session_create_id (void)
{
  uint8_t entropy[RANDOM_BYTES];
  int n;
  int i;
  char *base64;

  if (random_fd == -1) {
    random_fd = open ("/dev/random", O_RDONLY);
    if (random_fd < 0) {
      g_warning ("Could not open /dev/random, exiting");
      exit (1);
    }
  }

  i = 0;
  while (i < RANDOM_BYTES) {
    fd_set readfds;
    struct timeval timeout;
    int ret;

    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;
    FD_ZERO (&readfds);
    FD_SET (random_fd, &readfds);
    ret = select (random_fd + 1, &readfds, NULL, NULL, &timeout);
    if (ret == 0) {
      g_warning
          ("Waited too long to read random bytes.  Please install haveged.");
      exit (1);
    }

    n = read (random_fd, entropy + i, RANDOM_BYTES - i);
    if (n < 0) {
      g_warning ("Error reading /dev/random");
      exit (1);
    }
    i += n;
  }

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
  if (session->last_time + SESSION_TIMEOUT < now) {
    return FALSE;
  }
  return session->valid;
}

gboolean
gss_session_is_valid (GssSession * session)
{
  return __gss_session_is_valid (session, time (NULL));
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

  if (_ew_session_authorization_func) {
    session->priv = _ew_session_authorization_func (session,
        _ew_session_authorization_priv);
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
  _ew_session_authorization_func = func;
  _ew_session_authorization_priv = priv;
}

typedef struct _BrowserIDVerify BrowserIDVerify;
struct _BrowserIDVerify
{
  GssServer *ewserver;
  SoupServer *server;
  SoupMessage *msg;
  gchar *redirect_url;
};

static void
persona_verify_done (SoupSession * session, SoupMessage * msg,
    gpointer user_data)
{
  GssSession *login_session;
  BrowserIDVerify *v = (BrowserIDVerify *) user_data;
  JsonParser *jp;
  JsonNode *node;
  JsonNode *node2;
  JsonObject *object;
  GError *error = NULL;
  const char *s;
  gboolean ret;
  char *location;

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
    goto err;
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

  location = g_strdup_printf ("%s?session_id=%s", v->redirect_url,
      login_session->session_id);

  soup_message_headers_append (v->msg->response_headers, "Location", location);
  g_free (location);
  soup_message_set_response (v->msg, "text/plain", SOUP_MEMORY_STATIC, "", 0);
  soup_message_set_status (v->msg, SOUP_STATUS_SEE_OTHER);

  soup_server_unpause_message (v->server, v->msg);

  g_object_unref (jp);
  g_free (v->redirect_url);
  g_free (v);
  return;
err:
  g_object_unref (jp);
  soup_message_set_status (v->msg, SOUP_STATUS_UNAUTHORIZED);
  soup_server_unpause_message (v->server, v->msg);
  g_free (v->redirect_url);
  g_free (v);
}

static void
session_login_post_resource (GssTransaction * t)
{
  GssServer *ewserver = (GssServer *) t->server;
  char *hash;
  GHashTable *query_hash;
  const char *username = NULL;
  const char *password = NULL;
  const char *content_type;

  content_type = soup_message_headers_get_one (t->msg->request_headers,
      "Content-Type");

  query_hash = NULL;
  if (g_str_equal (content_type, "application/x-www-form-urlencoded")) {
    query_hash = soup_form_decode (t->msg->request_body->data);
  } else if (g_str_has_prefix (content_type, "multipart/form-data")) {
    char *filename;
    char *media_type;
    SoupBuffer *buffer;

    query_hash = soup_form_decode_multipart (t->msg, "dont_care",
        &filename, &media_type, &buffer);
  }

  if (query_hash) {
    const char *assertion;

    assertion = g_hash_table_lookup (query_hash, "assertion");

    if (assertion) {
      SoupMessage *client_msg;
      BrowserIDVerify *v;
      char *s;
      char *base_url;

      soup_server_pause_message (t->soupserver, t->msg);

      v = g_malloc0 (sizeof (BrowserIDVerify));
      v->ewserver = ewserver;
      v->server = t->soupserver;
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

      base_url = gss_transaction_get_base_url (t);
      s = g_strdup_printf
          ("https://persona.org/verify?assertion=%s&audience=%s",
          assertion, base_url);
      client_msg = soup_message_new ("POST", s);
      g_free (s);
      g_free (base_url);
      soup_message_headers_replace (client_msg->request_headers,
          "Content-Type", "application/x-www-form-urlencoded");

      soup_session_queue_message (ewserver->client_session, client_msg,
          persona_verify_done, v);

      g_hash_table_unref (query_hash);

      return;
    }
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
        gss_config_value_is_equal (ewserver->config, "admin_hash", hash);
    g_free (hash);
#endif
    hash = soup_auth_domain_digest_encode_password (username, REALM, password);
    valid = (strcmp (username, "admin") == 0) &&
        gss_config_value_is_equal (ewserver->config, "admin_token", hash);
    g_free (hash);

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
        redirect_url = g_strdup_printf ("%s/", base_url);
        g_free (base_url);
      }

      location = g_strdup_printf ("%s?session_id=%s",
          redirect_url, login_session->session_id);
      g_free (redirect_url);

      soup_message_headers_append (t->msg->response_headers, "Location",
          location);
      soup_message_set_response (t->msg, "text/plain", SOUP_MEMORY_STATIC, "",
          0);
      soup_message_set_status (t->msg, SOUP_STATUS_SEE_OTHER);
      g_free (location);

      g_hash_table_unref (query_hash);

      return;
    }
  }

  if (query_hash) {
    g_hash_table_unref (query_hash);
  }

  gss_html_error_404 (t->server, t->msg);
}

static void
session_login_get_resource (GssTransaction * t)
{
  GssServer *ewserver = (GssServer *) t->server;
  GString *s;
  char *redirect_url;
  char *location;

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

  gss_config_form_add_form (ewserver, s, location, "Login", login_fields, NULL);
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
  if (t->session) {
    gss_session_invalidate (t->session);
  }

  soup_message_headers_append (t->msg->response_headers, "Location", "/");
  soup_message_set_status (t->msg, SOUP_STATUS_TEMPORARY_REDIRECT);
}
