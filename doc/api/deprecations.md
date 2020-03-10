# Deprecated APIs

<!--introduced_in=v7.7.0-->
<!-- type=misc -->

Node.js may deprecate APIs for any of the following reasons:

* Use of the API is unsafe.
* An improved alternative API is available.
* Breaking changes to the API are expected in a future major release.

Node.js utilizes three kinds of Deprecations:

* Documentation-only
* Runtime
* End-of-Life

A Documentation-only deprecation is one that is expressed only within the
Node.js API docs. These generate no side-effects while running Node.js.
Some Documentation-only deprecations trigger a runtime warning when launched
with [`--pending-deprecation`][] flag (or its alternative,
`NODE_PENDING_DEPRECATION=1` environment variable), similarly to Runtime
deprecations below. Documentation-only deprecations that support that flag
are explicitly labeled as such in the
[list of Deprecated APIs](#deprecations_list_of_deprecated_apis).

A Runtime deprecation will, by default, generate a process warning that will
be printed to `stderr` the first time the deprecated API is used. When the
[`--throw-deprecation`][] command-line flag is used, a Runtime deprecation will
cause an error to be thrown.

An End-of-Life deprecation is used when functionality is or will soon be removed
from Node.js.

## Revoking deprecations

Occasionally, the deprecation of an API may be reversed. In such situations,
this document will be updated with information relevant to the decision.
However, the deprecation identifier will not be modified.

## List of Deprecated APIs

<a id="DEP0001"></a>
### DEP0001: `http.OutgoingMessage.prototype.flush`
<!-- YAML
changes:
  - version: v14.0.0
    pr-url: https://github.com/nodejs/node/pull/31164
    description: End-of-Life.
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v1.6.0
    pr-url: https://github.com/nodejs/node/pull/1156
    description: Runtime deprecation.
-->

Type: End-of-Life

`OutgoingMessage.prototype.flush()` has been removed. Use
`OutgoingMessage.prototype.flushHeaders()` instead.

<a id="DEP0002"></a>
### DEP0002: `require('_linklist')`
<!-- YAML
changes:
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12113
    description: End-of-Life.
  - version: v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v5.0.0
    pr-url: https://github.com/nodejs/node/pull/3078
    description: Runtime deprecation.
-->

Type: End-of-Life

The `_linklist` module is deprecated. Please use a userland alternative.

<a id="DEP0003"></a>
### DEP0003: `_writableState.buffer`
<!-- YAML
changes:
  - version: v14.0.0
    pr-url: https://github.com/nodejs/node/pull/31165
    description: End-of-Life
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v0.11.15
    pr-url: https://github.com/nodejs/node-v0.x-archive/pull/8826
    description: Runtime deprecation.
-->

Type: End-of-Life

The `_writableState.buffer` has been removed. Use `_writableState.getBuffer()`
instead.

<a id="DEP0004"></a>
### DEP0004: `CryptoStream.prototype.readyState`
<!-- YAML
changes:
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/17882
    description: End-of-Life.
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: 0.4.0
    commit: 9c7f89bf56abd37a796fea621ad2e47dd33d2b82
    description: Documentation-only deprecation.
-->

Type: End-of-Life

The `CryptoStream.prototype.readyState` property was removed.

<a id="DEP0005"></a>
### DEP0005: `Buffer()` constructor
<!-- YAML
changes:
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/19524
    description: Runtime deprecation.
  - version: v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v6.0.0
    pr-url: https://github.com/nodejs/node/pull/4682
    description: Documentation-only deprecation.
-->

Type: Runtime (supports [`--pending-deprecation`][])

The `Buffer()` function and `new Buffer()` constructor are deprecated due to
API usability issues that can lead to accidental security issues.

As an alternative, use one of the following methods of constructing `Buffer`
objects:

* [`Buffer.alloc(size[, fill[, encoding]])`][alloc]: Create a `Buffer` with
  *initialized* memory.
* [`Buffer.allocUnsafe(size)`][alloc_unsafe_size]: Create a `Buffer` with
  *uninitialized* memory.
* [`Buffer.allocUnsafeSlow(size)`][]: Create a `Buffer` with *uninitialized*
   memory.
* [`Buffer.from(array)`][]: Create a `Buffer` with a copy of `array`
* [`Buffer.from(arrayBuffer[, byteOffset[, length]])`][from_arraybuffer] -
  Create a `Buffer` that wraps the given `arrayBuffer`.
* [`Buffer.from(buffer)`][]: Create a `Buffer` that copies `buffer`.
* [`Buffer.from(string[, encoding])`][from_string_encoding]: Create a `Buffer`
  that copies `string`.

Without `--pending-deprecation`, runtime warnings occur only for code not in
`node_modules`. This means there will not be deprecation warnings for
`Buffer()` usage in dependencies. With `--pending-deprecation`, a runtime
warning results no matter where the `Buffer()` usage occurs.

<a id="DEP0006"></a>
### DEP0006: `child_process` `options.customFds`
<!-- YAML
changes:
  - version: v12.0.0
    pr-url: https://github.com/nodejs/node/pull/25279
    description: End-of-Life.
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v0.11.14
    description: Runtime deprecation.
  - version: v0.5.11
    description: Documentation-only deprecation.
-->

Type: End-of-Life

Within the [`child_process`][] module's `spawn()`, `fork()`, and `exec()`
methods, the `options.customFds` option is deprecated. The `options.stdio`
option should be used instead.

<a id="DEP0007"></a>
### DEP0007: Replace `cluster` `worker.suicide` with `worker.exitedAfterDisconnect`
<!-- YAML
changes:
  - version: v9.0.0
    pr-url: https://github.com/nodejs/node/pull/13702
    description: End-of-Life.
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/3747
    description: Runtime deprecation.
  - version: v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v6.0.0
    pr-url: https://github.com/nodejs/node/pull/3743
    description: Documentation-only deprecation.
-->

Type: End-of-Life

In an earlier version of the Node.js `cluster`, a boolean property with the name
`suicide` was added to the `Worker` object. The intent of this property was to
provide an indication of how and why the `Worker` instance exited. In Node.js
6.0.0, the old property was deprecated and replaced with a new
[`worker.exitedAfterDisconnect`][] property. The old property name did not
precisely describe the actual semantics and was unnecessarily emotion-laden.

<a id="DEP0008"></a>
### DEP0008: `require('constants')`
<!-- YAML
changes:
  - version: v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v6.3.0
    pr-url: https://github.com/nodejs/node/pull/6534
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The `constants` module is deprecated. When requiring access to constants
relevant to specific Node.js builtin modules, developers should instead refer
to the `constants` property exposed by the relevant module. For instance,
`require('fs').constants` and `require('os').constants`.

<a id="DEP0009"></a>
### DEP0009: `crypto.pbkdf2` without digest
<!-- YAML
changes:
  - version: v14.0.0
    pr-url: https://github.com/nodejs/node/pull/31166
    description: End-of-Life (for `digest === null`)
  - version: v11.0.0
    pr-url: https://github.com/nodejs/node/pull/22861
    description: Runtime deprecation (for `digest === null`).
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/11305
    description: End-of-Life (for `digest === undefined`).
  - version: v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v6.0.0
    pr-url: https://github.com/nodejs/node/pull/4047
    description: Runtime deprecation (for `digest === undefined`).
-->

Type: End-of-Life

Use of the [`crypto.pbkdf2()`][] API without specifying a digest was deprecated
in Node.js 6.0 because the method defaulted to using the non-recommended
`'SHA1'` digest. Previously, a deprecation warning was printed. Starting in
Node.js 8.0.0, calling `crypto.pbkdf2()` or `crypto.pbkdf2Sync()` with
`digest` set to `undefined` will throw a `TypeError`.

Beginning in Node.js v11.0.0, calling these functions with `digest` set to
`null` would print a deprecation warning to align with the behavior when `digest`
is `undefined`.

Now, however, passing either `undefined` or `null` will throw a `TypeError`.

<a id="DEP0010"></a>
### DEP0010: `crypto.createCredentials`
<!-- YAML
changes:
  - version: v11.0.0
    pr-url: https://github.com/nodejs/node/pull/21153
    description: End-of-Life.
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v0.11.13
    pr-url: https://github.com/nodejs/node-v0.x-archive/pull/7265
    description: Runtime deprecation.
-->

Type: End-of-Life

The `crypto.createCredentials()` API was removed. Please use
[`tls.createSecureContext()`][] instead.

<a id="DEP0011"></a>
### DEP0011: `crypto.Credentials`
<!-- YAML
changes:
  - version: v11.0.0
    pr-url: https://github.com/nodejs/node/pull/21153
    description: End-of-Life.
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v0.11.13
    pr-url: https://github.com/nodejs/node-v0.x-archive/pull/7265
    description: Runtime deprecation.
-->

Type: End-of-Life

The `crypto.Credentials` class was removed. Please use [`tls.SecureContext`][]
instead.

<a id="DEP0012"></a>
### DEP0012: `Domain.dispose`
<!-- YAML
changes:
  - version: v9.0.0
    pr-url: https://github.com/nodejs/node/pull/15412
    description: End-of-Life.
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v0.11.7
    pr-url: https://github.com/nodejs/node-v0.x-archive/pull/5021
    description: Runtime deprecation.
-->

Type: End-of-Life

`Domain.dispose()` has been removed. Recover from failed I/O actions
explicitly via error event handlers set on the domain instead.

<a id="DEP0013"></a>
### DEP0013: `fs` asynchronous function without callback
<!-- YAML
changes:
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/18668
    description: End-of-Life.
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: Runtime deprecation.
-->

Type: End-of-Life

Calling an asynchronous function without a callback throws a `TypeError`
in Node.js 10.0.0 onwards. See <https://github.com/nodejs/node/pull/12562>.

<a id="DEP0014"></a>
### DEP0014: `fs.read` legacy String interface
<!-- YAML
changes:
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/9683
    description: End-of-Life.
  - version: v6.0.0
    pr-url: https://github.com/nodejs/node/pull/4525
    description: Runtime deprecation.
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v0.1.96
    commit: c93e0aaf062081db3ec40ac45b3e2c979d5759d6
    description: Documentation-only deprecation.
-->

Type: End-of-Life

The [`fs.read()`][] legacy `String` interface is deprecated. Use the `Buffer`
API as mentioned in the documentation instead.

<a id="DEP0015"></a>
### DEP0015: `fs.readSync` legacy String interface
<!-- YAML
changes:
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/9683
    description: End-of-Life.
  - version: v6.0.0
    pr-url: https://github.com/nodejs/node/pull/4525
    description: Runtime deprecation.
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v0.1.96
    commit: c93e0aaf062081db3ec40ac45b3e2c979d5759d6
    description: Documentation-only deprecation.
-->

Type: End-of-Life

The [`fs.readSync()`][] legacy `String` interface is deprecated. Use the
`Buffer` API as mentioned in the documentation instead.

<a id="DEP0016"></a>
### DEP0016: `GLOBAL`/`root`
<!-- YAML
changes:
  - version: v14.0.0
    pr-url: https://github.com/nodejs/node/pull/31167
    description: End-of-Life
  - version: v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v6.0.0
    pr-url: https://github.com/nodejs/node/pull/1838
    description: Runtime deprecation.
-->

Type: End-of-Life

The `GLOBAL` and `root` aliases for the `global` property were deprecated
in Node.js 6.0.0 and have since been removed.

<a id="DEP0017"></a>
### DEP0017: `Intl.v8BreakIterator`
<!-- YAML
changes:
  - version: v9.0.0
    pr-url: https://github.com/nodejs/node/pull/15238
    description: End-of-Life.
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/8908
    description: Runtime deprecation.
-->

Type: End-of-Life

`Intl.v8BreakIterator` was a non-standard extension and has been removed.
See [`Intl.Segmenter`](https://github.com/tc39/proposal-intl-segmenter).

<a id="DEP0018"></a>
### DEP0018: Unhandled promise rejections
<!-- YAML
changes:
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/8217
    description: Runtime deprecation.
-->

Type: Runtime

Unhandled promise rejections are deprecated. In the future, promise rejections
that are not handled will terminate the Node.js process with a non-zero exit
code.

<a id="DEP0019"></a>
### DEP0019: `require('.')` resolved outside directory
<!-- YAML
changes:
  - version: v12.0.0
    pr-url: https://github.com/nodejs/node/pull/26973
    description: Removed functionality.
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v1.8.1
    pr-url: https://github.com/nodejs/node/pull/1363
    description: Runtime deprecation.
-->

Type: End-of-Life

In certain cases, `require('.')` could resolve outside the package directory.
This behavior has been removed.

<a id="DEP0020"></a>
### DEP0020: `Server.connections`
<!-- YAML
changes:
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v0.9.7
    pr-url: https://github.com/nodejs/node-v0.x-archive/pull/4595
    description: Runtime deprecation.
-->

Type: Runtime

The [`Server.connections`][] property is deprecated. Please use the
[`Server.getConnections()`][] method instead.

<a id="DEP0021"></a>
### DEP0021: `Server.listenFD`
<!-- YAML
changes:
  - version: v12.0.0
    pr-url: https://github.com/nodejs/node/pull/27127
    description: End-of-Life.
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v0.7.12
    commit: 41421ff9da1288aa241a5e9dcf915b685ade1c23
    description: Runtime deprecation.
-->

Type: End-of-Life

The `Server.listenFD()` method was deprecated and removed. Please use
[`Server.listen({fd: <number>})`][] instead.

<a id="DEP0022"></a>
### DEP0022: `os.tmpDir()`
<!-- YAML
changes:
  - version: v14.0.0
    pr-url: https://github.com/nodejs/node/pull/31169
    description: End-of-Life.
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/6739
    description: Runtime deprecation.
-->

Type: End-of-Life

The `os.tmpDir()` API was deprecated in Node.js 7.0.0 and has since been
removed. Please use [`os.tmpdir()`][] instead.

<a id="DEP0023"></a>
### DEP0023: `os.getNetworkInterfaces()`
<!-- YAML
changes:
  - version: v12.0.0
    pr-url: https://github.com/nodejs/node/pull/25280
    description: End-of-Life.
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v0.6.0
    commit: 37bb37d151fb6ee4696730e63ff28bb7a4924f97
    description: Runtime deprecation.
-->

Type: End-of-Life

The `os.getNetworkInterfaces()` method is deprecated. Please use the
[`os.networkInterfaces()`][] method instead.

<a id="DEP0024"></a>
### DEP0024: `REPLServer.prototype.convertToContext()`
<!-- YAML
changes:
  - version: v9.0.0
    pr-url: https://github.com/nodejs/node/pull/13434
    description: End-of-Life.
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7829
    description: Runtime deprecation.
-->

Type: End-of-Life

The `REPLServer.prototype.convertToContext()` API has been removed.

<a id="DEP0025"></a>
### DEP0025: `require('sys')`
<!-- YAML
changes:
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v1.0.0
    pr-url: https://github.com/nodejs/node/pull/317
    description: Runtime deprecation.
-->

Type: Runtime

The `sys` module is deprecated. Please use the [`util`][] module instead.

<a id="DEP0026"></a>
### DEP0026: `util.print()`
<!-- YAML
changes:
  - version: v12.0.0
    pr-url: https://github.com/nodejs/node/pull/25377
    description: End-of-Life.
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v0.11.3
    commit: 896b2aa7074fc886efd7dd0a397d694763cac7ce
    description: Runtime deprecation.
-->

Type: End-of-Life

`util.print()` has been removed. Please use [`console.log()`][] instead.

<a id="DEP0027"></a>
### DEP0027: `util.puts()`
<!-- YAML
changes:
  - version: v12.0.0
    pr-url: https://github.com/nodejs/node/pull/25377
    description: End-of-Life.
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v0.11.3
    commit: 896b2aa7074fc886efd7dd0a397d694763cac7ce
    description: Runtime deprecation.
-->

Type: End-of-Life

`util.puts()` has been removed. Please use [`console.log()`][] instead.

<a id="DEP0028"></a>
### DEP0028: `util.debug()`
<!-- YAML
changes:
  - version: v12.0.0
    pr-url: https://github.com/nodejs/node/pull/25377
    description: End-of-Life.
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v0.11.3
    commit: 896b2aa7074fc886efd7dd0a397d694763cac7ce
    description: Runtime deprecation.
-->

Type: End-of-Life

`util.debug()` has been removed. Please use [`console.error()`][] instead.

<a id="DEP0029"></a>
### DEP0029: `util.error()`
<!-- YAML
changes:
  - version: v12.0.0
    pr-url: https://github.com/nodejs/node/pull/25377
    description: End-of-Life.
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v0.11.3
    commit: 896b2aa7074fc886efd7dd0a397d694763cac7ce
    description: Runtime deprecation.
-->

Type: End-of-Life

`util.error()` has been removed. Please use [`console.error()`][] instead.

<a id="DEP0030"></a>
### DEP0030: `SlowBuffer`
<!-- YAML
changes:
  - version: v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v6.0.0
    pr-url: https://github.com/nodejs/node/pull/5833
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [`SlowBuffer`][] class is deprecated. Please use
[`Buffer.allocUnsafeSlow(size)`][] instead.

<a id="DEP0031"></a>
### DEP0031: `ecdh.setPublicKey()`
<!-- YAML
changes:
  - version: v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v5.2.0
    pr-url: https://github.com/nodejs/node/pull/3511
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [`ecdh.setPublicKey()`][] method is now deprecated as its inclusion in the
API is not useful.

<a id="DEP0032"></a>
### DEP0032: `domain` module
<!-- YAML
changes:
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v1.4.2
    pr-url: https://github.com/nodejs/node/pull/943
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [`domain`][] module is deprecated and should not be used.

<a id="DEP0033"></a>
### DEP0033: `EventEmitter.listenerCount()`
<!-- YAML
changes:
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v3.2.0
    pr-url: https://github.com/nodejs/node/pull/2349
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [`EventEmitter.listenerCount(emitter, eventName)`][] API is
deprecated. Please use [`emitter.listenerCount(eventName)`][] instead.

<a id="DEP0034"></a>
### DEP0034: `fs.exists(path, callback)`
<!-- YAML
changes:
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v1.0.0
    pr-url: https://github.com/iojs/io.js/pull/166
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [`fs.exists(path, callback)`][] API is deprecated. Please use
[`fs.stat()`][] or [`fs.access()`][] instead.

<a id="DEP0035"></a>
### DEP0035: `fs.lchmod(path, mode, callback)`
<!-- YAML
changes:
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v0.4.7
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [`fs.lchmod(path, mode, callback)`][] API is deprecated.

<a id="DEP0036"></a>
### DEP0036: `fs.lchmodSync(path, mode)`
<!-- YAML
changes:
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v0.4.7
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [`fs.lchmodSync(path, mode)`][] API is deprecated.

<a id="DEP0037"></a>
### DEP0037: `fs.lchown(path, uid, gid, callback)`
<!-- YAML
changes:
  - version: v10.6.0
    pr-url: https://github.com/nodejs/node/pull/21498
    description: Deprecation revoked.
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v0.4.7
    description: Documentation-only deprecation.
-->

Type: Deprecation revoked

The [`fs.lchown(path, uid, gid, callback)`][] API was deprecated. The
deprecation was revoked because the requisite supporting APIs were added in
libuv.

<a id="DEP0038"></a>
### DEP0038: `fs.lchownSync(path, uid, gid)`
<!-- YAML
changes:
  - version: v10.6.0
    pr-url: https://github.com/nodejs/node/pull/21498
    description: Deprecation revoked.
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v0.4.7
    description: Documentation-only deprecation.
-->

Type: Deprecation revoked

The [`fs.lchownSync(path, uid, gid)`][] API was deprecated. The deprecation was
revoked because the requisite supporting APIs were added in libuv.

<a id="DEP0039"></a>
### DEP0039: `require.extensions`
<!-- YAML
changes:
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v0.10.6
    commit: 7bd8a5a2a60b75266f89f9a32877d55294a3881c
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [`require.extensions`][] property is deprecated.

<a id="DEP0040"></a>
### DEP0040: `punycode` module
<!-- YAML
changes:
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7941
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [`punycode`][] module is deprecated. Please use a userland alternative
instead.

<a id="DEP0041"></a>
### DEP0041: `NODE_REPL_HISTORY_FILE` environment variable
<!-- YAML
changes:
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/13876
    description: End-of-Life.
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v3.0.0
    pr-url: https://github.com/nodejs/node/pull/2224
    description: Documentation-only deprecation.
-->

Type: End-of-Life

The `NODE_REPL_HISTORY_FILE` environment variable was removed. Please use
`NODE_REPL_HISTORY` instead.

<a id="DEP0042"></a>
### DEP0042: `tls.CryptoStream`
<!-- YAML
changes:
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/17882
    description: End-of-Life.
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v0.11.3
    commit: af80e7bc6e6f33c582eb1f7d37c7f5bbe9f910f7
    description: Documentation-only deprecation.
-->

Type: End-of-Life

The [`tls.CryptoStream`][] class was removed. Please use
[`tls.TLSSocket`][] instead.

<a id="DEP0043"></a>
### DEP0043: `tls.SecurePair`
<!-- YAML
changes:
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/11349
    description: Runtime deprecation.
  - version: v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v6.0.0
    pr-url: https://github.com/nodejs/node/pull/6063
    description: Documentation-only deprecation.
  - version: v0.11.15
    pr-url:
      - https://github.com/nodejs/node-v0.x-archive/pull/8695
      - https://github.com/nodejs/node-v0.x-archive/pull/8700
    description: Deprecation revoked.
  - version: v0.11.3
    commit: af80e7bc6e6f33c582eb1f7d37c7f5bbe9f910f7
    description: Runtime deprecation.
-->

Type: Documentation-only

The [`tls.SecurePair`][] class is deprecated. Please use
[`tls.TLSSocket`][] instead.

<a id="DEP0044"></a>
### DEP0044: `util.isArray()`
<!-- YAML
changes:
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version:
    - v3.3.1
    - v4.0.0
    pr-url: https://github.com/nodejs/node/pull/2447
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [`util.isArray()`][] API is deprecated. Please use `Array.isArray()`
instead.

<a id="DEP0045"></a>
### DEP0045: `util.isBoolean()`
<!-- YAML
changes:
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version:
    - v3.3.1
    - v4.0.0
    pr-url: https://github.com/nodejs/node/pull/2447
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [`util.isBoolean()`][] API is deprecated.

<a id="DEP0046"></a>
### DEP0046: `util.isBuffer()`
<!-- YAML
changes:
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version:
    - v3.3.1
    - v4.0.0
    pr-url: https://github.com/nodejs/node/pull/2447
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [`util.isBuffer()`][] API is deprecated. Please use
[`Buffer.isBuffer()`][] instead.

<a id="DEP0047"></a>
### DEP0047: `util.isDate()`
<!-- YAML
changes:
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version:
    - v3.3.1
    - v4.0.0
    pr-url: https://github.com/nodejs/node/pull/2447
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [`util.isDate()`][] API is deprecated.

<a id="DEP0048"></a>
### DEP0048: `util.isError()`
<!-- YAML
changes:
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version:
    - v3.3.1
    - v4.0.0
    pr-url: https://github.com/nodejs/node/pull/2447
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [`util.isError()`][] API is deprecated.

<a id="DEP0049"></a>
### DEP0049: `util.isFunction()`
<!-- YAML
changes:
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version:
    - v3.3.1
    - v4.0.0
    pr-url: https://github.com/nodejs/node/pull/2447
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [`util.isFunction()`][] API is deprecated.

<a id="DEP0050"></a>
### DEP0050: `util.isNull()`
<!-- YAML
changes:
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version:
    - v3.3.1
    - v4.0.0
    pr-url: https://github.com/nodejs/node/pull/2447
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [`util.isNull()`][] API is deprecated.

<a id="DEP0051"></a>
### DEP0051: `util.isNullOrUndefined()`
<!-- YAML
changes:
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version:
    - v3.3.1
    - v4.0.0
    pr-url: https://github.com/nodejs/node/pull/2447
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [`util.isNullOrUndefined()`][] API is deprecated.

<a id="DEP0052"></a>
### DEP0052: `util.isNumber()`
<!-- YAML
changes:
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version:
    - v3.3.1
    - v4.0.0
    pr-url: https://github.com/nodejs/node/pull/2447
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [`util.isNumber()`][] API is deprecated.

<a id="DEP0053"></a>
### DEP0053 `util.isObject()`
<!-- YAML
changes:
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version:
    - v3.3.1
    - v4.0.0
    pr-url: https://github.com/nodejs/node/pull/2447
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [`util.isObject()`][] API is deprecated.

<a id="DEP0054"></a>
### DEP0054: `util.isPrimitive()`
<!-- YAML
changes:
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version:
    - v3.3.1
    - v4.0.0
    pr-url: https://github.com/nodejs/node/pull/2447
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [`util.isPrimitive()`][] API is deprecated.

<a id="DEP0055"></a>
### DEP0055: `util.isRegExp()`
<!-- YAML
changes:
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version:
    - v3.3.1
    - v4.0.0
    pr-url: https://github.com/nodejs/node/pull/2447
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [`util.isRegExp()`][] API is deprecated.

<a id="DEP0056"></a>
### DEP0056: `util.isString()`
<!-- YAML
changes:
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version:
    - v3.3.1
    - v4.0.0
    pr-url: https://github.com/nodejs/node/pull/2447
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [`util.isString()`][] API is deprecated.

<a id="DEP0057"></a>
### DEP0057: `util.isSymbol()`
<!-- YAML
changes:
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version:
    - v3.3.1
    - v4.0.0
    pr-url: https://github.com/nodejs/node/pull/2447
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [`util.isSymbol()`][] API is deprecated.

<a id="DEP0058"></a>
### DEP0058: `util.isUndefined()`
<!-- YAML
changes:
  - version:
    - v4.8.6
    - v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version:
    - v3.3.1
    - v4.0.0
    pr-url: https://github.com/nodejs/node/pull/2447
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [`util.isUndefined()`][] API is deprecated.

<a id="DEP0059"></a>
### DEP0059: `util.log()`
<!-- YAML
changes:
  - version: v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v6.0.0
    pr-url: https://github.com/nodejs/node/pull/6161
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [`util.log()`][] API is deprecated.

<a id="DEP0060"></a>
### DEP0060: `util._extend()`
<!-- YAML
changes:
  - version: v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v6.0.0
    pr-url: https://github.com/nodejs/node/pull/4903
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [`util._extend()`][] API is deprecated.

<a id="DEP0061"></a>
### DEP0061: `fs.SyncWriteStream`
<!-- YAML
changes:
  - version: v11.0.0
    pr-url: https://github.com/nodejs/node/pull/20735
    description: End-of-Life.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/10467
    description: Runtime deprecation.
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/6749
    description: Documentation-only deprecation.
-->

Type: End-of-Life

The `fs.SyncWriteStream` class was never intended to be a publicly accessible
API and has been removed. No alternative API is available. Please use a userland
alternative.

<a id="DEP0062"></a>
### DEP0062: `node --debug`
<!-- YAML
changes:
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/10970
    description: Runtime deprecation.
  - version: v12.0.0
    pr-url: https://github.com/nodejs/node/pull/25828
    description: End-of-Life.
-->

Type: End-of-Life

`--debug` activates the legacy V8 debugger interface, which was removed as
of V8 5.8. It is replaced by Inspector which is activated with `--inspect`
instead.

<a id="DEP0063"></a>
### DEP0063: `ServerResponse.prototype.writeHeader()`
<!-- YAML
changes:
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/11355
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The `http` module `ServerResponse.prototype.writeHeader()` API is
deprecated. Please use `ServerResponse.prototype.writeHead()` instead.

The `ServerResponse.prototype.writeHeader()` method was never documented as an
officially supported API.

<a id="DEP0064"></a>
### DEP0064: `tls.createSecurePair()`
<!-- YAML
changes:
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/11349
    description: Runtime deprecation.
  - version: v6.12.0
    pr-url: https://github.com/nodejs/node/pull/10116
    description: A deprecation code has been assigned.
  - version: v6.0.0
    pr-url: https://github.com/nodejs/node/pull/6063
    description: Documentation-only deprecation.
  - version: v0.11.15
    pr-url:
      - https://github.com/nodejs/node-v0.x-archive/pull/8695
      - https://github.com/nodejs/node-v0.x-archive/pull/8700
    description: Deprecation revoked.
  - version: v0.11.3
    commit: af80e7bc6e6f33c582eb1f7d37c7f5bbe9f910f7
    description: Runtime deprecation.
-->

Type: Runtime

The `tls.createSecurePair()` API was deprecated in documentation in Node.js
0.11.3. Users should use `tls.Socket` instead.

<a id="DEP0065"></a>
### DEP0065: `repl.REPL_MODE_MAGIC` and `NODE_REPL_MODE=magic`
<!-- YAML
changes:
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/19187
    description: End-of-Life.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/11599
    description: Documentation-only deprecation.
-->

Type: End-of-Life

The `repl` module's `REPL_MODE_MAGIC` constant, used for `replMode` option, has
been removed. Its behavior has been functionally identical to that of
`REPL_MODE_SLOPPY` since Node.js 6.0.0, when V8 5.0 was imported. Please use
`REPL_MODE_SLOPPY` instead.

The `NODE_REPL_MODE` environment variable is used to set the underlying
`replMode` of an interactive `node` session. Its value, `magic`, is also
removed. Please use `sloppy` instead.

<a id="DEP0066"></a>
### DEP0066: `OutgoingMessage.prototype._headers, OutgoingMessage.prototype._headerNames`
<!-- YAML
changes:
  - version: v12.0.0
    pr-url: https://github.com/nodejs/node/pull/24167
    description: Runtime deprecation.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/10941
    description: Documentation-only deprecation.
-->

Type: Runtime

The `http` module `OutgoingMessage.prototype._headers` and
`OutgoingMessage.prototype._headerNames` properties are deprecated. Use one of
the public methods (e.g. `OutgoingMessage.prototype.getHeader()`,
`OutgoingMessage.prototype.getHeaders()`,
`OutgoingMessage.prototype.getHeaderNames()`,
`OutgoingMessage.prototype.hasHeader()`,
`OutgoingMessage.prototype.removeHeader()`,
`OutgoingMessage.prototype.setHeader()`) for working with outgoing headers.

The `OutgoingMessage.prototype._headers` and
`OutgoingMessage.prototype._headerNames` properties were never documented as
officially supported properties.

<a id="DEP0067"></a>
### DEP0067: `OutgoingMessage.prototype._renderHeaders`
<!-- YAML
changes:
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/10941
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The `http` module `OutgoingMessage.prototype._renderHeaders()` API is
deprecated.

The `OutgoingMessage.prototype._renderHeaders` property was never documented as
an officially supported API.

<a id="DEP0068"></a>
### DEP0068: `node debug`
<!-- YAML
changes:
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/11441
    description: Runtime deprecation.
-->

Type: Runtime

`node debug` corresponds to the legacy CLI debugger which has been replaced with
a V8-inspector based CLI debugger available through `node inspect`.

<a id="DEP0069"></a>
### DEP0069: `vm.runInDebugContext(string)`
<!-- YAML
changes:
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/13295
    description: End-of-Life.
  - version: v9.0.0
    pr-url: https://github.com/nodejs/node/pull/12815
    description: Runtime deprecation.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12243
    description: Documentation-only deprecation.
-->

Type: End-of-Life

DebugContext has been removed in V8 and is not available in Node.js 10+.

DebugContext was an experimental API.

<a id="DEP0070"></a>
### DEP0070: `async_hooks.currentId()`
<!-- YAML
changes:
  - version: v9.0.0
    pr-url: https://github.com/nodejs/node/pull/14414
    description: End-of-Life.
  - version: v8.2.0
    pr-url: https://github.com/nodejs/node/pull/13490
    description: Runtime deprecation.
-->

Type: End-of-Life

`async_hooks.currentId()` was renamed to `async_hooks.executionAsyncId()` for
clarity.

This change was made while `async_hooks` was an experimental API.

<a id="DEP0071"></a>
### DEP0071: `async_hooks.triggerId()`
<!-- YAML
changes:
  - version: v9.0.0
    pr-url: https://github.com/nodejs/node/pull/14414
    description: End-of-Life.
  - version: v8.2.0
    pr-url: https://github.com/nodejs/node/pull/13490
    description: Runtime deprecation.
-->

Type: End-of-Life

`async_hooks.triggerId()` was renamed to `async_hooks.triggerAsyncId()` for
clarity.

This change was made while `async_hooks` was an experimental API.

<a id="DEP0072"></a>
### DEP0072: `async_hooks.AsyncResource.triggerId()`
<!-- YAML
changes:
  - version: v9.0.0
    pr-url: https://github.com/nodejs/node/pull/14414
    description: End-of-Life.
  - version: v8.2.0
    pr-url: https://github.com/nodejs/node/pull/13490
    description: Runtime deprecation.
-->

Type: End-of-Life

`async_hooks.AsyncResource.triggerId()` was renamed to
`async_hooks.AsyncResource.triggerAsyncId()` for clarity.

This change was made while `async_hooks` was an experimental API.

<a id="DEP0073"></a>
### DEP0073: Several internal properties of `net.Server`
<!-- YAML
changes:
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/17141
    description: End-of-Life.
  - version: v9.0.0
    pr-url: https://github.com/nodejs/node/pull/14449
    description: Runtime deprecation.
-->

Type: End-of-Life

Accessing several internal, undocumented properties of `net.Server` instances
with inappropriate names is deprecated.

As the original API was undocumented and not generally useful for non-internal
code, no replacement API is provided.

<a id="DEP0074"></a>
### DEP0074: `REPLServer.bufferedCommand`
<!-- YAML
changes:
  - version: v9.0.0
    pr-url: https://github.com/nodejs/node/pull/13687
    description: Runtime deprecation.
-->

Type: Runtime

The `REPLServer.bufferedCommand` property was deprecated in favor of
[`REPLServer.clearBufferedCommand()`][].

<a id="DEP0075"></a>
### DEP0075: `REPLServer.parseREPLKeyword()`
<!-- YAML
changes:
  - version: v9.0.0
    pr-url: https://github.com/nodejs/node/pull/14223
    description: Runtime deprecation.
-->

Type: Runtime

`REPLServer.parseREPLKeyword()` was removed from userland visibility.

<a id="DEP0076"></a>
### DEP0076: `tls.parseCertString()`
<!-- YAML
changes:
  - version: v9.0.0
    pr-url: https://github.com/nodejs/node/pull/14249
    description: Runtime deprecation.
  - version: v8.6.0
    pr-url: https://github.com/nodejs/node/pull/14245
    description: Documentation-only deprecation.
-->

Type: Runtime

`tls.parseCertString()` is a trivial parsing helper that was made public by
mistake. This function can usually be replaced with:

```js
const querystring = require('querystring');
querystring.parse(str, '\n', '=');
```

This function is not completely equivalent to `querystring.parse()`. One
difference is that `querystring.parse()` does url decoding:

```sh
> querystring.parse('%E5%A5%BD=1', '\n', '=');
{ '好': '1' }
> tls.parseCertString('%E5%A5%BD=1');
{ '%E5%A5%BD': '1' }
```

<a id="DEP0077"></a>
### DEP0077: `Module._debug()`
<!-- YAML
changes:
  - version: v9.0.0
    pr-url: https://github.com/nodejs/node/pull/13948
    description: Runtime deprecation.
-->

Type: Runtime

`Module._debug()` is deprecated.

The `Module._debug()` function was never documented as an officially
supported API.

<a id="DEP0078"></a>
### DEP0078: `REPLServer.turnOffEditorMode()`
<!-- YAML
changes:
  - version: v9.0.0
    pr-url: https://github.com/nodejs/node/pull/15136
    description: Runtime deprecation.
-->

Type: Runtime

`REPLServer.turnOffEditorMode()` was removed from userland visibility.

<a id="DEP0079"></a>
### DEP0079: Custom inspection function on Objects via `.inspect()`
<!-- YAML
changes:
  - version: v11.0.0
    pr-url: https://github.com/nodejs/node/pull/20722
    description: End-of-Life.
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/16393
    description: Runtime deprecation.
  - version: v8.7.0
    pr-url: https://github.com/nodejs/node/pull/15631
    description: Documentation-only deprecation.
-->

Type: End-of-Life

Using a property named `inspect` on an object to specify a custom inspection
function for [`util.inspect()`][] is deprecated. Use [`util.inspect.custom`][]
instead. For backward compatibility with Node.js prior to version 6.4.0, both
may be specified.

<a id="DEP0080"></a>
### DEP0080: `path._makeLong()`
<!-- YAML
changes:
  - version: v9.0.0
    pr-url: https://github.com/nodejs/node/pull/14956
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The internal `path._makeLong()` was not intended for public use. However,
userland modules have found it useful. The internal API is deprecated
and replaced with an identical, public `path.toNamespacedPath()` method.

<a id="DEP0081"></a>
### DEP0081: `fs.truncate()` using a file descriptor
<!-- YAML
changes:
  - version: v9.0.0
    pr-url: https://github.com/nodejs/node/pull/15990
    description: Runtime deprecation.
-->

Type: Runtime

`fs.truncate()` `fs.truncateSync()` usage with a file descriptor is
deprecated. Please use `fs.ftruncate()` or `fs.ftruncateSync()` to work with
file descriptors.

<a id="DEP0082"></a>
### DEP0082: `REPLServer.prototype.memory()`
<!-- YAML
changes:
  - version: v9.0.0
    pr-url: https://github.com/nodejs/node/pull/16242
    description: Runtime deprecation.
-->

Type: Runtime

`REPLServer.prototype.memory()` is only necessary for the internal mechanics of
the `REPLServer` itself. Do not use this function.

<a id="DEP0083"></a>
### DEP0083: Disabling ECDH by setting `ecdhCurve` to `false`
<!-- YAML
changes:
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/19794
    description: End-of-Life.
  - version: v9.2.0
    pr-url: https://github.com/nodejs/node/pull/16130
    description: Runtime deprecation.
-->

Type: End-of-Life.

The `ecdhCurve` option to `tls.createSecureContext()` and `tls.TLSSocket` could
be set to `false` to disable ECDH entirely on the server only. This mode was
deprecated in preparation for migrating to OpenSSL 1.1.0 and consistency with
the client and is now unsupported. Use the `ciphers` parameter instead.

<a id="DEP0084"></a>
### DEP0084: requiring bundled internal dependencies
<!-- YAML
changes:
  - version: v12.0.0
    pr-url: https://github.com/nodejs/node/pull/25138
    description: This functionality has been removed.
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/16392
    description: Runtime deprecation.
-->

Type: End-of-Life

Since Node.js versions 4.4.0 and 5.2.0, several modules only intended for
internal usage were mistakenly exposed to user code through `require()`. These
modules were:

* `v8/tools/codemap`
* `v8/tools/consarray`
* `v8/tools/csvparser`
* `v8/tools/logreader`
* `v8/tools/profile_view`
* `v8/tools/profile`
* `v8/tools/SourceMap`
* `v8/tools/splaytree`
* `v8/tools/tickprocessor-driver`
* `v8/tools/tickprocessor`
* `node-inspect/lib/_inspect` (from 7.6.0)
* `node-inspect/lib/internal/inspect_client` (from 7.6.0)
* `node-inspect/lib/internal/inspect_repl` (from 7.6.0)

The `v8/*` modules do not have any exports, and if not imported in a specific
order would in fact throw errors. As such there are virtually no legitimate use
cases for importing them through `require()`.

On the other hand, `node-inspect` may be installed locally through a package
manager, as it is published on the npm registry under the same name. No source
code modification is necessary if that is done.

<a id="DEP0085"></a>
### DEP0085: AsyncHooks Sensitive API
<!-- YAML
changes:
  - version: 10.0.0
    pr-url: https://github.com/nodejs/node/pull/17147
    description: End-of-Life.
  - version:
    - v8.10.0
    - v9.4.0
    pr-url: https://github.com/nodejs/node/pull/16972
    description: Runtime deprecation.
-->

Type: End-of-Life

The AsyncHooks Sensitive API was never documented and had various minor issues.
Use the `AsyncResource` API instead. See
<https://github.com/nodejs/node/issues/15572>.

<a id="DEP0086"></a>
### DEP0086: Remove `runInAsyncIdScope`
<!-- YAML
changes:
  - version: 10.0.0
    pr-url: https://github.com/nodejs/node/pull/17147
    description: End-of-Life.
  - version:
    - v8.10.0
    - v9.4.0
    pr-url: https://github.com/nodejs/node/pull/16972
    description: Runtime deprecation.
-->

Type: End-of-Life

`runInAsyncIdScope` doesn't emit the `'before'` or `'after'` event and can thus
cause a lot of issues. See <https://github.com/nodejs/node/issues/14328>.

<a id="DEP0089"></a>
### DEP0089: `require('assert')`
<!-- YAML
changes:
  - version: v12.8.0
    pr-url: https://github.com/nodejs/node/pull/28892
    description: Deprecation revoked.
  - version:
      - v9.9.0
      - v10.0.0
    pr-url: https://github.com/nodejs/node/pull/17002
    description: Documentation-only deprecation.
-->

Type: Deprecation revoked

Importing assert directly was not recommended as the exposed functions use
loose equality checks. The deprecation was revoked because use of the `assert`
module is not discouraged, and the deprecation caused end user confusion.

<a id="DEP0090"></a>
### DEP0090: Invalid GCM authentication tag lengths
<!-- YAML
changes:
  - version: v11.0.0
    pr-url: https://github.com/nodejs/node/pull/17825
    description: End-of-Life.
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/18017
    description: Runtime deprecation.
-->

Type: End-of-Life

Node.js used to support all GCM authentication tag lengths which are accepted by
OpenSSL when calling [`decipher.setAuthTag()`][]. Beginning with Node.js
v11.0.0, only authentication tag lengths of 128, 120, 112, 104, 96, 64, and 32
bits are allowed. Authentication tags of other lengths are invalid per
[NIST SP 800-38D][].

<a id="DEP0091"></a>
### DEP0091: `crypto.DEFAULT_ENCODING`
<!-- YAML
changes:
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/18333
    description: Runtime deprecation.
-->

Type: Runtime

The [`crypto.DEFAULT_ENCODING`][] property is deprecated.

<a id="DEP0092"></a>
### DEP0092: Top-level `this` bound to `module.exports`
<!-- YAML
changes:
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/16878
    description: Documentation-only deprecation.
-->

Type: Documentation-only

Assigning properties to the top-level `this` as an alternative
to `module.exports` is deprecated. Developers should use `exports`
or `module.exports` instead.

<a id="DEP0093"></a>
### DEP0093: `crypto.fips` is deprecated and replaced.
<!-- YAML
changes:
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/18335
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [`crypto.fips`][] property is deprecated. Please use `crypto.setFips()`
and `crypto.getFips()` instead.

<a id="DEP0094"></a>
### DEP0094: Using `assert.fail()` with more than one argument.
<!-- YAML
changes:
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/18418
    description: Runtime deprecation.
-->

Type: Runtime

Using `assert.fail()` with more than one argument is deprecated. Use
`assert.fail()` with only one argument or use a different `assert` module
method.

<a id="DEP0095"></a>
### DEP0095: `timers.enroll()`
<!-- YAML
changes:
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/18066
    description: Runtime deprecation.
-->

Type: Runtime

`timers.enroll()` is deprecated. Please use the publicly documented
[`setTimeout()`][] or [`setInterval()`][] instead.

<a id="DEP0096"></a>
### DEP0096: `timers.unenroll()`
<!-- YAML
changes:
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/18066
    description: Runtime deprecation.
-->

Type: Runtime

`timers.unenroll()` is deprecated. Please use the publicly documented
[`clearTimeout()`][] or [`clearInterval()`][] instead.

<a id="DEP0097"></a>
### DEP0097: `MakeCallback` with `domain` property
<!-- YAML
changes:
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/17417
    description: Runtime deprecation.
-->

Type: Runtime

Users of `MakeCallback` that add the `domain` property to carry context,
should start using the `async_context` variant of `MakeCallback` or
`CallbackScope`, or the high-level `AsyncResource` class.

<a id="DEP0098"></a>
### DEP0098: AsyncHooks Embedder `AsyncResource.emitBefore` and `AsyncResource.emitAfter` APIs
<!-- YAML
changes:
  - version: v12.0.0
    pr-url: https://github.com/nodejs/node/pull/26530
    description: End-of-Life
  - version:
    - v8.12.0
    - v9.6.0
    - v10.0.0
    pr-url: https://github.com/nodejs/node/pull/18632
    description: Runtime deprecation.
-->

Type: End-of-Life

The embedded API provided by AsyncHooks exposes `.emitBefore()` and
`.emitAfter()` methods which are very easy to use incorrectly which can lead
to unrecoverable errors.

Use [`asyncResource.runInAsyncScope()`][] API instead which provides a much
safer, and more convenient, alternative. See
<https://github.com/nodejs/node/pull/18513>.

<a id="DEP0099"></a>
### DEP0099: async context-unaware `node::MakeCallback` C++ APIs
<!-- YAML
changes:
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/18632
    description: Compile-time deprecation.
-->

Type: Compile-time

Certain versions of `node::MakeCallback` APIs available to native modules are
deprecated. Please use the versions of the API that accept an `async_context`
parameter.

<a id="DEP0100"></a>
### DEP0100: `process.assert()`
<!-- YAML
changes:
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/18666
    description: Runtime deprecation.
  - version: v0.3.7
    description: Documentation-only deprecation.
-->

Type: Runtime

`process.assert()` is deprecated. Please use the [`assert`][] module instead.

This was never a documented feature.

<a id="DEP0101"></a>
### DEP0101: `--with-lttng`
<!-- YAML
changes:
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/18982
    description: End-of-Life.
-->

Type: End-of-Life

The `--with-lttng` compile-time option has been removed.

<a id="DEP0102"></a>
### DEP0102: Using `noAssert` in `Buffer#(read|write)` operations.
<!-- YAML
changes:
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/18395
    description: End-of-Life.
-->

Type: End-of-Life

Using the `noAssert` argument has no functionality anymore. All input is going
to be verified, no matter if it is set to true or not. Skipping the verification
could lead to hard to find errors and crashes.

<a id="DEP0103"></a>
### DEP0103: `process.binding('util').is[...]` typechecks
<!-- YAML
changes:
  - version: v10.9.0
    pr-url: https://github.com/nodejs/node/pull/22004
    description: Superseded by [DEP0111](#DEP0111).
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/18415
    description: Documentation-only deprecation.
-->

Type: Documentation-only (supports [`--pending-deprecation`][])

Using `process.binding()` in general should be avoided. The type checking
methods in particular can be replaced by using [`util.types`][].

This deprecation has been superseded by the deprecation of the
`process.binding()` API ([DEP0111](#DEP0111)).

<a id="DEP0104"></a>
### DEP0104: `process.env` string coercion
<!-- YAML
changes:
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/18990
    description: Documentation-only deprecation.
-->

Type: Documentation-only (supports [`--pending-deprecation`][])

When assigning a non-string property to [`process.env`][], the assigned value is
implicitly converted to a string. This behavior is deprecated if the assigned
value is not a string, boolean, or number. In the future, such assignment may
result in a thrown error. Please convert the property to a string before
assigning it to `process.env`.

<a id="DEP0105"></a>
### DEP0105: `decipher.finaltol`
<!-- YAML
changes:
  - version: v11.0.0
    pr-url: https://github.com/nodejs/node/pull/19941
    description: End-of-Life.
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/19353
    description: Runtime deprecation.
-->

Type: End-of-Life

`decipher.finaltol()` has never been documented and was an alias for
[`decipher.final()`][]. This API has been removed, and it is recommended to use
[`decipher.final()`][] instead.

<a id="DEP0106"></a>
### DEP0106: `crypto.createCipher` and `crypto.createDecipher`
<!-- YAML
changes:
  - version: v11.0.0
    pr-url: https://github.com/nodejs/node/pull/22089
    description: Runtime deprecation.
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/19343
    description: Documentation-only deprecation.
-->

Type: Runtime

Using [`crypto.createCipher()`][] and [`crypto.createDecipher()`][] should be
avoided as they use a weak key derivation function (MD5 with no salt) and static
initialization vectors. It is recommended to derive a key using
[`crypto.pbkdf2()`][] or [`crypto.scrypt()`][] and to use
[`crypto.createCipheriv()`][] and [`crypto.createDecipheriv()`][] to obtain the
[`Cipher`][] and [`Decipher`][] objects respectively.

<a id="DEP0107"></a>
### DEP0107: `tls.convertNPNProtocols()`
<!-- YAML
changes:
  - version: v11.0.0
    pr-url: https://github.com/nodejs/node/pull/20736
    description: End-of-Life.
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/19403
    description: Runtime deprecation.
-->

Type: End-of-Life

This was an undocumented helper function not intended for use outside Node.js
core and obsoleted by the removal of NPN (Next Protocol Negotiation) support.

<a id="DEP0108"></a>
### DEP0108: `zlib.bytesRead`
<!-- YAML
changes:
  - version: v11.0.0
    pr-url: https://github.com/nodejs/node/pull/23308
    description: Runtime deprecation.
  - version: v10.0.0
    pr-url: https://github.com/nodejs/node/pull/19414
    description: Documentation-only deprecation.
-->

Type: Runtime

Deprecated alias for [`zlib.bytesWritten`][]. This original name was chosen
because it also made sense to interpret the value as the number of bytes
read by the engine, but is inconsistent with other streams in Node.js that
expose values under these names.

<a id="DEP0109"></a>
### DEP0109: `http`, `https`, and `tls` support for invalid URLs
<!-- YAML
changes:
  - version: v11.0.0
    pr-url: https://github.com/nodejs/node/pull/20270
    description: Runtime deprecation.
-->

Type: Runtime

Some previously supported (but strictly invalid) URLs were accepted through the
[`http.request()`][], [`http.get()`][], [`https.request()`][],
[`https.get()`][], and [`tls.checkServerIdentity()`][] APIs because those were
accepted by the legacy `url.parse()` API. The mentioned APIs now use the WHATWG
URL parser that requires strictly valid URLs. Passing an invalid URL is
deprecated and support will be removed in the future.

<a id="DEP0110"></a>
### DEP0110: `vm.Script` cached data
<!-- YAML
changes:
  - version: v10.6.0
    pr-url: https://github.com/nodejs/node/pull/20300
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The `produceCachedData` option is deprecated. Use
[`script.createCachedData()`][] instead.

<a id="DEP0111"></a>
### DEP0111: `process.binding()`
<!-- YAML
changes:
  - version: v10.9.0
    pr-url: https://github.com/nodejs/node/pull/22004
    description: Documentation-only deprecation.
  - version: v11.12.0
    pr-url: https://github.com/nodejs/node/pull/26500
    description: Added support for `--pending-deprecation`.
-->

Type: Documentation-only (supports [`--pending-deprecation`][])

`process.binding()` is for use by Node.js internal code only.

<a id="DEP0112"></a>
### DEP0112: `dgram` private APIs
<!-- YAML
changes:
  - version: v11.0.0
    pr-url: https://github.com/nodejs/node/pull/22011
    description: Runtime deprecation.
-->

Type: Runtime

The `dgram` module previously contained several APIs that were never meant to
accessed outside of Node.js core: `Socket.prototype._handle`,
`Socket.prototype._receiving`, `Socket.prototype._bindState`,
`Socket.prototype._queue`, `Socket.prototype._reuseAddr`,
`Socket.prototype._healthCheck()`, `Socket.prototype._stopReceiving()`, and
`dgram._createSocketHandle()`.

<a id="DEP0113"></a>
### DEP0113: `Cipher.setAuthTag()`, `Decipher.getAuthTag()`
<!-- YAML
changes:
  - version: v12.0.0
    pr-url: https://github.com/nodejs/node/pull/26249
    description: End-of-Life.
  - version: v11.0.0
    pr-url: https://github.com/nodejs/node/pull/22126
    description: Runtime deprecation.
-->

Type: End-of-Life

`Cipher.setAuthTag()` and `Decipher.getAuthTag()` are no longer available. They
were never documented and would throw when called.

<a id="DEP0114"></a>
### DEP0114: `crypto._toBuf()`
<!-- YAML
changes:
  - version: v12.0.0
    pr-url: https://github.com/nodejs/node/pull/25338
    description: End-of-Life.
  - version: v11.0.0
    pr-url: https://github.com/nodejs/node/pull/22501
    description: Runtime deprecation.
-->

Type: End-of-Life

The `crypto._toBuf()` function was not designed to be used by modules outside
of Node.js core and was removed.

<a id="DEP0115"></a>
### DEP0115: `crypto.prng()`, `crypto.pseudoRandomBytes()`, `crypto.rng()`
<!-- YAML
changes:
  - version: v11.0.0
    pr-url:
      - https://github.com/nodejs/node/pull/22519
      - https://github.com/nodejs/node/pull/23017
    description: Added documentation-only deprecation
                 with `--pending-deprecation` support.
-->

Type: Documentation-only (supports [`--pending-deprecation`][])

In recent versions of Node.js, there is no difference between
[`crypto.randomBytes()`][] and `crypto.pseudoRandomBytes()`. The latter is
deprecated along with the undocumented aliases `crypto.prng()` and
`crypto.rng()` in favor of [`crypto.randomBytes()`][] and may be removed in a
future release.

<a id="DEP0116"></a>
### DEP0116: Legacy URL API
<!-- YAML
changes:
  - version: v11.0.0
    pr-url: https://github.com/nodejs/node/pull/22715
    description: Documentation-only deprecation.
-->

Type: Documentation-only

The [Legacy URL API][] is deprecated. This includes [`url.format()`][],
[`url.parse()`][], [`url.resolve()`][], and the [legacy `urlObject`][]. Please
use the [WHATWG URL API][] instead.

<a id="DEP0117"></a>
### DEP0117: Native crypto handles
<!-- YAML
changes:
  - version: v12.0.0
    pr-url: https://github.com/nodejs/node/pull/27011
    description: End-of-Life.
  - version: v11.0.0
    pr-url: https://github.com/nodejs/node/pull/22747
    description: Runtime deprecation.
-->

Type: End-of-Life

Previous versions of Node.js exposed handles to internal native objects through
the `_handle` property of the `Cipher`, `Decipher`, `DiffieHellman`,
`DiffieHellmanGroup`, `ECDH`, `Hash`, `Hmac`, `Sign`, and `Verify` classes.
The `_handle` property has been removed because improper use of the native
object can lead to crashing the application.

<a id="DEP0118"></a>
### DEP0118: `dns.lookup()` support for a falsy host name
<!-- YAML
changes:
  - version: v11.0.0
    pr-url: https://github.com/nodejs/node/pull/23173
    description: Runtime deprecation.
-->

Type: Runtime

Previous versions of Node.js supported `dns.lookup()` with a falsy host name
like `dns.lookup(false)` due to backward compatibility.
This behavior is undocumented and is thought to be unused in real world apps.
It will become an error in future versions of Node.js.

<a id="DEP0119"></a>
### DEP0119: `process.binding('uv').errname()` private API
<!-- YAML
changes:
  - version: v11.0.0
    pr-url: https://github.com/nodejs/node/pull/23597
    description: Documentation-only deprecation.
-->

Type: Documentation-only (supports [`--pending-deprecation`][])

`process.binding('uv').errname()` is deprecated. Please use
[`util.getSystemErrorName()`][] instead.

<a id="DEP0120"></a>
### DEP0120: Windows Performance Counter Support
<!-- YAML
changes:
  - version: v12.0.0
    pr-url: https://github.com/nodejs/node/pull/24862
    description: End-of-Life.
  - version: v11.0.0
    pr-url: https://github.com/nodejs/node/pull/22485
    description: Runtime deprecation.
-->

Type: End-of-Life

Windows Performance Counter support has been removed from Node.js. The
undocumented `COUNTER_NET_SERVER_CONNECTION()`,
`COUNTER_NET_SERVER_CONNECTION_CLOSE()`, `COUNTER_HTTP_SERVER_REQUEST()`,
`COUNTER_HTTP_SERVER_RESPONSE()`, `COUNTER_HTTP_CLIENT_REQUEST()`, and
`COUNTER_HTTP_CLIENT_RESPONSE()` functions have been deprecated.

<a id="DEP0121"></a>
### DEP0121: `net._setSimultaneousAccepts()`
<!-- YAML
changes:
  - version: v12.0.0
    pr-url: https://github.com/nodejs/node/pull/23760
    description: Runtime deprecation.
-->

Type: Runtime

The undocumented `net._setSimultaneousAccepts()` function was originally
intended for debugging and performance tuning when using the `child_process`
and `cluster` modules on Windows. The function is not generally useful and
is being removed. See discussion here:
https://github.com/nodejs/node/issues/18391

<a id="DEP0122"></a>
### DEP0122: `tls` `Server.prototype.setOptions()`
<!-- YAML
changes:
  - version: v12.0.0
    pr-url: https://github.com/nodejs/node/pull/23820
    description: Runtime deprecation.
-->

Type: Runtime

Please use `Server.prototype.setSecureContext()` instead.

<a id="DEP0123"></a>
### DEP0123: setting the TLS ServerName to an IP address
<!-- YAML
changes:
  - version: v12.0.0
    pr-url: https://github.com/nodejs/node/pull/23329
    description: Runtime deprecation.
-->

Type: Runtime

Setting the TLS ServerName to an IP address is not permitted by
[RFC 6066][]. This will be ignored in a future version.

<a id="DEP0124"></a>
### DEP0124: using `REPLServer.rli`
<!-- YAML
changes:
  - version: v12.0.0
    pr-url: https://github.com/nodejs/node/pull/26260
    description: Runtime deprecation.
-->

Type: Runtime

This property is a reference to the instance itself.

<a id="DEP0125"></a>
### DEP0125: `require('_stream_wrap')`
<!-- YAML
changes:
  - version: v12.0.0
    pr-url: https://github.com/nodejs/node/pull/26245
    description: Runtime deprecation.
-->

Type: Runtime

The `_stream_wrap` module is deprecated.

<a id="DEP0126"></a>
### DEP0126: `timers.active()`
<!-- YAML
changes:
  - version: v11.14.0
    pr-url: https://github.com/nodejs/node/pull/26760
    description: Runtime deprecation.
-->

Type: Runtime

The previously undocumented `timers.active()` is deprecated.
Please use the publicly documented [`timeout.refresh()`][] instead.
If re-referencing the timeout is necessary, [`timeout.ref()`][] can be used
with no performance impact since Node.js 10.

<a id="DEP0127"></a>
### DEP0127: `timers._unrefActive()`
<!-- YAML
changes:
  - version: v11.14.0
    pr-url: https://github.com/nodejs/node/pull/26760
    description: Runtime deprecation.
-->

Type: Runtime

The previously undocumented and "private" `timers._unrefActive()` is deprecated.
Please use the publicly documented [`timeout.refresh()`][] instead.
If unreferencing the timeout is necessary, [`timeout.unref()`][] can be used
with no performance impact since Node.js 10.

<a id="DEP0128"></a>
### DEP0128: modules with an invalid `main` entry and an `index.js` file
<!-- YAML
changes:
  - version: v12.0.0
    pr-url: https://github.com/nodejs/node/pull/26823
    description: Documentation-only.
-->

Type: Documentation-only (supports [`--pending-deprecation`][])

Modules that have an invalid `main` entry (e.g., `./does-not-exist.js`) and
also have an `index.js` file in the top level directory will resolve the
`index.js` file. That is deprecated and is going to throw an error in future
Node.js versions.

<a id="DEP0129"></a>
### DEP0129: `ChildProcess._channel`
<!-- YAML
changes:
  - version: v13.0.0
    pr-url: https://github.com/nodejs/node/pull/27949
    description: Runtime deprecation.
  - version: v11.14.0
    pr-url: https://github.com/nodejs/node/pull/26982
    description: Documentation-only.
-->

Type: Runtime

The `_channel` property of child process objects returned by `spawn()` and
similar functions is not intended for public use. Use `ChildProcess.channel`
instead.

<a id="DEP0130"></a>
### DEP0130: `Module.createRequireFromPath()`
<!-- YAML
changes:
  - version: v13.0.0
    pr-url: https://github.com/nodejs/node/pull/27951
    description: Runtime deprecation.
  - version: v12.2.0
    pr-url: https://github.com/nodejs/node/pull/27405
    description: Documentation-only.
-->

Type: Runtime

Module.createRequireFromPath() is deprecated. Please use
[`module.createRequire()`][] instead.

<a id="DEP0131"></a>
### DEP0131: Legacy HTTP parser
<!-- YAML
changes:
  - version: v13.0.0
    pr-url: https://github.com/nodejs/node/pull/29589
    description: This feature has been removed.
  - version: v12.3.0
    pr-url: https://github.com/nodejs/node/pull/27498
    description: Documentation-only.
-->

Type: End-of-Life

The legacy HTTP parser, used by default in versions of Node.js prior to 12.0.0,
is deprecated and has been removed in v13.0.0. Prior to v13.0.0, the
`--http-parser=legacy` command-line flag could be used to revert to using the
legacy parser.

<a id="DEP0132"></a>
### DEP0132: `worker.terminate()` with callback
<!-- YAML
changes:
  - version: v12.5.0
    pr-url: https://github.com/nodejs/node/pull/28021
    description: Runtime deprecation.
-->

Type: Runtime

Passing a callback to [`worker.terminate()`][] is deprecated. Use the returned
`Promise` instead, or a listener to the worker’s `'exit'` event.

<a id="DEP0133"></a>
### DEP0133: `http` `connection`
<!-- YAML
changes:
  - version: v12.12.0
    pr-url: https://github.com/nodejs/node/pull/29015
    description: Documentation-only deprecation.
-->

Type: Documentation-only

Prefer [`response.socket`][] over [`response.connection`][] and
[`request.socket`][] over [`request.connection`][].

<a id="DEP0134"></a>
### DEP0134: `process._tickCallback`
<!-- YAML
changes:
  - version: v12.12.0
    pr-url: https://github.com/nodejs/node/pull/29781
    description: Documentation-only deprecation.
-->
Type: Documentation-only (supports [`--pending-deprecation`][])

The `process._tickCallback` property was never documented as
an officially supported API.

<a id="DEP0135"></a>
### DEP0135: `WriteStream.open()` and `ReadStream.open()` are internal
<!-- YAML
changes:
  - version: v13.0.0
    pr-url: https://github.com/nodejs/node/pull/29061
-->

Type: Runtime

[`WriteStream.open()`][] and [`ReadStream.open()`][] are undocumented internal
APIs that do not make sense to use in userland. File streams should always be
opened through their corresponding factory methods [`fs.createWriteStream()`][]
and [`fs.createReadStream()`][]) or by passing a file descriptor in options.

<a id="DEP0136"></a>
### DEP0136: `http` `finished`
<!-- YAML
changes:
  - version: v13.4.0
    pr-url: https://github.com/nodejs/node/pull/28679
    description: Documentation-only deprecation.
-->

Type: Documentation-only

[`response.finished`][] indicates whether [`response.end()`][] has been
called, not whether `'finish'` has been emitted and the underlying data
is flushed.

Use [`response.writableFinished`][] or [`response.writableEnded`][]
accordingly instead to avoid the ambigiuty.

To maintain existing behaviour `response.finished` should be replaced with
`response.writableEnded`.

<a id="DEP0137"></a>
### DEP0137: Closing fs.FileHandle on garbage collection
<!-- YAML
changes:
  - version: v14.0.0
    pr-url: https://github.com/nodejs/node/pull/28396
    description: Runtime deprecation.
-->

Type: Runtime

Allowing a [`fs.FileHandle`][] object to be closed on garbage collection is
deprecated. In the future, doing so may result in a thrown error that will
terminate the process.

Please ensure that all `fs.FileHandle` objects are explicitly closed using
`FileHandle.prototype.close()` when the `fs.FileHandle` is no longer needed:

```js
const fsPromises = require('fs').promises;
async function openAndClose() {
  let filehandle;
  try {
    filehandle = await fsPromises.open('thefile.txt', 'r');
  } finally {
    if (filehandle !== undefined)
      await filehandle.close();
  }
}
```

<a id="DEP0138"></a>
### DEP0138: `process.mainModule`
<!-- YAML
changes:
  - version: v14.0.0
    pr-url: https://github.com/nodejs/node/pull/32232
    description: Documentation-only deprecation.
-->

Type: Documentation-only

[`process.mainModule`][] is a CommonJS-only feature while `process` global
object is shared with non-CommonJS environment. Its use within ECMAScript
modules is unsupported.

It is deprecated in favor of [`require.main`][], because it serves the same
purpose and is only available on CommonJS environment.

<a id="DEP0139"></a>
### DEP0139: `process.umask()` with no arguments
<!-- YAML
changes:
  - version: v14.0.0
    pr-url: https://github.com/nodejs/node/pull/32499
    description: Documentation-only deprecation.
-->

Type: Documentation-only

Calling `process.umask()` with no argument causes the process-wide umask to be
written twice. This introduces a race condition between threads, and is a
potential security vulnerability. There is no safe, cross-platform alternative
API.

[`--pending-deprecation`]: cli.html#cli_pending_deprecation
[`--throw-deprecation`]: cli.html#cli_throw_deprecation
[`Buffer.allocUnsafeSlow(size)`]: buffer.html#buffer_class_method_buffer_allocunsafeslow_size
[`Buffer.from(array)`]: buffer.html#buffer_class_method_buffer_from_array
[`Buffer.from(buffer)`]: buffer.html#buffer_class_method_buffer_from_buffer
[`Buffer.isBuffer()`]: buffer.html#buffer_class_method_buffer_isbuffer_obj
[`Cipher`]: crypto.html#crypto_class_cipher
[`Decipher`]: crypto.html#crypto_class_decipher
[`EventEmitter.listenerCount(emitter, eventName)`]: events.html#events_eventemitter_listenercount_emitter_eventname
[`REPLServer.clearBufferedCommand()`]: repl.html#repl_replserver_clearbufferedcommand
[`ReadStream.open()`]: fs.html#fs_class_fs_readstream
[`Server.connections`]: net.html#net_server_connections
[`Server.getConnections()`]: net.html#net_server_getconnections_callback
[`Server.listen({fd: <number>})`]: net.html#net_server_listen_handle_backlog_callback
[`SlowBuffer`]: buffer.html#buffer_class_slowbuffer
[`WriteStream.open()`]: fs.html#fs_class_fs_writestream
[`assert`]: assert.html
[`asyncResource.runInAsyncScope()`]: async_hooks.html#async_hooks_asyncresource_runinasyncscope_fn_thisarg_args
[`child_process`]: child_process.html
[`clearInterval()`]: timers.html#timers_clearinterval_timeout
[`clearTimeout()`]: timers.html#timers_cleartimeout_timeout
[`console.error()`]: console.html#console_console_error_data_args
[`console.log()`]: console.html#console_console_log_data_args
[`crypto.DEFAULT_ENCODING`]: crypto.html#crypto_crypto_default_encoding
[`crypto.createCipher()`]: crypto.html#crypto_crypto_createcipher_algorithm_password_options
[`crypto.createCipheriv()`]: crypto.html#crypto_crypto_createcipheriv_algorithm_key_iv_options
[`crypto.createDecipher()`]: crypto.html#crypto_crypto_createdecipher_algorithm_password_options
[`crypto.createDecipheriv()`]: crypto.html#crypto_crypto_createdecipheriv_algorithm_key_iv_options
[`crypto.fips`]: crypto.html#crypto_crypto_fips
[`crypto.pbkdf2()`]: crypto.html#crypto_crypto_pbkdf2_password_salt_iterations_keylen_digest_callback
[`crypto.randomBytes()`]: crypto.html#crypto_crypto_randombytes_size_callback
[`crypto.scrypt()`]: crypto.html#crypto_crypto_scrypt_password_salt_keylen_options_callback
[`decipher.final()`]: crypto.html#crypto_decipher_final_outputencoding
[`decipher.setAuthTag()`]: crypto.html#crypto_decipher_setauthtag_buffer
[`domain`]: domain.html
[`ecdh.setPublicKey()`]: crypto.html#crypto_ecdh_setpublickey_publickey_encoding
[`emitter.listenerCount(eventName)`]: events.html#events_emitter_listenercount_eventname
[`fs.FileHandle`]: fs.html#fs_class_filehandle
[`fs.access()`]: fs.html#fs_fs_access_path_mode_callback
[`fs.createReadStream()`]: fs.html#fs_fs_createreadstream_path_options
[`fs.createWriteStream()`]: fs.html#fs_fs_createwritestream_path_options
[`fs.exists(path, callback)`]: fs.html#fs_fs_exists_path_callback
[`fs.lchmod(path, mode, callback)`]: fs.html#fs_fs_lchmod_path_mode_callback
[`fs.lchmodSync(path, mode)`]: fs.html#fs_fs_lchmodsync_path_mode
[`fs.lchown(path, uid, gid, callback)`]: fs.html#fs_fs_lchown_path_uid_gid_callback
[`fs.lchownSync(path, uid, gid)`]: fs.html#fs_fs_lchownsync_path_uid_gid
[`fs.read()`]: fs.html#fs_fs_read_fd_buffer_offset_length_position_callback
[`fs.readSync()`]: fs.html#fs_fs_readsync_fd_buffer_offset_length_position
[`fs.stat()`]: fs.html#fs_fs_stat_path_options_callback
[`http.get()`]: http.html#http_http_get_options_callback
[`http.request()`]: http.html#http_http_request_options_callback
[`https.get()`]: https.html#https_https_get_options_callback
[`https.request()`]: https.html#https_https_request_options_callback
[`module.createRequire()`]: modules.html#modules_module_createrequire_filename
[`os.networkInterfaces()`]: os.html#os_os_networkinterfaces
[`os.tmpdir()`]: os.html#os_os_tmpdir
[`process.env`]: process.html#process_process_env
[`process.mainModule`]: process.html#process_process_mainmodule
[`punycode`]: punycode.html
[`require.extensions`]: modules.html#modules_require_extensions
[`require.main`]: modules.html#modules_accessing_the_main_module
[`request.socket`]: http.html#http_request_socket
[`request.connection`]: http.html#http_request_connection
[`response.socket`]: http.html#http_response_socket
[`response.connection`]: http.html#http_response_connection
[`response.end()`]: http.html#http_response_end_data_encoding_callback
[`response.finished`]: #http_response_finished
[`response.writableFinished`]: #http_response_writablefinished
[`response.writableEnded`]: #http_response_writableended
[`script.createCachedData()`]: vm.html#vm_script_createcacheddata
[`setInterval()`]: timers.html#timers_setinterval_callback_delay_args
[`setTimeout()`]: timers.html#timers_settimeout_callback_delay_args
[`timeout.ref()`]: timers.html#timers_timeout_ref
[`timeout.refresh()`]: timers.html#timers_timeout_refresh
[`timeout.unref()`]: timers.html#timers_timeout_unref
[`tls.CryptoStream`]: tls.html#tls_class_cryptostream
[`tls.SecureContext`]: tls.html#tls_tls_createsecurecontext_options
[`tls.SecurePair`]: tls.html#tls_class_securepair
[`tls.TLSSocket`]: tls.html#tls_class_tls_tlssocket
[`tls.checkServerIdentity()`]: tls.html#tls_tls_checkserveridentity_hostname_cert
[`tls.createSecureContext()`]: tls.html#tls_tls_createsecurecontext_options
[`url.format()`]: url.html#url_url_format_urlobject
[`url.parse()`]: url.html#url_url_parse_urlstring_parsequerystring_slashesdenotehost
[`url.resolve()`]: url.html#url_url_resolve_from_to
[`util._extend()`]: util.html#util_util_extend_target_source
[`util.getSystemErrorName()`]: util.html#util_util_getsystemerrorname_err
[`util.inspect()`]: util.html#util_util_inspect_object_options
[`util.inspect.custom`]: util.html#util_util_inspect_custom
[`util.isArray()`]: util.html#util_util_isarray_object
[`util.isBoolean()`]: util.html#util_util_isboolean_object
[`util.isBuffer()`]: util.html#util_util_isbuffer_object
[`util.isDate()`]: util.html#util_util_isdate_object
[`util.isError()`]: util.html#util_util_iserror_object
[`util.isFunction()`]: util.html#util_util_isfunction_object
[`util.isNull()`]: util.html#util_util_isnull_object
[`util.isNullOrUndefined()`]: util.html#util_util_isnullorundefined_object
[`util.isNumber()`]: util.html#util_util_isnumber_object
[`util.isObject()`]: util.html#util_util_isobject_object
[`util.isPrimitive()`]: util.html#util_util_isprimitive_object
[`util.isRegExp()`]: util.html#util_util_isregexp_object
[`util.isString()`]: util.html#util_util_isstring_object
[`util.isSymbol()`]: util.html#util_util_issymbol_object
[`util.isUndefined()`]: util.html#util_util_isundefined_object
[`util.log()`]: util.html#util_util_log_string
[`util.types`]: util.html#util_util_types
[`util`]: util.html
[`worker.exitedAfterDisconnect`]: cluster.html#cluster_worker_exitedafterdisconnect
[`worker.terminate()`]: worker_threads.html#worker_threads_worker_terminate
[`zlib.bytesWritten`]: zlib.html#zlib_zlib_byteswritten
[Legacy URL API]: url.html#url_legacy_url_api
[NIST SP 800-38D]: https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-38d.pdf
[RFC 6066]: https://tools.ietf.org/html/rfc6066#section-3
[WHATWG URL API]: url.html#url_the_whatwg_url_api
[alloc]: buffer.html#buffer_class_method_buffer_alloc_size_fill_encoding
[alloc_unsafe_size]: buffer.html#buffer_class_method_buffer_allocunsafe_size
[from_arraybuffer]: buffer.html#buffer_class_method_buffer_from_arraybuffer_byteoffset_length
[from_string_encoding]: buffer.html#buffer_class_method_buffer_from_string_encoding
[legacy `urlObject`]: url.html#url_legacy_urlobject
