This directory contains the V8 GYP files used to generate actual project files
for different build systems.

This is currently work in progress but this is expected to replace the SCons
based build system.

To use this a checkout of GYP is needed inside this directory. From the root of
the V8 project do the following:

$ svn co http://gyp.googlecode.com/svn/trunk build/gyp

Note for the command lines below that Debug is the default configuration,
so specifying that on the command lines is not required.


To generate Makefiles on Linux:
-------------------------------

$ build/gyp_v8

This will build makefiles for ia32, x64 and the ARM simulator with names
Makefile-ia32, Makefile-x64 and Makefile-armu respectively.

To build and run for ia32 in debug and release version do:

$ make -f Makefile-ia32
$ out/Debug/shell
$ make -f Makefile-ia32 BUILDTYPE=Release
$ out/Release/shell

Change the makefile to build and run for the other architectures.


To generate Xcode project files on Mac OS:
------------------------------------------

$ build/gyp_v8

This will make an Xcode project for the ia32 architecture. To build and run do:

$ xcodebuild -project build/all.xcodeproj
$ samples/build/Debug/shell
$ xcodebuild -project build/all.xcodeproj -configuration Release
$ samples/build/Release/shell


To generate Visual Studio solution and project files on Windows:
----------------------------------------------------------------

On Windows an additional third party component is required. This is cygwin in
the same version as is used by the Chromium project. This can be checked out
from the Chromium repository. From the root of the V8 project do the following:

> svn co http://src.chromium.org/svn/trunk/deps/third_party/cygwin@66844 third_party/cygwin

To run GYP Python is required and it is recommended to use the same version as
is used by the Chromium project. This can also be checked out from the Chromium
repository. From the root of the V8 project do the following:

> svn co http://src.chromium.org/svn/trunk/tools/third_party/python_26@89111 third_party/python_26

Now generate Visual Studio solution and project files for the ia32 architecture:

> third_party\python_26\python build/gyp_v8

Now open build\All.sln in Visual Studio.
