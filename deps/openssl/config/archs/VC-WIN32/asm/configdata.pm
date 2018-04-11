package configdata;

use strict;
use warnings;

use Exporter;
#use vars qw(@ISA @EXPORT);
our @ISA = qw(Exporter);
our @EXPORT = qw(%config %target %disabled %withargs %unified_info @disablables);

our %config = (
  b32 => "1",
  b64 => "0",
  b64l => "0",
  baseaddr => "0xFB00000",
  bn_ll => "1",
  build_file => "makefile",
  build_file_templates => [ "Configurations/windows-makefile.tmpl", "Configurations/common.tmpl" ],
  build_infos => [ "./build.info", "crypto/build.info", "ssl/build.info", "engines/build.info", "apps/build.info", "test/build.info", "util/build.info", "tools/build.info", "fuzz/build.info", "crypto/objects/build.info", "crypto/md4/build.info", "crypto/md5/build.info", "crypto/sha/build.info", "crypto/mdc2/build.info", "crypto/hmac/build.info", "crypto/ripemd/build.info", "crypto/whrlpool/build.info", "crypto/poly1305/build.info", "crypto/blake2/build.info", "crypto/des/build.info", "crypto/aes/build.info", "crypto/rc2/build.info", "crypto/rc4/build.info", "crypto/idea/build.info", "crypto/bf/build.info", "crypto/cast/build.info", "crypto/camellia/build.info", "crypto/seed/build.info", "crypto/chacha/build.info", "crypto/modes/build.info", "crypto/bn/build.info", "crypto/ec/build.info", "crypto/rsa/build.info", "crypto/dsa/build.info", "crypto/dh/build.info", "crypto/dso/build.info", "crypto/engine/build.info", "crypto/buffer/build.info", "crypto/bio/build.info", "crypto/stack/build.info", "crypto/lhash/build.info", "crypto/rand/build.info", "crypto/err/build.info", "crypto/evp/build.info", "crypto/asn1/build.info", "crypto/pem/build.info", "crypto/x509/build.info", "crypto/x509v3/build.info", "crypto/conf/build.info", "crypto/txt_db/build.info", "crypto/pkcs7/build.info", "crypto/pkcs12/build.info", "crypto/ocsp/build.info", "crypto/ui/build.info", "crypto/cms/build.info", "crypto/ts/build.info", "crypto/srp/build.info", "crypto/cmac/build.info", "crypto/ct/build.info", "crypto/async/build.info", "crypto/kdf/build.info" ],
  build_type => "release",
  builddir => ".",
  cc => "cl",
  cflags => "",
  conf_files => [ "Configurations/00-base-templates.conf", "Configurations/10-main.conf" ],
  cross_compile_prefix => "",
  defines => [ "DSO_WIN32", "NDEBUG", "OPENSSL_THREADS", "OPENSSL_NO_DYNAMIC_ENGINE", "OPENSSL_PIC", "OPENSSL_BN_ASM_PART_WORDS", "OPENSSL_IA32_SSE2", "OPENSSL_BN_ASM_MONT", "OPENSSL_BN_ASM_GF2m", "SHA1_ASM", "SHA256_ASM", "SHA512_ASM", "RC4_ASM", "MD5_ASM", "RMD160_ASM", "AES_ASM", "VPAES_ASM", "WHIRLPOOL_ASM", "GHASH_ASM", "ECP_NISTZ256_ASM", "PADLOCK_ASM", "POLY1305_ASM" ],
  dirs => [ "crypto", "ssl", "engines", "apps", "test", "util", "tools", "fuzz" ],
  dynamic_engines => "0",
  engdirs => [  ],
  ex_libs => "",
  export_var_as_fn => "1",
  fips => "0",
  fipslibdir => "/usr/local/ssl/fips-2.0/lib/",
  hashbangperl => "/usr/bin/env perl",
  libdir => "",
  major => "1",
  makedepprog => "/usr/bin/makedepend",
  minor => "1.0",
  openssl_algorithm_defines => [ "OPENSSL_NO_COMP", "OPENSSL_NO_MD2", "OPENSSL_NO_RC5" ],
  openssl_api_defines => [  ],
  openssl_other_defines => [ "OPENSSL_NO_AFALGENG", "OPENSSL_NO_ASAN", "OPENSSL_NO_CRYPTO_MDEBUG", "OPENSSL_NO_CRYPTO_MDEBUG_BACKTRACE", "OPENSSL_NO_EC_NISTP_64_GCC_128", "OPENSSL_NO_EGD", "OPENSSL_NO_FUZZ_AFL", "OPENSSL_NO_FUZZ_LIBFUZZER", "OPENSSL_NO_HEARTBEATS", "OPENSSL_NO_MSAN", "OPENSSL_NO_SCTP", "OPENSSL_NO_SSL_TRACE", "OPENSSL_NO_SSL3", "OPENSSL_NO_SSL3_METHOD", "OPENSSL_NO_UBSAN", "OPENSSL_NO_UNIT_TEST", "OPENSSL_NO_WEAK_SSL_CIPHERS", "OPENSSL_NO_AFALGENG" ],
  openssl_sys_defines => [ "OPENSSL_SYS_WIN32" ],
  openssl_thread_defines => [ "OPENSSL_THREADS" ],
  openssldir => "",
  options => " no-afalgeng no-asan no-comp no-crypto-mdebug no-crypto-mdebug-backtrace no-dynamic-engine no-ec_nistp_64_gcc_128 no-egd no-fuzz-afl no-fuzz-libfuzzer no-heartbeats no-md2 no-msan no-rc5 no-sctp no-shared no-ssl-trace no-ssl3 no-ssl3-method no-ubsan no-unit-test no-weak-ssl-ciphers no-zlib no-zlib-dynamic",
  perl => "/usr/bin/perl",
  perlargv => [ "no-comp", "no-shared", "no-afalgeng", "VC-WIN32" ],
  prefix => "",
  processor => "",
  rc4_int => "unsigned int",
  sdirs => [ "objects", "md4", "md5", "sha", "mdc2", "hmac", "ripemd", "whrlpool", "poly1305", "blake2", "des", "aes", "rc2", "rc4", "idea", "bf", "cast", "camellia", "seed", "chacha", "modes", "bn", "ec", "rsa", "dsa", "dh", "dso", "engine", "buffer", "bio", "stack", "lhash", "rand", "err", "evp", "asn1", "pem", "x509", "x509v3", "conf", "txt_db", "pkcs7", "pkcs12", "ocsp", "ui", "cms", "ts", "srp", "cmac", "ct", "async", "kdf" ],
  shared_ldflag => "",
  shlib_major => "1",
  shlib_minor => "1",
  shlib_version_history => "",
  shlib_version_number => "1.1",
  sourcedir => ".",
  target => "VC-WIN32",
  version => "1.1.0h",
  version_num => "0x1010008fL",
);

our %target = (
  _conf_fname_int => [ "Configurations/00-base-templates.conf", "Configurations/00-base-templates.conf", "Configurations/10-main.conf", "Configurations/10-main.conf", "Configurations/00-base-templates.conf", "Configurations/10-main.conf" ],
  aes_asm_src => "aes-586.s vpaes-x86.s aesni-x86.s",
  aes_obj => "aes-586.o vpaes-x86.o aesni-x86.o",
  apps_aux_src => "win32_init.c",
  apps_obj => "win32_init.o",
  ar => "lib",
  arflags => "/nologo",
  aroutflag => "/out:",
  as => "nasm",
  asflags => "-f win32",
  asoutflag => "-o",
  bf_asm_src => "bf-586.s",
  bf_obj => "bf-586.o",
  bin_cflags => "/Zi /Fdapp /MT",
  bin_lflags => "/subsystem:console /opt:ref",
  bn_asm_src => "bn-586.s co-586.s x86-mont.s x86-gf2m.s",
  bn_obj => "bn-586.o co-586.o x86-mont.o x86-gf2m.o",
  bn_ops => "BN_LLONG EXPORT_VAR_AS_FN",
  build_file => "makefile",
  build_scheme => [ "unified", "windows", "VC-W32" ],
  cast_asm_src => "c_enc.c",
  cast_obj => "c_enc.o",
  cc => "cl",
  cflags => "-W3 -wd4090 -Gs0 -GF -Gy -nologo -DOPENSSL_SYS_WIN32 -DWIN32_LEAN_AND_MEAN -DL_ENDIAN -D_CRT_SECURE_NO_DEPRECATE -DUNICODE -D_UNICODE  /O2",
  chacha_asm_src => "chacha-x86.s",
  chacha_obj => "chacha-x86.o",
  cmll_asm_src => "cmll-x86.s",
  cmll_obj => "cmll-x86.o",
  coutflag => "/Fo",
  cpuid_asm_src => "x86cpuid.s",
  cpuid_obj => "x86cpuid.o",
  defines => [  ],
  des_asm_src => "des-586.s crypt586.s",
  des_obj => "des-586.o crypt586.o",
  dso_cflags => "/Zi /Fddso",
  dso_extension => "",
  dso_scheme => "WIN32",
  ec_asm_src => "ecp_nistz256.c ecp_nistz256-x86.s",
  ec_obj => "ecp_nistz256.o ecp_nistz256-x86.o",
  ex_libs => "ws2_32.lib gdi32.lib advapi32.lib crypt32.lib user32.lib",
  exe_extension => "",
  ld => "link",
  lflags => "/nologo /debug",
  lib_cflags => "/Zi /Fdossl_static /MT /Zl",
  loutflag => "/out:",
  md5_asm_src => "md5-586.s",
  md5_obj => "md5-586.o",
  modes_asm_src => "ghash-x86.s",
  modes_obj => "ghash-x86.o",
  mt => "mt",
  mtflags => "-nologo",
  mtinflag => "-manifest ",
  mtoutflag => "-outputresource:",
  nm => "nm",
  padlock_asm_src => "e_padlock-x86.s",
  padlock_obj => "e_padlock-x86.o",
  perlasm_scheme => "win32n",
  poly1305_asm_src => "poly1305-x86.s",
  poly1305_obj => "poly1305-x86.o",
  ranlib => "\$(CROSS_COMPILE)ranlib",
  rc => "rc",
  rc4_asm_src => "rc4-586.s",
  rc4_obj => "rc4-586.o",
  rc5_asm_src => "rc5-586.s",
  rc5_obj => "rc5-586.o",
  rcoutflag => "/fo",
  rmd160_asm_src => "rmd-586.s",
  rmd160_obj => "rmd-586.o",
  sha1_asm_src => "sha1-586.s sha256-586.s sha512-586.s",
  sha1_obj => "sha1-586.o sha256-586.o sha512-586.o",
  shared_cflag => "",
  shared_defines => [  ],
  shared_extension => "",
  shared_extension_simple => "",
  shared_ldflag => "/dll",
  shared_rcflag => "",
  shared_target => "win-shared",
  sys_id => "WIN32",
  template => "1",
  thread_defines => [  ],
  thread_scheme => "winthreads",
  unistd => "<unistd.h>",
  uplink_aux_src => "",
  uplink_obj => "",
  wp_asm_src => "wp_block.c wp-mmx.s",
  wp_obj => "wp_block.o wp-mmx.o",
);

