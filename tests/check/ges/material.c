/* GStreamer Editing Services
 * Copyright (C) 2012 Volodymyr Rudyi <vladimir.rudoy@gmail.com>
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

#include <ges/ges.h>
#include <gst/check/gstcheck.h>

GMainLoop *mainloop;
static void
source_material_created (GESMaterial * material, GError * error,
    gpointer user_data)
{
  fail_unless (GES_IS_MATERIAL (material));
  assert_equals_int (error->domain, GST_RESOURCE_ERROR);

  gst_object_unref (material);
  g_main_loop_quit (mainloop);
}

GST_START_TEST (test_basic)
{
  gst_init (NULL, NULL);
  ges_init ();

  mainloop = g_main_loop_new (NULL, FALSE);
  fail_unless (ges_material_new (GES_TYPE_TIMELINE_FILE_SOURCE,
          source_material_created, NULL, "file:///this/is/not/for/real"));

  g_main_loop_run (mainloop);

  g_main_loop_unref (mainloop);
}

GST_END_TEST;

static Suite *
ges_suite (void)
{
  Suite *s = suite_create ("ges");
  TCase *tc_chain = tcase_create ("material");

  suite_add_tcase (s, tc_chain);

  tcase_add_test (tc_chain, test_basic);

  return s;
}

int
main (int argc, char **argv)
{
  int nf;

  Suite *s = ges_suite ();
  SRunner *sr = srunner_create (s);

  gst_check_init (&argc, &argv);
  ges_init ();

  srunner_run_all (sr, CK_NORMAL);
  nf = srunner_ntests_failed (sr);
  srunner_free (sr);

  return nf;
}
