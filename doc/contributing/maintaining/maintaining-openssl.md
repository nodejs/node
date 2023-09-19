# Maintaining OpenSSL

OpenSSL is automatically updated by the [update-openssl-action][].
There is also a script in `tools/dep_updaters` that can be used to update it.
This document describes how to manually update `deps/openssl/`.

If you need to provide updates across all active release lines you will
currently need to generate four PRs as follows:

* a PR for `main` which is generated following the instructions
  below for OpenSSL 3.x.x.
* a PR for 18.x following the instructions in the v18.x-staging version
  of this guide.
* a PR for 16.x following the instructions in the v16.x-staging version
  of this guide.

## Use of the quictls/openssl fork

Node.js currently uses the quictls/openssl fork, which closely tracks
the main openssl/openssl releases with the addition of APIs to support
the QUIC protocol.

Details on the fork, as well as the latest sources, can be found at
<https://github.com/quictls/openssl>.

Branches are used per OpenSSL version (for instance,
<https://github.com/quictls/openssl/tree/OpenSSL_1_1_1j+quic>).

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

Get a new source from <https://github.com/quictls/openssl/tree/openssl-3.0.5+quic>
and copy all files into `deps/openssl/openssl`. Then add all files and commit
them. (The link above, and the branch, will change with each new OpenSSL
release).

### OpenSSL 3.x.x

```bash
git clone https://github.com/quictls/openssl
cd openssl
cd ../node/deps/openssl
rm -rf openssl
cp -R ../../../openssl openssl
rm -rf openssl/.git* openssl/.travis*
git add --all openssl
git commit openssl
```

```text
deps: upgrade openssl sources to quictls/openssl-3.0.5+quic

This updates all sources in deps/openssl/openssl by:
    $ git clone git@github.com:quictls/openssl.git
    $ cd openssl
    $ git checkout openssl-3.0.5+quic
    $ cd ../node/deps/openssl
    $ rm -rf openssl
    $ cp -R ../../../openssl openssl
    $ rm -rf openssl/.git* openssl/.travis*
    $ git add --all openssl
    $ git commit openssl
```

## 2. Execute `make` in `deps/openssl/config` directory

Use `make` to regenerate all platform dependent files in
`deps/openssl/config/archs/`:

```bash
# On non-Linux machines
make gen-openssl

# On Linux machines
make -C deps/openssl/config clean
make -C deps/openssl/config
```

**Note**: If the 32-bit Windows is failing to compile run this workflow instead:

```bash
make -C deps/openssl/config clean
# Edit deps/openssl/openssl/crypto/perlasm/x86asm.pl changing
# #ifdef to %ifdef to make it compatible to nasm on 32-bit Windows.
# See: https://github.com/nodejs/node/pull/43603#issuecomment-1170670844
# Reference: https://github.com/openssl/openssl/issues/18459
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
`Makefile_VC-WIN64A` and `Makefile_VC-WIN32` that are manually
created. When source files or build options are updated in Windows,
it needs to change these two Makefiles by hand. If you are not sure,
please ask @shigeki for details.

## 4. Commit and make test

Update all architecture dependent files. Do not forget to git add or remove
files if they are changed before committing:

```bash
git add deps/openssl/config/archs
git add deps/openssl/openssl
git commit
```

The commit message can be written as (with the openssl version set
to the relevant value):

### OpenSSL 3.x.x

```text
deps: update archs files for quictls/openssl-3.0.5+quic

After an OpenSSL source update, all the config files need to be
regenerated and committed by:
    $ make -C deps/openssl/config
    $ git add deps/openssl/config/archs
    $ git add deps/openssl/openssl
    $ git commit
```

Finally, build Node.js and run the tests.

[update-openssl-action]: ../../../.github/workflows/update-openssl.yml
