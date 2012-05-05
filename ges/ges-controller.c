#include "ges.h"
#include "ges-controller.h"

G_DEFINE_TYPE (GESController, ges_controller, G_TYPE_OBJECT);

struct _GESControllerPrivate
{
  void *data;
};

static void
ges_controller_class_init (GESControllerClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class = object_class;
  g_type_class_add_private (klass, sizeof (GESControllerPrivate));

}

static void
ges_controller_init (GESController * object)
{
  object = object;
}

GESController *
ges_controller_new (GESTrackObject * track_object)
{
  return g_object_new (GES_TYPE_KEYFILE_FORMATTER, NULL);
}
