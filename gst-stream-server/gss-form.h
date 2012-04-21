
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


