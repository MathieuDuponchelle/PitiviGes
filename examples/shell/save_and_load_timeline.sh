#!/bin/sh

# The save-only switch means the timeline won't be played
# Use --save if you want it to be.
ges-launch-1.0 +test-clip snow duration=5 set-mute true --save-only example.xges
ges-launch-1.0 --load example.xges
