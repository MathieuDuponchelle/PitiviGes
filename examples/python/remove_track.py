from gi.repository import GES, Gst

from add_one_clip_in_each_layer import add_one_clip_in_each_layer
from play_timeline import play_timeline

if __name__=="__main__":
    Gst.init([])
    GES.init()

    timeline = GES.Timeline.new_audio_video()
    timeline.append_layer()
    add_one_clip_in_each_layer(timeline, 10 * Gst.SECOND)
    for track in timeline.get_tracks():
        if track.type == GES.TrackType.VIDEO:
            timeline.remove_track (track)

    timeline.commit()
    play_timeline (timeline)
