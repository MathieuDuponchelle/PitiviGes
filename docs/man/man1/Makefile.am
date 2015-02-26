ASCIIDOC = asciidoc
ASCIIDOC_DOCBOOK = docbook
XMLTO = xmlto

TXT_TO_XML = $(ASCIIDOC) -b $(ASCIIDOC_DOCBOOK)

man1_MANS = ges-launch-1.0.1 \
	   ges-launch-1.0-clip.1 \
	   ges-launch-1.0-effect.1 \
	   ges-launch-1.0-help.1 \
	   ges-launch-1.0-set-.1

EXTRA_DIST = ges-launch-1.0.txt \
	   ges-launch-1.0-clip.txt \
	   ges-launch-1.0-effect.txt \
	   ges-launch-1.0-help.txt \
	   ges-launch-1.0-set-.txt

%.1 : %.txt
	$(TXT_TO_XML) -d manpage -o $@ $< && \
	$(XMLTO) man $@

CLEANFILES = *.1
