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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:ges-video-transition
 * @short_description: implements video crossfade transition
 */

#include <ges/ges.h>
#include "ges-internal.h"

#include <gst/controller/gstdirectcontrolbinding.h>

G_DEFINE_TYPE (GESVideoTransition, ges_video_transition, GES_TYPE_TRANSITION);

static inline void
ges_video_transition_set_border_internal (GESVideoTransition * self,
    guint border);
static inline void
ges_video_transition_set_inverted_internal (GESVideoTransition *
    self, gboolean inverted);
static inline gboolean
ges_video_transition_set_transition_type_internal (GESVideoTransition
    * self, GESVideoStandardTransitionType type);
struct _GESVideoTransitionPrivate
{
  GESVideoStandardTransitionType type;

  /* prevents cases where the transitions have not been created yet */
  GESVideoStandardTransitionType pending_type;

  /* these enable video interpolation */
  GstTimedValueControlSource *crossfade_control_source;
  GstTimedValueControlSource *smpte_control_source;

  /* so we can support changing between wipes */
  GstElement *smpte;

  GstElement *crossfade_mixer;
  GstPad *crossfade_sinka;
  GstPad *crossfade_sinkb;

  GstElement *smpte_mixer;
  GstPad *smpte_sinka;
  GstPad *smpte_sinkb;

  GstElement *osela;
  GstElement *oselb;
  GstElement *isel;

  GstPad *crossfade_src1;
  GstPad *crossfade_src2;
  GstPad *crossfade_sink;

  GstPad *smpte_src1;
  GstPad *smpte_src2;
  GstPad *smpte_sink;

  /* This is in case the smpte doesn't exist yet */
  gint pending_border_value;
  gboolean pending_inverted;
};

enum
{
  PROP_0,
  PROP_BORDER,
  PROP_TRANSITION_TYPE,
  PROP_INVERT,
  PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

#define fast_element_link(a,b) gst_element_link_pads_full((a),"src",(b),"sink",GST_PAD_LINK_CHECK_NOTHING)

static GObject *link_element_to_mixer (GstElement * element,
    GstElement * mixer);

static GObject *link_element_to_mixer_with_smpte (GstBin * bin,
    GstElement * element, GstElement * mixer, gint type,
    GstElement ** smpteref);

static void
ges_video_transition_duration_changed (GESTrackElement * self,
    guint64 duration);

static GstElement *ges_video_transition_create_element (GESTrackElement * self);

static void ges_video_transition_dispose (GObject * object);

static void ges_video_transition_finalize (GObject * object);

static void ges_video_transition_get_property (GObject * object, guint
    property_id, GValue * value, GParamSpec * pspec);

static void ges_video_transition_set_property (GObject * object, guint
    property_id, const GValue * value, GParamSpec * pspec);

static void
duration_changed_cb (GESTrackElement * self, GParamSpec * arg G_GNUC_UNUSED)
{
  ges_video_transition_duration_changed (self,
      ges_timeline_element_get_duration (GES_TIMELINE_ELEMENT (self)));
}

static void
ges_video_transition_class_init (GESVideoTransitionClass * klass)
{
  GObjectClass *object_class;
  GESTrackElementClass *toclass;

  g_type_class_add_private (klass, sizeof (GESVideoTransitionPrivate));

  object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = ges_video_transition_get_property;
  object_class->set_property = ges_video_transition_set_property;
  object_class->dispose = ges_video_transition_dispose;
  object_class->finalize = ges_video_transition_finalize;

  /**
   * GESVideoTransition:border:
   *
   * This value represents the border width of the transition.
   *
   */
  properties[PROP_BORDER] =
      g_param_spec_uint ("border", "Border", "The border width", 0,
      G_MAXUINT, 0, G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_BORDER,
      properties[PROP_BORDER]);

  /**
   * GESVideoTransition:type:
   *
   * The #GESVideoStandardTransitionType currently applied on the object
   *
   */
  properties[PROP_TRANSITION_TYPE] =
      g_param_spec_enum ("transition-type", "Transition type",
      "The type of the transition", GES_VIDEO_STANDARD_TRANSITION_TYPE_TYPE,
      GES_VIDEO_STANDARD_TRANSITION_TYPE_NONE, G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_TRANSITION_TYPE,
      properties[PROP_TRANSITION_TYPE]);

  /**
   * GESVideoTransition:invert:
   *
   * This value represents the direction of the transition.
   *
   */
  properties[PROP_INVERT] =
      g_param_spec_boolean ("invert", "Invert",
      "Whether the transition is inverted", FALSE, G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_INVERT,
      properties[PROP_INVERT]);

  toclass = GES_TRACK_ELEMENT_CLASS (klass);
  toclass->create_element = ges_video_transition_create_element;
}

static void
ges_video_transition_init (GESVideoTransition * self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GES_TYPE_VIDEO_TRANSITION, GESVideoTransitionPrivate);

