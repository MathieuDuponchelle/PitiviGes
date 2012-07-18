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
#include <gst/pbutils/pbutils.h>
#include "ges.h"
#include "ges-types.h"
#include "ges-material-source.h"

G_DEFINE_TYPE (GESMaterialFileSource, ges_material_filesource,
    GES_TYPE_MATERIAL);

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
 * Internal structure to help avoid full loading
 * of one material several times
 */
typedef struct
{
  gboolean loaded;
  GESMaterialFileSource *material;
  guint64 ref_counter;

  /* List off callbacks to call when  the material is finally ready */
  GList *callbacks;

  GStaticMutex lock;
} GESMaterialFileSourceCacheEntry;

struct _GESMaterialFileSourcePrivate
{
  gchar *uri;
  GstDiscovererStreamInfo *stream_info;
  GstClockTime duration;
};

static GHashTable *
ges_material_filesource_cache_get (void)
{
  g_static_mutex_lock (&material_cache_lock);
  if (G_UNLIKELY (material_cache == NULL)) {
    material_cache = g_hash_table_new (g_str_hash, g_str_equal);
  }
  g_static_mutex_unlock (&material_cache_lock);

  return material_cache;
}

static inline GESMaterialFileSourceCacheEntry *
ges_material_filesource_cache_get_entry (const gchar * uri)
{
  GHashTable *cache = ges_material_filesource_cache_get ();
  GESMaterialFileSourceCacheEntry *entry = NULL;

  g_static_mutex_lock (&material_cache_lock);
  entry = g_hash_table_lookup (cache, uri);
  g_static_mutex_unlock (&material_cache_lock);

  return entry;
}

/**
* Looks for material with specified uri in cache and it's completely loaded.
* In other case returns NULL
*/
static GESMaterialFileSource *
ges_material_filesource_cache_lookup (const gchar * uri)
{
  GESMaterialFileSourceCacheEntry *entry = NULL;

  entry = ges_material_filesource_cache_get_entry (uri);
  if (entry && entry->loaded)
    return entry->material;

  return NULL;
}

static inline void
ges_material_filesource_cache_put (const gchar * uri,
    GESMaterialFileSourceCacheEntry * entry)
{
  GHashTable *cache = ges_material_filesource_cache_get ();

  GST_DEBUG_OBJECT (entry, "Adding to the cached list of materials");

  g_static_mutex_lock (&material_cache_lock);
  if (!g_hash_table_contains (cache, uri)) {
    g_hash_table_insert (cache, (gpointer) uri, (gpointer) entry);
  }
  g_static_mutex_unlock (&material_cache_lock);
}

static void
ges_material_filesource_discoverer_discover_uri (const gchar * uri)
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
ges_material_filesource_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GESMaterialFileSource *material = GES_MATERIAL_FILESOURCE (object);

  switch (property_id) {
    case PROP_URI:
      g_value_set_string (value, material->priv->uri);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ges_material_filesource_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GESMaterialFileSource *material = GES_MATERIAL_FILESOURCE (object);

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
ges_material_filesource_class_init (GESMaterialFileSourceClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  g_type_class_add_private (klass, sizeof (GESMaterialFileSourcePrivate));

  object_class->get_property = ges_material_filesource_get_property;
  object_class->set_property = ges_material_filesource_set_property;
  properties[PROP_URI] =
      g_param_spec_string ("uri",
      "URI of source material",
      "Get/set URI of source material",
      NULL, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, PROP_LAST, properties);
}

static void
ges_material_filesource_init (GESMaterialFileSource * self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GES_TYPE_MATERIAL, GESMaterialFileSourcePrivate);

  self->priv->stream_info = NULL;
  self->priv->duration = 0;
}

/**
* Full loading of material. Low-level function without caching.
*/
static void
ges_material_filesource_load (const gchar * uri,
    GAsyncReadyCallback material_loaded_cb, GSimpleAsyncResult * result)
{

  GESMaterialFileSource *material =
      g_object_new (GES_TYPE_MATERIAL_FILESOURCE, "uri",
      uri, "extractable-type", GES_TYPE_TIMELINE_FILE_SOURCE, NULL);

  GESMaterialFileSourceCacheEntry *cache_entry =
      g_slice_new (GESMaterialFileSourceCacheEntry);

  cache_entry->material = material;
  cache_entry->callbacks =
      g_list_append (cache_entry->callbacks, material_loaded_cb);
  g_static_mutex_init (&cache_entry->lock);

  ges_material_filesource_cache_put (uri, cache_entry);
  ges_material_filesource_discoverer_discover_uri (uri);
}

GESMaterialFileSource *
ges_material_filesource_new (const gchar * uri,
    GAsyncReadyCallback material_created_cb, gpointer user_data)
{
  GSimpleAsyncResult *simple;
  GESMaterialFileSource *material;

  material = ges_material_filesource_cache_lookup (uri);
  if (material) {
    /* FIXME check if loaded, if not lock and you need a list of pointer to
     * function in your struct, you can remove the ref_count prop, and just
     * have that list */

    simple = g_simple_async_result_new (NULL, material_created_cb, user_data,
        (gpointer) ges_material_filesource_new);

    g_simple_async_result_set_op_res_gpointer (simple,
        g_object_ref (material), g_object_unref);

    g_simple_async_result_complete_in_idle (simple);
    g_object_unref (simple);

    /* return a ref to the material */
    return material;

  } else if (material_created_cb == NULL) {
    /* FIXME Implement a syncronous version here */
    GST_WARNING ("No syncronous version yet, NOT DOING ANYTHING");
    return NULL;
  }

  ges_material_filesource_load (uri, material_created_cb, simple);

  return NULL;
}

GstDiscovererStreamInfo *
ges_material_filesource_get_stream_info (const GESMaterialFileSource * self)
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
  GESMaterialFileSourceCacheEntry *entry =
      ges_material_filesource_cache_get_entry (uri);


  g_static_mutex_lock (&entry->lock);
  entry->loaded = TRUE;
  g_static_mutex_unlock (&entry->lock);
}
