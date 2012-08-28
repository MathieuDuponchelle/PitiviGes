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

enum
{
  PROP_0,
  PROP_URI,
  PROP_LAST
};

static GParamSpec *properties[PROP_LAST];
static GHashTable *material_cache = NULL;


struct _GESMaterialSourcePrivate
{
  gchar *uri;
  GstDiscovererStreamInfo *stream_info;
  GstClockTime duration;
};

static GHashTable *
ges_material_source_cache_get (void)
{
  if (material_cache == NULL) {
    material_cache = g_hash_table_new (g_str_hash, g_str_equal);
  }

  return material_cache;
}

static GESMaterialSource *
ges_material_source_cache_lookup (const gchar * uri)
{
  GHashTable *cache = ges_material_source_cache_get ();

  GESMaterialSource *material =
      g_object_ref (GES_MATERIAL_SOURCE (g_hash_table_lookup (cache,
              (gpointer) uri)));

  return material;
}

static void
ges_material_source_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GESMaterialSource *material = GES_MATERIAL_SOURCE (object);

  switch (property_id) {
    case PROP_URI:
      g_value_set_string (value, material->priv->uri);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ges_material_source_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GESMaterialSource *material = GES_MATERIAL_SOURCE (object);

  switch (property_id) {
    case PROP_URI:
      if (material->priv->uri) {
        g_free (material->priv->uri);
      }

      material->priv->uri = g_strdup (g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ges_material_source_class_init (GESMaterialSourceClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  g_type_class_add_private (klass, sizeof (GESMaterialSourcePrivate));

  object_class->get_property = ges_material_source_get_property;
  object_class->set_property = ges_material_source_set_property;
  properties[PROP_URI] =
      g_param_spec_string ("uri",
      "URI of source material",
      "Get/set URI of source material",
      NULL, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, PROP_LAST, properties);
}

static void
ges_material_source_init (GESMaterialSource * self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GES_TYPE_MATERIAL, GESMaterialSourcePrivate);

  self->priv->stream_info = NULL;
  self->priv->duration = 0;
}

static void
ges_material_source_load (const gchar * uri,
    GAsyncReadyCallback material_created_cb, GSimpleAsyncResult * result)
{

}

void
ges_material_source_new_async (const gchar * uri,
    GAsyncReadyCallback material_created, gpointer user_data)
{
  GSimpleAsyncResult *simple;
  GESMaterialSource *material;

  simple = g_simple_async_result_new (NULL, material_created, user_data,
      (gpointer) ges_material_source_new_async);

  material = ges_material_source_cache_lookup (uri);

  if (material != NULL) {
    g_simple_async_result_set_op_res_gpointer (simple,
        g_object_ref (material), g_object_unref);

    g_simple_async_result_complete_in_idle (simple);
    g_object_unref (simple);
    g_object_unref (material);
  } else {
    ges_material_source_load (uri, material_created, simple);
  }
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
