# Node.js Core Test Common Modules

This directory contains modules used to test the Node.js implementation.

## Table of Contents

* [Common module API](#common-module-api)
* [WPT module](#wpt-module)

## Common Module API

The `common` module is used by tests for consistency across repeated
tasks.

### allowGlobals(...whitelist)
* `whitelist` [&lt;Array>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array) Array of Globals
* return [&lt;Array>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array)

Takes `whitelist` and concats that with predefined `knownGlobals`.

### arrayStream
A stream to push an array into a REPL

### busyLoop(time)
* `time` [&lt;Number>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Number_type)

Blocks for `time` amount of time.

### canCreateSymLink
API to indicate whether the current running process can create
symlinks. On Windows, this returns false if the process running
doesn't have privileges to create symlinks (specifically
[SeCreateSymbolicLinkPrivilege](https://msdn.microsoft.com/en-us/library/windows/desktop/bb530716(v=vs.85).aspx)).
On non-Windows platforms, this currently returns true.

### crashOnUnhandledRejection()

Installs a `process.on('unhandledRejection')` handler that crashes the process
after a tick. This is useful for tests that use Promises and need to make sure
no unexpected rejections occur, because currently they result in silent
failures.

### ddCommand(filename, kilobytes)
* return [&lt;Object>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object)

Platform normalizes the `dd` command

### enoughTestMem
* return [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Check if there is more than 1gb of total memory.

### expectsError(settings)
* `settings` [&lt;Object>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object)
  with the following optional properties:
  * `code` [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)
    expected error must have this value for its `code` property
  * `type` [&lt;Function>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Function)
    expected error must be an instance of `type`
  * `message` [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)
    or [&lt;RegExp>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp)
    if a string is provided for `message`, expected error must have it for its
    `message` property; if a regular expression is provided for `message`, the
    regular expression must match the `message` property of the expected error

* return function suitable for use as a validation function passed as the second
  argument to `assert.throws()`

The expected error should be [subclassed by the `internal/errors` module](https://github.com/nodejs/node/blob/master/doc/guides/using-internal-errors.md#api).

### expectWarning(name, expected)
* `name` [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)
* `expected` [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type) | [&lt;Array>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array)

Tests whether `name` and `expected` are part of a raised warning.

### fileExists(pathname)
* pathname [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)
* return [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Checks if `pathname` exists

### fixturesDir
* return [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)

Path to the 'fixtures' directory.

### getArrayBufferViews(buf)
* `buf` [&lt;Buffer>](https://nodejs.org/api/buffer.html#buffer_class_buffer)
* return [&lt;ArrayBufferView&#91;&#93;>](https://developer.mozilla.org/en-US/docs/Web/API/ArrayBufferView)

Returns an instance of all possible `ArrayBufferView`s of the provided Buffer.

### globalCheck
* return [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Turn this off if the test should not check for global leaks.

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

Platform check for macOS.

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

### mustCall([fn][, exact])
* `fn` [&lt;Function>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Function) default = `common.noop`
* `exact` [&lt;Number>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Number_type) default = 1
* return [&lt;Function>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Function)

Returns a function that calls `fn`. If the returned function has not been called
exactly `expected` number of times when the test is complete, then the test will
fail.

If `fn` is not provided, `common.noop` will be used.

### mustCallAtLeast([fn][, minimum])
* `fn` [&lt;Function>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Function) default = `common.noop`
* `minimum` [&lt;Number>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Number_type) default = 1
* return [&lt;Function>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Function)

Returns a function that calls `fn`. If the returned function has not been called
at least `minimum` number of times when the test is complete, then the test will
fail.

If `fn` is not provided, `common.noop` will be used.

### mustNotCall([msg])
* `msg` [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type) default = 'function should not have been called'
* return [&lt;Function>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Function)

Returns a function that triggers an `AssertionError` if it is invoked. `msg` is used as the error message for the `AssertionError`.

### nodeProcessAborted(exitCode, signal)
* `exitCode` [&lt;Number>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Number_type)
* `signal` [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)
* return [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Returns `true` if the exit code `exitCode` and/or signal name `signal` represent the exit code and/or signal name of a node process that aborted, `false` otherwise.

### noop

A non-op `Function` that can be used for a variety of scenarios.

For instance,

<!-- eslint-disable strict, no-undef -->
```js
const common = require('../common');

someAsyncAPI('foo', common.mustCall(common.noop));
```

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

### spawnPwd(options)
* `options` [&lt;Object>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object)
* return [&lt;Object>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object)

Platform normalizes the `pwd` command.

### spawnSyncPwd(options)
* `options` [&lt;Object>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object)
* return [&lt;Object>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object)

Synchronous version of `spawnPwd`.

### tmpDir
* return [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)

The realpath of the 'tmp' directory.

### tmpDirName
* return [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)

Name of the temp directory used by tests.

## WPT Module

The wpt.js module is a port of parts of
[W3C testharness.js](https://github.com/w3c/testharness.js) for testing the
Node.js
[WHATWG URL API](https://nodejs.org/api/url.html#url_the_whatwg_url_api)
implementation with tests from
[W3C Web Platform Tests](https://github.com/w3c/web-platform-tests).
