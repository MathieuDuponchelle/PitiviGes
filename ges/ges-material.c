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
  PROP_LAST
};

typedef enum
{
  MATERIAL_NOT_INITIALIZED,
  MATERIAL_INITIALIZING,
  MATERIAL_INITIALIZED
} GESMaterialState;

static GParamSpec *properties[PROP_LAST];

struct _GESMaterialPrivate
{
  GESMaterialState state;
  GType extractable_type;
};

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
} GESMaterialCacheEntry;

/**
 * Internal structure to store callbacks and corresponding user data pointers
  in lists 
*/
typedef struct
{
  GESMaterialCallback callback;
  gpointer user_data;
} GESMaterialCallbackData;

static GHashTable *material_cache = NULL;
static GStaticMutex material_cache_lock = G_STATIC_MUTEX_INIT;


/* GObject virtual methods implementation */
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

static void
ges_material_load_default (GESMaterial * material, GCancellable * cancellable)
{
}

static const gchar *
ges_material_get_id_default (GESMaterial * self)
{
  GString *id_str = g_string_new ("");
  g_string_printf (id_str, "object#%p", self);
  return g_string_free (id_str, FALSE);
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

  klass->get_id = ges_material_get_id_default;
  klass->load = ges_material_load_default;
}

void
ges_material_init (GESMaterial * self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GES_TYPE_MATERIAL, GESMaterialPrivate);

  self->priv->state = MATERIAL_NOT_INITIALIZED;
}

/* Some helper functions */
static gint
compare_gparamspec_str (GParamSpec * spec, const gchar * str)
{
  return g_strcmp0 (spec->name, str);
}

static GType
check_type_and_params (GType extractable_type,
    const gchar * first_property_name, va_list var_args)
{
  GType object_type;
  const gchar *name;
  GSList *tmp, *found, *params;

  if (g_type_is_a (extractable_type, G_TYPE_OBJECT) == G_TYPE_INVALID) {
    GST_WARNING ("%s not a GObject type", g_type_name (extractable_type));

    return G_TYPE_INVALID;
  }

  if (g_type_is_a (extractable_type, GES_TYPE_EXTRACTABLE) == G_TYPE_INVALID) {
    GST_WARNING ("%s not a GESExtractable type",
        g_type_name (extractable_type));

    return G_TYPE_INVALID;
  }

  object_type = ges_extractable_type_material_type (extractable_type);

  if (g_type_is_a (object_type, GES_TYPE_MATERIAL) == G_TYPE_INVALID) {
    GST_ERROR ("The extractable type %s is not an extractable_type",
        g_type_name (object_type));

    return G_TYPE_INVALID;
  }

  params = ges_extractable_type_mandatory_parameters (extractable_type);

  if (params == NULL)
    return object_type;

  if (first_property_name == NULL)
    return G_TYPE_INVALID;

  /* Go over all params and remove the parameter that we found from
   * the list of mandatory params */
  name = first_property_name;
  while (name) {
    found = g_slist_find_custom (params, name,
        (GCompareFunc) compare_gparamspec_str);

    if (found) {
      params = g_slist_delete_link (params, found);
    }

    name = va_arg (var_args, gchar *);
  }

  if (params) {
    for (tmp = params; tmp; tmp = tmp->next) {
      GST_WARNING ("Parameter %s missing to create material",
          G_PARAM_SPEC (tmp->data)->name);
      params = g_slist_delete_link (params, found);
    }
    return G_TYPE_INVALID;
  }

  return object_type;
}

