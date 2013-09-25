/* GStreamer Editing Services
 * Copyright (C) 2009 Edward Hervey <edward.hervey@collabora.co.uk>
 *               2009 Nokia Corporation
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:ges-video-source
 * @short_description: Base Class for video sources
 */

#include "ges-internal.h"
#include "ges/ges-meta-container.h"
#include "ges-track-element.h"
#include "ges-video-source.h"
#include "ges-layer.h"
#include "gstframepositionner.h"

G_DEFINE_ABSTRACT_TYPE (GESVideoSource, ges_video_source, GES_TYPE_SOURCE);

struct _GESVideoSourcePrivate
{
  GstFramePositionner *positionner;
  GstElement *capsfilter;
  GESLayer *layer;
};

/* TrackElement VMethods */
static void
layer_priority_changed_cb (GESLayer * layer, GParamSpec * arg G_GNUC_UNUSED,
    GESVideoSource * self)
{
  g_object_set (self->priv->positionner, "zorder",
      10000 - ges_layer_get_priority (layer), NULL);
}

static void
layer_changed_cb (GESClip * clip, GParamSpec * arg G_GNUC_UNUSED,
    GESVideoSource * self)
{
  GESVideoSourcePrivate *priv = self->priv;

  if (priv->layer) {
    g_signal_handlers_disconnect_by_func (priv->layer,
        layer_priority_changed_cb, self);
  }

  priv->layer = ges_clip_get_layer (clip);
  if (priv->layer == NULL)
    return;

  /* We do not need any ref ourself as our parent owns one and we are connected
   * to it */
  g_object_unref (priv->layer);
  /* 10000 is the max value of zorder on videomixerpad, hardcoded */
  g_signal_connect (self->priv->layer, "notify::priority",
      G_CALLBACK (layer_priority_changed_cb), self);

  g_object_set (self->priv->positionner, "zorder",
      10000 - ges_layer_get_priority (self->priv->layer), NULL);
}

static GstElement *
ges_video_source_create_element (GESTrackElement * trksrc)
{
  GstElement *topbin;
  GstElement *sub_element;
  GESVideoSourceClass *source_class = GES_VIDEO_SOURCE_GET_CLASS (trksrc);
  GESVideoSource *self;
  GstElement *positionner, *videoscale, *videorate, *capsfilter;
  const gchar *props[] = { "alpha", "posx", "posy", "width", "height", NULL };
  GESTimelineElement *parent;

  if (!source_class->create_source)
    return NULL;

  sub_element = source_class->create_source (trksrc);

  self = (GESVideoSource *) trksrc;

  /* That positionner will add metadata to buffers according to its
     properties, acting like a proxy for our smart-mixer dynamic pads. */
  positionner = gst_element_factory_make ("framepositionner", "frame_tagger");

  videoscale =
      gst_element_factory_make ("videoscale", "track-element-videoscale");
  videorate = gst_element_factory_make ("videorate", "track-element-videorate");
  capsfilter =
      gst_element_factory_make ("capsfilter", "track-element-capsfilter");

  g_object_set (videoscale, "add-borders", FALSE, NULL);

  ges_frame_positionner_set_source_and_filter (GST_FRAME_POSITIONNER
      (positionner), trksrc, capsfilter);

  ges_track_element_add_children_props (trksrc, positionner, NULL, NULL, props);
  topbin =
      ges_source_create_topbin ("videosrcbin", sub_element, positionner,
      videoscale, videorate, capsfilter, NULL);
  parent = ges_timeline_element_get_parent (GES_TIMELINE_ELEMENT (trksrc));
  if (parent) {
    self->priv->positionner = GST_FRAME_POSITIONNER (positionner);
    g_signal_connect (parent, "notify::layer",
        (GCallback) layer_changed_cb, trksrc);
    layer_changed_cb (GES_CLIP (parent), NULL, self);
    gst_object_unref (parent);
  } else {
    GST_ERROR ("No parent timeline element, SHOULD NOT HAPPEN");
  }

  self->priv->capsfilter = capsfilter;

  return topbin;
}

static gboolean
_set_parent (GESTimelineElement * self, GESTimelineElement * parent)
{
  GESVideoSourcePrivate *priv = GES_VIDEO_SOURCE (self)->priv;

  if (self->parent) {
    if (priv->layer) {
      g_signal_handlers_disconnect_by_func (priv->layer,
          layer_priority_changed_cb, self);
      priv->layer = NULL;
    }

    g_signal_handlers_disconnect_by_func (self->parent, layer_changed_cb, self);
  }

  if (parent && priv->positionner)
    layer_changed_cb (GES_CLIP (parent), NULL, GES_VIDEO_SOURCE (self));


  return TRUE;
}

static void
ges_video_source_class_init (GESVideoSourceClass * klass)
{
  GESTrackElementClass *track_class = GES_TRACK_ELEMENT_CLASS (klass);
  GESTimelineElementClass *element_class = GES_TIMELINE_ELEMENT_CLASS (klass);
  GESVideoSourceClass *video_source_class = GES_VIDEO_SOURCE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GESVideoSourcePrivate));

  element_class->set_parent = _set_parent;
  track_class->gnlobject_factorytype = "gnlsource";
  track_class->create_element = ges_video_source_create_element;
  video_source_class->create_source = NULL;
}

static void
ges_video_source_init (GESVideoSource * self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GES_TYPE_VIDEO_SOURCE, GESVideoSourcePrivate);
  self->priv->positionner = NULL;
  self->priv->capsfilter = NULL;
}
