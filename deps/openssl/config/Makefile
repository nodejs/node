PERL    = perl
CONFIGURE = ./Configure
COPT = no-shared no-symlinks

ARCHS = BSD-x86 BSD-x86_64 VC-WIN32 VC-WIN64A darwin64-x86_64-cc  \
darwin-i386-cc linux-armv4 linux-elf linux-x86_64 solaris-x86-gcc \
solaris64-x86_64-gcc

CFG = opensslconf.h
SRC_CFG = ../openssl/crypto/$(CFG)
BACKUP_FILES = ../openssl/Makefile ../openssl/Makefile.bak $(SRC_CFG)
BACKUP_EXT = iojsbackup

# OPENSSL_CPUID_OBJ is defined in openssl.gypi for use --openssl-no-asm
CPUIDFIX = 's/\#define OPENSSL_CPUID_OBJ$$//;'

X32FIX = 's/\#define OPENSSL_CPUID_OBJ$$//;\
s/RC4_CHUNK unsigned long$$/RC4_CHUNK unsigned long long/;\
s/define SIXTY_FOUR_BIT_LONG$$/undef SIXTY_FOUR_BIT_LONG/;\
s/undef SIXTY_FOUR_BIT$$/define SIXTY_FOUR_BIT/;'

MACFIX ='s/define RC4_INT unsigned char$$/define RC4_INT unsigned int/;'

WINFIX = 'if(/ifndef OPENSSL_DOING_MAKEDEPEND$$/){\
print "\n\#ifndef OPENSSL_NO_DYNAMIC_ENGINE\n";\
print "\# define OPENSSL_NO_DYNAMIC_ENGINE\n";\
print "\#endif\n";\
print "\#ifndef OPENSSL_NO_CAPIENG\n";\
print "\# define OPENSSL_NO_CAPIENG\n";\
print "\#endif\n\n";}'

PHONY = all clean backup restore
.PHONY: $(PHONY)

all: backup $(ARCHS) linux-x32 cleanconf fixdarwin64 fixwin restore

$(ARCHS):
	cd ../openssl; $(PERL) $(CONFIGURE) $(COPT) $@ > /dev/null
	$(PERL) -p -e $(CPUIDFIX) $(SRC_CFG) > ./archs/$@/$(CFG)

# linux-x32 was made by comparing define values of opensslconf.h which
# was generated `Configure linux-x32' in openssl-1.0.2a
linux-x32:
	cd ../openssl; $(PERL) $(CONFIGURE) $(COPT) linux-x86_64 > /dev/null;
	$(PERL) -p -e $(X32FIX) $(SRC_CFG) > ./archs/$@/$(CFG)

# The current openssl release does not use RC4 asm since it explicitly
# specified as `$asm=~s/rc4\-[^:]+//;` in
# https://github.com/openssl/openssl/blob/OpenSSL_1_0_1-stable/Configure#L584
# But iojs has used RC4 asm on MacOS for long time. Fix type of RC4_INT
# into `unsigned int` in opensslconf.h of darwin64-x86_64-cc to work on
# the RC4 asm.
fixdarwin64:
	$(PERL) -pi -e $(MACFIX) ./archs/darwin64-x86_64-cc/$(CFG)


# OPENSSL_NO_DYNAMIC_ENGINE is needed for building static
# library. OPENSSL_NO_CAPIENG is needed to avoid build errors on
# Win. See the comments in `deps/openssl/openssl/engines/e_capi.c` for
# detail.
fixwin:
	$(PERL) -pi -e $(WINFIX) ./archs/VC-WIN32/$(CFG) ./archs/VC-WIN64A/$(CFG)

# backup files to avoid to be overwritten
backup:
	@for f in $(BACKUP_FILES); do \
	    mv $$f $$f.$(BACKUP_EXT); \
	done

restore:
	@for f in $(BACKUP_FILES); do \
	    mv $$f.$(BACKUP_EXT) $$f ; \
	done

# remove unnecessary files create by Configure
cleanconf:
	@rm ../openssl/crypto/$(CFG)
	@rm ../openssl/Makefile
	@rm ../openssl/apps/CA.pl.bak
	@rm ../openssl/crypto/buildinf.h
	@rm ../openssl/crypto/$(CFG).bak
	@rm ../openssl/ms/version32.rc
	@rm ../openssl/tools/c_rehash.bak

clean:
	find archs -name $(CFG) -exec rm "{}" \;
