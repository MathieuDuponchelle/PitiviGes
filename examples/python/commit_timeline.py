from gi.repository import GES, Gst
from time import sleep

from create_timeline_with_n_layers import create_timeline_with_n_layers
from add_one_video_track import add_one_video_track
from play_timeline import prepare_pipeline, play_pipeline

if __name__=="__main__":
    Gst.init([])
    GES.init()

    # Let's setup a timeline with a 1-second clip, this part should now
    # look pretty familiar
    timeline = create_timeline_with_n_layers(1)
    add_one_video_track(timeline)
    layer = timeline.get_layer(0)
    clip = GES.TestClip.new()
    layer.add_clip(clip)

    clip.props.duration = 1 * Gst.SECOND

    # This returns us a paused pipeline, all the changes are commited
    # automatically when going from READY to PAUSED
    pipeline = prepare_pipeline(timeline)

    # This change will only be executed once we have commited the timeline
    clip.props.duration = 5 * Gst.SECOND

    # Even though the duration of the clip has been set to 5 seconds, playing
    # the pipeline will only last for 1 second as we haven't commited our
    # changes
    play_pipeline(pipeline)
    print("We played the pipeline once")
    # Let's wait for one second just to make sure we notice the difference
    sleep(1)

    timeline.commit_sync()

    # The changes were taken into account, this will play for 5 seconds
    print("Playing again")
    play_pipeline(pipeline)
    print("We played the timeline twice, bye o/")
