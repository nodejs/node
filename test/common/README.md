# Node.js Core Test Common Modules

This directory contains modules used to test the Node.js implementation.

## Table of Contents

* [Common module API](#common-module-api)
* [WPT module](#wpt-module)

## Common Module API

The `common` module is used by tests for consistency across repeated
tasks.

### allowGlobals(...whitelist)
* `whitelist` [&lt;Array>] Array of Globals
* return [&lt;Array>]

Takes `whitelist` and concats that with predefined `knownGlobals`.

### arrayStream
A stream to push an array into a REPL

### busyLoop(time)
* `time` [&lt;Number>]

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
* return [&lt;Object>]

Platform normalizes the `dd` command

### enoughTestMem
* return [&lt;Boolean>]

Check if there is more than 1gb of total memory.

### expectsError([fn, ]settings[, exact])
* `fn` [&lt;Function>] a function that should throw.
* `settings` [&lt;Object>]
  that must contain the `code` property plus any of the other following
  properties (some properties only apply for `AssertionError`):
  * `code` [&lt;String>]
    expected error must have this value for its `code` property.
  * `type` [&lt;Function>]
    expected error must be an instance of `type` and must be an Error subclass.
  * `message` [&lt;String>] or [&lt;RegExp>]
    if a string is provided for `message`, expected error must have it for its
    `message` property; if a regular expression is provided for `message`, the
    regular expression must match the `message` property of the expected error.
  * `name` [&lt;String>]
    expected error must have this value for its `name` property.
  * `generatedMessage` [&lt;String>]
    (`AssertionError` only) expected error must have this value for its
    `generatedMessage` property.
  * `actual` &lt;any>
    (`AssertionError` only) expected error must have this value for its
    `actual` property.
  * `expected` &lt;any>
    (`AssertionError` only) expected error must have this value for its
    `expected` property.
  * `operator` &lt;any>
    (`AssertionError` only) expected error must have this value for its
    `operator` property.
* `exact` [&lt;Number>] default = 1
* return [&lt;Function>]

  If `fn` is provided, it will be passed to `assert.throws` as first argument
  and `undefined` will be returned.
  Otherwise a function suitable as callback or for use as a validation function
  passed as the second argument to `assert.throws()` will be returned. If the
  returned function has not been called exactly `exact` number of times when the
  test is complete, then the test will fail.

### expectWarning(name, expected)
* `name` [&lt;String>]
* `expected` [&lt;String>] | [&lt;Array>]

Tests whether `name` and `expected` are part of a raised warning.

### fileExists(pathname)
* pathname [&lt;String>]
* return [&lt;Boolean>]

Checks if `pathname` exists

### fires(promise, [error], [timeoutMs])
* promise [&lt;Promise]
* error [&lt;String] default = 'timeout'
* timeoutMs [&lt;Number] default = 100

Returns a new promise that will propagate `promise` resolution or rejection if
that happens within the `timeoutMs` timespan, or rejects with `error` as
a reason otherwise.

### fixturesDir
* return [&lt;String>]

Path to the 'fixtures' directory.

### getArrayBufferViews(buf)
* `buf` [&lt;Buffer>]
* return [&lt;ArrayBufferView&#91;&#93;>]

Returns an instance of all possible `ArrayBufferView`s of the provided Buffer.

### globalCheck
* return [&lt;Boolean>]

Turn this off if the test should not check for global leaks.

### hasCrypto
* return [&lt;Boolean>]

Checks for 'openssl'.

### hasFipsCrypto
* return [&lt;Boolean>]

Checks `hasCrypto` and `crypto` with fips.

### hasIntl
* return [&lt;Boolean>]

Checks if [internationalization] is supported.

### hasSmallICU
* return [&lt;Boolean>]

Checks `hasIntl` and `small-icu` is supported.

### hasIPv6
* return [&lt;Boolean>]

Checks whether `IPv6` is supported on this platform.

### hasMultiLocalhost
* return [&lt;Boolean>]

Checks if there are multiple localhosts available.

### hijackStderr(listener)
* `listener` [&lt;Function>]: a listener with a single parameter
  called `data`.

Eavesdrop to `process.stderr.write` calls. Once `process.stderr.write` is
called, `listener` will also be called and the `data` of `write` function will
be passed to `listener`. What's more, `process.stderr.writeTimes` is a count of
the number of calls.

### hijackStdout(listener)
* `listener` [&lt;Function>]: a listener with a single parameter
  called `data`.

Eavesdrop to `process.stdout.write` calls. Once `process.stdout.write` is
called, `listener` will also be called and the `data` of `write` function will
be passed to `listener`. What's more, `process.stdout.writeTimes` is a count of
the number of calls.

### inFreeBSDJail
* return [&lt;Boolean>]

Checks whether free BSD Jail is true or false.

### isAIX
* return [&lt;Boolean>]

Platform check for Advanced Interactive eXecutive (AIX).

### isAlive(pid)
* `pid` [&lt;Number>]
* return [&lt;Boolean>]

Attempts to 'kill' `pid`

### isFreeBSD
* return [&lt;Boolean>]

Platform check for Free BSD.

### isLinux
* return [&lt;Boolean>]

Platform check for Linux.

### isLinuxPPCBE
* return [&lt;Boolean>]

Platform check for Linux on PowerPC.

### isOSX
* return [&lt;Boolean>]

Platform check for macOS.

### isSunOS
* return [&lt;Boolean>]

Platform check for SunOS.

### isWindows
* return [&lt;Boolean>]

Platform check for Windows.

### isWOW64
* return [&lt;Boolean>]

Platform check for Windows 32-bit on Windows 64-bit.

### leakedGlobals
* return [&lt;Array>]

Checks whether any globals are not on the `knownGlobals` list.

### localhostIPv4
* return [&lt;String>]

Gets IP of localhost

### localIPv6Hosts
* return [&lt;Array>]

Array of IPV6 hosts.

### mustCall([fn][, exact])
* `fn` [&lt;Function>] default = () => {}
* `exact` [&lt;Number>] default = 1
* return [&lt;Function>]

Returns a function that calls `fn`. If the returned function has not been called
exactly `exact` number of times when the test is complete, then the test will
fail.

If `fn` is not provided, an empty function will be used.

### mustCallAtLeast([fn][, minimum])
* `fn` [&lt;Function>] default = () => {}
* `minimum` [&lt;Number>] default = 1
* return [&lt;Function>]

Returns a function that calls `fn`. If the returned function has not been called
at least `minimum` number of times when the test is complete, then the test will
fail.

If `fn` is not provided, an empty function will be used.

### mustNotCall([msg])
* `msg` [&lt;String>] default = 'function should not have been called'
* return [&lt;Function>]

Returns a function that triggers an `AssertionError` if it is invoked. `msg` is
used as the error message for the `AssertionError`.

### nodeProcessAborted(exitCode, signal)
* `exitCode` [&lt;Number>]
* `signal` [&lt;String>]
* return [&lt;Boolean>]

Returns `true` if the exit code `exitCode` and/or signal name `signal` represent
the exit code and/or signal name of a node process that aborted, `false`
otherwise.

### opensslCli
* return [&lt;Boolean>]

Checks whether 'opensslCli' is supported.

### platformTimeout(ms)
* `ms` [&lt;Number>]
* return [&lt;Number>]

Platform normalizes timeout.

### PIPE
* return [&lt;String>]

Path to the test sock.

### PORT
* return [&lt;Number>] default = `12346`

Port tests are running on.

### printSkipMessage(msg)
* `msg` [&lt;String>]

Logs '1..0 # Skipped: ' + `msg`

### refreshTmpDir
* return [&lt;String>]

Deletes the 'tmp' dir and recreates it

### restoreStderr()

Restore the original `process.stderr.write`.

### restoreStdout()

Restore the original `process.stdout.write`.

### rootDir
* return [&lt;String>]

Path to the 'root' directory. either `/` or `c:\\` (windows)

### skip(msg)
* `msg` [&lt;String>]

Logs '1..0 # Skipped: ' + `msg` and exits with exit code `0`.

### skipIfInspectorDisabled()

Skip the rest of the tests in the current file when the Inspector
was disabled at compile time.

### skipIf32Bits()

Skip the rest of the tests in the current file when the Node.js executable
was compiled with a pointer size smaller than 64 bits.

### spawnPwd(options)
* `options` [&lt;Object>]
* return [&lt;Object>]

Platform normalizes the `pwd` command.

### spawnSyncPwd(options)
* `options` [&lt;Object>]
* return [&lt;Object>]

Synchronous version of `spawnPwd`.

### tmpDir
* return [&lt;String>]

The realpath of the 'tmp' directory.

### tmpDirName
* return [&lt;String>]

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

[&lt;Array>]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array
[&lt;ArrayBufferView&#91;&#93;>]: https://developer.mozilla.org/en-US/docs/Web/API/ArrayBufferView
[&lt;Boolean>]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type
[&lt;Buffer>]: https://nodejs.org/api/buffer.html#buffer_class_buffer
[&lt;Function>]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Function
[&lt;Number>]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Number_type
[&lt;Object>]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object
[&lt;RegExp>]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp
[&lt;String>]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type
[internationalization]: https://github.com/nodejs/node/wiki/Intl
