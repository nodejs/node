---
title: 'Profiling Chromium with V8'
description: 'This document explains how to use V8’s CPU and heap profilers with Chromium.'
---
[V8’s CPU & heap profilers](/docs/profile) are trivial to use from V8’s shells, but it may appear confusing how to use them with Chromium. This page should help you with it.

## Why is using V8’s profilers with Chromium different from using them with V8 shells?

Chromium is a complex application, unlike V8 shells. Below is the list of Chromium features that affect profiler usage:

- each renderer is a separate process (OK, not actually each, but let’s omit this detail), so they can’t share the same log file;
- sandbox built around renderer process prevents it from writing to a disk;
- Developer Tools configure profilers for their own purposes;
- V8’s logging code contains some optimizations to simplify logging state checks.

## How to run Chromium to get a CPU profile?

Here is how to run Chromium in order to get a CPU profile from the start of the process:

```bash
./Chromium --no-sandbox --user-data-dir=`mktemp -d` --incognito --js-flags='--prof'
```

Please note that you wouldn’t see profiles in Developer Tools, because all the data is being logged to a file, not to Developer Tools.

### Flags description

`--no-sandbox` turns off the renderer sandbox so chrome can write to the log file.

`--user-data-dir` is used to create a fresh profile, use this to avoid caches and potential side-effects from installed extensions (optional).

`--incognito` is used to further prevent pollution of your results (optional).

`--js-flags` contains the flags passed to V8:

- `--logfile=%t.log` specifies a name pattern for log files. `%t` gets expanded into the current time in milliseconds, so each process gets its own log file. You can use prefixes and suffixes if you want, like this: `prefix-%t-suffix.log`. By default every isolate gets a separate log file.
- `--prof` tells V8 to write statistical profiling information into the log file.

## Android

Chrome on Android has a number of unique points that make it a bit more complex to profile.

- The command line must be written via `adb` before starting Chrome on the device. As a result, quotes in the command line sometimes get lost, and it is best to separate arguments in `--js-flags` with a comma rather than trying to use whitespace and quotes.
- The path for the logfile must be specified as an absolute path to somewhere that is writable on Android’s filesystem.
- The sandboxing used for renderer processes on Android means that even with `--no-sandbox`, the renderer process still can’t write to files on the filesystem, therefore `--single-process` needs to be passed to run the renderer in the same process as the browser process.
- The `.so` is embedded in Chrome’s APK which means symbolization needs to convert from APK memory addresses to the unstripped `.so` file in the builds.

The following commands enable profiling on Android:

```bash
./build/android/adb_chrome_public_command_line --no-sandbox --single-process --js-flags='--logfile=/storage/emulated/0/Download/%t.log,--prof'
<Close and relaunch Chome on the Android device>
adb pull /storage/emulated/0/Download/<logfile>
./src/v8/tools/linux-tick-processor --apk-embedded-library=out/Release/lib.unstripped/libchrome.so --preprocess <logfile>
```

## Notes

Under Windows, be sure to turn on `.MAP` file creation for `chrome.dll`, but not for `chrome.exe`.
