/* GStreamer
 * Copyright (C) 2001 Wim Taymans <wim.taymans@gmail.com>
 *               2004-2008 Edward Hervey <bilboed@bilboed.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gnl.h"

/**
 * SECTION:element-gnlcomposition
 * @short_description: Combines and controls GNonLin elements
 *
 * A GnlComposition contains GnlObjects such as GnlSources and GnlOperations,
 * and connects them dynamically to create a composition timeline.
 */

static GstStaticPadTemplate gnl_composition_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC (gnlcomposition_debug);
#define GST_CAT_DEFAULT gnlcomposition_debug

#define _do_init							\
  GST_DEBUG_CATEGORY_INIT (gnlcomposition_debug,"gnlcomposition", GST_DEBUG_FG_BLUE | GST_DEBUG_BOLD, "GNonLin Composition");
#define gnl_composition_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GnlComposition, gnl_composition, GNL_TYPE_OBJECT,
    _do_init);


enum
{
  PROP_0,
  PROP_UPDATE,
};

/* Properties from GnlObject */
enum
{
  GNLOBJECT_PROP_START,
  GNLOBJECT_PROP_STOP,
  GNLOBJECT_PROP_DURATION,
  GNLOBJECT_PROP_LAST
};

typedef struct _GnlCompositionEntry GnlCompositionEntry;

struct _GnlCompositionPrivate
{
  gboolean dispose_has_run;

  /* 
     Sorted List of GnlObjects , ThreadSafe 
     objects_start : sorted by start-time then priority
     objects_stop : sorted by stop-time then priority
     objects_hash : contains signal handlers id for controlled objects
     objects_lock : mutex to acces/modify any of those lists/hashtable
   */
  GList *objects_start;
  GList *objects_stop;
  GHashTable *objects_hash;
  GMutex objects_lock;

  /* Update properties
   * can_update: If True, updates should be taken into account immediatly, else
   *   they should be postponed until it is set to True again. 
   * update_required: Set to True if an update is required when the
   *   can_update property just above is set back to True. */
  gboolean can_update;
  gboolean update_required;

  /*
     thread-safe Seek handling.
     flushing_lock : mutex to access flushing and pending_idle
     flushing : 
     pending_idle :
   */
  GMutex flushing_lock;
  gboolean flushing;
  guint pending_idle;

  /* source top-level ghostpad, probe and entry */
  GstPad *ghostpad;
  gulong ghosteventprobe;
  GnlCompositionEntry *toplevelentry;

  /* current stack, list of GnlObject* */
  GNode *current;

  /* List of GnlObject whose start/duration will be the same as the composition */
  GList *expandables;

  /* TRUE if the stack is valid.
   * This is meant to prevent the top-level pad to be unblocked before the stack
   * is fully done. Protected by OBJECTS_LOCK */
  gboolean stackvalid;

  /*
     current segment seek start/stop time. 
     Reconstruct pipeline ONLY if seeking outside of those values
     FIXME : segment_start isn't always the earliest time before which the
     timeline doesn't need to be modified
   */
  GstClockTime segment_start;
  GstClockTime segment_stop;

  /* pending child seek */
  GstEvent *childseek;
  /* TRUE if a user seek with the flush flag was received */
  gboolean user_seek_flush;

  /* Seek segment handler */
  GstSegment *segment;
  GstSegment *outside_segment;

  /* number of pads we are waiting to appear so be can do proper linking */
  guint waitingpads;

  /*
     OUR sync_handler on the child_bus 
     We are called before gnl_object_sync_handler
   */
  GstPadEventFunction gnl_event_pad_func;

};

static GParamSpec *gnlobject_properties[GNLOBJECT_PROP_LAST];

#define OBJECT_IN_ACTIVE_SEGMENT(comp,element)			\
  ((GNL_OBJECT_START(element) < comp->priv->segment_stop) &&	\
   (GNL_OBJECT_STOP(element) >= comp->priv->segment_start))

static void gnl_composition_dispose (GObject * object);
static void gnl_composition_finalize (GObject * object);
static void gnl_composition_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspsec);
static void gnl_composition_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspsec);
static void gnl_composition_reset (GnlComposition * comp);

static gboolean gnl_composition_add_object (GstBin * bin, GstElement * element);

static void gnl_composition_handle_message (GstBin * bin, GstMessage * message);

static gboolean
gnl_composition_remove_object (GstBin * bin, GstElement * element);

static GstStateChangeReturn
gnl_composition_change_state (GstElement * element, GstStateChange transition);

static GstPad *get_src_pad (GstElement * element);
static GstPadProbeReturn pad_blocked (GstPad * pad, GstPadProbeInfo * info,
    GnlComposition * comp);
static inline void gnl_composition_remove_ghostpad (GnlComposition * comp);

static gboolean
seek_handling (GnlComposition * comp, gboolean initial, gboolean update);
static gint objects_start_compare (GnlObject * a, GnlObject * b);
static gint objects_stop_compare (GnlObject * a, GnlObject * b);
static GstClockTime get_current_position (GnlComposition * comp);

static void gnl_composition_set_update (GnlComposition * comp, gboolean update);
static gboolean update_pipeline (GnlComposition * comp,
    GstClockTime currenttime, gboolean initial, gboolean change_state,
    gboolean modify);
static void no_more_pads_object_cb (GstElement * element,
    GnlComposition * comp);


/* COMP_REAL_START: actual position to start current playback at. */
#define COMP_REAL_START(comp)					\
  (MAX (comp->priv->segment->start, GNL_OBJECT_START (comp)))

#define COMP_REAL_STOP(comp)						\
  ((GST_CLOCK_TIME_IS_VALID (comp->priv->segment->stop)			\
    ? (MIN (comp->priv->segment->stop, GNL_OBJECT_STOP (comp))))	\
   : GNL_OBJECT_STOP (comp))

#define COMP_ENTRY(comp, object)					\
  (g_hash_table_lookup (comp->priv->objects_hash, (gconstpointer) object))

#define COMP_OBJECTS_LOCK(comp) G_STMT_START {				\
    GST_LOG_OBJECT (comp, "locking objects_lock from thread %p",	\
		    g_thread_self());					\
    g_mutex_lock (&comp->priv->objects_lock);				\
    GST_LOG_OBJECT (comp, "locked objects_lock from thread %p",		\
		    g_thread_self());					\
  } G_STMT_END

#define COMP_OBJECTS_UNLOCK(comp) G_STMT_START {			\
    GST_LOG_OBJECT (comp, "unlocking objects_lock from thread %p",	\
		    g_thread_self());					\
    g_mutex_unlock (&comp->priv->objects_lock);				\
  } G_STMT_END


#define COMP_FLUSHING_LOCK(comp) G_STMT_START {				\
    GST_LOG_OBJECT (comp, "locking flushing_lock from thread %p",	\
		    g_thread_self());					\
    g_mutex_lock (&comp->priv->flushing_lock);				\
    GST_LOG_OBJECT (comp, "locked flushing_lock from thread %p",	\
		    g_thread_self());					\
  } G_STMT_END

#define COMP_FLUSHING_UNLOCK(comp) G_STMT_START {			\
    GST_LOG_OBJECT (comp, "unlocking flushing_lock from thread %p",	\
		    g_thread_self());					\
    g_mutex_unlock (&comp->priv->flushing_lock);				\
  } G_STMT_END


struct _GnlCompositionEntry
{
  GnlObject *object;

  /* handler ids for property notifications */
  gulong starthandler;
  gulong stophandler;
  gulong priorityhandler;
  gulong activehandler;

  /* handler id for 'no-more-pads' signal */
  gulong nomorepadshandler;
  gulong padaddedhandler;
  gulong padremovedhandler;

  /* handler id for block probe */
  gulong probeid;
};

static void
gnl_composition_class_init (GnlCompositionClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBinClass *gstbin_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbin_class = (GstBinClass *) klass;

  g_type_class_add_private (klass, sizeof (GnlCompositionPrivate));

  gst_element_class_set_static_metadata (gstelement_class,
      "GNonLin Composition", "Filter/Editor", "Combines GNL objects",
      "Wim Taymans <wim.taymans@gmail.com>, Edward Hervey <bilboed@bilboed.com>");

  gobject_class->dispose = GST_DEBUG_FUNCPTR (gnl_composition_dispose);
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gnl_composition_finalize);
  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gnl_composition_set_property);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gnl_composition_get_property);

  gstelement_class->change_state = gnl_composition_change_state;

  gstbin_class->add_element = GST_DEBUG_FUNCPTR (gnl_composition_add_object);
  gstbin_class->remove_element =
      GST_DEBUG_FUNCPTR (gnl_composition_remove_object);
  gstbin_class->handle_message =
      GST_DEBUG_FUNCPTR (gnl_composition_handle_message);

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gnl_composition_src_template));

  /**
   * GnlComposition:update:
   *
   * If %TRUE, then all modifications to objects within the composition will
   * cause a internal pipeline update (if required).
   * If %FALSE, then only the composition's start/duration/stop properties
   * will be updated, and the internal pipeline will only be updated when the
   * property is set back to %TRUE.
   *
   * It is recommended to temporarily set this property to %FALSE before doing
   * more than one modification in the composition (like adding/moving/removing
   * several objects at once) in order to speed up the process, and then setting
   * back the property to %TRUE when done.
   */

  g_object_class_install_property (gobject_class, PROP_UPDATE,
      g_param_spec_boolean ("update", "Update",
          "Update the internal pipeline on every modification", TRUE,
          G_PARAM_READWRITE));

  /* Get the paramspec of the GnlObject klass so we can do
   * fast notifies */
  gnlobject_properties[GNLOBJECT_PROP_START] =
      g_object_class_find_property (gobject_class, "start");
  gnlobject_properties[GNLOBJECT_PROP_STOP] =
      g_object_class_find_property (gobject_class, "stop");
  gnlobject_properties[GNLOBJECT_PROP_DURATION] =
      g_object_class_find_property (gobject_class, "duration");
}

static void
hash_value_destroy (GnlCompositionEntry * entry)
{
  if (entry->starthandler)
    g_signal_handler_disconnect (entry->object, entry->starthandler);
  if (entry->stophandler)
    g_signal_handler_disconnect (entry->object, entry->stophandler);
  if (entry->priorityhandler)
    g_signal_handler_disconnect (entry->object, entry->priorityhandler);
  g_signal_handler_disconnect (entry->object, entry->activehandler);
  g_signal_handler_disconnect (entry->object, entry->padremovedhandler);
  g_signal_handler_disconnect (entry->object, entry->padaddedhandler);

  if (entry->nomorepadshandler)
    g_signal_handler_disconnect (entry->object, entry->nomorepadshandler);
  g_slice_free (GnlCompositionEntry, entry);
}

static void
gnl_composition_init (GnlComposition * comp)
{
  GnlCompositionPrivate *priv;

  GST_OBJECT_FLAG_SET (comp, GNL_OBJECT_SOURCE);

  priv = G_TYPE_INSTANCE_GET_PRIVATE (comp, GNL_TYPE_COMPOSITION,
      GnlCompositionPrivate);
  g_mutex_init (&priv->objects_lock);
  priv->objects_start = NULL;
  priv->objects_stop = NULL;

  priv->can_update = TRUE;
  priv->update_required = FALSE;

  g_mutex_init (&priv->flushing_lock);
  priv->flushing = FALSE;
  priv->pending_idle = 0;

  priv->segment = gst_segment_new ();
  priv->outside_segment = gst_segment_new ();

  priv->waitingpads = 0;

  priv->objects_hash = g_hash_table_new_full
      (g_direct_hash,
      g_direct_equal, NULL, (GDestroyNotify) hash_value_destroy);

  comp->priv = priv;

  gnl_composition_reset (comp);
}

