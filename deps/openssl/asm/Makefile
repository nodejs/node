PERL	?= perl
PERL	+= -I../openssl/crypto/perlasm -I../openssl/crypto/bn/asm

# OPENSSL_IA32_SSE2 flag is needed for checking the sse2 feature on ia32
# see https://github.com/openssl/openssl/blob/OpenSSL_1_0_2-stable/crypto/sha/asm/sha512-586.pl#L56
SSE2	= -DOPENSSL_IA32_SSE2

OUTPUTS	= \
	x86-elf-gas/aes/aes-586.s \
	x86-elf-gas/aes/aesni-x86.s \
	x86-elf-gas/aes/vpaes-x86.s \
	x86-elf-gas/bf/bf-586.s \
	x86-elf-gas/bn/bn-586.s \
	x86-elf-gas/bn/co-586.s \
	x86-elf-gas/bn/x86-mont.s \
	x86-elf-gas/bn/x86-gf2m.s \
	x86-elf-gas/camellia/cmll-x86.s \
	x86-elf-gas/cast/cast-586.s \
	x86-elf-gas/des/crypt586.s \
	x86-elf-gas/des/des-586.s \
	x86-elf-gas/md5/md5-586.s \
	x86-elf-gas/rc4/rc4-586.s \
	x86-elf-gas/ripemd/rmd-586.s \
	x86-elf-gas/sha/sha1-586.s \
	x86-elf-gas/sha/sha256-586.s \
	x86-elf-gas/sha/sha512-586.s \
	x86-elf-gas/whrlpool/wp-mmx.s \
	x86-elf-gas/modes/ghash-x86.s \
	x86-elf-gas/x86cpuid.s \
	x64-elf-gas/aes/aes-x86_64.s \
        x64-elf-gas/aes/aesni-mb-x86_64.s \
        x64-elf-gas/aes/aesni-sha256-x86_64.s \
	x64-elf-gas/aes/aesni-x86_64.s \
	x64-elf-gas/aes/vpaes-x86_64.s \
	x64-elf-gas/aes/bsaes-x86_64.s \
	x64-elf-gas/aes/aesni-sha1-x86_64.s \
        x64-elf-gas/bn/rsaz-avx2.s \
        x64-elf-gas/bn/rsaz-x86_64.s \
	x64-elf-gas/bn/x86_64-mont.s \
	x64-elf-gas/bn/x86_64-mont5.s \
	x64-elf-gas/bn/x86_64-gf2m.s \
	x64-elf-gas/camellia/cmll-x86_64.s \
        x64-elf-gas/ec/ecp_nistz256-x86_64.s \
	x64-elf-gas/md5/md5-x86_64.s \
	x64-elf-gas/rc4/rc4-x86_64.s \
	x64-elf-gas/rc4/rc4-md5-x86_64.s \
        x64-elf-gas/sha/sha1-mb-x86_64.s \
	x64-elf-gas/sha/sha1-x86_64.s \
        x64-elf-gas/sha/sha256-mb-x86_64.s \
	x64-elf-gas/sha/sha256-x86_64.s \
	x64-elf-gas/sha/sha512-x86_64.s \
	x64-elf-gas/whrlpool/wp-x86_64.s \
        x64-elf-gas/modes/aesni-gcm-x86_64.s \
	x64-elf-gas/modes/ghash-x86_64.s \
	x64-elf-gas/x86_64cpuid.s \
	arm-void-gas/aes/aes-armv4.S \
	arm-void-gas/aes/bsaes-armv7.S \
	arm-void-gas/aes/aesv8-armx.S \
	arm-void-gas/bn/armv4-mont.S \
	arm-void-gas/bn/armv4-gf2m.S \
	arm-void-gas/sha/sha1-armv4-large.S \
	arm-void-gas/sha/sha256-armv4.S \
	arm-void-gas/sha/sha512-armv4.S \
	arm-void-gas/modes/ghash-armv4.S \
	arm-void-gas/modes/ghashv8-armx.S \
	arm64-linux64-gas/aes/aesv8-armx.S \
	arm64-linux64-gas/modes/ghashv8-armx.S \
	arm64-linux64-gas/sha/sha1-armv8.S \
	arm64-linux64-gas/sha/sha256-armv8.S \
	arm64-linux64-gas/sha/sha512-armv8.S \
	x86-macosx-gas/aes/aes-586.s \
	x86-macosx-gas/aes/aesni-x86.s \
	x86-macosx-gas/aes/vpaes-x86.s \
	x86-macosx-gas/bf/bf-586.s \
	x86-macosx-gas/bn/bn-586.s \
	x86-macosx-gas/bn/co-586.s \
	x86-macosx-gas/bn/x86-mont.s \
	x86-macosx-gas/bn/x86-gf2m.s \
	x86-macosx-gas/camellia/cmll-x86.s \
	x86-macosx-gas/cast/cast-586.s \
	x86-macosx-gas/des/crypt586.s \
	x86-macosx-gas/des/des-586.s \
	x86-macosx-gas/md5/md5-586.s \
	x86-macosx-gas/rc4/rc4-586.s \
	x86-macosx-gas/ripemd/rmd-586.s \
	x86-macosx-gas/sha/sha1-586.s \
	x86-macosx-gas/sha/sha256-586.s \
	x86-macosx-gas/sha/sha512-586.s \
	x86-macosx-gas/whrlpool/wp-mmx.s \
	x86-macosx-gas/modes/ghash-x86.s \
	x86-macosx-gas/x86cpuid.s \
	x64-macosx-gas/aes/aes-x86_64.s \
	x64-macosx-gas/aes/aesni-x86_64.s \
	x64-macosx-gas/aes/vpaes-x86_64.s \
	x64-macosx-gas/aes/aesni-mb-x86_64.s \
	x64-macosx-gas/aes/aesni-sha256-x86_64.s \
	x64-macosx-gas/aes/bsaes-x86_64.s \
	x64-macosx-gas/aes/aesni-sha1-x86_64.s \
	x64-macosx-gas/bn/rsaz-avx2.s \
	x64-macosx-gas/bn/rsaz-x86_64.s \
	x64-macosx-gas/bn/x86_64-mont.s \
	x64-macosx-gas/bn/x86_64-mont5.s \
	x64-macosx-gas/bn/x86_64-gf2m.s \
	x64-macosx-gas/camellia/cmll-x86_64.s \
	x64-macosx-gas/ec/ecp_nistz256-x86_64.s \
	x64-macosx-gas/md5/md5-x86_64.s \
	x64-macosx-gas/sha/sha1-mb-x86_64.s \
	x64-macosx-gas/sha/sha1-x86_64.s \
	x64-macosx-gas/sha/sha256-mb-x86_64.s \
	x64-macosx-gas/sha/sha256-x86_64.s \
	x64-macosx-gas/sha/sha512-x86_64.s \
	x64-macosx-gas/whrlpool/wp-x86_64.s \
	x64-macosx-gas/modes/aesni-gcm-x86_64.s \
	x64-macosx-gas/modes/ghash-x86_64.s \
	x64-macosx-gas/x86_64cpuid.s \
	x86-win32-masm/aes/aes-586.asm \
	x86-win32-masm/aes/aesni-x86.asm \
	x86-win32-masm/aes/vpaes-x86.asm \
	x86-win32-masm/bf/bf-586.asm \
	x86-win32-masm/bn/bn-586.asm \
	x86-win32-masm/bn/co-586.asm \
	x86-win32-masm/bn/x86-mont.asm \
	x86-win32-masm/bn/x86-gf2m.asm \
	x86-win32-masm/camellia/cmll-x86.asm \
	x86-win32-masm/cast/cast-586.asm \
	x86-win32-masm/des/crypt586.asm \
	x86-win32-masm/des/des-586.asm \
	x86-win32-masm/md5/md5-586.asm \
	x86-win32-masm/rc4/rc4-586.asm \
	x86-win32-masm/ripemd/rmd-586.asm \
	x86-win32-masm/sha/sha1-586.asm \
	x86-win32-masm/sha/sha256-586.asm \
	x86-win32-masm/sha/sha512-586.asm \
	x86-win32-masm/whrlpool/wp-mmx.asm \
	x86-win32-masm/modes/ghash-x86.asm \
	x86-win32-masm/x86cpuid.asm \
	x64-win32-masm/aes/aes-x86_64.asm \
        x64-win32-masm/aes/aesni-mb-x86_64.asm \
        x64-win32-masm/aes/aesni-sha256-x86_64.asm \
	x64-win32-masm/aes/aesni-x86_64.asm \
	x64-win32-masm/aes/vpaes-x86_64.asm \
	x64-win32-masm/aes/bsaes-x86_64.asm \
	x64-win32-masm/aes/aesni-sha1-x86_64.asm \
        x64-win32-masm/bn/rsaz-avx2.asm \
        x64-win32-masm/bn/rsaz-x86_64.asm \
	x64-win32-masm/bn/x86_64-mont.asm \
	x64-win32-masm/bn/x86_64-mont5.asm \
	x64-win32-masm/bn/x86_64-gf2m.asm \
	x64-win32-masm/camellia/cmll-x86_64.asm \
        x64-win32-masm/ec/ecp_nistz256-x86_64.asm \
	x64-win32-masm/md5/md5-x86_64.asm \
	x64-win32-masm/rc4/rc4-x86_64.asm \
	x64-win32-masm/rc4/rc4-md5-x86_64.asm \
        x64-win32-masm/sha/sha1-mb-x86_64.asm \
	x64-win32-masm/sha/sha1-x86_64.asm \
        x64-win32-masm/sha/sha256-mb-x86_64.asm \
	x64-win32-masm/sha/sha256-x86_64.asm \
        x64-win32-masm/sha/sha512-x86_64.asm \
	x64-win32-masm/whrlpool/wp-x86_64.asm \
        x64-win32-masm/modes/aesni-gcm-x86_64.asm \
	x64-win32-masm/modes/ghash-x86_64.asm \
	x64-win32-masm/x86_64cpuid.asm \

