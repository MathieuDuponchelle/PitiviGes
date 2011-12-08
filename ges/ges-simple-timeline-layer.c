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

/**
 * SECTION:ges-simple-timeline-layer
 * @short_description: High-level #GESTimelineLayer
 *
 * #GESSimpleTimelineLayer allows using #GESTimelineObject(s) with a list-like
 * API. Clients can add any type of GESTimelineObject to a
 * GESSimpleTimelineLayer, and the layer will automatically compute the
 * appropriate start times.
 *
 * Users should be aware that GESTimelineTransition objects are considered to
 * have a negative duration for the purposes of positioning GESTimelineSource
 * objects (i.e., adding a GESTimelineTransition creates an overlap between
 * the two adjacent sources.
 */

#include <ges/ges.h>
#include "ges-internal.h"
#include "gesmarshal.h"

static void
ges_simple_timeline_layer_object_removed (GESTimelineLayer * layer,
    GESTimelineObject * object);

static void
ges_simple_timeline_layer_object_added (GESTimelineLayer * layer,
    GESTimelineObject * object);

static void
timeline_object_height_changed_cb (GESTimelineObject * object G_GNUC_UNUSED,
    GParamSpec * arg G_GNUC_UNUSED, GESSimpleTimelineLayer * layer);

static GList *get_objects (GESTimelineLayer * layer);

G_DEFINE_TYPE (GESSimpleTimelineLayer, ges_simple_timeline_layer,
    GES_TYPE_TIMELINE_LAYER);

struct _GESSimpleTimelineLayerPrivate
{
  /* Sorted list of objects */
  GList *objects;

  gboolean adding_object;
  gboolean valid;
};

enum
{
  OBJECT_MOVED,
  LAST_SIGNAL,
};

enum
{
  PROP_0,
  PROP_VALID,
  LAST_PROP,
};

static guint gstl_signals[LAST_SIGNAL] = { 0 };

static void
ges_simple_timeline_layer_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec)
{
  GESSimpleTimelineLayer *self;
  self = GES_SIMPLE_TIMELINE_LAYER (object);

  switch (property_id) {
    case PROP_VALID:
      g_value_set_boolean (value, self->priv->valid);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ges_simple_timeline_layer_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ges_simple_timeline_layer_class_init (GESSimpleTimelineLayerClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GESTimelineLayerClass *layer_class = GES_TIMELINE_LAYER_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GESSimpleTimelineLayerPrivate));

  object_class->get_property = ges_simple_timeline_layer_get_property;
  object_class->set_property = ges_simple_timeline_layer_set_property;

  /* Be informed when objects are being added/removed from elsewhere */
  layer_class->object_removed = ges_simple_timeline_layer_object_removed;
  layer_class->object_added = ges_simple_timeline_layer_object_added;
  layer_class->get_objects = get_objects;

  /**
   * GESSimpleTimelineLayer:valid:
   *
   * FALSE when the arrangement of objects in the layer would cause errors or
   * unexpected output during playback. Do not set the containing pipeline
   * state to PLAYING when this property is FALSE.
   */
  g_object_class_install_property (object_class, PROP_VALID,
      g_param_spec_boolean ("valid", "Valid",
          "Layer is in a valid configuration", FALSE, G_PARAM_READABLE));

  /**
   * GESSimpleTimelineLayer::object-moved
   * @layer: the #GESSimpleTimelineLayer
   * @object: the #GESTimelineObject that was added
   * @old: the previous position of the object
   * @new: the new position of the object
   *
   * Will be emitted when an object is moved with
   * #ges_simple_timeline_layer_move_object.
   */
  gstl_signals[OBJECT_MOVED] =
      g_signal_new ("object-moved", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (GESSimpleTimelineLayerClass,
          object_moved),
      NULL, NULL,
      ges_marshal_VOID__OBJECT_INT_INT, G_TYPE_NONE, 3,
      GES_TYPE_TIMELINE_OBJECT, G_TYPE_INT, G_TYPE_INT);
}

static void
ges_simple_timeline_layer_init (GESSimpleTimelineLayer * self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GES_TYPE_SIMPLE_TIMELINE_LAYER, GESSimpleTimelineLayerPrivate);

  self->priv->objects = NULL;
}