static gchar *
get_id_from_params (const gchar * id_property_name,
    const gchar * first_property_name, va_list var_args)
{
  const gchar *name = first_property_name;
  const gchar *value = NULL;
  while (name) {
    value = va_arg (var_args, gchar *);

    if (g_strcmp0 (id_property_name, value)) {
      return g_strdup (value);
    }

    name = va_arg (var_args, gchar *);
  }

  return value;
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

gboolean
ges_material_is_loaded (GESMaterial * self)
{
  return self->priv->state == MATERIAL_INITIALIZED;
}

void
ges_material_set_loaded (GESMaterial * self)
{
  self->priv->state = MATERIAL_INITIALIZED;
}

/**
 * ges_material_new:
 * @extractable_type: The #GType of the object that can be extracted from the new material.
 * The class must implement the #GESExtractable
 * @cancellable: (allow-none): optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the initialization is finished
 * @...: the value if the first property, followed by and other property value pairs, and ended by %NULL.
 *
 * Creates a new #GESMaterial asyncroniously, @error will be set if something went
 * wrong and the constructor will return %NULL in this case
 *
 * Returns: Created #GESMaterial or reference to existing one if it was created earlier
 * or %NULL on error
 */
GESMaterial *
ges_material_new (GType extractable_type,
    GCancellable * cancellable, GESMaterialCallback callback,
    gpointer user_data, const gchar * first_property_name, ...)
{
  /*GESMaterial *object; */
  GESMaterial *material = NULL;
  va_list var_args;
  GType object_type;

  const gchar *id_name = ges_extractable_type_get_id_name (extractable_type);
  const gchar *id = NULL;

  va_start (var_args, first_property_name);
  id = get_id_from_params (id_name, first_property_name, var_args);
  va_end (var_args);

  GST_DEBUG ("Id name is %s", id_name);
  if (id != NULL) {
    material = ges_material_cache_lookup (id);
    if (material != NULL) {
      if (ges_material_is_loaded (material)) {
        (*callback) (material, TRUE);
      } else {
        ges_material_cache_append_callback (id, callback);
      }

      return material;
    }
  }

  va_start (var_args, first_property_name);

  object_type = check_type_and_params (extractable_type,
      first_property_name, var_args);
  va_end (var_args);

  if (object_type == G_TYPE_INVALID) {
    GST_WARNING ("Could create material with %s as extractable_type,"
        "wrong input parameters", g_type_name (extractable_type));
    return NULL;
  }

  va_start (var_args, first_property_name);
  material = (GESMaterial *) g_object_new_valist (object_type,
      first_property_name, var_args);
  va_end (var_args);

  GST_DEBUG ("Pointer to material is %p", material);
  material->priv->state = MATERIAL_INITIALIZING;
  ges_material_cache_put (material);
  GST_DEBUG ("Size is %d", g_hash_table_size (ges_material_cache_get ()));
  ges_material_cache_append_callback (id, callback);
  (*GES_MATERIAL_GET_CLASS (material)->load) (material, cancellable);

  return material;
}

const gchar *
ges_material_get_id (GESMaterial * self)
{
  if (GES_MATERIAL_GET_CLASS (self)->get_id) {
    return (*GES_MATERIAL_GET_CLASS (self)->get_id) (self);
  } else
    return NULL;
}

/* Cache routines */
void
ges_material_cache_init (void)
{
  g_static_mutex_lock (&material_cache_lock);
  if (G_UNLIKELY (material_cache == NULL)) {
    material_cache = g_hash_table_new (g_str_hash, g_str_equal);
  }
  g_static_mutex_unlock (&material_cache_lock);
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

  g_static_mutex_lock (&material_cache_lock);
  if (entry) {
    if (ges_material_is_loaded (material)) {
      material = entry->material;
    } else {
      material = NULL;
    }
  }
  g_static_mutex_unlock (&material_cache_lock);
  return material;
}

gboolean
ges_material_cache_append_callback (const gchar * id, GESMaterialCallback cb)
{
  GESMaterialCacheEntry *entry = NULL;
  gboolean result = FALSE;
  entry = ges_material_cache_get_entry (id);

  g_static_mutex_lock (&material_cache_lock);
  if (entry) {
    GESMaterialCallbackData *cbdata = g_new (GESMaterialCallbackData, 1);
    cbdata->callback = cb;
    cbdata->user_data = NULL;
    entry->callbacks = g_list_append (entry->callbacks, cbdata);
    result = TRUE;
  }

  g_static_mutex_unlock (&material_cache_lock);
  return result;
}

gboolean
ges_material_cache_is_loaded (const gchar * id)
{
  GESMaterialCacheEntry *entry = NULL;
  gboolean loaded = FALSE;

  entry = ges_material_cache_get_entry (id);
  g_static_mutex_lock (&material_cache_lock);
  if (entry) {
    if (ges_material_is_loaded (entry->material)) {
      loaded = TRUE;
    } else {
      loaded = FALSE;
    }
  }

  g_static_mutex_lock (&material_cache_lock);
  return loaded;
}

static void
execute_callback_func (GESMaterialCallbackData * cbdata, GESMaterial * material)
{
  if (cbdata == NULL)
    return;

  (*cbdata->callback) (material, TRUE);
  g_free (cbdata);
}

gboolean
ges_material_cache_set_loaded (const gchar * id)
{
  GESMaterialCacheEntry *entry = NULL;
  gboolean loaded = FALSE;

  entry = ges_material_cache_get_entry (id);
  g_static_mutex_lock (&material_cache_lock);
  if (entry) {
    ges_material_set_loaded (entry->material);
    GST_DEBUG ("About to launch cb");
    g_list_foreach (entry->callbacks, (GFunc) execute_callback_func,
        entry->material);
    g_list_free (entry->callbacks);
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
  GHashTable *cache = ges_material_cache_get ();
  const gchar *material_id = ges_material_get_id (material);
  g_static_mutex_lock (&material_cache_lock);
  if (!g_hash_table_contains (cache, material_id)) {
    GESMaterialCacheEntry *entry = g_new (GESMaterialCacheEntry, 1);
    entry->material = material;
    entry->callbacks = NULL;
    g_hash_table_insert (cache, (gpointer) g_strdup (material_id),
        (gpointer) entry);
  }
  g_static_mutex_unlock (&material_cache_lock);
}
