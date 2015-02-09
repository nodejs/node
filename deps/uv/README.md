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

Build documentation as man pages:

    $ make man

Build documentation as ePub:

    $ make epub

NOTE: Windows users need to use make.bat instead of plain 'make'.

Documentation can be browsed online [here](http://docs.libuv.org).

### Other resources

 * [An Introduction to libuv](http://nikhilm.github.com/uvbook/)
   &mdash; An overview of libuv with tutorials.
 * [LXJS 2012 talk](http://www.youtube.com/watch?v=nGn60vDSxQ4)
   &mdash; High-level introductory talk about libuv.
 * [Tests and benchmarks](https://github.com/libuv/libuv/tree/master/test)
   &mdash; API specification and usage examples.
 * [libuv-dox](https://github.com/thlorenz/libuv-dox)
   &mdash; Documenting types and methods of libuv, mostly by reading uv.h.
 * [learnuv](https://github.com/thlorenz/learnuv)
   &mdash; Learn uv for fun and profit, a self guided workshop to libuv.

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
    OR
    $ svn co http://gyp.googlecode.com/svn/trunk build/gyp

### Unix

Run:

    $ ./gyp_uv.py -f make
    $ make -C out

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

### Running tests

Run:

    $ ./gyp_uv.py -f make
    $ make -C out
    $ ./out/Debug/run-tests

## Supported Platforms

Microsoft Windows operating systems since Windows XP SP2. It can be built
with either Visual Studio or MinGW. Consider using
[Visual Studio Express 2010][] or later if you do not have a full Visual
Studio license.

Linux using the GCC toolchain.

OS X using the GCC or XCode toolchain.

Solaris 121 and later using GCC toolchain.

## Patches

See the [guidelines for contributing][].

[node.js]: http://nodejs.org/
[GYP]: http://code.google.com/p/gyp/
[Python]: https://www.python.org/downloads/
[Visual Studio Express 2010]: http://www.microsoft.com/visualstudio/eng/products/visual-studio-2010-express
[guidelines for contributing]: https://github.com/libuv/libuv/blob/master/CONTRIBUTING.md
[libuv_banner]: https://raw.githubusercontent.com/libuv/libuv/master/img/banner.png