our %available_protocols = (
  tls => [ "ssl3", "tls1", "tls1_1", "tls1_2" ],
  dtls => [ "dtls1", "dtls1_2" ],
);

our @disablables = (
  "afalgeng",
  "asan",
  "asm",
  "async",
  "autoalginit",
  "autoerrinit",
  "bf",
  "blake2",
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
  "sock",
  "srp",
  "srtp",
  "sse2",
  "ssl",
  "ssl-trace",
  "static-engine",
  "stdio",
  "threads",
  "tls",
  "ts",
  "ubsan",
  "ui",
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
  "dtls1",
  "dtls1-method",
  "dtls1_2",
  "dtls1_2-method",
);

our %disabled = (
  "afalgeng" => "option",
  "asan" => "default",
  "comp" => "option",
  "crypto-mdebug" => "default",
  "crypto-mdebug-backtrace" => "default",
  "dynamic-engine" => "forced",
  "ec_nistp_64_gcc_128" => "default",
  "egd" => "default",
  "fuzz-afl" => "default",
  "fuzz-libfuzzer" => "default",
  "heartbeats" => "default",
  "md2" => "default",
  "msan" => "default",
  "rc5" => "default",
  "sctp" => "default",
  "shared" => "option",
  "ssl-trace" => "default",
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
                    "crypto/include/internal/bn_conf.h",
                    "crypto/include/internal/dso_conf.h",
                    "include/openssl/opensslconf.h",
                ],
            "apps/app_rand.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/apps.o" =>
                [
                    "apps/progs.h",
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
                    "libssl",
                ],
            "apps/openssl.o" =>
                [
                    "apps/progs.h",
                ],
            "apps/opt.o" =>
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
            "apps/s_cb.o" =>
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
            "apps/s_socket.o" =>
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
            "apps/win32_init.o" =>
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
            "crypto/include/internal/bn_conf.h" =>
                [
                    "configdata.pm",
                ],
            "crypto/include/internal/dso_conf.h" =>
                [
                    "configdata.pm",
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
            "include/openssl/opensslconf.h" =>
                [
                    "configdata.pm",
                ],
            "libssl" =>
                [
                    "libcrypto",
                ],
            "test/aborttest" =>
                [
                    "libcrypto",
                ],
            "test/afalgtest" =>
                [
                    "libcrypto",
                ],
            "test/asynciotest" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/asynctest" =>
                [
                    "libcrypto",
                ],
            "test/bad_dtls_test" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/bftest" =>
                [
                    "libcrypto",
                ],
            "test/bio_enc_test" =>
                [
                    "libcrypto",
                ],
            "test/bioprinttest" =>
                [
                    "libcrypto",
                ],
            "test/bntest" =>
                [
                    "libcrypto",
                ],
            "test/buildtest_aes" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_asn1" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_asn1t" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_async" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_bio" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_blowfish" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_bn" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_buffer" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_camellia" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_cast" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_cmac" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_cms" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_conf" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_conf_api" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_crypto" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_ct" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_des" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_dh" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_dsa" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_dtls1" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_e_os2" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_ebcdic" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_ec" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_ecdh" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_ecdsa" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_engine" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_err" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_evp" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_hmac" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_idea" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_kdf" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_lhash" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_md4" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_md5" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_mdc2" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_modes" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_obj_mac" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_objects" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_ocsp" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_opensslv" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_ossl_typ" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_pem" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_pem2" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_pkcs12" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_pkcs7" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_rand" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_rc2" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_rc4" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_ripemd" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_rsa" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_safestack" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_seed" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_sha" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_srp" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_srtp" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_ssl" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_ssl2" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_stack" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_symhacks" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_tls1" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_ts" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_txt_db" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_ui" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_whrlpool" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_x509" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_x509_vfy" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/buildtest_x509v3" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/casttest" =>
                [
                    "libcrypto",
                ],
            "test/cipherlist_test" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/clienthellotest" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/constant_time_test" =>
                [
                    "libcrypto",
                ],
            "test/crltest" =>
                [
                    "libcrypto",
                ],
            "test/ct_test" =>
                [
                    "libcrypto",
                ],
            "test/d2i_test" =>
                [
                    "libcrypto",
                ],
            "test/danetest" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/destest" =>
                [
                    "libcrypto",
                ],
            "test/dhtest" =>
                [
                    "libcrypto",
                ],
            "test/dsatest" =>
                [
                    "libcrypto",
                ],
            "test/dtlstest" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/dtlsv1listentest" =>
                [
                    "libssl",
                ],
            "test/ecdsatest" =>
                [
                    "libcrypto",
                ],
            "test/ectest" =>
                [
                    "libcrypto",
                ],
            "test/enginetest" =>
                [
                    "libcrypto",
                ],
            "test/evp_extra_test" =>
                [
                    "libcrypto",
                ],
            "test/evp_test" =>
                [
                    "libcrypto",
                ],
            "test/exdatatest" =>
                [
                    "libcrypto",
                ],
            "test/exptest" =>
                [
                    "libcrypto",
                ],
            "test/fatalerrtest" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/gmdifftest" =>
                [
                    "libcrypto",
                ],
            "test/heartbeat_test" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/hmactest" =>
                [
                    "libcrypto",
                ],
            "test/ideatest" =>
                [
                    "libcrypto",
                ],
            "test/igetest" =>
                [
                    "libcrypto",
                ],
            "test/md2test" =>
                [
                    "libcrypto",
                ],
            "test/md4test" =>
                [
                    "libcrypto",
                ],
            "test/md5test" =>
                [
                    "libcrypto",
                ],
            "test/mdc2test" =>
                [
                    "libcrypto",
                ],
            "test/memleaktest" =>
                [
                    "libcrypto",
                ],
            "test/ocspapitest" =>
                [
                    "libcrypto",
                ],
            "test/p5_crpt2_test" =>
                [
                    "libcrypto",
                ],
            "test/packettest" =>
                [
                    "libcrypto",
                ],
            "test/pbelutest" =>
                [
                    "libcrypto",
                ],
            "test/randtest" =>
                [
                    "libcrypto",
                ],
            "test/rc2test" =>
                [
                    "libcrypto",
                ],
            "test/rc4test" =>
                [
                    "libcrypto",
                ],
            "test/rc5test" =>
                [
                    "libcrypto",
                ],
            "test/rmdtest" =>
                [
                    "libcrypto",
                ],
            "test/rsa_test" =>
                [
                    "libcrypto",
                ],
            "test/sanitytest" =>
                [
                    "libcrypto",
                ],
            "test/secmemtest" =>
                [
                    "libcrypto",
                ],
            "test/sha1test" =>
                [
                    "libcrypto",
                ],
            "test/sha256t" =>
                [
                    "libcrypto",
                ],
            "test/sha512t" =>
                [
                    "libcrypto",
                ],
            "test/srptest" =>
                [
                    "libcrypto",
                ],
            "test/ssl_test" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/ssl_test_ctx_test" =>
                [
                    "libcrypto",
                ],
            "test/sslapitest" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/sslcorrupttest" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/ssltest_old" =>
                [
                    "libcrypto",
                    "libssl",
                ],
            "test/threadstest" =>
                [
                    "libcrypto",
                ],
            "test/v3ext" =>
                [
                    "libcrypto",
                ],
            "test/v3nametest" =>
                [
                    "libcrypto",
                ],
            "test/verify_extra_test" =>
                [
                    "libcrypto",
                ],
            "test/wp_test" =>
                [
                    "libcrypto",
                ],
            "test/x509aux" =>
                [
                    "libcrypto",
                ],
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
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
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
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
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
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
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
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
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
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
                    "\$(PROCESSOR)",
                ],
            "crypto/bn/bn-ia64.s" =>
                [
                    "crypto/bn/asm/ia64.S",
                ],
            "crypto/bn/bn-mips.s" =>
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
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
                    "\$(PROCESSOR)",
                ],
            "crypto/bn/ia64-mont.s" =>
                [
                    "crypto/bn/asm/ia64-mont.pl",
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
                ],
            "crypto/bn/mips-mont.s" =>
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
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
                    "\$(PROCESSOR)",
                ],
            "crypto/bn/x86-mont.s" =>
                [
                    "crypto/bn/asm/x86-mont.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
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
                    "\$(CFLAGS_Q)\"",
                    "\"\$(PLATFORM)\"",
                ],
            "crypto/camellia/cmll-x86.s" =>
                [
                    "crypto/camellia/asm/cmll-x86.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
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
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
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
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
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
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
                ],
            "crypto/des/des-586.s" =>
                [
                    "crypto/des/asm/des-586.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
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
            "crypto/ec/ecp_nistz256-sparcv9.S" =>
                [
                    "crypto/ec/asm/ecp_nistz256-sparcv9.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/ec/ecp_nistz256-x86.s" =>
                [
                    "crypto/ec/asm/ecp_nistz256-x86.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
                    "\$(PROCESSOR)",
                ],
            "crypto/ec/ecp_nistz256-x86_64.s" =>
                [
                    "crypto/ec/asm/ecp_nistz256-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "crypto/ia64cpuid.s" =>
                [
                    "crypto/ia64cpuid.S",
                ],
            "crypto/include/internal/bn_conf.h" =>
                [
                    "crypto/include/internal/bn_conf.h.in",
                ],
            "crypto/include/internal/dso_conf.h" =>
                [
                    "crypto/include/internal/dso_conf.h.in",
                ],
            "crypto/md5/md5-586.s" =>
                [
                    "crypto/md5/asm/md5-586.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
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
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
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
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
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
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
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
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
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
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
                ],
            "crypto/sha/sha1-586.s" =>
                [
                    "crypto/sha/asm/sha1-586.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
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
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
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
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
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
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
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
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
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
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
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
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
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
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
                    "\$(PROCESSOR)",
                ],
            "engines/e_padlock-x86.s" =>
                [
                    "engines/asm/e_padlock-x86.pl",
                    "\$(PERLASM_SCHEME)",
                    "\$(CFLAGS)",
                    "\$(LIB_CFLAGS)",
                    "\$(PROCESSOR)",
                ],
            "engines/e_padlock-x86_64.s" =>
                [
                    "engines/asm/e_padlock-x86_64.pl",
                    "\$(PERLASM_SCHEME)",
                ],
            "include/openssl/opensslconf.h" =>
                [
                    "include/openssl/opensslconf.h.in",
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
            "test/buildtest_err.c" =>
                [
                    "test/generate_buildtest.pl",
                    "err",
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
                    "apps",
                ],
            "apps/apps.o" =>
                [
                    ".",
                    "include",
                    "apps",
                ],
            "apps/asn1pars.o" =>
                [
                    ".",
                    "include",
                    "apps",
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
                    "apps",
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
                    "apps",
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
                    "apps",
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
            "apps/win32_init.o" =>
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
            "crypto/aes/aes-586.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/aes/aes-armv4.o" =>
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
            "crypto/aes/aes_cfb.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/aes/aes_ecb.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/aes/aes_ige.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/aes/aes_misc.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/aes/aes_ofb.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/aes/aes_wrap.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/aes/aesfx-sparcv9.o" =>
                [
                    "crypto",
                ],
            "crypto/aes/aesni-x86.o" =>
                [
                    ".",
                    "crypto/include",
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
                    "crypto/include",
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
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/a_d2i_fp.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/a_digest.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/a_dup.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/a_gentm.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/a_i2d_fp.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/a_int.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/a_mbstr.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/a_object.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/a_octet.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/a_print.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/a_sign.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/a_strex.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/a_strnid.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/a_time.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/a_type.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/a_utctm.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/a_utf8.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/a_verify.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/ameth_lib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/asn1_err.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/asn1_gen.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/asn1_lib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/asn1_par.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/asn_mime.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/asn_moid.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/asn_mstbl.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/asn_pack.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/bio_asn1.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/bio_ndef.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/d2i_pr.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/d2i_pu.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/evp_asn1.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/f_int.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/f_string.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/i2d_pr.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/i2d_pu.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/n_pkey.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/nsseq.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/p5_pbe.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/p5_pbev2.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/p5_scrypt.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/p8_pkey.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/t_bitst.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/t_pkey.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/t_spki.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/tasn_dec.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/tasn_enc.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/tasn_fre.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/tasn_new.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/tasn_prn.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/tasn_scn.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/tasn_typ.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/tasn_utl.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/x_algor.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/x_bignum.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/x_info.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/x_int64.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/x_long.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/x_pkey.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/x_sig.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/x_spki.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/asn1/x_val.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/async/arch/async_null.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/async/arch/async_posix.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/async/arch/async_win.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/async/async.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/async/async_err.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/async/async_wait.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bf/bf-586.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bf/bf_cfb64.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bf/bf_ecb.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bf/bf_ofb64.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bf/bf_skey.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bio/b_addr.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bio/b_dump.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bio/b_print.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bio/b_sock.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bio/b_sock2.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bio/bf_buff.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bio/bf_lbuf.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bio/bf_nbio.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bio/bf_null.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bio/bio_cb.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bio/bio_err.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bio/bio_lib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bio/bio_meth.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bio/bss_acpt.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bio/bss_bio.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bio/bss_conn.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bio/bss_dgram.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bio/bss_fd.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bio/bss_file.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bio/bss_log.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bio/bss_mem.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bio/bss_null.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bio/bss_sock.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/blake2/blake2b.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/blake2/blake2s.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/blake2/m_blake2b.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/blake2/m_blake2s.o" =>
                [
                    ".",
                    "crypto/include",
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
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_add.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_blind.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_const.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_ctx.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_depr.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_dh.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_div.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_err.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_exp.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                    "crypto",
                ],
            "crypto/bn/bn_exp2.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_gcd.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_gf2m.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_intern.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_kron.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_lib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_mod.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_mont.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_mpi.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_mul.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_nist.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_prime.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_print.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_rand.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_recp.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_shift.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_sqr.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_sqrt.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_srp.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_word.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/bn_x931p.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/co-586.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
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
                    "crypto/include",
                    "include",
                ],
            "crypto/bn/x86-mont.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/buffer/buf_err.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/buffer/buffer.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/buildinf.h" =>
                [
                    ".",
                ],
            "crypto/camellia/cmll-x86.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/camellia/cmll_cfb.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/camellia/cmll_ctr.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/camellia/cmll_ecb.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/camellia/cmll_ofb.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/camellia/cmllt4-sparcv9.o" =>
                [
                    "crypto",
                ],
            "crypto/cast/c_cfb64.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/cast/c_ecb.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/cast/c_enc.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/cast/c_ofb64.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/cast/c_skey.o" =>
                [
                    ".",
                    "crypto/include",
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
                    "crypto/include",
                    "include",
                ],
            "crypto/cmac/cm_ameth.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/cmac/cm_pmeth.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/cmac/cmac.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/cms/cms_asn1.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/cms/cms_att.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/cms/cms_cd.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/cms/cms_dd.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/cms/cms_enc.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/cms/cms_env.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/cms/cms_err.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/cms/cms_ess.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/cms/cms_io.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/cms/cms_kari.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/cms/cms_lib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/cms/cms_pwri.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/cms/cms_sd.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/cms/cms_smime.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/conf/conf_api.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/conf/conf_def.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/conf/conf_err.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/conf/conf_lib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/conf/conf_mall.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/conf/conf_mod.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/conf/conf_sap.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/cpt_err.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/cryptlib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ct/ct_b64.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ct/ct_err.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ct/ct_log.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ct/ct_oct.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ct/ct_policy.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ct/ct_prn.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ct/ct_sct.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ct/ct_sct_ctx.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ct/ct_vfy.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ct/ct_x509v3.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/cversion.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                    "crypto",
                ],
            "crypto/des/cbc_cksm.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/des/cbc_enc.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/des/cfb64ede.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/des/cfb64enc.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/des/cfb_enc.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/des/crypt586.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/des/des-586.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/des/dest4-sparcv9.o" =>
                [
                    "crypto",
                ],
            "crypto/des/ecb3_enc.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/des/ecb_enc.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/des/fcrypt.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/des/ofb64ede.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/des/ofb64enc.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/des/ofb_enc.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/des/pcbc_enc.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/des/qud_cksm.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/des/rand_key.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/des/rpc_enc.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/des/set_key.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/des/str2key.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/des/xcbc_enc.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dh/dh_ameth.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dh/dh_asn1.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dh/dh_check.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dh/dh_depr.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dh/dh_err.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dh/dh_gen.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dh/dh_kdf.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dh/dh_key.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dh/dh_lib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dh/dh_meth.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dh/dh_pmeth.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dh/dh_prn.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dh/dh_rfc5114.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dllmain.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dsa/dsa_ameth.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dsa/dsa_asn1.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dsa/dsa_depr.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dsa/dsa_err.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dsa/dsa_gen.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dsa/dsa_key.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dsa/dsa_lib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dsa/dsa_meth.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dsa/dsa_ossl.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dsa/dsa_pmeth.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dsa/dsa_prn.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dsa/dsa_sign.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dsa/dsa_vrf.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dso/dso_dl.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dso/dso_dlfcn.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dso/dso_err.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dso/dso_lib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dso/dso_openssl.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dso/dso_vms.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/dso/dso_win32.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ebcdic.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/curve25519.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ec2_mult.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ec2_oct.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ec2_smpl.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ec_ameth.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ec_asn1.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ec_check.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ec_curve.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ec_cvt.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ec_err.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ec_key.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ec_kmeth.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ec_lib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ec_mult.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ec_oct.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ec_pmeth.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ec_print.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ecdh_kdf.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ecdh_ossl.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ecdsa_ossl.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ecdsa_sign.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ecdsa_vrf.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/eck_prn.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ecp_mont.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ecp_nist.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ecp_nistp224.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ecp_nistp256.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ecp_nistp521.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ecp_nistputil.o" =>
                [
                    ".",
                    "crypto/include",
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
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ecp_nistz256.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ecp_oct.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ecp_smpl.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ec/ecx_meth.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/engine/eng_all.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/engine/eng_cnf.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/engine/eng_cryptodev.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/engine/eng_ctrl.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/engine/eng_dyn.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/engine/eng_err.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/engine/eng_fat.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/engine/eng_init.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/engine/eng_lib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/engine/eng_list.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/engine/eng_openssl.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/engine/eng_pkey.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/engine/eng_rdrand.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/engine/eng_table.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/engine/tb_asnmth.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/engine/tb_cipher.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/engine/tb_dh.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/engine/tb_digest.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/engine/tb_dsa.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/engine/tb_eckey.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/engine/tb_pkmeth.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/engine/tb_rand.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/engine/tb_rsa.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/err/err.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/err/err_all.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/err/err_prn.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/bio_b64.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/bio_enc.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/bio_md.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/bio_ok.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/c_allc.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/c_alld.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/cmeth_lib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/digest.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/e_aes.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                    "crypto",
                    "crypto/modes",
                ],
            "crypto/evp/e_aes_cbc_hmac_sha1.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                    "crypto/modes",
                ],
            "crypto/evp/e_aes_cbc_hmac_sha256.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                    "crypto/modes",
                ],
            "crypto/evp/e_bf.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/e_camellia.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                    "crypto",
                    "crypto/modes",
                ],
            "crypto/evp/e_cast.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/e_chacha20_poly1305.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/e_des.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                    "crypto",
                ],
            "crypto/evp/e_des3.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                    "crypto",
                ],
            "crypto/evp/e_idea.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/e_null.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/e_old.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/e_rc2.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/e_rc4.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/e_rc4_hmac_md5.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/e_rc5.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/e_seed.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/e_xcbc_d.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/encode.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/evp_cnf.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/evp_enc.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/evp_err.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/evp_key.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/evp_lib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/evp_pbe.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/evp_pkey.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/m_md2.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/m_md4.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/m_md5.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/m_md5_sha1.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/m_mdc2.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/m_null.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/m_ripemd.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/m_sha1.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/m_sigver.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/m_wp.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/names.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/p5_crpt.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/p5_crpt2.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/p_dec.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/p_enc.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/p_lib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/p_open.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/p_seal.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/p_sign.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/p_verify.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/pmeth_fn.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/pmeth_gn.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/pmeth_lib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/evp/scrypt.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ex_data.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/hmac/hm_ameth.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/hmac/hm_pmeth.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/hmac/hmac.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/idea/i_cbc.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/idea/i_cfb64.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/idea/i_ecb.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/idea/i_ofb64.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/idea/i_skey.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/include/internal/bn_conf.h" =>
                [
                    ".",
                ],
            "crypto/include/internal/dso_conf.h" =>
                [
                    ".",
                ],
            "crypto/init.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/kdf/hkdf.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/kdf/kdf_err.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/kdf/tls1_prf.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/lhash/lh_stats.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/lhash/lhash.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/md4/md4_dgst.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/md4/md4_one.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/md5/md5-586.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/md5/md5-sparcv9.o" =>
                [
                    "crypto",
                ],
            "crypto/md5/md5_dgst.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/md5/md5_one.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/mdc2/mdc2_one.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/mdc2/mdc2dgst.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/mem.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/mem_dbg.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/mem_sec.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/modes/cbc128.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/modes/ccm128.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/modes/cfb128.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/modes/ctr128.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/modes/cts128.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/modes/gcm128.o" =>
                [
                    ".",
                    "crypto/include",
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
                    "crypto/include",
                    "include",
                ],
            "crypto/modes/ghashv8-armx.o" =>
                [
                    "crypto",
                ],
            "crypto/modes/ocb128.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/modes/ofb128.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/modes/wrap128.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/modes/xts128.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/o_dir.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/o_fips.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/o_fopen.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/o_init.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/o_str.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/o_time.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/objects/o_names.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/objects/obj_dat.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/objects/obj_err.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/objects/obj_lib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/objects/obj_xref.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ocsp/ocsp_asn.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ocsp/ocsp_cl.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ocsp/ocsp_err.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ocsp/ocsp_ext.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ocsp/ocsp_ht.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ocsp/ocsp_lib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ocsp/ocsp_prn.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ocsp/ocsp_srv.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ocsp/ocsp_vfy.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ocsp/v3_ocsp.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pem/pem_all.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pem/pem_err.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pem/pem_info.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pem/pem_lib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pem/pem_oth.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pem/pem_pk8.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pem/pem_pkey.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pem/pem_sign.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pem/pem_x509.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pem/pem_xaux.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pem/pvkfmt.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pkcs12/p12_add.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pkcs12/p12_asn.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pkcs12/p12_attr.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pkcs12/p12_crpt.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pkcs12/p12_crt.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pkcs12/p12_decr.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pkcs12/p12_init.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pkcs12/p12_key.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pkcs12/p12_kiss.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pkcs12/p12_mutl.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pkcs12/p12_npas.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pkcs12/p12_p8d.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pkcs12/p12_p8e.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pkcs12/p12_sbag.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pkcs12/p12_utl.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pkcs12/pk12err.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pkcs7/bio_pk7.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pkcs7/pk7_asn1.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pkcs7/pk7_attr.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pkcs7/pk7_doit.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pkcs7/pk7_lib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pkcs7/pk7_mime.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pkcs7/pk7_smime.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/pkcs7/pkcs7err.o" =>
                [
                    ".",
                    "crypto/include",
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
            "crypto/poly1305/poly1305-s390x.o" =>
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
                    "crypto/include",
                    "include",
                ],
            "crypto/poly1305/poly1305.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rand/md_rand.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rand/rand_egd.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rand/rand_err.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rand/rand_lib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rand/rand_unix.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rand/rand_vms.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rand/rand_win.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rand/randfile.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rc2/rc2_cbc.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rc2/rc2_ecb.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rc2/rc2_skey.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rc2/rc2cfb64.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rc2/rc2ofb64.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rc4/rc4-586.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ripemd/rmd-586.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ripemd/rmd_dgst.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ripemd/rmd_one.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rsa/rsa_ameth.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rsa/rsa_asn1.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rsa/rsa_chk.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rsa/rsa_crpt.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rsa/rsa_depr.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rsa/rsa_err.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rsa/rsa_gen.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rsa/rsa_lib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rsa/rsa_meth.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rsa/rsa_none.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rsa/rsa_null.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rsa/rsa_oaep.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rsa/rsa_ossl.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rsa/rsa_pk1.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rsa/rsa_pmeth.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rsa/rsa_prn.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rsa/rsa_pss.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rsa/rsa_saos.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rsa/rsa_sign.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rsa/rsa_ssl.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rsa/rsa_x931.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/rsa/rsa_x931g.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/seed/seed.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/seed/seed_cbc.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/seed/seed_cfb.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/seed/seed_ecb.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/seed/seed_ofb.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/sha/sha1-586.o" =>
                [
                    ".",
                    "crypto/include",
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
                    "crypto/include",
                    "include",
                ],
            "crypto/sha/sha1dgst.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/sha/sha256-586.o" =>
                [
                    ".",
                    "crypto/include",
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
                    "crypto/include",
                    "include",
                ],
            "crypto/sha/sha512-586.o" =>
                [
                    ".",
                    "crypto/include",
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
                    "crypto/include",
                    "include",
                ],
            "crypto/srp/srp_lib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/srp/srp_vfy.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/stack/stack.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/threads_none.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/threads_pthread.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/threads_win.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ts/ts_asn1.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ts/ts_conf.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ts/ts_err.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ts/ts_lib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ts/ts_req_print.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ts/ts_req_utils.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ts/ts_rsp_print.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ts/ts_rsp_sign.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ts/ts_rsp_utils.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ts/ts_rsp_verify.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ts/ts_verify_ctx.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/txt_db/txt_db.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ui/ui_err.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ui/ui_lib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ui/ui_openssl.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/ui/ui_util.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/uid.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/whrlpool/wp-mmx.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/whrlpool/wp_block.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/whrlpool/wp_dgst.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/by_dir.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/by_file.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/t_crl.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/t_req.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/t_x509.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x509_att.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x509_cmp.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x509_d2.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x509_def.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x509_err.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x509_ext.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x509_lu.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x509_obj.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x509_r2x.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x509_req.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x509_set.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x509_trs.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x509_txt.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x509_v3.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x509_vfy.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x509_vpm.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x509cset.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x509name.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x509rset.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x509spki.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x509type.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x_all.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x_attrib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x_crl.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x_exten.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x_name.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x_pubkey.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x_req.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x_x509.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509/x_x509a.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/pcy_cache.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/pcy_data.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/pcy_lib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/pcy_map.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/pcy_node.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/pcy_tree.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_addr.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_akey.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_akeya.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_alt.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_asid.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_bcons.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_bitst.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_conf.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_cpols.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_crld.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_enum.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_extku.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_genn.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_ia5.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_info.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_int.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_lib.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_ncons.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_pci.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_pcia.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_pcons.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_pku.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_pmaps.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_prn.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_purp.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_skey.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_sxnet.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_tlsf.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3_utl.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x509v3/v3err.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "crypto/x86cpuid.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "engines/e_capi.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "engines/e_padlock-x86.o" =>
                [
                    ".",
                    "crypto/include",
                    "include",
                ],
            "engines/e_padlock.o" =>
                [
                    ".",
                    "crypto/include",
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
            "ssl/t1_ext.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/t1_lib.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/t1_reneg.o" =>
                [
                    ".",
                    "include",
                ],
            "ssl/t1_trce.o" =>
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
                    ".",
                    "include",
                ],
            "test/asynciotest.o" =>
                [
                    "include",
                ],
            "test/asynctest.o" =>
                [
                    ".",
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
            "test/bio_enc_test.o" =>
                [
                    "include",
                ],
            "test/bioprinttest.o" =>
                [
                    "include",
                ],
            "test/bntest.o" =>
                [
                    ".",
                    "crypto/include",
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
            "test/buildtest_err.o" =>
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
            "test/cipherlist_test.o" =>
                [
                    ".",
                    "include",
                ],
            "test/clienthellotest.o" =>
                [
                    "include",
                ],
            "test/constant_time_test.o" =>
                [
                    ".",
                    "include",
                ],
            "test/crltest.o" =>
                [
                    "include",
                ],
            "test/ct_test.o" =>
                [
                    "crypto/include",
                    "include",
                ],
            "test/d2i_test.o" =>
                [
                    ".",
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
            "test/dsatest.o" =>
                [
                    "include",
                ],
            "test/dtlstest.o" =>
                [
                    "include",
                    "test",
                ],
            "test/dtlsv1listentest.o" =>
                [
                    ".",
                    "include",
                ],
            "test/ecdsatest.o" =>
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
                    ".",
                ],
            "test/gmdifftest.o" =>
                [
                    "include",
                ],
            "test/handshake_helper.o" =>
                [
                    ".",
                    "include",
                ],
            "test/heartbeat_test.o" =>
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
                    ".",
                    "include",
                ],
            "test/md2test.o" =>
                [
                    "include",
                ],
            "test/md4test.o" =>
                [
                    "include",
                ],
            "test/md5test.o" =>
                [
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
            "test/ocspapitest.o" =>
                [
                    "include",
                    ".",
                ],
            "test/p5_crpt2_test.o" =>
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
            "test/randtest.o" =>
                [
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
            "test/rmdtest.o" =>
                [
                    "include",
                ],
            "test/rsa_test.o" =>
                [
                    ".",
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
            "test/sha1test.o" =>
                [
                    "include",
                ],
            "test/sha256t.o" =>
                [
                    "include",
                ],
            "test/sha512t.o" =>
                [
                    "include",
                ],
            "test/srptest.o" =>
                [
                    "include",
                ],
            "test/ssl_test.o" =>
                [
                    ".",
                    "include",
                ],
            "test/ssl_test_ctx.o" =>
                [
                    ".",
                    "include",
                ],
            "test/ssl_test_ctx_test.o" =>
                [
                    ".",
                    "include",
                ],
            "test/sslapitest.o" =>
                [
                    "include",
                    ".",
                ],
            "test/sslcorrupttest.o" =>
                [
                    "include",
                    "test",
                ],
            "test/ssltest_old.o" =>
                [
                    ".",
                    "include",
                ],
            "test/ssltestlib.o" =>
                [
                    "test",
                    ".",
                    "include",
                ],
            "test/testutil.o" =>
                [
                    "crypto/include",
                    "test",
                    "include",
                    ".",
                ],
            "test/threadstest.o" =>
                [
                    ".",
                    "include",
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
            "test/wp_test.o" =>
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
                    "tools/c_rehash.pl",
                ],
        },
    "ldadd" =>
        {
        },
    "libraries" =>
        [
            "libcrypto",
            "libssl",
        ],
    "ordinals" =>
        {
            "libcrypto" =>
                [
                    "crypto",
                    "util/libcrypto.num",
                ],
            "libssl" =>
                [
                    "ssl",
                    "util/libssl.num",
                ],
        },
    "overrides" =>
        [
            "crypto/bn/bn-mips3.o",
            "crypto/bn/pa-risc2.c",
            "crypto/bn/pa-risc2W.o",
        ],
    "programs" =>
        [
            "apps/openssl",
            "fuzz/asn1-test",
            "fuzz/asn1parse-test",
            "fuzz/bignum-test",
            "fuzz/bndiv-test",
            "fuzz/cms-test",
            "fuzz/conf-test",
            "fuzz/crl-test",
            "fuzz/ct-test",
            "fuzz/server-test",
            "fuzz/x509-test",
            "test/aborttest",
            "test/afalgtest",
            "test/asynciotest",
            "test/asynctest",
            "test/bad_dtls_test",
            "test/bftest",
            "test/bio_enc_test",
            "test/bioprinttest",
            "test/bntest",
            "test/buildtest_aes",
            "test/buildtest_asn1",
            "test/buildtest_asn1t",
            "test/buildtest_async",
            "test/buildtest_bio",
            "test/buildtest_blowfish",
            "test/buildtest_bn",
            "test/buildtest_buffer",
            "test/buildtest_camellia",
            "test/buildtest_cast",
            "test/buildtest_cmac",
            "test/buildtest_cms",
            "test/buildtest_conf",
            "test/buildtest_conf_api",
            "test/buildtest_crypto",
            "test/buildtest_ct",
            "test/buildtest_des",
            "test/buildtest_dh",
            "test/buildtest_dsa",
            "test/buildtest_dtls1",
            "test/buildtest_e_os2",
            "test/buildtest_ebcdic",
            "test/buildtest_ec",
            "test/buildtest_ecdh",
            "test/buildtest_ecdsa",
            "test/buildtest_engine",
            "test/buildtest_err",
            "test/buildtest_evp",
            "test/buildtest_hmac",
            "test/buildtest_idea",
            "test/buildtest_kdf",
            "test/buildtest_lhash",
            "test/buildtest_md4",
            "test/buildtest_md5",
            "test/buildtest_mdc2",
            "test/buildtest_modes",
            "test/buildtest_obj_mac",
            "test/buildtest_objects",
            "test/buildtest_ocsp",
            "test/buildtest_opensslv",
            "test/buildtest_ossl_typ",
            "test/buildtest_pem",
            "test/buildtest_pem2",
            "test/buildtest_pkcs12",
            "test/buildtest_pkcs7",
            "test/buildtest_rand",
            "test/buildtest_rc2",
            "test/buildtest_rc4",
            "test/buildtest_ripemd",
            "test/buildtest_rsa",
            "test/buildtest_safestack",
            "test/buildtest_seed",
            "test/buildtest_sha",
            "test/buildtest_srp",
            "test/buildtest_srtp",
            "test/buildtest_ssl",
            "test/buildtest_ssl2",
            "test/buildtest_stack",
            "test/buildtest_symhacks",
            "test/buildtest_tls1",
            "test/buildtest_ts",
            "test/buildtest_txt_db",
            "test/buildtest_ui",
            "test/buildtest_whrlpool",
            "test/buildtest_x509",
            "test/buildtest_x509_vfy",
            "test/buildtest_x509v3",
            "test/casttest",
            "test/cipherlist_test",
            "test/clienthellotest",
            "test/constant_time_test",
            "test/crltest",
            "test/ct_test",
            "test/d2i_test",
            "test/danetest",
            "test/destest",
            "test/dhtest",
            "test/dsatest",
            "test/dtlstest",
            "test/dtlsv1listentest",
            "test/ecdsatest",
            "test/ectest",
            "test/enginetest",
            "test/evp_extra_test",
            "test/evp_test",
            "test/exdatatest",
            "test/exptest",
            "test/fatalerrtest",
            "test/gmdifftest",
            "test/heartbeat_test",
            "test/hmactest",
            "test/ideatest",
            "test/igetest",
            "test/md2test",
            "test/md4test",
            "test/md5test",
            "test/mdc2test",
            "test/memleaktest",
            "test/ocspapitest",
            "test/p5_crpt2_test",
            "test/packettest",
            "test/pbelutest",
            "test/randtest",
            "test/rc2test",
            "test/rc4test",
            "test/rc5test",
            "test/rmdtest",
            "test/rsa_test",
            "test/sanitytest",
            "test/secmemtest",
            "test/sha1test",
            "test/sha256t",
            "test/sha512t",
            "test/srptest",
            "test/ssl_test",
            "test/ssl_test_ctx_test",
            "test/sslapitest",
            "test/sslcorrupttest",
            "test/ssltest_old",
            "test/threadstest",
            "test/v3ext",
            "test/v3nametest",
            "test/verify_extra_test",
            "test/wp_test",
            "test/x509aux",
        ],
    "rawlines" =>
        [
            "crypto/md5\\md5-ia64.asm: crypto/md5\\asm\\md5-ia64.S",
            "	\$(CC) \$(CFLAGS) -EP crypto/md5\\asm\\md5-ia64.S > \$\@.i && move /Y \$\@.i \$\@",
            "crypto/rc4\\rc4-ia64.asm: crypto/rc4\\asm\\rc4-ia64.pl",
            "	\$(PERL) crypto/rc4\\asm\\rc4-ia64.pl \$\@.S",
            "	\$(CC) -DSZ=4 -EP \$\@.S > \$\@.i && move /Y \$\@.i \$\@",
            "	del /Q \$\@.S",
        ],
    "rename" =>
        {
        },
    "scripts" =>
        [
            "apps/CA.pl",
            "apps/tsget.pl",
            "tools/c_rehash.pl",
        ],
    "shared_sources" =>
        {
            "apps/openssl" =>
                [
                ],
            "fuzz/asn1-test" =>
                [
                ],
            "fuzz/asn1parse-test" =>
                [
                ],
            "fuzz/bignum-test" =>
                [
                ],
            "fuzz/bndiv-test" =>
                [
                ],
            "fuzz/cms-test" =>
                [
                ],
            "fuzz/conf-test" =>
                [
                ],
            "fuzz/crl-test" =>
                [
                ],
            "fuzz/ct-test" =>
                [
                ],
            "fuzz/server-test" =>
                [
                ],
            "fuzz/x509-test" =>
                [
                ],
            "libcrypto" =>
                [
                    "crypto/dllmain.o",
                ],
            "libssl" =>
                [
                ],
            "test/aborttest" =>
                [
                ],
            "test/afalgtest" =>
                [
                ],
            "test/asynciotest" =>
                [
                ],
            "test/asynctest" =>
                [
                ],
            "test/bad_dtls_test" =>
                [
                ],
            "test/bftest" =>
                [
                ],
            "test/bio_enc_test" =>
                [
                ],
            "test/bioprinttest" =>
                [
                ],
            "test/bntest" =>
                [
                ],
            "test/buildtest_aes" =>
                [
                ],
            "test/buildtest_asn1" =>
                [
                ],
            "test/buildtest_asn1t" =>
                [
                ],
            "test/buildtest_async" =>
                [
                ],
            "test/buildtest_bio" =>
                [
                ],
            "test/buildtest_blowfish" =>
                [
                ],
            "test/buildtest_bn" =>
                [
                ],
            "test/buildtest_buffer" =>
                [
                ],
            "test/buildtest_camellia" =>
                [
                ],
            "test/buildtest_cast" =>
                [
                ],
            "test/buildtest_cmac" =>
                [
                ],
            "test/buildtest_cms" =>
                [
                ],
            "test/buildtest_conf" =>
                [
                ],
            "test/buildtest_conf_api" =>
                [
                ],
            "test/buildtest_crypto" =>
                [
                ],
            "test/buildtest_ct" =>
                [
                ],
            "test/buildtest_des" =>
                [
                ],
            "test/buildtest_dh" =>
                [
                ],
            "test/buildtest_dsa" =>
                [
                ],
            "test/buildtest_dtls1" =>
                [
                ],
            "test/buildtest_e_os2" =>
                [
                ],
            "test/buildtest_ebcdic" =>
                [
                ],
            "test/buildtest_ec" =>
                [
                ],
            "test/buildtest_ecdh" =>
                [
                ],
            "test/buildtest_ecdsa" =>
                [
                ],
            "test/buildtest_engine" =>
                [
                ],
            "test/buildtest_err" =>
                [
                ],
            "test/buildtest_evp" =>
                [
                ],
            "test/buildtest_hmac" =>
                [
                ],
            "test/buildtest_idea" =>
                [
                ],
            "test/buildtest_kdf" =>
                [
                ],
            "test/buildtest_lhash" =>
                [
                ],
            "test/buildtest_md4" =>
                [
                ],
            "test/buildtest_md5" =>
                [
                ],
            "test/buildtest_mdc2" =>
                [
                ],
            "test/buildtest_modes" =>
                [
                ],
            "test/buildtest_obj_mac" =>
                [
                ],
            "test/buildtest_objects" =>
                [
                ],
            "test/buildtest_ocsp" =>
                [
                ],
            "test/buildtest_opensslv" =>
                [
                ],
            "test/buildtest_ossl_typ" =>
                [
                ],
            "test/buildtest_pem" =>
                [
                ],
            "test/buildtest_pem2" =>
                [
                ],
            "test/buildtest_pkcs12" =>
                [
                ],
            "test/buildtest_pkcs7" =>
                [
                ],
            "test/buildtest_rand" =>
                [
                ],
            "test/buildtest_rc2" =>
                [
                ],
            "test/buildtest_rc4" =>
                [
                ],
            "test/buildtest_ripemd" =>
                [
                ],
            "test/buildtest_rsa" =>
                [
                ],
            "test/buildtest_safestack" =>
                [
                ],
            "test/buildtest_seed" =>
                [
                ],
            "test/buildtest_sha" =>
                [
                ],
            "test/buildtest_srp" =>
                [
                ],
            "test/buildtest_srtp" =>
                [
                ],
            "test/buildtest_ssl" =>
                [
                ],
            "test/buildtest_ssl2" =>
                [
                ],
            "test/buildtest_stack" =>
                [
                ],
            "test/buildtest_symhacks" =>
                [
                ],
            "test/buildtest_tls1" =>
                [
                ],
            "test/buildtest_ts" =>
                [
                ],
            "test/buildtest_txt_db" =>
                [
                ],
            "test/buildtest_ui" =>
                [
                ],
            "test/buildtest_whrlpool" =>
                [
                ],
            "test/buildtest_x509" =>
                [
                ],
            "test/buildtest_x509_vfy" =>
                [
                ],
            "test/buildtest_x509v3" =>
                [
                ],
            "test/casttest" =>
                [
                ],
            "test/cipherlist_test" =>
                [
                ],
            "test/clienthellotest" =>
                [
                ],
            "test/constant_time_test" =>
                [
                ],
            "test/crltest" =>
                [
                ],
            "test/ct_test" =>
                [
                ],
            "test/d2i_test" =>
                [
                ],
            "test/danetest" =>
                [
                ],
            "test/destest" =>
                [
                ],
            "test/dhtest" =>
                [
                ],
            "test/dsatest" =>
                [
                ],
            "test/dtlstest" =>
                [
                ],
            "test/dtlsv1listentest" =>
                [
                ],
            "test/ecdsatest" =>
                [
                ],
            "test/ectest" =>
                [
                ],
            "test/enginetest" =>
                [
                ],
            "test/evp_extra_test" =>
                [
                ],
            "test/evp_test" =>
                [
                ],
            "test/exdatatest" =>
                [
                ],
            "test/exptest" =>
                [
                ],
            "test/fatalerrtest" =>
                [
                ],
            "test/gmdifftest" =>
                [
                ],
            "test/heartbeat_test" =>
                [
                ],
            "test/hmactest" =>
                [
                ],
            "test/ideatest" =>
                [
                ],
            "test/igetest" =>
                [
                ],
            "test/md2test" =>
                [
                ],
            "test/md4test" =>
                [
                ],
            "test/md5test" =>
                [
                ],
            "test/mdc2test" =>
                [
                ],
            "test/memleaktest" =>
                [
                ],
            "test/ocspapitest" =>
                [
                ],
            "test/p5_crpt2_test" =>
                [
                ],
            "test/packettest" =>
                [
                ],
            "test/pbelutest" =>
                [
                ],
            "test/randtest" =>
                [
                ],
            "test/rc2test" =>
                [
                ],
            "test/rc4test" =>
                [
                ],
            "test/rc5test" =>
                [
                ],
            "test/rmdtest" =>
                [
                ],
            "test/rsa_test" =>
                [
                ],
            "test/sanitytest" =>
                [
                ],
            "test/secmemtest" =>
                [
                ],
            "test/sha1test" =>
                [
                ],
            "test/sha256t" =>
                [
                ],
            "test/sha512t" =>
                [
                ],
            "test/srptest" =>
                [
                ],
            "test/ssl_test" =>
                [
                ],
            "test/ssl_test_ctx_test" =>
                [
                ],
            "test/sslapitest" =>
                [
                ],
            "test/sslcorrupttest" =>
                [
                ],
            "test/ssltest_old" =>
                [
                ],
            "test/threadstest" =>
                [
                ],
            "test/v3ext" =>
                [
                ],
            "test/v3nametest" =>
                [
                ],
            "test/verify_extra_test" =>
                [
                ],
            "test/wp_test" =>
                [
                ],
            "test/x509aux" =>
                [
                ],
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
                    "apps/app_rand.o",
                    "apps/apps.o",
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
                    "apps/opt.o",
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
                    "apps/s_cb.o",
                    "apps/s_client.o",
                    "apps/s_server.o",
                    "apps/s_socket.o",
                    "apps/s_time.o",
                    "apps/sess_id.o",
                    "apps/smime.o",
                    "apps/speed.o",
                    "apps/spkac.o",
                    "apps/srp.o",
                    "apps/ts.o",
                    "apps/verify.o",
                    "apps/version.o",
                    "apps/win32_init.o",
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
            "apps/win32_init.o" =>
                [
                    "apps/win32_init.c",
                ],
            "apps/x509.o" =>
                [
                    "apps/x509.c",
                ],
            "crypto/aes/aes-586.o" =>
                [
                    "crypto/aes/aes-586.s",
                ],
            "crypto/aes/aes_cfb.o" =>
                [
                    "crypto/aes/aes_cfb.c",
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
            "crypto/des/rpc_enc.o" =>
                [
                    "crypto/des/rpc_enc.c",
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
            "crypto/dllmain.o" =>
                [
                    "crypto/dllmain.c",
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
            "crypto/ec/ec2_mult.o" =>
                [
                    "crypto/ec/ec2_mult.c",
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
            "crypto/engine/eng_cryptodev.o" =>
                [
                    "crypto/engine/eng_cryptodev.c",
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
            "crypto/evp/scrypt.o" =>
                [
                    "crypto/evp/scrypt.c",
                ],
            "crypto/ex_data.o" =>
                [
                    "crypto/ex_data.c",
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
            "crypto/rand/md_rand.o" =>
                [
                    "crypto/rand/md_rand.c",
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
            "crypto/rsa/rsa_none.o" =>
                [
                    "crypto/rsa/rsa_none.c",
                ],
            "crypto/rsa/rsa_null.o" =>
                [
                    "crypto/rsa/rsa_null.c",
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
                    "crypto/aes/aes-586.o",
                    "crypto/aes/aes_cfb.o",
                    "crypto/aes/aes_ecb.o",
                    "crypto/aes/aes_ige.o",
                    "crypto/aes/aes_misc.o",
                    "crypto/aes/aes_ofb.o",
                    "crypto/aes/aes_wrap.o",
                    "crypto/aes/aesni-x86.o",
                    "crypto/aes/vpaes-x86.o",
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
                    "crypto/des/rpc_enc.o",
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
                    "crypto/ec/ec2_mult.o",
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
                    "crypto/engine/eng_cryptodev.o",
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
                    "crypto/evp/pmeth_fn.o",
                    "crypto/evp/pmeth_gn.o",
                    "crypto/evp/pmeth_lib.o",
                    "crypto/evp/scrypt.o",
                    "crypto/ex_data.o",
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
                    "crypto/rand/md_rand.o",
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
                    "crypto/rsa/rsa_none.o",
                    "crypto/rsa/rsa_null.o",
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
                    "crypto/sha/sha1-586.o",
                    "crypto/sha/sha1_one.o",
                    "crypto/sha/sha1dgst.o",
                    "crypto/sha/sha256-586.o",
                    "crypto/sha/sha256.o",
                    "crypto/sha/sha512-586.o",
                    "crypto/sha/sha512.o",
                    "crypto/srp/srp_lib.o",
                    "crypto/srp/srp_vfy.o",
                    "crypto/stack/stack.o",
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
                    "ssl/pqueue.o",
                    "ssl/record/dtls1_bitmap.o",
                    "ssl/record/rec_layer_d1.o",
                    "ssl/record/rec_layer_s3.o",
                    "ssl/record/ssl3_buffer.o",
                    "ssl/record/ssl3_record.o",
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
                    "ssl/statem/statem.o",
                    "ssl/statem/statem_clnt.o",
                    "ssl/statem/statem_dtls.o",
                    "ssl/statem/statem_lib.o",
                    "ssl/statem/statem_srvr.o",
                    "ssl/t1_enc.o",
                    "ssl/t1_ext.o",
                    "ssl/t1_lib.o",
                    "ssl/t1_reneg.o",
                    "ssl/t1_trce.o",
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
            "ssl/t1_ext.o" =>
                [
                    "ssl/t1_ext.c",
                ],
            "ssl/t1_lib.o" =>
                [
                    "ssl/t1_lib.c",
                ],
            "ssl/t1_reneg.o" =>
                [
                    "ssl/t1_reneg.c",
                ],
            "ssl/t1_trce.o" =>
                [
                    "ssl/t1_trce.c",
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
            "test/bio_enc_test" =>
                [
                    "test/bio_enc_test.o",
                ],
            "test/bio_enc_test.o" =>
                [
                    "test/bio_enc_test.c",
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
            "test/buildtest_aes" =>
                [
                    "test/buildtest_aes.o",
                ],
            "test/buildtest_aes.o" =>
                [
                    "test/buildtest_aes.c",
                ],
            "test/buildtest_asn1" =>
                [
                    "test/buildtest_asn1.o",
                ],
            "test/buildtest_asn1.o" =>
                [
                    "test/buildtest_asn1.c",
                ],
            "test/buildtest_asn1t" =>
                [
                    "test/buildtest_asn1t.o",
                ],
            "test/buildtest_asn1t.o" =>
                [
                    "test/buildtest_asn1t.c",
                ],
            "test/buildtest_async" =>
                [
                    "test/buildtest_async.o",
                ],
            "test/buildtest_async.o" =>
                [
                    "test/buildtest_async.c",
                ],
            "test/buildtest_bio" =>
                [
                    "test/buildtest_bio.o",
                ],
            "test/buildtest_bio.o" =>
                [
                    "test/buildtest_bio.c",
                ],
            "test/buildtest_blowfish" =>
                [
                    "test/buildtest_blowfish.o",
                ],
            "test/buildtest_blowfish.o" =>
                [
                    "test/buildtest_blowfish.c",
                ],
            "test/buildtest_bn" =>
                [
                    "test/buildtest_bn.o",
                ],
            "test/buildtest_bn.o" =>
                [
                    "test/buildtest_bn.c",
                ],
            "test/buildtest_buffer" =>
                [
                    "test/buildtest_buffer.o",
                ],
            "test/buildtest_buffer.o" =>
                [
                    "test/buildtest_buffer.c",
                ],
            "test/buildtest_camellia" =>
                [
                    "test/buildtest_camellia.o",
                ],
            "test/buildtest_camellia.o" =>
                [
                    "test/buildtest_camellia.c",
                ],
            "test/buildtest_cast" =>
                [
                    "test/buildtest_cast.o",
                ],
            "test/buildtest_cast.o" =>
                [
                    "test/buildtest_cast.c",
                ],
            "test/buildtest_cmac" =>
                [
                    "test/buildtest_cmac.o",
                ],
            "test/buildtest_cmac.o" =>
                [
                    "test/buildtest_cmac.c",
                ],
            "test/buildtest_cms" =>
                [
                    "test/buildtest_cms.o",
                ],
            "test/buildtest_cms.o" =>
                [
                    "test/buildtest_cms.c",
                ],
            "test/buildtest_conf" =>
                [
                    "test/buildtest_conf.o",
                ],
            "test/buildtest_conf.o" =>
                [
                    "test/buildtest_conf.c",
                ],
            "test/buildtest_conf_api" =>
                [
                    "test/buildtest_conf_api.o",
                ],
            "test/buildtest_conf_api.o" =>
                [
                    "test/buildtest_conf_api.c",
                ],
            "test/buildtest_crypto" =>
                [
                    "test/buildtest_crypto.o",
                ],
            "test/buildtest_crypto.o" =>
                [
                    "test/buildtest_crypto.c",
                ],
            "test/buildtest_ct" =>
                [
                    "test/buildtest_ct.o",
                ],
            "test/buildtest_ct.o" =>
                [
                    "test/buildtest_ct.c",
                ],
            "test/buildtest_des" =>
                [
                    "test/buildtest_des.o",
                ],
            "test/buildtest_des.o" =>
                [
                    "test/buildtest_des.c",
                ],
            "test/buildtest_dh" =>
                [
                    "test/buildtest_dh.o",
                ],
            "test/buildtest_dh.o" =>
                [
                    "test/buildtest_dh.c",
                ],
            "test/buildtest_dsa" =>
                [
                    "test/buildtest_dsa.o",
                ],
            "test/buildtest_dsa.o" =>
                [
                    "test/buildtest_dsa.c",
                ],
            "test/buildtest_dtls1" =>
                [
                    "test/buildtest_dtls1.o",
                ],
            "test/buildtest_dtls1.o" =>
                [
                    "test/buildtest_dtls1.c",
                ],
            "test/buildtest_e_os2" =>
                [
                    "test/buildtest_e_os2.o",
                ],
            "test/buildtest_e_os2.o" =>
                [
                    "test/buildtest_e_os2.c",
                ],
            "test/buildtest_ebcdic" =>
                [
                    "test/buildtest_ebcdic.o",
                ],
            "test/buildtest_ebcdic.o" =>
                [
                    "test/buildtest_ebcdic.c",
                ],
            "test/buildtest_ec" =>
                [
                    "test/buildtest_ec.o",
                ],
            "test/buildtest_ec.o" =>
                [
                    "test/buildtest_ec.c",
                ],
            "test/buildtest_ecdh" =>
                [
                    "test/buildtest_ecdh.o",
                ],
            "test/buildtest_ecdh.o" =>
                [
                    "test/buildtest_ecdh.c",
                ],
            "test/buildtest_ecdsa" =>
                [
                    "test/buildtest_ecdsa.o",
                ],
            "test/buildtest_ecdsa.o" =>
                [
                    "test/buildtest_ecdsa.c",
                ],
            "test/buildtest_engine" =>
                [
                    "test/buildtest_engine.o",
                ],
            "test/buildtest_engine.o" =>
                [
                    "test/buildtest_engine.c",
                ],
            "test/buildtest_err" =>
                [
                    "test/buildtest_err.o",
                ],
            "test/buildtest_err.o" =>
                [
                    "test/buildtest_err.c",
                ],
            "test/buildtest_evp" =>
                [
                    "test/buildtest_evp.o",
                ],
            "test/buildtest_evp.o" =>
                [
                    "test/buildtest_evp.c",
                ],
            "test/buildtest_hmac" =>
                [
                    "test/buildtest_hmac.o",
                ],
            "test/buildtest_hmac.o" =>
                [
                    "test/buildtest_hmac.c",
                ],
            "test/buildtest_idea" =>
                [
                    "test/buildtest_idea.o",
                ],
            "test/buildtest_idea.o" =>
                [
                    "test/buildtest_idea.c",
                ],
            "test/buildtest_kdf" =>
                [
                    "test/buildtest_kdf.o",
                ],
            "test/buildtest_kdf.o" =>
                [
                    "test/buildtest_kdf.c",
                ],
            "test/buildtest_lhash" =>
                [
                    "test/buildtest_lhash.o",
                ],
            "test/buildtest_lhash.o" =>
                [
                    "test/buildtest_lhash.c",
                ],
            "test/buildtest_md4" =>
                [
                    "test/buildtest_md4.o",
                ],
            "test/buildtest_md4.o" =>
                [
                    "test/buildtest_md4.c",
                ],
            "test/buildtest_md5" =>
                [
                    "test/buildtest_md5.o",
                ],
            "test/buildtest_md5.o" =>
                [
                    "test/buildtest_md5.c",
                ],
            "test/buildtest_mdc2" =>
                [
                    "test/buildtest_mdc2.o",
                ],
            "test/buildtest_mdc2.o" =>
                [
                    "test/buildtest_mdc2.c",
                ],
            "test/buildtest_modes" =>
                [
                    "test/buildtest_modes.o",
                ],
            "test/buildtest_modes.o" =>
                [
                    "test/buildtest_modes.c",
                ],
            "test/buildtest_obj_mac" =>
                [
                    "test/buildtest_obj_mac.o",
                ],
            "test/buildtest_obj_mac.o" =>
                [
                    "test/buildtest_obj_mac.c",
                ],
            "test/buildtest_objects" =>
                [
                    "test/buildtest_objects.o",
                ],
            "test/buildtest_objects.o" =>
                [
                    "test/buildtest_objects.c",
                ],
            "test/buildtest_ocsp" =>
                [
                    "test/buildtest_ocsp.o",
                ],
            "test/buildtest_ocsp.o" =>
                [
                    "test/buildtest_ocsp.c",
                ],
            "test/buildtest_opensslv" =>
                [
                    "test/buildtest_opensslv.o",
                ],
            "test/buildtest_opensslv.o" =>
                [
                    "test/buildtest_opensslv.c",
                ],
            "test/buildtest_ossl_typ" =>
                [
                    "test/buildtest_ossl_typ.o",
                ],
            "test/buildtest_ossl_typ.o" =>
                [
                    "test/buildtest_ossl_typ.c",
                ],
            "test/buildtest_pem" =>
                [
                    "test/buildtest_pem.o",
                ],
            "test/buildtest_pem.o" =>
                [
                    "test/buildtest_pem.c",
                ],
            "test/buildtest_pem2" =>
                [
                    "test/buildtest_pem2.o",
                ],
            "test/buildtest_pem2.o" =>
                [
                    "test/buildtest_pem2.c",
                ],
            "test/buildtest_pkcs12" =>
                [
                    "test/buildtest_pkcs12.o",
                ],
            "test/buildtest_pkcs12.o" =>
                [
                    "test/buildtest_pkcs12.c",
                ],
            "test/buildtest_pkcs7" =>
                [
                    "test/buildtest_pkcs7.o",
                ],
            "test/buildtest_pkcs7.o" =>
                [
                    "test/buildtest_pkcs7.c",
                ],
            "test/buildtest_rand" =>
                [
                    "test/buildtest_rand.o",
                ],
            "test/buildtest_rand.o" =>
                [
                    "test/buildtest_rand.c",
                ],
            "test/buildtest_rc2" =>
                [
                    "test/buildtest_rc2.o",
                ],
            "test/buildtest_rc2.o" =>
                [
                    "test/buildtest_rc2.c",
                ],
            "test/buildtest_rc4" =>
                [
                    "test/buildtest_rc4.o",
                ],
            "test/buildtest_rc4.o" =>
                [
                    "test/buildtest_rc4.c",
                ],
            "test/buildtest_ripemd" =>
                [
                    "test/buildtest_ripemd.o",
                ],
            "test/buildtest_ripemd.o" =>
                [
                    "test/buildtest_ripemd.c",
                ],
            "test/buildtest_rsa" =>
                [
                    "test/buildtest_rsa.o",
                ],
            "test/buildtest_rsa.o" =>
                [
                    "test/buildtest_rsa.c",
                ],
            "test/buildtest_safestack" =>
                [
                    "test/buildtest_safestack.o",
                ],
            "test/buildtest_safestack.o" =>
                [
                    "test/buildtest_safestack.c",
                ],
            "test/buildtest_seed" =>
                [
                    "test/buildtest_seed.o",
                ],
            "test/buildtest_seed.o" =>
                [
                    "test/buildtest_seed.c",
                ],
            "test/buildtest_sha" =>
                [
                    "test/buildtest_sha.o",
                ],
            "test/buildtest_sha.o" =>
                [
                    "test/buildtest_sha.c",
                ],
            "test/buildtest_srp" =>
                [
                    "test/buildtest_srp.o",
                ],
            "test/buildtest_srp.o" =>
                [
                    "test/buildtest_srp.c",
                ],
            "test/buildtest_srtp" =>
                [
                    "test/buildtest_srtp.o",
                ],
            "test/buildtest_srtp.o" =>
                [
                    "test/buildtest_srtp.c",
                ],
            "test/buildtest_ssl" =>
                [
                    "test/buildtest_ssl.o",
                ],
            "test/buildtest_ssl.o" =>
                [
                    "test/buildtest_ssl.c",
                ],
            "test/buildtest_ssl2" =>
                [
                    "test/buildtest_ssl2.o",
                ],
            "test/buildtest_ssl2.o" =>
                [
                    "test/buildtest_ssl2.c",
                ],
            "test/buildtest_stack" =>
                [
                    "test/buildtest_stack.o",
                ],
            "test/buildtest_stack.o" =>
                [
                    "test/buildtest_stack.c",
                ],
            "test/buildtest_symhacks" =>
                [
                    "test/buildtest_symhacks.o",
                ],
            "test/buildtest_symhacks.o" =>
                [
                    "test/buildtest_symhacks.c",
                ],
            "test/buildtest_tls1" =>
                [
                    "test/buildtest_tls1.o",
                ],
            "test/buildtest_tls1.o" =>
                [
                    "test/buildtest_tls1.c",
                ],
            "test/buildtest_ts" =>
                [
                    "test/buildtest_ts.o",
                ],
            "test/buildtest_ts.o" =>
                [
                    "test/buildtest_ts.c",
                ],
            "test/buildtest_txt_db" =>
                [
                    "test/buildtest_txt_db.o",
                ],
            "test/buildtest_txt_db.o" =>
                [
                    "test/buildtest_txt_db.c",
                ],
            "test/buildtest_ui" =>
                [
                    "test/buildtest_ui.o",
                ],
            "test/buildtest_ui.o" =>
                [
                    "test/buildtest_ui.c",
                ],
            "test/buildtest_whrlpool" =>
                [
                    "test/buildtest_whrlpool.o",
                ],
            "test/buildtest_whrlpool.o" =>
                [
                    "test/buildtest_whrlpool.c",
                ],
            "test/buildtest_x509" =>
                [
                    "test/buildtest_x509.o",
                ],
            "test/buildtest_x509.o" =>
                [
                    "test/buildtest_x509.c",
                ],
            "test/buildtest_x509_vfy" =>
                [
                    "test/buildtest_x509_vfy.o",
                ],
            "test/buildtest_x509_vfy.o" =>
                [
                    "test/buildtest_x509_vfy.c",
                ],
            "test/buildtest_x509v3" =>
                [
                    "test/buildtest_x509v3.o",
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
            "test/cipherlist_test" =>
                [
                    "test/cipherlist_test.o",
                    "test/testutil.o",
                ],
            "test/cipherlist_test.o" =>
                [
                    "test/cipherlist_test.c",
                ],
            "test/clienthellotest" =>
                [
                    "test/clienthellotest.o",
                ],
            "test/clienthellotest.o" =>
                [
                    "test/clienthellotest.c",
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
                    "test/testutil.o",
                ],
            "test/crltest.o" =>
                [
                    "test/crltest.c",
                ],
            "test/ct_test" =>
                [
                    "test/ct_test.o",
                    "test/testutil.o",
                ],
            "test/ct_test.o" =>
                [
                    "test/ct_test.c",
                ],
            "test/d2i_test" =>
                [
                    "test/d2i_test.o",
                    "test/testutil.o",
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
            "test/dsatest" =>
                [
                    "test/dsatest.o",
                ],
            "test/dsatest.o" =>
                [
                    "test/dsatest.c",
                ],
            "test/dtlstest" =>
                [
                    "test/dtlstest.o",
                    "test/ssltestlib.o",
                    "test/testutil.o",
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
            "test/ecdsatest" =>
                [
                    "test/ecdsatest.o",
                ],
            "test/ecdsatest.o" =>
                [
                    "test/ecdsatest.c",
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
                    "test/testutil.o",
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
            "test/handshake_helper.o" =>
                [
                    "test/handshake_helper.c",
                ],
            "test/heartbeat_test" =>
                [
                    "test/heartbeat_test.o",
                    "test/testutil.o",
                ],
            "test/heartbeat_test.o" =>
                [
                    "test/heartbeat_test.c",
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
            "test/md2test" =>
                [
                    "test/md2test.o",
                ],
            "test/md2test.o" =>
                [
                    "test/md2test.c",
                ],
            "test/md4test" =>
                [
                    "test/md4test.o",
                ],
            "test/md4test.o" =>
                [
                    "test/md4test.c",
                ],
            "test/md5test" =>
                [
                    "test/md5test.o",
                ],
            "test/md5test.o" =>
                [
                    "test/md5test.c",
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
            "test/ocspapitest" =>
                [
                    "test/ocspapitest.o",
                    "test/testutil.o",
                ],
            "test/ocspapitest.o" =>
                [
                    "test/ocspapitest.c",
                ],
            "test/p5_crpt2_test" =>
                [
                    "test/p5_crpt2_test.o",
                ],
            "test/p5_crpt2_test.o" =>
                [
                    "test/p5_crpt2_test.c",
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
            "test/randtest" =>
                [
                    "test/randtest.o",
                ],
            "test/randtest.o" =>
                [
                    "test/randtest.c",
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
            "test/rmdtest" =>
                [
                    "test/rmdtest.o",
                ],
            "test/rmdtest.o" =>
                [
                    "test/rmdtest.c",
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
            "test/sha1test" =>
                [
                    "test/sha1test.o",
                ],
            "test/sha1test.o" =>
                [
                    "test/sha1test.c",
                ],
            "test/sha256t" =>
                [
                    "test/sha256t.o",
                ],
            "test/sha256t.o" =>
                [
                    "test/sha256t.c",
                ],
            "test/sha512t" =>
                [
                    "test/sha512t.o",
                ],
            "test/sha512t.o" =>
                [
                    "test/sha512t.c",
                ],
            "test/srptest" =>
                [
                    "test/srptest.o",
                ],
            "test/srptest.o" =>
                [
                    "test/srptest.c",
                ],
            "test/ssl_test" =>
                [
                    "test/handshake_helper.o",
                    "test/ssl_test.o",
                    "test/ssl_test_ctx.o",
                    "test/testutil.o",
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
                    "test/testutil.o",
                ],
            "test/ssl_test_ctx_test.o" =>
                [
                    "test/ssl_test_ctx_test.c",
                ],
            "test/sslapitest" =>
                [
                    "test/sslapitest.o",
                    "test/ssltestlib.o",
                    "test/testutil.o",
                ],
            "test/sslapitest.o" =>
                [
                    "test/sslapitest.c",
                ],
            "test/sslcorrupttest" =>
                [
                    "test/sslcorrupttest.o",
                    "test/ssltestlib.o",
                    "test/testutil.o",
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
            "test/testutil.o" =>
                [
                    "test/testutil.c",
                ],
            "test/threadstest" =>
                [
                    "test/threadstest.o",
                ],
            "test/threadstest.o" =>
                [
                    "test/threadstest.c",
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
            "test/wp_test" =>
                [
                    "test/wp_test.o",
                ],
            "test/wp_test.o" =>
                [
                    "test/wp_test.c",
                ],
            "test/x509aux" =>
                [
                    "test/x509aux.o",
                ],
            "test/x509aux.o" =>
                [
                    "test/x509aux.c",
                ],
            "tools/c_rehash.pl" =>
                [
                    "tools/c_rehash.in",
                ],
        },
);

1;
