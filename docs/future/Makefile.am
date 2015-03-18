DOCTOOL_OPTIONS = --link-to-gtk-doc --online -M $(top_srcdir) --sections-file ges.sections.txt

PAGES_DIRECTORY = pages
PYTHON_PAGES_DIRECTORY = python_pages
BUILT_MARKDOWN_DIRECTORY = built_markdown
HTML_DIRECTORY = html

DOCTOOL = g-ir-doc-tool

EXTRA_DIST = mallard_to_markdown.py

if BUILD_YELP_HTML
DOCS = html.stamp
else
DOCS =
endif

if ENABLE_MARKDOWN
DOCS += markdown.stamp
endif

all-local: $(DOCS)

clean-local:
	@echo "Cleaning up documentation ..."
	@rm -rf $(PAGES_DIRECTORY) $(HTML_DIRECTORY) $(BUILT_MARKDOWN_DIRECTORY) html.stamp markdown.stamp pages.stamp
	@echo "Cleaned up documentation"

html.stamp: pages.stamp
	@touch html.tmp
	@echo "Building html documentation ..."
	@mkdir -p html && cd html && \
	yelp-build html ../$(PAGES_DIRECTORY) && cd ..
	@mv -f html.tmp html.stamp
	@echo "Built html documentation"

markdown.stamp: pages.stamp $(top_srcdir)/docs/future/mallard_to_markdown.py $(top_srcdir)/docs/future/markdown/*
	@touch markdown.tmp
	@echo "Building markdown documentation ..."
	@$(PYTHON) $(top_srcdir)/docs/future/mallard_to_markdown.py $(PAGES_DIRECTORY)/* -o $(BUILT_MARKDOWN_DIRECTORY) --python-pages $(PYTHON_PAGES_DIRECTORY)
	@cp $(top_srcdir)/docs/future/markdown/* $(BUILT_MARKDOWN_DIRECTORY)
	@mkdir -p slate/source/includes
	@cp $(BUILT_MARKDOWN_DIRECTORY)/*.markdown slate/source/includes/
	@cp $(BUILT_MARKDOWN_DIRECTORY)/index.md slate/source/
	@mv -f markdown.tmp markdown.stamp
	@echo "Built markdown documentation ..."

pages.stamp: $(top_builddir)/ges/GES-1.0.gir
	@touch pages.tmp
	@echo "Generating documentation pages ..."
	@$(DOCTOOL) $(DOCTOOL_OPTIONS) $(top_builddir)/ges/GES-1.0.gir -o $(PAGES_DIRECTORY)
	@$(DOCTOOL) $(DOCTOOL_OPTIONS) $(top_builddir)/ges/GES-1.0.gir --language python -o $(PYTHON_PAGES_DIRECTORY)
	@echo "Generated documentation pages"
	@mv -f pages.tmp pages.stamp
