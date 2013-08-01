/* GStreamer Editing Services
 * Copyright (C) 2010 Thibault Saunier <tsaunier@gnome.org>
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
 * SECTION:ges-base-effect
 * @short_description: adds an effect to a stream in a GESSourceClip or a
 * #GESLayer
 */

#include <glib/gprintf.h>

#include "ges-utils.h"
#include "ges-internal.h"
#include "ges-track-element.h"
#include "ges-base-effect.h"

G_DEFINE_ABSTRACT_TYPE (GESBaseEffect, ges_base_effect, GES_TYPE_OPERATION);

struct _GESBaseEffectPrivate
{
  void *nothing;
};

static void
ges_base_effect_class_init (GESBaseEffectClass * klass)
{
  g_type_class_add_private (klass, sizeof (GESBaseEffectPrivate));
}

static void
ges_base_effect_init (GESBaseEffect * self)
{
  self->priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, GES_TYPE_BASE_EFFECT,
      GESBaseEffectPrivate);
}
