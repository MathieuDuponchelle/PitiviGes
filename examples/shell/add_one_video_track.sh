#!/bin/sh

# With ges-launch-1.0, we can restrict the types of tracks,
# but not actually add them.
ges-launch-1.0 +test-clip snow duration=5 layer=0 \
	       +test-clip smpte duration=10 layer=1 --track-types video
