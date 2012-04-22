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

