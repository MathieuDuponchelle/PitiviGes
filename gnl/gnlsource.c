/* Gnonlin
 * Copyright (C) <2001> Wim Taymans <wim.taymans@chello.be>
 *               <2004> Edward Hervey <bilboed@bilboed.com>
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

#include <string.h>
#include "config.h"
#include "gnlsource.h"
#include "gnlmarshal.h"

GstElementDetails gnl_source_details = GST_ELEMENT_DETAILS
(
  "GNL Source",
  "Filter/Editor",
  "Manages source elements",
  "Wim Taymans <wim.taymans@chello.be>"
);

struct _GnlSourcePrivate {
  gboolean	dispose_has_run;
  gint64	seek_start;	/* Beginning of seek in media_time */
  gint64	seek_stop;	/* End of seek in media_time */
  gboolean	queued;
};

enum {
  ARG_0,
  ARG_ELEMENT,
};

enum
{
  GET_PAD_FOR_STREAM_ACTION,
  LAST_SIGNAL
};

static void		gnl_source_base_init		(gpointer g_class);
static void 		gnl_source_class_init 		(GnlSourceClass *klass);
static void 		gnl_source_init 		(GnlSource *source);
static void 		gnl_source_dispose 		(GObject *object);
static void 		gnl_source_finalize 		(GObject *object);

static void		gnl_source_set_property 	(GObject *object, guint prop_id,
							 const GValue *value, GParamSpec *pspec);
static void		gnl_source_get_property 	(GObject *object, guint prop_id, GValue *value,
		                                         GParamSpec *pspec);

static GstPad*	 	gnl_source_request_new_pad 	(GstElement *element, GstPadTemplate *templ, 
							 const gchar *unused);

static gboolean 	gnl_source_prepare 		(GnlObject *object, GstEvent *event);

static GstElementStateReturn
			gnl_source_change_state 	(GstElement *element);


static GstData* 	source_getfunction 		(GstPad *pad);
static void 		source_chainfunction 		(GstPad *pad, GstData *buffer);

typedef struct 
{
  GnlSource *source;
  const gchar *padname;
  GstPad *target;
} LinkData;

static void		source_element_new_pad	 	(GstElement *element, 
							 GstPad *pad, 
							 LinkData *data);

static GnlObjectClass *parent_class = NULL;
static guint gnl_source_signals[LAST_SIGNAL] = { 0 };

enum {
  TYPE_NONE = 0,
  TYPE_AUDIO,
  TYPE_VIDEO,
  TYPE_OTHER
};

typedef struct {
  GSList *queue;
  GstPad *srcpad,
         *sinkpad;
  gboolean active;
  GstProbe	*probe;
  gint		type;
  gint	audiowidth;
  gint	nbchanns;
  gint	rate;
} SourcePadPrivate;

#define CLASS(source)  GNL_SOURCE_CLASS (G_OBJECT_GET_CLASS (source))

GType
gnl_source_get_type (void)
{
  static GType source_type = 0;

  if (!source_type) {
    static const GTypeInfo source_info = {
      sizeof (GnlSourceClass),
      (GBaseInitFunc) gnl_source_base_init,
      NULL,
      (GClassInitFunc) gnl_source_class_init,
      NULL,
      NULL,
      sizeof (GnlSource),
      32,
      (GInstanceInitFunc) gnl_source_init,
    };
    source_type = g_type_register_static (GNL_TYPE_OBJECT, "GnlSource", &source_info, 0);
  }
  return source_type;
}

static void
gnl_source_base_init (gpointer g_class)
{
  GstElementClass *gstclass = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details (gstclass, &gnl_object_details);
  
}


static void
gnl_source_class_init (GnlSourceClass *klass)
{
  GObjectClass 		*gobject_class;
  GstElementClass 	*gstelement_class;
  GnlObjectClass 	*gnlobject_class;

  gobject_class = 	(GObjectClass*)klass;
  gstelement_class = 	(GstElementClass*)klass;
  gnlobject_class = 	(GnlObjectClass*)klass;

  parent_class = g_type_class_ref (GNL_TYPE_OBJECT);

  gobject_class->set_property = GST_DEBUG_FUNCPTR (gnl_source_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gnl_source_get_property);
  gobject_class->dispose      = GST_DEBUG_FUNCPTR (gnl_source_dispose);
  gobject_class->finalize     = GST_DEBUG_FUNCPTR (gnl_source_finalize);

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_ELEMENT,
    g_param_spec_object ("element", "Element", "The element to manage",
                         GST_TYPE_ELEMENT, G_PARAM_READWRITE));

  gnl_source_signals[GET_PAD_FOR_STREAM_ACTION] =
    g_signal_new("get_pad_for_stream",
                 G_TYPE_FROM_CLASS(klass),
		 G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                 G_STRUCT_OFFSET (GnlSourceClass, get_pad_for_stream),
                 NULL, NULL,
                 gnl_marshal_OBJECT__STRING,
                 GST_TYPE_PAD, 1, G_TYPE_STRING);

  gstelement_class->change_state 	= gnl_source_change_state;
  gstelement_class->request_new_pad 	= gnl_source_request_new_pad;

  gnlobject_class->prepare 		= gnl_source_prepare;

  klass->get_pad_for_stream		= gnl_source_get_pad_for_stream;
}


