/* GStreamer Editing Services
 * Copyright (C) 2009 Edward Hervey <edward.hervey@collabora.co.uk>
 *               2009 Nokia Corporation
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

#include <ges/ges.h>
#include "ges-internal.h"

#define GES_GNONLIN_VERSION_NEEDED_MAJOR 0
#define GES_GNONLIN_VERSION_NEEDED_MINOR 11
#define GES_GNONLIN_VERSION_NEEDED_MICRO 0

GST_DEBUG_CATEGORY (_ges_debug);

/**
 * Internal structure to help avoid full loading
 * of one material several times
 */
typedef struct
{
  gboolean loaded;
  GESMaterial *material;
  guint64 ref_counter;

  /* List off callbacks to call when  the material is finally ready */
  GList *callbacks;

  GMutex *lock;
} GESMaterialCacheEntry;

static GHashTable *material_cache = NULL;
static GStaticMutex material_cache_lock = G_STATIC_MUTEX_INIT;

/**
 * SECTION:ges-common
 * @short_description: Initialization.
 */

static gboolean
ges_check_gnonlin_availability (void)
{
  gboolean ret = TRUE;
  if (!gst_registry_check_feature_version (gst_registry_get (),
          "gnlcomposition", GES_GNONLIN_VERSION_NEEDED_MAJOR,
          GES_GNONLIN_VERSION_NEEDED_MINOR, GES_GNONLIN_VERSION_NEEDED_MICRO)) {
    GST_ERROR ("GNonLin plugins not found, or not at least version %u.%u.%u",
        GES_GNONLIN_VERSION_NEEDED_MAJOR, GES_GNONLIN_VERSION_NEEDED_MINOR,
        GES_GNONLIN_VERSION_NEEDED_MICRO);
    ret = FALSE;
  }
  return ret;
}

static void
ges_material_cache_init (void)
{
  g_static_mutex_lock (&material_cache_lock);
  if (G_UNLIKELY (material_cache == NULL)) {
    material_cache = g_hash_table_new (g_str_hash, g_str_equal);
  }
  g_static_mutex_unlock (&material_cache_lock);
}

/**
 * ges_init:
 *
 * Initialize the GStreamer Editing Service. Call this before any usage of
 * GES. You should take care of initilizing GStreamer before calling this
 * function.
 */

gboolean
ges_init (void)
{
  /* initialize debugging category */
  GST_DEBUG_CATEGORY_INIT (_ges_debug, "ges", GST_DEBUG_FG_YELLOW,
      "GStreamer Editing Services");

  /* register timeline object classes with the system */

  GES_TYPE_TIMELINE_TEST_SOURCE;
  GES_TYPE_TIMELINE_FILE_SOURCE;
  GES_TYPE_TIMELINE_TITLE_SOURCE;
  GES_TYPE_TIMELINE_STANDARD_TRANSITION;
  GES_TYPE_TIMELINE_OVERLAY;

  /* check the gnonlin elements are available */
  if (!ges_check_gnonlin_availability ())
    return FALSE;

  ges_material_cache_init ();
  /* TODO: user-defined types? */

  GST_DEBUG ("GStreamer Editing Services initialized");

  return TRUE;
}


/**
 * ges_version:
 * @major: (out): pointer to a guint to store the major version number
 * @minor: (out): pointer to a guint to store the minor version number
 * @micro: (out): pointer to a guint to store the micro version number
 * @nano:  (out): pointer to a guint to store the nano version number
 *
 * Gets the version number of the GStreamer Editing Services library.
 */
void
ges_version (guint * major, guint * minor, guint * micro, guint * nano)
{
  g_return_if_fail (major);
  g_return_if_fail (minor);
  g_return_if_fail (micro);
  g_return_if_fail (nano);

  *major = GES_VERSION_MAJOR;
  *minor = GES_VERSION_MINOR;
  *micro = GES_VERSION_MICRO;
  *nano = GES_VERSION_NANO;
}

static GHashTable *
ges_material_cache_get (void)
{
  return material_cache;
}

static inline GESMaterialCacheEntry *
ges_material_cache_get_entry (const gchar * uri)
{
  GHashTable *cache = ges_material_cache_get ();
  GESMaterialCacheEntry *entry = NULL;

  g_static_mutex_lock (&material_cache_lock);
  entry = g_hash_table_lookup (cache, uri);
  g_static_mutex_unlock (&material_cache_lock);

  return entry;
}

/**
* Looks for material with specified uri in cache and it's completely loaded.
* @id String identifier of material
* @lookup_callback Callback that will be called after lookup. Can be %NULL 
* if lookup should be done synchronously. 
* In other case returns NULL
*/
GESMaterial *
ges_material_cache_lookup (const gchar * id)
{
  GESMaterialCacheEntry *entry = NULL;
  GESMaterial *material = NULL;

  entry = ges_material_cache_get_entry (id);

  if (entry) {
    g_mutex_lock (entry->lock);
    if (entry->loaded) {
      material = entry->material;
    } else {
      material = NULL;
    }
    g_mutex_unlock (entry->lock);
  }

  return material;
}

gboolean
ges_material_cache_append_callback (const gchar * id, GAsyncReadyCallback cb)
{
  GESMaterialCacheEntry *entry = NULL;
  entry = ges_material_cache_get_entry (id);

  if (entry) {
    g_mutex_lock (entry->lock);
    entry->callbacks = g_list_append (entry->callbacks, cb);
    g_mutex_unlock (entry->lock);
    return TRUE;
  } else {
    return FALSE;
  }
}

gboolean
ges_material_cache_is_loaded (const gchar * id)
{
  GESMaterialCacheEntry *entry = NULL;
  gboolean loaded = FALSE;

  entry = ges_material_cache_get_entry (id);

  if (entry) {
    g_mutex_lock (entry->lock);
    if (entry->loaded) {
      loaded = TRUE;
    } else {
      loaded = FALSE;
    }
    g_mutex_unlock (entry->lock);
  }

  return loaded;
}

gboolean
ges_material_cache_set_loaded (const gchar * id)
{
  GESMaterialCacheEntry *entry = NULL;
  gboolean loaded = FALSE;

  entry = ges_material_cache_get_entry (id);

  if (entry) {
    g_mutex_lock (entry->lock);
    entry->loaded = TRUE;
    loaded = TRUE;
    g_mutex_unlock (entry->lock);
  } else {
    loaded = FALSE;
  }

  return loaded;
}

void
ges_material_cache_put (GESMaterial * material)
{
  GHashTable *cache = ges_material_cache_get ();
  const gchar *material_id = ges_material_get_id (material);
  g_static_mutex_lock (&material_cache_lock);

  if (!g_hash_table_contains (cache, material_id)) {
    GESMaterialCacheEntry *entry = g_new (GESMaterialCacheEntry, 1);
    entry->lock = g_mutex_new ();
    entry->material = material;
    entry->callbacks = g_list_alloc ();
    entry->loaded = FALSE;
    g_hash_table_insert (cache, (gpointer) material_id, (gpointer) entry);
  }
  g_static_mutex_unlock (&material_cache_lock);
}
