
#ifndef _GSS_SESSION_H_
#define _GSS_SESSION_H_

#include <glib.h>
#include <libsoup/soup.h>



G_BEGIN_DECLS

typedef struct _EwSession EwSession;
struct _EwSession {
  char *session_id;
  char *username;
  time_t last_time;
};

EwSession * ew_session_new (const char *username);
void ew_session_add_session_callbacks (SoupServer *soupserver, gpointer priv);
void ew_session_notify_hosts_allow (const char *key, void *priv);
gboolean ew_addr_address_check (SoupClientContext *context);
gboolean ew_addr_is_localhost (SoupClientContext *context);
char * ew_session_create_id (void);
EwSession * ew_session_lookup (const char *session_id);
void ew_session_touch (EwSession *session);
EwSession * ew_session_message_get_session (SoupMessage *msg,
    GHashTable *query);
void ew_session_login_callback (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data);
void ew_session_logout_callback (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data);


G_END_DECLS

#endif

