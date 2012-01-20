/* Bon c'est mon copyright faites gaffe les gars je peux vous retrouver
** Also, props to Lubosz (pronounce lubosh like if you had a hair on your tongue) Sarnecki for the proof of concept.
*/

#include <ges/ges.h>
#include <gst/check/gstcheck.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
/* Here we include our library. You happy to know that ? */

GST_START_TEST (test_ges_simple_keyframe)
{
  GESTimeline *timeline;
  GESTimelineLayer *layer;
  GESTrack *track_audio, *track_video;
  GESTrackParseLaunchEffect *track_effect;
  GESTimelineTestSource *source;
  GESKeyframe *keyframe;

  /* Ok we start fun with that. Are you not entertained ? */
  ges_init ();

  timeline = ges_timeline_new ();
  layer = (GESTimelineLayer *) ges_simple_timeline_layer_new ();
  track_audio = ges_track_audio_raw_new ();
  track_video = ges_track_video_raw_new ();

  ges_timeline_add_track (timeline, track_audio);
  ges_timeline_add_track (timeline, track_video);
  ges_timeline_add_layer (timeline, layer);

  source = ges_timeline_test_source_new ();

  g_object_set (source, "duration", 10 * GST_SECOND, NULL);

  ges_simple_timeline_layer_add_object ((GESSimpleTimelineLayer *) (layer),
      (GESTimelineObject *) source, 0);


  GST_DEBUG ("Create effect");
  track_effect = ges_track_parse_launch_effect_new ("agingtv");

  ges_timeline_object_add_track_object (GES_TIMELINE_OBJECT
      (source), GES_TRACK_OBJECT (track_effect));
  ges_track_add_object (track_video, GES_TRACK_OBJECT (track_effect));
  keyframe = ges_keyframe_new ();
  fail_unless (keyframe != NULL);
}

GST_END_TEST;

static Suite *
ges_suite (void)
{
  Suite *s = suite_create ("ges-keyframe");
  TCase *tc_chain = tcase_create ("keyframe");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_ges_simple_keyframe);

  return s;
}

int
main (int argc, char **argv)
{
  int nf;

  Suite *s = ges_suite ();
  SRunner *sr = srunner_create (s);

  gst_check_init (&argc, &argv);

  srunner_run_all (sr, CK_NORMAL);
  nf = srunner_ntests_failed (sr);
  srunner_free (sr);

  return nf;
}
