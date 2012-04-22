
#include "config.h"

#include "gss-server.h"
#include "gss-config.h"
#include "gss-form.h"
#include "gss-html.h"

#include <glib/gstdio.h>
#include <glib-object.h>

#include <fcntl.h>
#if 0
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <string.h>
#endif


#define REALM "Entropy Wave E1000"
#define BASE "/"

static GList *sessions;

static Field login_fields[] = {
  { FIELD_SECTION, NULL, "Login" },
  { FIELD_TEXT_INPUT, "username", "User", "", 0 },
  { FIELD_PASSWORD, "password", "Password", "", 0 },
  { FIELD_SUBMIT, "submit", "Login", NULL, 1 },
  { FIELD_NONE }
};


typedef struct _AddrRange AddrRange;
struct _AddrRange {
  struct in6_addr addr;
  int mask;
};
AddrRange *hosts_allow;
int n_hosts_allow;


void
gss_session_add_session_callbacks (SoupServer *soupserver, gpointer priv)
{
  GssServer *ewserver = (GssServer *)priv;

  soup_server_add_handler (soupserver, "/login", gss_session_login_callback,
      ewserver, NULL);
  soup_server_add_handler (soupserver, "/logout", gss_session_logout_callback,
      ewserver, NULL);

  gss_config_set_notify (ewserver->config, "hosts_allow",
      gss_session_notify_hosts_allow, ewserver);
  gss_session_notify_hosts_allow ("hosts_allow", ewserver);
}

