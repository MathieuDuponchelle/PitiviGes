PAGES_DIRECTORY = pages
HTML_DIRECTORY = html

all-local: html.stamp
clean-local:
	@echo "Cleaning up documentation ..."
	@rm -rf $(PAGES_DIRECTORY) $(HTML_DIRECTORY) html.stamp
	@echo "Cleaned up documentation"

html.stamp: $(PAGES_DIRECTORY)
	@touch html.tmp
	@echo "Building html documentation ..."
	@mkdir -p html && cd html && \
	yelp-build html ../$(PAGES_DIRECTORY) && cd ..
	@mv -f html.tmp html.stamp
	@echo "Built html documentation"

$(PAGES_DIRECTORY): $(top_builddir)/ges/GES-1.0.gir
	@echo "Generating documentation pages ..."
	@g-ir-doc-tool $(top_builddir)/ges/GES-1.0.gir --language="C" -o $(PAGES_DIRECTORY)
	@echo "Generated documentation pages"
