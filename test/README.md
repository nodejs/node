# Table of Contents
* [Test directories](#test-directories)
* [Common module API](#common-module-api)

## Test Directories

### abort

Tests for when the `--abort-on-uncaught-exception` flag is used.

| Runs on CI |
|:----------:|
| No         |

### addons

Tests for [addon](https://nodejs.org/api/addons.html) functionality along with
some tests that require an addon to function properly.


| Runs on CI |
|:----------:|
| Yes        |

### cctest

C++ test that is run as part of the build process.

| Runs on CI |
|:----------:|
| Yes        |

### debugger

Tests for [debugger](https://nodejs.org/api/debugger.html) functionality.

| Runs on CI |
|:----------:|
| No         |

### disabled

Tests that have been disabled from running for various reasons.

| Runs on CI |
|:----------:|
| No         |

### fixtures

Test fixtures used in various tests throughout the test suite.

### gc

Tests for garbage collection related functionality.

| Runs on CI |
|:----------:|
| No         |


### inspector

Tests for the V8 inspector integration.

| Runs on CI |
|:----------:|
| Yes        |

### internet

Tests that make real outbound connections (mainly networking related modules).
Tests for networking related modules may also be present in other directories,
but those tests do not make outbound connections.

| Runs on CI |
|:----------:|
| No         |

### known_issues

Tests reproducing known issues within the system.

| Runs on CI |
|:----------:|
| No         |

### message

Tests for messages that are output for various conditions (`console.log`,
error messages etc.)

| Runs on CI |
|:----------:|
| Yes        |

### parallel

Various tests that are able to be run in parallel.

| Runs on CI |
|:----------:|
| Yes        |

### pummel

Various tests for various modules / system functionality operating under load.

| Runs on CI |
|:----------:|
| No         |

### sequential

Various tests that are run sequentially.

| Runs on CI |
|:----------:|
| Yes        |

### testpy

Test configuration utility used by various test suites.

### tick-processor

Tests for the V8 tick processor integration. The tests are for the logic in
`lib/internal/v8_prof_processor.js` and `lib/internal/v8_prof_polyfill.js`. The
tests confirm that the profile processor packages the correct set of scripts
from V8 and introduces the correct platform specific logic.

| Runs on CI |
|:----------:|
| No         |

### timers

Tests for [timing utilities](https://nodejs.org/api/timers.html) (`setTimeout`
and `setInterval`).

| Runs on CI |
|:----------:|
| No         |


## Common module API

The common.js module is used by tests for consistency across repeated
tasks. It has a number of helpful functions and properties to help with
writing tests.

### allowGlobals(...whitelist)
* `whitelist` [&lt;Array>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array) Array of Globals
* return [&lt;Array>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array)

Takes `whitelist` and concats that with predefined `knownGlobals`.

### arrayStream
A stream to push an array into a REPL

### busyLoop(time)
* `time` [&lt;Number>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Number_type)

Blocks for `time` amount of time.

### ddCommand(filename, kilobytes)
* return [&lt;Object>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object)

Platform normalizes the `dd` command

### enoughTestMem
* return [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Check if there is more than 1gb of total memory.

### expectWarning(name, expected)
* `name` [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)
* `expected` [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type) | [&lt;Array>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array)

Tests whether `name` and `expected` are part of a raised warning.

### hasCrypto
* return [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Checks for 'openssl'.

### hasFipsCrypto
* return [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Checks `hasCrypto` and `crypto` with fips.

### hasIPv6
* return [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Checks whether `IPv6` is supported on this platform.

### hasMultiLocalhost
* return [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Checks if there are multiple localhosts available.

### fail(msg)
* `msg` [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)
* return [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Throws an `AssertionError` with `msg`

### fileExists(pathname)
* pathname [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)
* return [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Checks if `pathname` exists

### fixturesDir
* return [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)

Path to the 'fixtures' directory.

### globalCheck
* return [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Turn this off if the test should not check for global leaks.

### inFreeBSDJail
* return [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Checks whether free BSD Jail is true or false.

### isAix
* return [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Platform check for Advanced Interactive eXecutive (AIX).

### isAlive(pid)
* `pid` [&lt;Number>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Number_type)
* return [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Attempts to 'kill' `pid`

### isFreeBSD
* return [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Platform check for Free BSD.

### isLinux
* return [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Platform check for Linux.

### isLinuxPPCBE
* return [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Platform check for Linux on PowerPC.

### isOSX
* return [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Platform check for OS X.

### isSunOS
* return [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Platform check for SunOS.

### isWindows
* return [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Platform check for Windows.

### isWOW64
* return [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Platform check for Windows 32-bit on Windows 64-bit.

### leakedGlobals
* return [&lt;Array>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array)

Checks whether any globals are not on the `knownGlobals` list.

### localhostIPv4
* return [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)

Gets IP of localhost

### localIPv6Hosts
* return [&lt;Array>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array)

Array of IPV6 hosts.

### mustCall(fn[, expected])
* fn [&lt;Function>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Function)
* expected [&lt;Number>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Number_type) default = 1
* return [&lt;Function>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Function)

Returns a function that calls `fn`. If the returned function has not been called
exactly `expected` number of times when the test is complete, then the test will
fail.

### nodeProcessAborted(exitCode, signal)
* `exitCode` [&lt;Number>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Number_type)
* `signal` [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)
* return [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Returns `true` if the exit code `exitCode` and/or signal name `signal` represent the exit code and/or signal name of a node process that aborted, `false` otherwise.

### opensslCli
* return [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Checks whether 'opensslCli' is supported.

### platformTimeout(ms)
* `ms` [&lt;Number>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Number_type)
* return [&lt;Number>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Number_type)

Platform normalizes timeout.

### PIPE
* return [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)

Path to the test sock.

### PORT
* return [&lt;Number>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Number_type) default = `12346`

Port tests are running on.

### refreshTmpDir
* return [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)

Deletes the 'tmp' dir and recreates it

### rootDir
* return [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)

Path to the 'root' directory. either `/` or `c:\\` (windows)

### skip(msg)
* `msg` [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)

Logs '1..0 # Skipped: ' + `msg`

### spawnCat(options)
* `options` [&lt;Object>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object)
* return [&lt;Object>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object)

Platform normalizes the `cat` command.

### spawnPwd(options)
* `options` [&lt;Object>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object)
* return [&lt;Object>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object)

Platform normalizes the `pwd` command.

### spawnSyncCat(options)
* `options` [&lt;Object>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object)
* return [&lt;Object>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object)

Synchronous version of `spawnCat`.

### spawnSyncPwd(options)
* `options` [&lt;Object>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object)
* return [&lt;Object>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object)

Synchronous version of `spawnPwd`.

### tmpDir
* return [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)

Path to the 'tmp' directory.

### tmpDirName
* return [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)

Name of the temp directory used by tests.
