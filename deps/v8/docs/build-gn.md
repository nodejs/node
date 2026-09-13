---
title: 'Building V8 with GN'
description: 'This document explains how to use GN to build V8.'
---
V8 is built with the help of [GN](https://gn.googlesource.com/gn/+/main/docs/). GN is a meta build system of sorts, as it generates build files for a number of other build systems. How you build therefore depends on what “back-end” build system and compiler you’re using.
The instructions below assume that you already have a [checkout of V8](/docs/source-code) and that you have [installed the build dependencies](/docs/build).

More information on GN can be found in [Chromium’s documentation](https://www.chromium.org/developers/gn-build-configuration) or [GN’s own docs](https://gn.googlesource.com/gn/+/main/docs/).

Building V8 from source involves three steps:

1. generating build files
1. compiling
1. running tests

There are two workflows for building V8:

- the convenience workflow using a helper script called `gm` that nicely combines all three steps
- the raw workflow, where you run separate commands on a lower level for each step manually

## Building V8 using `gm` (the convenience workflow) { #gm }

`gm` is a convenience all-in-one script that generates build files, triggers the build and optionally also runs the tests. It can be found at `tools/dev/gm.py` in your V8 checkout. We recommend adding an alias to your shell configuration:

```bash
alias gm=/path/to/v8/tools/dev/gm.py
```

You can then use `gm` to build V8 for known configurations, such as `x64.release`:

```bash
gm x64.release
```

To run the tests right after the build, run:

```bash
gm x64.release.check
```

`gm` outputs all the commands it’s executing, making it easy to track and re-execute them if necessary.

`gm` enables building the required binaries and running specific tests with a single command:

```bash
gm x64.debug mjsunit/foo cctest/test-bar/*
```

## Building V8: the raw, manual workflow { #manual }

### Step 1: generate build files { #generate-build-files }

There are several ways of generating the build files:

1. The raw, manual workflow involves using `gn` directly.
1. A helper script named `v8gen` streamlines the process for common configurations.

#### Generating build files using `gn`  { #gn }

Generate build files for the directory `out/foo` using `gn`:

```bash
gn args out/foo
```

This opens an editor window for specifying the [`gn` arguments](https://gn.googlesource.com/gn/+/main/docs/reference.md). Alternatively, you can pass the arguments on the command line:

```bash
gn gen out/foo --args='is_debug=false target_cpu="x64" v8_target_cpu="arm64"'
```

This generates build files for compiling V8 with the arm64 simulator in release mode.

For an overview of all available `gn` arguments, run:

```bash
gn args out/foo --list
```

#### Generate build files using `v8gen` { #v8gen }

The V8 repository includes a `v8gen` convenience script to more easily generate build files for common configurations. We recommend adding an alias to your shell configuration:

```bash
alias v8gen=/path/to/v8/tools/dev/v8gen.py
```

Call `v8gen --help` for more information.

List available configurations (or bots from a master):

```bash
v8gen list
```

```bash
v8gen list -m client.v8
```

Build like a particular bot from the `client.v8` waterfall in folder `foo`:

```bash
v8gen -b 'V8 Linux64 - debug builder' -m client.v8 foo
```

### Step 2: compile V8 { #compile }

To build all of V8 (assuming `gn` generated to the `x64.release` folder), run:

```bash
ninja -C out/x64.release
```

To build specific targets like `d8`, append them to the command:

```bash
ninja -C out/x64.release d8
```

### Step 3: run tests { #tests }

You can pass the output directory to the test driver. Other relevant flags are inferred from the build:

```bash
tools/run-tests.py --outdir out/foo
```

You can also test your most recently compiled build (in `out.gn`):

```bash
tools/run-tests.py --gn
```

**Build issues? File a bug at [v8.dev/bug](/bug) or ask for help on <v8-users@googlegroups.com>.**
