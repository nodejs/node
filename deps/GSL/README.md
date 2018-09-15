# GSL: Guideline Support Library [![Build Status](https://travis-ci.org/Microsoft/GSL.svg?branch=master)](https://travis-ci.org/Microsoft/GSL) [![Build status](https://ci.appveyor.com/api/projects/status/github/Microsoft/GSL?svg=true)](https://ci.appveyor.com/project/neilmacintosh/GSL)

The Guideline Support Library (GSL) contains functions and types that are suggested for use by the
[C++ Core Guidelines](https://github.com/isocpp/CppCoreGuidelines) maintained by the [Standard C++ Foundation](https://isocpp.org).
This repo contains Microsoft's implementation of GSL.

The library includes types like `span<T>`, `string_span`, `owner<>` and others.

The entire implementation is provided inline in the headers under the [gsl](./include/gsl) directory. The implementation generally assumes a platform that implements C++14 support. There are specific workarounds to support MSVC 2015.

While some types have been broken out into their own headers (e.g. [gsl/span](./include/gsl/span)),
it is simplest to just include [gsl/gsl](./include/gsl/gsl) and gain access to the entire library.

> NOTE: We encourage contributions that improve or refine any of the types in this library as well as ports to
other platforms. Please see [CONTRIBUTING.md](./CONTRIBUTING.md) for more information about contributing.

# Project Code of Conduct
This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

# Usage of Third Party Libraries
This project makes use of the [Catch](https://github.com/philsquared/catch) testing library. Please see the [ThirdPartyNotices.txt](./ThirdPartyNotices.txt) file for details regarding the licensing of Catch.

# Quick Start
## Supported Platforms
The test suite that exercises GSL has been built and passes successfully on the following platforms:<sup>1)</sup>

* Windows using Visual Studio 2015
* Windows using Visual Studio 2017
* Windows using Clang/LLVM 3.6
* Windows using GCC 5.1
* Windows using Intel C++ Compiler 18.0
* GNU/Linux using Clang/LLVM 3.6
* GNU/Linux using GCC 5.1
* OS X Yosemite using Xcode with Apple Clang 7.0.0.7000072
* OS X Yosemite using GCC-5.2.0
* OS X Sierra 10.12.4 using Apple LLVM version 8.1.0 (Clang-802.0.42)
* OS X El Capitan (10.11) using Xcode with AppleClang 8.0.0.8000042
* OS X High Sierra 10.13.2 (17C88) using Apple LLVM version 9.0.0 (clang-900.0.39.2)
* FreeBSD 10.x with Clang/LLVM 3.6

> If you successfully port GSL to another platform, we would love to hear from you. Please submit an issue to let us know. Also please consider
contributing any changes that were necessary back to this project to benefit the wider community.

<sup>1)</sup> For `gsl::byte` to work correctly with Clang and GCC you might have to use the ` -fno-strict-aliasing` compiler option.

## Building the tests
To build the tests, you will require the following:

* [CMake](http://cmake.org), version 3.1.3 or later to be installed and in your PATH.

These steps assume the source code of this repository has been cloned into a directory named `c:\GSL`.

1. Create a directory to contain the build outputs for a particular architecture (we name it c:\GSL\build-x86 in this example).

        cd GSL
        md build-x86
        cd build-x86

2. Configure CMake to use the compiler of your choice (you can see a list by running `cmake --help`).

        cmake -G "Visual Studio 14 2015" c:\GSL

3. Build the test suite (in this case, in the Debug configuration, Release is another good choice).

        cmake --build . --config Debug

4. Run the test suite.

        ctest -C Debug

All tests should pass - indicating your platform is fully supported and you are ready to use the GSL types!

## Using the libraries
As the types are entirely implemented inline in headers, there are no linking requirements.

You can copy the [gsl](./include/gsl) directory into your source tree so it is available
to your compiler, then include the appropriate headers in your program.

Alternatively set your compiler's *include path* flag to point to the GSL development folder (`c:\GSL\include` in the example above) or installation folder (after running the install). Eg.

MSVC++

    /I c:\GSL\include

GCC/clang

    -I$HOME/dev/GSL/include

Include the library using:

    #include <gsl/gsl>

## Debugging visualization support
For Visual Studio users, the file [GSL.natvis](./GSL.natvis) in the root directory of the repository can be added to your project if you would like more helpful visualization of GSL types in the Visual Studio debugger than would be offered by default.
