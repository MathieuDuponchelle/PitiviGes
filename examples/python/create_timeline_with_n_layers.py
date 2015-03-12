from gi.repository import GES

def create_timeline_with_n_layers(n_layers):
    timeline = GES.Timeline.new()

    while n_layers > 0:
        timeline.append_layer()
        n_layers -= 1

    return timeline
