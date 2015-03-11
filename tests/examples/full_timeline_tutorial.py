from gi.repository import GES, Gst

from create_timeline_with_n_layers import create_timeline_with_n_layers
from add_one_clip_in_each_layer import add_one_clip_in_each_layer
from add_one_video_track import add_one_video_track
from play_timeline import play_timeline

if __name__=="__main__":
    Gst.init([])
    GES.init()

    timeline = create_timeline_with_n_layers(2)
    add_one_clip_in_each_layer(timeline, 10 * Gst.SECOND)
    add_one_video_track (timeline)
    # Convenience function for the sake of examples.
    play_timeline (timeline)