  self->priv->crossfade_control_source = NULL;
  self->priv->smpte_control_source = NULL;
  self->priv->smpte = NULL;
  self->priv->crossfade_mixer = NULL;
  self->priv->crossfade_sinka = NULL;
  self->priv->crossfade_sinkb = NULL;
  self->priv->smpte_mixer = NULL;
  self->priv->smpte_sinka = NULL;
  self->priv->smpte_sinkb = NULL;
  self->priv->type = GES_VIDEO_STANDARD_TRANSITION_TYPE_NONE;
  self->priv->pending_type = GES_VIDEO_STANDARD_TRANSITION_TYPE_CROSSFADE;
  self->priv->pending_border_value = -1;
  self->priv->pending_inverted = FALSE;
}

static void
release_mixer (GstElement ** mixer, GstPad ** sinka, GstPad ** sinkb)
{
  if (*sinka && *sinkb) {
    gst_element_release_request_pad (*mixer, *sinka);
    gst_element_release_request_pad (*mixer, *sinkb);
    gst_object_unref (*sinka);
    gst_object_unref (*sinkb);
    *sinka = NULL;
    *sinkb = NULL;
  }

  if (*mixer) {
    gst_object_unref (*mixer);
    *mixer = NULL;
  }
}

static void
ges_video_transition_dispose (GObject * object)
{
  GESVideoTransition *self = GES_VIDEO_TRANSITION (object);
  GESVideoTransitionPrivate *priv = self->priv;

  GST_DEBUG ("disposing");

  if (priv->crossfade_control_source) {
    gst_object_unref (priv->crossfade_control_source);
    priv->crossfade_control_source = NULL;
  }

  if (priv->smpte_control_source) {
    gst_object_unref (priv->smpte_control_source);
    priv->smpte_control_source = NULL;
  }

  release_mixer (&priv->crossfade_mixer, &priv->crossfade_sinka,
      &priv->crossfade_sinkb);
  release_mixer (&priv->smpte_mixer, &priv->smpte_sinka, &priv->smpte_sinkb);

  if (priv->isel) {
    gst_element_release_request_pad (priv->osela, priv->crossfade_src1);
    gst_element_release_request_pad (priv->oselb, priv->crossfade_src2);
    gst_element_release_request_pad (priv->osela, priv->smpte_src1);
    gst_element_release_request_pad (priv->oselb, priv->smpte_src2);
    gst_element_release_request_pad (priv->isel, priv->smpte_sink);
    gst_element_release_request_pad (priv->isel, priv->crossfade_sink);

    gst_object_unref (priv->crossfade_src1);
    gst_object_unref (priv->crossfade_src2);
    gst_object_unref (priv->smpte_src1);
    gst_object_unref (priv->smpte_src2);
    gst_object_unref (priv->smpte_sink);
    gst_object_unref (priv->smpte_sink);

    gst_object_unref (priv->osela);
    gst_object_unref (priv->oselb);
    gst_object_unref (priv->isel);
  }

  g_signal_handlers_disconnect_by_func (GES_TRACK_ELEMENT (self),
      duration_changed_cb, NULL);

  G_OBJECT_CLASS (ges_video_transition_parent_class)->dispose (object);
}

static void
ges_video_transition_finalize (GObject * object)
{
  G_OBJECT_CLASS (ges_video_transition_parent_class)->finalize (object);
}

