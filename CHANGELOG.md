# io.js ChangeLog

## 2015-02-03, Version 1.1.0, @chrisdickinson

### Notable changes

* debug: fix v8 post-mortem debugging.
* crypto: publicEncrypt now supports password-protected private keys.
* crypto: ~30% speedup on hashing functions.
* errors
  - better formatting via util.inspect
  - more descriptive errors from fs. This necessitated a `NODE_MODULE_VERSION` bump.
  - more descriptive errors from http.setHeader
* dep updates:
  - npm: upgrade to 2.4.1
  - http-parser: rollback to 2.3.0
  - libuv: update to 1.3.0
  - v8: update to 4.1.0.14
* http.request: inherited properties on options are now respected
* add iterable interface to buffers (`for (let byte of buffer.values()) { }`)
* fs: fix fd leak on `fs.createReadStream`. See 497fd72 for details.
* installer: on Windows, emit WM_SETTINGCHANGE after install to make other running
  processes aware of the PATH changes.
* Added new collaborators:
  - Vladimir Kurchatkin (@vkurchatkin)
  - Micleușanu Nicu (@micnic)

### Known issues

* Surrogate pair in REPL can freeze terminal (https://github.com/iojs/io.js/issues/690)
* Not possible to build io.js as a static library (https://github.com/iojs/io.js/issues/686)

### Commits

* df48faf - tools: add release tool and docs, remove old tools (Rod Vagg)
* 14684d3 - v8abbr: ASCIISTRINGTAG => ONEBYTESTRINGTAG (Fedor Indutny)
* 6a5d731 - gyp: enable postmortem support, fix dtrace paths (Fedor Indutny)
* 8b88ff8 - deps: fix postmortem support in v8 (Fedor Indutny)
* d0b0bb4 - dtrace: fix removal of unused probes (Glen Keane)
* 3e67d7e - http: replace util.\_extend() with [].slice() (Jonathan Ong)
* 89dd8e0 - benchmark: clean up common.js (Brendan Ashworth)
* 6561274 - crypto: support passwords in publicEncrypt (Calvin Metcalf)
* e9eb2ec - process: fix regression in unlistening signals (Sam Roberts)
* 233e333 - events: remove indeterminancy from event ordering (Sam Roberts)
* d75fecf - src: remove unused dtrace probes (Glen Keane)
* 8c0742f - net: check close callback is function (Yosuke Furukawa)
* 207e48c - dgram: check close callback is function (Yosuke Furukawa)
* 6ac8bdc - lib: reduce util.is*() usage (cjihrig)
* bce7a26 - deps: make node-gyp work again on windows (Bert Belder)
* 1bdd74d - deps: make node-gyp fetch tarballs from iojs.org (Ben Noordhuis)
* faf34ff - deps: upgrade npm to 2.4.1 (Forrest L Norvell)
* 40e29dc - assert: use util.inspect() to create error messages (cjihrig)
* bc2c85c - fs: improve error messages (Bert Belder)
* 0767c2f - lib: fix max size check in Buffer constructor (Ben Noordhuis)
* 65b1e4f - dgram: implicit binds should be exclusive (Sam Roberts)
* 083c421 - benchmark: remove extra spacing in http options (Brendan Ashworth)
* e17e6fb - util: use on-stack buffer for Utf8Value (Fedor Indutny)
* 3d4e96f - crypto: use on-stack storage in HashUpdate (Fedor Indutny)
* aca2011 - string_bytes: introduce InlineDecoder (Fedor Indutny)
* c6367e7 - node: speed up ParseEncoding (Fedor Indutny)
* 7604e6d - docs: add note about default padding in crypto (Calvin Metcalf)
* cf3e908 - http: more descriptive setHeader errors (Qasim Zaidi)
* cbc1262 - deps: upgrade v8 to 4.1.0.14 (Ben Noordhuis)
* 00f822f - doc: add micnic as collaborator (Micleusanu Nicu)
* 514b1d9 - doc: add more info to benchmark/README.md (Fishrock123)
* 097fde7 - deps: update libuv to 1.3.0 (Saúl Ibarra Corretgé)
* 6ad236c - build: configure formatting, add final message (Roman Reiss)
* dd47a8c - src: set default signal dispositions at start-up (Ben Noordhuis)
* 63ae1d2 - src: rework early debug signal handling (Ben Noordhuis)
* 5756f92 - src: do platform-specific initialization earlier (Ben Noordhuis)
* 24bd4e0 - test: add http upgrade header regression test (Ben Noordhuis)
* 6605096 - deps: roll back http_parser to 2.3.0 (Ben Noordhuis)
* 90ddb46 - crypto: remove use of this.\_readableState (Calvin Metcalf)
* 45d8d9f - buffer: implement `iterable` interface (Vladimir Kurchatkin)
* 3cbb5cd - console: allow Object.prototype fields as labels (cjihrig)
* 87e62bd - crypto: implement privateEncrypt/publicDecrypt (Fedor Indutny)
* b50fea4 - watchdog: fix timeout for early polling return (Saúl Ibarra Corretgé)
* b5166cb - benchmark: add bench-(url & events) make targets (Yosuke Furukawa)
* 5843ae8 - Revert "doc: clarify fs.symlink and fs.symlinkSync parameters" (Bert Belder)
* 668bde8 - win,msi: broadcast WM_SETTINGCHANGE after install (Mathias Küsel)
* 69ce064 - build: remove artefacts on distclean (Johan Bergström)
* 1953886 - test: fs.createReadStream().destroy() fd leak (Rod Vagg)
* 497fd72 - fs: fix fd leak in ReadStream.destroy() (Alex Kocharin)
* 8b09ae7 - doc: add links for http_parser/libuv upgrades (Michael Hart)
* 683e096 - src: remove excessive license boilerplate (Aleksey Smolenchuk)
* 5c7ab96 - doc: fix net.Server.listen bind behavior (Andres Suarez)
* 84b05d4 - doc: update writable streams default encoding (Johnny Ray Austin)
* 1855267 - doc: fix minor grammar mistake in streams docs (ttrfwork)
* 4f68369 - build: disable v8 snapshots (Ben Noordhuis)
* c0a9d1b - versions: add http-parser patchlevel (Johan Bergström)
* 7854811 - child_process: clone spawn options argument (cjihrig)
* 88aaff9 - deps: update http_parser to 2.4.2 (Fedor Indutny)
* 804ab7e - doc: add seishun as a collaborator (Nikolai Vavilov)
* 301a968 - child_process: remove redundant condition (Vladimir Kurchatkin)
* 06cfff9 - http: don't bother making a copy of the options (Jonathan Ong)
* 55c222c - doc: add vkurchatkin as collaborator (Vladimir Kurchatkin)
* 50ac4b7 - Working on 1.0.5 (Rod Vagg)
* d1fc9c6 - 2015-01-24 io.js v1.0.4 Release (Rod Vagg)

## 2015-01-24, Version 1.0.4, @rvagg

### Notable changes

* npm upgrade to 2.3.0 fixes Windows "uid is undefined" errors
* crypto.pseudoRandomBytes() is now an alias for crypto.randomBytes()
  and will block if there is insufficient entropy to produce secure
  values. See https://github.com/iojs/io.js/commit/e5e5980 for details.
* Patch for V8 to properly detect ARMv6; binaries now work again on
  ARMv6 (Raspberry Pi etc.)
* Minor V8 upgrade from 4.1.0.7 to 4.1.0.12
* 'punycode' core module bumped from stability level 2-Unstable,
  to 3-Stable
* Added new collaborators:
  - Thorsten Lorenz (@thlorenz)
  - Stephen Belanger (@qard)
  - Jeremiah Senkpiel (@fishrock123)
  - Evan Lucas (@evanlucas)
  - Brendan Ashworth (@brendanashworth)

### Commits

* bb766d2 - doc: update "net" section in node to io.js changes (Andres Suarez)
* 73ddaa6 - tools: remove old updateAuthors.awk script (Rod Vagg)
* 6230bf9 - doc: update AUTHORS list (Rod Vagg)
* 33186fa - doc: adds brendanashworth as collaborator (Brendan Ashworth)
* 8f9502a - doc: add evanlucas to collaborators (Evan Lucas)
* 35a4f11 - doc: alphabetize all.markdown (Brendan Ashworth)
* a0831c5 - doc: add Fishrock123 to collaborators (Fishrock123)
* 5412487 - doc: add qard to collaborators (Stephen Belanger)
* 8b55048 - deps: make node-gyp work again on windows (Bert Belder)
* 82227f3 - deps: make node-gyp fetch tarballs from iojs.org (Ben Noordhuis)
* f5b35db - deps: upgrade npm to 2.3.0 (Forrest L Norvell)
* f3fed51 - doc: adding thlorenz to list of collaborators (Thorsten Lorenz)
* 8de89ec - lib: move default address logic to `net._listen2` (Vladimir Kurchatkin)
* 3143d73 - test: delete parallel/test-process-active-wraps (Ben Noordhuis)
* 4f95b5d - test: fix parallel/test-http-destroyed-socket-write2 (Ben Noordhuis)
* 5ba307a - test: fix parallel/test-dgram-error-message-address (Ben Noordhuis)
* f4c536b - debugger: don't override module binding (Vladimir Kurchatkin)
* 40ffed8 - stream: use nop as write() callback if omitted (cjihrig)
* df0d790 - doc: dns.lookupService has wrong header level (Icer Liang)
* 8b1db9c - doc: note in docs about missing interfaces (Todd Kennedy)
* 2928ac6 - doc: bump punycode api stability to 'stable' (Ben Noordhuis)
* 328e67b - doc: add TC meeting 2015-01-21 minutes (Rod Vagg)
* e5e5980 - lib,src: make pseudoRandomBytes alias randomBytes (Calvin Metcalf)
* c6cd460 - configure: remove unused arm_neon variable (Ben Noordhuis)
* 7d9d756 - configure: disable vfpv3 on armv6 (Ben Noordhuis)
* 297cadb - deps: fix v8 armv6 run-time detection (Ben Noordhuis)
* d481bb6 - doc: more explicit crypto.pseudoRandomBytes docs (Calvin Metcalf)
* 7d46247 - src: s/node/io.js/ in `iojs --help` message (Ben Noordhuis)
* 069c0df - deps: upgrade v8 to 4.1.0.12 (Ben Noordhuis)
* ada2a43 - doc: add TC meeting 2015-01-13 minutes (Rod Vagg)
* 60402b9 - docs: remove incorrect entry from changelog (Bert Belder)
* 8b98096 - fs: make fs.access() flags read only (Jackson Tian)
* 804e7aa - lib: use const to define constants (cjihrig)
* 803883b - v8: fix template literal NULL pointer deref (Ben Noordhuis)
* 5435cf2 - v8: optimize `getHeapStatistics` (Vladimir Kurchatkin)
* 5d01463 - benchmark: print score to five decimal places (Yosuke Furukawa)
* 752585d - src: silence clang warnings (Trevor Norris)
* 22e1aea - src: set node_is_initialized in node::Init (Cheng Zhao)
* 668420d - src: clean up unused macros in node_file.cc (Ben Noordhuis)
* 52f624e - src: rename ASSERT macros in node_crypto.cc (Ben Noordhuis)
* e95cfe1 - src: add ASSERT_EQ style macros (Ben Noordhuis)
* ee9cd00 - lib: fix TypeError with EventEmitter#on() abuse (Ben Noordhuis)
* 77d6807 - test: fix event-emitter-get-max-listeners style (Ben Noordhuis)
* 767ee73 - test: strip copyright boilerplate (Ben Noordhuis)
* 86eda17 - fs: define constants with const (cjihrig)

## 2015-01-20, Version 1.0.3, @rvagg

### Notable changes

* V8 upgrade from 3.31 to 4.1, this is not a major upgrade, the version number "4.1" signifies tracking towards Chrome 41. The 3.31 branch is now not tracking towards a stable release.
* Re-enable Windows XP / 2003 support
* npm upgrade to 2.2.0
* Improved FreeBSD support

### Known issues

* ARMv6 builds still not working, there is a hold-up in V8 on this, issue #283
* Template strings can cause segfaults in V8 4.1, https://codereview.chromium.org/857433004, also issue #333

### Commits

* 9419e1f - src: fix inconsistency between a check and error (toastynerd)
* 03ee4d8 - fs: add error code on null byte paths (cjihrig)
* e2558f0 - net: fix error details in connect() (cjihrig)
* 4af5746 - win,build: remove duplicate definition (Bert Belder)
* e8d0850 - win: bring back xp/2k3 support (Bert Belder)
* 4dd22b9 - cluster: avoid race enabling debugger in worker (Timothy J Fontaine)
* 6b91c78 - test: reland changes from 11c1bae (Ben Noordhuis)
* 992a1e7 - test: debug-signal-cluster should not be racey (Timothy J Fontaine)
* cdf0df1 - test: temporarily back out changes from 11c1bae (Ben Noordhuis)
* 1ea607c - test: move sequential/test-debug-port-from-cmdline (Ben Noordhuis)
* 2f33e00 - test: fix test-debug-port-from-cmdline.js (Julien Gilli)
* b7365c1 - repl: make REPL support multiline template literals (Xiaowei Li)
* 2253d30 - build: remove unused variable (Johan Bergström)
* ab04a43 - doc: add optional sudo to make install in README (Glen Keane)
* 1b1cd1c - build: shorten configurate script print on stdout (Roman Reiss)
* d566ded - deps: fix V8 debugger bugs (Jay Jaeho Lee)
* 6f36630 - doc: fix util.isBuffer examples (Thomas Jensen)
* 3abfb56 - benchmark: fix tcp bench after internal api change (Yosuke Furukawa)
* 50177fb - benchmark: stop v8 benchmark clobbering RegExp (Ben Noordhuis)
* 1952219 - deps: make node-gyp work again on windows (Bert Belder)
* a28de9b - deps: make node-gyp fetch tarballs from iojs.org (Ben Noordhuis)
* 9dc8f59 - deps: upgrade npm to 2.2.0 (Forrest L Norvell)
* e8ad773 - src: remove --noharmony_classes again (Ben Noordhuis)
* 334020e - deps: fix v8 build on FreeBSD (Fedor Indutny)
* 5e7ebc7 - deps: upgrade v8 to 4.1.0.7 (Ben Noordhuis)
* ea7750b - benchmark: add filter option for benchmark (Yosuke Furukawa)
* 4764eef - doc: fixed punctuation (Brenard Cubacub)
* de224d6 - configure: remove --ninja switch (Ben Noordhuis)
* 48774ec0 - configure: print warning for old compilers (Ben Noordhuis)
* daf9562 - doc: change to iojs from node in the usage message (Jongyeol Choi)
* 3fde649 - build: add tools/gflags to PYTHONPATH (Shigeki Ohtsu)
* 8b22df1 - doc: add python-gflags LICENSE block (Shigeki Ohtsu)
* 6242229 - tools: add python-gflags module (Shigeki Ohtsu)

## 2015-01-16, Version 1.0.2, @rvagg

### Notable changes

* Windows installer fixes
* Bundled node-gyp fixes for Windows
* [http_parser v2.4.1 upgrade](https://github.com/joyent/http-parser/compare/v2.3...v2.4.1)
* [libuv v1.2.1 upgrade](https://github.com/libuv/libuv/compare/v1.2.0...v1.2.1)

### Commits

* 265cb76 - build: add new installer config for OS X (Rod Vagg)
* 8cf6079 - doc: update AUTHORS list (Rod Vagg)
* c80a944 - doc: Add http keepalive behavior to CHANGELOG.md (Isaac Z. Schlueter)
* 9b81c3e - doc: fix author attribution (Tom Hughes)
* fd30eb2 - src: fix jslint errors (Yosuke Furukawa)
* 946eabd - tools: update closure linter to 2.3.17 (Yosuke Furukawa)
* 9e62ae4 - _debug_agent: use `readableObjectMode` option (Vladimir Kurchatkin)
* eec4c81 - doc: fix formatting in LICENSE for RTF generation (Rod Vagg)
* e789103 - doc: fix 404s for syntax highlighting js (Phil Hughes)
* ca039b4 - src: define AI_V4MAPPED for OpenBSD (Aaron Bieber)
* 753fcaa - doc: extend example of http.request by end event (Michal Tehnik)
* 8440cac - src: fix documentation url in help message (Shigeki Ohtsu)
* 24def66 - win,msi: warn that older io.js needs manual uninstall (Bert Belder)
* 59d9361 - win,msi: change UpgradeCode (Bert Belder)
* 5de334c - deps: make node-gyp work again on windows (Bert Belder)
* 07bd05b - deps: update libuv to 1.2.1 (Saúl Ibarra Corretgé)
* e177377 - doc: mention io.js alongside Node in Punycode docs (Mathias Bynens)
* 598efcb - deps: update http_parser to 2.4.1 (Fedor Indutny)
* 3dd7ebb - doc: update cluster entry in CHANGELOG (Ben Noordhuis)
* 0c5de1f - doc: fix double smalloc example (Mathias Buus)

## 2015-01-14, Version 1.0.1, @rvagg

Rebuild due to stale build slave git reflogs for 1.0.0 release

* doc: improve write style consistency (Rui Marinho)
* win,msi: correct doc website link (Bert Belder)

--------------------------------------

Below is a summary of the user-facing changes to be found in the io.js v1.0.0 release as compared to the
current _stable_ Node.js release, v0.10.35. At the time of the v1.0.0 release, the latest _unstable_
Node.js release is v0.11.14 with much progress made towards a v0.11.15 release. The io.js codebase inherits
the majority of the changes found in the v0.11 branch of the [joyent/node](https://github.com/joyent/node)
repository and therefore can be seen as an extension to v0.11.

## Summary of changes from Node.js v0.10.35 to io.js v1.0.0

### General

- The V8 JavaScript engine bundled with io.js was upgraded dramatically, from version 3.14.5.9 in Node.js v0.10.35 and 3.26.33 in Node.js v0.11.14 to 3.31.74.1 for io.js v1.0.0. This brings along many fixes and performance improvements, as well as additional support for new ES6 language features! For more information on this, check out [the io.js ES6 page](https://iojs.org/es6.html).
- Other bundled technologies were upgraded:
  - c-ares: 1.9.0-DEV to 1.10.0-DEV
  - http_parser: 1.0 to 2.3
  - libuv: 0.10.30 to 1.2.0
  - npm: 1.4.28 to 2.1.18
  - openssl: 1.0.1j to 1.0.1k
  - punycode: 1.2.0 to 1.3.2.
- Performance and stability improvements on all platforms.

### buffer

https://iojs.org/api/buffer.html

- Added `buf.writeUIntLE`, `buf.writeUIntBE`, `buf.writeIntLE`, `buf.writeIntBE`, `buf.readUIntLE`, `buf.readUIntBE`, `buf.readIntLE` and `buf.readIntBE` methods that read and write value up to 6 bytes.
- Added `Buffer.compare()` which does a `memcmp()` on two Buffer instances. Instances themselves also have a `compare()`.
- Added `buffer.equals()` that checks equality of Buffers by their contents.
- Added `new Buffer(otherBuffer)` constructor.
- Tweaked `SlowBuffer`'s semantics.
- Updated the output of `buffer.toJSON()` to not be the same as an array. Instead it is an object specifically tagged as a buffer, which can be recovered by passing it to (a new overload of) the `Buffer` constructor.

### child_process

https://iojs.org/api/child_process.html

- Added a `shell` option to `child_process.exec`.
- Added synchronous counterparts for the child process functions: `child_process.spawnSync`, `child_process.execSync`, and `child_process.execFileSync`.
- Added the path to any `ENOENT` errors, for easier debugging.

### console

https://iojs.org/api/console.html

- Added an `options` parameter to `console.dir`.

### cluster

https://iojs.org/api/cluster.html

- Updated `cluster` to use round-robin load balancing by default on non-Windows platforms. The scheduling policy is configurable however.
- `--debug` has been made cluster-aware.
- Many bug fixes.

### crypto

https://iojs.org/api/crypto.html

- Added support for custom generator values to `DiffieHellman` (defaulting to 2 for backwards compatibility).
- Added support for custom pbkdf2 digest methods.
- Added support for elliptic curve-based Diffie-Hellman.
- Added support for loading and setting the engine for some/all OpenSSL functions.
- Added support for passing in a passphrase for decrypting the signing key to `Sign.sign()`.
- Added support for private key passphrase in every method that accepts it.
- Added support for RSA public/private encryption/decryption functionality.
- Added support for setting and getting of authentication tags and setting additional authentication data when using ciphers such as AES-GCM.

### dgram

https://iojs.org/api/dgram.html

- Added support for receiving empty UDP packets.

### dns

https://iojs.org/api/dns.html

- Added `dns.resolveSoa`, `dns.getServers`, and `dns.setServers` methods.
- Added `hostname` on error messages when available.
- Improved error handling consistency.

### events

https://iojs.org/api/events.html

- Added chaining support to `EventEmitter.setMaxListeners`.
- Updated `require('events')` to return the `EventEmitter` constructor, allowing the module to be used like `var EventEmitter = require('events')` instead of `var EventEmitter = require('events').EventEmitter`.

### fs

https://iojs.org/api/fs.html

- Added `fs.access`, and deprecated `fs.exists`. Please read the documentation carefully.
- Added more informative errors and method call site details when the `NODE_DEBUG` environment is set to ease debugging.
- Added option to `fs.watch` for recursive sub-directory support (OS X only).
- Fixed missing callbacks errors just being printed instead of thrown.

### http

https://iojs.org/api/http.html

- Added support for `response.write` and `response.end` to receive a callback to know when the operation completes.
- Added support for 308 status code (see RFC 7238).
- Added `http.METHODS` array, listing the HTTP methods supported by the parser.
- Added `request.flush` method.
- Added `response.getHeader('header')` method that may be used before headers are flushed.
- Added `response.statusMessage` property.
- Added Client Keep-Alive behavior.  Set `keepAlive:true` in request options to reuse connections indefinitely.
- Added `rawHeaders` and `rawTrailers` members on incoming message.
- Removed default chunked encoding on `DELETE` and `OPTIONS`.

### net

https://iojs.org/api/net.html

- Changed `net.Server.listen` such that, when the bind address is omitted, IPv6 is tried first, and IPv4 is used as a fallback.

### os

https://iojs.org/api/os.html

- Added MAC addresses, netmasks and scope IDs for IPv6 addresses to `os.networkInterfaces` method output.
- Updated `os.tmpdir` on Windows to use the `%SystemRoot%` or `%WINDIR%` environment variables instead of the hard-coded value of `c:\windows` when determining the temporary directory location.

### path

https://iojs.org/api/path.html

- Added `path.isAbsolute` and `path.parse` methods.
- Added `path.win32` and `path.posix` objects that contain platform-specific versions of the various `path` functions.
- Improved `path.join` performance.

### process

https://iojs.org/api/process.html

- Added `beforeExit` event.
- Added `process.mainModule` and `process.exitCode`.

### querystring

https://iojs.org/api/querystring.html

- Added the ability to pass custom versions of `encodeURIComponent` and `decodeURIComponent` when stringifying or parsing a querystring.
- Fixed several issues with the formatting of query strings in edge cases.

### smalloc

https://iojs.org/api/smalloc.html

`smalloc` is a new core module for doing (external) raw memory allocation/deallocation/copying in JavaScript.

### streams

https://iojs.org/api/stream.html

The changes to streams are not as drastic as the transition from streams1 to streams2: they are a
refinement of existing ideas, and should make the API slightly less surprising for humans and faster
for computers. As a whole the changes are referred to as "streams3", but the changes should largely go
unnoticed by the majority of stream consumers and implementers.

#### Readable streams

The distinction between "flowing" and "non-flowing" modes has been refined. Entering "flowing" mode is
no longer an irreversible operation&mdash;it is possible to return to "non-flowing" mode from "flowing" mode.
Additionally, the two modes now flow through the same machinery instead of replacing methods. Any time
data is returned as a result of a `.read` call that data will *also* be emitted on the `"data"` event.

As before, adding a listener for the `"readable"` or `"data"` event will start flowing the stream; as
will piping to another stream.

#### Writable streams

The ability to "bulk write" to underlying resources has been added to `Writable` streams. For stream
implementers, one can signal that a stream is bulk-writable by specifying a [_writev](https://iojs.org/api/stream.html#stream_writable_writev_chunks_callback) method.
Bulk writes will occur in two situations:

1. When a bulk-writable stream is clearing its backlog of buffered write requests,
2. or if an end user has made use of the new `.cork()` and `.uncork()` API methods.

`.cork` and `.uncork` allow the end user to control the buffering behavior of writable streams separate
from exerting backpressure. `.cork` indicates that the stream should accept new writes (up to `highWaterMark`),
while `.uncork` resets that behavior and attempts to bulk-write all buffered writes to the underlying resource.

The only core stream API that **currently** implements `_writev` is `net.Socket`.

In addition to the bulk-write changes, the performance of repeated small writes to non-bulk-writable streams
(such as `fs.WriteStream`) has been drastically improved. Users piping high volume log streams to disk should
see an improvement.

For a detailed overview of how streams3 interact, [see this diagram](https://cloud.githubusercontent.com/assets/37303/5728694/f9a3e300-9b20-11e4-9e14-a6938b3327f0.png).

### timers

https://iojs.org/api/timers.html

- Removed `process.maxTickDepth`, allowing `process.nextTick` to be used recursively without limit.
- Updated `setImmediate` to process the full queue each turn of the event loop, instead of one per queue.

### tls

https://iojs.org/api/tls.html

- Added `detailed` boolean flag to `getPeerCertificate` to return detailed certificate information (with raw DER bytes).
- Added `renegotiate(options, callback)` method for session renegotiation.
- Added `setMaxSendFragment` method for varying TLS fragment size.
- Added a `dhparam` option for DH ciphers.
- Added a `ticketKeys` option for TLS ticket AES encryption keys setup.
- Added async OCSP-stapling callback.
- Added async session storage events.
- Added async SNI callback.
- Added multi-key server support (for example, ECDSA+RSA server).
- Added optional callback to `checkServerIdentity` for manual certificate validation in user-land.
- Added support for ECDSA/ECDHE cipher.
- Implemented TLS streams in C++, boosting their performance.
- Moved `createCredentials` to `tls` and renamed it to `createSecureContext`.
- Removed SSLv2 and SSLv3 support.

### url

https://iojs.org/api/url.html

- Improved escaping of certain characters.
- Improved parsing speed.

### util

https://iojs.org/api/util.html

- Added `util.debuglog`.
- Added a plethora of new type-testing methods. See [the docs](https://iojs.org/api/util.html).
- Updated `util.format` to receive several changes:
  - `-0` is now displayed as such, instead of as `0`.
  - Anything that is `instanceof Error` is now formatted as an error.
  - Circular references in JavaScript objects are now handled for the `%j` specifier.
  - Custom `inspect` functions are now allowed to return an object.
  - Custom `inspect` functions now receive any arguments passed to `util.inspect`.

## v8

https://iojs.org/api/v8.html

`v8` is a new core module for interfacing directly with the V8 engine.

### vm

https://iojs.org/api/vm.html

The `vm` module has been rewritten to work better, based on the excellent [Contextify](https://github.com/brianmcd/contextify) native module. All of the functionality of Contextify is now in core, with improvements!

- Added `vm.isContext(object)` method to determine whether `object` has been contextified.
- Added `vm.runInDebugContext(code)` method to compile and execute `code` inside the V8 debug context.
- Updated `vm.createContext(sandbox)` to "contextify" the sandbox, making it suitable for use as a global for `vm` scripts, and then return it. It no longer creates a separate context object.
- Updated most `vm` and `vm.Script` methods to accept an `options` object, allowing you to configure a timeout for the script, the error display behavior, and sometimes the filename (for stack traces).
- Updated the supplied sandbox object to be used directly as the global, remove error-prone copying of properties back and forth between the supplied sandbox object and the global that appears inside the scripts run by the `vm` module.

For more information, see the `vm` documentation linked above.

### zlib

https://iojs.org/api/zlib.html

- Added support for `zlib.flush` to specify a particular flush method (defaulting to `Z_FULL_FLUSH`).
- Added support for `zlib.params` to dynamically update the compression level and strategy when deflating.
- Added synchronous versions of the zlib methods.

### C++ API Changes

https://iojs.org/api/addons.html

In general it is recommended that you use [NAN](https://github.com/rvagg/nan) as a compatibility layer for your addons. This will also help with future changes in the V8 and Node/io.js C++ API. Most of the following changes are already handled by NAN-specific wrappers.

#### V8 highlights

- Exposed method signature has changed from `Handle<Value> Method(const Arguments& args)` to `void Method(const v8::FunctionCallbackInfo<Value>& args)` with the newly introduced `FunctionCallbackInfo` also taking the return value via `args.GetReturnValue().Set(value)` instead of `scope.Close(value)`, `Arguments` has been removed.
- Exposed setter signature has changed from `void Setter(Local<String> property, Local<Value> value, const v8::AccessorInfo& args)` `void Setter(Local<String> property, Local<Value> value, const v8::PropertyCallbackInfo<void>& args)`.
- Exposed getter signature has changed from `void Getter(Local<String> property, Local<Value> value, const v8::AccessorInfo& args)` `void Setter(Local<String> property, Local<Value> value, const v8::PropertyCallbackInfo<Value>& args)`.
- Exposed property setter signature has changed from `Handle<Value> Setter(Local<String> property, Local<Value> value, const v8::AccessorInfo& args)` `void Setter(Local<String> property, Local<Value> value, const v8::PropertyCallbackInfo<Value>& args)`.
- Exposed property getter signature has changed from `Handle<Value> Getter(Local<String> property, Local<Value> value, const v8::AccessorInfo& args)` `void Setter(Local<String> property, Local<Value> value, const v8::PropertyCallbackInfo<Value>& args)`.
- Similar changes have been made to property enumerators, property deleters, property query, index getter, index setter, index enumerator, index deleter, index query.
- V8 objects instantiated in C++ now require an `Isolate*` argument as the first argument. In most cases it is OK to simply pass `v8::Isolate::GetCurrent()`, e.g. `Date::New(Isolate::GetCurrent(), time)`, or `String::NewFromUtf8(Isolate::GetCurrent(), "foobar")`.
- `HandleScope scope` now requires an `Isolate*` argument, i.e. `HandleScope scope(isolate)`, in most cases `v8::Isolate::GetCurrent()` is OK.
- Similar changes have been made to `Locker` and `Unlocker`.
- V8 objects that need to "escape" a scope should be enclosed in a `EscapableHandleScope` rather than a `HandleScope` and should be returned with `scope.Escape(value)`.
- Exceptions are now thrown from isolates with `isolate->ThrowException(ExceptionObject)`.
- `Context::GetCurrent()` must now be done on an isolate, e.g. `Isolate::GetCurrent()->GetCurrentContext()`.
- `String::NewSymbol()` has been removed, use plain strings instead.
- `String::New()` has been removed, use `String::NewFromUtf8()` instead.
- `Persistent` objects no longer inherit from `Handle` and cannot be instantiated with another object. Instead, the `Persistent` should simply be declared, e.g. `Persistent<Type> handle` and then have a `Local` assigned to it with `handle.Reset(isolate, value)`. To get a `Local` from a `Persistent` you must instantiate it as the argument, i.e. `Local::New(Isolate*, Persistent)`.

#### Node / io.js

- Updated `node::Buffer::New()` to return a `Handle` directly so you no longer need to fetch the `handle_` property.
- Updated `node::MakeCallback()` to require an `Isolate*` as the first argument. Generally `Isolate::GetCurrent()` will be OK for this.


--------------------------------------

**The changelog below was inherited from joyent/node prior to the io.js fork.**


## 2014.09.24, Version 0.11.14 (Unstable)

* uv: Upgrade to v1.0.0-rc1
* http_parser: Upgrade to v2.3.0
* npm: Upgrade to v2.0.0
* openssl: Upgrade to v1.0.1i
* v8: Upgrade to 3.26.33
* Add fast path for simple URL parsing (Gabriel Wicke)
* Added support for options parameter in console.dir() (Xavi Magrinyà)
* Cluster: fix shared handles on Windows (Alexis Campailla)
* buffer: Fix incorrect Buffer.compare behavior (Feross Aboukhadijeh)
* buffer: construct new buffer from buffer toJSON() output (cjihrig)
* buffer: improve Buffer constructor (Kang-Hao Kenny)
* build: linking CoreFoundation framework for OSX (Thorsten Lorenz)
* child_process: accept uid/gid everywhere (Fedor Indutny)
* child_process: add path to spawn ENOENT Error (Ryan Cole)
* child_process: copy spawnSync() cwd option to proper buffer (cjihrig)
* child_process: do not access stderr when stdio set to 'ignore' (cjihrig)
* child_process: don't throw on EAGAIN (Charles)
* child_process: don't throw on EMFILE/ENFILE (Ben Noordhuis)
* child_process: use full path for cmd.exe on Win32 (Ed Morley)
* cluster: allow multiple calls to setupMaster() (Ryan Graham)
* cluster: centralize removal from workers list. (Julien Gilli)
* cluster: enable error/message events using .worker (cjihrig)
* cluster: include settings object in 'setup' event (Ryan Graham)
* cluster: restore v0.10.x setupMaster() behaviour (Ryan Graham)
* cluster: support options in Worker constructor (cjihrig)
* cluster: test events emit on cluster.worker (Sam Roberts)
* console: console.dir() accepts options object (Xavi Magrinyà)
* crypto: add `honorCipherOrder` argument (Fedor Indutny)
* crypto: allow padding in RSA methods (Fedor Indutny)
* crypto: clarify RandomBytes() error msg (Mickael van der Beek)
* crypto: never store pointer to conn in SSL_CTX (Fedor Indutny)
* crypto: unsigned value can't be negative (Brian White)
* dgram: remove new keyword from errnoException (Jackson Tian)
* dns: always set variable family in lookup() (cjihrig)
* dns: include host name in error message if available (Maciej Małecki)
* dns: introduce lookupService function (Saúl Ibarra Corretgé)
* dns: send lookup c-ares errors to callback (Chris Dickinson)
* dns: throw if hostname is not string or falsey (cjihrig)
* events: Output the event that is leaking (Arnout Kazemier)
* fs: close file if fstat() fails in readFile() (cjihrig)
* fs: fs.readFile should not throw uncaughtException (Jackson Tian)
* http: add 308 status_code, see RFC7238 (Yazhong Liu)
* http: don't default OPTIONS to chunked encoding (Nick Muerdter)
* http: fix bailout for writeHead (Alex Kocharin)
* http: remove unused code block (Fedor Indutny)
* http: write() after end() emits an error. (Julien Gilli)
* lib, src: add vm.runInDebugContext() (Ben Noordhuis)
* lib: noisy deprecation of child_process customFds (Ryan Graham)
* module: don't require fs several times (Robert Kowalski)
* net,dgram: workers can listen on exclusive ports (cjihrig)
* net,stream: add isPaused, don't read() when paused (Chris Dickinson)
* net: Ensure consistent binding to IPV6 if address is absent (Raymond Feng)
* net: add remoteFamily for socket (Jackson Tian)
* net: don't emit listening if handle is closed (Eli Skeggs)
* net: don't prefer IPv4 addresses during resolution (cjihrig)
* net: don't throw on net.Server.close() (cjihrig)
* net: reset `errorEmitted` on reconnect (Ed Umansky)
* node: set names for prototype methods (Trevor Norris)
* node: support v8 microtask queue (Vladimir Kurchatkin)
* path: fix slice OOB in trim (Lucio M. Tato)
* path: isAbsolute() should always return boolean (Herman Lee)
* process: throw TypeError if kill pid not a number (Sam Roberts)
* querystring: custom encode and decode (fengmk2)
* querystring: do not add sep for empty array (cjihrig)
* querystring: remove prepended ? from query field (Ezequiel Rabinovich)
* readline: fix close event of readline.Interface() (Yazhong Liu)
* readline: fixes scoping bug (Dan Kaplun)
* readline: implements keypress buffering (Dan Kaplun)
* repl: fix multi-line input (Fedor Indutny)
* repl: fix overwrite for this._prompt (Yazhong Liu)
* repl: proper `setPrompt()` and `multiline` support (Fedor Indutny)
* stream: don't try to finish if buffer is not empty (Vladimir Kurchatkin)
* stream: only end reading on null, not undefined (Jonathan Reem)
* streams: set default hwm properly for Duplex (Andrew Oppenlander)
* string_bytes: ucs2 support big endian (Andrew Low)
* tls, crypto: add DHE support (Shigeki Ohtsu)
* tls: `checkServerIdentity` option (Trevor Livingston)
* tls: add DHE-RSA-AES128-SHA256 to the def ciphers (Shigeki Ohtsu)
* tls: better error reporting at cert validation (Fedor Indutny)
* tls: support multiple keys/certs (Fedor Indutny)
* tls: throw an error, not string (Jackson Tian)
* udp: make it possible to receive empty udp packets (Andrius Bentkus)
* url: treat \ the same as / (isaacs)

## 2014.05.01, Version 0.11.13 (Unstable)

https://github.com/iojs/io.js/commit/99c9930ad626e2796af23def7cac19b65c608d18

* v8: upgrade to 3.24.35.22
* buffer: add compare and equals methods (Sean McArthur)
* buffer: improve {read,write}{U}Int* methods (Nick Apperson)
* buffer: return uint if MSB is 1 in readUInt32 (goussardg)
* buffer: truncate buffer after string decode (Fedor Indutny)
* child_process: fix assertion error in spawnSync (Shigeki Ohtsu)
* crypto: fix memory leak in CipherBase::Final (Fedor Indutny)
* crypto: improve error messages (Ingmar Runge)
* crypto: move `createCredentials` to tls (Fedor Indutny)
* crypto: work around OpenSSL oddness (Fedor Indutny)
* dgram: introduce `reuseAddr` option (Fedor Indutny)
* domain: don't crash on "throw null" (Alex Kocharin)
* events: check if _events is an own property (Vladimir Kurchatkin)
* fs: improve performance of all stat functions (James Pickard)
* fs: return blksize on stats object (Trevor Norris)
* http: add request.flush() method (Ben Noordhuis)
* http: better client "protocol not supported" error (Nathan Rajlich)
* http: use defaultAgent.protocol in protocol check (Nathan Rajlich)
* main: Handle SIGINT properly. (Geir Hauge)
* net: bind to `::` TCP address by default (Fedor Indutny)
* readline: consider newlines for cursor position (Yazhong Liu)
* stream: split `objectMode` for Duplex (Vladimir Kurchatkin)
* tls: `getPeerCertificate(detailed)` (Fedor Indutny)
* tls: do not call SNICallback unless present (Fedor Indutny)
* tls: force readable/writable to `true` (Fedor Indutny)
* tls: support OCSP on client and server (Fedor Indutny)
* util: made util.isArray a direct alias for Array.isArray (Evan Carroll)

## 2014.03.11, Version 0.11.12 (Unstable)

https://github.com/iojs/io.js/commit/7d6b8db40f32e817ff145b7cfe6b3aec3179fba7

* uv: Upgrade to v0.11.22 (Timothy J Fontaine)
* buffer: allow toString to accept Infinity for end (Brian White)
* child_process: add spawnSync/execSync (Bert Belder, Timothy J Fontaine)
* cluster: handle bind errors on Windows (Alexis Campailla)
* contextify: handle infinite recursion errors (Fedor Indutny)
* crypto: allow custom generator for DiffieHellman (Brian White)
* crypto: allow setting add'l authenticated data (Brian White)
* crypto: fix CipherFinal return value check (Brian White)
* crypto: make NewSessionDoneCb public (Fedor Indutny)
* dgram: pass the bytes sent to the send callback (Timothy J Fontaine)
* dns: validate arguments in resolver (Kenan Sulayman)
* dns: verify argument is valid function in resolve (Kenan Sulayman)
* http: avoid duplicate keys in writeHead (David Björklund)
* net: add localPort to connect options (Timothy J Fontaine)
* node: do not print SyntaxError hints to stderr (Fedor Indutny)
* node: invoke `beforeExit` again if loop was active (Fedor Indutny)
* node: make AsyncListenerInst field more explicit (Trevor Norris)
* os: networkInterfaces include scopeid for ipv6 (Xidorn Quan)
* process: allow changing `exitCode` in `on('exit')` (Fedor Indutny)
* readline: fix `line` event, if input emit 'end' (Yazhong Liu)
* src: add tracing.v8.on('gc') statistics hooks (Ben Noordhuis)
* src: add v8.getHeapStatistics() function (Ben Noordhuis)
* src: emit 'beforeExit' event on process object (Ben Noordhuis)
* src: move AsyncListener from process to tracing (Trevor Norris)
* tls: fix crash in SNICallback (Fedor Indutny)
* tls: introduce asynchronous `newSession` (Fedor Indutny)
* util: show meaningful values for boxed primitives (Nathan Rajlich)
* vm: don't copy Proxy object from parent context (Ben Noordhuis)
* windows: make stdout/sterr pipes blocking (Alexis Campailla)
* zlib: add sync versions for convenience methods (Nikolai Vavilov)

## 2014.01.29, Version 0.11.11 (Unstable)

https://github.com/iojs/io.js/commit/b46e77421581ea358e221a8a843d057c747f7e90

* v8: Upgrade to 3.22.24.19
* http_parser: Upgrade to 2.2.1
* openssl: Upgrade to 1.0.1f
* uv: Upgrade to 0.11.18
* async-listener: revamp of subsystem (Trevor Norris)
* node: do not ever close stdio (Fedor Indutny)
* http: use writev on chunked encoding (Trevor Norris)
* async_wrap/timers: remove Add/RemoveAsyncListener (Trevor Norris)
* child_process: better error reporting for exec (Fedor Indutny)
* crypto: add newline to cert and key if not present (Fedor Indutny)
* crypto: clear error in GetPeerCertificate (Fedor Indutny)
* crypto: honor default ciphers in client mode (Jacob Hoffman-Andrews)
* crypto: introduce .setEngine(engine, [flags]) (Fedor Indutny)
* crypto: support custom pbkdf2 digest methods (Ben Noordhuis)
* domain: fix off-by-one in Domain.exit() (Ryan Graham)
* http: concatenate duplicate headers by default (Alex Kocharin)
* http: do not emit EOF non-readable socket (Fedor Indutny)
* node: fix argument parsing with -p arg (Alexis Campailla)
* path: improve POSIX path.join() performance (Jo Liss)
* tls: emit `clientError` on early socket close (Fedor Indutny)
* tls: introduce `.setMaxSendFragment(size)` (Fedor Indutny)
* tls: make cert/pfx optional in tls.createServer() (Ben Noordhuis)
* tls: process accumulated input (Fedor Indutny)
* tls: show human-readable error messages (Ben Noordhuis)
* util: handle escaped forward slashes correctly (Tom Gallacher)

## 2013.12.31, Version 0.11.10 (Unstable)

https://github.com/iojs/io.js/commit/66931791f06207d1cdfea5ec1529edf3c94026d3

* http_parser: update to 2.2
* uv: Upgrade to v0.11.17
* v8: Upgrade to 3.22.24.10
* buffer: optimize writeInt* methods (Paul Loyd)
* child_process: better error handling (Alexis Campailla)
* cluster: do not synchronously emit 'setup' event (Sam Roberts)
* cluster: restore backwards compatibility and various fixes (Sam Roberts)
* crypto: remove unnecessary OpenSSL_add_all_digests (Yorkie)
* crypto: support GCM authenticated encryption mode. (Ingmar Runge)
* dns: add resolveSoa and 'SOA' rrtype (Tuğrul Topuz)
* events: move EE c'tor guts to EventEmitter.init (Bert Belder)
* http: DELETE shouldn't default to chunked encoding (Lalit Kapoor)
* http: parse the status message in a http response. (Cam Swords)
* node: fix removing AsyncListener in callback (Vladimir Kurchatkin)
* node: follow specification, zero-fill ArrayBuffers (Trevor Norris)
* openssl: use ASM optimized routines (Fedor Indutny)
* process: allow nextTick infinite recursion (Trevor Norris)
* querystring: remove `name` from `stringify()` (Yorkie)
* timers: setImmediate v8 optimization fix (pflannery)
* tls: add serialNumber to getPeerCertificate() (Ben Noordhuis)
* tls: reintroduce socket.encrypted (Fedor Indutny)
* tls: fix handling of asterisk in SNI context (Fedor Indutny)
* util: Format negative zero as '-0' (David Chan)
* vm: fix race condition in timeout (Alexis Campailla)
* windows: fix dns lookup of localhost with ipv6 (Alexis Campailla)

## 2013.11.20, Version 0.11.9 (Unstable)

https://github.com/iojs/io.js/commit/dcfd032bdd69dfb38c120e18438d6316ae522edc

* uv: upgrade to v0.11.15 (Timothy J Fontaine)
* v8: upgrade to 3.22.24.5 (Timothy J Fontaine)
* buffer: remove warning when no encoding is passed (Trevor Norris)
* build: make v8 use random seed for hash tables (Ben Noordhuis)
* crypto: build with shared openssl without NPN (Ben Noordhuis)
* crypto: update root certificates (Ben Noordhuis)
* debugger: pass on v8 debug switches (Ben Noordhuis)
* domain: use AsyncListener API (Trevor Norris)
* fs: add recursive subdirectory support to fs.watch (Nick Simmons)
* fs: make fs.watch() non-recursive by default (Ben Noordhuis)
* http: cleanup freeSockets when socket destroyed (fengmk2)
* http: force socket encoding to be null (isaacs)
* http: make DELETE requests set `req.method` (Nathan Rajlich)
* node: add AsyncListener support (Trevor Norris)
* src: remove global HandleScope that hid memory leaks (Ben Noordhuis)
* tls: add ECDH ciphers support (Erik Dubbelboer)
* tls: do not default to 'localhost' servername (Fedor Indutny)
* tls: more accurate wrapping of connecting socket (Fedor Indutny)

## 2013.10.30, Version 0.11.8 (Unstable)

https://github.com/iojs/io.js/commit/f8d86e24f3463c36f7f3f4c3b3ec779e5b6201e1

* uv: Upgrade to v0.11.14
* v8: upgrade 3.21.18.3
* assert: indicate if exception message is generated (Glen Mailer)
* buffer: add buf.toArrayBuffer() API (Trevor Norris)
* cluster: fix premature 'disconnect' event (Ben Noordhuis)
* crypto: add SPKAC support (Jason Gerfen)
* debugger: count space for line numbers correctly (Alex Kocharin)
* debugger: make busy loops SIGUSR1-interruptible (Ben Noordhuis)
* debugger: repeat last command (Alex Kocharin)
* debugger: show current line, fix for [#6150](https://github.com/joyent/node/issues/6150) (Alex Kocharin)
* dgram: send() can accept strings (Trevor Norris)
* dns: rename domain to hostname (Ben Noordhuis)
* dns: set hostname property on error object (Ben Noordhuis)
* dtrace, mdb_v8: support more string, frame types (Dave Pacheco)
* http: add statusMessage (Patrik Stutz)
* http: expose supported methods (Ben Noordhuis)
* http: provide backpressure for pipeline flood (isaacs)
* process: Add exitCode property (isaacs)
* tls: socket.renegotiate(options, callback) (Fedor Indutny)
* util: format as Error if instanceof Error (Rod Vagg)

## 2013.08.21, Version 0.11.7 (Unstable)

https://github.com/iojs/io.js/commit/be52549bfa5311208b5fcdb3ba09210460fa9ceb

* uv: upgrade to v0.11.13
* v8: upgrade to 3.20.17
* buffer: adhere to INSPECT_MAX_BYTES (Timothy J Fontaine)
* buffer: fix regression for large buffer creation (Trevor Norris)
* buffer: don't throw if slice length too long (Trevor Norris)
* buffer: Buffer(buf) constructor copies into the proper buffer (Ben Noordhuis)
* cli: remove --max-stack-size (Ben Noordhuis)
* cli: unknown command line options are errors (Ben Noordhuis)
* child_process: exec accept buffer as an encoding (Seth Fitzsimmons)
* crypto: make randomBytes/pbkdf2 callbacks domain aware (Ben Noordhuis)
* domain: deprecate domain.dispose(). (Forrest L Norvell)
* fs: Expose birthtime on stat objects (isaacs)
* http: Only send connection:keep-alive if necessary (isaacs)
* repl: Catch syntax errors better (isaacs, Nathan Rajlich)
* stream: change default highWaterMark for objectMode to 16 (Mathias Buus)
* stream: make setEncoding/pause/resume chainable (Julian Gruber, isaacs)
* util: pass opts to custom inspect functions (Timothy J Fontaine)
* vm: rewritten to behave like Contextify (Domenic Denicola)

## 2013.08.21, Version 0.11.6 (Unstable)

https://github.com/iojs/io.js/commit/04018d4b3938fd30ba14822e79195e4af2be36f6

* uv: Upgrade to v0.11.8
* v8: upgrade v8 to 3.20.14.1
* build: disable SSLv2 by default (Ben Noordhuis)
* build: don't auto-destroy existing configuration (Ben Noordhuis)
* crypto: add TLS 1.1 and 1.2 to secureProtocol list (Matthias Bartelmeß)
* crypto: fix memory leak in randomBytes() error path (Ben Noordhuis)
* dgram: don't call into js when send cb is omitted (Ben Noordhuis)
* dgram: fix regression in string argument handling (Ben Noordhuis)
* domains: performance improvements (Trevor Norris)
* events: EventEmitter = require('events') (Jake Verbaten)
* http: Add write()/end() callbacks (isaacs)
* http: Consistent 'finish' event semantics (isaacs)
* http: Prefer 'binary' over 'ascii' (isaacs)
* http: Support legacy agent.addRequest API (isaacs)
* http: Write hex/base64 chunks properly (isaacs)
* http: add agent.maxFreeSockets option (isaacs)
* http: provide access to raw headers/trailers (isaacs)
* http: removed headers stay removed (James Halliday)
* http,timers: improve callback performance (Ben Noordhuis)
* net: family option in net.connect (Vsevolod Strukchinsky)
* readline: pause stdin before turning off terminal raw mode (Daniel Chatfield)
* smalloc: allow different external array types (Trevor Norris)
* smalloc: expose ExternalArraySize (Trevor Norris)
* stream: Short-circuit buffer pushes when flowing (isaacs)
* tls: handle errors on socket before releasing it (Fedor Indutny)
* util: fix isPrimitive check (Trevor Norris)
* util: isObject should always return boolean (Trevor Norris)

## 2013.08.06, Version 0.11.5 (Unstable)

https://github.com/iojs/io.js/commit/6f92da2dd106b0c63fde563284f83e08e2a521b5

* v8: upgrade to 3.20.11
* uv: upgrade to v0.11.7
* buffer: return offset for end of last write (Trevor Norris)
* build: embed the mdb_v8.so into the binary (Timothy J Fontaine)
* build: fix --without-ssl build (Ben Noordhuis)
* child_process: add 'shell' option to .exec() (Ben Noordhuis)
* dgram: report send errors to cb, don't pass bytes (Ben Noordhuis)
* fs: write strings directly to disk (Trevor Norris)
* https: fix default port (Koichi Kobayashi)
* openssl: use asm for sha, md5, rmd (Fedor Indutny)
* os: add mac address to networkInterfaces() output (Brian White)
* smalloc: introduce smalloc module (Trevor Norris)
* stream: Simplify flowing, passive data listening (streams3) (isaacs)
* tls: asynchronous SNICallback (Fedor Indutny)
* tls: share tls tickets key between cluster workers (Fedor Indutny)
* util: don't throw on circular %j input to format() (Ben Noordhuis)

## 2013.07.12, Version 0.11.4 (Unstable)

https://github.com/iojs/io.js/commit/b5b84197ed037918fd1a26e5cb87cce7c812ca55

* npm: Upgrade to 1.3.4
* v8: Upgrade to v3.20.2
* c-ares: Upgrade to piscisaureus/cares@805d153
* timers: setImmediate process full queue each turn (Ben Noordhuis)
* http: Add agent.get/request methods (isaacs)
* http: Proper KeepAlive behavior (isaacs)
* configure: fix the --without-ssl option (Nathan Rajlich)
* buffer: propagate originating parent (Trevor Norris)
* tls_wrap: return Error not throw for missing cert (Timothy J Fontaine)
* src: enable native v8 typed arrays (Ben Noordhuis)
* stream: objectMode transform should allow falsey values (Jeff Barczewski)
* slab_allocator: remove SlabAllocator (Trevor Norris)
* crypto: fix memory leak in LoadPKCS12 (Fedor Indutny)
* tls: export TLSSocket (Fedor Indutny)
* zlib: allow changing of level and strategy (Brian White)
* zlib: allow custom flush type for flush() (Brian White)

## 2013.06.26, Version 0.11.3 (Unstable)

https://github.com/iojs/io.js/commit/38c0c47bbe280ddc42054418091571e532d82a1e

* uv: Upgrade to v0.11.5
* c-ares: upgrade to 1.10.0
* v8: upgrade to v3.19.13
* punycode: update to v1.2.3 (Mathias Bynens)
* debugger: break on uncaught exception (Miroslav Bajtos)
* child_process: emit 'disconnect' asynchronously (Ben Noordhuis)
* dtrace: enable uv's probes if enabled (Timothy J Fontaine)
* dtrace: unify dtrace and systemtap interfaces (Timothy J Fontaine)
* buffer: New API for backing data store (Trevor Norris)
* buffer: return `this` in fill() for chainability (Brian White)
* build: fix include order for building on windows (Timothy J Fontaine)
* build: add android support (Linus Mårtensson)
* readline: strip ctrl chars for prompt width calc (Krzysztof Chrapka)
* tls: introduce TLSSocket based on tls_wrap binding (Fedor Indutny)
* tls: add localAddress and localPort properties (Ben Noordhuis)
* crypto: free excessive memory in NodeBIO (Fedor Indutny)
* process: remove maxTickDepth (Trevor Norris)
* timers: use uv_now instead of Date.now (Timothy J Fontaine)
* util: Add debuglog, deprecate console lookalikes (isaacs)
* module: use path.sep instead of a custom solution (Robert Kowalski)
* http: don't escape request path, reject bad chars (Ben Noordhuis)
* net: emit dns 'lookup' event before connect (Ben Noordhuis)
* dns: add getServers and setServers (Timothy J Fontaine)

## 2013.05.13, Version 0.11.2 (Unstable)

https://github.com/iojs/io.js/commit/5d3dc0e4c3369dfb00b7b13e08936c2e652fa696

* uv: Upgrade to 0.11.2
* V8: Upgrade to 3.19.0
* npm: Upgrade to 1.2.21
* build: Makefile should respect configure --prefix (Timothy J Fontaine)
* cluster: use round-robin load balancing (Ben Noordhuis)
* debugger, cluster: each worker has new debug port (Miroslav Bajtoš)
* debugger: `restart` with custom debug port (Miroslav Bajtoš)
* debugger: breakpoints in scripts not loaded yet (Miroslav Bajtoš)
* event: EventEmitter#setMaxListeners() returns this (Sam Roberts)
* events: add EventEmitter.defaultMaxListeners (Ben Noordhuis)
* install: Support $(PREFIX) install target directory prefix (Olof Johansson)
* os: Include netmask in os.networkInterfaces() (Ben Kelly)
* path: add path.isAbsolute(path) (Ryan Doenges)
* stream: Guarantee ordering of 'finish' event (isaacs)
* streams: introduce .cork/.uncork/._writev (Fedor Indutny)
* vm: add support for timeout argument (Andrew Paprocki)

## 2013.04.19, Version 0.11.1 (Unstable)

https://github.com/iojs/io.js/commit/4babd2b46ebf9fbea2c9946af5cfae25a33b2b22

* V8: upgrade to 3.18.0
* uv: Upgrade to v0.11.1
* http: split into multiple separate modules (Timothy J Fontaine)
* http: escape unsafe characters in request path (Ben Noordhuis)
* url: Escape all unwise characters (isaacs)
* build: depend on v8 postmortem-metadata if enabled (Paddy Byers)
* etw: update prototypes to match dtrace provider (Timothy J Fontaine)
* buffer: change output of Buffer.prototype.toJSON() (David Braun)
* dtrace: actually use the _handle.fd value (Timothy J Fontaine)
* dtrace: pass more arguments to probes (Dave Pacheco)
* build: allow building with dtrace on osx (Dave Pacheco)
* zlib: allow passing options to convenience methods (Kyle Robinson Young)

## 2013.03.28, Version 0.11.0 (Unstable)

https://github.com/iojs/io.js/commit/bce38b3d74e64fcb7d04a2dd551151da6168cdc5

* V8: update to 3.17.13
* os: use %SystemRoot% or %windir% in os.tmpdir() (Suwon Chae)
* util: fix util.inspect() line width calculation (Marcin Kostrzewa)
* buffer: remove _charsWritten (Trevor Norris)
* fs: uv_[fl]stat now reports subsecond resolution (Timothy J Fontaine)
* fs: Throw if error raised and missing callback (bnoordhuis)
* tls: expose SSL_CTX_set_timeout via tls.createServer (Manav Rathi)
* tls: remove harmful unnecessary bounds checking (Marcel Laverdet)
* buffer: write ascii strings using WriteOneByte (Trevor Norris)
* dtrace: fix generation of v8 constants on freebsd (Fedor Indutny)
* dtrace: x64 ustack helper (Fedor Indutny)
* readline: handle wide characters properly (Nao Iizuka)
* repl: Use a domain to catch async errors safely (isaacs)
* repl: emit 'reset' event when context is reset (Sami Samhuri)
* util: custom `inspect()` method may return an Object (Nathan Rajlich)
* console: `console.dir()` bypasses inspect() methods (Nathan Rajlich)

## 2014.12.22, Version 0.10.35 (Stable)

* tls: re-add 1024-bit SSL certs removed by f9456a2 (Chris Dickinson)
* timers: don't close interval timers when unrefd (Julien Gilli)
* timers: don't mutate unref list while iterating it (Julien Gilli)

## 2014.12.17, Version 0.10.34 (Stable)

https://github.com/iojs/io.js/commit/52795f8fcc2de77cf997e671ea58614e5e425dfe

* uv: update to v0.10.30
* zlib: upgrade to v1.2.8
* child_process: check execFile args is an array (Sam Roberts)
* child_process: check fork args is an array (Sam Roberts)
* crypto: update root certificates (Ben Noordhuis)
* domains: fix issues with abort on uncaught (Julien Gilli)
* timers: Avoid linear scan in _unrefActive. (Julien Gilli)
* timers: fix unref() memory leak (Trevor Norris)
* v8: add api for aborting on uncaught exception (Julien Gilli)
* debugger: fix when using "use strict" (Julien Gilli)

## 2014.10.20, Version 0.10.33 (Stable)

https://github.com/iojs/io.js/commit/8d045a30e95602b443eb259a5021d33feb4df079

* openssl: Update to 1.0.1j (Addressing multiple CVEs)
* uv: Update to v0.10.29
* child_process: properly support optional args (cjihrig)
* crypto: Disable autonegotiation for SSLv2/3 by default (Fedor Indutny,
	Timothy J Fontaine, Alexis Campailla)
	This is a behavior change, by default we will not allow the negotiation to
	SSLv2 or SSLv3. If you want this behavior, run Node.js with either
	`--enable-ssl2` or `--enable-ssl3` respectively.
	This does not change the behavior for users specifically requesting
	`SSLv2_method` or `SSLv3_method`. While this behavior is not advised, it is
	assumed you know what you're doing since you're specifically asking to use
	these methods.

## 2014.09.16, Version 0.10.32 (Stable)

https://github.com/iojs/io.js/commit/0fe0d121551593c23a565db8397f85f17bb0f00e

* npm: Update to 1.4.28
* v8: fix a crash introduced by previous release (Fedor Indutny)
* configure: add --openssl-no-asm flag (Fedor Indutny)
* crypto: use domains for any callback-taking method (Chris Dickinson)
* http: do not send `0\r\n\r\n` in TE HEAD responses (Fedor Indutny)
* querystring: fix unescape override (Tristan Berger)
* url: Add support for RFC 3490 separators (Mathias Bynens)

## 2014.08.19, Version 0.10.31 (Stable)

https://github.com/iojs/io.js/commit/7fabdc23d843cb705d2d0739e7bbdaaf50aa3292

* v8: backport CVE-2013-6668
* openssl: Update to v1.0.1i
* npm: Update to v1.4.23
* cluster: disconnect should not be synchronous (Sam Roberts)
* fs: fix fs.readFileSync fd leak when get RangeError (Jackson Tian)
* stream: fix Readable.wrap objectMode falsy values (James Halliday)
* timers: fix timers with non-integer delay hanging. (Julien Gilli)

## 2014.07.31, Version 0.10.30 (Stable)

https://github.com/iojs/io.js/commit/bc0ff830aff1e016163d855e86ded5c98b0899e8

* uv: Upgrade to v0.10.28
* npm: Upgrade to v1.4.21
* v8: Interrupts must not mask stack overflow.
* Revert "stream: start old-mode read in a next tick" (Fedor Indutny)
* buffer: fix sign overflow in `readUIn32BE` (Fedor Indutny)
* buffer: improve {read,write}{U}Int* methods (Nick Apperson)
* child_process: handle writeUtf8String error (Fedor Indutny)
* deps: backport 4ed5fde4f from v8 upstream (Fedor Indutny)
* deps: cherry-pick eca441b2 from OpenSSL (Fedor Indutny)
* lib: remove and restructure calls to isNaN() (cjihrig)
* module: eliminate double `getenv()` (Maciej Małecki)
* stream2: flush extant data on read of ended stream (Chris Dickinson)
* streams: remove unused require('assert') (Rod Vagg)
* timers: backport f8193ab (Julien Gilli)
* util.h: interface compatibility (Oguz Bastemur)
* zlib: do not crash on write after close (Fedor Indutny)

## 2014.06.05, Version 0.10.29 (Stable)

https://github.com/iojs/io.js/commit/ce82d6b8474bde7ac7df6d425fb88fb1bcba35bc

* openssl: to 1.0.1h (CVE-2014-0224)
* npm: upgrade to 1.4.14
* utf8: Prevent Node from sending invalid UTF-8 (Felix Geisendörfer)
  - *NOTE* this introduces a breaking change, previously you could construct
    invalid UTF-8 and invoke an error in a client that was expecting valid
    UTF-8, now unmatched surrogate pairs are replaced with the unknown UTF-8
    character. To restore the old functionality simply have NODE_INVALID_UTF8
    environment variable set.

* child_process: do not set args before throwing (Greg Sabia Tucker)
* child_process: spawn() does not throw TypeError (Greg Sabia Tucker)
* constants: export O_NONBLOCK (Fedor Indutny)
* crypto: improve memory usage (Alexis Campailla)
* fs: close file if fstat() fails in readFile() (cjihrig)
* lib: name EventEmitter prototype methods (Ben Noordhuis)
* tls: fix performance issue (Alexis Campailla)

## 2014.05.01, Version 0.10.28 (Stable)

https://github.com/iojs/io.js/commit/b148cbe09d4657766fdb61575ba985734c2ff0a8

* npm: upgrade to v1.4.9

## 2014.05.01, Version 0.10.27 (Stable)

https://github.com/iojs/io.js/commit/cb7911f78ae96ef7a540df992cc1359ba9636e86

* npm: upgrade to v1.4.8
* openssl: upgrade to 1.0.1g
* uv: update to v0.10.27
* dns: fix certain txt entries (Fedor Indutny)
* assert: Ensure reflexivity of deepEqual (Mike Pennisi)
* child_process: fix deadlock when sending handles (Fedor Indutny)
* child_process: fix sending handle twice (Fedor Indutny)
* crypto: do not lowercase cipher/hash names (Fedor Indutny)
* dtrace: workaround linker bug on FreeBSD (Fedor Indutny)
* http: do not emit EOF non-readable socket (Fedor Indutny)
* http: invoke createConnection when no agent (Nathan Rajlich)
* stream: remove useless check (Brian White)
* timer: don't reschedule timer bucket in a domain (Greg Brail)
* url: treat \ the same as / (isaacs)
* util: format as Error if instanceof Error (Rod Vagg)

## 2014.02.18, Version 0.10.26 (Stable)

https://github.com/iojs/io.js/commit/cc56c62ed879ad4f93b1fdab3235c43e60f48b7e

* uv: Upgrade to v0.10.25 (Timothy J Fontaine)
* npm: upgrade to 1.4.3 (isaacs)
* v8: support compiling with VS2013 (Fedor Indutny)
* cares: backport TXT parsing fix (Fedor Indutny)
* crypto: throw on SignFinal failure (Fedor Indutny)
* crypto: update root certificates (Ben Noordhuis)
* debugger: Fix breakpoint not showing after restart (Farid Neshat)
* fs: make unwatchFile() insensitive to path (iamdoron)
* net: do not re-emit stream errors (Fedor Indutny)
* net: make Socket destroy() re-entrance safe (Jun Ma)
* net: reset `endEmitted` on reconnect (Fedor Indutny)
* node: do not close stdio implicitly (Fedor Indutny)
* zlib: avoid assertion in close (Fedor Indutny)

## 2014.01.23, Version 0.10.25 (Stable)

https://github.com/iojs/io.js/commit/b0e5f195dfce3e2b99f5091373d49f6616682596

* uv: Upgrade to v0.10.23
* npm: Upgrade to v1.3.24
* v8: Fix enumeration for objects with lots of properties
* child_process: fix spawn() optional arguments (Sam Roberts)
* cluster: report more errors to workers (Fedor Indutny)
* domains: exit() only affects active domains (Ryan Graham)
* src: OnFatalError handler must abort() (Timothy J Fontaine)
* stream: writes may return false but forget to emit drain (Yang Tianyang)

## 2013.12.18, Version 0.10.24 (Stable)

https://github.com/iojs/io.js/commit/b7fd6bc899ccb629d790c47aee06aba87e535c41

* uv: Upgrade to v0.10.21
* npm: upgrade to 1.3.21
* v8: backport fix for CVE-2013-{6639|6640}
* build: unix install node and dep library headers (Timothy J Fontaine)
* cluster, v8: fix --logfile=%p.log (Ben Noordhuis)
* module: only cache package main (Wyatt Preul)

## 2013.12.12, Version 0.10.23 (Stable)

https://github.com/iojs/io.js/commit/0462bc23564e7e950a70ae4577a840b04db6c7c6

* uv: Upgrade to v0.10.20 (Timothy J Fontaine)
* npm: Upgrade to 1.3.17 (isaacs)
* gyp: update to 78b26f7 (Timothy J Fontaine)
* build: include postmortem symbols on linux (Timothy J Fontaine)
* crypto: Make Decipher._flush() emit errors. (Kai Groner)
* dgram: fix abort when getting `fd` of closed dgram (Fedor Indutny)
* events: do not accept NaN in setMaxListeners (Fedor Indutny)
* events: avoid calling `once` functions twice (Tim Wood)
* events: fix TypeError in removeAllListeners (Jeremy Martin)
* fs: report correct path when EEXIST (Fedor Indutny)
* process: enforce allowed signals for kill (Sam Roberts)
* tls: emit 'end' on .receivedShutdown (Fedor Indutny)
* tls: fix potential data corruption (Fedor Indutny)
* tls: handle `ssl.start()` errors appropriately (Fedor Indutny)
* tls: reset NPN callbacks after SNI (Fedor Indutny)

## 2013.11.12, Version 0.10.22 (Stable)

https://github.com/iojs/io.js/commit/cbff8f091c22fb1df6b238c7a1b9145db950fa65

* npm: Upgrade to 1.3.14
* uv: Upgrade to v0.10.19
* child_process: don't assert on stale file descriptor events (Fedor Indutny)
* darwin: Fix "Not Responding" in Mavericks activity monitor (Fedor Indutny)
* debugger: Fix bug in sb() with unnamed script (Maxim Bogushevich)
* repl: do not insert duplicates into completions (Maciej Małecki)
* src: Fix memory leak on closed handles (Timothy J Fontaine)
* tls: prevent stalls by using read(0) (Fedor Indutny)
* v8: use correct timezone information on Solaris (Maciej Małecki)

## 2013.10.18, Version 0.10.21 (Stable)

https://github.com/iojs/io.js/commit/e2da042844a830fafb8031f6c477eb4f96195210

* uv: Upgrade to v0.10.18
* crypto: clear errors from verify failure (Timothy J Fontaine)
* dtrace: interpret two byte strings (Dave Pacheco)
* fs: fix fs.truncate() file content zeroing bug (Ben Noordhuis)
* http: provide backpressure for pipeline flood (isaacs)
* tls: fix premature connection termination (Ben Noordhuis)

## 2013.09.30, Version 0.10.20 (Stable)

https://github.com/iojs/io.js/commit/d7234c8d50a1af73f60d2d3c0cc7eed17429a481

* tls: fix sporadic hang and partial reads (Fedor Indutny)
  - fixes "npm ERR! cb() never called!"

## 2013.09.24, Version 0.10.19 (Stable)

https://github.com/iojs/io.js/commit/6b5e6a5a3ec8d994c9aab3b800b9edbf1b287904

* uv: Upgrade to v0.10.17
* npm: upgrade to 1.3.11
* readline: handle input starting with control chars (Eric Schrock)
* configure: add mips-float-abi (soft, hard) option (Andrei Sedoi)
* stream: objectMode transforms allow falsey values (isaacs)
* tls: prevent duplicate values returned from read (Nathan Rajlich)
* tls: NPN protocols are now local to connections (Fedor Indutny)

## 2013.09.04, Version 0.10.18 (Stable)

https://github.com/iojs/io.js/commit/67a1f0c52e0708e2596f3f2134b8386d6112561e

* uv: Upgrade to v0.10.15
* stream: Don't crash on unset _events property (isaacs)
* stream: Pass 'buffer' encoding with decoded writable chunks (isaacs)

## 2013.08.21, Version 0.10.17 (Stable)

https://github.com/iojs/io.js/commit/469a4a5091a677df62be319675056b869c31b35c

* uv: Upgrade v0.10.14
* http_parser: Do not accept PUN/GEM methods as PUT/GET (Chris Dickinson)
* tls: fix assertion when ssl is destroyed at read (Fedor Indutny)
* stream: Throw on 'error' if listeners removed (isaacs)
* dgram: fix assertion on bad send() arguments (Ben Noordhuis)
* readline: pause stdin before turning off terminal raw mode (Daniel Chatfield)

## 2013.08.16, Version 0.10.16 (Stable)

https://github.com/iojs/io.js/commit/50b4c905a4425430ae54db4906f88982309e128d

* v8: back-port fix for CVE-2013-2882
* npm: Upgrade to 1.3.8
* crypto: fix assert() on malformed hex input (Ben Noordhuis)
* crypto: fix memory leak in randomBytes() error path (Ben Noordhuis)
* events: fix memory leak, don't leak event names (Ben Noordhuis)
* http: Handle hex/base64 encodings properly (isaacs)
* http: improve chunked res.write(buf) performance (Ben Noordhuis)
* stream: Fix double pipe error emit (Eran Hammer)

## 2013.07.25, Version 0.10.15 (Stable)

https://github.com/iojs/io.js/commit/2426d65af860bda7be9f0832a99601cc43c6cf63

* src: fix process.getuid() return value (Ben Noordhuis)

## 2013.07.25, Version 0.10.14 (Stable)

https://github.com/iojs/io.js/commit/fdf57f811f9683a4ec49a74dc7226517e32e6c9d

* uv: Upgrade to v0.10.13
* npm: Upgrade to v1.3.5
* os: Don't report negative times in cpu info (Ben Noordhuis)
* fs: Handle large UID and GID (Ben Noordhuis)
* url: Fix edge-case when protocol is non-lowercase (Shuan Wang)
* doc: Streams API Doc Rewrite (isaacs)
* node: call MakeDomainCallback in all domain cases (Trevor Norris)
* crypto: fix memory leak in LoadPKCS12 (Fedor Indutny)

## 2013.07.09, Version 0.10.13 (Stable)

https://github.com/iojs/io.js/commit/e32660a984427d46af6a144983cf7b8045b7299c

* uv: Upgrade to v0.10.12
* npm: Upgrade to 1.3.2
* windows: get proper errno (Ben Noordhuis)
* tls: only wait for finish if we haven't seen it (Timothy J Fontaine)
* http: Dump response when request is aborted (isaacs)
* http: use an unref'd timer to fix delay in exit (Peter Rust)
* zlib: level can be negative (Brian White)
* zlib: allow zero values for level and strategy (Brian White)
* buffer: add comment explaining buffer alignment (Ben Noordhuis)
* string_bytes: properly detect 64bit (Timothy J Fontaine)
* src: fix memory leak in UsingDomains() (Ben Noordhuis)

## 2013.06.18, Version 0.10.12 (Stable)

https://github.com/iojs/io.js/commit/a088cf4f930d3928c97d239adf950ab43e7794aa

* npm: Upgrade to 1.2.32
* readline: make `ctrl + L` clear the screen (Yuan Chuan)
* v8: add setVariableValue debugger command (Ben Noordhuis)
* net: Do not destroy socket mid-write (isaacs)
* v8: fix build for mips32r2 architecture (Andrei Sedoi)
* configure: fix cross-compilation host_arch_cc() (Andrei Sedoi)

## 2013.06.13, Version 0.10.11 (Stable)

https://github.com/iojs/io.js/commit/d9d5bc465450ae5d60da32e9ffcf71c2767f1fad

* uv: upgrade to 0.10.11
* npm: Upgrade to 1.2.30
* openssl: add missing configuration pieces for MIPS (Andrei Sedoi)
* Revert "http: remove bodyHead from 'upgrade' events" (isaacs)
* v8: fix pointer arithmetic undefined behavior (Trevor Norris)
* crypto: fix utf8/utf-8 encoding check (Ben Noordhuis)
* net: Fix busy loop on POLLERR|POLLHUP on older linux kernels (Ben Noordhuis, isaacs)

## 2013.06.04, Version 0.10.10 (Stable)

https://github.com/iojs/io.js/commit/25e51c396aa23018603baae2b1d9390f5d9db496

* uv: Upgrade to 0.10.10
* npm: Upgrade to 1.2.25
* url: Properly parse certain oddly formed urls (isaacs)
* stream: unshift('') is a noop (isaacs)

## 2013.05.30, Version 0.10.9 (Stable)

https://github.com/iojs/io.js/commit/878ffdbe6a8eac918ef3a7f13925681c3778060b

* npm: Upgrade to 1.2.24
* uv: Upgrade to v0.10.9
* repl: fix JSON.parse error check (Brian White)
* tls: proper .destroySoon (Fedor Indutny)
* tls: invoke write cb only after opposite read end (Fedor Indutny)
* tls: ignore .shutdown() syscall error (Fedor Indutny)

## 2013.05.24, Version 0.10.8 (Stable)

https://github.com/iojs/io.js/commit/30d9e9fdd9d4c33d3d95a129d021cd8b5b91eddb

* v8: update to 3.14.5.9
* uv: upgrade to 0.10.8
* npm: Upgrade to 1.2.23
* http: remove bodyHead from 'upgrade' events (Nathan Zadoks)
* http: Return true on empty writes, not false (isaacs)
* http: save roundtrips, convert buffers to strings (Ben Noordhuis)
* configure: respect the --dest-os flag consistently (Nathan Rajlich)
* buffer: throw when writing beyond buffer (Trevor Norris)
* crypto: Clear error after DiffieHellman key errors (isaacs)
* string_bytes: strip padding from base64 strings (Trevor Norris)

## 2013.05.17, Version 0.10.7 (Stable)

https://github.com/iojs/io.js/commit/d2fdae197ac542f686ee06835d1153dd43b862e5

* uv: upgrade to v0.10.7
* npm: Upgrade to 1.2.21
* crypto: Don't ignore verify encoding argument (isaacs)
* buffer, crypto: fix default encoding regression (Ben Noordhuis)
* timers: fix setInterval() assert (Ben Noordhuis)

## 2013.05.14, Version 0.10.6 (Stable)

https://github.com/iojs/io.js/commit/5deb1672f2b5794f8be19498a425ea4dc0b0711f

* module: Deprecate require.extensions (isaacs)
* stream: make Readable.wrap support objectMode, empty streams (Daniel Moore)
* child_process: fix handle delivery (Ben Noordhuis)
* crypto: Fix performance regression (isaacs)
* src: DRY string encoding/decoding (isaacs)

## 2013.04.23, Version 0.10.5 (Stable)

https://github.com/iojs/io.js/commit/deeaf8fab978e3cadb364e46fb32dafdebe5f095

* uv: Upgrade to 0.10.5 (isaacs)
* build: added support for Visual Studio 2012 (Miroslav Bajtoš)
* http: Don't try to destroy nonexistent sockets (isaacs)
* crypto: LazyTransform on properties, not methods (isaacs)
* assert: put info in err.message, not err.name (Ryan Doenges)
* dgram: fix no address bind() (Ben Noordhuis)
* handle_wrap: fix NULL pointer dereference (Ben Noordhuis)
* os: fix unlikely buffer overflow in os.type() (Ben Noordhuis)
* stream: Fix unshift() race conditions (isaacs)

## 2013.04.11, Version 0.10.4 (Stable)

https://github.com/iojs/io.js/commit/9712aa9f76073c30850b20a188b1ed12ffb74d17

* uv: Upgrade to 0.10.4
* npm: Upgrade to 1.2.18
* v8: Avoid excessive memory growth in JSON.parse (Fedor Indutny)
* child_process, cluster: fix O(n*m) scan of cmd string (Ben Noordhuis)
* net: fix socket.bytesWritten Buffers support (Fedor Indutny)
* buffer: fix offset checks (Łukasz Walukiewicz)
* stream: call write cb before finish event (isaacs)
* http: Support write(data, 'hex') (isaacs)
* crypto: dh secret should be left-padded (Fedor Indutny)
* process: expose NODE_MODULE_VERSION in process.versions (Rod Vagg)
* crypto: fix constructor call in crypto streams (Andreas Madsen)
* net: account for encoding in .byteLength (Fedor Indutny)
* net: fix buffer iteration in bytesWritten (Fedor Indutny)
* crypto: zero is not an error if writing 0 bytes (Fedor Indutny)
* tls: Re-enable check of CN-ID in cert verification (Tobias Müllerleile)

## 2013.04.03, Version 0.10.3 (Stable)

https://github.com/iojs/io.js/commit/d4982f6f5e4a9a703127489a553b8d782997ea43

* npm: Upgrade to 1.2.17
* child_process: acknowledge sent handles (Fedor Indutny)
* etw: update prototypes to match dtrace provider (Timothy J Fontaine)
* dtrace: pass more arguments to probes (Dave Pacheco)
* build: allow building with dtrace on osx (Dave Pacheco)
* http: Remove legacy ECONNRESET workaround code (isaacs)
* http: Ensure socket cleanup on client response end (isaacs)
* tls: Destroy socket when encrypted side closes (isaacs)
* repl: isSyntaxError() catches "strict mode" errors (Nathan Rajlich)
* crypto: Pass options to ctor calls (isaacs)
* src: tie process.versions.uv to uv_version_string() (Ben Noordhuis)

## 2013.03.28, Version 0.10.2 (Stable)

https://github.com/iojs/io.js/commit/1e0de9c426e07a260bbec2d2196c2d2db8eb8886

* npm: Upgrade to 1.2.15
* uv: Upgrade to 0.10.3
* tls: handle SSL_ERROR_ZERO_RETURN (Fedor Indutny)
* tls: handle errors before calling C++ methods (Fedor Indutny)
* tls: remove harmful unnecessary bounds checking (Marcel Laverdet)
* crypto: make getCiphers() return non-SSL ciphers (Ben Noordhuis)
* crypto: check randomBytes() size argument (Ben Noordhuis)
* timers: do not calculate Timeout._when property (Alexey Kupershtokh)
* timers: fix off-by-one ms error (Alexey Kupershtokh)
* timers: handle signed int32 overflow in enroll() (Fedor Indutny)
* stream: Fix stall in Transform under very specific conditions (Gil Pedersen)
* stream: Handle late 'readable' event listeners (isaacs)
* stream: Fix early end in Writables on zero-length writes (isaacs)
* domain: fix domain callback from MakeCallback (Trevor Norris)
* child_process: don't emit same handle twice (Ben Noordhuis)
* child_process: fix sending utf-8 to child process (Ben Noordhuis)

## 2013.03.21, Version 0.10.1 (Stable)

https://github.com/iojs/io.js/commit/c274d1643589bf104122674a8c3fd147527a667d

* npm: upgrade to 1.2.15
* crypto: Improve performance of non-stream APIs (Fedor Indutny)
* tls: always reset this.ssl.error after handling (Fedor Indutny)
* tls: Prevent mid-stream hangs (Fedor Indutny, isaacs)
* net: improve arbitrary tcp socket support (Ben Noordhuis)
* net: handle 'finish' event only after 'connect' (Fedor Indutny)
* http: Don't hot-path end() for large buffers (isaacs)
* fs: Missing cb errors are deprecated, not a throw (isaacs)
* fs: make write/appendFileSync correctly set file mode (Raymond Feng)
* stream: Return self from readable.wrap (isaacs)
* stream: Never call decoder.end() multiple times (Gil Pedersen)
* windows: enable watching signals with process.on('SIGXYZ') (Bert Belder)
* node: revert removal of MakeCallback (Trevor Norris)
* node: Unwrap without aborting in handle fd getter (isaacs)

## 2013.03.11, Version 0.10.0 (Stable)

https://github.com/iojs/io.js/commit/163ca274230fce536afe76c64676c332693ad7c1

* npm: Upgrade to 1.2.14
* core: Append filename properly in dlopen on windows (isaacs)
* zlib: Manage flush flags appropriately (isaacs)
* domains: Handle errors thrown in nested error handlers (isaacs)
* buffer: Strip high bits when converting to ascii (Ben Noordhuis)
* win/msi: Enable modify and repair (Bert Belder)
* win/msi: Add feature selection for various node parts (Bert Belder)
* win/msi: use consistent registry key paths (Bert Belder)
* child_process: support sending dgram socket (Andreas Madsen)
* fs: Raise EISDIR on Windows when calling fs.read/write on a dir (isaacs)
* unix: fix strict aliasing warnings, macro-ify functions (Ben Noordhuis)
* unix: honor UV_THREADPOOL_SIZE environment var (Ben Noordhuis)
* win/tty: fix typo in color attributes enumeration (Bert Belder)
* win/tty: don't touch insert mode or quick edit mode (Bert Belder)

## 2013.03.06, Version 0.9.12 (Unstable)

https://github.com/iojs/io.js/commit/0debf5a82934da805592b6496756cdf27c993abc

* stream: Allow strings in Readable.push/unshift (isaacs)
* stream: Remove bufferSize option (isaacs)
* stream: Increase highWaterMark on large reads (isaacs)
* stream: _write: takes an encoding argument (isaacs)
* stream: _transform: remove output() method, provide encoding (isaacs)
* stream: Don't require read(0) to emit 'readable' event (isaacs)
* node: Add --throw-deprecation (isaacs)
* http: fix multiple timeout events (Eugene Girshov)
* http: More useful setTimeout API on server (isaacs)
* net: use close callback, not process.nextTick (Ben Noordhuis)
* net: Provide better error when writing after FIN (isaacs)
* dns: Support NAPTR queries (Pavel Lang)
* dns: fix ReferenceError in resolve() error path (Xidorn Quan)
* child_process: handle ENOENT correctly on Windows (Scott Blomquist)
* cluster: Rename destroy() to kill(signal=SIGTERM) (isaacs)
* build: define nightly tag external to build system (Timothy J Fontaine)
* build: make msi build work when spaces are present in the path (Bert Belder)
* build: fix msi build issue with WiX 3.7/3.8 (Raymond Feng)
* repl: make compatible with domains (Dave Olszewski)
* events: Code cleanup and performance improvements (Trevor Norris)

## 2013.03.01, Version 0.9.11 (Unstable)

https://github.com/iojs/io.js/commit/83392403b7a9b7782b37c17688938c75010f81ba

* V8: downgrade to 3.14.5
* openssl: update to 1.0.1e
* darwin: Make process.title work properly (Ben Noordhuis)
* fs: Support mode/flag options to read/append/writeFile (isaacs)
* stream: _read() no longer takes a callback (isaacs)
* stream: Add stream.unshift(chunk) (isaacs)
* stream: remove lowWaterMark feature (isaacs)
* net: omit superfluous 'connect' event (Ben Noordhuis)
* build, windows: disable SEH (Ben Noordhuis)
* core: remove errno global (Ben Noordhuis)
* core: Remove the nextTick for running the main file (isaacs)
* core: Mark exit() calls with status codes (isaacs)
* core: Fix debug signal handler race condition lock (isaacs)
* crypto: clear error stack (Ben Noordhuis)
* test: optionally set common.PORT via env variable (Timothy J Fontaine)
* path: Throw TypeError on non-string args to path.resolve/join (isaacs, Arianit Uka)
* crypto: fix uninitialized memory access in openssl (Ben Noordhuis)

## 2013.02.19, Version 0.9.10 (Unstable)

* V8: Upgrade to 3.15.11.15
* npm: Upgrade to 1.2.12
* fs: Change default WriteStream config, increase perf (isaacs)
* process: streamlining tick callback logic (Trevor Norris)
* stream_wrap, udp_wrap: add read-only fd property (Ben Noordhuis)
* buffer: accept negative indices in Buffer#slice() (Ben Noordhuis)
* tls: Cycle data when underlying socket drains (isaacs)
* stream: read(0) should not always trigger _read(n,cb) (isaacs)
* stream: Empty strings/buffers do not signal EOF any longer (isaacs)
* crypto: improve cipher/decipher error messages (Ben Noordhuis)
* net: Respect the 'readable' flag on sockets (isaacs)
* net: don't suppress ECONNRESET (Ben Noordhuis)
* typed arrays: copy Buffer in typed array constructor (Ben Noordhuis)
* typed arrays: make DataView throw on non-ArrayBuffer (Ben Noordhuis)
* windows: MSI installer enhancements (Scott Blomquist, Jim Schubert)

## 2013.02.07, Version 0.9.9 (Unstable)

https://github.com/iojs/io.js/commit/4b9f0d190cd6b22853caeb0e07145a98ce1d1d7f

* tls: port CryptoStream to streams2 (Fedor Indutny)
* typed arrays: only share ArrayBuffer backing store (Ben Noordhuis)
* stream: make Writable#end() accept a callback function (Nathan Rajlich)
* buffer: optimize 'hex' handling (Ben Noordhuis)
* dns, cares: don't filter NOTIMP, REFUSED, SERVFAIL (Ben Noordhuis)
* readline: treat bare \r as a line ending (isaacs)
* readline: make \r\n emit one 'line' event (Ben Noordhuis)
* cluster: support datagram sockets (Bert Belder)
* stream: Correct Transform class backpressure (isaacs)
* addon: Pass module object to NODE_MODULE init function (isaacs, Rod Vagg)
* buffer: slow buffer copy compatibility fix (Trevor Norris)
* Add bytesWritten to tls.CryptoStream (Andy Burke)

## 2013.01.24, Version 0.9.8 (Unstable)

https://github.com/iojs/io.js/commit/5f2f8400f665dc32c3e10e7d31d53d756ded9156

* npm: Upgrade to v1.2.3
* V8: Upgrade to 3.15.11.10
* streams: Support objects other than Buffers (Jake Verbaten)
* buffer: remove float write range checks (Trevor Norris)
* http: close connection on 304/204 responses with chunked encoding (Ben Noordhuis)
* build: fix build with dtrace support on FreeBSD (Fedor Indutny)
* console: Support formatting options in trace() (isaacs)
* domain: empty stack on all exceptions (Dave Olszewski)
* unix, windows: make uv_*_bind() error codes consistent (Andrius Bentkus)
* linux: add futimes() fallback (Ben Noordhuis)

## 2013.01.18, Version 0.9.7 (Unstable)

https://github.com/iojs/io.js/commit/9e7bebeb8305edd55735a95955a98fdbe47572e5

* V8: Upgrade to 3.15.11.7
* npm: Upgrade to 1.2.2
* punycode: Upgrade to 1.2.0 (Mathias Bynens)
* repl: make built-in modules available by default (Felix Böhm)
* windows: add support for '_Total' perf counters (Scott Blomquist)
* cluster: make --prof work for workers (Ben Noordhuis)
* child_process: do not keep list of sent sockets (Fedor Indutny)
* tls: Follow RFC6125 more strictly (Fedor Indutny)
* buffer: floating point read/write improvements (Trevor Norris)
* TypedArrays: Improve dataview perf without endian param (Dean McNamee)
* module: assert require() called with a non-empty string (Felix Böhm, James Campos)
* stdio: Set readable/writable flags properly (isaacs)
* stream: Properly handle large reads from push-streams (isaacs)

## 2013.01.11, Version 0.9.6 (Unstable)

https://github.com/iojs/io.js/commit/9313fdc71ca8335d5e3a391c103230ee6219b3e2

* V8: update to 3.15.11.5
* node: remove ev-emul.h (Ben Noordhuis)
* path: make basename and extname ignore trailing slashes (Bert Belder)
* typed arrays: fix sunos signed/unsigned char issue (Ben Noordhuis)
* child_process: Fix {stdio:'inherit'} regression (Ben Noordhuis)
* child_process: Fix pipe() from child stdio streams  (Maciej Małecki)
* child_process: make fork() execPath configurable (Bradley Meck)
* stream: Add readable.push(chunk) method (isaacs)
* dtrace: x64 ustack helper (Fedor Indutny)
* repl: fix floating point number parsing (Nirk Niggler)
* repl: allow overriding builtins (Ben Noordhuis)
* net: add localAddress and localPort to Socket (James Hight)
* fs: make pool size coincide with ReadStream bufferSize (Shigeki Ohtsu)
* typed arrays: implement load and store swizzling (Dean McNamee)
* windows: fix perfctr crash on XP and 2003 (Scott Blomquist)
* dgram: fix double implicit bind error (Ben Noordhuis)

## 2012.12.30, Version 0.9.5 (Unstable)

https://github.com/iojs/io.js/commit/01994e8119c24f2284bac0779b32acb49c95bee7

* assert: improve support for new execution contexts (lukebayes)
* domain: use camelCase instead of snake_case (isaacs)
* domain: Do not use uncaughtException handler (isaacs)
* fs: make 'end' work with ReadStream without 'start' (Ben Noordhuis)
* https: optimize createConnection() (Ryunosuke SATO)
* buffer: speed up base64 encoding by 20% (Ben Noordhuis)
* doc: Colorize API stabilitity index headers in docs (Luke Arduini)
* net: socket.readyState corrections (bentaber)
* http: Performance enhancements for http under streams2 (isaacs)
* stream: fix to emit end event on http.ClientResponse (Shigeki Ohtsu)
* stream: fix event handler leak in readstream pipe and unpipe (Andreas Madsen)
* build: Support ./configure --tag switch (Maciej Małecki)
* repl: don't touch `require.cache` (Nathan Rajlich)
* node: Emit 'exit' event when exiting for an uncaught exception (isaacs)

## 2012.12.21, Version 0.9.4 (Unstable)

https://github.com/iojs/io.js/commit/d86d83c75f6343b5368bb7bd328b4466a035e1d4

* streams: Update all streaming interfaces to use new classes (isaacs)
* node: remove idle gc (Ben Noordhuis)
* http: protect against response splitting attacks (Bert Belder)
* fs: Raise error when null bytes detected in paths (isaacs)
* fs: fix 'object is not a function' callback errors (Ben Noordhuis)
* fs: add autoClose=true option to fs.createReadStream (Farid Neshat)
* process: add getgroups(), setgroups(), initgroups() (Ben Noordhuis)
* openssl: optimized asm code on x86 and x64 (Bert Belder)
* crypto: fix leak in GetPeerCertificate (Fedor Indutny)
* add systemtap support (Jan Wynholds)
* windows: add ETW and PerfCounters support (Scott Blomquist)
* windows: fix normalization of UNC paths (Bert Belder)
* crypto: fix ssl error handling (Sergey Kholodilov)
* node: remove eio-emul.h (Ben Noordhuis)
* os: add os.endianness() function (Nathan Rajlich)
* readline: don't emit "line" events with a trailing '\n' char (Nathan Rajlich)
* build: add configure option to generate xcode build files (Timothy J Fontaine)
* build: allow linking against system libuv, cares, http_parser (Stephen Gallagher)
* typed arrays: add slice() support to ArrayBuffer (Anthony Pesch)
* debugger: exit and kill child on SIGTERM or SIGHUP (Fedor Indutny)
* url: url.format escapes delimiters in path and query (J. Lee Coltrane)

## 2012.10.24, Version 0.9.3 (Unstable)

https://github.com/iojs/io.js/commit/1ed4c6776e4f52956918b70565502e0f8869829d

* V8: Upgrade to 3.13.7.4
* crypto: Default to buffers instead of binary strings (isaacs, Fedor Indutny)
* crypto: add getHashes() and getCiphers() (Ben Noordhuis)
* unix: add custom thread pool, remove libeio (Ben Noordhuis)
* util: make `inspect()` accept an "options" argument (Nathan Rajlich)
* https: fix renegotation attack protection (Ben Noordhuis)
* cluster: make 'listening' handler see actual port (Aaditya Bhatia)
* windows: use USERPROFILE to get the user's home dir (Bert Belder)
* path: add platform specific path delimiter (Paul Serby)
* http: add response.headersSent property (Pavel Lang)
* child_process: make .fork()'d child auto-exit (Ben Noordhuis)
* events: add 'removeListener' event (Ben Noordhuis)
* string_decoder: Add 'end' method, do base64 properly (isaacs)
* buffer: include encoding value in exception when invalid (Ricky Ng-Adam)
* http: make http.ServerResponse no longer emit 'end' (isaacs)
* streams: fix pipe is destructed by 'end' from destination (koichik)

## 2012.09.17, Version 0.9.2 (Unstable)

https://github.com/iojs/io.js/commit/6e2055889091a424fbb5c500bc3ab9c05d1c28b4

* http_parser: upgrade to ad3b631
* openssl: upgrade 1.0.1c
* darwin: use FSEvents to watch directory changes (Fedor Indutny)
* unix: support missing API on NetBSD (Shigeki Ohtsu)
* unix: fix EMFILE busy loop (Ben Noordhuis)
* windows: un-break writable tty handles (Bert Belder)
* windows: map WSAESHUTDOWN to UV_EPIPE (Bert Belder)
* windows: make spawn with custom environment work again (Bert Belder)
* windows: map ERROR_DIRECTORY to UV_ENOENT (Bert Belder)
* tls, https: validate server certificate by default (Ben Noordhuis)
* tls, https: throw exception on missing key/cert (Ben Noordhuis)
* tls: async session storage (Fedor Indutny)
* installer: don't install header files (Ben Noordhuis)
* buffer: implement Buffer.prototype.toJSON() (Nathan Rajlich)
* buffer: added support for writing NaN and Infinity (koichik)
* http: make http.ServerResponse emit 'end' (Ben Noordhuis)
* build: ./configure --ninja (Ben Noordhuis, Timothy J Fontaine)
* installer: fix --without-npm (Ben Noordhuis)
* cli: make -p equivalent to -pe (Ben Noordhuis)
* url: Go much faster by using Url class (isaacs)

## 2012.08.28, Version 0.9.1 (Unstable)

https://github.com/iojs/io.js/commit/e6ce259d2caf338fec991c2dd447de763ce99ab7

* buffer: Add Buffer.isEncoding(enc) to test for valid encoding values (isaacs)
* Raise UV_ECANCELED on premature close. (Ben Noordhuis)
* Remove c-ares from libuv, move to a top-level node dependency (Bert Belder)
* ref/unref for all HandleWraps, timers, servers, and sockets (Timothy J Fontaine)
* addon: remove node-waf, superseded by node-gyp (Ben Noordhuis)
* child_process: emit error on exec failure (Ben Noordhuis)
* cluster: do not use internal server API (Andreas Madsen)
* constants: add O_DIRECT (Ian Babrou)
* crypto: add sync interface to crypto.pbkdf2() (Ben Noordhuis)
* darwin: emulate fdatasync() (Fedor Indutny)
* dgram: make .bind() always asynchronous (Ben Noordhuis)
* events: Make emitter.listeners() side-effect free (isaacs, Joe Andaverde)
* fs: Throw early on invalid encoding args (isaacs)
* fs: fix naming of truncate/ftruncate functions (isaacs)
* http: bubble up parser errors to ClientRequest (Brian White)
* linux: improve cpuinfo parser on ARM and MIPS (Ben Noordhuis)
* net: add support for IPv6 addresses ending in :: (Josh Erickson)
* net: support Server.listen(Pipe) (Andreas Madsen)
* node: don't scan add-on for "init" symbol (Ben Noordhuis)
* remove process.uvCounters() (Ben Noordhuis)
* repl: console writes to repl rather than process stdio (Nathan Rajlich)
* timers: implement setImmediate (Timothy J Fontaine)
* tls: fix segfault in pummel/test-tls-ci-reneg-attack (Ben Noordhuis)
* tools: Move gyp addon tools to node-gyp (Nathan Rajlich)
* unix: preliminary signal handler support (Ben Noordhuis)
* unix: remove dependency on ev_child (Ben Noordhuis)
* unix: work around darwin bug, don't poll() on pipe (Fedor Indutny)
* util: Formally deprecate util.pump() (Ben Noordhuis)
* windows: make active and closing handle state independent (Bert Belder)
* windows: report spawn errors to the exit callback (Bert Belder)
* windows: signal handling support with uv_signal_t (Bert Belder)

## 2012.07.20, Version 0.9.0 (Unstable)

https://github.com/iojs/io.js/commit/f9b237f478c372fd55e4590d7399dcd8f25f3603

* punycode: update to v1.1.1 (Mathias Bynens)
* c-ares: upgrade to 1.9.0 (Saúl Ibarra Corretgé)
* dns: ignore rogue DNS servers reported by windows (Saúl Ibarra Corretgé)
* unix: speed up uv_async_send() (Ben Noordhuis)
* darwin: get cpu model correctly on mac (Xidorn Quan)
* nextTick: Handle tick callbacks before any other I/O (isaacs)
* Enable color customization of `util.inspect` (Pavel Lang)
* tls: Speed and memory improvements (Fedor Indutny)
* readline: Use one history item for reentered line (Vladimir Beloborodov)
* Fix [#3521](https://github.com/joyent/node/issues/3521) Make process.env more like a regular Object (isaacs)

## 2013.06.13, Version 0.8.25 (maintenance)

https://github.com/iojs/io.js/commit/0b9bdb2bc7e1c872f0ea4713517fda22a4b0b202

* npm: Upgrade to 1.2.30
* child_process: fix handle delivery (Ben Noordhuis)

## 2013.06.04, Version 0.8.24 (maintenance)

https://github.com/iojs/io.js/commit/c1a1ab067721ea17ef7b05ec5c68b01321017f05

* npm: Upgrade to v1.2.24
* url: Properly parse certain oddly formed urls (isaacs)
* http: Don't try to destroy nonexistent sockets (isaacs)
* handle_wrap: fix NULL pointer dereference (Ben Noordhuis)

## 2013.04.09, Version 0.8.23 (maintenance)

https://github.com/iojs/io.js/commit/c67f8d0500fe15637a623eb759d2ad7eb9fb3b0b

* npm: Upgrade to v1.2.18
* http: Avoid EE warning on ECONNREFUSED handling (isaacs)
* tls: Re-enable check of CN-ID in cert verification (Tobias Müllerleile)
* child_process: fix sending utf-8 to child process (Ben Noordhuis)
* crypto: check key type in GetPeerCertificate() (Ben Noordhuis)
* win/openssl: mark assembled object files as seh safe (Bert Belder)
* windows/msi: fix msi build issue with WiX 3.7/3.8 (Raymond Feng)

## 2013.03.07, Version 0.8.22 (Stable)

https://github.com/iojs/io.js/commit/67a4cb4fe8c2346e30ffb83f7178e205cc2dab33

* npm: Update to 1.2.14
* cluster: propagate bind errors (Ben Noordhuis)
* crypto: don't assert when calling Cipher#final() twice (Ben Noordhuis)
* build, windows: disable SEH (Ben Noordhuis)

## 2013.02.25, Version 0.8.21 (Stable)

https://github.com/iojs/io.js/commit/530d8c05d4c546146f18e5ba811d7eb3b7b7c0c5

* http: Do not free the wrong parser on socket close (isaacs)
* http: Handle hangup writes more gently (isaacs)
* zlib: fix assert on bad input (Ben Noordhuis)
* test: add TAP output to the test runner (Timothy J Fontaine)
* unix: Handle EINPROGRESS from domain sockets (Ben Noordhuis)

## 2013.02.15, Version 0.8.20 (Stable)

https://github.com/iojs/io.js/commit/e10c75579b536581ddd7ae4e2c3bf8a9d550d343

* npm: Upgrade to v1.2.11
* http: Do not let Agent hand out destroyed sockets (isaacs)
* http: Raise hangup error on destroyed socket write (isaacs)
* http: protect against response splitting attacks (Bert Belder)

## 2013.02.06, Version 0.8.19 (Stable)

https://github.com/iojs/io.js/commit/53978bdf420622ff0121c63c0338c9e7c2e60869

* npm: Upgrade to v1.2.10
* zlib: pass object size hint to V8 (Ben Noordhuis)
* zlib: reduce memory consumption, release early (Ben Noordhuis)
* buffer: slow buffer copy compatibility fix (Trevor Norris)
* zlib: don't assert on malformed dictionary (Ben Noordhuis)
* zlib: don't assert on missing dictionary (Ben Noordhuis)
* windows: better ipv6 support (Bert Belder)
* windows: add error mappings related to unsupported protocols (Bert Belder)
* windows: map ERROR_DIRECTORY to UV_ENOENT (Bert Belder)

## 2013.01.18, Version 0.8.18 (Stable)

https://github.com/iojs/io.js/commit/2c4eef0d972838c51999d32c0d251857a713dc18

* npm: Upgrade to v1.2.2
* dns: make error message match errno (Dan Milon)
* tls: follow RFC6125 more stricly (Fedor Indutny)
* buffer: reject negative SlowBuffer offsets (Ben Noordhuis)
* install: add simplejson fallback (Chris Dent)
* http: fix "Cannot call method 'emit' of null" (Ben Noordhuis)

## 2013.01.09, Version 0.8.17 (Stable)

https://github.com/iojs/io.js/commit/c50c33e9397d7a0a8717e8ce7530572907c054ad

* npm: Upgrade to v1.2.0
  - peerDependencies (Domenic Denicola)
  - node-gyp v0.8.2 (Nathan Rajlich)
  - Faster installs from github user/project shorthands (Nathan Zadoks)

* typed arrays: fix 32 bit size/index overflow (Ben Noordhuis)
* http: Improve performance of single-packet responses (Ben Noordhuis)
* install: fix openbsd man page location (Ben Noordhuis)
* http: bubble up parser errors to ClientRequest (Brian White)

## 2012.12.13, Version 0.8.16 (Stable)

https://github.com/iojs/io.js/commit/1c9c6277d5cfcaaac8569c0c8f7daa64292048a9

* npm: Upgrade to 1.1.69
* fs: fix WriteStream/ReadStream fd leaks (Ben Noordhuis)
* crypto: fix leak in GetPeerCertificate (Fedor Indutny)
* buffer: Don't double-negate numeric buffer arg (Trevor Norris)
* net: More accurate IP address validation and IPv6 dotted notation. (Joshua Erickson)

## 2012.11.26, Version 0.8.15 (Stable)

https://github.com/iojs/io.js/commit/fdf91afb494a7a2fff2913d817f589c191a2c88f

* npm: Upgrade to 1.1.66 (isaacs)
* linux: use /proc/cpuinfo for CPU frequency (Ben Noordhuis)
* windows: map WSAESHUTDOWN to UV_EPIPE (Ben Noordhuis)
* windows: map ERROR_GEN_FAILURE to UV_EIO (Bert Belder)
* unix: do not set environ unless one is provided (Charlie McConnell)
* domains: don't crash if domain is set to null (Bert Belder)
* windows: fix the x64 debug build (Bert Belder)
* net, tls: fix connect() resource leak (Ben Noordhuis)

## 2012.10.25, Version 0.8.14 (Stable)

https://github.com/iojs/io.js/commit/b00527fcf05c3d9f https://github.com/iojs/io.js/commit/b5d5d790f9472906a59fe218

* events: Don't clobber pre-existing _events obj in EE ctor (isaacs)

## 2012.10.25, Version 0.8.13 (Stable)

https://github.com/iojs/io.js/commit/ff4c974873f9a7cc6a5b042eb9b6389bb8dde6d6

* V8: Upgrade to 3.11.10.25
* npm: Upgrade to 1.1.65
* url: parse hostnames that start with - or _ (Ben Noordhuis)
* repl: Fix Windows 8 terminal issue (Bert Belder)
* typed arrays: use signed char for signed int8s (Aaron Jacobs)
* crypto: fix bugs in DiffieHellman (Ben Noordhuis)
* configure: turn on VFPv3 on ARMv7 (Ben Noordhuis)
* Re-enable OpenSSL UI for entering passphrases via tty (Ben Noordhuis)
* repl: ensure each REPL instance gets its own "context" (Nathan Rajlich)

## 2012.10.12, Version 0.8.12 (Stable)

https://github.com/iojs/io.js/commit/38c72d4e29574dec5205bcf23c2a85efe65331a4

* npm: Upgrade to 1.1.63
* crypto: Reduce stability index to 2-Unstable (isaacs)
* windows: fix handle leak in uv_fs_utime (Bert Belder)
* windows: fix application crashed popup in debug version (Bert Belder)
* buffer: report proper retained size in profiler (Ben Noordhuis)
* buffer: fix byteLength with UTF-16LE (koichik)
* repl: make "end of input" JSON.parse() errors throw in the REPL (Nathan Rajlich)
* repl: make invalid RegExp modifiers throw in the REPL (Nathan Rajlich)
* http: handle multiple Proxy-Authenticate values (Willi Eggeling)

## 2012.09.27, Version 0.8.11 (Stable)

https://github.com/iojs/io.js/commit/e1f39468fa580c1e4cb15fac621f87944ee625dc

* fs: Fix stat() size reporting for large files (Ben Noordhuis)

## 2012.09.25, Version 0.8.10 (Stable)

https://github.com/iojs/io.js/commit/0bc273da4fcaa79b209ed755ad249a3e7be626a6

* npm: Upgrade to 1.1.62
* repl: make invalid RegExps throw in the REPL (Nathan Rajlich)
* v8: loosen artificial mmap constraint (Bryan Cantrill)
* process: fix setuid() and setgid() error reporting (Ben Noordhuis)
* domain: Properly exit() on domain disposal (isaacs)
* fs: fix watchFile() missing deletion events (Ben Noordhuis)
* fs: fix assert in fs.watch() (Ben Noordhuis)
* fs: don't segfault on deeply recursive stat() (Ben Noordhuis)
* http: Remove timeout handler when data arrives (Frédéric Germain)
* http: make the client "res" object gets the same domain as "req" (Nathan Rajlich)
* windows: don't blow up when an invalid FD is used (Bert Belder)
* unix: map EDQUOT to UV_ENOSPC (Charlie McConnell)
* linux: improve /proc/cpuinfo parser (Ben Noordhuis)
* win/tty: reset background brightness when color is set to default (Bert Belder)
* unix: put child process stdio fds in blocking mode (Ben Noordhuis)
* unix: fix EMFILE busy loop (Ben Noordhuis)
* sunos: don't set TCP_KEEPALIVE (Ben Noordhuis)
* tls: Use slab allocator for memory management (Fedor Indutny)
* openssl: Use optimized assembly code for x86 and x64 (Bert Belder)

## 2012.09.11, Version 0.8.9 (Stable)

https://github.com/iojs/io.js/commit/b88c3902b241cf934e75443b934f2033ad3915b1

* v8: upgrade to 3.11.10.22
* GYP: upgrade to r1477
* npm: Upgrade to 1.1.61
* npm: Don't create world-writable files (isaacs)
* windows: fix single-accept mode for shared server sockets (Bert Belder)
* windows: fix uninitialized memory access in uv_update_time() (Bert Belder)
* windows: don't throw when a signal handler is attached (Bert Belder)
* unix: fix memory leak in udp (Ben Noordhuis)
* unix: map errno ESPIPE (Ben Noordhuis)
* unix, windows: fix memory corruption in fs-poll.c (Ben Noordhuis)
* sunos: fix os.cpus() on x86_64 (Ben Noordhuis)
* child process: fix processes with IPC channel don't emit 'close' (Bert Belder)
* build: add a "--dest-os" option to force a gyp "flavor" (Nathan Rajlich)
* build: set `process.platform` to "sunos" on SunOS (Nathan Rajlich)
* build: fix `make -j` fails after `make clean` (Bearice Ren)
* build: fix openssl configuration for "arm" builds (Nathan Rajlich)
* tls: support unix domain socket/named pipe in tls.connect (Shigeki Ohtsu)
* https: make https.get() accept a URL (koichik)
* http: respect HTTP/1.0 TE header (Ben Noordhuis)
* crypto, tls: Domainify setSNICallback, pbkdf2, randomBytes (Ben Noordhuis)
* stream.pipe: Don't call destroy() unless it's a function (isaacs)

## 2012.08.22, Version 0.8.8 (Stable)

https://github.com/iojs/io.js/commit/a299c97bbc701f4d460e91214d7bfe7a9589d361

* V8: upgrade to 3.11.10.19
* npm: upgrade to 1.1.59
* windows: fix uninitialized memory access in uv_update_time() (Bert Belder)
* unix, windows: fix memory corruption in fs-poll.c (Ben Noordhuis)
* unix: fix integer overflow in uv_hrtime (Tim Holy)
* sunos: fix uv_cpu_info() on x86_64 (Ben Noordhuis)
* tls: update default cipher list (Ben Noordhuis)
* unix: Fix llvm and older gcc duplicate symbol warnings (Bert Belder)
* fs: fix use after free in stat watcher (Ben Noordhuis)
* build: Fix using manually compiled gcc on OS X (Nathan Rajlich)
* windows: make junctions work again (Bert Belder)

## 2012.08.15, Version 0.8.7 (Stable)

https://github.com/iojs/io.js/commit/f640c5d35cba96634cd8176a525a1d876e361a61

* npm: Upgrade to 1.1.49
* website: download page (Golo Roden)
* crypto: fix uninitialized memory access in openssl (Ben Noordhuis)
* buffer, crypto: fix buffer decoding (Ben Noordhuis)
* build: compile with -fno-tree-vrp when gcc >= 4.0 (Ben Noordhuis)
* tls: handle multiple CN fields when verifying cert (Ben Noordhuis)
* doc: remove unused util from child_process (Kyle Robinson Young)
* build: rework -fvisibility=hidden detection (Ben Noordhuis)
* windows: don't duplicate invalid stdio handles (Bert Belder)
* windows: fix typos in process-stdio.c (Bert Belder)

## 2012.08.07, Version 0.8.6 (Stable)

https://github.com/iojs/io.js/commit/0544a586ca6b6b900a42e164033dbf350765700a

* npm: Upgrade to v1.1.48
* Add 'make binary' to build binary tarballs for all Unixes (Nathan Rajlich)
* zlib: Emit 'close' on destroy(). (Dominic Tarr)
* child_process: Fix stdout=null when stdio=['pipe'] (Tyler Neylon)
* installer: prevent ETXTBSY errors (Ben Noordhuis)
* installer: honor --without-npm, default install path (Ben Noordhuis)
* net: make pause work with connecting sockets (Bert Belder)
* installer: fix cross-compile installs (Ben Noordhuis)
* net: fix .listen({fd:0}) (Ben Noordhuis)
* windows: map WSANO_DATA to UV_ENOENT (Bert Belder)

## 2012.08.02, Version 0.8.5 (Stable)

https://github.com/iojs/io.js/commit/9b86a4453f0c76f2707a75c0b2343aba33ec63bc

* node: tag Encode and friends NODE_EXTERN (Ben Noordhuis)
* fs: fix ReadStream / WriteStream missing callback (Gil Pedersen)
* fs: fix readFileSync("/proc/cpuinfo") regression (Ben Noordhuis)
* installer: don't assume bash is installed (Ben Noordhuis)
* Report errors properly from --eval and stdin (isaacs)
* assert: fix throws() throws an error without message property (koichik)
* cluster: fix libuv assert in net.listen() (Ben Noordhuis)
* build: always link sunos builds with libumem (Trent Mick)
* build: improve armv7 / hard-float detection (Adam Malcontenti-Wilson)
* https: Use host header as effective servername (isaacs)
* sunos: work around OS bug to prevent fs.watch() from spinning (Bryan Cantrill)
* linux: fix 'two watchers, one path' segfault (Ben Noordhuis)
* windows: fix memory leaks in many fs functions (Bert Belder)
* windows: don't allow directories to be opened for writing/appending (Bert Belder)
* windows: make fork() work even when not all stdio handles are valid (Bert Belder)
* windows: make unlink() not remove mount points, and improve performance (Bert Belder)
* build: Sign pkg installer for OS X (isaacs)

## 2012.07.25, Version 0.8.4 (Stable)

https://github.com/iojs/io.js/commit/f98562fcd7d1cab573ca4dc1612157d6999befd4

* V8: Upgrade to 3.11.10.17
* npm: Upgrade to 1.1.45
* net: fix Socket({ fd: 42 }) api (Ben Noordhuis)
* readline: Remove event listeners on close (isaacs)
* windows: correctly prep long path for fs.exists(Sync) (Bert Belder)
* debugger: wake up the event loop when a debugger command is dispatched (Peter Rybin)
* tls: verify server's identity (Fedor Indutny)
* net: ignore socket.setTimeout(Infinity or NaN) (Fedor Indutny)

## 2012.07.19, Version 0.8.3 (Stable)

https://github.com/iojs/io.js/commit/60bf2d6cb33e4ce55604f73889ab840a9de8bdab

* V8: upgrade to 3.11.10.15
* npm: Upgrade to 1.1.43
* net: fix net.Server.listen({fd:x}) error reporting (Ben Noordhuis)
* net: fix bogus errno reporting (Ben Noordhuis)
* build: Move npm shebang logic into an npm script (isaacs)
* build: fix add-on loading on freebsd (Ben Noordhuis)
* build: disable unsafe optimizations (Ben Noordhuis)
* build: fix spurious mksnapshot crashes for good (Ben Noordhuis)
* build: speed up genv8constants (Dave Pacheco)
* fs: make unwatchFile() remove a specific listener (Ben Noordhuis)
* domain: Remove first arg from intercepted fn (Toshihiro Nakamura)
* domain: Fix memory leak on error (isaacs)
* events: Fix memory leak from removeAllListeners (Nathan Rajlich)
* zlib: Fix memory leak in Unzip class. (isaacs)
* crypto: Fix memory leak in DecipherUpdate() (Ben Noordhuis)

## 2012.07.09, Version 0.8.2 (Stable)

https://github.com/iojs/io.js/commit/cc6084b9ac5cf1d4fe5e7165b71e8fc05d11be1f

* npm: Upgrade to 1.1.36
* readline: don't use Function#call() (Nathan Rajlich)
* Code cleanup to pass 'use strict' (Jonas Westerlund)
* module: add filename to require() json errors (TJ Holowaychuk)
* readline: fix for unicode prompts (Tim Macfarlane)
* timers: fix handling of large timeouts (Ben Noordhuis)
* repl: fix passing an empty line inserting "undefined" into the buffer (Nathan Rajlich)
* repl: fix crashes when buffering command (Maciej Małecki)
* build: rename strict_aliasing to node_no_strict_aliasing (Ben Noordhuis)
* build: disable -fstrict-aliasing for any gcc < 4.6.0 (Ben Noordhuis)
* build: detect cc version with -dumpversion (Ben Noordhuis)
* build: handle output of localized gcc or clang (Ben Noordhuis)
* unix: fix memory corruption in freebsd.c (Ben Noordhuis)
* unix: fix 'zero handles, one request' busy loop (Ben Noordhuis)
* unix: fix busy loop on unexpected tcp message (Ben Noordhuis)
* unix: fix EINPROGRESS busy loop (Ben Noordhuis)

## 2012.06.29, Version 0.8.1 (stable)

https://github.com/iojs/io.js/commit/2134aa3d5c622fc3c3b02ccb713fcde0e0df479a

* V8: upgrade to v3.11.10.12
* npm: upgrade to v1.1.33
  - Support for parallel use of the cache folder
  - Retry on registry timeouts or network failures (Trent Mick)
  - Reduce 'engines' failures to a warning
  - Use new zsh completion if aviailable (Jeremy Cantrell)

* Fix [#3577](https://github.com/joyent/node/issues/3577) Un-break require('sys')
* util: speed up formatting of large arrays/objects (Ben Noordhuis)
* windows: make fs.realpath(Sync) work with UNC paths (Bert Belder)
* build: fix --shared-v8 option (Ben Noordhuis)
* doc: `detached` is a boolean (Andreas Madsen)
* build: use proper python interpreter (Ben Noordhuis)
* build: expand ~ in `./configure --prefix=~/a/b/c` (Ben Noordhuis)
* build: handle CC env var with spaces (Gabriel de Perthuis)
* build: fix V8 build when compiling with gcc 4.5 (Ben Noordhuis)
* build: fix --shared-v8 option (Ben Noordhuis)
* windows msi: Fix icon issue which caused huge file size (Bert Belder)
* unix: assume that dlopen() may clobber dlerror() (Ben Noordhuis)
* sunos: fix memory corruption bugs (Ben Noordhuis)
* windows: better (f)utimes and (f)stat (Bert Belder)

## 2012.06.25, Version 0.8.0 (stable)

https://github.com/iojs/io.js/commit/8b8a7a7f9b41e74e1e810d0330738ad06fc302ec

* V8: upgrade to v3.11.10.10
* npm: Upgrade to 1.1.32
* Deprecate iowatcher (Ben Noordhuis)
* windows: update icon (Bert Belder)
* http: Hush 'MUST NOT have a body' warnings to debug() (isaacs)
* Move blog.nodejs.org content into repository (isaacs)
* Fix [#3503](https://github.com/joyent/node/issues/3503): stdin: resume() on pipe(dest) (isaacs)
* crypto: fix error reporting in SetKey() (Fedor Indutny)
* Add --no-deprecation and --trace-deprecation command-line flags (isaacs)
* fs: fix fs.watchFile() (Ben Noordhuis)
* fs: Fix fs.readfile() on pipes (isaacs)
* Rename GYP variable node_use_system_openssl to be consistent (Ryan Dahl)

## 2012.06.19, Version 0.7.12 (unstable)

https://github.com/iojs/io.js/commit/a72120190a8ffdbcd3d6ad2a2e6ceecd2087111e

* npm: Upgrade to 1.1.30
	- Improved 'npm init'
	- Fix the 'cb never called' error from 'oudated' and 'update'
	- Add --save-bundle|-B config
	- Fix isaacs/npm[#2465](https://github.com/joyent/node/issues/2465): Make npm script and windows shims cygwin-aware
	- Fix isaacs/npm[#2452](https://github.com/joyent/node/issues/2452) Use --save(-dev|-optional) in npm rm
	- `logstream` option to replace removed `logfd` (Rod Vagg)
	- Read default descriptions from README.md files

* Shims to support deprecated ev_* and eio_* methods (Ben Noordhuis)
* [#3118](https://github.com/joyent/node/issues/3118) net.Socket: Delay pause/resume until after connect (isaacs)
* [#3465](https://github.com/joyent/node/issues/3465) Add ./configure --no-ifaddrs flag (isaacs)
* child_process: add .stdin stream to forks (Fedor Indutny)
* build: fix `make install DESTDIR=/path` (Ben Noordhuis)
* tls: fix off-by-one error in renegotiation check (Ben Noordhuis)
* crypto: Fix diffie-hellman key generation UTF-8 errors (Fedor Indutny)
* node: change the constructor name of process from EventEmitter to process (Andreas Madsen)
* net: Prevent property access throws during close (Reid Burke)
* querystring: improved speed and code cleanup (Felix Böhm)
* sunos: fix assertion errors breaking fs.watch() (Fedor Indutny)
* unix: stat: detect sub-second changes (Ben Noordhuis)
* add stat() based file watcher (Ben Noordhuis)

## 2012.06.15, Version 0.7.11 (unstable)

https://github.com/iojs/io.js/commit/5cfe0b86d5be266ef51bbba369c39e412ee51944

* V8: Upgrade to v3.11.10
* npm: Upgrade to 1.1.26
* doc: Improve cross-linking in API docs markdown (Ben Kelly)
* Fix [#3425](https://github.com/joyent/node/issues/3425): removeAllListeners should delete array (Reid Burke)
* cluster: don't silently drop messages when the write queue gets big (Bert Belder)
* Add Buffer.concat method (isaacs)
* windows: make symlinks tolerant to forward slashes (Bert Belder)
* build: Add node.d and node.1 to installer (isaacs)
* cluster: rename worker.unqiueID to worker.id (Andreas Madsen)
* Windows: Enable ETW events on Windows for existing DTrace probes. (Igor Zinkovsky)
* test: bundle node-weak in test/gc so that it doesn't need to be downloaded (Nathan Rajlich)
* Make many tests pass on Windows (Bert Belder)
* Fix [#3388](https://github.com/joyent/node/issues/3388) Support listening on file descriptors (isaacs)
* Fix [#3407](https://github.com/joyent/node/issues/3407) Add os.tmpDir() (isaacs)
* Unbreak the snapshotted build on Windows (Bert Belder)
* Clean up child_process.kill throws (Bert Belder)
* crypto: make cipher/decipher accept buffer args (Ben Noordhuis)

## 2012.06.11, Version 0.7.10 (unstable)

https://github.com/iojs/io.js/commit/12a32a48a30182621b3f8e9b9695d1946b53c131

* Roll V8 back to 3.9.24.31
* build: x64 target should always pass -m64 (Robert Mustacchi)
* add NODE_EXTERN to node::Start (Joel Brandt)
* repl: Warn about running npm commands (isaacs)
* slab_allocator: fix crash in dtor if V8 is dead (Ben Noordhuis)
* slab_allocator: fix leak of Persistent handles (Shigeki Ohtsu)
* windows/msi: add node.js prompt to startmenu (Jeroen Janssen)
* windows/msi: fix adding node to PATH (Jeroen Janssen)
* windows/msi: add start menu links when installing (Jeroen Janssen)
* windows: don't install x64 version into the 'program files (x86)' folder (Matt Gollob)
* domain: Fix [#3379](https://github.com/joyent/node/issues/3379) domain.intercept no longer passes error arg to cb (Marc Harter)
* fs: make callbacks run in global context (Ben Noordhuis)
* fs: enable fs.realpath on windows (isaacs)
* child_process: expose UV_PROCESS_DETACHED as options.detached (Charlie McConnell)
* child_process: new stdio API for .spawn() method (Fedor Indutny)
* child_process: spawn().ref() and spawn().unref() (Fedor Indutny)
* Upgrade npm to 1.1.25
	- Enable npm link on windows
	- Properly remove sh-shim on Windows
	- Abstract out registry client and logger

## 2012.05.28, Version 0.7.9 (unstable)

https://github.com/iojs/io.js/commit/782277f11a753ded831439ed826448c06fc0f356

* Upgrade V8 to 3.11.1
* Upgrade npm to 1.1.23
* uv: rework reference counting scheme (Ben Noordhuis)
* uv: add interface for joining external event loops (Bert Belder)
* repl, readline: Handle Ctrl+Z and SIGCONT better (Nathan Rajlich)
* fs: 64bit offsets for fs calls (Igor Zinkovsky)
* fs: add sync open flags 'rs' and 'rs+' (Kevin Bowman)
* windows: enable creating directory junctions with fs.symlink (Igor Zinkovsky, Bert Belder)
* windows: fix fs.lstat to properly detect symlinks. (Igor Zinkovsky)
* Fix [#3270](https://github.com/joyent/node/issues/3270) Escape url.parse delims (isaacs)
* http: make http.get() accept a URL (Adam Malcontenti-Wilson)
* Cleanup vm module memory leakage (Marcel Laverdet)
* Optimize writing strings with Socket.write (Bert Belder)
* add support for CESU-8 and UTF-16LE encodings (koichik)
* path: add path.sep to get the path separator. (Yi, EungJun)
* net, http: add backlog parameter to .listen() (Erik Dubbelboer)
* debugger: support mirroring Date objects (Fedor Indutny)
* addon: add AtExit() function (Ben Noordhuis)
* net: signal localAddress bind failure in connect (Brian Schroeder)
* util: handle non-string return value in .inspect() (Alex Kocharin)

## 2012.04.18, Version 0.7.8 (unstable)

https://github.com/iojs/io.js/commit/c2b47097c0b483552efc1947c6766fa1128600b6

* Upgrade V8 to 3.9.24.9
* Upgrade OpenSSL to 1.0.0f
* Upgrade npm to 1.1.18
* Show licenses in Binary installers
* Domains (isaacs)
* readline: rename "end" to "close" (Nathan Rajlich)
* tcp: make getsockname() return address family as string (Shigeki Ohtsu)
* http, https: fix .setTimeout() (ssuda)
* os: add cross platform EOL character (Mustansir Golawala)
* typed arrays: unexport SizeOfArrayElementForType() (Aaron Jacobs)
* net: honor 'enable' flag in .setNoDelay() (Ben Noordhuis)
* child_process: emit error when .kill fails (Andreas Madsen)
* gyp: fix 'argument list too long' build error (Ben Noordhuis)
* fs.WriteStream: Handle modifications to fs.open (isaacs)
* repl, readline: Handle newlines better (Nathan Rajlich, Nathan Friedly)
* build: target OSX 10.5 when building on darwin (Nathan Rajlich)
* Fix [#3052](https://github.com/joyent/node/issues/3052) Handle errors properly in zlib (isaacs)
* build: add support for DTrace and postmortem (Dave Pacheco)
* core: add reusable Slab allocator (Ben Noordhuis)

## 2012.03.30, Version 0.7.7 (unstable)

https://github.com/iojs/io.js/commit/5cda2542fdb086f9fe5de889bea435a65e377dea

* Upgrade V8 to 3.9.24.7
* Upgrade npm to 1.1.15
* Handle Emoji characters properly (Erik Corry, Bert Belder)
* readline: migrate ansi/vt100 logic from tty to readline (Nathan Rajlich)
* readline: Fix multiline handling (Alex Kocharin)
* add a -i/--interactive flag to force the REPL (Nathan Rajlich)
* debugger: add breakOnException command (Fedor Indutny)
* cluster: kill workers when master dies (Andreas Madsen)
* cluster: add graceful disconnect support (Andreas Madsen)
* child_process: Separate 'close' event from 'exit' (Charlie McConnell)
* typed arrays: add Uint8ClampedArray (Mikael Bourges-Sevenier)
* buffer: Fix byte alignment issues (Ben Noordhuis, Erik Lundin)
* tls: fix CryptoStream.setKeepAlive() (Shigeki Ohtsu)
* Expose http parse error codes (Felix Geisendörfer)
* events: don't delete the listeners array (Ben Noordhuis, Nathan Rajlich)
* process: add process.config to view node's ./configure settings (Nathan Rajlich)
* process: process.execArgv to see node's arguments (Micheil Smith)
* process: fix process.title setter (Ben Noordhuis)
* timers: handle negative or non-numeric timeout values (Ben Noordhuis)

## 2012.03.13, Version 0.7.6 (unstable)

https://github.com/iojs/io.js/commit/f06abda6f58e517349d1b63a2cbf5a8d04a03505

* Upgrade v8 to 3.9.17
* Upgrade npm to 1.1.8
  - Add support for os/cpu fields in package.json (Adam Blackburn)
  - Automatically node-gyp packages containing a binding.gyp
  - Fix failures unpacking in UNC shares
  - Never create un-listable directories
  - Handle cases where an optionalDependency fails to build

* events: newListener emit correct fn when using 'once' (Roly Fentanes)
* url: Ignore empty port component (Łukasz Walukiewicz)
* module: replace 'children' array (isaacs)
* tls: parse multiple values of a key in ssl certificate (Sambasiva Suda)
* cluster: support passing of named pipes (Ben Noordhuis)
* Windows: include syscall in fs errors (Bert Belder)
* http: [#2888](https://github.com/joyent/node/issues/2888) Emit end event only once (Igor Zinkovsky)
* readline: add multiline support (Rlidwka)
* process: add `process.hrtime()` (Nathan Rajlich)
* net, http, https: add localAddress option (Dmitry Nizovtsev)
* addon improvements (Nathan Rajlich)
* build improvements (Ben Noordhuis, Sadique Ali, T.C. Hollingsworth, Nathan Rajlich)
* add support for "SEARCH" request methods (Nathan Rajlich)
* expose the zlib and http_parser version in process.versions (Nathan Rajlich)

## 2012.02.23, Version 0.7.5 (unstable)

https://github.com/iojs/io.js/commit/d384b8b0d2ab7f05465f0a3e15fe20b4e25b5f86

* startup speed improvements (Maciej Małecki)
* crypto: add function getDiffieHellman() (Tomasz Buchert)
* buffer: support decoding of URL-safe base64 (Ben Noordhuis)
* Make QueryString.parse() even faster (Brian White)
* url: decode url entities in auth section (Ben Noordhuis)
* http: support PURGE request method (Ben Noordhuis)
* http: Generate Date headers on responses (Mark Nottingham)
* Fix [#2762](https://github.com/joyent/node/issues/2762): Add callback to close function. (Mikeal Rogers)
* dgram: fix out-of-bound memory read (Ben Noordhuis)
* repl: add automatic loading of built-in libs (Brandon Benvie)
* repl: remove double calls where possible (Fedor Indutny)
* Readline improvements. Related: [#2737](https://github.com/joyent/node/issues/2737) [#2756](https://github.com/joyent/node/issues/2756) (Colton Baker)
* build: disable -fomit-frame-pointer on solaris (Dave Pacheco)
* build: arch detection improvements (Nathan Rajlich)
* build: Make a fat binary for the OS X `make pkg`. (Nathan Rajlich)
* jslint src/ and lib/ on 'make test' (isaacs)

## 2012.02.14, Version 0.7.4 (unstable)

https://github.com/iojs/io.js/commit/de21de920cf93ec40736ada3792a7f85f3eadeda

* Upgrade V8 to 3.9.5
* Upgrade npm to 1.1.1
* build: Detect host_arch better (Karl Skomski)
* debugger: export `debug_port` to `process` (Fedor Indutny)
* api docs: CSS bug fixes (isaacs)
* build: use -fPIC for native addons on UNIX (Nathan Rajlich)
* Re-add top-level v8::Locker (Marcel Laverdet)
* Move images out of the dist tarballs (isaacs)
* libuv: Remove uv_export and uv_import (Ben Noordhuis)
* build: Support x64 build on Windows (Igor Zinkovsky)

## 2012.02.07, Version 0.7.3 (unstable)

https://github.com/iojs/io.js/commit/99059aad8d654acda4abcfaa68df182b50f2ec90

* Upgrade V8 to 3.9.2
* Revert support for isolates. (Ben Noordhuis)
* cluster: Cleanup docs, event handling, and process.disconnect (Andreas Madsen)
* gyp_addon: link with node.lib on Windows (Nathan Rajlich)
* http: fix case where http-parser is freed twice (koichik)
* Windows: disable RTTI and exceptions (Bert Belder)

## 2012.02.01, Version 0.7.2 (unstable)

https://github.com/iojs/io.js/commit/ec79acb3a6166e30f0bf271fbbfda1fb575b3321

* Update V8 to 3.8.9
* Support for sharing streams across Isolates (Igor Zinkovsky)
* [#2636](https://github.com/joyent/node/issues/2636) - Fix case where http_parsers are freed too early (koichik)
* url: Support for IPv6 addresses in URLs (Łukasz Walukiewicz)
* child_process: Add disconnect() method to child processes (Andreas Madsen)
* fs: add O_EXCL support, exclusive open file (Ben Noordhuis)
* fs: more specific error messages (Tj Holowaychuk)
* tty: emit 'unknown' key event if key sequence not found (Dan VerWeire, Nathan Rajlich)
* build: compile release build too if BUILDTYPE=Debug (Ben Noordhuis)
* module: fix --debug-brk on symlinked scripts (Fedor Indutny)
* zlib: fix `Failed to set dictionary` issue (Fedor Indutny)
* waf: predict target arch for OS X (Fedor Indutny)

## 2012.01.23, Version 0.7.1 (unstable)

https://github.com/iojs/io.js/commit/a74354735ab5d5b0fa35a1e4ff7e653757d2069b

* Update V8 to 3.8.8
* Install node-waf by default (Fedor Indutny)
* crypto: Add ability to turn off PKCS padding (Ingmar Runge)
* v8: implement VirtualMemory class on SunOS (Ben Noordhuis)
* Add cluster.setupMaster (Andreas Madsen)
* move `path.exists*` to `fs.exists*` (Maciej Małecki)
* typed arrays: set class name (Ben Noordhuis)
* libuv bug fixes (Igor Zinkovsky, Ben Noordhuis, Dan VerWeire)

## 2012.01.16, Version 0.7.0 (unstable)

https://github.com/iojs/io.js/commit/9cc55dca6f67a6096c858b841c677b0593404321

* Upgrade V8 to 3.8.6
* Use GYP build system on unix (Ben Noordhuis)
* Experimenetal isolates support (Ben Noordhuis)
* Improvements to Cluster API (Andreas Madsen)
* Use isolates for internal debugger (Fedor Indutny)
* Bug fixes

## 2012.07.10 Version 0.6.20 (maintenance)

https://github.com/iojs/io.js/commit/952e513379169ec1b40909d1db056e9bf4294899

* npm: Upgrade to 1.1.37 (isaacs)
* benchmark: Backport improvements made in master (isaacs)
* build: always link with -lz (Trent Mick)
* core: use proper #include directives (Ben Noordhuis)
* cluster: don't silently drop messages when the write queue gets big (Bert Belder)
* windows: don't print error when GetConsoleTitleW returns an empty string (Bert Belder)

## 2012.06.06 Version 0.6.19 (stable)

https://github.com/iojs/io.js/commit/debf552ed2d4a53957446e82ff3c52a8182d5ff4

* npm: upgrade to 1.1.24
* fs: no end emit after createReadStream.pause() (Andreas Madsen)
* vm: cleanup module memory leakage (Marcel Laverdet)
* unix: fix loop starvation under high network load (Ben Noordhuis)
* unix: remove abort() in ev_unref() (Ben Noordhuis)
* windows/tty: never report error after forcibly aborting line-buffered read (Bert Belder)
* windows: skip GetFileAttributes call when opening a file (Bert Belder)

## 2012.05.15 Version 0.6.18 (stable)

https://github.com/iojs/io.js/commit/4bc1d395de6abed2cf1e4d0b7b3a1480a21c368f

* windows: skip GetFileAttributes call when opening a file (Bert Belder)
* crypto: add PKCS12/PFX support (Sambasiva Suda)
* [#3240](https://github.com/joyent/node/issues/3240): child_process: delete NODE_CHANNEL_FD from env in spawn (Ben Noordhuis)
* windows: add test for path.normalize with UNC paths (Bert Belder)
* windows: make path.normalize convert all slashes to backslashes (Bert Belder)
* fs: Automatically close FSWatcher on error (Bert Belder)
* [#3258](https://github.com/joyent/node/issues/3258): fs.ReadStream.pause() emits duplicate data event (koichik)
* pipe_wrap: don't assert() on pipe accept errors (Ben Noordhuis)
* Better exception output for module load and process.nextTick (Felix Geisendörfer)
* zlib: fix error reporting (Ben Noordhuis)
* http: Don't destroy on timeout (isaacs)
* [#3231](https://github.com/joyent/node/issues/3231): http: Don't try to emit error on a null'ed req object (isaacs)
* [#3236](https://github.com/joyent/node/issues/3236): http: Refactor ClientRequest.onSocket (isaacs)

## 2012.05.04 Version 0.6.17 (stable)

https://github.com/iojs/io.js/commit/4ced23deaf36493f4303a18f6fdce768c58becc0

* Upgrade npm to 1.1.21
* uv: Add support for EROFS errors (Ben Noordhuis, Maciej Małecki)
* uv: Add support for EIO and ENOSPC errors (Fedor Indutny)
* windows: Add support for EXDEV errors (Bert Belder)
* http: Fix client memory leaks (isaacs, Vincent Voyer)
* fs: fix file descriptor leak in sync functions (Ben Noordhuis)
* fs: fix ReadStream / WriteStream double close bug (Ben Noordhuis)

## 2012.04.30 Version 0.6.16 (stable)

https://github.com/iojs/io.js/commit/a1d193963ddc80a27da5da01b59751e14e33d1d6

* Upgrade V8 to 3.6.6.25
* Upgrade npm to 1.1.19
* Windows: add mappings for UV_ENOENT (Bert Belder)
* linux: add IN_MOVE_SELF to inotify event mask (Ben Noordhuis)
* unix: call pipe handle connection cb on accept() error (Ben Noordhuis)
* unix: handle EWOULDBLOCK (Ben Noordhuis)
* map EWOULDBLOCK to UV_EAGAIN (Ben Noordhuis)
* Map ENOMEM to UV_ENOMEM (isaacs)
* Child process: support the `gid` and `uid` options (Bert Belder)
* test: cluster: add worker death event test (Ben Noordhuis)
* typo in node_http_parser (isaacs)
* http_parser: Eat CRLF between requests, even on connection:close. (Ben Noordhuis)
* don't check return value of unsetenv (Ben Noordhuis)

## 2012.04.09 Version 0.6.15 (stable)

https://github.com/iojs/io.js/commit/f160a45b254e591eb33716311c92be533c6d86c4

* Update npm to 1.1.16
* Show licenses in binary installers.
* unix: add uv_fs_read64, uv_fs_write64 and uv_fs_ftruncate64 (Ben Noordhuis)
* add 64bit offset fs functions (Igor Zinkovsky)
* windows: don't report ENOTSOCK when attempting to bind an udp handle twice (Bert Belder)
* windows: backport pipe-connect-to-file fixes from master (Bert Belder)
* windows: never call fs event callbacks after closing the watcher (Bert Belder)
* fs.readFile: don't make the callback before the fd is closed (Bert Belder)
* windows: use 64bit offsets for uv_fs apis (Igor Zinkovsky)
* Fix [#2061](https://github.com/joyent/node/issues/2061): segmentation fault on OS X due to stat size mismatch (Ben Noordhuis)

## 2012.03.22 Version 0.6.14 (stable)

https://github.com/iojs/io.js/commit/e513ffef7549a56a5af728e1f0c2c0c8f290518a

* net: don't crash when queued write fails (Igor Zinkovsky)
* sunos: fix EMFILE on process.memoryUsage() (Bryan Cantrill)
* crypto: fix compile-time error with openssl 0.9.7e (Ben Noordhuis)
* unix: ignore ECONNABORTED errors from accept() (Ben Noordhuis)
* Add UV_ENOSPC and mappings to it (Bert Belder)
* http-parser: Fix response body is not read (koichik)
* Upgrade npm to 1.1.12
  - upgrade node-gyp to 0.3.7
  - work around AV-locked directories on Windows
  - Fix isaacs/npm[#2293](https://github.com/joyent/node/issues/2293) Don't try to 'uninstall' /
  - Exclude symbolic links from packages.
  - Fix isaacs/npm[#2275](https://github.com/joyent/node/issues/2275) Spurious 'unresolvable cycle' error.
  - Exclude/include dot files as if they were normal files

## 2012.03.15 Version 0.6.13 (stable)

https://github.com/iojs/io.js/commit/9f7f86b534f8556290eb8cad915984ff4ca54996

* Windows: Many libuv test fixes (Bert Belder)
* Windows: avoid uv_guess_handle crash in when fd < 0 (Bert Belder)
* Map EBUSY and ENOTEMPTY errors (Bert Belder)
* Windows: include syscall in fs errors (Bert Belder)
* Fix fs.watch ENOSYS on Linux kernel version mismatch (Ben Noordhuis)
* Update npm to 1.1.9
  - upgrade node-gyp to 0.3.5 (Nathan Rajlich)
  - Fix isaacs/npm[#2249](https://github.com/joyent/node/issues/2249) Add cache-max and cache-min configs
  - Properly redirect across https/http registry requests
  - log config usage if undefined key in set function (Kris Windham)
  - Add support for os/cpu fields in package.json (Adam Blackburn)
  - Automatically node-gyp packages containing a binding.gyp
  - Fix failures unpacking in UNC shares
  - Never create un-listable directories
  - Handle cases where an optionalDependency fails to build

## 2012.03.02 Version 0.6.12 (stable)

https://github.com/iojs/io.js/commit/48a2d34cfe6b7e1c9d15202a4ef5e3c82d1fba35

* Upgrade V8 to 3.6.6.24
* dtrace ustack helper improvements (Dave Pacheco)
* API Documentation refactor (isaacs)
* [#2827](https://github.com/joyent/node/issues/2827) net: fix race write() before and after connect() (koichik)
* [#2554](https://github.com/joyent/node/issues/2554) [#2567](https://github.com/joyent/node/issues/2567) throw if fs args for 'start' or 'end' are strings (AJ ONeal)
* punycode: Update to v1.0.0 (Mathias Bynens)
* Make a fat binary for the OS X pkg (isaacs)
* Fix hang on accessing process.stdin (isaacs)
* repl: make tab completion work on non-objects (Nathan Rajlich)
* Fix fs.watch on OS X (Ben Noordhuis)
* Fix [#2515](https://github.com/joyent/node/issues/2515) nested setTimeouts cause premature process exit (Ben Noordhuis)
* windows: fix time conversion in stat (Igor Zinkovsky)
* windows: fs: handle EOF in read (Brandon Philips)
* windows: avoid IOCP short-circuit on non-ifs lsps (Igor Zinkovsky)
* Upgrade npm to 1.1.4 (isaacs)
  - windows fixes
  - Bundle nested bundleDependencies properly
  - install: support --save with url install targets
  - shrinkwrap: behave properly with url-installed modules
  - support installing uncompressed tars or single file modules from urls etc.
  - don't run make clean on rebuild
  - support HTTPS-over-HTTP proxy tunneling

## 2012.02.17 Version 0.6.11 (stable)

https://github.com/iojs/io.js/commit/1eb1fe32250fc88cb5b0a97cddf3e02be02e3f4a

* http: allow multiple WebSocket RFC6455 headers (Einar Otto Stangvik)
* http: allow multiple WWW-Authenticate headers (Ben Noordhuis)
* windows: support unicode argv and environment variables (Bert Belder)
* tls: mitigate session renegotiation attacks (Ben Noordhuis)
* tcp, pipe: don't assert on uv_accept() errors (Ben Noordhuis)
* tls: Allow establishing secure connection on the existing socket (koichik)
* dgram: handle close of dgram socket before DNS lookup completes (Seth Fitzsimmons)
* windows: Support half-duplex pipes (Igor Zinkovsky)
* build: disable omit-frame-pointer on solaris systems (Dave Pacheco)
* debugger: fix --debug-brk (Ben Noordhuis)
* net: fix large file downloads failing (koichik)
* fs: fix ReadStream failure to read from existing fd (Christopher Jeffrey)
* net: destroy socket on DNS error (Stefan Rusu)
* dtrace: add missing translator (Dave Pacheco)
* unix: don't flush tty on switch to raw mode (Ben Noordhuis)
* windows: reset brightness when reverting to default text color (Bert Belder)
* npm: update to 1.1.1
  - Update which, fstream, mkdirp, request, and rimraf
  - Fix [#2123](https://github.com/joyent/node/issues/2123) Set path properly for lifecycle scripts on windows
  - Mark the root as seen, so we don't recurse into it. Fixes [#1838](https://github.com/joyent/node/issues/1838). (Martin Cooper)

## 2012.02.02, Version 0.6.10 (stable)

https://github.com/iojs/io.js/commit/051908e023f87894fa68f5b64d0b99a19a7db01e

* Update V8 to 3.6.6.20
* Add npm msysgit bash shim to msi installer (isaacs)
* buffers: fix intermittent out of bounds error (Ben Noordhuis)
* buffers: honor length argument in base64 decoder (Ben Noordhuis)
* windows: Fix path.exists regression (Bert Belder)
* Make QueryString.parse run faster (Philip Tellis)
* http: avoid freeing http-parser objects too early (koichik)
* timers: add v0.4 compatibility hack (Ben Noordhuis)
* Proper EPERM error code support (Igor Zinkovsky, Brandon Philips)
* dgram: Implement udp multicast methods on windows (Bert Belder)

## 2012.01.27, Version 0.6.9 (stable)

https://github.com/iojs/io.js/commit/f19e20d33f57c4d2853aaea7d2724d44f3b0012f

* dgram: Bring back missing functionality for Unix (Dan VerWeire, Roman Shtylman, Ben Noordhuis)
  - Note: Windows UDP support not yet complete.

* http: Fix parser memory leak (koichik)
* zlib: Fix [#2365](https://github.com/joyent/node/issues/2365) crashes on invalid input (Nicolas LaCasse)
* module: fix --debug-brk on symlinked scripts (Fedor Indutny)
* Documentation Restyling (Matthew Fitzsimmons)
* Update npm to 1.1.0-3 (isaacs)
* Windows: fix regression in stat() calls to C:\ (Bert Belder)

## 2012.01.19, Version 0.6.8 (stable)

https://github.com/iojs/io.js/commit/d18cebaf8a7ac701dabd71a3aa4eb0571db6a645

* Update V8 to 3.6.6.19
* Numeric key hash collision fix for V8 (Erik Corry, Fedor Indutny)
* Add missing TTY key translations for F1-F5 on Windows (Brandon Benvie)
* path.extname bugfix with . and .. paths (Bert Belder)
* cluster: don't always kill the master on uncaughtException (Ben Noordhuis)
* Update npm to 1.1.0-2 (isaacs)
* typed arrays: set class name (Ben Noordhuis)
* zlib binding cleanup (isaacs, Bert Belder)
* dgram: use slab memory allocator (Michael Bernstein)
* fix segfault [#2473](https://github.com/joyent/node/issues/2473)
* [#2521](https://github.com/joyent/node/issues/2521) 60% improvement in fs.stat on Windows (Igor Zinkovsky)

## 2012.01.06, Version 0.6.7 (stable)

https://github.com/iojs/io.js/commit/d5a189acef14a851287ee555f7a39431fe276e1c

* V8 hash collision fix (Breaks MIPS) (Bert Belder, Erik Corry)
* Upgrade V8 to 3.6.6.15
* Upgrade npm to 1.1.0-beta-10 (isaacs)
* many doc updates (Ben Noordhuis, Jeremy Martin, koichik, Dave Irvine,
  Seong-Rak Choi, Shannen, Adam Malcontenti-Wilson, koichik)

* Fix segfault in node_http_parser.cc
* dgram, timers: fix memory leaks (Ben Noordhuis, Yoshihiro Kikuchi)
* repl: fix repl.start not passing the `ignoreUndefined` arg (Damon Oehlman)
* [#1980](https://github.com/joyent/node/issues/1980): Socket.pause null reference when called on a closed Stream (koichik)
* [#2263](https://github.com/joyent/node/issues/2263): XMLHttpRequest piped in a writable file stream hang (koichik)
* [#2069](https://github.com/joyent/node/issues/2069): http resource leak (koichik)
* buffer.readInt global pollution fix (Phil Sung)
* timers: fix performance regression (Ben Noordhuis)
* [#2308](https://github.com/joyent/node/issues/2308), [#2246](https://github.com/joyent/node/issues/2246): node swallows openssl error on request (koichik)
* [#2114](https://github.com/joyent/node/issues/2114): timers: remove _idleTimeout from item in .unenroll() (James Hartig)
* [#2379](https://github.com/joyent/node/issues/2379): debugger: Request backtrace w/o refs (Fedor Indutny)
* simple DTrace ustack helper (Dave Pacheco)
* crypto: rewrite HexDecode without snprintf (Roman Shtylman)
* crypto: don't ignore DH init errors (Ben Noordhuis)

## 2011.12.14, Version 0.6.6 (stable)

https://github.com/iojs/io.js/commit/9a059ea69e1f6ebd8899246682d8ca257610b8ab

* npm update to 1.1.0-beta-4 (Isaac Z. Schlueter)
* cli: fix output of --help (Ben Noordhuis)
* new website
* pause/resume semantics for stdin (Isaac Z. Schlueter)
* Travis CI integration (Maciej Małecki)
* child_process: Fix bug regarding closed stdin (Ben Noordhuis)
* Enable upgrades in MSI. (Igor Zinkovsky)
* net: Fixes memory leak (Ben Noordhuis)
* fs: handle fractional or NaN ReadStream buffer size (Ben Noordhuis)
* crypto: fix memory leaks in PBKDF2 error path (Ben Noordhuis)

## 2011.12.04, Version 0.6.5 (stable)

https://github.com/iojs/io.js/commit/6cc94db653a2739ab28e33b2d6a63c51bd986a9f

* npm workaround Windows antivirus software (isaacs)
* Upgrade V8 to 3.6.6.11

## 2011.12.02, Version 0.6.4 (stable)

https://github.com/iojs/io.js/commit/9170077f13e5e5475b23d1d3c2e7f69bfe139727

* doc improvements (Kyle Young, Tim Oxley, Roman Shtylman, Mathias Bynens)
* upgrade bundled npm (Isaac Schlueter)
* polish Windows installer (Igor Zinkovsky, Isaac Schlueter)
* punycode: upgrade to v0.2.1 (Mathias Bynens)
* build: add –without-npm flag to configure script
* sys: deprecate module some more, print stack trace if NODE_DEBUG=sys
* cli: add -p switch, prints result of –eval
* [#1997](https://github.com/joyent/node/issues/1997): fix Blowfish ECB encryption and decryption (Ingmar Runge)
* [#2223](https://github.com/joyent/node/issues/2223): fix socket ‘close’ event being emitted twice
* [#2224](https://github.com/joyent/node/issues/2224): fix RSS memory usage > 4 GB reporting (Russ Bradberry)
* [#2225](https://github.com/joyent/node/issues/2225): fix util.inspect() object stringification bug (Nathan Rajlich)

## 2011.11.25, Version 0.6.3 (stable)

https://github.com/iojs/io.js/commit/b159c6d62e5756d3f8847419d29c6959ea288b56

* [#2083](https://github.com/joyent/node/issues/2083) Land NPM in Node. It is included in packages/installers and installed
  on `make install`.

* [#2076](https://github.com/joyent/node/issues/2076) Add logos to windows installer.
* [#1711](https://github.com/joyent/node/issues/1711) Correctly handle http requests without headers. (Ben Noordhuis,
  Felix Geisendörfer)

* TLS: expose more openssl SSL context options and constants. (Ben Noordhuis)
* [#2177](https://github.com/joyent/node/issues/2177) Windows: don't kill UDP socket when a packet fails to reach its
  destination. (Bert Belder)

* Windows: support paths longer than 260 characters. (Igor Zinkovsky)
* Windows: correctly resolve drive-relative paths. (Bert Belder)
* [#2166](https://github.com/joyent/node/issues/2166) Don't leave file descriptor open after lchmod. (Isaac Schlueter)
* [#2084](https://github.com/joyent/node/issues/2084) Add OS X .pkg build script to make file.
* [#2160](https://github.com/joyent/node/issues/2160) Documentation improvements. (Ben Noordhuis)

## 2011.11.18, Version 0.6.2 (stable)

https://github.com/iojs/io.js/commit/a4402f0b2e410b19375a1d5c5fb7fe7f66f3c7f8

* doc improvements (Artur Adib, Trevor Burnham, Ryan Emery, Trent Mick)
* timers: remember extra setTimeout() arguments when timeout==0
* punycode: use Mathias Bynens's punycode library, it's more compliant
* repl: improved tab completion (Ryan Emery)
* buffer: fix range checks in .writeInt() functions (Lukasz Walukiewicz)
* tls: make cipher list configurable
* addons: make Buffer and ObjectWrap visible to Windows add-ons (Bert Belder)
* crypto: add PKCS[#1](https://github.com/joyent/node/issues/1) a.k.a RSA public key verification support
* windows: fix stdout writes when redirected to nul
* sunos: fix build on Solaris and Illumos
* Upgrade V8 to 3.6.6.8

## 2011.11.11, Version 0.6.1 (stable)

https://github.com/iojs/io.js/commit/170f2addb2dd0c625bc4a6d461e89a31ad68b79b

* doc improvements (Eric Lovett, Ben Noordhuis, Scott Anderson, Yoji SHIDARA)
* crypto: make thread-safe (Ben Noordhuis)
* fix process.kill error object
* debugger: correctly handle source with multi-byte characters (Shigeki Ohtsu)
* make stdout and stderr non-destroyable (Igor Zinkovsky)
* fs: don't close uninitialized fs.watch handle (Ben Noordhuis)
* [#2026](https://github.com/joyent/node/issues/2026) fix man page install on BSDs (Ben Noordhuis)
* [#2040](https://github.com/joyent/node/issues/2040) fix unrecognized errno assert in uv_err_name
* [#2043](https://github.com/joyent/node/issues/2043) fs: mkdir() should call callback if mode is omitted
* [#2045](https://github.com/joyent/node/issues/2045) fs: fix fs.realpath on windows to return on error (Benjamin Pasero)
* [#2047](https://github.com/joyent/node/issues/2047) minor cluster improvements
* [#2052](https://github.com/joyent/node/issues/2052) readline get window columns correctly
* Upgrade V8 to 3.6.6.7

## 2011.11.04, Version 0.6.0 (stable)

https://github.com/iojs/io.js/commit/865b077819a9271a29f982faaef99dc635b57fbc

* print undefined on undefined values in REPL (Nathan Rajlich)
* doc improvements (koichik, seebees, bnoordhuis,
  Maciej Małecki, Jacob Kragh)

* support native addon loading in windows (Bert Belder)
* rename getNetworkInterfaces() to networkInterfaces() (bnoordhuis)
* add pending accepts knob for windows (igorzi)
* http.request(url.parse(x)) (seebees)
* [#1929](https://github.com/joyent/node/issues/1929) zlib Respond to 'resume' events properly (isaacs)
* stream.pipe: Remove resume and pause events
* test fixes for windows (igorzi)
* build system improvements (bnoordhuis)
* [#1936](https://github.com/joyent/node/issues/1936) tls: does not emit 'end' from EncryptedStream (koichik)
* [#758](https://github.com/joyent/node/issues/758) tls: add address(), remoteAddress/remotePort
* [#1399](https://github.com/joyent/node/issues/1399) http: emit Error object after .abort() (bnoordhuis)
* [#1999](https://github.com/joyent/node/issues/1999) fs: make mkdir() default to 0777 permissions (bnoordhuis)
* [#2001](https://github.com/joyent/node/issues/2001) fix pipe error codes
* [#2002](https://github.com/joyent/node/issues/2002) Socket.write should reset timeout timer
* stdout and stderr are blocking when associated with file too.
* remote debugger support on windows (Bert Belder)
* convenience methods for zlib (Matt Robenolt)
* process.kill support on windows (igorzi)
* process.uptime() support on windows (igorzi)
* Return IPv4 addresses before IPv6 addresses from getaddrinfo
* util.inspect improvements (Nathan Rajlich)
* cluster module api changes
* Downgrade V8 to 3.6.6.6

## 2011.10.21, Version 0.5.10 (unstable)

https://github.com/iojs/io.js/commit/220e61c1f65bf4db09699fcf6399c0809c0bc446

* Remove cmake build system, support for Cygwin, legacy code base,
	process.ENV, process.ARGV, process.memoryUsage().vsize, os.openOSHandle

* Documentation improvments (Igor Zinkovsky, Bert Belder, Ilya Dmitrichenko,
koichik, Maciej Małecki, Guglielmo Ferri, isaacs)

* Performance improvements (Daniel Ennis, Bert Belder, Ben Noordhuis)
* Long process.title support (Ben Noordhuis)
* net: register net.Server callback only once (Simen Brekken)
* net: fix connect queue bugs (Ben Noordhuis)
* debugger: fix backtrace err handling (Fedor Indutny)
* Use getaddrinfo instead of c-ares for dns.lookup
* Emit 'end' from crypto streams on close
* [#1902](https://github.com/joyent/node/issues/1902) buffer: use NO_NULL_TERMINATION flag (koichik)
* [#1907](https://github.com/joyent/node/issues/1907) http: Added support for HTTP PATCH verb (Thomas Parslow)
* [#1644](https://github.com/joyent/node/issues/1644) add GetCPUInfo on windows (Karl Skomski)
* [#1484](https://github.com/joyent/node/issues/1484), [#1834](https://github.com/joyent/node/issues/1834), [#1482](https://github.com/joyent/node/issues/1482), [#771](https://github.com/joyent/node/issues/771) Don't use a separate context for the repl.
  (isaacs)

* [#1882](https://github.com/joyent/node/issues/1882) zlib Update 'availOutBefore' value, and test (isaacs)
* [#1888](https://github.com/joyent/node/issues/1888) child_process.fork: don't modify args (koichik)
* [#1516](https://github.com/joyent/node/issues/1516) tls: requestCert unusable with Firefox and Chrome (koichik)
* [#1467](https://github.com/joyent/node/issues/1467) tls: The TLS API is inconsistent with the TCP API (koichik)
* [#1894](https://github.com/joyent/node/issues/1894) net: fix error handling in listen() (koichik)
* [#1860](https://github.com/joyent/node/issues/1860) console.error now goes through uv_tty_t
* Upgrade V8 to 3.7.0
* Upgrade GYP to r1081

## 2011.10.10, Version 0.5.9 (unstable)

https://github.com/iojs/io.js/commit/3bd9b08fb125b606f97a4079b147accfdeebb07d

* fs.watch interface backed by kqueue, inotify, and ReadDirectoryChangesW
  (Igor Zinkovsky, Ben Noordhuis)

* add dns.resolveTxt (Christian Tellnes)
* Remove legacy http library (Ben Noordhuis)
* child_process.fork returns and works on Windows. Allows passing handles.
  (Igor Zinkovsky, Bert Belder)

* [#1774](https://github.com/joyent/node/issues/1774) Lint and clean up for --harmony_block_scoping (Tyler Larson, Colton
  Baker)

* [#1813](https://github.com/joyent/node/issues/1813) Fix ctrl+c on Windows (Bert Belder)
* [#1844](https://github.com/joyent/node/issues/1844) unbreak --use-legacy (Ben Noordhuis)
* process.stderr now goes through libuv. Both process.stdout and
  process.stderr are blocking when referencing a TTY.

* net_uv performance improvements (Ben Noordhuis, Bert Belder)

## 2011.09.30, Version 0.5.8 (unstable)

https://github.com/iojs/io.js/commit/7cc17a0cea1d25188c103745a7d0c24375e3a609

* zlib bindings (isaacs)
* Windows supports TTY ANSI escape codes (Bert Belder)
* Debugger improvements (Fedor Indutny)
* crypto: look up SSL errors with ERR_print_errors() (Ben Noordhuis)
* dns callbacks go through MakeCallback now
* Raise an error when a malformed package.json file is found. (Ben Leslie)
* buffers: handle bad length argument in constructor (Ben Noordhuis)
* [#1726](https://github.com/joyent/node/issues/1726), unref process.stdout
* Doc improvements (Ben Noordhuis, Fedor Indutny, koichik)
* Upgrade libuv to fe18438

## 2011.09.16, Version 0.5.7 (unstable)

https://github.com/iojs/io.js/commit/558241166c4f3c516e5a448e676db0b57119212f

* Upgrade V8 to 3.6.4
* Improve Windows compatibility
* Documentation improvements
* Debugger and REPL improvements (Fedor Indutny)
* Add legacy API support: net.Stream(fd), process.stdout.writable,
  process.stdout.fd

* Fix mkdir EEXIST handling (isaacs)
* Use net_uv instead of net_legacy for stdio
* Do not load readline from util.inspect
* [#1673](https://github.com/joyent/node/issues/1673) Fix bug related to V8 context with accessors (Fedor Indutny)
* [#1634](https://github.com/joyent/node/issues/1634) util: Fix inspection for Error (koichik)
* [#1645](https://github.com/joyent/node/issues/1645) fs: Add positioned file writing feature to fs.WriteStream (Thomas
  Shinnick)

* [#1637](https://github.com/joyent/node/issues/1637) fs: Unguarded fs.watchFile cache statWatchers checking fixed (Thomas
  Shinnick)

* [#1695](https://github.com/joyent/node/issues/1695) Forward customFds to ChildProcess.spawn
* [#1707](https://github.com/joyent/node/issues/1707) Fix hasOwnProperty security problem in querystring (isaacs)
* [#1719](https://github.com/joyent/node/issues/1719) Drain OpenSSL error queue

## 2011.09.08, Version 0.5.6 (unstable)

https://github.com/iojs/io.js/commit/b49bec55806574a47403771bce1ee379c2b09ca2

* [#345](https://github.com/joyent/node/issues/345), [#1635](https://github.com/joyent/node/issues/1635), [#1648](https://github.com/joyent/node/issues/1648) Documentation improvements (Thomas Shinnick,
  Abimanyu Raja, AJ ONeal, Koichi Kobayashi, Michael Jackson, Logan Smyth,
  Ben Noordhuis)

* [#650](https://github.com/joyent/node/issues/650) Improve path parsing on windows (Bert Belder)
* [#752](https://github.com/joyent/node/issues/752) Remove headers sent check in OutgoingMessage.getHeader()
  (Peter Lyons)

* [#1236](https://github.com/joyent/node/issues/1236), [#1438](https://github.com/joyent/node/issues/1438), [#1506](https://github.com/joyent/node/issues/1506), [#1513](https://github.com/joyent/node/issues/1513), [#1621](https://github.com/joyent/node/issues/1621), [#1640](https://github.com/joyent/node/issues/1640), [#1647](https://github.com/joyent/node/issues/1647) Libuv-related bugs fixed
  (Jorge Chamorro Bieling, Peter Bright, Luis Lavena, Igor Zinkovsky)

* [#1296](https://github.com/joyent/node/issues/1296), [#1612](https://github.com/joyent/node/issues/1612) crypto: Fix BIO's usage. (Koichi Kobayashi)
* [#1345](https://github.com/joyent/node/issues/1345) Correctly set socket.remoteAddress with libuv backend (Bert Belder)
* [#1429](https://github.com/joyent/node/issues/1429) Don't clobber quick edit mode on windows (Peter Bright)
* [#1503](https://github.com/joyent/node/issues/1503) Make libuv backend default on unix, override with `node --use-legacy`
* [#1565](https://github.com/joyent/node/issues/1565) Fix fs.stat for paths ending with \ on windows (Igor Zinkovsky)
* [#1568](https://github.com/joyent/node/issues/1568) Fix x509 certificate subject parsing (Koichi Kobayashi)
* [#1586](https://github.com/joyent/node/issues/1586) Make socket write encoding case-insensitive (Koichi Kobayashi)
* [#1591](https://github.com/joyent/node/issues/1591), [#1656](https://github.com/joyent/node/issues/1656), [#1657](https://github.com/joyent/node/issues/1657) Implement fs in libuv, remove libeio and pthread-win32
  dependency on windows (Igor Zinkovsky, Ben Noordhuis, Ryan Dahl,
  Isaac Schlueter)

* [#1592](https://github.com/joyent/node/issues/1592) Don't load-time link against CreateSymbolicLink on windows
  (Peter Bright)

* [#1601](https://github.com/joyent/node/issues/1601) Improve API consistency when dealing with the socket underlying a HTTP
  client request (Mikeal Rogers)

* [#1610](https://github.com/joyent/node/issues/1610) Remove DigiNotar CA from trusted list (Isaac Schlueter)
* [#1617](https://github.com/joyent/node/issues/1617) Added some win32 os functions (Karl Skomski)
* [#1624](https://github.com/joyent/node/issues/1624) avoid buffer overrun with 'binary' encoding (Koichi Kobayashi)
* [#1633](https://github.com/joyent/node/issues/1633) make Buffer.write() always set _charsWritten (Koichi Kobayashi)
* [#1644](https://github.com/joyent/node/issues/1644) Windows: set executables to be console programs (Peter Bright)
* [#1651](https://github.com/joyent/node/issues/1651) improve inspection for sparse array (Koichi Kobayashi)
* [#1672](https://github.com/joyent/node/issues/1672) set .code='ECONNRESET' on socket hang up errors (Ben Noordhuis)
* Add test case for foaf+ssl client certificate (Niclas Hoyer)
* Added RPATH environment variable to override run-time library paths
  (Ashok Mudukutore)

* Added TLS client-side session resumption support (Sean Cunningham)
* Added additional properties to getPeerCertificate (Nathan Rixham,
  Niclas Hoyer)

* Don't eval repl command twice when an error is thrown (Nathan Rajlich)
* Improve util.isDate() (Nathan Rajlich)
* Improvements in libuv backend and bindings, upgrade libuv to
  bd6066cb349a9b3a1b0d87b146ddaee06db31d10

* Show warning when using lib/sys.js (Maciej Malecki)
* Support plus sign in url protocol (Maciej Malecki)
* Upgrade V8 to 3.6.2

## 2011.08.26, Version 0.5.5 (unstable)

https://github.com/iojs/io.js/commit/d2d53d4bb262f517a227cc178a1648094ba54c20

* typed arrays, implementation from Plesk
* fix IP multicast on SunOS
* fix DNS lookup order: IPv4 first, IPv6 second (--use-uv only)
* remove support for UNIX datagram sockets (--use-uv only)
* UDP support for Windows (Bert Belder)
* [#1572](https://github.com/joyent/node/issues/1572) improve tab completion for objects in the REPL (Nathan Rajlich)
* [#1563](https://github.com/joyent/node/issues/1563) fix buffer overflow in child_process module (reported by Dean McNamee)
* [#1546](https://github.com/joyent/node/issues/1546) fix performance regression in http module (reported by Brian Geffon)
* [#1491](https://github.com/joyent/node/issues/1491) add PBKDF2 crypto support (Glen Low)
* [#1447](https://github.com/joyent/node/issues/1447) remove deprecated http.cat() function (Mikeal Rogers)
* [#1140](https://github.com/joyent/node/issues/1140) fix incorrect dispatch of vm.runInContext's filename argument
  (Antranig Basman)

* [#1140](https://github.com/joyent/node/issues/1140) document vm.runInContext() and vm.createContext() (Antranig Basman)
* [#1428](https://github.com/joyent/node/issues/1428) fix os.freemem() on 64 bits freebsd (Artem Zaytsev)
* [#1164](https://github.com/joyent/node/issues/1164) make all DNS lookups async, fixes uncatchable exceptions
  (Koichi Kobayashi)

* fix incorrect ssl shutdown check (Tom Hughes)
* various cmake fixes (Tom Hughes)
* improved documentation (Koichi Kobayashi, Logan Smyth, Fedor Indutny,
  Mikeal Rogers, Maciej Małecki, Antranig Basman, Mickaël Delahaye)

* upgrade libuv to commit 835782a
* upgrade V8 to 3.5.8

## 2011.08.12, Version 0.5.4 (unstable)

https://github.com/iojs/io.js/commit/cfba1f59224ff8602c3fe9145181cad4c6df89a9

* libuv/Windows compatibility improvements
* Build on Microsoft Visual Studio via GYP. Use generate-projects.bat in the
  to build sln files. (Peter Bright, Igor Zinkovsky)

* Make Mikeal's HTTP agent client the default. Use old HTTP client with
  --use-http1

* Fixes https host header default port handling. (Mikeal Rogers)
* [#1440](https://github.com/joyent/node/issues/1440) strip byte order marker when loading *.js and *.json files
  (Ben Noordhuis)

* [#1434](https://github.com/joyent/node/issues/1434) Improve util.format() compatibility with browser. (Koichi Kobayashi)
* Provide unchecked uint entry points for integer Buffer.read/writeInt
  methods. (Robert Mustacchi)

* CMake improvements (Tom Huges)
* Upgrade V8 to 3.5.4.

## 2011.08.01, Version 0.5.3 (unstable)

https://github.com/iojs/io.js/commit/4585330afef44ddfb6a4054bd9b0f190b352628b

* Fix crypto encryption/decryption with Base64. (SAWADA Tadashi)
* [#243](https://github.com/joyent/node/issues/243) Add an optional length argument to Buffer.write() (koichik)
* [#657](https://github.com/joyent/node/issues/657) convert nonbuffer data to string in fs.writeFile/Sync
  (Daniel Pihlström)

* Add process.features, remove process.useUV (Ben Noordhuis)
* [#324](https://github.com/joyent/node/issues/324) Fix crypto hmac to accept binary keys + add test cases from rfc 2202
  and 4231 (Stefan Bühler)

* Add Socket::bytesRead, Socket::bytesWritten (Alexander Uvarov)
* [#572](https://github.com/joyent/node/issues/572) Don't print result of --eval in CLI (Ben Noordhuis)
* [#1223](https://github.com/joyent/node/issues/1223) Fix http.ClientRequest crashes if end() was called twice (koichik)
* [#1383](https://github.com/joyent/node/issues/1383) Emit 'close' after all connections have closed (Felix Geisendörfer)
* Add sprintf-like util.format() function (Ben Noordhuis)
* Add support for TLS SNI (Fedor Indutny)
* New http agent implementation. Off by default the command line flag
  --use-http2 will enable it. "make test-http2" will run the tests
	for the new implementation. (Mikeal Rogers)

* Revert AMD compatibility. (isaacs)
* Windows: improvements, child_process support.
* Remove pkg-config file.
* Fix startup time regressions.
* doc improvements

## 2011.07.22, Version 0.5.2 (unstable)

https://github.com/iojs/io.js/commit/08ffce1a00dde1199174b390a64a90b60768ddf5

* libuv improvements; named pipe support
* [#1242](https://github.com/joyent/node/issues/1242) check for SSL_COMP_get_compression_methods() (Ben Noordhuis)
* [#1348](https://github.com/joyent/node/issues/1348) remove require.paths (isaacs)
* [#1349](https://github.com/joyent/node/issues/1349) Delimit NODE_PATH with ; on Windows (isaacs)
* [#1335](https://github.com/joyent/node/issues/1335) Remove EventEmitter from C++
* [#1357](https://github.com/joyent/node/issues/1357) Load json files with require() (isaacs)
* [#1374](https://github.com/joyent/node/issues/1374) fix setting ServerResponse.statusCode in writeHead (Trent Mick)
* Fixed: GC was being run too often.
* Upgrade V8 to 3.4.14
* doc improvements

## 2011.07.14, Version 0.5.1 (unstable)

https://github.com/iojs/io.js/commit/f8bfa54d0fa509f9242637bef2869a1b1e842ec8

* [#1233](https://github.com/joyent/node/issues/1233) Fix os.totalmem on FreeBSD amd64 (Artem Zaytsev)
* [#1149](https://github.com/joyent/node/issues/1149) IDNA and Punycode support in url.parse
  (Jeremy Selier, Ben Noordhuis, isaacs)

* Export $CC and $CXX to uv and V8's build systems
* Include pthread-win32 static libraries in build (Igor Zinkovsky)
* [#1199](https://github.com/joyent/node/issues/1199), [#1094](https://github.com/joyent/node/issues/1094) Fix fs can't handle large file on 64bit platform (koichik)
* [#1281](https://github.com/joyent/node/issues/1281) Make require a public member of module (isaacs)
* [#1303](https://github.com/joyent/node/issues/1303) Stream.pipe returns the destination (Elijah Insua)
* [#1229](https://github.com/joyent/node/issues/1229) Addons should not -DEV_MULTIPLICITY=0 (Brian White)
* libuv backend improvements
* Upgrade V8 to 3.4.10

## 2011.07.05, Version 0.5.0 (unstable)

https://github.com/iojs/io.js/commit/ae7ed8482ea7e53c59acbdf3cf0e0a0ae9d792cd

* New non-default libuv backend to support IOCP on Windows.
  Use --use-uv to enable.

* deprecate http.cat
* docs improved.
* add child_process.fork
* add fs.utimes() and fs.futimes() support (Ben Noordhuis)
* add process.uptime() (Tom Huges)
* add path.relative (Tony Huang)
* add os.getNetworkInterfaces()
* add remoteAddress and remotePort for client TCP connections
  (Brian White)

* add secureOptions flag, setting ciphers,
  SSL_OP_CRYPTOPRO_TLSEXT_BUG to TLS (Theo Schlossnagle)

* add process.arch (Nathan Rajlich)
* add reading/writing of floats and doubles from/to buffers (Brian White)
* Allow script to be read from stdin
* [#477](https://github.com/joyent/node/issues/477) add Buffer::fill method to do memset (Konstantin Käfer)
* [#573](https://github.com/joyent/node/issues/573) Diffie-Hellman support to crypto module (Håvard Stranden)
* [#695](https://github.com/joyent/node/issues/695) add 'hex' encoding to buffer (isaacs)
* [#851](https://github.com/joyent/node/issues/851) Update how REPLServer uses contexts (Ben Weaver)
* [#853](https://github.com/joyent/node/issues/853) add fs.lchow, fs.lchmod, fs.fchmod, fs.fchown (isaacs)
* [#889](https://github.com/joyent/node/issues/889) Allow to remove all EventEmitter listeners at once
  (Felix Geisendörfer)

* [#926](https://github.com/joyent/node/issues/926) OpenSSL NPN support (Fedor Indutny)
* [#955](https://github.com/joyent/node/issues/955) Change ^C handling in REPL (isaacs)
* [#979](https://github.com/joyent/node/issues/979) add support for Unix Domain Sockets to HTTP (Mark Cavage)
* [#1173](https://github.com/joyent/node/issues/1173) [#1170](https://github.com/joyent/node/issues/1170) add AMD, asynchronous module definition (isaacs)
* DTrace probes: support X-Forwarded-For (Dave Pacheco)

## 2011.09.15, Version 0.4.12 (stable)

https://github.com/iojs/io.js/commit/771ba34ca7b839add2ef96879e1ffc684813cf7c

* Improve docs
* [#1563](https://github.com/joyent/node/issues/1563) overflow in ChildProcess custom_fd.
* [#1569](https://github.com/joyent/node/issues/1569), parse error on multi-line HTTP headers. (Ben Noordhuis)
* [#1586](https://github.com/joyent/node/issues/1586) net: Socket write encoding case sensitivity (koichik)
* [#1610](https://github.com/joyent/node/issues/1610) Remove DigiNotar CA from trusted list (isaacs)
* [#1624](https://github.com/joyent/node/issues/1624) buffer: Avoid overrun with 'binary' encoding. (koichik)
* [#1633](https://github.com/joyent/node/issues/1633) buffer: write() should always set _charsWritten. (koichik)
* [#1707](https://github.com/joyent/node/issues/1707) hasOwnProperty usage security hole in querystring (isaacs)
* [#1719](https://github.com/joyent/node/issues/1719) Drain OpenSSL error queue
* Fix error reporting in net.Server.listen

## 2011.08.17, Version 0.4.11 (stable)

https://github.com/iojs/io.js/commit/a745d19ce7d1c0e3778371af4f0346be70cf2c8e

* [#738](https://github.com/joyent/node/issues/738) Fix crypto encryption/decryption with Base64. (SAWADA Tadashi)
* [#1202](https://github.com/joyent/node/issues/1202) net.createConnection defer DNS lookup error events to next tick
  (Ben Noordhuis)

* [#1374](https://github.com/joyent/node/issues/1374) fix setting ServerResponse.statusCode in writeHead (Trent Mick)
* [#1417](https://github.com/joyent/node/issues/1417) Fix http.ClientRequest crashes if end() was called twice
* [#1497](https://github.com/joyent/node/issues/1497) querystring: Replace 'in' test with 'hasOwnProperty' (isaacs)
* [#1546](https://github.com/joyent/node/issues/1546) http perf improvement
* fix memleak in libeio (Tom Hughes)
* cmake improvements (Tom Hughes)
* node_net.cc: fix incorrect sizeof() (Tom Hughes)
* Windows/cygwin: no more GetConsoleTitleW errors on XP (Bert Belder)
* Doc improvments (koichik, Logan Smyth, Ben Noordhuis, Arnout Kazemier)

## 2011.07.19, Version 0.4.10 (stable)

https://github.com/iojs/io.js/commit/1b8dd65d6e3b82b6863ef38835cc436c5d30c1d5

* [#394](https://github.com/joyent/node/issues/394) Fix Buffer drops last null character in UTF-8
* [#829](https://github.com/joyent/node/issues/829) Backport r8577 from V8 (Ben Noordhuis)
* [#877](https://github.com/joyent/node/issues/877) Don't wait for HTTP Agent socket pool to establish connections.
* [#915](https://github.com/joyent/node/issues/915) Find kqueue on FreeBSD correctly (Brett Kiefer)
* [#1085](https://github.com/joyent/node/issues/1085) HTTP: Fix race in abort/dispatch code (Stefan Rusu)
* [#1274](https://github.com/joyent/node/issues/1274) debugger improvement (Yoshihiro Kikuchi)
* [#1291](https://github.com/joyent/node/issues/1291) Properly respond to HEAD during end(body) hot path (Reid Burke)
* [#1304](https://github.com/joyent/node/issues/1304) TLS: Fix race in abort/connection code (Stefan Rusu)
* [#1360](https://github.com/joyent/node/issues/1360) Allow _ in url hostnames.
* Revert 37d529f8 - unbreaks debugger command parsing.
* Bring back global execScript
* Doc improvements

## 2011.06.29, Version 0.4.9 (stable)

https://github.com/iojs/io.js/commit/de44eafd7854d06cd85006f509b7051e8540589b

* Improve documentation
* [#1095](https://github.com/joyent/node/issues/1095) error handling bug in stream.pipe() (Felix Geisendörfer)
* [#1097](https://github.com/joyent/node/issues/1097) Fix a few leaks in node_crypto.cc (Ben Noordhuis)
* [#562](https://github.com/joyent/node/issues/562) [#1078](https://github.com/joyent/node/issues/1078) Parse file:// urls properly (Ryan Petrello)
* [#880](https://github.com/joyent/node/issues/880) Option to disable SSLv2 (Jérémy Lal)
* [#1087](https://github.com/joyent/node/issues/1087) Disabling SSL compression disabled with early OpenSSLs.
* [#1144](https://github.com/joyent/node/issues/1144) debugger: don't allow users to input non-valid commands
  (Siddharth Mahendraker)

* Perf improvement for util.inherits
* [#1166](https://github.com/joyent/node/issues/1166) Support for signature verification with RSA/DSA public keys
  (Mark Cavage)

* [#1177](https://github.com/joyent/node/issues/1177) Remove node_modules lookup optimization to better support
  nested project structures (Mathias Buus)

* [#1203](https://github.com/joyent/node/issues/1203) Add missing scope.Close to fs.sendfileSync
* [#1187](https://github.com/joyent/node/issues/1187) Support multiple 'link' headers
* [#1196](https://github.com/joyent/node/issues/1196) Fix -e/--eval can't load module from node_modules (Koichi Kobayashi)
* Upgrade V8 to 3.1.8.25, upgrade http-parser.

## 2011.05.20, Version 0.4.8 (stable)

https://github.com/iojs/io.js/commit/7dd22c26e4365698dc3efddf138c4d399cb912c8

* [#974](https://github.com/joyent/node/issues/974) Properly report traceless errors (isaacs)
* [#983](https://github.com/joyent/node/issues/983) Better JSON.parse error detection in REPL (isaacs)
* [#836](https://github.com/joyent/node/issues/836) Agent socket errors bubble up to req only if req exists
* [#1041](https://github.com/joyent/node/issues/1041) Fix event listener leak check timing (koichik)
*	[#1038](https://github.com/joyent/node/issues/1038) Fix dns.resolve() with 'PTR' throws Error: Unknown type "PTR"
  (koichik)

* [#1073](https://github.com/joyent/node/issues/1073) Share SSL context between server connections (Fedor Indutny)
* Disable compression with OpenSSL. Improves memory perf.
* Implement os.totalmem() and os.freemem() for SunOS (Alexandre Marangone)
* Fix a special characters in URL regression (isaacs)
* Fix idle timeouts in HTTPS (Felix Geisendörfer)
* SlowBuffer.write() with 'ucs2' throws ReferenceError. (koichik)
* http.ServerRequest 'close' sometimes gets an error argument
  (Felix Geisendörfer)

* Doc improvements
* cleartextstream.destroy() should close(2) the socket. Previously was being
	mapped to a shutdown(2) syscall.

* No longer compile out asserts and debug statements in normal build.
* Debugger improvements.
* Upgrade V8 to 3.1.8.16.

## 2011.04.22, Version 0.4.7 (stable)

https://github.com/iojs/io.js/commit/c85455a954411b38232e79752d4abb61bb75031b

* Don't emit error on ECONNRESET from read() [#670](https://github.com/joyent/node/issues/670)
* Fix: Multiple pipes to the same stream were broken [#929](https://github.com/joyent/node/issues/929)
  (Felix Geisendörfer)

* URL parsing/formatting corrections [#954](https://github.com/joyent/node/issues/954) (isaacs)
* make it possible to do repl.start('', stream) (Wade Simmons)
* Add os.loadavg for SunOS (Robert Mustacchi)
* Fix timeouts with floating point numbers [#897](https://github.com/joyent/node/issues/897) (Jorge Chamorro Bieling)
* Improve docs.

## 2011.04.13, Version 0.4.6 (stable)

https://github.com/iojs/io.js/commit/58002d56bc79410c5ff397fc0e1ffec0665db38a

* Don't error on ENOTCONN from shutdown() [#670](https://github.com/joyent/node/issues/670)
* Auto completion of built-in debugger suggests prefix match rather than
	partial match. (koichik)

* circular reference in vm modules. [#822](https://github.com/joyent/node/issues/822) (Jakub Lekstan)
* http response.readable should be false after 'end' [#867](https://github.com/joyent/node/issues/867) (Abe Fettig)
* Implement os.cpus() and os.uptime() on Solaris (Scott McWhirter)
* fs.ReadStream: Allow omission of end option for range reads [#801](https://github.com/joyent/node/issues/801)
	(Felix Geisendörfer)

* Buffer.write() with UCS-2 should not be write partial char
	[#916](https://github.com/joyent/node/issues/916) (koichik)

* Pass secureProtocol through on tls.Server creation (Theo Schlossnagle)
* TLS use RC4-SHA by default
* Don't strangely drop out of event loop on HTTPS client uploads [#892](https://github.com/joyent/node/issues/892)
* Doc improvements
* Upgrade v8 to 3.1.8.10

## 2011.04.01, Version 0.4.5 (stable)

https://github.com/iojs/io.js/commit/787a343b588de26784fef97f953420b53a6e1d73

* Fix listener leak in stream.pipe() (Mikeal Rogers)
* Retain buffers in fs.read/write() GH-814 (Jorge Chamorro Bieling)
* TLS performance improvements
* SlowBuffer.prototype.slice bug GH-843
* process.stderr.write should return true
* Immediate pause/resume race condition GH-535 (isaacs)
* Set default host header properly GH-721 (isaacs)
* Upgrade V8 to 3.1.8.8

## 2011.03.26, Version 0.4.4 (stable)

https://github.com/iojs/io.js/commit/25122b986a90ba0982697b7abcb0158c302a1019

* CryptoStream.end shouldn't throw if not writable GH-820
* Drop out if connection destroyed before connect() GH-819
* expose https.Agent
* Correctly setsid in tty.open GH-815
* Bug fix for failed buffer construction
* Added support for removing .once listeners (GH-806)
* Upgrade V8 to 3.1.8.5

## 2011.03.18, Version 0.4.3 (stable)

https://github.com/iojs/io.js/commit/c095ce1a1b41ca015758a713283bf1f0bd41e4c4

* Don't decrease server connection counter again if destroy() is called more
	than once GH-431 (Andreas Reich, Anders Conbere)

* Documentation improvements (koichik)
* Fix bug with setMaxListeners GH-682
* Start up memory footprint improvement. (Tom Hughes)
* Solaris improvements.
* Buffer::Length(Buffer*) should not invoke itself recursively GH-759 (Ben
  Noordhuis)

* TLS: Advertise support for client certs GH-774 (Theo Schlossnagle)
* HTTP Agent bugs: GH-787, GH-784, GH-803.
* Don't call GetMemoryUsage every 5 seconds.
* Upgrade V8 to 3.1.8.3

## 2011.03.02, Version 0.4.2 (stable)

https://github.com/iojs/io.js/commit/39280e1b5731f3fcd8cc42ad41b86cdfdcb6d58b

* Improve docs.
* Fix process.on edge case with signal event (Alexis Sellier)
* Pragma HTTP header comma separation
* In addition to 'aborted' emit 'close' from incoming requests
  (Felix Geisendörfer)

* Fix memleak in vm.runInNewContext
* Do not cache modules that throw exceptions (Felix Geisendörfer)
* Build system changes for libnode (Aria Stewart)
* Read up the prototype of the 'env' object. (Nathan Rajlich)
* Add 'close' and 'aborted' events to Agent responses
* http: fix missing 'drain' events (Russell Haering)
* Fix process.stdout.end() throws ENOTSOCK error. (Koichi Kobayashi)
* REPL bug fixes (isaacs)
* node_modules folders should be highest priority (isaacs)
* URL parse more safely (isaacs)
* Expose errno with a string for dns/cares (Felix Geisendörfer)
* Fix tty.setWindowSize
* spawn: setuid after chdir (isaacs)
* SIGUSR1 should break the VM without delay
* Upgrade V8 to 3.1.8.

## 2011.02.19, Version 0.4.1 (stable)

https://github.com/iojs/io.js/commit/e8aef84191bc2c1ba2bcaa54f30aabde7f03769b

* Fixed field merging with progressive fields on writeHead()
  (TJ Holowaychuk)

* Make the repl respect node_modules folders (isaacs)
* Fix for DNS fail in HTTP request (Richard Rodger)
* Default to port 80 for http.request and http.get.
* Improve V8 support for Cygwin (Bert Belder)
* Fix fs.open param parsing. (Felix Geisendörfer)
* Fixed null signal.
* Fix various HTTP and HTTPS bugs
* cmake improvements (Tom Hughes)
* Fix: TLS sockets should not be writable after 'end'
* Fix os.cpus() on cygwin (Brian White)
* MinGW: OpenSSL support (Bert Belder)
* Upgrade V8 to 3.1.5, libev to 4.4.

## 2011.02.10, Version 0.4.0 (stable)

https://github.com/iojs/io.js/commit/eb155ea6f6a6aa341aa8c731dca8da545c6a4008

* require() improvements (isaacs)
  - understand package.json (isaacs)
  - look for 'node_modules' dir

* cmake fixes (Daniel Gröber)
* http: fix buffer writes to outgoing messages (Russell Haering)
* Expose UCS-2 Encoding (Konstantin Käfer)
* Support strings for octal modes (isaacs)
* Support array-ish args to Buffer ctor (isaacs)
* cygwin and mingw improvements (Bert Belder)
* TLS improvements
* Fewer syscalls during require (Bert Belder, isaacs)
* More DTrace probes (Bryan Cantrill,  Robert Mustacchi)
* 'pipe' event on pipe() (Mikeal Rogers)
* CRL support in TLS (Theo Schlossnagle)
* HTTP header manipulation methods (Tim Caswell, Charlie Robbins)
* Upgrade V8 to 3.1.2

## 2011.02.04, Version 0.3.8 (unstable)

https://github.com/iojs/io.js/commit/9493b7563bff31525b4080df5aeef09747782d5e

* Add req.abort() for client side requests.
* Add exception.code for easy testing:
  Example: if (err.code == 'EADDRINUSE');

* Add process.stderr.
* require.main is the main module. (Isaac Schlueter)
* dgram: setMulticastTTL, setMulticastLoopback and addMembership.
  (Joe Walnes)

* Fix throttling in TLS connections
* Add socket.bufferSize
* MinGW improvements (Bert Belder)
* Upgrade V8 to 3.1.1

## 2011.01.27, Version 0.3.7 (unstable)

https://github.com/iojs/io.js/commit/d8579c6afdbe868de6dffa8db78bbe4ba2d03e0e

* Expose agent in http and https client. (Mikeal Rogers)
* Fix bug in http request's end method. (Ali Farhadi)
* MinGW: better net support (Bert Belder)
* fs.open should set FD_CLOEXEC
* DTrace probes (Bryan Cantrill)
* REPL fixes and improvements (isaacs, Bert Belder)
* Fix many bugs with legacy http.Client interface
* Deprecate process.assert. Use require('assert').ok
* Add callback parameter to socket.setTimeout(). (Ali Farhadi)
* Fixing bug in http request default encoding (Ali Farhadi)
* require: A module ID with a trailing slash must be a dir.
  (isaacs)

* Add ext_key_usage to getPeerCertificate (Greg Hughes)
* Error when child_process.exec hits maxBuffer.
* Fix option parsing in tls.connect()
* Upgrade to V8 3.0.10

## 2011.01.21, Version 0.3.6 (unstable)

https://github.com/iojs/io.js/commit/bb3e71466e5240626d9d21cf791fe43e87d90011

* REPL and other improvements on MinGW (Bert Belder)
* listen/bind errors should close net.Server
* New HTTP and HTTPS client APIs
* Upgrade V8 to 3.0.9

## 2011.01.16, Version 0.3.5 (unstable)

https://github.com/iojs/io.js/commit/b622bc6305e3c675e0edfcdbaa387d849ad0bba0

* Built-in debugger improvements.
* Add setsid, setuid, setgid options to child_process.spawn
  (Isaac Schlueter)

* tty module improvements.
* Upgrade libev to 4.3, libeio to latest, c-ares to 1.7.4
* Allow third party hooks before main module load.
  (See 496be457b6a2bc5b01ec13644b9c9783976159b2)

* Don't stat() on cached modules. (Felix Geisendörfer)

## 2011.01.08, Version 0.3.4 (unstable)

https://github.com/iojs/io.js/commit/73f53e12e4a5b9ef7dbb4792bd5f8ad403094441

* Primordial mingw build (Bert Belder)
* HTTPS server
* Built in debugger 'node debug script.js'
* realpath files during module load (Mihai Călin Bazon)
* Rename net.Stream to net.Socket (existing name will continue to be
  supported)

* Fix process.platform

## 2011.01.02, Version 0.3.3 (unstable)

https://github.com/iojs/io.js/commit/57544ba1c54c7d0da890317deeb73076350c5647

* TLS improvements.
* url.parse(url, true) defaults query field to {} (Jeremy Martin)
* Upgrade V8 to 3.0.4
* Handle ECONNABORT properly (Theo Schlossnagle)
* Fix memory leaks (Tom Hughes)
* Add os.cpus(), os.freemem(), os.totalmem(), os.loadavg() and other
  functions for OSX, Linux, and Cygwin. (Brian White)

* Fix REPL syntax error bug (GH-543), improve how REPL commands are
  evaluated.

* Use process.stdin instead of process.openStdin().
* Disable TLS tests when node doesn't have OpenSSL.

## 2010.12.16, Version 0.3.2 (unstable)

https://github.com/iojs/io.js/commit/4bb914bde9f3c2d6de00853353b6b8fc9c66143a

* Rip out the old (broken) TLS implementation introduce new tested
  implementation and API. See docs. HTTPS not supported in this release.

* Introduce 'os' and 'tty' modules.
* Callback parameters for socket.write() and socket.connect().
* Support CNAME lookups in DNS module. (Ben Noordhuis)
* cmake support (Tom Hughes)
* 'make lint'
* oprofile support (./configure --oprofile)
* Lots of bug fixes, including:
  - Memory leak in ChildProcess:Spawn(). (Tom Hughes)
  - buffer.slice(0, 0)
  - Global variable leaks
  - clearTimeouts calling multiple times (Michael W)
  - utils.inspect's detection of circular structures (Tim Cooijmans)
  - Apple's threaded write()s bug (Jorge Chamorro Bieling)
  - Make sure raw mode is disabled when exiting a terminal-based REPL.
    (Brian White)

* Deprecate process.compile, process.ENV
* Upgrade V8 to 3.0.3, upgrade http-parser.

## 2010.11.16, Version 0.3.1 (unstable)

https://github.com/iojs/io.js/commit/ce9a54aa1fbf709dd30316af8a2f14d83150e947

* TLS improvements (Paul Querna)
  - Centralize error handling in SecureStream
  - Add SecurePair for handling of a ssl/tls stream.

* New documentation organization (Micheil Smith)
* allowHalfOpen TCP connections disabled by default.
* Add C++ API for constructing fast buffer from string
* Move idle timers into its own module
* Gracefully handle EMFILE and server.maxConnections
* make "node --eval" eval in the global scope.
  (Jorge Chamorro Bieling)

* Let exit listeners know the exit code (isaacs)
* Handle cyclic links smarter in fs.realpath (isaacs)
* Remove node-repl (just use 'node' without args)
* Rewrite libeio After callback to use req->result instead of req->errorno
  for error checking (Micheil Smith)

* Remove warning about deprecating 'sys' - too aggressive
* Make writes to process.env update the real environment. (Ben Noordhuis)
* Set FD_CLOEXEC flag on stdio FDs before spawning. (Guillaume Tuton)
* Move ev_loop out of javascript
* Switch \n with \r\n for all strings printed out.
* Added support for cross compilation (Rasmus Andersson)
* Add --profile flag to configure script, enables gprof profiling.
  (Ben Noordhuis)

* writeFileSync could exhibit pathological behavior when a buffer
  could not be written to the file in a single write() call.

* new path.join behavior (isaacs)
  - Express desired path.join behavior in tests.
  - Update fs.realpath to reflect new path.join behavior
  - Update url.resolve() to use new path.join behavior.

* API: Move process.binding('evals') to require('vm')
* Fix V8 build on Cygwin (Bert Belder)
* Add ref to buffer during fs.write and fs.read
* Fix segfault on test-crypto
* Upgrade http-parser to latest and V8 to 2.5.3

## 2010.10.23, Version 0.3.0 (unstable)

https://github.com/iojs/io.js/commit/1582cfebd6719b2d2373547994b3dca5c8c569c0

* Bugfix: Do not spin on accept() with EMFILE
* Improvements to readline.js (Trent Mick, Johan Euphrosine, Brian White)
* Safe constructors (missing 'new' doesn't segfault)
* Fix process.nextTick so thrown errors don't confuse it.
  (Benjamin Thomas)

* Allow Strings for ports on net.Server.listen (Bradley Meck)
* fs bugfixes (Tj Holowaychuk, Tobie Langel, Marco Rogers, isaacs)
* http bug fixes (Fedor Indutny, Mikeal Rogers)
* Faster buffers; breaks C++ API (Tim-Smart, Stéphan Kochen)
* crypto, tls improvements (Paul Querna)
* Add lfs flags to node addon script
* Simpler querystring parsing; breaks API (Peter Griess)
* HTTP trailers (Mark Nottingham)
* http 100-continue support (Mark Nottingham)
* Module system simplifications (Herbert Vojčík, isaacs, Tim-Smart)
  - remove require.async
  - remove registerExtension, add .extensions
  - expose require.resolve
  - expose require.cache
  - require looks in  node_modules folders

* Add --eval command line option (TJ Holowaychuk)
* Commas last in sys.inspect
* Constants moved from process object to require('constants')
* Fix parsing of linux memory (Vitali Lovich)
* inspect shows function names (Jorge Chamorro Bieling)
* uncaughtException corner cases (Felix Geisendörfer)
* TCP clients now buffer writes before connection
* Rename sys module to 'util' (Micheil Smith)
* Properly set stdio handlers to blocking on SIGTERM and SIGINT
  (Tom Hughes)

* Add destroy methods to HTTP messages
* base64 improvements (isaacs, Jorge Chamorro Bieling)
* API for defining REPL commands (Sami Samhuri)
* child_process.exec timeout fix (Aaron Heckmann)
* Upgrade V8 to 2.5.1, Libev to 4.00, libeio, http-parser

## 2010.08.20, Version 0.2.0

https://github.com/iojs/io.js/commit/9283e134e558900ba89d9a33c18a9bdedab07cb9

* process.title support for FreeBSD, Macintosh, Linux
* Fix OpenSSL 100% CPU usage on error (Illarionov Oleg)
* Implement net.Server.maxConnections.
* Fix process.platform, add process.version.
* Add --without-snapshot configure option.
* Readline REPL improvements (Trent Mick)
* Bug fixes.
* Upgrade V8 to 2.3.8

## 2010.08.13, Version 0.1.104

https://github.com/iojs/io.js/commit/b14dd49222687c12f3e8eac597cff4f2674f84e8

* Various bug fixes (console, querystring, require)
* Set cwd for child processes (Bert Belder)
* Tab completion for readline (Trent Mick)
* process.title getter/setter for OSX, Linux, Cygwin.
	(Rasmus Andersson, Bert Belder)

* Upgrade V8 to 2.3.6

## 2010.08.04, Version 0.1.103

https://github.com/iojs/io.js/commit/0b925d075d359d03426f0b32bb58a5e05825b4ea

* Implement keep-alive for http.Client (Mikeal Rogers)
* base64 fixes. (Ben Noordhuis)
* Fix --debug-brk (Danny Coates)
* Don't let path.normalize get above the root. (Isaac Schlueter)
* Allow signals to be used with process.on in addition to
  process.addListener. (Brian White)

* Globalize the Buffer object
* Use kqueue on recent macintosh builds
* Fix addrlen for unix_dgram sockets (Benjamin Kramer)
* Fix stats.isDirectory() and friends (Benjamin Kramer)
* Upgrade http-parser, V8 to 2.3.5

## 2010.07.25, Version 0.1.102

https://github.com/iojs/io.js/commit/2a4568c85f33869c75ff43ccd30f0ec188b43eab

* base64 encoding for Buffers.
* Buffer support for Cipher, Decipher, Hmac, Sign and Verify
  (Andrew Naylor)

* Support for reading byte ranges from files using fs.createReadStream.
  (Chandra Sekar)

* Fix Buffer.toString() on 0-length slices. (Peter Griess)
* Cache modules based on filename rather than ID (Isaac Schlueter)
* querystring improvments (Jan Kassens, Micheil Smith)
* Support DEL in the REPL. (Jérémy Lal)
* Upgrade http-parser, upgrade V8 to 2.3.2

## 2010.07.16, Version 0.1.101

https://github.com/iojs/io.js/commit/0174ceb6b24caa0bdfc523934c56af9600fa9b58

* Added env to child_process.exec (Сергей Крыжановский)
* Allow modules to optionally be loaded in separate contexts
  with env var NODE_MODULE_CONTEXTS=1.

* setTTL and setBroadcast for dgram (Matt Ranney)
* Use execPath for default NODE_PATH, not installPrefix
  (Isaac Schlueter)

* Support of console.dir + console.assert (Jerome Etienne)
* on() as alias to addListener()
* Use javascript port of Ronn to build docs (Jérémy Lal)
* Upgrade V8 to 2.3.0

## 2010.07.03, Version 0.1.100

https://github.com/iojs/io.js/commit/a6b8586e947f9c3ced180fe68c233d0c252add8b

* process.execPath (Marshall Culpepper)
* sys.pump (Mikeal Rogers)
* Remove ini and mjsunit libraries.
* Introduce console.log() and friends.
* Switch order of arguments for Buffer.write (Blake Mizerany)
* On overlapping buffers use memmove (Matt Ranney)
* Resolve .local domains with getaddrinfo()
* Upgrade http-parser, V8 to 2.2.21

## 2010.06.21, Version 0.1.99

https://github.com/iojs/io.js/commit/a620b7298f68f68a855306437a3b60b650d61d78

* Datagram sockets (Paul Querna)
* fs.writeFile could not handle utf8 (Felix Geisendörfer)
  and now accepts Buffers (Aaron Heckmann)

* Fix crypto memory leaks.
* A replacement for decodeURIComponent that doesn't throw.
  (Isaac Schlueter)

* Only concatenate some incoming HTTP headers. (Peter Griess)
* Upgrade V8 to 2.2.18

## 2010.06.11, Version 0.1.98

https://github.com/iojs/io.js/commit/10d8adb08933d1d4cea60192c2a31c56d896733d

* Port to Windows/Cygwin (Raffaele Sena)
* File descriptor passing on unix sockets. (Peter Griess)
* Simple, builtin readline library. REPL is now entered by
  executing "node" without arguments.

* Add a parameter to spawn() that sets the child's stdio file
  descriptors. (Orlando Vazquez)

* Upgrade V8 to 2.2.16, http-parser fixes, upgrade c-ares to 1.7.3.

## 2010.05.29, Version 0.1.97

https://github.com/iojs/io.js/commit/0c1aa36835fa6a3557843dcbc6ed6714d353a783

* HTTP throttling: outgoing messages emit 'drain' and write() returns false
  when send buffer is full.

* API: readFileSync without encoding argument now returns a Buffer
* Improve Buffer C++ API; addons now compile with debugging symbols.
* Improvements to  path.extname() and REPL; add fs.chown().
* fs.ReadStream now emits buffers, fs.readFileSync returns buffers.
* Bugfix: parsing HTTP responses to HEAD requests.
* Port to OpenBSD.
* Upgrade V8 to 2.2.12, libeio, http-parser.

## 2010.05.21, Version 0.1.96

https://github.com/iojs/io.js/commit/9514a4d5476225e8c8310ce5acae2857033bcaaa

* Thrown errors in http and socket call back get bubbled up.
* Add fs.fsync (Andrew Johnston)
* Bugfix: signal unregistering (Jonas Pfenniger)
* Added better error messages for async and sync fs calls with paths
  (TJ Holowaychuk)

* Support arrays and strings in buffer constructor.
  (Felix Geisendörfer)

* Fix errno reporting in DNS exceptions.
* Support buffers in fs.WriteStream.write.
* Bugfix: Safely decode a utf8 streams that are broken on a multbyte
  character (http and net). (Felix Geisendörfer)

* Make Buffer's C++ constructor public.
* Deprecate sys.p()
* FIX path.dirname('/tmp') => '/'. (Jonathan Rentzsch)

## 2010.05.13, Version 0.1.95

https://github.com/iojs/io.js/commit/0914d33842976c2c870df06573b68f9192a1fb7a

* Change GC idle notify so that it runs alongside setInterval
* Install node_buffer.h on make install
* fs.readFile returns Buffer by default (Tim Caswell)
* Fix error reporting in child_process callbacks
* Better logic for testing if an argument is a port
* Improve error reporting (single line "node.js:176:9" errors)
* Bugfix: Some http responses being truncated (appeared in 0.1.94)
* Fix long standing net idle timeout bugs. Enable 2 minute timeout
  by default in HTTP servers.

* Add fs.fstat (Ben Noordhuis)
* Upgrade to V8 2.2.9

## 2010.05.06, Version 0.1.94

https://github.com/iojs/io.js/commit/f711d5343b29d1e72e87107315708e40951a7826

* Look in /usr/local/lib/node for modules, so that there's a way
  to install modules globally (Issac Schlueter)

* SSL improvements (Rhys Jones, Paulo Matias)
* Added c-ares headers for linux-arm (Jonathan Knezek)
* Add symbols to release build
* HTTP upgrade improvements, docs (Micheil Smith)
* HTTP server emits 'clientError' instead of printing message
* Bugfix: Don't emit 'error' twice from http.Client
* Bugfix: Ignore SIGPIPE
* Bugfix: destroy() instead of end() http connection at end of
  pipeline

* Bugfix: http.Client may be prematurely released back to the
  free pool.  (Thomas Lee)

* Upgrade V8 to 2.2.8

## 2010.04.29, Version 0.1.93

https://github.com/iojs/io.js/commit/557ba6bd97bad3afe0f9bd3ac07efac0a39978c1

  * Fixed no 'end' event on long chunked HTTP messages
    https://github.com/joyent/node/issues/77

  * Remove legacy modules http_old and tcp_old
  * Support DNS MX queries (Jérémy Lal)

  * Fix large socket write (tlb@tlb.org)
  * Fix child process exit codes (Felix Geisendörfer)

  * Allow callers to disable PHP/Rails style parameter munging in
    querystring.stringify (Thomas Lee)

  * Upgrade V8 to 2.2.6

## 2010.04.23, Version 0.1.92

https://github.com/iojs/io.js/commit/caa828a242f39b6158084ef4376355161c14fe34

  * OpenSSL support. Still undocumented (see tests). (Rhys Jones)
  * API: Unhandled 'error' events throw.

  * Script class with eval-function-family in binding('evals') plus tests.
    (Herbert Vojcik)

  * stream.setKeepAlive (Julian Lamb)
  * Bugfix: Force no body on http 204 and 304

  * Upgrade Waf to 1.5.16, V8 to 2.2.4.2

## 2010.04.15, Version 0.1.91

https://github.com/iojs/io.js/commit/311d7dee19034ff1c6bc9098c36973b8d687eaba

  * Add incoming.httpVersion
  * Object.prototype problem with C-Ares binding

  * REPL can be run from multiple different streams. (Matt Ranney)
  * After V8 heap is compact, don't use a timer every 2 seconds.

  * Improve nextTick implementation.
  * Add primative support for Upgrading HTTP connections.
    (See commit log for docs 760bba5)

  * Add timeout and maxBuffer options to child_process.exec
  * Fix bugs.

  * Upgrade V8 to 2.2.3.1

## 2010.04.09, Version 0.1.90

https://github.com/iojs/io.js/commit/07e64d45ffa1856e824c4fa6afd0442ba61d6fd8

  * Merge writing of networking system (net2)
   - New Buffer object for binary data.
   - Support UNIX sockets, Pipes
   - Uniform stream API
   - Currently no SSL
   - Legacy modules can be accessed at 'http_old' and 'tcp_old'

  * Replace udns with c-ares. (Krishna Rajendran)
  * New documentation system using Markdown and Ronn
    (Tim Caswell, Micheil Smith)

  * Better idle-time GC
  * Countless small bug fixes.

  * Upgrade V8 to 2.2.X, WAF 1.5.15

## 2010.03.19, Version 0.1.33

https://github.com/iojs/io.js/commit/618296ef571e873976f608d91a3d6b9e65fe8284

  * Include lib/ directory in node executable. Compile on demand.
  * evalcx clean ups (Isaac Z. Schlueter, Tim-Smart)

  * Various fixes, clean ups
  * V8 upgraded to 2.1.5

## 2010.03.12, Version 0.1.32

https://github.com/iojs/io.js/commit/61c801413544a50000faa7f58376e9b33ba6254f

  * Optimize event emitter for single listener
  * Add process.evalcx, require.registerExtension (Tim Smart)

  * Replace --cflags with --vars
  * Fix bugs in fs.create*Stream (Felix Geisendörfer)

  * Deprecate process.mixin, process.unloop
  * Remove the 'Error: (no message)' exceptions, print stack
    trace instead

  * INI parser bug fixes (Isaac Schlueter)
  * FreeBSD fixes (Vanilla Hsu)

  * Upgrade to V8 2.1.3, WAF 1.5.14a, libev

## 2010.03.05, Version 0.1.31

https://github.com/iojs/io.js/commit/39b63dfe1737d46a8c8818c92773ef181fd174b3

  * API: - Move process.watchFile into fs module
         - Move process.inherits to sys

  * Improve Solaris port
  * tcp.Connection.prototype.write now returns boolean to indicate if
    argument was flushed to the kernel buffer.

  * Added fs.link, fs.symlink, fs.readlink, fs.realpath
    (Rasmus Andersson)

  * Add setgid,getgid (James Duncan)
  * Improve sys.inspect (Benjamin Thomas)

  * Allow passing env to child process (Isaac Schlueter)
  * fs.createWriteStream, fs.createReadStream (Felix Geisendörfer)

  * Add INI parser (Rob Ellis)
  * Bugfix: fs.readFile handling encoding (Jacek Becela)

  * Upgrade V8 to 2.1.2

## 2010.02.22, Version 0.1.30

https://github.com/iojs/io.js/commit/bb0d1e65e1671aaeb21fac186b066701da0bc33b

  * Major API Changes
    - Promises removed. See
      http://groups.google.com/group/nodejs/msg/426f3071f3eec16b
      http://groups.google.com/group/nodejs/msg/df199d233ff17efa
      The API for fs was
         fs.readdir("/usr").addCallback(function (files) {
           puts("/usr files: " + files);
         });
      It is now
         fs.readdir("/usr", function (err, files) {
           if (err) throw err;
           puts("/usr files: " + files);
         });
    - Synchronous fs operations exposed, use with care.
    - tcp.Connection.prototype.readPause() and readResume()
      renamed to pause() and resume()
    - http.ServerResponse.prototype.sendHeader() renamed to
      writeHeader(). Now accepts reasonPhrase.

  * Compact garbage on idle.
  * Configurable debug ports, and --debug-brk (Zoran Tomicic)

  * Better command line option parsing (Jeremy Ashkenas)
  * Add fs.chmod (Micheil Smith), fs.lstat (Isaac Z. Schlueter)

  * Fixes to process.mixin (Rasmus Andersson, Benjamin Thomas)
  * Upgrade V8 to 2.1.1

## 2010.02.17, Version 0.1.29

https://github.com/iojs/io.js/commit/87d5e5b316a4276bcf881f176971c1a237dcdc7a

  * Major API Changes
    - Remove 'file' module
    - require('posix') -----------------> require('fs')
    - fs.cat ---------------------------> fs.readFile
    - file.write -----------------------> fs.writeFile
    - TCP 'receive' event --------------> 'data'
    - TCP 'eof' event ------------------> 'end'
    - TCP send() -----------------------> write()
    - HTTP sendBody() ------------------> write()
    - HTTP finish() --------------------> close()
    - HTTP 'body' event ----------------> 'data'
    - HTTP 'complete' event ------------> 'end'
    - http.Client.prototype.close() (formerly finish()) no longer
      takes an argument. Add the 'response' listener manually.
    - Allow strings for the flag argument to fs.open
      ("r", "r+", "w", "w+", "a", "a+")

  * Added multiple arg support for sys.puts(), print(), etc.
    (tj@vision-media.ca)

  * sys.inspect(Date) now shows the date value (Mark Hansen)
  * Calculate page size with getpagesize for armel (Jérémy Lal)

  * Bugfix: stderr flushing.
  * Bugfix: Promise late chain (Yuichiro MASUI)

  * Bugfix: wait() on fired promises
    (Felix Geisendörfer, Jonas Pfenniger)

  * Bugfix: Use InstanceTemplate() instead of PrototypeTemplate() for
    accessor methods. Was causing a crash with Eclipse debugger.
    (Zoran Tomicic)

  * Bugfix: Throw from connection.connect if resolving.
    (Reported by James Golick)

## 2010.02.09, Version 0.1.28

https://github.com/iojs/io.js/commit/49de41ef463292988ddacfb01a20543b963d9669

  * Use Google's jsmin.py which can be used for evil.
  * Add posix.truncate()

  * Throw errors from server.listen()
  * stdio bugfix (test by Mikeal Rogers)

  * Module system refactor (Felix Geisendörfer, Blaine Cook)
  * Add process.setuid(), getuid() (Michael Carter)

  * sys.inspect refactor (Tim Caswell)
  * Multipart library rewrite (isaacs)

## 2010.02.03, Version 0.1.27

https://github.com/iojs/io.js/commit/0cfa789cc530848725a8cb5595224e78ae7b9dd0

  * Implemented __dirname (Felix Geisendörfer)
  * Downcase process.ARGV, process.ENV, GLOBAL
    (now process.argv, process.env, global)

  * Bug Fix: Late promise promise callbacks firing
    (Felix Geisendörfer, Jonas Pfenniger)

  * Make assert.AssertionError instance of Error
  * Removed inline require call for querystring
    (self@cloudhead.net)

  * Add support for MX, TXT, and SRV records in DNS module.
    (Blaine Cook)

  * Bugfix: HTTP client automatically reconnecting
  * Adding OS X .dmg build scripts. (Standa Opichal)

  * Bugfix: ObjectWrap memory leak
  * Bugfix: Multipart handle Content-Type headers with charset
    (Felix Geisendörfer)

  * Upgrade http-parser to fix header overflow attack.
  * Upgrade V8 to 2.1.0

  * Various other bug fixes, performance improvements.

## 2010.01.20, Version 0.1.26

https://github.com/iojs/io.js/commit/da00413196e432247346d9e587f8c78ce5ceb087

  * Bugfix, HTTP eof causing crash (Ben Williamson)
  * Better error message on SyntaxError

  * API: Move Promise and EventEmitter into 'events' module
  * API: Add process.nextTick()

  * Allow optional params to setTimeout, setInterval
    (Micheil Smith)

  * API: change some Promise behavior (Felix Geisendörfer)
    - Removed Promise.cancel()
    - Support late callback binding
    - Make unhandled Promise errors throw an exception

  * Upgrade V8 to 2.0.6.1
  * Solaris port (Erich Ocean)

## 2010.01.09, Version 0.1.25

https://github.com/iojs/io.js/commit/39ca93549af91575ca9d4cbafd1e170fbcef3dfa

  * sys.inspect() improvements (Tim Caswell)
  * path module improvements (isaacs, Benjamin Thomas)

  * API: request.uri -> request.url
    It is no longer an object, but a string. The 'url' module
    was addded to parse that string. That is, node no longer
    parses the request URL automatically.
       require('url').parse(request.url)
    is roughly equivlent to the old request.uri object.
    (isaacs)

  * Bugfix: Several libeio related race conditions.
  * Better errors for multipart library (Felix Geisendörfer)

  * Bugfix: Update node-waf version to 1.5.10
  * getmem for freebsd (Vanilla Hsu)

## 2009.12.31, Version 0.1.24

https://github.com/iojs/io.js/commit/642c2773a7eb2034f597af1cd404b9e086b59632

  * Bugfix: don't chunk responses to HTTP/1.0 clients, even if
    they send Connection: Keep-Alive (e.g. wget)

  * Bugfix: libeio race condition
  * Bugfix: Don't segfault on unknown http method

  * Simplify exception reporting
  * Upgrade V8 to 2.0.5.4

## 2009.12.22, Version 0.1.23

https://github.com/iojs/io.js/commit/f91e347eeeeac1a8bd6a7b462df0321b60f3affc

  * Bugfix: require("../blah") issues (isaacs)
  * Bugfix: posix.cat (Jonas Pfenniger)

  * Do not pause request for multipart parsing (Felix Geisendörfer)

## 2009.12.19, Version 0.1.22

https://github.com/iojs/io.js/commit/a2d809fe902f6c4102dba8f2e3e9551aad137c0f

  * Bugfix: child modules get wrong id with "index.js" (isaacs)
  * Bugfix: require("../foo") cycles (isaacs)

  * Bugfix: require() should throw error if module does.
  * New URI parser stolen from Narwhal (isaacs)

  * Bugfix: correctly check kqueue and epoll. (Rasmus Andersson)
  * Upgrade WAF to 1.5.10

  * Bugfix: posix.statSync() was crashing
  * Statically define string symbols for performance improvement

  * Bugfix: ARGV[0] weirdness
  * Added superCtor to ctor.super_ instead superCtor.prototype.
    (Johan Dahlberg)

  * http-parser supports webdav methods
  * API: http.Client.prototype.request() (Christopher Lenz)

## 2009.12.06, Version 0.1.21

https://github.com/iojs/io.js/commit/c6affb64f96a403a14d20035e7fbd6d0ce089db5

  * Feature: Add HTTP client TLS support (Rhys Jones)
  * Bugfix: use --jobs=1 with WAF

  * Bugfix: Don't use chunked encoding for 1.0 requests
  * Bugfix: Duplicated header weren't handled correctly

  * Improve sys.inspect (Xavier Shay)
  * Upgrade v8 to 2.0.3

  * Use CommonJS assert API (Felix Geisendörfer, Karl Guertin)

## 2009.11.28, Version 0.1.20

https://github.com/iojs/io.js/commit/aa42c6790da8ed2cd2b72051c07f6251fe1724d8

  * Add gnutls version to configure script
  * Add V8 heap info to process.memoryUsage()

  * process.watchFile callback has 2 arguments with the stat object
    (choonkeat@gmail.com)

## 2009.11.28, Version 0.1.19

https://github.com/iojs/io.js/commit/633d6be328708055897b72327b88ac88e158935f

  * Feature: Initial TLS support for TCP servers and clients.
    (Rhys Jones)

  * Add options to process.watchFile()
  * Add process.umask() (Friedemann Altrock)

  * Bugfix: only detach timers when active.
  * Bugfix: lib/file.js write(), shouldn't always emit errors or success
    (onne@onnlucky.com)

  * Bugfix: Memory leak in fs.write
    (Reported by onne@onnlucky.com)

  * Bugfix: Fix regular expressions detecting outgoing message headers.
    (Reported by Elliott Cable)

  * Improvements to Multipart parser (Felix Geisendörfer)
  * New HTTP parser

  * Upgrade v8 to 2.0.2

## 2009.11.17, Version 0.1.18

https://github.com/iojs/io.js/commit/027829d2853a14490e6de9fc5f7094652d045ab8

  * Feature: process.watchFile() process.unwatchFile()
  * Feature: "uncaughtException" event on process
    (Felix Geisendörfer)

  * Feature: 'drain' event to tcp.Connection
  * Bugfix: Promise.timeout() blocked the event loop
    (Felix Geisendörfer)

  * Bugfix: sendBody() and chunked utf8 strings
    (Felix Geisendörfer)

  * Supply the strerror as a second arg to the tcp.Connection close
    event (Johan Sørensen)

  * Add EventEmitter.removeListener (frodenius@gmail.com)
  * Format JSON for inspecting objects (Felix Geisendörfer)

  * Upgrade libev to latest CVS

## 2009.11.07, Version 0.1.17

https://github.com/iojs/io.js/commit/d1f69ef35dac810530df8249d523add168e09f03

  * Feature: process.chdir() (Brandon Beacher)
  * Revert http parser upgrade. (b893859c34f05db5c45f416949ebc0eee665cca6)
    Broke keep-alive.

  * API: rename process.inherits to sys.inherits

## 2009.11.03, Version 0.1.16

https://github.com/iojs/io.js/commit/726865af7bbafe58435986f4a193ff11c84e4bfe

  * API: Use CommonJS-style module requiring
    - require("/sys.js") becomes require("sys")
    - require("circle.js") becomes require("./circle")
    - process.path.join() becomes require("path").join()
    - __module becomes module

  * API: Many namespacing changes
    - Move node.* into process.*
    - Move node.dns into module "dns"
    - Move node.fs into module "posix"
    - process is no longer the global object. GLOBAL is.
  For more information on the API changes see:
    http://thread.gmane.org/gmane.comp.lang.javascript.nodejs/6
    http://thread.gmane.org/gmane.comp.lang.javascript.nodejs/14

  * Feature: process.platform, process.memoryUsage()
  * Feature: promise.cancel() (Felix Geisendörfer)

  * Upgrade V8 to 1.3.18

## 2009.10.28, Version 0.1.15

https://github.com/iojs/io.js/commit/eca2de73ed786b935507fd1c6faccd8df9938fd3

  * Many build system fixes (esp. for OSX users)
  * Feature: promise.timeout() (Felix Geisendörfer)

  * Feature: Added external interface for signal handlers, process.pid, and
    process.kill() (Brandon Beacher)

  * API: Rename node.libraryPaths to require.paths
  * Bugfix: 'data' event for stdio should emit a string

  * Large file support
  * Upgrade http_parser

  * Upgrade v8 to 1.3.16

## 2009.10.09, Version 0.1.14

https://github.com/iojs/io.js/commit/b12c809bb84d1265b6a4d970a5b54ee8a4890513

  * Feature: Improved addon builds with node-waf
  * Feature: node.SignalHandler (Brandon Beacher)

  * Feature: Enable V8 debugging (but still need to make a debugger)
  * API: Rename library /utils.js to /sys.js

  * Clean up Node's build system
  * Don't use parseUri for HTTP server

  * Remove node.pc
  * Don't use /bin/sh to create child process except with exec()

  * API: Add __module to reference current module
  * API: Remove include() add node.mixin()

  * Normalize http headers; "Content-Length" becomes "content-length"
  * Upgrade V8 to 1.3.15

## 2009.09.30, Version 0.1.13

https://github.com/iojs/io.js/commit/58493bb05b3da3dc8051fabc0bdea9e575c1a107

  * Feature: Multipart stream parser (Felix Geisendörfer)
  * API: Move node.puts(), node.exec() and others to /utils.js

  * API: Move http, tcp libraries to /http.js and /tcp.js
  * API: Rename node.exit() to process.exit()

  * Bugfix: require() and include() should work in callbacks.
  * Pass the Host header in http.cat calls

  * Add warning when coroutine stack size grows too large.
  * Enhance repl library (Ray Morgan)

  * Bugfix: build script for
      GCC 4.4 (removed -Werror in V8),
      on Linux 2.4,
      and with Python 2.4.4.

  * Add read() and write() to /file.js to read and write
    whole files at once.

## 2009.09.24, Version 0.1.12

https://github.com/iojs/io.js/commit/2f56ccb45e87510de712f56705598b3b4e3548ec

  * Feature: System modules, node.libraryPaths
  * API: Remove "raw" encoding, rename "raws" to "binary".

  * API: Added connection.setNoDElay() to disable Nagle algo.
  * Decrease default TCP server backlog to 128

  * Bugfix: memory leak involving node.fs.* methods.
  * Upgrade v8 to 1.3.13

## 2009.09.18, Version 0.1.11

https://github.com/iojs/io.js/commit/5ddc4f5d0c002bac0ae3d62fc0dc58f0d2d83ec4

  * API: default to utf8 encoding for node.fs.cat()
  * API: add node.exec()

  * API: node.fs.read() takes a normal encoding parameter.
  * API: Change arguments of emit(), emitSuccess(), emitError()

  * Bugfix: node.fs.write() was stack allocating buffer.
  * Bugfix: ReportException shouldn't forget the top frame.

  * Improve buffering for HTTP outgoing messages
  * Fix and reenable x64 macintosh build.

  * Upgrade v8 to 1.3.11

## 2009.09.11, Version 0.1.10

https://github.com/iojs/io.js/commit/12bb0d46ce761e3d00a27170e63b40408c15b558

  * Feature: raw string encoding "raws"
  * Feature: access to environ through "ENV"

  * Feature: add isDirectory, isFile, isSocket, ... methods
    to stats object.

  * Bugfix: Internally use full paths when loading modules
    this fixes a shebang loading problem.

  * Bugfix: Add '--' command line argument for seperating v8
    args from program args.

  * Add man page.
  * Add node-repl

  * Upgrade v8 to 1.3.10

## 2009.09.05, Version 0.1.9

https://github.com/iojs/io.js/commit/d029764bb32058389ecb31ed54a5d24d2915ad4c

  * Bugfix: Compile on Snow Leopard.
  * Bugfix: Malformed URIs raising exceptions.
## 2009.09.04, Version 0.1.8

https://github.com/iojs/io.js/commit/e6d712a937b61567e81b15085edba863be16ba96

  * Feature: External modules
  * Feature: setTimeout() for node.tcp.Connection

  * Feature: add node.cwd(), node.fs.readdir(), node.fs.mkdir()
  * Bugfix: promise.wait() releasing out of order.

  * Bugfix: Asyncly do getaddrinfo() on Apple.
  * Disable useless evcom error messages.

  * Better stack traces.
  * Built natively on x64.

  * Upgrade v8 to 1.3.9
## 2009.08.27, Version 0.1.7

https://github.com/iojs/io.js/commit/f7acef9acf8ba8433d697ad5ed99d2e857387e4b

  * Feature: global 'process' object. Emits "exit".
  * Feature: promise.wait()

  * Feature: node.stdio
  * Feature: EventEmitters emit "newListener" when listeners are
    added

  * API:  Use flat object instead of array-of-arrays for HTTP
    headers.

  * API: Remove buffered file object (node.File)
  * API: require(), include() are synchronous. (Uses
    continuations.)

  * API: Deprecate onLoad and onExit.
  * API: Rename node.Process to node.ChildProcess

  * Refactor node.Process to take advantage of evcom_reader/writer.
  * Upgrade v8 to 1.3.7
## 2009.08.22, Version 0.1.6

https://github.com/iojs/io.js/commit/9c97b1db3099d61cd292aa59ec2227a619f3a7ab

  * Bugfix: Ignore SIGPIPE.
## 2009.08.21, Version 0.1.5

https://github.com/iojs/io.js/commit/b0fd3e281cb5f7cd8d3a26bd2b89e1b59998e5ed

  * Bugfix: Buggy connections could crash node.js. Now check
    connection before sending data every time (Kevin van Zonneveld)

  * Bugfix: stdin fd (0) being ignored by node.File. (Abe Fettig)
  * API: Remove connnection.fullClose()

  * API: Return the EventEmitter from addListener for chaining.
  * API: tcp.Connection "disconnect" event renamed to "close"

  * Upgrade evcom
    Upgrade v8 to 1.3.6
## 2009.08.13, Version 0.1.4

https://github.com/iojs/io.js/commit/0f888ed6de153f68c17005211d7e0f960a5e34f3

  * Major refactor to evcom.
  * Enable test-tcp-many-clients.

  * Add -m32 gcc flag to udns.
  * Add connection.readPause() and connection.readResume()
    Add IncomingMessage.prototype.pause() and resume().

  * Fix http benchmark. Wasn't correctly dispatching.
  * Bugfix: response.setBodyEncoding("ascii") not working.

  * Bugfix: Negative ints in HTTP's on_body and node.fs.read()
  * Upgrade v8 to 1.3.4
    Upgrade libev to 3.8
    Upgrade http_parser to v0.2
## 2009.08.06, Version 0.1.3

https://github.com/iojs/io.js/commit/695f0296e35b30cf8322fd1bd934810403cca9f3

  * Upgrade v8 to 1.3.2
  * Bugfix: node.http.ServerRequest.setBodyEncoding('ascii') not
    working

  * Bugfix: node.encodeUtf8 was broken. (Connor Dunn)
  * Add ranlib to udns Makefile.

  * Upgrade evcom - fix accepting too many connections issue.
  * Initial support for shebang

  * Add simple command line switches
  * Add node.version API

## 2009.08.01, Version 0.1.2

https://github.com/iojs/io.js/commit/025a34244d1cea94d6d40ad7bf92671cb909a96c

  * Add DNS API
  * node.tcp.Server's backlog option is now an argument to listen()

  * Upgrade V8 to 1.3.1
  * Bugfix: Default to chunked for client requests without
    Content-Length.

  * Bugfix: Line numbers in stack traces.
  * Bugfix: negative integers in raw encoding stream

  * Bugfix: node.fs.File was not passing args to promise callbacks.

## 2009.07.27, Version 0.1.1

https://github.com/iojs/io.js/commit/77d407df2826b20e9177c26c0d2bb4481e497937

  * Simplify and clean up ObjectWrap.
  * Upgrade liboi (which is now called evcom)
    Upgrade libev to 3.7
    Upgrade V8 to 1.2.14

  * Array.prototype.encodeUtf8 renamed to node.encodeUtf8(array)
  * Move EventEmitter.prototype.emit() completely into C++.

  * Bugfix: Fix memory leak in event emitters.
    http://groups.google.com/group/nodejs/browse_thread/thread/a8d1dfc2fd57a6d1

  * Bugfix: Had problems reading scripts with non-ascii characters.
  * Bugfix: Fix Detach() in node::Server

  * Bugfix: Sockets not properly reattached if reconnected during
    disconnect event.

  * Bugfix: Server-side clients not attached between creation and
    on_connect.

  * Add 'close' event to node.tcp.Server
  * Simplify and clean up http.js. (Takes more advantage of event
    infrastructure.)

  * Add benchmark scripts. Run with "make benchmark".

## 2009.06.30, Version 0.1.0

https://github.com/iojs/io.js/commit/0fe44d52fe75f151bceb59534394658aae6ac328

  * Update documentation, use asciidoc.
  * EventEmitter and Promise interfaces. (Breaks previous API.)

  * Remove node.Process constructor in favor of node.createProcess
  * Add -m32 flags for compiling on x64 platforms.
    (Thanks to András Bártházi)

  * Upgrade v8 to 1.2.10 and libev to 3.6
  * Bugfix: Timer::RepeatSetter wasn't working.

  * Bugfix: Spawning many processes in a loop
    (reported by Felix Geisendörfer)

## 2009.06.24, Version 0.0.6

https://github.com/iojs/io.js/commit/fbe0be19ebfb422d8fa20ea5204c1713e9214d5f

  * Load modules via HTTP URLs (Urban Hafner)
  * Bugfix: Add HTTPConnection->size() and HTTPServer->size()

  * New node.Process API
  * Clean up build tools, use v8's test runner.

  * Use ev_unref() instead of starting/stopping the eio thread
    pool watcher.

## 2009.06.18, Version 0.0.5

https://github.com/iojs/io.js/commit/3a2b41de74b6c343b8464a68eff04c4bfd9aebea

  * Support for IPv6
  * Remove namespace node.constants

  * Upgrade v8 to 1.2.8.1
  * Accept ports as strings in the TCP client and server.

  * Bugfix: HTTP Client race
  * Bugfix: freeaddrinfo() wasn't getting called after
    getaddrinfo() for TCP servers

  * Add "opening" to TCP client readyState
  * Add remoteAddress to TCP client

  * Add global print() function.

## 2009.06.13, Version 0.0.4

https://github.com/iojs/io.js/commit/916b9ca715b229b0703f0ed6c2fc065410fb189c

 * Add interrupt() method to server-side HTTP requests.
 * Bugfix: onBodyComplete was not getting called on server-side
   HTTP

## 2009.06.11, Version 0.0.3

https://github.com/iojs/io.js/commit/6e0dfe50006ae4f5dac987f055e0c9338662f40a

 * Many bug fixes including the problem with http.Client on
   macintosh

 * Upgrades v8 to 1.2.7
 * Adds onExit hook

 * Guard against buffer overflow in http parser
 * require() and include() now need the ".js" extension

 * http.Client uses identity transfer encoding by default.