void
gss_session_notify_hosts_allow (const char *key, void *priv)
{
  GssServer *ewserver = (GssServer *)priv;
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

  hosts_allow = g_malloc0 ((n+1) * sizeof(AddrRange));
  for(i=0;i<n;i++) {
    if (strcmp (chunks[i], "all") == 0) {
      n_hosts_allow = 1;
      memset (&hosts_allow[0], 0, sizeof(AddrRange));
      break;
    } else if (strcmp (chunks[i], "segment") == 0) {
      /* IPv6 link local */
      memset (&hosts_allow[n_hosts_allow], 0, sizeof(struct in6_addr));
      hosts_allow[n_hosts_allow].addr.s6_addr[0] = 0xfe;
      hosts_allow[n_hosts_allow].addr.s6_addr[1] = 0x80;
      hosts_allow[n_hosts_allow].mask = 64;
      n_hosts_allow++;

#if 0
      memset (&hosts_allow[n_hosts_allow], 0, sizeof(struct in6_addr));
      hosts_allow[n_hosts_allow].addr.s6_addr[10] = 0xff;
      hosts_allow[n_hosts_allow].addr.s6_addr[11] = 0xff;
      hosts_allow[n_hosts_allow].mask = 96 + bits;
      n_hosts_allow++;
#endif
    } else {
      char **d;
      int bits;

      d = g_strsplit (chunks[i], "/", 0);
      if (d[0] && d[1]) {
        bits = strtol(d[1], &end, 0);
        if (end != d[1]) {
          int len = strlen(d[0]);

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
          } else if (d[0][0] == '[' && d[0][len-1] == ']') {
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
    hosts_allow = g_realloc (hosts_allow, 2*sizeof(AddrRange));
    memset (&hosts_allow[0], 0, sizeof(AddrRange));
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

#if 0
  for(i=0;i<n_hosts_allow;i++){
    struct in6_addr *in6a = &hosts_allow[i].addr;

    g_print("allow [%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]/%d\n",
        in6a->s6_addr[0], in6a->s6_addr[1], in6a->s6_addr[2], in6a->s6_addr[3],
        in6a->s6_addr[4], in6a->s6_addr[5], in6a->s6_addr[6], in6a->s6_addr[7],
        in6a->s6_addr[8], in6a->s6_addr[9], in6a->s6_addr[10], in6a->s6_addr[11],
        in6a->s6_addr[12], in6a->s6_addr[13], in6a->s6_addr[14], in6a->s6_addr[15],
        hosts_allow[i].mask);
  }
#endif

  g_strfreev (chunks);
}

static gboolean
host_validate (const struct in6_addr *in6a)
{
  int i;
  int j;

#if 0
  g_print("%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x\n",
      in6a->s6_addr[0], in6a->s6_addr[1], in6a->s6_addr[2], in6a->s6_addr[3],
      in6a->s6_addr[4], in6a->s6_addr[5], in6a->s6_addr[6], in6a->s6_addr[7],
      in6a->s6_addr[8], in6a->s6_addr[9], in6a->s6_addr[10], in6a->s6_addr[11],
      in6a->s6_addr[12], in6a->s6_addr[13], in6a->s6_addr[14], in6a->s6_addr[15]);
#endif

  for(i=0;i<n_hosts_allow;i++){
    int mask = hosts_allow[i].mask;

    //g_print("check %d\n", i);
    for(j=0;j<16;j++){
      if (mask >= 8) {
        if (hosts_allow[i].addr.s6_addr[j] != in6a->s6_addr[j]) {
          //g_print("+ %02x!=%02x\n", hosts_allow[i].addr.s6_addr[j], in6a->s6_addr[j]);
          break;
        }
        //g_print("+ %02x=%02x\n", hosts_allow[i].addr.s6_addr[j], in6a->s6_addr[j]);
      } else if (mask <= 0) {
        return TRUE;
      } else {
        int maskbits = 0xff & (0xff00 >> mask);
        if ((hosts_allow[i].addr.s6_addr[j] & maskbits) !=
            (in6a->s6_addr[j] & maskbits)) {
          //g_print("+ %02x!=%02x (%02x)\n", hosts_allow[i].addr.s6_addr[j], in6a->s6_addr[j], maskbits);
          break;
        }
        //g_print("+ %02x=%02x (%02x)\n", hosts_allow[i].addr.s6_addr[j], in6a->s6_addr[j], maskbits);
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
gss_addr_address_check (SoupClientContext *context)
{
  SoupAddress *addr;
  struct sockaddr *sa;
  int len;

  addr = soup_client_context_get_address (context);
  sa = soup_address_get_sockaddr (addr, &len);

  if (sa) {
    if (sa->sa_family == AF_INET) {
      struct sockaddr_in *sin = (struct sockaddr_in *)sa;
      struct in6_addr in6a;
      guint32 addr = ntohl(sin->sin_addr.s_addr);

      memset (&in6a.s6_addr[0], 0, 10);
      in6a.s6_addr[10] = 0xff;
      in6a.s6_addr[11] = 0xff;
      in6a.s6_addr[12] = (addr>>24)&0xff;
      in6a.s6_addr[13] = (addr>>16)&0xff;
      in6a.s6_addr[14] = (addr>>8)&0xff;
      in6a.s6_addr[15] = addr&0xff;

      return host_validate (&in6a);
    } else if (sa->sa_family == AF_INET6) {
      struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;
      struct in6_addr *in6a = &sin6->sin6_addr;

      return host_validate (in6a);
    }
  }

  return FALSE;
}

gboolean
gss_addr_is_localhost (SoupClientContext *context)
{
  SoupAddress *addr;
  struct sockaddr *sa;
  int len;
  char lh[16] = { 0,0,0,0,0,0,0,0,0,0,0xff,0xff,0x7f,0,0,1 };

  addr = soup_client_context_get_address (context);
  sa = soup_address_get_sockaddr (addr, &len);

  if (sa) {
    if (sa->sa_family == AF_INET) {
      struct sockaddr_in *sin = (struct sockaddr_in *)sa;
      struct in6_addr in6a;
      guint32 addr = ntohl(sin->sin_addr.s_addr);

      memset (&in6a.s6_addr[0], 0, 10);
      in6a.s6_addr[10] = 0xff;
      in6a.s6_addr[11] = 0xff;
      in6a.s6_addr[12] = (addr>>24)&0xff;
      in6a.s6_addr[13] = (addr>>16)&0xff;
      in6a.s6_addr[14] = (addr>>8)&0xff;
      in6a.s6_addr[15] = addr&0xff;

      return (memcmp (in6a.s6_addr, lh, 16) == 0);
    } else if (sa->sa_family == AF_INET6) {
      struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;
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
  hash = g_compute_checksum_for_string (G_CHECKSUM_SHA256, s, strlen(s));
  g_free (s);

  return hash;
}
#endif

#define RANDOM_BYTES 6

char *
gss_session_create_id (void)
{
  static int random_fd = -1;
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
    n = read (random_fd, entropy + i, RANDOM_BYTES - i);
    if (n < 0) {
      g_warning ("Error reading /dev/random");
      exit (1);
    }
    i += n;
  }

  base64 = g_base64_encode (entropy, RANDOM_BYTES);

  /* Convert to base64url (RFC 4648), since we use it in URLs */
  for(i=0;base64[i];i++){
    if (base64[i] == '+') base64[i] = '-';
    else if (base64[i] == '/') base64[i] = '_';
  }

  return base64;
}

#define SESSION_TIMEOUT 3600

GssSession *
gss_session_lookup (const char *session_id)
{
  GList *g;
  time_t now = time(NULL);

  for(g=sessions;g;g=g_list_next(g)) {
    GssSession *session = g->data;
    if (strcmp (session->session_id, session_id) == 0) {
      if (session->last_time + SESSION_TIMEOUT < now) {
        continue;
      }
      return session;
    }
  }
  return NULL;
}

void
gss_session_touch (GssSession *session)
{
  session->last_time = time(NULL);
}

GssSession *
gss_session_message_get_session (SoupMessage *msg, GHashTable *query)
{
  if (msg->method == SOUP_METHOD_GET || msg->method == SOUP_METHOD_POST) {
    char *id;

    if (query == NULL) return NULL;

    id = g_hash_table_lookup (query, "session_id");
    if (id == NULL) return NULL;

    return gss_session_lookup (id);
  }

  return NULL;
}

GssSession *
gss_session_new (const char *username)
{
  GssSession *session;

  session = g_malloc0 (sizeof(GssSession));
  session->username = g_strdup (username);
  session->session_id = gss_session_create_id();
  session->last_time = time(NULL);

  sessions = g_list_prepend (sessions, session);

  return session;
}

void
gss_session_login_callback (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data)
{
  GssServer *ewserver = (GssServer *)user_data;
  const char *content = "login\n";
  GString *s;
  char *redirect_url;
  char *location;

#if 0
  if (msg->method == SOUP_METHOD_GET) {
    g_print("GET %s\n", path);
  } else if (msg->method == SOUP_METHOD_POST) {
    g_print("POST %s\n", path);
  }
#endif

  if (msg->method != SOUP_METHOD_GET &&
      msg->method != SOUP_METHOD_POST) {
    soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);
    return;
  }

  if (msg->method == SOUP_METHOD_POST) {
    gboolean valid = FALSE;
    char *hash;
    GHashTable *query_hash;
    const char *username = NULL;
    const char *password = NULL;
    const char *content_type;

    content_type = soup_message_headers_get_one (msg->request_headers,
        "Content-Type");

    query_hash = NULL;
    if (g_str_equal (content_type, "application/x-www-form-urlencoded")) {
      query_hash = soup_form_decode (msg->request_body->data);
    } else if (g_str_has_prefix (content_type, "multipart/form-data")) {
      char *filename;
      char *media_type;
      SoupBuffer *buffer;

      query_hash = soup_form_decode_multipart (msg, "dont_care",
          &filename, &media_type, &buffer);
    }

    if (query_hash) {
      username = g_hash_table_lookup (query_hash, "username");
      password = g_hash_table_lookup (query_hash, "password");
    }

    if (username && password ) {
#if 0
      hash = password_hash (username, password);
      valid = (strcmp(username, "admin") == 0) &&
        gss_config_value_is_equal (ewserver->config, "admin_hash", hash);
      g_free (hash);
#endif
      hash = soup_auth_domain_digest_encode_password(username, REALM,
          password);
      valid = (strcmp(username, "admin") == 0) &&
        gss_config_value_is_equal (ewserver->config, "admin_token", hash);
      g_free (hash);
    }

    if (query_hash) {
      g_hash_table_unref (query_hash);
    }

    if (valid) {
      GssSession *session;
      char *location;

      session = gss_session_new (username);

      redirect_url = "/admin";
      if (query) {
        redirect_url = g_hash_table_lookup (query, "redirect_url");
      }
      location = g_strdup_printf("%s?session_id=%s", redirect_url,
          session->session_id);

      soup_message_headers_append (msg->response_headers,
          "Location", location);
      soup_message_set_response (msg, "text/plain", SOUP_MEMORY_STATIC, "", 0);
      //soup_message_set_status (msg, SOUP_STATUS_FOUND);
      soup_message_set_status (msg, SOUP_STATUS_SEE_OTHER);

      return;
    }

#if 0
    gss_html_error_404 (msg);

    return;
#endif
  }

  s = g_string_new ("");

  gss_html_header (ewserver, s, "Login");

  g_string_append(s, "<div id=\"header\">");
  gss_html_append_image (s, "/images/template_header_nologo.png",
      812, 36, NULL);

  g_string_append_printf(s,
      "</div><!-- end header div -->\n");
  g_string_append_printf(s, "<div id=\"content\">\n");

  redirect_url = NULL;
  if (query) {
    redirect_url = g_hash_table_lookup (query, "redirect_url");
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

  g_string_append (s, "</div><!-- end content div -->\n");
  gss_html_footer (ewserver, s, NULL);

  content = g_string_free (s, FALSE);
  soup_message_set_status (msg, SOUP_STATUS_OK);
  soup_message_set_response (msg, "text/html", SOUP_MEMORY_TAKE,
      content, strlen(content));
} 

void
gss_session_logout_callback (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data)
{
  //GssServer *ewserver = (GssServer *)user_data;
  GssSession *session;

  session = gss_session_message_get_session (msg, query);

  if (session == NULL) {
    soup_message_headers_append (msg->response_headers,
        "Location", "/login");
    soup_message_set_status (msg, SOUP_STATUS_TEMPORARY_REDIRECT);
    return;
  }

  sessions = g_list_remove (sessions, session);

  soup_message_headers_append (msg->response_headers,
      "Location", "/login");
  soup_message_set_status (msg, SOUP_STATUS_TEMPORARY_REDIRECT);
}


