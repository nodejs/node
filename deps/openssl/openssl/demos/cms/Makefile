#
# To run the demos when linked with a shared library (default) ensure that
# libcrypto is on the library path. For example, to run the
# cms_enc demo:
#
#    LD_LIBRARY_PATH=../.. ./cms_enc

TESTS = cms_comp \
        cms_ddec \
        cms_dec \
        cms_denc \
        cms_enc \
        cms_sign \
        cms_sign2 \
        cms_uncomp \
        cms_ver

CFLAGS  = -I../../include -g -Wall
LDFLAGS = -L../..
LDLIBS  = -lcrypto

all: $(TESTS)

cms_comp: cms_comp.o
cms_ddec: cms_ddec.o
cms_dec: cms_dec.o
cms_denc: cms_denc.o
cms_enc: cms_enc.o
cms_sign: cms_sign.o
cms_sign2: cms_sign2.o
cms_uncomp: cms_uncomp.o
cms_ver: cms_ver.o

$(TESTS):
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

clean:
	$(RM) $(TESTS) *.o

test: all
	@echo "\nCMS tests:"
	LD_LIBRARY_PATH=../.. ./cms_enc
	LD_LIBRARY_PATH=../.. ./cms_dec
	LD_LIBRARY_PATH=../.. ./cms_sign2
	LD_LIBRARY_PATH=../.. ./cms_ver
