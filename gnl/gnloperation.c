/* GStreamer
 * Copyright (C) 2001 Wim Taymans <wim.taymans@chello.be>
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



#include "gnloperation.h"

static void 		gnl_operation_class_init 	(GnlOperationClass *klass);
static void 		gnl_operation_init 		(GnlOperation *operation);

static void 		gnl_operation_set_element 	(GnlSource *source, GstElement *element);

static GnlSourceClass *parent_class = NULL;

GType
gnl_operation_get_type (void)
{
  static GType operation_type = 0;

  if (!operation_type) {
    static const GTypeInfo operation_info = {
      sizeof (GnlOperationClass),
      NULL,
      NULL,
      (GClassInitFunc) gnl_operation_class_init,
      NULL,
      NULL,
      sizeof (GnlOperation),
      32,
      (GInstanceInitFunc) gnl_operation_init,
    };
    operation_type = g_type_register_static (GNL_TYPE_SOURCE, "GnlOperation", &operation_info, 0);
  }
  return operation_type;
}

static void
gnl_operation_class_init (GnlOperationClass *klass)
{
  GObjectClass *gobject_class;
  GnlSourceClass        *gnlsource_class;

  gobject_class =       (GObjectClass*)klass;
  gnlsource_class =     (GnlSourceClass*)klass;

  parent_class = g_type_class_ref (GNL_TYPE_SOURCE);

  gnlsource_class->set_element =        gnl_operation_set_element;
}

static void
gnl_operation_init (GnlOperation *operation)
{
  operation->num_sinks = 0;
}

static void
gnl_operation_set_element (GnlSource *source, GstElement *element)
{
  const GList *walk;
  GnlOperation *operation = GNL_OPERATION (source);

  parent_class->set_element (source, element);

  walk = gst_element_get_pad_list (element);
  while (walk) {
    GstPad *pad = GST_PAD (walk->data);

    if (GST_PAD_IS_SINK (pad)) {
      gst_element_add_ghost_pad (GST_ELEMENT (source),
		           pad, g_strdup_printf ("sink%d", operation->num_sinks));
      operation->num_sinks++;
    }
    walk = g_list_next (walk);
  }
}

GnlOperation*
gnl_operation_new (const gchar *name)
{
  GnlOperation *new;

  g_return_val_if_fail (name != NULL, NULL);

  new = g_object_new (GNL_TYPE_OPERATION, NULL);
  gst_object_set_name (GST_OBJECT (new), name);

  return new;
}

guint
gnl_operation_get_num_sinks (GnlOperation *operation)
{
  return operation->num_sinks; 
}



