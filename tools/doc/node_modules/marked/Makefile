all:
	@cp lib/marked.js marked.js
	@uglifyjs --comments '/\*[^\0]+?Copyright[^\0]+?\*/' -o marked.min.js lib/marked.js

clean:
	@rm marked.js
	@rm marked.min.js

bench:
	@node test --bench

man/marked.1.txt:
	groff -man -Tascii man/marked.1 | col -b > man/marked.1.txt

.PHONY: clean all
