![libuv][libuv_banner]

## Overview

libuv is a multi-platform support library with a focus on asynchronous I/O. It
was primarily developed for use by [Node.js](http://nodejs.org), but it's also
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

## Community

 * [Mailing list](http://groups.google.com/group/libuv)
 * [IRC chatroom (#libuv@irc.freenode.org)](http://webchat.freenode.net?channels=libuv&uio=d4)

## Documentation

### Official API documentation

Located in the docs/ subdirectory. It uses the [Sphinx](http://sphinx-doc.org/)
framework, which makes it possible to build the documentation in multiple
formats.

Show different supported building options:

    $ make help

Build documentation as HTML:

    $ make html

Build documentation as HTML and live reload it when it changes (this requires
sphinx-autobuild to be installed and is only supported on Unix):

    $ make livehtml

Build documentation as man pages:

    $ make man

Build documentation as ePub:

    $ make epub

NOTE: Windows users need to use make.bat instead of plain 'make'.

Documentation can be browsed online [here](http://docs.libuv.org).

The [tests and benchmarks](https://github.com/libuv/libuv/tree/master/test)
also serve as API specification and usage examples.

### Other resources

 * [An Introduction to libuv](http://nikhilm.github.com/uvbook/)
   &mdash; An overview of libuv with tutorials.
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

    $ gpg --keyserver pool.sks-keyservers.net \
      --recv-keys AE9BC059

Importing a key from a git blob object:

    $ git show pubkey-saghul | gpg --import

### Verifying releases

Git tags are signed with the developer's key, they can be verified as follows:

    $ git verify-tag v1.6.1

Starting with libuv 1.7.0, the tarballs stored in the
[downloads site](http://dist.libuv.org/dist/) are signed and an accompanying
signature file sit alongside each. Once both the release tarball and the
signature file are downloaded, the file can be verified as follows:

    $ gpg --verify libuv-1.7.0.tar.gz.sign

## Build Instructions

For GCC there are two build methods: via autotools or via [GYP][].
GYP is a meta-build system which can generate MSVS, Makefile, and XCode
backends. It is best used for integration into other projects.

To build with autotools:

    $ sh autogen.sh
    $ ./configure
    $ make
    $ make check
    $ make install

### Windows

First, [Python][] 2.6 or 2.7 must be installed as it is required by [GYP][].
If python is not in your path, set the environment variable `PYTHON` to its
location. For example: `set PYTHON=C:\Python27\python.exe`

To build with Visual Studio, launch a git shell (e.g. Cmd or PowerShell)
and run vcbuild.bat which will checkout the GYP code into build/gyp and
generate uv.sln as well as related project files.

To have GYP generate build script for another system, checkout GYP into the
project tree manually:

    $ git clone https://chromium.googlesource.com/external/gyp.git build/gyp

### Unix

For Debug builds (recommended) run:

    $ ./gyp_uv.py -f make
    $ make -C out

For Release builds run:

    $ ./gyp_uv.py -f make
    $ BUILDTYPE=Release make -C out

Run `./gyp_uv.py -f make -Dtarget_arch=x32` to build [x32][] binaries.

### OS X

Run:

    $ ./gyp_uv.py -f xcode
    $ xcodebuild -ARCHS="x86_64" -project uv.xcodeproj \
         -configuration Release -target All

Using Homebrew:

    $ brew install --HEAD libuv

Note to OS X users:

Make sure that you specify the architecture you wish to build for in the
"ARCHS" flag. You can specify more than one by delimiting with a space
(e.g. "x86_64 i386").

### Android

Run:

    $ source ./android-configure NDK_PATH gyp
    $ make -C out

Note for UNIX users: compile your project with `-D_LARGEFILE_SOURCE` and
`-D_FILE_OFFSET_BITS=64`. GYP builds take care of that automatically.

### Using Ninja

To use ninja for build on ninja supported platforms, run:

    $ ./gyp_uv.py -f ninja
    $ ninja -C out/Debug     #for debug build OR
    $ ninja -C out/Release


### Running tests

Run:

    $ ./gyp_uv.py -f make
    $ make -C out
    $ ./out/Debug/run-tests

## Supported Platforms

Check the [SUPPORTED_PLATFORMS file](SUPPORTED_PLATFORMS.md).

### AIX Notes

AIX support for filesystem events requires the non-default IBM `bos.ahafs`
package to be installed.  This package provides the AIX Event Infrastructure
that is detected by `autoconf`.
[IBM documentation](http://www.ibm.com/developerworks/aix/library/au-aix_event_infrastructure/)
describes the package in more detail.

AIX support for filesystem events is not compiled when building with `gyp`.

## Patches

See the [guidelines for contributing][].

[node.js]: http://nodejs.org/
[GYP]: http://code.google.com/p/gyp/
[Python]: https://www.python.org/downloads/
[guidelines for contributing]: https://github.com/libuv/libuv/blob/master/CONTRIBUTING.md
[libuv_banner]: https://raw.githubusercontent.com/libuv/libuv/master/img/banner.png
[x32]: https://en.wikipedia.org/wiki/X32_ABI
