test:
	py.test

develop:
	pip install --editable .

tox-test:
	@tox

release:
	python scripts/make-release.py

upload-docs:
	$(MAKE) -C docs html dirhtml latex
	$(MAKE) -C docs/_build/latex all-pdf
	cd docs/_build/; mv html jinja-docs; zip -r jinja-docs.zip jinja-docs; mv jinja-docs html
	scp -r docs/_build/dirhtml/* flow.srv.pocoo.org:/srv/websites/jinja.pocoo.org/docs/
	scp -r docs/_build/latex/Jinja2.pdf flow.srv.pocoo.org:/srv/websites/jinja.pocoo.org/docs/jinja-docs.pdf
	scp -r docs/_build/jinja-docs.zip flow.srv.pocoo.org:/srv/websites/jinja.pocoo.org/docs/

.PHONY: test
