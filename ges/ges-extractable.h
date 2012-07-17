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

/* Default GESExtractable implementation */
#define GES_TYPE_EXTRACTABLE_OBJECT ges_extractable_object_get_type()
#define GES_EXTRACTABLE_OBJECT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GES_TYPE_EXTRACTABLE_OBJECT, GESExtractableObject))
#define GES_EXTRACTABLE_OBJECT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), GES_TYPE_EXTRACTABLE_OBJECT, GESExtractableObjectClass))
#define GES_IS_EXTRACTABLE_OBJECT(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GES_TYPE_EXTRACTABLE_OBJECT))
#define GES_IS_EXTRACTABLE_OBJECT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), GES_TYPE_EXTRACTABLE_OBJECT))
#define GES_EXTRACTABLE_OBJECT_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), GES_TYPE_EXTRACTABLE_OBJECT, GESExtractableObjectClass))
typedef struct _GESExtractableObjectPrivate GESExtractableObjectPrivate;
GType ges_extractable_object_get_type (void);

/* GESExtractable structures */
struct _GESExtractableInterface {
  GTypeInterface parent;
  GESMaterial* (*get_material) (GESExtractableInterface *self);
  GType (*get_material_type) (GESExtractableInterface *self);
};

/* Default GESExtrable implementation structures */
struct _GESExtractableObject
{
  GObject parent;
  /* <private> */
  GESExtractableObjectPrivate *priv;
  /* Padding for API extension */
  gpointer _ges_reserved[GES_PADDING];
};

struct _GESExtractableObjectClass
{
  GObjectClass parent;
  GParamSpecPool *mandatory_parameters;
  gpointer _ges_reserved[GES_PADDING];
};

/* GESExtractable helper functions */

GESMaterial* ges_extractable_get_material(GESExtractableInterface *self);
GType ges_extractable_get_material_type(GESExtractableInterface *self);


/* Default GESExtractable implementation functions */
GParamSpec** ges_extractable_object_class_get_mandatory_parameters(GESExtractableObjectClass* klass,
		guint *n_pspecs_p);

GESMaterial* ges_extractable_object_get_material(GESExtractableInterface *self);
GType ges_extractable_object_get_material_type(GESExtractableInterface *self);

G_END_DECLS
#endif /* _GES_EXTRACTABLE_ */
