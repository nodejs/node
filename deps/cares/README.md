c-ares
======

[![Build Status](https://api.cirrus-ci.com/github/c-ares/c-ares.svg)](https://cirrus-ci.com/github/c-ares/c-ares)
[![Windows Build Status](https://ci.appveyor.com/api/projects/status/aevgc5914tm72pvs/branch/master?svg=true)](https://ci.appveyor.com/project/c-ares/c-ares/branch/master)
[![Coverage Status](https://coveralls.io/repos/github/c-ares/c-ares/badge.svg)](https://coveralls.io/github/c-ares/c-ares)
[![CII Best Practices](https://bestpractices.coreinfrastructure.org/projects/291/badge)](https://bestpractices.coreinfrastructure.org/projects/291)
[![Fuzzing Status](https://oss-fuzz-build-logs.storage.googleapis.com/badges/c-ares.svg)](https://bugs.chromium.org/p/oss-fuzz/issues/list?sort=-opened&can=1&q=proj:c-ares)
[![Bugs](https://sonarcloud.io/api/project_badges/measure?project=c-ares_c-ares&metric=bugs)](https://sonarcloud.io/summary/new_code?id=c-ares_c-ares)
[![Coverity Scan Status](https://scan.coverity.com/projects/c-ares/badge.svg)](https://scan.coverity.com/projects/c-ares)

This is c-ares, an asynchronous resolver library.  It is intended for
applications which need to perform DNS queries without blocking, or need to
perform multiple DNS queries in parallel.  The primary examples of such
applications are servers which communicate with multiple clients and programs
with graphical user interfaces.

The full source code is available in the ['c-ares' release archives](https://c-ares.org/download/),
and in a git repository: https://github.com/c-ares/c-ares.  See the
[INSTALL.md](INSTALL.md) file for build information.

If you find bugs, correct flaws, have questions or have comments in general in
regard to c-ares (or by all means the original ares too), get in touch with us
on the c-ares mailing list: https://lists.haxx.se/listinfo/c-ares

c-ares is distributed under the MIT license.

You'll find all c-ares details and news here:
        https://c-ares.org/


Notes for c-ares hackers
------------------------

* The distributed `ares_build.h` file is only intended to be used on systems
  which can not run the also distributed configure script.

* The distributed `ares_build.h` file is generated as a copy of `ares_build.h.dist`
  when the c-ares source code distribution archive file is originally created.

* If you check out from git on a non-configure platform, you must run the
  appropriate `buildconf*` script to set up `ares_build.h` and other local files
  before being able to compile the library.

* On systems capable of running the `configure` script, the `configure` process
  will overwrite the distributed `ares_build.h` file with one that is suitable
  and specific to the library being configured and built, this new file is
  generated from the `ares_build.h.in` template file.

* If you intend to distribute an already compiled c-ares library you **MUST**
  also distribute along with it the generated `ares_build.h` which has been
  used to compile it. Otherwise the library will be of no use for the users of
  the library that you have built. It is **your** responsibility to provide this
  file. No one at the c-ares project can know how you have built the library.

* File `ares_build.h` includes platform and configuration dependent info,
  and must not be modified by anyone. Configure script generates it for you.

* We cannot assume anything else but very basic compiler features being
  present. While c-ares requires an ANSI C compiler to build, some of the
  earlier ANSI compilers clearly can't deal with some preprocessor operators.

* Newlines must remain unix-style for older compilers' sake.

* Comments must be written in the old-style /* unnested C-fashion */

* Try to keep line lengths below 80 columns.