static void
gstl_recalculate (GESSimpleTimelineLayer * self)
{
  GList *tmp;
  gint64 pos = 0;
  gint priority = 0;
  gint transition_priority = 0;
  gint height;
  GESTimelineObject *prev_object = NULL;
  GESTimelineObject *prev_transition = NULL;
  gboolean valid = TRUE;
  GESSimpleTimelineLayerPrivate *priv = self->priv;

  priority = GES_TIMELINE_LAYER (self)->min_gnl_priority + 2;

  GST_DEBUG ("recalculating values");

  if (priv->objects && GES_IS_TIMELINE_TRANSITION (priv->objects->data)) {
    valid = FALSE;
  }

  for (tmp = priv->objects; tmp; tmp = tmp->next) {
    GESTimelineObject *obj;
    guint64 dur;
    GList *l_next;

    obj = (GESTimelineObject *) tmp->data;
    dur = GES_TIMELINE_OBJECT_DURATION (obj);
    height = GES_TIMELINE_OBJECT_HEIGHT (obj);

    if (GES_IS_TIMELINE_SOURCE (obj)) {

      GST_LOG ("%p obj: height: %d: priority %d", obj, height, priority);

      if (G_UNLIKELY (GES_TIMELINE_OBJECT_START (obj) != pos)) {
        ges_timeline_object_set_start (obj, pos);
      }

      if (G_UNLIKELY (GES_TIMELINE_OBJECT_PRIORITY (obj) != priority)) {
        ges_timeline_object_set_priority (obj, priority);
      }

      transition_priority = MAX (0, priority - 1);
      priority += height;
      pos += dur;

      g_assert (priority != -1);

    } else if (GES_IS_TIMELINE_TRANSITION (obj)) {

      pos -= dur;
      if (pos < 0)
        pos = 0;

      GST_LOG ("%p obj: height: %d: trans_priority %d Position: %d, "
          "duration %d", obj, height, transition_priority, pos);

      g_assert (transition_priority != -1);

      if (G_UNLIKELY (GES_TIMELINE_OBJECT_START (obj) != pos))
        ges_timeline_object_set_start (obj, pos);

      if (G_UNLIKELY (GES_TIMELINE_OBJECT_PRIORITY (obj) !=
              transition_priority)) {
        ges_timeline_object_set_priority (obj, transition_priority);
      }

      /* sanity checks */
      l_next = g_list_next (tmp);

      if (GES_IS_TIMELINE_TRANSITION (prev_object)) {
        GST_ERROR ("two transitions in sequence!");
        valid = FALSE;
      }

      if (prev_object && (GES_TIMELINE_OBJECT_DURATION (prev_object) < dur)) {
        GST_ERROR ("transition duration exceeds that of previous neighbor!");
        valid = FALSE;
      }

      if (l_next && (GES_TIMELINE_OBJECT_DURATION (l_next->data) < dur)) {
        GST_ERROR ("transition duration exceeds that of next neighbor!");
        valid = FALSE;
      }

      if (prev_transition) {
        guint64 start, end;
        end = (GES_TIMELINE_OBJECT_DURATION (prev_transition) +
            GES_TIMELINE_OBJECT_START (prev_transition));

        start = pos;

        if (end > start) {
          GST_ERROR ("%" G_GUINT64_FORMAT ", %" G_GUINT64_FORMAT ": "
              "overlapping transitions!", start, end);
          valid = FALSE;
        }
      }
      prev_transition = obj;
    }

    prev_object = obj;

  }

  if (prev_object && GES_IS_TIMELINE_TRANSITION (prev_object)) {
    valid = FALSE;
  }

  GST_DEBUG ("Finished recalculating: final start pos is: %" GST_TIME_FORMAT,
      GST_TIME_ARGS (pos));

  GES_TIMELINE_LAYER (self)->max_gnl_priority = priority;

  if (valid != self->priv->valid) {
    self->priv->valid = valid;
    g_object_notify (G_OBJECT (self), "valid");
  }
}

