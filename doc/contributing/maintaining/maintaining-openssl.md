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

Get a new source from <https://github.com/openssl/openssl/tree/openssl-3.0.16>
and copy all files into `deps/openssl/openssl`. Then add all files and commit
them. (The link above, and the branch, will change with each new OpenSSL
release).

```bash
git clone https://github.com/openssl/openssl
cd openssl
cd ../node/deps/openssl
rm -rf openssl
cp -R ../../../openssl openssl
rm -rf openssl/.git*
git add --all openssl
git commit openssl
```

```text
deps: upgrade openssl sources to openssl-3.0.16

This updates all sources in deps/openssl/openssl by:
    $ git clone git@github.com:openssl/openssl.git
    $ cd openssl
    $ git checkout openssl-3.0.16
    $ cd ../node/deps/openssl
    $ rm -rf openssl
    $ cp -R ../../../openssl openssl
    $ rm -rf openssl/.git*
    $ git add --all openssl
    $ git commit openssl
```

## 2. Fix up assembler directives for 32-bit Windows.

This will allow the commits to be cherry-picked to older release lines that
still provide binaries on 32-bit Windows.

Edit `deps/openssl/openssl/crypto/perlasm/x86asm.pl` to use nasm-style `%ifdef`, but only
for win32. This endbranch subroutine can be used as a workaround to enable different
ifdef styles for different x86 systems:

```perl
sub ::endbranch
{
    my $ifdef = "#ifdef";
    my $endif = "#endif";
    if ($::win32) { $ifdef = "%ifdef"; $endif = "%endif"; }
    &::generic("$ifdef __CET__\n");
    &::data_byte(0xf3,0x0f,0x1e,0xfb);
    &::generic("$endif\n");
}
```

OpenSSL official build procedure is using the C preprocessor before the assembler
(which is nasm in win32 context).

More about the nasm-style `%ifdef` and gcc-style `#ifdef` issues:

* <https://github.com/openssl/openssl/issues/18459>
* <https://github.com/nodejs/node/pull/43603#issuecomment-1170670844>
* <https://github.com/nodejs/node/issues/44822>

## 3. Execute `make` in `deps/openssl/config` directory

Use `make` to regenerate all platform dependent files in
`deps/openssl/config/archs/`:

```bash
# On non-Linux machines
make gen-openssl

# On Linux machines
make -C deps/openssl/config clean
make -C deps/openssl/config
```

## 4. Check diffs

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

## 5. Commit and make test

Update all architecture dependent files. Do not forget to git add or remove
files if they are changed before committing:

```bash
git add deps/openssl/config/archs
git add deps/openssl/openssl
git commit
```

The commit message can be written as (with the openssl version set
to the relevant value):

```text
deps: update archs files for openssl-3.0.16

After an OpenSSL source update, all the config files need to be
regenerated and committed by:
    $ make -C deps/openssl/config
    $ git add deps/openssl/config/archs
    $ git add deps/openssl/openssl
    $ git commit
```

Finally, build Node.js and run the tests.

[update-openssl-action]: ../../../.github/workflows/update-openssl.yml
