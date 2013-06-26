# libuv

libuv is a new platform layer for Node. Its purpose is to abstract IOCP on
Windows and epoll/kqueue/event ports/etc. on Unix systems. We intend to
eventually contain all platform differences in this library.

http://nodejs.org/

## Features

 * Non-blocking TCP sockets

 * Non-blocking named pipes

 * UDP

 * Timers

 * Child process spawning

 * Asynchronous DNS via `uv_getaddrinfo`.

 * Asynchronous file system APIs `uv_fs_*`

 * High resolution time `uv_hrtime`

 * Current executable path look up `uv_exepath`

 * Thread pool scheduling `uv_queue_work`

 * ANSI escape code controlled TTY `uv_tty_t`

 * File system events Currently supports inotify, `ReadDirectoryChangesW`
   and kqueue. Event ports in the near future.
   `uv_fs_event_t`

 * IPC and socket sharing between processes `uv_write2`

## Community

 * [Mailing list](http://groups.google.com/group/libuv)

## Documentation

 * [include/uv.h](https://github.com/joyent/libuv/blob/master/include/uv.h)
   &mdash; API documentation in the form of detailed header comments.
 * [An Introduction to libuv](http://nikhilm.github.com/uvbook/) &mdash; An
   overview of libuv with tutorials.
 * [LXJS 2012 talk](http://www.youtube.com/watch?v=nGn60vDSxQ4) - High-level
   introductory talk about libuv.
 * [Tests and benchmarks](https://github.com/joyent/libuv/tree/master/test) -
   API specification and usage examples.

## Build Instructions

For GCC (including MinGW) there are two methods building: via normal
makefiles or via GYP. GYP is a meta-build system which can generate MSVS,
Makefile, and XCode backends. It is best used for integration into other
projects.  The old system is using plain GNU Makefiles.

To build via Makefile simply execute:

    make

MinGW users should run this instead:

    make PLATFORM=mingw

Out-of-tree builds are supported:

    make builddir_name=/path/to/builddir

To build a shared object:

    make libuv.so  # libuv.dylib on OS X

To build with Visual Studio run the vcbuild.bat file which will
checkout the GYP code into build/gyp and generate the uv.sln and
related files.

Windows users can also build from cmd-line using msbuild.  This is
done by running vcbuild.bat from Visual Studio command prompt.

To have GYP generate build script for another system, make sure that
you have Python 2.6 or 2.7 installed, then checkout GYP into the
project tree manually:

    mkdir -p build
    svn co http://gyp.googlecode.com/svn/trunk build/gyp

Or:

    mkdir -p build
    git clone https://git.chromium.org/external/gyp.git build/gyp

Unix users run

    ./gyp_uv -f make
    make -C out

Macintosh users run

    ./gyp_uv -f xcode
    xcodebuild -project uv.xcodeproj -configuration Release -target All

Note for UNIX users: compile your project with `-D_LARGEFILE_SOURCE` and
`-D_FILE_OFFSET_BITS=64`. GYP builds take care of that automatically.

Note for Linux users: compile your project with `-D_GNU_SOURCE` when you
include `uv.h`. GYP builds take care of that automatically. If you use
autotools, add a `AC_GNU_SOURCE` declaration to your `configure.ac`.

To build for android, locate your android NDK path, then run:

    source ./android-configure NDK_PATH
    make

To build for android with gyp, add "gyp" to the configuration:

    source ./android-configure NDK_PATH gyp
    make -C out

## Supported Platforms

Microsoft Windows operating systems since Windows XP SP2. It can be built
with either Visual Studio or MinGW. Consider using
[Visual Studio Express 2010][] or later if you do not have a full Visual
Studio license.

Linux 2.6 using the GCC toolchain.

MacOS using the GCC or XCode toolchain.

Solaris 121 and later using GCC toolchain.

[Visual Studio Express 2010]: http://www.microsoft.com/visualstudio/eng/products/visual-studio-2010-express
