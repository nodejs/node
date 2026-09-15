---
title: 'Building V8 from source'
description: 'This document explains how to build V8 from source.'
---
In order to be able to build V8 from scratch on Windows/Linux/macOS for x64, please follow the following steps.

## Getting the V8 source code

Follow the instructions in our guide on [checking out the V8 source code](/docs/source-code).

## Installing build dependencies

1. For macOS: install Xcode and accept its license agreement. (If you’ve installed the command-line tools separately, [remove them first](https://bugs.chromium.org/p/chromium/issues/detail?id=729990#c1).)

1. Make sure that you are in the V8 source directory. If you followed every step in the previous section, you’re already at the right location.

1. Download all the build dependencies:

   ```bash
   gclient sync
   ```

   For Googlers - If you see Failed to fetch file or Login required errors when running the hooks, try authenticating with Google Storage first by running:

   ```bash
   gsutil.py config
   ```

   Login with your @google.com account, and enter `0` when asked for a project ID.

1. This step is only needed on Linux. Install additional build dependencies:

    ```bash
    ./build/install-build-deps.sh
    ```

## Building V8

1. Make sure that you are in the V8 source directory on the `main` branch.

    ```bash
    cd /path/to/v8
    ```

1. Pull in the latest changes and install any new build dependencies:

    ```bash
    git pull && gclient sync
    ```

1. Compile the source:

    ```bash
    tools/dev/gm.py x64.release
    ```

    Or, to compile the source and immediately run the tests:

    ```bash
    tools/dev/gm.py x64.release.check
    ```

    For more information on the `gm.py` helper script and the commands it triggers, see [Building with GN](/docs/build-gn).