static void
gnl_source_init (GnlSource *source)
{
  source->element_added = FALSE;
  GST_FLAG_SET (source, GST_ELEMENT_DECOUPLED);
  GST_FLAG_SET (source, GST_ELEMENT_EVENT_AWARE);

  source->bin = gst_pipeline_new ("gnlpipeline");
  gst_bin_add(GST_BIN(source), GST_ELEMENT(source->bin));
  source->element = 0;
  source->linked_pads = 0;
  source->total_pads = 0;
  source->links = NULL;
  source->pending_seek = NULL;
  source->private = g_new0(GnlSourcePrivate, 1);
}

static void
gnl_source_dispose (GObject *object)
{
  GnlSource *source = GNL_SOURCE (object);
  GSList	*pads = source->links;
  SourcePadPrivate	*priv;

  if (source->private->dispose_has_run)
    return;

  GST_INFO("dispose");
  source->private->dispose_has_run = TRUE;

  
  while (pads) {
    priv = (SourcePadPrivate *) pads->data;

    g_slist_free (priv->queue);
    pads = g_slist_next (pads);
  }

  if (source->element)
    gst_bin_remove (GST_BIN (source->bin), source->element);
  gst_bin_remove(GST_BIN(source), GST_ELEMENT(source->bin));
  
  G_OBJECT_CLASS (parent_class)->dispose (object);
  GST_INFO("dispose END");
}

