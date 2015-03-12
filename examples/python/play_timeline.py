from gi.repository import GES, Gst, GLib

def play_timeline(timeline):
    pipeline = GES.Pipeline.new()
    pipeline.set_timeline (timeline)
    pipeline.set_state (Gst.State.PLAYING)
    loop = GLib.MainLoop()
    loop.run()
