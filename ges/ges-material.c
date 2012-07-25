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

static void ges_material_initable_interface_init (GInitableIface * iface);
static void ges_material_async_initable_interface_init (GAsyncInitableIface *
    iface);

G_DEFINE_TYPE_WITH_CODE (GESMaterial, ges_material, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
        ges_material_initable_interface_init);
    G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
        ges_material_async_initable_interface_init));

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

/* GInitable implementation */
static gboolean
initable_iface_init (GInitable * initable, GCancellable * cancellable,
    GError ** error)
{
  return TRUE;
}

static void
async_initable_init_async (GAsyncInitable * initable,
    int io_priority,
    GCancellable * cancellable,
    GAsyncReadyCallback callback, gpointer user_data)
{
}

static gboolean
async_initable_init_finish (GAsyncInitable * initable,
    GAsyncResult * res, GError ** error)
{
  return TRUE;
}


static void
ges_material_initable_interface_init (GInitableIface * iface)
{
  iface->init = initable_iface_init;
}

static void
ges_material_async_initable_interface_init (GAsyncInitableIface * iface)
{
  iface->init_async = async_initable_init_async;
  iface->init_finish = async_initable_init_finish;
}

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
  self->state = MATERIAL_NOT_INITIALIZED;

  self->get_id = NULL;
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
 * The class must implement the #GESExtractable
 * @cancellable: (allow-none): optional #GCancellable object, %NULL to ignore.
 * @error: (allow-none): a #GError location to store the error occurring, or %NULL to ignore.
 * @...: the value if the first property, followed by and other property value pairs, and ended by %NULL.
 *
 * Creates a new #GESMaterial syncroniously, @error will be set if something went
 * wrong and the constructor will return %NULL in this case
 *
 * Returns: Created #GESMaterial or reference to existing one if it was created earlier
 * or %NULL on error
 */
GESMaterial *
ges_material_new (GType extractable_type, GCancellable * cancellable,
    GError ** error, const gchar * first_property_name, ...)
{
  GESMaterial *object;
  va_list var_args;
  GType object_type;

  va_start (var_args, first_property_name);
  object_type = check_type_and_params (extractable_type,
      first_property_name, var_args);
  va_end (var_args);

  if (object_type == G_TYPE_INVALID) {
    GST_WARNING ("Could create material with %s as extractable_type,"
        "wrong input parameters", g_type_name (extractable_type));

    if (error)
      *error = g_error_new (GES_ERROR_DOMAIN, 0, "Wrong parameter");

    return NULL;
  }

  if (first_property_name == NULL)
    return g_initable_newv (object_type, 0, NULL, cancellable, error);

  va_start (var_args, first_property_name);
  object =
      GES_MATERIAL (g_initable_new_valist (object_type, first_property_name,
          var_args, cancellable, error));
  va_end (var_args);

  return object;
}

/**
 * ges_material_new_async:
 * @extractable_type: The #GType of the object that can be extracted from the new material.
 * The class must implement the #GESExtractable
 * @io_priority: The priority of the thread (Note that it is not always used,
 *    e.g: for GESMaterialFileSource it will not be used)
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
void
ges_material_new_async (GType extractable_type, gint io_priority,
    GCancellable * cancellable, GAsyncReadyCallback callback,
    gpointer user_data, const gchar * first_property_name, ...)
{
  /*GESMaterial *object; */
  va_list var_args;
  GType object_type;

  va_start (var_args, first_property_name);
  object_type = check_type_and_params (extractable_type,
      first_property_name, var_args);
  va_end (var_args);

  if (object_type == G_TYPE_INVALID) {
    GST_WARNING ("Could create material with %s as extractable_type,"
        "wrong input parameters", g_type_name (extractable_type));

    /* FIXME Define error codes */
    g_simple_async_report_error_in_idle (NULL, callback, user_data,
        GES_ERROR_DOMAIN, 0, "Wrong parameter");

    return;
  }

  va_start (var_args, first_property_name);
  g_async_initable_new_valist_async (object_type,
      first_property_name, var_args, io_priority,
      cancellable, callback, user_data);
  va_end (var_args);
}

const gchar *
ges_material_get_id (GESMaterial * self)
{
  if (self->get_id) {
    return (*self->get_id) (self);
  } else
    return NULL;
}
