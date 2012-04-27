/* GStreamer Editing Services
 * Copyright (C) 2010 Brandon Lewis <brandon.lewis@collabora.co.uk>
 *               2010 Nokia Corporation
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
 * SECTION:ges-track-video-transition
 * @short_description: implements video crossfade transition
 */

#include <ges/ges.h>
#include "ges-internal.h"

G_DEFINE_TYPE (GESTrackVideoTransition, ges_track_video_transition,
    GES_TYPE_TRACK_TRANSITION);

struct _GESTrackVideoTransitionPrivate
{
  GESVideoStandardTransitionType type;

  /* these enable video interpolation */
  GstController *controller;
  GstInterpolationControlSource *control_source;

  /* so we can support changing between wipes */
  GstElement *topbin;
  GstElement *smpte;
  GstElement *mixer;
  GstPad *sinka;
  GstPad *sinkb;

  /* these will be different depending on whether smptealpha or alpha element
   * is used */
  gdouble start_value;
  gdouble end_value;
  guint64 dur;
};

enum
{
  PROP_0,
};

static void
ges_track_video_transition_duration_changed (GESTrackObject * self,
    guint64 duration);

static GstElement *ges_track_video_transition_create_element (GESTrackObject
    * self);

static void ges_track_video_transition_dispose (GObject * object);

static void ges_track_video_transition_finalize (GObject * object);

static void ges_track_video_transition_get_property (GObject * object, guint
    property_id, GValue * value, GParamSpec * pspec);

static void ges_track_video_transition_set_property (GObject * object, guint
    property_id, const GValue * value, GParamSpec * pspec);

static void
ges_track_video_transition_class_init (GESTrackVideoTransitionClass * klass)
{
  GObjectClass *object_class;
  GESTrackObjectClass *toclass;

  g_type_class_add_private (klass, sizeof (GESTrackVideoTransitionPrivate));

  object_class = G_OBJECT_CLASS (klass);
  toclass = GES_TRACK_OBJECT_CLASS (klass);

  object_class->get_property = ges_track_video_transition_get_property;
  object_class->set_property = ges_track_video_transition_set_property;
  object_class->dispose = ges_track_video_transition_dispose;
  object_class->finalize = ges_track_video_transition_finalize;

  toclass->duration_changed = ges_track_video_transition_duration_changed;
  toclass->create_element = ges_track_video_transition_create_element;
}

static void
ges_track_video_transition_init (GESTrackVideoTransition * self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GES_TYPE_TRACK_VIDEO_TRANSITION, GESTrackVideoTransitionPrivate);

  self->priv->controller = NULL;
  self->priv->control_source = NULL;
  self->priv->smpte = NULL;
  self->priv->mixer = NULL;
  self->priv->sinka = NULL;
  self->priv->sinkb = NULL;
  self->priv->topbin = NULL;
  self->priv->type = GES_VIDEO_STANDARD_TRANSITION_TYPE_NONE;
  self->priv->start_value = 0.0;
  self->priv->end_value = 0.0;
  self->priv->dur = 0;
}

static void
ges_track_video_transition_dispose (GObject * object)
{
  GESTrackVideoTransition *self = GES_TRACK_VIDEO_TRANSITION (object);
  GESTrackVideoTransitionPrivate *priv = self->priv;

  GST_DEBUG ("disposing");
  GST_LOG ("mixer: %p smpte: %p sinka: %p sinkb: %p",
      priv->mixer, priv->smpte, priv->sinka, priv->sinkb);

  if (priv->controller) {
    g_object_unref (priv->controller);
    priv->controller = NULL;
    if (priv->control_source)
      gst_object_unref (priv->control_source);
    priv->control_source = NULL;
  }

  if (priv->sinka && priv->sinkb) {
    GST_DEBUG ("releasing request pads for mixer");
    gst_element_release_request_pad (priv->mixer, priv->sinka);
    gst_element_release_request_pad (priv->mixer, priv->sinkb);
    gst_object_unref (priv->sinka);
    gst_object_unref (priv->sinkb);
    priv->sinka = NULL;
    priv->sinkb = NULL;
  }

  if (priv->mixer) {
    GST_LOG ("unrefing mixer");
    gst_object_unref (priv->mixer);
    priv->mixer = NULL;
  }

  G_OBJECT_CLASS (ges_track_video_transition_parent_class)->dispose (object);
}

