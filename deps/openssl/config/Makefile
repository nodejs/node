PERL    = perl
CONFIGURE = ./Configure
COPT = no-shared no-symlinks

ARCHS = aix-gcc aix64-gcc BSD-x86 BSD-x86_64 VC-WIN32 \
VC-WIN64A darwin64-x86_64-cc darwin-i386-cc linux-aarch64 \
linux-armv4 linux-elf linux-x32 linux-x86_64 linux-ppc \
linux-ppc64 linux32-s390x linux64-s390x solaris-x86-gcc \
solaris64-x86_64-gcc

CFG = opensslconf.h
SRC_CFG = ../openssl/crypto/$(CFG)
BACKUP_FILES = ../openssl/Makefile ../openssl/Makefile.bak $(SRC_CFG)
BACKUP_EXT = nodebackup

# OPENSSL_CPUID_OBJ is defined in openssl.gypi for use --openssl-no-asm
CPUIDFIX = 's/\#define OPENSSL_CPUID_OBJ$$//;'

MACFIX ='s/define RC4_INT unsigned char$$/define RC4_INT unsigned int/;'

PHONY = all clean backup restore
.PHONY: $(PHONY)

all: backup $(ARCHS) cleanconf fixdarwin64 restore

$(ARCHS):
	cd ../openssl; $(PERL) $(CONFIGURE) $(COPT) $@ > /dev/null
	$(PERL) -p -e $(CPUIDFIX) $(SRC_CFG) > ./archs/$@/$(CFG)

# The current openssl release does not use RC4 asm since it explicitly
# specified as `$asm=~s/rc4\-[^:]+//;` in
# https://github.com/openssl/openssl/blob/OpenSSL_1_0_1-stable/Configure#L584
# But node has used RC4 asm on MacOS for long time. Fix type of RC4_INT
# into `unsigned int` in opensslconf.h of darwin64-x86_64-cc to work on
# the RC4 asm.
fixdarwin64:
	$(PERL) -pi -e $(MACFIX) ./archs/darwin64-x86_64-cc/$(CFG)

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
