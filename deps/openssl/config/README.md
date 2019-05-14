## Upgrading OpenSSL

### Requirements
- Linux environment (Only CentOS7.1 and Ubuntu16 are tested)
- `perl` Only Perl version 5 is tested.
- `nasm` (http://www.nasm.us/)  The version of 2.11 or higher is needed.
- GNU `as` in binutils. The version of 2.26 or higher is needed.

### 0. Check Requirements

```sh
$ perl -v

This is perl 5, version 22, subversion 1 (v5.22.1) built for
x86_64-linux-gnu-thread-multi
(with 60 registered patches, see perl -V for more detail)

$ as --version
GNU assembler (GNU Binutils for Ubuntu) 2.26.1
Copyright (C) 2015 Free Software Foundation, Inc.
...
$ nasm -v
NASM version 2.11.08
```

### 1. Obtain and extract new OpenSSL sources

Get a new source from  https://www.openssl.org/source/ and extract
all files into `deps/openssl/openssl`. Then add all files and commit
them.
```sh
$ cd deps/openssl/
$ rm -rf openssl
$ tar zxf ~/tmp/openssl-1.1.0h.tar.gz
$ mv openssl-1.1.0h openssl
$ git add --all openssl
$ git commit openssl
````

The commit message can be (with the openssl version set to the relevant value):
```
deps: upgrade openssl sources to 1.1.0h

This updates all sources in deps/openssl/openssl by:
    $ cd deps/openssl/
    $ rm -rf openssl
    $ tar zxf ~/tmp/openssl-1.1.0h.tar.gz
    $ mv openssl-1.1.0h openssl
    $ git add --all openssl
    $ git commit openssl
```

### 2. Apply a floating patch

Currently, one floating patch is needed to build S390 asm files:
```
Author: Shigeki Ohtsu <ohtsu@ohtsu.org>
Date:   Wed Mar 7 23:52:52 2018 +0900

    deps: add s390 asm rules for OpenSSL-1.1.0

    This is a floating patch against OpenSSL-1.1.0 to generate asm files
    with Makefile rules and it is to be submitted to the upstream.

    Fixes: https://github.com/nodejs/node/issues/4270
    PR-URL: https://github.com/nodejs/node/pull/19794
    Reviewed-By: James M Snell <jasnell@gmail.com>
    Reviewed-By: Rod Vagg <rod@vagg.org>
    Reviewed-By: Michael Dawson <michael_dawson@ca.ibm.com>

 deps/openssl/openssl/crypto/poly1305/build.info | 2 ++
```

Find the SHA of the previous commit of this patch:
```sh
$ git log -n1 --oneline -- deps/openssl/openssl/crypto/poly1305/build.info
```

Using the SHA found in the previous step, cherry pick it from the previous
commit (with the openssl version in the commit message set to the relevant
value):
```sh
$ git cherry-pick 45b9f5df6ff1548f01ed646ebee75e3f0873cefd
```


### 3. Execute `make` in `deps/openssl/config` directory

Use `make` to regenerate all platform dependent files in
`deps/openssl/config/archs/`:
```sh
$ cd deps/openssl/config; make
```

### 4. Check diffs

Check diffs if updates are right. Even if no updates in openssl
sources, `buildinf.h` files will be updated for they have a timestamp
data in them.
```sh
$ cd deps/openssl/config
$ git diff
```

*Note*: On Windows, OpenSSL Configure generates `makefile` that can be
used for `nmake` command. The `make` command in the step 2 above uses
 `Makefile_VC-WIN64A` and `Makefile_VC-WIN32` that are manually
created. When source files or build options are updated in Windows,
it needs to change these two Makefiles by hand. If you are not sure,
please ask @shigeki for details.

### 5. Commit and make test

Update all architecture dependent files. Do not forget to git add or remove
files if they are changed before commit:
```sh
$ git add deps/openssl/config/archs
$ git add deps/openssl/openssl/crypto/include/internal/bn_conf.h
$ git add deps/openssl/openssl/crypto/include/internal/dso_conf.h
$ git add deps/openssl/openssl/include/openssl/opensslconf.h
$ git commit
```

The commit message can be (with the openssl version set to the relevant value):
```
 deps: update archs files for OpenSSL-1.1.0

 After an OpenSSL source update, all the config files need to be regenerated and
 comitted by:
    $ cd deps/openssl/config
    $ make
    $ git add deps/openssl/config/archs
    $ git add deps/openssl/openssl/crypto/include/internal/bn_conf.h
    $ git add deps/openssl/openssl/crypto/include/internal/dso_conf.h
    $ git add deps/openssl/openssl/include/openssl/opensslconf.h
    $ git commit
```

Finally, build Node and run tests.
