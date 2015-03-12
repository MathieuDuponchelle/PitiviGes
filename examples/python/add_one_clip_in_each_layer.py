from gi.repository import GES

def add_one_clip_in_each_layer (timeline, duration):
    for layer in timeline.get_layers():
        clip = GES.TestClip.new()
        layer.add_clip (clip)
        clip.props.duration = duration
        clip.props.start = 0

    # We commit the timeline to have our changes taken into account
    timeline.commit()
