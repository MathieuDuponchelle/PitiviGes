#!/usr/bin/env python

from gi.repository import Gst, GES
from create_timeline_with_n_layers import create_timeline_with_n_layers
from add_one_video_track import add_one_video_track
from play_timeline import play_timeline

if __name__=="__main__":
    Gst.init([])
    GES.init()

    timeline = create_timeline_with_n_layers(1)
    add_one_video_track(timeline)
    layer = timeline.get_layer(0)
    clip = GES.TestClip.new()
    clip.props.duration = 1 * Gst.SECOND
    clip.props.name = "my-awesome-test-clip"
    layer.add_clip(clip)

    got_by_name_clip = timeline.get_element("my-awesome-test-clip")
    got_by_name_clip.props.duration = 5 * Gst.SECOND

    # As usual, the timeline commits itself automatically when going from READY
    # to PAUSED, playback will thus last 5 seconds
    play_timeline(timeline)
