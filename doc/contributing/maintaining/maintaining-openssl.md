# Maintaining OpenSSL

OpenSSL is automatically updated by the [update-openssl-action][].
There is also a script in `tools/dep_updaters` that can be used to update it.
This document describes how to manually update `deps/openssl/`.

## Requirements

* Linux environment.
* `perl` Only Perl version 5 is tested.
* `nasm` (<https://www.nasm.us/>) Version 2.11 or higher is needed.
* GNU `as` in binutils. Version 2.26 or higher is needed.

## 0. Check requirements

```console
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

Get the sources for the desired `openssl-<version>` tag from
<https://github.com/openssl/openssl> and copy all files into
`deps/openssl/openssl`. Then add all files and commit them.

```bash
git clone https://github.com/openssl/openssl
cd openssl
git checkout openssl-<version>
cd ../node/deps/openssl
rm -rf openssl
cp -R ../../../openssl openssl
rm -rf openssl/.git
git add --all openssl
git commit openssl
```

```text
deps: upgrade openssl sources to openssl-<version>

This updates all sources in deps/openssl/openssl by:
    $ git clone git@github.com:openssl/openssl.git
    $ cd openssl
    $ git checkout openssl-<version>
    $ cd ../node/deps/openssl
    $ rm -rf openssl
    $ cp -R ../../../openssl openssl
    $ rm -rf openssl/.git
    $ git add --all openssl
    $ git commit openssl
```

## 2. Execute `make` in `deps/openssl/config` directory

Fix up 32-bit Windows assembler directives. This will allow the commits to be
cherry-picked to older release lines that still provide binaries on 32-bit Windows.

```bash
# Edit deps/openssl/openssl/crypto/perlasm/x86asm.pl changing
# #ifdef/#endif to %ifdef/%endif for nasm on 32-bit Windows.
# See: https://github.com/nodejs/node/pull/43603#issuecomment-1170670844
# Reference: https://github.com/openssl/openssl/issues/18459
```

Use `make` to regenerate all platform dependent files in
`deps/openssl/config/archs/`:

```bash
# On non-Linux machines
make gen-openssl

# On Linux machines
make -C deps/openssl/config clean
make -C deps/openssl/config
```

## 3. Check diffs

Check diffs to ensure updates are right. Even if there are no updates in openssl
sources, `buildinf.h` files will be updated because they have timestamp
data in them.

```bash
git diff -- deps/openssl
```

_Note_: On Windows, OpenSSL Configure generates a `makefile` that can be
used for the `nmake` command. The `make` command in step 2 (above) uses
`Makefile_VC-WIN64A`, `Makefile_VC-WIN32`, and `Makefile_VC-WIN64-ARM`,
which are manually created. When source files or build options are updated in
Windows, these Makefiles need to be changed by hand. If you are not sure,
please ask @shigeki for details.

## 4. Commit and test

Update all architecture dependent files. Do not forget to git add or remove
files if they are changed before committing:

```bash
git add -A deps/openssl/config/archs
git add -A deps/openssl/openssl
git commit
```

The commit message can be written as (with the openssl version set
to the relevant value):

```text
deps: update archs files for openssl-<version>

After an OpenSSL source update, all the config files need to be
regenerated and committed by:
    $ make -C deps/openssl/config
    $ git add -A deps/openssl/config/archs
    $ git add -A deps/openssl/openssl
    $ git commit
```

Finally, build Node.js with the bundled OpenSSL and run the tests.

[update-openssl-action]: ../../../.github/workflows/update-openssl.yml
