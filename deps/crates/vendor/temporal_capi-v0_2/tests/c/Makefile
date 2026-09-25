.DEFAULT_GOAL := test
.PHONY: build test
FORCE:

HEADERS := ../../bindings/c/
ALL_HEADERS := $(wildcard ${HEADERS}/**/*.h)

C?=gcc

TEST_FILES := $(wildcard *.c)
OUT_FILES = $(patsubst %.c,%.out,$(TEST_FILES))

$(ALL_HEADERS):

../../../target/debug/libtemporal_capi.a: FORCE
	cargo rustc -p temporal_capi --crate-type staticlib

%.out: %.c ../../../target/debug/libtemporal_capi.a $(ALL_HEADERS)
	$(C) -L ../../../target/debug/ -I ${HEADERS} $< -ltemporal_capi -lm -o $@
	./$@

test: $(OUT_FILES)

clean:
	rm $(OUT_FILES)
