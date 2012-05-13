/* GStreamer Editing Services
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gst/gst.h>
#include "ges-types.h"
#include "ges-material.h"

G_DEFINE_ABSTRACT_TYPE (GESMaterial, ges_material, GST_TYPE_OBJECT);

enum
{
  PROP_0,
  PROP_COMPATIBLE_TRACK_TYPES,
  PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

struct _GESMaterialPrivate
{
  GESTrackType compatible_track_types;
};

static void
ges_material_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GESMaterial *material = GES_MATERIAL (object);

  switch (property_id) {
    case PROP_COMPATIBLE_TRACK_TYPES:
      g_value_set_flags (value, material->priv->compatible_track_types);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ges_material_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  /* GESMaterial *material = GES_MATERIAL (object); */

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}


void
ges_material_class_init (GESMaterialClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  g_type_class_add_private (klass, sizeof (GESMaterialPrivate));

  object_class->get_property = ges_material_get_property;
  object_class->set_property = ges_material_set_property;

  properties[PROP_COMPATIBLE_TRACK_TYPES] =
      g_param_spec_flags ("compatible-track-types",
      "Compatible track types of material",
      "Get compatible track types",
      GES_TYPE_TRACK_TYPE,
      GES_TRACK_TYPE_UNKNOWN, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, PROP_LAST, properties);
}

void
ges_material_init (GESMaterial * self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GES_TYPE_MATERIAL, GESMaterialPrivate);

  self->priv->compatible_track_types = GES_TRACK_TYPE_UNKNOWN;
}

GESTrackType
ges_material_get_compatible_track_types (GESMaterial * self)
{
  return self->priv->compatible_track_types;
}
