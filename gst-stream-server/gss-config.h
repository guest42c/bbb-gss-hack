
#ifndef _ESS_CONFIG_H_
#define _ESS_CONFIG_H_

#include <glib.h>
#include <stdlib.h>
#include <string.h>



G_BEGIN_DECLS

enum {
  GSS_CONFIG_FLAG_NOSAVE = 1

};

typedef struct _GssConfigDefault GssConfigDefault;
struct _GssConfigDefault {
  const char *name;
  const char *default_value;
  unsigned int flags;
};

typedef struct _GssConfig GssConfig;
struct _GssConfig {
  GHashTable *hash;
  char *config_filename;
  int config_timestamp;
};



typedef void (*GssConfigNotifyFunc) (const char *config_name, void *priv);

typedef struct _GssConfigField GssConfigField;
struct _GssConfigField {
  char *value;
  void (*notify) (const char *config_name, void *priv);
  void *notify_priv;
  gboolean locked;
  unsigned int flags;
};

GssConfig * gss_config_new (void);
void gss_config_set_config_filename (GssConfig *config, const char *filename);
void gss_config_write_config_to_file (GssConfig *config);
void gss_config_free (GssConfig *config);
void gss_config_check_config_file (GssConfig *config);
void gss_config_set (GssConfig *config, const char *key, const char *value);
const char * gss_config_get (GssConfig *config, const char *key);
gboolean gss_config_get_boolean (GssConfig *config, const char *key);
int gss_config_get_int (GssConfig *config, const char *key);
void gss_config_load_defaults (GssConfig *config, GssConfigDefault *list);
void gss_config_load_from_file (GssConfig *config);
void gss_config_load_from_file_locked (GssConfig *config, const char *filename);
void gss_config_lock (GssConfig *config, const char *key);
void gss_config_set_flags (GssConfig *config, const char *key, unsigned int flags);
void gss_config_set_notify (GssConfig *config, const char *key,
    GssConfigNotifyFunc notify, void *priv);
gboolean gss_config_value_is_equal (GssConfig *config, const char *key,
    const char *value);
gboolean gss_config_value_is_on (GssConfig *config, const char *key);
void gss_config_hash_to_string (GString *s, GHashTable *hash);
void gss_config_handle_post (GssConfig *config, SoupMessage *msg);


G_END_DECLS

#endif