static void
gnl_source_finalize (GObject *object)
{
  GnlSource *source = GNL_SOURCE (object);

  GST_INFO("finalize");
  g_free (source->private);
  g_slist_free (source->links);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/** 
 * gnl_source_new:
 * @name: The name of the new #GnlSource
 * @element: The element managed by this source
 *
 * Creates a new source object with the given name. The
 * source will manage the given GstElement
 *
 * Returns: a new #GnlSource object or NULL in case of
 * an error.
 */
GnlSource*
gnl_source_new (const gchar *name, GstElement *element)
{
  GnlSource *source;

  GST_INFO ("name[%s], element[%s]", name,
	    gst_element_get_name( element ) );
 
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (element != NULL, NULL);

  source = g_object_new(GNL_TYPE_SOURCE, NULL);
		  
  gst_object_set_name(GST_OBJECT(source), name);
  gnl_source_set_element(source, element);

  GST_INFO("sched source[%p] bin[%p]", 
	   GST_ELEMENT_SCHED(source),
	   GST_ELEMENT_SCHED(source->bin));

  return source;
}

/** 
 * gnl_source_get_element:
 * @source: The source element to get the element of
 *
 * Get the element managed by this source.
 *
 * Returns: The element managed by this source.
 */
GstElement*
gnl_source_get_element (GnlSource *source)
{
  g_return_val_if_fail (GNL_IS_SOURCE (source), NULL);

  return source->element;
}

/** 
 * gnl_source_set_element:
 * @source: The source element to set the element on
 * @element: The element that should be managed by the source
 *
 * Set the given element on the given source. If the source
 * was managing another element, it will be removed first.
 */
void
gnl_source_set_element (GnlSource *source, GstElement *element)
{
  gchar	*tmp;

  g_return_if_fail (GNL_IS_SOURCE (source));
  g_return_if_fail (GST_IS_ELEMENT (element));

  GST_INFO ("Source[%s] Element[%s] sched[%p]",
	    gst_element_get_name(GST_ELEMENT(source)),
	    gst_element_get_name(element),
	    GST_ELEMENT_SCHED(element));

  if (source->element) {
    gst_bin_remove (GST_BIN (source->bin), source->element);
    gst_object_unref (GST_OBJECT (source->element));
  }
  
  source->element = element;
  source->linked_pads = 0;
  source->total_pads = 0;
  source->links = NULL;
  source->pending_seek = NULL;
  source->private->seek_start = GST_CLOCK_TIME_NONE;
  source->private->seek_stop = GST_CLOCK_TIME_NONE;

  tmp = g_strdup_printf ("gnlsource_pipeline_%s", gst_element_get_name(element));
  gst_element_set_name (source->bin, tmp);
  g_free (tmp);

  gst_bin_add (GST_BIN (source->bin), source->element);
}

static GstCaps *source_getcaps (GstPad *pad)
{
  GstPad *otherpad;
  SourcePadPrivate *private;

  private = gst_pad_get_element_private (pad);
  
  otherpad = (GST_PAD_IS_SRC (pad)? private->sinkpad : private->srcpad);

  return gst_pad_get_allowed_caps (otherpad);
}

static GstPadLinkReturn
source_link (GstPad *pad, const GstCaps *caps)
{
  GstPad *otherpad;
  SourcePadPrivate *private;
  const gchar	*type;

  GST_INFO("linking");
  private = gst_pad_get_element_private (pad);
  type = gst_structure_get_name (gst_caps_get_structure (caps, 0));
  if (!g_ascii_strncasecmp (type, "audio/x-raw", 11)) {
    private->type = TYPE_AUDIO;
    if (!gst_structure_get_int (gst_caps_get_structure (caps, 0), "width", &private->audiowidth))
      GST_WARNING ("Couldn't get audio width from pad %s:%s",
		   GST_DEBUG_PAD_NAME (pad));
    if (private->audiowidth)
      private->audiowidth /= 8;
    if (!gst_structure_get_int (gst_caps_get_structure (caps, 0), "rate", &private->rate))
      GST_WARNING ("Couldn't get audio rate from pad %s:%s",
		   GST_DEBUG_PAD_NAME (pad));
    if (!gst_structure_get_int (gst_caps_get_structure (caps, 0), "channels", &private->nbchanns))
      GST_WARNING ("Couldn't get audio channels from pad %s:%s",
		   GST_DEBUG_PAD_NAME (pad));
  }
  else if (!g_ascii_strncasecmp (type, "video/x-raw", 11))
    private->type = TYPE_VIDEO;
  else private->type = TYPE_OTHER;
/*   GST_INFO ("->type : %d, audiowidth : %d, channs : %d, rate : %d", */
/* 	    private->type, private->audiowidth, */
/* 	    private->nbchanns, private->rate); */
  
  otherpad = (GST_PAD_IS_SRC (pad)? private->sinkpad : private->srcpad);

  return gst_pad_try_set_caps (otherpad, caps);
}

void
source_unlink (GstPad *pad)
{
  SourcePadPrivate	*private;

  GST_INFO("unlinking !!!");

  private = gst_pad_get_element_private (pad);
  private->type = TYPE_NONE;
  private->audiowidth = 0;
  private->nbchanns = 0;
  private->rate = 0;
}

/** 
 * gnl_source_get_pad_for_stream:
 * @source: The source element to query
 * @padname: The padname of the element managed by this source
 *
 * Get a handle to a pad that provides the data from the given pad
 * of the managed element.
 *
 * Returns: A pad 
 */
GstPad*
gnl_source_get_pad_for_stream (GnlSource *source, const gchar *padname)
{
  GstPad *srcpad, *sinkpad, *pad;
  SourcePadPrivate *private;
  gchar *ourpadname;

  g_return_val_if_fail (GNL_IS_SOURCE (source), NULL);
  g_return_val_if_fail (padname != NULL, NULL);

  GST_INFO("Source[%s] padname[%s] sched[%p] binsched[%p]",
	   gst_element_get_name(GST_ELEMENT(source)),
	   padname,
	   GST_ELEMENT_SCHED(source),
	   GST_ELEMENT_SCHED(source->bin));
  
  private = g_new0 (SourcePadPrivate, 1);

  srcpad = gst_pad_new (padname, GST_PAD_SRC);
  gst_element_add_pad (GST_ELEMENT (source), srcpad);
  gst_pad_set_element_private (srcpad, private);
  gst_pad_set_get_function (srcpad, source_getfunction);
  gst_pad_set_link_function (srcpad, source_link);
  gst_pad_set_getcaps_function (srcpad, source_getcaps);

  ourpadname = g_strdup_printf ("internal_sink_%s", padname);
  sinkpad = gst_pad_new (ourpadname, GST_PAD_SINK);
  g_free (ourpadname);
  gst_element_add_pad (GST_ELEMENT (source), sinkpad);
  gst_pad_set_element_private (sinkpad, private);
  gst_pad_set_chain_function (sinkpad, source_chainfunction);
  gst_pad_set_link_function (sinkpad, source_link);
  gst_pad_set_getcaps_function (sinkpad, source_getcaps);

  private->srcpad  = srcpad;
  private->sinkpad = sinkpad;

  source->links = g_slist_prepend (source->links, private);

  pad = gst_element_get_pad (source->element, padname);

  source->total_pads++;

  if (pad) {
    GST_INFO("%s linked straight away with %s",
	     gst_element_get_name(GST_ELEMENT(source)),
	     gst_pad_get_name(sinkpad));
    gst_pad_link (pad, sinkpad);
    source->linked_pads++;
  }
  else {
    LinkData *data = g_new0 (LinkData, 1);

    GST_INFO("%s links with delay...",
	     gst_element_get_name(GST_ELEMENT(source)));

    data->source = source;
    data->padname = padname;
    data->target = sinkpad;
    
    g_signal_connect (G_OBJECT (source->element), 
		      "new_pad", 
		      G_CALLBACK (source_element_new_pad), 
		      data);
  }

  return srcpad;
}

static GstPad*
gnl_source_request_new_pad (GstElement *element, GstPadTemplate *templ, 
		 	    const gchar *name)
{
  
  GST_INFO("element[%s] Template[##] name[%s]",
	   gst_element_get_name(element),
	   name);

  return gnl_source_get_pad_for_stream (GNL_SOURCE (element), name);
}

static void
clear_queues (GnlSource *source)
{
  GSList *walk = source->links;

  GST_INFO("clear_queues %p", walk);
  while (walk) {
    SourcePadPrivate *private = (SourcePadPrivate *) walk->data;
    
    if (private->queue) {
      g_slist_free (private->queue);
      private->queue = NULL;
    } else
      GST_INFO("queue already empty !");
    
    walk = g_slist_next (walk);
  }
}


static gboolean
source_send_seek (GnlSource *source, GstEvent *event)
{
  const GList *pads;
  gboolean	wasinplay = FALSE;
  gboolean	res = FALSE;

  /* ghost all pads */
  pads = gst_element_get_pad_list (source->element);

  if (!event)
    return FALSE;

  if (!pads)
    GST_WARNING("%s has no pads...",
	     gst_element_get_name (GST_ELEMENT (source->element)));

  source->private->seek_start = GST_EVENT_SEEK_OFFSET (event);
  source->private->seek_stop = GST_EVENT_SEEK_ENDOFFSET (event);

  GST_INFO("seek from %lld to %lld",
	   source->private->seek_start,
	   source->private->seek_stop);

  event = gst_event_new_seek(GST_FORMAT_TIME | GST_SEEK_METHOD_SET | GST_SEEK_FLAG_FLUSH,
			     source->private->seek_start);

  if (GST_STATE(source->bin) == GST_STATE_PLAYING)
    wasinplay = TRUE;
  if (!(gst_element_set_state(source->bin, GST_STATE_PAUSED)))
    GST_WARNING("couldn't set GnlSource's bin to PAUSED !!!");
  while (pads) {
    GstPad *pad = GST_PAD (GST_PAD_REALIZE (pads->data));
    
    GST_INFO ("Trying to seek on pad %s:%s",
	      GST_DEBUG_PAD_NAME (pad));
    gst_event_ref (event);
    
    GST_INFO ("%s: seeking to %lld on pad %s:%s",
	      gst_element_get_name (GST_ELEMENT (source)),
	      source->private->seek_start,
	      GST_DEBUG_PAD_NAME (pad));
    
    if (!gst_pad_send_event (pad, event)) {
      GST_WARNING ("%s: could not seek",
		   gst_element_get_name (GST_ELEMENT (source)));
      res &= FALSE;
    } else
      res = TRUE;

    pads = g_list_next (pads);
  }

  if (wasinplay)
    gst_element_set_state(source->bin, GST_STATE_PLAYING);

  clear_queues (source);

  return res;
}

static gboolean
queueing_probe (GstProbe *probe, GstData **data, gpointer user_data)
{
  GnlSource	*source = GNL_SOURCE (user_data);
  if (GST_IS_BUFFER (*data))
    source->private->queued = TRUE;

  return TRUE;
}

static gboolean
source_queue_media (GnlSource *source)
{
  gboolean filled = TRUE;
  GstElement		*prerollpipeline;
  GstProbe		*probe;
  GstElement		*fakesink;
  GstPad		*prevpad;

  if ((source->queueing) || (source->private->queued))
    return TRUE;
  GST_INFO ("prerolling/queueing in seperate pipeline");

  /* Disconnect pad and remember it */
  prevpad = GST_PAD_PEER (gst_element_get_pad (source->element, "src"));
  gst_pad_unlink (gst_element_get_pad (source->element, "src"), prevpad);

  /* moving source->Bin to pre-roll pipeline */
  prerollpipeline = gst_pipeline_new ("preroll-pipeline");

  fakesink = gst_element_factory_make ("fakesink", "afakesink");

  gst_object_ref (GST_OBJECT (source->element));
  gst_bin_remove (GST_BIN (source->bin), GST_ELEMENT (source->element));
  gst_bin_add (GST_BIN(prerollpipeline), GST_ELEMENT (source->element));
  gst_bin_add (GST_BIN(prerollpipeline), fakesink);

  gst_element_set_state (GST_ELEMENT (prerollpipeline), GST_STATE_PAUSED);

  if (!gst_element_link (GST_ELEMENT (source->element), fakesink)) {
    GST_ERROR ("Couldn't link...");
    return FALSE;
  }

  probe = gst_probe_new (FALSE, queueing_probe, source);
  gst_pad_add_probe (gst_element_get_pad (source->element, "src"),
		     probe);

  source->queueing = TRUE;

  GST_INFO("about to iterate");
  gst_element_set_state (GST_ELEMENT (prerollpipeline), GST_STATE_PLAYING);
  while (!source->private->queued) {
    if (!gst_bin_iterate (GST_BIN (prerollpipeline))) {
      break;
    }
    GST_INFO ("Merry go round...");
/*     filled = source_is_media_queued (source); */
  }
  gst_element_set_state (GST_ELEMENT (prerollpipeline), GST_STATE_PAUSED);
  GST_INFO("Finished iterating");

  source->queueing = FALSE;

  source->private->queued = TRUE;

  gst_object_ref (GST_OBJECT (source->element));
  gst_pad_remove_probe (gst_element_get_pad (source->element, "src"),
			probe);
  gst_probe_destroy (probe);

  gst_bin_remove (GST_BIN (prerollpipeline), GST_ELEMENT (source->element));
  gst_bin_add (GST_BIN (source->bin), GST_ELEMENT (source->element));
  gst_object_unref (GST_OBJECT (prerollpipeline));

  /* Reconnect element's pad */
  gst_pad_link (gst_element_get_pad (source->element, "src"), prevpad);
  
  GST_INFO("END : source media is queued [%d]",
	   filled);
  return filled;
}

static GstBuffer *
crop_incoming_buffer (GstPad *pad, GstBuffer *buf, GstClockTime start, GstClockTime stop)
{
  SourcePadPrivate	*private = gst_pad_get_element_private (pad);
  GstBuffer		*outbuffer;

  GST_INFO ("start : %" GST_TIME_FORMAT " , stop : %" GST_TIME_FORMAT,
	    GST_TIME_ARGS (start),
	    GST_TIME_ARGS (stop));
  if ((private->type == TYPE_AUDIO) || (private->type == TYPE_VIDEO)) {
    if (private->type == TYPE_AUDIO) {
      gint64 offset, size;
      GST_INFO ("Buffer Size : %d ==> time : %" GST_TIME_FORMAT,
		GST_BUFFER_SIZE (buf),
		GST_TIME_ARGS (
			   (GST_BUFFER_SIZE (buf) * GST_SECOND) / (private->nbchanns * private->audiowidth * private->rate)
			   ));
      GST_INFO ("start - timestamp : %" GST_TIME_FORMAT ", stop - start : %" GST_TIME_FORMAT,
		GST_TIME_ARGS (start - GST_BUFFER_TIMESTAMP (buf)),
		GST_TIME_ARGS (stop - start));
      offset = (start - GST_BUFFER_TIMESTAMP (buf)) * private->rate * private->nbchanns * private->audiowidth / GST_SECOND;
      offset -= offset % (private->nbchanns * private->audiowidth);
      size = (stop - start) * private->rate * private->nbchanns * private->audiowidth / GST_SECOND;
      size -= size % (private->nbchanns * private->audiowidth);
      GST_INFO ("offset : %lld , size : %lld, sum : %lld",
		offset, size, offset + size);

      if ((offset + size) > GST_BUFFER_SIZE (buf))
	size -= (offset + size) - GST_BUFFER_SIZE (buf);
      outbuffer = gst_buffer_create_sub (buf, (guint) offset, (guint) size);
      gst_buffer_unref (buf);
    } else
      outbuffer = buf;
    GST_BUFFER_TIMESTAMP (outbuffer) = start;
    GST_BUFFER_DURATION (outbuffer) = stop - start;
    GST_INFO ("Changed/created buffer with time : %" GST_TIME_FORMAT " , duration : %" GST_TIME_FORMAT,
	      GST_TIME_ARGS (start),
	      GST_TIME_ARGS (stop - start));
    return outbuffer;
  }
  GST_WARNING ("Can't resize incoming buffer because it isn't AUDIO or VIDEO...");
  return buf;
}

static void
source_chainfunction (GstPad *pad, GstData *buf)
{
  SourcePadPrivate *private;
  GnlSource *source;
  GnlObject *object;
  GstClockTimeDiff intime, dur;
  GstClockTime	mstart, mstop;
  GstBuffer	*buffer = GST_BUFFER(buf);

  private = gst_pad_get_element_private (pad);
  source = GNL_SOURCE (gst_pad_get_parent (pad));
  object = GNL_OBJECT (source);

  if (GST_IS_EVENT(buffer))
    GST_INFO("Chaining an event : %d",
	     GST_EVENT_TYPE(buffer));
  else
    GST_INFO("Chaining a buffer time : %" GST_TIME_FORMAT ", duration : %" GST_TIME_FORMAT,
	     GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)),
	     GST_TIME_ARGS (GST_BUFFER_DURATION (buffer)));
  if (GST_IS_BUFFER (buffer) && !source->queueing) {
    intime = GST_BUFFER_TIMESTAMP (buffer);
    dur = GST_BUFFER_DURATION (buffer);
    if (dur == GST_CLOCK_TIME_NONE)
      dur = 0LL;
    mstart = (object->media_start == GST_CLOCK_TIME_NONE) ? object->start : object->media_start;
    mstop = (object->media_stop == GST_CLOCK_TIME_NONE) ? object->stop : object->media_stop;

    if (dur + intime < mstart) {
      GST_INFO ("buffer doesn't start/end before source start, unreffing buffer");
      gst_buffer_unref (buffer);
      return;
    }
    if (intime >= mstop) {
      GST_INFO ("buffer is after stop, creating EOS");
      gst_buffer_unref (buffer);
      buffer = GST_BUFFER (gst_event_new (GST_EVENT_EOS));
    } else if ((intime < mstart) && ((intime + dur) > mstart)) {
      GST_INFO ("buffer starts before media_start, but ends after media_start");
      buffer = crop_incoming_buffer (pad, buffer, mstart, intime + dur);
    } else if ((intime < mstop) && ((intime + dur) > mstop)) {
      GST_INFO ("buffer starts before media_stop, but ends after media_stop");
      buffer = crop_incoming_buffer (pad, buffer, intime, mstop);
    }
  }
  
  private->queue = g_slist_append (private->queue, buffer);
  GST_INFO("end of chaining");
}

