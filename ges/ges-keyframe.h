#ifndef _GES_KEYFRAME
#define _GES_KEYFRAME

#define GES_KEYFRAME ges_keyframe_get_type()

#define GES_TYPE_KEYFRAME(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GES_TYPE_KEYFRAME, GESKeyframe))

#define GES_KEYFRAME_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GES_TYPE_KEYFRAME, GESPitiviKeyframeClass))

#define GES_IS_KEYFRAME(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GES_TYPE_KEYFRAME))

#define GES_IS_KEYFRAME_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GES_TYPE_KEYFRAME))

#define GES_KEYFRAME_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GES_TYPE_KEYFRAME, GESKeyframeClass))

typedef struct _GESKeyframePrivate GESKeyframePrivate;

/**
 * GESKeyframe:
 *
 * Creates a keyframe for interpolation purposes
 */
struct _GESKeyframe {
  GObject	parent;

  /*< private >*/
  GESKeyframePrivate *priv;

  /* Padding for API extension */
  gpointer _ges_reserved[GES_PADDING];
};

struct _GESKeyframeClass {
  /*< private >*/
  GESFormatterClass parent_class;

  /* Padding for API extension */
  gpointer _ges_reserved[GES_PADDING];
};

GType ges_keyframe_get_type (void);

GESKeyframe *ges_keyframe_new (void);

#endif
