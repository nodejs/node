This has a new binding scheme in building OpenSSL-1.1.0 library with
Node.js. OpenSSL-1.1.0 uses a new build system with `Perl` for various
supported platforms. See `openssl/Configurations/README` and
`openssl/Configurations/README.design` in the OpenSSL source for
details.

In order to build OpenSSL library without `Perl` in the build of Node.js
for various supported platforms, platform dependent files (e.g. asm
and header files ) are pre-generated and stored into the
`config/archs` directory.

- `config/Makefile` and `config/generate_gypi.pl`

  Makefile has supported platform list and generates and copies
  platform dependent files (e.g. asm files) into arch directory with
  `generate_gypi.pl`.  Platform dependent gypi files also created
  obtaining build information from `configdata.pm` that is generated
  with `Configure` in the OpenSSL build system.

  For Windows, `Configure` generates `makefile` that is only available
  to `nmake` command.  `config/Makefile_VC-WIN32` and
  `config/Makefile_VC-WIN64A` are made by hand for the use of GNU
  make. If `makefile` rules or targets are changed in the version up
  of OpenSSL, they should be also updated.

- gyp and gypi files (`openssl*.{gyp,gypi}`)

  `openssl.gyp` has two targets of openssl and openssl-cli referred
  from `node.gyp`. They include asm and no_asm gypi files with arch
  dependent gypi according to its build options and platforms. The
  gyp data which is common with asm and no_asm are stored in
  `openssl_common.gypi`.

- header files (`config/*.{h,h.tmpl}`)

  `bn_conf.h`, `dso_conf.h` and `opensslconf.h` are platform dependent
  in the OpenSSL sources. They are replaced with `config/*.h.tmpl`
  files to include the file in the `../../../config/` and referred to
  each arch file that depends on asm and no-asm option.

### Supported architectures for use of ASM

Here is a list of supported architectures for use of ASM in OpenSSL.

  | --dest-os | --dest-cpu | OpenSSL target arch  | CI  |
  | --------- | ---------- | -------------------- | --- |
  | aix       | ppc        | aix-gcc              | o   |
  | aix       | ppc64      | aix64-gcc            | o   |
  | linux     | ia32       | linux-elf            | o   |
  | linux     | x32        | linux-x32            | -   |
  | linux     | x64        | linux-x86_64         | o   |
  | linux     | arm        | linux-armv4          | o   |
  | linux     | arm64      | linux-aarch64        | o   |
  | linux     | ppc        | linux-ppc            | o   |
  | linux     | ppc64      | linux-ppc64          | o   |
  | linux     | ppc64(*1)  | linux-ppc64le        | o   |
  | linux     | s390       | linux32-s390x        | o   |
  | linux     | s390x      | linux64-s390x        | o   |
  | mac       | ia32       | darwin-i386-cc       | -   |
  | mac       | x64        | darwin64-x86-cc      | o   |
  | win       | ia32       | VC-WIN32             | -   |
  | win       | x64        | VC-WIN64A            | o   |
  | solaris   | ia32       | solaris-x86-gcc      | o   |
  | solaris   | x64        | solaris64-x86_64-gcc | o   |
  | freebsd   | ia32       | BSD-x86              | -   |
  | freebsd   | x64        | BSD-x86_64           | o   |
  | openbsd   | ia32       | BSD-x86              | -   |
  | openbsd   | x64        | BSD-x86_64           | -   |
  | others    | others     | linux-elf            | -   |

(*1: This needs to be configured with the variable of node_byteorder:
little)

These are listed in [config/Makefile](config/Makefile).
Please refer [config/opensslconf_asm.h](config/opensslconf_asm.h) for details.

### Upgrading OpenSSL

Please refer to [maintaining-openssl](../../doc/guides/maintaining-openssl.md).