static void
ges_video_transition_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec)
{
  GESVideoTransition *tr = GES_VIDEO_TRANSITION (object);

  switch (property_id) {
    case PROP_BORDER:
      g_value_set_uint (value, ges_video_transition_get_border (tr));
      break;
    case PROP_TRANSITION_TYPE:
      g_value_set_enum (value, ges_video_transition_get_transition_type (tr));
      break;
    case PROP_INVERT:
      g_value_set_boolean (value, ges_video_transition_is_inverted (tr));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ges_video_transition_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec)
{
  GESVideoTransition *tr = GES_VIDEO_TRANSITION (object);

  switch (property_id) {
    case PROP_BORDER:
      ges_video_transition_set_border_internal (tr, g_value_get_uint (value));
      break;
    case PROP_TRANSITION_TYPE:
      ges_video_transition_set_transition_type_internal (tr,
          g_value_get_enum (value));
      break;
    case PROP_INVERT:
      ges_video_transition_set_inverted_internal (tr,
          g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
on_caps_set (GstPad * srca_pad, GParamSpec * pspec, GstElement * capsfilt)
{
  GstCaps *orig_caps;

  orig_caps = gst_pad_get_current_caps (srca_pad);

  if (orig_caps) {
    gint width, height;
    const GstStructure *str;
    GstCaps *size_caps;

    /* Get width and height of first video */
    str = gst_caps_get_structure (orig_caps, 0);
    gst_structure_get_int (str, "width", &width);
    gst_structure_get_int (str, "height", &height);

    /* Set capsfilter to the size of the first video */
    size_caps =
        gst_caps_new_simple ("video/x-raw", "width", G_TYPE_INT, width,
        "height", G_TYPE_INT, height, NULL);
    g_object_set (capsfilt, "caps", size_caps, NULL);
    /* Shouldn't we need a reconfigure event here ? */
  }
}

static GstTimedValueControlSource *
set_interpolation (GstObject * element, GESVideoTransitionPrivate * priv,
    const gchar * propname)
{
  GstControlSource *control_source;

  g_object_set (element, propname, (gfloat) 0.0, NULL);

  control_source = gst_interpolation_control_source_new ();
  gst_object_add_control_binding (GST_OBJECT (element),
      gst_direct_control_binding_new (GST_OBJECT (element), propname,
          control_source));
  g_object_set (control_source, "mode", GST_INTERPOLATION_MODE_LINEAR, NULL);

  return GST_TIMED_VALUE_CONTROL_SOURCE (control_source);
}

static GstElement *
ges_video_transition_create_transition_bin (GESTrackElement * object,
    GESVideoStandardTransitionType type)
{
  GstElement *topbin, *iconva, *iconvb, *scalea, *scaleb, *capsfilt, *oconv;
  GstObject *target = NULL;
  const gchar *propname = NULL;
  GstElement *mixer = NULL;
  GstPad *sinka_target, *sinkb_target, *src_target, *sinka, *sinkb, *src,
      *srca_pad;
  GESVideoTransition *self;
  GESVideoTransitionPrivate *priv;

  self = GES_VIDEO_TRANSITION (object);
  priv = self->priv;

  GST_LOG ("creating a video bin");

  if (type == GES_VIDEO_STANDARD_TRANSITION_TYPE_CROSSFADE)
    topbin = gst_bin_new ("crossfade-bin");
  else
    topbin = gst_bin_new ("smpte-bin");

  iconva = gst_element_factory_make ("videoconvert", "tr-csp-a");
  iconvb = gst_element_factory_make ("videoconvert", "tr-csp-b");
  scalea = gst_element_factory_make ("videoscale", "vs-a");
  scaleb = gst_element_factory_make ("videoscale", "vs-b");
  capsfilt = gst_element_factory_make ("capsfilter", "capsfilt");
  oconv = gst_element_factory_make ("videoconvert", "tr-csp-output");

  gst_bin_add_many (GST_BIN (topbin), iconva, iconvb, scalea, scaleb, capsfilt,
      oconv, NULL);

  mixer = gst_element_factory_make ("videomixer", NULL);
  g_assert (mixer);
  g_object_set (G_OBJECT (mixer), "background", 1, NULL);
  gst_bin_add (GST_BIN (topbin), mixer);

  if (type != GES_VIDEO_STANDARD_TRANSITION_TYPE_CROSSFADE) {
    priv->smpte_sinka =
        (GstPad *) link_element_to_mixer_with_smpte (GST_BIN (topbin), iconva,
        mixer, type, NULL);
    priv->smpte_sinkb =
        (GstPad *) link_element_to_mixer_with_smpte (GST_BIN (topbin), iconvb,
        mixer, type, &priv->smpte);
    target = GST_OBJECT (priv->smpte);
    propname = "position";
  } else {
    gst_element_link_pads_full (iconva, "src", scalea, "sink",
        GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full (iconvb, "src", scaleb, "sink",
        GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full (scaleb, "src", capsfilt, "sink",
        GST_PAD_LINK_CHECK_NOTHING);

    priv->crossfade_sinka = (GstPad *) link_element_to_mixer (scalea, mixer);
    priv->crossfade_sinkb = (GstPad *) link_element_to_mixer (capsfilt, mixer);
    target = GST_OBJECT (priv->crossfade_sinkb);
    propname = "alpha";
  }

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

  if (type == GES_VIDEO_STANDARD_TRANSITION_TYPE_CROSSFADE) {
    priv->crossfade_control_source = set_interpolation (target, priv, propname);
    priv->crossfade_mixer = gst_object_ref (mixer);
  } else {
    priv->smpte_control_source = set_interpolation (target, priv, propname);
    priv->smpte_mixer = gst_object_ref (mixer);
  }

  return topbin;
}

static void
connect_transition_bin (GstElement * osela, GstElement * oselb,
    GstElement * isel, GstElement * transition_bin,
    GESVideoTransitionPrivate * priv, gboolean is_crossfade)
{
  GstPadTemplate *osel_template, *isel_template;
  GstPad *osela_src, *oselb_src;
  GstPad *sinka, *sinkb, *src;
  GstPad *isel_sink;

  osel_template =
      gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (osela),
      "src_%u");

  isel_template =
      gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (isel),
      "sink_%u");

  osela_src = gst_element_request_pad (osela, osel_template, NULL, NULL);
  oselb_src = gst_element_request_pad (oselb, osel_template, NULL, NULL);
  isel_sink = gst_element_request_pad (isel, isel_template, NULL, NULL);

  if (is_crossfade) {
    priv->crossfade_src1 = osela_src;
    priv->crossfade_src2 = oselb_src;
    priv->crossfade_sink = isel_sink;
  } else {
    priv->smpte_src1 = osela_src;
    priv->smpte_src2 = oselb_src;
    priv->smpte_sink = isel_sink;
  }

  sinka = gst_element_get_static_pad (transition_bin, "sinka");
  sinkb = gst_element_get_static_pad (transition_bin, "sinkb");
  src = gst_element_get_static_pad (transition_bin, "src");

  gst_pad_link_full (osela_src, sinka, GST_PAD_LINK_CHECK_NOTHING);
  gst_pad_link_full (oselb_src, sinkb, GST_PAD_LINK_CHECK_NOTHING);
  gst_pad_link_full (src, isel_sink, GST_PAD_LINK_CHECK_NOTHING);
}

static GstElement *
ges_video_transition_create_element (GESTrackElement * object)
{
  GstElement *topbin;
  GstElement *osela, *oselb, *isel;
  GstPad *sinka_target, *sinkb_target, *src_target, *sinka, *sinkb, *src;
  GstElement *crossfade_bin, *smpte_bin;
  GESVideoTransition *self;
  GESVideoTransitionPrivate *priv;

  self = GES_VIDEO_TRANSITION (object);
  priv = self->priv;

  topbin = gst_bin_new ("transition-bin-container");
  osela = gst_element_factory_make ("output-selector", "osela");
  oselb = gst_element_factory_make ("output-selector", "oselb");
  isel = gst_element_factory_make ("input-selector", "isel");

  smpte_bin =
      ges_video_transition_create_transition_bin (object,
      GES_VIDEO_STANDARD_TRANSITION_TYPE_BAR_WIPE_LR);
  crossfade_bin =
      ges_video_transition_create_transition_bin (object,
      GES_VIDEO_STANDARD_TRANSITION_TYPE_CROSSFADE);

  gst_bin_add_many (GST_BIN (topbin), osela, oselb, crossfade_bin, smpte_bin,
      isel, NULL);

  sinka_target = gst_element_get_static_pad (osela, "sink");
  sinkb_target = gst_element_get_static_pad (oselb, "sink");
  src_target = gst_element_get_static_pad (isel, "src");

  g_object_set (osela, "pad-negotiation-mode", 2, NULL);
  g_object_set (oselb, "pad-negotiation-mode", 2, NULL);

  sinka = gst_ghost_pad_new ("sinka", sinka_target);
  sinkb = gst_ghost_pad_new ("sinkb", sinkb_target);
  src = gst_ghost_pad_new ("src", src_target);

  gst_element_add_pad (topbin, src);
  gst_element_add_pad (topbin, sinka);
  gst_element_add_pad (topbin, sinkb);

  gst_object_unref (sinka_target);
  gst_object_unref (sinkb_target);
  gst_object_unref (src_target);

  connect_transition_bin (osela, oselb, isel, crossfade_bin, priv, TRUE);
  connect_transition_bin (osela, oselb, isel, smpte_bin, priv, FALSE);

  priv->osela = osela;
  priv->oselb = oselb;
  priv->isel = isel;

  ges_video_transition_set_transition_type_internal (self, priv->pending_type);

  ges_video_transition_duration_changed (object,
      ges_timeline_element_get_duration (GES_TIMELINE_ELEMENT (object)));

  g_signal_connect (object, "notify::duration",
      G_CALLBACK (duration_changed_cb), NULL);

  return topbin;
}

static GObject *
link_element_to_mixer (GstElement * element, GstElement * mixer)
{
  GstPad *sinkpad = gst_element_get_request_pad (mixer, "sink_%u");
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
  sinkpad = gst_element_get_request_pad (mixer, "sink_%u");
  gst_pad_link_full (srcpad, sinkpad, GST_PAD_LINK_CHECK_NOTHING);
  gst_object_unref (srcpad);

  return G_OBJECT (sinkpad);
}

static void
ges_video_transition_update_control_source (GstTimedValueControlSource * ts,
    guint64 duration, gdouble start_value, gdouble end_value)
{
  gst_timed_value_control_source_unset_all (ts);
  gst_timed_value_control_source_set (ts, 0, start_value);
  gst_timed_value_control_source_set (ts, duration, end_value);
}

static void
ges_video_transition_duration_changed (GESTrackElement * object,
    guint64 duration)
{
  GESVideoTransition *self = GES_VIDEO_TRANSITION (object);
  GESVideoTransitionPrivate *priv = self->priv;

  GST_LOG ("updating controller");

  ges_video_transition_update_control_source (priv->crossfade_control_source,
      duration, 0.0, 1.0);
  ges_video_transition_update_control_source (priv->smpte_control_source,
      duration, 1.0, 0.0);

  GST_LOG ("done updating controller");
}

static inline void
ges_video_transition_set_border_internal (GESVideoTransition * self,
    guint value)
{
  GESVideoTransitionPrivate *priv = self->priv;

  if (!priv->smpte) {
    priv->pending_border_value = value;
    return;
  }
  g_object_set (priv->smpte, "border", value, NULL);
}

static inline void
ges_video_transition_set_inverted_internal (GESVideoTransition *
    self, gboolean inverted)
{
  GESVideoTransitionPrivate *priv = self->priv;

  if (!priv->smpte) {
    priv->pending_inverted = !inverted;
    return;
  }
  g_object_set (priv->smpte, "invert", !inverted, NULL);
}


static inline gboolean
ges_video_transition_set_transition_type_internal (GESVideoTransition
    * self, GESVideoStandardTransitionType type)
{
  GESVideoTransitionPrivate *priv = self->priv;

  GST_DEBUG ("%p %d => %d", self, priv->type, type);

  if (!priv->isel) {
    priv->pending_type = type;
    return TRUE;
  }

  if (type == priv->type) {
    GST_INFO ("This type is already set on this transition\n");
    return TRUE;
  }

  if (type == GES_VIDEO_STANDARD_TRANSITION_TYPE_CROSSFADE) {
    g_object_set (priv->osela, "active-pad", priv->crossfade_src1, NULL);
    g_object_set (priv->oselb, "active-pad", priv->crossfade_src2, NULL);
    g_object_set (priv->isel, "active-pad", priv->crossfade_sink, NULL);
  } else {
    g_object_set (priv->osela, "active-pad", priv->smpte_src1, NULL);
    g_object_set (priv->oselb, "active-pad", priv->smpte_src2, NULL);
    g_object_set (priv->isel, "active-pad", priv->smpte_sink, NULL);
  }

  priv->type = type;

  if (type != GES_VIDEO_STANDARD_TRANSITION_TYPE_CROSSFADE) {
    g_object_set (priv->smpte, "type", (gint) type, NULL);
  }

  return TRUE;
}

/**
 * ges_video_transition_set_border:
 * @self: The #GESVideoTransition to set the border to
 * @value: The value of the border to set on @object
 *
 * Set the border property of @self, this value represents
 * the border width of the transition. In case this value does
 * not make sense for the current transition type, it is cached
 * for later use.
 */
void
ges_video_transition_set_border (GESVideoTransition * self, guint value)
{
  ges_video_transition_set_border_internal (self, value);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BORDER]);
}

/**
 * ges_video_transition_get_border:
 * @self: The #GESVideoTransition to get the border from
 *
 * Get the border property of @self, this value represents
 * the border width of the transition.
 *
 * Returns: The border values of @self or -1 if not meaningful
 * (this will happen when not using a smpte transition).
 */
gint
ges_video_transition_get_border (GESVideoTransition * self)
{
  gint value;

  if (!self->priv->smpte) {
    return -1;
  }

  g_object_get (self->priv->smpte, "border", &value, NULL);

  return value;
}

/**
 * ges_video_transition_set_inverted:
 * @self: The #GESVideoTransition to set invert on
 * @inverted: %TRUE if the transition should be inverted %FALSE otherwise
 *
 * Set the invert property of @self, this value represents
 * the direction of the transition. In case this value does
 * not make sense for the current transition type, it is cached
 * for later use.
 */
void
ges_video_transition_set_inverted (GESVideoTransition * self, gboolean inverted)
{
  ges_video_transition_set_inverted_internal (self, inverted);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INVERT]);
}

