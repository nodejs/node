BUILD?=build
VERBOSE?=0
PARALLEL_JOBS?=1
CMAKE?=cmake

all: doc package

$(BUILD)/Makefile:
	mkdir $(BUILD) || exit 0
	cd $(BUILD) && $(CMAKE) -DCMAKE_VERBOSE_MAKEFILE=$(VERBOSE) ..

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

.PHONY: build install clean doc package test cdash
