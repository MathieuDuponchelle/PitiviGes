IGNORE_HFILES = $(top_srcdir)/ges/gesmarshal.h \
		$(top_srcdir)/ges/ges-internal.h \
		$(top_srcdir)/ges/gstframepositionner.h \
		$(top_srcdir)/ges/ges-auto-transition.h \
		$(top_srcdir)/ges/ges-structured-interface.h \
		$(top_srcdir)/ges/ges-structure-parser.h \
		$(top_srcdir)/ges/parse_lex.h \
		$(top_srcdir)/ges/ges-smart-video-mixer.h \
		$(top_srcdir)/ges/ges-smart-adder.h

IGNORE_CFILES =

ALLHFILES = $(wildcard $(top_srcdir)/ges/*.h)
ALLCFILES = $(wildcard $(top_srcdir)/ges/*.c)

HFILES=$(top_srcdir)/ges/ges.h $(filter-out $(IGNORE_HFILES), $(ALLHFILES))
CFILES=$(filter-out $(IGNORE_CFILES), $(ALLCFILES))

MARKDOWN_FILES = $(wildcard $(srcdir)/md/*.markdown)
MARKDOWN_INDEX = $(srcdir)/md//index.markdown

CLANG_OPTIONS = "-I$(top_srcdir) $(GST_PBUTILS_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS)  $(GIO_CFLAGS) $(GCOV_CFLAGS) -D_GNU_SOURCE -DHAVE_CONFIG_H"

GIR_FILE = $(top_srcdir)/ges/GES-@GST_API_VERSION@.gir

doc.stamp: $(HFILES) $(CFILES) $(MARKDOWN_FILES) $(GIR_FILE) Makefile.am
	$(AM_V_GEN) \
		hotdoc --c-sources $(HFILES) $(CFILES) \
		--clang-options=$(CLANG_OPTIONS) \
		-i $(MARKDOWN_INDEX) \
		-o html \
		gi-extension \
		--gir-file $(GIR_FILE) \
		--languages c python javascript \
		--major-version "1.0"
	@touch doc.stamp

EXTRA_DIST = \
             $(MARKDOWN_FILES)

CLEANFILES = \
           doc.stamp

clean-local:
	@rm -rf html

all-local: doc.stamp

GITIGNOREFILES = html

-include $(top_srcdir)/git.mk
