from gi.repository import Gst, GES, GLib, GObject

class Simple:
    def __init__(self):
        timeline = GES.Timeline()
        trackv = GES.Track.video_raw_new()
        layer = GES.TimelineLayer()
        self.pipeline = GES.TimelinePipeline()
        self.pipeline.add_timeline(timeline)

        timeline.add_track(trackv)
        timeline.add_layer(layer)

        mfs = GES.Material.new(GES.TimelineFileSource, self.discoveredCb, None, "file:///home/mathieu/Videos/youtube-dl/15261921.mp4")
        self.layer = layer
        #src = GES.TimelineFileSource(uri="file:///home/mathieu/Videos/youtube-dl/15261921.mp4")
        #src.set_start(long(0))
        #src.set_duration(long(10 * Gst.SECOND))
        #layer.add_object(src)

    def discoveredCb(self, material, result, blop):
        print material, result, blop
        #src = material.extract()
        #src.set_start(long(0))
        #src.set_duration(long (10 * Gst.SECOND))
        #self.layer.add_object(src)
        print self.layer.add_material(material, long(0), long(0), long(10 * Gst.SECOND), 1.0, GES.TrackType.VIDEO)
        self.start()

    def start(self):
        self.pipeline.set_state(Gst.State.PLAYING)

if __name__ == "__main__":
    Gst.init([])
    GES.init()
    loop = GLib.MainLoop()
    widget = Simple()
    #widget.start()
    loop.run()