# sha512 asm files for x86_64 need 512 in the filenames for outputs
# so that we add new rules to generate sha512 asm files with
# specifying its filename in the second argument.  See
# https://github.com/openssl/openssl/blob/OpenSSL_1_0_2-stable/crypto/sha/asm/sha512-x86_64.pl#L137-L149

x64-elf-gas/sha/sha512-%.s:
	$(PERL) $< elf $@

x64-elf-gas/%.s:
	$(PERL) $< elf > $@

arm-void-gas/%.S:
	$(PERL) $< void > $@

arm64-linux64-gas/sha/sha512-%.S:
	$(PERL) $< linux64 $@

arm64-linux64-gas/%.S:
	$(PERL) $< linux64 > $@

x64-macosx-gas/sha/sha512-%.s:
	$(PERL) $< macosx $@

x64-macosx-gas/%.s:
	$(PERL) $< macosx > $@

x64-win32-masm/sha/sha512-%.asm:
	$(PERL) $< masm $@

x64-win32-masm/%.asm:
	$(PERL) $< masm > $@

x86-elf-gas/%.s:
	$(PERL) $< elf $(SSE2) > $@

x86-macosx-gas/%.s:
	$(PERL) $< macosx $(SSE2) > $@

x86-win32-masm/%.asm:
	$(PERL) $< win32 $(SSE2) > $@

