DOCTOOL_OPTIONS = --link-to-gtk-doc --online -M $(top_srcdir) --sections-file ges.sections.txt

PAGES_DIRECTORY = pages
PYTHON_PAGES_DIRECTORY = python_pages
YELP_HTML_DIRECTORY = yelp_html
RAKE_HTML_DIRECTORY = rake_html
BUILT_MARKDOWN_DIRECTORY = built_markdown

DOCTOOL = g-ir-doc-tool

EXTRA_DIST = mallard_to_markdown.py

if BUILD_YELP_HTML
DOCS = yelp_html.stamp
else
DOCS =
endif

if BUILD_RAKE_HTML
DOCS += rake_html.stamp
endif

if ENABLE_MARKDOWN
DOCS += markdown.stamp
endif

all-local: $(DOCS)

clean-local:
	@echo "Cleaning up documentation ..."
	@rm -rf $(PAGES_DIRECTORY) $(HTML_DIRECTORY) $(BUILT_MARKDOWN_DIRECTORY) rake_html.stamp yelp_html.stamp markdown.stamp pages.stamp
	@echo "Cleaned up documentation"

yelp_html.stamp: pages.stamp
	@touch yelp_html.tmp
	@echo "Building yelp html documentation ..."
	@mkdir -p yelp_html && cd yelp_html && \
	yelp-build html ../$(PAGES_DIRECTORY) && cd ..
	@mv -f yelp_html.tmp yelp_html.stamp
	@echo "Built yelp html documentation"


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

rake_html.stamp: markdown.stamp
	@touch rake_html.tmp
	@echo "Building rake html documentation ..."
	@cd slate && rake build && cd .. && cp -r slate/build $(RAKE_HTML_DIRECTORY)
	@mv -f rake_html.tmp rake_html.stamp
	@echo "Built rake html documentation"

pages.stamp: $(top_builddir)/ges/GES-1.0.gir
	@touch pages.tmp
	@echo "Generating documentation pages ..."
	@$(DOCTOOL) $(DOCTOOL_OPTIONS) $(top_builddir)/ges/GES-1.0.gir -o $(PAGES_DIRECTORY)
	@$(DOCTOOL) $(DOCTOOL_OPTIONS) $(top_builddir)/ges/GES-1.0.gir --language python -o $(PYTHON_PAGES_DIRECTORY)
	@echo "Generated documentation pages"
	@mv -f pages.tmp pages.stamp
