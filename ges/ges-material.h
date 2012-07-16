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

#ifndef _GES_MATERIAL_
#define _GES_MATERIAL_

#include <glib-object.h>
#include <ges/ges-types.h>
#include <ges/ges-enums.h>

G_BEGIN_DECLS
#define GES_TYPE_MATERIAL ges_material_get_type()
#define GES_MATERIAL(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GES_TYPE_MATERIAL, GESMaterial))
#define GES_MATERIAL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), GES_TYPE_MATERIAL, GESMaterialClass))
#define GES_IS_MATERIAL(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GES_TYPE_MATERIAL))
#define GES_IS_MATERIAL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), GES_TYPE_MATERIAL))
#define GES_MATERIAL_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), GES_TYPE_MATERIAL, GESMaterialClass))
typedef struct _GESMaterialPrivate GESMaterialPrivate;

GType ges_material_get_type (void);
/* Abstract type (for now) */
struct _GESMaterial
{
  GObject parent;

  /* <private> */
  GESMaterialPrivate *priv;

  /* Padding for API extension */
  gpointer _ges_reserved[GES_PADDING];
};

struct _GESMaterialClass
{
  GObjectClass parent;

  gpointer _ges_reserved[GES_PADDING];
};


GType ges_material_get_extractable_type (GESMaterial * self);
GESMaterial *ges_material_new(GType *extractable_type,
    const gchar *first_property_name, ...);

G_END_DECLS
#endif /* _GES_MATERIAL */
