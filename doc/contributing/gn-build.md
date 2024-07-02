# Node.js GN build guide

This document is designed to help you set up and use GN (Generate Ninja) as an alternative build system for Node.js
projects. While GN is primarily associated with Chromium, it can also be used for Node.js, though this usage is
unofficial. This guide will walk you through the setup process, current status, and how you can contribute to
improving GN support for Node.js.

## Introduction

GN offers an unofficial alternative build system for Node.js, providing a subset of build configurations. GN builds
are not required for Node.js pull requests, but contributions to improve GN support or address issues are highly
encouraged. GN builds are currently used by Electron for integrating Node.js with Chromium and by V8 for Node.js
integration testing.

## GN support files

Node.js includes several key GN support files for configuring the build:

* `node.gni`: Public GN arguments for configuring the build.
* `*/BUILD.gn`: GN looks for build rules here, typically including `unofficial.gni` to prevent unintended modifications.
* `*/unofficial.gni`: Actual build rules for each component.

## Building with GN

Unlike GYP, GN does not come with built-in rules for compiling projects. Projects using GN must provide their build
configurations, including how to invoke a C++ compiler. Node.js projects can utilize build configurations from V8
and Skia, and V8's Node.js integration testing repository can be used for building Node.js.

### Step 1: Installing `depot_tools`

To reuse V8's infrastructure, you need to install `depot_tools`. These tools, used by Chromium projects, facilitate code
checkout and dependency management. Follow these steps to install and add `depot_tools` to your `PATH`:

```bash
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH=/path/to/depot_tools:$PATH
```

For detailed instructions, refer to the [official `depot_tools` tutorial](https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html).

### Step 2: Checking out Node.js code

To get the latest Node.js main branch for building, use the `fetch` tool from `depot_tools`:

```bash
mkdir node_gn
cd node_gn
fetch node
```

You can speed up the process by omitting git history:

```bash
fetch --no-history node
```

After syncing, your directory structure will look like this:

```text
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

The `node_gn` directory serves as the workspace containing `gclient` configurations and caches,
with the repository located at `node_gn/node`.

### Step 3: Building

GN exclusively supports [`ninja`](https://ninja-build.org) for building. To build Node.js with GN,
first generate `ninja` build files, then invoke `ninja` to perform the actual build.

Run the provided script in the `node-ci` repository to call GN:

```bash
cd node  # Navigate to `node_gn/node` containing the node-ci checkout
./tools/gn-gen.py out/Release
```

This generates `ninja` build files in `node_gn/node/out/Release`.

Then, execute `ninja` to build Node.js:

```bash
ninja -C out/Release node
```

After the build completes, the compiled Node.js executable will be located in `out/Release/node`.

## GN build status

Currently, the GN build for Node.js is partially functional, supporting macOS and Linux.
Windows support is still under development, and some tests may fail with the GN build.
Efforts are ongoing to enable GN builds without relying on `depot_tools`, which you can track in
issue [#51689](https://github.com/nodejs/node/issues/51689). Your contributions to advancing the
GN build for Node.js are highly appreciated.
