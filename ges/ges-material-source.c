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
* SECTION: ges-material-source
* @short_description: An object that is used to constuct another objects from files
*
* FIXME: Long description needed
*/
#include <gst/pbutils/pbutils.h>
#include "ges.h"
#include "ges-internal.h"

G_DEFINE_TYPE (GESMaterialFileSource, ges_material_filesource,
    GES_TYPE_MATERIAL);

enum
{
  PROP_0,
  PROP_LAST
};

static GstDiscoverer *discoverer = NULL;


static GStaticMutex discoverer_lock = G_STATIC_MUTEX_INIT;

static void discoverer_finished_cb (GstDiscoverer * discoverer);
static void
discoverer_discovered_cb (GstDiscoverer * discoverer,
    GstDiscovererInfo * info, GError * err, gpointer user_data);

struct _GESMaterialFileSourcePrivate
{
  GstDiscovererInfo *info;
  GstClockTime duration;
};


static void
ges_material_filesource_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ges_material_filesource_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static GstDiscoverer *
ges_material_filesource_get_discoverer (void)
{
  g_static_mutex_lock (&discoverer_lock);
  if (discoverer == NULL) {
    discoverer = gst_discoverer_new (15 * GST_SECOND, NULL);
    g_signal_connect (discoverer, "finished",
        G_CALLBACK (discoverer_finished_cb), NULL);
    g_signal_connect (discoverer, "discovered",
        G_CALLBACK (discoverer_discovered_cb), NULL);

  }
  g_static_mutex_unlock (&discoverer_lock);

  return discoverer;
}

static gboolean
ges_material_filesource_start_loading (GESMaterial * material)
{
  const gchar *uri;


  GST_DEBUG ("Started loading %p", material);

  gst_discoverer_start (ges_material_filesource_get_discoverer ());

  uri = ges_material_get_id (material);

  return
      gst_discoverer_discover_uri_async (ges_material_filesource_get_discoverer
      (), uri);
}


static void
ges_material_filesource_class_init (GESMaterialFileSourceClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  g_type_class_add_private (klass, sizeof (GESMaterialFileSourcePrivate));

  object_class->get_property = ges_material_filesource_get_property;
  object_class->set_property = ges_material_filesource_set_property;

  GES_MATERIAL_CLASS (klass)->start_loading =
      ges_material_filesource_start_loading;
}

static void
ges_material_filesource_init (GESMaterialFileSource * self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GES_TYPE_MATERIAL_FILESOURCE, GESMaterialFileSourcePrivate);

  self->priv->info = NULL;
  self->priv->duration = GST_CLOCK_TIME_NONE;
}


/**
 * ges_material_filesource_get_info:
 * @self: Target material
 *
 * Gets GstDiscoverer information about specified object
 *
 * Returns: (transfer none): GstDiscovererInfo of specified material
 */
GstDiscovererInfo *
ges_material_filesource_get_info (const GESMaterialFileSource * self)
{
  return self->priv->info;
}

void
ges_material_filesource_set_info (GESMaterialFileSource * self,
    GstDiscovererInfo * info)
{
  if (self->priv->info != NULL) {
    g_object_unref (self->priv->info);
  }
  self->priv->info = info;
}

static void
discoverer_finished_cb (GstDiscoverer * discoverer)
{
}

static void
discoverer_discovered_cb (GstDiscoverer * discoverer,
    GstDiscovererInfo * info, GError * err, gpointer user_data)
{
  const gchar *uri = gst_discoverer_info_get_uri (info);
  GESMaterialFileSource *mfs =
      GES_MATERIAL_FILESOURCE (ges_material_cache_lookup (uri));

  ges_material_filesource_set_info (mfs, info);
  ges_material_cache_set_loaded (uri, err);
}
