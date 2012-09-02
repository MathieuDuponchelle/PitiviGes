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
* SECTION: ges-material-project
* @short_description: An object that is used to manage materials of a project
*
* FIXME: Long description needed
*/
#include <gst/pbutils/pbutils.h>
#include "ges.h"
#include "ges-internal.h"

G_DEFINE_TYPE (GESMaterialProject, ges_material_project, GES_TYPE_MATERIAL);

struct _GESMaterialProjectPrivate
{
  GList *materials;
};


static void
ges_material_project_class_init (GESMaterialProjectClass * klass)
{
  g_type_class_add_private (klass, sizeof (GESMaterialProjectPrivate));

}

static void
ges_material_project_init (GESMaterialProject * self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GES_TYPE_MATERIAL_PROJECT, GESMaterialProjectPrivate);

  self->priv->materials = NULL;
}
