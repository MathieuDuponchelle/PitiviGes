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
#ifndef _GES_EXTRACTABLE_
#define _GES_EXTRACTABLE_

#include <glib-object.h>
#include <gio/gio.h>
#include <ges/ges-types.h>
#include <ges/ges-material.h>

G_BEGIN_DECLS

/* GESExtractable interface declarations */
#define GES_TYPE_EXTRACTABLE                (ges_extractable_get_type ())
#define GES_EXTRACTABLE(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), GES_TYPE_EXTRACTABLE, GESExtractable))
#define GES_IS_EXTRACTABLE(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GES_TYPE_EXTRACTABLE))
#define GES_EXTRACTABLE_GET_INTERFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GES_TYPE_EXTRACTABLE, GESExtractableInterface))

GType ges_extractable_get_type (void);

/**
 * GESExtractableGetMaterial:
 * @self: The #GESExtractable
 *
 * A function that return the #GESMaterial that instanciated the object that
 * implements that interface
 *
 * Returns: (transfer-mode none): The #GESMaterial from which @self has been
 * extracted
 */

typedef GESMaterial* (*GESExtractableGetMaterial) (GESExtractable *self);

/**
 * GESExtractableSetMaterial:
 * @self: The #GESExtractable
 * @material: The #GESMaterial to set as creator
 *
 * Sets the #GESMaterial responsible for object creation
 */

typedef void (*GESExtractableSetMaterial) (GESExtractable *self,
                                           GESMaterial *material);

/**
 * GESExtractableGetId:
 * @self: The #GESExtractable
 *
 * Returns: The #id of the associated #GESMaterial
 */

typedef const gchar * (*GESExtractableGetId) (GESExtractable *self);

typedef const gchar * (*GESExtractableGetIdForType) (GType type, va_list var_args);
/**
 * GESExtractable:
 * @get_material: A #GESExtractableGetMaterial function
 */
struct _GESExtractableInterface
{
  GTypeInterface parent;

  GType material_type;

  /*<protected>*/
  GESExtractableGetId get_id;
  GESExtractableGetMaterial get_material;
  GESExtractableSetMaterial set_material;
  GESExtractableGetIdForType get_id_for_type;

  gpointer _ges_reserved[GES_PADDING];
};

GType ges_extractable_get_material_type        (GESExtractable *self);
GType ges_extractable_type_material_type       (GType type);
const gchar* ges_extractable_get_id_for_type   (GType type, const gchar *first_property,
  va_list va_args);

/**
 * GESExtractableGetMaterial
 * @self: The #GESExtractable
 *
 * Gets material that was used to create this extractable object
 *
 * Returns: (transfer:none) : The material that was used to create specified extractable object
 */
GESMaterial* ges_extractable_get_material      (GESExtractable *self);

void ges_extractable_set_material              (GESExtractable *self,
                                                GESMaterial *material);

const gchar * ges_extractable_get_id           (GESExtractable *self);

GSList *
ges_extractable_type_mandatory_parameters      (GType type);

G_END_DECLS
#endif /* _GES_EXTRACTABLE_ */
