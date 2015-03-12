from gi.repository import GES

def add_one_video_track (timeline):
    timeline.add_track (GES.VideoTrack.new())