static void
ges_track_video_transition_finalize (GObject * object)
{
  G_OBJECT_CLASS (ges_track_video_transition_parent_class)->finalize (object);
}

static void
ges_track_video_transition_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec)
{
  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ges_track_video_transition_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec)
{
  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static GstElement *
ges_track_video_transition_create_element (GESTrackObject * object)
{
  GstElement *topbin, *iconva, *iconvb, *oconv, *smpte;
  GstPad *sinka_target, *sinkb_target, *src_target, *sinka, *sinkb, *src;
  GESTrackVideoTransition *self;
  GESTrackVideoTransitionPrivate *priv;

  self = GES_TRACK_VIDEO_TRANSITION (object);
  priv = self->priv;

  GST_LOG ("creating a video bin");

  topbin = gst_bin_new ("transition-bin");
  iconva = gst_element_factory_make ("ffmpegcolorspace", "tr-csp-a");
  iconvb = gst_element_factory_make ("ffmpegcolorspace", "tr-csp-b");
  smpte = gst_element_factory_make ("smpte", "smpte");
  oconv = gst_element_factory_make ("ffmpegcolorspace", "tr-csp-output");

  gst_bin_add_many (GST_BIN (topbin), iconva, iconvb, oconv, smpte, NULL);

  g_object_set (smpte, "type", 3, NULL);

  if (priv->type != GES_VIDEO_STANDARD_TRANSITION_TYPE_CROSSFADE) {
    printf ("not a crossfade\n");
    gst_element_link_pads_full (iconva, "src", smpte, "sink1",
        GST_PAD_LINK_CHECK_CAPS);
    gst_element_link_pads_full (iconvb, "src", smpte, "sink2",
        GST_PAD_LINK_CHECK_CAPS);
  } else {

  }

  sinka_target = gst_element_get_static_pad (iconva, "sink");
  sinkb_target = gst_element_get_static_pad (iconvb, "sink");
  src_target = gst_element_get_static_pad (oconv, "src");

  sinkb = gst_ghost_pad_new ("sinka", sinkb_target);
  sinka = gst_ghost_pad_new ("sinkb", sinka_target);
  src = gst_ghost_pad_new ("src", src_target);

  gst_element_add_pad (topbin, src);
  gst_element_add_pad (topbin, sinkb);
  gst_element_add_pad (topbin, sinka);

  gst_element_link_pads_full (smpte, "src", oconv, "sink",
      GST_PAD_LINK_CHECK_CAPS);

  priv->smpte = gst_object_ref (smpte);

  gst_object_unref (sinka_target);
  gst_object_unref (sinkb_target);
  gst_object_unref (src_target);

  return topbin;
}

static void
my_blocked_pad (GstPad * sink, gboolean blocked,
    GESTrackVideoTransitionPrivate * priv)
{
  printf ("ENTERING\n");
  if (blocked) {
    /* We are gonna surgically replace the mixer with a smpte */
    GstPad *peer_src_pad;
    GstPad *smpte_sink_pad;

    peer_src_pad = gst_pad_get_peer (sink);
    gst_pad_unlink (peer_src_pad, sink);
    if (!priv->smpte) {
      GstPad *mixer_src_pad;
      GstPad *oconv_sink_pad;
      GstPad *smpte_src_pad;

      priv->smpte = gst_element_factory_make ("smpte", "smpte");
      g_object_set (G_OBJECT (priv->smpte), "duration", priv->dur, "type",
          priv->type, NULL);
      gst_bin_add (GST_BIN (priv->topbin), priv->smpte);
      gst_element_sync_state_with_parent (priv->smpte);

      mixer_src_pad = gst_element_get_static_pad (priv->mixer, "src");
      oconv_sink_pad = gst_pad_get_peer (mixer_src_pad);
      smpte_src_pad = gst_element_get_static_pad (priv->smpte, "src");

      gst_pad_unlink (mixer_src_pad, oconv_sink_pad);

      gst_pad_link_full (smpte_src_pad, oconv_sink_pad,
          GST_PAD_LINK_CHECK_CAPS);
      gst_pad_set_active (smpte_src_pad, TRUE);

      gst_object_unref (mixer_src_pad);
      gst_object_unref (oconv_sink_pad);
      gst_object_unref (smpte_src_pad);
    } else {
      gst_element_set_state (priv->mixer, GST_STATE_NULL);
      gst_bin_remove (GST_BIN (priv->topbin), priv->mixer);
    }
    if (priv->sinka == sink) {
      printf ("SINK A \n");
      smpte_sink_pad = gst_element_get_static_pad (priv->smpte, "sink1");
      priv->sinka = smpte_sink_pad;
    } else {
      printf ("SINK B\n");
      smpte_sink_pad = gst_element_get_static_pad (priv->smpte, "sink2");
      priv->sinkb = smpte_sink_pad;
    }
    if (smpte_sink_pad)
      printf ("The new pad has these caps : %s\n",
          gst_caps_to_string (gst_pad_get_caps (smpte_sink_pad)));
    else
      printf ("Couldn't get request_pad\n");
    gst_pad_link_full (peer_src_pad, smpte_sink_pad, GST_PAD_LINK_CHECK_CAPS);
    gst_pad_set_active (smpte_sink_pad, TRUE);
    gst_object_unref (peer_src_pad);
    gst_object_unref (sink);
    gst_pad_set_blocked_async (sink, FALSE,
        (GstPadBlockCallback) my_blocked_pad, NULL);
  } else
    printf ("unblocking\n");
}

static void
ges_track_video_transition_duration_changed (GESTrackObject * object,
    guint64 duration)
{
  GESTrackVideoTransition *self = GES_TRACK_VIDEO_TRANSITION (object);
  GESTrackVideoTransitionPrivate *priv = self->priv;

  printf ("lol\n");
  GST_LOG ("updating controller");
  if (priv->smpte)
    g_object_set (priv->smpte, "duration", duration, "type", 1, "border", 20000,
        NULL);
}

/**
 * ges_track_video_transition_set_transition_type:
 * @self: a #GESTrackVideoTransition
 * @type: a #GESVideoStandardTransitionType
 *
 * Sets the transition being used to @type.
 *
 * Returns: %TRUE if the transition type was properly changed, else %FALSE.
 */
gboolean
ges_track_video_transition_set_transition_type (GESTrackVideoTransition * self,
    GESVideoStandardTransitionType type)
{
  GESTrackVideoTransitionPrivate *priv = self->priv;

  GST_LOG ("%p %d => %d", self, priv->type, type);

  printf ("WHAT \n");
  return (TRUE);
  if (priv->type && (priv->type != type) &&
      ((type == GES_VIDEO_STANDARD_TRANSITION_TYPE_CROSSFADE) ||
          (priv->type == GES_VIDEO_STANDARD_TRANSITION_TYPE_CROSSFADE))) {
    priv->start_value = 1.0;
    priv->start_value = 0.0;
    priv->type = type;
    priv->smpte = NULL;
    gst_pad_set_blocked_async (priv->sinka, TRUE,
        (GstPadBlockCallback) my_blocked_pad, priv);
    gst_pad_set_blocked_async (priv->sinkb, TRUE,
        (GstPadBlockCallback) my_blocked_pad, priv);
    return TRUE;
  }
  priv->type = type;
  if (priv->smpte && (type != GES_VIDEO_STANDARD_TRANSITION_TYPE_CROSSFADE))
    g_object_set (priv->smpte, "type", (gint) type, NULL);
  return TRUE;
}

/**
 * ges_track_video_transition_get_transition_type:
 * @trans: a #GESTrackVideoTransition
 *
 * Get the transition type used by @trans.
 *
 * Returns: The transition type used by @trans.
 */
GESVideoStandardTransitionType
ges_track_video_transition_get_transition_type (GESTrackVideoTransition * trans)
{
  return trans->priv->type;
}

/**
 * ges_track_video_transition_new:
 *
 * Creates a new #GESTrackVideoTransition.
 *
 * Returns: The newly created #GESTrackVideoTransition, or %NULL if there was an
 * error.
 */
GESTrackVideoTransition *
ges_track_video_transition_new (void)
{
  return g_object_new (GES_TYPE_TRACK_VIDEO_TRANSITION, NULL);
}
