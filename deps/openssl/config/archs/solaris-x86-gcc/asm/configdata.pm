#! /usr/bin/env perl

package configdata;

use strict;
use warnings;

use Exporter;
#use vars qw(@ISA @EXPORT);
our @ISA = qw(Exporter);
our @EXPORT = qw(%config %target %disabled %withargs %unified_info @disablables);

our %config = (
  AR => "ar",
  ARFLAGS => [ "r" ],
  CC => "gcc",
  CFLAGS => [ "-Wall -O3 -fomit-frame-pointer" ],
  CPPDEFINES => [  ],
  CPPFLAGS => [  ],
  CPPINCLUDES => [  ],
  CXXFLAGS => [  ],
  HASHBANGPERL => "/usr/bin/env perl",
  LDFLAGS => [  ],
  LDLIBS => [  ],
  PERL => "/usr/bin/perl",
  RANLIB => "ranlib",
  RC => "windres",
  RCFLAGS => [  ],
  b32 => "1",
  b64 => "0",
  b64l => "0",
  bn_ll => "1",
  build_file => "Makefile",
  build_file_templates => [ "Configurations/common0.tmpl", "Configurations/unix-Makefile.tmpl", "Configurations/common.tmpl" ],
  build_infos => [ "./build.info", "crypto/build.info", "ssl/build.info", "engines/build.info", "apps/build.info", "test/build.info", "util/build.info", "tools/build.info", "fuzz/build.info", "crypto/objects/build.info", "crypto/md4/build.info", "crypto/md5/build.info", "crypto/sha/build.info", "crypto/mdc2/build.info", "crypto/hmac/build.info", "crypto/ripemd/build.info", "crypto/whrlpool/build.info", "crypto/poly1305/build.info", "crypto/blake2/build.info", "crypto/siphash/build.info", "crypto/sm3/build.info", "crypto/des/build.info", "crypto/aes/build.info", "crypto/rc2/build.info", "crypto/rc4/build.info", "crypto/idea/build.info", "crypto/aria/build.info", "crypto/bf/build.info", "crypto/cast/build.info", "crypto/camellia/build.info", "crypto/seed/build.info", "crypto/sm4/build.info", "crypto/chacha/build.info", "crypto/modes/build.info", "crypto/bn/build.info", "crypto/ec/build.info", "crypto/rsa/build.info", "crypto/dsa/build.info", "crypto/dh/build.info", "crypto/sm2/build.info", "crypto/dso/build.info", "crypto/engine/build.info", "crypto/buffer/build.info", "crypto/bio/build.info", "crypto/stack/build.info", "crypto/lhash/build.info", "crypto/rand/build.info", "crypto/err/build.info", "crypto/evp/build.info", "crypto/asn1/build.info", "crypto/pem/build.info", "crypto/x509/build.info", "crypto/x509v3/build.info", "crypto/conf/build.info", "crypto/txt_db/build.info", "crypto/pkcs7/build.info", "crypto/pkcs12/build.info", "crypto/ocsp/build.info", "crypto/ui/build.info", "crypto/cms/build.info", "crypto/ts/build.info", "crypto/srp/build.info", "crypto/cmac/build.info", "crypto/ct/build.info", "crypto/async/build.info", "crypto/kdf/build.info", "crypto/store/build.info", "test/ossl_shim/build.info" ],
  build_type => "release",
  builddir => ".",
  cflags => [ "-Wa,--noexecstack" ],
  conf_files => [ "Configurations/00-base-templates.conf", "Configurations/10-main.conf" ],
  cppflags => [  ],
  cxxflags => [  ],
  defines => [ "NDEBUG" ],
  dirs => [ "crypto", "ssl", "engines", "apps", "test", "util", "tools", "fuzz" ],
  dso_defines => [ "PADLOCK_ASM" ],
  dynamic_engines => "0",
  engdirs => [  ],
  ex_libs => [  ],
  export_var_as_fn => "0",
  includes => [  ],
  lflags => [  ],
  lib_defines => [ "OPENSSL_PIC", "OPENSSL_CPUID_OBJ", "OPENSSL_BN_ASM_PART_WORDS", "OPENSSL_IA32_SSE2", "OPENSSL_BN_ASM_MONT", "OPENSSL_BN_ASM_GF2m", "SHA1_ASM", "SHA256_ASM", "SHA512_ASM", "RC4_ASM", "MD5_ASM", "RMD160_ASM", "AESNI_ASM", "VPAES_ASM", "WHIRLPOOL_ASM", "GHASH_ASM", "ECP_NISTZ256_ASM", "POLY1305_ASM" ],
  libdir => "",
  major => "1",
  makedepprog => "\$(CROSS_COMPILE)gcc",
  minor => "1.1",
  openssl_algorithm_defines => [ "OPENSSL_NO_COMP", "OPENSSL_NO_MD2", "OPENSSL_NO_RC5" ],
  openssl_api_defines => [  ],
  openssl_other_defines => [ "OPENSSL_RAND_SEED_OS", "OPENSSL_NO_AFALGENG", "OPENSSL_NO_ASAN", "OPENSSL_NO_CRYPTO_MDEBUG", "OPENSSL_NO_CRYPTO_MDEBUG_BACKTRACE", "OPENSSL_NO_DEVCRYPTOENG", "OPENSSL_NO_EC_NISTP_64_GCC_128", "OPENSSL_NO_EGD", "OPENSSL_NO_EXTERNAL_TESTS", "OPENSSL_NO_FUZZ_AFL", "OPENSSL_NO_FUZZ_LIBFUZZER", "OPENSSL_NO_HEARTBEATS", "OPENSSL_NO_MSAN", "OPENSSL_NO_SCTP", "OPENSSL_NO_SSL3", "OPENSSL_NO_SSL3_METHOD", "OPENSSL_NO_UBSAN", "OPENSSL_NO_UNIT_TEST", "OPENSSL_NO_WEAK_SSL_CIPHERS", "OPENSSL_NO_DYNAMIC_ENGINE" ],
  openssl_sys_defines => [  ],
  openssl_thread_defines => [ "OPENSSL_THREADS" ],
  openssldir => "",
  options => "enable-ssl-trace no-afalgeng no-asan no-buildtest-c++ no-comp no-crypto-mdebug no-crypto-mdebug-backtrace no-devcryptoeng no-dynamic-engine no-ec_nistp_64_gcc_128 no-egd no-external-tests no-fuzz-afl no-fuzz-libfuzzer no-heartbeats no-md2 no-msan no-rc5 no-sctp no-shared no-ssl3 no-ssl3-method no-ubsan no-unit-test no-weak-ssl-ciphers no-zlib no-zlib-dynamic",
  perl_archname => "x86_64-linux-thread-multi",
  perl_cmd => "/usr/bin/perl",
  perl_version => "5.30.3",
  perlargv => [ "no-comp", "no-shared", "no-afalgeng", "enable-ssl-trace", "solaris-x86-gcc" ],
  perlenv => {
      "AR" => undef,
      "ARFLAGS" => undef,
      "AS" => undef,
      "ASFLAGS" => undef,
      "BUILDFILE" => undef,
      "CC" => "gcc",
      "CFLAGS" => undef,
      "CPP" => undef,
      "CPPDEFINES" => undef,
      "CPPFLAGS" => undef,
      "CPPINCLUDES" => undef,
      "CROSS_COMPILE" => undef,
      "CXX" => undef,
      "CXXFLAGS" => undef,
      "HASHBANGPERL" => undef,
      "LD" => undef,
      "LDFLAGS" => undef,
      "LDLIBS" => undef,
      "MT" => undef,
      "MTFLAGS" => undef,
      "OPENSSL_LOCAL_CONFIG_DIR" => undef,
      "PERL" => undef,
      "RANLIB" => undef,
      "RC" => undef,
      "RCFLAGS" => undef,
      "RM" => undef,
      "WINDRES" => undef,
      "__CNF_CFLAGS" => undef,
      "__CNF_CPPDEFINES" => undef,
      "__CNF_CPPFLAGS" => undef,
      "__CNF_CPPINCLUDES" => undef,
      "__CNF_CXXFLAGS" => undef,
      "__CNF_LDFLAGS" => undef,
      "__CNF_LDLIBS" => undef,
  },
  prefix => "",
  processor => "",
  rc4_int => "unsigned int",
  sdirs => [ "objects", "md4", "md5", "sha", "mdc2", "hmac", "ripemd", "whrlpool", "poly1305", "blake2", "siphash", "sm3", "des", "aes", "rc2", "rc4", "idea", "aria", "bf", "cast", "camellia", "seed", "sm4", "chacha", "modes", "bn", "ec", "rsa", "dsa", "dh", "sm2", "dso", "engine", "buffer", "bio", "stack", "lhash", "rand", "err", "evp", "asn1", "pem", "x509", "x509v3", "conf", "txt_db", "pkcs7", "pkcs12", "ocsp", "ui", "cms", "ts", "srp", "cmac", "ct", "async", "kdf", "store" ],
  shlib_major => "1",
  shlib_minor => "1",
  shlib_version_history => "",
  shlib_version_number => "1.1",
  sourcedir => ".",
  target => "solaris-x86-gcc",
  tdirs => [ "ossl_shim" ],
  version => "1.1.1j",
  version_num => "0x101010afL",
);

our %target = (
  AR => "ar",
  ARFLAGS => "r",
  CC => "gcc",
  CFLAGS => "-Wall -O3 -fomit-frame-pointer",
  HASHBANGPERL => "/usr/bin/env perl",
  RANLIB => "ranlib",
  RC => "windres",
  _conf_fname_int => [ "Configurations/00-base-templates.conf", "Configurations/00-base-templates.conf", "Configurations/10-main.conf", "Configurations/00-base-templates.conf", "Configurations/00-base-templates.conf", "Configurations/10-main.conf", "Configurations/shared-info.pl" ],
  aes_asm_src => "aes_core.c aes_cbc.c vpaes-x86.s aesni-x86.s",
  aes_obj => "aes_core.o aes_cbc.o vpaes-x86.o aesni-x86.o",
  apps_aux_src => "",
  apps_init_src => "",
  apps_obj => "",
  bf_asm_src => "bf-586.s",
  bf_obj => "bf-586.o",
  bn_asm_src => "bn-586.s co-586.s x86-mont.s x86-gf2m.s",
  bn_obj => "bn-586.o co-586.o x86-mont.o x86-gf2m.o",
  bn_ops => "BN_LLONG",
  build_file => "Makefile",
  build_scheme => [ "unified", "unix" ],
  cast_asm_src => "c_enc.c",
  cast_obj => "c_enc.o",
  cflags => "-pthread",
  chacha_asm_src => "chacha-x86.s",
  chacha_obj => "chacha-x86.o",
  cmll_asm_src => "cmll-x86.s",
  cmll_obj => "cmll-x86.o",
  cppflags => "",
  cpuid_asm_src => "x86cpuid.s",
  cpuid_obj => "x86cpuid.o",
  defines => [  ],
  des_asm_src => "des-586.s crypt586.s",
  des_obj => "des-586.o crypt586.o",
  disable => [  ],
  dso_extension => ".so",
  dso_scheme => "dlfcn",
  ec_asm_src => "ecp_nistz256.c ecp_nistz256-x86.s",
  ec_obj => "ecp_nistz256.o ecp_nistz256-x86.o",
  enable => [  ],
  ex_libs => "-lsocket -lnsl -ldl -pthread",
  exe_extension => "",
  includes => [  ],
  keccak1600_asm_src => "keccak1600.c",
  keccak1600_obj => "keccak1600.o",
  lflags => "",
  lib_cflags => "",
  lib_cppflags => "-DFILIO_H -DL_ENDIAN",
  lib_defines => [  ],
  md5_asm_src => "md5-586.s",
  md5_obj => "md5-586.o",
  modes_asm_src => "ghash-x86.s",
  modes_obj => "ghash-x86.o",
  module_cflags => "-fPIC",
  module_cxxflags => "",
  module_ldflags => "-shared -static-libgcc -Wl,-Bsymbolic",
  padlock_asm_src => "e_padlock-x86.s",
  padlock_obj => "e_padlock-x86.o",
  perlasm_scheme => "elf",
  poly1305_asm_src => "poly1305-x86.s",
  poly1305_obj => "poly1305-x86.o",
  rc4_asm_src => "rc4-586.s",
  rc4_obj => "rc4-586.o",
  rc5_asm_src => "rc5-586.s",
  rc5_obj => "rc5-586.o",
  rmd160_asm_src => "rmd-586.s",
  rmd160_obj => "rmd-586.o",
  sha1_asm_src => "sha1-586.s sha256-586.s sha512-586.s",
  sha1_obj => "sha1-586.o sha256-586.o sha512-586.o",
  shared_cflag => "-fPIC",
  shared_defflag => "-Wl,-M,",
  shared_defines => [  ],
  shared_extension => ".so.\$(SHLIB_VERSION_NUMBER)",
  shared_extension_simple => ".so",
  shared_ldflag => "-shared -static-libgcc -Wl,-Bsymbolic",
  shared_rcflag => "",
  shared_sonameflag => "-Wl,-h,",
  shared_target => "self",
  template => "1",
  thread_defines => [  ],
  thread_scheme => "pthreads",
  unistd => "<unistd.h>",
  uplink_aux_src => "",
  uplink_obj => "",
  wp_asm_src => "wp_block.c wp-mmx.s",
  wp_obj => "wp_block.o wp-mmx.o",
);

our %available_protocols = (
  tls => [ "ssl3", "tls1", "tls1_1", "tls1_2", "tls1_3" ],
  dtls => [ "dtls1", "dtls1_2" ],
);

our @disablables = (
  "afalgeng",
  "aria",
  "asan",
  "asm",
  "async",
  "autoalginit",
  "autoerrinit",
  "autoload-config",
  "bf",
  "blake2",
  "buildtest-c\\+\\+",
  "camellia",
  "capieng",
  "cast",
  "chacha",
  "cmac",
  "cms",
  "comp",
  "crypto-mdebug",
  "crypto-mdebug-backtrace",
  "ct",
  "deprecated",
  "des",
  "devcryptoeng",
  "dgram",
  "dh",
  "dsa",
  "dso",
  "dtls",
  "dynamic-engine",
  "ec",
  "ec2m",
  "ecdh",
  "ecdsa",
  "ec_nistp_64_gcc_128",
  "egd",
  "engine",
  "err",
  "external-tests",
  "filenames",
  "fuzz-libfuzzer",
  "fuzz-afl",
  "gost",
  "heartbeats",
  "hw(-.+)?",
  "idea",
  "makedepend",
  "md2",
  "md4",
  "mdc2",
  "msan",
  "multiblock",
  "nextprotoneg",
  "pinshared",
  "ocb",
  "ocsp",
  "pic",
  "poly1305",
  "posix-io",
  "psk",
  "rc2",
  "rc4",
  "rc5",
  "rdrand",
  "rfc3779",
  "rmd160",
  "scrypt",
  "sctp",
  "seed",
  "shared",
  "siphash",
  "sm2",
  "sm3",
  "sm4",
  "sock",
  "srp",
  "srtp",
  "sse2",
  "ssl",
  "ssl-trace",
  "static-engine",
  "stdio",
  "tests",
  "threads",
  "tls",
  "ts",
  "ubsan",
  "ui-console",
  "unit-test",
  "whirlpool",
  "weak-ssl-ciphers",
  "zlib",
  "zlib-dynamic",
  "ssl3",
  "ssl3-method",
  "tls1",
  "tls1-method",
  "tls1_1",
  "tls1_1-method",
  "tls1_2",
  "tls1_2-method",
  "tls1_3",
  "dtls1",
  "dtls1-method",
  "dtls1_2",
  "dtls1_2-method",
);

our %disabled = (
  "afalgeng" => "option",
  "asan" => "default",
  "buildtest-c++" => "default",
  "comp" => "option",
  "crypto-mdebug" => "default",
  "crypto-mdebug-backtrace" => "default",
  "devcryptoeng" => "default",
  "dynamic-engine" => "cascade",
  "ec_nistp_64_gcc_128" => "default",
  "egd" => "default",
  "external-tests" => "default",
  "fuzz-afl" => "default",
  "fuzz-libfuzzer" => "default",
  "heartbeats" => "default",
  "md2" => "default",
  "msan" => "default",
  "rc5" => "default",
  "sctp" => "default",
  "shared" => "option",
  "ssl3" => "default",
  "ssl3-method" => "default",
  "ubsan" => "default",
  "unit-test" => "default",
  "weak-ssl-ciphers" => "default",
  "zlib" => "default",
  "zlib-dynamic" => "default",
);

our %withargs = (
);

