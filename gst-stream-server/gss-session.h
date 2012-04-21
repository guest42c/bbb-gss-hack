
#ifndef _GSS_SESSION_H_
#define _GSS_SESSION_H_

#include <glib.h>
#include <libsoup/soup.h>



G_BEGIN_DECLS

typedef struct _GssSession GssSession;
struct _GssSession {
  char *session_id;
  char *username;
  time_t last_time;
};

GssSession * gss_session_new (const char *username);
void gss_session_add_session_callbacks (SoupServer *soupserver, gpointer priv);
void gss_session_notify_hosts_allow (const char *key, void *priv);
gboolean gss_addr_address_check (SoupClientContext *context);
gboolean gss_addr_is_localhost (SoupClientContext *context);
char * gss_session_create_id (void);
GssSession * gss_session_lookup (const char *session_id);
void gss_session_touch (GssSession *session);
GssSession * gss_session_message_get_session (SoupMessage *msg,
    GHashTable *query);
void gss_session_login_callback (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data);
void gss_session_logout_callback (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data);


G_END_DECLS

#endif

