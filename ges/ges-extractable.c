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
 * SECTION: ges-extractable
 * @short_description: An interface letting an object be extracted from a #GESMaterial
 *
 * FIXME: Long description needed
 */
#include "ges-material.h"
#include "ges-internal.h"
#include "ges-extractable.h"
#include "ges-timeline-file-source.h"

G_DEFINE_INTERFACE (GESExtractable, ges_extractable, G_TYPE_OBJECT);

static const gchar *
ges_extractable_get_default_get_id (GESExtractable * self)
{
  return g_type_name (G_OBJECT_TYPE (self));
}

static const gchar *
ges_extractable_default_get_id_for_type (GType type, va_list var_args)
{
  return g_type_name (type);
}

const gchar *
ges_extractable_get_id_for_type (GType type, const gchar * first_property,
    va_list var_args)
{
  GObjectClass *klass;
  GESExtractableInterface *iface;

  klass = g_type_class_ref (type);
  g_return_val_if_fail (G_IS_OBJECT_CLASS (klass), NULL);
  iface = g_type_interface_peek (klass, GES_TYPE_EXTRACTABLE);

  return (*iface->get_id_for_type) (type, var_args);
}

static void
ges_extractable_default_init (GESExtractableInterface * iface)
{
  iface->material_type = GES_TYPE_MATERIAL;
  iface->get_id = ges_extractable_get_default_get_id;
  iface->get_material = NULL;
  iface->set_material = NULL;
  iface->get_id_for_type = ges_extractable_default_get_id_for_type;
}

/**
 * ges_extractable_object_get_material:
 * @object: Target object
 *
 * Method to get material which was used to instaniate specified object
 *
 * Returns: (transfer none) : origin material
 */
GESMaterial *
ges_extractable_get_material (GESExtractable * self)
{
  GESExtractableInterface *iface;
  g_return_val_if_fail (GES_IS_EXTRACTABLE (self), NULL);

  iface = GES_EXTRACTABLE_GET_INTERFACE (self);
  g_return_val_if_fail (iface->get_material, NULL);

  return iface->get_material (self);
}

/**
 * ges_extractable_object_set_material:
 * @object: Target object
 * @material: The #GESMaterial to set
 *
 * Method to set material which was used to instaniate specified object
 */
void
ges_extractable_set_material (GESExtractable * self, GESMaterial * material)
{
  GESExtractableInterface *iface;
  g_return_if_fail (GES_IS_EXTRACTABLE (self));

  iface = GES_EXTRACTABLE_GET_INTERFACE (self);
  g_return_if_fail (iface->set_material);
  g_return_if_fail (iface->get_material);

  if (iface->get_material (self)) {
    GST_WARNING_OBJECT (self, "Can not reset material on object");

    return;
  }

  iface->set_material (self, material);
}

/**
 * ges_extractable_get_material_type:
 * @self: The #GESExtractable to retrive #GESMaterial type
 * to use.
 *
 * Lets user know the type of GESMaterial that should be used to extract the
 * object that implement that interface.
 */
GType
ges_extractable_get_material_type (GESExtractable * self)
{
  GESExtractableInterface *iface;

  g_return_val_if_fail (GES_IS_EXTRACTABLE (self), G_TYPE_INVALID);

  iface = GES_EXTRACTABLE_GET_INTERFACE (self);
  g_return_val_if_fail (iface->get_material, G_TYPE_INVALID);

  return iface->material_type;
}

/**
 * ges_extractable_get_id:
 * @self: The #GESExtractable
 *
 * Returns: The #id of the associated #GESMaterial
 */
const gchar *
ges_extractable_get_id (GESExtractable * self)
{
  GESExtractableInterface *iface;

  g_return_val_if_fail (GES_IS_EXTRACTABLE (self), NULL);

  iface = GES_EXTRACTABLE_GET_INTERFACE (self);
  g_return_val_if_fail (iface->get_id, NULL);

  return iface->get_id (self);
}

/**
 * ges_extractable_type_mandatory_parameters:
 * @type: The #GType implementing #GESExtractable
 *
 * Lists all the properties that needs to be passed to #ges_material_new to
 * be able to instanciate a #GESMaterial for the #GType pass to that same
 * function
 *
 * Returns: (transfer container) (element-type GParamSpec): an #GSlist of
 * GParamSpec* which should be freed after use.
 */
GSList *
ges_extractable_type_mandatory_parameters (GType type)
{
  guint nb_props, i;
  GObjectClass *klass;
  GParamSpec **all_specs;

  GSList *ret = NULL;

  g_return_val_if_fail (g_type_is_a (type, G_TYPE_OBJECT), NULL);
  g_return_val_if_fail (g_type_is_a (type, GES_TYPE_EXTRACTABLE), NULL);

  klass = g_type_class_ref (type);
  g_return_val_if_fail (G_IS_OBJECT_CLASS (klass), NULL);

  all_specs = g_object_class_list_properties (klass, &nb_props);

  for (i = 0; i < nb_props; i++) {
    if (all_specs[i]->flags & GES_PARAM_CONSTRUCT_MANDATORY)
      ret = g_slist_prepend (ret, all_specs[i]);
  }

  g_type_class_unref (klass);
  g_free (all_specs);

  return ret;
}

/**
 * ges_extractable_type_material_type:
 * @type: The #GType implementing #GESExtractable
 *
 * Get the #GType, subclass of #GES_TYPE_MATERIAL to instanciate
 * to be able to extract a @type
 *
 * Returns: the #GType to use to create a material to extract @type
 */
GType
ges_extractable_type_material_type (GType type)
{
  GObjectClass *klass;
  GESExtractableInterface *iface;

  g_return_val_if_fail (g_type_is_a (type, G_TYPE_OBJECT), G_TYPE_INVALID);
  g_return_val_if_fail (g_type_is_a (type, GES_TYPE_EXTRACTABLE),
      G_TYPE_INVALID);

  klass = g_type_class_ref (type);

  iface = g_type_interface_peek (klass, GES_TYPE_EXTRACTABLE);

  g_type_class_unref (klass);

  return iface->material_type;
}
