# C++ target for ANTLR 4

This folder contains the C++ runtime support for ANTLR.  See [the canonical antlr4 repository](https://github.com/antlr/antlr4) for in depth detail about how to use ANTLR 4.

## Authors and major contributors

ANTLR 4 is the result of substantial effort of the following people:
 
* [Terence Parr](http://www.cs.usfca.edu/~parrt/), parrt@cs.usfca.edu
  ANTLR project lead and supreme dictator for life
  [University of San Francisco](http://www.usfca.edu/)
* [Sam Harwell](http://tunnelvisionlabs.com/) 
  Tool co-author, Java and C# target)

The C++ target has been the work of the following people:

* Dan McLaughlin, dan.mclaughlin@gmail.com (initial port, got code to compile)
* David Sisson, dsisson@google.com (initial port, made the runtime C++ tests runnable)
* [Mike Lischke](www.soft-gems.net), mike@lischke-online.de (brought the initial port to a working library, made most runtime tests passing)

## Other contributors

* Marcin Szalowicz, mszalowicz@mailplus.pl (cmake build setup)
* Tim O'Callaghan, timo@linux.com (additional superbuild cmake pattern script)

## Project Status

* Building on OS X, Windows, and Linux
* No errors and warnings
* Library linking
* Some unit tests in the OSX project, for important base classes with almost 100% code coverage.
* All memory allocations checked
* Simple command line demo application working on all supported platforms.
* All runtime tests pass.

### Build + Usage Notes

The minimum C++ version to compile the ANTLR C++ runtime with is C++11. The supplied projects can built the runtime either as static or dynamic library, as both 32bit and 64bit arch. The OSX project contains a target for iOS and can also be built using cmake (instead of XCode).

Include the antlr4-runtime.h umbrella header in your target application to get everything needed to use the library.

If you are compiling with cmake, the minimum version required is cmake 2.8.

#### Compiling on Windows
Simply open the VS solution (VS 2013+) and build it.

#### Compiling on OSX
Either open the included XCode project and build that or use the cmake compilation as described for linux.

#### Compiling on Linux
- cd <antlr4-dir>/runtime/Cpp (this is where this readme is located)
- mkdir build && mkdir run && cd build
- cmake .. -DANTLR_JAR_LOCATION=full/path/to/antlr4-4.5.4-SNAPSHOT.jar -DWITH_DEMO=True
- make
- DESTDIR=<antlr4-dir>/runtime/Cpp/run make install

If you don't want to build the demo then simply run cmake without parameters.
There is another cmake script available in the subfolder cmake/ for those who prefer the superbuild cmake pattern.

