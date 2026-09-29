---
title: 'Evaluating code coverage'
description: 'This document explains what to do if you’re working on a change in V8 and you want to evaluate its code coverage.'
---
You are working on a change. You want to evaluate code coverage for your new code.

V8 provides two tools for doing this: local, on your machine; and build infrastructure support.

## Local

Relative to the root of the v8 repo, use `./tools/gcov.sh` (tested on linux). This uses gnu’s code coverage tooling and some scripting to produce an HTML report, where you can drill down coverage info per directory, file, and then down to line of code.

The script builds V8 under a separate `out` directory, using `gcov` settings. We use a separate directory to avoid clobbering your normal build settings. This separate directory is called `cov` — it is created immediately under the repo root. `gcov.sh` then runs the test suite, and produces the report. The path to the report is provided when the script completes.

If your change has architecture specific components, you can cumulatively collect coverage from architecture specific runs.

```bash
./tools/gcov.sh x64 arm
```

This rebuilds in-place for each architecture, clobbering the binaries from the previous run, but preserving and accumulating over the coverage results.

By default, the script collects from `Release` runs. If you want `Debug`, you may specify so:

```bash
BUILD_TYPE=Debug ./tools/gcov.sh x64 arm arm64
```

Running the script with no options will provide a summary of options as well.

## Code coverage bot

For each change that landed, we run a x64 coverage analysis — see the [coverage bot](https://ci.chromium.org/p/v8/builders/luci.v8.ci/V8%20Linux64%20-%20gcov%20coverage). We don't run bots for coverage for other architectures.

To get the report for a particular run, you want to list the build steps, find the “gsutil coverage report” one (towards the end), and open the “report” under it.
