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
/**
 * SECTION: ges-material
 * @short_description: A GESMaterial is an object from which objects can be extracted
 *
 * FIXME: Long description to be written
 */

#include <gst/gst.h>
#include "ges.h"
#include "ges-internal.h"

G_DEFINE_TYPE (GESMaterial, ges_material, G_TYPE_OBJECT);
static GHashTable *ges_material_cache_get (void);

enum
{
  PROP_0,
  PROP_TYPE,
  PROP_ID,
  PROP_LAST
};

typedef enum
{
  MATERIAL_NOT_INITIALIZED,
  MATERIAL_INITIALIZING,
  MATERIAL_INITIALIZED_WITH_ERROR,
  MATERIAL_INITIALIZED
} GESMaterialState;

static GParamSpec *properties[PROP_LAST];

struct _GESMaterialPrivate
{
  GESMaterialState state;
  gchar *id;
  GType extractable_type;
};

/* Internal structure to help avoid full loading
 * of one material several times
 */
typedef struct
{
  gboolean loaded;
  GESMaterial *material;
  guint64 ref_counter;

  GError *error;

  /* List off callbacks to call when  the material is finally ready */
  GList *callbacks;
} GESMaterialCacheEntry;

/* Internal structure to store callbacks and corresponding user data pointers
  in lists
*/
typedef struct
{
  GESMaterialCacheEntry *entry;

  GESMaterialCreatedCallback callback;
  gpointer user_data;

} GESMaterialCallbackData;

static GHashTable *material_cache = NULL;
static GStaticMutex material_cache_lock = G_STATIC_MUTEX_INIT;

/* GESMaterial virtual methods default implementation */
static gboolean
ges_material_start_loading_default (GESMaterial * material)
{
  return ges_material_cache_set_loaded (ges_material_get_id (material), NULL);
}

static GESExtractable *
ges_material_extract_default (GESMaterial * material)
{
  guint n_params;
  GParameter *params;
  GESMaterialPrivate *priv = material->priv;

  params = ges_extractable_type_get_parameters_from_id (priv->extractable_type,
      priv->id, &n_params);

  return g_object_newv (priv->extractable_type, n_params, params);
}

