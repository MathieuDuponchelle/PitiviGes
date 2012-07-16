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

G_DEFINE_TYPE (GESMaterial, ges_material, G_TYPE_OBJECT);

enum
{
  PROP_0,
  PROP_TYPE,
  PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

struct _GESMaterialPrivate
{
  GType extractable_type;
};

static void
ges_material_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  /*GESMaterial *material = GES_MATERIAL (object); */

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ges_material_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GESMaterial *material = GES_MATERIAL (object);

  switch (property_id) {
    case PROP_TYPE:
      material->priv->extractable_type = g_value_get_gtype (value);
      break;
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

  properties[PROP_TYPE] =
      g_param_spec_gtype ("extractable-type", "Extractable type",
      "The type of the Object that can be extracted out of the material",
      G_TYPE_OBJECT, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, PROP_LAST, properties);
}

void
ges_material_init (GESMaterial * self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GES_TYPE_MATERIAL, GESMaterialPrivate);
}

GType
ges_material_get_extractable_type (GESMaterial * self)
{
  return self->priv->extractable_type;
}

GESMaterial *
ges_material_new (GType * extractable_type,
    const gchar * first_property_name, ...)
{
  const gchar *mandatory_parameters;
  GESExtractableObjectClass *class = g_type_class_ref (extractable_type);

  g_return_if_fail (GES_IS_EXTRACTABLE_OBJECT_CLASS (class), NULL);

  needed_parameters = ges_extractable_object_class_get_mandatory_params (class);

  /* FIXME Check what parameters are actually needed and if they are present
   * here */
}
