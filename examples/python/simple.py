# GStreamer
#
# Copyright (C) 2013 Thibault Saunier <tsaunier@gnome.org
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
# Boston, MA 02110-1335, USA.

import os
from gi.repository import Gst, GES, GLib


class Simple:
    def __init__(self, uri):
        timeline = GES.Timeline.new_audio_video()
        self.project = timeline.get_asset()

        self.project.connect("asset-added", self._asset_added_cb)
        self.project.connect("error-loading-asset", self._error_loading_asset_cb)
        self.project.create_asset(uri, GES.UriClip)
        self.layer = timeline.append_layer()
        self._create_pipeline(timeline)
        self.loop = GLib.MainLoop()

    def _create_pipeline(self, timeline):
        self.pipeline = GES.Pipeline()
        self.pipeline.set_timeline(timeline)
        bus = self.pipeline.get_bus()
        bus.add_signal_watch()
        bus.connect("message", self.bus_message_cb)

    def bus_message_cb(self, unused_bus, message):
        if message.type == Gst.MessageType.EOS:
            print "eos"
            self.loop.quit()
        elif message.type == Gst.MessageType.ERROR:
            error = message.parse_error()
            print "error %s" % error[1]
            self.loop.quit()

    def start(self):
        self.loop.run()

    def _asset_added_cb(self, project, asset):
        self.layer.add_asset(asset, 0, 0, Gst.SECOND * 5, GES.TrackType.UNKNOWN)
        self.pipeline.set_state(Gst.State.PLAYING)

    def _error_loading_asset_cb(self, project, error, asset_id, type):
        print "Could not load asset %s: %s" % (asset_id, error)
        self.loop.quit()

if __name__ == "__main__":
    if len(os.sys.argv) != 2:
        print "You must specify a file URI"
        exit(-1)

    Gst.init(None)
    GES.init()
    simple = Simple(os.sys.argv[1])
    simple.start()
