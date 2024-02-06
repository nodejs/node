# GN Build

Similar to GYP, GN is a build system designed for building Chromium. The
official builds of Node.js are built with GYP, but GN build files are also
provided as an unofficial alternative build system.

Currently the GN build is used by:

1. Electron for building Node.js together with Chromium.
2. V8 for testing the integration of Node.js in CI.

## Files of GN

Node.js contains following GN build files:

* `node.gni` - Public GN arguments for configuring the build.
* `*/BUILD.gn` - This is the file where GN looks for build rules, in Node.js it
  just includes the `unofficial.gni` file to avoid accidental modifications.
* `*/unofficial.gni` - The actual build rules for each component.

## Build with GN

Unlike GYP, the GN tool does not include any built-in rules for compiling a
project, which means projects building with GN must provide their own build
configurations for things like how to invoke a C++ compiler. Chromium related
projects like V8 and skia choose to reuse Chromium's build configurations, and
we are reusing V8's Node.js integration testing repository
([node-ci](https://chromium.googlesource.com/v8/node-ci/)) for building Node.js.

### 1. Install `depot_tools`

The `depot_tools` is a set of tools used by Chromium related projects for
checking out code and managing dependencies, and since we are reusing the infra
of V8, we have to install and add it to `PATH`:

```bash
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH=/path/to/depot_tools:$PATH
```

You can also follow the [official tutorial of
`depot_tools`](https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html).

### 2. Check out code

To check out the latest main branch of Node.js for building, use the `fetch`
tool from `depot_tools`:

```bash
mkdir node_gn
cd node_gn
fetch node
```

You can choose to save some time by omitting git history:

```bash
fetch --no-history node
```

After syncing is done, you will get a directory structure like this:

```console
node_gn/
├── .gclient
├── .gclient_entries
├── ...
└── node
    ├── DEPS
    ├── ...
    ├── build/
    ├── node/
    └── tools/
```

The `node_gn` is a workspace directory, which only contains configurations and
caches of the `gclient` tool from `depot_tools`, and the repository is checked
out at `node_gn/node`.

The `node_gn/node` directory is not the repository of Node.js, it is actually
node-ci, the repository used by V8 for testing integration with Node.js. The
source code of Node.js is checked out at `node_gn/node/node`.

### 3. Build

GN only supports [`ninja`](https://ninja-build.org) for building, so to build
Node.js with GN we have to generate `ninja` build files first and then call
`ninja` to do the building.

The `node-ci` repository provides a script for calling GN:

```bash
./tools/gn-gen.py out/Release
```

which writes `ninja` build files into the `out/Release` directory.

And then you can execute `ninja`:

```bash
ninja -C out/Release node
```

## Status of the GN build

Currently the GN build of Node.js is not fully functioning. It builds for macOS
and Linux, but we are still fixing a few issues to make the Windows build work.
And not all tests are passing using the GN build.

There are also efforts on [making GN build work without using
`depot_tools`](https://github.com/photoionization/node_with_gn), which uses a
[forked version of GN](https://github.com/yue/build-gn).
