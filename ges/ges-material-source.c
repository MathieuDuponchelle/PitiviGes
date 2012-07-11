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
static GstDiscoverer *discoverer;

static GStaticMutex material_cache_lock = G_STATIC_MUTEX_INIT;
static GStaticMutex discoverer_lock = G_STATIC_MUTEX_INIT;

static void discoverer_finished_cb (GstDiscoverer * discoverer);
static void
discoverer_discovered_cb (GstDiscoverer * discoverer,
    GstDiscovererInfo * info, GError * err);

/** 
   Internal structure to help avoid full loading
   of one material several times */
typedef struct
{
  gboolean loaded;
  GESMaterialSource *material;
  guint64 ref_counter;
  GAsyncReadyCallback material_loaded;
} GESMaterialSourceCacheEntry;

struct _GESMaterialSourcePrivate
{
  gchar *uri;
  GstDiscovererStreamInfo *stream_info;
  GstClockTime duration;
};

static GHashTable *
ges_material_source_cache_get (void)
{
  g_static_mutex_lock (&material_cache_lock);
  if (material_cache == NULL) {
    material_cache = g_hash_table_new (g_str_hash, g_str_equal);
  }
  g_static_mutex_unlock (&material_cache_lock);

  return material_cache;
}

static GESMaterialSourceCacheEntry *
ges_material_source_cache_get_entry (const gchar * uri)
{
  GHashTable *cache = ges_material_source_cache_get ();
  GESMaterialSourceCacheEntry *entry = NULL;

  g_static_mutex_lock (&material_cache_lock);
  entry =
      (GESMaterialSourceCacheEntry *) (g_hash_table_lookup (cache,
          (gpointer) uri));
  g_static_mutex_unlock (&material_cache_lock);

  return entry;
}

/**
* Looks for material with specified uri in cache and it's completely loaded.
* In other case returns NULL
*/
static GESMaterialSource *
ges_material_source_cache_lookup (const gchar * uri)
{
  GHashTable *cache = ges_material_source_cache_get ();
  GESMaterialSourceCacheEntry *entry = NULL;

  g_static_mutex_lock (&material_cache_lock);
  entry =
      (GESMaterialSourceCacheEntry *) (g_hash_table_lookup (cache,
          (gpointer) uri));
  g_static_mutex_unlock (&material_cache_lock);

  if (entry->loaded) {
    return entry->material;
  } else {
    return NULL;
  }

}

static void
ges_material_source_cache_put (const gchar * uri,
    GESMaterialSourceCacheEntry * entry)
{
  GHashTable *cache = ges_material_source_cache_get ();

  g_static_mutex_lock (&material_cache_lock);
  if (!g_hash_table_contains (cache, uri)) {
    g_hash_table_insert (cache, (gpointer) uri, (gpointer) entry);
  }
  g_static_mutex_unlock (&material_cache_lock);
}

static void
ges_material_source_discoverer_discover_uri (const gchar * uri)
{
  g_static_mutex_lock (&discoverer_lock);
  if (discoverer == NULL) {
    discoverer = gst_discoverer_new (15 * GST_SECOND, NULL);
    g_signal_connect (discoverer, "finished",
        G_CALLBACK (discoverer_finished_cb), NULL);
    g_signal_connect (discoverer, "discovered",
        G_CALLBACK (discoverer_discovered_cb), NULL);

    gst_discoverer_start (discoverer);
  }
  g_static_mutex_unlock (&discoverer_lock);

  gst_discoverer_discover_uri_async (discoverer, uri);
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

/**
* Full loading of material. Low-level function without caching.
*/
static void
ges_material_source_load (const gchar * uri,
    GAsyncReadyCallback material_loaded_cb, GSimpleAsyncResult * result)
{

  GESMaterialSource *material = g_object_new (GES_TYPE_MATERIAL_SOURCE, "uri",
      uri, NULL);

  GESMaterialSourceCacheEntry *cache_entry = g_new (GESMaterialSourceCacheEntry,
      1);

  cache_entry->material = material;
  cache_entry->material_loaded = material_loaded_cb;
  cache_entry->ref_counter = 0;
  cache_entry->loaded = FALSE;

  ges_material_source_cache_put (uri, cache_entry);
  ges_material_source_discoverer_discover_uri (uri);
}

void
ges_material_source_new_async (const gchar * uri,
    GAsyncReadyCallback material_created_cb, gpointer user_data)
{
  GSimpleAsyncResult *simple;
  GESMaterialSource *material;

  simple = g_simple_async_result_new (NULL, material_created_cb, user_data,
      (gpointer) ges_material_source_new_async);

  material = ges_material_source_cache_lookup (uri);

  if (material != NULL) {
    g_simple_async_result_set_op_res_gpointer (simple,
        g_object_ref (material), g_object_unref);

    g_simple_async_result_complete_in_idle (simple);
    g_object_unref (simple);
    g_object_unref (material);
  } else {
    ges_material_source_load (uri, material_created_cb, simple);
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

static void
discoverer_finished_cb (GstDiscoverer * discoverer)
{
}

static void
discoverer_discovered_cb (GstDiscoverer * discoverer,
    GstDiscovererInfo * info, GError * err)
{
  const gchar *uri = gst_discoverer_info_get_uri (info);
  GESMaterialSourceCacheEntry *entry =
      ges_material_source_cache_get_entry (uri);


  g_static_mutex_lock (&material_cache_lock);
  entry->loaded = TRUE;
  g_static_mutex_unlock (&material_cache_lock);
}
