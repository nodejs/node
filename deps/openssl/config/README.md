## Upgrading OpenSSL-1.1.0

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
The commit message can be

```
deps: upgrade openssl sources to 1.1.0h

This updates all sources in deps/openssl/openssl with openssl-1.1.0h.
```
### 2. Apply a floating patch

Currently, one floating patch is needed to build S390 asm files.
```
 commit 094465362758ebf967b33c84d5c96230b46a34b3
 Author: Shigeki Ohtsu <ohtsu@ohtsu.org>
 Date:   Wed Mar 7 23:52:52 2018 +0900

     deps: add s390 asm rules for OpenSSL-1.1.0

     This is a floating patch against OpenSSL-1.1.0 to generate asm files
         with Makefile rules and it is to be submitted to the upstream.
```

Cherry pick it from the previous commit.
```sh
$ git cherry-pick 45b9f5df6ff1548f01ed646ebee75e3f0873cefd
```
### 3. Execute `make` in `deps/openssl/config` directory

Just type `make` then it generates all platform dependent files into
`deps/openssl/config/archs` directory.

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
$ git add deps/openssl/openssl/.gitignore
$ git commit
```

The commit message can be
```
 commit 8cb1de45c60f2d520551166610115531db673518
 Author: Shigeki Ohtsu <ohtsu@ohtsu.org>
 Date:   Thu Mar 29 16:46:11 2018 +0900

     deps: update archs files for OpenSSL-1.1.0

     `cd deps/openssl/config; make` updates all archs dependant files.
```

Finally, build Node and run tests.
