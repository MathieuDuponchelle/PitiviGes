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
#ifndef _GES_MATERIAL_SOURCE_
#define _GES_MATERIAL_SOURCE_

#include <glib-object.h>
#include <gio/gio.h>
#include <ges/ges-types.h>
#include <ges/ges-material.h>

G_BEGIN_DECLS
#define GES_TYPE_MATERIAL_SOURCE ges_material_source_get_type()
#define GES_MATERIAL_SOURCE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GES_TYPE_MATERIAL_SOURCE, GESMaterial))
#define GES_MATERIAL_SOURCE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), GES_TYPE_MATERIAL_SOURCE, GESMaterialSourceClass))
#define GES_IS_MATERIAL_SOURCE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GES_TYPE_MATERIAL_SOURCE))
#define GES_IS_MATERIAL_SOURCE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), GES_TYPE_MATERIAL_SOURCE))
#define GES_MATERIAL_SOURCE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), GES_TYPE_MATERIAL_SOURCE, GESMaterialSourceClass))
    
typedef struct _GESMaterialSourcePrivate GESMaterialSourcePrivate;

GType ges_material_source_get_type (void);

struct _GESMaterialSource
{
  /* FIXME or GstObject? Does it have a parent? It has a name... */
  GESMaterial parent;

  /* <private> */
  GESMaterialSourcePrivate *priv;

  /* Padding for API extension */
  gpointer __ges_reserved[GES_PADDING];
};

struct _GESMaterialSourceClass
{
  GESMaterialClass parent_class;

  gpointer _ges_reserved[GES_PADDING];
};

/* If material_created == NULL discover syncronously and return the material
 * If no need to discover @uri return the (refed) GESMaterialSource directly
 * otherwize return %NULL and call material_created when discovered
 */
GESMaterialSource *ges_material_source_new (const gchar * uri,
    GAsyncReadyCallback * material_created, gpointer user_data);

GstClockTime ges_material_source_get_duration (const GESMaterialSource *
    material);

GstDiscovererStreamInfo *ges_material_source_get_stream_info (const GESMaterialSource
    * material);

G_END_DECLS
#endif /* _GES_MATERIAL_SOURCE */
