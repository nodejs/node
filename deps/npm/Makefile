# vim: set softtabstop=2 shiftwidth=2:
SHELL = bash

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

htmldocs = $(api_htmldocs) $(cli_htmldocs) $(files_htmldocs) $(misc_htmldocs)

all: doc

latest:
	@echo "Installing latest published npm"
	@echo "Use 'make install' or 'make link' to install the code"
	@echo "in this folder that you're looking at right now."
	node cli.js install -g -f npm

install: docclean all
	node cli.js install -g -f

# backwards compat
dev: install

link: uninstall
	node cli.js link -f

clean: ronnclean doc-clean uninstall
	rm -rf npmrc
	node cli.js cache clean

uninstall:
	node cli.js rm npm -g -f

doc: $(mandocs) $(htmldocs)

ronnclean:
	rm -rf node_modules/ronn node_modules/.bin/ronn .building_ronn

docclean: doc-clean
doc-clean:
	rm -rf \
    .building_ronn \
    html/doc \
    html/api \
    man

# use `npm install ronn` for this to work.
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

html/doc/api/%.html: doc/api/%.md $(html_docdeps)
	@[ -d html/doc/api ] || mkdir -p html/doc/api
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



ronn: node_modules/.bin/ronn

node_modules/.bin/ronn:
	node cli.js install ronn --no-global

doc: man

man: $(cli_docs) $(api_docs)

test: doc
	node cli.js test

publish: link doc
	@git push origin :v$(shell npm -v) 2>&1 || true
	@npm unpublish npm@$(shell npm -v) 2>&1 || true
	git clean -fd &&\
	git push origin &&\
	git push origin --tags &&\
	npm publish &&\
	make doc-publish &&\
	make zip-publish

docpublish: doc-publish
doc-publish: doc
	# legacy urls
	for f in $$(find html/doc/{cli,files,misc}/ -name '*.html'); do \
    j=$$(basename $$f | sed 's|^npm-||g'); \
    if ! [ -f html/doc/$$j ] && [ $$j != README.html ] && [ $$j != index.html ]; then \
      perl -pi -e 's/ href="\.\.\// href="/g' <$$f >html/doc/$$j; \
    fi; \
  done
	mkdir -p html/api
	for f in $$(find html/doc/api/ -name '*.html'); do \
    j=$$(basename $$f | sed 's|^npm-||g'); \
    perl -pi -e 's/ href="\.\.\// href="/g' <$$f >html/api/$$j; \
  done
	rsync -vazu --stats --no-implied-dirs --delete \
    html/doc/* \
    ../npm-www/doc
	rsync -vazu --stats --no-implied-dirs --delete \
    html/static/style.css \
    ../npm-www/static/
	#cleanup
	rm -rf html/api
	for f in html/doc/*.html; do \
    case $$f in \
      html/doc/README.html) continue ;; \
      html/doc/index.html) continue ;; \
      *) rm $$f ;; \
    esac; \
  done

zip-publish: release
	scp release/* node@nodejs.org:dist/npm/

release:
	@bash scripts/release.sh

sandwich:
	@[ $$(whoami) = "root" ] && (echo "ok"; echo "ham" > sandwich) || (echo "make it yourself" && exit 13)

.PHONY: all latest install dev link doc clean uninstall test man doc-publish doc-clean docclean docpublish release zip-publish
