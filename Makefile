BUILDTYPE ?= Debug

all: out/Makefile
	$(MAKE) -C out BUILDTYPE=$(BUILDTYPE)

out/Makefile:
	tools/gyp_node -f make

clean:
	rm -rf out

distclean:
	rm -rf out

.PHONY: all distclean
