        GObject
        ├── GInitiallyUnowned
        │   ├── GstObject
        │   │   ╰── GstElement
        │   │       ╰── GstBin
        │   │           ├── GstPipeline
        │   │           │   ╰── GESPipeline
        │   │           ├── GESTimeline
        │   │           ╰── GESTrack
        │   ├── GESFormatter
        │   ├── GESLayer
        │   ╰── GESTimelineElement
        │       ├── GESContainer
        │       │   ╰── GESClip
        │       │       ├── GESOperationClip
        │       │       │   ├── GESOverlayClip
        │       │       │   │   ╰── GESTextOverlayClip
        │       │       │   ├── GESBaseTransitionClip
        │       │       │   │   ╰── GESTransitionClip
        │       │       │   ╰── GESBaseEffectClip
        │       │       │       ╰── GESEffectClip
        │       │       ╰── GESSourceClip
        │       │           ├── GESTestClip
        │       │           ├── GESUriClip
        │       │           ╰── GESTitleClip
        │       ╰── GESTrackElement
        │           ├── GESSource
        │           │   ├── GESAudioSource
        │           │   │   ├── GESAudioTestSource
        │           │   │   ╰── GESAudioUriSource
        │           │   ╰── GESVideoSource
        │           │       ├── GESVideoUriSource
        │           │       ├── GESImageSource
        │           │       ├── GESMultiFileSource
        │           │       ├── GESTitleSource
        │           │       ╰── GESVideoTestSource
        │           ╰── GESOperation
        │               ├── GESTransition
        │               │   ├── GESAudioTransition
        │               │   ╰── GESVideoTransition
        │               ├── GESBaseEffect
        │               │   ╰── GESEffect
        │               ╰── GESTextOverlay
        ╰── GESAsset
            ├── GESProject
            ├── GESClipAsset
            │   ╰── GESUriClipAsset
            ╰── GESTrackElementAsset
                ╰── GESUriSourceAsset
        GInterface
        ├── GESExtractable
        ╰── GESMetaContainer
