---
title: 'V8’s Linux `perf` integration'
description: 'This document explains how to analyze the performance of V8’s JITted code with the Linux `perf` tool.'
---

V8 has built-in support for the Linux `perf` tool. It is enabled by the
`--perf-prof` command-line options.
V8 writes out performance data during execution into a file that can be used to
analyze the performance of V8’s JITted code (including JS-function names) with
the Linux `perf` tool.

## Requirements

- `linux-perf` version 5 or higher (previous versions don't have JIT support).
- Build V8/Chrome with `enable_profiling=true` for better symbolized C++ code.

## Building V8

To use V8’s integration with Linux perf you need to build it with the
`enable_profiling = true` gn flag:

```bash
echo 'enable_profiling = true' >> out/x64.release/args.gn
autoninja -C out/x64.release
```

## Profiling `d8` with [`linux-perf-d8.py`](https://source.chromium.org/search?q=linux-perf-d8.py)

After building `d8`, you can start using linux perf:

```bash
tools/profiling/linux-perf-d8.py out/x64.release/d8 path/to/test.js;
```

A more complete example:

```bash
echo '(function f() {
    var s = 0; for (var i = 0; i < 1000000000; i++) { s += i; } return s;
  })();' > test.js;

# Use custom V8 flags and a separate output dir for less clutter:
mkdir perf_results
tools/profiling/linux-perf-d8.py --perf-data-dir=perf_results \
    out/x64.release/d8 --expose-gc --allow-natives-syntax test.js;

# Fancy UI (`-flame` is googler-only, use `-web` as a public alternative):
pprof -flame perf_results/XXX_perf.data.jitted;
# Terminal-based tool:
perf report -i perf_results/XXX_perf.data.jitted;
```

Check `linux-perf-d8.py --help` for more details. Note that you can use all
`d8` flags after the d8 binary argument.

## Profiling Chrome or content_shell with crossbench

We recommend using [crossbench](https://chromium.googlesource.com/crossbench)
for profiling Chrome on all platforms (which uses linux-perf on linux).

```bash
$CHROME_CHECKOUT/src/tools/perf/cb loading \
  --browser=out/x64.release/chrome \
  --probe=profiling \
  --url=http://test.com
```

## Profiling Chrome or content_shell on Linux with [linux-perf-chrome.py](https://source.chromium.org/search?q=linux-perf-chrome.py)

1. You can use the [linux-perf-chrome.py](https://source.chromium.org/search?q=linux-perf-chrome.py)
   script to profile Chrome. Make sure to add the [required chrome gn flags](https://chromium.googlesource.com/chromium/src/+/main/docs/profiling.md#General-checkout-setup)
   to get proper C++ symbols.

2. Once your build is ready, you can profile a website with both full symbols
   for C++ and JS code.

    ```bash
    mkdir perf_results;
    tools/profiling/linux-perf-chrome.py out/x64.release/chrome \
        --perf-data-dir=perf_results --timeout=30
    ```

3. Navigate to your website and then close the browser (or wait for the
   `--timeout` to complete).

4. After quitting the browser, `linux-perf-chrome.py` will post-process the
   files and show a list with a result file for each renderer process:

   ```
   chrome_renderer_1583105_3.perf.data.jitted      19.79MiB
   chrome_renderer_1583105_2.perf.data.jitted       8.59MiB
   chrome_renderer_1583105_4.perf.data.jitted       0.18MiB
   chrome_renderer_1583105_1.perf.data.jitted       0.16MiB
   ```

## Exploring linux-perf results

Finally, you can use the Linux `perf` tool to explore the profile of a `d8` or
Chrome renderer process:

```bash
perf report -i perf_results/XXX_perf.data.jitted
```

You can also use [pprof](https://github.com/google/pprof) to generate more
visualizations:

```bash
# Note: `-flame` is google-only, use `-web` as a public alternative:
pprof -flame perf_results/XXX_perf.data.jitted;
```

### V8 linux-perf Flags

[`--perf-prof`](https://source.chromium.org/search?q=FLAG_perf_prof) is used on
the V8 command-line to record performance samples in JIT code.

[`--interpreted-frames-native-stack`](https://source.chromium.org/search?q=FLAG_interpreted_frames_native_stack)
is used to create different entry points (copied versions of
InterpreterEntryTrampoline) for interpreted functions so they can be
distinguished by `perf` based on the address alone. Since the
InterpreterEntryTrampoline has to be copied, this comes at a slight performance
and memory regression.
