#
# To run the demos when linked with a shared library (default) ensure that
# libcrypto is on the library path. For example, to run the
# sm_enc demo:
#
#    LD_LIBRARY_PATH=../.. ./sms_enc

TESTS = smenc \
        smdec \
        smsign \
        smsign2 \
        smver

CFLAGS  = -I../../include -g -Wall
LDFLAGS = -L../..
LDLIBS  = -lcrypto

all: $(TESTS)

smenc: smenc.o
smdec: smdec.o
smsign: smsign.o
smsign2: smsign2.o
smver: smver.o

$(TESTS):
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

clean:
	$(RM) $(TESTS) *.o

test: all
	@echo "\nS/MIME tests:"
	LD_LIBRARY_PATH=../.. ./smenc
	LD_LIBRARY_PATH=../.. ./smdec
	LD_LIBRARY_PATH=../.. ./smsign2
	LD_LIBRARY_PATH=../.. ./smver
