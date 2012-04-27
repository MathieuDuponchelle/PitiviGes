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

#define fast_element_link(a,b) gst_element_link_pads_full((a),"src",(b),"sink",GST_PAD_LINK_CHECK_NOTHING)

static GObject *link_element_to_mixer (GstElement * element,
    GstElement * mixer);

static GObject *link_element_to_mixer_with_smpte (GstBin * bin,
    GstElement * element, GstElement * mixer, gint type,
    GstElement ** smpteref);

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

static void
on_caps_set (GstPad * srca_pad, GParamSpec * pspec, GstElement * capsfilt)
{
  gint width, height;
  const GstStructure *str;
  GstCaps *size_caps = NULL;

  if (GST_PAD_CAPS (srca_pad)) {
    /* Get width and height of first video */
    str = gst_caps_get_structure (GST_PAD_CAPS (srca_pad), 0);
    gst_structure_get_int (str, "width", &width);
    gst_structure_get_int (str, "height", &height);

    /* Set capsfilter to the size of the first video */
    size_caps =
        gst_caps_new_simple ("video/x-raw-yuv",
        "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('A', 'Y', 'U', 'V'),
        "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, NULL);
    g_object_set (capsfilt, "caps", size_caps, NULL);
  }
}

static GstElement *
ges_track_video_transition_create_element (GESTrackObject * object)
{
  GstElement *topbin, *iconva, *iconvb, *scalea, *scaleb, *capsfilt, *capsfiltb,
      *oconv;
  GObject *target = NULL;
  const gchar *propname = NULL;
  GstElement *mixer = NULL;
  GstPad *sinka_target, *sinkb_target, *src_target, *sinka, *sinkb, *src,
      *srca_pad;
  GstController *controller;
  GstInterpolationControlSource *control_source;
  GESTrackVideoTransition *self;
  GESTrackVideoTransitionPrivate *priv;
  GstCaps *caps;

  self = GES_TRACK_VIDEO_TRANSITION (object);
  priv = self->priv;

  GST_LOG ("creating a video bin");

  topbin = gst_bin_new ("transition-bin");
  iconva = gst_element_factory_make ("ffmpegcolorspace", "tr-csp-a");
  iconvb = gst_element_factory_make ("ffmpegcolorspace", "tr-csp-b");
  scalea = gst_element_factory_make ("videoscale", "vs-a");
  scaleb = gst_element_factory_make ("videoscale", "vs-b");
  capsfilt = gst_element_factory_make ("capsfilter", "capsfilt");
  capsfiltb = gst_element_factory_make ("capsfilter", "capsfilt-b");
  oconv = gst_element_factory_make ("ffmpegcolorspace", "tr-csp-output");
  printf ("lolzob\n");
  caps = gst_caps_new_simple ("video/x-raw-yuv",
      "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('A', 'Y', 'U', 'V'), NULL);

  g_object_set (capsfiltb, "caps", caps, NULL);
  g_object_set (capsfilt, "caps", caps, NULL);
  gst_bin_add_many (GST_BIN (topbin), iconva, iconvb, scalea, scaleb, capsfilt,
      oconv, capsfiltb, NULL);
  printf ("dafuq\n");
  /* Prefer videomixer2 to videomixer */
  mixer = gst_element_factory_make ("videomixer2", NULL);
  if (mixer == NULL)
    mixer = gst_element_factory_make ("videomixer", NULL);
  g_object_set (G_OBJECT (mixer), "background", 1, NULL);
  gst_bin_add (GST_BIN (topbin), mixer);

  if (priv->type != GES_VIDEO_STANDARD_TRANSITION_TYPE_CROSSFADE) {
    priv->sinka =
        (GstPad *) link_element_to_mixer_with_smpte (GST_BIN (topbin), iconva,
        mixer, priv->type, NULL);
    priv->sinkb =
        (GstPad *) link_element_to_mixer_with_smpte (GST_BIN (topbin), iconvb,
        mixer, priv->type, &priv->smpte);
    target = (GObject *) priv->smpte;
    propname = "position";
    priv->start_value = 1.0;
    priv->end_value = 0.0;
  } else {
    gst_element_link_pads_full (iconva, "src", scalea, "sink",
        GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full (scalea, "src", capsfiltb, "sink",
        GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full (iconvb, "src", scaleb, "sink",
        GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full (scaleb, "src", capsfilt, "sink",
        GST_PAD_LINK_CHECK_NOTHING);

    priv->sinka = (GstPad *) link_element_to_mixer (capsfiltb, mixer);
    priv->sinkb = (GstPad *) link_element_to_mixer (capsfilt, mixer);
    target = (GObject *) priv->sinkb;
    propname = "alpha";
    priv->start_value = 0.0;
    priv->end_value = 1.0;
  }

  priv->mixer = gst_object_ref (mixer);

  fast_element_link (mixer, oconv);

  sinka_target = gst_element_get_static_pad (iconva, "sink");
  sinkb_target = gst_element_get_static_pad (iconvb, "sink");
  src_target = gst_element_get_static_pad (oconv, "src");

  sinka = gst_ghost_pad_new ("sinka", sinka_target);
  sinkb = gst_ghost_pad_new ("sinkb", sinkb_target);
  src = gst_ghost_pad_new ("src", src_target);

  gst_element_add_pad (topbin, src);
  gst_element_add_pad (topbin, sinka);
  gst_element_add_pad (topbin, sinkb);

  srca_pad = gst_element_get_static_pad (scalea, "src");
  g_signal_connect (srca_pad, "notify::caps", G_CALLBACK (on_caps_set),
      (GstElement *) capsfilt);

  gst_object_unref (sinka_target);
  gst_object_unref (sinkb_target);
  gst_object_unref (src_target);
  gst_object_unref (srca_pad);

  /* set up interpolation */

  g_object_set (target, propname, (gfloat) 0.0, NULL);

  controller = gst_object_control_properties (target, propname, NULL);

  control_source = gst_interpolation_control_source_new ();
  gst_controller_set_control_source (controller,
      propname, GST_CONTROL_SOURCE (control_source));
  gst_interpolation_control_source_set_interpolation_mode (control_source,
      GST_INTERPOLATE_LINEAR);

  priv->controller = controller;
  priv->control_source = control_source;
  priv->topbin = topbin;

  return topbin;
}

static void
my_blocked_pad (GstPad * sink, gboolean blocked,
    GESTrackVideoTransitionPrivate * priv)
{
  GstPad *peer;

  if (blocked) {
    GstPad *srcpad, *sinkpad;
    GstElement *smptealpha = gst_element_factory_make ("smptealpha", NULL);

    printf ("blocking\n");
    g_object_set (smptealpha,
        "type", (gint) priv->type, "invert", (gboolean) TRUE, NULL);
    gst_bin_add (GST_BIN (priv->topbin), smptealpha);
    peer = gst_pad_get_peer (sink);
    srcpad = gst_element_get_static_pad (smptealpha, "src");
    printf ("negotiated caps : %s\n",
        gst_caps_to_string (gst_pad_get_negotiated_caps (sink)));
    printf ("Can accept caps : %d\n", gst_pad_accept_caps (srcpad,
            gst_pad_get_negotiated_caps (sink)));
    printf ("Cause has caps : %s\n",
        gst_caps_to_string (gst_pad_get_caps (srcpad)));
    gst_pad_unlink (peer, sink);

    sinkpad = gst_element_get_static_pad (smptealpha, "sink");
    gst_pad_link_full (peer, sinkpad, GST_PAD_LINK_CHECK_NOTHING);
    gst_pad_set_active (sinkpad, TRUE);
    gst_object_unref (sinkpad);

    /* Link smpte src pad to our mixer sink */
    gst_pad_link (srcpad, sink);
    gst_pad_set_active (srcpad, TRUE);
    gst_object_unref (srcpad);

    if (!priv->smpte) {
      GValue start_value = { 0, };
      GValue end_value = { 0, };

      g_object_set (smptealpha, "position", (gfloat) 0.0, NULL);
      priv->controller =
          gst_object_control_properties (G_OBJECT (smptealpha), "position",
          NULL);
      gst_interpolation_control_source_unset_all (priv->control_source);
      priv->control_source = gst_interpolation_control_source_new ();
      gst_controller_set_control_source (priv->controller,
          "position", GST_CONTROL_SOURCE (priv->control_source));
      gst_interpolation_control_source_set_interpolation_mode
          (priv->control_source, GST_INTERPOLATE_LINEAR);
      g_value_init (&start_value, G_TYPE_DOUBLE);
      g_value_init (&end_value, G_TYPE_DOUBLE);
      g_value_set_double (&start_value, 1.0);
      g_value_set_double (&end_value, 0.0);
      gst_interpolation_control_source_set (priv->control_source, 0,
          &start_value);
      gst_interpolation_control_source_set (priv->control_source, priv->dur,
          &end_value);
    }
    priv->smpte = smptealpha;

    gst_pad_set_blocked_async (sink, FALSE,
        (GstPadBlockCallback) my_blocked_pad, NULL);
  } else
    printf ("unblocking\n");
}

static GObject *
link_element_to_mixer (GstElement * element, GstElement * mixer)
{
  GstPad *sinkpad = gst_element_get_request_pad (mixer, "sink_%d");
  GstPad *srcpad = gst_element_get_static_pad (element, "src");

  gst_pad_link_full (srcpad, sinkpad, GST_PAD_LINK_CHECK_NOTHING);
  gst_object_unref (srcpad);

  return G_OBJECT (sinkpad);
}

static GObject *
link_element_to_mixer_with_smpte (GstBin * bin, GstElement * element,
    GstElement * mixer, gint type, GstElement ** smpteref)
{
  GstPad *srcpad, *sinkpad;
  GstElement *smptealpha = gst_element_factory_make ("smptealpha", NULL);

  g_object_set (G_OBJECT (smptealpha),
      "type", (gint) type, "invert", (gboolean) TRUE, NULL);
  gst_bin_add (bin, smptealpha);

  fast_element_link (element, smptealpha);

  /* crack */
  if (smpteref) {
    *smpteref = smptealpha;
  }

  srcpad = gst_element_get_static_pad (smptealpha, "src");
  sinkpad = gst_element_get_request_pad (mixer, "sink_%d");
  gst_pad_link_full (srcpad, sinkpad, GST_PAD_LINK_CHECK_NOTHING);
  gst_object_unref (srcpad);

  return G_OBJECT (sinkpad);
}

static void
ges_track_video_transition_duration_changed (GESTrackObject * object,
    guint64 duration)
{
  GValue start_value = { 0, };
  GValue end_value = { 0, };
  GstElement *gnlobj = ges_track_object_get_gnlobject (object);
  GESTrackVideoTransition *self = GES_TRACK_VIDEO_TRANSITION (object);
  GESTrackVideoTransitionPrivate *priv = self->priv;

  GST_LOG ("updating controller");

  if (G_UNLIKELY (!gnlobj || !priv->control_source))
    return;

  GST_INFO ("duration: %" G_GUINT64_FORMAT, duration);
  g_value_init (&start_value, G_TYPE_DOUBLE);
  g_value_init (&end_value, G_TYPE_DOUBLE);
  g_value_set_double (&start_value, priv->start_value);
  g_value_set_double (&end_value, priv->end_value);

  GST_LOG ("setting values on controller");

  gst_interpolation_control_source_unset_all (priv->control_source);
  gst_interpolation_control_source_set (priv->control_source, 0, &start_value);
  gst_interpolation_control_source_set (priv->control_source,
      duration, &end_value);

  priv->dur = duration;
  GST_LOG ("done updating controller");
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

  GST_ERROR ("%p %d => %d", self, priv->type, type);

  if (priv->type && (priv->type != type) &&
      ((type == GES_VIDEO_STANDARD_TRANSITION_TYPE_CROSSFADE) ||
          (priv->type == GES_VIDEO_STANDARD_TRANSITION_TYPE_CROSSFADE))) {
    printf ("About to block pad\n");
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
  printf ("LOLOLOL\n");
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
