
prep:
	@echo
	# Install needed packages
	sudo apt-get install subversion fakeroot python-setuptools python-subversion
	#
	@echo
	# Check that the person has .pypirc
	@if [ ! -e ~/.pypirc ]; then \
		echo "Please create a ~/.pypirc with the following contents:"; \
		echo "[server-login]"; \
		echo "username:google_opensource"; \
		echo "password:<see valentine>"; \
	fi
	#
	@echo
	# FIXME(tansell): Check that the person has .dputrc for PPA

clean:
	# Clean up any build files.
	python setup.py clean --all
	#
	# Clean up the debian stuff
	fakeroot ./debian/rules clean
	#
	# Clean up everything else
	rm MANIFEST || true
	rm -rf build-*
	#
	# Clean up the egg files
	rm -rf *egg*
	#
	# Remove dist
	rm -rf dist

dist:
	# Generate the tarball based on MANIFEST.in
	python setup.py sdist
	#
	# Build the debian packages
	fakeroot ./debian/rules binary
	mv ../python-gflags*.deb ./dist/
	#
	# Build the python Egg
	python setup.py bdist_egg
	#
	@echo
	@echo "Files to upload:"
	@echo "--------------------------"
	@ls -l ./dist/

push:
	# Send the updates to svn
	# Upload the source package to code.google.com
	- /home/build/opensource/tools/googlecode_upload.py \
		-p python-gflags ./dist/*
	#
	# Upload the package to PyPi
	- python setup.py sdist upload
	- python setup.py bdist_egg upload
	#
	# Upload the package to the ppa
	# FIXME(tansell): dput should run here

check:
	# Run all the tests.
	for test in tests/*.py; do PYTHONPATH=. python $$test || exit 1; done

.PHONY: prep dist clean push check
