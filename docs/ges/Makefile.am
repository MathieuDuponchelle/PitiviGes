IGNORE_HFILES = gesmarshal.h ges-internal.h
IGNORE_CFILES =

ALLHFILES = $(wildcard $(top_srcdir)/ges/*.h)
ALLCFILES = $(wildcard $(top_srcdir)/ges/*.c)

HFILES=$(filter-out $(IGNORE_HFILES), $(ALLHFILES))
CFILES=$(filter-out $(IGNORE_CFILES), $(ALLCFILES))

MARKDOWN_FILES = $(wildcard $(srcdir)/md/*.markdown)
MARKDOWN_INDEX = $(srcdir)/md//index.markdown

CLANG_OPTIONS = "-I$(top_srcdir) $(GST_PBUTILS_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS)  $(GIO_CFLAGS) $(GCOV_CFLAGS)"

GIR_FILE = $(top_srcdir)/ges/GES-@GST_API_VERSION@.gir

doc.stamp: $(HFILES) $(CFILES) $(MARKDOWN_FILES) $(GIR_FILE) Makefile.am
	$(AM_V_GEN) CLANG_OPTIONS=$(CLANG_OPTIONS) \
		hotdoc --filenames $(FILES) $(CFILES) $(HFILES) \
		-i $(MARKDOWN_INDEX) \
    -o html \
    gi-extension \
    --gir-file $(GIR_FILE) \
    --languages c python javascript
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
