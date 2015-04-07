from gi.repository import GES, Gst, GLib

def bus_message_cb (bus, message, loop):
    if message.type == Gst.MessageType.EOS:
        loop.quit()

def play_timeline(timeline):
    pipeline = GES.Pipeline.new()
    loop = GLib.MainLoop()

    bus = pipeline.get_bus()
    bus.add_signal_watch()
    bus.connect ("message", bus_message_cb, loop)

    pipeline.set_timeline (timeline)
    pipeline.set_state (Gst.State.PLAYING)
    loop.run()
    pipeline.set_state (Gst.State.NULL)