.PHONY:	all outputs clean check-cc check-asm
all:	check-cc check-asm outputs

outputs: $(OUTPUTS)
	# strip trailing whitespace and final blank newline
	$(PERL) -pi -e 's/\s+$$/\n/; s/^\n$$// if eof' $^

clean:
	find . -iname '*.asm' -exec rm "{}" \;
	find . -iname '*.s' -exec rm "{}" \;
	find . -iname '*.S' -exec rm "{}" \;

# CC and ASM enviroments are needed to genrate asm files from perl.
# Use gcc and nasm to run on unix.  See
# https://github.com/openssl/openssl/blob/OpenSSL_1_0_2-stable/crypto/sha/asm/sha512-x86_64.pl#L112-L129

check-cc:
ifndef CC
	  $(error CC is not set.)
endif

check-asm:
ifndef ASM
	  $(error ASM is not set.)
endif

x64-elf-gas/aes/aes-x86_64.s: ../openssl/crypto/aes/asm/aes-x86_64.pl
x64-elf-gas/aes/aesni-x86_64.s: ../openssl/crypto/aes/asm/aesni-x86_64.pl
x64-elf-gas/aes/aesni-mb-x86_64.s: ../openssl/crypto/aes/asm/aesni-mb-x86_64.pl
x64-elf-gas/aes/aesni-sha256-x86_64.s:  ../openssl/crypto/aes/asm/aesni-sha256-x86_64.pl
x64-elf-gas/aes/vpaes-x86_64.s: ../openssl/crypto/aes/asm/vpaes-x86_64.pl
x64-elf-gas/aes/bsaes-x86_64.s: ../openssl/crypto/aes/asm/bsaes-x86_64.pl
x64-elf-gas/aes/aesni-sha1-x86_64.s: ../openssl/crypto/aes/asm/aesni-sha1-x86_64.pl
x64-elf-gas/bn/rsaz-avx2.s: ../openssl/crypto/bn/asm/rsaz-avx2.pl
x64-elf-gas/bn/rsaz-x86_64.s: ../openssl/crypto/bn/asm/rsaz-x86_64.pl
x64-elf-gas/bn/x86_64-mont.s: ../openssl/crypto/bn/asm/x86_64-mont.pl
x64-elf-gas/bn/x86_64-mont5.s: ../openssl/crypto/bn/asm/x86_64-mont5.pl
x64-elf-gas/bn/x86_64-gf2m.s: ../openssl/crypto/bn/asm/x86_64-gf2m.pl
x64-elf-gas/camellia/cmll-x86_64.s: ../openssl/crypto/camellia/asm/cmll-x86_64.pl
x64-elf-gas/ec/ecp_nistz256-x86_64.s: ../openssl/crypto/ec/asm/ecp_nistz256-x86_64.pl
x64-elf-gas/md5/md5-x86_64.s: ../openssl/crypto/md5/asm/md5-x86_64.pl
x64-elf-gas/rc4/rc4-x86_64.s: ../openssl/crypto/rc4/asm/rc4-x86_64.pl
x64-elf-gas/rc4/rc4-md5-x86_64.s: ../openssl/crypto/rc4/asm/rc4-md5-x86_64.pl
x64-elf-gas/sha/sha1-mb-x86_64.s: ../openssl/crypto/sha/asm/sha1-mb-x86_64.pl
x64-elf-gas/sha/sha1-x86_64.s: ../openssl/crypto/sha/asm/sha1-x86_64.pl
x64-elf-gas/sha/sha512-x86_64.s: ../openssl/crypto/sha/asm/sha512-x86_64.pl
x64-elf-gas/sha/sha256-mb-x86_64.s: ../openssl/crypto/sha/asm/sha256-mb-x86_64.pl
x64-elf-gas/sha/sha256-x86_64.s: ../openssl/crypto/sha/asm/sha512-x86_64.pl
x64-elf-gas/whrlpool/wp-x86_64.s: ../openssl/crypto/whrlpool/asm/wp-x86_64.pl
x64-elf-gas/modes/aesni-gcm-x86_64.s: ../openssl/crypto/modes/asm/aesni-gcm-x86_64.pl
x64-elf-gas/modes/ghash-x86_64.s: ../openssl/crypto/modes/asm/ghash-x86_64.pl
x64-elf-gas/x86_64cpuid.s: ../openssl/crypto/x86_64cpuid.pl
x64-macosx-gas/aes/aes-x86_64.s: ../openssl/crypto/aes/asm/aes-x86_64.pl
x64-macosx-gas/aes/aesni-x86_64.s: ../openssl/crypto/aes/asm/aesni-x86_64.pl
x64-macosx-gas/aes/vpaes-x86_64.s: ../openssl/crypto/aes/asm/vpaes-x86_64.pl
x64-macosx-gas/aes/aesni-mb-x86_64.s: ../openssl/crypto/aes/asm/aesni-mb-x86_64.pl
x64-macosx-gas/aes/aesni-sha256-x86_64.s: ../openssl/crypto/aes/asm/aesni-sha256-x86_64.pl
x64-macosx-gas/aes/bsaes-x86_64.s: ../openssl/crypto/aes/asm/bsaes-x86_64.pl
x64-macosx-gas/aes/aesni-sha1-x86_64.s: ../openssl/crypto/aes/asm/aesni-sha1-x86_64.pl
x64-macosx-gas/bn/rsaz-avx2.s: ../openssl/crypto/bn/asm/rsaz-avx2.pl
x64-macosx-gas/bn/rsaz-x86_64.s: ../openssl/crypto/bn/asm/rsaz-x86_64.pl
x64-macosx-gas/bn/x86_64-mont.s: ../openssl/crypto/bn/asm/x86_64-mont.pl
x64-macosx-gas/bn/x86_64-mont5.s: ../openssl/crypto/bn/asm/x86_64-mont5.pl
x64-macosx-gas/bn/x86_64-gf2m.s: ../openssl/crypto/bn/asm/x86_64-gf2m.pl
x64-macosx-gas/camellia/cmll-x86_64.s: ../openssl/crypto/camellia/asm/cmll-x86_64.pl
x64-macosx-gas/ec/ecp_nistz256-x86_64.s: ../openssl/crypto/ec/asm/ecp_nistz256-x86_64.pl
x64-macosx-gas/md5/md5-x86_64.s: ../openssl/crypto/md5/asm/md5-x86_64.pl
x64-macosx-gas/sha/sha1-x86_64.s: ../openssl/crypto/sha/asm/sha1-x86_64.pl
x64-macosx-gas/sha/sha1-mb-x86_64.s: ../openssl/crypto/sha/asm/sha1-mb-x86_64.pl
x64-macosx-gas/sha/sha256-mb-x86_64.s: ../openssl/crypto/sha/asm/sha256-mb-x86_64.pl
x64-macosx-gas/sha/sha256-x86_64.s: ../openssl/crypto/sha/asm/sha512-x86_64.pl
x64-macosx-gas/sha/sha512-x86_64.s: ../openssl/crypto/sha/asm/sha512-x86_64.pl
x64-macosx-gas/whrlpool/wp-x86_64.s: ../openssl/crypto/whrlpool/asm/wp-x86_64.pl
x64-macosx-gas/modes/aesni-gcm-x86_64.s: ../openssl/crypto/modes/asm/aesni-gcm-x86_64.pl
x64-macosx-gas/modes/ghash-x86_64.s: ../openssl/crypto/modes/asm/ghash-x86_64.pl
x64-macosx-gas/x86_64cpuid.s: ../openssl/crypto/x86_64cpuid.pl
x64-win32-masm/aes/aes-x86_64.asm: ../openssl/crypto/aes/asm/aes-x86_64.pl
x64-win32-masm/aes/aesni-x86_64.asm: ../openssl/crypto/aes/asm/aesni-x86_64.pl
x64-win32-masm/aes/aesni-mb-x86_64.asm: ../openssl/crypto/aes/asm/aesni-mb-x86_64.pl
x64-win32-masm/aes/aesni-sha256-x86_64.asm: ../openssl/crypto/aes/asm/aesni-sha256-x86_64.pl
x64-win32-masm/aes/vpaes-x86_64.asm: ../openssl/crypto/aes/asm/vpaes-x86_64.pl
x64-win32-masm/aes/bsaes-x86_64.asm: ../openssl/crypto/aes/asm/bsaes-x86_64.pl
x64-win32-masm/aes/aesni-sha1-x86_64.asm: ../openssl/crypto/aes/asm/aesni-sha1-x86_64.pl
x64-win32-masm/bn/rsaz-avx2.asm: ../openssl/crypto/bn/asm/rsaz-avx2.pl
x64-win32-masm/bn/rsaz-x86_64.asm: ../openssl/crypto/bn/asm/rsaz-x86_64.pl
x64-win32-masm/bn/x86_64-mont.asm: ../openssl/crypto/bn/asm/x86_64-mont.pl
x64-win32-masm/bn/x86_64-mont5.asm: ../openssl/crypto/bn/asm/x86_64-mont5.pl
x64-win32-masm/bn/x86_64-gf2m.asm: ../openssl/crypto/bn/asm/x86_64-gf2m.pl
x64-win32-masm/camellia/cmll-x86_64.asm: ../openssl/crypto/camellia/asm/cmll-x86_64.pl
x64-win32-masm/ec/ecp_nistz256-x86_64.asm: ../openssl/crypto/ec/asm/ecp_nistz256-x86_64.pl
x64-win32-masm/md5/md5-x86_64.asm: ../openssl/crypto/md5/asm/md5-x86_64.pl
x64-win32-masm/rc4/rc4-x86_64.asm: ../openssl/crypto/rc4/asm/rc4-x86_64.pl
x64-win32-masm/rc4/rc4-md5-x86_64.asm: ../openssl/crypto/rc4/asm/rc4-md5-x86_64.pl
x64-win32-masm/sha/sha1-mb-x86_64.asm: ../openssl/crypto/sha/asm/sha1-mb-x86_64.pl
x64-win32-masm/sha/sha1-x86_64.asm: ../openssl/crypto/sha/asm/sha1-x86_64.pl
x64-win32-masm/sha/sha256-mb-x86_64.asm: ../openssl/crypto/sha/asm/sha256-mb-x86_64.pl
x64-win32-masm/sha/sha256-x86_64.asm: ../openssl/crypto/sha/asm/sha512-x86_64.pl
x64-win32-masm/sha/sha512-x86_64.asm: ../openssl/crypto/sha/asm/sha512-x86_64.pl
x64-win32-masm/whrlpool/wp-x86_64.asm: ../openssl/crypto/whrlpool/asm/wp-x86_64.pl
x64-win32-masm/modes/aesni-gcm-x86_64.asm: ../openssl/crypto/modes/asm/aesni-gcm-x86_64.pl
x64-win32-masm/modes/ghash-x86_64.asm: ../openssl/crypto/modes/asm/ghash-x86_64.pl
x64-win32-masm/x86_64cpuid.asm: ../openssl/crypto/x86_64cpuid.pl
x86-elf-gas/aes/aes-586.s: ../openssl/crypto/aes/asm/aes-586.pl
x86-elf-gas/aes/aesni-x86.s: ../openssl/crypto/aes/asm/aesni-x86.pl
x86-elf-gas/aes/vpaes-x86.s: ../openssl/crypto/aes/asm/vpaes-x86.pl
x86-elf-gas/bf/bf-586.s: ../openssl/crypto/bf/asm/bf-586.pl
x86-elf-gas/bn/bn-586.s: ../openssl/crypto/bn/asm/bn-586.pl
x86-elf-gas/bn/co-586.s: ../openssl/crypto/bn/asm/co-586.pl
x86-elf-gas/bn/x86-mont.s: ../openssl/crypto/bn/asm/x86-mont.pl
x86-elf-gas/bn/x86-gf2m.s: ../openssl/crypto/bn/asm/x86-gf2m.pl
x86-elf-gas/camellia/cmll-x86.s: ../openssl/crypto/camellia/asm/cmll-x86.pl
x86-elf-gas/cast/cast-586.s: ../openssl/crypto/cast/asm/cast-586.pl
x86-elf-gas/des/crypt586.s: ../openssl/crypto/des/asm/crypt586.pl
x86-elf-gas/des/des-586.s: ../openssl/crypto/des/asm/des-586.pl
x86-elf-gas/md5/md5-586.s: ../openssl/crypto/md5/asm/md5-586.pl
x86-elf-gas/rc4/rc4-586.s: ../openssl/crypto/rc4/asm/rc4-586.pl
x86-elf-gas/rc5/rc5-586.s: ../openssl/crypto/rc5/asm/rc5-586.pl
x86-elf-gas/ripemd/rmd-586.s: ../openssl/crypto/ripemd/asm/rmd-586.pl
x86-elf-gas/sha/sha1-586.s: ../openssl/crypto/sha/asm/sha1-586.pl
x86-elf-gas/sha/sha256-586.s: ../openssl/crypto/sha/asm/sha256-586.pl
x86-elf-gas/sha/sha512-586.s: ../openssl/crypto/sha/asm/sha512-586.pl
x86-elf-gas/whrlpool/wp-mmx.s: ../openssl/crypto/whrlpool/asm/wp-mmx.pl
x86-elf-gas/modes/ghash-x86.s: ../openssl/crypto/modes/asm/ghash-x86.pl
x86-elf-gas/x86cpuid.s: ../openssl/crypto/x86cpuid.pl
x86-macosx-gas/aes/aes-586.s: ../openssl/crypto/aes/asm/aes-586.pl
x86-macosx-gas/aes/aesni-x86.s: ../openssl/crypto/aes/asm/aesni-x86.pl
x86-macosx-gas/aes/vpaes-x86.s: ../openssl/crypto/aes/asm/vpaes-x86.pl
x86-macosx-gas/bf/bf-586.s: ../openssl/crypto/bf/asm/bf-686.pl
x86-macosx-gas/bn/bn-586.s: ../openssl/crypto/bn/asm/bn-586.pl
x86-macosx-gas/bn/co-586.s: ../openssl/crypto/bn/asm/co-586.pl
x86-macosx-gas/bn/x86-mont.s: ../openssl/crypto/bn/asm/x86-mont.pl
x86-macosx-gas/bn/x86-gf2m.s: ../openssl/crypto/bn/asm/x86-gf2m.pl
x86-macosx-gas/camellia/cmll-x86.s: ../openssl/crypto/camellia/asm/cmll-x86.pl
x86-macosx-gas/cast/cast-586.s: ../openssl/crypto/cast/asm/cast-586.pl
x86-macosx-gas/des/crypt586.s: ../openssl/crypto/des/asm/crypt586.pl
x86-macosx-gas/des/des-586.s: ../openssl/crypto/des/asm/des-586.pl
x86-macosx-gas/md5/md5-586.s: ../openssl/crypto/md5/asm/md5-586.pl
x86-macosx-gas/rc4/rc4-586.s: ../openssl/crypto/rc4/asm/rc4-586.pl
x86-macosx-gas/rc5/rc5-586.s: ../openssl/crypto/rc5/asm/rc5-586.pl
x86-macosx-gas/ripemd/rmd-586.s: ../openssl/crypto/ripemd/asm/rmd-586.pl
x86-macosx-gas/sha/sha1-586.s: ../openssl/crypto/sha/asm/sha1-586.pl
x86-macosx-gas/sha/sha256-586.s: ../openssl/crypto/sha/asm/sha256-586.pl
x86-macosx-gas/sha/sha512-586.s: ../openssl/crypto/sha/asm/sha512-586.pl
x86-macosx-gas/whrlpool/wp-mmx.s: ../openssl/crypto/whrlpool/asm/wp-mmx.pl
x86-macosx-gas/modes/ghash-x86.s: ../openssl/crypto/modes/asm/ghash-x86.pl
x86-macosx-gas/x86cpuid.s: ../openssl/crypto/x86cpuid.pl
x86-win32-masm/aes/aes-586.asm: ../openssl/crypto/aes/asm/aes-586.pl
x86-win32-masm/aes/aesni-x86.asm: ../openssl/crypto/aes/asm/aesni-x86.pl
x86-win32-masm/aes/vpaes-x86.asm: ../openssl/crypto/aes/asm/vpaes-x86.pl
x86-win32-masm/bf/bf-586.asm: ../openssl/crypto/bf/asm/bf-586.pl
x86-win32-masm/bn/bn-586.asm: ../openssl/crypto/bn/asm/bn-586.pl
x86-win32-masm/bn/co-586.asm: ../openssl/crypto/bn/asm/co-586.pl
x86-win32-masm/bn/x86-gf2m.asm: ../openssl/crypto/bn/asm/x86-gf2m.pl
x86-win32-masm/bn/x86-mont.asm: ../openssl/crypto/bn/asm/x86-mont.pl
x86-win32-masm/camellia/cmll-x86.asm: ../openssl/crypto/camellia/asm/cmll-x86.pl
x86-win32-masm/cast/cast-586.asm: ../openssl/crypto/cast/asm/cast-586.pl
x86-win32-masm/des/crypt586.asm: ../openssl/crypto/des/asm/crypt586.pl
x86-win32-masm/des/des-586.asm: ../openssl/crypto/des/asm/des-586.pl
x86-win32-masm/md5/md5-586.asm: ../openssl/crypto/md5/asm/md5-586.pl
x86-win32-masm/rc4/rc4-586.asm: ../openssl/crypto/rc4/asm/rc4-586.pl
x86-win32-masm/ripemd/rmd-586.asm: ../openssl/crypto/ripemd/asm/rmd-586.pl
x86-win32-masm/sha/sha1-586.asm: ../openssl/crypto/sha/asm/sha1-586.pl
x86-win32-masm/sha/sha256-586.asm: ../openssl/crypto/sha/asm/sha256-586.pl
x86-win32-masm/sha/sha512-586.asm: ../openssl/crypto/sha/asm/sha512-586.pl
x86-win32-masm/whrlpool/wp-mmx.asm: ../openssl/crypto/whrlpool/asm/wp-mmx.pl
x86-win32-masm/modes/ghash-x86.asm: ../openssl/crypto/modes/asm/ghash-x86.pl
x86-win32-masm/x86cpuid.asm: ../openssl/crypto/x86cpuid.pl
arm-void-gas/aes/aes-armv4.S: ../openssl/crypto/aes/asm/aes-armv4.pl
arm-void-gas/aes/bsaes-armv7.S: ../openssl/crypto/aes/asm/bsaes-armv7.pl
arm-void-gas/aes/aesv8-armx.S: ../openssl/crypto/aes/asm/aesv8-armx.pl
arm-void-gas/bn/armv4-mont.S: ../openssl/crypto/bn/asm/armv4-mont.pl
arm-void-gas/bn/armv4-gf2m.S: ../openssl/crypto/bn/asm/armv4-gf2m.pl
arm-void-gas/sha/sha1-armv4-large.S: ../openssl/crypto/sha/asm/sha1-armv4-large.pl
arm-void-gas/sha/sha512-armv4.S: ../openssl/crypto/sha/asm/sha512-armv4.pl
arm-void-gas/sha/sha256-armv4.S: ../openssl/crypto/sha/asm/sha256-armv4.pl
arm-void-gas/modes/ghash-armv4.S: ../openssl/crypto/modes/asm/ghash-armv4.pl
arm-void-gas/modes/ghashv8-armx.S: ../openssl/crypto/modes/asm/ghashv8-armx.pl
arm64-linux64-gas/aes/aesv8-armx.S: ../openssl/crypto/aes/asm/aesv8-armx.pl
arm64-linux64-gas/modes/ghashv8-armx.S: ../openssl/crypto/modes/asm/ghashv8-armx.pl
arm64-linux64-gas/sha/sha1-armv8.S: ../openssl/crypto/sha/asm/sha1-armv8.pl
arm64-linux64-gas/sha/sha256-armv8.S: ../openssl/crypto/sha/asm/sha512-armv8.pl
arm64-linux64-gas/sha/sha512-armv8.S: ../openssl/crypto/sha/asm/sha512-armv8.pl
