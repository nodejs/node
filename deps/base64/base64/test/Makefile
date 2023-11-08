CFLAGS += -std=c99 -O3 -Wall -Wextra -pedantic
ifdef OPENMP
  CFLAGS += -fopenmp
endif

TARGET := $(shell $(CC) -dumpmachine)
ifneq (, $(findstring darwin, $(TARGET)))
  BENCH_LDFLAGS=
else
  # default to linux, -lrt needed
  BENCH_LDFLAGS=-lrt
endif

.PHONY: clean test valgrind

test: clean test_base64 benchmark
	./test_base64
	./benchmark

valgrind: clean test_base64
	valgrind --error-exitcode=2 ./test_base64

test_base64: test_base64.c codec_supported.o ../lib/libbase64.o
	$(CC) $(CFLAGS) -o $@ $^

benchmark: benchmark.c codec_supported.o ../lib/libbase64.o
	$(CC) $(CFLAGS) -o $@ $^ $(BENCH_LDFLAGS)

../%:
	make -C .. $*

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f benchmark test_base64 *.o
