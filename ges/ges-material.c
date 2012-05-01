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

struct _GESMaterialPrivate
{
  GstTagList *metadatas;
  GESTrackType compatible_track_types;
};

void
ges_material_class_init (GESMaterialClass * klass)
{
  g_type_class_add_private (klass, sizeof (GESMaterialPrivate));
}

void
ges_material_init (GESMaterial * self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GES_TYPE_MATERIAL, GESMaterialPrivate);

  self->priv->metadatas = NULL;
  self->priv->compatible_track_types = GES_TRACK_TYPE_UNKNOWN;
}

GstTagList *
ges_material_get_metadatas (GESMaterial * self)
{
  return self->priv->metadatas;
}

GESTrackType
ges_material_get_compatible_track_types (GESMaterial * self)
{
  return self->priv->compatible_track_types;
}
