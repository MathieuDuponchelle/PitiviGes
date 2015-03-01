ASCIIDOC = asciidoc
ASCIIDOC_DOCBOOK = docbook
XMLTO = xmlto

TXT_TO_XML = $(ASCIIDOC) -b $(ASCIIDOC_DOCBOOK)

MAN5_TXT = ges-launch-1.0-timeline.txt

MAN5_MANUALS = ges-launch-1.0-timeline.5

MAN5_MARKDOWNS = ges-launch-1.0-timeline.md

TO_SLATE_CODE = sed "s/[ ]*{.c}/c/g;s/[ ]*{.python}/python/g;s/[ ]*{.shell}/shell/g" -i

man5_MANS = $(MAN5_MANUALS)

all-local: $(MAN5_MARKDOWNS)

EXTRA_DIST = $(MAN5_TXT) $(MAN5_MARKDOWNS)

%.md: %.txt
	$(TXT_TO_XML) -o $@.xml $< && \
	pandoc -f docbook -t markdown -s $@.xml -o $@ && \
	$(TO_SLATE_CODE) $@

%.5 : %.txt
	$(TXT_TO_XML) -d manpage -o $@.xml $< && \
	$(XMLTO) man $@.xml

CLEANFILES = *.5 *.xml *.md
