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
	save_and_load_timeline \
	$(graphical)


AM_CFLAGS =  -I$(top_srcdir) $(GST_PBUTILS_CFLAGS) $(GST_CFLAGS) $(GTK_CFLAGS)
AM_LDFLAGS = -export-dynamic
LDADD = $(top_builddir)/ges/libges-@GST_API_VERSION@.la $(GST_PBUTILS_LIBS) $(GST_LIBS) $(GTK_LIBS) -L.libs -lexamples

example_noinst_libraries=libexamples.la
example_noinst_headers=examples.h

libexamples_la_LIBADD= $(top_builddir)/ges/libges-@GST_API_VERSION@.la $(GST_PBUTILS_LIBS) $(GST_LIBS)
libexamples_la_CFLAGS=-I$(top_srcdir) $(GST_PBUTILS_CFLAGS) $(GST_CFLAGS)
libexamples_la_SOURCES= \
           add_one_video_track.c \
	   add_one_clip_in_each_layer.c \
	   add_one_clip_in_each_layer.sh \
	   create_timeline_with_n_layers.c \
	   play_timeline.c

noinst_LTLIBRARIES=$(example_noinst_libraries)
noinst_HEADERS=$(example_noinst_headers)
