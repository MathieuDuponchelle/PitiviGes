from gi.repository import GES, Gst, GLib

loop = GLib.MainLoop()

def bus_message_cb (bus, message):
    global loop
    if message.type == Gst.MessageType.EOS:
        loop.quit()

def play_timeline(timeline):
    global loop
    pipeline = GES.Pipeline.new()

    bus = pipeline.get_bus()
    bus.add_signal_watch()
    bus.connect ("message", bus_message_cb)

    pipeline.set_timeline (timeline)
    pipeline.set_state (Gst.State.PLAYING)

    try:
        loop.run()
    except KeyboardInterrupt:
        pass

    pipeline.set_state (Gst.State.NULL)

def play_pipeline(pipeline):
    global loop
    pipeline.seek_simple(Gst.Format.TIME, Gst.SeekFlags.FLUSH |
            Gst.SeekFlags.ACCURATE, 0)
    pipeline.set_state(Gst.State.PLAYING)
    loop.run()
    pipeline.set_state(Gst.State.PAUSED)
    pipeline.get_state(Gst.CLOCK_TIME_NONE)

def prepare_pipeline(timeline):
    global loop
    pipeline = GES.Pipeline.new()
    bus = pipeline.get_bus()
    bus.add_signal_watch()
    bus.connect("message", bus_message_cb)
    pipeline.set_timeline (timeline)
    pipeline.set_state(Gst.State.PAUSED)
    pipeline.get_state(Gst.CLOCK_TIME_NONE)
    return pipeline
