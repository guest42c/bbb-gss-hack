
#ifndef _EW_CONFIG_H_
#define _EW_CONFIG_H_

#include <glib.h>
#include <stdlib.h>
#include <string.h>

G_BEGIN_DECLS

enum {
  EW_CONFIG_FLAG_NOSAVE = 1

};

typedef struct _EwConfigDefault EwConfigDefault;
struct _EwConfigDefault {
  const char *name;
  const char *default_value;
  unsigned int flags;
};

typedef struct _EwConfig EwConfig;
struct _EwConfig {
  GHashTable *hash;
  char *config_filename;
  int config_timestamp;
};

typedef struct _EwSession EwSession;
struct _EwSession {
  char *session_id;
  char *username;
  time_t last_time;
};



typedef void (*EwConfigNotifyFunc) (const char *config_name, void *priv);

typedef struct _EwConfigField EwConfigField;
struct _EwConfigField {
  char *value;
  void (*notify) (const char *config_name, void *priv);
  void *notify_priv;
  gboolean locked;
  unsigned int flags;
};

EwConfig * ew_config_new (void);
void ew_config_set_config_filename (EwConfig *config, const char *filename);
void ew_config_write_config_to_file (EwConfig *config);
void ew_config_free (EwConfig *config);
void ew_config_check_config_file (EwConfig *config);
void ew_config_set (EwConfig *config, const char *key, const char *value);
const char * ew_config_get (EwConfig *config, const char *key);
gboolean ew_config_get_boolean (EwConfig *config, const char *key);
int ew_config_get_int (EwConfig *config, const char *key);
void ew_config_load_defaults (EwConfig *config, EwConfigDefault *list);
void ew_config_load_from_file (EwConfig *config);
void ew_config_load_from_file_locked (EwConfig *config, const char *filename);
void ew_config_lock (EwConfig *config, const char *key);
void ew_config_set_flags (EwConfig *config, const char *key, unsigned int flags);
void ew_config_set_notify (EwConfig *config, const char *key,
    EwConfigNotifyFunc notify, void *priv);
gboolean ew_config_value_is_equal (EwConfig *config, const char *key,
    const char *value);
gboolean ew_config_value_is_on (EwConfig *config, const char *key);


G_END_DECLS

#endif

