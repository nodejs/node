# Node.js 18 ChangeLog

<!--lint disable maximum-line-length no-literal-urls prohibited-strings-->

<table>
<tr>
<th>Current</th>
</tr>
<tr>
<td>
<a href="#18.0.0">18.0.0</a><br/>
</td>
</tr>
</table>

* Other Versions
  * [17.x](CHANGELOG_V17.md)
  * [16.x](CHANGELOG_V16.md)
  * [15.x](CHANGELOG_V15.md)
  * [14.x](CHANGELOG_V14.md)
  * [13.x](CHANGELOG_V13.md)
  * [12.x](CHANGELOG_V12.md)
  * [11.x](CHANGELOG_V11.md)
  * [10.x](CHANGELOG_V10.md)
  * [9.x](CHANGELOG_V9.md)
  * [8.x](CHANGELOG_V8.md)
  * [7.x](CHANGELOG_V7.md)
  * [6.x](CHANGELOG_V6.md)
  * [5.x](CHANGELOG_V5.md)
  * [4.x](CHANGELOG_V4.md)
  * [0.12.x](CHANGELOG_V012.md)
  * [0.10.x](CHANGELOG_V010.md)
  * [io.js](CHANGELOG_IOJS.md)
  * [Archive](CHANGELOG_ARCHIVE.md)

<a id="18.0.0"></a>

## 2022-04-19, Version 18.0.0 (Current), @BethGriggs

Node.js 18 is here! Highlights include the update of the V8 JavaScript engine to 10.1, global fetch enabled by default, and a core test runner module.

Initially, Node.js 18 will replace Node.js 17 as our ‘Current’ release line. As per the release schedule, Node.js 18 will be the ‘Current’ release for the next 6 months and then promoted to Long-term Support (LTS) in October 2022. Once promoted to long-term support the release will be designated the codename ‘Hydrogen’. Node.js 18 will be supported until April 2025.

### Notable Changes

#### Deprecations and Removals

