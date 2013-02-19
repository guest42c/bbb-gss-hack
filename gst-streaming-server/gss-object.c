/* GStreamer Streaming Server
 * Copyright (C) 2009-2013 Entropy Wave Inc <info@entropywave.com>
 * Copyright (C) 2009-2013 David Schleef <ds@schleef.org>
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

#include "gss-types.h"
#include "gss-object.h"
#include "gss-utils.h"

enum
{
  PROP_NAME = 1,
  PROP_UUID,
  PROP_DESCRIPTION
};

#define DEFAULT_NAME NULL
#define DEFAULT_UUID "00000000-0000-0000-0000-000000000000"
#define DEFAULT_DESCRIPTION ""



static void gss_object_finalize (GObject * object);
static void gss_object_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gss_object_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);


static GObjectClass *parent_class;

G_DEFINE_TYPE (GssObject, gss_object, G_TYPE_OBJECT);

static void
gss_object_init (GssObject * object)
{
  guint8 uuid[16];

  object->name = g_strdup (DEFAULT_NAME);
  object->description = g_strdup (DEFAULT_DESCRIPTION);
  gss_uuid_create (uuid);
  object->uuid = gss_uuid_to_string (uuid);
}

static void
gss_object_class_init (GssObjectClass * object_class)
{
  G_OBJECT_CLASS (object_class)->set_property = gss_object_set_property;
  G_OBJECT_CLASS (object_class)->get_property = gss_object_get_property;
  G_OBJECT_CLASS (object_class)->finalize = gss_object_finalize;

  g_object_class_install_property (G_OBJECT_CLASS (object_class),
      PROP_NAME, g_param_spec_string ("name", "Name",
          "Name", DEFAULT_NAME,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (G_OBJECT_CLASS (object_class),
      PROP_UUID, g_param_spec_string ("uuid", "UUID",
          "Unique Identifier", DEFAULT_UUID,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (G_OBJECT_CLASS (object_class),
      PROP_DESCRIPTION, g_param_spec_string ("description", "Description",
          "Description", DEFAULT_DESCRIPTION,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  parent_class = g_type_class_peek_parent (object_class);
}

static void
gss_object_finalize (GObject * gobject)
{
  GssObject *object = GSS_OBJECT (gobject);

  g_free (object->name);
  g_free (object->description);
  g_free (object->uuid);

  parent_class->finalize (gobject);
}

static void
gss_object_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GssObject *gssobject;

  gssobject = GSS_OBJECT (object);

  switch (prop_id) {
    case PROP_NAME:
      gss_object_set_name (gssobject, g_value_get_string (value));
      break;
    case PROP_DESCRIPTION:
      g_free (gssobject->description);
      gssobject->description = g_value_dup_string (value);
      break;
    case PROP_UUID:
      g_free (gssobject->uuid);
      gssobject->uuid = g_value_dup_string (value);
      break;
    default:
      g_assert_not_reached ();
      break;
  }
}

static void
gss_object_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GssObject *gssobject;

  gssobject = GSS_OBJECT (object);

  switch (prop_id) {
    case PROP_NAME:
      g_value_set_string (value, gssobject->name);
      break;
    case PROP_DESCRIPTION:
      g_value_set_string (value, gssobject->description);
      break;
    case PROP_UUID:
      g_value_set_string (value, gssobject->uuid);
      break;
    default:
      g_assert_not_reached ();
      break;
  }
}

void
gss_object_set_name (GssObject * object, const char *name)
{
  g_return_if_fail (GSS_IS_OBJECT (object));
  g_return_if_fail (name != NULL);

  object->name = g_strdup (name);
}
