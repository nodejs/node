---
title: 'Cross-compiling for iOS'
description: 'This document explains how to cross-compile V8 for iOS.'
---
This page serves as a brief introduction to building V8 for iOS targets.

## Requirements

- A macOS (OS X) host machine with Xcode installed.
- A 64-bit target iOS device (legacy 32-bit iOS devices are unsupported).
- V8 v7.5 or newer.
- jitless is a hard requirement for iOS (as of Dec. 2020). Therefore please use the flags '--expose_gc --jitless'

## Initial setup

Follow [the instructions to build V8](/docs/build).

Fetch additional tools needed for iOS cross-compilation by adding `target_os` in your `.gclient` configuration file, located in the parent directory of the `v8` source directory:

```python
# [... other contents of .gclient such as the 'solutions' variable ...]
target_os = ['ios']
```

After updating `.gclient`, run `gclient sync` to download the additional tools.

## Manual build

This section shows how to build a monolithic V8 version for use on either a physical iOS device or the Xcode iOS simulator. The output of this build is a `libv8_monolith.a` file that contains all V8 libraries as well as the V8 snapshot.

Set up GN build files by running `gn args out/release-ios` and inserting the following keys:

```python
ios_deployment_target = 10
is_component_build = false
is_debug = false
target_cpu = "arm64"                  # "x64" for a simulator build.
target_os = "ios"
use_custom_libcxx = false             # Use Xcode's libcxx.
v8_enable_i18n_support = false        # Produces a smaller binary.
v8_monolithic = true                  # Enable the v8_monolith target.
v8_use_external_startup_data = false  # The snapshot is included in the binary.
v8_enable_pointer_compression = false # Unsupported on iOS.
```

Now build:

```bash
ninja -C out/release-ios v8_monolith
```

Finally, add the generated `libv8_monolith.a` file to your Xcode project as a static library. For further documentation on embedding V8 in your application, see [Getting started with embedding V8](/docs/embed).
