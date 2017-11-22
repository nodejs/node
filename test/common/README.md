# Node.js Core Test Common Modules

This directory contains modules used to test the Node.js implementation.

## Table of Contents

* [Common module API](#common-module-api)
* [Duplex pair helper](#duplex-pair-helper)
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

### canCreateSymLink()
* return [&lt;Boolean>]

Checks whether the current running process can create symlinks. On Windows, this
returns `false` if the process running doesn't have privileges to create
symlinks
([SeCreateSymbolicLinkPrivilege](https://msdn.microsoft.com/en-us/library/windows/desktop/bb530716(v=vs.85).aspx)).
On non-Windows platforms, this always returns `true`.

### crashOnUnhandledRejection()

Installs a `process.on('unhandledRejection')` handler that crashes the process
after a tick. This is useful for tests that use Promises and need to make sure
no unexpected rejections occur, because currently they result in silent
failures.

### ddCommand(filename, kilobytes)
* return [&lt;Object>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object)

Platform normalizes the `dd` command

### enoughTestMem
* [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Indicates if there is more than 1gb of total memory.

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
* [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)

Path to the 'fixtures' directory.

### getArrayBufferViews(buf)
* `buf` [&lt;Buffer>](https://nodejs.org/api/buffer.html#buffer_class_buffer)
* return [&lt;ArrayBufferView&#91;&#93;>](https://developer.mozilla.org/en-US/docs/Web/API/ArrayBufferView)

Returns an instance of all possible `ArrayBufferView`s of the provided Buffer.

### getCallSite(func)
* `func` [&lt;Function>]
* return [&lt;String>]

Returns the file name and line number for the provided Function.

### globalCheck
* [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Set to `false` if the test should not check for global leaks.

### hasCrypto
* [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Indicates whether OpenSSL is available.

### hasFipsCrypto
* [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Indicates `hasCrypto` and `crypto` with fips.

### hasIPv6
* [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Indicates whether `IPv6` is supported on this platform.

### hasMultiLocalhost
* [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Indicates if there are multiple localhosts available.

### inFreeBSDJail
* [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Checks whether free BSD Jail is true or false.

### isAix
* [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Platform check for Advanced Interactive eXecutive (AIX).

### isAlive(pid)
* `pid` [&lt;Number>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Number_type)
* return [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Attempts to 'kill' `pid`

### isFreeBSD
* [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Platform check for Free BSD.

### isLinux
* [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Platform check for Linux.

### isLinuxPPCBE
* [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Platform check for Linux on PowerPC.

### isOSX
* [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Platform check for macOS.

### isSunOS
* [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Platform check for SunOS.

### isWindows
* [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Platform check for Windows.

### isWOW64
* [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Platform check for Windows 32-bit on Windows 64-bit.

### leakedGlobals
* [&lt;Array>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array)

Indicates whether any globals are not on the `knownGlobals` list.

### localhostIPv4
* [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)

IP of `localhost`.

### localIPv6Hosts
* [&lt;Array>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array)

Array of IPV6 representations for `localhost`.

### mustCall([fn][, exact])
* `fn` [&lt;Function>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Function) default = () => {}
* `exact` [&lt;Number>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Number_type) default = 1
* return [&lt;Function>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Function)

Returns a function that calls `fn`. If the returned function has not been called
exactly `expected` number of times when the test is complete, then the test will
fail.

If `fn` is not provided, an empty function will be used.

### mustCallAtLeast([fn][, minimum])
* `fn` [&lt;Function>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Function) default = () => {}
* `minimum` [&lt;Number>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Number_type) default = 1
* return [&lt;Function>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Function)

Returns a function that calls `fn`. If the returned function has not been called
at least `minimum` number of times when the test is complete, then the test will
fail.

If `fn` is not provided, an empty function will be used.

### mustNotCall([msg])
* `msg` [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type) default = 'function should not have been called'
* return [&lt;Function>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Function)

Returns a function that triggers an `AssertionError` if it is invoked. `msg` is used as the error message for the `AssertionError`.

### nodeProcessAborted(exitCode, signal)
* `exitCode` [&lt;Number>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Number_type)
* `signal` [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)
* return [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Returns `true` if the exit code `exitCode` and/or signal name `signal` represent the exit code and/or signal name of a node process that aborted, `false` otherwise.

### opensslCli
* [&lt;Boolean>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type)

Indicates whether 'opensslCli' is supported.

### platformTimeout(ms)
* `ms` [&lt;Number>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Number_type)
* return [&lt;Number>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Number_type)

Platform normalizes timeout.

### PIPE
* [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)

Path to the test socket.

### PORT
* [&lt;Number>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Number_type) default = `12346`

A port number for tests to use if one is needed.

### printSkipMessage(msg)
* `msg` [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)

Logs '1..0 # Skipped: ' + `msg`

### refreshTmpDir
* return [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)

Deletes the 'tmp' dir and recreates it

### rootDir
* [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)

Path to the 'root' directory. either `/` or `c:\\` (windows)

### skip(msg)
* `msg` [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)

Logs '1..0 # Skipped: ' + `msg` and exits with exit code `0`.

### spawnPwd(options)
* `options` [&lt;Object>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object)
* return [&lt;Object>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object)

Platform normalizes the `pwd` command.

### spawnSyncPwd(options)
* `options` [&lt;Object>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object)
* return [&lt;Object>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object)

Synchronous version of `spawnPwd`.

### tmpDir
* [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)

The realpath of the 'tmp' directory.

### tmpDirName
* return [&lt;String>](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type)

Name of the temp directory used by tests.

## Countdown Module

The `Countdown` module provides a simple countdown mechanism for tests that
require a particular action to be taken after a given number of completed
tasks (for instance, shutting down an HTTP server after a specific number of
requests).

<!-- eslint-disable strict, required-modules -->
```js
const Countdown = require('../common/countdown');

function doSomething() {
  console.log('.');
}

const countdown = new Countdown(2, doSomething);
countdown.dec();
countdown.dec();
```

### new Countdown(limit, callback)

* `limit` {number}
* `callback` {function}

Creates a new `Countdown` instance.

### Countdown.prototype.dec()

Decrements the `Countdown` counter.

### Coutndown.prototype.remaining

Specifies the remaining number of times `Countdown.prototype.dec()` must be
called before the callback is invoked.

## Duplex pair helper

The `common/duplexpair` module exports a single function `makeDuplexPair`,
which returns an object `{ clientSide, serverSide }` where each side is a
`Duplex` stream connected to the other side.

There is no difference between client or server side beyond their names.

## Fixtures Module

The `common/fixtures` module provides convenience methods for working with
files in the `test/fixtures` directory.

### fixtures.fixturesDir

* [&lt;String>]

The absolute path to the `test/fixtures/` directory.

### fixtures.path(...args)

* `...args` [&lt;String>]

Returns the result of `path.join(fixtures.fixturesDir, ...args)`.

### fixtures.readSync(args[, enc])

* `args` [&lt;String>] | [&lt;Array>]

Returns the result of
`fs.readFileSync(path.join(fixtures.fixturesDir, ...args), 'enc')`.

### fixtures.readKey(arg[, enc])

* `arg` [&lt;String>]

Returns the result of
`fs.readFileSync(path.join(fixtures.fixturesDir, 'keys', arg), 'enc')`.

## WPT Module

The wpt.js module is a port of parts of
[W3C testharness.js](https://github.com/w3c/testharness.js) for testing the
Node.js
[WHATWG URL API](https://nodejs.org/api/url.html#url_the_whatwg_url_api)
implementation with tests from
[W3C Web Platform Tests](https://github.com/w3c/web-platform-tests).
