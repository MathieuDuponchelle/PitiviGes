if HAVE_GTK_X11
graphical=ges-ui
else
graphical=
endif

noinst_PROGRAMS = 	\
	concatenate	\
	simple1		\
	test1		\
	test2		\
	test3		\
	test4		\
	transition	\
	thumbnails	\
	overlays	\
	text_properties	\
	assets \
	multifilesrc \
	full_timeline_tutorial \
	$(graphical)

EXTRA_DIST=full_timeline_tutorial.markdown \
	   full_timeline_tutorial.py \
	   add_one_video_track.markdown \
	   add_one_video_track.c \
	   add_one_video_track.py \
	   add_one_clip_in_each_layer.markdown \
	   add_one_clip_in_each_layer.c \
	   add_one_clip_in_each_layer.py \
	   add_one_clip_in_each_layer.sh \
	   create_timeline_with_n_layers.markdown \
	   create_timeline_with_n_layers.c \
	   create_timeline_with_n_layers.py \
	   play_timeline.c \
	   play_timeline.py

AM_CFLAGS =  -I$(top_srcdir) $(GST_PBUTILS_CFLAGS) $(GST_CFLAGS) $(GTK_CFLAGS)
AM_LDFLAGS = -export-dynamic
LDADD = $(top_builddir)/ges/libges-@GST_API_VERSION@.la $(GST_PBUTILS_LIBS) $(GST_LIBS) $(GTK_LIBS)
