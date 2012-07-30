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
  PROP_URI,
  PROP_LAST
};

static GParamSpec *properties[PROP_LAST];
static GstDiscoverer *discoverer;


static GStaticMutex discoverer_lock = G_STATIC_MUTEX_INIT;

static void discoverer_finished_cb (GstDiscoverer * discoverer);
static void
discoverer_discovered_cb (GstDiscoverer * discoverer,
    GstDiscovererInfo * info, GError * err);

struct _GESMaterialFileSourcePrivate
{
  gchar *uri;
  GstDiscovererInfo *info;
  GstClockTime duration;
};


static void
ges_material_filesource_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GESMaterialFileSource *material = GES_MATERIAL_FILESOURCE (object);

  switch (property_id) {
    case PROP_URI:
      g_value_set_string (value, material->priv->uri);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ges_material_filesource_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GESMaterialFileSource *material = GES_MATERIAL_FILESOURCE (object);

  switch (property_id) {
    case PROP_URI:
      ges_material_filesource_set_uri (material, g_value_dup_string (value));
      break;
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

static void
ges_material_filesource_load (GESMaterial * material,
    GCancellable * cancellable,
    GAsyncReadyCallback callback, gpointer user_data)
{
  gst_discoverer_start (discoverer);
  gst_discoverer_discover_uri_async (ges_material_filesource_get_discoverer (),
      GES_MATERIAL_FILESOURCE (material)->priv->uri);
}


static const gchar *
ges_material_filesource_get_id (GESMaterial * self)
{
  return (GES_MATERIAL_FILESOURCE (self)->priv->uri);
}

static void
ges_material_filesource_class_init (GESMaterialFileSourceClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  g_type_class_add_private (klass, sizeof (GESMaterialFileSourcePrivate));

  object_class->get_property = ges_material_filesource_get_property;
  object_class->set_property = ges_material_filesource_set_property;
  properties[PROP_URI] =
      g_param_spec_string ("uri",
      "URI of source material",
      "Get/set URI of source material",
      NULL, G_PARAM_CONSTRUCT | G_PARAM_READWRITE
      | GES_PARAM_CONSTRUCT_MANDATORY);

  g_object_class_install_properties (object_class, PROP_LAST, properties);
  GES_MATERIAL_CLASS (klass)->get_id = ges_material_filesource_get_id;
  GES_MATERIAL_CLASS (klass)->load = ges_material_filesource_load;
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
ges_material_filesource_set_uri (GESMaterialFileSource * self, gchar * uri)
{
  if (self->priv->uri)
    g_free (self->priv->uri);

  self->priv->uri = uri;
}

static void
discoverer_finished_cb (GstDiscoverer * discoverer)
{
}

static void
discoverer_discovered_cb (GstDiscoverer * discoverer,
    GstDiscovererInfo * info, GError * err)
{
  const gchar *uri = gst_discoverer_info_get_uri (info);
  ges_material_cache_set_loaded (uri);
}