/**
 * ges_simple_timeline_layer_add_object:
 * @layer: a #GESSimpleTimelineLayer
 * @object: the #GESTimelineObject to add
 * @position: the position at which to add the object
 *
 * Adds the object at the given position in the layer. The position is where
 * the object will be inserted. To put the object before all objects, use
 * position 0. To put after all objects, use position -1.
 *
 * When adding transitions, it is important that the adjacent objects
 * (objects at position, and position + 1) be (1) A derivative of
 * GESTimelineSource or other non-transition, and (2) have a duration at least
 * as long as the duration of the transition.
 *
 * The layer will steal a reference to the provided object.
 *
 * Returns: TRUE if the object was successfuly added, else FALSE.
 */
gboolean
ges_simple_timeline_layer_add_object (GESSimpleTimelineLayer * layer,
    GESTimelineObject * object, gint position)
{
  gboolean res;
  GList *nth;
  GESSimpleTimelineLayerPrivate *priv = layer->priv;

  GST_DEBUG ("layer:%p, object:%p, position:%d", layer, object, position);

  nth = g_list_nth (priv->objects, position);

  if (GES_IS_TIMELINE_TRANSITION (object)) {
    GList *lprev = g_list_previous (nth);

    GESTimelineObject *prev = GES_TIMELINE_OBJECT (lprev ? lprev->data : NULL);
    GESTimelineObject *next = GES_TIMELINE_OBJECT (nth ? nth->data : NULL);

    if ((prev && GES_IS_TIMELINE_TRANSITION (prev)) ||
        (next && GES_IS_TIMELINE_TRANSITION (next))) {
      GST_ERROR ("Not adding transition: Only insert transitions between two"
          " sources, or at the begining or end the layer\n");
      return FALSE;
    }
  }


  priv->adding_object = TRUE;

  /* provisionally insert the object */
  priv->objects = g_list_insert (priv->objects, object, position);

  res = ges_timeline_layer_add_object ((GESTimelineLayer *) layer, object);

  /* Add to layer */
  if (G_UNLIKELY (!res)) {
    priv->adding_object = FALSE;
    /* we failed to add the object, so remove it from our list */
    priv->objects = g_list_remove (priv->objects, object);
    return FALSE;
  }

  priv->adding_object = FALSE;

  GST_DEBUG ("Adding object %p to the list", object);


  g_signal_connect (G_OBJECT (object), "notify::height", G_CALLBACK
      (timeline_object_height_changed_cb), layer);

  /* recalculate positions */
  gstl_recalculate (layer);

  return TRUE;
}

/**
 * ges_simple_timeline_layer_nth:
 * @layer: a #GESSimpleTimelineLayer
 * @position: The position in position to get, starting from 0.
 *
 * Gets the timeline object at the given position.
 *
 * Returns: (transfer none): The #GESTimelineObject at the given position or NULL if
 * the position is off the end of the layer.
 */

GESTimelineObject *
ges_simple_timeline_layer_nth (GESSimpleTimelineLayer * layer, gint position)
{
  GList *list;
  GESSimpleTimelineLayerPrivate *priv = layer->priv;

  list = g_list_nth (priv->objects, position);

  if (list)
    return GES_TIMELINE_OBJECT (list->data);

  return NULL;
}

/**
 * ges_simple_timeline_layer_index:
 * @layer: a #GESSimpleTimelineLayer
 * @object: a #GESTimelineObject in the layer
 *
 * Gets the position of the given object within the given layer.
 *
 * Returns: The position of the object starting from 0, or -1 if the
 * object was not found.
 */

gint
ges_simple_timeline_layer_index (GESSimpleTimelineLayer * layer,
    GESTimelineObject * object)
{
  GESSimpleTimelineLayerPrivate *priv = layer->priv;
  return g_list_index (priv->objects, object);
}

/**
 * ges_simple_timeline_layer_move_object:
 * @layer: a #GESSimpleTimelineLayer
 * @object: the #GESTimelineObject to move
 * @newposition: the new position at which to move the object
 *
 * Moves the object to the given position in the layer. To put the object before
 * all other objects, use position 0. To put the objects after all objects, use
 * position -1.
 *
 * Returns: TRUE if the object was successfuly moved, else FALSE.
 */

