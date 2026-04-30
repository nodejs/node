---
title: 'Compiling on Arm64 Linux'
description: 'Tips and tricks to build V8 natively on Arm64 Linux'
---
If you've gone through instructions on how to [check out](/docs/source-code) and [build](/docs/build-gn) V8 on a machine that is neither x86 nor an Apple Silicon Mac, you may have ran into a bit of trouble, due to the build system downloading native binaries and then not being able to run them. However, even though using an Arm64 Linux machine to work on V8 is __not officially supported__, overcoming those hurdles is pretty straightforward.

## Bypassing `vpython`

`fetch v8`, `gclient sync` and other `depot_tools` commands use a wrapper for python called "vpython". If you see errors related to it, you can define the following variable to use the system's python installation instead:

```bash
export VPYTHON_BYPASS="manually managed python not supported by chrome operations"
```

## Compatible `ninja` binary

The first thing to do is to make sure we use a native binary for `ninja`, which we pick instead of the one in `depot_tools`. A simple way to do this is to tweak your PATH as follows when installing `depot_tools`:

```bash
export PATH=$PATH:/path/to/depot_tools
```

This way, you'll be able to use your system's `ninja` installation, given it's likely to be available. Although if it isn't you can [build it from source](https://github.com/ninja-build/ninja#building-ninja-itself).

## Compiling clang

By default, V8 will want to use its own build of clang that may not run on your machine. You could tweak GN arguments in order to [use the system's clang or GCC](#system_clang_gcc), however, you may want to use the same clang as upstream, as it will be the most supported version.

You can build it locally directly from the V8 checkout:

```bash
./tools/clang/scripts/build.py --without-android --without-fuchsia \
                               --host-cc=gcc --host-cxx=g++ \
                               --gcc-toolchain=/usr \
                               --use-system-cmake --disable-asserts
```

## Setting up GN arguments manually

Convenience scripts may not work by default, instead you'll have to set GN arguments manually following the [manual](/docs/build-gn#gn) workflow. You can get the usual "release", "optdebug" and "debug" configurations with the following arguments:

- `release`

```bash
is_debug=false
```

- `optdebug`

```bash
is_debug=true
v8_enable_backtrace=true
v8_enable_slow_dchecks=true
```

- `debug`

```bash
is_debug=true
v8_enable_backtrace=true
v8_enable_slow_dchecks=true
v8_optimized_debug=false
```

## Using the system's clang or GCC { #system_clang_gcc }

Building with GCC is only a case of disabling compiling with clang:

```bash
is_clang=false
```

Note that by default, V8 will link using `lld`, which requires a recent version of GCC. You can use `use_lld=false` to switch to the gold linker, or additionally use `use_gold=false` to use `ld`.

If you'd like to use the clang that's installed with your system, say in `/usr`, you can use the following arguments:

```bash
clang_base_path="/usr"
clang_use_chrome_plugins=false
```

However, given the system's clang version may not be well supported, you're likely to deal with warnings, such as unknown compiler flags. In this case it's useful to stop treating warnings as errors with:

```bash
treat_warnings_as_errors=false
```
