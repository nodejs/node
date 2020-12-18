# vim: set softtabstop=2 shiftwidth=2:
SHELL = bash

PUBLISHTAG = $(shell node scripts/publish-tag.js)
BRANCH = $(shell git rev-parse --abbrev-ref HEAD)

markdowns = $(shell find docs -name '*.md' | grep -v 'index')

# these docs have the @VERSION@ tag in them, so they have to be rebuilt
# whenever the package.json is touched, in case the version changed.
version_mandocs = $(shell grep -rl '@VERSION@' docs/content \
									|sed 's|.md|.1|g' \
									|sed 's|docs/content/commands/|man/man1/|g' )

cli_mandocs = $(shell find docs/content/commands -name '*.md' \
               |sed 's|.md|.1|g' \
               |sed 's|docs/content/commands/|man/man1/|g' )

files_mandocs = $(shell find docs/content/configuring-npm -name '*.md' \
               |sed 's|.md|.5|g' \
               |sed 's|docs/content/configuring-npm/|man/man5/|g' ) \

misc_mandocs = $(shell find docs/content/using-npm -name '*.md' \
               |sed 's|.md|.7|g' \
               |sed 's|docs/content/using-npm/|man/man7/|g' ) \

mandocs = $(cli_mandocs) $(files_mandocs) $(misc_mandocs)

all: docs

docs: mandocs htmldocs

mandocs: dev-deps $(mandocs)

$(version_mandocs): package.json

htmldocs: dev-deps
	node bin/npm-cli.js rebuild
	cd docs && node dockhand.js >&2

clean: docs-clean gitclean

docsclean: docs-clean

docs-clean:
	rm -rf man

## build-time dependencies for the documentation
dev-deps:
	node bin/npm-cli.js install --only=dev --no-audit --ignore-scripts

## targets for man files, these are encouraged to be only built by running `make docs` or `make mandocs`
man/man1/%.1: docs/content/commands/%.md scripts/docs-build.js
	@[ -d man/man1 ] || mkdir -p man/man1
	node scripts/docs-build.js $< $@

man/man5/npm-json.5: man/man5/package.json.5
	cp $< $@

man/man5/npm-global.5: man/man5/folders.5
	cp $< $@

man/man5/%.5: docs/content/configuring-npm/%.md scripts/docs-build.js
	@[ -d man/man5 ] || mkdir -p man/man5
	node scripts/docs-build.js $< $@

man/man7/%.7: docs/content/using-npm/%.md scripts/docs-build.js
	@[ -d man/man7 ] || mkdir -p man/man7
	node scripts/docs-build.js $< $@

test: dev-deps
	node bin/npm-cli.js test

ls-ok:
	node . ls --production >/dev/null

gitclean:
	git clean -fd

uninstall:
	node bin/npm-cli.js rm -g -f npm

link: uninstall
	node bin/npm-cli.js link -f --ignore-scripts

prune:
	node bin/npm-cli.js prune --production --no-save --no-audit
	@[[ "$(shell git status -s)" != "" ]] && echo "ERR: found unpruned files" && exit 1 || echo "git status is clean"


publish: gitclean ls-ok link test docs prune
	@git push origin :v$(shell node bin/npm-cli.js --no-timing -v) 2>&1 || true
	git push origin $(BRANCH) &&\
	git push origin --tags &&\
	node bin/npm-cli.js publish --tag=$(PUBLISHTAG)

release: gitclean ls-ok docs prune
	@bash scripts/release.sh

.PHONY: all latest install dev link docs clean uninstall test man docs-clean docsclean release ls-ok dev-deps prune