static void
gnl_composition_dispose (GObject * object)
{
  GnlComposition *comp = GNL_COMPOSITION (object);
  GnlCompositionPrivate *priv = comp->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  priv->can_update = TRUE;
  priv->update_required = FALSE;

  if (priv->ghostpad)
    gnl_composition_remove_ghostpad (comp);

  if (priv->childseek) {
    gst_event_unref (priv->childseek);
    priv->childseek = NULL;
  }
  comp->priv->user_seek_flush = FALSE;

  if (priv->current) {
    g_node_destroy (priv->current);
    priv->current = NULL;
  }

  if (priv->expandables) {
    g_list_free (priv->expandables);
    priv->expandables = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gnl_composition_finalize (GObject * object)
{
  GnlComposition *comp = GNL_COMPOSITION (object);
  GnlCompositionPrivate *priv = comp->priv;

  GST_INFO ("finalize");

  COMP_OBJECTS_LOCK (comp);
  g_list_free (priv->objects_start);
  g_list_free (priv->objects_stop);
  if (priv->current)
    g_node_destroy (priv->current);
  g_hash_table_destroy (priv->objects_hash);
  COMP_OBJECTS_UNLOCK (comp);

  gst_segment_free (priv->segment);
  gst_segment_free (priv->outside_segment);

  g_mutex_clear (&priv->objects_lock);
  g_mutex_clear (&priv->flushing_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gnl_composition_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GnlComposition *comp = (GnlComposition *) object;

  switch (prop_id) {
    case PROP_UPDATE:
      gnl_composition_set_update (comp, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gnl_composition_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GnlComposition *comp = (GnlComposition *) object;

  switch (prop_id) {
    case PROP_UPDATE:
      g_value_set_boolean (value, comp->priv->can_update);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
wait_no_more_pads (GnlComposition * comp, gpointer object,
    GnlCompositionEntry * entry, gboolean wait)
{
  if (wait) {
    GST_INFO_OBJECT (object, "no existing pad, connecting to 'no-more-pads'");
    entry->nomorepadshandler = g_signal_connect
        (G_OBJECT (object), "no-more-pads",
        G_CALLBACK (no_more_pads_object_cb), comp);
    comp->priv->waitingpads++;
  } else {
    GST_INFO_OBJECT (object, "disconnecting from 'no-more-pads'");
    g_signal_handler_disconnect (object, entry->nomorepadshandler);
    entry->nomorepadshandler = 0;
    comp->priv->waitingpads--;
  }

  GST_INFO_OBJECT (comp, "the number of waiting pads is now %d",
      comp->priv->waitingpads);
}

/* signal_duration_change
 * Creates a new GST_MESSAGE_DURATION_CHANGED with the currently configured
 * composition duration and sends that on the bus.
 */

static inline void
signal_duration_change (GnlComposition * comp)
{
  gst_element_post_message (GST_ELEMENT_CAST (comp),
      gst_message_new_duration_changed (GST_OBJECT_CAST (comp)));
}

static gboolean
unblock_child_pads (GValue * item, GValue * ret G_GNUC_UNUSED,
    GnlComposition * comp)
{
  GstPad *pad;
  GstElement *child = g_value_get_object (item);
  GnlCompositionEntry *entry = COMP_ENTRY (comp, child);

  GST_DEBUG_OBJECT (child, "unblocking pads");

  pad = get_src_pad (child);
  if (pad) {
    if (entry->probeid) {
      gst_pad_remove_probe (pad, entry->probeid);
      entry->probeid = 0;
    }
    gst_object_unref (pad);
  }
  return TRUE;
}

static void
unblock_childs (GnlComposition * comp)
{
  GstIterator *childs;

  childs = gst_bin_iterate_elements (GST_BIN (comp));

retry:
  if (G_UNLIKELY (gst_iterator_fold (childs,
              (GstIteratorFoldFunction) unblock_child_pads, NULL,
              comp) == GST_ITERATOR_RESYNC)) {
    gst_iterator_resync (childs);
    goto retry;
  }
  gst_iterator_free (childs);
}


static gboolean
reset_child (GValue * item, GValue * ret G_GNUC_UNUSED, gpointer user_data)
{
  GnlCompositionEntry *entry;
  GstElement *child = g_value_get_object (item);
  GnlComposition *comp = GNL_COMPOSITION (user_data);

  GST_DEBUG_OBJECT (child, "unlocking state");
  gst_element_set_locked_state (child, FALSE);

  entry = COMP_ENTRY (comp, child);
  if (entry->nomorepadshandler)
    wait_no_more_pads (comp, child, entry, FALSE);

  return TRUE;
}

static gboolean
lock_child_state (GValue * item, GValue * ret G_GNUC_UNUSED,
    gpointer udata G_GNUC_UNUSED)
{
  GstElement *child = g_value_get_object (item);

  GST_DEBUG_OBJECT (child, "locking state");
  gst_element_set_locked_state (child, TRUE);

  return TRUE;
}

static void
reset_childs (GnlComposition * comp)
{
  GstIterator *childs;

  childs = gst_bin_iterate_elements (GST_BIN (comp));
retry:
  if (G_UNLIKELY (gst_iterator_fold (childs,
              (GstIteratorFoldFunction) reset_child, NULL,
              comp) == GST_ITERATOR_RESYNC)) {
    gst_iterator_resync (childs);
    goto retry;
  }
  gst_iterator_free (childs);
}

static void
gnl_composition_reset (GnlComposition * comp)
{
  GnlCompositionPrivate *priv = comp->priv;

  GST_DEBUG_OBJECT (comp, "resetting");

  priv->segment_start = GST_CLOCK_TIME_NONE;
  priv->segment_stop = GST_CLOCK_TIME_NONE;

  gst_segment_init (priv->segment, GST_FORMAT_TIME);
  gst_segment_init (priv->outside_segment, GST_FORMAT_TIME);

  if (priv->current)
    g_node_destroy (priv->current);
  priv->current = NULL;

  priv->stackvalid = FALSE;

  if (priv->ghostpad)
    gnl_composition_remove_ghostpad (comp);

  if (priv->childseek) {
    gst_event_unref (priv->childseek);
    priv->childseek = NULL;
  }
  comp->priv->user_seek_flush = FALSE;

  reset_childs (comp);

  COMP_FLUSHING_LOCK (comp);

  if (priv->pending_idle)
    g_source_remove (priv->pending_idle);
  priv->pending_idle = 0;
  priv->flushing = FALSE;

  COMP_FLUSHING_UNLOCK (comp);

  priv->update_required = FALSE;

  GST_DEBUG_OBJECT (comp, "Composition now resetted");
}

static gboolean
eos_main_thread (GnlComposition * comp)
{
  GnlCompositionPrivate *priv = comp->priv;
  gboolean reverse = (priv->segment->rate < 0.0);

  /* Set up a non-initial seek on segment_stop */

  if (!reverse) {
    GST_DEBUG_OBJECT (comp,
        "Setting segment->start to segment_stop:%" GST_TIME_FORMAT,
        GST_TIME_ARGS (priv->segment_stop));
    priv->segment->start = priv->segment_stop;
  } else {
    GST_DEBUG_OBJECT (comp,
        "Setting segment->stop to segment_start:%" GST_TIME_FORMAT,
        GST_TIME_ARGS (priv->segment_start));
    priv->segment->stop = priv->segment_start;
  }

  seek_handling (comp, TRUE, TRUE);

  if (!priv->current) {
    /* If we're at the end, post SEGMENT_DONE, or push EOS */
    GST_DEBUG_OBJECT (comp, "Nothing else to play");

    if (!(priv->segment->flags & GST_SEEK_FLAG_SEGMENT)
        && priv->ghostpad) {
      GST_LOG_OBJECT (comp, "Pushing out EOS");
      gst_pad_push_event (priv->ghostpad, gst_event_new_eos ());
    } else if (priv->segment->flags & GST_SEEK_FLAG_SEGMENT) {
      gint64 epos;

      if (GST_CLOCK_TIME_IS_VALID (priv->segment->stop))
        epos = (MIN (priv->segment->stop, GNL_OBJECT_STOP (comp)));
      else
        epos = GNL_OBJECT_STOP (comp);

      GST_LOG_OBJECT (comp, "Emitting segment done pos %" GST_TIME_FORMAT,
          GST_TIME_ARGS (epos));
      gst_element_post_message (GST_ELEMENT_CAST (comp),
          gst_message_new_segment_done (GST_OBJECT (comp),
              priv->segment->format, epos));
      gst_pad_push_event (priv->ghostpad,
          gst_event_new_segment_done (priv->segment->format, epos));
    }
  }
  return FALSE;
}

static GstPadProbeReturn
ghost_event_probe_handler (GstPad * ghostpad G_GNUC_UNUSED,
    GstPadProbeInfo * info, GnlComposition * comp)
{
  GstPadProbeReturn retval = GST_PAD_PROBE_OK;
  GnlCompositionPrivate *priv = comp->priv;
  GstEvent *event = GST_PAD_PROBE_INFO_EVENT (info);

  GST_DEBUG_OBJECT (comp, "event: %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEGMENT:
    {
      COMP_FLUSHING_LOCK (comp);
      if (priv->pending_idle) {
        GST_DEBUG_OBJECT (comp, "removing pending seek for main thread");
        g_source_remove (priv->pending_idle);
      }
      priv->pending_idle = 0;
      priv->flushing = FALSE;
      COMP_FLUSHING_UNLOCK (comp);
    }
      break;
    case GST_EVENT_EOS:
    {
      COMP_FLUSHING_LOCK (comp);
      if (priv->flushing) {
        GST_DEBUG_OBJECT (comp, "flushing, bailing out");
        COMP_FLUSHING_UNLOCK (comp);
        retval = GST_PAD_PROBE_DROP;
        break;
      }
      COMP_FLUSHING_UNLOCK (comp);

      GST_DEBUG_OBJECT (comp, "Adding eos handling to main thread");
      if (priv->pending_idle) {
        GST_WARNING_OBJECT (comp,
            "There was already a pending eos in main thread !");
        g_source_remove (priv->pending_idle);
      }

      /* FIXME : This should be switched to using a g_thread_create() instead
       * of a g_idle_add(). EXTENSIVE TESTING AND ANALYSIS REQUIRED BEFORE
       * DOING THE SWITCH !!! */
      priv->pending_idle =
          g_idle_add ((GSourceFunc) eos_main_thread, (gpointer) comp);

      retval = GST_PAD_PROBE_DROP;
    }
      break;
    default:
      break;
  }

  return retval;
}



/* Warning : Don't take the objects lock in this method */
static void
gnl_composition_handle_message (GstBin * bin, GstMessage * message)
{
  GnlComposition *comp = (GnlComposition *) bin;
  gboolean dropit = FALSE;

  GST_DEBUG_OBJECT (comp, "message:%s from %s",
      gst_message_type_get_name (GST_MESSAGE_TYPE (message)),
      GST_MESSAGE_SRC (message) ? GST_ELEMENT_NAME (GST_MESSAGE_SRC (message)) :
      "UNKNOWN");

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:
    case GST_MESSAGE_WARNING:
    {
      /* FIXME / HACK
       * There is a massive issue with reverse negotiation and dynamic pipelines.
       *
       * Since we're not waiting for the pads of the previous stack to block before
       * re-switching, we might end up switching sources in the middle of a downstrea
       * negotiation which will do reverse negotiation... with the new source (which
       * is no longer the one that issues the request). That negotiation will fail
       * and the original source will emit an ERROR message.
       *
       * In order to avoid those issues, we just ignore error messages from elements
       * which aren't in the currently configured stack
       */
      if (GST_MESSAGE_SRC (message) && GNL_IS_OBJECT (GST_MESSAGE_SRC (message))
          && !OBJECT_IN_ACTIVE_SEGMENT (comp, GST_MESSAGE_SRC (message))) {
        GST_DEBUG_OBJECT (comp,
            "HACK Dropping error message from object not in currently configured stack !");
        dropit = TRUE;
      }
    }
    default:
      break;
  }

  if (dropit)
    gst_message_unref (message);
  else
    GST_BIN_CLASS (parent_class)->handle_message (bin, message);
}

static gint
priority_comp (GnlObject * a, GnlObject * b)
{
  if (a->priority < b->priority)
    return -1;

  if (a->priority > b->priority)
    return 1;

  return 0;
}

static inline gboolean
have_to_update_pipeline (GnlComposition * comp)
{
  GnlCompositionPrivate *priv = comp->priv;

  GST_DEBUG_OBJECT (comp,
      "segment[%" GST_TIME_FORMAT "--%" GST_TIME_FORMAT "] current[%"
      GST_TIME_FORMAT "--%" GST_TIME_FORMAT "]",
      GST_TIME_ARGS (priv->segment->start),
      GST_TIME_ARGS (priv->segment->stop),
      GST_TIME_ARGS (priv->segment_start), GST_TIME_ARGS (priv->segment_stop));

  if (priv->segment->start < priv->segment_start)
    return TRUE;

  if (priv->segment->start >= priv->segment_stop)
    return TRUE;

  return FALSE;
}

static void
gnl_composition_set_update (GnlComposition * comp, gboolean update)
{
  GnlCompositionPrivate *priv = comp->priv;

  if (G_UNLIKELY (update == priv->can_update))
    return;

  GST_DEBUG_OBJECT (comp, "update:%d [currently %d], update_required:%d",
      update, priv->can_update, priv->update_required);

  COMP_OBJECTS_LOCK (comp);
  priv->can_update = update;

  if (update && priv->update_required) {
    GstClockTime curpos;

    /* Get current position */
    if ((curpos = get_current_position (comp)) == GST_CLOCK_TIME_NONE) {
      if (GST_CLOCK_TIME_IS_VALID (priv->segment_start))
        curpos = priv->segment->start = priv->segment_start;
      else
        curpos = 0;
    }

    COMP_OBJECTS_UNLOCK (comp);

    /* update pipeline to that position */
    update_pipeline (comp, curpos, TRUE, TRUE, TRUE);
  } else
    COMP_OBJECTS_UNLOCK (comp);
}

/*
 * get_new_seek_event:
 *
 * Returns a seek event for the currently configured segment
 * and start/stop values
 *
 * The GstSegment and segment_start|stop must have been configured
 * before calling this function.
 */
static GstEvent *
get_new_seek_event (GnlComposition * comp, gboolean initial,
    gboolean updatestoponly)
{
  GstSeekFlags flags = GST_SEEK_FLAG_ACCURATE | GST_SEEK_FLAG_FLUSH;
  gint64 start, stop;
  GstSeekType starttype = GST_SEEK_TYPE_SET;
  GnlCompositionPrivate *priv = comp->priv;

  GST_DEBUG_OBJECT (comp, "initial:%d", initial);
  /* remove the seek flag */
  if (!initial)
    flags |= (GstSeekFlags) priv->segment->flags;

  GST_DEBUG_OBJECT (comp,
      "private->segment->start:%" GST_TIME_FORMAT " segment_start%"
      GST_TIME_FORMAT, GST_TIME_ARGS (priv->segment->start),
      GST_TIME_ARGS (priv->segment_start));

  GST_DEBUG_OBJECT (comp,
      "private->segment->stop:%" GST_TIME_FORMAT " segment_stop%"
      GST_TIME_FORMAT, GST_TIME_ARGS (priv->segment->stop),
      GST_TIME_ARGS (priv->segment_stop));

  start = MAX (priv->segment->start, priv->segment_start);
  stop = GST_CLOCK_TIME_IS_VALID (priv->segment->stop)
      ? MIN (priv->segment->stop, priv->segment_stop)
      : priv->segment_stop;

  if (updatestoponly) {
    starttype = GST_SEEK_TYPE_NONE;
    start = GST_CLOCK_TIME_NONE;
  }

  GST_DEBUG_OBJECT (comp,
      "Created new seek event. Flags:%d, start:%" GST_TIME_FORMAT ", stop:%"
      GST_TIME_FORMAT ", rate:%lf", flags, GST_TIME_ARGS (start),
      GST_TIME_ARGS (stop), priv->segment->rate);

  return gst_event_new_seek (priv->segment->rate,
      priv->segment->format, flags, starttype, start, GST_SEEK_TYPE_SET, stop);
}

/* OBJECTS LOCK must be taken when calling this ! */
static GstClockTime
get_current_position (GnlComposition * comp)
{
  GstPad *pad;
  GnlObject *obj;
  GnlCompositionPrivate *priv = comp->priv;
  gboolean res;
  gint64 value = GST_CLOCK_TIME_NONE;

  /* 1. Try querying position downstream */
  if (priv->ghostpad) {
    GstPad *peer = gst_pad_get_peer (priv->ghostpad);

    if (peer) {
      res = gst_pad_query_position (peer, GST_FORMAT_TIME, &value);
      gst_object_unref (peer);

      if (res) {
        GST_LOG_OBJECT (comp,
            "Successfully got downstream position %" GST_TIME_FORMAT,
            GST_TIME_ARGS ((guint64) value));
        goto beach;
      }
    }

    GST_DEBUG_OBJECT (comp, "Downstream position query failed");

    /* resetting format/value */
    value = GST_CLOCK_TIME_NONE;
  }

  /* 2. If downstream fails , try within the current stack */
  if (!priv->current) {
    GST_DEBUG_OBJECT (comp, "No current stack, can't send query");
    goto beach;
  }

  obj = (GnlObject *) priv->current->data;

  if (!(pad = get_src_pad ((GstElement *) obj)))
    goto beach;

  res = gst_pad_query_position (pad, GST_FORMAT_TIME, &value);

  if (G_UNLIKELY (res == FALSE)) {
    GST_WARNING_OBJECT (comp,
        "query failed or returned a format different from TIME");
    value = GST_CLOCK_TIME_NONE;
  } else {
    GST_LOG_OBJECT (comp, "Query returned %" GST_TIME_FORMAT,
        GST_TIME_ARGS ((guint64) value));
  }

beach:
  return (guint64) value;
}

/*
  Figures out if pipeline needs updating.
  Updates it and sends the seek event.
  Sends flush events downstream if needed.
  can be called by user_seek or segment_done

  initial : FIXME : ???? Always seems to be TRUE
  update : TRUE from EOS, FALSE from seek
*/

static gboolean
seek_handling (GnlComposition * comp, gboolean initial, gboolean update)
{
  GST_DEBUG_OBJECT (comp, "initial:%d, update:%d", initial, update);

  COMP_FLUSHING_LOCK (comp);
  GST_DEBUG_OBJECT (comp, "Setting flushing to TRUE");
  comp->priv->flushing = TRUE;
  COMP_FLUSHING_UNLOCK (comp);

  if (update || have_to_update_pipeline (comp)) {
    if (comp->priv->segment->rate >= 0.0)
      update_pipeline (comp, comp->priv->segment->start, initial, TRUE,
          !update);
    else
      update_pipeline (comp, comp->priv->segment->stop, initial, TRUE, !update);
  }

  return TRUE;
}

static void
handle_seek_event (GnlComposition * comp, GstEvent * event)
{
  gdouble rate;
  GstFormat format;
  GstSeekFlags flags;
  GstSeekType cur_type, stop_type;
  gint64 cur, stop;
  GnlCompositionPrivate *priv = comp->priv;

  gst_event_parse_seek (event, &rate, &format, &flags,
      &cur_type, &cur, &stop_type, &stop);

  GST_DEBUG_OBJECT (comp,
      "start:%" GST_TIME_FORMAT " -- stop:%" GST_TIME_FORMAT "  flags:%d",
      GST_TIME_ARGS (cur), GST_TIME_ARGS (stop), flags);

  gst_segment_do_seek (priv->segment,
      rate, format, flags, cur_type, cur, stop_type, stop, NULL);
  gst_segment_do_seek (priv->outside_segment,
      rate, format, flags, cur_type, cur, stop_type, stop, NULL);

  GST_DEBUG_OBJECT (comp, "Segment now has flags:%d", priv->segment->flags);

  /* crop the segment start/stop values */
  /* Only crop segment start value if we don't have a default object */
  if (priv->expandables == NULL)
    priv->segment->start = MAX (priv->segment->start, GNL_OBJECT_START (comp));
  priv->segment->stop = MIN (priv->segment->stop, GNL_OBJECT_STOP (comp));

  comp->priv->user_seek_flush = ! !(flags & GST_SEEK_FLAG_FLUSH);
  seek_handling (comp, TRUE, TRUE);
}

static gboolean
gnl_composition_event_handler (GstPad * ghostpad, GstObject * parent,
    GstEvent * event)
{
  GnlComposition *comp = (GnlComposition *) parent;
  GnlCompositionPrivate *priv = comp->priv;
  gboolean res = TRUE;

  GST_DEBUG_OBJECT (comp, "event type:%s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
    {
      GstEvent *nevent;

      handle_seek_event (comp, event);

      /* the incoming event might not be quite correct, we get a new proper
       * event to pass on to the childs. */
      COMP_OBJECTS_LOCK (comp);
      nevent = get_new_seek_event (comp, FALSE, FALSE);
      COMP_OBJECTS_UNLOCK (comp);
      gst_event_unref (event);
      event = nevent;
      break;
    }
    case GST_EVENT_QOS:
    {
      gdouble prop;
      GstQOSType qostype;
      GstClockTimeDiff diff;
      GstClockTime timestamp;

      gst_event_parse_qos (event, &qostype, &prop, &diff, &timestamp);

      GST_INFO_OBJECT (comp,
          "timestamp:%" GST_TIME_FORMAT " segment.start:%" GST_TIME_FORMAT
          " segment.stop:%" GST_TIME_FORMAT " segment_start%" GST_TIME_FORMAT
          " segment_stop:%" GST_TIME_FORMAT, GST_TIME_ARGS (timestamp),
          GST_TIME_ARGS (priv->outside_segment->start),
          GST_TIME_ARGS (priv->outside_segment->stop),
          GST_TIME_ARGS (priv->segment_start),
          GST_TIME_ARGS (priv->segment_stop));

      /* The problem with QoS events is the following:
       * At each new internal segment (i.e. when we re-arrange our internal
       * elements) we send flushing seeks to those elements (to properly
       * configure their playback range) but don't let the FLUSH events get
       * downstream.
       *
       * The problem is that the QoS running timestamps we receive from
       * downstream will not have taken into account those flush.
       *
       * What we need to do is to translate to our internal running timestamps
       * which for each configured segment starts at 0 for those elements.
       *
       * The generic algorithm for the incoming running timestamp translation
       * is therefore:
       *     (original_seek_time : original seek position received from usptream)
       *     (current_segment_start : Start position of the currently configured
       *                              timeline segment)
       *
       *     difference = original_seek_time - current_segment_start
       *     new_qos_position = upstream_qos_position - difference
       *
       * The new_qos_position is only valid when:
       *    * it applies to the current segment (difference > 0)
       *    * The QoS difference + timestamp is greater than the difference
       *
       */

      if (GST_CLOCK_TIME_IS_VALID (priv->outside_segment->start)) {
        GstClockTimeDiff curdiff;

        /* We'll either create a new event or discard it */
        gst_event_unref (event);

        if (priv->segment->rate < 0.0)
          curdiff = priv->outside_segment->stop - priv->segment_stop;
        else
          curdiff = priv->segment_start - priv->outside_segment->start;
        GST_DEBUG ("curdiff %" GST_TIME_FORMAT, GST_TIME_ARGS (curdiff));
        if ((curdiff != 0) && ((timestamp < curdiff)
                || (curdiff > timestamp + diff))) {
          GST_DEBUG_OBJECT (comp,
              "QoS event outside of current segment, discarding");
          /* The QoS timestamp is before the currently set-up pipeline */
          goto beach;
        }

        /* Substract the amount of running time we've already outputted
         * until the currently configured pipeline from the QoS timestamp.*/
        timestamp -= curdiff;
        GST_INFO_OBJECT (comp,
            "Creating new QoS event with timestamp %" GST_TIME_FORMAT,
            GST_TIME_ARGS (timestamp));
        event = gst_event_new_qos (qostype, prop, diff, timestamp);
      }
      break;
    }
    default:
      break;
  }

  if (res && priv->ghostpad) {
    COMP_OBJECTS_LOCK (comp);

    /* If the timeline isn't entirely reconstructed, we silently ignore the 
     * event. In the case of seeks the pipeline will already be correctly 
     * configured at this point*/
    if (priv->waitingpads == 0) {
      GST_DEBUG_OBJECT (comp, "About to call gnl_event_pad_func()");
      res = priv->gnl_event_pad_func (priv->ghostpad, parent, event);
      GST_DEBUG_OBJECT (comp, "Done calling gnl_event_pad_func() %d", res);
    } else
      gst_event_unref (event);

    COMP_OBJECTS_UNLOCK (comp);
  }

beach:
  return res;
}

static GstPadProbeReturn
pad_blocked (GstPad * pad, GstPadProbeInfo * info, GnlComposition * comp)
{
  GST_DEBUG_OBJECT (comp, "Pad : %s:%s", GST_DEBUG_PAD_NAME (pad));

  return GST_PAD_PROBE_OK;
}

static inline void
gnl_composition_remove_ghostpad (GnlComposition * comp)
{
  GnlCompositionPrivate *priv = comp->priv;

  GST_DEBUG_OBJECT (comp, "Removing ghostpad");

  gnl_object_remove_ghost_pad (GNL_OBJECT (comp), priv->ghostpad);
  priv->ghostpad = NULL;
  priv->ghosteventprobe = 0;
  priv->toplevelentry = NULL;
}

/* gnl_composition_ghost_pad_set_target:
 * target: The target #GstPad. The refcount will be decremented (given to the ghostpad).
 * entry: The GnlCompositionEntry to which the pad belongs
 */

static void
gnl_composition_ghost_pad_set_target (GnlComposition * comp, GstPad * target,
    GnlCompositionEntry * entry)
{
  GnlCompositionPrivate *priv = comp->priv;
  gboolean hadghost = priv->ghostpad ? TRUE : FALSE;

  if (target)
    GST_DEBUG_OBJECT (comp, "target:%s:%s , hadghost:%d",
        GST_DEBUG_PAD_NAME (target), hadghost);
  else
    GST_DEBUG_OBJECT (comp, "Removing target, hadghost:%d", hadghost);

  if (!hadghost) {
    /* Create new ghostpad */
    GstPad *ghostpad =
        gnl_object_ghost_pad_no_target ((GnlObject *) comp, "src", GST_PAD_SRC);

    if (!priv->gnl_event_pad_func) {
      GST_DEBUG_OBJECT (ghostpad, "About to replace event_pad_func");
      priv->gnl_event_pad_func = GST_PAD_EVENTFUNC (ghostpad);
    }

    gst_pad_set_event_function (ghostpad,
        GST_DEBUG_FUNCPTR (gnl_composition_event_handler));
    GST_DEBUG_OBJECT (ghostpad, "eventfunc is now %s",
        GST_DEBUG_FUNCPTR_NAME (GST_PAD_EVENTFUNC (ghostpad)));

    priv->ghostpad = ghostpad;
  } else {
    GstPad *ptarget = gst_ghost_pad_get_target (GST_GHOST_PAD (priv->ghostpad));

    if (ptarget && ptarget == target) {
      GST_DEBUG_OBJECT (comp,
          "Target of ghostpad is the same as existing one, not changing");
      gst_object_unref (ptarget);
      return;
    }

    /* Unset previous target */
    if (ptarget) {
      GST_DEBUG_OBJECT (comp, "Previous target was %s:%s",
          GST_DEBUG_PAD_NAME (ptarget));

      if (!priv->toplevelentry->probeid) {
        /* If it's not blocked, block it */
        priv->toplevelentry->probeid =
            gst_pad_add_probe (ptarget,
            GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM | GST_PAD_PROBE_TYPE_IDLE,
            (GstPadProbeCallback) pad_blocked, comp, NULL);
      }

      /* remove event probe */
      if (priv->ghosteventprobe) {
        gst_pad_remove_probe (ptarget, priv->ghosteventprobe);
        priv->ghosteventprobe = 0;
      }
      gst_object_unref (ptarget);

    }
  }

  /* Actually set the target */
  gnl_object_ghost_pad_set_target ((GnlObject *) comp, priv->ghostpad, target);

  /* Set top-level entry (will be NULL if unsetting) */
  priv->toplevelentry = entry;

  if (target && (priv->ghosteventprobe == 0)) {
    priv->ghosteventprobe =
        gst_pad_add_probe (target, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
        (GstPadProbeCallback) ghost_event_probe_handler, comp, NULL);
    GST_DEBUG_OBJECT (comp, "added event probe %lu", priv->ghosteventprobe);
  }

  if (!hadghost) {
    gst_pad_set_active (priv->ghostpad, TRUE);

    COMP_OBJECTS_UNLOCK (comp);
    if (!gst_element_add_pad (GST_ELEMENT (comp), priv->ghostpad))
      GST_WARNING ("Couldn't add the ghostpad");
    else
      gst_element_no_more_pads (GST_ELEMENT (comp));
    COMP_OBJECTS_LOCK (comp);
  }

  GST_DEBUG_OBJECT (comp, "END");
}

static void
refine_start_stop_in_region_above_priority (GnlComposition * composition,
    GstClockTime timestamp, GstClockTime start,
    GstClockTime stop,
    GstClockTime * rstart, GstClockTime * rstop, guint32 priority)
{
  GList *tmp;
  GnlObject *object;
  GstClockTime nstart = start, nstop = stop;

  GST_DEBUG_OBJECT (composition,
      "timestamp:%" GST_TIME_FORMAT " start: %" GST_TIME_FORMAT " stop: %"
      GST_TIME_FORMAT " priority:%u", GST_TIME_ARGS (timestamp),
      GST_TIME_ARGS (start), GST_TIME_ARGS (stop), priority);

  for (tmp = composition->priv->objects_start; tmp; tmp = tmp->next) {
    object = (GnlObject *) tmp->data;

    GST_LOG_OBJECT (object, "START %" GST_TIME_FORMAT "--%" GST_TIME_FORMAT,
        GST_TIME_ARGS (object->start), GST_TIME_ARGS (object->stop));

    if ((object->priority >= priority) || (!object->active))
      continue;

    if (object->start <= timestamp)
      continue;

    if (object->start >= nstop)
      continue;

    nstop = object->start;

    GST_DEBUG_OBJECT (composition,
        "START Found %s [prio:%u] at %" GST_TIME_FORMAT,
        GST_OBJECT_NAME (object), object->priority,
        GST_TIME_ARGS (object->start));

    break;
  }

  for (tmp = composition->priv->objects_stop; tmp; tmp = tmp->next) {
    object = (GnlObject *) tmp->data;

    GST_LOG_OBJECT (object, "STOP %" GST_TIME_FORMAT "--%" GST_TIME_FORMAT,
        GST_TIME_ARGS (object->start), GST_TIME_ARGS (object->stop));

    if ((object->priority >= priority) || (!object->active))
      continue;

    if (object->stop >= timestamp)
      continue;

    if (object->stop <= nstart)
      continue;

    nstart = object->stop;

    GST_DEBUG_OBJECT (composition,
        "STOP Found %s [prio:%u] at %" GST_TIME_FORMAT,
        GST_OBJECT_NAME (object), object->priority,
        GST_TIME_ARGS (object->start));

    break;
  }

  if (*rstart)
    *rstart = nstart;

  if (*rstop)
    *rstop = nstop;
}


/*
 * Converts a sorted list to a tree
 * Recursive
 *
 * stack will be set to the next item to use in the parent.
 * If operations number of sinks is limited, it will only use that number.
 */

static GNode *
convert_list_to_tree (GList ** stack, GstClockTime * start,
    GstClockTime * stop, guint32 * highprio)
{
  GNode *ret;
  guint nbsinks;
  gboolean limit;
  GList *tmp;
  GnlObject *object;

  if (!stack || !*stack)
    return NULL;

  object = (GnlObject *) (*stack)->data;

  GST_DEBUG ("object:%s , *start:%" GST_TIME_FORMAT ", *stop:%"
      GST_TIME_FORMAT " highprio:%d",
      GST_ELEMENT_NAME (object), GST_TIME_ARGS (*start),
      GST_TIME_ARGS (*stop), *highprio);

  /* update earliest stop */
  if (GST_CLOCK_TIME_IS_VALID (*stop)) {
    if (GST_CLOCK_TIME_IS_VALID (object->stop) && (*stop > object->stop))
      *stop = object->stop;
  } else {
    *stop = object->stop;
  }

  if (GST_CLOCK_TIME_IS_VALID (*start)) {
    if (GST_CLOCK_TIME_IS_VALID (object->start) && (*start < object->start))
      *start = object->start;
  } else {
    *start = object->start;
  }

  if (GNL_OBJECT_IS_SOURCE (object)) {
    *stack = g_list_next (*stack);

    /* update highest priority.
     * We do this here, since it's only used with sources (leafs of the tree) */
    if (object->priority > *highprio)
      *highprio = object->priority;

    ret = g_node_new (object);

    goto beach;
  } else {
    /* GnlOperation */
    GnlOperation *oper = (GnlOperation *) object;

    GST_LOG_OBJECT (oper, "operation, num_sinks:%d", oper->num_sinks);

    ret = g_node_new (object);
    limit = (oper->dynamicsinks == FALSE);
    nbsinks = oper->num_sinks;

    /* FIXME : if num_sinks == -1 : request the proper number of pads */
    for (tmp = g_list_next (*stack); tmp && (!limit || nbsinks);) {
      g_node_append (ret, convert_list_to_tree (&tmp, start, stop, highprio));
      if (limit)
        nbsinks--;
    }

    *stack = tmp;
  }

beach:
  GST_DEBUG_OBJECT (object,
      "*start:%" GST_TIME_FORMAT " *stop:%" GST_TIME_FORMAT
      " priority:%u", GST_TIME_ARGS (*start), GST_TIME_ARGS (*stop), *highprio);

  return ret;
}

/*
 * get_stack_list:
 * @comp: The #GnlComposition
 * @timestamp: The #GstClockTime to look at
 * @priority: The priority level to start looking from
 * @activeonly: Only look for active elements if TRUE
 * @start: The biggest start time of the objects in the stack
 * @stop: The smallest stop time of the objects in the stack
 * @highprio: The highest priority in the stack
 *
 * Not MT-safe, you should take the objects lock before calling it.
 * Returns: A tree of #GNode sorted in priority order, corresponding
 * to the given search arguments. The returned value can be #NULL.
 */

static GNode *
get_stack_list (GnlComposition * comp, GstClockTime timestamp,
    guint32 priority, gboolean activeonly, GstClockTime * start,
    GstClockTime * stop, guint * highprio)
{
  GList *tmp;
  GList *stack = NULL;
  GNode *ret = NULL;
  GstClockTime nstart = GST_CLOCK_TIME_NONE;
  GstClockTime nstop = GST_CLOCK_TIME_NONE;
  GstClockTime first_out_of_stack = GST_CLOCK_TIME_NONE;
  guint32 highest = 0;
  gboolean reverse = (comp->priv->segment->rate < 0.0);

  GST_DEBUG_OBJECT (comp,
      "timestamp:%" GST_TIME_FORMAT ", priority:%u, activeonly:%d",
      GST_TIME_ARGS (timestamp), priority, activeonly);

  GST_LOG ("objects_start:%p objects_stop:%p", comp->priv->objects_start,
      comp->priv->objects_stop);

  if (reverse) {
    for (tmp = comp->priv->objects_stop; tmp; tmp = g_list_next (tmp)) {
      GnlObject *object = (GnlObject *) tmp->data;

      GST_LOG_OBJECT (object,
          "start: %" GST_TIME_FORMAT ", stop:%" GST_TIME_FORMAT " , duration:%"
          GST_TIME_FORMAT ", priority:%u, active:%d",
          GST_TIME_ARGS (object->start), GST_TIME_ARGS (object->stop),
          GST_TIME_ARGS (object->duration), object->priority, object->active);

      if (object->stop >= timestamp) {
        if ((object->start < timestamp) &&
            (object->priority >= priority) &&
            ((!activeonly) || (object->active))) {
          GST_LOG_OBJECT (comp, "adding %s: sorted to the stack",
              GST_OBJECT_NAME (object));
          stack = g_list_insert_sorted (stack, object,
              (GCompareFunc) priority_comp);
        }
      } else {
        GST_LOG_OBJECT (comp, "too far, stopping iteration");
        first_out_of_stack = object->stop;
        break;
      }
    }
  } else {
    for (tmp = comp->priv->objects_start; tmp; tmp = g_list_next (tmp)) {
      GnlObject *object = (GnlObject *) tmp->data;

      GST_LOG_OBJECT (object,
          "start: %" GST_TIME_FORMAT " , stop:%" GST_TIME_FORMAT " , duration:%"
          GST_TIME_FORMAT ", priority:%u", GST_TIME_ARGS (object->start),
          GST_TIME_ARGS (object->stop), GST_TIME_ARGS (object->duration),
          object->priority);

      if (object->start <= timestamp) {
        if ((object->stop > timestamp) &&
            (object->priority >= priority) &&
            ((!activeonly) || (object->active))) {
          GST_LOG_OBJECT (comp, "adding %s: sorted to the stack",
              GST_OBJECT_NAME (object));
          stack = g_list_insert_sorted (stack, object,
              (GCompareFunc) priority_comp);
        }
      } else {
        GST_LOG_OBJECT (comp, "too far, stopping iteration");
        first_out_of_stack = object->start;
        break;
      }
    }
  }

  /* Insert the expandables */
  if (G_LIKELY (timestamp < GNL_OBJECT_STOP (comp)))
    for (tmp = comp->priv->expandables; tmp; tmp = tmp->next) {
      GST_DEBUG_OBJECT (comp, "Adding expandable %s sorted to the list",
          GST_OBJECT_NAME (tmp->data));
      stack = g_list_insert_sorted (stack, tmp->data,
          (GCompareFunc) priority_comp);
    }

  /* convert that list to a stack */
  tmp = stack;
  ret = convert_list_to_tree (&tmp, &nstart, &nstop, &highest);
  if (GST_CLOCK_TIME_IS_VALID (first_out_of_stack)) {
    if (reverse && nstart < first_out_of_stack)
      nstart = first_out_of_stack;
    else if (!reverse && nstop > first_out_of_stack)
      nstop = first_out_of_stack;
  }

  GST_DEBUG ("nstart:%" GST_TIME_FORMAT ", nstop:%" GST_TIME_FORMAT,
      GST_TIME_ARGS (nstart), GST_TIME_ARGS (nstop));

  if (*stop)
    *stop = nstop;
  if (*start)
    *start = nstart;
  if (highprio)
    *highprio = highest;

  g_list_free (stack);

  return ret;
}

/*
 * get_clean_toplevel_stack:
 * @comp: The #GnlComposition
 * @timestamp: The #GstClockTime to look at
 * @stop_time: Pointer to a #GstClockTime for min stop time of returned stack
 * @start_time: Pointer to a #GstClockTime for greatest start time of returned stack
 *
 * Returns: The new current stack for the given #GnlComposition and @timestamp.
 */

static GNode *
get_clean_toplevel_stack (GnlComposition * comp, GstClockTime * timestamp,
    GstClockTime * start_time, GstClockTime * stop_time)
{
  GNode *stack = NULL;
  GList *tmp;
  GstClockTime start = G_MAXUINT64;
  GstClockTime stop = G_MAXUINT64;
  guint highprio;
  gboolean reverse = (comp->priv->segment->rate < 0.0);

  GST_DEBUG_OBJECT (comp, "timestamp:%" GST_TIME_FORMAT,
      GST_TIME_ARGS (*timestamp));
  GST_DEBUG ("start:%" GST_TIME_FORMAT ", stop:%" GST_TIME_FORMAT,
      GST_TIME_ARGS (start), GST_TIME_ARGS (stop));

  stack = get_stack_list (comp, *timestamp, 0, TRUE, &start, &stop, &highprio);

  if (!stack) {
    GnlObject *object = NULL;

    /* Case for gaps, therefore no objects at specified *timestamp */
    GST_DEBUG_OBJECT (comp,
        "Got empty stack, checking if it really was after the last object");

    if (reverse) {
      /* Find the first active object just before *timestamp */
      for (tmp = comp->priv->objects_stop; tmp; tmp = g_list_next (tmp)) {
        object = (GnlObject *) tmp->data;

        if (object->stop < *timestamp && object->active)
          break;
      }
    } else {
      /* Find the first active object just after *timestamp */
      for (tmp = comp->priv->objects_start; tmp; tmp = g_list_next (tmp)) {
        object = (GnlObject *) tmp->data;

        if (object->start > *timestamp && object->active)
          break;
      }
    }

    if (tmp) {
      GST_DEBUG_OBJECT (comp,
          "Found a valid object %s %" GST_TIME_FORMAT " : %s [%"
          GST_TIME_FORMAT " - %" GST_TIME_FORMAT "]",
          (reverse ? "before" : "after"), GST_TIME_ARGS (*timestamp),
          GST_ELEMENT_NAME (object), GST_TIME_ARGS (object->start),
          GST_TIME_ARGS (object->stop));
      *timestamp = (reverse ? object->stop : object->start);
      stack =
          get_stack_list (comp, *timestamp, 0, TRUE, &start, &stop, &highprio);
    }
  }

  GST_DEBUG ("start:%" GST_TIME_FORMAT ", stop:%" GST_TIME_FORMAT,
      GST_TIME_ARGS (start), GST_TIME_ARGS (stop));

  if (stack) {
    guint32 top_priority = GNL_OBJECT_PRIORITY (stack->data);

    /* Figure out if there's anything blocking us with smaller priority */
    refine_start_stop_in_region_above_priority (comp, *timestamp, start,
        stop, &start, &stop, (highprio == 0) ? top_priority : highprio);
  }

  if (*stop_time) {
    if (stack)
      *stop_time = stop;
    else
      *stop_time = 0;
  }

  if (*start_time) {
    if (stack)
      *start_time = start;
    else
      *start_time = 0;
  }

  GST_DEBUG_OBJECT (comp,
      "Returning timestamp:%" GST_TIME_FORMAT " , start_time:%"
      GST_TIME_FORMAT " , stop_time:%" GST_TIME_FORMAT,
      GST_TIME_ARGS (*timestamp), GST_TIME_ARGS (*start_time),
      GST_TIME_ARGS (*stop_time));

  return stack;
}


/*
 *
 * UTILITY FUNCTIONS
 *
 */

/*
 * get_src_pad:
 * element: a #GstElement
 *
 * Returns: The src pad for the given element. A reference was added to the
 * returned pad, remove it when you don't need that pad anymore.
 * Returns NULL if there's no source pad.
 */
static GstPad *
get_src_pad (GstElement * element)
{
  GstIterator *it;
  GstIteratorResult itres;
  GValue item = { 0, };
  GstPad *srcpad;

  it = gst_element_iterate_src_pads (element);

  itres = gst_iterator_next (it, &item);
  if (itres != GST_ITERATOR_OK) {
    GST_DEBUG ("%s doesn't have a src pad !", GST_ELEMENT_NAME (element));
    srcpad = NULL;
  } else {
    srcpad = g_value_get_object (&item);
    gst_object_ref (srcpad);
    g_value_reset (&item);
  }

  gst_iterator_free (it);

  return srcpad;
}


/*
 *
 * END OF UTILITY FUNCTIONS
 *
 */

static gboolean
set_child_caps (GValue * item, GValue * ret G_GNUC_UNUSED, GnlObject * comp)
{
  GstElement *child = g_value_get_object (item);

  gnl_object_set_caps ((GnlObject *) child, comp->caps);

  return TRUE;
}


static GstStateChangeReturn
gnl_composition_change_state (GstElement * element, GstStateChange transition)
{
  GnlComposition *comp = (GnlComposition *) element;
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  GST_DEBUG_OBJECT (comp, "%s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    {
      GstIterator *childs;

      gnl_composition_reset (comp);

      /* state-lock all elements */
      GST_DEBUG_OBJECT (comp,
          "Setting all childs to READY and locking their state");

      childs = gst_bin_iterate_elements (GST_BIN (comp));

    retry_lock:
      if (G_UNLIKELY (gst_iterator_fold (childs,
                  (GstIteratorFoldFunction) lock_child_state, NULL,
                  NULL) == GST_ITERATOR_RESYNC)) {
        gst_iterator_resync (childs);
        goto retry_lock;
      }

      gst_iterator_free (childs);

      /* Set caps on all objects */
      if (G_UNLIKELY (!gst_caps_is_any (GNL_OBJECT (comp)->caps))) {
        childs = gst_bin_iterate_elements (GST_BIN (comp));

      retry_caps:
        if (G_UNLIKELY (gst_iterator_fold (childs,
                    (GstIteratorFoldFunction) set_child_caps, NULL,
                    comp) == GST_ITERATOR_RESYNC)) {
          gst_iterator_resync (childs);
          goto retry_caps;
        }
        gst_iterator_free (childs);
      }

      /* set ghostpad target */
      if (!(update_pipeline (comp, COMP_REAL_START (comp), TRUE, FALSE, TRUE))) {
        ret = GST_STATE_CHANGE_FAILURE;
        goto beach;
      }
    }
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
    case GST_STATE_CHANGE_READY_TO_NULL:
      gnl_composition_reset (comp);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
    case GST_STATE_CHANGE_READY_TO_NULL:
      unblock_childs (comp);
      break;
    default:
      break;
  }

beach:
  return ret;
}

static gint
objects_start_compare (GnlObject * a, GnlObject * b)
{
  if (a->start == b->start) {
    if (a->priority < b->priority)
      return -1;
    if (a->priority > b->priority)
      return 1;
    return 0;
  }
  if (a->start < b->start)
    return -1;
  if (a->start > b->start)
    return 1;
  return 0;
}

static gint
objects_stop_compare (GnlObject * a, GnlObject * b)
{
  if (a->stop == b->stop) {
    if (a->priority < b->priority)
      return -1;
    if (a->priority > b->priority)
      return 1;
    return 0;
  }
  if (b->stop < a->stop)
    return -1;
  if (b->stop > a->stop)
    return 1;
  return 0;
}

static void
update_start_stop_duration (GnlComposition * comp)
{
  GnlObject *obj;
  GnlObject *cobj = (GnlObject *) comp;
  GnlCompositionPrivate *priv = comp->priv;

  if (!priv->objects_start) {
    GST_LOG ("no objects, resetting everything to 0");

    if (cobj->start) {
      cobj->start = 0;
#if GLIB_CHECK_VERSION(2,26,0)
      g_object_notify_by_pspec (G_OBJECT (cobj),
          gnlobject_properties[GNLOBJECT_PROP_START]);
#else
      g_object_notify (G_OBJECT (cobj), "start");
#endif
    }

    if (cobj->duration) {
      cobj->duration = 0;
#if GLIB_CHECK_VERSION(2,26,0)
      g_object_notify_by_pspec (G_OBJECT (cobj),
          gnlobject_properties[GNLOBJECT_PROP_DURATION]);
#else
      g_object_notify (G_OBJECT (cobj), "duration");
#endif
      signal_duration_change (comp);
    }

    if (cobj->stop) {
      cobj->stop = 0;
#if GLIB_CHECK_VERSION(2,26,0)
      g_object_notify_by_pspec (G_OBJECT (cobj),
          gnlobject_properties[GNLOBJECT_PROP_STOP]);
#else
      g_object_notify (G_OBJECT (cobj), "stop");
#endif
    }

    return;
  }

  /* If we have a default object, the start position is 0 */
  if (priv->expandables) {
    GST_LOG_OBJECT (cobj,
        "Setting start to 0 because we have a default object");

    if (cobj->start != 0) {
      cobj->start = 0;
#if GLIB_CHECK_VERSION(2,26,0)
      g_object_notify_by_pspec (G_OBJECT (cobj),
          gnlobject_properties[GNLOBJECT_PROP_START]);
#else
      g_object_notify (G_OBJECT (cobj), "start");
#endif
    }

  } else {

    /* Else it's the first object's start value */
    obj = (GnlObject *) priv->objects_start->data;

    if (obj->start != cobj->start) {
      GST_LOG_OBJECT (obj, "setting start from %s to %" GST_TIME_FORMAT,
          GST_OBJECT_NAME (obj), GST_TIME_ARGS (obj->start));
      cobj->start = obj->start;
#if GLIB_CHECK_VERSION(2,26,0)
      g_object_notify_by_pspec (G_OBJECT (cobj),
          gnlobject_properties[GNLOBJECT_PROP_START]);
#else
      g_object_notify (G_OBJECT (cobj), "start");
#endif
    }

  }

  obj = (GnlObject *) priv->objects_stop->data;

  if (obj->stop != cobj->stop) {
    GST_LOG_OBJECT (obj, "setting stop from %s to %" GST_TIME_FORMAT,
        GST_OBJECT_NAME (obj), GST_TIME_ARGS (obj->stop));

    if (priv->expandables) {
      GList *tmp;

      for (tmp = priv->expandables; tmp; tmp = tmp->next) {
        g_object_set (tmp->data, "duration", obj->stop, NULL);
        g_object_set (tmp->data, "media-duration", obj->stop, NULL);
      }
    }

    priv->segment->stop = obj->stop;
    cobj->stop = obj->stop;
#if GLIB_CHECK_VERSION(2,26,0)
    g_object_notify_by_pspec (G_OBJECT (cobj),
        gnlobject_properties[GNLOBJECT_PROP_STOP]);
#else
    g_object_notify (G_OBJECT (cobj), "stop");
#endif
  }

  if ((cobj->stop - cobj->start) != cobj->duration) {
    cobj->duration = cobj->stop - cobj->start;
#if GLIB_CHECK_VERSION(2,26,0)
    g_object_notify_by_pspec (G_OBJECT (cobj),
        gnlobject_properties[GNLOBJECT_PROP_DURATION]);
#else
    g_object_notify (G_OBJECT (cobj), "duration");
#endif
    signal_duration_change (comp);
  }

  GST_LOG_OBJECT (comp,
      "start:%" GST_TIME_FORMAT
      " stop:%" GST_TIME_FORMAT
      " duration:%" GST_TIME_FORMAT,
      GST_TIME_ARGS (cobj->start),
      GST_TIME_ARGS (cobj->stop), GST_TIME_ARGS (cobj->duration));
}

static void
no_more_pads_object_cb (GstElement * element, GnlComposition * comp)
{
  GnlCompositionPrivate *priv = comp->priv;
  GnlObject *object = (GnlObject *) element;
  GNode *tmp;
  GstPad *pad = NULL;
  GnlCompositionEntry *entry;

  GST_LOG_OBJECT (comp, "no more pads on element %s",
      GST_ELEMENT_NAME (element));

  if (!(pad = get_src_pad (element)))
    goto no_source;

  COMP_OBJECTS_LOCK (comp);

  if (G_UNLIKELY (priv->current == NULL)) {
    GST_DEBUG_OBJECT (comp, "current stack is empty !");
    goto done;
  }

  tmp = g_node_find (priv->current, G_IN_ORDER, G_TRAVERSE_ALL, object);

  if (G_UNLIKELY (tmp == NULL))
    goto not_in_stack;

  entry = COMP_ENTRY (comp, object);
  wait_no_more_pads (comp, object, entry, FALSE);

  if (tmp->parent) {
    GstElement *parent = (GstElement *) tmp->parent->data;
    GstPad *sinkpad;

    /* Get an unlinked sinkpad from the parent */
    sinkpad = get_unlinked_sink_ghost_pad ((GnlOperation *) parent);

    if (G_UNLIKELY (sinkpad == NULL)) {
      GST_WARNING_OBJECT (comp,
          "Couldn't find an unlinked sinkpad from %s",
          GST_ELEMENT_NAME (parent));
      goto done;
    }

    /* Link pad to parent sink pad */
    if (G_UNLIKELY (gst_pad_link_full (pad, sinkpad,
                GST_PAD_LINK_CHECK_NOTHING) != GST_PAD_LINK_OK)) {
      GST_WARNING_OBJECT (comp, "Failed to link pads %s:%s - %s:%s",
          GST_DEBUG_PAD_NAME (pad), GST_DEBUG_PAD_NAME (sinkpad));
      gst_object_unref (sinkpad);
      goto done;
    }

    /* inform operation of incoming stream priority */
    gnl_operation_signal_input_priority_changed ((GnlOperation *) parent,
        sinkpad, object->priority);
    gst_object_unref (sinkpad);
    gst_pad_remove_probe (pad, entry->probeid);
    entry->probeid = 0;
  }

  /* If there are no more waiting pads, activate the current stack */
  if (priv->current && (priv->waitingpads == 0)
      && priv->stackvalid) {
    GnlCompositionEntry *topentry = COMP_ENTRY (comp, priv->current->data);
    GstPad *tpad = NULL;

    /* There are no more waiting pads for the currently configured timeline */
    /* stack. */
    tpad = get_src_pad (GST_ELEMENT (priv->current->data));
    GST_LOG_OBJECT (comp,
        "top-level pad %s:%s, Setting target of ghostpad to it",
        GST_DEBUG_PAD_NAME (tpad));

    /* 2. send pending seek */
    if (priv->childseek) {
      GstEvent *childseek = priv->childseek;

      priv->childseek = NULL;
      GST_INFO_OBJECT (comp, "Sending pending seek on %s:%s",
          GST_DEBUG_PAD_NAME (tpad));

      COMP_OBJECTS_UNLOCK (comp);

      if (!(gst_pad_send_event (tpad, childseek)))
        GST_ERROR_OBJECT (comp, "Sending seek event failed!");

      COMP_OBJECTS_LOCK (comp);
    }
    priv->childseek = NULL;

    /* 1. set target of ghostpad to toplevel element src pad */
    gnl_composition_ghost_pad_set_target (comp, tpad, topentry);

    /* Check again if the top-level element is still in the stack */
    if (priv->current &&
        g_node_find (priv->current, G_IN_ORDER, G_TRAVERSE_ALL, object)) {

      /* 3. unblock ghostpad */
      if (topentry->probeid) {
        if (comp->priv->user_seek_flush) {
          COMP_OBJECTS_UNLOCK (comp);
          gst_pad_push_event (comp->priv->ghostpad,
              gst_event_new_flush_start ());
          GST_PAD_STREAM_LOCK (comp->priv->ghostpad);
          GST_PAD_STREAM_UNLOCK (comp->priv->ghostpad);
          gst_pad_push_event (comp->priv->ghostpad,
              gst_event_new_flush_stop (TRUE));
          COMP_OBJECTS_LOCK (comp);
          comp->priv->user_seek_flush = FALSE;
        }

        GST_LOG_OBJECT (comp, "About to unblock top-level pad : %s:%s",
            GST_DEBUG_PAD_NAME (tpad));
        gst_pad_remove_probe (tpad, topentry->probeid);
        topentry->probeid = 0;
        GST_LOG_OBJECT (comp, "Unblocked top-level pad");
      }
    } else
      GST_DEBUG ("Element went away from currently configured stack");

    if (tpad)
      gst_object_unref (tpad);
  }

done:
  COMP_OBJECTS_UNLOCK (comp);

  if (pad)
    gst_object_unref (pad);

  GST_DEBUG_OBJECT (comp, "end");

  return;

no_source:
  {
    GST_LOG_OBJECT (comp, "no source pad");
    return;
  }

not_in_stack:
  {
    GST_LOG_OBJECT (comp,
        "The following object is not in currently configured stack : %s",
        GST_ELEMENT_NAME (object));
    goto done;
  }
}

/*
 * recursive depth-first relink stack function on new stack
 *
 * _ relink nodes with changed parent/order
 * _ links new nodes with parents
 * _ unblocks available source pads (except for toplevel)
 *
 * WITH OBJECTS LOCK TAKEN
 */

static void
compare_relink_single_node (GnlComposition * comp, GNode * node,
    GNode * oldstack)
{
  GNode *child;
  GNode *oldnode = NULL;
  GnlObject *newobj;
  GnlObject *newparent;
  GnlObject *oldparent = NULL;
  GstPad *srcpad = NULL, *sinkpad = NULL;
  GnlCompositionEntry *entry;

  if (G_UNLIKELY (!node))
    return;

  newparent = G_NODE_IS_ROOT (node) ? NULL : (GnlObject *) node->parent->data;
  newobj = (GnlObject *) node->data;
  if (oldstack) {
    oldnode = g_node_find (oldstack, G_IN_ORDER, G_TRAVERSE_ALL, newobj);
    if (oldnode)
      oldparent =
          G_NODE_IS_ROOT (oldnode) ? NULL : (GnlObject *) oldnode->parent->data;
  }

  GST_DEBUG_OBJECT (comp, "newobj:%s",
      GST_ELEMENT_NAME ((GstElement *) newobj));
  srcpad = get_src_pad ((GstElement *) newobj);

  /* 1. Make sure the source pad is blocked for new objects */
  if (G_UNLIKELY (!oldnode && srcpad)) {
    GnlCompositionEntry *oldentry = COMP_ENTRY (comp, newobj);
    if (!oldentry->probeid) {
      GST_LOG_OBJECT (comp, "block_async(%s:%s, TRUE)",
          GST_DEBUG_PAD_NAME (srcpad));
      oldentry->probeid =
          gst_pad_add_probe (srcpad,
          GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM | GST_PAD_PROBE_TYPE_IDLE,
          (GstPadProbeCallback) pad_blocked, comp, NULL);
    }
  }

  entry = COMP_ENTRY (comp, newobj);
  /* 2. link to parent if needed.
   *
   * If entry->nomorepadshandler is not zero, it means that srcpad didn't exist
   * before and so we connected to no-more-pads. This can happen since there's a
   * window of time between gnlsource adds its srcpad and then emits
   * no-more-pads. In that case, we just wait for no-more-pads to be emitted.
   */
  if (srcpad && entry->nomorepadshandler == 0) {
    GST_LOG_OBJECT (comp, "has a valid source pad");
    /* POST PROCESSING */
    if ((oldparent != newparent) ||
        (oldparent && newparent &&
            (g_node_child_index (node,
                    newobj) != g_node_child_index (oldnode, newobj)))) {
      GST_LOG_OBJECT (comp,
          "not same parent, or same parent but in different order");
      /* relink to new parent in required order */
      if (newparent) {
        GstPad *sinkpad;
        GST_LOG_OBJECT (comp, "Linking %s and %s",
            GST_ELEMENT_NAME (GST_ELEMENT (newobj)),
            GST_ELEMENT_NAME (GST_ELEMENT (newparent)));
        sinkpad = get_unlinked_sink_ghost_pad ((GnlOperation *) newparent);
        if (G_UNLIKELY (sinkpad == NULL)) {
          GST_WARNING_OBJECT (comp,
              "Couldn't find an unlinked sinkpad from %s",
              GST_ELEMENT_NAME (newparent));
        } else {
          if (G_UNLIKELY (gst_pad_link_full (srcpad, sinkpad,
                      GST_PAD_LINK_CHECK_NOTHING) != GST_PAD_LINK_OK)) {
            GST_WARNING_OBJECT (comp, "Failed to link pads %s:%s - %s:%s",
                GST_DEBUG_PAD_NAME (srcpad), GST_DEBUG_PAD_NAME (sinkpad));
          }
          gst_object_unref (sinkpad);
        }
      }
    } else
      GST_LOG_OBJECT (newobj, "Same parent and same position in the new stack");
    /* If there's an operation, inform it about priority changes */
    if (newparent) {
      sinkpad = gst_pad_get_peer (srcpad);
      gnl_operation_signal_input_priority_changed ((GnlOperation *)
          newparent, sinkpad, newobj->priority);
      gst_object_unref (sinkpad);
    }

  } else if (entry->nomorepadshandler) {
    GST_INFO_OBJECT (newobj,
        "we have a pad but we are connected to 'no-more-pads'");
  } else {
    wait_no_more_pads (comp, newobj, entry, TRUE);
  }

  /* 3. Handle childs */
  if (GNL_IS_OPERATION (newobj)) {
    guint nbchilds = g_node_n_children (node);
    GnlOperation *oper = (GnlOperation *) newobj;
    GST_LOG_OBJECT (newobj, "is a %s operation, analyzing the %d childs",
        oper->dynamicsinks ? "dynamic" : "regular", nbchilds);
    /* Update the operation's number of sinks, that will make it have the proper
     * number of sink pads to connect the childs to. */
    if (oper->dynamicsinks)
      g_object_set (G_OBJECT (newobj), "sinks", nbchilds, NULL);
    for (child = node->children; child; child = child->next)
      compare_relink_single_node (comp, child, oldstack);
    if (G_UNLIKELY (nbchilds < oper->num_sinks))
      GST_ERROR
          ("Not enough sinkpads to link all objects to the operation ! %d / %d",
          oper->num_sinks, nbchilds);
    if (G_UNLIKELY (nbchilds == 0))
      GST_ERROR ("Operation has no child objects to be connected to !!!");
    /* Make sure we have enough sinkpads */
  } else {
    /* FIXME : do we need to do something specific for sources ? */
  }

  /* 4. Unblock source pad */
  if ((srcpad && entry->nomorepadshandler == 0) && !G_NODE_IS_ROOT (node) &&
      entry->probeid) {
    GST_LOG_OBJECT (comp, "Unblocking pad %s:%s", GST_DEBUG_PAD_NAME (srcpad));
    gst_pad_remove_probe (srcpad, entry->probeid);
    entry->probeid = 0;
  }

  if (G_LIKELY (srcpad))
    gst_object_unref (srcpad);
  GST_LOG_OBJECT (comp, "done with object %s",
      GST_ELEMENT_NAME (GST_ELEMENT (newobj)));
}

/*
 * recursive depth-first compare stack function on old stack
 *
 * _ Add no-longer used objects to the deactivate list
 * _ unlink child-parent relations that have changed (not same parent, or not same order)
 * _ blocks available source pads
 *
 * FIXME : modify is only used for the root element.
 *    It is TRUE all the time except when the update is done from a seek
 *
 * WITH OBJECTS LOCK TAKEN
 */

static GList *
compare_deactivate_single_node (GnlComposition * comp, GNode * node,
    GNode * newstack, gboolean modify)
{
  GNode *child;
  GNode *newnode = NULL;        /* Same node in newstack */
  GnlObject *oldparent;
  GList *deactivate = NULL;
  GnlObject *oldobj = NULL;
  GstPad *srcpad = NULL;

  if (G_UNLIKELY (!node))
    return NULL;

  /* The former parent GnlObject (i.e. downstream) of the given node */
  oldparent = G_NODE_IS_ROOT (node) ? NULL : (GnlObject *) node->parent->data;

  /* The former GnlObject */
  oldobj = (GnlObject *) node->data;

  /* The node corresponding to oldobj in the new stack */
  if (newstack)
    newnode = g_node_find (newstack, G_IN_ORDER, G_TRAVERSE_ALL, oldobj);

  GST_DEBUG_OBJECT (comp, "oldobj:%s",
      GST_ELEMENT_NAME ((GstElement *) oldobj));
  srcpad = get_src_pad ((GstElement *) oldobj);

  if (G_LIKELY (srcpad)) {
    GstPad *peerpad = NULL;
    GnlCompositionEntry *entry = COMP_ENTRY (comp, oldobj);

    /* 1. Block source pad
     *   This makes sure that no data/event flow will come out of this element after this
     *   point.
     *
     * If entry is NULL, this means the element is in the process of being removed.
     */
    if (entry && !entry->probeid) {
      GST_LOG_OBJECT (comp, "Setting BLOCKING probe on %s:%s",
          GST_DEBUG_PAD_NAME (srcpad));
      entry->probeid =
          gst_pad_add_probe (srcpad,
          GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM | GST_PAD_PROBE_TYPE_IDLE,
          (GstPadProbeCallback) pad_blocked, comp, NULL);
    }

    /* 2. If we have to modify or we have a parent, flush downstream 
     *   This ensures the streaming thread going through the current object has
     *   either stopped or is blocking against the source pad. */
    if ((modify || oldparent) && (peerpad = gst_pad_get_peer (srcpad))) {
      GST_LOG_OBJECT (comp, "Sending flush start/stop downstream ");
      gst_pad_send_event (peerpad, gst_event_new_flush_start ());
      gst_pad_send_event (peerpad, gst_event_new_flush_stop (TRUE));
      GST_DEBUG_OBJECT (comp, "DONE Sending flush events downstream");
      gst_object_unref (peerpad);
    }

  } else {
    GST_LOG_OBJECT (comp, "No source pad available");
  }

  /* 3. Unlink from the parent if we've changed position */

  GST_LOG_OBJECT (comp,
      "Checking if we need to unlink from downstream element");
  if (G_UNLIKELY (!oldparent)) {
    GST_LOG_OBJECT (comp, "Top-level object");
    /* for top-level objects we just set the ghostpad target to NULL */
    if (comp->priv->ghostpad) {
      GST_LOG_OBJECT (comp, "Setting ghostpad target to NULL");
      gnl_composition_ghost_pad_set_target (comp, NULL, NULL);
    } else
      GST_LOG_OBJECT (comp, "No ghostpad");
  } else {
    GnlObject *newparent = NULL;

    GST_LOG_OBJECT (comp, "non-toplevel object");

    if (newnode)
      newparent =
          G_NODE_IS_ROOT (newnode) ? NULL : (GnlObject *) newnode->parent->data;

    if ((!newnode) || (oldparent != newparent) ||
        (newparent &&
            (g_node_child_index (node,
                    oldobj) != g_node_child_index (newnode, oldobj)))) {
      GstPad *peerpad = NULL;

      GST_LOG_OBJECT (comp, "Topology changed, unlinking from downstream");

      if (srcpad && (peerpad = gst_pad_get_peer (srcpad))) {
        GST_LOG_OBJECT (peerpad, "Sending flush start/stop");
        gst_pad_send_event (peerpad, gst_event_new_flush_start ());
        gst_pad_send_event (peerpad, gst_event_new_flush_stop (TRUE));
        gst_pad_unlink (srcpad, peerpad);
        gst_object_unref (peerpad);
      }
    } else
      GST_LOG_OBJECT (comp, "Topology unchanged");
  }

  /* 4. If we're dealing with an operation, call this method recursively on it */
  if (G_UNLIKELY (GNL_IS_OPERATION (oldobj))) {
    GST_LOG_OBJECT (comp,
        "Object is an operation, recursively calling on childs");
    for (child = node->children; child; child = child->next) {
      GList *newdeac =
          compare_deactivate_single_node (comp, child, newstack, modify);

      if (newdeac)
        deactivate = g_list_concat (deactivate, newdeac);
    }
  }

  /* 5. If object isn't used anymore, add it to the list of objects to deactivate */
  if (G_LIKELY (!newnode)) {
    GST_LOG_OBJECT (comp, "Object doesn't exist in new stack");
    deactivate = g_list_prepend (deactivate, oldobj);
  }

  if (G_LIKELY (srcpad))
    gst_object_unref (srcpad);
  GST_LOG_OBJECT (comp, "done with object %s",
      GST_ELEMENT_NAME (GST_ELEMENT (oldobj)));

  return deactivate;
}

/*
 * compare_relink_stack:
 * @comp: The #GnlComposition
 * @stack: The new stack
 * @modify: TRUE if the timeline has changed and needs downstream flushes.
 *
 * Compares the given stack to the current one and relinks it if needed.
 *
 * WITH OBJECTS LOCK TAKEN
 *
 * Returns: The #GList of #GnlObject no longer used
 */

static GList *
compare_relink_stack (GnlComposition * comp, GNode * stack, gboolean modify)
{
  GList *deactivate = NULL;

  /* 1. Traverse old stack to deactivate no longer used objects */
  deactivate =
      compare_deactivate_single_node (comp, comp->priv->current, stack, modify);

  /* 2. Traverse new stack to do needed (re)links */
  compare_relink_single_node (comp, stack, comp->priv->current);

  return deactivate;
}

static void
unlock_activate_stack (GnlComposition * comp, GNode * node,
    gboolean change_state, GstState state)
{
  GNode *child;

  GST_LOG_OBJECT (comp, "object:%s",
      GST_ELEMENT_NAME ((GstElement *) (node->data)));

  gst_element_set_locked_state ((GstElement *) (node->data), FALSE);

  if (change_state)
    gst_element_set_state (GST_ELEMENT (node->data), state);

  for (child = node->children; child; child = child->next)
    unlock_activate_stack (comp, child, change_state, state);
}

static gboolean
are_same_stacks (GNode * stack1, GNode * stack2)
{
  gboolean res = FALSE;

  /* TODO : FIXME : we should also compare start/media-start */
  /* stacks are not equal if one of them is NULL but not the other */
  if ((!stack1 && stack2) || (stack1 && !stack2))
    goto beach;

  if (stack1 && stack2) {
    GNode *child1, *child2;

    /* if they don't contain the same source, not equal */
    if (!(stack1->data == stack2->data))
      goto beach;

    /* if they don't have the same number of childs, not equal */
    if (!(g_node_n_children (stack1) == g_node_n_children (stack2)))
      goto beach;

    child1 = stack1->children;
    child2 = stack2->children;
    while (child1 && child2) {
      if (!(are_same_stacks (child1, child2)))
        goto beach;
      child1 = g_node_next_sibling (child1);
      child2 = g_node_next_sibling (child2);
    }

    /* if there's a difference in child number, stacks are not equal */
    if (child1 || child2)
      goto beach;
  }

  /* if stack1 AND stack2 are NULL, then they're equal (both empty) */
  res = TRUE;

beach:
  GST_LOG ("Stacks are equal : %d", res);

  return res;
}

/*
 * update_pipeline:
 * @comp: The #GnlComposition
 * @currenttime: The #GstClockTime to update at, can be GST_CLOCK_TIME_NONE.
 * @initial: TRUE if this is the first setup
 * @change_state: Change the state of the (de)activated objects if TRUE.
 * @modify: Flush downstream if TRUE. Needed for modified timelines.
 *
 * Updates the internal pipeline and properties. If @currenttime is 
 * GST_CLOCK_TIME_NONE, it will not modify the current pipeline
 *
 * Returns: FALSE if there was an error updating the pipeline.
 */

static gboolean
update_pipeline (GnlComposition * comp, GstClockTime currenttime,
    gboolean initial, gboolean change_state, gboolean modify)
{
  gboolean ret = TRUE;
  GnlCompositionPrivate *priv = comp->priv;

  GST_DEBUG_OBJECT (comp,
      "currenttime:%" GST_TIME_FORMAT
      " initial:%d , change_state:%d , modify:%d", GST_TIME_ARGS (currenttime),
      initial, change_state, modify);

  COMP_OBJECTS_LOCK (comp);

  if (G_UNLIKELY (!priv->can_update)) {
    COMP_OBJECTS_UNLOCK (comp);
    return TRUE;
  }

  update_start_stop_duration (comp);

  if (GST_CLOCK_TIME_IS_VALID (currenttime)) {
    GstState state = GST_STATE (comp);
    GstState nextstate =
        (GST_STATE_NEXT (comp) ==
        GST_STATE_VOID_PENDING) ? GST_STATE (comp) : GST_STATE_NEXT (comp);
    GNode *stack = NULL;
    GList *todeactivate = NULL;
    GstClockTime new_start = GST_CLOCK_TIME_NONE;
    GstClockTime new_stop = GST_CLOCK_TIME_NONE;
    gboolean samestack = FALSE;
    gboolean startchanged, stopchanged;

    GST_DEBUG_OBJECT (comp,
        "now really updating the pipeline, current-state:%s",
        gst_element_state_get_name (state));

    /* 1. Get new stack and compare it to current one */
    stack =
        get_clean_toplevel_stack (comp, &currenttime, &new_start, &new_stop);
    samestack = are_same_stacks (priv->current, stack);

    /* 2. If stacks are different, unlink/relink objects */
    if (!samestack)
      todeactivate = compare_relink_stack (comp, stack, modify);

    if (priv->segment->rate >= 0.0) {
      startchanged = priv->segment_start != currenttime;
      stopchanged = priv->segment_stop != new_stop;
    } else {
      startchanged = priv->segment_start != new_start;
      stopchanged = priv->segment_stop != currenttime;
    }

    /* 3. set new segment_start/stop (the current zone over which the new stack
     *    is valid) */
    if (priv->segment->rate >= 0.0) {
      priv->segment_start = currenttime;
      priv->segment_stop = new_stop;
    } else {
      priv->segment_start = new_start;
      priv->segment_stop = currenttime;
    }

    /* 4. Clear pending child seek
     *    We'll be creating a new one */
    if (priv->childseek) {
      GST_DEBUG ("unreffing event %p", priv->childseek);
      gst_event_unref (priv->childseek);
      priv->childseek = NULL;
    }

    /* Invalidate current stack */
    if (priv->current)
      g_node_destroy (priv->current);
    priv->current = NULL;

    /* invalidate the stack while modifying it */
    priv->stackvalid = FALSE;

    COMP_OBJECTS_UNLOCK (comp);

    /* 5. deactivate unused elements */
    if (todeactivate) {
      GList *tmp;
      GnlCompositionEntry *entry;
      GstElement *element;

      GST_DEBUG_OBJECT (comp, "De-activating objects no longer used");

      /* state-lock elements no more used */
      for (tmp = todeactivate; tmp; tmp = tmp->next) {
        element = GST_ELEMENT_CAST (tmp->data);

        if (change_state)
          gst_element_set_state (element, state);
        gst_element_set_locked_state (element, TRUE);
        entry = COMP_ENTRY (comp, element);

        /* entry can be NULL here if update_pipeline was called by
         * gnl_composition_remove_object (comp, tmp->data)
         */
        if (entry && entry->nomorepadshandler)
          wait_no_more_pads (comp, element, entry, FALSE);
      }

      g_list_free (todeactivate);

      GST_DEBUG_OBJECT (comp, "Finished de-activating objects no longer used");
    }

    /* 6. Unlock all elements in new stack */
    GST_DEBUG_OBJECT (comp, "Setting current stack");
    priv->current = stack;

    if (!samestack && stack) {
      GST_DEBUG_OBJECT (comp, "activating objects in new stack to %s",
          gst_element_state_get_name (nextstate));
      unlock_activate_stack (comp, stack, change_state, nextstate);
      GST_DEBUG_OBJECT (comp, "Finished activating objects in new stack");
    }

    /* 7. Activate stack (might happen asynchronously) */
    if (priv->current) {
      GstEvent *event;

      COMP_OBJECTS_LOCK (comp);

      priv->stackvalid = TRUE;

      /* 7.1. Create new seek event for newly configured timeline stack */
      if (samestack && (startchanged || stopchanged))
        event =
            get_new_seek_event (comp,
            (state == GST_STATE_PLAYING) ? FALSE : TRUE, !startchanged);
      else
        event = get_new_seek_event (comp, initial, FALSE);

      /* 7.2.a If the stack entirely ready, send seek out synchronously */
      if (priv->waitingpads == 0) {
        GstPad *pad;
        GstElement *topelement = GST_ELEMENT (priv->current->data);

        /* Get toplevel object source pad */
        if ((pad = get_src_pad (topelement))) {
          GnlCompositionEntry *topentry = COMP_ENTRY (comp, topelement);

          GST_DEBUG_OBJECT (comp,
              "We have a valid toplevel element pad %s:%s",
              GST_DEBUG_PAD_NAME (pad));

          COMP_OBJECTS_UNLOCK (comp);
          /* Send seek event */
          GST_LOG_OBJECT (comp, "sending seek event");
          if (gst_pad_send_event (pad, event)) {
            /* Unconditionnaly set the ghostpad target to pad */
            GST_LOG_OBJECT (comp,
                "Setting the composition's ghostpad target to %s:%s",
                GST_DEBUG_PAD_NAME (pad));

            COMP_OBJECTS_LOCK (comp);
            gnl_composition_ghost_pad_set_target (comp, pad, topentry);

            if (topentry->probeid) {
              if (comp->priv->user_seek_flush) {
                COMP_OBJECTS_UNLOCK (comp);
                gst_pad_push_event (comp->priv->ghostpad,
                    gst_event_new_flush_start ());
                GST_PAD_STREAM_LOCK (comp->priv->ghostpad);
                GST_PAD_STREAM_UNLOCK (comp->priv->ghostpad);
                gst_pad_push_event (comp->priv->ghostpad,
                    gst_event_new_flush_stop (TRUE));
                COMP_OBJECTS_LOCK (comp);
                comp->priv->user_seek_flush = FALSE;
              }

              /* unblock top-level pad */
              GST_LOG_OBJECT (comp, "About to unblock top-level srcpad");
              gst_pad_remove_probe (pad, topentry->probeid);
              topentry->probeid = 0;
            }
          } else {
            COMP_OBJECTS_LOCK (comp);
            ret = FALSE;
          }
          gst_object_unref (pad);

        } else {
          GST_WARNING_OBJECT (comp,
              "Timeline is entirely linked, but couldn't get top-level element's source pad");

          ret = FALSE;
        }
      } else {
        /* 7.2.b. Stack isn't entirely ready, save seek event for later on */
        GST_LOG_OBJECT (comp,
            "The timeline stack isn't entirely linked, delaying sending seek event (waitingpads:%d)",
            priv->waitingpads);

        priv->childseek = event;
        ret = TRUE;
      }
      COMP_OBJECTS_UNLOCK (comp);
    } else {
      if ((!priv->objects_start) && priv->ghostpad) {
        GST_DEBUG_OBJECT (comp, "composition is now empty, removing ghostpad");
        gnl_composition_remove_ghostpad (comp);
        priv->segment_start = 0;
        priv->segment_stop = GST_CLOCK_TIME_NONE;
      }
    }
  } else {
    COMP_OBJECTS_UNLOCK (comp);
  }

  GST_DEBUG_OBJECT (comp, "Returning %d", ret);
  return ret;
}

/* 
 * Child modification updates
 */

static void
object_start_stop_priority_changed (GnlObject * object,
    GParamSpec * arg G_GNUC_UNUSED, GnlComposition * comp)
{
  GnlCompositionPrivate *priv = comp->priv;

  GST_DEBUG_OBJECT (object,
      "start/stop/priority  changed (%" GST_TIME_FORMAT "/%" GST_TIME_FORMAT
      "/%d), evaluating pipeline update", GST_TIME_ARGS (object->start),
      GST_TIME_ARGS (object->stop), object->priority);

  /* The topology of the ocmposition might have changed, update the lists */
  priv->objects_start = g_list_sort
      (priv->objects_start, (GCompareFunc) objects_start_compare);
  priv->objects_stop = g_list_sort
      (priv->objects_stop, (GCompareFunc) objects_stop_compare);

  if (!priv->can_update) {
    priv->update_required = TRUE;
    update_start_stop_duration (comp);
    return;
  }

  /* Update pipeline if needed */
  if (priv->current &&
      (OBJECT_IN_ACTIVE_SEGMENT (comp, object) ||
          g_node_find (priv->current, G_IN_ORDER, G_TRAVERSE_ALL, object))) {
    GstClockTime curpos = get_current_position (comp);

    if (curpos == GST_CLOCK_TIME_NONE)
      curpos = priv->segment->start = priv->segment_start;

    update_pipeline (comp, curpos, TRUE, TRUE, TRUE);
  } else
    update_start_stop_duration (comp);
}

static void
object_active_changed (GnlObject * object, GParamSpec * arg G_GNUC_UNUSED,
    GnlComposition * comp)
{
  GnlCompositionPrivate *priv = comp->priv;

  GST_DEBUG_OBJECT (object,
      "active flag changed (%d), evaluating pipeline update", object->active);

  if (!priv->can_update) {
    priv->update_required = TRUE;
    return;
  }

  if (priv->current && OBJECT_IN_ACTIVE_SEGMENT (comp, object)) {
    GstClockTime curpos = get_current_position (comp);

    if (curpos == GST_CLOCK_TIME_NONE)
      curpos = priv->segment->start = priv->segment_start;
    update_pipeline (comp, curpos, TRUE, TRUE, TRUE);
  } else
    update_start_stop_duration (comp);
}

static void
object_pad_removed (GnlObject * object, GstPad * pad, GnlComposition * comp)
{
  GnlCompositionPrivate *priv = comp->priv;

  GST_DEBUG_OBJECT (comp, "pad %s:%s was removed", GST_DEBUG_PAD_NAME (pad));

  if (GST_PAD_IS_SRC (pad)) {
    /* remove ghostpad if it's the current top stack object */
    if (priv->current && GNL_OBJECT (priv->current->data) == object
        && priv->ghostpad)
      gnl_composition_remove_ghostpad (comp);
    else {
      GnlCompositionEntry *entry = COMP_ENTRY (comp, object);

      if (entry->probeid) {
        gst_pad_remove_probe (pad, entry->probeid);
        entry->probeid = 0;
      }
    }
  }
}

static void
object_pad_added (GnlObject * object G_GNUC_UNUSED, GstPad * pad,
    GnlComposition * comp)
{
  GnlCompositionEntry *entry;

  if (GST_PAD_DIRECTION (pad) == GST_PAD_SINK)
    return;

  entry = COMP_ENTRY (comp, object);

  if (!entry->probeid) {
    GST_DEBUG_OBJECT (comp, "pad %s:%s was added, blocking it",
        GST_DEBUG_PAD_NAME (pad));
    entry->probeid =
        gst_pad_add_probe (pad,
        GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM | GST_PAD_PROBE_TYPE_IDLE,
        (GstPadProbeCallback) pad_blocked, comp, NULL);
  }
}

static gboolean
gnl_composition_add_object (GstBin * bin, GstElement * element)
{
  GnlComposition *comp = (GnlComposition *) bin;
  GnlCompositionEntry *entry;
  GstClockTime curpos = GST_CLOCK_TIME_NONE;
  GnlCompositionPrivate *priv = comp->priv;
  gboolean ret;
  gboolean update_required;

  /* we only accept GnlObject */
  g_return_val_if_fail (GNL_IS_OBJECT (element), FALSE);

  GST_DEBUG_OBJECT (bin, "element %s", GST_OBJECT_NAME (element));
  GST_DEBUG_OBJECT (element, "%" GST_TIME_FORMAT "--%" GST_TIME_FORMAT,
      GST_TIME_ARGS (GNL_OBJECT_START (element)),
      GST_TIME_ARGS (GNL_OBJECT_STOP (element)));

  gst_object_ref (element);

  COMP_OBJECTS_LOCK (comp);

  if (((GNL_OBJECT_PRIORITY (element) == G_MAXUINT32) ||
          GNL_OBJECT_IS_EXPANDABLE (element)) &&
      g_list_find (priv->expandables, element)) {
    GST_WARNING_OBJECT (comp,
        "We already have an expandable, remove it before adding new one");
    ret = FALSE;

    goto chiringuito;
  }

  /* Call parent class ::add_element() */
  ret = GST_BIN_CLASS (parent_class)->add_element (bin, element);

  if (!ret) {
    GST_WARNING_OBJECT (bin, "couldn't add element");
    goto chiringuito;
  }

  /* lock state of child ! */
  GST_LOG_OBJECT (bin, "Locking state of %s", GST_ELEMENT_NAME (element));
  gst_element_set_locked_state (element, TRUE);

  /* wrap new element in a GnlCompositionEntry ... */
  entry = g_slice_new0 (GnlCompositionEntry);
  entry->object = (GnlObject *) element;

  if (G_LIKELY ((GNL_OBJECT_PRIORITY (element) != G_MAXUINT32) &&
          !GNL_OBJECT_IS_EXPANDABLE (element))) {
    /* Only react on non-default objects properties */
    entry->starthandler = g_signal_connect (G_OBJECT (element),
        "notify::start", G_CALLBACK (object_start_stop_priority_changed), comp);
    entry->stophandler =
        g_signal_connect (G_OBJECT (element), "notify::stop",
        G_CALLBACK (object_start_stop_priority_changed), comp);
    entry->priorityhandler =
        g_signal_connect (G_OBJECT (element), "notify::priority",
        G_CALLBACK (object_start_stop_priority_changed), comp);
  } else {
    /* We set the default source start/stop values to 0 and composition-stop */
    g_object_set (element,
        "start", (GstClockTime) 0,
        "media-start", (GstClockTime) 0,
        "duration", (GstClockTimeDiff) GNL_OBJECT_STOP (comp),
        "media-duration", (GstClockTimeDiff) GNL_OBJECT_STOP (comp), NULL);
  }

  entry->activehandler = g_signal_connect (G_OBJECT (element),
      "notify::active", G_CALLBACK (object_active_changed), comp);
  entry->padremovedhandler = g_signal_connect (G_OBJECT (element),
      "pad-removed", G_CALLBACK (object_pad_removed), comp);
  entry->padaddedhandler = g_signal_connect (G_OBJECT (element),
      "pad-added", G_CALLBACK (object_pad_added), comp);

  /* ...and add it to the hash table */
  g_hash_table_insert (priv->objects_hash, element, entry);

  /* Set the caps of the composition */
  if (G_UNLIKELY (!gst_caps_is_any (((GnlObject *) comp)->caps)))
    gnl_object_set_caps ((GnlObject *) element, ((GnlObject *) comp)->caps);

  /* Special case for default source. */
  if ((GNL_OBJECT_PRIORITY (element) == G_MAXUINT32) ||
      GNL_OBJECT_IS_EXPANDABLE (element)) {
    /* It doesn't get added to objects_start and objects_stop. */
    priv->expandables = g_list_prepend (priv->expandables, element);
    goto check_update;
  }

  /* add it sorted to the objects list */
  priv->objects_start = g_list_insert_sorted
      (priv->objects_start, element, (GCompareFunc) objects_start_compare);

  if (priv->objects_start)
    GST_LOG_OBJECT (comp,
        "Head of objects_start is now %s [%" GST_TIME_FORMAT "--%"
        GST_TIME_FORMAT "]",
        GST_OBJECT_NAME (priv->objects_start->data),
        GST_TIME_ARGS (GNL_OBJECT_START (priv->objects_start->data)),
        GST_TIME_ARGS (GNL_OBJECT_STOP (priv->objects_start->data)));

  priv->objects_stop = g_list_insert_sorted
      (priv->objects_stop, element, (GCompareFunc) objects_stop_compare);

  if (priv->objects_stop)
    GST_LOG_OBJECT (comp,
        "Head of objects_stop is now %s [%" GST_TIME_FORMAT "--%"
        GST_TIME_FORMAT "]", GST_OBJECT_NAME (priv->objects_stop->data),
        GST_TIME_ARGS (GNL_OBJECT_START (priv->objects_stop->data)),
        GST_TIME_ARGS (GNL_OBJECT_STOP (priv->objects_stop->data)));

  GST_DEBUG_OBJECT (comp,
      "segment_start:%" GST_TIME_FORMAT " segment_stop:%" GST_TIME_FORMAT,
      GST_TIME_ARGS (priv->segment_start), GST_TIME_ARGS (priv->segment_stop));

check_update:
  update_required = OBJECT_IN_ACTIVE_SEGMENT (comp, element) ||
      (!priv->current) ||
      (GNL_OBJECT_PRIORITY (element) == G_MAXUINT32) ||
      GNL_OBJECT_IS_EXPANDABLE (element);

  /* We only need the current position if we're going to update */
  if (update_required && priv->can_update)
    if ((curpos = get_current_position (comp)) == GST_CLOCK_TIME_NONE)
      curpos = priv->segment_start;

  COMP_OBJECTS_UNLOCK (comp);

  /* If we added within currently configured segment OR the pipeline was *
   * previously empty, THEN update pipeline */
  if (G_LIKELY (update_required && priv->can_update))
    update_pipeline (comp, curpos, TRUE, TRUE, TRUE);
  else {
    if (!priv->can_update)
      priv->update_required |= update_required;
    update_start_stop_duration (comp);
  }

beach:
  gst_object_unref (element);
  return ret;

chiringuito:
  {
    COMP_OBJECTS_UNLOCK (comp);
    update_start_stop_duration (comp);
    goto beach;
  }
}


static gboolean
gnl_composition_remove_object (GstBin * bin, GstElement * element)
{
  GnlComposition *comp = (GnlComposition *) bin;
  GnlCompositionPrivate *priv = comp->priv;
  GstClockTime curpos = GST_CLOCK_TIME_NONE;
  gboolean ret = FALSE;
  gboolean update_required;
  GnlCompositionEntry *entry;
  GstPad *srcpad;
  gulong probeid;

  GST_DEBUG_OBJECT (bin, "element %s", GST_OBJECT_NAME (element));

  /* we only accept GnlObject */
  g_return_val_if_fail (GNL_IS_OBJECT (element), FALSE);
  COMP_OBJECTS_LOCK (comp);
  entry = COMP_ENTRY (comp, element);
  if (entry == NULL) {
    COMP_OBJECTS_UNLOCK (comp);
    goto out;
  }

  if (entry->nomorepadshandler)
    wait_no_more_pads (comp, element, entry, FALSE);
  gst_object_ref (element);
  gst_element_set_locked_state (element, FALSE);

  /* handle default source */
  if ((GNL_OBJECT_PRIORITY (element) == G_MAXUINT32) ||
      GNL_OBJECT_IS_EXPANDABLE (element)) {
    /* Find it in the list */
    priv->expandables = g_list_remove (priv->expandables, element);
  } else {
    /* remove it from the objects list and resort the lists */
    priv->objects_start = g_list_remove (priv->objects_start, element);
    priv->objects_stop = g_list_remove (priv->objects_stop, element);
    GST_LOG_OBJECT (element, "Removed from the objects start/stop list");
  }

  probeid = entry->probeid;

  g_hash_table_remove (priv->objects_hash, element);
  update_required = OBJECT_IN_ACTIVE_SEGMENT (comp, element) ||
      (GNL_OBJECT_PRIORITY (element) == G_MAXUINT32) ||
      GNL_OBJECT_IS_EXPANDABLE (element);
  if (update_required && priv->can_update) {
    curpos = get_current_position (comp);
    if (G_UNLIKELY (curpos == GST_CLOCK_TIME_NONE))
      curpos = priv->segment_start;
  }

  COMP_OBJECTS_UNLOCK (comp);
  /* If we removed within currently configured segment, or it was the default source, *
   * update pipeline */
  if (G_LIKELY (update_required))
    update_pipeline (comp, curpos, TRUE, TRUE, TRUE);
  else {
    if (!priv->can_update)
      priv->update_required |= update_required;
    update_start_stop_duration (comp);
  }

  ret = GST_BIN_CLASS (parent_class)->remove_element (bin, element);
  GST_LOG_OBJECT (element, "Done removing from the composition");

  /* unblock source pad */
  if (probeid && (srcpad = get_src_pad (element))) {
    gst_pad_remove_probe (srcpad, probeid);
    gst_object_unref (srcpad);
  }

  gst_object_unref (element);
out:
  return ret;
}