our %unified_info = (
    "depends" =>
        {
            "" =>
                [
                    "include/crypto/bn_conf.h",
                    "include/crypto/dso_conf.h",
                    "include/openssl/opensslconf.h",
                ],
            "apps/asn1pars.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/ca.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/ciphers.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/cms.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/crl.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/crl2p7.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/dgst.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/dhparam.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/dsa.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/dsaparam.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/ec.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/ecparam.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/enc.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/engine.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/errstr.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/gendsa.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/genpkey.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/genrsa.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/nseq.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/ocsp.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/openssl" =>
                [
                    "apps/libapps.a",
                    "libssl",
                ],
            "apps/openssl.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/passwd.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/pkcs12.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/pkcs7.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/pkcs8.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/pkey.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/pkeyparam.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/pkeyutl.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/prime.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/progs.h" =>
                [
                    "configdata.pm",
                ],
            "apps/rand.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/rehash.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/req.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/rsa.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/rsautl.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/s_client.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/s_server.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/s_time.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/sess_id.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/smime.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/speed.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/spkac.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/srp.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/storeutl.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/ts.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/verify.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/version.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/x509.o" =>
                [
                    "apps/progs.h",
                ],
            "crypto/aes/aes-586.s" =>
                [
                    "crypto/perlasm/x86asm.pl",
                ],
            "crypto/aes/aesni-586.s" =>
                [
                    "crypto/perlasm/x86asm.pl",
                ],
            "crypto/aes/aest4-sparcv9.S" =>
                [
                    "crypto/perlasm/sparcv9_modes.pl",
                ],
            "crypto/aes/vpaes-586.s" =>
                [
                    "crypto/perlasm/x86asm.pl",
                ],
            "crypto/bf/bf-586.s" =>
                [
                    "crypto/perlasm/cbc.pl",
                    "crypto/perlasm/x86asm.pl",
                ],
            "crypto/bn/bn-586.s" =>
                [
                    "crypto/perlasm/x86asm.pl",
                ],
            "crypto/bn/co-586.s" =>
                [
                    "crypto/perlasm/x86asm.pl",
                ],
            "crypto/bn/x86-gf2m.s" =>
                [
                    "crypto/perlasm/x86asm.pl",
                ],
            "crypto/bn/x86-mont.s" =>
                [
                    "crypto/perlasm/x86asm.pl",
                ],
            "crypto/buildinf.h" =>
                [
                    "configdata.pm",
                ],
            "crypto/camellia/cmll-x86.s" =>
                [
                    "crypto/perlasm/x86asm.pl",
                ],
            "crypto/camellia/cmllt4-sparcv9.S" =>
                [
                    "crypto/perlasm/sparcv9_modes.pl",
                ],
            "crypto/cast/cast-586.s" =>
                [
                    "crypto/perlasm/cbc.pl",
                    "crypto/perlasm/x86asm.pl",
                ],
            "crypto/cversion.o" =>
                [
                    "crypto/buildinf.h",
                ],
            "crypto/des/crypt586.s" =>
                [
                    "crypto/perlasm/cbc.pl",
                    "crypto/perlasm/x86asm.pl",
                ],
            "crypto/des/des-586.s" =>
                [
                    "crypto/perlasm/cbc.pl",
                    "crypto/perlasm/x86asm.pl",
                ],
            "crypto/rc4/rc4-586.s" =>
                [
                    "crypto/perlasm/x86asm.pl",
                ],
            "crypto/ripemd/rmd-586.s" =>
                [
                    "crypto/perlasm/x86asm.pl",
                ],
            "crypto/sha/sha1-586.s" =>
                [
                    "crypto/perlasm/x86asm.pl",
                ],
            "crypto/sha/sha256-586.s" =>
                [
                    "crypto/perlasm/x86asm.pl",
                ],
            "crypto/sha/sha512-586.s" =>
                [
                    "crypto/perlasm/x86asm.pl",
                ],
            "crypto/whrlpool/wp-mmx.s" =>
                [
                    "crypto/perlasm/x86asm.pl",
                ],
            "crypto/x86cpuid.s" =>
                [
                    "crypto/perlasm/x86asm.pl",
                ],
            "fuzz/asn1-test" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "fuzz/asn1parse-test" =>
                [
                    "libcrypto",
                ],
            "fuzz/bignum-test" =>
                [
                    "libcrypto",
                ],
            "fuzz/bndiv-test" =>
                [
                    "libcrypto",
                ],
            "fuzz/client-test" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "fuzz/cms-test" =>
                [
                    "libcrypto",
                ],
            "fuzz/conf-test" =>
                [
                    "libcrypto",
                ],
            "fuzz/crl-test" =>
                [
                    "libcrypto",
                ],
            "fuzz/ct-test" =>
                [
                    "libcrypto",
                ],
            "fuzz/server-test" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "fuzz/x509-test" =>
                [
                    "libcrypto",
                ],
            "include/crypto/bn_conf.h" =>
                [
                    "configdata.pm",
                ],
            "include/crypto/dso_conf.h" =>
                [
                    "configdata.pm",
                ],
            "include/openssl/opensslconf.h" =>
                [
                    "configdata.pm",
                ],
            "libcrypto.map" =>
                [
                    "util/libcrypto.num",
                ],
            "libssl" =>
                [
                    "libcrypto",
                ],
            "libssl.map" =>
                [
                    "util/libssl.num",
                ],
            "test/aborttest" =>
                [
                    "libcrypto",
                ],
            "test/afalgtest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/asn1_decode_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/asn1_encode_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/asn1_internal_test" =>
                [
                    "libcrypto.a",
                    "test/libtestutil.a",
                ],
            "test/asn1_string_table_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/asn1_time_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/asynciotest" =>
                [
                    "libcrypto",
                    "libssl",
                    "test/libtestutil.a",
                ],
            "test/asynctest" =>
                [
                    "libcrypto",
                ],
            "test/bad_dtls_test" =>
                [
                    "libcrypto",
                    "libssl",
                    "test/libtestutil.a",
                ],
            "test/bftest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/bio_callback_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/bio_enc_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/bio_memleak_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/bioprinttest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/bntest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/buildtest_c_aes" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_asn1" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_asn1t" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_async" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_bio" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_blowfish" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_bn" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_buffer" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_camellia" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_cast" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_cmac" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_cms" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_conf" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_conf_api" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_crypto" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_ct" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_des" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_dh" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_dsa" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_dtls1" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_e_os2" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_ebcdic" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_ec" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_ecdh" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_ecdsa" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_engine" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_evp" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_hmac" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_idea" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_kdf" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_lhash" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_md4" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_md5" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_mdc2" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_modes" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_obj_mac" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_objects" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_ocsp" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_opensslv" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_ossl_typ" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_pem" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_pem2" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_pkcs12" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_pkcs7" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_rand" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_rand_drbg" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_rc2" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_rc4" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_ripemd" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_rsa" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_safestack" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_seed" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_sha" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_srp" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_srtp" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_ssl" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_ssl2" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_stack" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_store" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_symhacks" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_tls1" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_ts" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_txt_db" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_ui" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_whrlpool" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_x509" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_x509_vfy" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_c_x509v3" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/casttest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/chacha_internal_test" =>
                [
                    "libcrypto.a",
                    "test/libtestutil.a",
                ],
            "test/cipher_overhead_test" =>
                [
                    "libcrypto",
                    "libssl",
                    "test/libtestutil.a",
                ],
            "test/cipherbytes_test" =>
                [
                    "libcrypto",
                    "libssl",
                    "test/libtestutil.a",
                ],
            "test/cipherlist_test" =>
                [
                    "libcrypto",
                    "libssl",
                    "test/libtestutil.a",
                ],
            "test/ciphername_test" =>
                [
                    "libcrypto",
                    "libssl",
                    "test/libtestutil.a",
                ],
            "test/clienthellotest" =>
                [
                    "libcrypto",
                    "libssl",
                    "test/libtestutil.a",
                ],
            "test/cmactest" =>
                [
                    "libcrypto.a",
                    "test/libtestutil.a",
                ],
            "test/cmsapitest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/conf_include_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/constant_time_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/crltest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/ct_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/ctype_internal_test" =>
                [
                    "libcrypto.a",
                    "test/libtestutil.a",
                ],
            "test/curve448_internal_test" =>
                [
                    "libcrypto.a",
                    "test/libtestutil.a",
                ],
            "test/d2i_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/danetest" =>
                [
                    "libcrypto",
                    "libssl",
                    "test/libtestutil.a",
                ],
            "test/destest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/dhtest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/drbg_cavs_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/drbgtest" =>
                [
                    "libcrypto.a",
                    "test/libtestutil.a",
                ],
            "test/dsa_no_digest_size_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/dsatest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/dtls_mtu_test" =>
                [
                    "libcrypto",
                    "libssl",
                    "test/libtestutil.a",
                ],
            "test/dtlstest" =>
                [
                    "libcrypto",
                    "libssl",
                    "test/libtestutil.a",
                ],
            "test/dtlsv1listentest" =>
                [
                    "libssl",
                    "test/libtestutil.a",
                ],
            "test/ec_internal_test" =>
                [
                    "libcrypto.a",
                    "test/libtestutil.a",
                ],
            "test/ecdsatest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/ecstresstest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/ectest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/enginetest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/errtest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/evp_extra_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/evp_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/exdatatest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/exptest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/fatalerrtest" =>
                [
                    "libcrypto",
                    "libssl",
                    "test/libtestutil.a",
                ],
            "test/gmdifftest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/gosttest" =>
                [
                    "libcrypto",
                    "libssl",
                    "test/libtestutil.a",
                ],
            "test/hmactest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/ideatest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/igetest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/lhash_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/libtestutil.a" =>
                [
                    "libcrypto",
                ],
            "test/md2test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/mdc2_internal_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/mdc2test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/memleaktest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/modes_internal_test" =>
                [
                    "libcrypto.a",
                    "test/libtestutil.a",
                ],
            "test/ocspapitest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/packettest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/pbelutest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/pemtest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/pkey_meth_kdf_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/pkey_meth_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/poly1305_internal_test" =>
                [
                    "libcrypto.a",
                    "test/libtestutil.a",
                ],
            "test/rc2test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/rc4test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/rc5test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/rdrand_sanitytest" =>
                [
                    "libcrypto.a",
                    "test/libtestutil.a",
                ],
            "test/recordlentest" =>
                [
                    "libcrypto",
                    "libssl",
                    "test/libtestutil.a",
                ],
            "test/rsa_mp_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/rsa_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/sanitytest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/secmemtest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/servername_test" =>
                [
                    "libcrypto",
                    "libssl",
                    "test/libtestutil.a",
                ],
            "test/siphash_internal_test" =>
                [
                    "libcrypto.a",
                    "test/libtestutil.a",
                ],
            "test/sm2_internal_test" =>
                [
                    "libcrypto.a",
                    "test/libtestutil.a",
                ],
            "test/sm4_internal_test" =>
                [
                    "libcrypto.a",
                    "test/libtestutil.a",
                ],
            "test/srptest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/ssl_cert_table_internal_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/ssl_ctx_test" =>
                [
                    "libcrypto",
                    "libssl",
                    "test/libtestutil.a",
                ],
            "test/ssl_test" =>
                [
                    "libcrypto",
                    "libssl",
                    "test/libtestutil.a",
                ],
            "test/ssl_test_ctx_test" =>
                [
                    "libcrypto",
                    "libssl",
                    "test/libtestutil.a",
                ],
            "test/sslapitest" =>
                [
                    "libcrypto",
                    "libssl",
                    "test/libtestutil.a",
                ],
            "test/sslbuffertest" =>
                [
                    "libcrypto",
                    "libssl",
                    "test/libtestutil.a",
                ],
            "test/sslcorrupttest" =>
                [
                    "libcrypto",
                    "libssl",
                    "test/libtestutil.a",
                ],
            "test/ssltest_old" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/stack_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/sysdefaulttest" =>
                [
                    "libcrypto",
                    "libssl",
                    "test/libtestutil.a",
                ],
            "test/test_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/threadstest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/time_offset_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/tls13ccstest" =>
                [
                    "libcrypto",
                    "libssl",
                    "test/libtestutil.a",
                ],
            "test/tls13encryptiontest" =>
                [
                    "libcrypto",
                    "libssl.a",
                    "test/libtestutil.a",
                ],
            "test/uitest" =>
                [
                    "apps/libapps.a",
                    "libcrypto",
                    "libssl",
                    "test/libtestutil.a",
                ],
            "test/v3ext" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/v3nametest" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/verify_extra_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/versions" =>
                [
                    "libcrypto",
                ],
            "test/wpackettest" =>
                [
                    "libcrypto",
                    "libssl.a",
                    "test/libtestutil.a",
                ],
            "test/x509_check_cert_pkey_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/x509_dup_cert_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/x509_internal_test" =>
                [
                    "libcrypto.a",
                    "test/libtestutil.a",
                ],
            "test/x509_time_test" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
            "test/x509aux" =>
                [
                    "libcrypto",
                    "test/libtestutil.a",
                ],
        },
    "dirinfo" =>
        {
            "apps" =>
                {
                    "products" =>
                        {
                            "bin" =>
                                [
                                    "apps/openssl",
                                ],
                            "lib" =>
                                [
                                    "apps/libapps.a",
                                ],
                            "script" =>
                                [
                                    "apps/CA.pl",
                                    "apps/tsget.pl",
                                ],
                        },
                },
            "crypto" =>
                {
                    "deps" =>
                        [
                            "crypto/cpt_err.o",
                            "crypto/cryptlib.o",
                            "crypto/ctype.o",
                            "crypto/cversion.o",
                            "crypto/ebcdic.o",
                            "crypto/ex_data.o",
                            "crypto/getenv.o",
                            "crypto/init.o",
                            "crypto/mem.o",
                            "crypto/mem_dbg.o",
                            "crypto/mem_sec.o",
                            "crypto/o_dir.o",
                            "crypto/o_fips.o",
                            "crypto/o_fopen.o",
                            "crypto/o_init.o",
                            "crypto/o_str.o",
                            "crypto/o_time.o",
                            "crypto/threads_none.o",
                            "crypto/threads_pthread.o",
                            "crypto/threads_win.o",
                            "crypto/uid.o",
                            "crypto/x86cpuid.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/aes" =>
                {
                    "deps" =>
                        [
                            "crypto/aes/aes_cbc.o",
                            "crypto/aes/aes_cfb.o",
                            "crypto/aes/aes_core.o",
                            "crypto/aes/aes_ecb.o",
                            "crypto/aes/aes_ige.o",
                            "crypto/aes/aes_misc.o",
                            "crypto/aes/aes_ofb.o",
                            "crypto/aes/aes_wrap.o",
                            "crypto/aes/aesni-x86.o",
                            "crypto/aes/vpaes-x86.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/aria" =>
                {
                    "deps" =>
                        [
                            "crypto/aria/aria.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/asn1" =>
                {
                    "deps" =>
                        [
                            "crypto/asn1/a_bitstr.o",
                            "crypto/asn1/a_d2i_fp.o",
                            "crypto/asn1/a_digest.o",
                            "crypto/asn1/a_dup.o",
                            "crypto/asn1/a_gentm.o",
                            "crypto/asn1/a_i2d_fp.o",
                            "crypto/asn1/a_int.o",
                            "crypto/asn1/a_mbstr.o",
                            "crypto/asn1/a_object.o",
                            "crypto/asn1/a_octet.o",
                            "crypto/asn1/a_print.o",
                            "crypto/asn1/a_sign.o",
                            "crypto/asn1/a_strex.o",
                            "crypto/asn1/a_strnid.o",
                            "crypto/asn1/a_time.o",
                            "crypto/asn1/a_type.o",
                            "crypto/asn1/a_utctm.o",
                            "crypto/asn1/a_utf8.o",
                            "crypto/asn1/a_verify.o",
                            "crypto/asn1/ameth_lib.o",
                            "crypto/asn1/asn1_err.o",
                            "crypto/asn1/asn1_gen.o",
                            "crypto/asn1/asn1_item_list.o",
                            "crypto/asn1/asn1_lib.o",
                            "crypto/asn1/asn1_par.o",
                            "crypto/asn1/asn_mime.o",
                            "crypto/asn1/asn_moid.o",
                            "crypto/asn1/asn_mstbl.o",
                            "crypto/asn1/asn_pack.o",
                            "crypto/asn1/bio_asn1.o",
                            "crypto/asn1/bio_ndef.o",
                            "crypto/asn1/d2i_pr.o",
                            "crypto/asn1/d2i_pu.o",
                            "crypto/asn1/evp_asn1.o",
                            "crypto/asn1/f_int.o",
                            "crypto/asn1/f_string.o",
                            "crypto/asn1/i2d_pr.o",
                            "crypto/asn1/i2d_pu.o",
                            "crypto/asn1/n_pkey.o",
                            "crypto/asn1/nsseq.o",
                            "crypto/asn1/p5_pbe.o",
                            "crypto/asn1/p5_pbev2.o",
                            "crypto/asn1/p5_scrypt.o",
                            "crypto/asn1/p8_pkey.o",
                            "crypto/asn1/t_bitst.o",
                            "crypto/asn1/t_pkey.o",
                            "crypto/asn1/t_spki.o",
                            "crypto/asn1/tasn_dec.o",
                            "crypto/asn1/tasn_enc.o",
                            "crypto/asn1/tasn_fre.o",
                            "crypto/asn1/tasn_new.o",
                            "crypto/asn1/tasn_prn.o",
                            "crypto/asn1/tasn_scn.o",
                            "crypto/asn1/tasn_typ.o",
                            "crypto/asn1/tasn_utl.o",
                            "crypto/asn1/x_algor.o",
                            "crypto/asn1/x_bignum.o",
                            "crypto/asn1/x_info.o",
                            "crypto/asn1/x_int64.o",
                            "crypto/asn1/x_long.o",
                            "crypto/asn1/x_pkey.o",
                            "crypto/asn1/x_sig.o",
                            "crypto/asn1/x_spki.o",
                            "crypto/asn1/x_val.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/async" =>
                {
                    "deps" =>
                        [
                            "crypto/async/async.o",
                            "crypto/async/async_err.o",
                            "crypto/async/async_wait.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/async/arch" =>
                {
                    "deps" =>
                        [
                            "crypto/async/arch/async_null.o",
                            "crypto/async/arch/async_posix.o",
                            "crypto/async/arch/async_win.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/bf" =>
                {
                    "deps" =>
                        [
                            "crypto/bf/bf-586.o",
                            "crypto/bf/bf_cfb64.o",
                            "crypto/bf/bf_ecb.o",
                            "crypto/bf/bf_ofb64.o",
                            "crypto/bf/bf_skey.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/bio" =>
                {
                    "deps" =>
                        [
                            "crypto/bio/b_addr.o",
                            "crypto/bio/b_dump.o",
                            "crypto/bio/b_print.o",
                            "crypto/bio/b_sock.o",
                            "crypto/bio/b_sock2.o",
                            "crypto/bio/bf_buff.o",
                            "crypto/bio/bf_lbuf.o",
                            "crypto/bio/bf_nbio.o",
                            "crypto/bio/bf_null.o",
                            "crypto/bio/bio_cb.o",
                            "crypto/bio/bio_err.o",
                            "crypto/bio/bio_lib.o",
                            "crypto/bio/bio_meth.o",
                            "crypto/bio/bss_acpt.o",
                            "crypto/bio/bss_bio.o",
                            "crypto/bio/bss_conn.o",
                            "crypto/bio/bss_dgram.o",
                            "crypto/bio/bss_fd.o",
                            "crypto/bio/bss_file.o",
                            "crypto/bio/bss_log.o",
                            "crypto/bio/bss_mem.o",
                            "crypto/bio/bss_null.o",
                            "crypto/bio/bss_sock.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/blake2" =>
                {
                    "deps" =>
                        [
                            "crypto/blake2/blake2b.o",
                            "crypto/blake2/blake2s.o",
                            "crypto/blake2/m_blake2b.o",
                            "crypto/blake2/m_blake2s.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/bn" =>
                {
                    "deps" =>
                        [
                            "crypto/bn/bn-586.o",
                            "crypto/bn/bn_add.o",
                            "crypto/bn/bn_blind.o",
                            "crypto/bn/bn_const.o",
                            "crypto/bn/bn_ctx.o",
                            "crypto/bn/bn_depr.o",
                            "crypto/bn/bn_dh.o",
                            "crypto/bn/bn_div.o",
                            "crypto/bn/bn_err.o",
                            "crypto/bn/bn_exp.o",
                            "crypto/bn/bn_exp2.o",
                            "crypto/bn/bn_gcd.o",
                            "crypto/bn/bn_gf2m.o",
                            "crypto/bn/bn_intern.o",
                            "crypto/bn/bn_kron.o",
                            "crypto/bn/bn_lib.o",
                            "crypto/bn/bn_mod.o",
                            "crypto/bn/bn_mont.o",
                            "crypto/bn/bn_mpi.o",
                            "crypto/bn/bn_mul.o",
                            "crypto/bn/bn_nist.o",
                            "crypto/bn/bn_prime.o",
                            "crypto/bn/bn_print.o",
                            "crypto/bn/bn_rand.o",
                            "crypto/bn/bn_recp.o",
                            "crypto/bn/bn_shift.o",
                            "crypto/bn/bn_sqr.o",
                            "crypto/bn/bn_sqrt.o",
                            "crypto/bn/bn_srp.o",
                            "crypto/bn/bn_word.o",
                            "crypto/bn/bn_x931p.o",
                            "crypto/bn/co-586.o",
                            "crypto/bn/x86-gf2m.o",
                            "crypto/bn/x86-mont.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/buffer" =>
                {
                    "deps" =>
                        [
                            "crypto/buffer/buf_err.o",
                            "crypto/buffer/buffer.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/camellia" =>
                {
                    "deps" =>
                        [
                            "crypto/camellia/cmll-x86.o",
                            "crypto/camellia/cmll_cfb.o",
                            "crypto/camellia/cmll_ctr.o",
                            "crypto/camellia/cmll_ecb.o",
                            "crypto/camellia/cmll_ofb.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/cast" =>
                {
                    "deps" =>
                        [
                            "crypto/cast/c_cfb64.o",
                            "crypto/cast/c_ecb.o",
                            "crypto/cast/c_enc.o",
                            "crypto/cast/c_ofb64.o",
                            "crypto/cast/c_skey.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/chacha" =>
                {
                    "deps" =>
                        [
                            "crypto/chacha/chacha-x86.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/cmac" =>
                {
                    "deps" =>
                        [
                            "crypto/cmac/cm_ameth.o",
                            "crypto/cmac/cm_pmeth.o",
                            "crypto/cmac/cmac.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/cms" =>
                {
                    "deps" =>
                        [
                            "crypto/cms/cms_asn1.o",
                            "crypto/cms/cms_att.o",
                            "crypto/cms/cms_cd.o",
                            "crypto/cms/cms_dd.o",
                            "crypto/cms/cms_enc.o",
                            "crypto/cms/cms_env.o",
                            "crypto/cms/cms_err.o",
                            "crypto/cms/cms_ess.o",
                            "crypto/cms/cms_io.o",
                            "crypto/cms/cms_kari.o",
                            "crypto/cms/cms_lib.o",
                            "crypto/cms/cms_pwri.o",
                            "crypto/cms/cms_sd.o",
                            "crypto/cms/cms_smime.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/conf" =>
                {
                    "deps" =>
                        [
                            "crypto/conf/conf_api.o",
                            "crypto/conf/conf_def.o",
                            "crypto/conf/conf_err.o",
                            "crypto/conf/conf_lib.o",
                            "crypto/conf/conf_mall.o",
                            "crypto/conf/conf_mod.o",
                            "crypto/conf/conf_sap.o",
                            "crypto/conf/conf_ssl.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/ct" =>
                {
                    "deps" =>
                        [
                            "crypto/ct/ct_b64.o",
                            "crypto/ct/ct_err.o",
                            "crypto/ct/ct_log.o",
                            "crypto/ct/ct_oct.o",
                            "crypto/ct/ct_policy.o",
                            "crypto/ct/ct_prn.o",
                            "crypto/ct/ct_sct.o",
                            "crypto/ct/ct_sct_ctx.o",
                            "crypto/ct/ct_vfy.o",
                            "crypto/ct/ct_x509v3.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/des" =>
                {
                    "deps" =>
                        [
                            "crypto/des/cbc_cksm.o",
                            "crypto/des/cbc_enc.o",
                            "crypto/des/cfb64ede.o",
                            "crypto/des/cfb64enc.o",
                            "crypto/des/cfb_enc.o",
                            "crypto/des/crypt586.o",
                            "crypto/des/des-586.o",
                            "crypto/des/ecb3_enc.o",
                            "crypto/des/ecb_enc.o",
                            "crypto/des/fcrypt.o",
                            "crypto/des/ofb64ede.o",
                            "crypto/des/ofb64enc.o",
                            "crypto/des/ofb_enc.o",
                            "crypto/des/pcbc_enc.o",
                            "crypto/des/qud_cksm.o",
                            "crypto/des/rand_key.o",
                            "crypto/des/set_key.o",
                            "crypto/des/str2key.o",
                            "crypto/des/xcbc_enc.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/dh" =>
                {
                    "deps" =>
                        [
                            "crypto/dh/dh_ameth.o",
                            "crypto/dh/dh_asn1.o",
                            "crypto/dh/dh_check.o",
                            "crypto/dh/dh_depr.o",
                            "crypto/dh/dh_err.o",
                            "crypto/dh/dh_gen.o",
                            "crypto/dh/dh_kdf.o",
                            "crypto/dh/dh_key.o",
                            "crypto/dh/dh_lib.o",
                            "crypto/dh/dh_meth.o",
                            "crypto/dh/dh_pmeth.o",
                            "crypto/dh/dh_prn.o",
                            "crypto/dh/dh_rfc5114.o",
                            "crypto/dh/dh_rfc7919.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/dsa" =>
                {
                    "deps" =>
                        [
                            "crypto/dsa/dsa_ameth.o",
                            "crypto/dsa/dsa_asn1.o",
                            "crypto/dsa/dsa_depr.o",
                            "crypto/dsa/dsa_err.o",
                            "crypto/dsa/dsa_gen.o",
                            "crypto/dsa/dsa_key.o",
                            "crypto/dsa/dsa_lib.o",
                            "crypto/dsa/dsa_meth.o",
                            "crypto/dsa/dsa_ossl.o",
                            "crypto/dsa/dsa_pmeth.o",
                            "crypto/dsa/dsa_prn.o",
                            "crypto/dsa/dsa_sign.o",
                            "crypto/dsa/dsa_vrf.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/dso" =>
                {
                    "deps" =>
                        [
                            "crypto/dso/dso_dl.o",
                            "crypto/dso/dso_dlfcn.o",
                            "crypto/dso/dso_err.o",
                            "crypto/dso/dso_lib.o",
                            "crypto/dso/dso_openssl.o",
                            "crypto/dso/dso_vms.o",
                            "crypto/dso/dso_win32.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/ec" =>
                {
                    "deps" =>
                        [
                            "crypto/ec/curve25519.o",
                            "crypto/ec/ec2_oct.o",
                            "crypto/ec/ec2_smpl.o",
                            "crypto/ec/ec_ameth.o",
                            "crypto/ec/ec_asn1.o",
                            "crypto/ec/ec_check.o",
                            "crypto/ec/ec_curve.o",
                            "crypto/ec/ec_cvt.o",
                            "crypto/ec/ec_err.o",
                            "crypto/ec/ec_key.o",
                            "crypto/ec/ec_kmeth.o",
                            "crypto/ec/ec_lib.o",
                            "crypto/ec/ec_mult.o",
                            "crypto/ec/ec_oct.o",
                            "crypto/ec/ec_pmeth.o",
                            "crypto/ec/ec_print.o",
                            "crypto/ec/ecdh_kdf.o",
                            "crypto/ec/ecdh_ossl.o",
                            "crypto/ec/ecdsa_ossl.o",
                            "crypto/ec/ecdsa_sign.o",
                            "crypto/ec/ecdsa_vrf.o",
                            "crypto/ec/eck_prn.o",
                            "crypto/ec/ecp_mont.o",
                            "crypto/ec/ecp_nist.o",
                            "crypto/ec/ecp_nistp224.o",
                            "crypto/ec/ecp_nistp256.o",
                            "crypto/ec/ecp_nistp521.o",
                            "crypto/ec/ecp_nistputil.o",
                            "crypto/ec/ecp_nistz256-x86.o",
                            "crypto/ec/ecp_nistz256.o",
                            "crypto/ec/ecp_oct.o",
                            "crypto/ec/ecp_smpl.o",
                            "crypto/ec/ecx_meth.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/ec/curve448" =>
                {
                    "deps" =>
                        [
                            "crypto/ec/curve448/curve448.o",
                            "crypto/ec/curve448/curve448_tables.o",
                            "crypto/ec/curve448/eddsa.o",
                            "crypto/ec/curve448/f_generic.o",
                            "crypto/ec/curve448/scalar.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/ec/curve448/arch_32" =>
                {
                    "deps" =>
                        [
                            "crypto/ec/curve448/arch_32/f_impl.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/engine" =>
                {
                    "deps" =>
                        [
                            "crypto/engine/eng_all.o",
                            "crypto/engine/eng_cnf.o",
                            "crypto/engine/eng_ctrl.o",
                            "crypto/engine/eng_dyn.o",
                            "crypto/engine/eng_err.o",
                            "crypto/engine/eng_fat.o",
                            "crypto/engine/eng_init.o",
                            "crypto/engine/eng_lib.o",
                            "crypto/engine/eng_list.o",
                            "crypto/engine/eng_openssl.o",
                            "crypto/engine/eng_pkey.o",
                            "crypto/engine/eng_rdrand.o",
                            "crypto/engine/eng_table.o",
                            "crypto/engine/tb_asnmth.o",
                            "crypto/engine/tb_cipher.o",
                            "crypto/engine/tb_dh.o",
                            "crypto/engine/tb_digest.o",
                            "crypto/engine/tb_dsa.o",
                            "crypto/engine/tb_eckey.o",
                            "crypto/engine/tb_pkmeth.o",
                            "crypto/engine/tb_rand.o",
                            "crypto/engine/tb_rsa.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/err" =>
                {
                    "deps" =>
                        [
                            "crypto/err/err.o",
                            "crypto/err/err_all.o",
                            "crypto/err/err_prn.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/evp" =>
                {
                    "deps" =>
                        [
                            "crypto/evp/bio_b64.o",
                            "crypto/evp/bio_enc.o",
                            "crypto/evp/bio_md.o",
                            "crypto/evp/bio_ok.o",
                            "crypto/evp/c_allc.o",
                            "crypto/evp/c_alld.o",
                            "crypto/evp/cmeth_lib.o",
                            "crypto/evp/digest.o",
                            "crypto/evp/e_aes.o",
                            "crypto/evp/e_aes_cbc_hmac_sha1.o",
                            "crypto/evp/e_aes_cbc_hmac_sha256.o",
                            "crypto/evp/e_aria.o",
                            "crypto/evp/e_bf.o",
                            "crypto/evp/e_camellia.o",
                            "crypto/evp/e_cast.o",
                            "crypto/evp/e_chacha20_poly1305.o",
                            "crypto/evp/e_des.o",
                            "crypto/evp/e_des3.o",
                            "crypto/evp/e_idea.o",
                            "crypto/evp/e_null.o",
                            "crypto/evp/e_old.o",
                            "crypto/evp/e_rc2.o",
                            "crypto/evp/e_rc4.o",
                            "crypto/evp/e_rc4_hmac_md5.o",
                            "crypto/evp/e_rc5.o",
                            "crypto/evp/e_seed.o",
                            "crypto/evp/e_sm4.o",
                            "crypto/evp/e_xcbc_d.o",
                            "crypto/evp/encode.o",
                            "crypto/evp/evp_cnf.o",
                            "crypto/evp/evp_enc.o",
                            "crypto/evp/evp_err.o",
                            "crypto/evp/evp_key.o",
                            "crypto/evp/evp_lib.o",
                            "crypto/evp/evp_pbe.o",
                            "crypto/evp/evp_pkey.o",
                            "crypto/evp/m_md2.o",
                            "crypto/evp/m_md4.o",
                            "crypto/evp/m_md5.o",
                            "crypto/evp/m_md5_sha1.o",
                            "crypto/evp/m_mdc2.o",
                            "crypto/evp/m_null.o",
                            "crypto/evp/m_ripemd.o",
                            "crypto/evp/m_sha1.o",
                            "crypto/evp/m_sha3.o",
                            "crypto/evp/m_sigver.o",
                            "crypto/evp/m_wp.o",
                            "crypto/evp/names.o",
                            "crypto/evp/p5_crpt.o",
                            "crypto/evp/p5_crpt2.o",
                            "crypto/evp/p_dec.o",
                            "crypto/evp/p_enc.o",
                            "crypto/evp/p_lib.o",
                            "crypto/evp/p_open.o",
                            "crypto/evp/p_seal.o",
                            "crypto/evp/p_sign.o",
                            "crypto/evp/p_verify.o",
                            "crypto/evp/pbe_scrypt.o",
                            "crypto/evp/pmeth_fn.o",
                            "crypto/evp/pmeth_gn.o",
                            "crypto/evp/pmeth_lib.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/hmac" =>
                {
                    "deps" =>
                        [
                            "crypto/hmac/hm_ameth.o",
                            "crypto/hmac/hm_pmeth.o",
                            "crypto/hmac/hmac.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/idea" =>
                {
                    "deps" =>
                        [
                            "crypto/idea/i_cbc.o",
                            "crypto/idea/i_cfb64.o",
                            "crypto/idea/i_ecb.o",
                            "crypto/idea/i_ofb64.o",
                            "crypto/idea/i_skey.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/kdf" =>
                {
                    "deps" =>
                        [
                            "crypto/kdf/hkdf.o",
                            "crypto/kdf/kdf_err.o",
                            "crypto/kdf/scrypt.o",
                            "crypto/kdf/tls1_prf.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/lhash" =>
                {
                    "deps" =>
                        [
                            "crypto/lhash/lh_stats.o",
                            "crypto/lhash/lhash.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/md4" =>
                {
                    "deps" =>
                        [
                            "crypto/md4/md4_dgst.o",
                            "crypto/md4/md4_one.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/md5" =>
                {
                    "deps" =>
                        [
                            "crypto/md5/md5-586.o",
                            "crypto/md5/md5_dgst.o",
                            "crypto/md5/md5_one.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/mdc2" =>
                {
                    "deps" =>
                        [
                            "crypto/mdc2/mdc2_one.o",
                            "crypto/mdc2/mdc2dgst.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/modes" =>
                {
                    "deps" =>
                        [
                            "crypto/modes/cbc128.o",
                            "crypto/modes/ccm128.o",
                            "crypto/modes/cfb128.o",
                            "crypto/modes/ctr128.o",
                            "crypto/modes/cts128.o",
                            "crypto/modes/gcm128.o",
                            "crypto/modes/ghash-x86.o",
                            "crypto/modes/ocb128.o",
                            "crypto/modes/ofb128.o",
                            "crypto/modes/wrap128.o",
                            "crypto/modes/xts128.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/objects" =>
                {
                    "deps" =>
                        [
                            "crypto/objects/o_names.o",
                            "crypto/objects/obj_dat.o",
                            "crypto/objects/obj_err.o",
                            "crypto/objects/obj_lib.o",
                            "crypto/objects/obj_xref.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/ocsp" =>
                {
                    "deps" =>
                        [
                            "crypto/ocsp/ocsp_asn.o",
                            "crypto/ocsp/ocsp_cl.o",
                            "crypto/ocsp/ocsp_err.o",
                            "crypto/ocsp/ocsp_ext.o",
                            "crypto/ocsp/ocsp_ht.o",
                            "crypto/ocsp/ocsp_lib.o",
                            "crypto/ocsp/ocsp_prn.o",
                            "crypto/ocsp/ocsp_srv.o",
                            "crypto/ocsp/ocsp_vfy.o",
                            "crypto/ocsp/v3_ocsp.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/pem" =>
                {
                    "deps" =>
                        [
                            "crypto/pem/pem_all.o",
                            "crypto/pem/pem_err.o",
                            "crypto/pem/pem_info.o",
                            "crypto/pem/pem_lib.o",
                            "crypto/pem/pem_oth.o",
                            "crypto/pem/pem_pk8.o",
                            "crypto/pem/pem_pkey.o",
                            "crypto/pem/pem_sign.o",
                            "crypto/pem/pem_x509.o",
                            "crypto/pem/pem_xaux.o",
                            "crypto/pem/pvkfmt.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/pkcs12" =>
                {
                    "deps" =>
                        [
                            "crypto/pkcs12/p12_add.o",
                            "crypto/pkcs12/p12_asn.o",
                            "crypto/pkcs12/p12_attr.o",
                            "crypto/pkcs12/p12_crpt.o",
                            "crypto/pkcs12/p12_crt.o",
                            "crypto/pkcs12/p12_decr.o",
                            "crypto/pkcs12/p12_init.o",
                            "crypto/pkcs12/p12_key.o",
                            "crypto/pkcs12/p12_kiss.o",
                            "crypto/pkcs12/p12_mutl.o",
                            "crypto/pkcs12/p12_npas.o",
                            "crypto/pkcs12/p12_p8d.o",
                            "crypto/pkcs12/p12_p8e.o",
                            "crypto/pkcs12/p12_sbag.o",
                            "crypto/pkcs12/p12_utl.o",
                            "crypto/pkcs12/pk12err.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/pkcs7" =>
                {
                    "deps" =>
                        [
                            "crypto/pkcs7/bio_pk7.o",
                            "crypto/pkcs7/pk7_asn1.o",
                            "crypto/pkcs7/pk7_attr.o",
                            "crypto/pkcs7/pk7_doit.o",
                            "crypto/pkcs7/pk7_lib.o",
                            "crypto/pkcs7/pk7_mime.o",
                            "crypto/pkcs7/pk7_smime.o",
                            "crypto/pkcs7/pkcs7err.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/poly1305" =>
                {
                    "deps" =>
                        [
                            "crypto/poly1305/poly1305-x86.o",
                            "crypto/poly1305/poly1305.o",
                            "crypto/poly1305/poly1305_ameth.o",
                            "crypto/poly1305/poly1305_pmeth.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/rand" =>
                {
                    "deps" =>
                        [
                            "crypto/rand/drbg_ctr.o",
                            "crypto/rand/drbg_lib.o",
                            "crypto/rand/rand_egd.o",
                            "crypto/rand/rand_err.o",
                            "crypto/rand/rand_lib.o",
                            "crypto/rand/rand_unix.o",
                            "crypto/rand/rand_vms.o",
                            "crypto/rand/rand_win.o",
                            "crypto/rand/randfile.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/rc2" =>
                {
                    "deps" =>
                        [
                            "crypto/rc2/rc2_cbc.o",
                            "crypto/rc2/rc2_ecb.o",
                            "crypto/rc2/rc2_skey.o",
                            "crypto/rc2/rc2cfb64.o",
                            "crypto/rc2/rc2ofb64.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/rc4" =>
                {
                    "deps" =>
                        [
                            "crypto/rc4/rc4-586.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/ripemd" =>
                {
                    "deps" =>
                        [
                            "crypto/ripemd/rmd-586.o",
                            "crypto/ripemd/rmd_dgst.o",
                            "crypto/ripemd/rmd_one.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/rsa" =>
                {
                    "deps" =>
                        [
                            "crypto/rsa/rsa_ameth.o",
                            "crypto/rsa/rsa_asn1.o",
                            "crypto/rsa/rsa_chk.o",
                            "crypto/rsa/rsa_crpt.o",
                            "crypto/rsa/rsa_depr.o",
                            "crypto/rsa/rsa_err.o",
                            "crypto/rsa/rsa_gen.o",
                            "crypto/rsa/rsa_lib.o",
                            "crypto/rsa/rsa_meth.o",
                            "crypto/rsa/rsa_mp.o",
                            "crypto/rsa/rsa_none.o",
                            "crypto/rsa/rsa_oaep.o",
                            "crypto/rsa/rsa_ossl.o",
                            "crypto/rsa/rsa_pk1.o",
                            "crypto/rsa/rsa_pmeth.o",
                            "crypto/rsa/rsa_prn.o",
                            "crypto/rsa/rsa_pss.o",
                            "crypto/rsa/rsa_saos.o",
                            "crypto/rsa/rsa_sign.o",
                            "crypto/rsa/rsa_ssl.o",
                            "crypto/rsa/rsa_x931.o",
                            "crypto/rsa/rsa_x931g.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/seed" =>
                {
                    "deps" =>
                        [
                            "crypto/seed/seed.o",
                            "crypto/seed/seed_cbc.o",
                            "crypto/seed/seed_cfb.o",
                            "crypto/seed/seed_ecb.o",
                            "crypto/seed/seed_ofb.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/sha" =>
                {
                    "deps" =>
                        [
                            "crypto/sha/keccak1600.o",
                            "crypto/sha/sha1-586.o",
                            "crypto/sha/sha1_one.o",
                            "crypto/sha/sha1dgst.o",
                            "crypto/sha/sha256-586.o",
                            "crypto/sha/sha256.o",
                            "crypto/sha/sha512-586.o",
                            "crypto/sha/sha512.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/siphash" =>
                {
                    "deps" =>
                        [
                            "crypto/siphash/siphash.o",
                            "crypto/siphash/siphash_ameth.o",
                            "crypto/siphash/siphash_pmeth.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/sm2" =>
                {
                    "deps" =>
                        [
                            "crypto/sm2/sm2_crypt.o",
                            "crypto/sm2/sm2_err.o",
                            "crypto/sm2/sm2_pmeth.o",
                            "crypto/sm2/sm2_sign.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/sm3" =>
                {
                    "deps" =>
                        [
                            "crypto/sm3/m_sm3.o",
                            "crypto/sm3/sm3.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/sm4" =>
                {
                    "deps" =>
                        [
                            "crypto/sm4/sm4.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/srp" =>
                {
                    "deps" =>
                        [
                            "crypto/srp/srp_lib.o",
                            "crypto/srp/srp_vfy.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/stack" =>
                {
                    "deps" =>
                        [
                            "crypto/stack/stack.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/store" =>
                {
                    "deps" =>
                        [
                            "crypto/store/loader_file.o",
                            "crypto/store/store_err.o",
                            "crypto/store/store_init.o",
                            "crypto/store/store_lib.o",
                            "crypto/store/store_register.o",
                            "crypto/store/store_strings.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/ts" =>
                {
                    "deps" =>
                        [
                            "crypto/ts/ts_asn1.o",
                            "crypto/ts/ts_conf.o",
                            "crypto/ts/ts_err.o",
                            "crypto/ts/ts_lib.o",
                            "crypto/ts/ts_req_print.o",
                            "crypto/ts/ts_req_utils.o",
                            "crypto/ts/ts_rsp_print.o",
                            "crypto/ts/ts_rsp_sign.o",
                            "crypto/ts/ts_rsp_utils.o",
                            "crypto/ts/ts_rsp_verify.o",
                            "crypto/ts/ts_verify_ctx.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/txt_db" =>
                {
                    "deps" =>
                        [
                            "crypto/txt_db/txt_db.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/ui" =>
                {
                    "deps" =>
                        [
                            "crypto/ui/ui_err.o",
                            "crypto/ui/ui_lib.o",
                            "crypto/ui/ui_null.o",
                            "crypto/ui/ui_openssl.o",
                            "crypto/ui/ui_util.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/whrlpool" =>
                {
                    "deps" =>
                        [
                            "crypto/whrlpool/wp-mmx.o",
                            "crypto/whrlpool/wp_block.o",
                            "crypto/whrlpool/wp_dgst.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/x509" =>
                {
                    "deps" =>
                        [
                            "crypto/x509/by_dir.o",
                            "crypto/x509/by_file.o",
                            "crypto/x509/t_crl.o",
                            "crypto/x509/t_req.o",
                            "crypto/x509/t_x509.o",
                            "crypto/x509/x509_att.o",
                            "crypto/x509/x509_cmp.o",
                            "crypto/x509/x509_d2.o",
                            "crypto/x509/x509_def.o",
                            "crypto/x509/x509_err.o",
                            "crypto/x509/x509_ext.o",
                            "crypto/x509/x509_lu.o",
                            "crypto/x509/x509_meth.o",
                            "crypto/x509/x509_obj.o",
                            "crypto/x509/x509_r2x.o",
                            "crypto/x509/x509_req.o",
                            "crypto/x509/x509_set.o",
                            "crypto/x509/x509_trs.o",
                            "crypto/x509/x509_txt.o",
                            "crypto/x509/x509_v3.o",
                            "crypto/x509/x509_vfy.o",
                            "crypto/x509/x509_vpm.o",
                            "crypto/x509/x509cset.o",
                            "crypto/x509/x509name.o",
                            "crypto/x509/x509rset.o",
                            "crypto/x509/x509spki.o",
                            "crypto/x509/x509type.o",
                            "crypto/x509/x_all.o",
                            "crypto/x509/x_attrib.o",
                            "crypto/x509/x_crl.o",
                            "crypto/x509/x_exten.o",
                            "crypto/x509/x_name.o",
                            "crypto/x509/x_pubkey.o",
                            "crypto/x509/x_req.o",
                            "crypto/x509/x_x509.o",
                            "crypto/x509/x_x509a.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "crypto/x509v3" =>
                {
                    "deps" =>
                        [
                            "crypto/x509v3/pcy_cache.o",
                            "crypto/x509v3/pcy_data.o",
                            "crypto/x509v3/pcy_lib.o",
                            "crypto/x509v3/pcy_map.o",
                            "crypto/x509v3/pcy_node.o",
                            "crypto/x509v3/pcy_tree.o",
                            "crypto/x509v3/v3_addr.o",
                            "crypto/x509v3/v3_admis.o",
                            "crypto/x509v3/v3_akey.o",
                            "crypto/x509v3/v3_akeya.o",
                            "crypto/x509v3/v3_alt.o",
                            "crypto/x509v3/v3_asid.o",
                            "crypto/x509v3/v3_bcons.o",
                            "crypto/x509v3/v3_bitst.o",
                            "crypto/x509v3/v3_conf.o",
                            "crypto/x509v3/v3_cpols.o",
                            "crypto/x509v3/v3_crld.o",
                            "crypto/x509v3/v3_enum.o",
                            "crypto/x509v3/v3_extku.o",
                            "crypto/x509v3/v3_genn.o",
                            "crypto/x509v3/v3_ia5.o",
                            "crypto/x509v3/v3_info.o",
                            "crypto/x509v3/v3_int.o",
                            "crypto/x509v3/v3_lib.o",
                            "crypto/x509v3/v3_ncons.o",
                            "crypto/x509v3/v3_pci.o",
                            "crypto/x509v3/v3_pcia.o",
                            "crypto/x509v3/v3_pcons.o",
                            "crypto/x509v3/v3_pku.o",
                            "crypto/x509v3/v3_pmaps.o",
                            "crypto/x509v3/v3_prn.o",
                            "crypto/x509v3/v3_purp.o",
                            "crypto/x509v3/v3_skey.o",
                            "crypto/x509v3/v3_sxnet.o",
                            "crypto/x509v3/v3_tlsf.o",
                            "crypto/x509v3/v3_utl.o",
                            "crypto/x509v3/v3err.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "engines" =>
                {
                    "deps" =>
                        [
                            "engines/e_capi.o",
                            "engines/e_padlock-x86.o",
                            "engines/e_padlock.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libcrypto",
                                ],
                        },
                },
            "fuzz" =>
                {
                    "products" =>
                        {
                            "bin" =>
                                [
                                    "fuzz/asn1-test",
                                    "fuzz/asn1parse-test",
                                    "fuzz/bignum-test",
                                    "fuzz/bndiv-test",
                                    "fuzz/client-test",
                                    "fuzz/cms-test",
                                    "fuzz/conf-test",
                                    "fuzz/crl-test",
                                    "fuzz/ct-test",
                                    "fuzz/server-test",
                                    "fuzz/x509-test",
                                ],
                        },
                },
            "ssl" =>
                {
                    "deps" =>
                        [
                            "ssl/bio_ssl.o",
                            "ssl/d1_lib.o",
                            "ssl/d1_msg.o",
                            "ssl/d1_srtp.o",
                            "ssl/methods.o",
                            "ssl/packet.o",
                            "ssl/pqueue.o",
                            "ssl/s3_cbc.o",
                            "ssl/s3_enc.o",
                            "ssl/s3_lib.o",
                            "ssl/s3_msg.o",
                            "ssl/ssl_asn1.o",
                            "ssl/ssl_cert.o",
                            "ssl/ssl_ciph.o",
                            "ssl/ssl_conf.o",
                            "ssl/ssl_err.o",
                            "ssl/ssl_init.o",
                            "ssl/ssl_lib.o",
                            "ssl/ssl_mcnf.o",
                            "ssl/ssl_rsa.o",
                            "ssl/ssl_sess.o",
                            "ssl/ssl_stat.o",
                            "ssl/ssl_txt.o",
                            "ssl/ssl_utst.o",
                            "ssl/t1_enc.o",
                            "ssl/t1_lib.o",
                            "ssl/t1_trce.o",
                            "ssl/tls13_enc.o",
                            "ssl/tls_srp.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libssl",
                                ],
                        },
                },
            "ssl/record" =>
                {
                    "deps" =>
                        [
                            "ssl/record/dtls1_bitmap.o",
                            "ssl/record/rec_layer_d1.o",
                            "ssl/record/rec_layer_s3.o",
                            "ssl/record/ssl3_buffer.o",
                            "ssl/record/ssl3_record.o",
                            "ssl/record/ssl3_record_tls13.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libssl",
                                ],
                        },
                },
            "ssl/statem" =>
                {
                    "deps" =>
                        [
                            "ssl/statem/extensions.o",
                            "ssl/statem/extensions_clnt.o",
                            "ssl/statem/extensions_cust.o",
                            "ssl/statem/extensions_srvr.o",
                            "ssl/statem/statem.o",
                            "ssl/statem/statem_clnt.o",
                            "ssl/statem/statem_dtls.o",
                            "ssl/statem/statem_lib.o",
                            "ssl/statem/statem_srvr.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "libssl",
                                ],
                        },
                },
            "test/testutil" =>
                {
                    "deps" =>
                        [
                            "test/testutil/basic_output.o",
                            "test/testutil/cb.o",
                            "test/testutil/driver.o",
                            "test/testutil/format_output.o",
                            "test/testutil/main.o",
                            "test/testutil/output_helpers.o",
                            "test/testutil/random.o",
                            "test/testutil/stanza.o",
                            "test/testutil/tap_bio.o",
                            "test/testutil/test_cleanup.o",
                            "test/testutil/tests.o",
                            "test/testutil/testutil_init.o",
                        ],
                    "products" =>
                        {
                            "lib" =>
                                [
                                    "test/libtestutil.a",
                                ],
                        },
                },
            "tools" =>
                {
                    "products" =>
                        {
                            "script" =>
                                [
                                    "tools/c_rehash",
                                ],
                        },
                },
            "util" =>
                {
                    "products" =>
                        {
                            "script" =>
                                [
                                    "util/shlib_wrap.sh",
                                ],
                        },
                },
        },
    "engines" =>
        [
        ],
    "extra" =>
        [
            "crypto/alphacpuid.pl",
            "crypto/arm64cpuid.pl",
            "crypto/armv4cpuid.pl",
            "crypto/ia64cpuid.S",
            "crypto/pariscid.pl",
            "crypto/ppccpuid.pl",
            "crypto/x86_64cpuid.pl",
            "crypto/x86cpuid.pl",
            "ms/applink.c",
            "ms/uplink-x86.pl",
            "ms/uplink.c",
        ],
    "generate" =>
        {
            "apps/progs.h" =>
                [
                    "apps/progs.pl",
                    "\$(APPS_OPENSSL)",
                ],
            "crypto/aes/aes-586.s" =>
                [
                    "crypto/aes/asm/aes-586.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                    "\$(PROCESSOR)",
                ],
            "crypto/aes/aes-armv4.S" =>
                [
                    "crypto/aes/asm/aes-armv4.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/aes/aes-ia64.s" =>
                [
                    "crypto/aes/asm/aes-ia64.S",
                ],
            "crypto/aes/aes-mips.S" =>
                [
                    "crypto/aes/asm/aes-mips.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/aes/aes-parisc.s" =>
                [
                    "crypto/aes/asm/aes-parisc.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/aes/aes-ppc.s" =>
                [
                    "crypto/aes/asm/aes-ppc.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/aes/aes-s390x.S" =>
                [
                    "crypto/aes/asm/aes-s390x.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/aes/aes-sparcv9.S" =>
                [
                    "crypto/aes/asm/aes-sparcv9.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/aes/aes-x86_64.s" =>
                [
                    "crypto/aes/asm/aes-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/aes/aesfx-sparcv9.S" =>
                [
                    "crypto/aes/asm/aesfx-sparcv9.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/aes/aesni-mb-x86_64.s" =>
                [
                    "crypto/aes/asm/aesni-mb-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/aes/aesni-sha1-x86_64.s" =>
                [
                    "crypto/aes/asm/aesni-sha1-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/aes/aesni-sha256-x86_64.s" =>
                [
                    "crypto/aes/asm/aesni-sha256-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/aes/aesni-x86.s" =>
                [
                    "crypto/aes/asm/aesni-x86.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                    "\$(PROCESSOR)",
                ],
            "crypto/aes/aesni-x86_64.s" =>
                [
                    "crypto/aes/asm/aesni-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/aes/aesp8-ppc.s" =>
                [
                    "crypto/aes/asm/aesp8-ppc.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/aes/aest4-sparcv9.S" =>
                [
                    "crypto/aes/asm/aest4-sparcv9.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/aes/aesv8-armx.S" =>
                [
                    "crypto/aes/asm/aesv8-armx.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/aes/bsaes-armv7.S" =>
                [
                    "crypto/aes/asm/bsaes-armv7.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/aes/bsaes-x86_64.s" =>
                [
                    "crypto/aes/asm/bsaes-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/aes/vpaes-armv8.S" =>
                [
                    "crypto/aes/asm/vpaes-armv8.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/aes/vpaes-ppc.s" =>
                [
                    "crypto/aes/asm/vpaes-ppc.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/aes/vpaes-x86.s" =>
                [
                    "crypto/aes/asm/vpaes-x86.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                    "\$(PROCESSOR)",
                ],
            "crypto/aes/vpaes-x86_64.s" =>
                [
                    "crypto/aes/asm/vpaes-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/alphacpuid.s" =>
                [
                    "crypto/alphacpuid.pl",
                ],
            "crypto/arm64cpuid.S" =>
                [
                    "crypto/arm64cpuid.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/armv4cpuid.S" =>
                [
                    "crypto/armv4cpuid.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/bf/bf-586.s" =>
                [
                    "crypto/bf/asm/bf-586.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                    "\$(PROCESSOR)",
                ],
            "crypto/bn/alpha-mont.S" =>
                [
                    "crypto/bn/asm/alpha-mont.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/bn/armv4-gf2m.S" =>
                [
                    "crypto/bn/asm/armv4-gf2m.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/bn/armv4-mont.S" =>
                [
                    "crypto/bn/asm/armv4-mont.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/bn/armv8-mont.S" =>
                [
                    "crypto/bn/asm/armv8-mont.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/bn/bn-586.s" =>
                [
                    "crypto/bn/asm/bn-586.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                    "\$(PROCESSOR)",
                ],
            "crypto/bn/bn-ia64.s" =>
                [
                    "crypto/bn/asm/ia64.S",
                ],
            "crypto/bn/bn-mips.S" =>
                [
                    "crypto/bn/asm/mips.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/bn/bn-ppc.s" =>
                [
                    "crypto/bn/asm/ppc.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/bn/co-586.s" =>
                [
                    "crypto/bn/asm/co-586.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                    "\$(PROCESSOR)",
                ],
            "crypto/bn/ia64-mont.s" =>
                [
                    "crypto/bn/asm/ia64-mont.pl",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                ],
            "crypto/bn/mips-mont.S" =>
                [
                    "crypto/bn/asm/mips-mont.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/bn/parisc-mont.s" =>
                [
                    "crypto/bn/asm/parisc-mont.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/bn/ppc-mont.s" =>
                [
                    "crypto/bn/asm/ppc-mont.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/bn/ppc64-mont.s" =>
                [
                    "crypto/bn/asm/ppc64-mont.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/bn/rsaz-avx2.s" =>
                [
                    "crypto/bn/asm/rsaz-avx2.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/bn/rsaz-x86_64.s" =>
                [
                    "crypto/bn/asm/rsaz-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/bn/s390x-gf2m.s" =>
                [
                    "crypto/bn/asm/s390x-gf2m.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/bn/s390x-mont.S" =>
                [
                    "crypto/bn/asm/s390x-mont.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/bn/sparct4-mont.S" =>
                [
                    "crypto/bn/asm/sparct4-mont.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/bn/sparcv9-gf2m.S" =>
                [
                    "crypto/bn/asm/sparcv9-gf2m.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/bn/sparcv9-mont.S" =>
                [
                    "crypto/bn/asm/sparcv9-mont.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/bn/sparcv9a-mont.S" =>
                [
                    "crypto/bn/asm/sparcv9a-mont.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/bn/vis3-mont.S" =>
                [
                    "crypto/bn/asm/vis3-mont.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/bn/x86-gf2m.s" =>
                [
                    "crypto/bn/asm/x86-gf2m.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                    "\$(PROCESSOR)",
                ],
            "crypto/bn/x86-mont.s" =>
                [
                    "crypto/bn/asm/x86-mont.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                    "\$(PROCESSOR)",
                ],
            "crypto/bn/x86_64-gf2m.s" =>
                [
                    "crypto/bn/asm/x86_64-gf2m.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/bn/x86_64-mont.s" =>
                [
                    "crypto/bn/asm/x86_64-mont.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/bn/x86_64-mont5.s" =>
                [
                    "crypto/bn/asm/x86_64-mont5.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/buildinf.h" =>
                [
                    "util/mkbuildinf.pl",
                    "\"\$(CC)",
                    "\$(LIB_CFLAGS)",
                    "\$(CPPFLAGS_Q)\"",
                    "\"\$(PLATFORM)\"",
                ],
            "crypto/camellia/cmll-x86.s" =>
                [
                    "crypto/camellia/asm/cmll-x86.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                    "\$(PROCESSOR)",
                ],
            "crypto/camellia/cmll-x86_64.s" =>
                [
                    "crypto/camellia/asm/cmll-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/camellia/cmllt4-sparcv9.S" =>
                [
                    "crypto/camellia/asm/cmllt4-sparcv9.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/cast/cast-586.s" =>
                [
                    "crypto/cast/asm/cast-586.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                    "\$(PROCESSOR)",
                ],
            "crypto/chacha/chacha-armv4.S" =>
                [
                    "crypto/chacha/asm/chacha-armv4.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/chacha/chacha-armv8.S" =>
                [
                    "crypto/chacha/asm/chacha-armv8.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/chacha/chacha-ppc.s" =>
                [
                    "crypto/chacha/asm/chacha-ppc.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/chacha/chacha-s390x.S" =>
                [
                    "crypto/chacha/asm/chacha-s390x.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/chacha/chacha-x86.s" =>
                [
                    "crypto/chacha/asm/chacha-x86.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                    "\$(PROCESSOR)",
                ],
            "crypto/chacha/chacha-x86_64.s" =>
                [
                    "crypto/chacha/asm/chacha-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/des/crypt586.s" =>
                [
                    "crypto/des/asm/crypt586.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                ],
            "crypto/des/des-586.s" =>
                [
                    "crypto/des/asm/des-586.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                ],
            "crypto/des/des_enc-sparc.S" =>
                [
                    "crypto/des/asm/des_enc.m4",
                ],
            "crypto/des/dest4-sparcv9.S" =>
                [
                    "crypto/des/asm/dest4-sparcv9.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/ec/ecp_nistz256-armv4.S" =>
                [
                    "crypto/ec/asm/ecp_nistz256-armv4.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/ec/ecp_nistz256-armv8.S" =>
                [
                    "crypto/ec/asm/ecp_nistz256-armv8.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/ec/ecp_nistz256-avx2.s" =>
                [
                    "crypto/ec/asm/ecp_nistz256-avx2.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/ec/ecp_nistz256-ppc64.s" =>
                [
                    "crypto/ec/asm/ecp_nistz256-ppc64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/ec/ecp_nistz256-sparcv9.S" =>
                [
                    "crypto/ec/asm/ecp_nistz256-sparcv9.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/ec/ecp_nistz256-x86.s" =>
                [
                    "crypto/ec/asm/ecp_nistz256-x86.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                    "\$(PROCESSOR)",
                ],
            "crypto/ec/ecp_nistz256-x86_64.s" =>
                [
                    "crypto/ec/asm/ecp_nistz256-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/ec/x25519-ppc64.s" =>
                [
                    "crypto/ec/asm/x25519-ppc64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/ec/x25519-x86_64.s" =>
                [
                    "crypto/ec/asm/x25519-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/ia64cpuid.s" =>
                [
                    "crypto/ia64cpuid.S",
                ],
            "crypto/md5/md5-586.s" =>
                [
                    "crypto/md5/asm/md5-586.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                ],
            "crypto/md5/md5-sparcv9.S" =>
                [
                    "crypto/md5/asm/md5-sparcv9.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/md5/md5-x86_64.s" =>
                [
                    "crypto/md5/asm/md5-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/modes/aesni-gcm-x86_64.s" =>
                [
                    "crypto/modes/asm/aesni-gcm-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/modes/ghash-alpha.S" =>
                [
                    "crypto/modes/asm/ghash-alpha.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/modes/ghash-armv4.S" =>
                [
                    "crypto/modes/asm/ghash-armv4.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/modes/ghash-ia64.s" =>
                [
                    "crypto/modes/asm/ghash-ia64.pl",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                ],
            "crypto/modes/ghash-parisc.s" =>
                [
                    "crypto/modes/asm/ghash-parisc.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/modes/ghash-s390x.S" =>
                [
                    "crypto/modes/asm/ghash-s390x.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/modes/ghash-sparcv9.S" =>
                [
                    "crypto/modes/asm/ghash-sparcv9.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/modes/ghash-x86.s" =>
                [
                    "crypto/modes/asm/ghash-x86.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                    "\$(PROCESSOR)",
                ],
            "crypto/modes/ghash-x86_64.s" =>
                [
                    "crypto/modes/asm/ghash-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/modes/ghashp8-ppc.s" =>
                [
                    "crypto/modes/asm/ghashp8-ppc.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/modes/ghashv8-armx.S" =>
                [
                    "crypto/modes/asm/ghashv8-armx.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/pariscid.s" =>
                [
                    "crypto/pariscid.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/poly1305/poly1305-armv4.S" =>
                [
                    "crypto/poly1305/asm/poly1305-armv4.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/poly1305/poly1305-armv8.S" =>
                [
                    "crypto/poly1305/asm/poly1305-armv8.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/poly1305/poly1305-mips.S" =>
                [
                    "crypto/poly1305/asm/poly1305-mips.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/poly1305/poly1305-ppc.s" =>
                [
                    "crypto/poly1305/asm/poly1305-ppc.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/poly1305/poly1305-ppcfp.s" =>
                [
                    "crypto/poly1305/asm/poly1305-ppcfp.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/poly1305/poly1305-s390x.S" =>
                [
                    "crypto/poly1305/asm/poly1305-s390x.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/poly1305/poly1305-sparcv9.S" =>
                [
                    "crypto/poly1305/asm/poly1305-sparcv9.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/poly1305/poly1305-x86.s" =>
                [
                    "crypto/poly1305/asm/poly1305-x86.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                    "\$(PROCESSOR)",
                ],
            "crypto/poly1305/poly1305-x86_64.s" =>
                [
                    "crypto/poly1305/asm/poly1305-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/ppccpuid.s" =>
                [
                    "crypto/ppccpuid.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/rc4/rc4-586.s" =>
                [
                    "crypto/rc4/asm/rc4-586.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                    "\$(PROCESSOR)",
                ],
            "crypto/rc4/rc4-md5-x86_64.s" =>
                [
                    "crypto/rc4/asm/rc4-md5-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/rc4/rc4-parisc.s" =>
                [
                    "crypto/rc4/asm/rc4-parisc.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/rc4/rc4-s390x.s" =>
                [
                    "crypto/rc4/asm/rc4-s390x.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/rc4/rc4-x86_64.s" =>
                [
                    "crypto/rc4/asm/rc4-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/ripemd/rmd-586.s" =>
                [
                    "crypto/ripemd/asm/rmd-586.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                ],
            "crypto/s390xcpuid.S" =>
                [
                    "crypto/s390xcpuid.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/keccak1600-armv4.S" =>
                [
                    "crypto/sha/asm/keccak1600-armv4.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/keccak1600-armv8.S" =>
                [
                    "crypto/sha/asm/keccak1600-armv8.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/keccak1600-ppc64.s" =>
                [
                    "crypto/sha/asm/keccak1600-ppc64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/keccak1600-s390x.S" =>
                [
                    "crypto/sha/asm/keccak1600-s390x.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/keccak1600-x86_64.s" =>
                [
                    "crypto/sha/asm/keccak1600-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha1-586.s" =>
                [
                    "crypto/sha/asm/sha1-586.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                    "\$(PROCESSOR)",
                ],
            "crypto/sha/sha1-alpha.S" =>
                [
                    "crypto/sha/asm/sha1-alpha.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha1-armv4-large.S" =>
                [
                    "crypto/sha/asm/sha1-armv4-large.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha1-armv8.S" =>
                [
                    "crypto/sha/asm/sha1-armv8.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha1-ia64.s" =>
                [
                    "crypto/sha/asm/sha1-ia64.pl",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                ],
            "crypto/sha/sha1-mb-x86_64.s" =>
                [
                    "crypto/sha/asm/sha1-mb-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha1-mips.S" =>
                [
                    "crypto/sha/asm/sha1-mips.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha1-parisc.s" =>
                [
                    "crypto/sha/asm/sha1-parisc.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha1-ppc.s" =>
                [
                    "crypto/sha/asm/sha1-ppc.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha1-s390x.S" =>
                [
                    "crypto/sha/asm/sha1-s390x.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha1-sparcv9.S" =>
                [
                    "crypto/sha/asm/sha1-sparcv9.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha1-x86_64.s" =>
                [
                    "crypto/sha/asm/sha1-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha256-586.s" =>
                [
                    "crypto/sha/asm/sha256-586.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                    "\$(PROCESSOR)",
                ],
            "crypto/sha/sha256-armv4.S" =>
                [
                    "crypto/sha/asm/sha256-armv4.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha256-armv8.S" =>
                [
                    "crypto/sha/asm/sha512-armv8.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha256-ia64.s" =>
                [
                    "crypto/sha/asm/sha512-ia64.pl",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                ],
            "crypto/sha/sha256-mb-x86_64.s" =>
                [
                    "crypto/sha/asm/sha256-mb-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha256-mips.S" =>
                [
                    "crypto/sha/asm/sha512-mips.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha256-parisc.s" =>
                [
                    "crypto/sha/asm/sha512-parisc.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha256-ppc.s" =>
                [
                    "crypto/sha/asm/sha512-ppc.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha256-s390x.S" =>
                [
                    "crypto/sha/asm/sha512-s390x.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha256-sparcv9.S" =>
                [
                    "crypto/sha/asm/sha512-sparcv9.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha256-x86_64.s" =>
                [
                    "crypto/sha/asm/sha512-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha256p8-ppc.s" =>
                [
                    "crypto/sha/asm/sha512p8-ppc.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha512-586.s" =>
                [
                    "crypto/sha/asm/sha512-586.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                    "\$(PROCESSOR)",
                ],
            "crypto/sha/sha512-armv4.S" =>
                [
                    "crypto/sha/asm/sha512-armv4.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha512-armv8.S" =>
                [
                    "crypto/sha/asm/sha512-armv8.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha512-ia64.s" =>
                [
                    "crypto/sha/asm/sha512-ia64.pl",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                ],
            "crypto/sha/sha512-mips.S" =>
                [
                    "crypto/sha/asm/sha512-mips.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha512-parisc.s" =>
                [
                    "crypto/sha/asm/sha512-parisc.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha512-ppc.s" =>
                [
                    "crypto/sha/asm/sha512-ppc.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha512-s390x.S" =>
                [
                    "crypto/sha/asm/sha512-s390x.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha512-sparcv9.S" =>
                [
                    "crypto/sha/asm/sha512-sparcv9.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha512-x86_64.s" =>
                [
                    "crypto/sha/asm/sha512-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/sha/sha512p8-ppc.s" =>
                [
                    "crypto/sha/asm/sha512p8-ppc.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/uplink-ia64.s" =>
                [
                    "ms/uplink-ia64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/uplink-x86.s" =>
                [
                    "ms/uplink-x86.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/uplink-x86_64.s" =>
                [
                    "ms/uplink-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/whrlpool/wp-mmx.s" =>
                [
                    "crypto/whrlpool/asm/wp-mmx.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                    "\$(PROCESSOR)",
                ],
            "crypto/whrlpool/wp-x86_64.s" =>
                [
                    "crypto/whrlpool/asm/wp-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/x86_64cpuid.s" =>
                [
                    "crypto/x86_64cpuid.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/x86cpuid.s" =>
                [
                    "crypto/x86cpuid.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                    "\$(PROCESSOR)",
                ],
            "engines/e_padlock-x86.s" =>
                [
                    "engines/asm/e_padlock-x86.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(LIB_CFLAGS)",
                    "\$(LIB_CPPFLAGS)",
                    "\$(PROCESSOR)",
                ],
            "engines/e_padlock-x86_64.s" =>
                [
                    "engines/asm/e_padlock-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "include/crypto/bn_conf.h" =>
                [
                    "include/crypto/bn_conf.h.in",
                ],
            "include/crypto/dso_conf.h" =>
                [
                    "include/crypto/dso_conf.h.in",
                ],
            "include/openssl/opensslconf.h" =>
                [
                    "include/openssl/opensslconf.h.in",
                ],
            "libcrypto.map" =>
                [
                    "util/mkdef.pl",
                    "crypto",
                    "linux",
                ],
            "libssl.map" =>
                [
                    "util/mkdef.pl",
                    "ssl",
                    "linux",
                ],
            "test/buildtest_aes.c" =>
                [
                    "test/generate_buildtest.pl",
                    "aes",
                ],
            "test/buildtest_asn1.c" =>
                [
                    "test/generate_buildtest.pl",
                    "asn1",
                ],
            "test/buildtest_asn1t.c" =>
                [
                    "test/generate_buildtest.pl",
                    "asn1t",
                ],
            "test/buildtest_async.c" =>
                [
                    "test/generate_buildtest.pl",
                    "async",
                ],
            "test/buildtest_bio.c" =>
                [
                    "test/generate_buildtest.pl",
                    "bio",
                ],
            "test/buildtest_blowfish.c" =>
                [
                    "test/generate_buildtest.pl",
                    "blowfish",
                ],
            "test/buildtest_bn.c" =>
                [
                    "test/generate_buildtest.pl",
                    "bn",
                ],
            "test/buildtest_buffer.c" =>
                [
                    "test/generate_buildtest.pl",
                    "buffer",
                ],
            "test/buildtest_camellia.c" =>
                [
                    "test/generate_buildtest.pl",
                    "camellia",
                ],
            "test/buildtest_cast.c" =>
                [
                    "test/generate_buildtest.pl",
                    "cast",
                ],
            "test/buildtest_cmac.c" =>
                [
                    "test/generate_buildtest.pl",
                    "cmac",
                ],
            "test/buildtest_cms.c" =>
                [
                    "test/generate_buildtest.pl",
                    "cms",
                ],
            "test/buildtest_conf.c" =>
                [
                    "test/generate_buildtest.pl",
                    "conf",
                ],
            "test/buildtest_conf_api.c" =>
                [
                    "test/generate_buildtest.pl",
                    "conf_api",
                ],
            "test/buildtest_crypto.c" =>
                [
                    "test/generate_buildtest.pl",
                    "crypto",
                ],
            "test/buildtest_ct.c" =>
                [
                    "test/generate_buildtest.pl",
                    "ct",
                ],
            "test/buildtest_des.c" =>
                [
                    "test/generate_buildtest.pl",
                    "des",
                ],
            "test/buildtest_dh.c" =>
                [
                    "test/generate_buildtest.pl",
                    "dh",
                ],
            "test/buildtest_dsa.c" =>
                [
                    "test/generate_buildtest.pl",
                    "dsa",
                ],
            "test/buildtest_dtls1.c" =>
                [
                    "test/generate_buildtest.pl",
                    "dtls1",
                ],
            "test/buildtest_e_os2.c" =>
                [
                    "test/generate_buildtest.pl",
                    "e_os2",
                ],
            "test/buildtest_ebcdic.c" =>
                [
                    "test/generate_buildtest.pl",
                    "ebcdic",
                ],
            "test/buildtest_ec.c" =>
                [
                    "test/generate_buildtest.pl",
                    "ec",
                ],
            "test/buildtest_ecdh.c" =>
                [
                    "test/generate_buildtest.pl",
                    "ecdh",
                ],
            "test/buildtest_ecdsa.c" =>
                [
                    "test/generate_buildtest.pl",
                    "ecdsa",
                ],
            "test/buildtest_engine.c" =>
                [
                    "test/generate_buildtest.pl",
                    "engine",
                ],
            "test/buildtest_evp.c" =>
                [
                    "test/generate_buildtest.pl",
                    "evp",
                ],
            "test/buildtest_hmac.c" =>
                [
                    "test/generate_buildtest.pl",
                    "hmac",
                ],
            "test/buildtest_idea.c" =>
                [
                    "test/generate_buildtest.pl",
                    "idea",
                ],
            "test/buildtest_kdf.c" =>
                [
                    "test/generate_buildtest.pl",
                    "kdf",
                ],
            "test/buildtest_lhash.c" =>
                [
                    "test/generate_buildtest.pl",
                    "lhash",
                ],
            "test/buildtest_md4.c" =>
                [
                    "test/generate_buildtest.pl",
                    "md4",
                ],
            "test/buildtest_md5.c" =>
                [
                    "test/generate_buildtest.pl",
                    "md5",
                ],
            "test/buildtest_mdc2.c" =>
                [
                    "test/generate_buildtest.pl",
                    "mdc2",
                ],
            "test/buildtest_modes.c" =>
                [
                    "test/generate_buildtest.pl",
                    "modes",
                ],
            "test/buildtest_obj_mac.c" =>
                [
                    "test/generate_buildtest.pl",
                    "obj_mac",
                ],
            "test/buildtest_objects.c" =>
                [
                    "test/generate_buildtest.pl",
                    "objects",
                ],
            "test/buildtest_ocsp.c" =>
                [
                    "test/generate_buildtest.pl",
                    "ocsp",
                ],
            "test/buildtest_opensslv.c" =>
                [
                    "test/generate_buildtest.pl",
                    "opensslv",
                ],
            "test/buildtest_ossl_typ.c" =>
                [
                    "test/generate_buildtest.pl",
                    "ossl_typ",
                ],
            "test/buildtest_pem.c" =>
                [
                    "test/generate_buildtest.pl",
                    "pem",
                ],
            "test/buildtest_pem2.c" =>
                [
                    "test/generate_buildtest.pl",
                    "pem2",
                ],
            "test/buildtest_pkcs12.c" =>
                [
                    "test/generate_buildtest.pl",
                    "pkcs12",
                ],
            "test/buildtest_pkcs7.c" =>
                [
                    "test/generate_buildtest.pl",
                    "pkcs7",
                ],
            "test/buildtest_rand.c" =>
                [
                    "test/generate_buildtest.pl",
                    "rand",
                ],
            "test/buildtest_rand_drbg.c" =>
                [
                    "test/generate_buildtest.pl",
                    "rand_drbg",
                ],
            "test/buildtest_rc2.c" =>
                [
                    "test/generate_buildtest.pl",
                    "rc2",
                ],
            "test/buildtest_rc4.c" =>
                [
                    "test/generate_buildtest.pl",
                    "rc4",
                ],
            "test/buildtest_ripemd.c" =>
                [
                    "test/generate_buildtest.pl",
                    "ripemd",
                ],
            "test/buildtest_rsa.c" =>
                [
                    "test/generate_buildtest.pl",
                    "rsa",
                ],
            "test/buildtest_safestack.c" =>
                [
                    "test/generate_buildtest.pl",
                    "safestack",
                ],
            "test/buildtest_seed.c" =>
                [
                    "test/generate_buildtest.pl",
                    "seed",
                ],
            "test/buildtest_sha.c" =>
                [
                    "test/generate_buildtest.pl",
                    "sha",
                ],
            "test/buildtest_srp.c" =>
                [
                    "test/generate_buildtest.pl",
                    "srp",
                ],
            "test/buildtest_srtp.c" =>
                [
                    "test/generate_buildtest.pl",
                    "srtp",
                ],
            "test/buildtest_ssl.c" =>
                [
                    "test/generate_buildtest.pl",
                    "ssl",
                ],
            "test/buildtest_ssl2.c" =>
                [
                    "test/generate_buildtest.pl",
                    "ssl2",
                ],
            "test/buildtest_stack.c" =>
                [
                    "test/generate_buildtest.pl",
                    "stack",
                ],
            "test/buildtest_store.c" =>
                [
                    "test/generate_buildtest.pl",
                    "store",
                ],
            "test/buildtest_symhacks.c" =>
                [
                    "test/generate_buildtest.pl",
                    "symhacks",
                ],
            "test/buildtest_tls1.c" =>
                [
                    "test/generate_buildtest.pl",
                    "tls1",
                ],
            "test/buildtest_ts.c" =>
                [
                    "test/generate_buildtest.pl",
                    "ts",
                ],
            "test/buildtest_txt_db.c" =>
                [
                    "test/generate_buildtest.pl",
                    "txt_db",
                ],
            "test/buildtest_ui.c" =>
                [
                    "test/generate_buildtest.pl",
                    "ui",
                ],
            "test/buildtest_whrlpool.c" =>
                [
                    "test/generate_buildtest.pl",
                    "whrlpool",
                ],
            "test/buildtest_x509.c" =>
                [
                    "test/generate_buildtest.pl",
                    "x509",
                ],
            "test/buildtest_x509_vfy.c" =>
                [
                    "test/generate_buildtest.pl",
                    "x509_vfy",
                ],
            "test/buildtest_x509v3.c" =>
                [
                    "test/generate_buildtest.pl",
                    "x509v3",
                ],
        },
    "includes" =>
        {
            "apps/app_rand.o" =>
                [
                    ".",
                    "include",
                ],
            "apps/apps.o" =>
                [
                    ".",
                    "include",
                ],
            "apps/asn1pars.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/bf_prefix.o" =>
                [
                    ".",
                    "include",
                ],
            "apps/ca.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/ciphers.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/cms.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/crl.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/crl2p7.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/dgst.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/dhparam.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/dsa.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/dsaparam.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/ec.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/ecparam.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/enc.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/engine.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/errstr.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/gendsa.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/genpkey.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/genrsa.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/nseq.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/ocsp.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/openssl.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/opt.o" =>
                [
                    ".",
                    "include",
                ],
            "apps/passwd.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/pkcs12.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/pkcs7.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/pkcs8.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/pkey.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/pkeyparam.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/pkeyutl.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/prime.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/progs.h" =>
                [
                    ".",
                ],
            "apps/rand.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/rehash.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/req.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/rsa.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/rsautl.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/s_cb.o" =>
                [
                    ".",
                    "include",
                ],
            "apps/s_client.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/s_server.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/s_socket.o" =>
                [
                    ".",
                    "include",
                ],
            "apps/s_time.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/sess_id.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/smime.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/speed.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/spkac.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/srp.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/storeutl.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/ts.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/verify.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/version.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/x509.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "crypto/aes/aes-armv4.o" =>
                [
                    "crypto",
                ],
            "crypto/aes/aes-mips.o" =>
                [
                    "crypto",
                ],
            "crypto/aes/aes-s390x.o" =>
                [
                    "crypto",
                ],
            "crypto/aes/aes-sparcv9.o" =>
                [
                    "crypto",
                ],
            "crypto/aes/aes_cbc.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/aes/aes_cfb.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/aes/aes_core.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/aes/aes_ecb.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/aes/aes_ige.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/aes/aes_misc.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/aes/aes_ofb.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/aes/aes_wrap.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/aes/aesfx-sparcv9.o" =>
                [
                    "crypto",
                ],
            "crypto/aes/aesni-x86.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/aes/aest4-sparcv9.o" =>
                [
                    "crypto",
                ],
            "crypto/aes/aesv8-armx.o" =>
                [
                    "crypto",
                ],
            "crypto/aes/bsaes-armv7.o" =>
                [
                    "crypto",
                ],
            "crypto/aes/vpaes-x86.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/aria/aria.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/arm64cpuid.o" =>
                [
                    "crypto",
                ],
            "crypto/armv4cpuid.o" =>
                [
                    "crypto",
                ],
            "crypto/asn1/a_bitstr.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/a_d2i_fp.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/a_digest.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/a_dup.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/a_gentm.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/a_i2d_fp.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/a_int.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/a_mbstr.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/a_object.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/a_octet.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/a_print.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/a_sign.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/a_strex.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/a_strnid.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/a_time.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/a_type.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/a_utctm.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/a_utf8.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/a_verify.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/ameth_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/asn1_err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/asn1_gen.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/asn1_item_list.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/asn1_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/asn1_par.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/asn_mime.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/asn_moid.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/asn_mstbl.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/asn_pack.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/bio_asn1.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/bio_ndef.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/d2i_pr.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/d2i_pu.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/evp_asn1.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/f_int.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/f_string.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/i2d_pr.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/i2d_pu.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/n_pkey.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/nsseq.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/p5_pbe.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/p5_pbev2.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/p5_scrypt.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/p8_pkey.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/t_bitst.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/t_pkey.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/t_spki.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/tasn_dec.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/tasn_enc.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/tasn_fre.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/tasn_new.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/tasn_prn.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/tasn_scn.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/tasn_typ.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/tasn_utl.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/x_algor.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/x_bignum.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/x_info.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/x_int64.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/x_long.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/x_pkey.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/x_sig.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/x_spki.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/asn1/x_val.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/async/arch/async_null.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/async/arch/async_posix.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/async/arch/async_win.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/async/async.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/async/async_err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/async/async_wait.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bf/bf-586.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bf/bf_cfb64.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bf/bf_ecb.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bf/bf_ofb64.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bf/bf_skey.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bio/b_addr.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bio/b_dump.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bio/b_print.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bio/b_sock.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bio/b_sock2.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bio/bf_buff.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bio/bf_lbuf.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bio/bf_nbio.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bio/bf_null.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bio/bio_cb.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bio/bio_err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bio/bio_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bio/bio_meth.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bio/bss_acpt.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bio/bss_bio.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bio/bss_conn.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bio/bss_dgram.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bio/bss_fd.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bio/bss_file.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bio/bss_log.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bio/bss_mem.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bio/bss_null.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bio/bss_sock.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/blake2/blake2b.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/blake2/blake2s.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/blake2/m_blake2b.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/blake2/m_blake2s.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/armv4-gf2m.o" =>
                [
                    "crypto",
                ],
            "crypto/bn/armv4-mont.o" =>
                [
                    "crypto",
                ],
            "crypto/bn/bn-586.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn-mips.o" =>
                [
                    "crypto",
                ],
            "crypto/bn/bn_add.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_blind.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_const.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_ctx.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_depr.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_dh.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_div.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_exp.o" =>
                [
                    ".",
                    "include",
                    "crypto",
                ],
            "crypto/bn/bn_exp2.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_gcd.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_gf2m.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_intern.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_kron.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_mod.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_mont.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_mpi.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_mul.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_nist.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_prime.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_print.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_rand.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_recp.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_shift.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_sqr.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_sqrt.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_srp.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_word.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/bn_x931p.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/co-586.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/mips-mont.o" =>
                [
                    "crypto",
                ],
            "crypto/bn/sparct4-mont.o" =>
                [
                    "crypto",
                ],
            "crypto/bn/sparcv9-gf2m.o" =>
                [
                    "crypto",
                ],
            "crypto/bn/sparcv9-mont.o" =>
                [
                    "crypto",
                ],
            "crypto/bn/sparcv9a-mont.o" =>
                [
                    "crypto",
                ],
            "crypto/bn/vis3-mont.o" =>
                [
                    "crypto",
                ],
            "crypto/bn/x86-gf2m.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/bn/x86-mont.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/buffer/buf_err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/buffer/buffer.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/buildinf.h" =>
                [
                    ".",
                ],
            "crypto/camellia/cmll-x86.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/camellia/cmll_cfb.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/camellia/cmll_ctr.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/camellia/cmll_ecb.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/camellia/cmll_ofb.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/camellia/cmllt4-sparcv9.o" =>
                [
                    "crypto",
                ],
            "crypto/cast/c_cfb64.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/cast/c_ecb.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/cast/c_enc.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/cast/c_ofb64.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/cast/c_skey.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/chacha/chacha-armv4.o" =>
                [
                    "crypto",
                ],
            "crypto/chacha/chacha-armv8.o" =>
                [
                    "crypto",
                ],
            "crypto/chacha/chacha-s390x.o" =>
                [
                    "crypto",
                ],
            "crypto/chacha/chacha-x86.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/cmac/cm_ameth.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/cmac/cm_pmeth.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/cmac/cmac.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/cms/cms_asn1.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/cms/cms_att.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/cms/cms_cd.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/cms/cms_dd.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/cms/cms_enc.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/cms/cms_env.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/cms/cms_err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/cms/cms_ess.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/cms/cms_io.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/cms/cms_kari.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/cms/cms_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/cms/cms_pwri.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/cms/cms_sd.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/cms/cms_smime.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/conf/conf_api.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/conf/conf_def.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/conf/conf_err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/conf/conf_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/conf/conf_mall.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/conf/conf_mod.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/conf/conf_sap.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/conf/conf_ssl.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/cpt_err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/cryptlib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ct/ct_b64.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ct/ct_err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ct/ct_log.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ct/ct_oct.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ct/ct_policy.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ct/ct_prn.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ct/ct_sct.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ct/ct_sct_ctx.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ct/ct_vfy.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ct/ct_x509v3.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ctype.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/cversion.o" =>
                [
                    ".",
                    "include",
                    "crypto",
                ],
            "crypto/des/cbc_cksm.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/des/cbc_enc.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/des/cfb64ede.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/des/cfb64enc.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/des/cfb_enc.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/des/crypt586.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/des/des-586.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/des/dest4-sparcv9.o" =>
                [
                    "crypto",
                ],
            "crypto/des/ecb3_enc.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/des/ecb_enc.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/des/fcrypt.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/des/ofb64ede.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/des/ofb64enc.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/des/ofb_enc.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/des/pcbc_enc.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/des/qud_cksm.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/des/rand_key.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/des/set_key.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/des/str2key.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/des/xcbc_enc.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dh/dh_ameth.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dh/dh_asn1.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dh/dh_check.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dh/dh_depr.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dh/dh_err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dh/dh_gen.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dh/dh_kdf.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dh/dh_key.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dh/dh_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dh/dh_meth.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dh/dh_pmeth.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dh/dh_prn.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dh/dh_rfc5114.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dh/dh_rfc7919.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dsa/dsa_ameth.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dsa/dsa_asn1.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dsa/dsa_depr.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dsa/dsa_err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dsa/dsa_gen.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dsa/dsa_key.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dsa/dsa_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dsa/dsa_meth.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dsa/dsa_ossl.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dsa/dsa_pmeth.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dsa/dsa_prn.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dsa/dsa_sign.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dsa/dsa_vrf.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dso/dso_dl.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dso/dso_dlfcn.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dso/dso_err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dso/dso_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dso/dso_openssl.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dso/dso_vms.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/dso/dso_win32.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ebcdic.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/curve25519.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/curve448/arch_32/f_impl.o" =>
                [
                    ".",
                    "include",
                    "crypto/ec/curve448/arch_32",
                    "crypto/ec/curve448",
                ],
            "crypto/ec/curve448/curve448.o" =>
                [
                    ".",
                    "include",
                    "crypto/ec/curve448/arch_32",
                    "crypto/ec/curve448",
                ],
            "crypto/ec/curve448/curve448_tables.o" =>
                [
                    ".",
                    "include",
                    "crypto/ec/curve448/arch_32",
                    "crypto/ec/curve448",
                ],
            "crypto/ec/curve448/eddsa.o" =>
                [
                    ".",
                    "include",
                    "crypto/ec/curve448/arch_32",
                    "crypto/ec/curve448",
                ],
            "crypto/ec/curve448/f_generic.o" =>
                [
                    ".",
                    "include",
                    "crypto/ec/curve448/arch_32",
                    "crypto/ec/curve448",
                ],
            "crypto/ec/curve448/scalar.o" =>
                [
                    ".",
                    "include",
                    "crypto/ec/curve448/arch_32",
                    "crypto/ec/curve448",
                ],
            "crypto/ec/ec2_oct.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ec2_smpl.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ec_ameth.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ec_asn1.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ec_check.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ec_curve.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ec_cvt.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ec_err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ec_key.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ec_kmeth.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ec_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ec_mult.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ec_oct.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ec_pmeth.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ec_print.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ecdh_kdf.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ecdh_ossl.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ecdsa_ossl.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ecdsa_sign.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ecdsa_vrf.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/eck_prn.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ecp_mont.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ecp_nist.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ecp_nistp224.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ecp_nistp256.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ecp_nistp521.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ecp_nistputil.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ecp_nistz256-armv4.o" =>
                [
                    "crypto",
                ],
            "crypto/ec/ecp_nistz256-armv8.o" =>
                [
                    "crypto",
                ],
            "crypto/ec/ecp_nistz256-sparcv9.o" =>
                [
                    "crypto",
                ],
            "crypto/ec/ecp_nistz256-x86.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ecp_nistz256.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ecp_oct.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ecp_smpl.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ec/ecx_meth.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/engine/eng_all.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/engine/eng_cnf.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/engine/eng_ctrl.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/engine/eng_dyn.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/engine/eng_err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/engine/eng_fat.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/engine/eng_init.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/engine/eng_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/engine/eng_list.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/engine/eng_openssl.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/engine/eng_pkey.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/engine/eng_rdrand.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/engine/eng_table.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/engine/tb_asnmth.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/engine/tb_cipher.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/engine/tb_dh.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/engine/tb_digest.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/engine/tb_dsa.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/engine/tb_eckey.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/engine/tb_pkmeth.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/engine/tb_rand.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/engine/tb_rsa.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/err/err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/err/err_all.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/err/err_prn.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/bio_b64.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/bio_enc.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/bio_md.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/bio_ok.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/c_allc.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/c_alld.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/cmeth_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/digest.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/e_aes.o" =>
                [
                    ".",
                    "include",
                    "crypto",
                    "crypto/modes",
                ],
            "crypto/evp/e_aes_cbc_hmac_sha1.o" =>
                [
                    ".",
                    "include",
                    "crypto/modes",
                ],
            "crypto/evp/e_aes_cbc_hmac_sha256.o" =>
                [
                    ".",
                    "include",
                    "crypto/modes",
                ],
            "crypto/evp/e_aria.o" =>
                [
                    ".",
                    "include",
                    "crypto",
                    "crypto/modes",
                ],
            "crypto/evp/e_bf.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/e_camellia.o" =>
                [
                    ".",
                    "include",
                    "crypto",
                    "crypto/modes",
                ],
            "crypto/evp/e_cast.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/e_chacha20_poly1305.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/e_des.o" =>
                [
                    ".",
                    "include",
                    "crypto",
                ],
            "crypto/evp/e_des3.o" =>
                [
                    ".",
                    "include",
                    "crypto",
                ],
            "crypto/evp/e_idea.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/e_null.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/e_old.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/e_rc2.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/e_rc4.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/e_rc4_hmac_md5.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/e_rc5.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/e_seed.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/e_sm4.o" =>
                [
                    ".",
                    "include",
                    "crypto",
                    "crypto/modes",
                ],
            "crypto/evp/e_xcbc_d.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/encode.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/evp_cnf.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/evp_enc.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/evp_err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/evp_key.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/evp_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/evp_pbe.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/evp_pkey.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/m_md2.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/m_md4.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/m_md5.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/m_md5_sha1.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/m_mdc2.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/m_null.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/m_ripemd.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/m_sha1.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/m_sha3.o" =>
                [
                    ".",
                    "include",
                    "crypto",
                ],
            "crypto/evp/m_sigver.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/m_wp.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/names.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/p5_crpt.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/p5_crpt2.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/p_dec.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/p_enc.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/p_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/p_open.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/p_seal.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/p_sign.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/p_verify.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/pbe_scrypt.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/pmeth_fn.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/pmeth_gn.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/evp/pmeth_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ex_data.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/getenv.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/hmac/hm_ameth.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/hmac/hm_pmeth.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/hmac/hmac.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/idea/i_cbc.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/idea/i_cfb64.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/idea/i_ecb.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/idea/i_ofb64.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/idea/i_skey.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/init.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/kdf/hkdf.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/kdf/kdf_err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/kdf/scrypt.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/kdf/tls1_prf.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/lhash/lh_stats.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/lhash/lhash.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/md4/md4_dgst.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/md4/md4_one.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/md5/md5-586.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/md5/md5-sparcv9.o" =>
                [
                    "crypto",
                ],
            "crypto/md5/md5_dgst.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/md5/md5_one.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/mdc2/mdc2_one.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/mdc2/mdc2dgst.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/mem.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/mem_dbg.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/mem_sec.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/modes/cbc128.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/modes/ccm128.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/modes/cfb128.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/modes/ctr128.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/modes/cts128.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/modes/gcm128.o" =>
                [
                    ".",
                    "include",
                    "crypto",
                ],
            "crypto/modes/ghash-armv4.o" =>
                [
                    "crypto",
                ],
            "crypto/modes/ghash-s390x.o" =>
                [
                    "crypto",
                ],
            "crypto/modes/ghash-sparcv9.o" =>
                [
                    "crypto",
                ],
            "crypto/modes/ghash-x86.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/modes/ghashv8-armx.o" =>
                [
                    "crypto",
                ],
            "crypto/modes/ocb128.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/modes/ofb128.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/modes/wrap128.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/modes/xts128.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/o_dir.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/o_fips.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/o_fopen.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/o_init.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/o_str.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/o_time.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/objects/o_names.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/objects/obj_dat.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/objects/obj_err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/objects/obj_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/objects/obj_xref.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ocsp/ocsp_asn.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ocsp/ocsp_cl.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ocsp/ocsp_err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ocsp/ocsp_ext.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ocsp/ocsp_ht.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ocsp/ocsp_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ocsp/ocsp_prn.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ocsp/ocsp_srv.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ocsp/ocsp_vfy.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ocsp/v3_ocsp.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pem/pem_all.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pem/pem_err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pem/pem_info.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pem/pem_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pem/pem_oth.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pem/pem_pk8.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pem/pem_pkey.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pem/pem_sign.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pem/pem_x509.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pem/pem_xaux.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pem/pvkfmt.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pkcs12/p12_add.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pkcs12/p12_asn.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pkcs12/p12_attr.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pkcs12/p12_crpt.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pkcs12/p12_crt.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pkcs12/p12_decr.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pkcs12/p12_init.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pkcs12/p12_key.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pkcs12/p12_kiss.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pkcs12/p12_mutl.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pkcs12/p12_npas.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pkcs12/p12_p8d.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pkcs12/p12_p8e.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pkcs12/p12_sbag.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pkcs12/p12_utl.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pkcs12/pk12err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pkcs7/bio_pk7.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pkcs7/pk7_asn1.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pkcs7/pk7_attr.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pkcs7/pk7_doit.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pkcs7/pk7_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pkcs7/pk7_mime.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pkcs7/pk7_smime.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/pkcs7/pkcs7err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/poly1305/poly1305-armv4.o" =>
                [
                    "crypto",
                ],
            "crypto/poly1305/poly1305-armv8.o" =>
                [
                    "crypto",
                ],
            "crypto/poly1305/poly1305-mips.o" =>
                [
                    "crypto",
                ],
            "crypto/poly1305/poly1305-sparcv9.o" =>
                [
                    "crypto",
                ],
            "crypto/poly1305/poly1305-x86.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/poly1305/poly1305.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/poly1305/poly1305_ameth.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/poly1305/poly1305_pmeth.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rand/drbg_ctr.o" =>
                [
                    ".",
                    "include",
                    "crypto/modes",
                ],
            "crypto/rand/drbg_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rand/rand_egd.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rand/rand_err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rand/rand_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rand/rand_unix.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rand/rand_vms.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rand/rand_win.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rand/randfile.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rc2/rc2_cbc.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rc2/rc2_ecb.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rc2/rc2_skey.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rc2/rc2cfb64.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rc2/rc2ofb64.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rc4/rc4-586.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ripemd/rmd-586.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ripemd/rmd_dgst.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ripemd/rmd_one.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rsa/rsa_ameth.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rsa/rsa_asn1.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rsa/rsa_chk.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rsa/rsa_crpt.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rsa/rsa_depr.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rsa/rsa_err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rsa/rsa_gen.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rsa/rsa_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rsa/rsa_meth.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rsa/rsa_mp.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rsa/rsa_none.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rsa/rsa_oaep.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rsa/rsa_ossl.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rsa/rsa_pk1.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rsa/rsa_pmeth.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rsa/rsa_prn.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rsa/rsa_pss.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rsa/rsa_saos.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rsa/rsa_sign.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rsa/rsa_ssl.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rsa/rsa_x931.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/rsa/rsa_x931g.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/s390xcpuid.o" =>
                [
                    "crypto",
                ],
            "crypto/seed/seed.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/seed/seed_cbc.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/seed/seed_cfb.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/seed/seed_ecb.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/seed/seed_ofb.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/sha/keccak1600-armv4.o" =>
                [
                    "crypto",
                ],
            "crypto/sha/keccak1600.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/sha/sha1-586.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/sha/sha1-armv4-large.o" =>
                [
                    "crypto",
                ],
            "crypto/sha/sha1-armv8.o" =>
                [
                    "crypto",
                ],
            "crypto/sha/sha1-mips.o" =>
                [
                    "crypto",
                ],
            "crypto/sha/sha1-s390x.o" =>
                [
                    "crypto",
                ],
            "crypto/sha/sha1-sparcv9.o" =>
                [
                    "crypto",
                ],
            "crypto/sha/sha1_one.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/sha/sha1dgst.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/sha/sha256-586.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/sha/sha256-armv4.o" =>
                [
                    "crypto",
                ],
            "crypto/sha/sha256-armv8.o" =>
                [
                    "crypto",
                ],
            "crypto/sha/sha256-mips.o" =>
                [
                    "crypto",
                ],
            "crypto/sha/sha256-s390x.o" =>
                [
                    "crypto",
                ],
            "crypto/sha/sha256-sparcv9.o" =>
                [
                    "crypto",
                ],
            "crypto/sha/sha256.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/sha/sha512-586.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/sha/sha512-armv4.o" =>
                [
                    "crypto",
                ],
            "crypto/sha/sha512-armv8.o" =>
                [
                    "crypto",
                ],
            "crypto/sha/sha512-mips.o" =>
                [
                    "crypto",
                ],
            "crypto/sha/sha512-s390x.o" =>
                [
                    "crypto",
                ],
            "crypto/sha/sha512-sparcv9.o" =>
                [
                    "crypto",
                ],
            "crypto/sha/sha512.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/siphash/siphash.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/siphash/siphash_ameth.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/siphash/siphash_pmeth.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/sm2/sm2_crypt.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/sm2/sm2_err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/sm2/sm2_pmeth.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/sm2/sm2_sign.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/sm3/m_sm3.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/sm3/sm3.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/sm4/sm4.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/srp/srp_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/srp/srp_vfy.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/stack/stack.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/store/loader_file.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/store/store_err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/store/store_init.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/store/store_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/store/store_register.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/store/store_strings.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/threads_none.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/threads_pthread.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/threads_win.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ts/ts_asn1.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ts/ts_conf.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ts/ts_err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ts/ts_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ts/ts_req_print.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ts/ts_req_utils.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ts/ts_rsp_print.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ts/ts_rsp_sign.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ts/ts_rsp_utils.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ts/ts_rsp_verify.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ts/ts_verify_ctx.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/txt_db/txt_db.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ui/ui_err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ui/ui_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ui/ui_null.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ui/ui_openssl.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/ui/ui_util.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/uid.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/whrlpool/wp-mmx.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/whrlpool/wp_block.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/whrlpool/wp_dgst.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/by_dir.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/by_file.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/t_crl.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/t_req.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/t_x509.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x509_att.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x509_cmp.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x509_d2.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x509_def.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x509_err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x509_ext.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x509_lu.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x509_meth.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x509_obj.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x509_r2x.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x509_req.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x509_set.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x509_trs.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x509_txt.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x509_v3.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x509_vfy.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x509_vpm.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x509cset.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x509name.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x509rset.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x509spki.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x509type.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x_all.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x_attrib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x_crl.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x_exten.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x_name.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x_pubkey.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x_req.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x_x509.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509/x_x509a.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/pcy_cache.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/pcy_data.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/pcy_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/pcy_map.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/pcy_node.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/pcy_tree.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_addr.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_admis.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_akey.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_akeya.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_alt.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_asid.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_bcons.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_bitst.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_conf.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_cpols.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_crld.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_enum.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_extku.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_genn.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_ia5.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_info.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_int.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_ncons.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_pci.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_pcia.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_pcons.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_pku.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_pmaps.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_prn.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_purp.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_skey.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_sxnet.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_tlsf.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3_utl.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x509v3/v3err.o" =>
                [
                    ".",
                    "include",
                ],
            "crypto/x86cpuid.o" =>
                [
                    ".",
                    "include",
                ],
            "engines/e_capi.o" =>
                [
                    ".",
                    "include",
                ],
            "engines/e_padlock-x86.o" =>
                [
                    ".",
                    "include",
                ],
            "engines/e_padlock.o" =>
                [
                    ".",
                    "include",
                ],
            "fuzz/asn1.o" =>
                [
                    "include",
                ],
            "fuzz/asn1parse.o" =>
                [
                    "include",
                ],
            "fuzz/bignum.o" =>
                [
                    "include",
                ],
            "fuzz/bndiv.o" =>
                [
                    "include",
                ],
            "fuzz/client.o" =>
                [
                    "include",
                ],
            "fuzz/cms.o" =>
                [
                    "include",
                ],
            "fuzz/conf.o" =>
                [
                    "include",
                ],
            "fuzz/crl.o" =>
                [
                    "include",
                ],
            "fuzz/ct.o" =>
                [
                    "include",
                ],
            "fuzz/server.o" =>
                [
                    "include",
                ],
            "fuzz/test-corpus.o" =>
                [
                    "include",
                ],
            "fuzz/x509.o" =>
                [
                    "include",
                ],
            "include/crypto/bn_conf.h" =>
                [
                    ".",
                ],
            "include/crypto/dso_conf.h" =>
                [
                    ".",
                ],
            "include/openssl/opensslconf.h" =>
                [
                    ".",
                ],
            "ssl/bio_ssl.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/d1_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/d1_msg.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/d1_srtp.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/methods.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/packet.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/pqueue.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/record/dtls1_bitmap.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/record/rec_layer_d1.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/record/rec_layer_s3.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/record/ssl3_buffer.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/record/ssl3_record.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/record/ssl3_record_tls13.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/s3_cbc.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/s3_enc.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/s3_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/s3_msg.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/ssl_asn1.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/ssl_cert.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/ssl_ciph.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/ssl_conf.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/ssl_err.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/ssl_init.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/ssl_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/ssl_mcnf.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/ssl_rsa.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/ssl_sess.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/ssl_stat.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/ssl_txt.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/ssl_utst.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/statem/extensions.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/statem/extensions_clnt.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/statem/extensions_cust.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/statem/extensions_srvr.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/statem/statem.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/statem/statem_clnt.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/statem/statem_dtls.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/statem/statem_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/statem/statem_srvr.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/t1_enc.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/t1_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/t1_trce.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/tls13_enc.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/tls_srp.o" =>
                [
                    ".",
                    "include",
                ],
            "test/aborttest.o" =>
                [
                    "include",
                ],
            "test/afalgtest.o" =>
                [
                    "include",
                ],
            "test/asn1_decode_test.o" =>
                [
                    "include",
                ],
            "test/asn1_encode_test.o" =>
                [
                    "include",
                ],
            "test/asn1_internal_test.o" =>
                [
                    ".",
                    "include",
                ],
            "test/asn1_string_table_test.o" =>
                [
                    "include",
                ],
            "test/asn1_time_test.o" =>
                [
                    "include",
                ],
            "test/asynciotest.o" =>
                [
                    "include",
                ],
            "test/asynctest.o" =>
                [
                    "include",
                ],
            "test/bad_dtls_test.o" =>
                [
                    "include",
                ],
            "test/bftest.o" =>
                [
                    "include",
                ],
            "test/bio_callback_test.o" =>
                [
                    "include",
                ],
            "test/bio_enc_test.o" =>
                [
                    "include",
                ],
            "test/bio_memleak_test.o" =>
                [
                    "include",
                ],
            "test/bioprinttest.o" =>
                [
                    "include",
                ],
            "test/bntest.o" =>
                [
                    "include",
                ],
            "test/buildtest_aes.o" =>
                [
                    "include",
                ],
            "test/buildtest_asn1.o" =>
                [
                    "include",
                ],
            "test/buildtest_asn1t.o" =>
                [
                    "include",
                ],
            "test/buildtest_async.o" =>
                [
                    "include",
                ],
            "test/buildtest_bio.o" =>
                [
                    "include",
                ],
            "test/buildtest_blowfish.o" =>
                [
                    "include",
                ],
            "test/buildtest_bn.o" =>
                [
                    "include",
                ],
            "test/buildtest_buffer.o" =>
                [
                    "include",
                ],
            "test/buildtest_camellia.o" =>
                [
                    "include",
                ],
            "test/buildtest_cast.o" =>
                [
                    "include",
                ],
            "test/buildtest_cmac.o" =>
                [
                    "include",
                ],
            "test/buildtest_cms.o" =>
                [
                    "include",
                ],
            "test/buildtest_conf.o" =>
                [
                    "include",
                ],
            "test/buildtest_conf_api.o" =>
                [
                    "include",
                ],
            "test/buildtest_crypto.o" =>
                [
                    "include",
                ],
            "test/buildtest_ct.o" =>
                [
                    "include",
                ],
            "test/buildtest_des.o" =>
                [
                    "include",
                ],
            "test/buildtest_dh.o" =>
                [
                    "include",
                ],
            "test/buildtest_dsa.o" =>
                [
                    "include",
                ],
            "test/buildtest_dtls1.o" =>
                [
                    "include",
                ],
            "test/buildtest_e_os2.o" =>
                [
                    "include",
                ],
            "test/buildtest_ebcdic.o" =>
                [
                    "include",
                ],
            "test/buildtest_ec.o" =>
                [
                    "include",
                ],
            "test/buildtest_ecdh.o" =>
                [
                    "include",
                ],
            "test/buildtest_ecdsa.o" =>
                [
                    "include",
                ],
            "test/buildtest_engine.o" =>
                [
                    "include",
                ],
            "test/buildtest_evp.o" =>
                [
                    "include",
                ],
            "test/buildtest_hmac.o" =>
                [
                    "include",
                ],
            "test/buildtest_idea.o" =>
                [
                    "include",
                ],
            "test/buildtest_kdf.o" =>
                [
                    "include",
                ],
            "test/buildtest_lhash.o" =>
                [
                    "include",
                ],
            "test/buildtest_md4.o" =>
                [
                    "include",
                ],
            "test/buildtest_md5.o" =>
                [
                    "include",
                ],
            "test/buildtest_mdc2.o" =>
                [
                    "include",
                ],
            "test/buildtest_modes.o" =>
                [
                    "include",
                ],
            "test/buildtest_obj_mac.o" =>
                [
                    "include",
                ],
            "test/buildtest_objects.o" =>
                [
                    "include",
                ],
            "test/buildtest_ocsp.o" =>
                [
                    "include",
                ],
            "test/buildtest_opensslv.o" =>
                [
                    "include",
                ],
            "test/buildtest_ossl_typ.o" =>
                [
                    "include",
                ],
            "test/buildtest_pem.o" =>
                [
                    "include",
                ],
            "test/buildtest_pem2.o" =>
                [
                    "include",
                ],
            "test/buildtest_pkcs12.o" =>
                [
                    "include",
                ],
            "test/buildtest_pkcs7.o" =>
                [
                    "include",
                ],
            "test/buildtest_rand.o" =>
                [
                    "include",
                ],
            "test/buildtest_rand_drbg.o" =>
                [
                    "include",
                ],
            "test/buildtest_rc2.o" =>
                [
                    "include",
                ],
            "test/buildtest_rc4.o" =>
                [
                    "include",
                ],
            "test/buildtest_ripemd.o" =>
                [
                    "include",
                ],
            "test/buildtest_rsa.o" =>
                [
                    "include",
                ],
            "test/buildtest_safestack.o" =>
                [
                    "include",
                ],
            "test/buildtest_seed.o" =>
                [
                    "include",
                ],
            "test/buildtest_sha.o" =>
                [
                    "include",
                ],
            "test/buildtest_srp.o" =>
                [
                    "include",
                ],
            "test/buildtest_srtp.o" =>
                [
                    "include",
                ],
            "test/buildtest_ssl.o" =>
                [
                    "include",
                ],
            "test/buildtest_ssl2.o" =>
                [
                    "include",
                ],
            "test/buildtest_stack.o" =>
                [
                    "include",
                ],
            "test/buildtest_store.o" =>
                [
                    "include",
                ],
            "test/buildtest_symhacks.o" =>
                [
                    "include",
                ],
            "test/buildtest_tls1.o" =>
                [
                    "include",
                ],
            "test/buildtest_ts.o" =>
                [
                    "include",
                ],
            "test/buildtest_txt_db.o" =>
                [
                    "include",
                ],
            "test/buildtest_ui.o" =>
                [
                    "include",
                ],
            "test/buildtest_whrlpool.o" =>
                [
                    "include",
                ],
            "test/buildtest_x509.o" =>
                [
                    "include",
                ],
            "test/buildtest_x509_vfy.o" =>
                [
                    "include",
                ],
            "test/buildtest_x509v3.o" =>
                [
                    "include",
                ],
            "test/casttest.o" =>
                [
                    "include",
                ],
            "test/chacha_internal_test.o" =>
                [
                    ".",
                    "include",
                ],
            "test/cipher_overhead_test.o" =>
                [
                    ".",
                    "include",
                ],
            "test/cipherbytes_test.o" =>
                [
                    "include",
                ],
            "test/cipherlist_test.o" =>
                [
                    "include",
                ],
            "test/ciphername_test.o" =>
                [
                    "include",
                ],
            "test/clienthellotest.o" =>
                [
                    "include",
                ],
            "test/cmactest.o" =>
                [
                    "include",
                ],
            "test/cmsapitest.o" =>
                [
                    "include",
                ],
            "test/conf_include_test.o" =>
                [
                    "include",
                ],
            "test/constant_time_test.o" =>
                [
                    "include",
                ],
            "test/crltest.o" =>
                [
                    "include",
                ],
            "test/ct_test.o" =>
                [
                    "include",
                ],
            "test/ctype_internal_test.o" =>
                [
                    ".",
                    "include",
                ],
            "test/curve448_internal_test.o" =>
                [
                    ".",
                    "include",
                    "crypto/ec/curve448",
                ],
            "test/d2i_test.o" =>
                [
                    "include",
                ],
            "test/danetest.o" =>
                [
                    "include",
                ],
            "test/destest.o" =>
                [
                    "include",
                ],
            "test/dhtest.o" =>
                [
                    "include",
                ],
            "test/drbg_cavs_data.o" =>
                [
                    "include",
                    "test",
                    ".",
                ],
            "test/drbg_cavs_test.o" =>
                [
                    "include",
                    "test",
                    ".",
                ],
            "test/drbgtest.o" =>
                [
                    "include",
                ],
            "test/dsa_no_digest_size_test.o" =>
                [
                    "include",
                ],
            "test/dsatest.o" =>
                [
                    "include",
                ],
            "test/dtls_mtu_test.o" =>
                [
                    ".",
                    "include",
                ],
            "test/dtlstest.o" =>
                [
                    "include",
                ],
            "test/dtlsv1listentest.o" =>
                [
                    "include",
                ],
            "test/ec_internal_test.o" =>
                [
                    "include",
                    "crypto/ec",
                ],
            "test/ecdsatest.o" =>
                [
                    "include",
                ],
            "test/ecstresstest.o" =>
                [
                    "include",
                ],
            "test/ectest.o" =>
                [
                    "include",
                ],
            "test/enginetest.o" =>
                [
                    "include",
                ],
            "test/errtest.o" =>
                [
                    "include",
                ],
            "test/evp_extra_test.o" =>
                [
                    "include",
                ],
            "test/evp_test.o" =>
                [
                    "include",
                ],
            "test/exdatatest.o" =>
                [
                    "include",
                ],
            "test/exptest.o" =>
                [
                    "include",
                ],
            "test/fatalerrtest.o" =>
                [
                    "include",
                ],
            "test/gmdifftest.o" =>
                [
                    "include",
                ],
            "test/gosttest.o" =>
                [
                    "include",
                    ".",
                ],
            "test/handshake_helper.o" =>
                [
                    ".",
                    "include",
                ],
            "test/hmactest.o" =>
                [
                    "include",
                ],
            "test/ideatest.o" =>
                [
                    "include",
                ],
            "test/igetest.o" =>
                [
                    "include",
                ],
            "test/lhash_test.o" =>
                [
                    "include",
                ],
            "test/md2test.o" =>
                [
                    "include",
                ],
            "test/mdc2_internal_test.o" =>
                [
                    ".",
                    "include",
                ],
            "test/mdc2test.o" =>
                [
                    "include",
                ],
            "test/memleaktest.o" =>
                [
                    "include",
                ],
            "test/modes_internal_test.o" =>
                [
                    ".",
                    "include",
                ],
            "test/ocspapitest.o" =>
                [
                    "include",
                ],
            "test/packettest.o" =>
                [
                    "include",
                ],
            "test/pbelutest.o" =>
                [
                    "include",
                ],
            "test/pemtest.o" =>
                [
                    "include",
                ],
            "test/pkey_meth_kdf_test.o" =>
                [
                    "include",
                ],
            "test/pkey_meth_test.o" =>
                [
                    "include",
                ],
            "test/poly1305_internal_test.o" =>
                [
                    ".",
                    "include",
                ],
            "test/rc2test.o" =>
                [
                    "include",
                ],
            "test/rc4test.o" =>
                [
                    "include",
                ],
            "test/rc5test.o" =>
                [
                    "include",
                ],
            "test/rdrand_sanitytest.o" =>
                [
                    "include",
                ],
            "test/recordlentest.o" =>
                [
                    "include",
                ],
            "test/rsa_complex.o" =>
                [
                    "include",
                ],
            "test/rsa_mp_test.o" =>
                [
                    "include",
                ],
            "test/rsa_test.o" =>
                [
                    "include",
                ],
            "test/sanitytest.o" =>
                [
                    "include",
                ],
            "test/secmemtest.o" =>
                [
                    "include",
                ],
            "test/servername_test.o" =>
                [
                    "include",
                ],
            "test/siphash_internal_test.o" =>
                [
                    ".",
                    "include",
                ],
            "test/sm2_internal_test.o" =>
                [
                    "include",
                ],
            "test/sm4_internal_test.o" =>
                [
                    ".",
                    "include",
                ],
            "test/srptest.o" =>
                [
                    "include",
                ],
            "test/ssl_cert_table_internal_test.o" =>
                [
                    ".",
                    "include",
                ],
            "test/ssl_ctx_test.o" =>
                [
                    "include",
                ],
            "test/ssl_test.o" =>
                [
                    "include",
                ],
            "test/ssl_test_ctx.o" =>
                [
                    "include",
                ],
            "test/ssl_test_ctx_test.o" =>
                [
                    "include",
                ],
            "test/sslapitest.o" =>
                [
                    "include",
                    ".",
                ],
            "test/sslbuffertest.o" =>
                [
                    "include",
                ],
            "test/sslcorrupttest.o" =>
                [
                    "include",
                ],
            "test/ssltest_old.o" =>
                [
                    ".",
                    "include",
                ],
            "test/ssltestlib.o" =>
                [
                    ".",
                    "include",
                ],
            "test/stack_test.o" =>
                [
                    "include",
                ],
            "test/sysdefaulttest.o" =>
                [
                    "include",
                ],
            "test/test_test.o" =>
                [
                    "include",
                ],
            "test/testutil/basic_output.o" =>
                [
                    "include",
                ],
            "test/testutil/cb.o" =>
                [
                    "include",
                ],
            "test/testutil/driver.o" =>
                [
                    "include",
                ],
            "test/testutil/format_output.o" =>
                [
                    "include",
                ],
            "test/testutil/main.o" =>
                [
                    "include",
                ],
            "test/testutil/output_helpers.o" =>
                [
                    "include",
                ],
            "test/testutil/random.o" =>
                [
                    "include",
                ],
            "test/testutil/stanza.o" =>
                [
                    "include",
                ],
            "test/testutil/tap_bio.o" =>
                [
                    "include",
                ],
            "test/testutil/test_cleanup.o" =>
                [
                    "include",
                ],
            "test/testutil/tests.o" =>
                [
                    "include",
                ],
            "test/testutil/testutil_init.o" =>
                [
                    "include",
                ],
            "test/threadstest.o" =>
                [
                    "include",
                ],
            "test/time_offset_test.o" =>
                [
                    "include",
                ],
            "test/tls13ccstest.o" =>
                [
                    "include",
                ],
            "test/tls13encryptiontest.o" =>
                [
                    ".",
                    "include",
                ],
            "test/uitest.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "test/v3ext.o" =>
                [
                    "include",
                ],
            "test/v3nametest.o" =>
                [
                    "include",
                ],
            "test/verify_extra_test.o" =>
                [
                    "include",
                ],
            "test/versions.o" =>
                [
                    "include",
                ],
            "test/wpackettest.o" =>
                [
                    "include",
                ],
            "test/x509_check_cert_pkey_test.o" =>
                [
                    "include",
                ],
            "test/x509_dup_cert_test.o" =>
                [
                    "include",
                ],
            "test/x509_internal_test.o" =>
                [
                    ".",
                    "include",
                ],
            "test/x509_time_test.o" =>
                [
                    "include",
                ],
            "test/x509aux.o" =>
                [
                    "include",
                ],
        },
    "install" =>
        {
            "libraries" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "programs" =>
                [
                    "apps/openssl",
                ],
            "scripts" =>
                [
                    "apps/CA.pl",
                    "apps/tsget.pl",
                    "tools/c_rehash",
                ],
        },
    "ldadd" =>
        {
        },
    "libraries" =>
        [
            "apps/libapps.a",
            "libcrypto",
            "libssl",
            "test/libtestutil.a",
        ],
    "overrides" =>
        [
        ],
    "programs" =>
        [
            "apps/openssl",
            "fuzz/asn1-test",
            "fuzz/asn1parse-test",
            "fuzz/bignum-test",
            "fuzz/bndiv-test",
            "fuzz/client-test",
            "fuzz/cms-test",
            "fuzz/conf-test",
            "fuzz/crl-test",
            "fuzz/ct-test",
            "fuzz/server-test",
            "fuzz/x509-test",
            "test/aborttest",
            "test/afalgtest",
            "test/asn1_decode_test",
            "test/asn1_encode_test",
            "test/asn1_internal_test",
            "test/asn1_string_table_test",
            "test/asn1_time_test",
            "test/asynciotest",
            "test/asynctest",
            "test/bad_dtls_test",
            "test/bftest",
            "test/bio_callback_test",
            "test/bio_enc_test",
            "test/bio_memleak_test",
            "test/bioprinttest",
            "test/bntest",
            "test/buildtest_c_aes",
            "test/buildtest_c_asn1",
            "test/buildtest_c_asn1t",
            "test/buildtest_c_async",
            "test/buildtest_c_bio",
            "test/buildtest_c_blowfish",
            "test/buildtest_c_bn",
            "test/buildtest_c_buffer",
            "test/buildtest_c_camellia",
            "test/buildtest_c_cast",
            "test/buildtest_c_cmac",
            "test/buildtest_c_cms",
            "test/buildtest_c_conf",
            "test/buildtest_c_conf_api",
            "test/buildtest_c_crypto",
            "test/buildtest_c_ct",
            "test/buildtest_c_des",
            "test/buildtest_c_dh",
            "test/buildtest_c_dsa",
            "test/buildtest_c_dtls1",
            "test/buildtest_c_e_os2",
            "test/buildtest_c_ebcdic",
            "test/buildtest_c_ec",
            "test/buildtest_c_ecdh",
            "test/buildtest_c_ecdsa",
            "test/buildtest_c_engine",
            "test/buildtest_c_evp",
            "test/buildtest_c_hmac",
            "test/buildtest_c_idea",
            "test/buildtest_c_kdf",
            "test/buildtest_c_lhash",
            "test/buildtest_c_md4",
            "test/buildtest_c_md5",
            "test/buildtest_c_mdc2",
            "test/buildtest_c_modes",
            "test/buildtest_c_obj_mac",
            "test/buildtest_c_objects",
            "test/buildtest_c_ocsp",
            "test/buildtest_c_opensslv",
            "test/buildtest_c_ossl_typ",
            "test/buildtest_c_pem",
            "test/buildtest_c_pem2",
            "test/buildtest_c_pkcs12",
            "test/buildtest_c_pkcs7",
            "test/buildtest_c_rand",
            "test/buildtest_c_rand_drbg",
            "test/buildtest_c_rc2",
            "test/buildtest_c_rc4",
            "test/buildtest_c_ripemd",
            "test/buildtest_c_rsa",
            "test/buildtest_c_safestack",
            "test/buildtest_c_seed",
            "test/buildtest_c_sha",
            "test/buildtest_c_srp",
            "test/buildtest_c_srtp",
            "test/buildtest_c_ssl",
            "test/buildtest_c_ssl2",
            "test/buildtest_c_stack",
            "test/buildtest_c_store",
            "test/buildtest_c_symhacks",
            "test/buildtest_c_tls1",
            "test/buildtest_c_ts",
            "test/buildtest_c_txt_db",
            "test/buildtest_c_ui",
            "test/buildtest_c_whrlpool",
            "test/buildtest_c_x509",
            "test/buildtest_c_x509_vfy",
            "test/buildtest_c_x509v3",
            "test/casttest",
            "test/chacha_internal_test",
            "test/cipher_overhead_test",
            "test/cipherbytes_test",
            "test/cipherlist_test",
            "test/ciphername_test",
            "test/clienthellotest",
            "test/cmactest",
            "test/cmsapitest",
            "test/conf_include_test",
            "test/constant_time_test",
            "test/crltest",
            "test/ct_test",
            "test/ctype_internal_test",
            "test/curve448_internal_test",
            "test/d2i_test",
            "test/danetest",
            "test/destest",
            "test/dhtest",
            "test/drbg_cavs_test",
            "test/drbgtest",
            "test/dsa_no_digest_size_test",
            "test/dsatest",
            "test/dtls_mtu_test",
            "test/dtlstest",
            "test/dtlsv1listentest",
            "test/ec_internal_test",
            "test/ecdsatest",
            "test/ecstresstest",
            "test/ectest",
            "test/enginetest",
            "test/errtest",
            "test/evp_extra_test",
            "test/evp_test",
            "test/exdatatest",
            "test/exptest",
            "test/fatalerrtest",
            "test/gmdifftest",
            "test/gosttest",
            "test/hmactest",
            "test/ideatest",
            "test/igetest",
            "test/lhash_test",
            "test/md2test",
            "test/mdc2_internal_test",
            "test/mdc2test",
            "test/memleaktest",
            "test/modes_internal_test",
            "test/ocspapitest",
            "test/packettest",
            "test/pbelutest",
            "test/pemtest",
            "test/pkey_meth_kdf_test",
            "test/pkey_meth_test",
            "test/poly1305_internal_test",
            "test/rc2test",
            "test/rc4test",
            "test/rc5test",
            "test/rdrand_sanitytest",
            "test/recordlentest",
            "test/rsa_complex",
            "test/rsa_mp_test",
            "test/rsa_test",
            "test/sanitytest",
            "test/secmemtest",
            "test/servername_test",
            "test/siphash_internal_test",
            "test/sm2_internal_test",
            "test/sm4_internal_test",
            "test/srptest",
            "test/ssl_cert_table_internal_test",
            "test/ssl_ctx_test",
            "test/ssl_test",
            "test/ssl_test_ctx_test",
            "test/sslapitest",
            "test/sslbuffertest",
            "test/sslcorrupttest",
            "test/ssltest_old",
            "test/stack_test",
            "test/sysdefaulttest",
            "test/test_test",
            "test/threadstest",
            "test/time_offset_test",
            "test/tls13ccstest",
            "test/tls13encryptiontest",
            "test/uitest",
            "test/v3ext",
            "test/v3nametest",
            "test/verify_extra_test",
            "test/versions",
            "test/wpackettest",
            "test/x509_check_cert_pkey_test",
            "test/x509_dup_cert_test",
            "test/x509_internal_test",
            "test/x509_time_test",
            "test/x509aux",
        ],
    "rawlines" =>
        [
            "##### SHA assembler implementations",
            "",
            "# GNU make \"catch all\"",
            "crypto/sha/sha1-%.S:	crypto/sha/asm/sha1-%.pl",
            "	CC=\"\$(CC)\" \$(PERL) \$< \$(PERLASM_SCHEME) \$\@",
            "crypto/sha/sha256-%.S:	crypto/sha/asm/sha512-%.pl",
            "	CC=\"\$(CC)\" \$(PERL) \$< \$(PERLASM_SCHEME) \$\@",
            "crypto/sha/sha512-%.S:	crypto/sha/asm/sha512-%.pl",
            "	CC=\"\$(CC)\" \$(PERL) \$< \$(PERLASM_SCHEME) \$\@",
            "crypto/poly1305/poly1305-%.S:	crypto/poly1305/asm/poly1305-%.pl",
            "	CC=\"\$(CC)\" \$(PERL) \$< \$(PERLASM_SCHEME) \$\@",
            "##### AES assembler implementations",
            "",
            "# GNU make \"catch all\"",
            "crypto/aes/aes-%.S:	crypto/aes/asm/aes-%.pl",
            "	CC=\"\$(CC)\" \$(PERL) \$< \$(PERLASM_SCHEME) \$\@",
            "crypto/aes/bsaes-%.S:	crypto/aes/asm/bsaes-%.pl",
            "	CC=\"\$(CC)\" \$(PERL) \$< \$(PERLASM_SCHEME) \$\@",
            "",
            "# GNU make \"catch all\"",
            "crypto/rc4/rc4-%.s:	crypto/rc4/asm/rc4-%.pl",
            "	CC=\"\$(CC)\" \$(PERL) \$< \$(PERLASM_SCHEME) \$\@",
            "##### CHACHA assembler implementations",
            "",
            "crypto/chacha/chacha-%.S:	crypto/chacha/asm/chacha-%.pl",
            "	CC=\"\$(CC)\" \$(PERL) \$< \$(PERLASM_SCHEME) \$\@",
            "# GNU make \"catch all\"",
            "crypto/modes/ghash-%.S:	crypto/modes/asm/ghash-%.pl",
            "	CC=\"\$(CC)\" \$(PERL) \$< \$(PERLASM_SCHEME) \$\@",
            "crypto/ec/ecp_nistz256-%.S:	crypto/ec/asm/ecp_nistz256-%.pl",
            "	CC=\"\$(CC)\" \$(PERL) \$< \$(PERLASM_SCHEME) \$\@",
        ],
    "rename" =>
        {
        },
    "scripts" =>
        [
            "apps/CA.pl",
            "apps/tsget.pl",
            "tools/c_rehash",
            "util/shlib_wrap.sh",
        ],
    "shared_sources" =>
        {
        },
    "sources" =>
        {
            "apps/CA.pl" =>
                [
                    "apps/CA.pl.in",
                ],
            "apps/app_rand.o" =>
                [
                    "apps/app_rand.c",
                ],
            "apps/apps.o" =>
                [
                    "apps/apps.c",
                ],
            "apps/asn1pars.o" =>
                [
                    "apps/asn1pars.c",
                ],
            "apps/bf_prefix.o" =>
                [
                    "apps/bf_prefix.c",
                ],
            "apps/ca.o" =>
                [
                    "apps/ca.c",
                ],
            "apps/ciphers.o" =>
                [
                    "apps/ciphers.c",
                ],
            "apps/cms.o" =>
                [
                    "apps/cms.c",
                ],
            "apps/crl.o" =>
                [
                    "apps/crl.c",
                ],
            "apps/crl2p7.o" =>
                [
                    "apps/crl2p7.c",
                ],
            "apps/dgst.o" =>
                [
                    "apps/dgst.c",
                ],
            "apps/dhparam.o" =>
                [
                    "apps/dhparam.c",
                ],
            "apps/dsa.o" =>
                [
                    "apps/dsa.c",
                ],
            "apps/dsaparam.o" =>
                [
                    "apps/dsaparam.c",
                ],
            "apps/ec.o" =>
                [
                    "apps/ec.c",
                ],
            "apps/ecparam.o" =>
                [
                    "apps/ecparam.c",
                ],
            "apps/enc.o" =>
                [
                    "apps/enc.c",
                ],
            "apps/engine.o" =>
                [
                    "apps/engine.c",
                ],
            "apps/errstr.o" =>
                [
                    "apps/errstr.c",
                ],
            "apps/gendsa.o" =>
                [
                    "apps/gendsa.c",
                ],
            "apps/genpkey.o" =>
                [
                    "apps/genpkey.c",
                ],
            "apps/genrsa.o" =>
                [
                    "apps/genrsa.c",
                ],
            "apps/libapps.a" =>
                [
                    "apps/app_rand.o",
                    "apps/apps.o",
                    "apps/bf_prefix.o",
                    "apps/opt.o",
                    "apps/s_cb.o",
                    "apps/s_socket.o",
                ],
            "apps/nseq.o" =>
                [
                    "apps/nseq.c",
                ],
            "apps/ocsp.o" =>
                [
                    "apps/ocsp.c",
                ],
            "apps/openssl" =>
                [
                    "apps/asn1pars.o",
                    "apps/ca.o",
                    "apps/ciphers.o",
                    "apps/cms.o",
                    "apps/crl.o",
                    "apps/crl2p7.o",
                    "apps/dgst.o",
                    "apps/dhparam.o",
                    "apps/dsa.o",
                    "apps/dsaparam.o",
                    "apps/ec.o",
                    "apps/ecparam.o",
                    "apps/enc.o",
                    "apps/engine.o",
                    "apps/errstr.o",
                    "apps/gendsa.o",
                    "apps/genpkey.o",
                    "apps/genrsa.o",
                    "apps/nseq.o",
                    "apps/ocsp.o",
                    "apps/openssl.o",
                    "apps/passwd.o",
                    "apps/pkcs12.o",
                    "apps/pkcs7.o",
                    "apps/pkcs8.o",
                    "apps/pkey.o",
                    "apps/pkeyparam.o",
                    "apps/pkeyutl.o",
                    "apps/prime.o",
                    "apps/rand.o",
                    "apps/rehash.o",
                    "apps/req.o",
                    "apps/rsa.o",
                    "apps/rsautl.o",
                    "apps/s_client.o",
                    "apps/s_server.o",
                    "apps/s_time.o",
                    "apps/sess_id.o",
                    "apps/smime.o",
                    "apps/speed.o",
                    "apps/spkac.o",
                    "apps/srp.o",
                    "apps/storeutl.o",
                    "apps/ts.o",
                    "apps/verify.o",
                    "apps/version.o",
                    "apps/x509.o",
                ],
            "apps/openssl.o" =>
                [
                    "apps/openssl.c",
                ],
            "apps/opt.o" =>
                [
                    "apps/opt.c",
                ],
            "apps/passwd.o" =>
                [
                    "apps/passwd.c",
                ],
            "apps/pkcs12.o" =>
                [
                    "apps/pkcs12.c",
                ],
            "apps/pkcs7.o" =>
                [
                    "apps/pkcs7.c",
                ],
            "apps/pkcs8.o" =>
                [
                    "apps/pkcs8.c",
                ],
            "apps/pkey.o" =>
                [
                    "apps/pkey.c",
                ],
            "apps/pkeyparam.o" =>
                [
                    "apps/pkeyparam.c",
                ],
            "apps/pkeyutl.o" =>
                [
                    "apps/pkeyutl.c",
                ],
            "apps/prime.o" =>
                [
                    "apps/prime.c",
                ],
            "apps/rand.o" =>
                [
                    "apps/rand.c",
                ],
            "apps/rehash.o" =>
                [
                    "apps/rehash.c",
                ],
            "apps/req.o" =>
                [
                    "apps/req.c",
                ],
            "apps/rsa.o" =>
                [
                    "apps/rsa.c",
                ],
            "apps/rsautl.o" =>
                [
                    "apps/rsautl.c",
                ],
            "apps/s_cb.o" =>
                [
                    "apps/s_cb.c",
                ],
            "apps/s_client.o" =>
                [
                    "apps/s_client.c",
                ],
            "apps/s_server.o" =>
                [
                    "apps/s_server.c",
                ],
            "apps/s_socket.o" =>
                [
                    "apps/s_socket.c",
                ],
            "apps/s_time.o" =>
                [
                    "apps/s_time.c",
                ],
            "apps/sess_id.o" =>
                [
                    "apps/sess_id.c",
                ],
            "apps/smime.o" =>
                [
                    "apps/smime.c",
                ],
            "apps/speed.o" =>
                [
                    "apps/speed.c",
                ],
            "apps/spkac.o" =>
                [
                    "apps/spkac.c",
                ],
            "apps/srp.o" =>
                [
                    "apps/srp.c",
                ],
            "apps/storeutl.o" =>
                [
                    "apps/storeutl.c",
                ],
            "apps/ts.o" =>
                [
                    "apps/ts.c",
                ],
            "apps/tsget.pl" =>
                [
                    "apps/tsget.in",
                ],
            "apps/verify.o" =>
                [
                    "apps/verify.c",
                ],
            "apps/version.o" =>
                [
                    "apps/version.c",
                ],
            "apps/x509.o" =>
                [
                    "apps/x509.c",
                ],
            "crypto/aes/aes_cbc.o" =>
                [
                    "crypto/aes/aes_cbc.c",
                ],
            "crypto/aes/aes_cfb.o" =>
                [
                    "crypto/aes/aes_cfb.c",
                ],
            "crypto/aes/aes_core.o" =>
                [
                    "crypto/aes/aes_core.c",
                ],
            "crypto/aes/aes_ecb.o" =>
                [
                    "crypto/aes/aes_ecb.c",
                ],
            "crypto/aes/aes_ige.o" =>
                [
                    "crypto/aes/aes_ige.c",
                ],
            "crypto/aes/aes_misc.o" =>
                [
                    "crypto/aes/aes_misc.c",
                ],
            "crypto/aes/aes_ofb.o" =>
                [
                    "crypto/aes/aes_ofb.c",
                ],
            "crypto/aes/aes_wrap.o" =>
                [
                    "crypto/aes/aes_wrap.c",
                ],
            "crypto/aes/aesni-x86.o" =>
                [
                    "crypto/aes/aesni-x86.s",
                ],
            "crypto/aes/vpaes-x86.o" =>
                [
                    "crypto/aes/vpaes-x86.s",
                ],
            "crypto/aria/aria.o" =>
                [
                    "crypto/aria/aria.c",
                ],
            "crypto/asn1/a_bitstr.o" =>
                [
                    "crypto/asn1/a_bitstr.c",
                ],
            "crypto/asn1/a_d2i_fp.o" =>
                [
                    "crypto/asn1/a_d2i_fp.c",
                ],
            "crypto/asn1/a_digest.o" =>
                [
                    "crypto/asn1/a_digest.c",
                ],
            "crypto/asn1/a_dup.o" =>
                [
                    "crypto/asn1/a_dup.c",
                ],
            "crypto/asn1/a_gentm.o" =>
                [
                    "crypto/asn1/a_gentm.c",
                ],
            "crypto/asn1/a_i2d_fp.o" =>
                [
                    "crypto/asn1/a_i2d_fp.c",
                ],
            "crypto/asn1/a_int.o" =>
                [
                    "crypto/asn1/a_int.c",
                ],
            "crypto/asn1/a_mbstr.o" =>
                [
                    "crypto/asn1/a_mbstr.c",
                ],
            "crypto/asn1/a_object.o" =>
                [
                    "crypto/asn1/a_object.c",
                ],
            "crypto/asn1/a_octet.o" =>
                [
                    "crypto/asn1/a_octet.c",
                ],
            "crypto/asn1/a_print.o" =>
                [
                    "crypto/asn1/a_print.c",
                ],
            "crypto/asn1/a_sign.o" =>
                [
                    "crypto/asn1/a_sign.c",
                ],
            "crypto/asn1/a_strex.o" =>
                [
                    "crypto/asn1/a_strex.c",
                ],
            "crypto/asn1/a_strnid.o" =>
                [
                    "crypto/asn1/a_strnid.c",
                ],
            "crypto/asn1/a_time.o" =>
                [
                    "crypto/asn1/a_time.c",
                ],
            "crypto/asn1/a_type.o" =>
                [
                    "crypto/asn1/a_type.c",
                ],
            "crypto/asn1/a_utctm.o" =>
                [
                    "crypto/asn1/a_utctm.c",
                ],
            "crypto/asn1/a_utf8.o" =>
                [
                    "crypto/asn1/a_utf8.c",
                ],
            "crypto/asn1/a_verify.o" =>
                [
                    "crypto/asn1/a_verify.c",
                ],
            "crypto/asn1/ameth_lib.o" =>
                [
                    "crypto/asn1/ameth_lib.c",
                ],
            "crypto/asn1/asn1_err.o" =>
                [
                    "crypto/asn1/asn1_err.c",
                ],
            "crypto/asn1/asn1_gen.o" =>
                [
                    "crypto/asn1/asn1_gen.c",
                ],
            "crypto/asn1/asn1_item_list.o" =>
                [
                    "crypto/asn1/asn1_item_list.c",
                ],
            "crypto/asn1/asn1_lib.o" =>
                [
                    "crypto/asn1/asn1_lib.c",
                ],
            "crypto/asn1/asn1_par.o" =>
                [
                    "crypto/asn1/asn1_par.c",
                ],
            "crypto/asn1/asn_mime.o" =>
                [
                    "crypto/asn1/asn_mime.c",
                ],
            "crypto/asn1/asn_moid.o" =>
                [
                    "crypto/asn1/asn_moid.c",
                ],
            "crypto/asn1/asn_mstbl.o" =>
                [
                    "crypto/asn1/asn_mstbl.c",
                ],
            "crypto/asn1/asn_pack.o" =>
                [
                    "crypto/asn1/asn_pack.c",
                ],
            "crypto/asn1/bio_asn1.o" =>
                [
                    "crypto/asn1/bio_asn1.c",
                ],
            "crypto/asn1/bio_ndef.o" =>
                [
                    "crypto/asn1/bio_ndef.c",
                ],
            "crypto/asn1/d2i_pr.o" =>
                [
                    "crypto/asn1/d2i_pr.c",
                ],
            "crypto/asn1/d2i_pu.o" =>
                [
                    "crypto/asn1/d2i_pu.c",
                ],
            "crypto/asn1/evp_asn1.o" =>
                [
                    "crypto/asn1/evp_asn1.c",
                ],
            "crypto/asn1/f_int.o" =>
                [
                    "crypto/asn1/f_int.c",
                ],
            "crypto/asn1/f_string.o" =>
                [
                    "crypto/asn1/f_string.c",
                ],
            "crypto/asn1/i2d_pr.o" =>
                [
                    "crypto/asn1/i2d_pr.c",
                ],
            "crypto/asn1/i2d_pu.o" =>
                [
                    "crypto/asn1/i2d_pu.c",
                ],
            "crypto/asn1/n_pkey.o" =>
                [
                    "crypto/asn1/n_pkey.c",
                ],
            "crypto/asn1/nsseq.o" =>
                [
                    "crypto/asn1/nsseq.c",
                ],
            "crypto/asn1/p5_pbe.o" =>
                [
                    "crypto/asn1/p5_pbe.c",
                ],
            "crypto/asn1/p5_pbev2.o" =>
                [
                    "crypto/asn1/p5_pbev2.c",
                ],
            "crypto/asn1/p5_scrypt.o" =>
                [
                    "crypto/asn1/p5_scrypt.c",
                ],
            "crypto/asn1/p8_pkey.o" =>
                [
                    "crypto/asn1/p8_pkey.c",
                ],
            "crypto/asn1/t_bitst.o" =>
                [
                    "crypto/asn1/t_bitst.c",
                ],
            "crypto/asn1/t_pkey.o" =>
                [
                    "crypto/asn1/t_pkey.c",
                ],
            "crypto/asn1/t_spki.o" =>
                [
                    "crypto/asn1/t_spki.c",
                ],
            "crypto/asn1/tasn_dec.o" =>
                [
                    "crypto/asn1/tasn_dec.c",
                ],
            "crypto/asn1/tasn_enc.o" =>
                [
                    "crypto/asn1/tasn_enc.c",
                ],
            "crypto/asn1/tasn_fre.o" =>
                [
                    "crypto/asn1/tasn_fre.c",
                ],
            "crypto/asn1/tasn_new.o" =>
                [
                    "crypto/asn1/tasn_new.c",
                ],
            "crypto/asn1/tasn_prn.o" =>
                [
                    "crypto/asn1/tasn_prn.c",
                ],
            "crypto/asn1/tasn_scn.o" =>
                [
                    "crypto/asn1/tasn_scn.c",
                ],
            "crypto/asn1/tasn_typ.o" =>
                [
                    "crypto/asn1/tasn_typ.c",
                ],
            "crypto/asn1/tasn_utl.o" =>
                [
                    "crypto/asn1/tasn_utl.c",
                ],
            "crypto/asn1/x_algor.o" =>
                [
                    "crypto/asn1/x_algor.c",
                ],
            "crypto/asn1/x_bignum.o" =>
                [
                    "crypto/asn1/x_bignum.c",
                ],
            "crypto/asn1/x_info.o" =>
                [
                    "crypto/asn1/x_info.c",
                ],
            "crypto/asn1/x_int64.o" =>
                [
                    "crypto/asn1/x_int64.c",
                ],
            "crypto/asn1/x_long.o" =>
                [
                    "crypto/asn1/x_long.c",
                ],
            "crypto/asn1/x_pkey.o" =>
                [
                    "crypto/asn1/x_pkey.c",
                ],
            "crypto/asn1/x_sig.o" =>
                [
                    "crypto/asn1/x_sig.c",
                ],
            "crypto/asn1/x_spki.o" =>
                [
                    "crypto/asn1/x_spki.c",
                ],
            "crypto/asn1/x_val.o" =>
                [
                    "crypto/asn1/x_val.c",
                ],
            "crypto/async/arch/async_null.o" =>
                [
                    "crypto/async/arch/async_null.c",
                ],
            "crypto/async/arch/async_posix.o" =>
                [
                    "crypto/async/arch/async_posix.c",
                ],
            "crypto/async/arch/async_win.o" =>
                [
                    "crypto/async/arch/async_win.c",
                ],
            "crypto/async/async.o" =>
                [
                    "crypto/async/async.c",
                ],
            "crypto/async/async_err.o" =>
                [
                    "crypto/async/async_err.c",
                ],
            "crypto/async/async_wait.o" =>
                [
                    "crypto/async/async_wait.c",
                ],
            "crypto/bf/bf-586.o" =>
                [
                    "crypto/bf/bf-586.s",
                ],
            "crypto/bf/bf_cfb64.o" =>
                [
                    "crypto/bf/bf_cfb64.c",
                ],
            "crypto/bf/bf_ecb.o" =>
                [
                    "crypto/bf/bf_ecb.c",
                ],
            "crypto/bf/bf_ofb64.o" =>
                [
                    "crypto/bf/bf_ofb64.c",
                ],
            "crypto/bf/bf_skey.o" =>
                [
                    "crypto/bf/bf_skey.c",
                ],
            "crypto/bio/b_addr.o" =>
                [
                    "crypto/bio/b_addr.c",
                ],
            "crypto/bio/b_dump.o" =>
                [
                    "crypto/bio/b_dump.c",
                ],
            "crypto/bio/b_print.o" =>
                [
                    "crypto/bio/b_print.c",
                ],
            "crypto/bio/b_sock.o" =>
                [
                    "crypto/bio/b_sock.c",
                ],
            "crypto/bio/b_sock2.o" =>
                [
                    "crypto/bio/b_sock2.c",
                ],
            "crypto/bio/bf_buff.o" =>
                [
                    "crypto/bio/bf_buff.c",
                ],
            "crypto/bio/bf_lbuf.o" =>
                [
                    "crypto/bio/bf_lbuf.c",
                ],
            "crypto/bio/bf_nbio.o" =>
                [
                    "crypto/bio/bf_nbio.c",
                ],
            "crypto/bio/bf_null.o" =>
                [
                    "crypto/bio/bf_null.c",
                ],
            "crypto/bio/bio_cb.o" =>
                [
                    "crypto/bio/bio_cb.c",
                ],
            "crypto/bio/bio_err.o" =>
                [
                    "crypto/bio/bio_err.c",
                ],
            "crypto/bio/bio_lib.o" =>
                [
                    "crypto/bio/bio_lib.c",
                ],
            "crypto/bio/bio_meth.o" =>
                [
                    "crypto/bio/bio_meth.c",
                ],
            "crypto/bio/bss_acpt.o" =>
                [
                    "crypto/bio/bss_acpt.c",
                ],
            "crypto/bio/bss_bio.o" =>
                [
                    "crypto/bio/bss_bio.c",
                ],
            "crypto/bio/bss_conn.o" =>
                [
                    "crypto/bio/bss_conn.c",
                ],
            "crypto/bio/bss_dgram.o" =>
                [
                    "crypto/bio/bss_dgram.c",
                ],
            "crypto/bio/bss_fd.o" =>
                [
                    "crypto/bio/bss_fd.c",
                ],
            "crypto/bio/bss_file.o" =>
                [
                    "crypto/bio/bss_file.c",
                ],
            "crypto/bio/bss_log.o" =>
                [
                    "crypto/bio/bss_log.c",
                ],
            "crypto/bio/bss_mem.o" =>
                [
                    "crypto/bio/bss_mem.c",
                ],
            "crypto/bio/bss_null.o" =>
                [
                    "crypto/bio/bss_null.c",
                ],
            "crypto/bio/bss_sock.o" =>
                [
                    "crypto/bio/bss_sock.c",
                ],
            "crypto/blake2/blake2b.o" =>
                [
                    "crypto/blake2/blake2b.c",
                ],
            "crypto/blake2/blake2s.o" =>
                [
                    "crypto/blake2/blake2s.c",
                ],
            "crypto/blake2/m_blake2b.o" =>
                [
                    "crypto/blake2/m_blake2b.c",
                ],
            "crypto/blake2/m_blake2s.o" =>
                [
                    "crypto/blake2/m_blake2s.c",
                ],
            "crypto/bn/bn-586.o" =>
                [
                    "crypto/bn/bn-586.s",
                ],
            "crypto/bn/bn_add.o" =>
                [
                    "crypto/bn/bn_add.c",
                ],
            "crypto/bn/bn_blind.o" =>
                [
                    "crypto/bn/bn_blind.c",
                ],
            "crypto/bn/bn_const.o" =>
                [
                    "crypto/bn/bn_const.c",
                ],
            "crypto/bn/bn_ctx.o" =>
                [
                    "crypto/bn/bn_ctx.c",
                ],
            "crypto/bn/bn_depr.o" =>
                [
                    "crypto/bn/bn_depr.c",
                ],
            "crypto/bn/bn_dh.o" =>
                [
                    "crypto/bn/bn_dh.c",
                ],
            "crypto/bn/bn_div.o" =>
                [
                    "crypto/bn/bn_div.c",
                ],
            "crypto/bn/bn_err.o" =>
                [
                    "crypto/bn/bn_err.c",
                ],
            "crypto/bn/bn_exp.o" =>
                [
                    "crypto/bn/bn_exp.c",
                ],
            "crypto/bn/bn_exp2.o" =>
                [
                    "crypto/bn/bn_exp2.c",
                ],
            "crypto/bn/bn_gcd.o" =>
                [
                    "crypto/bn/bn_gcd.c",
                ],
            "crypto/bn/bn_gf2m.o" =>
                [
                    "crypto/bn/bn_gf2m.c",
                ],
            "crypto/bn/bn_intern.o" =>
                [
                    "crypto/bn/bn_intern.c",
                ],
            "crypto/bn/bn_kron.o" =>
                [
                    "crypto/bn/bn_kron.c",
                ],
            "crypto/bn/bn_lib.o" =>
                [
                    "crypto/bn/bn_lib.c",
                ],
            "crypto/bn/bn_mod.o" =>
                [
                    "crypto/bn/bn_mod.c",
                ],
            "crypto/bn/bn_mont.o" =>
                [
                    "crypto/bn/bn_mont.c",
                ],
            "crypto/bn/bn_mpi.o" =>
                [
                    "crypto/bn/bn_mpi.c",
                ],
            "crypto/bn/bn_mul.o" =>
                [
                    "crypto/bn/bn_mul.c",
                ],
            "crypto/bn/bn_nist.o" =>
                [
                    "crypto/bn/bn_nist.c",
                ],
            "crypto/bn/bn_prime.o" =>
                [
                    "crypto/bn/bn_prime.c",
                ],
            "crypto/bn/bn_print.o" =>
                [
                    "crypto/bn/bn_print.c",
                ],
            "crypto/bn/bn_rand.o" =>
                [
                    "crypto/bn/bn_rand.c",
                ],
            "crypto/bn/bn_recp.o" =>
                [
                    "crypto/bn/bn_recp.c",
                ],
            "crypto/bn/bn_shift.o" =>
                [
                    "crypto/bn/bn_shift.c",
                ],
            "crypto/bn/bn_sqr.o" =>
                [
                    "crypto/bn/bn_sqr.c",
                ],
            "crypto/bn/bn_sqrt.o" =>
                [
                    "crypto/bn/bn_sqrt.c",
                ],
            "crypto/bn/bn_srp.o" =>
                [
                    "crypto/bn/bn_srp.c",
                ],
            "crypto/bn/bn_word.o" =>
                [
                    "crypto/bn/bn_word.c",
                ],
            "crypto/bn/bn_x931p.o" =>
                [
                    "crypto/bn/bn_x931p.c",
                ],
            "crypto/bn/co-586.o" =>
                [
                    "crypto/bn/co-586.s",
                ],
            "crypto/bn/x86-gf2m.o" =>
                [
                    "crypto/bn/x86-gf2m.s",
                ],
            "crypto/bn/x86-mont.o" =>
                [
                    "crypto/bn/x86-mont.s",
                ],
            "crypto/buffer/buf_err.o" =>
                [
                    "crypto/buffer/buf_err.c",
                ],
            "crypto/buffer/buffer.o" =>
                [
                    "crypto/buffer/buffer.c",
                ],
            "crypto/camellia/cmll-x86.o" =>
                [
                    "crypto/camellia/cmll-x86.s",
                ],
            "crypto/camellia/cmll_cfb.o" =>
                [
                    "crypto/camellia/cmll_cfb.c",
                ],
            "crypto/camellia/cmll_ctr.o" =>
                [
                    "crypto/camellia/cmll_ctr.c",
                ],
            "crypto/camellia/cmll_ecb.o" =>
                [
                    "crypto/camellia/cmll_ecb.c",
                ],
            "crypto/camellia/cmll_ofb.o" =>
                [
                    "crypto/camellia/cmll_ofb.c",
                ],
            "crypto/cast/c_cfb64.o" =>
                [
                    "crypto/cast/c_cfb64.c",
                ],
            "crypto/cast/c_ecb.o" =>
                [
                    "crypto/cast/c_ecb.c",
                ],
            "crypto/cast/c_enc.o" =>
                [
                    "crypto/cast/c_enc.c",
                ],
            "crypto/cast/c_ofb64.o" =>
                [
                    "crypto/cast/c_ofb64.c",
                ],
            "crypto/cast/c_skey.o" =>
                [
                    "crypto/cast/c_skey.c",
                ],
            "crypto/chacha/chacha-x86.o" =>
                [
                    "crypto/chacha/chacha-x86.s",
                ],
            "crypto/cmac/cm_ameth.o" =>
                [
                    "crypto/cmac/cm_ameth.c",
                ],
            "crypto/cmac/cm_pmeth.o" =>
                [
                    "crypto/cmac/cm_pmeth.c",
                ],
            "crypto/cmac/cmac.o" =>
                [
                    "crypto/cmac/cmac.c",
                ],
            "crypto/cms/cms_asn1.o" =>
                [
                    "crypto/cms/cms_asn1.c",
                ],
            "crypto/cms/cms_att.o" =>
                [
                    "crypto/cms/cms_att.c",
                ],
            "crypto/cms/cms_cd.o" =>
                [
                    "crypto/cms/cms_cd.c",
                ],
            "crypto/cms/cms_dd.o" =>
                [
                    "crypto/cms/cms_dd.c",
                ],
            "crypto/cms/cms_enc.o" =>
                [
                    "crypto/cms/cms_enc.c",
                ],
            "crypto/cms/cms_env.o" =>
                [
                    "crypto/cms/cms_env.c",
                ],
            "crypto/cms/cms_err.o" =>
                [
                    "crypto/cms/cms_err.c",
                ],
            "crypto/cms/cms_ess.o" =>
                [
                    "crypto/cms/cms_ess.c",
                ],
            "crypto/cms/cms_io.o" =>
                [
                    "crypto/cms/cms_io.c",
                ],
            "crypto/cms/cms_kari.o" =>
                [
                    "crypto/cms/cms_kari.c",
                ],
            "crypto/cms/cms_lib.o" =>
                [
                    "crypto/cms/cms_lib.c",
                ],
            "crypto/cms/cms_pwri.o" =>
                [
                    "crypto/cms/cms_pwri.c",
                ],
            "crypto/cms/cms_sd.o" =>
                [
                    "crypto/cms/cms_sd.c",
                ],
            "crypto/cms/cms_smime.o" =>
                [
                    "crypto/cms/cms_smime.c",
                ],
            "crypto/conf/conf_api.o" =>
                [
                    "crypto/conf/conf_api.c",
                ],
            "crypto/conf/conf_def.o" =>
                [
                    "crypto/conf/conf_def.c",
                ],
            "crypto/conf/conf_err.o" =>
                [
                    "crypto/conf/conf_err.c",
                ],
            "crypto/conf/conf_lib.o" =>
                [
                    "crypto/conf/conf_lib.c",
                ],
            "crypto/conf/conf_mall.o" =>
                [
                    "crypto/conf/conf_mall.c",
                ],
            "crypto/conf/conf_mod.o" =>
                [
                    "crypto/conf/conf_mod.c",
                ],
            "crypto/conf/conf_sap.o" =>
                [
                    "crypto/conf/conf_sap.c",
                ],
            "crypto/conf/conf_ssl.o" =>
                [
                    "crypto/conf/conf_ssl.c",
                ],
            "crypto/cpt_err.o" =>
                [
                    "crypto/cpt_err.c",
                ],
            "crypto/cryptlib.o" =>
                [
                    "crypto/cryptlib.c",
                ],
            "crypto/ct/ct_b64.o" =>
                [
                    "crypto/ct/ct_b64.c",
                ],
            "crypto/ct/ct_err.o" =>
                [
                    "crypto/ct/ct_err.c",
                ],
            "crypto/ct/ct_log.o" =>
                [
                    "crypto/ct/ct_log.c",
                ],
            "crypto/ct/ct_oct.o" =>
                [
                    "crypto/ct/ct_oct.c",
                ],
            "crypto/ct/ct_policy.o" =>
                [
                    "crypto/ct/ct_policy.c",
                ],
            "crypto/ct/ct_prn.o" =>
                [
                    "crypto/ct/ct_prn.c",
                ],
            "crypto/ct/ct_sct.o" =>
                [
                    "crypto/ct/ct_sct.c",
                ],
            "crypto/ct/ct_sct_ctx.o" =>
                [
                    "crypto/ct/ct_sct_ctx.c",
                ],
            "crypto/ct/ct_vfy.o" =>
                [
                    "crypto/ct/ct_vfy.c",
                ],
            "crypto/ct/ct_x509v3.o" =>
                [
                    "crypto/ct/ct_x509v3.c",
                ],
            "crypto/ctype.o" =>
                [
                    "crypto/ctype.c",
                ],
            "crypto/cversion.o" =>
                [
                    "crypto/cversion.c",
                ],
            "crypto/des/cbc_cksm.o" =>
                [
                    "crypto/des/cbc_cksm.c",
                ],
            "crypto/des/cbc_enc.o" =>
                [
                    "crypto/des/cbc_enc.c",
                ],
            "crypto/des/cfb64ede.o" =>
                [
                    "crypto/des/cfb64ede.c",
                ],
            "crypto/des/cfb64enc.o" =>
                [
                    "crypto/des/cfb64enc.c",
                ],
            "crypto/des/cfb_enc.o" =>
                [
                    "crypto/des/cfb_enc.c",
                ],
            "crypto/des/crypt586.o" =>
                [
                    "crypto/des/crypt586.s",
                ],
            "crypto/des/des-586.o" =>
                [
                    "crypto/des/des-586.s",
                ],
            "crypto/des/ecb3_enc.o" =>
                [
                    "crypto/des/ecb3_enc.c",
                ],
            "crypto/des/ecb_enc.o" =>
                [
                    "crypto/des/ecb_enc.c",
                ],
            "crypto/des/fcrypt.o" =>
                [
                    "crypto/des/fcrypt.c",
                ],
            "crypto/des/ofb64ede.o" =>
                [
                    "crypto/des/ofb64ede.c",
                ],
            "crypto/des/ofb64enc.o" =>
                [
                    "crypto/des/ofb64enc.c",
                ],
            "crypto/des/ofb_enc.o" =>
                [
                    "crypto/des/ofb_enc.c",
                ],
            "crypto/des/pcbc_enc.o" =>
                [
                    "crypto/des/pcbc_enc.c",
                ],
            "crypto/des/qud_cksm.o" =>
                [
                    "crypto/des/qud_cksm.c",
                ],
            "crypto/des/rand_key.o" =>
                [
                    "crypto/des/rand_key.c",
                ],
            "crypto/des/set_key.o" =>
                [
                    "crypto/des/set_key.c",
                ],
            "crypto/des/str2key.o" =>
                [
                    "crypto/des/str2key.c",
                ],
            "crypto/des/xcbc_enc.o" =>
                [
                    "crypto/des/xcbc_enc.c",
                ],
            "crypto/dh/dh_ameth.o" =>
                [
                    "crypto/dh/dh_ameth.c",
                ],
            "crypto/dh/dh_asn1.o" =>
                [
                    "crypto/dh/dh_asn1.c",
                ],
            "crypto/dh/dh_check.o" =>
                [
                    "crypto/dh/dh_check.c",
                ],
            "crypto/dh/dh_depr.o" =>
                [
                    "crypto/dh/dh_depr.c",
                ],
            "crypto/dh/dh_err.o" =>
                [
                    "crypto/dh/dh_err.c",
                ],
            "crypto/dh/dh_gen.o" =>
                [
                    "crypto/dh/dh_gen.c",
                ],
            "crypto/dh/dh_kdf.o" =>
                [
                    "crypto/dh/dh_kdf.c",
                ],
            "crypto/dh/dh_key.o" =>
                [
                    "crypto/dh/dh_key.c",
                ],
            "crypto/dh/dh_lib.o" =>
                [
                    "crypto/dh/dh_lib.c",
                ],
            "crypto/dh/dh_meth.o" =>
                [
                    "crypto/dh/dh_meth.c",
                ],
            "crypto/dh/dh_pmeth.o" =>
                [
                    "crypto/dh/dh_pmeth.c",
                ],
            "crypto/dh/dh_prn.o" =>
                [
                    "crypto/dh/dh_prn.c",
                ],
            "crypto/dh/dh_rfc5114.o" =>
                [
                    "crypto/dh/dh_rfc5114.c",
                ],
            "crypto/dh/dh_rfc7919.o" =>
                [
                    "crypto/dh/dh_rfc7919.c",
                ],
            "crypto/dsa/dsa_ameth.o" =>
                [
                    "crypto/dsa/dsa_ameth.c",
                ],
            "crypto/dsa/dsa_asn1.o" =>
                [
                    "crypto/dsa/dsa_asn1.c",
                ],
            "crypto/dsa/dsa_depr.o" =>
                [
                    "crypto/dsa/dsa_depr.c",
                ],
            "crypto/dsa/dsa_err.o" =>
                [
                    "crypto/dsa/dsa_err.c",
                ],
            "crypto/dsa/dsa_gen.o" =>
                [
                    "crypto/dsa/dsa_gen.c",
                ],
            "crypto/dsa/dsa_key.o" =>
                [
                    "crypto/dsa/dsa_key.c",
                ],
            "crypto/dsa/dsa_lib.o" =>
                [
                    "crypto/dsa/dsa_lib.c",
                ],
            "crypto/dsa/dsa_meth.o" =>
                [
                    "crypto/dsa/dsa_meth.c",
                ],
            "crypto/dsa/dsa_ossl.o" =>
                [
                    "crypto/dsa/dsa_ossl.c",
                ],
            "crypto/dsa/dsa_pmeth.o" =>
                [
                    "crypto/dsa/dsa_pmeth.c",
                ],
            "crypto/dsa/dsa_prn.o" =>
                [
                    "crypto/dsa/dsa_prn.c",
                ],
            "crypto/dsa/dsa_sign.o" =>
                [
                    "crypto/dsa/dsa_sign.c",
                ],
            "crypto/dsa/dsa_vrf.o" =>
                [
                    "crypto/dsa/dsa_vrf.c",
                ],
            "crypto/dso/dso_dl.o" =>
                [
                    "crypto/dso/dso_dl.c",
                ],
            "crypto/dso/dso_dlfcn.o" =>
                [
                    "crypto/dso/dso_dlfcn.c",
                ],
            "crypto/dso/dso_err.o" =>
                [
                    "crypto/dso/dso_err.c",
                ],
            "crypto/dso/dso_lib.o" =>
                [
                    "crypto/dso/dso_lib.c",
                ],
            "crypto/dso/dso_openssl.o" =>
                [
                    "crypto/dso/dso_openssl.c",
                ],
            "crypto/dso/dso_vms.o" =>
                [
                    "crypto/dso/dso_vms.c",
                ],
            "crypto/dso/dso_win32.o" =>
                [
                    "crypto/dso/dso_win32.c",
                ],
            "crypto/ebcdic.o" =>
                [
                    "crypto/ebcdic.c",
                ],
            "crypto/ec/curve25519.o" =>
                [
                    "crypto/ec/curve25519.c",
                ],
            "crypto/ec/curve448/arch_32/f_impl.o" =>
                [
                    "crypto/ec/curve448/arch_32/f_impl.c",
                ],
            "crypto/ec/curve448/curve448.o" =>
                [
                    "crypto/ec/curve448/curve448.c",
                ],
            "crypto/ec/curve448/curve448_tables.o" =>
                [
                    "crypto/ec/curve448/curve448_tables.c",
                ],
            "crypto/ec/curve448/eddsa.o" =>
                [
                    "crypto/ec/curve448/eddsa.c",
                ],
            "crypto/ec/curve448/f_generic.o" =>
                [
                    "crypto/ec/curve448/f_generic.c",
                ],
            "crypto/ec/curve448/scalar.o" =>
                [
                    "crypto/ec/curve448/scalar.c",
                ],
            "crypto/ec/ec2_oct.o" =>
                [
                    "crypto/ec/ec2_oct.c",
                ],
            "crypto/ec/ec2_smpl.o" =>
                [
                    "crypto/ec/ec2_smpl.c",
                ],
            "crypto/ec/ec_ameth.o" =>
                [
                    "crypto/ec/ec_ameth.c",
                ],
            "crypto/ec/ec_asn1.o" =>
                [
                    "crypto/ec/ec_asn1.c",
                ],
            "crypto/ec/ec_check.o" =>
                [
                    "crypto/ec/ec_check.c",
                ],
            "crypto/ec/ec_curve.o" =>
                [
                    "crypto/ec/ec_curve.c",
                ],
            "crypto/ec/ec_cvt.o" =>
                [
                    "crypto/ec/ec_cvt.c",
                ],
            "crypto/ec/ec_err.o" =>
                [
                    "crypto/ec/ec_err.c",
                ],
            "crypto/ec/ec_key.o" =>
                [
                    "crypto/ec/ec_key.c",
                ],
            "crypto/ec/ec_kmeth.o" =>
                [
                    "crypto/ec/ec_kmeth.c",
                ],
            "crypto/ec/ec_lib.o" =>
                [
                    "crypto/ec/ec_lib.c",
                ],
            "crypto/ec/ec_mult.o" =>
                [
                    "crypto/ec/ec_mult.c",
                ],
            "crypto/ec/ec_oct.o" =>
                [
                    "crypto/ec/ec_oct.c",
                ],
            "crypto/ec/ec_pmeth.o" =>
                [
                    "crypto/ec/ec_pmeth.c",
                ],
            "crypto/ec/ec_print.o" =>
                [
                    "crypto/ec/ec_print.c",
                ],
            "crypto/ec/ecdh_kdf.o" =>
                [
                    "crypto/ec/ecdh_kdf.c",
                ],
            "crypto/ec/ecdh_ossl.o" =>
                [
                    "crypto/ec/ecdh_ossl.c",
                ],
            "crypto/ec/ecdsa_ossl.o" =>
                [
                    "crypto/ec/ecdsa_ossl.c",
                ],
            "crypto/ec/ecdsa_sign.o" =>
                [
                    "crypto/ec/ecdsa_sign.c",
                ],
            "crypto/ec/ecdsa_vrf.o" =>
                [
                    "crypto/ec/ecdsa_vrf.c",
                ],
            "crypto/ec/eck_prn.o" =>
                [
                    "crypto/ec/eck_prn.c",
                ],
            "crypto/ec/ecp_mont.o" =>
                [
                    "crypto/ec/ecp_mont.c",
                ],
            "crypto/ec/ecp_nist.o" =>
                [
                    "crypto/ec/ecp_nist.c",
                ],
            "crypto/ec/ecp_nistp224.o" =>
                [
                    "crypto/ec/ecp_nistp224.c",
                ],
            "crypto/ec/ecp_nistp256.o" =>
                [
                    "crypto/ec/ecp_nistp256.c",
                ],
            "crypto/ec/ecp_nistp521.o" =>
                [
                    "crypto/ec/ecp_nistp521.c",
                ],
            "crypto/ec/ecp_nistputil.o" =>
                [
                    "crypto/ec/ecp_nistputil.c",
                ],
            "crypto/ec/ecp_nistz256-x86.o" =>
                [
                    "crypto/ec/ecp_nistz256-x86.s",
                ],
            "crypto/ec/ecp_nistz256.o" =>
                [
                    "crypto/ec/ecp_nistz256.c",
                ],
            "crypto/ec/ecp_oct.o" =>
                [
                    "crypto/ec/ecp_oct.c",
                ],
            "crypto/ec/ecp_smpl.o" =>
                [
                    "crypto/ec/ecp_smpl.c",
                ],
            "crypto/ec/ecx_meth.o" =>
                [
                    "crypto/ec/ecx_meth.c",
                ],
            "crypto/engine/eng_all.o" =>
                [
                    "crypto/engine/eng_all.c",
                ],
            "crypto/engine/eng_cnf.o" =>
                [
                    "crypto/engine/eng_cnf.c",
                ],
            "crypto/engine/eng_ctrl.o" =>
                [
                    "crypto/engine/eng_ctrl.c",
                ],
            "crypto/engine/eng_dyn.o" =>
                [
                    "crypto/engine/eng_dyn.c",
                ],
            "crypto/engine/eng_err.o" =>
                [
                    "crypto/engine/eng_err.c",
                ],
            "crypto/engine/eng_fat.o" =>
                [
                    "crypto/engine/eng_fat.c",
                ],
            "crypto/engine/eng_init.o" =>
                [
                    "crypto/engine/eng_init.c",
                ],
            "crypto/engine/eng_lib.o" =>
                [
                    "crypto/engine/eng_lib.c",
                ],
            "crypto/engine/eng_list.o" =>
                [
                    "crypto/engine/eng_list.c",
                ],
            "crypto/engine/eng_openssl.o" =>
                [
                    "crypto/engine/eng_openssl.c",
                ],
            "crypto/engine/eng_pkey.o" =>
                [
                    "crypto/engine/eng_pkey.c",
                ],
            "crypto/engine/eng_rdrand.o" =>
                [
                    "crypto/engine/eng_rdrand.c",
                ],
            "crypto/engine/eng_table.o" =>
                [
                    "crypto/engine/eng_table.c",
                ],
            "crypto/engine/tb_asnmth.o" =>
                [
                    "crypto/engine/tb_asnmth.c",
                ],
            "crypto/engine/tb_cipher.o" =>
                [
                    "crypto/engine/tb_cipher.c",
                ],
            "crypto/engine/tb_dh.o" =>
                [
                    "crypto/engine/tb_dh.c",
                ],
            "crypto/engine/tb_digest.o" =>
                [
                    "crypto/engine/tb_digest.c",
                ],
            "crypto/engine/tb_dsa.o" =>
                [
                    "crypto/engine/tb_dsa.c",
                ],
            "crypto/engine/tb_eckey.o" =>
                [
                    "crypto/engine/tb_eckey.c",
                ],
            "crypto/engine/tb_pkmeth.o" =>
                [
                    "crypto/engine/tb_pkmeth.c",
                ],
            "crypto/engine/tb_rand.o" =>
                [
                    "crypto/engine/tb_rand.c",
                ],
            "crypto/engine/tb_rsa.o" =>
                [
                    "crypto/engine/tb_rsa.c",
                ],
            "crypto/err/err.o" =>
                [
                    "crypto/err/err.c",
                ],
            "crypto/err/err_all.o" =>
                [
                    "crypto/err/err_all.c",
                ],
            "crypto/err/err_prn.o" =>
                [
                    "crypto/err/err_prn.c",
                ],
            "crypto/evp/bio_b64.o" =>
                [
                    "crypto/evp/bio_b64.c",
                ],
            "crypto/evp/bio_enc.o" =>
                [
                    "crypto/evp/bio_enc.c",
                ],
            "crypto/evp/bio_md.o" =>
                [
                    "crypto/evp/bio_md.c",
                ],
            "crypto/evp/bio_ok.o" =>
                [
                    "crypto/evp/bio_ok.c",
                ],
            "crypto/evp/c_allc.o" =>
                [
                    "crypto/evp/c_allc.c",
                ],
            "crypto/evp/c_alld.o" =>
                [
                    "crypto/evp/c_alld.c",
                ],
            "crypto/evp/cmeth_lib.o" =>
                [
                    "crypto/evp/cmeth_lib.c",
                ],
            "crypto/evp/digest.o" =>
                [
                    "crypto/evp/digest.c",
                ],
            "crypto/evp/e_aes.o" =>
                [
                    "crypto/evp/e_aes.c",
                ],
            "crypto/evp/e_aes_cbc_hmac_sha1.o" =>
                [
                    "crypto/evp/e_aes_cbc_hmac_sha1.c",
                ],
            "crypto/evp/e_aes_cbc_hmac_sha256.o" =>
                [
                    "crypto/evp/e_aes_cbc_hmac_sha256.c",
                ],
            "crypto/evp/e_aria.o" =>
                [
                    "crypto/evp/e_aria.c",
                ],
            "crypto/evp/e_bf.o" =>
                [
                    "crypto/evp/e_bf.c",
                ],
            "crypto/evp/e_camellia.o" =>
                [
                    "crypto/evp/e_camellia.c",
                ],
            "crypto/evp/e_cast.o" =>
                [
                    "crypto/evp/e_cast.c",
                ],
            "crypto/evp/e_chacha20_poly1305.o" =>
                [
                    "crypto/evp/e_chacha20_poly1305.c",
                ],
            "crypto/evp/e_des.o" =>
                [
                    "crypto/evp/e_des.c",
                ],
            "crypto/evp/e_des3.o" =>
                [
                    "crypto/evp/e_des3.c",
                ],
            "crypto/evp/e_idea.o" =>
                [
                    "crypto/evp/e_idea.c",
                ],
            "crypto/evp/e_null.o" =>
                [
                    "crypto/evp/e_null.c",
                ],
            "crypto/evp/e_old.o" =>
                [
                    "crypto/evp/e_old.c",
                ],
            "crypto/evp/e_rc2.o" =>
                [
                    "crypto/evp/e_rc2.c",
                ],
            "crypto/evp/e_rc4.o" =>
                [
                    "crypto/evp/e_rc4.c",
                ],
            "crypto/evp/e_rc4_hmac_md5.o" =>
                [
                    "crypto/evp/e_rc4_hmac_md5.c",
                ],
            "crypto/evp/e_rc5.o" =>
                [
                    "crypto/evp/e_rc5.c",
                ],
            "crypto/evp/e_seed.o" =>
                [
                    "crypto/evp/e_seed.c",
                ],
            "crypto/evp/e_sm4.o" =>
                [
                    "crypto/evp/e_sm4.c",
                ],
            "crypto/evp/e_xcbc_d.o" =>
                [
                    "crypto/evp/e_xcbc_d.c",
                ],
            "crypto/evp/encode.o" =>
                [
                    "crypto/evp/encode.c",
                ],
            "crypto/evp/evp_cnf.o" =>
                [
                    "crypto/evp/evp_cnf.c",
                ],
            "crypto/evp/evp_enc.o" =>
                [
                    "crypto/evp/evp_enc.c",
                ],
            "crypto/evp/evp_err.o" =>
                [
                    "crypto/evp/evp_err.c",
                ],
            "crypto/evp/evp_key.o" =>
                [
                    "crypto/evp/evp_key.c",
                ],
            "crypto/evp/evp_lib.o" =>
                [
                    "crypto/evp/evp_lib.c",
                ],
            "crypto/evp/evp_pbe.o" =>
                [
                    "crypto/evp/evp_pbe.c",
                ],
            "crypto/evp/evp_pkey.o" =>
                [
                    "crypto/evp/evp_pkey.c",
                ],
            "crypto/evp/m_md2.o" =>
                [
                    "crypto/evp/m_md2.c",
                ],
            "crypto/evp/m_md4.o" =>
                [
                    "crypto/evp/m_md4.c",
                ],
            "crypto/evp/m_md5.o" =>
                [
                    "crypto/evp/m_md5.c",
                ],
            "crypto/evp/m_md5_sha1.o" =>
                [
                    "crypto/evp/m_md5_sha1.c",
                ],
            "crypto/evp/m_mdc2.o" =>
                [
                    "crypto/evp/m_mdc2.c",
                ],
            "crypto/evp/m_null.o" =>
                [
                    "crypto/evp/m_null.c",
                ],
            "crypto/evp/m_ripemd.o" =>
                [
                    "crypto/evp/m_ripemd.c",
                ],
            "crypto/evp/m_sha1.o" =>
                [
                    "crypto/evp/m_sha1.c",
                ],
            "crypto/evp/m_sha3.o" =>
                [
                    "crypto/evp/m_sha3.c",
                ],
            "crypto/evp/m_sigver.o" =>
                [
                    "crypto/evp/m_sigver.c",
                ],
            "crypto/evp/m_wp.o" =>
                [
                    "crypto/evp/m_wp.c",
                ],
            "crypto/evp/names.o" =>
                [
                    "crypto/evp/names.c",
                ],
            "crypto/evp/p5_crpt.o" =>
                [
                    "crypto/evp/p5_crpt.c",
                ],
            "crypto/evp/p5_crpt2.o" =>
                [
                    "crypto/evp/p5_crpt2.c",
                ],
            "crypto/evp/p_dec.o" =>
                [
                    "crypto/evp/p_dec.c",
                ],
            "crypto/evp/p_enc.o" =>
                [
                    "crypto/evp/p_enc.c",
                ],
            "crypto/evp/p_lib.o" =>
                [
                    "crypto/evp/p_lib.c",
                ],
            "crypto/evp/p_open.o" =>
                [
                    "crypto/evp/p_open.c",
                ],
            "crypto/evp/p_seal.o" =>
                [
                    "crypto/evp/p_seal.c",
                ],
            "crypto/evp/p_sign.o" =>
                [
                    "crypto/evp/p_sign.c",
                ],
            "crypto/evp/p_verify.o" =>
                [
                    "crypto/evp/p_verify.c",
                ],
            "crypto/evp/pbe_scrypt.o" =>
                [
                    "crypto/evp/pbe_scrypt.c",
                ],
            "crypto/evp/pmeth_fn.o" =>
                [
                    "crypto/evp/pmeth_fn.c",
                ],
            "crypto/evp/pmeth_gn.o" =>
                [
                    "crypto/evp/pmeth_gn.c",
                ],
            "crypto/evp/pmeth_lib.o" =>
                [
                    "crypto/evp/pmeth_lib.c",
                ],
            "crypto/ex_data.o" =>
                [
                    "crypto/ex_data.c",
                ],
            "crypto/getenv.o" =>
                [
                    "crypto/getenv.c",
                ],
            "crypto/hmac/hm_ameth.o" =>
                [
                    "crypto/hmac/hm_ameth.c",
                ],
            "crypto/hmac/hm_pmeth.o" =>
                [
                    "crypto/hmac/hm_pmeth.c",
                ],
            "crypto/hmac/hmac.o" =>
                [
                    "crypto/hmac/hmac.c",
                ],
            "crypto/idea/i_cbc.o" =>
                [
                    "crypto/idea/i_cbc.c",
                ],
            "crypto/idea/i_cfb64.o" =>
                [
                    "crypto/idea/i_cfb64.c",
                ],
            "crypto/idea/i_ecb.o" =>
                [
                    "crypto/idea/i_ecb.c",
                ],
            "crypto/idea/i_ofb64.o" =>
                [
                    "crypto/idea/i_ofb64.c",
                ],
            "crypto/idea/i_skey.o" =>
                [
                    "crypto/idea/i_skey.c",
                ],
            "crypto/init.o" =>
                [
                    "crypto/init.c",
                ],
            "crypto/kdf/hkdf.o" =>
                [
                    "crypto/kdf/hkdf.c",
                ],
            "crypto/kdf/kdf_err.o" =>
                [
                    "crypto/kdf/kdf_err.c",
                ],
            "crypto/kdf/scrypt.o" =>
                [
                    "crypto/kdf/scrypt.c",
                ],
            "crypto/kdf/tls1_prf.o" =>
                [
                    "crypto/kdf/tls1_prf.c",
                ],
            "crypto/lhash/lh_stats.o" =>
                [
                    "crypto/lhash/lh_stats.c",
                ],
            "crypto/lhash/lhash.o" =>
                [
                    "crypto/lhash/lhash.c",
                ],
            "crypto/md4/md4_dgst.o" =>
                [
                    "crypto/md4/md4_dgst.c",
                ],
            "crypto/md4/md4_one.o" =>
                [
                    "crypto/md4/md4_one.c",
                ],
            "crypto/md5/md5-586.o" =>
                [
                    "crypto/md5/md5-586.s",
                ],
            "crypto/md5/md5_dgst.o" =>
                [
                    "crypto/md5/md5_dgst.c",
                ],
            "crypto/md5/md5_one.o" =>
                [
                    "crypto/md5/md5_one.c",
                ],
            "crypto/mdc2/mdc2_one.o" =>
                [
                    "crypto/mdc2/mdc2_one.c",
                ],
            "crypto/mdc2/mdc2dgst.o" =>
                [
                    "crypto/mdc2/mdc2dgst.c",
                ],
            "crypto/mem.o" =>
                [
                    "crypto/mem.c",
                ],
            "crypto/mem_dbg.o" =>
                [
                    "crypto/mem_dbg.c",
                ],
            "crypto/mem_sec.o" =>
                [
                    "crypto/mem_sec.c",
                ],
            "crypto/modes/cbc128.o" =>
                [
                    "crypto/modes/cbc128.c",
                ],
            "crypto/modes/ccm128.o" =>
                [
                    "crypto/modes/ccm128.c",
                ],
            "crypto/modes/cfb128.o" =>
                [
                    "crypto/modes/cfb128.c",
                ],
            "crypto/modes/ctr128.o" =>
                [
                    "crypto/modes/ctr128.c",
                ],
            "crypto/modes/cts128.o" =>
                [
                    "crypto/modes/cts128.c",
                ],
            "crypto/modes/gcm128.o" =>
                [
                    "crypto/modes/gcm128.c",
                ],
            "crypto/modes/ghash-x86.o" =>
                [
                    "crypto/modes/ghash-x86.s",
                ],
            "crypto/modes/ocb128.o" =>
                [
                    "crypto/modes/ocb128.c",
                ],
            "crypto/modes/ofb128.o" =>
                [
                    "crypto/modes/ofb128.c",
                ],
            "crypto/modes/wrap128.o" =>
                [
                    "crypto/modes/wrap128.c",
                ],
            "crypto/modes/xts128.o" =>
                [
                    "crypto/modes/xts128.c",
                ],
            "crypto/o_dir.o" =>
                [
                    "crypto/o_dir.c",
                ],
            "crypto/o_fips.o" =>
                [
                    "crypto/o_fips.c",
                ],
            "crypto/o_fopen.o" =>
                [
                    "crypto/o_fopen.c",
                ],
            "crypto/o_init.o" =>
                [
                    "crypto/o_init.c",
                ],
            "crypto/o_str.o" =>
                [
                    "crypto/o_str.c",
                ],
            "crypto/o_time.o" =>
                [
                    "crypto/o_time.c",
                ],
            "crypto/objects/o_names.o" =>
                [
                    "crypto/objects/o_names.c",
                ],
            "crypto/objects/obj_dat.o" =>
                [
                    "crypto/objects/obj_dat.c",
                ],
            "crypto/objects/obj_err.o" =>
                [
                    "crypto/objects/obj_err.c",
                ],
            "crypto/objects/obj_lib.o" =>
                [
                    "crypto/objects/obj_lib.c",
                ],
            "crypto/objects/obj_xref.o" =>
                [
                    "crypto/objects/obj_xref.c",
                ],
            "crypto/ocsp/ocsp_asn.o" =>
                [
                    "crypto/ocsp/ocsp_asn.c",
                ],
            "crypto/ocsp/ocsp_cl.o" =>
                [
                    "crypto/ocsp/ocsp_cl.c",
                ],
            "crypto/ocsp/ocsp_err.o" =>
                [
                    "crypto/ocsp/ocsp_err.c",
                ],
            "crypto/ocsp/ocsp_ext.o" =>
                [
                    "crypto/ocsp/ocsp_ext.c",
                ],
            "crypto/ocsp/ocsp_ht.o" =>
                [
                    "crypto/ocsp/ocsp_ht.c",
                ],
            "crypto/ocsp/ocsp_lib.o" =>
                [
                    "crypto/ocsp/ocsp_lib.c",
                ],
            "crypto/ocsp/ocsp_prn.o" =>
                [
                    "crypto/ocsp/ocsp_prn.c",
                ],
            "crypto/ocsp/ocsp_srv.o" =>
                [
                    "crypto/ocsp/ocsp_srv.c",
                ],
            "crypto/ocsp/ocsp_vfy.o" =>
                [
                    "crypto/ocsp/ocsp_vfy.c",
                ],
            "crypto/ocsp/v3_ocsp.o" =>
                [
                    "crypto/ocsp/v3_ocsp.c",
                ],
            "crypto/pem/pem_all.o" =>
                [
                    "crypto/pem/pem_all.c",
                ],
            "crypto/pem/pem_err.o" =>
                [
                    "crypto/pem/pem_err.c",
                ],
            "crypto/pem/pem_info.o" =>
                [
                    "crypto/pem/pem_info.c",
                ],
            "crypto/pem/pem_lib.o" =>
                [
                    "crypto/pem/pem_lib.c",
                ],
            "crypto/pem/pem_oth.o" =>
                [
                    "crypto/pem/pem_oth.c",
                ],
            "crypto/pem/pem_pk8.o" =>
                [
                    "crypto/pem/pem_pk8.c",
                ],
            "crypto/pem/pem_pkey.o" =>
                [
                    "crypto/pem/pem_pkey.c",
                ],
            "crypto/pem/pem_sign.o" =>
                [
                    "crypto/pem/pem_sign.c",
                ],
            "crypto/pem/pem_x509.o" =>
                [
                    "crypto/pem/pem_x509.c",
                ],
            "crypto/pem/pem_xaux.o" =>
                [
                    "crypto/pem/pem_xaux.c",
                ],
            "crypto/pem/pvkfmt.o" =>
                [
                    "crypto/pem/pvkfmt.c",
                ],
            "crypto/pkcs12/p12_add.o" =>
                [
                    "crypto/pkcs12/p12_add.c",
                ],
            "crypto/pkcs12/p12_asn.o" =>
                [
                    "crypto/pkcs12/p12_asn.c",
                ],
            "crypto/pkcs12/p12_attr.o" =>
                [
                    "crypto/pkcs12/p12_attr.c",
                ],
            "crypto/pkcs12/p12_crpt.o" =>
                [
                    "crypto/pkcs12/p12_crpt.c",
                ],
            "crypto/pkcs12/p12_crt.o" =>
                [
                    "crypto/pkcs12/p12_crt.c",
                ],
            "crypto/pkcs12/p12_decr.o" =>
                [
                    "crypto/pkcs12/p12_decr.c",
                ],
            "crypto/pkcs12/p12_init.o" =>
                [
                    "crypto/pkcs12/p12_init.c",
                ],
            "crypto/pkcs12/p12_key.o" =>
                [
                    "crypto/pkcs12/p12_key.c",
                ],
            "crypto/pkcs12/p12_kiss.o" =>
                [
                    "crypto/pkcs12/p12_kiss.c",
                ],
            "crypto/pkcs12/p12_mutl.o" =>
                [
                    "crypto/pkcs12/p12_mutl.c",
                ],
            "crypto/pkcs12/p12_npas.o" =>
                [
                    "crypto/pkcs12/p12_npas.c",
                ],
            "crypto/pkcs12/p12_p8d.o" =>
                [
                    "crypto/pkcs12/p12_p8d.c",
                ],
            "crypto/pkcs12/p12_p8e.o" =>
                [
                    "crypto/pkcs12/p12_p8e.c",
                ],
            "crypto/pkcs12/p12_sbag.o" =>
                [
                    "crypto/pkcs12/p12_sbag.c",
                ],
            "crypto/pkcs12/p12_utl.o" =>
                [
                    "crypto/pkcs12/p12_utl.c",
                ],
            "crypto/pkcs12/pk12err.o" =>
                [
                    "crypto/pkcs12/pk12err.c",
                ],
            "crypto/pkcs7/bio_pk7.o" =>
                [
                    "crypto/pkcs7/bio_pk7.c",
                ],
            "crypto/pkcs7/pk7_asn1.o" =>
                [
                    "crypto/pkcs7/pk7_asn1.c",
                ],
            "crypto/pkcs7/pk7_attr.o" =>
                [
                    "crypto/pkcs7/pk7_attr.c",
                ],
            "crypto/pkcs7/pk7_doit.o" =>
                [
                    "crypto/pkcs7/pk7_doit.c",
                ],
            "crypto/pkcs7/pk7_lib.o" =>
                [
                    "crypto/pkcs7/pk7_lib.c",
                ],
            "crypto/pkcs7/pk7_mime.o" =>
                [
                    "crypto/pkcs7/pk7_mime.c",
                ],
            "crypto/pkcs7/pk7_smime.o" =>
                [
                    "crypto/pkcs7/pk7_smime.c",
                ],
            "crypto/pkcs7/pkcs7err.o" =>
                [
                    "crypto/pkcs7/pkcs7err.c",
                ],
            "crypto/poly1305/poly1305-x86.o" =>
                [
                    "crypto/poly1305/poly1305-x86.s",
                ],
            "crypto/poly1305/poly1305.o" =>
                [
                    "crypto/poly1305/poly1305.c",
                ],
            "crypto/poly1305/poly1305_ameth.o" =>
                [
                    "crypto/poly1305/poly1305_ameth.c",
                ],
            "crypto/poly1305/poly1305_pmeth.o" =>
                [
                    "crypto/poly1305/poly1305_pmeth.c",
                ],
            "crypto/rand/drbg_ctr.o" =>
                [
                    "crypto/rand/drbg_ctr.c",
                ],
            "crypto/rand/drbg_lib.o" =>
                [
                    "crypto/rand/drbg_lib.c",
                ],
            "crypto/rand/rand_egd.o" =>
                [
                    "crypto/rand/rand_egd.c",
                ],
            "crypto/rand/rand_err.o" =>
                [
                    "crypto/rand/rand_err.c",
                ],
            "crypto/rand/rand_lib.o" =>
                [
                    "crypto/rand/rand_lib.c",
                ],
            "crypto/rand/rand_unix.o" =>
                [
                    "crypto/rand/rand_unix.c",
                ],
            "crypto/rand/rand_vms.o" =>
                [
                    "crypto/rand/rand_vms.c",
                ],
            "crypto/rand/rand_win.o" =>
                [
                    "crypto/rand/rand_win.c",
                ],
            "crypto/rand/randfile.o" =>
                [
                    "crypto/rand/randfile.c",
                ],
            "crypto/rc2/rc2_cbc.o" =>
                [
                    "crypto/rc2/rc2_cbc.c",
                ],
            "crypto/rc2/rc2_ecb.o" =>
                [
                    "crypto/rc2/rc2_ecb.c",
                ],
            "crypto/rc2/rc2_skey.o" =>
                [
                    "crypto/rc2/rc2_skey.c",
                ],
            "crypto/rc2/rc2cfb64.o" =>
                [
                    "crypto/rc2/rc2cfb64.c",
                ],
            "crypto/rc2/rc2ofb64.o" =>
                [
                    "crypto/rc2/rc2ofb64.c",
                ],
            "crypto/rc4/rc4-586.o" =>
                [
                    "crypto/rc4/rc4-586.s",
                ],
            "crypto/ripemd/rmd-586.o" =>
                [
                    "crypto/ripemd/rmd-586.s",
                ],
            "crypto/ripemd/rmd_dgst.o" =>
                [
                    "crypto/ripemd/rmd_dgst.c",
                ],
            "crypto/ripemd/rmd_one.o" =>
                [
                    "crypto/ripemd/rmd_one.c",
                ],
            "crypto/rsa/rsa_ameth.o" =>
                [
                    "crypto/rsa/rsa_ameth.c",
                ],
            "crypto/rsa/rsa_asn1.o" =>
                [
                    "crypto/rsa/rsa_asn1.c",
                ],
            "crypto/rsa/rsa_chk.o" =>
                [
                    "crypto/rsa/rsa_chk.c",
                ],
            "crypto/rsa/rsa_crpt.o" =>
                [
                    "crypto/rsa/rsa_crpt.c",
                ],
            "crypto/rsa/rsa_depr.o" =>
                [
                    "crypto/rsa/rsa_depr.c",
                ],
            "crypto/rsa/rsa_err.o" =>
                [
                    "crypto/rsa/rsa_err.c",
                ],
            "crypto/rsa/rsa_gen.o" =>
                [
                    "crypto/rsa/rsa_gen.c",
                ],
            "crypto/rsa/rsa_lib.o" =>
                [
                    "crypto/rsa/rsa_lib.c",
                ],
            "crypto/rsa/rsa_meth.o" =>
                [
                    "crypto/rsa/rsa_meth.c",
                ],
            "crypto/rsa/rsa_mp.o" =>
                [
                    "crypto/rsa/rsa_mp.c",
                ],
            "crypto/rsa/rsa_none.o" =>
                [
                    "crypto/rsa/rsa_none.c",
                ],
            "crypto/rsa/rsa_oaep.o" =>
                [
                    "crypto/rsa/rsa_oaep.c",
                ],
            "crypto/rsa/rsa_ossl.o" =>
                [
                    "crypto/rsa/rsa_ossl.c",
                ],
            "crypto/rsa/rsa_pk1.o" =>
                [
                    "crypto/rsa/rsa_pk1.c",
                ],
            "crypto/rsa/rsa_pmeth.o" =>
                [
                    "crypto/rsa/rsa_pmeth.c",
                ],
            "crypto/rsa/rsa_prn.o" =>
                [
                    "crypto/rsa/rsa_prn.c",
                ],
            "crypto/rsa/rsa_pss.o" =>
                [
                    "crypto/rsa/rsa_pss.c",
                ],
            "crypto/rsa/rsa_saos.o" =>
                [
                    "crypto/rsa/rsa_saos.c",
                ],
            "crypto/rsa/rsa_sign.o" =>
                [
                    "crypto/rsa/rsa_sign.c",
                ],
            "crypto/rsa/rsa_ssl.o" =>
                [
                    "crypto/rsa/rsa_ssl.c",
                ],
            "crypto/rsa/rsa_x931.o" =>
                [
                    "crypto/rsa/rsa_x931.c",
                ],
            "crypto/rsa/rsa_x931g.o" =>
                [
                    "crypto/rsa/rsa_x931g.c",
                ],
            "crypto/seed/seed.o" =>
                [
                    "crypto/seed/seed.c",
                ],
            "crypto/seed/seed_cbc.o" =>
                [
                    "crypto/seed/seed_cbc.c",
                ],
            "crypto/seed/seed_cfb.o" =>
                [
                    "crypto/seed/seed_cfb.c",
                ],
            "crypto/seed/seed_ecb.o" =>
                [
                    "crypto/seed/seed_ecb.c",
                ],
            "crypto/seed/seed_ofb.o" =>
                [
                    "crypto/seed/seed_ofb.c",
                ],
            "crypto/sha/keccak1600.o" =>
                [
                    "crypto/sha/keccak1600.c",
                ],
            "crypto/sha/sha1-586.o" =>
                [
                    "crypto/sha/sha1-586.s",
                ],
            "crypto/sha/sha1_one.o" =>
                [
                    "crypto/sha/sha1_one.c",
                ],
            "crypto/sha/sha1dgst.o" =>
                [
                    "crypto/sha/sha1dgst.c",
                ],
            "crypto/sha/sha256-586.o" =>
                [
                    "crypto/sha/sha256-586.s",
                ],
            "crypto/sha/sha256.o" =>
                [
                    "crypto/sha/sha256.c",
                ],
            "crypto/sha/sha512-586.o" =>
                [
                    "crypto/sha/sha512-586.s",
                ],
            "crypto/sha/sha512.o" =>
                [
                    "crypto/sha/sha512.c",
                ],
            "crypto/siphash/siphash.o" =>
                [
                    "crypto/siphash/siphash.c",
                ],
            "crypto/siphash/siphash_ameth.o" =>
                [
                    "crypto/siphash/siphash_ameth.c",
                ],
            "crypto/siphash/siphash_pmeth.o" =>
                [
                    "crypto/siphash/siphash_pmeth.c",
                ],
            "crypto/sm2/sm2_crypt.o" =>
                [
                    "crypto/sm2/sm2_crypt.c",
                ],
            "crypto/sm2/sm2_err.o" =>
                [
                    "crypto/sm2/sm2_err.c",
                ],
            "crypto/sm2/sm2_pmeth.o" =>
                [
                    "crypto/sm2/sm2_pmeth.c",
                ],
            "crypto/sm2/sm2_sign.o" =>
                [
                    "crypto/sm2/sm2_sign.c",
                ],
            "crypto/sm3/m_sm3.o" =>
                [
                    "crypto/sm3/m_sm3.c",
                ],
            "crypto/sm3/sm3.o" =>
                [
                    "crypto/sm3/sm3.c",
                ],
            "crypto/sm4/sm4.o" =>
                [
                    "crypto/sm4/sm4.c",
                ],
            "crypto/srp/srp_lib.o" =>
                [
                    "crypto/srp/srp_lib.c",
                ],
            "crypto/srp/srp_vfy.o" =>
                [
                    "crypto/srp/srp_vfy.c",
                ],
            "crypto/stack/stack.o" =>
                [
                    "crypto/stack/stack.c",
                ],
            "crypto/store/loader_file.o" =>
                [
                    "crypto/store/loader_file.c",
                ],
            "crypto/store/store_err.o" =>
                [
                    "crypto/store/store_err.c",
                ],
            "crypto/store/store_init.o" =>
                [
                    "crypto/store/store_init.c",
                ],
            "crypto/store/store_lib.o" =>
                [
                    "crypto/store/store_lib.c",
                ],
            "crypto/store/store_register.o" =>
                [
                    "crypto/store/store_register.c",
                ],
            "crypto/store/store_strings.o" =>
                [
                    "crypto/store/store_strings.c",
                ],
            "crypto/threads_none.o" =>
                [
                    "crypto/threads_none.c",
                ],
            "crypto/threads_pthread.o" =>
                [
                    "crypto/threads_pthread.c",
                ],
            "crypto/threads_win.o" =>
                [
                    "crypto/threads_win.c",
                ],
            "crypto/ts/ts_asn1.o" =>
                [
                    "crypto/ts/ts_asn1.c",
                ],
            "crypto/ts/ts_conf.o" =>
                [
                    "crypto/ts/ts_conf.c",
                ],
            "crypto/ts/ts_err.o" =>
                [
                    "crypto/ts/ts_err.c",
                ],
            "crypto/ts/ts_lib.o" =>
                [
                    "crypto/ts/ts_lib.c",
                ],
            "crypto/ts/ts_req_print.o" =>
                [
                    "crypto/ts/ts_req_print.c",
                ],
            "crypto/ts/ts_req_utils.o" =>
                [
                    "crypto/ts/ts_req_utils.c",
                ],
            "crypto/ts/ts_rsp_print.o" =>
                [
                    "crypto/ts/ts_rsp_print.c",
                ],
            "crypto/ts/ts_rsp_sign.o" =>
                [
                    "crypto/ts/ts_rsp_sign.c",
                ],
            "crypto/ts/ts_rsp_utils.o" =>
                [
                    "crypto/ts/ts_rsp_utils.c",
                ],
            "crypto/ts/ts_rsp_verify.o" =>
                [
                    "crypto/ts/ts_rsp_verify.c",
                ],
            "crypto/ts/ts_verify_ctx.o" =>
                [
                    "crypto/ts/ts_verify_ctx.c",
                ],
            "crypto/txt_db/txt_db.o" =>
                [
                    "crypto/txt_db/txt_db.c",
                ],
            "crypto/ui/ui_err.o" =>
                [
                    "crypto/ui/ui_err.c",
                ],
            "crypto/ui/ui_lib.o" =>
                [
                    "crypto/ui/ui_lib.c",
                ],
            "crypto/ui/ui_null.o" =>
                [
                    "crypto/ui/ui_null.c",
                ],
            "crypto/ui/ui_openssl.o" =>
                [
                    "crypto/ui/ui_openssl.c",
                ],
            "crypto/ui/ui_util.o" =>
                [
                    "crypto/ui/ui_util.c",
                ],
            "crypto/uid.o" =>
                [
                    "crypto/uid.c",
                ],
            "crypto/whrlpool/wp-mmx.o" =>
                [
                    "crypto/whrlpool/wp-mmx.s",
                ],
            "crypto/whrlpool/wp_block.o" =>
                [
                    "crypto/whrlpool/wp_block.c",
                ],
            "crypto/whrlpool/wp_dgst.o" =>
                [
                    "crypto/whrlpool/wp_dgst.c",
                ],
            "crypto/x509/by_dir.o" =>
                [
                    "crypto/x509/by_dir.c",
                ],
            "crypto/x509/by_file.o" =>
                [
                    "crypto/x509/by_file.c",
                ],
            "crypto/x509/t_crl.o" =>
                [
                    "crypto/x509/t_crl.c",
                ],
            "crypto/x509/t_req.o" =>
                [
                    "crypto/x509/t_req.c",
                ],
            "crypto/x509/t_x509.o" =>
                [
                    "crypto/x509/t_x509.c",
                ],
            "crypto/x509/x509_att.o" =>
                [
                    "crypto/x509/x509_att.c",
                ],
            "crypto/x509/x509_cmp.o" =>
                [
                    "crypto/x509/x509_cmp.c",
                ],
            "crypto/x509/x509_d2.o" =>
                [
                    "crypto/x509/x509_d2.c",
                ],
            "crypto/x509/x509_def.o" =>
                [
                    "crypto/x509/x509_def.c",
                ],
            "crypto/x509/x509_err.o" =>
                [
                    "crypto/x509/x509_err.c",
                ],
            "crypto/x509/x509_ext.o" =>
                [
                    "crypto/x509/x509_ext.c",
                ],
            "crypto/x509/x509_lu.o" =>
                [
                    "crypto/x509/x509_lu.c",
                ],
            "crypto/x509/x509_meth.o" =>
                [
                    "crypto/x509/x509_meth.c",
                ],
            "crypto/x509/x509_obj.o" =>
                [
                    "crypto/x509/x509_obj.c",
                ],
            "crypto/x509/x509_r2x.o" =>
                [
                    "crypto/x509/x509_r2x.c",
                ],
            "crypto/x509/x509_req.o" =>
                [
                    "crypto/x509/x509_req.c",
                ],
            "crypto/x509/x509_set.o" =>
                [
                    "crypto/x509/x509_set.c",
                ],
            "crypto/x509/x509_trs.o" =>
                [
                    "crypto/x509/x509_trs.c",
                ],
            "crypto/x509/x509_txt.o" =>
                [
                    "crypto/x509/x509_txt.c",
                ],
            "crypto/x509/x509_v3.o" =>
                [
                    "crypto/x509/x509_v3.c",
                ],
            "crypto/x509/x509_vfy.o" =>
                [
                    "crypto/x509/x509_vfy.c",
                ],
            "crypto/x509/x509_vpm.o" =>
                [
                    "crypto/x509/x509_vpm.c",
                ],
            "crypto/x509/x509cset.o" =>
                [
                    "crypto/x509/x509cset.c",
                ],
            "crypto/x509/x509name.o" =>
                [
                    "crypto/x509/x509name.c",
                ],
            "crypto/x509/x509rset.o" =>
                [
                    "crypto/x509/x509rset.c",
                ],
            "crypto/x509/x509spki.o" =>
                [
                    "crypto/x509/x509spki.c",
                ],
            "crypto/x509/x509type.o" =>
                [
                    "crypto/x509/x509type.c",
                ],
            "crypto/x509/x_all.o" =>
                [
                    "crypto/x509/x_all.c",
                ],
            "crypto/x509/x_attrib.o" =>
                [
                    "crypto/x509/x_attrib.c",
                ],
            "crypto/x509/x_crl.o" =>
                [
                    "crypto/x509/x_crl.c",
                ],
            "crypto/x509/x_exten.o" =>
                [
                    "crypto/x509/x_exten.c",
                ],
            "crypto/x509/x_name.o" =>
                [
                    "crypto/x509/x_name.c",
                ],
            "crypto/x509/x_pubkey.o" =>
                [
                    "crypto/x509/x_pubkey.c",
                ],
            "crypto/x509/x_req.o" =>
                [
                    "crypto/x509/x_req.c",
                ],
            "crypto/x509/x_x509.o" =>
                [
                    "crypto/x509/x_x509.c",
                ],
            "crypto/x509/x_x509a.o" =>
                [
                    "crypto/x509/x_x509a.c",
                ],
            "crypto/x509v3/pcy_cache.o" =>
                [
                    "crypto/x509v3/pcy_cache.c",
                ],
            "crypto/x509v3/pcy_data.o" =>
                [
                    "crypto/x509v3/pcy_data.c",
                ],
            "crypto/x509v3/pcy_lib.o" =>
                [
                    "crypto/x509v3/pcy_lib.c",
                ],
            "crypto/x509v3/pcy_map.o" =>
                [
                    "crypto/x509v3/pcy_map.c",
                ],
            "crypto/x509v3/pcy_node.o" =>
                [
                    "crypto/x509v3/pcy_node.c",
                ],
            "crypto/x509v3/pcy_tree.o" =>
                [
                    "crypto/x509v3/pcy_tree.c",
                ],
            "crypto/x509v3/v3_addr.o" =>
                [
                    "crypto/x509v3/v3_addr.c",
                ],
            "crypto/x509v3/v3_admis.o" =>
                [
                    "crypto/x509v3/v3_admis.c",
                ],
            "crypto/x509v3/v3_akey.o" =>
                [
                    "crypto/x509v3/v3_akey.c",
                ],
            "crypto/x509v3/v3_akeya.o" =>
                [
                    "crypto/x509v3/v3_akeya.c",
                ],
            "crypto/x509v3/v3_alt.o" =>
                [
                    "crypto/x509v3/v3_alt.c",
                ],
            "crypto/x509v3/v3_asid.o" =>
                [
                    "crypto/x509v3/v3_asid.c",
                ],
            "crypto/x509v3/v3_bcons.o" =>
                [
                    "crypto/x509v3/v3_bcons.c",
                ],
            "crypto/x509v3/v3_bitst.o" =>
                [
                    "crypto/x509v3/v3_bitst.c",
                ],
            "crypto/x509v3/v3_conf.o" =>
                [
                    "crypto/x509v3/v3_conf.c",
                ],
            "crypto/x509v3/v3_cpols.o" =>
                [
                    "crypto/x509v3/v3_cpols.c",
                ],
            "crypto/x509v3/v3_crld.o" =>
                [
                    "crypto/x509v3/v3_crld.c",
                ],
            "crypto/x509v3/v3_enum.o" =>
                [
                    "crypto/x509v3/v3_enum.c",
                ],
            "crypto/x509v3/v3_extku.o" =>
                [
                    "crypto/x509v3/v3_extku.c",
                ],
            "crypto/x509v3/v3_genn.o" =>
                [
                    "crypto/x509v3/v3_genn.c",
                ],
            "crypto/x509v3/v3_ia5.o" =>
                [
                    "crypto/x509v3/v3_ia5.c",
                ],
            "crypto/x509v3/v3_info.o" =>
                [
                    "crypto/x509v3/v3_info.c",
                ],
            "crypto/x509v3/v3_int.o" =>
                [
                    "crypto/x509v3/v3_int.c",
                ],
            "crypto/x509v3/v3_lib.o" =>
                [
                    "crypto/x509v3/v3_lib.c",
                ],
            "crypto/x509v3/v3_ncons.o" =>
                [
                    "crypto/x509v3/v3_ncons.c",
                ],
            "crypto/x509v3/v3_pci.o" =>
                [
                    "crypto/x509v3/v3_pci.c",
                ],
            "crypto/x509v3/v3_pcia.o" =>
                [
                    "crypto/x509v3/v3_pcia.c",
                ],
            "crypto/x509v3/v3_pcons.o" =>
                [
                    "crypto/x509v3/v3_pcons.c",
                ],
            "crypto/x509v3/v3_pku.o" =>
                [
                    "crypto/x509v3/v3_pku.c",
                ],
            "crypto/x509v3/v3_pmaps.o" =>
                [
                    "crypto/x509v3/v3_pmaps.c",
                ],
            "crypto/x509v3/v3_prn.o" =>
                [
                    "crypto/x509v3/v3_prn.c",
                ],
            "crypto/x509v3/v3_purp.o" =>
                [
                    "crypto/x509v3/v3_purp.c",
                ],
            "crypto/x509v3/v3_skey.o" =>
                [
                    "crypto/x509v3/v3_skey.c",
                ],
            "crypto/x509v3/v3_sxnet.o" =>
                [
                    "crypto/x509v3/v3_sxnet.c",
                ],
            "crypto/x509v3/v3_tlsf.o" =>
                [
                    "crypto/x509v3/v3_tlsf.c",
                ],
            "crypto/x509v3/v3_utl.o" =>
                [
                    "crypto/x509v3/v3_utl.c",
                ],
            "crypto/x509v3/v3err.o" =>
                [
                    "crypto/x509v3/v3err.c",
                ],
            "crypto/x86cpuid.o" =>
                [
                    "crypto/x86cpuid.s",
                ],
            "engines/e_capi.o" =>
                [
                    "engines/e_capi.c",
                ],
            "engines/e_padlock-x86.o" =>
                [
                    "engines/e_padlock-x86.s",
                ],
            "engines/e_padlock.o" =>
                [
                    "engines/e_padlock.c",
                ],
            "fuzz/asn1-test" =>
                [
                    "fuzz/asn1.o",
                    "fuzz/test-corpus.o",
                ],
            "fuzz/asn1.o" =>
                [
                    "fuzz/asn1.c",
                ],
            "fuzz/asn1parse-test" =>
                [
                    "fuzz/asn1parse.o",
                    "fuzz/test-corpus.o",
                ],
            "fuzz/asn1parse.o" =>
                [
                    "fuzz/asn1parse.c",
                ],
            "fuzz/bignum-test" =>
                [
                    "fuzz/bignum.o",
                    "fuzz/test-corpus.o",
                ],
            "fuzz/bignum.o" =>
                [
                    "fuzz/bignum.c",
                ],
            "fuzz/bndiv-test" =>
                [
                    "fuzz/bndiv.o",
                    "fuzz/test-corpus.o",
                ],
            "fuzz/bndiv.o" =>
                [
                    "fuzz/bndiv.c",
                ],
            "fuzz/client-test" =>
                [
                    "fuzz/client.o",
                    "fuzz/test-corpus.o",
                ],
            "fuzz/client.o" =>
                [
                    "fuzz/client.c",
                ],
            "fuzz/cms-test" =>
                [
                    "fuzz/cms.o",
                    "fuzz/test-corpus.o",
                ],
            "fuzz/cms.o" =>
                [
                    "fuzz/cms.c",
                ],
            "fuzz/conf-test" =>
                [
                    "fuzz/conf.o",
                    "fuzz/test-corpus.o",
                ],
            "fuzz/conf.o" =>
                [
                    "fuzz/conf.c",
                ],
            "fuzz/crl-test" =>
                [
                    "fuzz/crl.o",
                    "fuzz/test-corpus.o",
                ],
            "fuzz/crl.o" =>
                [
                    "fuzz/crl.c",
                ],
            "fuzz/ct-test" =>
                [
                    "fuzz/ct.o",
                    "fuzz/test-corpus.o",
                ],
            "fuzz/ct.o" =>
                [
                    "fuzz/ct.c",
                ],
            "fuzz/server-test" =>
                [
                    "fuzz/server.o",
                    "fuzz/test-corpus.o",
                ],
            "fuzz/server.o" =>
                [
                    "fuzz/server.c",
                ],
            "fuzz/test-corpus.o" =>
                [
                    "fuzz/test-corpus.c",
                ],
            "fuzz/x509-test" =>
                [
                    "fuzz/test-corpus.o",
                    "fuzz/x509.o",
                ],
            "fuzz/x509.o" =>
                [
                    "fuzz/x509.c",
                ],
            "libcrypto" =>
                [
                    "crypto/aes/aes_cbc.o",
                    "crypto/aes/aes_cfb.o",
                    "crypto/aes/aes_core.o",
                    "crypto/aes/aes_ecb.o",
                    "crypto/aes/aes_ige.o",
                    "crypto/aes/aes_misc.o",
                    "crypto/aes/aes_ofb.o",
                    "crypto/aes/aes_wrap.o",
                    "crypto/aes/aesni-x86.o",
                    "crypto/aes/vpaes-x86.o",
                    "crypto/aria/aria.o",
                    "crypto/asn1/a_bitstr.o",
                    "crypto/asn1/a_d2i_fp.o",
                    "crypto/asn1/a_digest.o",
                    "crypto/asn1/a_dup.o",
                    "crypto/asn1/a_gentm.o",
                    "crypto/asn1/a_i2d_fp.o",
                    "crypto/asn1/a_int.o",
                    "crypto/asn1/a_mbstr.o",
                    "crypto/asn1/a_object.o",
                    "crypto/asn1/a_octet.o",
                    "crypto/asn1/a_print.o",
                    "crypto/asn1/a_sign.o",
                    "crypto/asn1/a_strex.o",
                    "crypto/asn1/a_strnid.o",
                    "crypto/asn1/a_time.o",
                    "crypto/asn1/a_type.o",
                    "crypto/asn1/a_utctm.o",
                    "crypto/asn1/a_utf8.o",
                    "crypto/asn1/a_verify.o",
                    "crypto/asn1/ameth_lib.o",
                    "crypto/asn1/asn1_err.o",
                    "crypto/asn1/asn1_gen.o",
                    "crypto/asn1/asn1_item_list.o",
                    "crypto/asn1/asn1_lib.o",
                    "crypto/asn1/asn1_par.o",
                    "crypto/asn1/asn_mime.o",
                    "crypto/asn1/asn_moid.o",
                    "crypto/asn1/asn_mstbl.o",
                    "crypto/asn1/asn_pack.o",
                    "crypto/asn1/bio_asn1.o",
                    "crypto/asn1/bio_ndef.o",
                    "crypto/asn1/d2i_pr.o",
                    "crypto/asn1/d2i_pu.o",
                    "crypto/asn1/evp_asn1.o",
                    "crypto/asn1/f_int.o",
                    "crypto/asn1/f_string.o",
                    "crypto/asn1/i2d_pr.o",
                    "crypto/asn1/i2d_pu.o",
                    "crypto/asn1/n_pkey.o",
                    "crypto/asn1/nsseq.o",
                    "crypto/asn1/p5_pbe.o",
                    "crypto/asn1/p5_pbev2.o",
                    "crypto/asn1/p5_scrypt.o",
                    "crypto/asn1/p8_pkey.o",
                    "crypto/asn1/t_bitst.o",
                    "crypto/asn1/t_pkey.o",
                    "crypto/asn1/t_spki.o",
                    "crypto/asn1/tasn_dec.o",
                    "crypto/asn1/tasn_enc.o",
                    "crypto/asn1/tasn_fre.o",
                    "crypto/asn1/tasn_new.o",
                    "crypto/asn1/tasn_prn.o",
                    "crypto/asn1/tasn_scn.o",
                    "crypto/asn1/tasn_typ.o",
                    "crypto/asn1/tasn_utl.o",
                    "crypto/asn1/x_algor.o",
                    "crypto/asn1/x_bignum.o",
                    "crypto/asn1/x_info.o",
                    "crypto/asn1/x_int64.o",
                    "crypto/asn1/x_long.o",
                    "crypto/asn1/x_pkey.o",
                    "crypto/asn1/x_sig.o",
                    "crypto/asn1/x_spki.o",
                    "crypto/asn1/x_val.o",
                    "crypto/async/arch/async_null.o",
                    "crypto/async/arch/async_posix.o",
                    "crypto/async/arch/async_win.o",
                    "crypto/async/async.o",
                    "crypto/async/async_err.o",
                    "crypto/async/async_wait.o",
                    "crypto/bf/bf-586.o",
                    "crypto/bf/bf_cfb64.o",
                    "crypto/bf/bf_ecb.o",
                    "crypto/bf/bf_ofb64.o",
                    "crypto/bf/bf_skey.o",
                    "crypto/bio/b_addr.o",
                    "crypto/bio/b_dump.o",
                    "crypto/bio/b_print.o",
                    "crypto/bio/b_sock.o",
                    "crypto/bio/b_sock2.o",
                    "crypto/bio/bf_buff.o",
                    "crypto/bio/bf_lbuf.o",
                    "crypto/bio/bf_nbio.o",
                    "crypto/bio/bf_null.o",
                    "crypto/bio/bio_cb.o",
                    "crypto/bio/bio_err.o",
                    "crypto/bio/bio_lib.o",
                    "crypto/bio/bio_meth.o",
                    "crypto/bio/bss_acpt.o",
                    "crypto/bio/bss_bio.o",
                    "crypto/bio/bss_conn.o",
                    "crypto/bio/bss_dgram.o",
                    "crypto/bio/bss_fd.o",
                    "crypto/bio/bss_file.o",
                    "crypto/bio/bss_log.o",
                    "crypto/bio/bss_mem.o",
                    "crypto/bio/bss_null.o",
                    "crypto/bio/bss_sock.o",
                    "crypto/blake2/blake2b.o",
                    "crypto/blake2/blake2s.o",
                    "crypto/blake2/m_blake2b.o",
                    "crypto/blake2/m_blake2s.o",
                    "crypto/bn/bn-586.o",
                    "crypto/bn/bn_add.o",
                    "crypto/bn/bn_blind.o",
                    "crypto/bn/bn_const.o",
                    "crypto/bn/bn_ctx.o",
                    "crypto/bn/bn_depr.o",
                    "crypto/bn/bn_dh.o",
                    "crypto/bn/bn_div.o",
                    "crypto/bn/bn_err.o",
                    "crypto/bn/bn_exp.o",
                    "crypto/bn/bn_exp2.o",
                    "crypto/bn/bn_gcd.o",
                    "crypto/bn/bn_gf2m.o",
                    "crypto/bn/bn_intern.o",
                    "crypto/bn/bn_kron.o",
                    "crypto/bn/bn_lib.o",
                    "crypto/bn/bn_mod.o",
                    "crypto/bn/bn_mont.o",
                    "crypto/bn/bn_mpi.o",
                    "crypto/bn/bn_mul.o",
                    "crypto/bn/bn_nist.o",
                    "crypto/bn/bn_prime.o",
                    "crypto/bn/bn_print.o",
                    "crypto/bn/bn_rand.o",
                    "crypto/bn/bn_recp.o",
                    "crypto/bn/bn_shift.o",
                    "crypto/bn/bn_sqr.o",
                    "crypto/bn/bn_sqrt.o",
                    "crypto/bn/bn_srp.o",
                    "crypto/bn/bn_word.o",
                    "crypto/bn/bn_x931p.o",
                    "crypto/bn/co-586.o",
                    "crypto/bn/x86-gf2m.o",
                    "crypto/bn/x86-mont.o",
                    "crypto/buffer/buf_err.o",
                    "crypto/buffer/buffer.o",
                    "crypto/camellia/cmll-x86.o",
                    "crypto/camellia/cmll_cfb.o",
                    "crypto/camellia/cmll_ctr.o",
                    "crypto/camellia/cmll_ecb.o",
                    "crypto/camellia/cmll_ofb.o",
                    "crypto/cast/c_cfb64.o",
                    "crypto/cast/c_ecb.o",
                    "crypto/cast/c_enc.o",
                    "crypto/cast/c_ofb64.o",
                    "crypto/cast/c_skey.o",
                    "crypto/chacha/chacha-x86.o",
                    "crypto/cmac/cm_ameth.o",
                    "crypto/cmac/cm_pmeth.o",
                    "crypto/cmac/cmac.o",
                    "crypto/cms/cms_asn1.o",
                    "crypto/cms/cms_att.o",
                    "crypto/cms/cms_cd.o",
                    "crypto/cms/cms_dd.o",
                    "crypto/cms/cms_enc.o",
                    "crypto/cms/cms_env.o",
                    "crypto/cms/cms_err.o",
                    "crypto/cms/cms_ess.o",
                    "crypto/cms/cms_io.o",
                    "crypto/cms/cms_kari.o",
                    "crypto/cms/cms_lib.o",
                    "crypto/cms/cms_pwri.o",
                    "crypto/cms/cms_sd.o",
                    "crypto/cms/cms_smime.o",
                    "crypto/conf/conf_api.o",
                    "crypto/conf/conf_def.o",
                    "crypto/conf/conf_err.o",
                    "crypto/conf/conf_lib.o",
                    "crypto/conf/conf_mall.o",
                    "crypto/conf/conf_mod.o",
                    "crypto/conf/conf_sap.o",
                    "crypto/conf/conf_ssl.o",
                    "crypto/cpt_err.o",
                    "crypto/cryptlib.o",
                    "crypto/ct/ct_b64.o",
                    "crypto/ct/ct_err.o",
                    "crypto/ct/ct_log.o",
                    "crypto/ct/ct_oct.o",
                    "crypto/ct/ct_policy.o",
                    "crypto/ct/ct_prn.o",
                    "crypto/ct/ct_sct.o",
                    "crypto/ct/ct_sct_ctx.o",
                    "crypto/ct/ct_vfy.o",
                    "crypto/ct/ct_x509v3.o",
                    "crypto/ctype.o",
                    "crypto/cversion.o",
                    "crypto/des/cbc_cksm.o",
                    "crypto/des/cbc_enc.o",
                    "crypto/des/cfb64ede.o",
                    "crypto/des/cfb64enc.o",
                    "crypto/des/cfb_enc.o",
                    "crypto/des/crypt586.o",
                    "crypto/des/des-586.o",
                    "crypto/des/ecb3_enc.o",
                    "crypto/des/ecb_enc.o",
                    "crypto/des/fcrypt.o",
                    "crypto/des/ofb64ede.o",
                    "crypto/des/ofb64enc.o",
                    "crypto/des/ofb_enc.o",
                    "crypto/des/pcbc_enc.o",
                    "crypto/des/qud_cksm.o",
                    "crypto/des/rand_key.o",
                    "crypto/des/set_key.o",
                    "crypto/des/str2key.o",
                    "crypto/des/xcbc_enc.o",
                    "crypto/dh/dh_ameth.o",
                    "crypto/dh/dh_asn1.o",
                    "crypto/dh/dh_check.o",
                    "crypto/dh/dh_depr.o",
                    "crypto/dh/dh_err.o",
                    "crypto/dh/dh_gen.o",
                    "crypto/dh/dh_kdf.o",
                    "crypto/dh/dh_key.o",
                    "crypto/dh/dh_lib.o",
                    "crypto/dh/dh_meth.o",
                    "crypto/dh/dh_pmeth.o",
                    "crypto/dh/dh_prn.o",
                    "crypto/dh/dh_rfc5114.o",
                    "crypto/dh/dh_rfc7919.o",
                    "crypto/dsa/dsa_ameth.o",
                    "crypto/dsa/dsa_asn1.o",
                    "crypto/dsa/dsa_depr.o",
                    "crypto/dsa/dsa_err.o",
                    "crypto/dsa/dsa_gen.o",
                    "crypto/dsa/dsa_key.o",
                    "crypto/dsa/dsa_lib.o",
                    "crypto/dsa/dsa_meth.o",
                    "crypto/dsa/dsa_ossl.o",
                    "crypto/dsa/dsa_pmeth.o",
                    "crypto/dsa/dsa_prn.o",
                    "crypto/dsa/dsa_sign.o",
                    "crypto/dsa/dsa_vrf.o",
                    "crypto/dso/dso_dl.o",
                    "crypto/dso/dso_dlfcn.o",
                    "crypto/dso/dso_err.o",
                    "crypto/dso/dso_lib.o",
                    "crypto/dso/dso_openssl.o",
                    "crypto/dso/dso_vms.o",
                    "crypto/dso/dso_win32.o",
                    "crypto/ebcdic.o",
                    "crypto/ec/curve25519.o",
                    "crypto/ec/curve448/arch_32/f_impl.o",
                    "crypto/ec/curve448/curve448.o",
                    "crypto/ec/curve448/curve448_tables.o",
                    "crypto/ec/curve448/eddsa.o",
                    "crypto/ec/curve448/f_generic.o",
                    "crypto/ec/curve448/scalar.o",
                    "crypto/ec/ec2_oct.o",
                    "crypto/ec/ec2_smpl.o",
                    "crypto/ec/ec_ameth.o",
                    "crypto/ec/ec_asn1.o",
                    "crypto/ec/ec_check.o",
                    "crypto/ec/ec_curve.o",
                    "crypto/ec/ec_cvt.o",
                    "crypto/ec/ec_err.o",
                    "crypto/ec/ec_key.o",
                    "crypto/ec/ec_kmeth.o",
                    "crypto/ec/ec_lib.o",
                    "crypto/ec/ec_mult.o",
                    "crypto/ec/ec_oct.o",
                    "crypto/ec/ec_pmeth.o",
                    "crypto/ec/ec_print.o",
                    "crypto/ec/ecdh_kdf.o",
                    "crypto/ec/ecdh_ossl.o",
                    "crypto/ec/ecdsa_ossl.o",
                    "crypto/ec/ecdsa_sign.o",
                    "crypto/ec/ecdsa_vrf.o",
                    "crypto/ec/eck_prn.o",
                    "crypto/ec/ecp_mont.o",
                    "crypto/ec/ecp_nist.o",
                    "crypto/ec/ecp_nistp224.o",
                    "crypto/ec/ecp_nistp256.o",
                    "crypto/ec/ecp_nistp521.o",
                    "crypto/ec/ecp_nistputil.o",
                    "crypto/ec/ecp_nistz256-x86.o",
                    "crypto/ec/ecp_nistz256.o",
                    "crypto/ec/ecp_oct.o",
                    "crypto/ec/ecp_smpl.o",
                    "crypto/ec/ecx_meth.o",
                    "crypto/engine/eng_all.o",
                    "crypto/engine/eng_cnf.o",
                    "crypto/engine/eng_ctrl.o",
                    "crypto/engine/eng_dyn.o",
                    "crypto/engine/eng_err.o",
                    "crypto/engine/eng_fat.o",
                    "crypto/engine/eng_init.o",
                    "crypto/engine/eng_lib.o",
                    "crypto/engine/eng_list.o",
                    "crypto/engine/eng_openssl.o",
                    "crypto/engine/eng_pkey.o",
                    "crypto/engine/eng_rdrand.o",
                    "crypto/engine/eng_table.o",
                    "crypto/engine/tb_asnmth.o",
                    "crypto/engine/tb_cipher.o",
                    "crypto/engine/tb_dh.o",
                    "crypto/engine/tb_digest.o",
                    "crypto/engine/tb_dsa.o",
                    "crypto/engine/tb_eckey.o",
                    "crypto/engine/tb_pkmeth.o",
                    "crypto/engine/tb_rand.o",
                    "crypto/engine/tb_rsa.o",
                    "crypto/err/err.o",
                    "crypto/err/err_all.o",
                    "crypto/err/err_prn.o",
                    "crypto/evp/bio_b64.o",
                    "crypto/evp/bio_enc.o",
                    "crypto/evp/bio_md.o",
                    "crypto/evp/bio_ok.o",
                    "crypto/evp/c_allc.o",
                    "crypto/evp/c_alld.o",
                    "crypto/evp/cmeth_lib.o",
                    "crypto/evp/digest.o",
                    "crypto/evp/e_aes.o",
                    "crypto/evp/e_aes_cbc_hmac_sha1.o",
                    "crypto/evp/e_aes_cbc_hmac_sha256.o",
                    "crypto/evp/e_aria.o",
                    "crypto/evp/e_bf.o",
                    "crypto/evp/e_camellia.o",
                    "crypto/evp/e_cast.o",
                    "crypto/evp/e_chacha20_poly1305.o",
                    "crypto/evp/e_des.o",
                    "crypto/evp/e_des3.o",
                    "crypto/evp/e_idea.o",
                    "crypto/evp/e_null.o",
                    "crypto/evp/e_old.o",
                    "crypto/evp/e_rc2.o",
                    "crypto/evp/e_rc4.o",
                    "crypto/evp/e_rc4_hmac_md5.o",
                    "crypto/evp/e_rc5.o",
                    "crypto/evp/e_seed.o",
                    "crypto/evp/e_sm4.o",
                    "crypto/evp/e_xcbc_d.o",
                    "crypto/evp/encode.o",
                    "crypto/evp/evp_cnf.o",
                    "crypto/evp/evp_enc.o",
                    "crypto/evp/evp_err.o",
                    "crypto/evp/evp_key.o",
                    "crypto/evp/evp_lib.o",
                    "crypto/evp/evp_pbe.o",
                    "crypto/evp/evp_pkey.o",
                    "crypto/evp/m_md2.o",
                    "crypto/evp/m_md4.o",
                    "crypto/evp/m_md5.o",
                    "crypto/evp/m_md5_sha1.o",
                    "crypto/evp/m_mdc2.o",
                    "crypto/evp/m_null.o",
                    "crypto/evp/m_ripemd.o",
                    "crypto/evp/m_sha1.o",
                    "crypto/evp/m_sha3.o",
                    "crypto/evp/m_sigver.o",
                    "crypto/evp/m_wp.o",
                    "crypto/evp/names.o",
                    "crypto/evp/p5_crpt.o",
                    "crypto/evp/p5_crpt2.o",
                    "crypto/evp/p_dec.o",
                    "crypto/evp/p_enc.o",
                    "crypto/evp/p_lib.o",
                    "crypto/evp/p_open.o",
                    "crypto/evp/p_seal.o",
                    "crypto/evp/p_sign.o",
                    "crypto/evp/p_verify.o",
                    "crypto/evp/pbe_scrypt.o",
                    "crypto/evp/pmeth_fn.o",
                    "crypto/evp/pmeth_gn.o",
                    "crypto/evp/pmeth_lib.o",
                    "crypto/ex_data.o",
                    "crypto/getenv.o",
                    "crypto/hmac/hm_ameth.o",
                    "crypto/hmac/hm_pmeth.o",
                    "crypto/hmac/hmac.o",
                    "crypto/idea/i_cbc.o",
                    "crypto/idea/i_cfb64.o",
                    "crypto/idea/i_ecb.o",
                    "crypto/idea/i_ofb64.o",
                    "crypto/idea/i_skey.o",
                    "crypto/init.o",
                    "crypto/kdf/hkdf.o",
                    "crypto/kdf/kdf_err.o",
                    "crypto/kdf/scrypt.o",
                    "crypto/kdf/tls1_prf.o",
                    "crypto/lhash/lh_stats.o",
                    "crypto/lhash/lhash.o",
                    "crypto/md4/md4_dgst.o",
                    "crypto/md4/md4_one.o",
                    "crypto/md5/md5-586.o",
                    "crypto/md5/md5_dgst.o",
                    "crypto/md5/md5_one.o",
                    "crypto/mdc2/mdc2_one.o",
                    "crypto/mdc2/mdc2dgst.o",
                    "crypto/mem.o",
                    "crypto/mem_dbg.o",
                    "crypto/mem_sec.o",
                    "crypto/modes/cbc128.o",
                    "crypto/modes/ccm128.o",
                    "crypto/modes/cfb128.o",
                    "crypto/modes/ctr128.o",
                    "crypto/modes/cts128.o",
                    "crypto/modes/gcm128.o",
                    "crypto/modes/ghash-x86.o",
                    "crypto/modes/ocb128.o",
                    "crypto/modes/ofb128.o",
                    "crypto/modes/wrap128.o",
                    "crypto/modes/xts128.o",
                    "crypto/o_dir.o",
                    "crypto/o_fips.o",
                    "crypto/o_fopen.o",
                    "crypto/o_init.o",
                    "crypto/o_str.o",
                    "crypto/o_time.o",
                    "crypto/objects/o_names.o",
                    "crypto/objects/obj_dat.o",
                    "crypto/objects/obj_err.o",
                    "crypto/objects/obj_lib.o",
                    "crypto/objects/obj_xref.o",
                    "crypto/ocsp/ocsp_asn.o",
                    "crypto/ocsp/ocsp_cl.o",
                    "crypto/ocsp/ocsp_err.o",
                    "crypto/ocsp/ocsp_ext.o",
                    "crypto/ocsp/ocsp_ht.o",
                    "crypto/ocsp/ocsp_lib.o",
                    "crypto/ocsp/ocsp_prn.o",
                    "crypto/ocsp/ocsp_srv.o",
                    "crypto/ocsp/ocsp_vfy.o",
                    "crypto/ocsp/v3_ocsp.o",
                    "crypto/pem/pem_all.o",
                    "crypto/pem/pem_err.o",
                    "crypto/pem/pem_info.o",
                    "crypto/pem/pem_lib.o",
                    "crypto/pem/pem_oth.o",
                    "crypto/pem/pem_pk8.o",
                    "crypto/pem/pem_pkey.o",
                    "crypto/pem/pem_sign.o",
                    "crypto/pem/pem_x509.o",
                    "crypto/pem/pem_xaux.o",
                    "crypto/pem/pvkfmt.o",
                    "crypto/pkcs12/p12_add.o",
                    "crypto/pkcs12/p12_asn.o",
                    "crypto/pkcs12/p12_attr.o",
                    "crypto/pkcs12/p12_crpt.o",
                    "crypto/pkcs12/p12_crt.o",
                    "crypto/pkcs12/p12_decr.o",
                    "crypto/pkcs12/p12_init.o",
                    "crypto/pkcs12/p12_key.o",
                    "crypto/pkcs12/p12_kiss.o",
                    "crypto/pkcs12/p12_mutl.o",
                    "crypto/pkcs12/p12_npas.o",
                    "crypto/pkcs12/p12_p8d.o",
                    "crypto/pkcs12/p12_p8e.o",
                    "crypto/pkcs12/p12_sbag.o",
                    "crypto/pkcs12/p12_utl.o",
                    "crypto/pkcs12/pk12err.o",
                    "crypto/pkcs7/bio_pk7.o",
                    "crypto/pkcs7/pk7_asn1.o",
                    "crypto/pkcs7/pk7_attr.o",
                    "crypto/pkcs7/pk7_doit.o",
                    "crypto/pkcs7/pk7_lib.o",
                    "crypto/pkcs7/pk7_mime.o",
                    "crypto/pkcs7/pk7_smime.o",
                    "crypto/pkcs7/pkcs7err.o",
                    "crypto/poly1305/poly1305-x86.o",
                    "crypto/poly1305/poly1305.o",
                    "crypto/poly1305/poly1305_ameth.o",
                    "crypto/poly1305/poly1305_pmeth.o",
                    "crypto/rand/drbg_ctr.o",
                    "crypto/rand/drbg_lib.o",
                    "crypto/rand/rand_egd.o",
                    "crypto/rand/rand_err.o",
                    "crypto/rand/rand_lib.o",
                    "crypto/rand/rand_unix.o",
                    "crypto/rand/rand_vms.o",
                    "crypto/rand/rand_win.o",
                    "crypto/rand/randfile.o",
                    "crypto/rc2/rc2_cbc.o",
                    "crypto/rc2/rc2_ecb.o",
                    "crypto/rc2/rc2_skey.o",
                    "crypto/rc2/rc2cfb64.o",
                    "crypto/rc2/rc2ofb64.o",
                    "crypto/rc4/rc4-586.o",
                    "crypto/ripemd/rmd-586.o",
                    "crypto/ripemd/rmd_dgst.o",
                    "crypto/ripemd/rmd_one.o",
                    "crypto/rsa/rsa_ameth.o",
                    "crypto/rsa/rsa_asn1.o",
                    "crypto/rsa/rsa_chk.o",
                    "crypto/rsa/rsa_crpt.o",
                    "crypto/rsa/rsa_depr.o",
                    "crypto/rsa/rsa_err.o",
                    "crypto/rsa/rsa_gen.o",
                    "crypto/rsa/rsa_lib.o",
                    "crypto/rsa/rsa_meth.o",
                    "crypto/rsa/rsa_mp.o",
                    "crypto/rsa/rsa_none.o",
                    "crypto/rsa/rsa_oaep.o",
                    "crypto/rsa/rsa_ossl.o",
                    "crypto/rsa/rsa_pk1.o",
                    "crypto/rsa/rsa_pmeth.o",
                    "crypto/rsa/rsa_prn.o",
                    "crypto/rsa/rsa_pss.o",
                    "crypto/rsa/rsa_saos.o",
                    "crypto/rsa/rsa_sign.o",
                    "crypto/rsa/rsa_ssl.o",
                    "crypto/rsa/rsa_x931.o",
                    "crypto/rsa/rsa_x931g.o",
                    "crypto/seed/seed.o",
                    "crypto/seed/seed_cbc.o",
                    "crypto/seed/seed_cfb.o",
                    "crypto/seed/seed_ecb.o",
                    "crypto/seed/seed_ofb.o",
                    "crypto/sha/keccak1600.o",
                    "crypto/sha/sha1-586.o",
                    "crypto/sha/sha1_one.o",
                    "crypto/sha/sha1dgst.o",
                    "crypto/sha/sha256-586.o",
                    "crypto/sha/sha256.o",
                    "crypto/sha/sha512-586.o",
                    "crypto/sha/sha512.o",
                    "crypto/siphash/siphash.o",
                    "crypto/siphash/siphash_ameth.o",
                    "crypto/siphash/siphash_pmeth.o",
                    "crypto/sm2/sm2_crypt.o",
                    "crypto/sm2/sm2_err.o",
                    "crypto/sm2/sm2_pmeth.o",
                    "crypto/sm2/sm2_sign.o",
                    "crypto/sm3/m_sm3.o",
                    "crypto/sm3/sm3.o",
                    "crypto/sm4/sm4.o",
                    "crypto/srp/srp_lib.o",
                    "crypto/srp/srp_vfy.o",
                    "crypto/stack/stack.o",
                    "crypto/store/loader_file.o",
                    "crypto/store/store_err.o",
                    "crypto/store/store_init.o",
                    "crypto/store/store_lib.o",
                    "crypto/store/store_register.o",
                    "crypto/store/store_strings.o",
                    "crypto/threads_none.o",
                    "crypto/threads_pthread.o",
                    "crypto/threads_win.o",
                    "crypto/ts/ts_asn1.o",
                    "crypto/ts/ts_conf.o",
                    "crypto/ts/ts_err.o",
                    "crypto/ts/ts_lib.o",
                    "crypto/ts/ts_req_print.o",
                    "crypto/ts/ts_req_utils.o",
                    "crypto/ts/ts_rsp_print.o",
                    "crypto/ts/ts_rsp_sign.o",
                    "crypto/ts/ts_rsp_utils.o",
                    "crypto/ts/ts_rsp_verify.o",
                    "crypto/ts/ts_verify_ctx.o",
                    "crypto/txt_db/txt_db.o",
                    "crypto/ui/ui_err.o",
                    "crypto/ui/ui_lib.o",
                    "crypto/ui/ui_null.o",
                    "crypto/ui/ui_openssl.o",
                    "crypto/ui/ui_util.o",
                    "crypto/uid.o",
                    "crypto/whrlpool/wp-mmx.o",
                    "crypto/whrlpool/wp_block.o",
                    "crypto/whrlpool/wp_dgst.o",
                    "crypto/x509/by_dir.o",
                    "crypto/x509/by_file.o",
                    "crypto/x509/t_crl.o",
                    "crypto/x509/t_req.o",
                    "crypto/x509/t_x509.o",
                    "crypto/x509/x509_att.o",
                    "crypto/x509/x509_cmp.o",
                    "crypto/x509/x509_d2.o",
                    "crypto/x509/x509_def.o",
                    "crypto/x509/x509_err.o",
                    "crypto/x509/x509_ext.o",
                    "crypto/x509/x509_lu.o",
                    "crypto/x509/x509_meth.o",
                    "crypto/x509/x509_obj.o",
                    "crypto/x509/x509_r2x.o",
                    "crypto/x509/x509_req.o",
                    "crypto/x509/x509_set.o",
                    "crypto/x509/x509_trs.o",
                    "crypto/x509/x509_txt.o",
                    "crypto/x509/x509_v3.o",
                    "crypto/x509/x509_vfy.o",
                    "crypto/x509/x509_vpm.o",
                    "crypto/x509/x509cset.o",
                    "crypto/x509/x509name.o",
                    "crypto/x509/x509rset.o",
                    "crypto/x509/x509spki.o",
                    "crypto/x509/x509type.o",
                    "crypto/x509/x_all.o",
                    "crypto/x509/x_attrib.o",
                    "crypto/x509/x_crl.o",
                    "crypto/x509/x_exten.o",
                    "crypto/x509/x_name.o",
                    "crypto/x509/x_pubkey.o",
                    "crypto/x509/x_req.o",
                    "crypto/x509/x_x509.o",
                    "crypto/x509/x_x509a.o",
                    "crypto/x509v3/pcy_cache.o",
                    "crypto/x509v3/pcy_data.o",
                    "crypto/x509v3/pcy_lib.o",
                    "crypto/x509v3/pcy_map.o",
                    "crypto/x509v3/pcy_node.o",
                    "crypto/x509v3/pcy_tree.o",
                    "crypto/x509v3/v3_addr.o",
                    "crypto/x509v3/v3_admis.o",
                    "crypto/x509v3/v3_akey.o",
                    "crypto/x509v3/v3_akeya.o",
                    "crypto/x509v3/v3_alt.o",
                    "crypto/x509v3/v3_asid.o",
                    "crypto/x509v3/v3_bcons.o",
                    "crypto/x509v3/v3_bitst.o",
                    "crypto/x509v3/v3_conf.o",
                    "crypto/x509v3/v3_cpols.o",
                    "crypto/x509v3/v3_crld.o",
                    "crypto/x509v3/v3_enum.o",
                    "crypto/x509v3/v3_extku.o",
                    "crypto/x509v3/v3_genn.o",
                    "crypto/x509v3/v3_ia5.o",
                    "crypto/x509v3/v3_info.o",
                    "crypto/x509v3/v3_int.o",
                    "crypto/x509v3/v3_lib.o",
                    "crypto/x509v3/v3_ncons.o",
                    "crypto/x509v3/v3_pci.o",
                    "crypto/x509v3/v3_pcia.o",
                    "crypto/x509v3/v3_pcons.o",
                    "crypto/x509v3/v3_pku.o",
                    "crypto/x509v3/v3_pmaps.o",
                    "crypto/x509v3/v3_prn.o",
                    "crypto/x509v3/v3_purp.o",
                    "crypto/x509v3/v3_skey.o",
                    "crypto/x509v3/v3_sxnet.o",
                    "crypto/x509v3/v3_tlsf.o",
                    "crypto/x509v3/v3_utl.o",
                    "crypto/x509v3/v3err.o",
                    "crypto/x86cpuid.o",
                    "engines/e_capi.o",
                    "engines/e_padlock-x86.o",
                    "engines/e_padlock.o",
                ],
            "libssl" =>
                [
                    "ssl/bio_ssl.o",
                    "ssl/d1_lib.o",
                    "ssl/d1_msg.o",
                    "ssl/d1_srtp.o",
                    "ssl/methods.o",
                    "ssl/packet.o",
                    "ssl/pqueue.o",
                    "ssl/record/dtls1_bitmap.o",
                    "ssl/record/rec_layer_d1.o",
                    "ssl/record/rec_layer_s3.o",
                    "ssl/record/ssl3_buffer.o",
                    "ssl/record/ssl3_record.o",
                    "ssl/record/ssl3_record_tls13.o",
                    "ssl/s3_cbc.o",
                    "ssl/s3_enc.o",
                    "ssl/s3_lib.o",
                    "ssl/s3_msg.o",
                    "ssl/ssl_asn1.o",
                    "ssl/ssl_cert.o",
                    "ssl/ssl_ciph.o",
                    "ssl/ssl_conf.o",
                    "ssl/ssl_err.o",
                    "ssl/ssl_init.o",
                    "ssl/ssl_lib.o",
                    "ssl/ssl_mcnf.o",
                    "ssl/ssl_rsa.o",
                    "ssl/ssl_sess.o",
                    "ssl/ssl_stat.o",
                    "ssl/ssl_txt.o",
                    "ssl/ssl_utst.o",
                    "ssl/statem/extensions.o",
                    "ssl/statem/extensions_clnt.o",
                    "ssl/statem/extensions_cust.o",
                    "ssl/statem/extensions_srvr.o",
                    "ssl/statem/statem.o",
                    "ssl/statem/statem_clnt.o",
                    "ssl/statem/statem_dtls.o",
                    "ssl/statem/statem_lib.o",
                    "ssl/statem/statem_srvr.o",
                    "ssl/t1_enc.o",
                    "ssl/t1_lib.o",
                    "ssl/t1_trce.o",
                    "ssl/tls13_enc.o",
                    "ssl/tls_srp.o",
                ],
            "ssl/bio_ssl.o" =>
                [
                    "ssl/bio_ssl.c",
                ],
            "ssl/d1_lib.o" =>
                [
                    "ssl/d1_lib.c",
                ],
            "ssl/d1_msg.o" =>
                [
                    "ssl/d1_msg.c",
                ],
            "ssl/d1_srtp.o" =>
                [
                    "ssl/d1_srtp.c",
                ],
            "ssl/methods.o" =>
                [
                    "ssl/methods.c",
                ],
            "ssl/packet.o" =>
                [
                    "ssl/packet.c",
                ],
            "ssl/pqueue.o" =>
                [
                    "ssl/pqueue.c",
                ],
            "ssl/record/dtls1_bitmap.o" =>
                [
                    "ssl/record/dtls1_bitmap.c",
                ],
            "ssl/record/rec_layer_d1.o" =>
                [
                    "ssl/record/rec_layer_d1.c",
                ],
            "ssl/record/rec_layer_s3.o" =>
                [
                    "ssl/record/rec_layer_s3.c",
                ],
            "ssl/record/ssl3_buffer.o" =>
                [
                    "ssl/record/ssl3_buffer.c",
                ],
            "ssl/record/ssl3_record.o" =>
                [
                    "ssl/record/ssl3_record.c",
                ],
            "ssl/record/ssl3_record_tls13.o" =>
                [
                    "ssl/record/ssl3_record_tls13.c",
                ],
            "ssl/s3_cbc.o" =>
                [
                    "ssl/s3_cbc.c",
                ],
            "ssl/s3_enc.o" =>
                [
                    "ssl/s3_enc.c",
                ],
            "ssl/s3_lib.o" =>
                [
                    "ssl/s3_lib.c",
                ],
            "ssl/s3_msg.o" =>
                [
                    "ssl/s3_msg.c",
                ],
            "ssl/ssl_asn1.o" =>
                [
                    "ssl/ssl_asn1.c",
                ],
            "ssl/ssl_cert.o" =>
                [
                    "ssl/ssl_cert.c",
                ],
            "ssl/ssl_ciph.o" =>
                [
                    "ssl/ssl_ciph.c",
                ],
            "ssl/ssl_conf.o" =>
                [
                    "ssl/ssl_conf.c",
                ],
            "ssl/ssl_err.o" =>
                [
                    "ssl/ssl_err.c",
                ],
            "ssl/ssl_init.o" =>
                [
                    "ssl/ssl_init.c",
                ],
            "ssl/ssl_lib.o" =>
                [
                    "ssl/ssl_lib.c",
                ],
            "ssl/ssl_mcnf.o" =>
                [
                    "ssl/ssl_mcnf.c",
                ],
            "ssl/ssl_rsa.o" =>
                [
                    "ssl/ssl_rsa.c",
                ],
            "ssl/ssl_sess.o" =>
                [
                    "ssl/ssl_sess.c",
                ],
            "ssl/ssl_stat.o" =>
                [
                    "ssl/ssl_stat.c",
                ],
            "ssl/ssl_txt.o" =>
                [
                    "ssl/ssl_txt.c",
                ],
            "ssl/ssl_utst.o" =>
                [
                    "ssl/ssl_utst.c",
                ],
            "ssl/statem/extensions.o" =>
                [
                    "ssl/statem/extensions.c",
                ],
            "ssl/statem/extensions_clnt.o" =>
                [
                    "ssl/statem/extensions_clnt.c",
                ],
            "ssl/statem/extensions_cust.o" =>
                [
                    "ssl/statem/extensions_cust.c",
                ],
            "ssl/statem/extensions_srvr.o" =>
                [
                    "ssl/statem/extensions_srvr.c",
                ],
            "ssl/statem/statem.o" =>
                [
                    "ssl/statem/statem.c",
                ],
            "ssl/statem/statem_clnt.o" =>
                [
                    "ssl/statem/statem_clnt.c",
                ],
            "ssl/statem/statem_dtls.o" =>
                [
                    "ssl/statem/statem_dtls.c",
                ],
            "ssl/statem/statem_lib.o" =>
                [
                    "ssl/statem/statem_lib.c",
                ],
            "ssl/statem/statem_srvr.o" =>
                [
                    "ssl/statem/statem_srvr.c",
                ],
            "ssl/t1_enc.o" =>
                [
                    "ssl/t1_enc.c",
                ],
            "ssl/t1_lib.o" =>
                [
                    "ssl/t1_lib.c",
                ],
            "ssl/t1_trce.o" =>
                [
                    "ssl/t1_trce.c",
                ],
            "ssl/tls13_enc.o" =>
                [
                    "ssl/tls13_enc.c",
                ],
            "ssl/tls_srp.o" =>
                [
                    "ssl/tls_srp.c",
                ],
            "test/aborttest" =>
                [
                    "test/aborttest.o",
                ],
            "test/aborttest.o" =>
                [
                    "test/aborttest.c",
                ],
            "test/afalgtest" =>
                [
                    "test/afalgtest.o",
                ],
            "test/afalgtest.o" =>
                [
                    "test/afalgtest.c",
                ],
            "test/asn1_decode_test" =>
                [
                    "test/asn1_decode_test.o",
                ],
            "test/asn1_decode_test.o" =>
                [
                    "test/asn1_decode_test.c",
                ],
            "test/asn1_encode_test" =>
                [
                    "test/asn1_encode_test.o",
                ],
            "test/asn1_encode_test.o" =>
                [
                    "test/asn1_encode_test.c",
                ],
            "test/asn1_internal_test" =>
                [
                    "test/asn1_internal_test.o",
                ],
            "test/asn1_internal_test.o" =>
                [
                    "test/asn1_internal_test.c",
                ],
            "test/asn1_string_table_test" =>
                [
                    "test/asn1_string_table_test.o",
                ],
            "test/asn1_string_table_test.o" =>
                [
                    "test/asn1_string_table_test.c",
                ],
            "test/asn1_time_test" =>
                [
                    "test/asn1_time_test.o",
                ],
            "test/asn1_time_test.o" =>
                [
                    "test/asn1_time_test.c",
                ],
            "test/asynciotest" =>
                [
                    "test/asynciotest.o",
                    "test/ssltestlib.o",
                ],
            "test/asynciotest.o" =>
                [
                    "test/asynciotest.c",
                ],
            "test/asynctest" =>
                [
                    "test/asynctest.o",
                ],
            "test/asynctest.o" =>
                [
                    "test/asynctest.c",
                ],
            "test/bad_dtls_test" =>
                [
                    "test/bad_dtls_test.o",
                ],
            "test/bad_dtls_test.o" =>
                [
                    "test/bad_dtls_test.c",
                ],
            "test/bftest" =>
                [
                    "test/bftest.o",
                ],
            "test/bftest.o" =>
                [
                    "test/bftest.c",
                ],
            "test/bio_callback_test" =>
                [
                    "test/bio_callback_test.o",
                ],
            "test/bio_callback_test.o" =>
                [
                    "test/bio_callback_test.c",
                ],
            "test/bio_enc_test" =>
                [
                    "test/bio_enc_test.o",
                ],
            "test/bio_enc_test.o" =>
                [
                    "test/bio_enc_test.c",
                ],
            "test/bio_memleak_test" =>
                [
                    "test/bio_memleak_test.o",
                ],
            "test/bio_memleak_test.o" =>
                [
                    "test/bio_memleak_test.c",
                ],
            "test/bioprinttest" =>
                [
                    "test/bioprinttest.o",
                ],
            "test/bioprinttest.o" =>
                [
                    "test/bioprinttest.c",
                ],
            "test/bntest" =>
                [
                    "test/bntest.o",
                ],
            "test/bntest.o" =>
                [
                    "test/bntest.c",
                ],
            "test/buildtest_aes.o" =>
                [
                    "test/buildtest_aes.c",
                ],
            "test/buildtest_asn1.o" =>
                [
                    "test/buildtest_asn1.c",
                ],
            "test/buildtest_asn1t.o" =>
                [
                    "test/buildtest_asn1t.c",
                ],
            "test/buildtest_async.o" =>
                [
                    "test/buildtest_async.c",
                ],
            "test/buildtest_bio.o" =>
                [
                    "test/buildtest_bio.c",
                ],
            "test/buildtest_blowfish.o" =>
                [
                    "test/buildtest_blowfish.c",
                ],
            "test/buildtest_bn.o" =>
                [
                    "test/buildtest_bn.c",
                ],
            "test/buildtest_buffer.o" =>
                [
                    "test/buildtest_buffer.c",
                ],
            "test/buildtest_c_aes" =>
                [
                    "test/buildtest_aes.o",
                ],
            "test/buildtest_c_asn1" =>
                [
                    "test/buildtest_asn1.o",
                ],
            "test/buildtest_c_asn1t" =>
                [
                    "test/buildtest_asn1t.o",
                ],
            "test/buildtest_c_async" =>
                [
                    "test/buildtest_async.o",
                ],
            "test/buildtest_c_bio" =>
                [
                    "test/buildtest_bio.o",
                ],
            "test/buildtest_c_blowfish" =>
                [
                    "test/buildtest_blowfish.o",
                ],
            "test/buildtest_c_bn" =>
                [
                    "test/buildtest_bn.o",
                ],
            "test/buildtest_c_buffer" =>
                [
                    "test/buildtest_buffer.o",
                ],
            "test/buildtest_c_camellia" =>
                [
                    "test/buildtest_camellia.o",
                ],
            "test/buildtest_c_cast" =>
                [
                    "test/buildtest_cast.o",
                ],
            "test/buildtest_c_cmac" =>
                [
                    "test/buildtest_cmac.o",
                ],
            "test/buildtest_c_cms" =>
                [
                    "test/buildtest_cms.o",
                ],
            "test/buildtest_c_conf" =>
                [
                    "test/buildtest_conf.o",
                ],
            "test/buildtest_c_conf_api" =>
                [
                    "test/buildtest_conf_api.o",
                ],
            "test/buildtest_c_crypto" =>
                [
                    "test/buildtest_crypto.o",
                ],
            "test/buildtest_c_ct" =>
                [
                    "test/buildtest_ct.o",
                ],
            "test/buildtest_c_des" =>
                [
                    "test/buildtest_des.o",
                ],
            "test/buildtest_c_dh" =>
                [
                    "test/buildtest_dh.o",
                ],
            "test/buildtest_c_dsa" =>
                [
                    "test/buildtest_dsa.o",
                ],
            "test/buildtest_c_dtls1" =>
                [
                    "test/buildtest_dtls1.o",
                ],
            "test/buildtest_c_e_os2" =>
                [
                    "test/buildtest_e_os2.o",
                ],
            "test/buildtest_c_ebcdic" =>
                [
                    "test/buildtest_ebcdic.o",
                ],
            "test/buildtest_c_ec" =>
                [
                    "test/buildtest_ec.o",
                ],
            "test/buildtest_c_ecdh" =>
                [
                    "test/buildtest_ecdh.o",
                ],
            "test/buildtest_c_ecdsa" =>
                [
                    "test/buildtest_ecdsa.o",
                ],
            "test/buildtest_c_engine" =>
                [
                    "test/buildtest_engine.o",
                ],
            "test/buildtest_c_evp" =>
                [
                    "test/buildtest_evp.o",
                ],
            "test/buildtest_c_hmac" =>
                [
                    "test/buildtest_hmac.o",
                ],
            "test/buildtest_c_idea" =>
                [
                    "test/buildtest_idea.o",
                ],
            "test/buildtest_c_kdf" =>
                [
                    "test/buildtest_kdf.o",
                ],
            "test/buildtest_c_lhash" =>
                [
                    "test/buildtest_lhash.o",
                ],
            "test/buildtest_c_md4" =>
                [
                    "test/buildtest_md4.o",
                ],
            "test/buildtest_c_md5" =>
                [
                    "test/buildtest_md5.o",
                ],
            "test/buildtest_c_mdc2" =>
                [
                    "test/buildtest_mdc2.o",
                ],
            "test/buildtest_c_modes" =>
                [
                    "test/buildtest_modes.o",
                ],
            "test/buildtest_c_obj_mac" =>
                [
                    "test/buildtest_obj_mac.o",
                ],
            "test/buildtest_c_objects" =>
                [
                    "test/buildtest_objects.o",
                ],
            "test/buildtest_c_ocsp" =>
                [
                    "test/buildtest_ocsp.o",
                ],
            "test/buildtest_c_opensslv" =>
                [
                    "test/buildtest_opensslv.o",
                ],
            "test/buildtest_c_ossl_typ" =>
                [
                    "test/buildtest_ossl_typ.o",
                ],
            "test/buildtest_c_pem" =>
                [
                    "test/buildtest_pem.o",
                ],
            "test/buildtest_c_pem2" =>
                [
                    "test/buildtest_pem2.o",
                ],
            "test/buildtest_c_pkcs12" =>
                [
                    "test/buildtest_pkcs12.o",
                ],
            "test/buildtest_c_pkcs7" =>
                [
                    "test/buildtest_pkcs7.o",
                ],
            "test/buildtest_c_rand" =>
                [
                    "test/buildtest_rand.o",
                ],
            "test/buildtest_c_rand_drbg" =>
                [
                    "test/buildtest_rand_drbg.o",
                ],
            "test/buildtest_c_rc2" =>
                [
                    "test/buildtest_rc2.o",
                ],
            "test/buildtest_c_rc4" =>
                [
                    "test/buildtest_rc4.o",
                ],
            "test/buildtest_c_ripemd" =>
                [
                    "test/buildtest_ripemd.o",
                ],
            "test/buildtest_c_rsa" =>
                [
                    "test/buildtest_rsa.o",
                ],
            "test/buildtest_c_safestack" =>
                [
                    "test/buildtest_safestack.o",
                ],
            "test/buildtest_c_seed" =>
                [
                    "test/buildtest_seed.o",
                ],
            "test/buildtest_c_sha" =>
                [
                    "test/buildtest_sha.o",
                ],
            "test/buildtest_c_srp" =>
                [
                    "test/buildtest_srp.o",
                ],
            "test/buildtest_c_srtp" =>
                [
                    "test/buildtest_srtp.o",
                ],
            "test/buildtest_c_ssl" =>
                [
                    "test/buildtest_ssl.o",
                ],
            "test/buildtest_c_ssl2" =>
                [
                    "test/buildtest_ssl2.o",
                ],
            "test/buildtest_c_stack" =>
                [
                    "test/buildtest_stack.o",
                ],
            "test/buildtest_c_store" =>
                [
                    "test/buildtest_store.o",
                ],
            "test/buildtest_c_symhacks" =>
                [
                    "test/buildtest_symhacks.o",
                ],
            "test/buildtest_c_tls1" =>
                [
                    "test/buildtest_tls1.o",
                ],
            "test/buildtest_c_ts" =>
                [
                    "test/buildtest_ts.o",
                ],
            "test/buildtest_c_txt_db" =>
                [
                    "test/buildtest_txt_db.o",
                ],
            "test/buildtest_c_ui" =>
                [
                    "test/buildtest_ui.o",
                ],
            "test/buildtest_c_whrlpool" =>
                [
                    "test/buildtest_whrlpool.o",
                ],
            "test/buildtest_c_x509" =>
                [
                    "test/buildtest_x509.o",
                ],
            "test/buildtest_c_x509_vfy" =>
                [
                    "test/buildtest_x509_vfy.o",
                ],
            "test/buildtest_c_x509v3" =>
                [
                    "test/buildtest_x509v3.o",
                ],
            "test/buildtest_camellia.o" =>
                [
                    "test/buildtest_camellia.c",
                ],
            "test/buildtest_cast.o" =>
                [
                    "test/buildtest_cast.c",
                ],
            "test/buildtest_cmac.o" =>
                [
                    "test/buildtest_cmac.c",
                ],
            "test/buildtest_cms.o" =>
                [
                    "test/buildtest_cms.c",
                ],
            "test/buildtest_conf.o" =>
                [
                    "test/buildtest_conf.c",
                ],
            "test/buildtest_conf_api.o" =>
                [
                    "test/buildtest_conf_api.c",
                ],
            "test/buildtest_crypto.o" =>
                [
                    "test/buildtest_crypto.c",
                ],
            "test/buildtest_ct.o" =>
                [
                    "test/buildtest_ct.c",
                ],
            "test/buildtest_des.o" =>
                [
                    "test/buildtest_des.c",
                ],
            "test/buildtest_dh.o" =>
                [
                    "test/buildtest_dh.c",
                ],
            "test/buildtest_dsa.o" =>
                [
                    "test/buildtest_dsa.c",
                ],
            "test/buildtest_dtls1.o" =>
                [
                    "test/buildtest_dtls1.c",
                ],
            "test/buildtest_e_os2.o" =>
                [
                    "test/buildtest_e_os2.c",
                ],
            "test/buildtest_ebcdic.o" =>
                [
                    "test/buildtest_ebcdic.c",
                ],
            "test/buildtest_ec.o" =>
                [
                    "test/buildtest_ec.c",
                ],
            "test/buildtest_ecdh.o" =>
                [
                    "test/buildtest_ecdh.c",
                ],
            "test/buildtest_ecdsa.o" =>
                [
                    "test/buildtest_ecdsa.c",
                ],
            "test/buildtest_engine.o" =>
                [
                    "test/buildtest_engine.c",
                ],
            "test/buildtest_evp.o" =>
                [
                    "test/buildtest_evp.c",
                ],
            "test/buildtest_hmac.o" =>
                [
                    "test/buildtest_hmac.c",
                ],
            "test/buildtest_idea.o" =>
                [
                    "test/buildtest_idea.c",
                ],
            "test/buildtest_kdf.o" =>
                [
                    "test/buildtest_kdf.c",
                ],
            "test/buildtest_lhash.o" =>
                [
                    "test/buildtest_lhash.c",
                ],
            "test/buildtest_md4.o" =>
                [
                    "test/buildtest_md4.c",
                ],
            "test/buildtest_md5.o" =>
                [
                    "test/buildtest_md5.c",
                ],
            "test/buildtest_mdc2.o" =>
                [
                    "test/buildtest_mdc2.c",
                ],
            "test/buildtest_modes.o" =>
                [
                    "test/buildtest_modes.c",
                ],
            "test/buildtest_obj_mac.o" =>
                [
                    "test/buildtest_obj_mac.c",
                ],
            "test/buildtest_objects.o" =>
                [
                    "test/buildtest_objects.c",
                ],
            "test/buildtest_ocsp.o" =>
                [
                    "test/buildtest_ocsp.c",
                ],
            "test/buildtest_opensslv.o" =>
                [
                    "test/buildtest_opensslv.c",
                ],
            "test/buildtest_ossl_typ.o" =>
                [
                    "test/buildtest_ossl_typ.c",
                ],
            "test/buildtest_pem.o" =>
                [
                    "test/buildtest_pem.c",
                ],
            "test/buildtest_pem2.o" =>
                [
                    "test/buildtest_pem2.c",
                ],
            "test/buildtest_pkcs12.o" =>
                [
                    "test/buildtest_pkcs12.c",
                ],
            "test/buildtest_pkcs7.o" =>
                [
                    "test/buildtest_pkcs7.c",
                ],
            "test/buildtest_rand.o" =>
                [
                    "test/buildtest_rand.c",
                ],
            "test/buildtest_rand_drbg.o" =>
                [
                    "test/buildtest_rand_drbg.c",
                ],
            "test/buildtest_rc2.o" =>
                [
                    "test/buildtest_rc2.c",
                ],
            "test/buildtest_rc4.o" =>
                [
                    "test/buildtest_rc4.c",
                ],
            "test/buildtest_ripemd.o" =>
                [
                    "test/buildtest_ripemd.c",
                ],
            "test/buildtest_rsa.o" =>
                [
                    "test/buildtest_rsa.c",
                ],
            "test/buildtest_safestack.o" =>
                [
                    "test/buildtest_safestack.c",
                ],
            "test/buildtest_seed.o" =>
                [
                    "test/buildtest_seed.c",
                ],
            "test/buildtest_sha.o" =>
                [
                    "test/buildtest_sha.c",
                ],
            "test/buildtest_srp.o" =>
                [
                    "test/buildtest_srp.c",
                ],
            "test/buildtest_srtp.o" =>
                [
                    "test/buildtest_srtp.c",
                ],
            "test/buildtest_ssl.o" =>
                [
                    "test/buildtest_ssl.c",
                ],
            "test/buildtest_ssl2.o" =>
                [
                    "test/buildtest_ssl2.c",
                ],
            "test/buildtest_stack.o" =>
                [
                    "test/buildtest_stack.c",
                ],
            "test/buildtest_store.o" =>
                [
                    "test/buildtest_store.c",
                ],
            "test/buildtest_symhacks.o" =>
                [
                    "test/buildtest_symhacks.c",
                ],
            "test/buildtest_tls1.o" =>
                [
                    "test/buildtest_tls1.c",
                ],
            "test/buildtest_ts.o" =>
                [
                    "test/buildtest_ts.c",
                ],
            "test/buildtest_txt_db.o" =>
                [
                    "test/buildtest_txt_db.c",
                ],
            "test/buildtest_ui.o" =>
                [
                    "test/buildtest_ui.c",
                ],
            "test/buildtest_whrlpool.o" =>
                [
                    "test/buildtest_whrlpool.c",
                ],
            "test/buildtest_x509.o" =>
                [
                    "test/buildtest_x509.c",
                ],
            "test/buildtest_x509_vfy.o" =>
                [
                    "test/buildtest_x509_vfy.c",
                ],
            "test/buildtest_x509v3.o" =>
                [
                    "test/buildtest_x509v3.c",
                ],
            "test/casttest" =>
                [
                    "test/casttest.o",
                ],
            "test/casttest.o" =>
                [
                    "test/casttest.c",
                ],
            "test/chacha_internal_test" =>
                [
                    "test/chacha_internal_test.o",
                ],
            "test/chacha_internal_test.o" =>
                [
                    "test/chacha_internal_test.c",
                ],
            "test/cipher_overhead_test" =>
                [
                    "test/cipher_overhead_test.o",
                ],
            "test/cipher_overhead_test.o" =>
                [
                    "test/cipher_overhead_test.c",
                ],
            "test/cipherbytes_test" =>
                [
                    "test/cipherbytes_test.o",
                ],
            "test/cipherbytes_test.o" =>
                [
                    "test/cipherbytes_test.c",
                ],
            "test/cipherlist_test" =>
                [
                    "test/cipherlist_test.o",
                ],
            "test/cipherlist_test.o" =>
                [
                    "test/cipherlist_test.c",
                ],
            "test/ciphername_test" =>
                [
                    "test/ciphername_test.o",
                ],
            "test/ciphername_test.o" =>
                [
                    "test/ciphername_test.c",
                ],
            "test/clienthellotest" =>
                [
                    "test/clienthellotest.o",
                ],
            "test/clienthellotest.o" =>
                [
                    "test/clienthellotest.c",
                ],
            "test/cmactest" =>
                [
                    "test/cmactest.o",
                ],
            "test/cmactest.o" =>
                [
                    "test/cmactest.c",
                ],
            "test/cmsapitest" =>
                [
                    "test/cmsapitest.o",
                ],
            "test/cmsapitest.o" =>
                [
                    "test/cmsapitest.c",
                ],
            "test/conf_include_test" =>
                [
                    "test/conf_include_test.o",
                ],
            "test/conf_include_test.o" =>
                [
                    "test/conf_include_test.c",
                ],
            "test/constant_time_test" =>
                [
                    "test/constant_time_test.o",
                ],
            "test/constant_time_test.o" =>
                [
                    "test/constant_time_test.c",
                ],
            "test/crltest" =>
                [
                    "test/crltest.o",
                ],
            "test/crltest.o" =>
                [
                    "test/crltest.c",
                ],
            "test/ct_test" =>
                [
                    "test/ct_test.o",
                ],
            "test/ct_test.o" =>
                [
                    "test/ct_test.c",
                ],
            "test/ctype_internal_test" =>
                [
                    "test/ctype_internal_test.o",
                ],
            "test/ctype_internal_test.o" =>
                [
                    "test/ctype_internal_test.c",
                ],
            "test/curve448_internal_test" =>
                [
                    "test/curve448_internal_test.o",
                ],
            "test/curve448_internal_test.o" =>
                [
                    "test/curve448_internal_test.c",
                ],
            "test/d2i_test" =>
                [
                    "test/d2i_test.o",
                ],
            "test/d2i_test.o" =>
                [
                    "test/d2i_test.c",
                ],
            "test/danetest" =>
                [
                    "test/danetest.o",
                ],
            "test/danetest.o" =>
                [
                    "test/danetest.c",
                ],
            "test/destest" =>
                [
                    "test/destest.o",
                ],
            "test/destest.o" =>
                [
                    "test/destest.c",
                ],
            "test/dhtest" =>
                [
                    "test/dhtest.o",
                ],
            "test/dhtest.o" =>
                [
                    "test/dhtest.c",
                ],
            "test/drbg_cavs_data.o" =>
                [
                    "test/drbg_cavs_data.c",
                ],
            "test/drbg_cavs_test" =>
                [
                    "test/drbg_cavs_data.o",
                    "test/drbg_cavs_test.o",
                ],
            "test/drbg_cavs_test.o" =>
                [
                    "test/drbg_cavs_test.c",
                ],
            "test/drbgtest" =>
                [
                    "test/drbgtest.o",
                ],
            "test/drbgtest.o" =>
                [
                    "test/drbgtest.c",
                ],
            "test/dsa_no_digest_size_test" =>
                [
                    "test/dsa_no_digest_size_test.o",
                ],
            "test/dsa_no_digest_size_test.o" =>
                [
                    "test/dsa_no_digest_size_test.c",
                ],
            "test/dsatest" =>
                [
                    "test/dsatest.o",
                ],
            "test/dsatest.o" =>
                [
                    "test/dsatest.c",
                ],
            "test/dtls_mtu_test" =>
                [
                    "test/dtls_mtu_test.o",
                    "test/ssltestlib.o",
                ],
            "test/dtls_mtu_test.o" =>
                [
                    "test/dtls_mtu_test.c",
                ],
            "test/dtlstest" =>
                [
                    "test/dtlstest.o",
                    "test/ssltestlib.o",
                ],
            "test/dtlstest.o" =>
                [
                    "test/dtlstest.c",
                ],
            "test/dtlsv1listentest" =>
                [
                    "test/dtlsv1listentest.o",
                ],
            "test/dtlsv1listentest.o" =>
                [
                    "test/dtlsv1listentest.c",
                ],
            "test/ec_internal_test" =>
                [
                    "test/ec_internal_test.o",
                ],
            "test/ec_internal_test.o" =>
                [
                    "test/ec_internal_test.c",
                ],
            "test/ecdsatest" =>
                [
                    "test/ecdsatest.o",
                ],
            "test/ecdsatest.o" =>
                [
                    "test/ecdsatest.c",
                ],
            "test/ecstresstest" =>
                [
                    "test/ecstresstest.o",
                ],
            "test/ecstresstest.o" =>
                [
                    "test/ecstresstest.c",
                ],
            "test/ectest" =>
                [
                    "test/ectest.o",
                ],
            "test/ectest.o" =>
                [
                    "test/ectest.c",
                ],
            "test/enginetest" =>
                [
                    "test/enginetest.o",
                ],
            "test/enginetest.o" =>
                [
                    "test/enginetest.c",
                ],
            "test/errtest" =>
                [
                    "test/errtest.o",
                ],
            "test/errtest.o" =>
                [
                    "test/errtest.c",
                ],
            "test/evp_extra_test" =>
                [
                    "test/evp_extra_test.o",
                ],
            "test/evp_extra_test.o" =>
                [
                    "test/evp_extra_test.c",
                ],
            "test/evp_test" =>
                [
                    "test/evp_test.o",
                ],
            "test/evp_test.o" =>
                [
                    "test/evp_test.c",
                ],
            "test/exdatatest" =>
                [
                    "test/exdatatest.o",
                ],
            "test/exdatatest.o" =>
                [
                    "test/exdatatest.c",
                ],
            "test/exptest" =>
                [
                    "test/exptest.o",
                ],
            "test/exptest.o" =>
                [
                    "test/exptest.c",
                ],
            "test/fatalerrtest" =>
                [
                    "test/fatalerrtest.o",
                    "test/ssltestlib.o",
                ],
            "test/fatalerrtest.o" =>
                [
                    "test/fatalerrtest.c",
                ],
            "test/gmdifftest" =>
                [
                    "test/gmdifftest.o",
                ],
            "test/gmdifftest.o" =>
                [
                    "test/gmdifftest.c",
                ],
            "test/gosttest" =>
                [
                    "test/gosttest.o",
                    "test/ssltestlib.o",
                ],
            "test/gosttest.o" =>
                [
                    "test/gosttest.c",
                ],
            "test/handshake_helper.o" =>
                [
                    "test/handshake_helper.c",
                ],
            "test/hmactest" =>
                [
                    "test/hmactest.o",
                ],
            "test/hmactest.o" =>
                [
                    "test/hmactest.c",
                ],
            "test/ideatest" =>
                [
                    "test/ideatest.o",
                ],
            "test/ideatest.o" =>
                [
                    "test/ideatest.c",
                ],
            "test/igetest" =>
                [
                    "test/igetest.o",
                ],
            "test/igetest.o" =>
                [
                    "test/igetest.c",
                ],
            "test/lhash_test" =>
                [
                    "test/lhash_test.o",
                ],
            "test/lhash_test.o" =>
                [
                    "test/lhash_test.c",
                ],
            "test/libtestutil.a" =>
                [
                    "test/testutil/basic_output.o",
                    "test/testutil/cb.o",
                    "test/testutil/driver.o",
                    "test/testutil/format_output.o",
                    "test/testutil/main.o",
                    "test/testutil/output_helpers.o",
                    "test/testutil/random.o",
                    "test/testutil/stanza.o",
                    "test/testutil/tap_bio.o",
                    "test/testutil/test_cleanup.o",
                    "test/testutil/tests.o",
                    "test/testutil/testutil_init.o",
                ],
            "test/md2test" =>
                [
                    "test/md2test.o",
                ],
            "test/md2test.o" =>
                [
                    "test/md2test.c",
                ],
            "test/mdc2_internal_test" =>
                [
                    "test/mdc2_internal_test.o",
                ],
            "test/mdc2_internal_test.o" =>
                [
                    "test/mdc2_internal_test.c",
                ],
            "test/mdc2test" =>
                [
                    "test/mdc2test.o",
                ],
            "test/mdc2test.o" =>
                [
                    "test/mdc2test.c",
                ],
            "test/memleaktest" =>
                [
                    "test/memleaktest.o",
                ],
            "test/memleaktest.o" =>
                [
                    "test/memleaktest.c",
                ],
            "test/modes_internal_test" =>
                [
                    "test/modes_internal_test.o",
                ],
            "test/modes_internal_test.o" =>
                [
                    "test/modes_internal_test.c",
                ],
            "test/ocspapitest" =>
                [
                    "test/ocspapitest.o",
                ],
            "test/ocspapitest.o" =>
                [
                    "test/ocspapitest.c",
                ],
            "test/packettest" =>
                [
                    "test/packettest.o",
                ],
            "test/packettest.o" =>
                [
                    "test/packettest.c",
                ],
            "test/pbelutest" =>
                [
                    "test/pbelutest.o",
                ],
            "test/pbelutest.o" =>
                [
                    "test/pbelutest.c",
                ],
            "test/pemtest" =>
                [
                    "test/pemtest.o",
                ],
            "test/pemtest.o" =>
                [
                    "test/pemtest.c",
                ],
            "test/pkey_meth_kdf_test" =>
                [
                    "test/pkey_meth_kdf_test.o",
                ],
            "test/pkey_meth_kdf_test.o" =>
                [
                    "test/pkey_meth_kdf_test.c",
                ],
            "test/pkey_meth_test" =>
                [
                    "test/pkey_meth_test.o",
                ],
            "test/pkey_meth_test.o" =>
                [
                    "test/pkey_meth_test.c",
                ],
            "test/poly1305_internal_test" =>
                [
                    "test/poly1305_internal_test.o",
                ],
            "test/poly1305_internal_test.o" =>
                [
                    "test/poly1305_internal_test.c",
                ],
            "test/rc2test" =>
                [
                    "test/rc2test.o",
                ],
            "test/rc2test.o" =>
                [
                    "test/rc2test.c",
                ],
            "test/rc4test" =>
                [
                    "test/rc4test.o",
                ],
            "test/rc4test.o" =>
                [
                    "test/rc4test.c",
                ],
            "test/rc5test" =>
                [
                    "test/rc5test.o",
                ],
            "test/rc5test.o" =>
                [
                    "test/rc5test.c",
                ],
            "test/rdrand_sanitytest" =>
                [
                    "test/rdrand_sanitytest.o",
                ],
            "test/rdrand_sanitytest.o" =>
                [
                    "test/rdrand_sanitytest.c",
                ],
            "test/recordlentest" =>
                [
                    "test/recordlentest.o",
                    "test/ssltestlib.o",
                ],
            "test/recordlentest.o" =>
                [
                    "test/recordlentest.c",
                ],
            "test/rsa_complex" =>
                [
                    "test/rsa_complex.o",
                ],
            "test/rsa_complex.o" =>
                [
                    "test/rsa_complex.c",
                ],
            "test/rsa_mp_test" =>
                [
                    "test/rsa_mp_test.o",
                ],
            "test/rsa_mp_test.o" =>
                [
                    "test/rsa_mp_test.c",
                ],
            "test/rsa_test" =>
                [
                    "test/rsa_test.o",
                ],
            "test/rsa_test.o" =>
                [
                    "test/rsa_test.c",
                ],
            "test/sanitytest" =>
                [
                    "test/sanitytest.o",
                ],
            "test/sanitytest.o" =>
                [
                    "test/sanitytest.c",
                ],
            "test/secmemtest" =>
                [
                    "test/secmemtest.o",
                ],
            "test/secmemtest.o" =>
                [
                    "test/secmemtest.c",
                ],
            "test/servername_test" =>
                [
                    "test/servername_test.o",
                    "test/ssltestlib.o",
                ],
            "test/servername_test.o" =>
                [
                    "test/servername_test.c",
                ],
            "test/siphash_internal_test" =>
                [
                    "test/siphash_internal_test.o",
                ],
            "test/siphash_internal_test.o" =>
                [
                    "test/siphash_internal_test.c",
                ],
            "test/sm2_internal_test" =>
                [
                    "test/sm2_internal_test.o",
                ],
            "test/sm2_internal_test.o" =>
                [
                    "test/sm2_internal_test.c",
                ],
            "test/sm4_internal_test" =>
                [
                    "test/sm4_internal_test.o",
                ],
            "test/sm4_internal_test.o" =>
                [
                    "test/sm4_internal_test.c",
                ],
            "test/srptest" =>
                [
                    "test/srptest.o",
                ],
            "test/srptest.o" =>
                [
                    "test/srptest.c",
                ],
            "test/ssl_cert_table_internal_test" =>
                [
                    "test/ssl_cert_table_internal_test.o",
                ],
            "test/ssl_cert_table_internal_test.o" =>
                [
                    "test/ssl_cert_table_internal_test.c",
                ],
            "test/ssl_ctx_test" =>
                [
                    "test/ssl_ctx_test.o",
                ],
            "test/ssl_ctx_test.o" =>
                [
                    "test/ssl_ctx_test.c",
                ],
            "test/ssl_test" =>
                [
                    "test/handshake_helper.o",
                    "test/ssl_test.o",
                    "test/ssl_test_ctx.o",
                ],
            "test/ssl_test.o" =>
                [
                    "test/ssl_test.c",
                ],
            "test/ssl_test_ctx.o" =>
                [
                    "test/ssl_test_ctx.c",
                ],
            "test/ssl_test_ctx_test" =>
                [
                    "test/ssl_test_ctx.o",
                    "test/ssl_test_ctx_test.o",
                ],
            "test/ssl_test_ctx_test.o" =>
                [
                    "test/ssl_test_ctx_test.c",
                ],
            "test/sslapitest" =>
                [
                    "test/sslapitest.o",
                    "test/ssltestlib.o",
                ],
            "test/sslapitest.o" =>
                [
                    "test/sslapitest.c",
                ],
            "test/sslbuffertest" =>
                [
                    "test/sslbuffertest.o",
                    "test/ssltestlib.o",
                ],
            "test/sslbuffertest.o" =>
                [
                    "test/sslbuffertest.c",
                ],
            "test/sslcorrupttest" =>
                [
                    "test/sslcorrupttest.o",
                    "test/ssltestlib.o",
                ],
            "test/sslcorrupttest.o" =>
                [
                    "test/sslcorrupttest.c",
                ],
            "test/ssltest_old" =>
                [
                    "test/ssltest_old.o",
                ],
            "test/ssltest_old.o" =>
                [
                    "test/ssltest_old.c",
                ],
            "test/ssltestlib.o" =>
                [
                    "test/ssltestlib.c",
                ],
            "test/stack_test" =>
                [
                    "test/stack_test.o",
                ],
            "test/stack_test.o" =>
                [
                    "test/stack_test.c",
                ],
            "test/sysdefaulttest" =>
                [
                    "test/sysdefaulttest.o",
                ],
            "test/sysdefaulttest.o" =>
                [
                    "test/sysdefaulttest.c",
                ],
            "test/test_test" =>
                [
                    "test/test_test.o",
                ],
            "test/test_test.o" =>
                [
                    "test/test_test.c",
                ],
            "test/testutil/basic_output.o" =>
                [
                    "test/testutil/basic_output.c",
                ],
            "test/testutil/cb.o" =>
                [
                    "test/testutil/cb.c",
                ],
            "test/testutil/driver.o" =>
                [
                    "test/testutil/driver.c",
                ],
            "test/testutil/format_output.o" =>
                [
                    "test/testutil/format_output.c",
                ],
            "test/testutil/main.o" =>
                [
                    "test/testutil/main.c",
                ],
            "test/testutil/output_helpers.o" =>
                [
                    "test/testutil/output_helpers.c",
                ],
            "test/testutil/random.o" =>
                [
                    "test/testutil/random.c",
                ],
            "test/testutil/stanza.o" =>
                [
                    "test/testutil/stanza.c",
                ],
            "test/testutil/tap_bio.o" =>
                [
                    "test/testutil/tap_bio.c",
                ],
            "test/testutil/test_cleanup.o" =>
                [
                    "test/testutil/test_cleanup.c",
                ],
            "test/testutil/tests.o" =>
                [
                    "test/testutil/tests.c",
                ],
            "test/testutil/testutil_init.o" =>
                [
                    "test/testutil/testutil_init.c",
                ],
            "test/threadstest" =>
                [
                    "test/threadstest.o",
                ],
            "test/threadstest.o" =>
                [
                    "test/threadstest.c",
                ],
            "test/time_offset_test" =>
                [
                    "test/time_offset_test.o",
                ],
            "test/time_offset_test.o" =>
                [
                    "test/time_offset_test.c",
                ],
            "test/tls13ccstest" =>
                [
                    "test/ssltestlib.o",
                    "test/tls13ccstest.o",
                ],
            "test/tls13ccstest.o" =>
                [
                    "test/tls13ccstest.c",
                ],
            "test/tls13encryptiontest" =>
                [
                    "test/tls13encryptiontest.o",
                ],
            "test/tls13encryptiontest.o" =>
                [
                    "test/tls13encryptiontest.c",
                ],
            "test/uitest" =>
                [
                    "test/uitest.o",
                ],
            "test/uitest.o" =>
                [
                    "test/uitest.c",
                ],
            "test/v3ext" =>
                [
                    "test/v3ext.o",
                ],
            "test/v3ext.o" =>
                [
                    "test/v3ext.c",
                ],
            "test/v3nametest" =>
                [
                    "test/v3nametest.o",
                ],
            "test/v3nametest.o" =>
                [
                    "test/v3nametest.c",
                ],
            "test/verify_extra_test" =>
                [
                    "test/verify_extra_test.o",
                ],
            "test/verify_extra_test.o" =>
                [
                    "test/verify_extra_test.c",
                ],
            "test/versions" =>
                [
                    "test/versions.o",
                ],
            "test/versions.o" =>
                [
                    "test/versions.c",
                ],
            "test/wpackettest" =>
                [
                    "test/wpackettest.o",
                ],
            "test/wpackettest.o" =>
                [
                    "test/wpackettest.c",
                ],
            "test/x509_check_cert_pkey_test" =>
                [
                    "test/x509_check_cert_pkey_test.o",
                ],
            "test/x509_check_cert_pkey_test.o" =>
                [
                    "test/x509_check_cert_pkey_test.c",
                ],
            "test/x509_dup_cert_test" =>
                [
                    "test/x509_dup_cert_test.o",
                ],
            "test/x509_dup_cert_test.o" =>
                [
                    "test/x509_dup_cert_test.c",
                ],
            "test/x509_internal_test" =>
                [
                    "test/x509_internal_test.o",
                ],
            "test/x509_internal_test.o" =>
                [
                    "test/x509_internal_test.c",
                ],
            "test/x509_time_test" =>
                [
                    "test/x509_time_test.o",
                ],
            "test/x509_time_test.o" =>
                [
                    "test/x509_time_test.c",
                ],
            "test/x509aux" =>
                [
                    "test/x509aux.o",
                ],
            "test/x509aux.o" =>
                [
                    "test/x509aux.c",
                ],
            "tools/c_rehash" =>
                [
                    "tools/c_rehash.in",
                ],
            "util/shlib_wrap.sh" =>
                [
                    "util/shlib_wrap.sh.in",
                ],
        },
);

# The following data is only used when this files is use as a script
my @makevars = (
    'AR',
    'ARFLAGS',
    'AS',
    'ASFLAGS',
    'CC',
    'CFLAGS',
    'CPP',
    'CPPDEFINES',
    'CPPFLAGS',
    'CPPINCLUDES',
    'CROSS_COMPILE',
    'CXX',
    'CXXFLAGS',
    'HASHBANGPERL',
    'LD',
    'LDFLAGS',
    'LDLIBS',
    'MT',
    'MTFLAGS',
    'PERL',
    'RANLIB',
    'RC',
    'RCFLAGS',
    'RM',
);
my %disabled_info = (
    'afalgeng' => {
        macro => 'OPENSSL_NO_AFALGENG',
    },
    'asan' => {
        macro => 'OPENSSL_NO_ASAN',
    },
    'comp' => {
        macro => 'OPENSSL_NO_COMP',
        skipped => [ 'crypto/comp' ],
    },
    'crypto-mdebug' => {
        macro => 'OPENSSL_NO_CRYPTO_MDEBUG',
    },
    'crypto-mdebug-backtrace' => {
        macro => 'OPENSSL_NO_CRYPTO_MDEBUG_BACKTRACE',
    },
    'devcryptoeng' => {
        macro => 'OPENSSL_NO_DEVCRYPTOENG',
    },
    'ec_nistp_64_gcc_128' => {
        macro => 'OPENSSL_NO_EC_NISTP_64_GCC_128',
    },
    'egd' => {
        macro => 'OPENSSL_NO_EGD',
    },
    'external-tests' => {
        macro => 'OPENSSL_NO_EXTERNAL_TESTS',
    },
    'fuzz-afl' => {
        macro => 'OPENSSL_NO_FUZZ_AFL',
    },
    'fuzz-libfuzzer' => {
        macro => 'OPENSSL_NO_FUZZ_LIBFUZZER',
    },
    'heartbeats' => {
        macro => 'OPENSSL_NO_HEARTBEATS',
    },
    'md2' => {
        macro => 'OPENSSL_NO_MD2',
        skipped => [ 'crypto/md2' ],
    },
    'msan' => {
        macro => 'OPENSSL_NO_MSAN',
    },
    'rc5' => {
        macro => 'OPENSSL_NO_RC5',
        skipped => [ 'crypto/rc5' ],
    },
    'sctp' => {
        macro => 'OPENSSL_NO_SCTP',
    },
    'ssl3' => {
        macro => 'OPENSSL_NO_SSL3',
    },
    'ssl3-method' => {
        macro => 'OPENSSL_NO_SSL3_METHOD',
    },
    'ubsan' => {
        macro => 'OPENSSL_NO_UBSAN',
    },
    'unit-test' => {
        macro => 'OPENSSL_NO_UNIT_TEST',
    },
    'weak-ssl-ciphers' => {
        macro => 'OPENSSL_NO_WEAK_SSL_CIPHERS',
    },
);
my @user_crossable = qw( AR AS CC CXX CPP LD MT RANLIB RC );
# If run directly, we can give some answers, and even reconfigure
unless (caller) {
    use Getopt::Long;
    use File::Spec::Functions;
    use File::Basename;
    use Pod::Usage;

    my $here = dirname($0);

    my $dump = undef;
    my $cmdline = undef;
    my $options = undef;
    my $target = undef;
    my $envvars = undef;
    my $makevars = undef;
    my $buildparams = undef;
    my $reconf = undef;
    my $verbose = undef;
    my $help = undef;
    my $man = undef;
    GetOptions('dump|d'                 => \$dump,
               'command-line|c'         => \$cmdline,
               'options|o'              => \$options,
               'target|t'               => \$target,
               'environment|e'          => \$envvars,
               'make-variables|m'       => \$makevars,
               'build-parameters|b'     => \$buildparams,
               'reconfigure|reconf|r'   => \$reconf,
               'verbose|v'              => \$verbose,
               'help'                   => \$help,
               'man'                    => \$man)
        or die "Errors in command line arguments\n";

    unless ($dump || $cmdline || $options || $target || $envvars || $makevars
            || $buildparams || $reconf || $verbose || $help || $man) {
        print STDERR <<"_____";
You must give at least one option.
For more information, do '$0 --help'
_____
        exit(2);
    }

    if ($help) {
        pod2usage(-exitval => 0,
                  -verbose => 1);
    }
    if ($man) {
        pod2usage(-exitval => 0,
                  -verbose => 2);
    }
    if ($dump || $cmdline) {
        print "\nCommand line (with current working directory = $here):\n\n";
        print '    ',join(' ',
                          $config{PERL},
                          catfile($config{sourcedir}, 'Configure'),
                          @{$config{perlargv}}), "\n";
        print "\nPerl information:\n\n";
        print '    ',$config{perl_cmd},"\n";
        print '    ',$config{perl_version},' for ',$config{perl_archname},"\n";
    }
    if ($dump || $options) {
        my $longest = 0;
        my $longest2 = 0;
        foreach my $what (@disablables) {
            $longest = length($what) if $longest < length($what);
            $longest2 = length($disabled{$what})
                if $disabled{$what} && $longest2 < length($disabled{$what});
        }
        print "\nEnabled features:\n\n";
        foreach my $what (@disablables) {
            print "    $what\n"
                unless grep { $_ =~ /^${what}$/ } keys %disabled;
        }
        print "\nDisabled features:\n\n";
        foreach my $what (@disablables) {
            my @what2 = grep { $_ =~ /^${what}$/ } keys %disabled;
            my $what3 = $what2[0];
            if ($what3) {
                print "    $what3", ' ' x ($longest - length($what3) + 1),
                    "[$disabled{$what3}]", ' ' x ($longest2 - length($disabled{$what3}) + 1);
                print $disabled_info{$what3}->{macro}
                    if $disabled_info{$what3}->{macro};
                print ' (skip ',
                    join(', ', @{$disabled_info{$what3}->{skipped}}),
                    ')'
                    if $disabled_info{$what3}->{skipped};
                print "\n";
            }
        }
    }
    if ($dump || $target) {
        print "\nConfig target attributes:\n\n";
        foreach (sort keys %target) {
            next if $_ =~ m|^_| || $_ eq 'template';
            my $quotify = sub {
                map { (my $x = $_) =~ s|([\\\$\@"])|\\$1|g; "\"$x\""} @_;
            };
            print '    ', $_, ' => ';
            if (ref($target{$_}) eq "ARRAY") {
                print '[ ', join(', ', $quotify->(@{$target{$_}})), " ],\n";
            } else {
                print $quotify->($target{$_}), ",\n"
            }
        }
    }
    if ($dump || $envvars) {
        print "\nRecorded environment:\n\n";
        foreach (sort keys %{$config{perlenv}}) {
            print '    ',$_,' = ',($config{perlenv}->{$_} || ''),"\n";
        }
    }
    if ($dump || $makevars) {
        print "\nMakevars:\n\n";
        foreach my $var (@makevars) {
            my $prefix = '';
            $prefix = $config{CROSS_COMPILE}
                if grep { $var eq $_ } @user_crossable;
            $prefix //= '';
            print '    ',$var,' ' x (16 - length $var),'= ',
                (ref $config{$var} eq 'ARRAY'
                 ? join(' ', @{$config{$var}})
                 : $prefix.$config{$var}),
                "\n"
                if defined $config{$var};
        }

        my @buildfile = ($config{builddir}, $config{build_file});
        unshift @buildfile, $here
            unless file_name_is_absolute($config{builddir});
        my $buildfile = canonpath(catdir(@buildfile));
        print <<"_____";

NOTE: These variables only represent the configuration view.  The build file
template may have processed these variables further, please have a look at the
build file for more exact data:
    $buildfile
_____
    }
    if ($dump || $buildparams) {
        my @buildfile = ($config{builddir}, $config{build_file});
        unshift @buildfile, $here
            unless file_name_is_absolute($config{builddir});
        print "\nbuild file:\n\n";
        print "    ", canonpath(catfile(@buildfile)),"\n";

        print "\nbuild file templates:\n\n";
        foreach (@{$config{build_file_templates}}) {
            my @tmpl = ($_);
            unshift @tmpl, $here
                unless file_name_is_absolute($config{sourcedir});
            print '    ',canonpath(catfile(@tmpl)),"\n";
        }
    }
    if ($reconf) {
        if ($verbose) {
            print 'Reconfiguring with: ', join(' ',@{$config{perlargv}}), "\n";
            foreach (sort keys %{$config{perlenv}}) {
                print '    ',$_,' = ',($config{perlenv}->{$_} || ""),"\n";
            }
        }

        chdir $here;
        exec $^X,catfile($config{sourcedir}, 'Configure'),'reconf';
    }
}

1;

__END__

=head1 NAME

configdata.pm - configuration data for OpenSSL builds

=head1 SYNOPSIS

Interactive:

  perl configdata.pm [options]

As data bank module:

  use configdata;

=head1 DESCRIPTION

This module can be used in two modes, interactively and as a module containing
all the data recorded by OpenSSL's Configure script.

When used interactively, simply run it as any perl script, with at least one
option, and you will get the information you ask for.  See L</OPTIONS> below.

When loaded as a module, you get a few databanks with useful information to
perform build related tasks.  The databanks are:

    %config             Configured things.
    %target             The OpenSSL config target with all inheritances
                        resolved.
    %disabled           The features that are disabled.
    @disablables        The list of features that can be disabled.
    %withargs           All data given through --with-THING options.
    %unified_info       All information that was computed from the build.info
                        files.

=head1 OPTIONS

=over 4

=item B<--help>

Print a brief help message and exit.

=item B<--man>

Print the manual page and exit.

=item B<--dump> | B<-d>

Print all relevant configuration data.  This is equivalent to B<--command-line>
B<--options> B<--target> B<--environment> B<--make-variables>
B<--build-parameters>.

=item B<--command-line> | B<-c>

Print the current configuration command line.

=item B<--options> | B<-o>

Print the features, both enabled and disabled, and display defined macro and
skipped directories where applicable.

=item B<--target> | B<-t>

Print the config attributes for this config target.

=item B<--environment> | B<-e>

Print the environment variables and their values at the time of configuration.

=item B<--make-variables> | B<-m>

Print the main make variables generated in the current configuration

=item B<--build-parameters> | B<-b>

Print the build parameters, i.e. build file and build file templates.

=item B<--reconfigure> | B<--reconf> | B<-r>

Redo the configuration.

=item B<--verbose> | B<-v>

Verbose output.

=back

=cut

