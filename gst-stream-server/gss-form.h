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

#define GSS_FORM_NUM_OPTIONS 32

typedef enum {
  GSS_FIELD_NONE = 0,
  GSS_FIELD_TEXT_INPUT,
  GSS_FIELD_PASSWORD,
  GSS_FIELD_CHECKBOX,
  GSS_FIELD_SELECT,
  GSS_FIELD_FILE,
  GSS_FIELD_RADIO,
  GSS_FIELD_SUBMIT,
  GSS_FIELD_VERTICAL_SPACE,
  GSS_FIELD_SECTION,
  GSS_FIELD_HIDDEN,
  GSS_FIELD_ENABLE
} GssFieldType;

typedef struct _GssOption GssOption;
struct _GssOption {
  char *config_name;
  char *long_name;
};

typedef struct _GssField GssField;
struct _GssField {
  GssFieldType type;
  char *config_name;
  char *long_name;
  char *default_value;
  int indent;

  GssOption options[GSS_FORM_NUM_OPTIONS];
};

void gss_config_form_add_select (GString *s, GssField *item, const char *value);
void gss_config_form_add_text_input (GString *s, GssField *item, const char *value);
void gss_config_form_add_password (GString *s, GssField *item, const char *value);
void gss_config_form_add_checkbox (GString *s, GssField *item, const char *value);
void gss_config_form_add_file (GString *s, GssField *item, const char *value);
void gss_config_form_add_radio (GString *s, GssField *item, const char *value);
void gss_config_form_add_submit (GString *s, GssField *item, const char *value);
void gss_config_form_add_hidden (GString *s, GssField *item, const char *value);
void gss_config_form_add_form (GssServer *server, GString * s, const char *action,
    const char *name, GssField *fields, GssSession *session);



G_END_DECLS

#endif


