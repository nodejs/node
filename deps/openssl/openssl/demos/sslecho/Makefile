#
# To run the demos when linked with a shared library (default) ensure that
# libcrypto and libssl are on the library path. For example:
#
#    LD_LIBRARY_PATH=../.. ./sslecho

TESTS = sslecho

CFLAGS  = -I../../include -g -Wall
LDFLAGS = -L../..
LDLIBS  = -lssl -lcrypto

all: $(TESTS)

sslecho: main.o

$(TESTS):
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

clean:
	$(RM) $(TESTS) *.o

test: all
	@echo "\nSSL Echo tests:"
	@echo "skipped"
