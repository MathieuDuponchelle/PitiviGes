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
#include <gst/pbutils/pbutils.h>
#include "ges-types.h"
#include "ges-material-source.h"

G_DEFINE_TYPE (GESMaterialSource, ges_material_source, GES_TYPE_MATERIAL);

struct _GESMaterialSourcePrivate
{
  gchar *uri;
  GstDiscovererStreamInfo *stream_info;
  GstClockTime duration;
};

void
ges_material_source_class_init (GESMaterialSourceClass * klass)
{
  g_type_class_add_private (klass, sizeof (GESMaterialSourcePrivate));
}

void
ges_material_source_init (GESMaterialSource * self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GES_TYPE_MATERIAL, GESMaterialSourcePrivate);

  self->priv->stream_info = NULL;
  self->priv->duration = 0;
}

GESMaterialSource *
ges_material_source_new (const gchar * uri,
    GAsyncReadyCallback * material_created, gpointer user_data)
{
  return NULL;
}

GstClockTime
ges_material_source_get_duration (const GESMaterialSource * self)
{
  return self->priv->duration;
}

GstDiscovererStreamInfo *
ges_material_source_get_stream_info (const GESMaterialSource * self)
{
  return self->priv->stream_info;
}
