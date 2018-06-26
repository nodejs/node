![libuv][libuv_banner]

## Overview

libuv is a multi-platform support library with a focus on asynchronous I/O. It
was primarily developed for use by [Node.js][], but it's also
used by [Luvit](http://luvit.io/), [Julia](http://julialang.org/),
[pyuv](https://github.com/saghul/pyuv), and [others](https://github.com/libuv/libuv/wiki/Projects-that-use-libuv).

## Feature highlights

 * Full-featured event loop backed by epoll, kqueue, IOCP, event ports.

 * Asynchronous TCP and UDP sockets

 * Asynchronous DNS resolution

 * Asynchronous file and file system operations

 * File system events

 * ANSI escape code controlled TTY

 * IPC with socket sharing, using Unix domain sockets or named pipes (Windows)

 * Child processes

 * Thread pool

 * Signal handling

 * High resolution clock

 * Threading and synchronization primitives

## Versioning

Starting with version 1.0.0 libuv follows the [semantic versioning](http://semver.org/)
scheme. The API change and backwards compatibility rules are those indicated by
SemVer. libuv will keep a stable ABI across major releases.

The ABI/API changes can be tracked [here](http://abi-laboratory.pro/tracker/timeline/libuv/).

## Licensing

libuv is licensed under the MIT license. Check the [LICENSE file](LICENSE).
The documentation is licensed under the CC BY 4.0 license. Check the [LICENSE-docs file](LICENSE-docs).

## Community

 * [Support](https://github.com/libuv/help)
 * [Mailing list](http://groups.google.com/group/libuv)
 * [IRC chatroom (#libuv@irc.freenode.org)](http://webchat.freenode.net?channels=libuv&uio=d4)

## Documentation

### Official documentation

Located in the docs/ subdirectory. It uses the [Sphinx](http://sphinx-doc.org/)
framework, which makes it possible to build the documentation in multiple
formats.

Show different supported building options:

```bash
$ make help
```

Build documentation as HTML:

```bash
$ make html
```

Build documentation as HTML and live reload it when it changes (this requires
sphinx-autobuild to be installed and is only supported on Unix):

```bash
$ make livehtml
```

Build documentation as man pages:

```bash
$ make man
```

Build documentation as ePub:

```bash
$ make epub
```

NOTE: Windows users need to use make.bat instead of plain 'make'.

Documentation can be browsed online [here](http://docs.libuv.org).

The [tests and benchmarks](https://github.com/libuv/libuv/tree/master/test)
also serve as API specification and usage examples.

### Other resources

 * [LXJS 2012 talk](http://www.youtube.com/watch?v=nGn60vDSxQ4)
   &mdash; High-level introductory talk about libuv.
 * [libuv-dox](https://github.com/thlorenz/libuv-dox)
   &mdash; Documenting types and methods of libuv, mostly by reading uv.h.
 * [learnuv](https://github.com/thlorenz/learnuv)
   &mdash; Learn uv for fun and profit, a self guided workshop to libuv.

These resources are not handled by libuv maintainers and might be out of
date. Please verify it before opening new issues.

## Downloading

libuv can be downloaded either from the
[GitHub repository](https://github.com/libuv/libuv)
or from the [downloads site](http://dist.libuv.org/dist/).

Starting with libuv 1.7.0, binaries for Windows are also provided. This is to
be considered EXPERIMENTAL.

Before verifying the git tags or signature files, importing the relevant keys
is necessary. Key IDs are listed in the
[MAINTAINERS](https://github.com/libuv/libuv/blob/master/MAINTAINERS.md)
file, but are also available as git blob objects for easier use.

Importing a key the usual way:

```bash
$ gpg --keyserver pool.sks-keyservers.net --recv-keys AE9BC059
```

Importing a key from a git blob object:

```bash
$ git show pubkey-saghul | gpg --import
```

### Verifying releases

Git tags are signed with the developer's key, they can be verified as follows:

```bash
$ git verify-tag v1.6.1
```

Starting with libuv 1.7.0, the tarballs stored in the
[downloads site](http://dist.libuv.org/dist/) are signed and an accompanying
signature file sit alongside each. Once both the release tarball and the
signature file are downloaded, the file can be verified as follows:

```bash
$ gpg --verify libuv-1.7.0.tar.gz.sign
```

## Build Instructions

For GCC there are two build methods: via autotools or via [GYP][].
GYP is a meta-build system which can generate MSVS, Makefile, and XCode
backends. It is best used for integration into other projects.

To build with autotools:

```bash
$ sh autogen.sh
$ ./configure
$ make
$ make check
$ make install
```

To build with [CMake](https://cmake.org/):

```bash
$ mkdir -p out/cmake ; cd out/cmake ; cmake -DBUILD_TESTING=ON ../..
$ make all test
# Or manually:
$ ./uv_run_tests    # shared library build
$ ./uv_run_tests_a  # static library build
```

To build with GYP, first run:

```bash
$ git clone https://chromium.googlesource.com/external/gyp build/gyp
```

### Windows

Prerequisites:

* [Python 2.6 or 2.7][] as it is required
  by [GYP][].
  If python is not in your path, set the environment variable `PYTHON` to its
  location. For example: `set PYTHON=C:\Python27\python.exe`
* One of:
  * [Visual C++ Build Tools][]
  * [Visual Studio 2015 Update 3][], all editions
    including the Community edition (remember to select
    "Common Tools for Visual C++ 2015" feature during installation).
  * [Visual Studio 2017][], any edition (including the Build Tools SKU).
    **Required Components:** "MSbuild", "VC++ 2017 v141 toolset" and one of the
    Windows SDKs (10 or 8.1).
* Basic Unix tools required for some tests,
  [Git for Windows][] includes Git Bash
  and tools which can be included in the global `PATH`.

To build, launch a git shell (e.g. Cmd or PowerShell), run `vcbuild.bat`
(to build with VS2017 you need to explicitly add a `vs2017` argument),
which will checkout the GYP code into `build/gyp`, generate `uv.sln`
as well as the necesery related project files, and start building.

```console
> vcbuild
```

Or:

```console
> vcbuild vs2017
```

To run the tests:

```console
> vcbuild test
```

To see all the options that could passed to `vcbuild`:

```console
> vcbuild help
vcbuild.bat [debug/release] [test/bench] [clean] [noprojgen] [nobuild] [vs2017] [x86/x64] [static/shared]
Examples:
  vcbuild.bat              : builds debug build
  vcbuild.bat test         : builds debug build and runs tests
  vcbuild.bat release bench: builds release build and runs benchmarks
```


### Unix

For Debug builds (recommended) run:

```bash
$ ./gyp_uv.py -f make
$ make -C out
```

For Release builds run:

```bash
$ ./gyp_uv.py -f make
$ BUILDTYPE=Release make -C out
```

Run `./gyp_uv.py -f make -Dtarget_arch=x32` to build [x32][] binaries.

### OS X

Run:

```bash
$ ./gyp_uv.py -f xcode
$ xcodebuild -ARCHS="x86_64" -project uv.xcodeproj \
     -configuration Release -target All
```

Using Homebrew:

```bash
$ brew install --HEAD libuv
```

Note to OS X users:

Make sure that you specify the architecture you wish to build for in the
"ARCHS" flag. You can specify more than one by delimiting with a space
(e.g. "x86_64 i386").

### Android

Run:

```bash
$ source ./android-configure NDK_PATH gyp [API_LEVEL]
$ make -C out
```

The default API level is 24, but a different one can be selected as follows:

```bash
$ source ./android-configure ~/android-ndk-r15b gyp 21
$ make -C out
```

Note for UNIX users: compile your project with `-D_LARGEFILE_SOURCE` and
`-D_FILE_OFFSET_BITS=64`. GYP builds take care of that automatically.

### Using Ninja

To use ninja for build on ninja supported platforms, run:

```bash
$ ./gyp_uv.py -f ninja
$ ninja -C out/Debug     #for debug build OR
$ ninja -C out/Release
```


### Running tests

Run:

```bash
$ ./gyp_uv.py -f make
$ make -C out
$ ./out/Debug/run-tests
```

## Supported Platforms

Check the [SUPPORTED_PLATFORMS file](SUPPORTED_PLATFORMS.md).

### AIX Notes

AIX support for filesystem events requires the non-default IBM `bos.ahafs`
package to be installed.  This package provides the AIX Event Infrastructure
that is detected by `autoconf`.
[IBM documentation](http://www.ibm.com/developerworks/aix/library/au-aix_event_infrastructure/)
describes the package in more detail.

AIX support for filesystem events is not compiled when building with `gyp`.

### z/OS Notes

z/OS creates System V semaphores and message queues. These persist on the system
after the process terminates unless the event loop is closed.

Use the `ipcrm` command to manually clear up System V resources.

## Patches

See the [guidelines for contributing][].

[node.js]: http://nodejs.org/
[GYP]: http://code.google.com/p/gyp/
[guidelines for contributing]: https://github.com/libuv/libuv/blob/master/CONTRIBUTING.md
[libuv_banner]: https://raw.githubusercontent.com/libuv/libuv/master/img/banner.png
[x32]: https://en.wikipedia.org/wiki/X32_ABI
[Python 2.6 or 2.7]: https://www.python.org/downloads/
[Visual C++ Build Tools]: http://landinghub.visualstudio.com/visual-cpp-build-tools
[Visual Studio 2015 Update 3]: https://www.visualstudio.com/vs/older-downloads/
[Visual Studio 2017]: https://www.visualstudio.com/downloads/
[Git for Windows]: http://git-scm.com/download/win
