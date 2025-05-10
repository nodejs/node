#
# To run the demos when linked with a shared library (default) ensure that
# libcrypto and libssl are on the library path. For example:
#
#    LD_LIBRARY_PATH=../.. ./server-arg

TESTS = client-arg \
        client-conf \
        saccept \
        sconnect \
        server-arg \
        server-cmod \
        server-conf

CFLAGS  = -I../../include -g -Wall
LDFLAGS = -L../..
LDLIBS  = -lssl -lcrypto

all: $(TESTS)

client-arg: client-arg.o
client-conf: client-conf.o
saccept: saccept.o
sconnect: sconnect.o
server-arg: server-arg.o
server-cmod: server-cmod.o
server-conf: server-conf.o

$(TESTS):
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

clean:
	$(RM) $(TESTS) *.o

test: all
	@echo "\nBIO tests:"
	@echo "skipped"
