# vim: set softtabstop=2 shiftwidth=2:
SHELL = bash

PUBLISHTAG = $(shell node scripts/publish-tag.js)
BRANCH = $(shell git rev-parse --abbrev-ref HEAD)

markdowns = $(shell find docs -name '*.md' | grep -v 'index') README.md

cli_mandocs = $(shell find docs/content/cli-commands -name '*.md' \
               |sed 's|.md|.1|g' \
               |sed 's|docs/content/cli-commands/|man/man1/|g' ) \
               man/man1/npm-README.1 \
               man/man1/npx.1

files_mandocs = $(shell find docs/content/configuring-npm -name '*.md' \
               |sed 's|.md|.5|g' \
               |sed 's|docs/content/configuring-npm/|man/man5/|g' ) \

misc_mandocs = $(shell find docs/content/using-npm -name '*.md' \
               |sed 's|.md|.7|g' \
               |sed 's|docs/content/using-npm/|man/man7/|g' ) \

mandocs = $(cli_mandocs) $(files_mandocs) $(misc_mandocs)

all: docs

latest:
	@echo "Installing latest published npm"
	@echo "Use 'make install' or 'make link' to install the code"
	@echo "in this folder that you're looking at right now."
	node bin/npm-cli.js install -g -f npm ${NPMOPTS}

install: all
	node bin/npm-cli.js install -g -f ${NPMOPTS} $(shell node bin/npm-cli.js pack | tail -1)

# backwards compat
dev: install

link: uninstall
	node bin/npm-cli.js link -f

clean: markedclean marked-manclean docs-clean
	rm -rf npmrc
	node bin/npm-cli.js cache clean --force

uninstall:
	node bin/npm-cli.js rm npm -g -f

mandocs: $(mandocs)

htmldocs:
	cd docs && node ../bin/npm-cli.js install && \
	node ../bin/npm-cli.js run build:static echo>&2 && \
	rm -rf node_modules .cache public/*js public/*json public/404* public/page-data public/manifest*

docs: mandocs htmldocs

markedclean:
	rm -rf node_modules/marked node_modules/.bin/marked .building_marked

marked-manclean:
	rm -rf node_modules/marked-man node_modules/.bin/marked-man .building_marked-man

docsclean: docs-clean
docs-clean:
	rm -rf \
    .building_marked \
    .building_marked-man \
    man \
    docs/node_modules \
    docs/public \
    docs/.cache

## build-time tools for the documentation
build-doc-tools := node_modules/.bin/marked \
                   node_modules/.bin/marked-man

# use `npm install marked-man` for this to work.
man/man1/npm-README.1: README.md scripts/docs-build.js package.json $(build-doc-tools)
	@[ -d man/man1 ] || mkdir -p man/man1
	node scripts/docs-build.js $< $@

man/man1/%.1: docs/content/cli-commands/%.md scripts/docs-build.js package.json $(build-doc-tools)
	@[ -d man/man1 ] || mkdir -p man/man1
	node scripts/docs-build.js $< $@

man/man1/npx.1: node_modules/libnpx/libnpx.1
	cat $< | sed s/libnpx/npx/ > $@

man/man5/npm-json.5: man/man5/package.json.5
	cp $< $@

man/man5/npm-global.5: man/man5/folders.5
	cp $< $@

man/man5/%.5: docs/content/configuring-npm/%.md scripts/docs-build.js package.json $(build-doc-tools)
	@[ -d man/man5 ] || mkdir -p man/man5
	node scripts/docs-build.js $< $@

man/man7/%.7: docs/content/using-npm/%.md scripts/docs-build.js package.json $(build-doc-tools)
	@[ -d man/man7 ] || mkdir -p man/man7
	node scripts/docs-build.js $< $@

marked: node_modules/.bin/marked

node_modules/.bin/marked:
	node bin/npm-cli.js install marked --no-global --no-timing --no-save

marked-man: node_modules/.bin/marked-man

node_modules/.bin/marked-man:
	node bin/npm-cli.js install marked-man --no-global --no-timing --no-save

test: docs
	node bin/npm-cli.js test

tag:
	node bin/npm-cli.js tag npm@$(PUBLISHTAG) latest

ls-ok:
	node . ls >/dev/null

gitclean:
	git clean -fd

publish: gitclean ls-ok link docs-clean docs
	@git push origin :v$(shell node bin/npm-cli.js --no-timing -v) 2>&1 || true
	git push origin $(BRANCH) &&\
	git push origin --tags &&\
	node bin/npm-cli.js publish --tag=$(PUBLISHTAG)

release: gitclean ls-ok markedclean marked-manclean docs-clean docs
	node bin/npm-cli.js prune --production --no-save
	@bash scripts/release.sh

sandwich:
	@[ $$(whoami) = "root" ] && (echo "ok"; echo "ham" > sandwich) || (echo "make it yourself" && exit 13)

.PHONY: all latest install dev link docs clean uninstall test man docs-clean docclean release ls-ok realclean
