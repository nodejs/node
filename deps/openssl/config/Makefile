# This Makefile is confirmed to be run only on Linux (CentOS and
# Ubuntu). perl5 and gas(>=2.26) are needed.

UNAME_S := $(shell uname -s)
ifneq ($(UNAME_S),Linux)
  $(error This can be run only on Linux)
endif

PERL    = perl

# Supported architecture list
ASM_ARCHS = aix64-gcc-as BSD-x86 BSD-x86_64 \
darwin64-x86_64-cc darwin-i386-cc darwin64-arm64-cc linux-aarch64 \
linux-armv4 linux-elf linux-x86_64 \
linux-ppc64le linux32-s390x linux64-s390x linux64-mips64\
solaris-x86-gcc solaris64-x86_64-gcc VC-WIN64A VC-WIN32

NO_ASM_ARCHS = VC-WIN64-ARM linux64-riscv64 linux64-loongarch64

CC = gcc
FAKE_GCC = ../config/fake_gcc.pl

CONFIGURE = ./Configure
# no-comp: against CRIME attack
# no-shared: openssl-cli needs static link
# no-afalgeng: old Linux kernel < 4.0 does not support it
# enable-ssl-trace: cause the optional SSL_trace API to be built
COPTS =  no-comp no-shared no-afalgeng enable-ssl-trace enable-fips

# disable platform check in Configure
NO_WARN_ENV = CONFIGURE_CHECKER_WARN=1

GENERATE = ./generate_gypi.pl

OPSSL_SRC = ../openssl

# Header files generated with Configure
#INT_CFGS = bn_conf.h dso_conf.h
INT_CFG_DIR = $(OPSSL_SRC)/include/crypto
GEN_HEADERS = asn1 asn1t bio cmp cms configuration conf crmf crypto ct err    \
              ess fipskey lhash ocsp opensslv pkcs12 pkcs7 safestack srp ssl  \
	      ui x509 x509v3 x509_vfy conf

CRYPTO_GEN_HEADERS = bn_conf dso_conf

PHONY = all clean replace generate_headers
.PHONY: $(PHONY)

#all: $(ASM_ARCHS) $(NO_ASM_ARCHS) generate_headers replace
all: $(ASM_ARCHS) $(NO_ASM_ARCHS) generate_headers

# Configure and generate openssl asm files for each archs
$(ASM_ARCHS):
	cd $(OPSSL_SRC); $(NO_WARN_ENV) CC=$(CC) $(PERL) $(CONFIGURE) $(COPTS) $@;
	$(PERL) -w -I$(OPSSL_SRC) $(GENERATE) asm $@ "${GEN_HEADERS}" "${CRYPTO_GEN_HEADERS}"
# Confgure asm_avx2 and generate upto avx2 support
	cd $(OPSSL_SRC); $(NO_WARN_ENV) CC=$(FAKE_GCC) $(PERL) $(CONFIGURE) \
	$(COPTS) $@;
	$(PERL) -w -I$(OPSSL_SRC) $(GENERATE) asm_avx2 $@ "${GEN_HEADERS}" "${CRYTO_GEN_HEADERS}"
# Configure no-asm and generate no-asm sources
	cd $(OPSSL_SRC); $(NO_WARN_ENV) $(PERL) $(CONFIGURE) $(COPTS) \
	no-asm $@;
	$(PERL) -w -I$(OPSSL_SRC) $(GENERATE) no-asm $@ "${GEN_HEADERS}" "${CRYPTO_GEN_HEADERS}"

$(NO_ASM_ARCHS):
# Configure no-asm and generate no-asm sources
	cd $(OPSSL_SRC); $(NO_WARN_ENV) $(PERL) $(CONFIGURE) $(COPTS) \
	no-asm $@;
	$(PERL) -w -I$(OPSSL_SRC) $(GENERATE) no-asm $@ "${GEN_HEADERS}" "${CRYPTO_GEN_HEADERS}"

generate_headers:
	@$(PERL) -w -I$(OPSSL_SRC) ./generate_headers.pl "${GEN_HEADERS}" "${CRYPTO_GEN_HEADERS}"

clean:
	find archs \( -name \*.S -o -name \*.s -o -name \*.asm -o \
	-name \*.gypi -o -name \*.h -o -name \*.pm -o -name \*.rc \) -exec rm "{}" \;
