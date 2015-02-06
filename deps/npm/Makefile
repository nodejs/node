# vim: set softtabstop=2 shiftwidth=2:
SHELL = bash

PUBLISHTAG = $(shell node scripts/publish-tag.js)
BRANCH = $(shell git rev-parse --abbrev-ref HEAD)

markdowns = $(shell find doc -name '*.md' | grep -v 'index') README.md

html_docdeps = html/dochead.html \
               html/docfoot.html \
               scripts/doc-build.sh \
               package.json

cli_mandocs = $(shell find doc/cli -name '*.md' \
               |sed 's|.md|.1|g' \
               |sed 's|doc/cli/|man/man1/|g' ) \
               man/man1/npm-README.1

api_mandocs = $(shell find doc/api -name '*.md' \
               |sed 's|.md|.3|g' \
               |sed 's|doc/api/|man/man3/|g' )

files_mandocs = $(shell find doc/files -name '*.md' \
               |sed 's|.md|.5|g' \
               |sed 's|doc/files/|man/man5/|g' ) \
               man/man5/npm-json.5 \
               man/man5/npm-global.5

misc_mandocs = $(shell find doc/misc -name '*.md' \
               |sed 's|.md|.7|g' \
               |sed 's|doc/misc/|man/man7/|g' ) \
               man/man7/npm-index.7


cli_partdocs = $(shell find doc/cli -name '*.md' \
                |sed 's|.md|.html|g' \
                |sed 's|doc/cli/|html/partial/doc/cli/|g' ) \
                html/partial/doc/README.html

api_partdocs = $(shell find doc/api -name '*.md' \
                |sed 's|.md|.html|g' \
                |sed 's|doc/api/|html/partial/doc/api/|g' )

files_partdocs = $(shell find doc/files -name '*.md' \
                  |sed 's|.md|.html|g' \
                  |sed 's|doc/files/|html/partial/doc/files/|g' ) \
                  html/partial/doc/files/npm-json.html \
                  html/partial/doc/files/npm-global.html

misc_partdocs = $(shell find doc/misc -name '*.md' \
                 |sed 's|.md|.html|g' \
                 |sed 's|doc/misc/|html/partial/doc/misc/|g' ) \
                 html/partial/doc/index.html


cli_htmldocs = $(shell find doc/cli -name '*.md' \
                |sed 's|.md|.html|g' \
                |sed 's|doc/cli/|html/doc/cli/|g' ) \
                html/doc/README.html

api_htmldocs = $(shell find doc/api -name '*.md' \
                |sed 's|.md|.html|g' \
                |sed 's|doc/api/|html/doc/api/|g' )

files_htmldocs = $(shell find doc/files -name '*.md' \
                  |sed 's|.md|.html|g' \
                  |sed 's|doc/files/|html/doc/files/|g' ) \
                  html/doc/files/npm-json.html \
                  html/doc/files/npm-global.html

misc_htmldocs = $(shell find doc/misc -name '*.md' \
                 |sed 's|.md|.html|g' \
                 |sed 's|doc/misc/|html/doc/misc/|g' ) \
                 html/doc/index.html

mandocs = $(api_mandocs) $(cli_mandocs) $(files_mandocs) $(misc_mandocs)

partdocs = $(api_partdocs) $(cli_partdocs) $(files_partdocs) $(misc_partdocs)

htmldocs = $(api_htmldocs) $(cli_htmldocs) $(files_htmldocs) $(misc_htmldocs)

all: doc

latest:
	@echo "Installing latest published npm"
	@echo "Use 'make install' or 'make link' to install the code"
	@echo "in this folder that you're looking at right now."
	node cli.js install -g -f npm ${NPMOPTS}

install: all
	node cli.js install -g -f ${NPMOPTS}

# backwards compat
dev: install

link: uninstall
	node cli.js link -f

clean: markedclean marked-manclean doc-clean uninstall
	rm -rf npmrc
	node cli.js cache clean

uninstall:
	node cli.js rm npm -g -f

doc: $(mandocs) $(htmldocs) $(partdocs)

markedclean:
	rm -rf node_modules/marked node_modules/.bin/marked .building_marked

marked-manclean:
	rm -rf node_modules/marked-man node_modules/.bin/marked-man .building_marked-man

docclean: doc-clean
doc-clean:
	rm -rf \
    .building_marked \
    .building_marked-man \
    html/doc \
    html/api \
    man

# use `npm install marked-man` for this to work.
man/man1/npm-README.1: README.md scripts/doc-build.sh package.json
	@[ -d man/man1 ] || mkdir -p man/man1
	scripts/doc-build.sh $< $@

