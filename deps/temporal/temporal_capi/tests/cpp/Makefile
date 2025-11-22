.DEFAULT_GOAL := test
.PHONY: build test
FORCE:

HEADERS := ../../bindings/cpp/
ALL_HEADERS := $(wildcard ${HEADERS}/**/*.hpp)

CXX?=g++

TEST_FILES := $(wildcard *.cpp)
OUT_FILES = $(patsubst %.cpp,%.out,$(TEST_FILES))

$(ALL_HEADERS):

../../../target/debug/libtemporal_capi.a: FORCE
	cargo rustc -p temporal_capi --crate-type staticlib

%.out: %.cpp ../../../target/debug/libtemporal_capi.a $(ALL_HEADERS)
	$(CXX) -std=c++17 -L ../../../target/debug/ -I ${HEADERS} $< -ltemporal_capi -lm -o $@
	./$@

test: $(OUT_FILES)

clean:
	rm $(OUT_FILES)
