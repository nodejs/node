# Node.js Core Test Common Modules

This directory contains modules used to test the Node.js implementation.

## Table of Contents

* [Benchmark module](#benchmark-module)
* [Common module API](#common-module-api)
* [Countdown module](#countdown-module)
* [DNS module](#dns-module)
* [Duplex pair helper](#duplex-pair-helper)
* [Fixtures module](#fixtures-module)
* [Heap dump checker module](#heap-dump-checker-module)
* [HTTP2 module](#http2-module)
* [Internet module](#internet-module)
* [tick module](#tick-module)
* [tmpdir module](#tmpdir-module)
* [WPT module](#wpt-module)

## Benchmark Module

The `benchmark` module is used by tests to run benchmarks.

### runBenchmark(name, args, env)

* `name` [&lt;string>] Name of benchmark suite to be run.
* `args` [&lt;Array>] Array of environment variable key/value pairs (ex:
  `n=1`) to be applied via `--set`.
* `env` [&lt;Object>] Environment variables to be applied during the run.

## Common Module API

The `common` module is used by tests for consistency across repeated
tasks.

### allowGlobals(...whitelist)
* `whitelist` [&lt;Array>] Array of Globals
* return [&lt;Array>]

Takes `whitelist` and concats that with predefined `knownGlobals`.

### busyLoop(time)
* `time` [&lt;number>]

Blocks for `time` amount of time.

### canCreateSymLink()
* return [&lt;boolean>]

Checks whether the current running process can create symlinks. On Windows, this
returns `false` if the process running doesn't have privileges to create
symlinks
([SeCreateSymbolicLinkPrivilege](https://msdn.microsoft.com/en-us/library/windows/desktop/bb530716(v=vs.85).aspx)).
On non-Windows platforms, this always returns `true`.

### createZeroFilledFile(filename)

Creates a 10 MB file of all null characters.

### disableCrashOnUnhandledRejection()

Removes the `process.on('unhandledRejection')` handler that crashes the process
after a tick. The handler is useful for tests that use Promises and need to make
sure no unexpected rejections occur, because currently they result in silent
failures. However, it is useful in some rare cases to disable it, for example if
the `unhandledRejection` hook is directly used by the test.

### enoughTestMem
* [&lt;boolean>]

Indicates if there is more than 1gb of total memory.

### expectsError([fn, ]settings[, exact])
* `fn` [&lt;Function>] a function that should throw.
* `settings` [&lt;Object>]
  that must contain the `code` property plus any of the other following
  properties (some properties only apply for `AssertionError`):
  * `code` [&lt;string>]
    expected error must have this value for its `code` property.
  * `type` [&lt;Function>]
    expected error must be an instance of `type` and must be an Error subclass.
  * `message` [&lt;string>] or [&lt;RegExp>]
    if a string is provided for `message`, expected error must have it for its
    `message` property; if a regular expression is provided for `message`, the
    regular expression must match the `message` property of the expected error.
  * `name` [&lt;string>]
    expected error must have this value for its `name` property.
  * `info` &lt;Object> expected error must have the same `info` property
    that is deeply equal to this value.
  * `generatedMessage` [&lt;string>]
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
* `exact` [&lt;number>] default = 1
* return [&lt;Function>]

  If `fn` is provided, it will be passed to `assert.throws` as first argument
  and `undefined` will be returned.
  Otherwise a function suitable as callback or for use as a validation function
  passed as the second argument to `assert.throws()` will be returned. If the
  returned function has not been called exactly `exact` number of times when the
  test is complete, then the test will fail.

### expectWarning(name[, expected[, code]])
* `name` [&lt;string>] | [&lt;Object>]
* `expected` [&lt;string>] | [&lt;Array>] | [&lt;Object>]
* `code` [&lt;string>]

Tests whether `name`, `expected`, and `code` are part of a raised warning.

The code is required in case the name is set to `'DeprecationWarning'`.

Examples:

```js
const { expectWarning } = require('../common');

expectWarning('Warning', 'Foobar is really bad');

expectWarning('DeprecationWarning', 'Foobar is deprecated', 'DEP0XXX');

expectWarning('DeprecationWarning', [
  'Foobar is deprecated', 'DEP0XXX'
]);

expectWarning('DeprecationWarning', [
  ['Foobar is deprecated', 'DEP0XXX'],
  ['Baz is also deprecated', 'DEP0XX2']
]);

expectWarning('DeprecationWarning', {
  DEP0XXX: 'Foobar is deprecated',
  DEP0XX2: 'Baz is also deprecated'
});

expectWarning({
  DeprecationWarning: {
    DEP0XXX: 'Foobar is deprecated',
    DEP0XX1: 'Baz is also deprecated'
  },
  Warning: [
    ['Multiple array entries are fine', 'SpecialWarningCode'],
    ['No code is also fine']
  ],
  SingleEntry: ['This will also work', 'WarningCode'],
  SingleString: 'Single string entries without code will also work'
});
```

### getArrayBufferViews(buf)
* `buf` [&lt;Buffer>]
* return [&lt;ArrayBufferView&#91;&#93;>]

Returns an instance of all possible `ArrayBufferView`s of the provided Buffer.

### getBufferSources(buf)
* `buf` [&lt;Buffer>]
* return [&lt;BufferSource&#91;&#93;>]

Returns an instance of all possible `BufferSource`s of the provided Buffer,
consisting of all `ArrayBufferView` and an `ArrayBuffer`.

### getCallSite(func)
* `func` [&lt;Function>]
* return [&lt;string>]

Returns the file name and line number for the provided Function.

### getTTYfd()

Attempts to get a valid TTY file descriptor. Returns `-1` if it fails.

The TTY file descriptor is assumed to be capable of being writable.

### hasCrypto
* [&lt;boolean>]

Indicates whether OpenSSL is available.

### hasFipsCrypto
* [&lt;boolean>]

Indicates `hasCrypto` and `crypto` with fips.

### hasIntl
* [&lt;boolean>]

Indicates if [internationalization] is supported.

### hasIPv6
* [&lt;boolean>]

Indicates whether `IPv6` is supported on this platform.

### hasMultiLocalhost
* [&lt;boolean>]

Indicates if there are multiple localhosts available.

### inFreeBSDJail
* [&lt;boolean>]

Checks whether free BSD Jail is true or false.

### isAIX
* [&lt;boolean>]

Platform check for Advanced Interactive eXecutive (AIX).

### isAlive(pid)
* `pid` [&lt;number>]
* return [&lt;boolean>]

Attempts to 'kill' `pid`

### isFreeBSD
* [&lt;boolean>]

Platform check for Free BSD.

### isLinux
* [&lt;boolean>]

Platform check for Linux.

### isLinuxPPCBE
* [&lt;boolean>]

Platform check for Linux on PowerPC.

### isOSX
* [&lt;boolean>]

Platform check for macOS.

### isSunOS
* [&lt;boolean>]

Platform check for SunOS.

### isWindows
* [&lt;boolean>]

Platform check for Windows.

### localhostIPv4
* [&lt;string>]

IP of `localhost`.

### localIPv6Hosts
* [&lt;Array>]

Array of IPV6 representations for `localhost`.

### mustCall([fn][, exact])
* `fn` [&lt;Function>] default = () => {}
* `exact` [&lt;number>] default = 1
* return [&lt;Function>]

Returns a function that calls `fn`. If the returned function has not been called
exactly `exact` number of times when the test is complete, then the test will
fail.

If `fn` is not provided, an empty function will be used.

### mustCallAtLeast([fn][, minimum])
* `fn` [&lt;Function>] default = () => {}
* `minimum` [&lt;number>] default = 1
* return [&lt;Function>]

Returns a function that calls `fn`. If the returned function has not been called
at least `minimum` number of times when the test is complete, then the test will
fail.

If `fn` is not provided, an empty function will be used.

### mustNotCall([msg])
* `msg` [&lt;string>] default = 'function should not have been called'
* return [&lt;Function>]

Returns a function that triggers an `AssertionError` if it is invoked. `msg` is
used as the error message for the `AssertionError`.

### nodeProcessAborted(exitCode, signal)
* `exitCode` [&lt;number>]
* `signal` [&lt;string>]
* return [&lt;boolean>]

Returns `true` if the exit code `exitCode` and/or signal name `signal` represent
the exit code and/or signal name of a node process that aborted, `false`
otherwise.

### opensslCli
* [&lt;boolean>]

Indicates whether 'opensslCli' is supported.

### platformTimeout(ms)
* `ms` [&lt;number>] | [&lt;bigint>]
* return [&lt;number>] | [&lt;bigint>]

Returns a timeout value based on detected conditions. For example, a debug build
may need extra time so the returned value will be larger than on a release
build.

### PIPE
* [&lt;string>]

Path to the test socket.

### PORT
* [&lt;number>]

A port number for tests to use if one is needed.

### printSkipMessage(msg)
* `msg` [&lt;string>]

Logs '1..0 # Skipped: ' + `msg`

### pwdCommand
* [&lt;array>] First two argument for the `spawn`/`exec` functions.

Platform normalized `pwd` command options. Usage example:
```js
const common = require('../common');
const { spawn } = require('child_process');

spawn(...common.pwdCommand, { stdio: ['pipe'] });
```

### rootDir
* [&lt;string>]

Path to the 'root' directory. either `/` or `c:\\` (windows)

### runWithInvalidFD(func)
* `func` [&lt;Function>]

Runs `func` with an invalid file descriptor that is an unsigned integer and
can be used to trigger `EBADF` as the first argument. If no such file
descriptor could be generated, a skip message will be printed and the `func`
will not be run.

### skip(msg)
* `msg` [&lt;string>]

Logs '1..0 # Skipped: ' + `msg` and exits with exit code `0`.

### skipIfEslintMissing()

Skip the rest of the tests in the current file when `ESLint` is not available
at `tools/node_modules/eslint`

### skipIfInspectorDisabled()

Skip the rest of the tests in the current file when the Inspector
was disabled at compile time.

### skipIf32Bits()

Skip the rest of the tests in the current file when the Node.js executable
was compiled with a pointer size smaller than 64 bits.

### skipIfWorker()

Skip the rest of the tests in the current file when not running on a main
thread.

## ArrayStream Module

The `ArrayStream` module provides a simple `Stream` that pushes elements from
a given array.

<!-- eslint-disable no-undef, node-core/required-modules -->
```js
const ArrayStream = require('../common/arraystream');
const stream = new ArrayStream();
stream.run(['a', 'b', 'c']);
```

It can be used within tests as a simple mock stream.

## Countdown Module

The `Countdown` module provides a simple countdown mechanism for tests that
require a particular action to be taken after a given number of completed
tasks (for instance, shutting down an HTTP server after a specific number of
requests). The Countdown will fail the test if the remainder did not reach 0.

<!-- eslint-disable strict, node-core/required-modules -->
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

### Countdown.prototype.remaining

Specifies the remaining number of times `Countdown.prototype.dec()` must be
called before the callback is invoked.

## DNS Module

The `DNS` module provides utilities related to the `dns` built-in module.

### errorLookupMock(code, syscall)

* `code` [&lt;string>] Defaults to `dns.mockedErrorCode`.
* `syscall` [&lt;string>] Defaults to `dns.mockedSysCall`.
* return [&lt;Function>]

A mock for the `lookup` option of `net.connect()` that would result in an error
with the `code` and the `syscall` specified. Returns a function that has the
same signature as `dns.lookup()`.

### mockedErrorCode

The default `code` of errors generated by `errorLookupMock`.

### mockedSysCall

The default `syscall` of errors generated by `errorLookupMock`.

### readDomainFromPacket(buffer, offset)

* `buffer` [&lt;Buffer>]
* `offset` [&lt;number>]
* return [&lt;Object>]

Reads the domain string from a packet and returns an object containing the
number of bytes read and the domain.

### parseDNSPacket(buffer)

* `buffer` [&lt;Buffer>]
* return [&lt;Object>]

Parses a DNS packet. Returns an object with the values of the various flags of
the packet depending on the type of packet.

### writeIPv6(ip)

* `ip` [&lt;string>]
* return [&lt;Buffer>]

Reads an IPv6 String and returns a Buffer containing the parts.

### writeDomainName(domain)

* `domain` [&lt;string>]
* return [&lt;Buffer>]

Reads a Domain String and returns a Buffer containing the domain.

### writeDNSPacket(parsed)

* `parsed` [&lt;Object>]
* return [&lt;Buffer>]

Takes in a parsed Object and writes its fields to a DNS packet as a Buffer
object.

## Duplex pair helper

The `common/duplexpair` module exports a single function `makeDuplexPair`,
which returns an object `{ clientSide, serverSide }` where each side is a
`Duplex` stream connected to the other side.

There is no difference between client or server side beyond their names.

## Fixtures Module

The `common/fixtures` module provides convenience methods for working with
files in the `test/fixtures` directory.

### fixtures.fixturesDir

* [&lt;string>]

The absolute path to the `test/fixtures/` directory.

### fixtures.path(...args)

* `...args` [&lt;string>]

Returns the result of `path.join(fixtures.fixturesDir, ...args)`.

### fixtures.readSync(args[, enc])

* `args` [&lt;string>] | [&lt;Array>]

Returns the result of
`fs.readFileSync(path.join(fixtures.fixturesDir, ...args), 'enc')`.

### fixtures.readKey(arg[, enc])

* `arg` [&lt;string>]

Returns the result of
`fs.readFileSync(path.join(fixtures.fixturesDir, 'keys', arg), 'enc')`.

## Heap dump checker module

This provides utilities for checking the validity of heap dumps.
This requires the usage of `--expose-internals`.

### heap.recordState()

Create a heap dump and an embedder graph copy for inspection.
The returned object has a `validateSnapshotNodes` function similar to the
one listed below. (`heap.validateSnapshotNodes(...)` is a shortcut for
`heap.recordState().validateSnapshotNodes(...)`.)

### heap.validateSnapshotNodes(name, expected, options)

* `name` [&lt;string>] Look for this string as the name of heap dump nodes.
* `expected` [&lt;Array>] A list of objects, possibly with an `children`
  property that points to expected other adjacent nodes.
* `options` [&lt;Array>]
  * `loose` [&lt;boolean>] Do not expect an exact listing of occurrences
    of nodes with name `name` in `expected`.

Create a heap dump and an embedder graph copy and validate occurrences.

<!-- eslint-disable no-undef, node-core/required-modules -->
```js
validateSnapshotNodes('TLSWRAP', [
  {
    children: [
      { name: 'enc_out' },
      { name: 'enc_in' },
      { name: 'TLSWrap' }
    ]
  }
]);
```

## hijackstdio Module

The `hijackstdio` module provides utility functions for temporarily redirecting
`stdout` and `stderr` output.

<!-- eslint-disable no-undef, node-core/required-modules -->
```js
const { hijackStdout, restoreStdout } = require('../common/hijackstdio');

hijackStdout((data) => {
  /* Do something with data */
  restoreStdout();
});

console.log('this is sent to the hijacked listener');
```

### hijackStderr(listener)
* `listener` [&lt;Function>]: a listener with a single parameter
  called `data`.

Eavesdrop to `process.stderr.write()` calls. Once `process.stderr.write()` is
called, `listener` will also be called and the `data` of `write` function will
be passed to `listener`. What's more, `process.stderr.writeTimes` is a count of
the number of calls.

### hijackStdout(listener)
* `listener` [&lt;Function>]: a listener with a single parameter
  called `data`.

Eavesdrop to `process.stdout.write()` calls. Once `process.stdout.write()` is
called, `listener` will also be called and the `data` of `write` function will
be passed to `listener`. What's more, `process.stdout.writeTimes` is a count of
the number of calls.

### restoreStderr()

Restore the original `process.stderr.write()`. Used to restore `stderr` to its
original state after calling [`hijackstdio.hijackStdErr()`][].

### restoreStdout()

Restore the original `process.stdout.write()`. Used to restore `stdout` to its
original state after calling [`hijackstdio.hijackStdOut()`][].


## HTTP/2 Module

The http2.js module provides a handful of utilities for creating mock HTTP/2
frames for testing of HTTP/2 endpoints

<!-- eslint-disable no-unused-vars, node-core/required-modules -->
```js
const http2 = require('../common/http2');
```

### Class: Frame

The `http2.Frame` is a base class that creates a `Buffer` containing a
serialized HTTP/2 frame header.

<!-- eslint-disable no-undef, node-core/required-modules -->
```js
// length is a 24-bit unsigned integer
// type is an 8-bit unsigned integer identifying the frame type
// flags is an 8-bit unsigned integer containing the flag bits
// id is the 32-bit stream identifier, if any.
const frame = new http2.Frame(length, type, flags, id);

// Write the frame data to a socket
socket.write(frame.data);
```

The serialized `Buffer` may be retrieved using the `frame.data` property.

### Class: DataFrame extends Frame

The `http2.DataFrame` is a subclass of `http2.Frame` that serializes a `DATA`
frame.

<!-- eslint-disable no-undef, node-core/required-modules -->
```js
// id is the 32-bit stream identifier
// payload is a Buffer containing the DATA payload
// padlen is an 8-bit integer giving the number of padding bytes to include
// final is a boolean indicating whether the End-of-stream flag should be set,
// defaults to false.
const frame = new http2.DataFrame(id, payload, padlen, final);

socket.write(frame.data);
```

### Class: HeadersFrame

The `http2.HeadersFrame` is a subclass of `http2.Frame` that serializes a
`HEADERS` frame.

<!-- eslint-disable no-undef, node-core/required-modules -->
```js
// id is the 32-bit stream identifier
// payload is a Buffer containing the HEADERS payload (see either
// http2.kFakeRequestHeaders or http2.kFakeResponseHeaders).
// padlen is an 8-bit integer giving the number of padding bytes to include
// final is a boolean indicating whether the End-of-stream flag should be set,
// defaults to false.
const frame = new http2.HeadersFrame(id, payload, padlen, final);

socket.write(frame.data);
```

### Class: SettingsFrame

The `http2.SettingsFrame` is a subclass of `http2.Frame` that serializes an
empty `SETTINGS` frame.

<!-- eslint-disable no-undef, node-core/required-modules -->
```js
// ack is a boolean indicating whether or not to set the ACK flag.
const frame = new http2.SettingsFrame(ack);

socket.write(frame.data);
```

### http2.kFakeRequestHeaders

Set to a `Buffer` instance that contains a minimal set of serialized HTTP/2
request headers to be used as the payload of a `http2.HeadersFrame`.

<!-- eslint-disable no-undef, node-core/required-modules -->
```js
const frame = new http2.HeadersFrame(1, http2.kFakeRequestHeaders, 0, true);

socket.write(frame.data);
```

### http2.kFakeResponseHeaders

Set to a `Buffer` instance that contains a minimal set of serialized HTTP/2
response headers to be used as the payload a `http2.HeadersFrame`.

<!-- eslint-disable no-undef, node-core/required-modules -->
```js
const frame = new http2.HeadersFrame(1, http2.kFakeResponseHeaders, 0, true);

socket.write(frame.data);
```

### http2.kClientMagic

Set to a `Buffer` containing the preamble bytes an HTTP/2 client must send
upon initial establishment of a connection.

<!-- eslint-disable no-undef, node-core/required-modules -->
```js
socket.write(http2.kClientMagic);
```

## Internet Module

The `common/internet` module provides utilities for working with
internet-related tests.

### internet.addresses

* [&lt;Object>]
  * `INET_HOST` [&lt;string>] A generic host that has registered common
    DNS records, supports both IPv4 and IPv6, and provides basic HTTP/HTTPS
    services
  * `INET4_HOST` [&lt;string>] A host that provides IPv4 services
  * `INET6_HOST` [&lt;string>] A host that provides IPv6 services
  * `INET4_IP` [&lt;string>] An accessible IPv4 IP, defaults to the
    Google Public DNS IPv4 address
  * `INET6_IP` [&lt;string>] An accessible IPv6 IP, defaults to the
    Google Public DNS IPv6 address
  * `INVALID_HOST` [&lt;string>] An invalid host that cannot be resolved
  * `MX_HOST` [&lt;string>] A host with MX records registered
  * `SRV_HOST` [&lt;string>] A host with SRV records registered
  * `PTR_HOST` [&lt;string>] A host with PTR records registered
  * `NAPTR_HOST` [&lt;string>] A host with NAPTR records registered
  * `SOA_HOST` [&lt;string>] A host with SOA records registered
  * `CNAME_HOST` [&lt;string>] A host with CNAME records registered
  * `NS_HOST` [&lt;string>] A host with NS records registered
  * `TXT_HOST` [&lt;string>] A host with TXT records registered
  * `DNS4_SERVER` [&lt;string>] An accessible IPv4 DNS server
  * `DNS6_SERVER` [&lt;string>] An accessible IPv6 DNS server

A set of addresses for internet-related tests. All properties are configurable
via `NODE_TEST_*` environment variables. For example, to configure
`internet.addresses.INET_HOST`, set the environment
variable `NODE_TEST_INET_HOST` to a specified host.

## ongc Module

The `ongc` module allows a garbage collection listener to be installed. The
module exports a single `onGC()` function.

```js
require('../common');
const onGC = require('../common/ongc');

onGC({}, { ongc() { console.log('collected'); } });
```

### onGC(target, listener)
* `target` [&lt;Object>]
* `listener` [&lt;Object>]
  * `ongc` [&lt;Function>]

Installs a GC listener for the collection of `target`.

This uses `async_hooks` for GC tracking. This means that it enables
`async_hooks` tracking, which may affect the test functionality. It also
means that between a `global.gc()` call and the listener being invoked
a full `setImmediate()` invocation passes.

`listener` is an object to make it easier to use a closure; the target object
should not be in scope when `listener.ongc()` is created.


## tick Module

The `tick` module provides a helper function that can be used to call a callback
after a given number of event loop "ticks".

### tick(x, cb)

* `x` [&lt;number>] Number of event loop "ticks".
* `cb` [&lt;Function>] A callback function.

## tmpdir Module

The `tmpdir` module supports the use of a temporary directory for testing.

### path
* [&lt;string>]

The realpath of the testing temporary directory.

### refresh()

Deletes and recreates the testing temporary directory.

## WPT Module

### harness

A legacy port of [Web Platform Tests][] harness.

See the source code for definitions. Please avoid using it in new
code - the current usage of this port in tests is being migrated to
the original WPT harness, see [the WPT tests README][].

### Class: WPTRunner

A driver class for running WPT with the WPT harness in a vm.

See [the WPT tests README][] for details.


[&lt;Array>]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array
[&lt;ArrayBufferView&#91;&#93;>]: https://developer.mozilla.org/en-US/docs/Web/API/ArrayBufferView
[&lt;Buffer>]: https://nodejs.org/api/buffer.html#buffer_class_buffer
[&lt;Function>]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Function
[&lt;Object>]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object
[&lt;RegExp>]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp
[&lt;bigint>]: https://github.com/tc39/proposal-bigint
[&lt;boolean>]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Boolean_type
[&lt;number>]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Number_type
[&lt;string>]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#String_type
[Web Platform Tests]: https://github.com/web-platform-tests/wpt
[`hijackstdio.hijackStdErr()`]: #hijackstderrlistener
[`hijackstdio.hijackStdOut()`]: #hijackstdoutlistener
[internationalization]: https://github.com/nodejs/node/wiki/Intl
[the WPT tests README]: ../wpt/README.md
