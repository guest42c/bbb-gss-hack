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


#ifndef _EW_FORM_H_
#define _EW_FORM_H_

#include <glib.h>

#include "gss-session.h"


G_BEGIN_DECLS

typedef enum {
  FIELD_NONE = 0,
  FIELD_TEXT_INPUT,
  FIELD_PASSWORD,
  FIELD_CHECKBOX,
  FIELD_SELECT,
  FIELD_FILE,
  FIELD_RADIO,
  FIELD_SUBMIT,
  FIELD_VERTICAL_SPACE,
  FIELD_SECTION,
  FIELD_HIDDEN
} FieldType;

typedef struct _Option Option;
struct _Option {
  char *config_name;
  char *long_name;
};

typedef struct _Field Field;
struct _Field {
  FieldType type;
  char *config_name;
  char *long_name;
  char *default_value;
  int indent;

  Option options[10];
};

void gss_config_form_add_select (GString *s, Field *item, const char *value);
void gss_config_form_add_text_input (GString *s, Field *item, const char *value);
void gss_config_form_add_password (GString *s, Field *item, const char *value);
void gss_config_form_add_checkbox (GString *s, Field *item, const char *value);
void gss_config_form_add_file (GString *s, Field *item, const char *value);
void gss_config_form_add_radio (GString *s, Field *item, const char *value);
void gss_config_form_add_submit (GString *s, Field *item, const char *value);
void gss_config_form_add_hidden (GString *s, Field *item, const char *value);
void gss_config_form_add_form (GssServer *server, GString * s, const char *action,
    const char *name, Field *fields, GssSession *session);



G_END_DECLS

#endif


