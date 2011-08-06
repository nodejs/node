BUILDTYPE ?= Release

all: out/Makefile
	$(MAKE) -C out BUILDTYPE=$(BUILDTYPE)

out/Makefile: build/gyp
	build/gyp_uv -f make

build/gyp:
	svn co http://gyp.googlecode.com/svn/trunk@983 build/gyp

clean:
	rm -rf out

distclean:
	rm -rf out

test: all
	./out/$(BUILDTYPE)/run-tests

bench: all
	./out/$(BUILDTYPE)/run-benchmarks

.PHONY: all clean distclean test bench
