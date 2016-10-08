man/marked-man.1: README.md
	./bin/marked-man -o $@ $<

doc: man/marked-man.1

check:
	node test/compare.js