static GstData*
source_getfunction (GstPad *pad)
{
  GstBuffer *buffer, *qbuffer = NULL;
  SourcePadPrivate *private;
  GnlSource *source;
  GnlObject *object;
  gboolean found = FALSE;

  private = gst_pad_get_element_private (pad);
  source = GNL_SOURCE (gst_pad_get_parent (pad));
  object = GNL_OBJECT (source);

  if (!GST_PAD_IS_ACTIVE (pad)) {
    GST_INFO("%s[State:%d] : pad [%s:%s] not active, creating EOS",
	     gst_element_get_name (GST_ELEMENT (source)),
	     gst_element_get_state (GST_ELEMENT (source)),
	     GST_DEBUG_PAD_NAME (GST_PAD_REALIZE (pad)));
    found = TRUE;
    buffer = GST_BUFFER (gst_event_new (GST_EVENT_EOS));
  }

  while (!found) {
    /* No data in private queue, EOS */
    while (!private->queue) {
      if (!gst_bin_iterate (GST_BIN (source->bin))) {
	GST_INFO("Iterate returned FALSE, Nothing more coming from %s",
		 gst_element_get_name(GST_ELEMENT(source->bin)));
        buffer = GST_BUFFER (gst_event_new (GST_EVENT_EOS));
	found = TRUE;
        break;
      }
      GST_INFO("while !private->queue %p", private->queue);
    }

    /* Data in private queue */
    if (private->queue) {
      buffer = GST_BUFFER (private->queue->data);
      qbuffer = buffer;
      GST_INFO ("Processing buffer from private queue");
      
      /* if DATA is EOS, forward it*/ 
      if (GST_IS_EVENT (buffer)) {
	GST_INFO("Event Buffer type : %d", GST_EVENT_TYPE(buffer));
        if (GST_EVENT_TYPE (buffer) == GST_EVENT_EOS) {
          GST_INFO ("%s: EOS at %lld %lld %lld %lld / now:%lld", 
		    gst_element_get_name (GST_ELEMENT (source)), 
		    object->media_start,
		    object->media_stop,
		    object->start,
		    object->stop,
		    object->current_time);

          //object->current_time = object->media_start + object->start;
          object->current_time++;
	  gst_pad_set_active (pad, FALSE);
	  found = TRUE;
        } else if (GST_EVENT_TYPE (buffer) == GST_EVENT_DISCONTINUOUS) {
	  gint64	dvalue = 0LL;
	  
	  if (!gst_event_discont_get_value (GST_EVENT (buffer), GST_FORMAT_TIME, &dvalue))
	    GST_WARNING ("couldn't get TIME value from discont event !");
	  else {
	    gst_data_unref (GST_DATA(buffer));
	    if (dvalue >= source->private->seek_start && (gnl_media_to_object_time (object, dvalue, &dvalue))) {
	      object->current_time = dvalue;
	      buffer = GST_BUFFER (gst_event_new_discontinuous (FALSE, GST_FORMAT_TIME, dvalue, NULL));
	      found = TRUE;
	    } else
	      GST_WARNING ("value from discont event is outside limits, discarding event");
	  }
	}
      }
      else {
	/* If data is buffer */
        GstClockTimeDiff outtime, intime, in_endtime;
	gboolean	inlimits = FALSE;

        intime = GST_BUFFER_TIMESTAMP (buffer);

	if (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_DURATION (buffer)))
	  in_endtime = intime + GST_BUFFER_DURATION (buffer);
	else
	  in_endtime = GST_CLOCK_TIME_NONE;

	inlimits = gnl_media_to_object_time (object, intime, &outtime);

	/* check if buffer is outside seek range */
	if ((GST_CLOCK_TIME_IS_VALID(source->private->seek_stop)
	     && (intime >= source->private->seek_stop))) {
	  GST_INFO("Data is after seek_stop, creating EOS");
	  gst_data_unref(GST_DATA(buffer));
	  buffer = GST_BUFFER (gst_event_new (GST_EVENT_EOS));
	  found = TRUE;
	} else if ((!inlimits) || 
		   ((GST_CLOCK_TIME_IS_VALID(in_endtime)?in_endtime:intime) < source->private->seek_start)) {
	  GST_WARNING ("buffer time is out of media/seek limits ! Dropping buffer");
	  gst_data_unref (GST_DATA(buffer));
	} else {
/* 	  outtime = intime - object->media_start + object->start; */
	  
	  if ( GST_CLOCK_TIME_IS_VALID(in_endtime)) {
	    if ( GST_CLOCK_TIME_IS_VALID(source->private->seek_stop) &&
		 ( in_endtime > source->private->seek_stop)) {
	      GST_INFO("Data starts before seek_stop, but ends after seek_stop, cutting buffer");
	      buffer = crop_incoming_buffer( pad, buffer, intime, 
					     source->private->seek_stop);
	    } else if ( GST_CLOCK_TIME_IS_VALID(source->private->seek_start) &&
			( intime < source->private->seek_start)) {
	      GST_INFO("Data starts before seek_start, but ends after seek_start, cutting buffer");
	      buffer = crop_incoming_buffer (pad, buffer, source->private->seek_start , in_endtime );
	    }
	  }

	  object->current_time = outtime;
	  
	  GST_INFO ("%s: got %" GST_TIME_FORMAT " corrected to %" GST_TIME_FORMAT " (Source[%" GST_TIME_FORMAT " -> %" GST_TIME_FORMAT "] media[%" GST_TIME_FORMAT " -> %" GST_TIME_FORMAT "])", 
		    gst_element_get_name (GST_ELEMENT (source)), 
		    GST_TIME_ARGS(intime),
		    GST_TIME_ARGS(outtime),
		    GST_TIME_ARGS(object->start),
		    GST_TIME_ARGS(object->stop),
		    GST_TIME_ARGS(object->media_start),
		    GST_TIME_ARGS(object->media_stop));
	  GST_INFO ("Seek was from %" GST_TIME_FORMAT " -> %" GST_TIME_FORMAT,
		    GST_TIME_ARGS(source->private->seek_start),
		    GST_TIME_ARGS(source->private->seek_stop));
	  
	  GST_BUFFER_TIMESTAMP (buffer) = outtime;
	  
	  found = TRUE;
	}
      }
      /* flush last element in queue */
/*       private->queue = g_slist_remove (private->queue, buffer); */
      if (qbuffer)
	private->queue = g_slist_remove (private->queue, buffer);
    }
  }
  
  {
    GSList *walk;
    gboolean eos = TRUE;
    
    walk = source->links;
    while (walk) {
      SourcePadPrivate *test_priv = (SourcePadPrivate *) walk->data;

      if (GST_PAD_IS_ACTIVE (test_priv->srcpad)) {
	eos = FALSE;
	break;
      }
      walk = g_slist_next (walk);
    }
    if (eos) {
      GST_INFO("EOS on source");
      gst_element_set_eos (GST_ELEMENT (source));
      GST_INFO("End of EOS on source");
    }
  }
  GST_INFO("END");