/**
 * ges_video_transition_is_inverted:
 * @self: The #GESVideoTransition to get the inversion from
 *
 * Get the invert property of @self, this value represents
 * the direction of the transition.
 *
 * Returns: The invert value of @self
 */
gboolean
ges_video_transition_is_inverted (GESVideoTransition * self)
{
  gboolean inverted;

  if (!self->priv->smpte) {
    return FALSE;
  }

  g_object_get (self->priv->smpte, "invert", &inverted, NULL);

  return !inverted;
}

/**
 * ges_video_transition_set_transition_type:
 * @self: a #GESVideoTransition
 * @type: a #GESVideoStandardTransitionType
 *
 * Sets the transition being used to @type.
 *
 * Returns: %TRUE if the transition type was properly changed, else %FALSE.
 */
gboolean
ges_video_transition_set_transition_type (GESVideoTransition * self,
    GESVideoStandardTransitionType type)
{
  gboolean ret = ges_video_transition_set_transition_type_internal (self, type);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TRANSITION_TYPE]);

  return ret;
}

/**
 * ges_video_transition_get_transition_type:
 * @trans: a #GESVideoTransition
 *
 * Get the transition type used by @trans.
 *
 * Returns: The transition type used by @trans.
 */
GESVideoStandardTransitionType
ges_video_transition_get_transition_type (GESVideoTransition * trans)
{
  if (trans->priv->pending_type)
    return trans->priv->pending_type;
  return trans->priv->type;
}

/**
 * ges_video_transition_new:
 *
 * Creates a new #GESVideoTransition.
 *
 * Returns: The newly created #GESVideoTransition, or %NULL if there was an
 * error.
 */
GESVideoTransition *
ges_video_transition_new (void)
{
  return g_object_new (GES_TYPE_VIDEO_TRANSITION, "track-type",
      GES_TRACK_TYPE_VIDEO, NULL);
}
