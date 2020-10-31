# This is only for jsoncpp developers/contributors.
# We use this to sign releases, generate documentation, etc.
VER?=$(shell cat version.txt)

default:
	@echo "VER=${VER}"
sign: jsoncpp-${VER}.tar.gz
	gpg --armor --detach-sign $<
	gpg --verify $<.asc
	# Then upload .asc to the release.
jsoncpp-%.tar.gz:
	curl https://github.com/open-source-parsers/jsoncpp/archive/$*.tar.gz -o $@
dox:
	python doxybuild.py --doxygen=$$(which doxygen) --in doc/web_doxyfile.in
	rsync -va -c --delete dist/doxygen/jsoncpp-api-html-${VER}/ ../jsoncpp-docs/doxygen/
	# Then 'git add -A' and 'git push' in jsoncpp-docs.
build:
	mkdir -p build/debug
	cd build/debug; cmake -DCMAKE_BUILD_TYPE=debug -DBUILD_SHARED_LIBS=ON -G "Unix Makefiles" ../..
	make -C build/debug

# Currently, this depends on include/json/version.h generated
# by cmake.
test-amalgamate:
	python2.7 amalgamate.py
	python3.4 amalgamate.py
	cd dist; gcc -I. -c jsoncpp.cpp

valgrind:
	valgrind --error-exitcode=42 --leak-check=full ./build/debug/src/test_lib_json/jsoncpp_test

clean:
	\rm -rf *.gz *.asc dist/

.PHONY: build