/* GObject virtual methods implementation */
static void
ges_material_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GESMaterial *material = GES_MATERIAL (object);

  switch (property_id) {
    case PROP_TYPE:
      g_value_set_gtype (value, material->priv->extractable_type);
      break;
    case PROP_ID:
      g_value_set_string (value, material->priv->id);
      break;
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
    case PROP_ID:
      material->priv->id = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ges_material_finalize (GObject * object)
{
  GESMaterialPrivate *priv = GES_MATERIAL (object)->priv;

  if (priv->id)
    g_free (priv->id);

  G_OBJECT_CLASS (ges_material_parent_class)->finalize (object);
}

void
ges_material_class_init (GESMaterialClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  g_type_class_add_private (klass, sizeof (GESMaterialPrivate));

  object_class->get_property = ges_material_get_property;
  object_class->set_property = ges_material_set_property;
  object_class->finalize = ges_material_finalize;

  properties[PROP_TYPE] =
      g_param_spec_gtype ("extractable-type", "Extractable type",
      "The type of the Object that can be extracted out of the material",
      G_TYPE_OBJECT, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  properties[PROP_ID] =
      g_param_spec_string ("id", "Identifier",
      "The unic identifier of the material", NULL,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, PROP_LAST, properties);

  klass->start_loading = ges_material_start_loading_default;
  klass->extract = ges_material_extract_default;
}

void
ges_material_init (GESMaterial * self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GES_TYPE_MATERIAL, GESMaterialPrivate);

  self->priv->state = MATERIAL_NOT_INITIALIZED;
}

/* Some helper functions */
/* Cache routines */
/* WARNING: Must be called WITH material_cache_lock */
static GHashTable *
ges_material_cache_get (void)
{
  if (G_UNLIKELY (material_cache == NULL)) {
    material_cache = g_hash_table_new (g_str_hash, g_str_equal);
  }

  return material_cache;
}

/* WARNING: Must be called WITH material_cache_lock */
static inline GESMaterialCacheEntry *
ges_material_cache_get_entry (const gchar * uri)
{
  GHashTable *cache = ges_material_cache_get ();
  GESMaterialCacheEntry *entry = NULL;

  entry = g_hash_table_lookup (cache, uri);

  return entry;
}

/**
 * ges_material_cache_lookup:
 *
 * @id String identifier of material
 * @lookup_callback Callback that will be called after lookup. Can be %NULL
 * if lookup should be done synchronously.
 * In other case returns NULL
 *
 * Looks for material with specified uri in cache and it's completely loaded.
 *
 * Returns: (transfer none): The #GESMaterial found or %NULL
 */
GESMaterial *
ges_material_cache_lookup (const gchar * id)
{
  GESMaterialCacheEntry *entry = NULL;
  GESMaterial *material = NULL;

  g_return_val_if_fail (id, NULL);

  entry = ges_material_cache_get_entry (id);

  g_static_mutex_lock (&material_cache_lock);
  if (entry) {
    material = entry->material;
  }
  g_static_mutex_unlock (&material_cache_lock);
  return material;
}

static gboolean
ges_material_cache_append_callback (const gchar * id,
    GESMaterialCreatedCallback cb)
{
  GESMaterialCacheEntry *entry = NULL;
  gboolean result = FALSE;

  g_static_mutex_lock (&material_cache_lock);
  entry = ges_material_cache_get_entry (id);

  if (entry) {
    GESMaterialCallbackData *cbdata = g_new (GESMaterialCallbackData, 1);

    cbdata->entry = entry;
    cbdata->callback = cb;
    cbdata->user_data = NULL;
    entry->callbacks = g_list_append (entry->callbacks, cbdata);
    result = TRUE;
  }

  g_static_mutex_unlock (&material_cache_lock);
  return result;
}

static void
execute_callback_func (GESMaterialCallbackData * cbdata)
{
  gst_object_ref (cbdata->entry->material);
  cbdata->callback (cbdata->entry->material, cbdata->entry->error,
      cbdata->user_data);
  g_free (cbdata);
}

gboolean
ges_material_cache_set_loaded (const gchar * id, GError * error)
{
  GESMaterialCacheEntry *entry = NULL;
  GESMaterial *material;
  gboolean loaded = FALSE;

  g_static_mutex_lock (&material_cache_lock);
  entry = ges_material_cache_get_entry (id);
  if (entry) {
    GST_DEBUG_OBJECT (entry->material, "loaded, calling callback: %s", error
        ? error->message : "");

    material = entry->material;
    if (error) {
      entry->error = error;
      material->priv->state = MATERIAL_INITIALIZED_WITH_ERROR;
    } else
      material->priv->state = MATERIAL_INITIALIZED;

    g_list_free_full (entry->callbacks, (GDestroyNotify) execute_callback_func);
    entry->callbacks = NULL;

    loaded = TRUE;
  } else {
    loaded = FALSE;
  }
  g_static_mutex_unlock (&material_cache_lock);

  return loaded;
}

void
ges_material_cache_put (GESMaterial * material)
{
  GHashTable *cache;
  const gchar *material_id;

  /* Needing to work with the cache, taking the lock */
  g_static_mutex_lock (&material_cache_lock);

  cache = ges_material_cache_get ();
  material_id = ges_material_get_id (material);

  if (!g_hash_table_contains (cache, material_id)) {
    GESMaterialCacheEntry *entry = g_new (GESMaterialCacheEntry, 1);

    entry->material = material;
    entry->callbacks = NULL;
    g_hash_table_insert (cache, (gpointer) g_strdup (material_id),
        (gpointer) entry);
  } else {
    GST_DEBUG ("%s alerady in cache, not adding it again", material_id);
  }
  g_static_mutex_unlock (&material_cache_lock);
}

/* API implementation */
/**
 * ges_material_get_extractable_type:
 * @self: The #GESMaterial
 *
 * Gets the type of object that can be extracted from @self
 *
 * Returns: the type of object that can be extracted from @self
 */
GType
ges_material_get_extractable_type (GESMaterial * self)
{
  return self->priv->extractable_type;
}

/**
 * ges_material_new:
 * @extractable_type: The #GType of the object that can be extracted from the new material.
 *    The class must implement the #GESExtractable interface.
 * @callback: (scope async): a #GESMaterialCreatedCallback to call when the initialization is finished
 * @user_data: The user data to pass when @callback is called
 * @id: The Identifier of the material we want to create. This identifier depends of the extractable
 * type you want. By default it is the name of the class itself (or %NULL), but for example for a
 * GESTrackParseLaunchEffect, it will be the pipeline description, for a GESTimelineFileSource it
 * will be the name of the file, etc... You should refer to the documentation of the #GESExtractable
 * type you want to create a #GESMaterial for.
 *
 * Creates a new #GESMaterial asyncronously, @callback will be called when the materail is loaded
 *
 * Returns: %TRUE if the material could be loaded to load %FALSE otherwize
 */
gboolean
ges_material_new (GType extractable_type, GESMaterialCreatedCallback callback,
    gpointer user_data, const gchar * id)
{
  GESMaterial *material;
  gchar *real_id;
  gboolean ret = FALSE;

  g_return_val_if_fail (callback, FALSE);
  g_return_val_if_fail (g_type_is_a (extractable_type, G_TYPE_OBJECT), FALSE);
  g_return_val_if_fail (g_type_is_a (extractable_type, GES_TYPE_EXTRACTABLE),
      FALSE);

  /* Check if we already have a material for this ID */
  real_id = ges_extractable_type_check_id (extractable_type, id);
  if (real_id == NULL) {
    GST_WARNING ("Wrong ID %s, can not create material", id);

    return FALSE;
  }

  material = ges_material_cache_lookup (real_id);
  if (material != NULL) {
    switch (material->priv->state) {
      case MATERIAL_INITIALIZED:
        gst_object_ref (material);
        callback (material, NULL, user_data);
        ret = TRUE;

        goto done;
      case MATERIAL_INITIALIZING:
        ges_material_cache_append_callback (real_id, callback);
        ret = TRUE;

        goto done;
      default:
        break;
    }
  } else {
    GType object_type =
        ges_extractable_type_get_material_type (extractable_type);
    material =
        g_object_new (object_type, "extractable-type", extractable_type, "id",
        real_id, NULL);
  }

  /* Now initialize the material */
  material->priv->state = MATERIAL_INITIALIZING;
  ges_material_cache_put (material);
  ges_material_cache_append_callback (real_id, callback);

  if (GES_MATERIAL_GET_CLASS (material)->start_loading (material)
      == FALSE) {
    GError *error = g_error_new (GES_ERROR_DOMAIN, 1,
        "Could not start loading material");

    /* FIXME Define error code */
    ges_material_cache_set_loaded (real_id, error);
    g_error_free (error);
    ret = FALSE;

    goto done;
  }

  ret = TRUE;

done:
  g_free (real_id);

  return ret;
}

/**
 * ges_material_get_id:
 * @self: The #GESMaterial to get ID from
 *
 * Gets the ID of a #GESMaterial
 *
 * Returns: The ID of @self
 */
const gchar *
ges_material_get_id (GESMaterial * self)
{
  g_return_val_if_fail (GES_IS_MATERIAL (self), NULL);

  return self->priv->id;
}

/**
 * ges_material_extract:
 * @self: The #GESMaterial to get extract an object from
 *
 * Extracts a new #GObject from @material. The type of the object is
 * defined by the extractable-type of @material, you can check what
 * type will be extracted from @material using
 * #ges_material_get_extractable_type
 *
 * Returns: A newly created #GESExtractable
 */
GESExtractable *
ges_material_extract (GESMaterial * self)
{
  GESExtractable *extractable;
  g_return_val_if_fail (GES_IS_MATERIAL (self), NULL);
  g_return_val_if_fail (GES_MATERIAL_GET_CLASS (self)->extract, NULL);

  extractable = GES_MATERIAL_GET_CLASS (self)->extract (self);
  ges_extractable_set_material (extractable, self);

  return extractable;
}
