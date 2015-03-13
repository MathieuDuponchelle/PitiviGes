SUBDIRS = python c shell

EXTRA_DIST=full_timeline_tutorial.markdown \
	   add_one_video_track.markdown \
	   add_one_clip_in_each_layer.markdown \
	   create_timeline_with_n_layers.markdown \
	   save_and_load_timeline.markdown

AM_CFLAGS =  -I$(top_srcdir) $(GST_PBUTILS_CFLAGS) $(GST_CFLAGS) $(GTK_CFLAGS)
AM_LDFLAGS = -export-dynamic
LDADD = $(top_builddir)/ges/libges-@GST_API_VERSION@.la $(GST_PBUTILS_LIBS) $(GST_LIBS) $(GTK_LIBS)