/*   if (GST_IS_EVENT(buffer) && (GST_EVENT_TYPE (buffer) == GST_EVENT_EOS)) */
/*     gnl_object_set_active(object, FALSE); */
  return (GstData *) buffer;
}

static gboolean
gnl_source_prepare (GnlObject *object, GstEvent *event)
{
  GnlSource *source = GNL_SOURCE (object);
  gboolean res = TRUE;

  GST_INFO("Object[%s] [%lld]->[%lld] State:%d",
	   gst_element_get_name(GST_ELEMENT(object)),
	   GST_EVENT_SEEK_OFFSET(event),
	   GST_EVENT_SEEK_ENDOFFSET(event),
	   gst_element_get_state (GST_ELEMENT(object)));

  source->pending_seek = event;
  
  if (gst_element_get_state (GST_ELEMENT (object)) >= GST_STATE_READY) {
    clear_queues (source);
    res = source_send_seek (source, source->pending_seek);
  }
  
  return res;
}


static void
source_element_new_pad (GstElement *element, GstPad *pad, LinkData *data)
{
  GST_INFO ("source %s new pad %s", GST_OBJECT_NAME (data->source), GST_PAD_NAME (pad));
  GST_INFO ("link %s new pad %s %d", data->padname, gst_pad_get_name (pad),
     					GST_PAD_IS_LINKED (data->target));

  if (!strcmp (gst_pad_get_name (pad), data->padname) && 
     !GST_PAD_IS_LINKED (data->target)) 
  {
     gst_pad_link (pad, data->target);
     gst_pad_set_active (data->target, TRUE);
  }
}

