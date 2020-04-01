# Maintaining OpenSSL

This document describes how to update `deps/openssl/`.

## Requirements
* Linux environment
* `perl` Only Perl version 5 is tested.
* `nasm` (http://www.nasm.us/)  The version of 2.11 or higher is needed.
* GNU `as` in binutils. The version of 2.26 or higher is needed.

## 0. Check Requirements

```sh
% perl -v

This is perl 5, version 22, subversion 1 (v5.22.1) built for
x86_64-linux-gnu-thread-multi
(with 60 registered patches, see perl -V for more detail)

% as --version
GNU assembler (GNU Binutils for Ubuntu) 2.26.1
Copyright (C) 2015 Free Software Foundation, Inc.
...
% nasm -v
NASM version 2.11.08
```

## 1. Obtain and extract new OpenSSL sources

Get a new source from  https://www.openssl.org/source/ and extract
all files into `deps/openssl/openssl`. Then add all files and commit
them.
```sh
% cd deps/openssl/
% rm -rf openssl
% tar zxf ~/tmp/openssl-1.1.0h.tar.gz
% mv openssl-1.1.0h openssl
% git add --all openssl
% git commit openssl
````

The commit message can be (with the openssl version set to the relevant value):
```text
deps: upgrade openssl sources to 1.1.0h

This updates all sources in deps/openssl/openssl by:
    $ cd deps/openssl/
    $ rm -rf openssl
    $ tar zxf ~/tmp/openssl-1.1.0h.tar.gz
    $ mv openssl-1.1.0h openssl
    $ git add --all openssl
    $ git commit openssl
```

## 2. Execute `make` in `deps/openssl/config` directory

Use `make` to regenerate all platform dependent files in
`deps/openssl/config/archs/`:
```sh
% cd deps/openssl/config; make
```

## 3. Check diffs

Check diffs if updates are right. Even if no updates in openssl
sources, `buildinf.h` files will be updated for they have a timestamp
data in them.
```sh
% cd deps/openssl/config
% git diff
```

*Note*: On Windows, OpenSSL Configure generates `makefile` that can be
used for `nmake` command. The `make` command in the step 2 above uses
 `Makefile_VC-WIN64A` and `Makefile_VC-WIN32` that are manually
created. When source files or build options are updated in Windows,
it needs to change these two Makefiles by hand. If you are not sure,
please ask @shigeki for details.

## 4. Commit and make test

Update all architecture dependent files. Do not forget to git add or remove
files if they are changed before commit:
```sh
% git add deps/openssl/config/archs
% git add deps/openssl/openssl/include/crypto/bn_conf.h
% git add deps/openssl/openssl/include/crypto/dso_conf.h
% git add deps/openssl/openssl/include/openssl/opensslconf.h
% git commit
```

The commit message can be (with the openssl version set to the relevant value):
```text
 deps: update archs files for OpenSSL-1.1.0

 After an OpenSSL source update, all the config files need to be
 regenerated and committed by:
    $ cd deps/openssl/config
    $ make
    $ git add deps/openssl/config/archs
    $ git add deps/openssl/openssl/include/crypto/bn_conf.h
    $ git add deps/openssl/openssl/include/crypto/dso_conf.h
    $ git add deps/openssl/openssl/include/openssl/opensslconf.h
    $ git commit
```

Finally, build Node.js and run tests.
