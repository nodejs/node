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

files_mandocs = $(shell find doc/files -name '*.md' \
               |sed 's|.md|.5|g' \
               |sed 's|doc/files/|man/man5/|g' ) \
               man/man5/npm-json.5 \
               man/man5/npm-global.5

misc_mandocs = $(shell find doc/misc -name '*.md' \
               |sed 's|.md|.7|g' \
               |sed 's|doc/misc/|man/man7/|g' ) \
               man/man7/npm-index.7

cli_htmldocs = $(shell find doc/cli -name '*.md' \
                |sed 's|.md|.html|g' \
                |sed 's|doc/cli/|html/doc/cli/|g' ) \
                html/doc/README.html

files_htmldocs = $(shell find doc/files -name '*.md' \
                  |sed 's|.md|.html|g' \
                  |sed 's|doc/files/|html/doc/files/|g' ) \
                  html/doc/files/npm-json.html \
                  html/doc/files/npm-global.html

misc_htmldocs = $(shell find doc/misc -name '*.md' \
                 |sed 's|.md|.html|g' \
                 |sed 's|doc/misc/|html/doc/misc/|g' ) \
                 html/doc/index.html

mandocs = $(cli_mandocs) $(files_mandocs) $(misc_mandocs)

htmldocs = $(cli_htmldocs) $(files_htmldocs) $(misc_htmldocs)

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

doc: $(mandocs) $(htmldocs)

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
    man

# use `npm install marked-man` for this to work.
man/man1/npm-README.1: README.md scripts/doc-build.sh package.json
	@[ -d man/man1 ] || mkdir -p man/man1
	scripts/doc-build.sh $< $@

man/man1/%.1: doc/cli/%.md scripts/doc-build.sh package.json
	@[ -d man/man1 ] || mkdir -p man/man1
	scripts/doc-build.sh $< $@

man/man5/npm-json.5: man/man5/package.json.5
	cp $< $@

man/man5/npm-global.5: man/man5/npm-folders.5
	cp $< $@

man/man5/%.5: doc/files/%.md scripts/doc-build.sh package.json
	@[ -d man/man5 ] || mkdir -p man/man5
	scripts/doc-build.sh $< $@

doc/misc/npm-index.md: scripts/index-build.js package.json
	node scripts/index-build.js > $@

html/doc/index.html: doc/misc/npm-index.md $(html_docdeps)
	@[ -d html/doc ] || mkdir -p html/doc
	scripts/doc-build.sh $< $@

man/man7/%.7: doc/misc/%.md scripts/doc-build.sh package.json
	@[ -d man/man7 ] || mkdir -p man/man7
	scripts/doc-build.sh $< $@

html/doc/README.html: README.md $(html_docdeps)
	@[ -d html/doc ] || mkdir -p html/doc
	scripts/doc-build.sh $< $@

html/doc/cli/%.html: doc/cli/%.md $(html_docdeps)
	@[ -d html/doc/cli ] || mkdir -p html/doc/cli
	scripts/doc-build.sh $< $@

html/doc/files/npm-json.html: html/doc/files/package.json.html
	cp $< $@

html/doc/files/npm-global.html: html/doc/files/npm-folders.html
	cp $< $@

html/doc/files/%.html: doc/files/%.md $(html_docdeps)
	@[ -d html/doc/files ] || mkdir -p html/doc/files
	scripts/doc-build.sh $< $@

html/doc/misc/%.html: doc/misc/%.md $(html_docdeps)
	@[ -d html/doc/misc ] || mkdir -p html/doc/misc
	scripts/doc-build.sh $< $@


marked: node_modules/.bin/marked

node_modules/.bin/marked:
	node cli.js install marked --no-global

marked-man: node_modules/.bin/marked-man

node_modules/.bin/marked-man:
	node cli.js install marked-man --no-global

doc: man

man: $(cli_docs)

test: doc
	node cli.js test

tag:
	npm tag npm@$(PUBLISHTAG) latest

ls-ok:
	node . ls >/dev/null

gitclean:
	git clean -fd

publish: gitclean ls-ok link doc-clean doc
	@git push origin :v$(shell npm -v) 2>&1 || true
	git push origin $(BRANCH) &&\
	git push origin --tags &&\
	npm publish --tag=$(PUBLISHTAG)

release: gitclean ls-ok markedclean marked-manclean doc-clean doc
	node cli.js prune --production
	@bash scripts/release.sh

sandwich:
	@[ $$(whoami) = "root" ] && (echo "ok"; echo "ham" > sandwich) || (echo "make it yourself" && exit 13)

.PHONY: all latest install dev link doc clean uninstall test man doc-clean docclean release ls-ok realclean