static GstElementStateReturn
gnl_source_change_state (GstElement *element)
{
  GnlSource *source = GNL_SOURCE (element);
  GstElementStateReturn	res = GST_STATE_SUCCESS;
  GstElementStateReturn	res2 = GST_STATE_SUCCESS;
  gint	transition = GST_STATE_TRANSITION (source);

  GST_DEBUG ("Calling parent change_state");
  res2 = GST_ELEMENT_CLASS (parent_class)->change_state (element);
  if (!res2)
    return GST_STATE_FAILURE;
  GST_DEBUG ("doing our own stuff %d", transition);
  switch (transition) {
  case GST_STATE_NULL_TO_READY:
    break;
  case GST_STATE_READY_TO_PAUSED:
    source->pending_seek = \
      gst_event_new_seek (GST_FORMAT_TIME | GST_SEEK_METHOD_SET | GST_SEEK_FLAG_FLUSH, 0LL);
    if (!source_queue_media (source))
      res = GST_STATE_FAILURE;
    break;
  case GST_STATE_PAUSED_TO_PLAYING:
    if (!GNL_OBJECT(source)->active)
      GST_WARNING("Trying to change state but Source %s is not active ! This might be normal...",
		  gst_element_get_name(element));
/*     else if (!gst_element_set_state (source->bin, GST_STATE_PLAYING)) */
/*       res = GST_STATE_FAILURE; */
    break;
  case GST_STATE_PLAYING_TO_PAUSED:
    /* done by GstBin->change_state */
    /*     if (!gst_element_set_state (source->bin, GST_STATE_PAUSED)) */
    /*       res = GST_STATE_FAILURE; */
    break;
  case GST_STATE_PAUSED_TO_READY:
    source->private->queued = FALSE;
    source->queueing = FALSE;
    break;
  case GST_STATE_READY_TO_NULL:
    break;
  default:
    GST_INFO ("TRANSITION NOT HANDLED ???");
    break;
  }
  
  if ((res != GST_STATE_SUCCESS) || (res2 != GST_STATE_SUCCESS)) {
    GST_WARNING("%s : something went wrong",
		gst_element_get_name(element));
    return GST_STATE_FAILURE;
  }
  GST_INFO("%s : change_state returns %d/%d",
	   gst_element_get_name(element),
	   res, res2);
  return res2;
}

static void
gnl_source_set_property (GObject *object, guint prop_id,
			 const GValue *value, GParamSpec *pspec)
{
  GnlSource *source;

  g_return_if_fail (GNL_IS_SOURCE (object));

  source = GNL_SOURCE (object);

  switch (prop_id) {
    case ARG_ELEMENT:
      gnl_source_set_element (source, GST_ELEMENT (g_value_get_object (value)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gnl_source_get_property (GObject *object, guint prop_id, 
			 GValue *value, GParamSpec *pspec)
{
  GnlSource *source;
  
  g_return_if_fail (GNL_IS_SOURCE (object));

  source = GNL_SOURCE (object);

  switch (prop_id) {
    case ARG_ELEMENT:
      g_value_set_object (value, gnl_source_get_element (source));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
