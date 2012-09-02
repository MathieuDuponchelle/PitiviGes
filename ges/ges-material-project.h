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
#ifndef _GES_MATERIAL_PROJECT_
#define _GES_MATERIAL_PROJECT_

#include <glib-object.h>
#include <gio/gio.h>
#include <ges/ges-types.h>
#include <ges/ges-material.h>

G_BEGIN_DECLS
#define GES_TYPE_MATERIAL_PROJECT ges_material_project_get_type()
#define GES_MATERIAL_PROJECT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GES_TYPE_MATERIAL_PROJECT, GESMaterialProject))
#define GES_MATERIAL_PROJECT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), GES_TYPE_MATERIAL_PROJECT, GESMaterialProjectClass))
#define GES_IS_MATERIAL_PROJECT(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GES_TYPE_MATERIAL_PROJECT))
#define GES_IS_MATERIAL_PROJECT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), GES_TYPE_MATERIAL_PROJECT))
#define GES_MATERIAL_PROJECT_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), GES_TYPE_MATERIAL_PROJECT, GESMaterialProjectClass))

typedef struct _GESMaterialProjectPrivate GESMaterialProjectPrivate;

GType ges_material_project_get_type (void);

struct _GESMaterialProject
{
  /* FIXME or GstObject? Does it have a parent? It has a name... */
  GESMaterial parent;

  /* <private> */
  GESMaterialProjectPrivate *priv;

  /* Padding for API extension */
  gpointer __ges_reserved[GES_PADDING];
};

struct _GESMaterialProjectClass
{
  GESMaterialClass parent_class;

  gpointer _ges_reserved[GES_PADDING];
};

void
ges_material_project_add_material(GESMaterialProject* self,
								  GESMaterial *material);
void
ges_material_project_remove_material(GESMaterialProject *self,
									 const gchar *id);

GList*
ges_material_project_get_materials(GESMaterialProject *self);

G_END_DECLS

#endif  /* _GES_MATERIAL_PROJECT */