* **(SEMVER-MAJOR)** **fs**: runtime deprecate string coercion in `fs.write`, `fs.writeFileSync` (Livia Medeiros) [#42607](https://github.com/nodejs/node/pull/42607)
* **(SEMVER-MAJOR)** **dns**: remove `dns.lookup` and `dnsPromises.lookup` options type coercion (Antoine du Hamel) [#41431](https://github.com/nodejs/node/pull/41431)
* **(SEMVER-MAJOR)** **process**: runtime deprecate multipleResolves (Benjamin Gruenbaum) [#41896](https://github.com/nodejs/node/pull/41896)
* **(SEMVER-MAJOR)** **stream**: remove thenable support (Robert Nagy) [#40773](https://github.com/nodejs/node/pull/40773)
* **(SEMVER-MAJOR)** **tls**: move tls.parseCertString to end-of-life (Tobias Nießen) [#41479](https://github.com/nodejs/node/pull/41479)

#### fetch (experimental)

An experimental fetch API is available on the global scope by default. The implementation is based upon [undici](https://undici.nodejs.org/#/), an HTTP/1.1 client written for Node.js by contributors to the project.

```mjs
const res = await fetch('https://nodejs.org/api/documentation.json');
if (res.ok) {
  const data = await res.json();
  console.log(data);
}
```

Through this addition, the following globals are made available: `fetch`, `FormData`, `Headers`, `Request`, `Response`.

Disable this API with the `--no-experimental-fetch` command-line flag.

Contributed by Michaël Zasso in [#41811](https://github.com/nodejs/node/pull/41811).

#### HTTP Timeouts

`server.headersTimeout` which limits the amount of time the parser will wait to receive the complete HTTP headers is now set to `60000` (60 seconds) by default.

`server.requestTimeout` which sets the timeout value in milliseconds for receiving the entire request from the client is now set to `300000` (5 minutes) by default.

If these timeouts expire, the server responds with status 408 without forwarding the request to the request listener and then closes the connection.

Both timeouts must be set to a non-zero value to protect against potential Denial-of-Service attacks in case the server is deployed without a reverse proxy in front.

Contributed by Paolo Insogna in [#41263](https://github.com/nodejs/node/pull/41263).

#### Test Runner module (experimental)

The `node:test` module facilitates the creation of JavaScript tests that report results in TAP format. To access it:

`import test from 'node:test';`

This module is only available under the `node:` scheme.

The following is an example implementation of a parent test with two subtests:

```js
test('top level test', async (t) => {
  await t.test('subtest 1', (t) => {
    assert.strictEqual(1, 1);
  });

  await t.test('subtest 2', (t) => {
    assert.strictEqual(2, 2);
  });
});
```

Read more in <https://nodejs.org/dist/latest-v18.x/docs/api/test.html>.

Contributed by Colin Ihrig in [#42325](https://github.com/nodejs/node/pull/42325).

#### Toolchain and Compiler Upgrades

* Prebuilt binaries for Linux are now built on Red Hat Enterprise Linux (RHEL) 8 and are compatible with Linux distributions based on glibc 2.28 or later, for example, Debian 10, RHEL 8, Ubuntu 20.04.
* Prebuilt binaries for macOS now require macOS 10.15 or later.
* For AIX the minimum supported architecture has been raised from Power 7 to Power 8.

Prebuilt binaries for 32-bit Windows will initially not be available due to issues building the V8 dependency in Node.js. We hope to restore 32-bit Windows binaries for Node.js 18 with a future V8 update.

Node.js does not support running on operating systems that are no longer supported by their vendor. For operating systems where their vendor has planned to end support earlier than April 2025, such as Windows 8.1 (January 2023) and Windows Server 2012 R2 (October 2023), support for Node.js 18 will end at the earlier date.

Full details about the supported toolchains and compilers are documented in the Node.js [BUILDING.md](https://github.com/nodejs/node/blob/v18.x/BUILDING.md#supported-platforms) file.

Contributed by Richard Lau in [#42292](https://github.com/nodejs/node/pull/42292), [#42604](https://github.com/nodejs/node/pull/42604) and [#42659](https://github.com/nodejs/node/pull/42659),and Michaël Zasso in [#42105](https://github.com/nodejs/node/pull/42105) and [#42666](https://github.com/nodejs/node/pull/42666).

#### V8 10.1

The V8 engine is updated to version 10.1, which is part of Chromium 101. Compared to the version included in Node.js 17.9.0, the following new features are included:

* The [`findLast` and `findLastIndex` array methods](https://v8.dev/features/finding-in-arrays).
* Improvements to the [`Intl.Locale` API](https://v8.dev/blog/v8-release-99#intl.locale-extensions).
* The [`Intl.supportedValuesOf` function](https://v8.dev/blog/v8-release-99#intl-enumeration).
* Improved performance of [class fields](https://bugs.chromium.org/p/v8/issues/detail?id=9888) and [private class methods](https://bugs.chromium.org/p/v8/issues/detail?id=10793) (the initialization of them is now as fast as ordinary property stores).

The data format returned by the serialization API (`v8.serialize(value)`) has changed, and cannot be deserialized by earlier versions of Node.js. On the other hand, it is still possible to deserialize the previous format, as the API is backwards-compatible.

Contributed by Michaël Zasso in <https://github.com/nodejs/node/pull/42657>.

#### Web Streams API (experimental)

Node.js now exposes the experimental implementation of the [Web Streams API](https://developer.mozilla.org/en-US/docs/Web/API/Streams_API) on the global scope. This means the following APIs are now globally available:

* `ReadableStream`, `ReadableStreamDefaultReader`, `ReadableStreamBYOBReader`, `ReadableStreamBYOBRequest`, `ReadableByteStreamController`, `ReadableStreamDefaultController`, `TransformStream`, `TransformStreamDefaultController`, `WritableStream`, `WritableStreamDefaultWriter`, `WritableStreamDefaultController`, `ByteLengthQueuingStrategy`, `CountQueuingStrategy`, `TextEncoderStream`, `TextDecoderStream`, `CompressionStream`, `DecompressionStream`.

Contributed James Snell in <https://github.com/nodejs/node/pull/39062>, and Antoine du Hamel in <https://github.com/nodejs/node/pull/42225>.

#### Other Notable Changes

* **(SEMVER-MAJOR)** **buffer**: expose Blob as a global (James M Snell) [#41270](https://github.com/nodejs/node/pull/41270)
* **(SEMVER-MAJOR)** **child\_process**: improve argument validation (Rich Trott) [#41305](https://github.com/nodejs/node/pull/41305)
* **doc**: add RafaelGSS to collaborators (RafaelGSS) [#42718](https://github.com/nodejs/node/pull/42718)
* **(SEMVER-MAJOR)** **http**: make TCP noDelay enabled by default (Paolo Insogna) [#42163](https://github.com/nodejs/node/pull/42163)
* **(SEMVER-MAJOR)** **net**: make `server.address()` return an integer for `family` (Antoine du Hamel) [#41431](https://github.com/nodejs/node/pull/41431)
* **(SEMVER-MAJOR)** **worker**: expose BroadcastChannel as a global (James M Snell) [#41271](https://github.com/nodejs/node/pull/41271)
* **(SEMVER-MAJOR)** **worker**: graduate BroadcastChannel to supported (James M Snell) [#41271](https://github.com/nodejs/node/pull/41271)

### Semver-Major Commits

* \[[`dab8ab2837`](https://github.com/nodejs/node/commit/dab8ab2837)] - **(SEMVER-MAJOR)** **assert,util**: compare RegExp.lastIndex while using deep equal checks (Ruben Bridgewater) [#41020](https://github.com/nodejs/node/pull/41020)
* \[[`cff14bcaef`](https://github.com/nodejs/node/commit/cff14bcaef)] - **(SEMVER-MAJOR)** **buffer**: refactor `byteLength` to remove outdated optimizations (Rongjian Zhang) [#38545](https://github.com/nodejs/node/pull/38545)
* \[[`cea76dbf33`](https://github.com/nodejs/node/commit/cea76dbf33)] - **(SEMVER-MAJOR)** **buffer**: expose Blob as a global (James M Snell) [#41270](https://github.com/nodejs/node/pull/41270)
* \[[`99c18f4786`](https://github.com/nodejs/node/commit/99c18f4786)] - **(SEMVER-MAJOR)** **buffer**: graduate Blob from experimental (James M Snell) [#41270](https://github.com/nodejs/node/pull/41270)
* \[[`35d72bf4ec`](https://github.com/nodejs/node/commit/35d72bf4ec)] - **(SEMVER-MAJOR)** **build**: make x86 Windows support temporarily experimental (Michaël Zasso) [#42666](https://github.com/nodejs/node/pull/42666)
* \[[`1134d8faf8`](https://github.com/nodejs/node/commit/1134d8faf8)] - **(SEMVER-MAJOR)** **build**: bump macOS deployment target to 10.15 (Richard Lau) [#42292](https://github.com/nodejs/node/pull/42292)
* \[[`27eb91d378`](https://github.com/nodejs/node/commit/27eb91d378)] - **(SEMVER-MAJOR)** **build**: downgrade Windows 8.1 and server 2012 R2 to experimental (Michaël Zasso) [#42105](https://github.com/nodejs/node/pull/42105)
* \[[`26c973d4b3`](https://github.com/nodejs/node/commit/26c973d4b3)] - **(SEMVER-MAJOR)** **child\_process**: improve argument validation (Rich Trott) [#41305](https://github.com/nodejs/node/pull/41305)
* \[[`38007df999`](https://github.com/nodejs/node/commit/38007df999)] - **(SEMVER-MAJOR)** **cluster**: make `kill` to be just `process.kill` (Bar Admoni) [#34312](https://github.com/nodejs/node/pull/34312)
* \[[`aed18dfe59`](https://github.com/nodejs/node/commit/aed18dfe59)] - **(SEMVER-MAJOR)** **crypto**: cleanup validation (Mohammed Keyvanzadeh) [#39841](https://github.com/nodejs/node/pull/39841)
* \[[`e1fb6ae02f`](https://github.com/nodejs/node/commit/e1fb6ae02f)] - **(SEMVER-MAJOR)** **crypto**: prettify othername in PrintGeneralName (Tobias Nießen) [#42123](https://github.com/nodejs/node/pull/42123)
* \[[`36fb79030e`](https://github.com/nodejs/node/commit/36fb79030e)] - **(SEMVER-MAJOR)** **crypto**: fix X509Certificate toLegacyObject (Tobias Nießen) [#42124](https://github.com/nodejs/node/pull/42124)
* \[[`563b2ed000`](https://github.com/nodejs/node/commit/563b2ed000)] - **(SEMVER-MAJOR)** **crypto**: use RFC2253 format in PrintGeneralName (Tobias Nießen) [#42002](https://github.com/nodejs/node/pull/42002)
* \[[`18365d8ee6`](https://github.com/nodejs/node/commit/18365d8ee6)] - **(SEMVER-MAJOR)** **crypto**: change default check(Host|Email) behavior (Tobias Nießen) [#41600](https://github.com/nodejs/node/pull/41600)
* \[[`58f3fdcccd`](https://github.com/nodejs/node/commit/58f3fdcccd)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick semver-major commits from 10.2 (Michaël Zasso) [#42657](https://github.com/nodejs/node/pull/42657)
* \[[`fd4f80ce54`](https://github.com/nodejs/node/commit/fd4f80ce54)] - **(SEMVER-MAJOR)** **deps**: update V8 to 10.1.124.6 (Michaël Zasso) [#42657](https://github.com/nodejs/node/pull/42657)
* \[[`974ab4060f`](https://github.com/nodejs/node/commit/974ab4060f)] - **(SEMVER-MAJOR)** **deps**: update V8 to 9.8.177.9 (Michaël Zasso) [#41610](https://github.com/nodejs/node/pull/41610)
* \[[`270253c4e2`](https://github.com/nodejs/node/commit/270253c4e2)] - **(SEMVER-MAJOR)** **deps**: update V8 to 9.7.106.18 (Michaël Zasso) [#40907](https://github.com/nodejs/node/pull/40907)
* \[[`08773e3c04`](https://github.com/nodejs/node/commit/08773e3c04)] - **(SEMVER-MAJOR)** **dns**: remove `dns.lookup` and `dnsPromises.lookup` options type coercion (Antoine du Hamel) [#41431](https://github.com/nodejs/node/pull/41431)
* \[[`3671cc0432`](https://github.com/nodejs/node/commit/3671cc0432)] - **(SEMVER-MAJOR)** **doc**: update minimum glibc requirements for Linux (Richard Lau) [#42659](https://github.com/nodejs/node/pull/42659)
* \[[`646e057680`](https://github.com/nodejs/node/commit/646e057680)] - **(SEMVER-MAJOR)** **doc**: update AIX minimum supported arch (Richard Lau) [#42604](https://github.com/nodejs/node/pull/42604)
* \[[`0bac5478eb`](https://github.com/nodejs/node/commit/0bac5478eb)] - **(SEMVER-MAJOR)** **fs**: runtime deprecate string coercion in `fs.write`, `fs.writeFileSync` (Livia Medeiros) [#42607](https://github.com/nodejs/node/pull/42607)
* \[[`3caa2c1a00`](https://github.com/nodejs/node/commit/3caa2c1a00)] - **(SEMVER-MAJOR)** **http**: refactor headersTimeout and requestTimeout logic (Paolo Insogna) [#41263](https://github.com/nodejs/node/pull/41263)
* \[[`eacd45656a`](https://github.com/nodejs/node/commit/eacd45656a)] - **(SEMVER-MAJOR)** **http**: make TCP noDelay enabled by default (Paolo Insogna) [#42163](https://github.com/nodejs/node/pull/42163)
* \[[`4944ad0b9e`](https://github.com/nodejs/node/commit/4944ad0b9e)] - **(SEMVER-MAJOR)** **lib**: enable fetch by default (Michaël Zasso) [#41811](https://github.com/nodejs/node/pull/41811)
* \[[`8c4b8b201a`](https://github.com/nodejs/node/commit/8c4b8b201a)] - **(SEMVER-MAJOR)** **lib**: replace validator and error (Mohammed Keyvanzadeh) [#41678](https://github.com/nodejs/node/pull/41678)
* \[[`3c4ee5267a`](https://github.com/nodejs/node/commit/3c4ee5267a)] - **(SEMVER-MAJOR)** **module,repl**: support 'node:'-only core modules (Colin Ihrig) [#42325](https://github.com/nodejs/node/pull/42325)
* \[[`3a26db9697`](https://github.com/nodejs/node/commit/3a26db9697)] - **(SEMVER-MAJOR)** **net**: make `server.address()` return an integer for `family` (Antoine du Hamel) [#41431](https://github.com/nodejs/node/pull/41431)
* \[[`e6a7300a10`](https://github.com/nodejs/node/commit/e6a7300a10)] - **(SEMVER-MAJOR)** **process**: disallow some uses of Object.defineProperty() on process.env (Himself65) [#28006](https://github.com/nodejs/node/pull/28006)
* \[[`60b8e79599`](https://github.com/nodejs/node/commit/60b8e79599)] - **(SEMVER-MAJOR)** **process**: runtime deprecate multipleResolves (Benjamin Gruenbaum) [#41896](https://github.com/nodejs/node/pull/41896)
* \[[`d36b60e69a`](https://github.com/nodejs/node/commit/d36b60e69a)] - **(SEMVER-MAJOR)** **readline**: fix question still called after closed (Xuguang Mei) [#42464](https://github.com/nodejs/node/pull/42464)
* \[[`58e645de63`](https://github.com/nodejs/node/commit/58e645de63)] - **(SEMVER-MAJOR)** **stream**: remove thenable support (Robert Nagy) [#40773](https://github.com/nodejs/node/pull/40773)
* \[[`560cbc5849`](https://github.com/nodejs/node/commit/560cbc5849)] - **(SEMVER-MAJOR)** **stream**: expose web streams globals, remove runtime experimental warning (Antoine du Hamel) [#42225](https://github.com/nodejs/node/pull/42225)
* \[[`9fb7ac3bbd`](https://github.com/nodejs/node/commit/9fb7ac3bbd)] - **(SEMVER-MAJOR)** **stream**: need to cleanup event listeners if last stream is readable (Xuguang Mei) [#41954](https://github.com/nodejs/node/pull/41954)
* \[[`ceaa299958`](https://github.com/nodejs/node/commit/ceaa299958)] - **(SEMVER-MAJOR)** **stream**: revert revert `map` spec compliance (Benjamin Gruenbaum) [#41933](https://github.com/nodejs/node/pull/41933)
* \[[`fe7ca085a7`](https://github.com/nodejs/node/commit/fe7ca085a7)] - **(SEMVER-MAJOR)** **stream**: throw invalid arg type from End Of Stream (Jithil P Ponnan) [#41766](https://github.com/nodejs/node/pull/41766)
* \[[`48e784043d`](https://github.com/nodejs/node/commit/48e784043d)] - **(SEMVER-MAJOR)** **stream**: don't emit finish after destroy (Robert Nagy) [#40852](https://github.com/nodejs/node/pull/40852)
* \[[`f2170253b6`](https://github.com/nodejs/node/commit/f2170253b6)] - **(SEMVER-MAJOR)** **stream**: add errored and closed props (Robert Nagy) [#40696](https://github.com/nodejs/node/pull/40696)
* \[[`432d1b50e0`](https://github.com/nodejs/node/commit/432d1b50e0)] - **(SEMVER-MAJOR)** **test**: add initial test module (Colin Ihrig) [#42325](https://github.com/nodejs/node/pull/42325)
* \[[`92567283f4`](https://github.com/nodejs/node/commit/92567283f4)] - **(SEMVER-MAJOR)** **timers**: refactor internal classes to ES2015 syntax (Rabbit) [#37408](https://github.com/nodejs/node/pull/37408)
* \[[`65910c0d6c`](https://github.com/nodejs/node/commit/65910c0d6c)] - **(SEMVER-MAJOR)** **tls**: represent registeredID numerically always (Tobias Nießen) [#41561](https://github.com/nodejs/node/pull/41561)
* \[[`807c7e14f4`](https://github.com/nodejs/node/commit/807c7e14f4)] - **(SEMVER-MAJOR)** **tls**: move tls.parseCertString to end-of-life (Tobias Nießen) [#41479](https://github.com/nodejs/node/pull/41479)
* \[[`f524306077`](https://github.com/nodejs/node/commit/f524306077)] - **(SEMVER-MAJOR)** **url**: throw on NULL in IPv6 hostname (Rich Trott) [#42313](https://github.com/nodejs/node/pull/42313)
* \[[`0187bc5cdc`](https://github.com/nodejs/node/commit/0187bc5cdc)] - **(SEMVER-MAJOR)** **v8**: make v8.writeHeapSnapshot() error codes consistent (Darshan Sen) [#42577](https://github.com/nodejs/node/pull/42577)
* \[[`74b9baa426`](https://github.com/nodejs/node/commit/74b9baa426)] - **(SEMVER-MAJOR)** **v8**: make writeHeapSnapshot throw if fopen fails (Antonio Román) [#41373](https://github.com/nodejs/node/pull/41373)
* \[[`ce4d3adf50`](https://github.com/nodejs/node/commit/ce4d3adf50)] - **(SEMVER-MAJOR)** **worker**: expose BroadcastChannel as a global (James M Snell) [#41271](https://github.com/nodejs/node/pull/41271)
* \[[`6486a304d3`](https://github.com/nodejs/node/commit/6486a304d3)] - **(SEMVER-MAJOR)** **worker**: graduate BroadcastChannel to supported (James M Snell) [#41271](https://github.com/nodejs/node/pull/41271)

### Semver-Minor Commits

* \[[`415726b8c4`](https://github.com/nodejs/node/commit/415726b8c4)] - **(SEMVER-MINOR)** **stream**: add writableAborted (Robert Nagy) [#40802](https://github.com/nodejs/node/pull/40802)
* \[[`54819f08e0`](https://github.com/nodejs/node/commit/54819f08e0)] - **(SEMVER-MINOR)** **test\_runner**: support 'only' tests (Colin Ihrig) [#42514](https://github.com/nodejs/node/pull/42514)

### Semver-Patch Commits

* \[[`7533d08b94`](https://github.com/nodejs/node/commit/7533d08b94)] - **buffer**: fix `atob` input validation (Austin Kelleher) [#42662](https://github.com/nodejs/node/pull/42662)
* \[[`96673bcb96`](https://github.com/nodejs/node/commit/96673bcb96)] - **build**: run clang-format on CI (Darshan Sen) [#42681](https://github.com/nodejs/node/pull/42681)
* \[[`d5462e4558`](https://github.com/nodejs/node/commit/d5462e4558)] - **build**: reset embedder string to "-node.0" (Michaël Zasso) [#42657](https://github.com/nodejs/node/pull/42657)
* \[[`aa52873887`](https://github.com/nodejs/node/commit/aa52873887)] - **build**: add configure option --v8-enable-short-builtin-calls (daomingq) [#42109](https://github.com/nodejs/node/pull/42109)
* \[[`7ee8a7a463`](https://github.com/nodejs/node/commit/7ee8a7a463)] - **build**: reset embedder string to "-node.0" (Michaël Zasso) [#41610](https://github.com/nodejs/node/pull/41610)
* \[[`a189dee52a`](https://github.com/nodejs/node/commit/a189dee52a)] - **build**: reset embedder string to "-node.0" (Michaël Zasso) [#40907](https://github.com/nodejs/node/pull/40907)
* \[[`e8697cfe38`](https://github.com/nodejs/node/commit/e8697cfe38)] - **crypto**: improve prime size argument validation (Tobias Nießen) [#42234](https://github.com/nodejs/node/pull/42234)
* \[[`a9c0689786`](https://github.com/nodejs/node/commit/a9c0689786)] - **crypto**: fix return type prob reported by coverity (Michael Dawson) [#42135](https://github.com/nodejs/node/pull/42135)
* \[[`e938515b41`](https://github.com/nodejs/node/commit/e938515b41)] - **deps**: patch V8 to 10.1.124.8 (Michaël Zasso) [#42730](https://github.com/nodejs/node/pull/42730)
* \[[`eba7d2db7f`](https://github.com/nodejs/node/commit/eba7d2db7f)] - **deps**: V8: cherry-pick ad21d212fc14 (Michaël Zasso) [#42657](https://github.com/nodejs/node/pull/42657)
* \[[`004137e269`](https://github.com/nodejs/node/commit/004137e269)] - **deps**: V8: cherry-pick 4c29cf1b7885 (Michaël Zasso) [#42657](https://github.com/nodejs/node/pull/42657)
* \[[`a052c03033`](https://github.com/nodejs/node/commit/a052c03033)] - **deps**: V8: cherry-pick ca2a787a0b49 (Michaël Zasso) [#42657](https://github.com/nodejs/node/pull/42657)
* \[[`01cea9a8d8`](https://github.com/nodejs/node/commit/01cea9a8d8)] - **deps**: V8: cherry-pick a2cae2180a7a (Michaël Zasso) [#42657](https://github.com/nodejs/node/pull/42657)
* \[[`d9d26b08ef`](https://github.com/nodejs/node/commit/d9d26b08ef)] - **deps**: V8: cherry-pick 87ce4f5d98a5 (Michaël Zasso) [#42657](https://github.com/nodejs/node/pull/42657)
* \[[`64a6328505`](https://github.com/nodejs/node/commit/64a6328505)] - **deps**: make V8 compilable with older glibc (Michaël Zasso) [#42657](https://github.com/nodejs/node/pull/42657)
* \[[`fde59217b9`](https://github.com/nodejs/node/commit/fde59217b9)] - **deps**: V8: fix v8-cppgc.h for MSVC (Jiawen Geng) [#42657](https://github.com/nodejs/node/pull/42657)
* \[[`cdcc82cced`](https://github.com/nodejs/node/commit/cdcc82cced)] - **deps**: silence V8's warning on CompileFunction (Michaël Zasso) [#40907](https://github.com/nodejs/node/pull/40907)
* \[[`2f51e121da`](https://github.com/nodejs/node/commit/2f51e121da)] - **deps**: update Acorn to v8.7.0 (Michaël Zasso) [#42667](https://github.com/nodejs/node/pull/42667)
* \[[`6d4b01774b`](https://github.com/nodejs/node/commit/6d4b01774b)] - **deps**: update ICU to 71.1 (Michaël Zasso) [#42655](https://github.com/nodejs/node/pull/42655)
* \[[`2d84620f86`](https://github.com/nodejs/node/commit/2d84620f86)] - **deps**: upgrade npm to 8.6.0 (npm team) [#42550](https://github.com/nodejs/node/pull/42550)
* \[[`c7ac11fa25`](https://github.com/nodejs/node/commit/c7ac11fa25)] - **deps**: update undici to 5.0.0 (Node.js GitHub Bot) [#42583](https://github.com/nodejs/node/pull/42583)
* \[[`468fffdf66`](https://github.com/nodejs/node/commit/468fffdf66)] - **deps**: V8: cherry-pick 50d5fb7a457c (Michaël Zasso) [#41610](https://github.com/nodejs/node/pull/41610)
* \[[`48708be57b`](https://github.com/nodejs/node/commit/48708be57b)] - **deps**: V8: cherry-pick 79a9d2eb3477 (Michaël Zasso) [#41610](https://github.com/nodejs/node/pull/41610)
* \[[`3c8782f70e`](https://github.com/nodejs/node/commit/3c8782f70e)] - **deps**: silence V8's warning on CompileFunction (Michaël Zasso) [#40907](https://github.com/nodejs/node/pull/40907)
* \[[`9318408c49`](https://github.com/nodejs/node/commit/9318408c49)] - **deps**: silence V8's warning on CompileFunction (Michaël Zasso) [#40907](https://github.com/nodejs/node/pull/40907)
* \[[`e23e345b6c`](https://github.com/nodejs/node/commit/e23e345b6c)] - **deps**: V8: cherry-pick 80bbbb143c24 (Michaël Zasso) [#40907](https://github.com/nodejs/node/pull/40907)
* \[[`696ce7df26`](https://github.com/nodejs/node/commit/696ce7df26)] - **deps**: V8: cherry-pick 1cc12b278e22 (Michaël Zasso) [#40907](https://github.com/nodejs/node/pull/40907)
* \[[`aa88e5e4b9`](https://github.com/nodejs/node/commit/aa88e5e4b9)] - **doc**: revise data imports and node: imports sections (Rich Trott) [#42734](https://github.com/nodejs/node/pull/42734)
* \[[`a058cefe29`](https://github.com/nodejs/node/commit/a058cefe29)] - **doc**: fix ESM JSON/data URL import example (Rich Trott) [#42728](https://github.com/nodejs/node/pull/42728)
* \[[`e61b62b9d4`](https://github.com/nodejs/node/commit/e61b62b9d4)] - **doc**: improve doc for http.ServerResponse inheritance (Luigi Pinca) [#42693](https://github.com/nodejs/node/pull/42693)
* \[[`6669b3857f`](https://github.com/nodejs/node/commit/6669b3857f)] - **doc**: add RafaelGSS to collaborators (RafaelGSS) [#42718](https://github.com/nodejs/node/pull/42718)
* \[[`f825341bab`](https://github.com/nodejs/node/commit/f825341bab)] - **doc**: add NodeEdKeyGenParams to CryptoKey.algorithm (Tobias Nießen) [#42629](https://github.com/nodejs/node/pull/42629)
* \[[`d4d78361f2`](https://github.com/nodejs/node/commit/d4d78361f2)] - **doc**: fix the example for embedders (Momtchil Momtchev) [#42671](https://github.com/nodejs/node/pull/42671)
* \[[`6706be1cdb`](https://github.com/nodejs/node/commit/6706be1cdb)] - **doc**: change AES-GCM IV recommendation in WebCrypto (Tobias Nießen) [#42611](https://github.com/nodejs/node/pull/42611)
* \[[`4508c8caa4`](https://github.com/nodejs/node/commit/4508c8caa4)] - **doc**: fix `added:` info for some methods (Luigi Pinca) [#42661](https://github.com/nodejs/node/pull/42661)
* \[[`951dbc045a`](https://github.com/nodejs/node/commit/951dbc045a)] - **doc**: remove unneeded new in Buffer example (Niklas Mischkulnig) [#42682](https://github.com/nodejs/node/pull/42682)
* \[[`65e838071b`](https://github.com/nodejs/node/commit/65e838071b)] - **doc**: mark worker.id as integer in cluster docs (Tobias Nießen) [#42684](https://github.com/nodejs/node/pull/42684)
* \[[`a82713cbb6`](https://github.com/nodejs/node/commit/a82713cbb6)] - **doc**: recommend `fh.createWriteStream` for fsPromises methods (Antoine du Hamel) [#42653](https://github.com/nodejs/node/pull/42653)
* \[[`13ad8d4e09`](https://github.com/nodejs/node/commit/13ad8d4e09)] - **doc**: fix outgoingMessage.removeHeader() signature (Luigi Pinca) [#42652](https://github.com/nodejs/node/pull/42652)
* \[[`a0461255c0`](https://github.com/nodejs/node/commit/a0461255c0)] - **doc**: mark tlsSocket.authorized as boolean property (Tobias Nießen) [#42647](https://github.com/nodejs/node/pull/42647)
* \[[`3ac7f86c2b`](https://github.com/nodejs/node/commit/3ac7f86c2b)] - **doc**: add missing punctuation in Web Streams doc (Tobias Nießen) [#42672](https://github.com/nodejs/node/pull/42672)
* \[[`b98386c977`](https://github.com/nodejs/node/commit/b98386c977)] - **doc**: add missing article in session ticket section (Tobias Nießen) [#42632](https://github.com/nodejs/node/pull/42632)
* \[[`a113468383`](https://github.com/nodejs/node/commit/a113468383)] - **doc**: link to dynamic import function (Tobias Nießen) [#42634](https://github.com/nodejs/node/pull/42634)
* \[[`dfc2dc8b65`](https://github.com/nodejs/node/commit/dfc2dc8b65)] - **doc**: add note about header values encoding (Shogun) [#42624](https://github.com/nodejs/node/pull/42624)
* \[[`ec5a359ffd`](https://github.com/nodejs/node/commit/ec5a359ffd)] - **doc**: add missing word in rootCertificates section (Tobias Nießen) [#42633](https://github.com/nodejs/node/pull/42633)
* \[[`c08a361f70`](https://github.com/nodejs/node/commit/c08a361f70)] - **doc**: add history entries for DEP0162 on `fs.md` (Antoine du Hamel) [#42608](https://github.com/nodejs/node/pull/42608)
* \[[`4fade6acb4`](https://github.com/nodejs/node/commit/4fade6acb4)] - **doc**: fix brackets position (Livia Medeiros) [#42649](https://github.com/nodejs/node/pull/42649)
* \[[`8055c7ba5d`](https://github.com/nodejs/node/commit/8055c7ba5d)] - **doc**: copyedit corepack.md (Rich Trott) [#42620](https://github.com/nodejs/node/pull/42620)
* \[[`85a65c3260`](https://github.com/nodejs/node/commit/85a65c3260)] - **doc**: delete chakra tt from diagnostic tooling support tiers (Tony Gorez) [#42627](https://github.com/nodejs/node/pull/42627)
* \[[`63bb6dcf0f`](https://github.com/nodejs/node/commit/63bb6dcf0f)] - **doc**: align links in table to top (nikoladev) [#41396](https://github.com/nodejs/node/pull/41396)
* \[[`28d8614add`](https://github.com/nodejs/node/commit/28d8614add)] - **http**: document that ClientRequest inherits from OutgoingMessage (K.C.Ashish Kumar) [#42642](https://github.com/nodejs/node/pull/42642)
* \[[`c37fdacb34`](https://github.com/nodejs/node/commit/c37fdacb34)] - **lib**: use class fields in observe.js (Joyee Cheung) [#42361](https://github.com/nodejs/node/pull/42361)
* \[[`ea0668a27e`](https://github.com/nodejs/node/commit/ea0668a27e)] - **lib**: use class fields in Event and EventTarget (Joyee Cheung) [#42361](https://github.com/nodejs/node/pull/42361)
* \[[`eb7b89c829`](https://github.com/nodejs/node/commit/eb7b89c829)] - **lib**: update class fields TODO in abort\_controller.js (Joyee Cheung) [#42361](https://github.com/nodejs/node/pull/42361)
* \[[`d835b1f1c1`](https://github.com/nodejs/node/commit/d835b1f1c1)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#42677](https://github.com/nodejs/node/pull/42677)
* \[[`29492496e8`](https://github.com/nodejs/node/commit/29492496e8)] - **meta**: move one or more collaborators to emeritus (Node.js GitHub Bot) [#42599](https://github.com/nodejs/node/pull/42599)
* \[[`93c4dc5e5a`](https://github.com/nodejs/node/commit/93c4dc5e5a)] - **module**: ensure 'node:'-only modules can access node\_modules (Colin Ihrig) [#42430](https://github.com/nodejs/node/pull/42430)
* \[[`3a26db9697`](https://github.com/nodejs/node/commit/3a26db9697)] - **net**: make `server.address()` return an integer for `family` (Antoine du Hamel) [#41431](https://github.com/nodejs/node/pull/41431)
* \[[`44fdf953ba`](https://github.com/nodejs/node/commit/44fdf953ba)] - **node-api,src**: fix module registration in MSVC C++ (Vladimir Morozov) [#42459](https://github.com/nodejs/node/pull/42459)
* \[[`3026ca0bf2`](https://github.com/nodejs/node/commit/3026ca0bf2)] - **src**: fix coverity report (Michael Dawson) [#42663](https://github.com/nodejs/node/pull/42663)
* \[[`01fd048c6e`](https://github.com/nodejs/node/commit/01fd048c6e)] - **src**: update NODE\_MODULE\_VERSION to 108 (Michaël Zasso) [#42657](https://github.com/nodejs/node/pull/42657)
* \[[`75a71dc7ae`](https://github.com/nodejs/node/commit/75a71dc7ae)] - **src**: fix alphabetically sorted binding list (Tobias Nießen) [#42687](https://github.com/nodejs/node/pull/42687)
* \[[`457567f72c`](https://github.com/nodejs/node/commit/457567f72c)] - **src**: include crypto in the bootstrap snapshot (Joyee Cheung) [#42203](https://github.com/nodejs/node/pull/42203)
* \[[`aa7dc808f5`](https://github.com/nodejs/node/commit/aa7dc808f5)] - **src**: update ImportModuleDynamically (Camillo Bruni) [#41610](https://github.com/nodejs/node/pull/41610)
* \[[`fa0439e66c`](https://github.com/nodejs/node/commit/fa0439e66c)] - **src**: update NODE\_MODULE\_VERSION to 105 (Michaël Zasso) [#41610](https://github.com/nodejs/node/pull/41610)
* \[[`6ec1664dc8`](https://github.com/nodejs/node/commit/6ec1664dc8)] - **src**: update NODE\_MODULE\_VERSION to 104 (Michaël Zasso) [#40907](https://github.com/nodejs/node/pull/40907)
* \[[`a706342368`](https://github.com/nodejs/node/commit/a706342368)] - **src**: add kNoBrowserGlobals flag for Environment (Cheng Zhao) [#40532](https://github.com/nodejs/node/pull/40532)
* \[[`0c57a37dd0`](https://github.com/nodejs/node/commit/0c57a37dd0)] - **src,crypto**: remove uses of AllocatedBuffer from crypto\_tls.cc (Darshan Sen) [#42589](https://github.com/nodejs/node/pull/42589)
* \[[`be01185844`](https://github.com/nodejs/node/commit/be01185844)] - **src,inspector**: fix empty MaybeLocal crash (Darshan Sen) [#42409](https://github.com/nodejs/node/pull/42409)
* \[[`340b770d3f`](https://github.com/nodejs/node/commit/340b770d3f)] - **stream**: unify writableErrored and readableErrored (Robert Nagy) [#40799](https://github.com/nodejs/node/pull/40799)
* \[[`19064bec34`](https://github.com/nodejs/node/commit/19064bec34)] - **test**: delete test/pummel/test-repl-empty-maybelocal-crash.js (Darshan Sen) [#42720](https://github.com/nodejs/node/pull/42720)
* \[[`9d6af7d1fe`](https://github.com/nodejs/node/commit/9d6af7d1fe)] - **test**: improve `internal/url.js` coverage (Yoshiki Kurihara) [#42650](https://github.com/nodejs/node/pull/42650)
* \[[`d49df5ca8d`](https://github.com/nodejs/node/commit/d49df5ca8d)] - **test**: adapt message tests for V8 10.2 (Michaël Zasso) [#42657](https://github.com/nodejs/node/pull/42657)
* \[[`c6b4e9604f`](https://github.com/nodejs/node/commit/c6b4e9604f)] - **test**: adapt test-worker-debug for V8 10.0 (Michaël Zasso) [#42657](https://github.com/nodejs/node/pull/42657)
* \[[`0854fce8bc`](https://github.com/nodejs/node/commit/0854fce8bc)] - **test**: adapt test-v8-serdes for V8 9.9 (Michaël Zasso) [#42657](https://github.com/nodejs/node/pull/42657)
* \[[`73d53fe9f5`](https://github.com/nodejs/node/commit/73d53fe9f5)] - **test**: only skip slow tests on Raspberry Pi devices (Richard Lau) [#42645](https://github.com/nodejs/node/pull/42645)
* \[[`db7fa9f4b7`](https://github.com/nodejs/node/commit/db7fa9f4b7)] - **test**: allow numeric string for lookupService test (Daeyeon Jeong) [#42596](https://github.com/nodejs/node/pull/42596)
* \[[`0525a147b2`](https://github.com/nodejs/node/commit/0525a147b2)] - **test**: remove an unnecessary `undefined` in wpt (Khaidi Chu) [#41470](https://github.com/nodejs/node/pull/41470)
* \[[`bb762c5bd0`](https://github.com/nodejs/node/commit/bb762c5bd0)] - **test**: simplify test-http-write-callbacks.js (Tobias Nießen) [#42628](https://github.com/nodejs/node/pull/42628)
* \[[`1600869eb7`](https://github.com/nodejs/node/commit/1600869eb7)] - **test**: fix comments in test files (Daeyeon Jeong) [#42536](https://github.com/nodejs/node/pull/42536)
* \[[`82181bb9b8`](https://github.com/nodejs/node/commit/82181bb9b8)] - **test**: fix failure in test/sequential/test-heapdump.js (Darshan Sen) [#41772](https://github.com/nodejs/node/pull/41772)
* \[[`ba5b5acaf1`](https://github.com/nodejs/node/commit/ba5b5acaf1)] - **test**: improve `worker_threads ` coverage (Erick Wendel) [#41818](https://github.com/nodejs/node/pull/41818)
* \[[`f076c36335`](https://github.com/nodejs/node/commit/f076c36335)] - **tools**: update clang-format 1.6.0 to 1.7.0 (Rich Trott) [#42724](https://github.com/nodejs/node/pull/42724)
* \[[`45162bf9e7`](https://github.com/nodejs/node/commit/45162bf9e7)] - **tools**: update clang-format from 1.2.3 to 1.6.0 (Rich Trott) [#42685](https://github.com/nodejs/node/pull/42685)
* \[[`40bc08089d`](https://github.com/nodejs/node/commit/40bc08089d)] - **tools**: update V8 gypfiles for 10.1 (Michaël Zasso) [#42657](https://github.com/nodejs/node/pull/42657)
* \[[`09513cd1a3`](https://github.com/nodejs/node/commit/09513cd1a3)] - **tools**: update eslint to 8.13.0 (Node.js GitHub Bot) [#42678](https://github.com/nodejs/node/pull/42678)
* \[[`b99bb57416`](https://github.com/nodejs/node/commit/b99bb57416)] - **tools**: update gyp-next to v0.12.1 (Michaël Zasso) [#42625](https://github.com/nodejs/node/pull/42625)
* \[[`2468db1f53`](https://github.com/nodejs/node/commit/2468db1f53)] - **tools**: update lint-md-dependencies to @rollup/plugin-commonjs\@21.0.3 (Node.js GitHub Bot) [#42584](https://github.com/nodejs/node/pull/42584)
* \[[`8a3f28a05c`](https://github.com/nodejs/node/commit/8a3f28a05c)] - **tools**: add v8-embedder-state-scope.h to distributed headers (Michaël Zasso) [#41610](https://github.com/nodejs/node/pull/41610)
* \[[`30c4e1d952`](https://github.com/nodejs/node/commit/30c4e1d952)] - **tools**: update V8 gypfiles for 9.8 (Michaël Zasso) [#41610](https://github.com/nodejs/node/pull/41610)
* \[[`1ad44094a2`](https://github.com/nodejs/node/commit/1ad44094a2)] - **tools**: update V8 gypfiles for 9.7 (Michaël Zasso) [#40907](https://github.com/nodejs/node/pull/40907)
* \[[`86b77f7d0f`](https://github.com/nodejs/node/commit/86b77f7d0f)] - **tools,doc**: use V8::DisposePlatform (Michaël Zasso) [#41610](https://github.com/nodejs/node/pull/41610)
* \[[`62e62757b3`](https://github.com/nodejs/node/commit/62e62757b3)] - **tools,test**: fix V8 initialization order (Camillo Bruni) [#42657](https://github.com/nodejs/node/pull/42657)
* \[[`0187bc5cdc`](https://github.com/nodejs/node/commit/0187bc5cdc)] - **v8**: make v8.writeHeapSnapshot() error codes consistent (Darshan Sen) [#42577](https://github.com/nodejs/node/pull/42577)
* \[[`74b9baa426`](https://github.com/nodejs/node/commit/74b9baa426)] - **v8**: make writeHeapSnapshot throw if fopen fails (Antonio Román) [#41373](https://github.com/nodejs/node/pull/41373)
