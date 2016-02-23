**Build issues? File a bug at code.google.com/p/v8/issues or ask for help on v8-users@googlegroups.com.**

# Building V8

V8 is built with the help of [GYP](http://code.google.com/p/gyp/). GYP is a meta build system of sorts, as it generates build files for a number of other build systems. How you build therefore depends on what "back-end" build system and compiler you're using.
The instructions below assume that you already have a [checkout of V8](using_git.md) but haven't yet installed the build dependencies.

If you intend to develop on V8, i.e., send patches and work with changelists, you will need to install the dependencies as described [here](using_git.md).


## Prerequisite: Installing GYP

First, you need GYP itself. GYP is fetched together with the other dependencies by running:

```
gclient sync
```

## Building

### GCC + make

Requires GNU make 3.81 or later. Should work with any GCC >= 4.8 or any recent clang (3.5 highly recommended).

#### Build instructions


The top-level Makefile defines a number of targets for each target architecture (`ia32`, `x64`, `arm`, `arm64`) and mode (`debug`, `optdebug`, or `release`). So your basic command for building is:
```
make ia32.release
```

or analogously for the other architectures and modes. You can build both debug and release binaries with just one command:
```
make ia32
```

To automatically build in release mode for the host architecture:
```
make native
```

You can also can build all architectures in a given mode at once:
```
make release
```

Or everything:
```
make
```

#### Optional parameters

  * `-j` specifies the number of parallel build processes. Set it (roughly) to the number of CPU cores your machine has. The GYP/make based V8 build also supports distcc, so you can compile with `-j100` or so, provided you have enough machines around.

  * `OUTDIR=foo` specifies where the compiled binaries go. It defaults to `./out/`. In this directory, a subdirectory will be created for each architecture and mode. You will find the d8 shell's binary in `foo/ia32.release/d8`, for example.

  * `library=shared` or `component=shared_library` (the two are completely equivalent) builds V8 as a shared library (`libv8.so`).

  * `soname_version=1.2.3` is only relevant for shared library builds and configures the SONAME of the library. Both the SONAME and the filename of the library will be `libv8.so.1.2.3` if you specify this. Due to a peculiarity in GYP, if you specify a custom SONAME, the library's path will no longer be encoded in the binaries, so you'll have to run d8 as follows:
```
LD_LIBRARY_PATH=out/ia32.release/lib.target out/ia32.release/d8
```

  * `console=readline` enables readline support for the d8 shell. You need readline development headers for this (`libreadline-dev` on Ubuntu).

  * `disassembler=on` enables the disassembler for release mode binaries (it's always enabled for debug binaries). This is useful if you want to inspect generated machine code.

  * `snapshot=off` disables building with a heap snapshot. Compiling will be a little faster, but V8’s start up will be slightly slower.

  * `gdbjit=on` enables GDB JIT support.

  * `liveobjectlist=on` enables the Live Object List feature.

  * `vfp3=off` is only relevant for ARM builds with snapshot and disables the use of VFP3 instructions in the snapshot.

  * `debuggersupport=off` disables the javascript debugger.

  * `werror=no` omits the -Werror flag. This is especially useful for not officially supported C++ compilers (e.g. newer versions of the GCC) so that compile warnings are ignored.

  * `strictaliasing=off` passes the -fno-strict-aliasing flag to GCC. This may help to work around build failures on officially unsupported platforms and/or GCC versions.

  * `regexp=interpreted` chooses the interpreted mode of the irregexp regular expression engine instead of the native code mode.

  * `hardfp=on` creates "hardfp" binaries on ARM.

### Ninja

To build d8:
```
export GYP_GENERATORS=ninja
build/gyp_v8
ninja -C out/Debug d8
```

Specify `out/Release` for a release build. I recommend setting up an alias so that you don't need to type out that build directory path.

If you want to build all targets, use `ninja -C out/Debug all`. It's faster to build only the target you're working on, like `d8` or `unittests`.

Note: You need to set `v8_target_arch` if you want a non-native build, i.e. either
```
export GYP_DEFINES="v8_target_arch=arm"
build/gyp_v8 ...
```
or
```
build/gyp_v8 -Dv8_target_arch=arm ...
```


#### Using goma (Googlers only)

To use goma you need to set the `use_goma` gyp define, either by passing it to `gyp_v8`, i.e.
```
build/gyp_v8 -Duse_goma=1
```
or by setting the environment variable `$GYP_DEFINES` appropriately:
```
export GYP_DEFINES="use_goma=1"
```
Note: You may need to also set `gomadir` to point to the directory where you installed goma, if it's not in the default location.

If you are using goma, you'll also want to bump the job limit, i.e.
```
ninja -j 100 -C out/Debug d8
```


### Cross-compiling

Similar to building with Clang, you can also use a cross-compiler. Just export your toolchain (`CXX`/`LINK` environment variables should be enough) and compile. For example:
```
export CXX=/path/to/cross-compile-g++
export LINK=/path/to/cross-compile-g++
make arm.release
```


### Xcode

From the root of your V8 checkout, run either of:
```
build/gyp_v8 -Dtarget_arch=ia32
build/gyp_v8 -Dtarget_arch=x64
```

This will generate Xcode project files in `build/` that you can then either open with Xcode or compile directly from the command line:
```
xcodebuild -project build/all.xcodeproj -configuration Release
xcodebuild -project build/all.xcodeproj
```

Note: If you have configured your `GYP_GENERATORS` environment variable, either unset it, or set it to `xcode` for this to work.


#### Custom build settings

You can export the `GYP_DEFINES` environment variable in your shell to configure custom build options. The syntax is `GYP_DEFINES="-Dvariable1=value1 -Dvariable2=value2"` and so on for as many variables as you wish. Possibly interesting options include:
  * `-Dcomponent=shared_library` (see `library=shared` in the [GCC + make](#Optional_parameters.md) section above)
  * `-Dconsole=readline` (see `console=readline`)
  * `-Dv8_enable_disassembler=1` (see `disassembler=on`)
  * `-Dv8_use_snapshot='false'` (see `snapshot=off`)
  * `-Dv8_enable_gdbjit=1` (see `gdbjit=on`)
  * `-Dv8_use_liveobjectlist=true` (see `liveobjectlist=on`)


### Visual Studio

You need Visual Studio 2013, older versions might still work at the moment, but this will probably change soon because we intend to use C++11 features.

#### Prerequisites

After you created [checkout of V8](using_git.md), all dependencies will be already installed.

If you are getting errors during build mentioning that 'python' could not be found, add the 'python.exe' to PATH.

If you have Visual Studio 2013 and 2015 installed side-by-side and set the environment variable GYP\_MSVS\_VERSION to '2013'. In that case the right project files are going to be created.

#### Building
  * If you use the command prompt:
    1. Generate project files:
```
python build\gyp_v8
```
> > > Specify the path to `python.exe` if you don't have it in your PATH.
> > > Append `-Dtarget_arch=x64` if you want to build 64bit binaries. If you switch between ia32 and x64 targets, you may have to manually delete the generated .vcproj/.sln files before regenerating them.
> > > Example:
```
third_party/python_26/python.exe build\gyp_v8 -Dtarget_arch=x64
```
    1. Build:
> > > Either open `build\All.sln` in Visual Studio, or compile on the command line as follows (adapt the path as necessary, or simply put `devenv.com` in your PATH):
```
"c:\Program Files (x86)\Microsoft Visual Studio 9.0\Common7\IDE\devenv.com" /build Release build\All.sln
```
> > > Replace `Release` with `Debug` to build in Debug mode.
> > > The built binaries will be in build\Release\ or build\Debug\.

  * If you use cygwin, the workflow is the same, but the syntax is slightly different:
    1. Generate project files:
```
build/gyp_v8
```
> > > This will spit out a bunch of warnings about missing input files, but it seems to be OK to ignore them. (If you have time to figure this out, we'd happily accept a patch that makes the warnings go away!)
    1. Build:
```
/cygdrive/c/Program\ Files\ (x86)/Microsoft\ Visual\ Studio\ 9.0/Common7/IDE/devenv.com /build Release build/all.sln
```


#### Custom build settings

See the "custom build settings" section for [Xcode](#Xcode) above.


#### Running tests

You can abuse the test driver's --buildbot flag to make it find the executables where MSVC puts them:
```
python tools/run-tests.py --buildbot --outdir build --arch ia32 --mode Release
```


### MinGW

Building on MinGW is not officially supported, but it is possible. You even have two options:

#### Option 1: With Cygwin Installed

Requirements:
  * MinGW
  * Cygwin, including Python
  * Python from www.python.org _(yes, you need two Python installations!)_

Building:
  1. Open a MinGW shell
  1. `export PATH=$PATH:/c/cygwin/bin` _(or wherever you installed Cygwin)_
  1. `make ia32.release -j8`

Running tests:
  1. Open a MinGW shell
  1. `export PATH=/c/Python27:$PATH` _(or wherever you installed Python)_
  1. `make ia32.release.check -j8`

#### Option 2: Without Cygwin, just MinGW

Requirements:
  * MinGW
  * Python from www.python.org

Building and testing:
  1. Open a MinGW shell
  1. `tools/mingw-generate-makefiles.sh` _(re-run this any time a `*`.gyp`*` file changed, such as after updating your checkout)_
  1. `make ia32.release` _(unfortunately -jX doesn't seem to work here)_
  1. `make ia32.release.check -j8`


# Final Note
<font color='darkred'><b>If you have problems or questions, please file bugs at code.google.com/p/v8/issues or send mail to v8-users@googlegroups.com. Comments on this page are likely to go unnoticed and unanswered.</b></font>