man/man1/%.1: doc/cli/%.md scripts/doc-build.sh package.json
	@[ -d man/man1 ] || mkdir -p man/man1
	scripts/doc-build.sh $< $@

man/man3/%.3: doc/api/%.md scripts/doc-build.sh package.json
	@[ -d man/man3 ] || mkdir -p man/man3
	scripts/doc-build.sh $< $@

man/man5/npm-json.5: man/man5/package.json.5
	cp $< $@

man/man5/npm-global.5: man/man5/npm-folders.5
	cp $< $@

man/man5/%.5: doc/files/%.md scripts/doc-build.sh package.json
	@[ -d man/man5 ] || mkdir -p man/man5
	scripts/doc-build.sh $< $@

man/man7/%.7: doc/misc/%.md scripts/doc-build.sh package.json
	@[ -d man/man7 ] || mkdir -p man/man7
	scripts/doc-build.sh $< $@


doc/misc/npm-index.md: scripts/index-build.js package.json
	node scripts/index-build.js > $@


# html/doc depends on html/partial/doc
html/doc/%.html: html/partial/doc/%.html
	@[ -d html/doc ] || mkdir -p html/doc
	scripts/doc-build.sh $< $@

html/doc/README.html: html/partial/doc/README.html
	@[ -d html/doc ] || mkdir -p html/doc
	scripts/doc-build.sh $< $@

html/doc/cli/%.html: html/partial/doc/cli/%.html
	@[ -d html/doc/cli ] || mkdir -p html/doc/cli
	scripts/doc-build.sh $< $@

html/doc/misc/%.html: html/partial/doc/misc/%.html
	@[ -d html/doc/misc ] || mkdir -p html/doc/misc
	scripts/doc-build.sh $< $@

html/doc/files/%.html: html/partial/doc/files/%.html
	@[ -d html/doc/files ] || mkdir -p html/doc/files
	scripts/doc-build.sh $< $@

html/doc/api/%.html: html/partial/doc/api/%.html
	@[ -d html/doc/api ] || mkdir -p html/doc/api
	scripts/doc-build.sh $< $@


html/partial/doc/index.html: doc/misc/npm-index.md $(html_docdeps)
	@[ -d html/partial/doc ] || mkdir -p html/partial/doc
	scripts/doc-build.sh $< $@

html/partial/doc/README.html: README.md $(html_docdeps)
	@[ -d html/partial/doc ] || mkdir -p html/partial/doc
	scripts/doc-build.sh $< $@

html/partial/doc/cli/%.html: doc/cli/%.md $(html_docdeps)
	@[ -d html/partial/doc/cli ] || mkdir -p html/partial/doc/cli
	scripts/doc-build.sh $< $@

html/partial/doc/api/%.html: doc/api/%.md $(html_docdeps)
	@[ -d html/partial/doc/api ] || mkdir -p html/partial/doc/api
	scripts/doc-build.sh $< $@

html/partial/doc/files/npm-json.html: html/partial/doc/files/package.json.html
	cp $< $@
html/partial/doc/files/npm-global.html: html/partial/doc/files/npm-folders.html
	cp $< $@

html/partial/doc/files/%.html: doc/files/%.md $(html_docdeps)
	@[ -d html/partial/doc/files ] || mkdir -p html/partial/doc/files
	scripts/doc-build.sh $< $@

html/partial/doc/misc/%.html: doc/misc/%.md $(html_docdeps)
	@[ -d html/partial/doc/misc ] || mkdir -p html/partial/doc/misc
	scripts/doc-build.sh $< $@




marked: node_modules/.bin/marked

node_modules/.bin/marked:
	node cli.js install marked --no-global

marked-man: node_modules/.bin/marked-man

node_modules/.bin/marked-man:
	node cli.js install marked-man --no-global

doc: man

man: $(cli_docs) $(api_docs)

test: doc
	node cli.js test

tag:
	npm tag npm@$(PUBLISHTAG) latest

authors:
	@bash scripts/update-authors.sh &&\
	git add AUTHORS &&\
	git commit -m "update AUTHORS" || true

publish: link doc authors
	@git push origin :v$(shell npm -v) 2>&1 || true
	git clean -fd &&\
	git push origin $(BRANCH) &&\
	git push origin --tags &&\
	npm publish --tag=$(PUBLISHTAG)

release:
	@bash scripts/release.sh

sandwich:
	@[ $$(whoami) = "root" ] && (echo "ok"; echo "ham" > sandwich) || (echo "make it yourself" && exit 13)

.PHONY: all latest install dev link doc clean uninstall test man doc-clean docclean release authors
