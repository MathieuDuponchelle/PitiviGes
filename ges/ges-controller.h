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

#ifndef _GES_CONTROLLER
#define _GES_CONTROLLER

#include <glib-object.h>
#include <gst/gst.h>
#include <ges/ges-types.h>
#include <ges/ges-enums.h>

#define GES_TYPE_CONTROLLER ges_controller_get_type()

#define GES_CONTROLLER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GES_TYPE_CONTROLLER, GESController))

#define GES_CONTROLLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GES_TYPE_CONTROLLER, GESControllerClass))

#define GES_IS_CONTROLLER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GES_TYPE_CONTROLLER))

#define GES_IS_CONTROLLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GES_TYPE_CONTROLLER))

#define GES_CONTROLLER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GES_TYPE_CONTROLLER, GESControllerClass))

typedef struct _GESControllerPrivate GESControllerPrivate;

/**
 * GESController:
 *
 * Controls a #GESTrackObject
 */

struct _GESController {
  GObject parent;

  /*< private >*/
  GESControllerPrivate * priv;

  /* Padding for API extension */
  gpointer _ges_reserved[GES_PADDING];
};

/**
 * GESControllerClass:
 */

struct _GESControllerClass {
  /*< private >*/
  GstBinClass parent_class;

  /* Padding for API extension */
  gpointer _ges_reserved[GES_PADDING];
};

typedef struct
{
  GstClockTime timestamp;
  GValue value;
} GESKeyframe;

typedef struct
{
  GList *keyframes;
  GstInterpolationControlSource *source;
} source_keyframes;

G_BEGIN_DECLS

GType ges_controller_get_type (void);

GESController*
ges_controller_new (GESTrackObject *track_object);

gboolean
ges_controller_add_keyframe(GESController *self, const gchar *param, guint64 timestamp, GValue value);

const GList *
ges_controller_get_keyframes(GESController *self, const gchar *param);

const GESKeyframe *
ges_controller_get_keyframe(GESController *self, const gchar *param, guint64 timestamp);

gboolean
ges_controller_remove_keyframe(GESController *self, const gchar *param, guint64 timestamp);

GESTrackObject *ges_controller_get_controlled(GESController *self);

void
ges_controller_set_controlled(GESController *self, GESTrackObject *controlled);


G_END_DECLS

#endif /* _GES_CONTROLLER */
