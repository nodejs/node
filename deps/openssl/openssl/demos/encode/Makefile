#
# To run the demos when linked with a shared library (default) ensure
# that libcrypto is on the library path.  For example:
#
#    LD_LIBRARY_PATH=../.. ./rsa_encode

TESTS = ec_encode \
        rsa_encode

CFLAGS  = -I../../include -g -Wall
LDFLAGS = -L../..
LDLIBS  = -lcrypto

all: $(TESTS)

ec_encode: ec_encode.o
rsa_encode: rsa_encode.o

$(TESTS):
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

clean:
	$(RM) *.o $(TESTS)

.PHONY: test
test: all
	@echo "\nencode tests:"
	@echo "skipped"
