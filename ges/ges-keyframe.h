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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _GES_KEYFRAME
#define _GES_KEYFRAME

#include <glib-object.h>
#include <gst/gst.h>
#include <ges/ges-types.h>
#include <ges/ges-enums.h>

G_BEGIN_DECLS

#define GES_TYPE_KEYFRAME ges_keyframe_get_type()

#define GES_KEYFRAME(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GES_TYPE_KEYFRAME, GESKeyframe))

#define GES_KEYFRAME_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GES_TYPE_KEYFRAME, GESKeyframeClass))

#define GES_IS_KEYFRAME(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GES_TYPE_KEYFRAME))

#define GES_IS_KEYFRAME_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GES_TYPE_KEYFRAME))

#define GES_KEYFRAME_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GES_TYPE_KEYFRAME, GESKeyframeClass))

typedef struct _GESKeyframePrivate GESKeyframePrivate;

/**
 * GESKeyframe:
 *
 * Controls a #GESTrackObject
 */

struct _GESKeyframe {
  GObject parent;

  /*< private >*/
  GESKeyframePrivate * priv;

  /* Padding for API extension */
  gpointer _ges_reserved[GES_PADDING];
};

/**
 * GESKeyframeClass:
 */

struct _GESKeyframeClass {
  /*< private >*/
  GstBinClass parent_class;

  /* Padding for API extension */
  gpointer _ges_reserved[GES_PADDING];
};

GType ges_keyframe_get_type (void);

GESKeyframe *
ges_keyframe_new (void);

void
ges_keyframe_set_timestamp(GESKeyframe *keyframe, GstClockTime timestamp);

GstClockTime
ges_keyframe_get_timestamp(GESKeyframe *self);

void
ges_keyframe_set_value(GESKeyframe *self, GValue value);

const GValue *
ges_keyframe_get_value(GESKeyframe *self);

G_END_DECLS

#endif /* _GES_KEYFRAME */
