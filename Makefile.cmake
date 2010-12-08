BUILD?=build
VERBOSE?=0
PARALLEL_JOBS?=1
CMAKE?=cmake
TOOLCHAIN_FILE=#./cmake/codesourcery-arm-toolchain.cmake

all: package

$(BUILD)/Makefile:
	mkdir $(BUILD) || exit 0
	cd $(BUILD) && $(CMAKE) -DCMAKE_VERBOSE_MAKEFILE=$(VERBOSE) -DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN_FILE) ..

build: $(BUILD)/Makefile
	cd $(BUILD) && make -j $(PARALLEL_JOBS)

install: build
	cd $(BUILD) && sudo make install

clean:
	rm -rf $(BUILD)

doc: $(BUILD)/Makefile
	cd $(BUILD) && make doc

package: $(BUILD)/Makefile
	cd $(BUILD) && make package

test: $(BUILD)/Makefile
	cd $(BUILD) && make test

cdash: $(BUILD)/Makefile
	cd $(BUILD) && make Experimental

cdash-cov: $(BUILD)/Makefile
	cd $(BUILD) && $(CMAKE) -DUSE_GCOV=True .. && make Experimental

cdash-mem: $(BUILD)/Makefile
	cd $(BUILD) && make NightlyMemoryCheck

.PHONY: build install clean doc package test cdash cdash-cov cdash-mem
