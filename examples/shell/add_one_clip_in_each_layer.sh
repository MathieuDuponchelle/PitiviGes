#!/bin/sh

# With ges-launch-1.0, audio and video tracks will be
# automatically added
ges-launch-1.0 +test-clip snow duration=5 layer=0 \
	       +test-clip smpte duration=10 layer=1
