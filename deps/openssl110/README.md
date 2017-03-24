This has a new binding scheme in builing OpenSSL-1.1.0 library with
Node. OpenSSL-1.1.0 uses a new build system with perl for various
supported platforms. See `Configurations/README` and
`Configurations/README.design` in the OpenSSL source for details.

In order to build OpenSSL library without perl in the build of Node
for various supported platforms, platform dependent files (e.g. asm
and header files ) are pre-generated and stored into the
`config/archs` directory.

- `config/Makefile` and `config/generate.pl`

  Makefile has supported platform list and generates and copies
  platform dependent files (e.g. asm files) into arch directory with
  generate.pl.  Platform dependent gypi files also created obtaining
  build information from `configdata.pm` that is generated with
  `Configure` in the OpenSSL build system.

  For Windows, `Configure` generates makefile that is only available to
  nmake command.  `config/Makefile_VC-WIN32` and
  `config/Makefile_VC-WIN64A` are made created by hand for the use of
  GNU make. If make rules or targets are changed in the version up of
  OpenSSL, they should be also updated.

  Theses are usually used in upgrading openssl-1.1.0.

- gyp and gypi files (`openssl*.{gyp,gypi}`)

  `openssl.gyp` has two targets of openssl and openssl-cli referred
  from `node.gyp`. They includes asm and no_asm gypi files with arch
  dependent gypi according to its build options and platforms . The
  gyp data which is common with asm and no_asm are stored in
  openssl_common.gypi.

- header files (`config/*.{h,h.tmpl}`)

  `bn_conf.h`, `dso_conf.h` and `opensslconf.h` are platform dependent
  in the OpenSSL sources. They are replaced with `config/*.h.tmpl`
  files to include the file in the `../../../config/` and referred to
  each arch files that depends on asm and no-asm option.

### Upgrading OpenSSL

Please refer [config/README.md](config/README.md) .