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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/* TODO
 * Add a deinit function
 *
 * Do not forget to
 *  + g_ptr_array_unref (new_paths);
 *  + g_hash_table_unref (tried_uris);
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <ges/ges.h>
#include <grilo.h>

#include "ges/gstframepositionner.h"
#include "ges-internal.h"
#include "ges/nle/nle.h"

#define GES_GNONLIN_VERSION_NEEDED_MAJOR 1
#define GES_GNONLIN_VERSION_NEEDED_MINOR 2
#define GES_GNONLIN_VERSION_NEEDED_MICRO 0

GST_DEBUG_CATEGORY (_ges_debug);

static gboolean ges_initialized = FALSE;

struct _elements_entry
{
  const gchar *name;
    GType (*type) (void);
};

static struct _elements_entry _elements[] = {
  {"nlesource", nle_source_get_type},
  {"nlecomposition", nle_composition_get_type},
  {"nleoperation", nle_operation_get_type},
  {"nleurisource", nle_urisource_get_type},
  {NULL, 0}
};

static void
initialize_grilo (void)
{
  GError *error = NULL;
  GrlRegistry *registry;

  grl_init (NULL, NULL);
  registry = grl_registry_get_default ();

  grl_registry_load_all_plugins (registry, &error);
  g_assert_no_error (error);
}

/**
 * ges_init:
 *
 * Initialize the GStreamer Editing Service. Call this before any usage of
 * GES. You should take care of initilizing GStreamer before calling this
 * function.
 */

gboolean
ges_init (void)
{
  gint i = 0;

  /* initialize debugging category */
  GST_DEBUG_CATEGORY_INIT (_ges_debug, "ges", GST_DEBUG_FG_YELLOW,
      "GStreamer Editing Services");

  if (ges_initialized) {
    GST_DEBUG ("already initialized ges");
    return TRUE;
  }

  initialize_grilo ();

  /* register clip classes with the system */

  GES_TYPE_TEST_CLIP;
  GES_TYPE_URI_CLIP;
  GES_TYPE_TITLE_CLIP;
  GES_TYPE_TRANSITION_CLIP;
  GES_TYPE_OVERLAY_CLIP;
  GES_TYPE_OVERLAY_TEXT_CLIP;

  GES_TYPE_GROUP;

  /* register formatter types with the system */
  GES_TYPE_PITIVI_FORMATTER;
  GES_TYPE_COMMAND_LINE_FORMATTER;
  GES_TYPE_XML_FORMATTER;

  /* Register track elements */
  GES_TYPE_EFFECT;

  /* Register interfaces */
  GES_TYPE_META_CONTAINER;

  ges_asset_cache_init ();

  gst_element_register (NULL, "framepositionner", 0,
      GST_TYPE_FRAME_POSITIONNER);
  gst_element_register (NULL, "gespipeline", 0, GES_TYPE_PIPELINE);

  for (; _elements[i].name; i++)
    if (!(gst_element_register (NULL,
                _elements[i].name, GST_RANK_NONE, (_elements[i].type) ())))
      return FALSE;

  nle_init_ghostpad_category ();

  /* TODO: user-defined types? */
  ges_initialized = TRUE;

  GST_DEBUG ("GStreamer Editing Services initialized");

  return TRUE;
}

#ifndef GST_DISABLE_OPTION_PARSING
static gboolean
parse_goption_arg (const gchar * s_opt,
    const gchar * arg, gpointer data, GError ** err)
{
  if (g_strcmp0 (s_opt, "--ges-version") == 0) {
    g_print ("GStreamer Editing Services version %s\n", PACKAGE_VERSION);
    exit (0);
  } else if (g_strcmp0 (s_opt, "--ges-sample-paths") == 0) {
    ges_add_missing_uri_relocation_uri (arg, FALSE);
  } else if (g_strcmp0 (s_opt, "--ges-sample-path-recurse") == 0) {
    ges_add_missing_uri_relocation_uri (arg, TRUE);
  }

  return TRUE;
}
#endif

/**
 * ges_init_get_option_group: (skip)
 *
 * Returns a #GOptionGroup with GES's argument specifications. The
 * group is set up to use standard GOption callbacks, so when using this
 * group in combination with GOption parsing methods, all argument parsing
 * and initialization is automated.
 *
 * This function is useful if you want to integrate GES with other
 * libraries that use GOption (see g_option_context_add_group() ).
 *
 * If you use this function, you should make sure you initialise the GStreamer
 * as one of the very first things in your program.
 *
 * Returns: (transfer full): a pointer to GES's option group.
 */
GOptionGroup *
ges_init_get_option_group (void)
{
#ifndef GST_DISABLE_OPTION_PARSING

  GOptionGroup *group;
  static const GOptionEntry ges_args[] = {
    {"ges-version", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
          (gpointer) parse_goption_arg,
          "Print the GStreamer Editing Services version",
        NULL},
    {"ges-sample-paths", 0, 0, G_OPTION_ARG_CALLBACK,
          (gpointer) parse_goption_arg,
        "List of pathes to look assets in if they were moved"},
    {"ges-sample-path-recurse", 0, 0, G_OPTION_ARG_CALLBACK,
          (gpointer) parse_goption_arg,
        "Same as above, but recursing into the folder"},
    {NULL}
  };

  group = g_option_group_new ("GES", "GStreamer Editing Services Options",
      "Show GStreamer Options", NULL, NULL);
  g_option_group_add_entries (group, ges_args);

  return group;

#else
  return NULL;
#endif
}

/**
 * ges_version:
 * @major: (out): pointer to a guint to store the major version number
 * @minor: (out): pointer to a guint to store the minor version number
 * @micro: (out): pointer to a guint to store the micro version number
 * @nano:  (out): pointer to a guint to store the nano version number
 *
 * Gets the version number of the GStreamer Editing Services library.
 */
void
ges_version (guint * major, guint * minor, guint * micro, guint * nano)
{
  g_return_if_fail (major);
  g_return_if_fail (minor);
  g_return_if_fail (micro);
  g_return_if_fail (nano);

  *major = GES_VERSION_MAJOR;
  *minor = GES_VERSION_MINOR;
  *micro = GES_VERSION_MICRO;
  *nano = GES_VERSION_NANO;
}

/**
 * ges_init_check:
 * @argc: (inout) (allow-none): pointer to application's argc
 * @argv: (inout) (array length=argc) (allow-none): pointer to application's argv
 * @err: pointer to a #GError to which a message will be posted on error
 *
 * Initializes the GStreamer Editing Services library, setting up internal path lists,
 * and loading evrything needed.
 *
 * This function will return %FALSE if GES could not be initialized
 * for some reason.
 *
 * Returns: %TRUE if GES could be initialized.
 */
gboolean
ges_init_check (int *argc, char **argv[], GError ** err)
{
#ifndef GST_DISABLE_OPTION_PARSING
  GOptionGroup *group;
  GOptionContext *ctx;
#endif
  gboolean res;

  if (ges_initialized) {
    GST_DEBUG ("already initialized ges");
    return TRUE;
  }
#ifndef GST_DISABLE_OPTION_PARSING
  ctx = g_option_context_new ("- GStreamer Editing Services initialization");
  g_option_context_set_ignore_unknown_options (ctx, TRUE);
  g_option_context_set_help_enabled (ctx, FALSE);
  group = ges_init_get_option_group ();
  g_option_context_add_group (ctx, group);
  res = g_option_context_parse (ctx, argc, argv, err);
  g_option_context_free (ctx);
#endif
  if (!res)
    return res;

  return ges_init ();
}
