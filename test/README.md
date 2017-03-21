# Node.js Core Tests

This folder contains code and data used to test the Node.js implementation.

For a detailed guide on how to write tests in this
directory, see [the guide on writing tests](../doc/guides/writing-tests.md).

On how to run tests in this direcotry, see
[the contributing guide](../CONTRIBUTING.md#step-5-test).

## Table of Contents

* [Test directories](#test-directories)
* [Common module API](#common-module-api)

## Test Directories

<table>
  <thead>
    <tr>
      <th>Directory</th>
      <th>Runs on CI</th>
      <th>Purpose</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>abort</td>
      <td>No</td>
      <td>
        Tests for when the <code>--abort-on-uncaught-exception</code>
        flag is used.
      </td>
    </tr>
    <tr>
      <td>addons</td>
      <td>Yes</td>
      <td>
        Tests for <a href="https://nodejs.org/api/addons.html">addon</a>
        functionality along with some tests that require an addon to function
        properly.
      </td>
    </tr>
    <tr>
      <td>cctest</td>
      <td>Yes</td>
      <td>
        C++ test that is run as part of the build process.
      </td>
    </tr>
    <tr>
      <td>debugger</td>
      <td>No</td>
      <td>
        Tests for <a href="https://nodejs.org/api/debugger.html">debugger</a>
        functionality along with some tests that require an addon to function
        properly.
      </td>
    </tr>
    <tr>
      <td>disabled</td>
      <td>No</td>
      <td>
        Tests that have been disabled from running for various reasons.
      </td>
    </tr>
    <tr>
      <td>fixtures</td>
      <td></td>
      <td>Test fixtures used in various tests throughout the test suite.</td>
    </tr>
    <tr>
      <td>gc</td>
      <td>No</td>
      <td>Tests for garbage collection related functionality.</td>
    </tr>
    <tr>
      <td>inspector</td>
      <td>Yes</td>
      <td>Tests for the V8 inspector integration.</td>
    </tr>
    <tr>
      <td>internet</td>
      <td>No</td>
      <td>
        Tests that make real outbound connections (mainly networking related
        modules). Tests for networking related modules may also be present in
        other directories, but those tests do not make outbound connections.
      </td>
    </tr>
    <tr>
      <td>known_issues</td>
      <td>No</td>
      <td>Tests reproducing known issues within the system.</td>
    </tr>
    <tr>
      <td>message</td>
      <td>Yes</td>
      <td>
        Tests for messages that are output for various conditions
        (<code>console.log</code>, error messages etc.)</td>
    </tr>
    <tr>
      <td>parallel</td>
      <td>Yes</td>
      <td>Various tests that are able to be run in parallel.</td>
    </tr>
    <tr>
      <td>pseudo-tty</td>
      <td>Yes</td>
      <td>Tests that require stdin/stdout/stderr to be a TTY.</td>
    </tr>
    <tr>
      <td>pummel</td>
      <td>No</td>
      <td>
        Various tests for various modules / system functionality operating
        under load.
      </td>
    </tr>
    <tr>
      <td>sequential</td>
      <td>Yes</td>
      <td>
        Various tests that are run sequentially.
      </td>
    </tr>
    <tr>
      <td>testpy</td>
      <td></td>
      <td>
        Test configuration utility used by various test suites.
      </td>
    </tr>
    <tr>
      <td>tick-processor</td>
      <td>No</td>
      <td>
        Tests for the V8 tick processor integration. The tests are for the
        logic in <code>lib/internal/v8_prof_processor.js</code> and
        <code>lib/internal/v8_prof_polyfill.js</code>. The tests confirm that
        the profile processor packages the correct set of scripts from V8 and
        introduces the correct platform specific logic.
      </td>
    </tr>
    <tr>
      <td>timers</td>
      <td>No</td>
      <td>
        Tests for
        <a href="https://nodejs.org/api/timers.html">timing utilities</a>
        (<code>setTimeout</code> and <code>setInterval</code>).
      </td>
    </tr>
  </tbody>
</table>

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

### canCreateSymLink
API to indicate whether the current running process can create
symlinks. On Windows, this returns false if the process running
doesn't have privileges to create symlinks (specifically
[SeCreateSymbolicLinkPrivilege](https://msdn.microsoft.com/en-us/library/windows/desktop/bb530716(v=vs.85).aspx)).
On non-Windows platforms, this currently returns true.

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

### WPT

A port of parts of
[W3C testharness.js](https://github.com/w3c/testharness.js) for testing the
Node.js
[WHATWG URL API](https://nodejs.org/api/url.html#url_the_whatwg_url_api)
implementation with tests from
[W3C Web Platform Tests](https://github.com/w3c/web-platform-tests).
