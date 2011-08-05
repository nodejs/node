BUILDTYPE ?= Debug

all: out/Makefile
	$(MAKE) -C out BUILDTYPE=$(BUILDTYPE)

out/Makefile:
	tools/gyp_node -f make

distclean:
	rm -rf out

.PHONY: all distclean