gboolean
ges_simple_timeline_layer_move_object (GESSimpleTimelineLayer * layer,
    GESTimelineObject * object, gint newposition)
{
  gint idx;
  GESSimpleTimelineLayerPrivate *priv = layer->priv;
  GESTimelineLayer *tl_obj_layer;

  GST_DEBUG ("layer:%p, object:%p, newposition:%d", layer, object, newposition);

  tl_obj_layer = ges_timeline_object_get_layer (object);
  if (G_UNLIKELY (tl_obj_layer != (GESTimelineLayer *) layer)) {
    GST_WARNING ("TimelineObject doesn't belong to this layer");
    if (tl_obj_layer != NULL)
      g_object_unref (tl_obj_layer);
    return FALSE;
  }
  if (tl_obj_layer != NULL)
    g_object_unref (tl_obj_layer);

  /* Find it's current position */
  idx = g_list_index (priv->objects, object);
  if (G_UNLIKELY (idx == -1)) {
    GST_WARNING ("TimelineObject not controlled by this layer");
    return FALSE;
  }

  GST_DEBUG ("Object was previously at position %d", idx);

  /* If we don't have to change its position, don't */
  if (idx == newposition)
    return TRUE;

  /* pop it off the list */
  priv->objects = g_list_remove (priv->objects, object);

  /* re-add it at the proper position */
  priv->objects = g_list_insert (priv->objects, object, newposition);

  /* recalculate positions */
  gstl_recalculate (layer);

  g_signal_emit (layer, gstl_signals[OBJECT_MOVED], 0, object, idx,
      newposition);

  return TRUE;
}

/**
 * ges_simple_timeline_layer_new:
 *
 * Creates a new #GESSimpleTimelineLayer.
 *
 * Returns: The new #GESSimpleTimelineLayer
 */
GESSimpleTimelineLayer *
ges_simple_timeline_layer_new (void)
{
  return g_object_new (GES_TYPE_SIMPLE_TIMELINE_LAYER, NULL);
}


/**
 * ges_simple_timeline_layer_is_valid:
 * @layer: a #GESSimpleTimelineLayer
 *
 * Checks whether the arrangement of objects in the layer would cause errors
 * or unexpected output during playback. Do not set the containing pipeline
 * state to PLAYING when this property is FALSE.
 *
 * Returns: #TRUE if current arrangement of the layer is valid else #FALSE.
 */
gboolean
ges_simple_timeline_layer_is_valid (GESSimpleTimelineLayer * layer)
{
  return layer->priv->valid;
}

static void
ges_simple_timeline_layer_object_removed (GESTimelineLayer * layer,
    GESTimelineObject * object)
{
  GESSimpleTimelineLayer *sl = (GESSimpleTimelineLayer *) layer;

  /* remove object from our list */
  sl->priv->objects = g_list_remove (sl->priv->objects, object);
  gstl_recalculate (sl);
}

static void
ges_simple_timeline_layer_object_added (GESTimelineLayer * layer,
    GESTimelineObject * object)
{
  GESSimpleTimelineLayer *sl = (GESSimpleTimelineLayer *) layer;

  if (sl->priv->adding_object == FALSE) {
    /* remove object from our list */
    sl->priv->objects = g_list_append (sl->priv->objects, object);
    gstl_recalculate (sl);
  }
  g_signal_connect_swapped (object, "notify::duration",
      G_CALLBACK (gstl_recalculate), layer);
}

static void
timeline_object_height_changed_cb (GESTimelineObject * object,
    GParamSpec * arg G_GNUC_UNUSED, GESSimpleTimelineLayer * layer)
{
  GST_LOG ("layer %p: notify height changed %p", layer, object);
  gstl_recalculate (layer);
}

static GList *
get_objects (GESTimelineLayer * l)
{
  GList *ret;
  GList *tmp;
  GESSimpleTimelineLayer *layer = (GESSimpleTimelineLayer *) l;

  ret = g_list_copy (layer->priv->objects);

  for (tmp = ret; tmp; tmp = tmp->next) {
    g_object_ref (tmp->data);
  }

  return ret;
}
