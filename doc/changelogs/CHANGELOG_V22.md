# Node.js 22 ChangeLog

<!--lint disable maximum-line-length no-literal-urls prohibited-strings-->

<table>
<tr>
<th>LTS 'Jod'</th>
<th>Current</th>
</tr>
<tr>
<td>
<a href="#22.17.1">22.17.1</a><br/>
<a href="#22.17.0">22.17.0</a><br/>
<a href="#22.16.0">22.16.0</a><br/>
<a href="#22.15.1">22.15.1</a><br/>
<a href="#22.15.0">22.15.0</a><br/>
<a href="#22.14.0">22.14.0</a><br/>
<a href="#22.13.1">22.13.1</a><br/>
<a href="#22.13.0">22.13.0</a><br/>
<a href="#22.12.0">22.12.0</a><br/>
<a href="#22.11.0">22.11.0</a><br/>
</td>
<td>
<a href="#22.10.0">22.10.0</a><br/>
<a href="#22.9.0">22.9.0</a><br/>
<a href="#22.8.0">22.8.0</a><br/>
<a href="#22.7.0">22.7.0</a><br/>
<a href="#22.6.0">22.6.0</a><br/>
<a href="#22.5.1">22.5.1</a><br/>
<a href="#22.5.0">22.5.0</a><br/>
<a href="#22.4.1">22.4.1</a><br/>
<a href="#22.4.0">22.4.0</a><br/>
<a href="#22.3.0">22.3.0</a><br/>
<a href="#22.2.0">22.2.0</a><br/>
<a href="#22.1.0">22.1.0</a><br/>
<a href="#22.0.0">22.0.0</a><br/>
</td>
</tr>
</table>

* Other Versions
  * [21.x](CHANGELOG_V21.md)
  * [20.x](CHANGELOG_V20.md)
  * [19.x](CHANGELOG_V19.md)
  * [18.x](CHANGELOG_V18.md)
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

<a id="22.17.1"></a>

## 2025-07-15, Version 22.17.1 'Jod' (LTS), @RafaelGSS

This is a security release.

### Notable Changes

* (CVE-2025-27210) Windows Device Names (CON, PRN, AUX) Bypass Path Traversal Protection in path.normalize()

### Commits

* \[[`8cf5d66ab7`](https://github.com/nodejs/node/commit/8cf5d66ab7)] - **(CVE-2025-27210)** **lib**: handle all windows reserved driver name (RafaelGSS) [nodejs-private/node-private#721](https://github.com/nodejs-private/node-private/pull/721)
* \[[`9c0cb487ec`](https://github.com/nodejs/node/commit/9c0cb487ec)] - **win,build**: fix MSVS v17.14 compilation issue (StefanStojanovic) [#58902](https://github.com/nodejs/node/pull/58902)

<a id="22.17.0"></a>

## 2025-06-24, Version 22.17.0 'Jod' (LTS), @aduh95

### Notable Changes

#### ⚠️ Deprecations

##### Instantiating `node:http` classes **without `new`**

Constructing classes like `IncomingMessage` or `ServerResponse` without the `new`
keyword is now discouraged. This clarifies API expectations and aligns with standard
JavaScript behavior. It may warn or error in future versions.

Contributed by Yagiz Nizipli in [#58518](https://github.com/nodejs/node/pull/58518).

##### `options.shell = ""` in `node:child_process`

Using an empty string for `shell` previously had undefined behavior. This change
encourages explicit choices (e.g., `shell: true` or a shell path) and avoids
relying on implementation quirks.

Contributed by Antoine du Hamel and Renegade334 [#58564](https://github.com/nodejs/node/pull/58564).

##### HTTP/2 priority signaling

The HTTP/2 prioritization API (e.g., `stream.priority`) is now deprecated due to
poor real-world support. Applications should avoid using priority hints and expect future removal.

Contributed by Matteo Collina and Antoine du Hamel [#58313](https://github.com/nodejs/node/pull/58313).

#### ✅ Features graduated to stable

##### `assert.partialDeepStrictEqual()`

This method compares only a subset of properties in deep object comparisons,
useful for flexible test assertions. Its stabilization means it's now safe for
general use and won't change unexpectedly in future releases.

Contributed by Ruben Bridgewater in [#57370](https://github.com/nodejs/node/pull/57370).

#### Miscellaneous

* `dirent.parentPath`
* `filehandle.readableWebStream()`
* `fs.glob()`
* `fs.openAsBlob()`
* `node:readline/promises`
* `port.hasRef()`
* `readable.compose()`
* `readable.iterator()`
* `readable.readableAborted`
* `readable.readableDidRead`
* `Duplex.fromWeb()`
* `Duplex.toWeb()`
* `Readable.fromWeb()`
* `Readable.isDisturbed()`
* `Readable.toWeb()`
* `stream.isErrored()`
* `stream.isReadable()`
* `URL.createObjectURL()`
* `URL.revokeObjectURL()`
* `v8.setHeapSnapshotNearHeapLimit()`
* `Writable.fromWeb()`
* `Writable.toWeb()`
* `writable.writableAborted`
* Startup Snapshot API
* `ERR_INPUT_TYPE_NOT_ALLOWED`
* `ERR_UNKNOWN_FILE_EXTENSION`
* `ERR_UNKNOWN_MODULE_FORMAT`
* `ERR_USE_AFTER_CLOSE`

Contributed by James M Snell in
[#57513](https://github.com/nodejs/node/pull/57513) and
[#58541](https://github.com/nodejs/node/pull/58541).

#### Semver-minor features

##### 🔧 `fs.FileHandle.readableWebStream` gets `autoClose` option

This gives developers explicit control over whether the file descriptor should
be closed when the stream ends. Helps avoid subtle resource leaks.

Contributed by James M Snell in [#58548](https://github.com/nodejs/node/pull/58548).

##### 🔧 `fs.Dir` now supports **explicit resource management**

This improves ergonomics around async iteration of directories. Developers can
now manually control when a directory is closed using `.close()` or with `Symbol.asyncDispose`.

Contributed by Antoine du Hamel in [#58206](https://github.com/nodejs/node/pull/58206).

##### 📊 `http2` gains diagnostics channel: `http2.server.stream.finish`

Adds observability support for when a stream finishes. Useful for logging,
monitoring, and debugging HTTP/2 behavior without patching internals.

Contributed by Darshan Sen in [#58560](https://github.com/nodejs/node/pull/58560).

##### 🔐 Permissions: implicit allow-fs-read to entrypoint

Node.js permissions model now allows read access to the entry file by default.
It makes running permission-restricted apps smoother while preserving security.

Contributed by Rafael Gonzaga in [#58579](https://github.com/nodejs/node/pull/58579).

##### 🎨 `util.styleText()` adds `'none'` style

This lets developers remove styling cleanly without hacks. Useful for overriding
inherited terminal styles when composing styled strings.

Contributed by James M Snell in [#58437](https://github.com/nodejs/node/pull/58437).

#### 🧑‍💻 Community updates

* \[[`0105c13556`](https://github.com/nodejs/node/commit/0105c13556)] - **doc**: add Filip Skokan to TSC (Rafael Gonzaga) [#58499](https://github.com/nodejs/node/pull/58499)
* \[[`3b857735ef`](https://github.com/nodejs/node/commit/3b857735ef)] - **doc**: add JonasBa to collaborators (Jonas Badalic) [#58355](https://github.com/nodejs/node/pull/58355)
* \[[`fdf7612735`](https://github.com/nodejs/node/commit/fdf7612735)] - **doc**: add puskin to collaborators (Giovanni Bucci) [#58308](https://github.com/nodejs/node/pull/58308)

### Commits

* \[[`ffe7e1ace0`](https://github.com/nodejs/node/commit/ffe7e1ace0)] - **(SEMVER-MINOR)** **assert**: mark partialDeepStrictEqual() as stable (Ruben Bridgewater) [#57370](https://github.com/nodejs/node/pull/57370)
* \[[`269931f289`](https://github.com/nodejs/node/commit/269931f289)] - **async\_hooks**: ensure AsyncLocalStore instances work isolated (Gerhard Stöbich) [#58149](https://github.com/nodejs/node/pull/58149)
* \[[`9e0746a4ff`](https://github.com/nodejs/node/commit/9e0746a4ff)] - **benchmark**: fix broken fs.cpSync benchmark (Dario Piotrowicz) [#58472](https://github.com/nodejs/node/pull/58472)
* \[[`dee8cb5bcb`](https://github.com/nodejs/node/commit/dee8cb5bcb)] - **benchmark**: add more options to cp-sync (Sonny) [#58278](https://github.com/nodejs/node/pull/58278)
* \[[`e840fd5b85`](https://github.com/nodejs/node/commit/e840fd5b85)] - **benchmark**: fix typo in method name for error-stack (Miguel Marcondes Filho) [#58128](https://github.com/nodejs/node/pull/58128)
* \[[`b9a16e97e0`](https://github.com/nodejs/node/commit/b9a16e97e0)] - **buffer**: give names to `Buffer.prototype.*Write()` functions (Livia Medeiros) [#58258](https://github.com/nodejs/node/pull/58258)
* \[[`d56a5e40af`](https://github.com/nodejs/node/commit/d56a5e40af)] - **buffer**: use constexpr where possible (Yagiz Nizipli) [#58141](https://github.com/nodejs/node/pull/58141)
* \[[`215587feca`](https://github.com/nodejs/node/commit/215587feca)] - **build**: add support for OpenHarmony operating system (hqzing) [#58350](https://github.com/nodejs/node/pull/58350)
* \[[`9bcef6821c`](https://github.com/nodejs/node/commit/9bcef6821c)] - **build**: fix uvwasi pkgname (Antoine du Hamel) [#58270](https://github.com/nodejs/node/pull/58270)
* \[[`7c3883c2ae`](https://github.com/nodejs/node/commit/7c3883c2ae)] - **build**: search for libnode.so in multiple places (Jan Staněk) [#58213](https://github.com/nodejs/node/pull/58213)
* \[[`3f954accb3`](https://github.com/nodejs/node/commit/3f954accb3)] - **build**: fix pointer compression builds (Joyee Cheung) [#58171](https://github.com/nodejs/node/pull/58171)
* \[[`04c8f59f84`](https://github.com/nodejs/node/commit/04c8f59f84)] - **build**: use FILE\_OFFSET\_BITS=64 esp. on 32-bit arch (RafaelGSS) [#58090](https://github.com/nodejs/node/pull/58090)
* \[[`8c2cf3a372`](https://github.com/nodejs/node/commit/8c2cf3a372)] - **build**: use //third\_party/simdutf by default in GN (Shelley Vohr) [#58115](https://github.com/nodejs/node/pull/58115)
* \[[`cff8006792`](https://github.com/nodejs/node/commit/cff8006792)] - **child\_process**: give names to `ChildProcess` functions (Livia Medeiros) [#58370](https://github.com/nodejs/node/pull/58370)
* \[[`6816d779b6`](https://github.com/nodejs/node/commit/6816d779b6)] - **child\_process**: give names to promisified `exec()` and `execFile()` (LiviaMedeiros) [#57916](https://github.com/nodejs/node/pull/57916)
* \[[`5572cecca4`](https://github.com/nodejs/node/commit/5572cecca4)] - **crypto**: expose crypto.constants.OPENSSL\_IS\_BORINGSSL (Shelley Vohr) [#58387](https://github.com/nodejs/node/pull/58387)
* \[[`d6aa02889c`](https://github.com/nodejs/node/commit/d6aa02889c)] - **deps**: use proper C standard when building libuv (Yaksh Bariya) [#58587](https://github.com/nodejs/node/pull/58587)
* \[[`375a6413d5`](https://github.com/nodejs/node/commit/375a6413d5)] - **deps**: update simdjson to 3.12.3 (Node.js GitHub Bot) [#57682](https://github.com/nodejs/node/pull/57682)
* \[[`e0cd138e52`](https://github.com/nodejs/node/commit/e0cd138e52)] - **deps**: update googletest to e9092b1 (Node.js GitHub Bot) [#58565](https://github.com/nodejs/node/pull/58565)
* \[[`31e592631f`](https://github.com/nodejs/node/commit/31e592631f)] - **deps**: update corepack to 0.33.0 (Node.js GitHub Bot) [#58566](https://github.com/nodejs/node/pull/58566)
* \[[`386c24260b`](https://github.com/nodejs/node/commit/386c24260b)] - **deps**: update sqlite to 3.50.0 (Node.js GitHub Bot) [#58272](https://github.com/nodejs/node/pull/58272)
* \[[`f84998d40b`](https://github.com/nodejs/node/commit/f84998d40b)] - **deps**: update OpenSSL gen container to Ubuntu 22.04 (Michaël Zasso) [#58432](https://github.com/nodejs/node/pull/58432)
* \[[`d49fd29859`](https://github.com/nodejs/node/commit/d49fd29859)] - **deps**: update llhttp to 9.3.0 (Fedor Indutny) [#58144](https://github.com/nodejs/node/pull/58144)
* \[[`e397980a1a`](https://github.com/nodejs/node/commit/e397980a1a)] - **deps**: update libuv to 1.51.0 (Node.js GitHub Bot) [#58124](https://github.com/nodejs/node/pull/58124)
* \[[`a28c33645c`](https://github.com/nodejs/node/commit/a28c33645c)] - **dns**: fix dns query cache implementation (Ethan Arrowood) [#58404](https://github.com/nodejs/node/pull/58404)
* \[[`6939b0c624`](https://github.com/nodejs/node/commit/6939b0c624)] - **doc**: fix the order of `process.md` sections (Allon Murienik) [#58403](https://github.com/nodejs/node/pull/58403)
* \[[`1ca253c363`](https://github.com/nodejs/node/commit/1ca253c363)] - **doc**: add support link for panva (Filip Skokan) [#58591](https://github.com/nodejs/node/pull/58591)
* \[[`8319edbcf6`](https://github.com/nodejs/node/commit/8319edbcf6)] - **doc**: update metadata for \_transformState deprecation (James M Snell) [#58530](https://github.com/nodejs/node/pull/58530)
* \[[`697d258136`](https://github.com/nodejs/node/commit/697d258136)] - **doc**: deprecate passing an empty string to `options.shell` (Antoine du Hamel) [#58564](https://github.com/nodejs/node/pull/58564)
* \[[`132fc804e8`](https://github.com/nodejs/node/commit/132fc804e8)] - **doc**: correct formatting of example definitions for `--test-shard` (Jacob Smith) [#58571](https://github.com/nodejs/node/pull/58571)
* \[[`7d0df646f6`](https://github.com/nodejs/node/commit/7d0df646f6)] - **doc**: clarify DEP0194 scope (Antoine du Hamel) [#58504](https://github.com/nodejs/node/pull/58504)
* \[[`1e6d7da0ce`](https://github.com/nodejs/node/commit/1e6d7da0ce)] - **doc**: deprecate HTTP/2 priority signaling (Matteo Collina) [#58313](https://github.com/nodejs/node/pull/58313)
* \[[`5a917bc1d0`](https://github.com/nodejs/node/commit/5a917bc1d0)] - **doc**: explain child\_process code and signal null values everywhere (Darshan Sen) [#58479](https://github.com/nodejs/node/pull/58479)
* \[[`0105c13556`](https://github.com/nodejs/node/commit/0105c13556)] - **doc**: add Filip Skokan to TSC (Rafael Gonzaga) [#58499](https://github.com/nodejs/node/pull/58499)
* \[[`2bdc87cd64`](https://github.com/nodejs/node/commit/2bdc87cd64)] - **doc**: update `git node release` example (Antoine du Hamel) [#58475](https://github.com/nodejs/node/pull/58475)
* \[[`28f9b43186`](https://github.com/nodejs/node/commit/28f9b43186)] - **doc**: add missing options.info for ZstdOptions (Jimmy Leung) [#58360](https://github.com/nodejs/node/pull/58360)
* \[[`e19496dfc1`](https://github.com/nodejs/node/commit/e19496dfc1)] - **doc**: add missing options.info for BrotliOptions (Jimmy Leung) [#58359](https://github.com/nodejs/node/pull/58359)
* \[[`7f905863db`](https://github.com/nodejs/node/commit/7f905863db)] - **doc**: clarify x509.checkIssued only checks metadata (Filip Skokan) [#58457](https://github.com/nodejs/node/pull/58457)
* \[[`5cc97df637`](https://github.com/nodejs/node/commit/5cc97df637)] - **doc**: add links to parent class for `node:zlib` classes (Antoine du Hamel) [#58433](https://github.com/nodejs/node/pull/58433)
* \[[`36e0d5539b`](https://github.com/nodejs/node/commit/36e0d5539b)] - **doc**: remove remaining uses of `@@wellknown` syntax (René) [#58413](https://github.com/nodejs/node/pull/58413)
* \[[`2f36f8e863`](https://github.com/nodejs/node/commit/2f36f8e863)] - **doc**: clarify behavior of --watch-path and --watch flags (Juan Ignacio Benito) [#58136](https://github.com/nodejs/node/pull/58136)
* \[[`3b857735ef`](https://github.com/nodejs/node/commit/3b857735ef)] - **doc**: add JonasBa to collaborators (Jonas Badalic) [#58355](https://github.com/nodejs/node/pull/58355)
* \[[`9d5e969bb6`](https://github.com/nodejs/node/commit/9d5e969bb6)] - **doc**: add latest security release steward (Rafael Gonzaga) [#58339](https://github.com/nodejs/node/pull/58339)
* \[[`b22bb03167`](https://github.com/nodejs/node/commit/b22bb03167)] - **doc**: fix CryptoKey.algorithm type and other interfaces in webcrypto.md (Filip Skokan) [#58294](https://github.com/nodejs/node/pull/58294)
* \[[`670f31060b`](https://github.com/nodejs/node/commit/670f31060b)] - **doc**: mark the callback argument of crypto.generatePrime as mandatory (Allon Murienik) [#58299](https://github.com/nodejs/node/pull/58299)
* \[[`39d9a61239`](https://github.com/nodejs/node/commit/39d9a61239)] - **doc**: remove comma delimiter mention on permissions doc (Rafael Gonzaga) [#58297](https://github.com/nodejs/node/pull/58297)
* \[[`573b0b7bfe`](https://github.com/nodejs/node/commit/573b0b7bfe)] - **doc**: make Stability labels not sticky in Stability index (Livia Medeiros) [#58291](https://github.com/nodejs/node/pull/58291)
* \[[`a5a686a3ae`](https://github.com/nodejs/node/commit/a5a686a3ae)] - **doc**: update commit-queue documentation (Dario Piotrowicz) [#58275](https://github.com/nodejs/node/pull/58275)
* \[[`fdf7612735`](https://github.com/nodejs/node/commit/fdf7612735)] - **doc**: add puskin to collaborators (Giovanni Bucci) [#58308](https://github.com/nodejs/node/pull/58308)
* \[[`be492a1708`](https://github.com/nodejs/node/commit/be492a1708)] - **doc**: update stability status for diagnostics\_channel to experimental (Idan Goshen) [#58261](https://github.com/nodejs/node/pull/58261)
* \[[`7d00fc2206`](https://github.com/nodejs/node/commit/7d00fc2206)] - **doc**: clarify napi\_get\_value\_string\_\* for bufsize 0 (Tobias Nießen) [#58158](https://github.com/nodejs/node/pull/58158)
* \[[`c8500a2c4a`](https://github.com/nodejs/node/commit/c8500a2c4a)] - **doc**: fix typo of file `http.md`, `outgoingMessage.setTimeout` section (yusheng chen) [#58188](https://github.com/nodejs/node/pull/58188)
* \[[`34a9b856c3`](https://github.com/nodejs/node/commit/34a9b856c3)] - **doc**: update return types for eventNames method in EventEmitter (Yukihiro Hasegawa) [#58083](https://github.com/nodejs/node/pull/58083)
* \[[`faedee59d2`](https://github.com/nodejs/node/commit/faedee59d2)] - **doc**: fix typo in benchmark script path (Miguel Marcondes Filho) [#58129](https://github.com/nodejs/node/pull/58129)
* \[[`570d8d3f10`](https://github.com/nodejs/node/commit/570d8d3f10)] - **doc**: clarify future Corepack removal in v25+ (Trivikram Kamat) [#57825](https://github.com/nodejs/node/pull/57825)
* \[[`a71b9fc2ff`](https://github.com/nodejs/node/commit/a71b9fc2ff)] - **doc**: mark multiple APIs stable (James M Snell) [#57513](https://github.com/nodejs/node/pull/57513)
* \[[`73a97d47f3`](https://github.com/nodejs/node/commit/73a97d47f3)] - **doc,lib**: update source map links to ECMA426 (Chengzhong Wu) [#58597](https://github.com/nodejs/node/pull/58597)
* \[[`8b41429499`](https://github.com/nodejs/node/commit/8b41429499)] - **doc,src,test**: fix typos (Noritaka Kobayashi) [#58477](https://github.com/nodejs/node/pull/58477)
* \[[`0cea14ec7f`](https://github.com/nodejs/node/commit/0cea14ec7f)] - **errors**: show url of unsupported attributes in the error message (Aditi) [#58303](https://github.com/nodejs/node/pull/58303)
* \[[`b9586bf898`](https://github.com/nodejs/node/commit/b9586bf898)] - **(SEMVER-MINOR)** **fs**: add autoClose option to FileHandle readableWebStream (James M Snell) [#58548](https://github.com/nodejs/node/pull/58548)
* \[[`72a1b061f3`](https://github.com/nodejs/node/commit/72a1b061f3)] - **fs**: unexpose internal constants (Chengzhong Wu) [#58327](https://github.com/nodejs/node/pull/58327)
* \[[`5c36510dec`](https://github.com/nodejs/node/commit/5c36510dec)] - **fs**: add support for `URL` for `fs.glob`'s `cwd` option (Antoine du Hamel) [#58182](https://github.com/nodejs/node/pull/58182)
* \[[`3642b0d944`](https://github.com/nodejs/node/commit/3642b0d944)] - **fs**: improve cpSync no-filter copyDir performance (Dario Piotrowicz) [#58461](https://github.com/nodejs/node/pull/58461)
* \[[`24865bc7e8`](https://github.com/nodejs/node/commit/24865bc7e8)] - **fs**: improve `cpSync` dest overriding performance (Dario Piotrowicz) [#58160](https://github.com/nodejs/node/pull/58160)
* \[[`1b3847694d`](https://github.com/nodejs/node/commit/1b3847694d)] - **(SEMVER-MINOR)** **fs**: add to `Dir` support for explicit resource management (Antoine du Hamel) [#58206](https://github.com/nodejs/node/pull/58206)
* \[[`cff62e3265`](https://github.com/nodejs/node/commit/cff62e3265)] - **fs**: ensure `dir.read()` does not throw synchronously (Antoine du Hamel) [#58228](https://github.com/nodejs/node/pull/58228)
* \[[`cb39e4ca1f`](https://github.com/nodejs/node/commit/cb39e4ca1f)] - **fs**: glob is stable, so should not emit experimental warnings (Théo LUDWIG) [#58236](https://github.com/nodejs/node/pull/58236)
* \[[`597bfefbe1`](https://github.com/nodejs/node/commit/597bfefbe1)] - **http**: deprecate instantiating classes without new (Yagiz Nizipli) [#58518](https://github.com/nodejs/node/pull/58518)
* \[[`5298da0102`](https://github.com/nodejs/node/commit/5298da0102)] - **http**: remove unused functions and add todos (Yagiz Nizipli) [#58143](https://github.com/nodejs/node/pull/58143)
* \[[`cff440e0fa`](https://github.com/nodejs/node/commit/cff440e0fa)] - **http,https**: give names to anonymous or misnamed functions (Livia Medeiros) [#58180](https://github.com/nodejs/node/pull/58180)
* \[[`43bf1f619a`](https://github.com/nodejs/node/commit/43bf1f619a)] - **http2**: add raw header array support to h2Session.request() (Tim Perry) [#57917](https://github.com/nodejs/node/pull/57917)
* \[[`e8a0f5b063`](https://github.com/nodejs/node/commit/e8a0f5b063)] - **http2**: add lenient flag for RFC-9113 (Carlos Fuentes) [#58116](https://github.com/nodejs/node/pull/58116)
* \[[`49cb90d4a5`](https://github.com/nodejs/node/commit/49cb90d4a5)] - **(SEMVER-MINOR)** **http2**: add diagnostics channel 'http2.server.stream.finish' (Darshan Sen) [#58560](https://github.com/nodejs/node/pull/58560)
* \[[`6a56c68728`](https://github.com/nodejs/node/commit/6a56c68728)] - **http2**: add diagnostics channel 'http2.server.stream.error' (Darshan Sen) [#58512](https://github.com/nodejs/node/pull/58512)
* \[[`59806b41d3`](https://github.com/nodejs/node/commit/59806b41d3)] - **http2**: add diagnostics channel 'http2.server.stream.start' (Darshan Sen) [#58449](https://github.com/nodejs/node/pull/58449)
* \[[`d3d662ae47`](https://github.com/nodejs/node/commit/d3d662ae47)] - **http2**: remove no longer userful options.selectPadding (Jimmy Leung) [#58373](https://github.com/nodejs/node/pull/58373)
* \[[`dec6c9af8c`](https://github.com/nodejs/node/commit/dec6c9af8c)] - **http2**: add diagnostics channel 'http2.server.stream.created' (Darshan Sen) [#58390](https://github.com/nodejs/node/pull/58390)
* \[[`9e98899986`](https://github.com/nodejs/node/commit/9e98899986)] - **http2**: add diagnostics channel 'http2.client.stream.close' (Darshan Sen) [#58329](https://github.com/nodejs/node/pull/58329)
* \[[`86610389d8`](https://github.com/nodejs/node/commit/86610389d8)] - **http2**: add diagnostics channel 'http2.client.stream.finish' (Darshan Sen) [#58317](https://github.com/nodejs/node/pull/58317)
* \[[`2d3071671e`](https://github.com/nodejs/node/commit/2d3071671e)] - **http2**: add diagnostics channel 'http2.client.stream.error' (Darshan Sen) [#58306](https://github.com/nodejs/node/pull/58306)
* \[[`6c3e426d6f`](https://github.com/nodejs/node/commit/6c3e426d6f)] - **http2**: add diagnostics channel 'http2.client.stream.start' (Darshan Sen) [#58292](https://github.com/nodejs/node/pull/58292)
* \[[`b99d131a61`](https://github.com/nodejs/node/commit/b99d131a61)] - **http2**: add diagnostics channel 'http2.client.stream.created' (Darshan Sen) [#58246](https://github.com/nodejs/node/pull/58246)
* \[[`b0644330f5`](https://github.com/nodejs/node/commit/b0644330f5)] - **http2**: give name to promisified `connect()` (LiviaMedeiros) [#57916](https://github.com/nodejs/node/pull/57916)
* \[[`4092d3611a`](https://github.com/nodejs/node/commit/4092d3611a)] - **inspector**: add mimeType and charset support to Network.Response (Shima Ryuhei) [#58192](https://github.com/nodejs/node/pull/58192)
* \[[`d7d8599f7c`](https://github.com/nodejs/node/commit/d7d8599f7c)] - **inspector**: add protocol method Network.dataReceived (Chengzhong Wu) [#58001](https://github.com/nodejs/node/pull/58001)
* \[[`aabafbc28f`](https://github.com/nodejs/node/commit/aabafbc28f)] - **inspector**: support for worker inspection in chrome devtools (Shima Ryuhei) [#56759](https://github.com/nodejs/node/pull/56759)
* \[[`20d978de9a`](https://github.com/nodejs/node/commit/20d978de9a)] - **lib**: make ERM functions into wrappers returning undefined (Livia Medeiros) [#58400](https://github.com/nodejs/node/pull/58400)
* \[[`13567eac5f`](https://github.com/nodejs/node/commit/13567eac5f)] - **(SEMVER-MINOR)** **lib**: graduate error codes that have been around for years (James M Snell) [#58541](https://github.com/nodejs/node/pull/58541)
* \[[`342b5a0d7a`](https://github.com/nodejs/node/commit/342b5a0d7a)] - **lib**: remove no-mixed-operators eslint rule (Ruben Bridgewater) [#58375](https://github.com/nodejs/node/pull/58375)
* \[[`af7baef75a`](https://github.com/nodejs/node/commit/af7baef75a)] - **lib**: fix sourcemaps with ts module mocking (Marco Ippolito) [#58193](https://github.com/nodejs/node/pull/58193)
* \[[`8f73f42d4e`](https://github.com/nodejs/node/commit/8f73f42d4e)] - **meta**: bump github/codeql-action from 3.28.16 to 3.28.18 (dependabot\[bot]) [#58552](https://github.com/nodejs/node/pull/58552)
* \[[`5dfedbeb86`](https://github.com/nodejs/node/commit/5dfedbeb86)] - **meta**: bump codecov/codecov-action from 5.4.2 to 5.4.3 (dependabot\[bot]) [#58551](https://github.com/nodejs/node/pull/58551)
* \[[`53c50f66f5`](https://github.com/nodejs/node/commit/53c50f66f5)] - **meta**: bump step-security/harden-runner from 2.11.0 to 2.12.0 (dependabot\[bot]) [#58109](https://github.com/nodejs/node/pull/58109)
* \[[`a1a7024831`](https://github.com/nodejs/node/commit/a1a7024831)] - **meta**: bump ossf/scorecard-action from 2.4.1 to 2.4.2 (dependabot\[bot]) [#58550](https://github.com/nodejs/node/pull/58550)
* \[[`a379988ef6`](https://github.com/nodejs/node/commit/a379988ef6)] - **meta**: bump rtCamp/action-slack-notify from 2.3.2 to 2.3.3 (dependabot\[bot]) [#58108](https://github.com/nodejs/node/pull/58108)
* \[[`f6a46c87f2`](https://github.com/nodejs/node/commit/f6a46c87f2)] - **meta**: move one or more collaborators to emeritus (Node.js GitHub Bot) [#58456](https://github.com/nodejs/node/pull/58456)
* \[[`98b6aa0dcd`](https://github.com/nodejs/node/commit/98b6aa0dcd)] - **meta**: bump github/codeql-action from 3.28.11 to 3.28.16 (dependabot\[bot]) [#58112](https://github.com/nodejs/node/pull/58112)
* \[[`5202b262e3`](https://github.com/nodejs/node/commit/5202b262e3)] - **meta**: bump codecov/codecov-action from 5.4.0 to 5.4.2 (dependabot\[bot]) [#58110](https://github.com/nodejs/node/pull/58110)
* \[[`d97616ac6e`](https://github.com/nodejs/node/commit/d97616ac6e)] - **meta**: bump actions/download-artifact from 4.2.1 to 4.3.0 (dependabot\[bot]) [#58106](https://github.com/nodejs/node/pull/58106)
* \[[`f4065074cf`](https://github.com/nodejs/node/commit/f4065074cf)] - **meta**: ignore mailmap changes in linux ci (Jonas Badalic) [#58356](https://github.com/nodejs/node/pull/58356)
* \[[`e6d1224e54`](https://github.com/nodejs/node/commit/e6d1224e54)] - **meta**: bump actions/setup-node from 4.3.0 to 4.4.0 (dependabot\[bot]) [#58111](https://github.com/nodejs/node/pull/58111)
* \[[`26da160ab2`](https://github.com/nodejs/node/commit/26da160ab2)] - **meta**: bump actions/setup-python from 5.5.0 to 5.6.0 (dependabot\[bot]) [#58107](https://github.com/nodejs/node/pull/58107)
* \[[`4cc4195493`](https://github.com/nodejs/node/commit/4cc4195493)] - **module**: handle instantiated async module jobs in require(esm) (Joyee Cheung) [#58067](https://github.com/nodejs/node/pull/58067)
* \[[`72fac71b92`](https://github.com/nodejs/node/commit/72fac71b92)] - **module**: clarify cjs global-like error on ModuleJobSync (Carlos Espa) [#56491](https://github.com/nodejs/node/pull/56491)
* \[[`fecd841c93`](https://github.com/nodejs/node/commit/fecd841c93)] - **net**: always publish to 'net.client.socket' diagnostics channel (Darshan Sen) [#58349](https://github.com/nodejs/node/pull/58349)
* \[[`25ee328d2b`](https://github.com/nodejs/node/commit/25ee328d2b)] - **path**: improve path.resolve() performance when used as process.cwd() (Ruben Bridgewater) [#58362](https://github.com/nodejs/node/pull/58362)
* \[[`6fd1b23260`](https://github.com/nodejs/node/commit/6fd1b23260)] - **permission**: remove useless conditional (Juan José) [#58514](https://github.com/nodejs/node/pull/58514)
* \[[`5b2cca51a3`](https://github.com/nodejs/node/commit/5b2cca51a3)] - **report**: use uv\_getrusage\_thread in report (theanarkh) [#58405](https://github.com/nodejs/node/pull/58405)
* \[[`63ec23e84b`](https://github.com/nodejs/node/commit/63ec23e84b)] - **sqlite**: add build option to build without sqlite (Michael Dawson) [#58122](https://github.com/nodejs/node/pull/58122)
* \[[`9d8677bff5`](https://github.com/nodejs/node/commit/9d8677bff5)] - **sqlite**: handle thrown errors in result callback (Colin Ihrig) [#58426](https://github.com/nodejs/node/pull/58426)
* \[[`3490c75760`](https://github.com/nodejs/node/commit/3490c75760)] - **sqlite**: set `name` and `length` on `sqlite.backup()` (Livia Medeiros) [#58251](https://github.com/nodejs/node/pull/58251)
* \[[`50bdd94e0b`](https://github.com/nodejs/node/commit/50bdd94e0b)] - **src**: update std::vector\<v8::Local\<T>> to use v8::LocalVector\<T> (Aditi) [#58500](https://github.com/nodejs/node/pull/58500)
* \[[`7de58417cc`](https://github.com/nodejs/node/commit/7de58417cc)] - **src**: env\_vars caching and local variable scope optimization (Mert Can Altin) [#57624](https://github.com/nodejs/node/pull/57624)
* \[[`6d99ec33a4`](https://github.com/nodejs/node/commit/6d99ec33a4)] - **src**: fix FIPS init error handling (Tobias Nießen) [#58379](https://github.com/nodejs/node/pull/58379)
* \[[`4c23a9575e`](https://github.com/nodejs/node/commit/4c23a9575e)] - **src**: fix possible dereference of null pointer (Eusgor) [#58459](https://github.com/nodejs/node/pull/58459)
* \[[`eb143e902b`](https://github.com/nodejs/node/commit/eb143e902b)] - **src**: fix -Wreturn-stack-address error (Shelley Vohr) [#58439](https://github.com/nodejs/node/pull/58439)
* \[[`31058b8785`](https://github.com/nodejs/node/commit/31058b8785)] - **src**: reorganize ContextifyFunction methods (Chengzhong Wu) [#58434](https://github.com/nodejs/node/pull/58434)
* \[[`7521077299`](https://github.com/nodejs/node/commit/7521077299)] - **src**: improve CompileFunctionAndCacheResult error handling (Chengzhong Wu) [#58434](https://github.com/nodejs/node/pull/58434)
* \[[`0c9efccb12`](https://github.com/nodejs/node/commit/0c9efccb12)] - **src**: fix build when using shared simdutf (Antoine du Hamel) [#58407](https://github.com/nodejs/node/pull/58407)
* \[[`aa00f5946f`](https://github.com/nodejs/node/commit/aa00f5946f)] - **src**: add a variant of ToV8Value() for primitive arrays (Aditi) [#57576](https://github.com/nodejs/node/pull/57576)
* \[[`29a11506fc`](https://github.com/nodejs/node/commit/29a11506fc)] - **src**: remove unused `checkMessagePort` internal binding (Dario Piotrowicz) [#58267](https://github.com/nodejs/node/pull/58267)
* \[[`0ce3feed5b`](https://github.com/nodejs/node/commit/0ce3feed5b)] - **src**: remove unused `shouldRetryAsESM` internal binding (Dario Piotrowicz) [#58265](https://github.com/nodejs/node/pull/58265)
* \[[`517219613d`](https://github.com/nodejs/node/commit/517219613d)] - **src**: add a couple fast apis in node\_os (James M Snell) [#58210](https://github.com/nodejs/node/pull/58210)
* \[[`3f834da09e`](https://github.com/nodejs/node/commit/3f834da09e)] - **src**: fix module buffer allocation (X-BW) [#57738](https://github.com/nodejs/node/pull/57738)
* \[[`a793706db0`](https://github.com/nodejs/node/commit/a793706db0)] - **src**: remove overzealous tcsetattr error check (Ben Noordhuis) [#58200](https://github.com/nodejs/node/pull/58200)
* \[[`5656c74517`](https://github.com/nodejs/node/commit/5656c74517)] - **src**: remove NonCopyableMaybe (Tobias Nießen) [#58168](https://github.com/nodejs/node/pull/58168)
* \[[`cab242334b`](https://github.com/nodejs/node/commit/cab242334b)] - **src**: improve parsing of boolean options (Edy Silva) [#58039](https://github.com/nodejs/node/pull/58039)
* \[[`a5df778150`](https://github.com/nodejs/node/commit/a5df778150)] - **src,lib**: obtain sourceURL in magic comments from V8 (Chengzhong Wu) [#58389](https://github.com/nodejs/node/pull/58389)
* \[[`bd6743b434`](https://github.com/nodejs/node/commit/bd6743b434)] - **src,permission**: implicit allow-fs-read to app entrypoint (Rafael Gonzaga) [#58579](https://github.com/nodejs/node/pull/58579)
* \[[`5bd99e4a4d`](https://github.com/nodejs/node/commit/5bd99e4a4d)] - **stream**: making DecompressionStream spec compilent for trailing junk (0hm☘️) [#58316](https://github.com/nodejs/node/pull/58316)
* \[[`6582b19488`](https://github.com/nodejs/node/commit/6582b19488)] - **test**: reduce flakiness in test-heapdump-http2 (Joyee Cheung) [#58148](https://github.com/nodejs/node/pull/58148)
* \[[`0f6a262744`](https://github.com/nodejs/node/commit/0f6a262744)] - **test**: improve flakiness detection on stack corruption tests (Darshan Sen) [#58601](https://github.com/nodejs/node/pull/58601)
* \[[`983affaea2`](https://github.com/nodejs/node/commit/983affaea2)] - **test**: mark timeouts & flaky test as flaky on IBM i (Abdirahim Musse) [#58583](https://github.com/nodejs/node/pull/58583)
* \[[`3603362b6f`](https://github.com/nodejs/node/commit/3603362b6f)] - **test**: rewrite test-child-process-spawn-args (Michaël Zasso) [#58546](https://github.com/nodejs/node/pull/58546)
* \[[`93900b0c17`](https://github.com/nodejs/node/commit/93900b0c17)] - **test**: make sqlite-database-sync tests work with system sqlite (Jelle Licht) [#58507](https://github.com/nodejs/node/pull/58507)
* \[[`7d505f4185`](https://github.com/nodejs/node/commit/7d505f4185)] - **test**: force slow JSON.stringify path for overflow (Shelley Vohr) [#58181](https://github.com/nodejs/node/pull/58181)
* \[[`2e8570b8f9`](https://github.com/nodejs/node/commit/2e8570b8f9)] - **test**: account for truthy signal in flaky async\_hooks tests (Darshan Sen) [#58478](https://github.com/nodejs/node/pull/58478)
* \[[`1f1e194c0f`](https://github.com/nodejs/node/commit/1f1e194c0f)] - **test**: update WPT for WebCryptoAPI to 591c95ce61 (Node.js GitHub Bot) [#58176](https://github.com/nodejs/node/pull/58176)
* \[[`d822632d91`](https://github.com/nodejs/node/commit/d822632d91)] - **test**: remove --no-warnings flag (Tobias Nießen) [#58424](https://github.com/nodejs/node/pull/58424)
* \[[`01377713d7`](https://github.com/nodejs/node/commit/01377713d7)] - **test**: add tests ensuring worker threads cannot access internals (Joe) [#58332](https://github.com/nodejs/node/pull/58332)
* \[[`99a20902fc`](https://github.com/nodejs/node/commit/99a20902fc)] - **test**: leverage process.features.openssl\_is\_boringssl in test (Shelley Vohr) [#58421](https://github.com/nodejs/node/pull/58421)
* \[[`b3e0cf1b15`](https://github.com/nodejs/node/commit/b3e0cf1b15)] - **test**: fix test-buffer-tostring-range on allocation failure (Joyee Cheung) [#58416](https://github.com/nodejs/node/pull/58416)
* \[[`1d4b3451c5`](https://github.com/nodejs/node/commit/1d4b3451c5)] - **test**: skip in test-buffer-tostring-rangeerror on allocation failure (Joyee Cheung) [#58415](https://github.com/nodejs/node/pull/58415)
* \[[`612c393c71`](https://github.com/nodejs/node/commit/612c393c71)] - **test**: fix missing edge case in test-blob-slice-with-large-size (Joyee Cheung) [#58414](https://github.com/nodejs/node/pull/58414)
* \[[`b11b9cdad8`](https://github.com/nodejs/node/commit/b11b9cdad8)] - **test**: make crypto tests work with BoringSSL (Shelley Vohr) [#58117](https://github.com/nodejs/node/pull/58117)
* \[[`99711ee548`](https://github.com/nodejs/node/commit/99711ee548)] - **test**: test reordering of setAAD and setAuthTag (Tobias Nießen) [#58396](https://github.com/nodejs/node/pull/58396)
* \[[`828aaaa3f7`](https://github.com/nodejs/node/commit/828aaaa3f7)] - **test**: switch from deprecated `optparse` to `argparse` (Aviv Keller) [#58224](https://github.com/nodejs/node/pull/58224)
* \[[`9af305408e`](https://github.com/nodejs/node/commit/9af305408e)] - **test**: do not skip OCB decryption in FIPS mode (Tobias Nießen) [#58382](https://github.com/nodejs/node/pull/58382)
* \[[`9527c876bf`](https://github.com/nodejs/node/commit/9527c876bf)] - **test**: show more information in test-http2-debug upon failure (Joyee Cheung) [#58391](https://github.com/nodejs/node/pull/58391)
* \[[`9be0601112`](https://github.com/nodejs/node/commit/9be0601112)] - **test**: remove loop over single element (Tobias Nießen) [#58368](https://github.com/nodejs/node/pull/58368)
* \[[`40a03d3d14`](https://github.com/nodejs/node/commit/40a03d3d14)] - **test**: add chacha20-poly1305 to auth tag order test (Tobias Nießen) [#58367](https://github.com/nodejs/node/pull/58367)
* \[[`cccb15df7e`](https://github.com/nodejs/node/commit/cccb15df7e)] - **test**: skip wasm-allocation tests for pointer compression builds (Joyee Cheung) [#58171](https://github.com/nodejs/node/pull/58171)
* \[[`f18041ae8e`](https://github.com/nodejs/node/commit/f18041ae8e)] - **test**: remove references to create(De|C)ipher (Tobias Nießen) [#58363](https://github.com/nodejs/node/pull/58363)
* \[[`ca8d66c1fc`](https://github.com/nodejs/node/commit/ca8d66c1fc)] - **test**: remove unnecessary `console.log` from test-repl-null-thrown (Dario Piotrowicz) [#58281](https://github.com/nodejs/node/pull/58281)
* \[[`455372023d`](https://github.com/nodejs/node/commit/455372023d)] - **test**: allow `tmpDir.path` to be modified (Aviv Keller) [#58173](https://github.com/nodejs/node/pull/58173)
* \[[`1f1fab60c7`](https://github.com/nodejs/node/commit/1f1fab60c7)] - **test**: fix executable flags (Livia Medeiros) [#58250](https://github.com/nodejs/node/pull/58250)
* \[[`8bafc0f061`](https://github.com/nodejs/node/commit/8bafc0f061)] - **test**: deflake test-http2-client-socket-destroy (Luigi Pinca) [#58212](https://github.com/nodejs/node/pull/58212)
* \[[`97aac9f17a`](https://github.com/nodejs/node/commit/97aac9f17a)] - **test**: skip test-buffer-tostring-rangeerror when low on memory (Ruben Bridgewater) [#58142](https://github.com/nodejs/node/pull/58142)
* \[[`2896760da1`](https://github.com/nodejs/node/commit/2896760da1)] - **test**: mark `test-http2-debug` as flaky on LinuxONE (Richard Lau) [#58494](https://github.com/nodejs/node/pull/58494)
* \[[`7327d14780`](https://github.com/nodejs/node/commit/7327d14780)] - **test**: reduce iteration count in test-child-process-stdout-flush-exit (Antoine du Hamel) [#58273](https://github.com/nodejs/node/pull/58273)
* \[[`1bd7a2edf9`](https://github.com/nodejs/node/commit/1bd7a2edf9)] - **test\_runner**: support mocking json modules (Jacob Smith) [#58007](https://github.com/nodejs/node/pull/58007)
* \[[`a3877c53b1`](https://github.com/nodejs/node/commit/a3877c53b1)] - **test\_runner**: add level parameter to reporter.diagnostic (Jacopo Martinelli) [#57923](https://github.com/nodejs/node/pull/57923)
* \[[`253772c2d9`](https://github.com/nodejs/node/commit/253772c2d9)] - **tools**: bump the eslint group in `/tools/eslint` with 6 updates (dependabot\[bot]) [#58549](https://github.com/nodejs/node/pull/58549)
* \[[`b7feda97b0`](https://github.com/nodejs/node/commit/b7feda97b0)] - **tools**: disable failing coverage jobs (Antoine du Hamel) [#58770](https://github.com/nodejs/node/pull/58770)
* \[[`8a47096093`](https://github.com/nodejs/node/commit/8a47096093)] - **tools**: ignore `deps/` and `benchmark/` for CodeQL (Rafael Gonzaga) [#58254](https://github.com/nodejs/node/pull/58254)
* \[[`70be158126`](https://github.com/nodejs/node/commit/70be158126)] - **tools**: add read permission to workflows that read contents (Antoine du Hamel) [#58255](https://github.com/nodejs/node/pull/58255)
* \[[`e4373be766`](https://github.com/nodejs/node/commit/e4373be766)] - **tools**: exclude deps/v8/tools from CodeQL scans (Rich Trott) [#58132](https://github.com/nodejs/node/pull/58132)
* \[[`23ceb364d4`](https://github.com/nodejs/node/commit/23ceb364d4)] - **tools**: bump the eslint group in /tools/eslint with 6 updates (dependabot\[bot]) [#58105](https://github.com/nodejs/node/pull/58105)
* \[[`5b6ced3255`](https://github.com/nodejs/node/commit/5b6ced3255)] - **tty**: improve color terminal color detection (Ruben Bridgewater) [#58146](https://github.com/nodejs/node/pull/58146)
* \[[`7be70979c6`](https://github.com/nodejs/node/commit/7be70979c6)] - **tty**: use terminal VT mode on Windows (Anna Henningsen) [#58358](https://github.com/nodejs/node/pull/58358)
* \[[`b7d7ffe793`](https://github.com/nodejs/node/commit/b7d7ffe793)] - **typings**: add inspector internalBinding typing (Shima Ryuhei) [#58492](https://github.com/nodejs/node/pull/58492)
* \[[`0056d1a2e2`](https://github.com/nodejs/node/commit/0056d1a2e2)] - **typings**: remove no longer valid `FixedSizeBlobCopyJob` type (Dario Piotrowicz) [#58305](https://github.com/nodejs/node/pull/58305)
* \[[`581c0738f9`](https://github.com/nodejs/node/commit/581c0738f9)] - **typings**: remove no longer valid `revokeDataObject` type (Dario Piotrowicz) [#58305](https://github.com/nodejs/node/pull/58305)
* \[[`1db1c870f0`](https://github.com/nodejs/node/commit/1db1c870f0)] - **typings**: add missing typings for `TypedArray` (Jason Zhang) [#58248](https://github.com/nodejs/node/pull/58248)
* \[[`6d3f43c9a6`](https://github.com/nodejs/node/commit/6d3f43c9a6)] - **url**: improve performance of the format function (Giovanni Bucci) [#57099](https://github.com/nodejs/node/pull/57099)
* \[[`54288bdb42`](https://github.com/nodejs/node/commit/54288bdb42)] - **(SEMVER-MINOR)** **util**: add 'none' style to styleText (James M Snell) [#58437](https://github.com/nodejs/node/pull/58437)
* \[[`6af5358f9c`](https://github.com/nodejs/node/commit/6af5358f9c)] - **util**: add internal `assignFunctionName()` function (LiviaMedeiros) [#57916](https://github.com/nodejs/node/pull/57916)
* \[[`9f2e5aad38`](https://github.com/nodejs/node/commit/9f2e5aad38)] - **vm**: import call should return a promise in the current context (Chengzhong Wu) [#58309](https://github.com/nodejs/node/pull/58309)
* \[[`92304a5e62`](https://github.com/nodejs/node/commit/92304a5e62)] - **watch**: fix watch args not being properly filtered (Dario Piotrowicz) [#58279](https://github.com/nodejs/node/pull/58279)
* \[[`539df8e98d`](https://github.com/nodejs/node/commit/539df8e98d)] - **win,tools**: use Azure Trusted Signing (Stefan Stojanovic) [#58502](https://github.com/nodejs/node/pull/58502)
* \[[`ef66357637`](https://github.com/nodejs/node/commit/ef66357637)] - **worker**: give names to `MessagePort` functions (Livia Medeiros) [#58307](https://github.com/nodejs/node/pull/58307)
* \[[`b3cd847528`](https://github.com/nodejs/node/commit/b3cd847528)] - **zlib**: remove mentions of unexposed Z\_TREES constant (Jimmy Leung) [#58371](https://github.com/nodejs/node/pull/58371)

<a id="22.16.0"></a>

## 2025-05-21, Version 22.16.0 'Jod' (LTS), @aduh95

### Notable Changes

* \[[`c3ceaebb7a`](https://github.com/nodejs/node/commit/c3ceaebb7a)] - **deps**: update timezone to 2025b (Node.js GitHub Bot) [#57857](https://github.com/nodejs/node/pull/57857)
* \[[`5059a746ec`](https://github.com/nodejs/node/commit/5059a746ec)] - **doc**: add dario-piotrowicz to collaborators (Dario Piotrowicz) [#58102](https://github.com/nodejs/node/pull/58102)
* \[[`c8ceaaf397`](https://github.com/nodejs/node/commit/c8ceaaf397)] - **(SEMVER-MINOR)** **doc**: graduate multiple experimental apis (James M Snell) [#57765](https://github.com/nodejs/node/pull/57765)
* \[[`e21b37d9df`](https://github.com/nodejs/node/commit/e21b37d9df)] - **(SEMVER-MINOR)** **esm**: graduate import.meta properties (James M Snell) [#58011](https://github.com/nodejs/node/pull/58011)
* \[[`832640c35e`](https://github.com/nodejs/node/commit/832640c35e)] - **(SEMVER-MINOR)** **esm**: support top-level Wasm without package type (Guy Bedford) [#57610](https://github.com/nodejs/node/pull/57610)
* \[[`c510391d2f`](https://github.com/nodejs/node/commit/c510391d2f)] - **(SEMVER-MINOR)** **sqlite**: add StatementSync.prototype.columns() (Colin Ihrig) [#57490](https://github.com/nodejs/node/pull/57490)
* \[[`5d1230bec0`](https://github.com/nodejs/node/commit/5d1230bec0)] - **(SEMVER-MINOR)** **src**: set default config as `node.config.json` (Marco Ippolito) [#57171](https://github.com/nodejs/node/pull/57171)
* \[[`30bb1ccbb0`](https://github.com/nodejs/node/commit/30bb1ccbb0)] - **(SEMVER-MINOR)** **src**: create `THROW_ERR_OPTIONS_BEFORE_BOOTSTRAPPING` (Marco Ippolito) [#57016](https://github.com/nodejs/node/pull/57016)
* \[[`0350c6f478`](https://github.com/nodejs/node/commit/0350c6f478)] - **(SEMVER-MINOR)** **src**: add config file support (Marco Ippolito) [#57016](https://github.com/nodejs/node/pull/57016)
* \[[`e1d3a9e192`](https://github.com/nodejs/node/commit/e1d3a9e192)] - **(SEMVER-MINOR)** **src**: add ExecutionAsyncId getter for any Context (Attila Szegedi) [#57820](https://github.com/nodejs/node/pull/57820)
* \[[`0ec912f452`](https://github.com/nodejs/node/commit/0ec912f452)] - **(SEMVER-MINOR)** **stream**: preserve AsyncLocalStorage context in finished() (Gürgün Dayıoğlu) [#57865](https://github.com/nodejs/node/pull/57865)
* \[[`43490c8797`](https://github.com/nodejs/node/commit/43490c8797)] - **(SEMVER-MINOR)** **util**: add `types.isFloat16Array()` (Livia Medeiros) [#57879](https://github.com/nodejs/node/pull/57879)
* \[[`dda6ca9172`](https://github.com/nodejs/node/commit/dda6ca9172)] - **(SEMVER-MINOR)** **worker**: add worker.getHeapStatistics() (Matteo Collina) [#57888](https://github.com/nodejs/node/pull/57888)

### Commits

* \[[`4252dc798c`](https://github.com/nodejs/node/commit/4252dc798c)] - **assert**: support `Float16Array` in loose deep equality checks (Livia Medeiros) [#57881](https://github.com/nodejs/node/pull/57881)
* \[[`1c7396b078`](https://github.com/nodejs/node/commit/1c7396b078)] - **assert,util**: fix constructor lookup in deep equal comparison (Ruben Bridgewater) [#57876](https://github.com/nodejs/node/pull/57876)
* \[[`1ded5f25c8`](https://github.com/nodejs/node/commit/1ded5f25c8)] - **assert,util**: improve deep object comparison performance (Ruben Bridgewater) [#57648](https://github.com/nodejs/node/pull/57648)
* \[[`696b5f85ca`](https://github.com/nodejs/node/commit/696b5f85ca)] - **assert,util**: improve unequal number comparison performance (Ruben Bridgewater) [#57619](https://github.com/nodejs/node/pull/57619)
* \[[`775ee4d40f`](https://github.com/nodejs/node/commit/775ee4d40f)] - **assert,util**: improve array comparison (Ruben Bridgewater) [#57619](https://github.com/nodejs/node/pull/57619)
* \[[`3766992ba4`](https://github.com/nodejs/node/commit/3766992ba4)] - **benchmark**: add sqlite prepare select get (Vinícius Lourenço) [#58040](https://github.com/nodejs/node/pull/58040)
* \[[`8390276be3`](https://github.com/nodejs/node/commit/8390276be3)] - **benchmark**: add sqlite prepare select all (Vinícius Lourenço) [#58040](https://github.com/nodejs/node/pull/58040)
* \[[`6a9b79e5c1`](https://github.com/nodejs/node/commit/6a9b79e5c1)] - **benchmark**: add sqlite is transaction (Vinícius Lourenço) [#58040](https://github.com/nodejs/node/pull/58040)
* \[[`f689f98344`](https://github.com/nodejs/node/commit/f689f98344)] - **benchmark**: add sqlite prepare insert (Vinícius Lourenço) [#58040](https://github.com/nodejs/node/pull/58040)
* \[[`14a82804d7`](https://github.com/nodejs/node/commit/14a82804d7)] - **benchmark**: disambiguate `filename` and `dirname` read perf (Antoine du Hamel) [#58056](https://github.com/nodejs/node/pull/58056)
* \[[`e7e8256d35`](https://github.com/nodejs/node/commit/e7e8256d35)] - **buffer**: avoid creating unnecessary environment (Yagiz Nizipli) [#58053](https://github.com/nodejs/node/pull/58053)
* \[[`d7d8e8e994`](https://github.com/nodejs/node/commit/d7d8e8e994)] - **buffer**: define global v8::CFunction objects as const (Mert Can Altin) [#57676](https://github.com/nodejs/node/pull/57676)
* \[[`f37633e85a`](https://github.com/nodejs/node/commit/f37633e85a)] - **build**: use `$(BUILDTYPE)` when cleaning coverage files (Aviv Keller) [#57995](https://github.com/nodejs/node/pull/57995)
* \[[`e5bf67fe77`](https://github.com/nodejs/node/commit/e5bf67fe77)] - **build**: define python when generating `out/Makefile` (Aviv Keller) [#57970](https://github.com/nodejs/node/pull/57970)
* \[[`718f874ae0`](https://github.com/nodejs/node/commit/718f874ae0)] - **build**: fix zstd libname (Antoine du Hamel) [#57999](https://github.com/nodejs/node/pull/57999)
* \[[`53c5fdcae1`](https://github.com/nodejs/node/commit/53c5fdcae1)] - **crypto**: fix cross-realm `SharedArrayBuffer` validation (Antoine du Hamel) [#57974](https://github.com/nodejs/node/pull/57974)
* \[[`78f4ffee5d`](https://github.com/nodejs/node/commit/78f4ffee5d)] - **crypto**: fix cross-realm check of `ArrayBuffer` (Felipe Forbeck) [#57828](https://github.com/nodejs/node/pull/57828)
* \[[`f606352b63`](https://github.com/nodejs/node/commit/f606352b63)] - **crypto**: forbid passing `Float16Array` to `getRandomValues()` (Livia Medeiros) [#57880](https://github.com/nodejs/node/pull/57880)
* \[[`23c4e941c2`](https://github.com/nodejs/node/commit/23c4e941c2)] - **crypto**: remove BoringSSL dh-primes addition (Shelley Vohr) [#57023](https://github.com/nodejs/node/pull/57023)
* \[[`8339d9bc14`](https://github.com/nodejs/node/commit/8339d9bc14)] - **deps**: V8: cherry-pick f915fa4c9f41 (Chengzhong Wu) [#55484](https://github.com/nodejs/node/pull/55484)
* \[[`c2111dd126`](https://github.com/nodejs/node/commit/c2111dd126)] - **deps**: V8: backport e5dbbbadcbff (Darshan Sen) [#58120](https://github.com/nodejs/node/pull/58120)
* \[[`4cc49be951`](https://github.com/nodejs/node/commit/4cc49be951)] - **deps**: update zstd to 1.5.7 (Node.js GitHub Bot) [#57940](https://github.com/nodejs/node/pull/57940)
* \[[`c956d37c84`](https://github.com/nodejs/node/commit/c956d37c84)] - **deps**: update zlib to 1.3.0.1-motley-780819f (Node.js GitHub Bot) [#57768](https://github.com/nodejs/node/pull/57768)
* \[[`c3ceaebb7a`](https://github.com/nodejs/node/commit/c3ceaebb7a)] - **deps**: update timezone to 2025b (Node.js GitHub Bot) [#57857](https://github.com/nodejs/node/pull/57857)
* \[[`b5cd0eb590`](https://github.com/nodejs/node/commit/b5cd0eb590)] - **deps**: update simdutf to 6.4.2 (Node.js GitHub Bot) [#57855](https://github.com/nodejs/node/pull/57855)
* \[[`3eb6b814e9`](https://github.com/nodejs/node/commit/3eb6b814e9)] - **deps**: update simdutf to 6.4.0 (Node.js GitHub Bot) [#56764](https://github.com/nodejs/node/pull/56764)
* \[[`0be9fa3218`](https://github.com/nodejs/node/commit/0be9fa3218)] - **deps**: update icu to 77.1 (Node.js GitHub Bot) [#57455](https://github.com/nodejs/node/pull/57455)
* \[[`d5cf4254fb`](https://github.com/nodejs/node/commit/d5cf4254fb)] - **doc**: add HBSPS as triager (Wiyeong Seo) [#57980](https://github.com/nodejs/node/pull/57980)
* \[[`ad0861dba0`](https://github.com/nodejs/node/commit/ad0861dba0)] - **doc**: add ambassaor message (Brian Muenzenmeyer) [#57600](https://github.com/nodejs/node/pull/57600)
* \[[`0d3ec1aafe`](https://github.com/nodejs/node/commit/0d3ec1aafe)] - **doc**: fix misaligned options in vm.compileFunction() (Jimmy Leung) [#58145](https://github.com/nodejs/node/pull/58145)
* \[[`1f70baf3b0`](https://github.com/nodejs/node/commit/1f70baf3b0)] - **doc**: add missing options.signal to readlinePromises.createInterface() (Jimmy Leung) [#55456](https://github.com/nodejs/node/pull/55456)
* \[[`ec6a48621f`](https://github.com/nodejs/node/commit/ec6a48621f)] - **doc**: fix typo of file `zlib.md` (yusheng chen) [#58093](https://github.com/nodejs/node/pull/58093)
* \[[`37e360e386`](https://github.com/nodejs/node/commit/37e360e386)] - **doc**: make stability labels more consistent (Antoine du Hamel) [#57516](https://github.com/nodejs/node/pull/57516)
* \[[`2b5d63d36e`](https://github.com/nodejs/node/commit/2b5d63d36e)] - **doc**: allow the $schema property in node.config.json (Remco Haszing) [#57560](https://github.com/nodejs/node/pull/57560)
* \[[`a2063638e2`](https://github.com/nodejs/node/commit/a2063638e2)] - **doc**: fix `AsyncLocalStorage` example response changes after node v18 (Naor Tedgi (Abu Emma)) [#57969](https://github.com/nodejs/node/pull/57969)
* \[[`474c2b14c3`](https://github.com/nodejs/node/commit/474c2b14c3)] - **doc**: mark Node.js 18 as End-of-Life (Richard Lau) [#58084](https://github.com/nodejs/node/pull/58084)
* \[[`5059a746ec`](https://github.com/nodejs/node/commit/5059a746ec)] - **doc**: add dario-piotrowicz to collaborators (Dario Piotrowicz) [#58102](https://github.com/nodejs/node/pull/58102)
* \[[`1eec170fc3`](https://github.com/nodejs/node/commit/1eec170fc3)] - **doc**: fix formatting of `import.meta.filename` section (Antoine du Hamel) [#58079](https://github.com/nodejs/node/pull/58079)
* \[[`7f108de525`](https://github.com/nodejs/node/commit/7f108de525)] - **doc**: fix env variable name in `util.styleText` (Antoine du Hamel) [#58072](https://github.com/nodejs/node/pull/58072)
* \[[`54b3f7fffc`](https://github.com/nodejs/node/commit/54b3f7fffc)] - **doc**: add returns for https.get (Eng Zer Jun) [#58025](https://github.com/nodejs/node/pull/58025)
* \[[`66f2c605a8`](https://github.com/nodejs/node/commit/66f2c605a8)] - **doc**: fix typo in `buffer.md` (chocolateboy) [#58052](https://github.com/nodejs/node/pull/58052)
* \[[`b0256dd42b`](https://github.com/nodejs/node/commit/b0256dd42b)] - **doc**: correct deprecation type of `assert.CallTracker` (René) [#57997](https://github.com/nodejs/node/pull/57997)
* \[[`581439c9e6`](https://github.com/nodejs/node/commit/581439c9e6)] - **doc**: mark devtools integration section as active development (Chengzhong Wu) [#57886](https://github.com/nodejs/node/pull/57886)
* \[[`a2a2a2f027`](https://github.com/nodejs/node/commit/a2a2a2f027)] - **doc**: fix typo in `module.md` (Alex Schwartz) [#57889](https://github.com/nodejs/node/pull/57889)
* \[[`c0ec4e2935`](https://github.com/nodejs/node/commit/c0ec4e2935)] - **doc**: increase z-index of header element (Dario Piotrowicz) [#57851](https://github.com/nodejs/node/pull/57851)
* \[[`93d19ec6cd`](https://github.com/nodejs/node/commit/93d19ec6cd)] - **doc**: add missing TS formats for `load` hooks (Antoine du Hamel) [#57837](https://github.com/nodejs/node/pull/57837)
* \[[`f5ea06c61f`](https://github.com/nodejs/node/commit/f5ea06c61f)] - **doc**: clarify the multi REPL example (Dario Piotrowicz) [#57759](https://github.com/nodejs/node/pull/57759)
* \[[`80c4fe1b70`](https://github.com/nodejs/node/commit/80c4fe1b70)] - **doc**: fix deprecation type for `DEP0148` (Livia Medeiros) [#57785](https://github.com/nodejs/node/pull/57785)
* \[[`01cad99da0`](https://github.com/nodejs/node/commit/01cad99da0)] - **doc**: list DOMException as a potential error raised by Node.js (Chengzhong Wu) [#57783](https://github.com/nodejs/node/pull/57783)
* \[[`a08b714a46`](https://github.com/nodejs/node/commit/a08b714a46)] - **doc**: add missing v0.x changelog entries (Antoine du Hamel) [#57779](https://github.com/nodejs/node/pull/57779)
* \[[`d0b48350fd`](https://github.com/nodejs/node/commit/d0b48350fd)] - **doc**: fix typo in writing-docs (Sebastian Beltran) [#57776](https://github.com/nodejs/node/pull/57776)
* \[[`bde3725f8b`](https://github.com/nodejs/node/commit/bde3725f8b)] - **doc**: clarify examples section in REPL doc (Dario Piotrowicz) [#57762](https://github.com/nodejs/node/pull/57762)
* \[[`c8ceaaf397`](https://github.com/nodejs/node/commit/c8ceaaf397)] - **(SEMVER-MINOR)** **doc**: graduate multiple experimental apis (James M Snell) [#57765](https://github.com/nodejs/node/pull/57765)
* \[[`92428c2609`](https://github.com/nodejs/node/commit/92428c2609)] - **doc**: explicitly state that corepack will be removed in v25+ (Trivikram Kamat) [#57747](https://github.com/nodejs/node/pull/57747)
* \[[`298969e1dd`](https://github.com/nodejs/node/commit/298969e1dd)] - **doc**: update position type to integer | null in fs (Yukihiro Hasegawa) [#57745](https://github.com/nodejs/node/pull/57745)
* \[[`a9d28e27c9`](https://github.com/nodejs/node/commit/a9d28e27c9)] - **doc**: update CI instructions (Antoine du Hamel) [#57743](https://github.com/nodejs/node/pull/57743)
* \[[`133d2878a1`](https://github.com/nodejs/node/commit/133d2878a1)] - **doc**: update example of using `await` in REPL (Dario Piotrowicz) [#57653](https://github.com/nodejs/node/pull/57653)
* \[[`fc5f126629`](https://github.com/nodejs/node/commit/fc5f126629)] - **doc**: add back mention of visa fees to onboarding doc (Darshan Sen) [#57730](https://github.com/nodejs/node/pull/57730)
* \[[`945f4ac538`](https://github.com/nodejs/node/commit/945f4ac538)] - **doc**: process.execve is only unavailable for Windows (Yaksh Bariya) [#57726](https://github.com/nodejs/node/pull/57726)
* \[[`f3b885bb5e`](https://github.com/nodejs/node/commit/f3b885bb5e)] - **doc**: clarify `unhandledRejection` events behaviors in process doc (Dario Piotrowicz) [#57654](https://github.com/nodejs/node/pull/57654)
* \[[`7326dda5b0`](https://github.com/nodejs/node/commit/7326dda5b0)] - **doc**: improved fetch docs (Alessandro Miliucci) [#57296](https://github.com/nodejs/node/pull/57296)
* \[[`6906c5eb1f`](https://github.com/nodejs/node/commit/6906c5eb1f)] - **doc**: document REPL custom eval arguments (Dario Piotrowicz) [#57690](https://github.com/nodejs/node/pull/57690)
* \[[`47a7564e8f`](https://github.com/nodejs/node/commit/47a7564e8f)] - **doc**: classify Chrome DevTools Protocol as tier 2 (Chengzhong Wu) [#57634](https://github.com/nodejs/node/pull/57634)
* \[[`e274cc1310`](https://github.com/nodejs/node/commit/e274cc1310)] - **doc**: replace NOTE that does not render properly (Colin Ihrig) [#57484](https://github.com/nodejs/node/pull/57484)
* \[[`bef06b11df`](https://github.com/nodejs/node/commit/bef06b11df)] - **esm**: avoid `import.meta` setup costs for unused properties (Antoine du Hamel) [#57286](https://github.com/nodejs/node/pull/57286)
* \[[`e21b37d9df`](https://github.com/nodejs/node/commit/e21b37d9df)] - **(SEMVER-MINOR)** **esm**: graduate import.meta properties (James M Snell) [#58011](https://github.com/nodejs/node/pull/58011)
* \[[`832640c35e`](https://github.com/nodejs/node/commit/832640c35e)] - **(SEMVER-MINOR)** **esm**: support top-level Wasm without package type (Guy Bedford) [#57610](https://github.com/nodejs/node/pull/57610)
* \[[`8f643471ef`](https://github.com/nodejs/node/commit/8f643471ef)] - **fs**: improve globSync performance (Rich Trott) [#57725](https://github.com/nodejs/node/pull/57725)
* \[[`bf9e17ecc6`](https://github.com/nodejs/node/commit/bf9e17ecc6)] - **http2**: use args.This() instead of args.Holder() (Joyee Cheung) [#58004](https://github.com/nodejs/node/pull/58004)
* \[[`137717354f`](https://github.com/nodejs/node/commit/137717354f)] - **http2**: fix graceful session close (Kushagra Pandey) [#57808](https://github.com/nodejs/node/pull/57808)
* \[[`9baf580269`](https://github.com/nodejs/node/commit/9baf580269)] - **http2**: fix check for `frame->hd.type` (hanguanqiang) [#57644](https://github.com/nodejs/node/pull/57644)
* \[[`b8189242b2`](https://github.com/nodejs/node/commit/b8189242b2)] - **http2**: skip writeHead if stream is closed (Shima Ryuhei) [#57686](https://github.com/nodejs/node/pull/57686)
* \[[`4e02a1650a`](https://github.com/nodejs/node/commit/4e02a1650a)] - **lib**: remove unused file `fetch_module` (Michaël Zasso) [#55880](https://github.com/nodejs/node/pull/55880)
* \[[`d9700fef26`](https://github.com/nodejs/node/commit/d9700fef26)] - **lib**: avoid StackOverflow on `serializeError` (Chengzhong Wu) [#58075](https://github.com/nodejs/node/pull/58075)
* \[[`f3a16b6d9c`](https://github.com/nodejs/node/commit/f3a16b6d9c)] - **lib**: resolve the issue of not adhering to the specified buffer size (0hm☘️🏳️‍⚧️) [#55896](https://github.com/nodejs/node/pull/55896)
* \[[`d4fc282f73`](https://github.com/nodejs/node/commit/d4fc282f73)] - **lib**: fix AbortSignal.any() with timeout signals (Gürgün Dayıoğlu) [#57867](https://github.com/nodejs/node/pull/57867)
* \[[`f7e2902861`](https://github.com/nodejs/node/commit/f7e2902861)] - **lib**: use Map primordial for ActiveAsyncContextFrame (Gürgün Dayıoğlu) [#57670](https://github.com/nodejs/node/pull/57670)
* \[[`8652b0e168`](https://github.com/nodejs/node/commit/8652b0e168)] - **meta**: set nodejs/config as codeowner (Marco Ippolito) [#57237](https://github.com/nodejs/node/pull/57237)
* \[[`e98504ed95`](https://github.com/nodejs/node/commit/e98504ed95)] - **meta**: allow penetration testing on live system with prior authorization (Matteo Collina) [#57966](https://github.com/nodejs/node/pull/57966)
* \[[`340731bea0`](https://github.com/nodejs/node/commit/340731bea0)] - **meta**: fix subsystem in commit title (Luigi Pinca) [#57945](https://github.com/nodejs/node/pull/57945)
* \[[`d767cbffcf`](https://github.com/nodejs/node/commit/d767cbffcf)] - **meta**: bump Mozilla-Actions/sccache-action from 0.0.8 to 0.0.9 (dependabot\[bot]) [#57720](https://github.com/nodejs/node/pull/57720)
* \[[`575f904b13`](https://github.com/nodejs/node/commit/575f904b13)] - **meta**: bump actions/download-artifact from 4.1.9 to 4.2.1 (dependabot\[bot]) [#57719](https://github.com/nodejs/node/pull/57719)
* \[[`acd323c069`](https://github.com/nodejs/node/commit/acd323c069)] - **meta**: bump actions/setup-python from 5.4.0 to 5.5.0 (dependabot\[bot]) [#57718](https://github.com/nodejs/node/pull/57718)
* \[[`21246fec20`](https://github.com/nodejs/node/commit/21246fec20)] - **meta**: bump peter-evans/create-pull-request from 7.0.7 to 7.0.8 (dependabot\[bot]) [#57717](https://github.com/nodejs/node/pull/57717)
* \[[`97f32d5849`](https://github.com/nodejs/node/commit/97f32d5849)] - **meta**: bump github/codeql-action from 3.28.10 to 3.28.13 (dependabot\[bot]) [#57716](https://github.com/nodejs/node/pull/57716)
* \[[`90ddbb8cfa`](https://github.com/nodejs/node/commit/90ddbb8cfa)] - **meta**: bump actions/cache from 4.2.2 to 4.2.3 (dependabot\[bot]) [#57715](https://github.com/nodejs/node/pull/57715)
* \[[`728425d03e`](https://github.com/nodejs/node/commit/728425d03e)] - **meta**: bump actions/setup-node from 4.2.0 to 4.3.0 (dependabot\[bot]) [#57714](https://github.com/nodejs/node/pull/57714)
* \[[`1f799140e0`](https://github.com/nodejs/node/commit/1f799140e0)] - **meta**: bump actions/upload-artifact from 4.6.1 to 4.6.2 (dependabot\[bot]) [#57713](https://github.com/nodejs/node/pull/57713)
* \[[`021b174a1f`](https://github.com/nodejs/node/commit/021b174a1f)] - **module**: tidy code string concat → string templates (Jacob Smith) [#55820](https://github.com/nodejs/node/pull/55820)
* \[[`44c5718476`](https://github.com/nodejs/node/commit/44c5718476)] - **module**: fix incorrect formatting in require(esm) cycle error message (haykam821) [#57453](https://github.com/nodejs/node/pull/57453)
* \[[`bb09b4d4ae`](https://github.com/nodejs/node/commit/bb09b4d4ae)] - **module**: improve `getPackageType` performance (Dario Piotrowicz) [#57599](https://github.com/nodejs/node/pull/57599)
* \[[`9e6054e715`](https://github.com/nodejs/node/commit/9e6054e715)] - **module**: remove unnecessary `readPackage` function (Dario Piotrowicz) [#57596](https://github.com/nodejs/node/pull/57596)
* \[[`4a8db273ba`](https://github.com/nodejs/node/commit/4a8db273ba)] - **node-api**: add nested object wrap and napi\_ref test (Chengzhong Wu) [#57981](https://github.com/nodejs/node/pull/57981)
* \[[`3c65058f20`](https://github.com/nodejs/node/commit/3c65058f20)] - **node-api**: convert NewEnv to node\_napi\_env\_\_::New (Vladimir Morozov) [#57834](https://github.com/nodejs/node/pull/57834)
* \[[`a4105db1f7`](https://github.com/nodejs/node/commit/a4105db1f7)] - **os**: fix netmask format check condition in getCIDR function (Wiyeong Seo) [#57324](https://github.com/nodejs/node/pull/57324)
* \[[`248c938139`](https://github.com/nodejs/node/commit/248c938139)] - **process**: disable building execve on IBM i (Abdirahim Musse) [#57883](https://github.com/nodejs/node/pull/57883)
* \[[`972275697a`](https://github.com/nodejs/node/commit/972275697a)] - **repl**: deprecate `repl.builtinModules` (Dario Piotrowicz) [#57508](https://github.com/nodejs/node/pull/57508)
* \[[`7485309d7e`](https://github.com/nodejs/node/commit/7485309d7e)] - **sqlite**: add location method (Edy Silva) [#57860](https://github.com/nodejs/node/pull/57860)
* \[[`c12cd2a190`](https://github.com/nodejs/node/commit/c12cd2a190)] - **sqlite**: add timeout options to DatabaseSync (Edy Silva) [#57752](https://github.com/nodejs/node/pull/57752)
* \[[`5e0503a967`](https://github.com/nodejs/node/commit/5e0503a967)] - **sqlite**: add setReturnArrays method to StatementSync (Gürgün Dayıoğlu) [#57542](https://github.com/nodejs/node/pull/57542)
* \[[`ed9d2fd51a`](https://github.com/nodejs/node/commit/ed9d2fd51a)] - **sqlite**: enable common flags (Edy Silva) [#57621](https://github.com/nodejs/node/pull/57621)
* \[[`06dcb318bc`](https://github.com/nodejs/node/commit/06dcb318bc)] - **sqlite**: refactor prepared statement iterator (Colin Ihrig) [#57569](https://github.com/nodejs/node/pull/57569)
* \[[`c510391d2f`](https://github.com/nodejs/node/commit/c510391d2f)] - **(SEMVER-MINOR)** **sqlite**: add StatementSync.prototype.columns() (Colin Ihrig) [#57490](https://github.com/nodejs/node/pull/57490)
* \[[`4e24456a1a`](https://github.com/nodejs/node/commit/4e24456a1a)] - **sqlite**: reset statement immediately in run() (Colin Ihrig) [#57350](https://github.com/nodejs/node/pull/57350)
* \[[`a9a6891b0b`](https://github.com/nodejs/node/commit/a9a6891b0b)] - **sqlite**: fix coverity warnings related to backup() (Colin Ihrig) [#56961](https://github.com/nodejs/node/pull/56961)
* \[[`d2e1bcf3d4`](https://github.com/nodejs/node/commit/d2e1bcf3d4)] - **sqlite**: fix use-after-free in StatementSync due to premature GC (Divy Srivastava) [#56840](https://github.com/nodejs/node/pull/56840)
* \[[`cfe15ca7b4`](https://github.com/nodejs/node/commit/cfe15ca7b4)] - **sqlite**: handle conflicting SQLite and JS errors (Colin Ihrig) [#56787](https://github.com/nodejs/node/pull/56787)
* \[[`0e999eb65f`](https://github.com/nodejs/node/commit/0e999eb65f)] - **sqlite**: add getter to detect transactions (Colin Ihrig) [#57925](https://github.com/nodejs/node/pull/57925)
* \[[`20b27331c0`](https://github.com/nodejs/node/commit/20b27331c0)] - **sqlite, test**: expose sqlite online backup api (Edy Silva) [#56253](https://github.com/nodejs/node/pull/56253)
* \[[`8856712171`](https://github.com/nodejs/node/commit/8856712171)] - **sqlite,doc,test**: add aggregate function (Edy Silva) [#56600](https://github.com/nodejs/node/pull/56600)
* \[[`120050db97`](https://github.com/nodejs/node/commit/120050db97)] - **sqlite,src**: refactor sqlite value conversion (Edy Silva) [#57571](https://github.com/nodejs/node/pull/57571)
* \[[`4c5555d558`](https://github.com/nodejs/node/commit/4c5555d558)] - **src**: initialize privateSymbols for per\_context (Jason Zhang) [#57479](https://github.com/nodejs/node/pull/57479)
* \[[`d2ce9023b1`](https://github.com/nodejs/node/commit/d2ce9023b1)] - **src**: ensure primordials are initialized exactly once (Chengzhong Wu) [#57519](https://github.com/nodejs/node/pull/57519)
* \[[`06179be6ca`](https://github.com/nodejs/node/commit/06179be6ca)] - **src**: disable abseil deadlock detection (Chengzhong Wu) [#57582](https://github.com/nodejs/node/pull/57582)
* \[[`5121c47990`](https://github.com/nodejs/node/commit/5121c47990)] - **src**: fix node\_config\_file.h compilation error in GN build (Cheng) [#57210](https://github.com/nodejs/node/pull/57210)
* \[[`5d1230bec0`](https://github.com/nodejs/node/commit/5d1230bec0)] - **(SEMVER-MINOR)** **src**: set default config as `node.config.json` (Marco Ippolito) [#57171](https://github.com/nodejs/node/pull/57171)
* \[[`ccee741c43`](https://github.com/nodejs/node/commit/ccee741c43)] - **src**: namespace config file flags (Marco Ippolito) [#57170](https://github.com/nodejs/node/pull/57170)
* \[[`30bb1ccbb0`](https://github.com/nodejs/node/commit/30bb1ccbb0)] - **(SEMVER-MINOR)** **src**: create `THROW_ERR_OPTIONS_BEFORE_BOOTSTRAPPING` (Marco Ippolito) [#57016](https://github.com/nodejs/node/pull/57016)
* \[[`0350c6f478`](https://github.com/nodejs/node/commit/0350c6f478)] - **(SEMVER-MINOR)** **src**: add config file support (Marco Ippolito) [#57016](https://github.com/nodejs/node/pull/57016)
* \[[`eef37d00cb`](https://github.com/nodejs/node/commit/eef37d00cb)] - **src**: add more debug logs and comments in NodePlatform (Joyee Cheung) [#58047](https://github.com/nodejs/node/pull/58047)
* \[[`678e8f57c0`](https://github.com/nodejs/node/commit/678e8f57c0)] - **src**: add dcheck\_eq for Object::New constructor calls (Jonas) [#57943](https://github.com/nodejs/node/pull/57943)
* \[[`aee45e2036`](https://github.com/nodejs/node/commit/aee45e2036)] - **src**: move windows specific fns to `_WIN32` (Yagiz Nizipli) [#57951](https://github.com/nodejs/node/pull/57951)
* \[[`6206a8edbc`](https://github.com/nodejs/node/commit/6206a8edbc)] - **src**: improve thread safety of TaskQueue (Shelley Vohr) [#57910](https://github.com/nodejs/node/pull/57910)
* \[[`03936f31c1`](https://github.com/nodejs/node/commit/03936f31c1)] - **src**: fixup errorhandling more in various places (James M Snell) [#57852](https://github.com/nodejs/node/pull/57852)
* \[[`010dd91a19`](https://github.com/nodejs/node/commit/010dd91a19)] - **src**: fix typo in comments (Edy Silva) [#57868](https://github.com/nodejs/node/pull/57868)
* \[[`e00c1ecbd2`](https://github.com/nodejs/node/commit/e00c1ecbd2)] - **src**: add BaseObjectPtr nullptr operations (Chengzhong Wu) [#56585](https://github.com/nodejs/node/pull/56585)
* \[[`648ad252e1`](https://github.com/nodejs/node/commit/648ad252e1)] - **src**: remove `void*` -> `char*` -> `void*` casts (Tobias Nießen) [#57791](https://github.com/nodejs/node/pull/57791)
* \[[`680b434a62`](https://github.com/nodejs/node/commit/680b434a62)] - **src**: improve error handing in node\_messaging (James M Snell) [#57760](https://github.com/nodejs/node/pull/57760)
* \[[`18f5301747`](https://github.com/nodejs/node/commit/18f5301747)] - **src**: remove unused detachArrayBuffer method (Yagiz Nizipli) [#58055](https://github.com/nodejs/node/pull/58055)
* \[[`065e8cd670`](https://github.com/nodejs/node/commit/065e8cd670)] - **src**: use macros to reduce code duplication is cares\_wrap (James M Snell) [#57937](https://github.com/nodejs/node/pull/57937)
* \[[`39af5d678f`](https://github.com/nodejs/node/commit/39af5d678f)] - **src**: improve error handling in cares\_wrap (James M Snell) [#57937](https://github.com/nodejs/node/pull/57937)
* \[[`ca020fdc4e`](https://github.com/nodejs/node/commit/ca020fdc4e)] - **src**: fix -Wunreachable-code-return in node\_sea (Shelley Vohr) [#57664](https://github.com/nodejs/node/pull/57664)
* \[[`32b6e7094a`](https://github.com/nodejs/node/commit/32b6e7094a)] - **src**: change DCHECK to CHECK (Wuli Zuo) [#57948](https://github.com/nodejs/node/pull/57948)
* \[[`e1d3a9e192`](https://github.com/nodejs/node/commit/e1d3a9e192)] - **(SEMVER-MINOR)** **src**: add ExecutionAsyncId getter for any Context (Attila Szegedi) [#57820](https://github.com/nodejs/node/pull/57820)
* \[[`96243a723a`](https://github.com/nodejs/node/commit/96243a723a)] - **src**: update std::vector\<v8::Local\<T>> to use v8::LocalVector\<T> (Aditi) [#57646](https://github.com/nodejs/node/pull/57646)
* \[[`0f2cbc17c7`](https://github.com/nodejs/node/commit/0f2cbc17c7)] - **src**: update std::vector\<v8::Local\<T>> to use v8::LocalVector\<T> (Aditi) [#57642](https://github.com/nodejs/node/pull/57642)
* \[[`d1c6f861d5`](https://github.com/nodejs/node/commit/d1c6f861d5)] - **src**: update std::vector\<v8::Local\<T>> to use v8::LocalVector\<T> (Aditi) [#57578](https://github.com/nodejs/node/pull/57578)
* \[[`ab0d3a38db`](https://github.com/nodejs/node/commit/ab0d3a38db)] - **src**: improve error message for invalid child stdio type in spawn\_sync (Dario Piotrowicz) [#57589](https://github.com/nodejs/node/pull/57589)
* \[[`24b182e7b3`](https://github.com/nodejs/node/commit/24b182e7b3)] - **src**: implement util.types fast API calls (Ruben Bridgewater) [#57819](https://github.com/nodejs/node/pull/57819)
* \[[`dda6423be9`](https://github.com/nodejs/node/commit/dda6423be9)] - **src**: enter and lock isolate properly in json parser (Joyee Cheung) [#57823](https://github.com/nodejs/node/pull/57823)
* \[[`4754c693f8`](https://github.com/nodejs/node/commit/4754c693f8)] - **src**: improve error handling in `node_env_var.cc` (Antoine du Hamel) [#57767](https://github.com/nodejs/node/pull/57767)
* \[[`db483bbe63`](https://github.com/nodejs/node/commit/db483bbe63)] - **src**: improve error handling in node\_http2 (James M Snell) [#57764](https://github.com/nodejs/node/pull/57764)
* \[[`b0277700d6`](https://github.com/nodejs/node/commit/b0277700d6)] - **src**: improve error handling in crypto\_x509 (James M Snell) [#57757](https://github.com/nodejs/node/pull/57757)
* \[[`353587f984`](https://github.com/nodejs/node/commit/353587f984)] - **src**: improve error handling in callback.cc (James M Snell) [#57758](https://github.com/nodejs/node/pull/57758)
* \[[`bec053ab20`](https://github.com/nodejs/node/commit/bec053ab20)] - **src**: remove unused variable in crypto\_x509.cc (Michaël Zasso) [#57754](https://github.com/nodejs/node/pull/57754)
* \[[`38a329a857`](https://github.com/nodejs/node/commit/38a329a857)] - **src**: fix kill signal 0 on Windows (Stefan Stojanovic) [#57695](https://github.com/nodejs/node/pull/57695)
* \[[`70bb387f82`](https://github.com/nodejs/node/commit/70bb387f82)] - **src**: fix inefficient usage of v8\_inspector::StringView (Simon Zünd) [#52372](https://github.com/nodejs/node/pull/52372)
* \[[`be038f0273`](https://github.com/nodejs/node/commit/be038f0273)] - **src,permission**: make ERR\_ACCESS\_DENIED more descriptive (Rafael Gonzaga) [#57585](https://github.com/nodejs/node/pull/57585)
* \[[`0ec912f452`](https://github.com/nodejs/node/commit/0ec912f452)] - **(SEMVER-MINOR)** **stream**: preserve AsyncLocalStorage context in finished() (Gürgün Dayıoğlu) [#57865](https://github.com/nodejs/node/pull/57865)
* \[[`6ffb66f82f`](https://github.com/nodejs/node/commit/6ffb66f82f)] - **test**: fix permission fixtures lint (Rafael Gonzaga) [#55819](https://github.com/nodejs/node/pull/55819)
* \[[`fd37891186`](https://github.com/nodejs/node/commit/fd37891186)] - **test**: add repl preview timeout test (Chengzhong Wu) [#55484](https://github.com/nodejs/node/pull/55484)
* \[[`1be5a8c1b4`](https://github.com/nodejs/node/commit/1be5a8c1b4)] - **test**: skip `test-config-json-schema` with quic (Richard Lau) [#57225](https://github.com/nodejs/node/pull/57225)
* \[[`e90583b657`](https://github.com/nodejs/node/commit/e90583b657)] - **test**: add more coverage to `node_config_file` (Marco Ippolito) [#57170](https://github.com/nodejs/node/pull/57170)
* \[[`df2a36bfcc`](https://github.com/nodejs/node/commit/df2a36bfcc)] - **test**: remove deadlock workaround (Joyee Cheung) [#58047](https://github.com/nodejs/node/pull/58047)
* \[[`103034b051`](https://github.com/nodejs/node/commit/103034b051)] - **test**: prevent extraneous HOSTNAME substitution in test-runner-output (René) [#58076](https://github.com/nodejs/node/pull/58076)
* \[[`3e58f81a38`](https://github.com/nodejs/node/commit/3e58f81a38)] - **test**: update WPT for WebCryptoAPI to b48efd681e (Node.js GitHub Bot) [#58044](https://github.com/nodejs/node/pull/58044)
* \[[`2f4e4164a3`](https://github.com/nodejs/node/commit/2f4e4164a3)] - **test**: add missing newlines to repl .exit writes (Dario Piotrowicz) [#58041](https://github.com/nodejs/node/pull/58041)
* \[[`b40769292e`](https://github.com/nodejs/node/commit/b40769292e)] - **test**: add fast api tests for getLibuvNow() (Yagiz Nizipli) [#58022](https://github.com/nodejs/node/pull/58022)
* \[[`cbd5768d47`](https://github.com/nodejs/node/commit/cbd5768d47)] - **test**: add ALS test using http agent keep alive (Gerhard Stöbich) [#58017](https://github.com/nodejs/node/pull/58017)
* \[[`9e31ab502a`](https://github.com/nodejs/node/commit/9e31ab502a)] - **test**: deflake test-http2-options-max-headers-block-length (Luigi Pinca) [#57959](https://github.com/nodejs/node/pull/57959)
* \[[`13f8f9cc12`](https://github.com/nodejs/node/commit/13f8f9cc12)] - **test**: rename to getCallSites (Wuli Zuo) [#57948](https://github.com/nodejs/node/pull/57948)
* \[[`92dce6ed6b`](https://github.com/nodejs/node/commit/92dce6ed6b)] - **test**: force GC in test-file-write-stream4 (Luigi Pinca) [#57930](https://github.com/nodejs/node/pull/57930)
* \[[`aa755d3acf`](https://github.com/nodejs/node/commit/aa755d3acf)] - **test**: enable skipped colorize test (Shima Ryuhei) [#57887](https://github.com/nodejs/node/pull/57887)
* \[[`331f44c78c`](https://github.com/nodejs/node/commit/331f44c78c)] - **test**: update WPT for WebCryptoAPI to 164426ace2 (Node.js GitHub Bot) [#57854](https://github.com/nodejs/node/pull/57854)
* \[[`4aaa8438b4`](https://github.com/nodejs/node/commit/4aaa8438b4)] - **test**: add test for frame count being 0.5 (Jake Yuesong Li) [#57732](https://github.com/nodejs/node/pull/57732)
* \[[`fb51d3a0c5`](https://github.com/nodejs/node/commit/fb51d3a0c5)] - **test**: fix the decimal fractions explaination (Jake Yuesong Li) [#57732](https://github.com/nodejs/node/pull/57732)
* \[[`c6a45a9087`](https://github.com/nodejs/node/commit/c6a45a9087)] - _**Revert**_ "**test**: add tests for REPL custom evals" (Tobias Nießen) [#57793](https://github.com/nodejs/node/pull/57793)
* \[[`f3a4d03963`](https://github.com/nodejs/node/commit/f3a4d03963)] - **test**: add tests for REPL custom evals (Dario Piotrowicz) [#57691](https://github.com/nodejs/node/pull/57691)
* \[[`a3be0df337`](https://github.com/nodejs/node/commit/a3be0df337)] - **test**: update expected error message for macOS (Antoine du Hamel) [#57742](https://github.com/nodejs/node/pull/57742)
* \[[`a7e73a0a74`](https://github.com/nodejs/node/commit/a7e73a0a74)] - **test**: fix dangling promise in test\_runner no isolation test setup (Jacob Smith) [#57595](https://github.com/nodejs/node/pull/57595)
* \[[`edb7dd1ec7`](https://github.com/nodejs/node/commit/edb7dd1ec7)] - **test\_runner**: match minimum file column to 'all files' (Shima Ryuhei) [#57848](https://github.com/nodejs/node/pull/57848)
* \[[`c56f495e83`](https://github.com/nodejs/node/commit/c56f495e83)] - **tools**: extract target abseil to abseil.gyp (Chengzhong Wu) [#57289](https://github.com/nodejs/node/pull/57289)
* \[[`1b37161a27`](https://github.com/nodejs/node/commit/1b37161a27)] - **tools**: ignore V8 tests in CodeQL scans (Rich Trott) [#58081](https://github.com/nodejs/node/pull/58081)
* \[[`23386308dd`](https://github.com/nodejs/node/commit/23386308dd)] - **tools**: enable CodeQL config file (Rich Trott) [#58036](https://github.com/nodejs/node/pull/58036)
* \[[`9c21abc169`](https://github.com/nodejs/node/commit/9c21abc169)] - **tools**: ignore test directory in CodeQL scans (Rich Trott) [#57978](https://github.com/nodejs/node/pull/57978)
* \[[`f210a1530d`](https://github.com/nodejs/node/commit/f210a1530d)] - **tools**: add semver-major release support to release-lint (Antoine du Hamel) [#57892](https://github.com/nodejs/node/pull/57892)
* \[[`234c417e98`](https://github.com/nodejs/node/commit/234c417e98)] - **tools**: add codeql nightly (Rafael Gonzaga) [#57788](https://github.com/nodejs/node/pull/57788)
* \[[`938f1532da`](https://github.com/nodejs/node/commit/938f1532da)] - **tools**: edit create-release-proposal workflow to handle pr body length (Elves Vieira) [#57841](https://github.com/nodejs/node/pull/57841)
* \[[`b362339f72`](https://github.com/nodejs/node/commit/b362339f72)] - **tools**: add zstd updater to workflow (KASEYA\yahor.siarheyenka) [#57831](https://github.com/nodejs/node/pull/57831)
* \[[`61180db9c0`](https://github.com/nodejs/node/commit/61180db9c0)] - **tools**: remove unused `osx-pkg-postinstall.sh` (Antoine du Hamel) [#57667](https://github.com/nodejs/node/pull/57667)
* \[[`3ae04c94eb`](https://github.com/nodejs/node/commit/3ae04c94eb)] - **tools**: do not use temp files when merging PRs (Antoine du Hamel) [#57790](https://github.com/nodejs/node/pull/57790)
* \[[`d623c2c2b4`](https://github.com/nodejs/node/commit/d623c2c2b4)] - **tools**: update gyp-next to 0.20.0 (Node.js GitHub Bot) [#57683](https://github.com/nodejs/node/pull/57683)
* \[[`43ea4c532a`](https://github.com/nodejs/node/commit/43ea4c532a)] - **tools**: bump the eslint group in /tools/eslint with 4 updates (dependabot\[bot]) [#57721](https://github.com/nodejs/node/pull/57721)
* \[[`5703147470`](https://github.com/nodejs/node/commit/5703147470)] - **tools**: enable linter in `test/fixtures/source-map/output` (Antoine du Hamel) [#57700](https://github.com/nodejs/node/pull/57700)
* \[[`80d58c372d`](https://github.com/nodejs/node/commit/80d58c372d)] - **tools**: enable linter in `test/fixtures/errors` (Antoine du Hamel) [#57701](https://github.com/nodejs/node/pull/57701)
* \[[`ef5275b7be`](https://github.com/nodejs/node/commit/ef5275b7be)] - **tools**: enable linter in `test/fixtures/test-runner/output` (Antoine du Hamel) [#57698](https://github.com/nodejs/node/pull/57698)
* \[[`631733e41f`](https://github.com/nodejs/node/commit/631733e41f)] - **tools**: enable linter in `test/fixtures/eval` (Antoine du Hamel) [#57699](https://github.com/nodejs/node/pull/57699)
* \[[`6d0128695f`](https://github.com/nodejs/node/commit/6d0128695f)] - **tools**: enable linter on some fixtures file (Antoine du Hamel) [#57674](https://github.com/nodejs/node/pull/57674)
* \[[`f4d7cbae89`](https://github.com/nodejs/node/commit/f4d7cbae89)] - **tools**: update ESLint to 9.23 (Antoine du Hamel) [#57673](https://github.com/nodejs/node/pull/57673)
* \[[`5a39a24cd1`](https://github.com/nodejs/node/commit/5a39a24cd1)] - **typings**: fix `ModulesBinding` types (Antoine du Hamel) [#55549](https://github.com/nodejs/node/pull/55549)
* \[[`2df7ce9ebd`](https://github.com/nodejs/node/commit/2df7ce9ebd)] - **util**: fix parseEnv handling of invalid lines (Augustin Mauroy) [#57798](https://github.com/nodejs/node/pull/57798)
* \[[`416052a9f2`](https://github.com/nodejs/node/commit/416052a9f2)] - **util**: fix formatting of objects with built-in Symbol.toPrimitive (Shima Ryuhei) [#57832](https://github.com/nodejs/node/pull/57832)
* \[[`43490c8797`](https://github.com/nodejs/node/commit/43490c8797)] - **(SEMVER-MINOR)** **util**: add `types.isFloat16Array()` (Livia Medeiros) [#57879](https://github.com/nodejs/node/pull/57879)
* \[[`30060e13d3`](https://github.com/nodejs/node/commit/30060e13d3)] - **util**: preserve `length` of deprecated functions (Livia Medeiros) [#57806](https://github.com/nodejs/node/pull/57806)
* \[[`9837e08a84`](https://github.com/nodejs/node/commit/9837e08a84)] - **util**: fix parseEnv incorrectly splitting multiple ‘=‘ in value (HEESEUNG) [#57421](https://github.com/nodejs/node/pull/57421)
* \[[`af41dd3c07`](https://github.com/nodejs/node/commit/af41dd3c07)] - **watch**: clarify completion/failure watch mode messages (Dario Piotrowicz) [#57926](https://github.com/nodejs/node/pull/57926)
* \[[`7229a29b47`](https://github.com/nodejs/node/commit/7229a29b47)] - **watch**: check parent and child path properly (Jason Zhang) [#57425](https://github.com/nodejs/node/pull/57425)
* \[[`1b5a7c6dc8`](https://github.com/nodejs/node/commit/1b5a7c6dc8)] - **win**: fix SIGQUIT on ClangCL (Stefan Stojanovic) [#57659](https://github.com/nodejs/node/pull/57659)
* \[[`e935c3c6f2`](https://github.com/nodejs/node/commit/e935c3c6f2)] - **worker**: add ESM version examples to worker docs (fisker Cheung) [#57645](https://github.com/nodejs/node/pull/57645)
* \[[`dda6ca9172`](https://github.com/nodejs/node/commit/dda6ca9172)] - **(SEMVER-MINOR)** **worker**: add worker.getHeapStatistics() (Matteo Collina) [#57888](https://github.com/nodejs/node/pull/57888)
* \[[`f2159f2a44`](https://github.com/nodejs/node/commit/f2159f2a44)] - **zlib**: fix pointer alignment (jhofstee) [#57727](https://github.com/nodejs/node/pull/57727)

<a id="22.15.1"></a>

## 2025-05-14, Version 22.15.1 'Jod' (LTS), @RafaelGSS

This is a security release.

### Notable Changes

* (CVE-2025-23166) fix error handling on async crypto operation
* (CVE-2025-23165) add missing call to uv\_fs\_req\_cleanup

### Commits

* \[[`edaf54da00`](https://github.com/nodejs/node/commit/edaf54da00)] - **fs**: added test for missing call to uv\_fs\_req\_cleanup (Justin Nietzel) [#57811](https://github.com/nodejs/node/pull/57811)
* \[[`9f403e98ef`](https://github.com/nodejs/node/commit/9f403e98ef)] - **(CVE-2025-23165)** **fs**: add missing call to uv\_fs\_req\_cleanup (Justin Nietzel) [#57811](https://github.com/nodejs/node/pull/57811)
* \[[`f4494d38f1`](https://github.com/nodejs/node/commit/f4494d38f1)] - **(CVE-2025-23166)** **src**: fix error handling on async crypto operations (RafaelGSS) [nodejs-private/node-private#709](https://github.com/nodejs-private/node-private/pull/709)

<a id="22.15.0"></a>

## 2025-04-23, Version 22.15.0 'Jod' (LTS), @UlisesGascon prepared by @RafaelGSS

### Notable Changes

* \[[`3c88f3938b`](https://github.com/nodejs/node/commit/3c88f3938b)] - **(SEMVER-MINOR)** **assert**: implement partial error comparison (Ruben Bridgewater) [#57370](https://github.com/nodejs/node/pull/57370)
* \[[`db19a3f9fc`](https://github.com/nodejs/node/commit/db19a3f9fc)] - **(SEMVER-MINOR)** **assert**: improve partialDeepStrictEqual (Ruben Bridgewater) [#57370](https://github.com/nodejs/node/pull/57370)
* \[[`1ee5f840b4`](https://github.com/nodejs/node/commit/1ee5f840b4)] - **(SEMVER-MINOR)** **cli**: allow --cpu-prof\* in NODE\_OPTIONS (Carlos Espa) [#57018](https://github.com/nodejs/node/pull/57018)
* \[[`872ee0f2ac`](https://github.com/nodejs/node/commit/872ee0f2ac)] - **crypto**: update root certificates to NSS 3.108 (Node.js GitHub Bot) [#57381](https://github.com/nodejs/node/pull/57381)
* \[[`03a0f3a56b`](https://github.com/nodejs/node/commit/03a0f3a56b)] - **(SEMVER-MINOR)** **crypto**: support --use-system-ca on Windows (Joyee Cheung) [#56833](https://github.com/nodejs/node/pull/56833)
* \[[`94647bbdb2`](https://github.com/nodejs/node/commit/94647bbdb2)] - **(SEMVER-MINOR)** **crypto**: added support for reading certificates from macOS system store (Tim Jacomb) [#56599](https://github.com/nodejs/node/pull/56599)
* \[[`8f7b86a6e7`](https://github.com/nodejs/node/commit/8f7b86a6e7)] - **deps**: update timezone to 2025a (Node.js GitHub Bot) [#56876](https://github.com/nodejs/node/pull/56876)
* \[[`f9f611fb58`](https://github.com/nodejs/node/commit/f9f611fb58)] - **(SEMVER-MINOR)** **deps,tools**: add zstd 1.5.6 (Jan Martin) [#52100](https://github.com/nodejs/node/pull/52100)
* \[[`07a6d5f8cf`](https://github.com/nodejs/node/commit/07a6d5f8cf)] - **(SEMVER-MINOR)** **dns**: add TLSA record query and parsing (Rithvik Vibhu) [#52983](https://github.com/nodejs/node/pull/52983)
* \[[`d8a83ef2f3`](https://github.com/nodejs/node/commit/d8a83ef2f3)] - **doc**: add @geeksilva97 to collaborators (Edy Silva) [#57241](https://github.com/nodejs/node/pull/57241)
* \[[`6b93ba723b`](https://github.com/nodejs/node/commit/6b93ba723b)] - **(SEMVER-MINOR)** **module**: use synchronous hooks for preparsing in import(cjs) (Joyee Cheung) [#55698](https://github.com/nodejs/node/pull/55698)
* \[[`b2e44a8079`](https://github.com/nodejs/node/commit/b2e44a8079)] - **(SEMVER-MINOR)** **module**: implement module.registerHooks() (Joyee Cheung) [#55698](https://github.com/nodejs/node/pull/55698)
* \[[`dc91ae7471`](https://github.com/nodejs/node/commit/dc91ae7471)] - **(SEMVER-MINOR)** **process**: add execve (Paolo Insogna) [#56496](https://github.com/nodejs/node/pull/56496)
* \[[`bc672fcfdd`](https://github.com/nodejs/node/commit/bc672fcfdd)] - **(SEMVER-MINOR)** **sqlite**: allow returning `ArrayBufferView`s from user-defined functions (René) [#56790](https://github.com/nodejs/node/pull/56790)
* \[[`5edee197ab`](https://github.com/nodejs/node/commit/5edee197ab)] - **(SEMVER-MINOR)** **tls**: implement tls.getCACertificates() (Joyee Cheung) [#57107](https://github.com/nodejs/node/pull/57107)
* \[[`f9fe0e09ee`](https://github.com/nodejs/node/commit/f9fe0e09ee)] - **(SEMVER-MINOR)** **util**: expose diff function used by the assertion errors (Giovanni Bucci) [#57462](https://github.com/nodejs/node/pull/57462)
* \[[`673a424180`](https://github.com/nodejs/node/commit/673a424180)] - **(SEMVER-MINOR)** **v8**: add v8.getCppHeapStatistics() method (Aditi) [#57146](https://github.com/nodejs/node/pull/57146)
* \[[`4991e5d826`](https://github.com/nodejs/node/commit/4991e5d826)] - **(SEMVER-MINOR)** **zlib**: add zstd support (Jan Martin) [#52100](https://github.com/nodejs/node/pull/52100)

### Commits

* \[[`ea70a379c3`](https://github.com/nodejs/node/commit/ea70a379c3)] - **assert**: improve partialDeepStrictEqual performance (Ruben Bridgewater) [#57509](https://github.com/nodejs/node/pull/57509)
* \[[`2b419d7e79`](https://github.com/nodejs/node/commit/2b419d7e79)] - **(SEMVER-MINOR)** **assert**: implement partial error comparison (Ruben Bridgewater) [#57370](https://github.com/nodejs/node/pull/57370)
* \[[`d817c17fd7`](https://github.com/nodejs/node/commit/d817c17fd7)] - **(SEMVER-MINOR)** **assert**: improve partialDeepStrictEqual (Ruben Bridgewater) [#57370](https://github.com/nodejs/node/pull/57370)
* \[[`7af0440073`](https://github.com/nodejs/node/commit/7af0440073)] - **assert**: improve myers diff performance (Giovanni Bucci) [#57279](https://github.com/nodejs/node/pull/57279)
* \[[`01cf5fb871`](https://github.com/nodejs/node/commit/01cf5fb871)] - **(SEMVER-MINOR)** **assert,util**: improve performance (Ruben Bridgewater) [#57370](https://github.com/nodejs/node/pull/57370)
* \[[`a58842cee4`](https://github.com/nodejs/node/commit/a58842cee4)] - **(SEMVER-MINOR)** **benchmark**: adjust assert runtimes (Ruben Bridgewater) [#57370](https://github.com/nodejs/node/pull/57370)
* \[[`b20b3697aa`](https://github.com/nodejs/node/commit/b20b3697aa)] - **(SEMVER-MINOR)** **benchmark**: skip running some assert benchmarks by default (Ruben Bridgewater) [#57370](https://github.com/nodejs/node/pull/57370)
* \[[`ec5570fd1e`](https://github.com/nodejs/node/commit/ec5570fd1e)] - **(SEMVER-MINOR)** **benchmark**: add assert partialDeepStrictEqual benchmark (Ruben Bridgewater) [#57370](https://github.com/nodejs/node/pull/57370)
* \[[`b991bf4ca6`](https://github.com/nodejs/node/commit/b991bf4ca6)] - **benchmark**: add a warmup on bench-openSync (Elves Vieira) [#57051](https://github.com/nodejs/node/pull/57051)
* \[[`4a455bc806`](https://github.com/nodejs/node/commit/4a455bc806)] - **build**: fix update-wpt workflow (Jonas) [#57468](https://github.com/nodejs/node/pull/57468)
* \[[`6ec397e61c`](https://github.com/nodejs/node/commit/6ec397e61c)] - **build**: fix compatibility with V8's `depot_tools` (Richard Lau) [#57330](https://github.com/nodejs/node/pull/57330)
* \[[`475aaca336`](https://github.com/nodejs/node/commit/475aaca336)] - **build**: print 'Formatting Markdown...' for long task markdown formatting (1ilsang) [#57108](https://github.com/nodejs/node/pull/57108)
* \[[`73fced7a97`](https://github.com/nodejs/node/commit/73fced7a97)] - **build**: fix GN build failure (Cheng) [#57013](https://github.com/nodejs/node/pull/57013)
* \[[`af05f91425`](https://github.com/nodejs/node/commit/af05f91425)] - **build**: fix GN build of uv (Cheng) [#56955](https://github.com/nodejs/node/pull/56955)
* \[[`fd3053e947`](https://github.com/nodejs/node/commit/fd3053e947)] - **build**: gyp exclude libm linking on macOS (deepak1556) [#56901](https://github.com/nodejs/node/pull/56901)
* \[[`5ec6b9a50f`](https://github.com/nodejs/node/commit/5ec6b9a50f)] - **build**: remove explicit linker call to libm on macOS (deepak1556) [#56901](https://github.com/nodejs/node/pull/56901)
* \[[`a893da9be7`](https://github.com/nodejs/node/commit/a893da9be7)] - **build**: link with Security.framework in GN build (Cheng) [#56895](https://github.com/nodejs/node/pull/56895)
* \[[`02cd8e0a50`](https://github.com/nodejs/node/commit/02cd8e0a50)] - **build**: do not put commands in sources variables (Cheng) [#56885](https://github.com/nodejs/node/pull/56885)
* \[[`73dc8c2140`](https://github.com/nodejs/node/commit/73dc8c2140)] - **build**: add double quotes around <(python) (Luigi Pinca) [#56826](https://github.com/nodejs/node/pull/56826)
* \[[`65a3b5f73c`](https://github.com/nodejs/node/commit/65a3b5f73c)] - **build**: add build option suppress\_all\_error\_on\_warn (Michael Dawson) [#56647](https://github.com/nodejs/node/pull/56647)
* \[[`424aacc942`](https://github.com/nodejs/node/commit/424aacc942)] - **build,win**: disable node pch with ccache (Stefan Stojanovic) [#57224](https://github.com/nodejs/node/pull/57224)
* \[[`901685c723`](https://github.com/nodejs/node/commit/901685c723)] - **build,win**: enable ccache (Stefan Stojanovic) [#56847](https://github.com/nodejs/node/pull/56847)
* \[[`79987676c1`](https://github.com/nodejs/node/commit/79987676c1)] - **cli**: clarify --cpu-prof-name allowed values (Eugenio Ceschia) [#57433](https://github.com/nodejs/node/pull/57433)
* \[[`503d4237aa`](https://github.com/nodejs/node/commit/503d4237aa)] - **(SEMVER-MINOR)** **cli**: allow --cpu-prof\* in NODE\_OPTIONS (Carlos Espa) [#57018](https://github.com/nodejs/node/pull/57018)
* \[[`ada572b733`](https://github.com/nodejs/node/commit/ada572b733)] - **crypto**: ensure expected JWK alg in SubtleCrypto.importKey RSA imports (Filip Skokan) [#57450](https://github.com/nodejs/node/pull/57450)
* \[[`7e5aabde55`](https://github.com/nodejs/node/commit/7e5aabde55)] - **crypto**: update root certificates to NSS 3.108 (Node.js GitHub Bot) [#57381](https://github.com/nodejs/node/pull/57381)
* \[[`7ea6ac1e09`](https://github.com/nodejs/node/commit/7ea6ac1e09)] - **crypto**: add support for intermediate certs in --use-system-ca (Tim Jacomb) [#57164](https://github.com/nodejs/node/pull/57164)
* \[[`44b19ec534`](https://github.com/nodejs/node/commit/44b19ec534)] - **crypto**: support --use-system-ca on non-Windows and non-macOS (Joyee Cheung) [#57009](https://github.com/nodejs/node/pull/57009)
* \[[`e21d126438`](https://github.com/nodejs/node/commit/e21d126438)] - **crypto**: fix missing OPENSSL\_NO\_ENGINE guard (Shelley Vohr) [#57012](https://github.com/nodejs/node/pull/57012)
* \[[`2fdf82b357`](https://github.com/nodejs/node/commit/2fdf82b357)] - **crypto**: cleanup root certificates and skip PEM deserialization (Joyee Cheung) [#56999](https://github.com/nodejs/node/pull/56999)
* \[[`03a0f3a56b`](https://github.com/nodejs/node/commit/03a0f3a56b)] - **(SEMVER-MINOR)** **crypto**: support --use-system-ca on Windows (Joyee Cheung) [#56833](https://github.com/nodejs/node/pull/56833)
* \[[`bbdb10bc2c`](https://github.com/nodejs/node/commit/bbdb10bc2c)] - **crypto**: fix X509\* leak in --use-system-ca (Joyee Cheung) [#56832](https://github.com/nodejs/node/pull/56832)
* \[[`5470cab6d3`](https://github.com/nodejs/node/commit/5470cab6d3)] - **crypto**: add api to get openssl security level (Michael Dawson) [#56601](https://github.com/nodejs/node/pull/56601)
* \[[`94647bbdb2`](https://github.com/nodejs/node/commit/94647bbdb2)] - **(SEMVER-MINOR)** **crypto**: added support for reading certificates from macOS system store (Tim Jacomb) [#56599](https://github.com/nodejs/node/pull/56599)
* \[[`caf81ca549`](https://github.com/nodejs/node/commit/caf81ca549)] - **debugger**: fix behavior of plain object exec in debugger repl (Dario Piotrowicz) [#57498](https://github.com/nodejs/node/pull/57498)
* \[[`1d703fe220`](https://github.com/nodejs/node/commit/1d703fe220)] - **deps**: update c-ares to v1.34.5 (Node.js GitHub Bot) [#57792](https://github.com/nodejs/node/pull/57792)
* \[[`98457dfea3`](https://github.com/nodejs/node/commit/98457dfea3)] - **deps**: update undici to 6.21.2 (Matteo Collina) [#57442](https://github.com/nodejs/node/pull/57442)
* \[[`4a852ba11b`](https://github.com/nodejs/node/commit/4a852ba11b)] - **deps**: V8: cherry-pick c172ffc5bf54 (Choongwoo Han) [#57437](https://github.com/nodejs/node/pull/57437)
* \[[`54a12e0bcc`](https://github.com/nodejs/node/commit/54a12e0bcc)] - **deps**: update googletest to 0bdccf4 (Node.js GitHub Bot) [#57380](https://github.com/nodejs/node/pull/57380)
* \[[`2e350963e5`](https://github.com/nodejs/node/commit/2e350963e5)] - **deps**: update acorn to 8.14.1 (Node.js GitHub Bot) [#57382](https://github.com/nodejs/node/pull/57382)
* \[[`95e5d01c25`](https://github.com/nodejs/node/commit/95e5d01c25)] - **deps**: update amaro to 0.4.1 (marco-ippolito) [#57121](https://github.com/nodejs/node/pull/57121)
* \[[`ef216deb05`](https://github.com/nodejs/node/commit/ef216deb05)] - **deps**: update amaro to 0.3.2 (marco-ippolito) [#56916](https://github.com/nodejs/node/pull/56916)
* \[[`4ef4d6ecf6`](https://github.com/nodejs/node/commit/4ef4d6ecf6)] - **deps**: update amaro to 0.3.1 (Node.js GitHub Bot) [#56785](https://github.com/nodejs/node/pull/56785)
* \[[`a8bf5ef4a7`](https://github.com/nodejs/node/commit/a8bf5ef4a7)] - **deps**: update simdjson to 3.12.2 (Node.js GitHub Bot) [#57084](https://github.com/nodejs/node/pull/57084)
* \[[`0bd612bb32`](https://github.com/nodejs/node/commit/0bd612bb32)] - **deps**: update archs files for openssl-3.0.16 (Node.js GitHub Bot) [#57335](https://github.com/nodejs/node/pull/57335)
* \[[`7d65f79306`](https://github.com/nodejs/node/commit/7d65f79306)] - **deps**: upgrade openssl sources to quictls/openssl-3.0.16 (Node.js GitHub Bot) [#57335](https://github.com/nodejs/node/pull/57335)
* \[[`5c88c52491`](https://github.com/nodejs/node/commit/5c88c52491)] - **deps**: update corepack to 0.32.0 (Node.js GitHub Bot) [#57265](https://github.com/nodejs/node/pull/57265)
* \[[`fa04bf4999`](https://github.com/nodejs/node/commit/fa04bf4999)] - **deps**: update gyp file for ngtcp2 1.11.0 (Richard Lau) [#57225](https://github.com/nodejs/node/pull/57225)
* \[[`ca6b07258d`](https://github.com/nodejs/node/commit/ca6b07258d)] - **deps**: update cjs-module-lexer to 2.1.0 (Node.js GitHub Bot) [#57180](https://github.com/nodejs/node/pull/57180)
* \[[`0a72b16fe1`](https://github.com/nodejs/node/commit/0a72b16fe1)] - **deps**: update ngtcp2 to 1.11.0 (Node.js GitHub Bot) [#57179](https://github.com/nodejs/node/pull/57179)
* \[[`600fb41f54`](https://github.com/nodejs/node/commit/600fb41f54)] - **deps**: update sqlite to 3.49.1 (Node.js GitHub Bot) [#57178](https://github.com/nodejs/node/pull/57178)
* \[[`7eb3b44010`](https://github.com/nodejs/node/commit/7eb3b44010)] - **deps**: update zlib to 1.3.0.1-motley-788cb3c (Node.js GitHub Bot) [#56655](https://github.com/nodejs/node/pull/56655)
* \[[`257d22e181`](https://github.com/nodejs/node/commit/257d22e181)] - **deps**: update sqlite to 3.49.0 (Node.js GitHub Bot) [#56654](https://github.com/nodejs/node/pull/56654)
* \[[`53a7bfce01`](https://github.com/nodejs/node/commit/53a7bfce01)] - **deps**: V8: cherry-pick 9ab40592f697 (Levi Zim) [#56781](https://github.com/nodejs/node/pull/56781)
* \[[`636f65cb1a`](https://github.com/nodejs/node/commit/636f65cb1a)] - **deps**: update cjs-module-lexer to 2.0.0 (Michael Dawson) [#56855](https://github.com/nodejs/node/pull/56855)
* \[[`8f7b86a6e7`](https://github.com/nodejs/node/commit/8f7b86a6e7)] - **deps**: update timezone to 2025a (Node.js GitHub Bot) [#56876](https://github.com/nodejs/node/pull/56876)
* \[[`db31276bfa`](https://github.com/nodejs/node/commit/db31276bfa)] - **deps**: update simdjson to 3.12.0 (Node.js GitHub Bot) [#56874](https://github.com/nodejs/node/pull/56874)
* \[[`d1d58d6198`](https://github.com/nodejs/node/commit/d1d58d6198)] - **deps**: update googletest to e235eb3 (Node.js GitHub Bot) [#56873](https://github.com/nodejs/node/pull/56873)
* \[[`05b3dff275`](https://github.com/nodejs/node/commit/05b3dff275)] - **deps**: update simdjson to 3.11.6 (Node.js GitHub Bot) [#56250](https://github.com/nodejs/node/pull/56250)
* \[[`f9f611fb58`](https://github.com/nodejs/node/commit/f9f611fb58)] - **(SEMVER-MINOR)** **deps,tools**: add zstd 1.5.6 (Jan Martin) [#52100](https://github.com/nodejs/node/pull/52100)
* \[[`ef212a41a7`](https://github.com/nodejs/node/commit/ef212a41a7)] - **dns**: restore dns query cache ttl (Ethan Arrowood) [#57640](https://github.com/nodejs/node/pull/57640)
* \[[`7a10b01e97`](https://github.com/nodejs/node/commit/7a10b01e97)] - **dns**: remove redundant code using common variable (Deokjin Kim) [#57386](https://github.com/nodejs/node/pull/57386)
* \[[`bc2603f086`](https://github.com/nodejs/node/commit/bc2603f086)] - **(SEMVER-MINOR)** **dns**: add TLSA record query and parsing (Rithvik Vibhu) [#52983](https://github.com/nodejs/node/pull/52983)
* \[[`38a2e5d60b`](https://github.com/nodejs/node/commit/38a2e5d60b)] - **doc**: add gurgunday as triager (Gürgün Dayıoğlu) [#57594](https://github.com/nodejs/node/pull/57594)
* \[[`b7ac0bd129`](https://github.com/nodejs/node/commit/b7ac0bd129)] - **doc**: clarify behaviour of node-api adjust function (Michael Dawson) [#57463](https://github.com/nodejs/node/pull/57463)
* \[[`fa834896c8`](https://github.com/nodejs/node/commit/fa834896c8)] - **doc**: remove Corepack documentation (Antoine du Hamel) [#57635](https://github.com/nodejs/node/pull/57635)
* \[[`8988173286`](https://github.com/nodejs/node/commit/8988173286)] - **doc**: remove mention of `--require` not supporting ES modules (Huáng Jùnliàng) [#57620](https://github.com/nodejs/node/pull/57620)
* \[[`3a7d179dbd`](https://github.com/nodejs/node/commit/3a7d179dbd)] - **doc**: mention reports should align with Node.js CoC (Rafael Gonzaga) [#57607](https://github.com/nodejs/node/pull/57607)
* \[[`983c5087f6`](https://github.com/nodejs/node/commit/983c5087f6)] - **doc**: add section stating that very stale PRs should be closed (Dario Piotrowicz) [#57541](https://github.com/nodejs/node/pull/57541)
* \[[`f4e1f702d4`](https://github.com/nodejs/node/commit/f4e1f702d4)] - **doc**: add bjohansebas as triager (Sebastian Beltran) [#57564](https://github.com/nodejs/node/pull/57564)
* \[[`9b7fd6b076`](https://github.com/nodejs/node/commit/9b7fd6b076)] - **doc**: update support channels (Claudio W.) [#57538](https://github.com/nodejs/node/pull/57538)
* \[[`ef624aff55`](https://github.com/nodejs/node/commit/ef624aff55)] - **doc**: remove cryptoStream API reference (Jonas) [#57579](https://github.com/nodejs/node/pull/57579)
* \[[`4a2afc255a`](https://github.com/nodejs/node/commit/4a2afc255a)] - **doc**: module resolution pseudocode corrections (Marcel Laverdet) [#57080](https://github.com/nodejs/node/pull/57080)
* \[[`ee5059426d`](https://github.com/nodejs/node/commit/ee5059426d)] - **doc**: add history entry for DEP0190 in `child_process.md` (Antoine du Hamel) [#57544](https://github.com/nodejs/node/pull/57544)
* \[[`4deebb4fca`](https://github.com/nodejs/node/commit/4deebb4fca)] - **doc**: remove deprecated pattern in `child_process.md` (Antoine du Hamel) [#57568](https://github.com/nodejs/node/pull/57568)
* \[[`6cd7b37d9c`](https://github.com/nodejs/node/commit/6cd7b37d9c)] - **doc**: mark multiple experimental APIS as stable (James M Snell) [#57510](https://github.com/nodejs/node/pull/57510)
* \[[`c2f1fa0928`](https://github.com/nodejs/node/commit/c2f1fa0928)] - **doc**: remove mertcanaltin from Triagers (Mert Can Altin) [#57531](https://github.com/nodejs/node/pull/57531)
* \[[`9b6047e520`](https://github.com/nodejs/node/commit/9b6047e520)] - **doc**: recommend watching the collaborators repo in the onboarding doc (Darshan Sen) [#57527](https://github.com/nodejs/node/pull/57527)
* \[[`bf1e297079`](https://github.com/nodejs/node/commit/bf1e297079)] - **doc**: remove mention of visa fees from onboarding doc (Darshan Sen) [#57526](https://github.com/nodejs/node/pull/57526)
* \[[`1041331094`](https://github.com/nodejs/node/commit/1041331094)] - **doc**: deprecate passing `args` to `spawn` and `execFile` (Antoine du Hamel) [#57389](https://github.com/nodejs/node/pull/57389)
* \[[`06994d5a75`](https://github.com/nodejs/node/commit/06994d5a75)] - **doc**: remove some inconsistencies in `deprecations.md` (Antoine du Hamel) [#57512](https://github.com/nodejs/node/pull/57512)
* \[[`707f851ba3`](https://github.com/nodejs/node/commit/707f851ba3)] - **doc**: run license-builder (github-actions\[bot]) [#57511](https://github.com/nodejs/node/pull/57511)
* \[[`a7793195d6`](https://github.com/nodejs/node/commit/a7793195d6)] - **doc**: add new writing-docs contributing md (Dario Piotrowicz) [#57502](https://github.com/nodejs/node/pull/57502)
* \[[`30d4a43b3d`](https://github.com/nodejs/node/commit/30d4a43b3d)] - **doc**: add node.js streams references to Web Streams doc (Dario Piotrowicz) [#57393](https://github.com/nodejs/node/pull/57393)
* \[[`e08365980b`](https://github.com/nodejs/node/commit/e08365980b)] - **doc**: prefer to sign commits under nodejs repository (Rafael Gonzaga) [#57311](https://github.com/nodejs/node/pull/57311)
* \[[`c35e1f9048`](https://github.com/nodejs/node/commit/c35e1f9048)] - **doc**: fixed the incorrect splitting of multiple words (letianpailove) [#57454](https://github.com/nodejs/node/pull/57454)
* \[[`3e1f3bc2bb`](https://github.com/nodejs/node/commit/3e1f3bc2bb)] - **doc**: add review guidelines for collaborator nominations (Antoine du Hamel) [#57449](https://github.com/nodejs/node/pull/57449)
* \[[`fef3f82a41`](https://github.com/nodejs/node/commit/fef3f82a41)] - **doc**: add history info for --use-system-ca (Darshan Sen) [#57432](https://github.com/nodejs/node/pull/57432)
* \[[`96afdf949d`](https://github.com/nodejs/node/commit/96afdf949d)] - **doc**: remove typo YAML snippet from tls.getCACertificates doc (Darshan Sen) [#57459](https://github.com/nodejs/node/pull/57459)
* \[[`800d61d47e`](https://github.com/nodejs/node/commit/800d61d47e)] - **doc**: fix typo in sqlite.md (Tobias Nießen) [#57473](https://github.com/nodejs/node/pull/57473)
* \[[`4876aee775`](https://github.com/nodejs/node/commit/4876aee775)] - **doc**: explicit mention arbitrary code execution as a vuln (Rafael Gonzaga) [#57426](https://github.com/nodejs/node/pull/57426)
* \[[`2dd72c658f`](https://github.com/nodejs/node/commit/2dd72c658f)] - **doc**: update maintaining-openssl.md for openssl (Richard Lau) [#57413](https://github.com/nodejs/node/pull/57413)
* \[[`a49fd31f04`](https://github.com/nodejs/node/commit/a49fd31f04)] - **doc**: add missing `deprecated` badges in `fs.md` (Yukihiro Hasegawa) [#57384](https://github.com/nodejs/node/pull/57384)
* \[[`3a4ed77674`](https://github.com/nodejs/node/commit/3a4ed77674)] - **doc**: add note about sync nodejs-private branches (Rafael Gonzaga) [#57404](https://github.com/nodejs/node/pull/57404)
* \[[`1025e6dc7c`](https://github.com/nodejs/node/commit/1025e6dc7c)] - **doc**: update Xcode version used for arm64 and pkg (Michaël Zasso) [#57104](https://github.com/nodejs/node/pull/57104)
* \[[`77b9e04a70`](https://github.com/nodejs/node/commit/77b9e04a70)] - **doc**: improve type stripping documentation (Marco Ippolito) [#56916](https://github.com/nodejs/node/pull/56916)
* \[[`3a75e8410d`](https://github.com/nodejs/node/commit/3a75e8410d)] - **doc**: specificy support for erasable ts syntax (Marco Ippolito) [#56916](https://github.com/nodejs/node/pull/56916)
* \[[`69f12f9686`](https://github.com/nodejs/node/commit/69f12f9686)] - **doc**: make first parameter optional in `util.getCallSites` (Deokjin Kim) [#57387](https://github.com/nodejs/node/pull/57387)
* \[[`2b4e737ffb`](https://github.com/nodejs/node/commit/2b4e737ffb)] - **doc**: fix usage of module.registerSync in comment (Timo Kössler) [#57328](https://github.com/nodejs/node/pull/57328)
* \[[`f320593958`](https://github.com/nodejs/node/commit/f320593958)] - **doc**: add Darshan back as voting TSC member (Michael Dawson) [#57402](https://github.com/nodejs/node/pull/57402)
* \[[`2b7765469a`](https://github.com/nodejs/node/commit/2b7765469a)] - **doc**: revise webcrypto.md types, interfaces, and added versions (Filip Skokan) [#57376](https://github.com/nodejs/node/pull/57376)
* \[[`649828c74a`](https://github.com/nodejs/node/commit/649828c74a)] - **doc**: add info on how project manages social media (Michael Dawson) [#57318](https://github.com/nodejs/node/pull/57318)
* \[[`2a2e1cfd71`](https://github.com/nodejs/node/commit/2a2e1cfd71)] - **doc**: revise `tsconfig.json` note (Steven) [#57353](https://github.com/nodejs/node/pull/57353)
* \[[`17883b1d46`](https://github.com/nodejs/node/commit/17883b1d46)] - **doc**: use more clear name in getSystemErrorMessage's example (ikuma-t) [#57310](https://github.com/nodejs/node/pull/57310)
* \[[`7feed9989b`](https://github.com/nodejs/node/commit/7feed9989b)] - **doc**: recommend setting `noEmit: true` in `tsconfig.json` (Steven) [#57320](https://github.com/nodejs/node/pull/57320)
* \[[`fe707ab162`](https://github.com/nodejs/node/commit/fe707ab162)] - **doc**: ping nodejs/tsc for each security pull request (Rafael Gonzaga) [#57309](https://github.com/nodejs/node/pull/57309)
* \[[`f3c58ab693`](https://github.com/nodejs/node/commit/f3c58ab693)] - **doc**: fix Windows ccache section position (Stefan Stojanovic) [#57326](https://github.com/nodejs/node/pull/57326)
* \[[`e69170bacd`](https://github.com/nodejs/node/commit/e69170bacd)] - **doc**: update node-api version matrix (Chengzhong Wu) [#57287](https://github.com/nodejs/node/pull/57287)
* \[[`0bc1fd2245`](https://github.com/nodejs/node/commit/0bc1fd2245)] - **doc**: recommend `erasableSyntaxOnly` in ts docs (Rob Palmer) [#57271](https://github.com/nodejs/node/pull/57271)
* \[[`068013744e`](https://github.com/nodejs/node/commit/068013744e)] - **doc**: clarify `path.isAbsolute` is not path traversal mitigation (Eric Fortis) [#57073](https://github.com/nodejs/node/pull/57073)
* \[[`238b0e856e`](https://github.com/nodejs/node/commit/238b0e856e)] - **doc**: fix rendering of DEP0174 description (David Sanders) [#56835](https://github.com/nodejs/node/pull/56835)
* \[[`db0bcefd14`](https://github.com/nodejs/node/commit/db0bcefd14)] - **doc**: add 1ilsang to triage team (1ilsang) [#57183](https://github.com/nodejs/node/pull/57183)
* \[[`52a593feab`](https://github.com/nodejs/node/commit/52a593feab)] - **doc**: add @geeksilva97 to collaborators (Edy Silva) [#57241](https://github.com/nodejs/node/pull/57241)
* \[[`89f4475e32`](https://github.com/nodejs/node/commit/89f4475e32)] - **doc**: add missing assert return types (Colin Ihrig) [#57219](https://github.com/nodejs/node/pull/57219)
* \[[`62b6d94c03`](https://github.com/nodejs/node/commit/62b6d94c03)] - **doc**: add streamResetBurst and streamResetRate (Sujal Raj) [#57195](https://github.com/nodejs/node/pull/57195)
* \[[`f150017e70`](https://github.com/nodejs/node/commit/f150017e70)] - **doc**: add esm examples to node:util (Alfredo González) [#56793](https://github.com/nodejs/node/pull/56793)
* \[[`99465ffa9c`](https://github.com/nodejs/node/commit/99465ffa9c)] - **doc**: update options to filehandle.appendFile() (Hasegawa-Yukihiro) [#56972](https://github.com/nodejs/node/pull/56972)
* \[[`6242520a90`](https://github.com/nodejs/node/commit/6242520a90)] - **doc**: add additional caveat for fs.watch (Michael Dawson) [#57150](https://github.com/nodejs/node/pull/57150)
* \[[`19cda4791a`](https://github.com/nodejs/node/commit/19cda4791a)] - **doc**: fix typo in Windows building instructions (Tim Jacomb) [#57158](https://github.com/nodejs/node/pull/57158)
* \[[`ef206add59`](https://github.com/nodejs/node/commit/ef206add59)] - **doc**: fix web.libera.chat link in pull-requests.md (Samuel Bronson) [#57076](https://github.com/nodejs/node/pull/57076)
* \[[`7243c1713d`](https://github.com/nodejs/node/commit/7243c1713d)] - **doc**: remove buffered flag from performance hooks examples (Pavel Romanov) [#52607](https://github.com/nodejs/node/pull/52607)
* \[[`617fe71f67`](https://github.com/nodejs/node/commit/617fe71f67)] - **doc**: fix 'introduced\_in' version in typescript module (1ilsang) [#57109](https://github.com/nodejs/node/pull/57109)
* \[[`6cc15b8dc9`](https://github.com/nodejs/node/commit/6cc15b8dc9)] - **doc**: fix link and history of `SourceMap` sections (Antoine du Hamel) [#57098](https://github.com/nodejs/node/pull/57098)
* \[[`6be8189041`](https://github.com/nodejs/node/commit/6be8189041)] - **doc**: add `module namespace object` links (Dario Piotrowicz) [#57093](https://github.com/nodejs/node/pull/57093)
* \[[`8611c4a3ea`](https://github.com/nodejs/node/commit/8611c4a3ea)] - **doc**: disambiguate pseudo-code statement (Dario Piotrowicz) [#57092](https://github.com/nodejs/node/pull/57092)
* \[[`79da145a55`](https://github.com/nodejs/node/commit/79da145a55)] - **doc**: update clang-cl on Windows building guide (Joyee Cheung) [#57087](https://github.com/nodejs/node/pull/57087)
* \[[`845eaf91be`](https://github.com/nodejs/node/commit/845eaf91be)] - **doc**: fix wrong articles used to address modules (Dario Piotrowicz) [#57090](https://github.com/nodejs/node/pull/57090)
* \[[`42c5e23eb1`](https://github.com/nodejs/node/commit/42c5e23eb1)] - **doc**: `modules.md`: fix `distance` definition (Alexander “weej” Jones) [#57046](https://github.com/nodejs/node/pull/57046)
* \[[`bda851aaa3`](https://github.com/nodejs/node/commit/bda851aaa3)] - **doc**: fix wrong verb form (Dario Piotrowicz) [#57091](https://github.com/nodejs/node/pull/57091)
* \[[`64e13fd36e`](https://github.com/nodejs/node/commit/64e13fd36e)] - **doc**: fix transpiler loader hooks documentation (Joyee Cheung) [#57037](https://github.com/nodejs/node/pull/57037)
* \[[`51494d8b78`](https://github.com/nodejs/node/commit/51494d8b78)] - **doc**: add a note about `require('../common')` in testing documentation (Aditi) [#56953](https://github.com/nodejs/node/pull/56953)
* \[[`053b128e9c`](https://github.com/nodejs/node/commit/053b128e9c)] - **doc**: recommend writing tests in new files and including comments (Joyee Cheung) [#57028](https://github.com/nodejs/node/pull/57028)
* \[[`a20c62a00c`](https://github.com/nodejs/node/commit/a20c62a00c)] - **doc**: improve documentation on argument validation (Aditi) [#56954](https://github.com/nodejs/node/pull/56954)
* \[[`2921658813`](https://github.com/nodejs/node/commit/2921658813)] - **doc**: buffer: fix typo on `Buffer.copyBytesFrom(` `offset` option (tpoisseau) [#57015](https://github.com/nodejs/node/pull/57015)
* \[[`6f4ab1c9b2`](https://github.com/nodejs/node/commit/6f4ab1c9b2)] - **doc**: update cleanup to trust on vuln db automation (Rafael Gonzaga) [#57004](https://github.com/nodejs/node/pull/57004)
* \[[`5285facb3e`](https://github.com/nodejs/node/commit/5285facb3e)] - **doc**: move stability index after history section for consistency (Antoine du Hamel) [#56997](https://github.com/nodejs/node/pull/56997)
* \[[`a7646e17ff`](https://github.com/nodejs/node/commit/a7646e17ff)] - **doc**: add `signal` to `filehandle.writeFile()` options (Yukihiro Hasegawa) [#56804](https://github.com/nodejs/node/pull/56804)
* \[[`ba031089e6`](https://github.com/nodejs/node/commit/ba031089e6)] - **doc**: run license-builder (github-actions\[bot]) [#56985](https://github.com/nodejs/node/pull/56985)
* \[[`afa6f93a32`](https://github.com/nodejs/node/commit/afa6f93a32)] - **doc**: update history of stream.Readable.toWeb() (Jimmy Leung) [#56928](https://github.com/nodejs/node/pull/56928)
* \[[`cc644de126`](https://github.com/nodejs/node/commit/cc644de126)] - **doc**: make MDN links to global classes more consistent (Antoine du Hamel) [#56924](https://github.com/nodejs/node/pull/56924)
* \[[`93bba4eee1`](https://github.com/nodejs/node/commit/93bba4eee1)] - **doc**: make MDN links to global classes more consistent in `assert.md` (Antoine du Hamel) [#56920](https://github.com/nodejs/node/pull/56920)
* \[[`ad03c85f98`](https://github.com/nodejs/node/commit/ad03c85f98)] - **doc**: make MDN links to global classes more consistent (Antoine du Hamel) [#56923](https://github.com/nodejs/node/pull/56923)
* \[[`96c2a90dee`](https://github.com/nodejs/node/commit/96c2a90dee)] - **doc**: make MDN links to global classes more consistent in `util.md` (Antoine du Hamel) [#56922](https://github.com/nodejs/node/pull/56922)
* \[[`6bb73c0745`](https://github.com/nodejs/node/commit/6bb73c0745)] - **doc**: make MDN links to global classes more consistent in `buffer.md` (Antoine du Hamel) [#56921](https://github.com/nodejs/node/pull/56921)
* \[[`824cf35475`](https://github.com/nodejs/node/commit/824cf35475)] - **doc**: update post sec release process (Rafael Gonzaga) [#56907](https://github.com/nodejs/node/pull/56907)
* \[[`027749eb17`](https://github.com/nodejs/node/commit/027749eb17)] - **doc**: update websocket link to avoid linking to self (Chengzhong Wu) [#56897](https://github.com/nodejs/node/pull/56897)
* \[[`5dcb9d632b`](https://github.com/nodejs/node/commit/5dcb9d632b)] - **doc**: mark `--env-file-if-exists` flag as experimental (Juan José) [#56893](https://github.com/nodejs/node/pull/56893)
* \[[`4f6d751bf5`](https://github.com/nodejs/node/commit/4f6d751bf5)] - **doc**: fix typo in cjs example of `util.styleText` (Deokjin Kim) [#56769](https://github.com/nodejs/node/pull/56769)
* \[[`313d9db7a5`](https://github.com/nodejs/node/commit/313d9db7a5)] - **doc**: clarify sqlite user-defined function behaviour (René) [#56786](https://github.com/nodejs/node/pull/56786)
* \[[`eff42956c4`](https://github.com/nodejs/node/commit/eff42956c4)] - **doc**: correct customization hook types & clarify descriptions (Jacob Smith) [#56454](https://github.com/nodejs/node/pull/56454)
* \[[`64180421c2`](https://github.com/nodejs/node/commit/64180421c2)] - **events**: getMaxListeners detects 0 listeners (Matthew Aitken) [#56807](https://github.com/nodejs/node/pull/56807)
* \[[`2de27787b4`](https://github.com/nodejs/node/commit/2de27787b4)] - **fs**: apply exclude function to root path (Rich Trott) [#57420](https://github.com/nodejs/node/pull/57420)
* \[[`b6df9e350a`](https://github.com/nodejs/node/commit/b6df9e350a)] - **fs**: handle UV\_ENOTDIR in `fs.statSync` with `throwIfNoEntry` provided (Juan José Arboleda) [#56996](https://github.com/nodejs/node/pull/56996)
* \[[`14b2d496a0`](https://github.com/nodejs/node/commit/14b2d496a0)] - **fs**: make `FileHandle.readableWebStream` always create byte streams (Ian Kerins) [#55461](https://github.com/nodejs/node/pull/55461)
* \[[`10d2f1d898`](https://github.com/nodejs/node/commit/10d2f1d898)] - **http**: coerce content-length to number (Marco Ippolito) [#57458](https://github.com/nodejs/node/pull/57458)
* \[[`9192b7fa25`](https://github.com/nodejs/node/commit/9192b7fa25)] - **http**: be more generational GC friendly (ywave620) [#56767](https://github.com/nodejs/node/pull/56767)
* \[[`1cf98a8788`](https://github.com/nodejs/node/commit/1cf98a8788)] - **inspector**: convert event params to protocol without json (Chengzhong Wu) [#57027](https://github.com/nodejs/node/pull/57027)
* \[[`6dcad868bb`](https://github.com/nodejs/node/commit/6dcad868bb)] - **inspector**: skip promise hook in the inspector async hook (Joyee Cheung) [#57148](https://github.com/nodejs/node/pull/57148)
* \[[`787e93f75a`](https://github.com/nodejs/node/commit/787e93f75a)] - **inspector**: add Network.Initiator in inspector protocol (Chengzhong Wu) [#56805](https://github.com/nodejs/node/pull/56805)
* \[[`c7c04d0dc8`](https://github.com/nodejs/node/commit/c7c04d0dc8)] - **inspector**: fix GN build (Cheng) [#56798](https://github.com/nodejs/node/pull/56798)
* \[[`177da9c3c3`](https://github.com/nodejs/node/commit/177da9c3c3)] - **inspector**: fix StringUtil::CharacterCount for unicodes (Chengzhong Wu) [#56788](https://github.com/nodejs/node/pull/56788)
* \[[`1b5418eeea`](https://github.com/nodejs/node/commit/1b5418eeea)] - **lib**: add warning when binding inspector to public IP (Demian Parkhomenko) [#55736](https://github.com/nodejs/node/pull/55736)
* \[[`cc4d33842b`](https://github.com/nodejs/node/commit/cc4d33842b)] - **lib**: limit split function calls to prevent excessive array length (Gürgün Dayıoğlu) [#57501](https://github.com/nodejs/node/pull/57501)
* \[[`0546612d1d`](https://github.com/nodejs/node/commit/0546612d1d)] - **lib**: make getCallSites sourceMap option truly optional (James M Snell) [#57388](https://github.com/nodejs/node/pull/57388)
* \[[`d7d54e6bf3`](https://github.com/nodejs/node/commit/d7d54e6bf3)] - **lib**: optimize priority queue (Gürgün Dayıoğlu) [#57100](https://github.com/nodejs/node/pull/57100)
* \[[`62761c73a1`](https://github.com/nodejs/node/commit/62761c73a1)] - **lib**: fixup incorrect argument order in assertEncoding (James M Snell) [#57177](https://github.com/nodejs/node/pull/57177)
* \[[`5dce55c376`](https://github.com/nodejs/node/commit/5dce55c376)] - **meta**: add some clarification to the nomination process (James M Snell) [#57503](https://github.com/nodejs/node/pull/57503)
* \[[`a2a4cf1d95`](https://github.com/nodejs/node/commit/a2a4cf1d95)] - **meta**: remove collaborator self-nomination (Rich Trott) [#57537](https://github.com/nodejs/node/pull/57537)
* \[[`244f74b844`](https://github.com/nodejs/node/commit/244f74b844)] - **meta**: edit collaborator nomination process (Antoine du Hamel) [#57483](https://github.com/nodejs/node/pull/57483)
* \[[`dec204bb3f`](https://github.com/nodejs/node/commit/dec204bb3f)] - **meta**: move ovflowd to emeritus (Claudio W.) [#57443](https://github.com/nodejs/node/pull/57443)
* \[[`c0b8b84384`](https://github.com/nodejs/node/commit/c0b8b84384)] - **meta**: bump codecov/codecov-action from 5.3.1 to 5.4.0 (dependabot\[bot]) [#57257](https://github.com/nodejs/node/pull/57257)
* \[[`14cbe292da`](https://github.com/nodejs/node/commit/14cbe292da)] - **meta**: bump github/codeql-action from 3.28.8 to 3.28.10 (dependabot\[bot]) [#57254](https://github.com/nodejs/node/pull/57254)
* \[[`69d2dd69e2`](https://github.com/nodejs/node/commit/69d2dd69e2)] - **meta**: bump ossf/scorecard-action from 2.4.0 to 2.4.1 (dependabot\[bot]) [#57253](https://github.com/nodejs/node/pull/57253)
* \[[`5f3428ded6`](https://github.com/nodejs/node/commit/5f3428ded6)] - **meta**: move RaisinTen back to collaborators, triagers and SEA champion (Darshan Sen) [#57292](https://github.com/nodejs/node/pull/57292)
* \[[`3eea8c72fc`](https://github.com/nodejs/node/commit/3eea8c72fc)] - **meta**: bump actions/download-artifact from 4.1.8 to 4.1.9 (dependabot\[bot]) [#57260](https://github.com/nodejs/node/pull/57260)
* \[[`2508893edb`](https://github.com/nodejs/node/commit/2508893edb)] - **meta**: bump peter-evans/create-pull-request from 7.0.6 to 7.0.7 (dependabot\[bot]) [#57259](https://github.com/nodejs/node/pull/57259)
* \[[`fc09523f44`](https://github.com/nodejs/node/commit/fc09523f44)] - **meta**: bump step-security/harden-runner from 2.10.4 to 2.11.0 (dependabot\[bot]) [#57258](https://github.com/nodejs/node/pull/57258)
* \[[`b162402440`](https://github.com/nodejs/node/commit/b162402440)] - **meta**: bump actions/cache from 4.2.0 to 4.2.2 (dependabot\[bot]) [#57256](https://github.com/nodejs/node/pull/57256)
* \[[`f781be1332`](https://github.com/nodejs/node/commit/f781be1332)] - **meta**: bump actions/upload-artifact from 4.6.0 to 4.6.1 (dependabot\[bot]) [#57255](https://github.com/nodejs/node/pull/57255)
* \[[`7934ad9fc0`](https://github.com/nodejs/node/commit/7934ad9fc0)] - **meta**: bump `actions/setup-python` from 5.3.0 to 5.4.0 (dependabot\[bot]) [#56867](https://github.com/nodejs/node/pull/56867)
* \[[`eb4fb9ce90`](https://github.com/nodejs/node/commit/eb4fb9ce90)] - **meta**: bump `peter-evans/create-pull-request` from 7.0.5 to 7.0.6 (dependabot\[bot]) [#56866](https://github.com/nodejs/node/pull/56866)
* \[[`a14e7f1cc4`](https://github.com/nodejs/node/commit/a14e7f1cc4)] - **meta**: bump `mozilla-actions/sccache-action` from 0.0.6 to 0.0.7 (dependabot\[bot]) [#56865](https://github.com/nodejs/node/pull/56865)
* \[[`6c8a9e3d0d`](https://github.com/nodejs/node/commit/6c8a9e3d0d)] - **meta**: bump `codecov/codecov-action` from 5.0.7 to 5.3.1 (dependabot\[bot]) [#56864](https://github.com/nodejs/node/pull/56864)
* \[[`f438c27cbf`](https://github.com/nodejs/node/commit/f438c27cbf)] - **meta**: bump `step-security/harden-runner` from 2.10.2 to 2.10.4 (dependabot\[bot]) [#56863](https://github.com/nodejs/node/pull/56863)
* \[[`24b7fcb153`](https://github.com/nodejs/node/commit/24b7fcb153)] - **meta**: bump `actions/cache` from 4.1.2 to 4.2.0 (dependabot\[bot]) [#56862](https://github.com/nodejs/node/pull/56862)
* \[[`a0afc47988`](https://github.com/nodejs/node/commit/a0afc47988)] - **meta**: bump `actions/stale` from 9.0.0 to 9.1.0 (dependabot\[bot]) [#56860](https://github.com/nodejs/node/pull/56860)
* \[[`8abf4e5d7d`](https://github.com/nodejs/node/commit/8abf4e5d7d)] - **meta**: bump `github/codeql-action` from 3.27.5 to 3.28.8 (dependabot\[bot]) [#56859](https://github.com/nodejs/node/pull/56859)
* \[[`c5bff736e9`](https://github.com/nodejs/node/commit/c5bff736e9)] - **meta**: add CODEOWNERS for SQLite (Colin Ihrig) [#57147](https://github.com/nodejs/node/pull/57147)
* \[[`fd2abaa088`](https://github.com/nodejs/node/commit/fd2abaa088)] - **meta**: update last name for jkrems (Jan Martin) [#57006](https://github.com/nodejs/node/pull/57006)
* \[[`2383f00aae`](https://github.com/nodejs/node/commit/2383f00aae)] - **meta**: bump `actions/upload-artifact` from 4.4.3 to 4.6.0 (dependabot\[bot]) [#56861](https://github.com/nodejs/node/pull/56861)
* \[[`35b3140d03`](https://github.com/nodejs/node/commit/35b3140d03)] - **meta**: bump `actions/setup-node` from 4.1.0 to 4.2.0 (dependabot\[bot]) [#56868](https://github.com/nodejs/node/pull/56868)
* \[[`815fcef73d`](https://github.com/nodejs/node/commit/815fcef73d)] - **meta**: move one or more collaborators to emeritus (Node.js GitHub Bot) [#56889](https://github.com/nodejs/node/pull/56889)
* \[[`08001127a2`](https://github.com/nodejs/node/commit/08001127a2)] - **meta**: add @nodejs/url as codeowner (Chengzhong Wu) [#56783](https://github.com/nodejs/node/pull/56783)
* \[[`3ceda2a035`](https://github.com/nodejs/node/commit/3ceda2a035)] - **module**: handle cached linked async jobs in require(esm) (Joyee Cheung) [#57187](https://github.com/nodejs/node/pull/57187)
* \[[`4c29cc7e6b`](https://github.com/nodejs/node/commit/4c29cc7e6b)] - **module**: add dynamic file-specific ESM warnings (Mert Can Altin) [#56628](https://github.com/nodejs/node/pull/56628)
* \[[`d1845edd21`](https://github.com/nodejs/node/commit/d1845edd21)] - **module**: improve error message from asynchronicity in require(esm) (Joyee Cheung) [#57126](https://github.com/nodejs/node/pull/57126)
* \[[`41fa7d3c21`](https://github.com/nodejs/node/commit/41fa7d3c21)] - **module**: allow omitting context in synchronous next hooks (Joyee Cheung) [#57056](https://github.com/nodejs/node/pull/57056)
* \[[`deddecce3a`](https://github.com/nodejs/node/commit/deddecce3a)] - **module**: fix require.resolve() crash on non-string paths (Aditi) [#56942](https://github.com/nodejs/node/pull/56942)
* \[[`926b887534`](https://github.com/nodejs/node/commit/926b887534)] - **module**: fixing url change in load sync hook chain (Vitalii Akimov) [#56402](https://github.com/nodejs/node/pull/56402)
* \[[`6b93ba723b`](https://github.com/nodejs/node/commit/6b93ba723b)] - **(SEMVER-MINOR)** **module**: use synchronous hooks for preparsing in import(cjs) (Joyee Cheung) [#55698](https://github.com/nodejs/node/pull/55698)
* \[[`b2e44a8079`](https://github.com/nodejs/node/commit/b2e44a8079)] - **(SEMVER-MINOR)** **module**: implement module.registerHooks() (Joyee Cheung) [#55698](https://github.com/nodejs/node/pull/55698)
* \[[`e79e67f6dc`](https://github.com/nodejs/node/commit/e79e67f6dc)] - **net**: validate non-string host for socket.connect (Daeyeon Jeong) [#57198](https://github.com/nodejs/node/pull/57198)
* \[[`e23056212e`](https://github.com/nodejs/node/commit/e23056212e)] - **net**: replace brand checks with identity checks (Yagiz Nizipli) [#57341](https://github.com/nodejs/node/pull/57341)
* \[[`9c0d5e140b`](https://github.com/nodejs/node/commit/9c0d5e140b)] - **net**: emit an error when custom lookup resolves to a non-string address (Edy Silva) [#57192](https://github.com/nodejs/node/pull/57192)
* \[[`2ce79787de`](https://github.com/nodejs/node/commit/2ce79787de)] - **(SEMVER-MINOR)** **process**: add execve (Paolo Insogna) [#56496](https://github.com/nodejs/node/pull/56496)
* \[[`712db2232c`](https://github.com/nodejs/node/commit/712db2232c)] - **readline**: add support for `Symbol.dispose` (Antoine du Hamel) [#57276](https://github.com/nodejs/node/pull/57276)
* \[[`55fb81c0f1`](https://github.com/nodejs/node/commit/55fb81c0f1)] - **readline**: fix unresolved promise on abortion (Daniel Venable) [#54030](https://github.com/nodejs/node/pull/54030)
* \[[`dfcd9b1ac2`](https://github.com/nodejs/node/commit/dfcd9b1ac2)] - **sea**: suppress builtin warning with disableExperimentalSEAWarning option (koooge) [#57086](https://github.com/nodejs/node/pull/57086)
* \[[`bd5c90654a`](https://github.com/nodejs/node/commit/bd5c90654a)] - **sqlite**: add support for unknown named parameters (Colin Ihrig) [#57552](https://github.com/nodejs/node/pull/57552)
* \[[`ec571382a4`](https://github.com/nodejs/node/commit/ec571382a4)] - **sqlite**: add DatabaseSync.prototype.isOpen (Colin Ihrig) [#57522](https://github.com/nodejs/node/pull/57522)
* \[[`bb3bbed126`](https://github.com/nodejs/node/commit/bb3bbed126)] - **sqlite**: add DatabaseSync.prototype\[Symbol.dispose]\() (Colin Ihrig) [#57506](https://github.com/nodejs/node/pull/57506)
* \[[`6067bea027`](https://github.com/nodejs/node/commit/6067bea027)] - **sqlite**: restore changes from #55373 (Colin Ihrig) [#56908](https://github.com/nodejs/node/pull/56908)
* \[[`bc672fcfdd`](https://github.com/nodejs/node/commit/bc672fcfdd)] - **(SEMVER-MINOR)** **sqlite**: allow returning `ArrayBufferView`s from user-defined functions (René) [#56790](https://github.com/nodejs/node/pull/56790)
* \[[`227603dc30`](https://github.com/nodejs/node/commit/227603dc30)] - **sqlite,test,doc**: allow Buffer and URL as database location (Edy Silva) [#56991](https://github.com/nodejs/node/pull/56991)
* \[[`9dd324467a`](https://github.com/nodejs/node/commit/9dd324467a)] - **src**: cleanup aliased\_buffer.h (Mohammed Keyvanzadeh) [#57395](https://github.com/nodejs/node/pull/57395)
* \[[`45a2b8532b`](https://github.com/nodejs/node/commit/45a2b8532b)] - **src**: do not pass nullptr to std::string ctor (Charles Kerr) [#57354](https://github.com/nodejs/node/pull/57354)
* \[[`854370a06c`](https://github.com/nodejs/node/commit/854370a06c)] - **src**: fix process exit listeners not receiving unsettled tla codes (Dario Piotrowicz) [#56872](https://github.com/nodejs/node/pull/56872)
* \[[`f7fb259193`](https://github.com/nodejs/node/commit/f7fb259193)] - **src**: refactor SubtleCrypto algorithm and length validations (Filip Skokan) [#57319](https://github.com/nodejs/node/pull/57319)
* \[[`c7bcc2d6c8`](https://github.com/nodejs/node/commit/c7bcc2d6c8)] - **src**: allow embedder customization of OOMErrorHandler (Shelley Vohr) [#57325](https://github.com/nodejs/node/pull/57325)
* \[[`fbd8862156`](https://github.com/nodejs/node/commit/fbd8862156)] - **src**: use Maybe\<void> in ProcessEmitWarningSync (Daeyeon Jeong) [#57250](https://github.com/nodejs/node/pull/57250)
* \[[`04de550289`](https://github.com/nodejs/node/commit/04de550289)] - **src**: make even more improvements to error handling (James M Snell) [#57264](https://github.com/nodejs/node/pull/57264)
* \[[`f1c5e46f89`](https://github.com/nodejs/node/commit/f1c5e46f89)] - **src**: use cached `emit` v8::String (Daeyeon Jeong) [#57249](https://github.com/nodejs/node/pull/57249)
* \[[`65b8e12689`](https://github.com/nodejs/node/commit/65b8e12689)] - **src**: refactor SubtleCrypto algorithm and length validations (Filip Skokan) [#57273](https://github.com/nodejs/node/pull/57273)
* \[[`b6091a8b21`](https://github.com/nodejs/node/commit/b6091a8b21)] - **src**: make more error handling improvements (James M Snell) [#57262](https://github.com/nodejs/node/pull/57262)
* \[[`3bd8a6c76e`](https://github.com/nodejs/node/commit/3bd8a6c76e)] - **src**: fix typo in comment (Antoine du Hamel) [#57291](https://github.com/nodejs/node/pull/57291)
* \[[`f7e39385ae`](https://github.com/nodejs/node/commit/f7e39385ae)] - **src**: improve error handling in `node_messaging.cc` (James M Snell) [#57211](https://github.com/nodejs/node/pull/57211)
* \[[`1bb561bede`](https://github.com/nodejs/node/commit/1bb561bede)] - **src**: improve error handling in `tty_wrap.cc` (James M Snell) [#57211](https://github.com/nodejs/node/pull/57211)
* \[[`567d321a40`](https://github.com/nodejs/node/commit/567d321a40)] - **src**: improve error handling in `tcp_wrap.cc` (James M Snell) [#57211](https://github.com/nodejs/node/pull/57211)
* \[[`f8bee871f7`](https://github.com/nodejs/node/commit/f8bee871f7)] - **src**: fix ThrowInvalidURL call in PathToFileURL (Daniel M Brasil) [#57141](https://github.com/nodejs/node/pull/57141)
* \[[`817f7d0e2e`](https://github.com/nodejs/node/commit/817f7d0e2e)] - **src**: improve error handling in buffer and dotenv (James M Snell) [#57189](https://github.com/nodejs/node/pull/57189)
* \[[`11ef7f9d9c`](https://github.com/nodejs/node/commit/11ef7f9d9c)] - **src**: improve error handling in module\_wrap (James M Snell) [#57188](https://github.com/nodejs/node/pull/57188)
* \[[`3b08d718b1`](https://github.com/nodejs/node/commit/3b08d718b1)] - **src**: improve error handling in spawn\_sync (James M Snell) [#57185](https://github.com/nodejs/node/pull/57185)
* \[[`9221c2ad87`](https://github.com/nodejs/node/commit/9221c2ad87)] - **src**: detect whether the string is one byte representation or not (theweipeng) [#56147](https://github.com/nodejs/node/pull/56147)
* \[[`e323694772`](https://github.com/nodejs/node/commit/e323694772)] - **src**: fix crash when lazy getter is invoked in a vm context (Chengzhong Wu) [#57168](https://github.com/nodejs/node/pull/57168)
* \[[`9363b05a91`](https://github.com/nodejs/node/commit/9363b05a91)] - **src**: do not format single string argument for THROW\_ERR\_\* (Joyee Cheung) [#57126](https://github.com/nodejs/node/pull/57126)
* \[[`5d6a1bc35b`](https://github.com/nodejs/node/commit/5d6a1bc35b)] - **src**: move instead of copy shared pointer in node\_blob (Michaël Zasso) [#57120](https://github.com/nodejs/node/pull/57120)
* \[[`5dab48fd9f`](https://github.com/nodejs/node/commit/5dab48fd9f)] - **src**: replace NewFromUtf8 with OneByteString where appropriate (James M Snell) [#57096](https://github.com/nodejs/node/pull/57096)
* \[[`0fe60b478d`](https://github.com/nodejs/node/commit/0fe60b478d)] - **src**: port `defineLazyProperties` to native code (Antoine du Hamel) [#57081](https://github.com/nodejs/node/pull/57081)
* \[[`792959db1d`](https://github.com/nodejs/node/commit/792959db1d)] - **src**: improve error handling in node\_blob (James M Snell) [#57078](https://github.com/nodejs/node/pull/57078)
* \[[`e05e2cfb1e`](https://github.com/nodejs/node/commit/e05e2cfb1e)] - **src**: fix accessing empty string (Cheng) [#57014](https://github.com/nodejs/node/pull/57014)
* \[[`619e52ce8d`](https://github.com/nodejs/node/commit/619e52ce8d)] - **src**: lock the isolate properly in IsolateData destructor (Joyee Cheung) [#57031](https://github.com/nodejs/node/pull/57031)
* \[[`844a4a884d`](https://github.com/nodejs/node/commit/844a4a884d)] - **src**: add self-assigment memcpy checks (Burkov Egor) [#56986](https://github.com/nodejs/node/pull/56986)
* \[[`0d1e79740f`](https://github.com/nodejs/node/commit/0d1e79740f)] - **src**: improve node::Dotenv trimming (Dario Piotrowicz) [#56983](https://github.com/nodejs/node/pull/56983)
* \[[`50f164e23b`](https://github.com/nodejs/node/commit/50f164e23b)] - **src**: improve error handling in string\_bytes/decoder (James M Snell) [#56978](https://github.com/nodejs/node/pull/56978)
* \[[`93aa4393a4`](https://github.com/nodejs/node/commit/93aa4393a4)] - **src**: improve error handling in process\_wrap (James M Snell) [#56977](https://github.com/nodejs/node/pull/56977)
* \[[`c1c824e38d`](https://github.com/nodejs/node/commit/c1c824e38d)] - **src**: use `args.This()` in zlib (Michaël Zasso) [#56988](https://github.com/nodejs/node/pull/56988)
* \[[`0a8e474bdc`](https://github.com/nodejs/node/commit/0a8e474bdc)] - **src**: add nullptr handling for `NativeKeyObject` (Burkov Egor) [#56900](https://github.com/nodejs/node/pull/56900)
* \[[`1ea6198a5a`](https://github.com/nodejs/node/commit/1ea6198a5a)] - **src**: disallow copy/move fns/constructors (Yagiz Nizipli) [#56811](https://github.com/nodejs/node/pull/56811)
* \[[`e4100853cb`](https://github.com/nodejs/node/commit/e4100853cb)] - **src**: add a hard dependency v8\_inspector\_headers (Chengzhong Wu) [#56805](https://github.com/nodejs/node/pull/56805)
* \[[`a1f92898c0`](https://github.com/nodejs/node/commit/a1f92898c0)] - **src**: improve error handling in encoding\_binding.cc (James M Snell) [#56915](https://github.com/nodejs/node/pull/56915)
* \[[`dee8793d94`](https://github.com/nodejs/node/commit/dee8793d94)] - **src**: improve error handling in permission.cc (James M Snell) [#56904](https://github.com/nodejs/node/pull/56904)
* \[[`f41bc4cfd7`](https://github.com/nodejs/node/commit/f41bc4cfd7)] - **src**: improve error handling in node\_sqlite (James M Snell) [#56891](https://github.com/nodejs/node/pull/56891)
* \[[`e4df6181bf`](https://github.com/nodejs/node/commit/e4df6181bf)] - **src**: improve error handling in node\_os by removing ToLocalChecked (James M Snell) [#56888](https://github.com/nodejs/node/pull/56888)
* \[[`2c96e7a32c`](https://github.com/nodejs/node/commit/2c96e7a32c)] - **src**: improve error handling in node\_url (James M Snell) [#56886](https://github.com/nodejs/node/pull/56886)
* \[[`36926ae8d8`](https://github.com/nodejs/node/commit/36926ae8d8)] - **src**: add check for Bignum in GroupOrderSize (Burkov Egor) [#56702](https://github.com/nodejs/node/pull/56702)
* \[[`a68f127a30`](https://github.com/nodejs/node/commit/a68f127a30)] - **src**: reduce string allocations on sqlite (Yagiz Nizipli) [#57227](https://github.com/nodejs/node/pull/57227)
* \[[`e41b1735f1`](https://github.com/nodejs/node/commit/e41b1735f1)] - **stream**: fix sizeAlgorithm validation in WritableStream (Daeyeon Jeong) [#57280](https://github.com/nodejs/node/pull/57280)
* \[[`3bc877dc5c`](https://github.com/nodejs/node/commit/3bc877dc5c)] - **test**: add more number cases for buffer.indexOf (Meghan Denny) [#57200](https://github.com/nodejs/node/pull/57200)
* \[[`cac9a4e832`](https://github.com/nodejs/node/commit/cac9a4e832)] - **test**: update parallel/test-tls-dhe for OpenSSL 3.5 (Richard Lau) [#57477](https://github.com/nodejs/node/pull/57477)
* \[[`3082ab3a64`](https://github.com/nodejs/node/commit/3082ab3a64)] - **test**: module syntax should throw (Marco Ippolito) [#57121](https://github.com/nodejs/node/pull/57121)
* \[[`9b0dfc9a44`](https://github.com/nodejs/node/commit/9b0dfc9a44)] - **test**: update snapshots for amaro v0.3.2 (Marco Ippolito) [#56916](https://github.com/nodejs/node/pull/56916)
* \[[`2defc35ea8`](https://github.com/nodejs/node/commit/2defc35ea8)] - **test**: test runner run plan (Pietro Marchini) [#57304](https://github.com/nodejs/node/pull/57304)
* \[[`ccb3df70be`](https://github.com/nodejs/node/commit/ccb3df70be)] - **test**: update WPT for WebCryptoAPI to edd42c005c (Node.js GitHub Bot) [#57365](https://github.com/nodejs/node/pull/57365)
* \[[`528103c5d0`](https://github.com/nodejs/node/commit/528103c5d0)] - **test**: simplify test-tls-connect-abort-controller.js (Yagiz Nizipli) [#57338](https://github.com/nodejs/node/pull/57338)
* \[[`17e21e6eb5`](https://github.com/nodejs/node/commit/17e21e6eb5)] - **test**: use `assert.match` in `test-esm-import-meta` (Antoine du Hamel) [#57290](https://github.com/nodejs/node/pull/57290)
* \[[`77bbee5184`](https://github.com/nodejs/node/commit/77bbee5184)] - **test**: update compression wpt (Yagiz Nizipli) [#56960](https://github.com/nodejs/node/pull/56960)
* \[[`4fe88f8f53`](https://github.com/nodejs/node/commit/4fe88f8f53)] - _**Revert**_ "**test**: temporary remove resource check from fs read-write" (Rafael Gonzaga) [#56906](https://github.com/nodejs/node/pull/56906)
* \[[`766efc7758`](https://github.com/nodejs/node/commit/766efc7758)] - **test**: more common.mustNotCall in net, tls (Meghan Denny) [#57246](https://github.com/nodejs/node/pull/57246)
* \[[`562e635e11`](https://github.com/nodejs/node/commit/562e635e11)] - **test**: swap assert.strictEqual() parameters (Luigi Pinca) [#57217](https://github.com/nodejs/node/pull/57217)
* \[[`64fdfd5622`](https://github.com/nodejs/node/commit/64fdfd5622)] - **test**: assert write return values in buffer-bigint64 (Meghan Denny) [#57212](https://github.com/nodejs/node/pull/57212)
* \[[`dd538e7cf1`](https://github.com/nodejs/node/commit/dd538e7cf1)] - **test**: allow embedder running async context frame test (Shelley Vohr) [#57193](https://github.com/nodejs/node/pull/57193)
* \[[`937bbeb2b6`](https://github.com/nodejs/node/commit/937bbeb2b6)] - **test**: resolve race condition in test-net-write-fully-async-\* (Matteo Collina) [#57022](https://github.com/nodejs/node/pull/57022)
* \[[`32df9f27d8`](https://github.com/nodejs/node/commit/32df9f27d8)] - **test**: add doAppendAndCancel test (Hasegawa-Yukihiro) [#56972](https://github.com/nodejs/node/pull/56972)
* \[[`90c98df258`](https://github.com/nodejs/node/commit/90c98df258)] - **test**: fix test-without-async-context-frame.mjs in debug mode (Joyee Cheung) [#57034](https://github.com/nodejs/node/pull/57034)
* \[[`974817c9fc`](https://github.com/nodejs/node/commit/974817c9fc)] - **test**: make eval snapshot comparison more flexible (Shelley Vohr) [#57020](https://github.com/nodejs/node/pull/57020)
* \[[`09741cd129`](https://github.com/nodejs/node/commit/09741cd129)] - **test**: simplify test-http2-client-promisify-connect-error (Luigi Pinca) [#57144](https://github.com/nodejs/node/pull/57144)
* \[[`89f3feb364`](https://github.com/nodejs/node/commit/89f3feb364)] - **test**: improve error output of test-http2-client-promisify-connect-error (Antoine du Hamel) [#57135](https://github.com/nodejs/node/pull/57135)
* \[[`25751eba4d`](https://github.com/nodejs/node/commit/25751eba4d)] - **test**: add case for unrecognised fields within pjson "exports" (Jacob Smith) [#57026](https://github.com/nodejs/node/pull/57026)
* \[[`bf0b9fa7c0`](https://github.com/nodejs/node/commit/bf0b9fa7c0)] - **test**: remove unnecessary assert requiring from tests (Dario Piotrowicz) [#57008](https://github.com/nodejs/node/pull/57008)
* \[[`8cfb2df466`](https://github.com/nodejs/node/commit/8cfb2df466)] - **test**: reduce flakiness on test-net-write-fully-async-buffer (Yagiz Nizipli) [#56971](https://github.com/nodejs/node/pull/56971)
* \[[`43c8c101da`](https://github.com/nodejs/node/commit/43c8c101da)] - **test**: remove flakiness on macOS test (Yagiz Nizipli) [#56971](https://github.com/nodejs/node/pull/56971)
* \[[`bd47178f7f`](https://github.com/nodejs/node/commit/bd47178f7f)] - **test**: improve timeout duration for debugger events (Yagiz Nizipli) [#56970](https://github.com/nodejs/node/pull/56970)
* \[[`65694aa2fd`](https://github.com/nodejs/node/commit/65694aa2fd)] - **test**: remove unnecessary syscall to cpuinfo (Yagiz Nizipli) [#56968](https://github.com/nodejs/node/pull/56968)
* \[[`5633c4b2df`](https://github.com/nodejs/node/commit/5633c4b2df)] - **test**: update webstorage wpt (Yagiz Nizipli) [#56963](https://github.com/nodejs/node/pull/56963)
* \[[`2244a2776a`](https://github.com/nodejs/node/commit/2244a2776a)] - **test**: execute shell directly for refresh() (Yagiz Nizipli) [#55051](https://github.com/nodejs/node/pull/55051)
* \[[`afae4b1216`](https://github.com/nodejs/node/commit/afae4b1216)] - **test**: change jenkins reporter (Carlos Espa) [#56808](https://github.com/nodejs/node/pull/56808)
* \[[`b26592a7c4`](https://github.com/nodejs/node/commit/b26592a7c4)] - **test**: fix race condition in test-child-process-bad-stdio (Colin Ihrig) [#56845](https://github.com/nodejs/node/pull/56845)
* \[[`72c2279649`](https://github.com/nodejs/node/commit/72c2279649)] - **test**: adjust check to use OpenSSL sec level (Michael Dawson) [#56819](https://github.com/nodejs/node/pull/56819)
* \[[`9551b27651`](https://github.com/nodejs/node/commit/9551b27651)] - **test**: test-crypto-scrypt.js doesn't need internals (Meghan Denny) [#56673](https://github.com/nodejs/node/pull/56673)
* \[[`3095db84be`](https://github.com/nodejs/node/commit/3095db84be)] - **test**: set `test-fs-cp` as flaky (Stefan Stojanovic) [#56799](https://github.com/nodejs/node/pull/56799)
* \[[`31f98d7ccd`](https://github.com/nodejs/node/commit/31f98d7ccd)] - **test**: search cctest files (Chengzhong Wu) [#56791](https://github.com/nodejs/node/pull/56791)
* \[[`267f17d5f6`](https://github.com/nodejs/node/commit/267f17d5f6)] - **test**: convert test\_encoding\_binding.cc to a JS test (Chengzhong Wu) [#56791](https://github.com/nodejs/node/pull/56791)
* \[[`a875d7bdd1`](https://github.com/nodejs/node/commit/a875d7bdd1)] - **test**: test-crypto-prime.js doesn't need internals (Meghan Denny) [#56675](https://github.com/nodejs/node/pull/56675)
* \[[`85482d69c6`](https://github.com/nodejs/node/commit/85482d69c6)] - **test**: temporary remove resource check from fs read-write (Rafael Gonzaga) [#56789](https://github.com/nodejs/node/pull/56789)
* \[[`ec63d72f16`](https://github.com/nodejs/node/commit/ec63d72f16)] - **test**: mark test-without-async-context-frame flaky on windows (James M Snell) [#56753](https://github.com/nodejs/node/pull/56753)
* \[[`f16acc8521`](https://github.com/nodejs/node/commit/f16acc8521)] - **test**: remove unnecessary code (Luigi Pinca) [#56784](https://github.com/nodejs/node/pull/56784)
* \[[`0573c19a97`](https://github.com/nodejs/node/commit/0573c19a97)] - **test**: mark `test-esm-loader-hooks-inspect-wait` flaky (Richard Lau) [#56803](https://github.com/nodejs/node/pull/56803)
* \[[`48e0fd3f13`](https://github.com/nodejs/node/commit/48e0fd3f13)] - **test**: update WPT for url to a23788b77a (Node.js GitHub Bot) [#56779](https://github.com/nodejs/node/pull/56779)
* \[[`642959b87f`](https://github.com/nodejs/node/commit/642959b87f)] - **test**: remove duplicate error reporter from ci (Carlos Espa) [#56739](https://github.com/nodejs/node/pull/56739)
* \[[`2023237b4e`](https://github.com/nodejs/node/commit/2023237b4e)] - **test,crypto**: make tests work for BoringSSL (Shelley Vohr) [#57021](https://github.com/nodejs/node/pull/57021)
* \[[`1b33b976ec`](https://github.com/nodejs/node/commit/1b33b976ec)] - **test\_runner**: refactor testPlan counter increse (Pietro Marchini) [#56765](https://github.com/nodejs/node/pull/56765)
* \[[`d860f2bf42`](https://github.com/nodejs/node/commit/d860f2bf42)] - **test\_runner**: differentiate test types in enqueue dequeue events (Eddie Abbondanzio) [#54049](https://github.com/nodejs/node/pull/54049)
* \[[`993bab646c`](https://github.com/nodejs/node/commit/993bab646c)] - **test\_runner**: print formatted errors on summary (Pietro Marchini) [#56911](https://github.com/nodejs/node/pull/56911)
* \[[`3ed3ba438f`](https://github.com/nodejs/node/commit/3ed3ba438f)] - **test\_runner**: allow special characters in snapshot keys (Carlos Espa) [#57017](https://github.com/nodejs/node/pull/57017)
* \[[`d1da9a3a2f`](https://github.com/nodejs/node/commit/d1da9a3a2f)] - **timers**: optimize timer functions with improved argument handling (Gürgün Dayıoğlu) [#57072](https://github.com/nodejs/node/pull/57072)
* \[[`44aa13990a`](https://github.com/nodejs/node/commit/44aa13990a)] - **timers**: remove unnecessary allocation of \_onTimeout (Gürgün Dayıoğlu) [#57497](https://github.com/nodejs/node/pull/57497)
* \[[`401b965977`](https://github.com/nodejs/node/commit/401b965977)] - **timers**: remove unused parameter from insertGuarded (Gürgün Dayıoğlu) [#57251](https://github.com/nodejs/node/pull/57251)
* \[[`9eac9c02c9`](https://github.com/nodejs/node/commit/9eac9c02c9)] - **timers**: simplify the compareTimersLists function (Gürgün Dayıoğlu) [#57110](https://github.com/nodejs/node/pull/57110)
* \[[`01215af350`](https://github.com/nodejs/node/commit/01215af350)] - **tls**: remove unnecessary type check on normalize (Yagiz Nizipli) [#57336](https://github.com/nodejs/node/pull/57336)
* \[[`f5e2b12a60`](https://github.com/nodejs/node/commit/f5e2b12a60)] - **(SEMVER-MINOR)** **tls**: implement tls.getCACertificates() (Joyee Cheung) [#57107](https://github.com/nodejs/node/pull/57107)
* \[[`7a777cdb58`](https://github.com/nodejs/node/commit/7a777cdb58)] - **tools**: fix WPT update cron string (Antoine du Hamel) [#57665](https://github.com/nodejs/node/pull/57665)
* \[[`c6d90dbf9b`](https://github.com/nodejs/node/commit/c6d90dbf9b)] - **tools**: remove stalled label on unstalled issues and PRs (Rich Trott) [#57630](https://github.com/nodejs/node/pull/57630)
* \[[`96f7f64602`](https://github.com/nodejs/node/commit/96f7f64602)] - **tools**: update sccache to support GH cache changes (Michaël Zasso) [#57573](https://github.com/nodejs/node/pull/57573)
* \[[`0b87027520`](https://github.com/nodejs/node/commit/0b87027520)] - **tools**: bump @babel/helpers from 7.26.9 to 7.26.10 in /tools/eslint (dependabot\[bot]) [#57444](https://github.com/nodejs/node/pull/57444)
* \[[`7d561eb90c`](https://github.com/nodejs/node/commit/7d561eb90c)] - **tools**: add config subspace (Marco Ippolito) [#57239](https://github.com/nodejs/node/pull/57239)
* \[[`46efdbf59f`](https://github.com/nodejs/node/commit/46efdbf59f)] - **tools**: import rather than require ESLint plugins (Michaël Zasso) [#57315](https://github.com/nodejs/node/pull/57315)
* \[[`502bfaf876`](https://github.com/nodejs/node/commit/502bfaf876)] - **tools**: switch back to official OpenSSL (Richard Lau) [#57301](https://github.com/nodejs/node/pull/57301)
* \[[`ea821f419d`](https://github.com/nodejs/node/commit/ea821f419d)] - **tools**: revert to use @stylistic/eslint-plugin-js v3 (Joyee Cheung) [#57314](https://github.com/nodejs/node/pull/57314)
* \[[`bb857615d3`](https://github.com/nodejs/node/commit/bb857615d3)] - **tools**: add more details about rolling inspector\_protocol (Chengzhong Wu) [#57167](https://github.com/nodejs/node/pull/57167)
* \[[`3f29d39c1b`](https://github.com/nodejs/node/commit/3f29d39c1b)] - **tools**: bump the eslint group in /tools/eslint with 5 updates (dependabot\[bot]) [#57261](https://github.com/nodejs/node/pull/57261)
* \[[`b3caac83d4`](https://github.com/nodejs/node/commit/b3caac83d4)] - **tools**: remove deps/zlib/GN-scraper.py (Chengzhong Wu) [#57238](https://github.com/nodejs/node/pull/57238)
* \[[`ace99ffe79`](https://github.com/nodejs/node/commit/ace99ffe79)] - **tools**: run Linux tests on GitHub arm64 runners as well (Dennis Ameling) [#57162](https://github.com/nodejs/node/pull/57162)
* \[[`e65e6269b7`](https://github.com/nodejs/node/commit/e65e6269b7)] - **tools**: consolidate 'introduced\_in' check for docs (1ilsang) [#57109](https://github.com/nodejs/node/pull/57109)
* \[[`890841e64b`](https://github.com/nodejs/node/commit/890841e64b)] - **tools**: do not run major-release workflow on forks (Rich Trott) [#57064](https://github.com/nodejs/node/pull/57064)
* \[[`e3f86c5a0c`](https://github.com/nodejs/node/commit/e3f86c5a0c)] - **tools**: fix release URL computation in update-root-certs.mjs (Joyee Cheung) [#56843](https://github.com/nodejs/node/pull/56843)
* \[[`280316f773`](https://github.com/nodejs/node/commit/280316f773)] - **tools**: add support for `import source` syntax in linter (Antoine du Hamel) [#56992](https://github.com/nodejs/node/pull/56992)
* \[[`998b2ae3cd`](https://github.com/nodejs/node/commit/998b2ae3cd)] - **tools**: bump eslint version (dependabot\[bot]) [#56869](https://github.com/nodejs/node/pull/56869)
* \[[`ca4121b95a`](https://github.com/nodejs/node/commit/ca4121b95a)] - **tools**: remove test-asan/ubsan workflows (Michaël Zasso) [#56823](https://github.com/nodejs/node/pull/56823)
* \[[`866ac37255`](https://github.com/nodejs/node/commit/866ac37255)] - **tools**: run macOS test workflow with Xcode 16.1 (Michaël Zasso) [#56831](https://github.com/nodejs/node/pull/56831)
* \[[`55ca46ad8e`](https://github.com/nodejs/node/commit/55ca46ad8e)] - **tools**: update sccache and sccache-action (Michaël Zasso) [#56815](https://github.com/nodejs/node/pull/56815)
* \[[`be9c1c93a8`](https://github.com/nodejs/node/commit/be9c1c93a8)] - **tools**: fix license-builder for inspector\_protocol (Michaël Zasso) [#56814](https://github.com/nodejs/node/pull/56814)
* \[[`6dab980fab`](https://github.com/nodejs/node/commit/6dab980fab)] - **typings**: fix `ImportModuleDynamicallyCallback` return type (Chengzhong Wu) [#57160](https://github.com/nodejs/node/pull/57160)
* \[[`e301098854`](https://github.com/nodejs/node/commit/e301098854)] - **util**: avoid run debug when enabled is false (fengmk2) [#57494](https://github.com/nodejs/node/pull/57494)
* \[[`17016d7722`](https://github.com/nodejs/node/commit/17016d7722)] - **(SEMVER-MINOR)** **util**: expose diff function used by the assertion errors (Giovanni Bucci) [#57462](https://github.com/nodejs/node/pull/57462)
* \[[`42b9e19f6b`](https://github.com/nodejs/node/commit/42b9e19f6b)] - **util**: enforce shouldColorize in styleText array arg (Marco Ippolito) [#56722](https://github.com/nodejs/node/pull/56722)
* \[[`5ed6d8be40`](https://github.com/nodejs/node/commit/5ed6d8be40)] - **(SEMVER-MINOR)** **v8**: add v8.getCppHeapStatistics() method (Aditi) [#57146](https://github.com/nodejs/node/pull/57146)
* \[[`c06d218b23`](https://github.com/nodejs/node/commit/c06d218b23)] - **win,build**: add option to enable Control Flow Guard (Hüseyin Açacak) [#56605](https://github.com/nodejs/node/pull/56605)
* \[[`8202211140`](https://github.com/nodejs/node/commit/8202211140)] - **win,test**: disable test case failing with ClangCL (Stefan Stojanovic) [#57397](https://github.com/nodejs/node/pull/57397)
* \[[`1a12b4c119`](https://github.com/nodejs/node/commit/1a12b4c119)] - **zlib**: use modern class syntax for zstd classes (Yagiz Nizipli) [#56965](https://github.com/nodejs/node/pull/56965)
* \[[`f9b3680268`](https://github.com/nodejs/node/commit/f9b3680268)] - **zlib**: make all zstd functions experimental (Yagiz Nizipli) [#56964](https://github.com/nodejs/node/pull/56964)
* \[[`4991e5d826`](https://github.com/nodejs/node/commit/4991e5d826)] - **(SEMVER-MINOR)** **zlib**: add zstd support (Jan Martin) [#52100](https://github.com/nodejs/node/pull/52100)

<a id="22.14.0"></a>

## 2025-02-11, Version 22.14.0 'Jod' (LTS), @aduh95

### Notable Changes

* \[[`82a9000e9e`](https://github.com/nodejs/node/commit/82a9000e9e)] - **crypto**: update root certificates to NSS 3.107 (Node.js GitHub Bot) [#56566](https://github.com/nodejs/node/pull/56566)
* \[[`b7fe54fc88`](https://github.com/nodejs/node/commit/b7fe54fc88)] - **(SEMVER-MINOR)** **fs**: allow `exclude` option in globs to accept glob patterns (Daeyeon Jeong) [#56489](https://github.com/nodejs/node/pull/56489)
* \[[`3ac92ef607`](https://github.com/nodejs/node/commit/3ac92ef607)] - **(SEMVER-MINOR)** **lib**: add typescript support to STDIN eval (Marco Ippolito) [#56359](https://github.com/nodejs/node/pull/56359)
* \[[`1614e8e7bc`](https://github.com/nodejs/node/commit/1614e8e7bc)] - **(SEMVER-MINOR)** **module**: add ERR\_UNSUPPORTED\_TYPESCRIPT\_SYNTAX (Marco Ippolito) [#56610](https://github.com/nodejs/node/pull/56610)
* \[[`6d6cffa9cc`](https://github.com/nodejs/node/commit/6d6cffa9cc)] - **(SEMVER-MINOR)** **module**: add `findPackageJSON` util (Jacob Smith) [#55412](https://github.com/nodejs/node/pull/55412)
* \[[`d35333ae18`](https://github.com/nodejs/node/commit/d35333ae18)] - **(SEMVER-MINOR)** **process**: add process.ref() and process.unref() methods (James M Snell) [#56400](https://github.com/nodejs/node/pull/56400)
* \[[`07ff3ddcb5`](https://github.com/nodejs/node/commit/07ff3ddcb5)] - **(SEMVER-MINOR)** **sqlite**: support TypedArray and DataView in `StatementSync` (Alex Yang) [#56385](https://github.com/nodejs/node/pull/56385)
* \[[`94d3fe1b62`](https://github.com/nodejs/node/commit/94d3fe1b62)] - **(SEMVER-MINOR)** **src**: add --disable-sigusr1 to prevent signal i/o thread (Rafael Gonzaga) [#56441](https://github.com/nodejs/node/pull/56441)
* \[[`5afffb4415`](https://github.com/nodejs/node/commit/5afffb4415)] - **(SEMVER-MINOR)** **src,worker**: add isInternalWorker (Carlos Espa) [#56469](https://github.com/nodejs/node/pull/56469)
* \[[`697a851fb3`](https://github.com/nodejs/node/commit/697a851fb3)] - **(SEMVER-MINOR)** **test\_runner**: add TestContext.prototype.waitFor() (Colin Ihrig) [#56595](https://github.com/nodejs/node/pull/56595)
* \[[`047537b48c`](https://github.com/nodejs/node/commit/047537b48c)] - **(SEMVER-MINOR)** **test\_runner**: add t.assert.fileSnapshot() (Colin Ihrig) [#56459](https://github.com/nodejs/node/pull/56459)
* \[[`926cf84e95`](https://github.com/nodejs/node/commit/926cf84e95)] - **(SEMVER-MINOR)** **test\_runner**: add assert.register() API (Colin Ihrig) [#56434](https://github.com/nodejs/node/pull/56434)
* \[[`c658a8afdf`](https://github.com/nodejs/node/commit/c658a8afdf)] - **(SEMVER-MINOR)** **worker**: add eval ts input (Marco Ippolito) [#56394](https://github.com/nodejs/node/pull/56394)

### Commits

* \[[`bad1ad8650`](https://github.com/nodejs/node/commit/bad1ad8650)] - **assert**: make myers\_diff function more performant (Giovanni Bucci) [#56303](https://github.com/nodejs/node/pull/56303)
* \[[`e222e36f3b`](https://github.com/nodejs/node/commit/e222e36f3b)] - **assert**: make partialDeepStrictEqual work with urls and File prototypes (Giovanni Bucci) [#56231](https://github.com/nodejs/node/pull/56231)
* \[[`e232789fe2`](https://github.com/nodejs/node/commit/e232789fe2)] - **assert**: show diff when doing partial comparisons (Giovanni Bucci) [#56211](https://github.com/nodejs/node/pull/56211)
* \[[`c99de1fdcf`](https://github.com/nodejs/node/commit/c99de1fdcf)] - **assert**: make partialDeepStrictEqual throw when comparing \[0] with \[-0] (Giovanni) [#56237](https://github.com/nodejs/node/pull/56237)
* \[[`2386fd5840`](https://github.com/nodejs/node/commit/2386fd5840)] - **benchmark**: add validateStream to styleText bench (Rafael Gonzaga) [#56556](https://github.com/nodejs/node/pull/56556)
* \[[`b197dfa7ec`](https://github.com/nodejs/node/commit/b197dfa7ec)] - **build**: fix GN build for ngtcp2 (Cheng) [#56300](https://github.com/nodejs/node/pull/56300)
* \[[`2a3cdd34ff`](https://github.com/nodejs/node/commit/2a3cdd34ff)] - **build**: test macos-13 on GitHub actions (Michaël Zasso) [#56307](https://github.com/nodejs/node/pull/56307)
* \[[`12f716be0a`](https://github.com/nodejs/node/commit/12f716be0a)] - **build**: build v8 with -fvisibility=hidden on macOS (Joyee Cheung) [#56275](https://github.com/nodejs/node/pull/56275)
* \[[`c5ca15bd34`](https://github.com/nodejs/node/commit/c5ca15bd34)] - **child\_process**: fix parsing messages with splitted length field (Maksim Gorkov) [#56106](https://github.com/nodejs/node/pull/56106)
* \[[`8346b8fc2c`](https://github.com/nodejs/node/commit/8346b8fc2c)] - **crypto**: add missing return value check (Michael Dawson) [#56615](https://github.com/nodejs/node/pull/56615)
* \[[`82a9000e9e`](https://github.com/nodejs/node/commit/82a9000e9e)] - **crypto**: update root certificates to NSS 3.107 (Node.js GitHub Bot) [#56566](https://github.com/nodejs/node/pull/56566)
* \[[`890eef20a1`](https://github.com/nodejs/node/commit/890eef20a1)] - **crypto**: fix checkPrime crash with large buffers (Santiago Gimeno) [#56559](https://github.com/nodejs/node/pull/56559)
* \[[`5edb7b5e87`](https://github.com/nodejs/node/commit/5edb7b5e87)] - **crypto**: fix warning of ignoring return value (Cheng) [#56527](https://github.com/nodejs/node/pull/56527)
* \[[`b89f123a0b`](https://github.com/nodejs/node/commit/b89f123a0b)] - **crypto**: make generatePrime/checkPrime interruptible (James M Snell) [#56460](https://github.com/nodejs/node/pull/56460)
* \[[`63c1859e01`](https://github.com/nodejs/node/commit/63c1859e01)] - **deps**: update corepack to 0.31.0 (Node.js GitHub Bot) [#56795](https://github.com/nodejs/node/pull/56795)
* \[[`a48430d4d3`](https://github.com/nodejs/node/commit/a48430d4d3)] - **deps**: move inspector\_protocol to deps (Chengzhong Wu) [#56649](https://github.com/nodejs/node/pull/56649)
* \[[`74cccc824f`](https://github.com/nodejs/node/commit/74cccc824f)] - **deps**: macro ENODATA is deprecated in libc++ (Cheng) [#56698](https://github.com/nodejs/node/pull/56698)
* \[[`fa869ea0f2`](https://github.com/nodejs/node/commit/fa869ea0f2)] - **deps**: fixup some minor coverity warnings (James M Snell) [#56612](https://github.com/nodejs/node/pull/56612)
* \[[`1a4fa2b015`](https://github.com/nodejs/node/commit/1a4fa2b015)] - **deps**: update amaro to 0.3.0 (Node.js GitHub Bot) [#56568](https://github.com/nodejs/node/pull/56568)
* \[[`b47076fd82`](https://github.com/nodejs/node/commit/b47076fd82)] - **deps**: update amaro to 0.2.2 (Node.js GitHub Bot) [#56568](https://github.com/nodejs/node/pull/56568)
* \[[`46bd4b8731`](https://github.com/nodejs/node/commit/46bd4b8731)] - **deps**: update simdutf to 6.0.3 (Node.js GitHub Bot) [#56567](https://github.com/nodejs/node/pull/56567)
* \[[`8ead9c693b`](https://github.com/nodejs/node/commit/8ead9c693b)] - **deps**: update simdutf to 5.7.2 (Node.js GitHub Bot) [#56388](https://github.com/nodejs/node/pull/56388)
* \[[`18d4b502af`](https://github.com/nodejs/node/commit/18d4b502af)] - **deps**: update amaro to 0.2.1 (Node.js GitHub Bot) [#56390](https://github.com/nodejs/node/pull/56390)
* \[[`d938d7cc86`](https://github.com/nodejs/node/commit/d938d7cc86)] - **deps**: update googletest to 7d76a23 (Node.js GitHub Bot) [#56387](https://github.com/nodejs/node/pull/56387)
* \[[`9761e7dccb`](https://github.com/nodejs/node/commit/9761e7dccb)] - **deps**: update googletest to e54519b (Node.js GitHub Bot) [#56370](https://github.com/nodejs/node/pull/56370)
* \[[`8319dc6bc5`](https://github.com/nodejs/node/commit/8319dc6bc5)] - **deps**: update ngtcp2 to 1.10.0 (Node.js GitHub Bot) [#56334](https://github.com/nodejs/node/pull/56334)
* \[[`6eacd19d6a`](https://github.com/nodejs/node/commit/6eacd19d6a)] - **deps**: update simdutf to 5.7.0 (Node.js GitHub Bot) [#56332](https://github.com/nodejs/node/pull/56332)
* \[[`28bec2dda3`](https://github.com/nodejs/node/commit/28bec2dda3)] - **diagnostics\_channel**: capture console messages (Stephen Belanger) [#56292](https://github.com/nodejs/node/pull/56292)
* \[[`d519d33502`](https://github.com/nodejs/node/commit/d519d33502)] - **doc**: update macOS and Xcode versions for releases (Michaël Zasso) [#56337](https://github.com/nodejs/node/pull/56337)
* \[[`fcfe650507`](https://github.com/nodejs/node/commit/fcfe650507)] - **doc**: add note for features using `InternalWorker` with permission model (Antoine du Hamel) [#56706](https://github.com/nodejs/node/pull/56706)
* \[[`efbba182b5`](https://github.com/nodejs/node/commit/efbba182b5)] - **doc**: add entry to changelog about SQLite Session Extension (Bart Louwers) [#56318](https://github.com/nodejs/node/pull/56318)
* \[[`31bf9c7dd9`](https://github.com/nodejs/node/commit/31bf9c7dd9)] - **doc**: move anatoli to emeritus (Michael Dawson) [#56592](https://github.com/nodejs/node/pull/56592)
* \[[`6096e38c7c`](https://github.com/nodejs/node/commit/6096e38c7c)] - **doc**: fix styles of the expandable TOC (Antoine du Hamel) [#56755](https://github.com/nodejs/node/pull/56755)
* \[[`d423638281`](https://github.com/nodejs/node/commit/d423638281)] - **doc**: add "Skip to content" button (Antoine du Hamel) [#56750](https://github.com/nodejs/node/pull/56750)
* \[[`edeb157d75`](https://github.com/nodejs/node/commit/edeb157d75)] - **doc**: improve accessibility of expandable lists (Antoine du Hamel) [#56749](https://github.com/nodejs/node/pull/56749)
* \[[`1a79e87687`](https://github.com/nodejs/node/commit/1a79e87687)] - **doc**: add note regarding commit message trailers (Dario Piotrowicz) [#56736](https://github.com/nodejs/node/pull/56736)
* \[[`927c7e47e4`](https://github.com/nodejs/node/commit/927c7e47e4)] - **doc**: fix typo in example code for util.styleText (Robin Mehner) [#56720](https://github.com/nodejs/node/pull/56720)
* \[[`fade522538`](https://github.com/nodejs/node/commit/fade522538)] - **doc**: fix inconsistencies in `WeakSet` and `WeakMap` comparison details (Shreyans Pathak) [#56683](https://github.com/nodejs/node/pull/56683)
* \[[`55533bf147`](https://github.com/nodejs/node/commit/55533bf147)] - **doc**: add RafaelGSS as latest sec release stewards (Rafael Gonzaga) [#56682](https://github.com/nodejs/node/pull/56682)
* \[[`8e978bdee1`](https://github.com/nodejs/node/commit/8e978bdee1)] - **doc**: clarify cjs/esm diff in `queueMicrotask()` vs `process.nextTick()` (Dario Piotrowicz) [#56659](https://github.com/nodejs/node/pull/56659)
* \[[`ae360c30dc`](https://github.com/nodejs/node/commit/ae360c30dc)] - **doc**: `WeakSet` and `WeakMap` comparison details (Shreyans Pathak) [#56648](https://github.com/nodejs/node/pull/56648)
* \[[`acd2a2fda5`](https://github.com/nodejs/node/commit/acd2a2fda5)] - **doc**: mention prepare --security (Rafael Gonzaga) [#56617](https://github.com/nodejs/node/pull/56617)
* \[[`d3c0a2831d`](https://github.com/nodejs/node/commit/d3c0a2831d)] - **doc**: tweak info on reposts in ambassador program (Michael Dawson) [#56589](https://github.com/nodejs/node/pull/56589)
* \[[`3299505b49`](https://github.com/nodejs/node/commit/3299505b49)] - **doc**: add type stripping to ambassadors program (Marco Ippolito) [#56598](https://github.com/nodejs/node/pull/56598)
* \[[`b1a6ffa4e4`](https://github.com/nodejs/node/commit/b1a6ffa4e4)] - **doc**: improve internal documentation on built-in snapshot (Joyee Cheung) [#56505](https://github.com/nodejs/node/pull/56505)
* \[[`1641a28930`](https://github.com/nodejs/node/commit/1641a28930)] - **doc**: document CLI way to open the nodejs/bluesky PR (Antoine du Hamel) [#56506](https://github.com/nodejs/node/pull/56506)
* \[[`2042628fda`](https://github.com/nodejs/node/commit/2042628fda)] - **doc**: add section about using npx with permission model (Rafael Gonzaga) [#56539](https://github.com/nodejs/node/pull/56539)
* \[[`ace19a0263`](https://github.com/nodejs/node/commit/ace19a0263)] - **doc**: update gcc-version for ubuntu-lts (Kunal Kumar) [#56553](https://github.com/nodejs/node/pull/56553)
* \[[`4aa57b50f8`](https://github.com/nodejs/node/commit/4aa57b50f8)] - **doc**: fix parentheses in options (Tobias Nießen) [#56563](https://github.com/nodejs/node/pull/56563)
* \[[`b40b01b4d3`](https://github.com/nodejs/node/commit/b40b01b4d3)] - **doc**: include CVE to EOL lines as sec release process (Rafael Gonzaga) [#56520](https://github.com/nodejs/node/pull/56520)
* \[[`6701360113`](https://github.com/nodejs/node/commit/6701360113)] - **doc**: add esm examples to node:trace\_events (Alfredo González) [#56514](https://github.com/nodejs/node/pull/56514)
* \[[`d3207cca3e`](https://github.com/nodejs/node/commit/d3207cca3e)] - **doc**: add message for Ambassadors to promote (Michael Dawson) [#56235](https://github.com/nodejs/node/pull/56235)
* \[[`97ece4ae06`](https://github.com/nodejs/node/commit/97ece4ae06)] - **doc**: allow request for TSC reviews via the GitHub UI (Antoine du Hamel) [#56493](https://github.com/nodejs/node/pull/56493)
* \[[`03f25055ab`](https://github.com/nodejs/node/commit/03f25055ab)] - **doc**: add example for piping ReadableStream (Gabriel Schulhof) [#56415](https://github.com/nodejs/node/pull/56415)
* \[[`516d07482c`](https://github.com/nodejs/node/commit/516d07482c)] - **doc**: expand description of `parseArg`'s `default` (Kevin Gibbons) [#54431](https://github.com/nodejs/node/pull/54431)
* \[[`a6491effcb`](https://github.com/nodejs/node/commit/a6491effcb)] - **doc**: use `<ul>` instead of `<ol>` in `SECURITY.md` (Antoine du Hamel) [#56346](https://github.com/nodejs/node/pull/56346)
* \[[`e4ec134b21`](https://github.com/nodejs/node/commit/e4ec134b21)] - **doc**: clarify that WASM is trusted (Matteo Collina) [#56345](https://github.com/nodejs/node/pull/56345)
* \[[`0f7aed8a59`](https://github.com/nodejs/node/commit/0f7aed8a59)] - **doc**: fix the `crc32` documentation (Kevin Toshihiro Uehara) [#55898](https://github.com/nodejs/node/pull/55898)
* \[[`721104a296`](https://github.com/nodejs/node/commit/721104a296)] - **doc**: fix links in `module.md` (Antoine du Hamel) [#56283](https://github.com/nodejs/node/pull/56283)
* \[[`928540d792`](https://github.com/nodejs/node/commit/928540d792)] - **doc**: fix typos (Nathan Baulch) [#55066](https://github.com/nodejs/node/pull/55066)
* \[[`e69d35f03b`](https://github.com/nodejs/node/commit/e69d35f03b)] - **doc**: add history info for Permission Model (Antoine du Hamel) [#56707](https://github.com/nodejs/node/pull/56707)
* \[[`c6fd867ab5`](https://github.com/nodejs/node/commit/c6fd867ab5)] - **esm**: fix jsdoc type refs to `ModuleJobBase` in esm/loader (Jacob Smith) [#56499](https://github.com/nodejs/node/pull/56499)
* \[[`9cf9046bd7`](https://github.com/nodejs/node/commit/9cf9046bd7)] - _**Revert**_ "**events**: add hasEventListener util for validate" (origranot) [#56282](https://github.com/nodejs/node/pull/56282)
* \[[`b7fe54fc88`](https://github.com/nodejs/node/commit/b7fe54fc88)] - **(SEMVER-MINOR)** **fs**: allow `exclude` option in globs to accept glob patterns (Daeyeon Jeong) [#56489](https://github.com/nodejs/node/pull/56489)
* \[[`6ca27c2a59`](https://github.com/nodejs/node/commit/6ca27c2a59)] - **http2**: omit server name when HTTP2 host is IP address (islandryu) [#56530](https://github.com/nodejs/node/pull/56530)
* \[[`9f1fa199bf`](https://github.com/nodejs/node/commit/9f1fa199bf)] - **inspector**: roll inspector\_protocol (Chengzhong Wu) [#56649](https://github.com/nodejs/node/pull/56649)
* \[[`0dae4bb3ab`](https://github.com/nodejs/node/commit/0dae4bb3ab)] - **inspector**: add undici http tracking support (Chengzhong Wu) [#56488](https://github.com/nodejs/node/pull/56488)
* \[[`2c6124cec4`](https://github.com/nodejs/node/commit/2c6124cec4)] - **inspector**: report loadingFinished until the response data is consumed (Chengzhong Wu) [#56372](https://github.com/nodejs/node/pull/56372)
* \[[`96ec862ce2`](https://github.com/nodejs/node/commit/96ec862ce2)] - **lib**: refactor execution.js (Marco Ippolito) [#56358](https://github.com/nodejs/node/pull/56358)
* \[[`3ac92ef607`](https://github.com/nodejs/node/commit/3ac92ef607)] - **(SEMVER-MINOR)** **lib**: add typescript support to STDIN eval (Marco Ippolito) [#56359](https://github.com/nodejs/node/pull/56359)
* \[[`d5bf3db0cf`](https://github.com/nodejs/node/commit/d5bf3db0cf)] - **lib**: allow skipping source maps in node\_modules (Chengzhong Wu) [#56639](https://github.com/nodejs/node/pull/56639)
* \[[`d33eaf2bcb`](https://github.com/nodejs/node/commit/d33eaf2bcb)] - **lib**: ensure FORCE\_COLOR forces color output in non-TTY environments (Pietro Marchini) [#55404](https://github.com/nodejs/node/pull/55404)
* \[[`dc003218a8`](https://github.com/nodejs/node/commit/dc003218a8)] - **lib**: optimize `prepareStackTrace` on builtin frames (Chengzhong Wu) [#56299](https://github.com/nodejs/node/pull/56299)
* \[[`df06524863`](https://github.com/nodejs/node/commit/df06524863)] - **lib**: suppress source map lookup exceptions (Chengzhong Wu) [#56299](https://github.com/nodejs/node/pull/56299)
* \[[`35335a5a66`](https://github.com/nodejs/node/commit/35335a5a66)] - **meta**: move one or more collaborators to emeritus (Node.js GitHub Bot) [#56580](https://github.com/nodejs/node/pull/56580)
* \[[`1faabdb150`](https://github.com/nodejs/node/commit/1faabdb150)] - **meta**: add codeowners of security release document (Rafael Gonzaga) [#56521](https://github.com/nodejs/node/pull/56521)
* \[[`b4ece22ef5`](https://github.com/nodejs/node/commit/b4ece22ef5)] - **meta**: move one or more collaborators to emeritus (Node.js GitHub Bot) [#56342](https://github.com/nodejs/node/pull/56342)
* \[[`9ec67e7ce0`](https://github.com/nodejs/node/commit/9ec67e7ce0)] - **meta**: move MoLow to TSC regular member (Moshe Atlow) [#56276](https://github.com/nodejs/node/pull/56276)
* \[[`bae4b2e20a`](https://github.com/nodejs/node/commit/bae4b2e20a)] - **module**: use more defensive code when handling SWC errors (Antoine du Hamel) [#56646](https://github.com/nodejs/node/pull/56646)
* \[[`1614e8e7bc`](https://github.com/nodejs/node/commit/1614e8e7bc)] - **(SEMVER-MINOR)** **module**: add ERR\_UNSUPPORTED\_TYPESCRIPT\_SYNTAX (Marco Ippolito) [#56610](https://github.com/nodejs/node/pull/56610)
* \[[`174d88eab1`](https://github.com/nodejs/node/commit/174d88eab1)] - **module**: support eval with ts syntax detection (Marco Ippolito) [#56285](https://github.com/nodejs/node/pull/56285)
* \[[`299d6fa829`](https://github.com/nodejs/node/commit/299d6fa829)] - **module**: fix jsdoc for `format` parameter in cjs/loader (pacexy) [#56501](https://github.com/nodejs/node/pull/56501)
* \[[`0307e4dd59`](https://github.com/nodejs/node/commit/0307e4dd59)] - **module**: unify TypeScript and .mjs handling in CommonJS (Joyee Cheung) [#55590](https://github.com/nodejs/node/pull/55590)
* \[[`1f4f9be93d`](https://github.com/nodejs/node/commit/1f4f9be93d)] - **module**: fix async resolution error within the sync `findPackageJSON` (Jacob Smith) [#56382](https://github.com/nodejs/node/pull/56382)
* \[[`bbedffa0f0`](https://github.com/nodejs/node/commit/bbedffa0f0)] - **module**: simplify `findPackageJSON` implementation (Antoine du Hamel) [#55543](https://github.com/nodejs/node/pull/55543)
* \[[`6d6cffa9cc`](https://github.com/nodejs/node/commit/6d6cffa9cc)] - **(SEMVER-MINOR)** **module**: add `findPackageJSON` util (Jacob Smith) [#55412](https://github.com/nodejs/node/pull/55412)
* \[[`cd7ce18233`](https://github.com/nodejs/node/commit/cd7ce18233)] - **module**: fix bad `require.resolve` with option paths for `.` and `..` (Dario Piotrowicz) [#56735](https://github.com/nodejs/node/pull/56735)
* \[[`152df4da21`](https://github.com/nodejs/node/commit/152df4da21)] - **module**: rethrow amaro error message (Marco Ippolito) [#56568](https://github.com/nodejs/node/pull/56568)
* \[[`acba5dc87e`](https://github.com/nodejs/node/commit/acba5dc87e)] - **module**: use buffer.toString base64 (Chengzhong Wu) [#56315](https://github.com/nodejs/node/pull/56315)
* \[[`01e69be8ff`](https://github.com/nodejs/node/commit/01e69be8ff)] - **node-api**: define version 10 (Gabriel Schulhof) [#55676](https://github.com/nodejs/node/pull/55676)
* \[[`724524528e`](https://github.com/nodejs/node/commit/724524528e)] - **node-api**: remove deprecated attribute from napi\_module\_register (Vladimir Morozov) [#56162](https://github.com/nodejs/node/pull/56162)
* \[[`c78e11064f`](https://github.com/nodejs/node/commit/c78e11064f)] - **process**: remove support for undocumented symbol (Antoine du Hamel) [#56552](https://github.com/nodejs/node/pull/56552)
* \[[`3f69b18a23`](https://github.com/nodejs/node/commit/3f69b18a23)] - **process**: fix symbol key and mark experimental new `node:process` methods (Antoine du Hamel) [#56517](https://github.com/nodejs/node/pull/56517)
* \[[`d35333ae18`](https://github.com/nodejs/node/commit/d35333ae18)] - **(SEMVER-MINOR)** **process**: add process.ref() and process.unref() methods (James M Snell) [#56400](https://github.com/nodejs/node/pull/56400)
* \[[`fa49f0f7d5`](https://github.com/nodejs/node/commit/fa49f0f7d5)] - **punycode**: limit deprecation warning (Colin Ihrig) [#56632](https://github.com/nodejs/node/pull/56632)
* \[[`d77c7073b7`](https://github.com/nodejs/node/commit/d77c7073b7)] - **sqlite**: disable memstatus APIs at build time (Colin Ihrig) [#56541](https://github.com/nodejs/node/pull/56541)
* \[[`07ff3ddcb5`](https://github.com/nodejs/node/commit/07ff3ddcb5)] - **(SEMVER-MINOR)** **sqlite**: support TypedArray and DataView in `StatementSync` (Alex Yang) [#56385](https://github.com/nodejs/node/pull/56385)
* \[[`b6c2e91365`](https://github.com/nodejs/node/commit/b6c2e91365)] - **sqlite**: enable SQL math functions (Colin Ihrig) [#56447](https://github.com/nodejs/node/pull/56447)
* \[[`3462263e8b`](https://github.com/nodejs/node/commit/3462263e8b)] - **sqlite**: pass conflict type to conflict resolution handler (Bart Louwers) [#56352](https://github.com/nodejs/node/pull/56352)
* \[[`89ba3af743`](https://github.com/nodejs/node/commit/89ba3af743)] - **src**: add nullptr handling from X509\_STORE\_new() (Burkov Egor) [#56700](https://github.com/nodejs/node/pull/56700)
* \[[`89a7c82e0c`](https://github.com/nodejs/node/commit/89a7c82e0c)] - **src**: add default value for RSACipherConfig mode field (Burkov Egor) [#56701](https://github.com/nodejs/node/pull/56701)
* \[[`7bae51e62e`](https://github.com/nodejs/node/commit/7bae51e62e)] - **src**: fix build with GCC 15 (tjuhaszrh) [#56740](https://github.com/nodejs/node/pull/56740)
* \[[`432a4b8bd6`](https://github.com/nodejs/node/commit/432a4b8bd6)] - **src**: fix to generate path from wchar\_t via wstring (yamachu) [#56696](https://github.com/nodejs/node/pull/56696)
* \[[`8c9eaf82f0`](https://github.com/nodejs/node/commit/8c9eaf82f0)] - **src**: initialize FSReqWrapSync in path that uses it (Michaël Zasso) [#56613](https://github.com/nodejs/node/pull/56613)
* \[[`bcdb42d40b`](https://github.com/nodejs/node/commit/bcdb42d40b)] - **src**: handle duplicate paths granted (Rafael Gonzaga) [#56591](https://github.com/nodejs/node/pull/56591)
* \[[`d6a7acc207`](https://github.com/nodejs/node/commit/d6a7acc207)] - **src**: update ECKeyPointer in ncrypto (James M Snell) [#56526](https://github.com/nodejs/node/pull/56526)
* \[[`01922f8b1f`](https://github.com/nodejs/node/commit/01922f8b1f)] - **src**: update ECPointPointer in ncrypto (James M Snell) [#56526](https://github.com/nodejs/node/pull/56526)
* \[[`2a3a36eceb`](https://github.com/nodejs/node/commit/2a3a36eceb)] - **src**: update ECGroupPointer in ncrypto (James M Snell) [#56526](https://github.com/nodejs/node/pull/56526)
* \[[`67c10cdacb`](https://github.com/nodejs/node/commit/67c10cdacb)] - **src**: update ECDASSigPointer implementation in ncrypto (James M Snell) [#56526](https://github.com/nodejs/node/pull/56526)
* \[[`17f931c68b`](https://github.com/nodejs/node/commit/17f931c68b)] - **src**: cleaning up more crypto internals for ncrypto (James M Snell) [#56526](https://github.com/nodejs/node/pull/56526)
* \[[`94d3fe1b62`](https://github.com/nodejs/node/commit/94d3fe1b62)] - **(SEMVER-MINOR)** **src**: add --disable-sigusr1 to prevent signal i/o thread (Rafael Gonzaga) [#56441](https://github.com/nodejs/node/pull/56441)
* \[[`6594ee8dff`](https://github.com/nodejs/node/commit/6594ee8dff)] - **src**: fix undefined script name in error source (Chengzhong Wu) [#56502](https://github.com/nodejs/node/pull/56502)
* \[[`b46bad3e91`](https://github.com/nodejs/node/commit/b46bad3e91)] - **src**: refactor --trace-env to reuse option selection and handling (Joyee Cheung) [#56293](https://github.com/nodejs/node/pull/56293)
* \[[`76921b822b`](https://github.com/nodejs/node/commit/76921b822b)] - **src**: minor cleanups on OneByteString usage (James M Snell) [#56482](https://github.com/nodejs/node/pull/56482)
* \[[`3f0d1dd4fe`](https://github.com/nodejs/node/commit/3f0d1dd4fe)] - **src**: move more crypto impl detail to ncrypto dep (James M Snell) [#56421](https://github.com/nodejs/node/pull/56421)
* \[[`04f623b283`](https://github.com/nodejs/node/commit/04f623b283)] - **src**: fixup more ToLocalChecked uses in node\_file (James M Snell) [#56484](https://github.com/nodejs/node/pull/56484)
* \[[`5aa436f5a1`](https://github.com/nodejs/node/commit/5aa436f5a1)] - **src**: make some minor ToLocalChecked cleanups (James M Snell) [#56483](https://github.com/nodejs/node/pull/56483)
* \[[`6eec5e7ec2`](https://github.com/nodejs/node/commit/6eec5e7ec2)] - **src**: lock the thread properly in snapshot builder (Joyee Cheung) [#56327](https://github.com/nodejs/node/pull/56327)
* \[[`5614993968`](https://github.com/nodejs/node/commit/5614993968)] - **src**: drain platform tasks before creating startup snapshot (Chengzhong Wu) [#56403](https://github.com/nodejs/node/pull/56403)
* \[[`48493e9fd5`](https://github.com/nodejs/node/commit/48493e9fd5)] - **src**: use LocalVector in more places (James M Snell) [#56457](https://github.com/nodejs/node/pull/56457)
* \[[`7e5ea0681e`](https://github.com/nodejs/node/commit/7e5ea0681e)] - **src**: use v8::LocalVector consistently with other minor cleanups (James M Snell) [#56417](https://github.com/nodejs/node/pull/56417)
* \[[`ad3d857f2b`](https://github.com/nodejs/node/commit/ad3d857f2b)] - **src**: use starts\_with in fs\_permission.cc (ishabi) [#55811](https://github.com/nodejs/node/pull/55811)
* \[[`5afffb4415`](https://github.com/nodejs/node/commit/5afffb4415)] - **(SEMVER-MINOR)** **src,worker**: add isInternalWorker (Carlos Espa) [#56469](https://github.com/nodejs/node/pull/56469)
* \[[`7d1676e72e`](https://github.com/nodejs/node/commit/7d1676e72e)] - **stream**: fix typo in ReadableStreamBYOBReader.readIntoRequests (Mattias Buelens) [#56560](https://github.com/nodejs/node/pull/56560)
* \[[`e658ea6b26`](https://github.com/nodejs/node/commit/e658ea6b26)] - **stream**: validate undefined sizeAlgorithm in WritableStream (Jason Zhang) [#56067](https://github.com/nodejs/node/pull/56067)
* \[[`e4f133c20c`](https://github.com/nodejs/node/commit/e4f133c20c)] - **test**: add ts eval snapshots (Marco Ippolito) [#56358](https://github.com/nodejs/node/pull/56358)
* \[[`f041742400`](https://github.com/nodejs/node/commit/f041742400)] - **test**: remove empty lines from snapshots (Marco Ippolito) [#56358](https://github.com/nodejs/node/pull/56358)
* \[[`801cde91f6`](https://github.com/nodejs/node/commit/801cde91f6)] - **test**: reduce number of written chunks (Luigi Pinca) [#56757](https://github.com/nodejs/node/pull/56757)
* \[[`6fdf1879ab`](https://github.com/nodejs/node/commit/6fdf1879ab)] - **test**: fix invalid common.mustSucceed() usage (Luigi Pinca) [#56756](https://github.com/nodejs/node/pull/56756)
* \[[`d2bfbfa364`](https://github.com/nodejs/node/commit/d2bfbfa364)] - **test**: use strict mode in global setters test (Rich Trott) [#56742](https://github.com/nodejs/node/pull/56742)
* \[[`5c030da42f`](https://github.com/nodejs/node/commit/5c030da42f)] - **test**: cleanup and simplify test-crypto-aes-wrap (James M Snell) [#56748](https://github.com/nodejs/node/pull/56748)
* \[[`f1442d6eaf`](https://github.com/nodejs/node/commit/f1442d6eaf)] - **test**: do not use common.isMainThread (Luigi Pinca) [#56768](https://github.com/nodejs/node/pull/56768)
* \[[`49405bd9e7`](https://github.com/nodejs/node/commit/49405bd9e7)] - **test**: make some requires lazy in common/index (James M Snell) [#56715](https://github.com/nodejs/node/pull/56715)
* \[[`52ef376788`](https://github.com/nodejs/node/commit/52ef376788)] - **test**: add test that uses multibyte for path and resolves modules (yamachu) [#56696](https://github.com/nodejs/node/pull/56696)
* \[[`b811dea85a`](https://github.com/nodejs/node/commit/b811dea85a)] - **test**: replace more uses of `global` with `globalThis` (James M Snell) [#56712](https://github.com/nodejs/node/pull/56712)
* \[[`eb97076199`](https://github.com/nodejs/node/commit/eb97076199)] - **test**: make common/index slightly less node.js specific (James M Snell) [#56712](https://github.com/nodejs/node/pull/56712)
* \[[`1795202d19`](https://github.com/nodejs/node/commit/1795202d19)] - **test**: rely less on duplicative common test harness utilities (James M Snell) [#56712](https://github.com/nodejs/node/pull/56712)
* \[[`5be29a274e`](https://github.com/nodejs/node/commit/5be29a274e)] - **test**: simplify common/index.js (James M Snell) [#56712](https://github.com/nodejs/node/pull/56712)
* \[[`92e99780f0`](https://github.com/nodejs/node/commit/92e99780f0)] - **test**: move hasMultiLocalhost to common/net (James M Snell) [#56716](https://github.com/nodejs/node/pull/56716)
* \[[`1c3204a4cc`](https://github.com/nodejs/node/commit/1c3204a4cc)] - **test**: move crypto related common utilities in common/crypto (James M Snell) [#56714](https://github.com/nodejs/node/pull/56714)
* \[[`fe79d63be0`](https://github.com/nodejs/node/commit/fe79d63be0)] - **test**: add missing test for env file (Jonas) [#56642](https://github.com/nodejs/node/pull/56642)
* \[[`e08af61537`](https://github.com/nodejs/node/commit/e08af61537)] - **test**: enforce strict mode in test-zlib-const (Rich Trott) [#56689](https://github.com/nodejs/node/pull/56689)
* \[[`c96792d7f8`](https://github.com/nodejs/node/commit/c96792d7f8)] - **test**: fix localization data for ICU 74.2 (Antoine du Hamel) [#56661](https://github.com/nodejs/node/pull/56661)
* \[[`48b72f1195`](https://github.com/nodejs/node/commit/48b72f1195)] - **test**: use --permission instead of --experimental-permission (Rafael Gonzaga) [#56685](https://github.com/nodejs/node/pull/56685)
* \[[`de81d90fce`](https://github.com/nodejs/node/commit/de81d90fce)] - **test**: test-stream-compose.js doesn't need internals (Meghan Denny) [#56619](https://github.com/nodejs/node/pull/56619)
* \[[`f5b8499ad0`](https://github.com/nodejs/node/commit/f5b8499ad0)] - **test**: add maxCount and gcOptions to gcUntil() (Joyee Cheung) [#56522](https://github.com/nodejs/node/pull/56522)
* \[[`d9e5a81041`](https://github.com/nodejs/node/commit/d9e5a81041)] - **test**: add line break at end of file (Rafael Gonzaga) [#56588](https://github.com/nodejs/node/pull/56588)
* \[[`59be346fbf`](https://github.com/nodejs/node/commit/59be346fbf)] - **test**: mark test-worker-prof as flaky on smartos (Joyee Cheung) [#56583](https://github.com/nodejs/node/pull/56583)
* \[[`12a2cae9e5`](https://github.com/nodejs/node/commit/12a2cae9e5)] - **test**: update test-child-process-bad-stdio to use node:test (Colin Ihrig) [#56562](https://github.com/nodejs/node/pull/56562)
* \[[`2dc4a30e19`](https://github.com/nodejs/node/commit/2dc4a30e19)] - **test**: disable openssl 3.4.0 incompatible tests (Jelle van der Waa) [#56160](https://github.com/nodejs/node/pull/56160)
* \[[`1950fbf51d`](https://github.com/nodejs/node/commit/1950fbf51d)] - **test**: make test-crypto-hash compatible with OpenSSL > 3.4.0 (Jelle van der Waa) [#56160](https://github.com/nodejs/node/pull/56160)
* \[[`a533420a91`](https://github.com/nodejs/node/commit/a533420a91)] - **test**: clarify fork inherit permission flags (Rafael Gonzaga) [#56523](https://github.com/nodejs/node/pull/56523)
* \[[`697e799dc1`](https://github.com/nodejs/node/commit/697e799dc1)] - **test**: add error only reporter for node:test (Carlos Espa) [#56438](https://github.com/nodejs/node/pull/56438)
* \[[`4844fa212d`](https://github.com/nodejs/node/commit/4844fa212d)] - **test**: mark test-http-server-request-timeouts-mixed as flaky (Joyee Cheung) [#56503](https://github.com/nodejs/node/pull/56503)
* \[[`843c2389b9`](https://github.com/nodejs/node/commit/843c2389b9)] - **test**: update error code in tls-psk-circuit for for OpenSSL 3.4 (sebastianas) [#56420](https://github.com/nodejs/node/pull/56420)
* \[[`ccb2ddbd83`](https://github.com/nodejs/node/commit/ccb2ddbd83)] - **test**: update compiled sqlite tests to match other tests (Colin Ihrig) [#56446](https://github.com/nodejs/node/pull/56446)
* \[[`b40f50324d`](https://github.com/nodejs/node/commit/b40f50324d)] - **test**: add initial test426 coverage (Chengzhong Wu) [#56436](https://github.com/nodejs/node/pull/56436)
* \[[`059f81e4fd`](https://github.com/nodejs/node/commit/059f81e4fd)] - **test**: update test-set-http-max-http-headers to use node:test (Colin Ihrig) [#56439](https://github.com/nodejs/node/pull/56439)
* \[[`ec2940b418`](https://github.com/nodejs/node/commit/ec2940b418)] - **test**: update test-child-process-windows-hide to use node:test (Colin Ihrig) [#56437](https://github.com/nodejs/node/pull/56437)
* \[[`0362924880`](https://github.com/nodejs/node/commit/0362924880)] - **test**: use unusual chars in the path to ensure our tests are robust (Antoine du Hamel) [#48409](https://github.com/nodejs/node/pull/48409)
* \[[`b6c3869910`](https://github.com/nodejs/node/commit/b6c3869910)] - **test**: improve abort signal dropping test (Edy Silva) [#56339](https://github.com/nodejs/node/pull/56339)
* \[[`cc648ef923`](https://github.com/nodejs/node/commit/cc648ef923)] - **test**: enable ts test on win arm64 (Marco Ippolito) [#56349](https://github.com/nodejs/node/pull/56349)
* \[[`68819b4997`](https://github.com/nodejs/node/commit/68819b4997)] - **test**: deflake test-watch-file-shared-dependency (Luigi Pinca) [#56344](https://github.com/nodejs/node/pull/56344)
* \[[`ca6ed2190c`](https://github.com/nodejs/node/commit/ca6ed2190c)] - **test**: skip `test-sqlite-extensions` when SQLite is not built by us (Antoine du Hamel) [#56341](https://github.com/nodejs/node/pull/56341)
* \[[`8ffeb8b58c`](https://github.com/nodejs/node/commit/8ffeb8b58c)] - **test**: increase spin for eventloop test on s390 (Michael Dawson) [#56228](https://github.com/nodejs/node/pull/56228)
* \[[`6ae9950f08`](https://github.com/nodejs/node/commit/6ae9950f08)] - **test**: migrate message eval tests from Python to JS (Yiyun Lei) [#50482](https://github.com/nodejs/node/pull/50482)
* \[[`4352bf69e9`](https://github.com/nodejs/node/commit/4352bf69e9)] - **test**: check typescript loader (Marco Ippolito) [#54657](https://github.com/nodejs/node/pull/54657)
* \[[`406e7db9c3`](https://github.com/nodejs/node/commit/406e7db9c3)] - **test**: remove async-hooks/test-writewrap flaky designation (Luigi Pinca) [#56048](https://github.com/nodejs/node/pull/56048)
* \[[`fa56ab2bba`](https://github.com/nodejs/node/commit/fa56ab2bba)] - **test**: deflake test-esm-loader-hooks-inspect-brk (Luigi Pinca) [#56050](https://github.com/nodejs/node/pull/56050)
* \[[`8e149aac99`](https://github.com/nodejs/node/commit/8e149aac99)] - **test**: add test case for listeners (origranot) [#56282](https://github.com/nodejs/node/pull/56282)
* \[[`a3f5ef22cd`](https://github.com/nodejs/node/commit/a3f5ef22cd)] - **test**: make `test-permission-sqlite-load-extension` more robust (Antoine du Hamel) [#56295](https://github.com/nodejs/node/pull/56295)
* \[[`8cbb7cc838`](https://github.com/nodejs/node/commit/8cbb7cc838)] - **test\_runner**: print failing assertion only once with spec reporter (Pietro Marchini) [#56662](https://github.com/nodejs/node/pull/56662)
* \[[`1f426bad9a`](https://github.com/nodejs/node/commit/1f426bad9a)] - **test\_runner**: remove unused errors (Pietro Marchini) [#56607](https://github.com/nodejs/node/pull/56607)
* \[[`697a851fb3`](https://github.com/nodejs/node/commit/697a851fb3)] - **(SEMVER-MINOR)** **test\_runner**: add TestContext.prototype.waitFor() (Colin Ihrig) [#56595](https://github.com/nodejs/node/pull/56595)
* \[[`047537b48c`](https://github.com/nodejs/node/commit/047537b48c)] - **(SEMVER-MINOR)** **test\_runner**: add t.assert.fileSnapshot() (Colin Ihrig) [#56459](https://github.com/nodejs/node/pull/56459)
* \[[`19b4aa4b14`](https://github.com/nodejs/node/commit/19b4aa4b14)] - **test\_runner**: run single test file benchmark (Pietro Marchini) [#56479](https://github.com/nodejs/node/pull/56479)
* \[[`926cf84e95`](https://github.com/nodejs/node/commit/926cf84e95)] - **(SEMVER-MINOR)** **test\_runner**: add assert.register() API (Colin Ihrig) [#56434](https://github.com/nodejs/node/pull/56434)
* \[[`fb4661a4cf`](https://github.com/nodejs/node/commit/fb4661a4cf)] - **test\_runner**: finish marking snapshot testing as stable (Colin Ihrig) [#56425](https://github.com/nodejs/node/pull/56425)
* \[[`900c6c3940`](https://github.com/nodejs/node/commit/900c6c3940)] - **tls**: fix error stack conversion in cryptoErrorListToException() (Joyee Cheung) [#56554](https://github.com/nodejs/node/pull/56554)
* \[[`e9f185b658`](https://github.com/nodejs/node/commit/e9f185b658)] - **tools**: update doc to new version (Node.js GitHub Bot) [#56259](https://github.com/nodejs/node/pull/56259)
* \[[`7644c7e619`](https://github.com/nodejs/node/commit/7644c7e619)] - **tools**: update inspector\_protocol roller (Chengzhong Wu) [#56649](https://github.com/nodejs/node/pull/56649)
* \[[`362272b0a4`](https://github.com/nodejs/node/commit/362272b0a4)] - **tools**: do not throw on missing `create-release-proposal.sh` (Antoine du Hamel) [#56704](https://github.com/nodejs/node/pull/56704)
* \[[`df8b835953`](https://github.com/nodejs/node/commit/df8b835953)] - **tools**: fix tools-deps-update (Daniel Lemire) [#56684](https://github.com/nodejs/node/pull/56684)
* \[[`feba5d3274`](https://github.com/nodejs/node/commit/feba5d3274)] - **tools**: do not throw on missing `create-release-proposal.sh` (Antoine du Hamel) [#56695](https://github.com/nodejs/node/pull/56695)
* \[[`9827f7d395`](https://github.com/nodejs/node/commit/9827f7d395)] - **tools**: fix permissions in `lint-release-proposal` workflow (Antoine du Hamel) [#56614](https://github.com/nodejs/node/pull/56614)
* \[[`14c562c0dc`](https://github.com/nodejs/node/commit/14c562c0dc)] - **tools**: remove github reporter (Carlos Espa) [#56468](https://github.com/nodejs/node/pull/56468)
* \[[`ed1785d0ae`](https://github.com/nodejs/node/commit/ed1785d0ae)] - **tools**: edit `create-release-proposal` workflow (Antoine du Hamel) [#56540](https://github.com/nodejs/node/pull/56540)
* \[[`294e4c42f5`](https://github.com/nodejs/node/commit/294e4c42f5)] - **tools**: validate commit list as part of `lint-release-commit` (Antoine du Hamel) [#56291](https://github.com/nodejs/node/pull/56291)
* \[[`98d3474267`](https://github.com/nodejs/node/commit/98d3474267)] - **tools**: fix loong64 build failed (Xiao-Tao) [#56466](https://github.com/nodejs/node/pull/56466)
* \[[`3e729ceec8`](https://github.com/nodejs/node/commit/3e729ceec8)] - **tools**: disable unneeded rule ignoring in Python linting (Rich Trott) [#56429](https://github.com/nodejs/node/pull/56429)
* \[[`d5c05328e2`](https://github.com/nodejs/node/commit/d5c05328e2)] - **tools**: use a configurable value for number of open dependabot PRs (Antoine du Hamel) [#56427](https://github.com/nodejs/node/pull/56427)
* \[[`1705cbe002`](https://github.com/nodejs/node/commit/1705cbe002)] - **tools**: bump the eslint group in /tools/eslint with 4 updates (dependabot\[bot]) [#56426](https://github.com/nodejs/node/pull/56426)
* \[[`53b29b0469`](https://github.com/nodejs/node/commit/53b29b0469)] - **tools**: fix `require-common-first` lint rule from subfolder (Antoine du Hamel) [#56325](https://github.com/nodejs/node/pull/56325)
* \[[`105c4ed4fb`](https://github.com/nodejs/node/commit/105c4ed4fb)] - **tools**: add release line label when opening release proposal (Antoine du Hamel) [#56317](https://github.com/nodejs/node/pull/56317)
* \[[`30f61f4aa5`](https://github.com/nodejs/node/commit/30f61f4aa5)] - **url**: use resolved path to convert UNC paths to URL (Antoine du Hamel) [#56302](https://github.com/nodejs/node/pull/56302)
* \[[`a0aef4dfb6`](https://github.com/nodejs/node/commit/a0aef4dfb6)] - **util**: inspect: do not crash on an Error stack that contains a Symbol (Jordan Harband) [#56573](https://github.com/nodejs/node/pull/56573)
* \[[`a8a060341f`](https://github.com/nodejs/node/commit/a8a060341f)] - **util**: inspect: do not crash on an Error with a regex `name` (Jordan Harband) [#56574](https://github.com/nodejs/node/pull/56574)
* \[[`ea66bf3553`](https://github.com/nodejs/node/commit/ea66bf3553)] - **util**: rename CallSite.column to columnNumber (Chengzhong Wu) [#56584](https://github.com/nodejs/node/pull/56584)
* \[[`9cdc3b373c`](https://github.com/nodejs/node/commit/9cdc3b373c)] - **util**: do not crash on inspecting function with `Symbol` name (Jordan Harband) [#56572](https://github.com/nodejs/node/pull/56572)
* \[[`0bfbb68569`](https://github.com/nodejs/node/commit/0bfbb68569)] - **util**: expose CallSite.scriptId (Chengzhong Wu) [#56551](https://github.com/nodejs/node/pull/56551)
* \[[`5dd7116e09`](https://github.com/nodejs/node/commit/5dd7116e09)] - **watch**: reload env file for --env-file-if-exists (Jonas) [#56643](https://github.com/nodejs/node/pull/56643)
* \[[`c658a8afdf`](https://github.com/nodejs/node/commit/c658a8afdf)] - **(SEMVER-MINOR)** **worker**: add eval ts input (Marco Ippolito) [#56394](https://github.com/nodejs/node/pull/56394)
* \[[`2e5d038f48`](https://github.com/nodejs/node/commit/2e5d038f48)] - **worker**: refactor stdio to improve performance (Matteo Collina) [#56630](https://github.com/nodejs/node/pull/56630)
* \[[`f959805d01`](https://github.com/nodejs/node/commit/f959805d01)] - **worker**: flush stdout and stderr on exit (Matteo Collina) [#56428](https://github.com/nodejs/node/pull/56428)

<a id="22.13.1"></a>

## 2025-01-21, Version 22.13.1 'Jod' (LTS), @RafaelGSS

This is a security release.

### Notable Changes

* CVE-2025-23083 - src,loader,permission: throw on InternalWorker use when permission model is enabled (High)
* CVE-2025-23085 - src: fix HTTP2 mem leak on premature close and ERR\_PROTO (Medium)
* CVE-2025-23084 - path: fix path traversal in normalize() on Windows (Medium)

Dependency update:

* CVE-2025-22150 - Use of Insufficiently Random Values in undici fetch() (Medium)

### Commits

* \[[`520da342e0`](https://github.com/nodejs/node/commit/520da342e0)] - **(CVE-2025-22150)** **deps**: update undici to v6.21.1 (Matteo Collina) [nodejs-private/node-private#662](https://github.com/nodejs-private/node-private/pull/662)
* \[[`99f217369f`](https://github.com/nodejs/node/commit/99f217369f)] - **(CVE-2025-23084)** **path**: fix path traversal in normalize() on Windows (Tobias Nießen) [nodejs-private/node-private#555](https://github.com/nodejs-private/node-private/pull/555)
* \[[`984f735e35`](https://github.com/nodejs/node/commit/984f735e35)] - **(CVE-2025-23085)** **src**: fix HTTP2 mem leak on premature close and ERR\_PROTO (RafaelGSS) [nodejs-private/node-private#650](https://github.com/nodejs-private/node-private/pull/650)
* \[[`2446870618`](https://github.com/nodejs/node/commit/2446870618)] - **(CVE-2025-23083)** **src,loader,permission**: throw on InternalWorker use (RafaelGSS) [nodejs-private/node-private#651](https://github.com/nodejs-private/node-private/pull/651)

<a id="22.13.0"></a>

## 2025-01-07, Version 22.13.0 'Jod' (LTS), @ruyadorno

### Notable Changes

#### Stabilize Permission Model

Upgrades the Permission Model status from Active Development to Stable.

Contributed by Rafael Gonzaga [#56201](https://github.com/nodejs/node/pull/56201)

#### Graduate WebCryptoAPI Ed25519 and X25519 algorithms as stable

Following the merge of Curve25519 into the [Web Cryptography API Editor's Draft](https://w3c.github.io/webcrypto/) the `Ed25519` and `X25519` algorithm identifiers are now stable and will no longer emit an ExperimentalWarning upon use.

Contributed by (Filip Skokan) [#56142](https://github.com/nodejs/node/pull/56142)

#### Other Notable Changes

* \[[`05d6227a88`](https://github.com/nodejs/node/commit/05d6227a88)] - **(SEMVER-MINOR)** **assert**: add partialDeepStrictEqual (Giovanni Bucci) [#54630](https://github.com/nodejs/node/pull/54630)
* \[[`a933103499`](https://github.com/nodejs/node/commit/a933103499)] - **(SEMVER-MINOR)** **cli**: implement --trace-env and --trace-env-\[js|native]-stack (Joyee Cheung) [#55604](https://github.com/nodejs/node/pull/55604)
* \[[`ba9d5397de`](https://github.com/nodejs/node/commit/ba9d5397de)] - **(SEMVER-MINOR)** **dgram**: support blocklist in udp (theanarkh) [#56087](https://github.com/nodejs/node/pull/56087)
* \[[`f6d0c01303`](https://github.com/nodejs/node/commit/f6d0c01303)] - **doc**: stabilize util.styleText (Rafael Gonzaga) [#56265](https://github.com/nodejs/node/pull/56265)
* \[[`34c68827af`](https://github.com/nodejs/node/commit/34c68827af)] - **doc**: move typescript support to active development (Marco Ippolito) [#55536](https://github.com/nodejs/node/pull/55536)
* \[[`dd14b80350`](https://github.com/nodejs/node/commit/dd14b80350)] - **doc**: add LJHarb to collaborators (Jordan Harband) [#56132](https://github.com/nodejs/node/pull/56132)
* \[[`5263086169`](https://github.com/nodejs/node/commit/5263086169)] - **(SEMVER-MINOR)** **doc**: add report version and history section (Chengzhong Wu) [#56130](https://github.com/nodejs/node/pull/56130)
* \[[`8cb3c2018d`](https://github.com/nodejs/node/commit/8cb3c2018d)] - **(SEMVER-MINOR)** **doc**: sort --report-exclude alphabetically (Rafael Gonzaga) [#55788](https://github.com/nodejs/node/pull/55788)
* \[[`55239a48b6`](https://github.com/nodejs/node/commit/55239a48b6)] - **(SEMVER-MINOR)** **doc,lib,src,test**: unflag sqlite module (Colin Ihrig) [#55890](https://github.com/nodejs/node/pull/55890)
* \[[`7cbe3de1d8`](https://github.com/nodejs/node/commit/7cbe3de1d8)] - **(SEMVER-MINOR)** **module**: only emit require(esm) warning under --trace-require-module (Joyee Cheung) [#56194](https://github.com/nodejs/node/pull/56194)
* \[[`6575b76042`](https://github.com/nodejs/node/commit/6575b76042)] - **(SEMVER-MINOR)** **module**: add module.stripTypeScriptTypes (Marco Ippolito) [#55282](https://github.com/nodejs/node/pull/55282)
* \[[`bacfe6d5c9`](https://github.com/nodejs/node/commit/bacfe6d5c9)] - **(SEMVER-MINOR)** **net**: support blocklist in net.connect (theanarkh) [#56075](https://github.com/nodejs/node/pull/56075)
* \[[`b47888d390`](https://github.com/nodejs/node/commit/b47888d390)] - **(SEMVER-MINOR)** **net**: support blocklist for net.Server (theanarkh) [#56079](https://github.com/nodejs/node/pull/56079)
* \[[`566f0a1d25`](https://github.com/nodejs/node/commit/566f0a1d25)] - **(SEMVER-MINOR)** **net**: add SocketAddress.parse (James M Snell) [#56076](https://github.com/nodejs/node/pull/56076)
* \[[`ed7eab1421`](https://github.com/nodejs/node/commit/ed7eab1421)] - **(SEMVER-MINOR)** **net**: add net.BlockList.isBlockList(value) (James M Snell) [#56078](https://github.com/nodejs/node/pull/56078)
* \[[`ea4891856d`](https://github.com/nodejs/node/commit/ea4891856d)] - **(SEMVER-MINOR)** **process**: deprecate `features.{ipv6,uv}` and `features.tls_*` (René) [#55545](https://github.com/nodejs/node/pull/55545)
* \[[`01eb308f26`](https://github.com/nodejs/node/commit/01eb308f26)] - **(SEMVER-MINOR)** **report**: fix typos in report keys and bump the version (Yuan-Ming Hsu) [#56068](https://github.com/nodejs/node/pull/56068)
* \[[`97c38352d0`](https://github.com/nodejs/node/commit/97c38352d0)] - **(SEMVER-MINOR)** **sqlite**: aggregate constants in a single property (Edigleysson Silva (Edy)) [#56213](https://github.com/nodejs/node/pull/56213)
* \[[`b4041e554a`](https://github.com/nodejs/node/commit/b4041e554a)] - **(SEMVER-MINOR)** **sqlite**: add `StatementSync.prototype.iterate` method (tpoisseau) [#54213](https://github.com/nodejs/node/pull/54213)
* \[[`2e3ca1bbdd`](https://github.com/nodejs/node/commit/2e3ca1bbdd)] - **(SEMVER-MINOR)** **src**: add cli option to preserve env vars on diagnostic reports (Rafael Gonzaga) [#55697](https://github.com/nodejs/node/pull/55697)
* \[[`bcfe9c80fc`](https://github.com/nodejs/node/commit/bcfe9c80fc)] - **(SEMVER-MINOR)** **util**: add sourcemap support to getCallSites (Marco Ippolito) [#55589](https://github.com/nodejs/node/pull/55589)

### Commits

* \[[`e9024779c0`](https://github.com/nodejs/node/commit/e9024779c0)] - **assert**: make Maps be partially compared in partialDeepStrictEqual (Giovanni Bucci) [#56195](https://github.com/nodejs/node/pull/56195)
* \[[`4c13d8e587`](https://github.com/nodejs/node/commit/4c13d8e587)] - **assert**: make partialDeepStrictEqual work with ArrayBuffers (Giovanni Bucci) [#56098](https://github.com/nodejs/node/pull/56098)
* \[[`a4fa31a86e`](https://github.com/nodejs/node/commit/a4fa31a86e)] - **assert**: optimize partial comparison of two `Set`s (Antoine du Hamel) [#55970](https://github.com/nodejs/node/pull/55970)
* \[[`05d6227a88`](https://github.com/nodejs/node/commit/05d6227a88)] - **(SEMVER-MINOR)** **assert**: add partialDeepStrictEqual (Giovanni Bucci) [#54630](https://github.com/nodejs/node/pull/54630)
* \[[`5e1321abd7`](https://github.com/nodejs/node/commit/5e1321abd7)] - **buffer**: document concat zero-fill (Duncan) [#55562](https://github.com/nodejs/node/pull/55562)
* \[[`be5ba7c648`](https://github.com/nodejs/node/commit/be5ba7c648)] - **build**: set DESTCPU correctly for 'make binary' on loongarch64 (吴小白) [#56271](https://github.com/nodejs/node/pull/56271)
* \[[`38cf37ee2d`](https://github.com/nodejs/node/commit/38cf37ee2d)] - **build**: fix missing fp16 dependency in d8 builds (Joyee Cheung) [#56266](https://github.com/nodejs/node/pull/56266)
* \[[`dbb7557455`](https://github.com/nodejs/node/commit/dbb7557455)] - **build**: add major release action (Rafael Gonzaga) [#56199](https://github.com/nodejs/node/pull/56199)
* \[[`27cc90f3be`](https://github.com/nodejs/node/commit/27cc90f3be)] - **build**: fix C string encoding for `PRODUCT_DIR_ABS` (Anna Henningsen) [#56111](https://github.com/nodejs/node/pull/56111)
* \[[`376561c2b4`](https://github.com/nodejs/node/commit/376561c2b4)] - **build**: use variable for simdutf path (Shelley Vohr) [#56196](https://github.com/nodejs/node/pull/56196)
* \[[`126ae15000`](https://github.com/nodejs/node/commit/126ae15000)] - **build**: allow overriding clang usage (Shelley Vohr) [#56016](https://github.com/nodejs/node/pull/56016)
* \[[`97bb8f7c76`](https://github.com/nodejs/node/commit/97bb8f7c76)] - **build**: remove defaults for create-release-proposal (Rafael Gonzaga) [#56042](https://github.com/nodejs/node/pull/56042)
* \[[`a8fb1a06f3`](https://github.com/nodejs/node/commit/a8fb1a06f3)] - **build**: set node\_arch to target\_cpu in GN (Shelley Vohr) [#55967](https://github.com/nodejs/node/pull/55967)
* \[[`9f48ca27f1`](https://github.com/nodejs/node/commit/9f48ca27f1)] - **build**: use variable for crypto dep path (Shelley Vohr) [#55928](https://github.com/nodejs/node/pull/55928)
* \[[`e47ccd2287`](https://github.com/nodejs/node/commit/e47ccd2287)] - **build**: fix GN build for sqlite (Cheng) [#55912](https://github.com/nodejs/node/pull/55912)
* \[[`8d70b99a5a`](https://github.com/nodejs/node/commit/8d70b99a5a)] - **build**: compile bundled simdutf conditionally (Jakub Jirutka) [#55886](https://github.com/nodejs/node/pull/55886)
* \[[`826fd35242`](https://github.com/nodejs/node/commit/826fd35242)] - **build**: compile bundled simdjson conditionally (Jakub Jirutka) [#55886](https://github.com/nodejs/node/pull/55886)
* \[[`1015b22085`](https://github.com/nodejs/node/commit/1015b22085)] - **build**: compile bundled ada conditionally (Jakub Jirutka) [#55886](https://github.com/nodejs/node/pull/55886)
* \[[`77e2869ca6`](https://github.com/nodejs/node/commit/77e2869ca6)] - **build**: use glob for dependencies of out/Makefile (Richard Lau) [#55789](https://github.com/nodejs/node/pull/55789)
* \[[`a933103499`](https://github.com/nodejs/node/commit/a933103499)] - **(SEMVER-MINOR)** **cli**: implement --trace-env and --trace-env-\[js|native]-stack (Joyee Cheung) [#55604](https://github.com/nodejs/node/pull/55604)
* \[[`72e8e0684e`](https://github.com/nodejs/node/commit/72e8e0684e)] - **crypto**: graduate WebCryptoAPI Ed25519 and X25519 algorithms as stable (Filip Skokan) [#56142](https://github.com/nodejs/node/pull/56142)
* \[[`fe2b344ddb`](https://github.com/nodejs/node/commit/fe2b344ddb)] - **crypto**: ensure CryptoKey usages and algorithm are cached objects (Filip Skokan) [#56108](https://github.com/nodejs/node/pull/56108)
* \[[`9ee9f524a7`](https://github.com/nodejs/node/commit/9ee9f524a7)] - **crypto**: allow non-multiple of 8 in SubtleCrypto.deriveBits (Filip Skokan) [#55296](https://github.com/nodejs/node/pull/55296)
* \[[`76f242d993`](https://github.com/nodejs/node/commit/76f242d993)] - **deps**: update nghttp3 to 1.6.0 (Node.js GitHub Bot) [#56258](https://github.com/nodejs/node/pull/56258)
* \[[`c7ff2ea6b5`](https://github.com/nodejs/node/commit/c7ff2ea6b5)] - **deps**: update simdutf to 5.6.4 (Node.js GitHub Bot) [#56255](https://github.com/nodejs/node/pull/56255)
* \[[`04230be1ef`](https://github.com/nodejs/node/commit/04230be1ef)] - **deps**: update libuv to 1.49.2 (Luigi Pinca) [#56224](https://github.com/nodejs/node/pull/56224)
* \[[`88589b85b7`](https://github.com/nodejs/node/commit/88589b85b7)] - **deps**: update c-ares to v1.34.4 (Node.js GitHub Bot) [#56256](https://github.com/nodejs/node/pull/56256)
* \[[`5c2e0618f3`](https://github.com/nodejs/node/commit/5c2e0618f3)] - **deps**: define V8\_PRESERVE\_MOST as no-op on Windows (Stefan Stojanovic) [#56238](https://github.com/nodejs/node/pull/56238)
* \[[`9f8f3c9658`](https://github.com/nodejs/node/commit/9f8f3c9658)] - **deps**: update sqlite to 3.47.2 (Node.js GitHub Bot) [#56178](https://github.com/nodejs/node/pull/56178)
* \[[`17b6931d3b`](https://github.com/nodejs/node/commit/17b6931d3b)] - **deps**: update ngtcp2 to 1.9.1 (Node.js GitHub Bot) [#56095](https://github.com/nodejs/node/pull/56095)
* \[[`22b453b619`](https://github.com/nodejs/node/commit/22b453b619)] - **deps**: upgrade npm to 10.9.2 (npm team) [#56135](https://github.com/nodejs/node/pull/56135)
* \[[`d7eb41b382`](https://github.com/nodejs/node/commit/d7eb41b382)] - **deps**: update sqlite to 3.47.1 (Node.js GitHub Bot) [#56094](https://github.com/nodejs/node/pull/56094)
* \[[`669c722aa9`](https://github.com/nodejs/node/commit/669c722aa9)] - **deps**: update zlib to 1.3.0.1-motley-82a5fec (Node.js GitHub Bot) [#55980](https://github.com/nodejs/node/pull/55980)
* \[[`f61a0454d2`](https://github.com/nodejs/node/commit/f61a0454d2)] - **deps**: update corepack to 0.30.0 (Node.js GitHub Bot) [#55977](https://github.com/nodejs/node/pull/55977)
* \[[`d98bf0b891`](https://github.com/nodejs/node/commit/d98bf0b891)] - **deps**: update ngtcp2 to 1.9.0 (Node.js GitHub Bot) [#55975](https://github.com/nodejs/node/pull/55975)
* \[[`fc362624bf`](https://github.com/nodejs/node/commit/fc362624bf)] - **deps**: update simdutf to 5.6.3 (Node.js GitHub Bot) [#55973](https://github.com/nodejs/node/pull/55973)
* \[[`f61dcc4df4`](https://github.com/nodejs/node/commit/f61dcc4df4)] - **deps**: upgrade npm to 10.9.1 (npm team) [#55951](https://github.com/nodejs/node/pull/55951)
* \[[`bfe7982491`](https://github.com/nodejs/node/commit/bfe7982491)] - **deps**: update zlib to 1.3.0.1-motley-7e2e4d7 (Node.js GitHub Bot) [#54432](https://github.com/nodejs/node/pull/54432)
* \[[`d714367ef8`](https://github.com/nodejs/node/commit/d714367ef8)] - **deps**: update simdjson to 3.10.1 (Node.js GitHub Bot) [#54678](https://github.com/nodejs/node/pull/54678)
* \[[`ccc9b105ec`](https://github.com/nodejs/node/commit/ccc9b105ec)] - **deps**: update simdutf to 5.6.2 (Node.js GitHub Bot) [#55889](https://github.com/nodejs/node/pull/55889)
* \[[`ba9d5397de`](https://github.com/nodejs/node/commit/ba9d5397de)] - **(SEMVER-MINOR)** **dgram**: support blocklist in udp (theanarkh) [#56087](https://github.com/nodejs/node/pull/56087)
* \[[`7ddbf94849`](https://github.com/nodejs/node/commit/7ddbf94849)] - **dgram**: check udp buffer size to avoid fd leak (theanarkh) [#56084](https://github.com/nodejs/node/pull/56084)
* \[[`360d68de0f`](https://github.com/nodejs/node/commit/360d68de0f)] - **doc**: fix color contrast issue in light mode (Rich Trott) [#56272](https://github.com/nodejs/node/pull/56272)
* \[[`f6d0c01303`](https://github.com/nodejs/node/commit/f6d0c01303)] - **doc**: stabilize util.styleText (Rafael Gonzaga) [#56265](https://github.com/nodejs/node/pull/56265)
* \[[`9436c3c949`](https://github.com/nodejs/node/commit/9436c3c949)] - **doc**: clarify util.aborted resource usage (Kunal Kumar) [#55780](https://github.com/nodejs/node/pull/55780)
* \[[`b1cec2cef9`](https://github.com/nodejs/node/commit/b1cec2cef9)] - **doc**: add esm examples to node:repl (Alfredo González) [#55432](https://github.com/nodejs/node/pull/55432)
* \[[`d6a84cf781`](https://github.com/nodejs/node/commit/d6a84cf781)] - **doc**: add esm examples to node:readline (Alfredo González) [#55335](https://github.com/nodejs/node/pull/55335)
* \[[`a11ac1c0f2`](https://github.com/nodejs/node/commit/a11ac1c0f2)] - **doc**: fix 'which' to 'that' and add commas (Selveter Senitro) [#56216](https://github.com/nodejs/node/pull/56216)
* \[[`5331df7911`](https://github.com/nodejs/node/commit/5331df7911)] - **doc**: fix winget config path (Alex Yang) [#56233](https://github.com/nodejs/node/pull/56233)
* \[[`7a8071b43c`](https://github.com/nodejs/node/commit/7a8071b43c)] - **doc**: add esm examples to node:tls (Alfredo González) [#56229](https://github.com/nodejs/node/pull/56229)
* \[[`7d8c1e72d5`](https://github.com/nodejs/node/commit/7d8c1e72d5)] - **doc**: add esm examples to node:perf\_hooks (Alfredo González) [#55257](https://github.com/nodejs/node/pull/55257)
* \[[`ea53c4b1ae`](https://github.com/nodejs/node/commit/ea53c4b1ae)] - **doc**: `sea.getRawAsset(key)` always returns an ArrayBuffer (沈鸿飞) [#56206](https://github.com/nodejs/node/pull/56206)
* \[[`7a94100a3e`](https://github.com/nodejs/node/commit/7a94100a3e)] - **doc**: update announce documentation for releases (Rafael Gonzaga) [#56200](https://github.com/nodejs/node/pull/56200)
* \[[`44c4e57e32`](https://github.com/nodejs/node/commit/44c4e57e32)] - **doc**: update blog link to /vulnerability (Rafael Gonzaga) [#56198](https://github.com/nodejs/node/pull/56198)
* \[[`5e5b4b0cbd`](https://github.com/nodejs/node/commit/5e5b4b0cbd)] - **doc**: call out import.meta is only supported in ES modules (Anton Kastritskii) [#56186](https://github.com/nodejs/node/pull/56186)
* \[[`a83de32d35`](https://github.com/nodejs/node/commit/a83de32d35)] - **doc**: add ambassador message - benefits of Node.js (Michael Dawson) [#56085](https://github.com/nodejs/node/pull/56085)
* \[[`bb880dd21a`](https://github.com/nodejs/node/commit/bb880dd21a)] - **doc**: fix incorrect link to style guide (Yuan-Ming Hsu) [#56181](https://github.com/nodejs/node/pull/56181)
* \[[`39ce902e58`](https://github.com/nodejs/node/commit/39ce902e58)] - **doc**: fix c++ addon hello world sample (Edigleysson Silva (Edy)) [#56172](https://github.com/nodejs/node/pull/56172)
* \[[`19c72c4acc`](https://github.com/nodejs/node/commit/19c72c4acc)] - **doc**: update blog release-post link (Ruy Adorno) [#56123](https://github.com/nodejs/node/pull/56123)
* \[[`b667cc4669`](https://github.com/nodejs/node/commit/b667cc4669)] - **doc**: fix module.md headings (Chengzhong Wu) [#56131](https://github.com/nodejs/node/pull/56131)
* \[[`34c68827af`](https://github.com/nodejs/node/commit/34c68827af)] - **doc**: move typescript support to active development (Marco Ippolito) [#55536](https://github.com/nodejs/node/pull/55536)
* \[[`c4a97d810b`](https://github.com/nodejs/node/commit/c4a97d810b)] - **doc**: mention `-a` flag for the release script (Ruy Adorno) [#56124](https://github.com/nodejs/node/pull/56124)
* \[[`dd14b80350`](https://github.com/nodejs/node/commit/dd14b80350)] - **doc**: add LJHarb to collaborators (Jordan Harband) [#56132](https://github.com/nodejs/node/pull/56132)
* \[[`2feb0781ed`](https://github.com/nodejs/node/commit/2feb0781ed)] - **doc**: add create-release-action to process (Rafael Gonzaga) [#55993](https://github.com/nodejs/node/pull/55993)
* \[[`71f6263942`](https://github.com/nodejs/node/commit/71f6263942)] - **doc**: rename file to advocacy-ambassador-program.md (Tobias Nießen) [#56046](https://github.com/nodejs/node/pull/56046)
* \[[`8efa240500`](https://github.com/nodejs/node/commit/8efa240500)] - **doc**: remove unused import from sample code (Blended Bram) [#55570](https://github.com/nodejs/node/pull/55570)
* \[[`e64cef8bf4`](https://github.com/nodejs/node/commit/e64cef8bf4)] - **doc**: add FAQ to releases section (Rafael Gonzaga) [#55992](https://github.com/nodejs/node/pull/55992)
* \[[`4bb0f30f92`](https://github.com/nodejs/node/commit/4bb0f30f92)] - **doc**: move history entry to class description (Luigi Pinca) [#55991](https://github.com/nodejs/node/pull/55991)
* \[[`6d02bd6873`](https://github.com/nodejs/node/commit/6d02bd6873)] - **doc**: add history entry for textEncoder.encodeInto() (Luigi Pinca) [#55990](https://github.com/nodejs/node/pull/55990)
* \[[`e239382ed8`](https://github.com/nodejs/node/commit/e239382ed8)] - **doc**: improve GN build documentation a bit (Shelley Vohr) [#55968](https://github.com/nodejs/node/pull/55968)
* \[[`78b6aef6bc`](https://github.com/nodejs/node/commit/78b6aef6bc)] - **doc**: fix deprecation codes (Filip Skokan) [#56018](https://github.com/nodejs/node/pull/56018)
* \[[`474bf80a44`](https://github.com/nodejs/node/commit/474bf80a44)] - **doc**: remove confusing and outdated sentence (Luigi Pinca) [#55988](https://github.com/nodejs/node/pull/55988)
* \[[`57381076c5`](https://github.com/nodejs/node/commit/57381076c5)] - **doc**: deprecate passing invalid types in `fs.existsSync` (Carlos Espa) [#55892](https://github.com/nodejs/node/pull/55892)
* \[[`e529cf6b26`](https://github.com/nodejs/node/commit/e529cf6b26)] - **doc**: add doc for PerformanceObserver.takeRecords() (skyclouds2001) [#55786](https://github.com/nodejs/node/pull/55786)
* \[[`a6ef0f6f6e`](https://github.com/nodejs/node/commit/a6ef0f6f6e)] - **doc**: add vetted courses to the ambassador benefits (Matteo Collina) [#55934](https://github.com/nodejs/node/pull/55934)
* \[[`63526049f2`](https://github.com/nodejs/node/commit/63526049f2)] - **doc**: order `node:crypto` APIs alphabetically (Julian Gassner) [#55831](https://github.com/nodejs/node/pull/55831)
* \[[`36080b7b61`](https://github.com/nodejs/node/commit/36080b7b61)] - **doc**: doc how to add message for promotion (Michael Dawson) [#55843](https://github.com/nodejs/node/pull/55843)
* \[[`12b2ad4287`](https://github.com/nodejs/node/commit/12b2ad4287)] - **doc**: add esm example for zlib (Leonardo Peixoto) [#55946](https://github.com/nodejs/node/pull/55946)
* \[[`352daac296`](https://github.com/nodejs/node/commit/352daac296)] - **doc**: fix typo (Alex Yang) [#56125](https://github.com/nodejs/node/pull/56125)
* \[[`6e7e9a126d`](https://github.com/nodejs/node/commit/6e7e9a126d)] - **doc**: document approach for building wasm in deps (Michael Dawson) [#55940](https://github.com/nodejs/node/pull/55940)
* \[[`0b3ac05422`](https://github.com/nodejs/node/commit/0b3ac05422)] - **doc**: remove RedYetiDev from triagers team (Aviv Keller) [#55947](https://github.com/nodejs/node/pull/55947)
* \[[`20be5e2f80`](https://github.com/nodejs/node/commit/20be5e2f80)] - **doc**: add esm examples to node:timers (Alfredo González) [#55857](https://github.com/nodejs/node/pull/55857)
* \[[`3ba9b57436`](https://github.com/nodejs/node/commit/3ba9b57436)] - **doc**: fix relative path mention in --allow-fs (Rafael Gonzaga) [#55791](https://github.com/nodejs/node/pull/55791)
* \[[`3e6b3a9a8b`](https://github.com/nodejs/node/commit/3e6b3a9a8b)] - **doc**: include git node release --promote to steps (Rafael Gonzaga) [#55835](https://github.com/nodejs/node/pull/55835)
* \[[`5bdfde8dc6`](https://github.com/nodejs/node/commit/5bdfde8dc6)] - **doc**: add history entry for import assertion removal (Antoine du Hamel) [#55883](https://github.com/nodejs/node/pull/55883)
* \[[`c842146c05`](https://github.com/nodejs/node/commit/c842146c05)] - **doc**: add a note on console stream behavior (Gireesh Punathil) [#55616](https://github.com/nodejs/node/pull/55616)
* \[[`5263086169`](https://github.com/nodejs/node/commit/5263086169)] - **(SEMVER-MINOR)** **doc**: add report version and history section (Chengzhong Wu) [#56130](https://github.com/nodejs/node/pull/56130)
* \[[`8cb3c2018d`](https://github.com/nodejs/node/commit/8cb3c2018d)] - **(SEMVER-MINOR)** **doc**: sort --report-exclude alphabetically (Rafael Gonzaga) [#55788](https://github.com/nodejs/node/pull/55788)
* \[[`55239a48b6`](https://github.com/nodejs/node/commit/55239a48b6)] - **(SEMVER-MINOR)** **doc,lib,src,test**: unflag sqlite module (Colin Ihrig) [#55890](https://github.com/nodejs/node/pull/55890)
* \[[`04d7c7a349`](https://github.com/nodejs/node/commit/04d7c7a349)] - **fs**: make mutating `options` in Callback `readdir()` not affect results (LiviaMedeiros) [#56057](https://github.com/nodejs/node/pull/56057)
* \[[`92bcd528e7`](https://github.com/nodejs/node/commit/92bcd528e7)] - **fs**: make mutating `options` in Promises `readdir()` not affect results (LiviaMedeiros) [#56057](https://github.com/nodejs/node/pull/56057)
* \[[`3a55bd9448`](https://github.com/nodejs/node/commit/3a55bd9448)] - **fs**: lazily load ReadFileContext (Gürgün Dayıoğlu) [#55998](https://github.com/nodejs/node/pull/55998)
* \[[`0331b3fdd3`](https://github.com/nodejs/node/commit/0331b3fdd3)] - **fs,win**: fix readdir for named pipe (Hüseyin Açacak) [#56110](https://github.com/nodejs/node/pull/56110)
* \[[`79152b54e9`](https://github.com/nodejs/node/commit/79152b54e9)] - **http**: add setDefaultHeaders option to http.request (Tim Perry) [#56112](https://github.com/nodejs/node/pull/56112)
* \[[`19782855a8`](https://github.com/nodejs/node/commit/19782855a8)] - **http**: don't emit error after destroy (Robert Nagy) [#55457](https://github.com/nodejs/node/pull/55457)
* \[[`8494512c17`](https://github.com/nodejs/node/commit/8494512c17)] - **http2**: remove duplicate codeblock (Vitaly Aminev) [#55915](https://github.com/nodejs/node/pull/55915)
* \[[`d2f82223d1`](https://github.com/nodejs/node/commit/d2f82223d1)] - **http2**: support ALPNCallback option (ZYSzys) [#56187](https://github.com/nodejs/node/pull/56187)
* \[[`2616f1247a`](https://github.com/nodejs/node/commit/2616f1247a)] - **http2**: fix memory leak caused by premature listener removing (ywave620) [#55966](https://github.com/nodejs/node/pull/55966)
* \[[`598fe048f2`](https://github.com/nodejs/node/commit/598fe048f2)] - **lib**: remove redundant global regexps (Gürgün Dayıoğlu) [#56182](https://github.com/nodejs/node/pull/56182)
* \[[`a3c8739530`](https://github.com/nodejs/node/commit/a3c8739530)] - **lib**: clean up persisted signals when they are settled (Edigleysson Silva (Edy)) [#56001](https://github.com/nodejs/node/pull/56001)
* \[[`11144ab158`](https://github.com/nodejs/node/commit/11144ab158)] - **lib**: handle Float16Array in node:v8 serdes (Bartek Iwańczuk) [#55996](https://github.com/nodejs/node/pull/55996)
* \[[`81c94a32e4`](https://github.com/nodejs/node/commit/81c94a32e4)] - **lib**: disable default memory leak warning for AbortSignal (Lenz Weber-Tronic) [#55816](https://github.com/nodejs/node/pull/55816)
* \[[`68dda61420`](https://github.com/nodejs/node/commit/68dda61420)] - **lib**: add validation for options in compileFunction (Taejin Kim) [#56023](https://github.com/nodejs/node/pull/56023)
* \[[`d2007aec28`](https://github.com/nodejs/node/commit/d2007aec28)] - **lib**: fix `fs.readdir` recursive async (Rafael Gonzaga) [#56041](https://github.com/nodejs/node/pull/56041)
* \[[`0571d5556f`](https://github.com/nodejs/node/commit/0571d5556f)] - **lib**: avoid excluding symlinks in recursive fs.readdir with filetypes (Juan José) [#55714](https://github.com/nodejs/node/pull/55714)
* \[[`843943d0ce`](https://github.com/nodejs/node/commit/843943d0ce)] - **meta**: bump github/codeql-action from 3.27.0 to 3.27.5 (dependabot\[bot]) [#56103](https://github.com/nodejs/node/pull/56103)
* \[[`1529027f03`](https://github.com/nodejs/node/commit/1529027f03)] - **meta**: bump actions/checkout from 4.1.7 to 4.2.2 (dependabot\[bot]) [#56102](https://github.com/nodejs/node/pull/56102)
* \[[`8e265de9f5`](https://github.com/nodejs/node/commit/8e265de9f5)] - **meta**: bump step-security/harden-runner from 2.10.1 to 2.10.2 (dependabot\[bot]) [#56101](https://github.com/nodejs/node/pull/56101)
* \[[`0fba3a3b9b`](https://github.com/nodejs/node/commit/0fba3a3b9b)] - **meta**: bump actions/setup-node from 4.0.3 to 4.1.0 (dependabot\[bot]) [#56100](https://github.com/nodejs/node/pull/56100)
* \[[`2e3fdfdb19`](https://github.com/nodejs/node/commit/2e3fdfdb19)] - **meta**: add releasers as CODEOWNERS to proposal action (Rafael Gonzaga) [#56043](https://github.com/nodejs/node/pull/56043)
* \[[`7cbe3de1d8`](https://github.com/nodejs/node/commit/7cbe3de1d8)] - **(SEMVER-MINOR)** **module**: only emit require(esm) warning under --trace-require-module (Joyee Cheung) [#56194](https://github.com/nodejs/node/pull/56194)
* \[[`8a5429c9b3`](https://github.com/nodejs/node/commit/8a5429c9b3)] - **module**: prevent main thread exiting before esm worker ends (Shima Ryuhei) [#56183](https://github.com/nodejs/node/pull/56183)
* \[[`6575b76042`](https://github.com/nodejs/node/commit/6575b76042)] - **(SEMVER-MINOR)** **module**: add module.stripTypeScriptTypes (Marco Ippolito) [#55282](https://github.com/nodejs/node/pull/55282)
* \[[`0794861bc3`](https://github.com/nodejs/node/commit/0794861bc3)] - **module**: simplify ts under node\_modules check (Marco Ippolito) [#55440](https://github.com/nodejs/node/pull/55440)
* \[[`28a11adf14`](https://github.com/nodejs/node/commit/28a11adf14)] - **module**: mark evaluation rejection in require(esm) as handled (Joyee Cheung) [#56122](https://github.com/nodejs/node/pull/56122)
* \[[`bacfe6d5c9`](https://github.com/nodejs/node/commit/bacfe6d5c9)] - **(SEMVER-MINOR)** **net**: support blocklist in net.connect (theanarkh) [#56075](https://github.com/nodejs/node/pull/56075)
* \[[`566f0a1d25`](https://github.com/nodejs/node/commit/566f0a1d25)] - **(SEMVER-MINOR)** **net**: add SocketAddress.parse (James M Snell) [#56076](https://github.com/nodejs/node/pull/56076)
* \[[`ed7eab1421`](https://github.com/nodejs/node/commit/ed7eab1421)] - **(SEMVER-MINOR)** **net**: add net.BlockList.isBlockList(value) (James M Snell) [#56078](https://github.com/nodejs/node/pull/56078)
* \[[`b47888d390`](https://github.com/nodejs/node/commit/b47888d390)] - **(SEMVER-MINOR)** **net**: support blocklist for net.Server (theanarkh) [#56079](https://github.com/nodejs/node/pull/56079)
* \[[`481770a38f`](https://github.com/nodejs/node/commit/481770a38f)] - **node-api**: allow napi\_delete\_reference in finalizers (Chengzhong Wu) [#55620](https://github.com/nodejs/node/pull/55620)
* \[[`2beb4f1f8c`](https://github.com/nodejs/node/commit/2beb4f1f8c)] - **permission**: ignore internalModuleStat on module loading (Rafael Gonzaga) [#55797](https://github.com/nodejs/node/pull/55797)
* \[[`ea4891856d`](https://github.com/nodejs/node/commit/ea4891856d)] - **(SEMVER-MINOR)** **process**: deprecate `features.{ipv6,uv}` and `features.tls_*` (René) [#55545](https://github.com/nodejs/node/pull/55545)
* \[[`c907b2f358`](https://github.com/nodejs/node/commit/c907b2f358)] - **quic**: update more QUIC implementation (James M Snell) [#55986](https://github.com/nodejs/node/pull/55986)
* \[[`43c25e2e0d`](https://github.com/nodejs/node/commit/43c25e2e0d)] - **quic**: multiple updates to quic impl (James M Snell) [#55971](https://github.com/nodejs/node/pull/55971)
* \[[`01eb308f26`](https://github.com/nodejs/node/commit/01eb308f26)] - **(SEMVER-MINOR)** **report**: fix typos in report keys and bump the version (Yuan-Ming Hsu) [#56068](https://github.com/nodejs/node/pull/56068)
* \[[`1cfa31fb82`](https://github.com/nodejs/node/commit/1cfa31fb82)] - **sea**: only assert snapshot main function for main threads (Joyee Cheung) [#56120](https://github.com/nodejs/node/pull/56120)
* \[[`97c38352d0`](https://github.com/nodejs/node/commit/97c38352d0)] - **(SEMVER-MINOR)** **sqlite**: aggregate constants in a single property (Edigleysson Silva (Edy)) [#56213](https://github.com/nodejs/node/pull/56213)
* \[[`2268c1ea8b`](https://github.com/nodejs/node/commit/2268c1ea8b)] - **sqlite**: add support for custom functions (Colin Ihrig) [#55985](https://github.com/nodejs/node/pull/55985)
* \[[`f5c6955722`](https://github.com/nodejs/node/commit/f5c6955722)] - **sqlite**: support `db.loadExtension` (Alex Yang) [#53900](https://github.com/nodejs/node/pull/53900)
* \[[`9a60bea6b7`](https://github.com/nodejs/node/commit/9a60bea6b7)] - **sqlite**: deps include `sqlite3ext.h` (Alex Yang) [#56010](https://github.com/nodejs/node/pull/56010)
* \[[`b4041e554a`](https://github.com/nodejs/node/commit/b4041e554a)] - **(SEMVER-MINOR)** **sqlite**: add `StatementSync.prototype.iterate` method (tpoisseau) [#54213](https://github.com/nodejs/node/pull/54213)
* \[[`2889e8da04`](https://github.com/nodejs/node/commit/2889e8da04)] - **src**: fix outdated js2c.cc references (Chengzhong Wu) [#56133](https://github.com/nodejs/node/pull/56133)
* \[[`5ce020b0c9`](https://github.com/nodejs/node/commit/5ce020b0c9)] - **src**: use spaceship operator in SocketAddress (James M Snell) [#56059](https://github.com/nodejs/node/pull/56059)
* \[[`a32fa30847`](https://github.com/nodejs/node/commit/a32fa30847)] - **src**: add missing qualifiers to env.cc (Yagiz Nizipli) [#56062](https://github.com/nodejs/node/pull/56062)
* \[[`974b7b61ef`](https://github.com/nodejs/node/commit/974b7b61ef)] - **src**: use std::string\_view for process emit fns (Yagiz Nizipli) [#56086](https://github.com/nodejs/node/pull/56086)
* \[[`4559fac862`](https://github.com/nodejs/node/commit/4559fac862)] - **src**: remove dead code in async\_wrap (Gerhard Stöbich) [#56065](https://github.com/nodejs/node/pull/56065)
* \[[`e42e4b20be`](https://github.com/nodejs/node/commit/e42e4b20be)] - **src**: avoid copy on getV8FastApiCallCount (Yagiz Nizipli) [#56081](https://github.com/nodejs/node/pull/56081)
* \[[`c188660e8b`](https://github.com/nodejs/node/commit/c188660e8b)] - **src**: fix check fd (theanarkh) [#56000](https://github.com/nodejs/node/pull/56000)
* \[[`d894cb76ff`](https://github.com/nodejs/node/commit/d894cb76ff)] - **src**: safely remove the last line from dotenv (Shima Ryuhei) [#55982](https://github.com/nodejs/node/pull/55982)
* \[[`2ca9f4b65a`](https://github.com/nodejs/node/commit/2ca9f4b65a)] - **src**: fix kill signal on Windows (Hüseyin Açacak) [#55514](https://github.com/nodejs/node/pull/55514)
* \[[`2e3ca1bbdd`](https://github.com/nodejs/node/commit/2e3ca1bbdd)] - **(SEMVER-MINOR)** **src**: add cli option to preserve env vars on dr (Rafael Gonzaga) [#55697](https://github.com/nodejs/node/pull/55697)
* \[[`359fff1c4e`](https://github.com/nodejs/node/commit/359fff1c4e)] - **src,build**: add no user defined deduction guides of CTAD check (Chengzhong Wu) [#56071](https://github.com/nodejs/node/pull/56071)
* \[[`57bb983215`](https://github.com/nodejs/node/commit/57bb983215)] - **(SEMVER-MINOR)** **src,lib**: stabilize permission model (Rafael Gonzaga) [#56201](https://github.com/nodejs/node/pull/56201)
* \[[`d352b0465a`](https://github.com/nodejs/node/commit/d352b0465a)] - **stream**: commit pull-into descriptors after filling from queue (Mattias Buelens) [#56072](https://github.com/nodejs/node/pull/56072)
* \[[`eef9bd1bf6`](https://github.com/nodejs/node/commit/eef9bd1bf6)] - **test**: remove test-sqlite-statement-sync flaky designation (Luigi Pinca) [#56051](https://github.com/nodejs/node/pull/56051)
* \[[`8718135a5d`](https://github.com/nodejs/node/commit/8718135a5d)] - **test**: use --permission over --experimental-permission (Rafael Gonzaga) [#56239](https://github.com/nodejs/node/pull/56239)
* \[[`9c68d4f180`](https://github.com/nodejs/node/commit/9c68d4f180)] - **test**: remove exludes for sea tests on PPC (Michael Dawson) [#56217](https://github.com/nodejs/node/pull/56217)
* \[[`c5d0472968`](https://github.com/nodejs/node/commit/c5d0472968)] - **test**: fix test-abortsignal-drop-settled-signals flakiness (Edigleysson Silva (Edy)) [#56197](https://github.com/nodejs/node/pull/56197)
* \[[`4adf518689`](https://github.com/nodejs/node/commit/4adf518689)] - **test**: move localizationd data from `test-icu-env` to external file (Livia Medeiros) [#55618](https://github.com/nodejs/node/pull/55618)
* \[[`02383b4267`](https://github.com/nodejs/node/commit/02383b4267)] - **test**: update WPT for url to 6fa3fe8a92 (Node.js GitHub Bot) [#56136](https://github.com/nodejs/node/pull/56136)
* \[[`0e24eebf24`](https://github.com/nodejs/node/commit/0e24eebf24)] - **test**: remove `hasOpenSSL3x` utils (Antoine du Hamel) [#56164](https://github.com/nodejs/node/pull/56164)
* \[[`381e705385`](https://github.com/nodejs/node/commit/381e705385)] - **test**: update streams wpt (Mattias Buelens) [#56072](https://github.com/nodejs/node/pull/56072)
* \[[`ad107ca0d9`](https://github.com/nodejs/node/commit/ad107ca0d9)] - **test**: remove test-fs-utimes flaky designation (Luigi Pinca) [#56052](https://github.com/nodejs/node/pull/56052)
* \[[`e15c5dab79`](https://github.com/nodejs/node/commit/e15c5dab79)] - **test**: ensure `cli.md` is in alphabetical order (Antoine du Hamel) [#56025](https://github.com/nodejs/node/pull/56025)
* \[[`d0302e7d2d`](https://github.com/nodejs/node/commit/d0302e7d2d)] - **test**: update WPT for WebCryptoAPI to 3e3374efde (Node.js GitHub Bot) [#56093](https://github.com/nodejs/node/pull/56093)
* \[[`a0b1e8f400`](https://github.com/nodejs/node/commit/a0b1e8f400)] - **test**: update WPT for WebCryptoAPI to 76dfa54e5d (Node.js GitHub Bot) [#56093](https://github.com/nodejs/node/pull/56093)
* \[[`211f058a12`](https://github.com/nodejs/node/commit/211f058a12)] - **test**: move test-worker-arraybuffer-zerofill to parallel (Luigi Pinca) [#56053](https://github.com/nodejs/node/pull/56053)
* \[[`c52bc5d71c`](https://github.com/nodejs/node/commit/c52bc5d71c)] - **test**: update WPT for url to 67880a4eb83ca9aa732eec4b35a1971ff5bf37ff (Node.js GitHub Bot) [#55999](https://github.com/nodejs/node/pull/55999)
* \[[`1a78bde8d4`](https://github.com/nodejs/node/commit/1a78bde8d4)] - **test**: make HTTP/1.0 connection test more robust (Arne Keller) [#55959](https://github.com/nodejs/node/pull/55959)
* \[[`ff7b1445a0`](https://github.com/nodejs/node/commit/ff7b1445a0)] - **test**: convert readdir test to use test runner (Thomas Chetwin) [#55750](https://github.com/nodejs/node/pull/55750)
* \[[`b296b5a4e4`](https://github.com/nodejs/node/commit/b296b5a4e4)] - **test**: make x509 crypto tests work with BoringSSL (Shelley Vohr) [#55927](https://github.com/nodejs/node/pull/55927)
* \[[`97458ad74b`](https://github.com/nodejs/node/commit/97458ad74b)] - **test**: fix determining lower priority (Livia Medeiros) [#55908](https://github.com/nodejs/node/pull/55908)
* \[[`bb4aa7a296`](https://github.com/nodejs/node/commit/bb4aa7a296)] - **test,crypto**: update WebCryptoAPI WPT (Filip Skokan) [#55997](https://github.com/nodejs/node/pull/55997)
* \[[`fb98fa4967`](https://github.com/nodejs/node/commit/fb98fa4967)] - **test\_runner**: refactor Promise chain in run() (Colin Ihrig) [#55958](https://github.com/nodejs/node/pull/55958)
* \[[`18c94961f8`](https://github.com/nodejs/node/commit/18c94961f8)] - **test\_runner**: refactor build Promise in Suite() (Colin Ihrig) [#55958](https://github.com/nodejs/node/pull/55958)
* \[[`bf3967fd3a`](https://github.com/nodejs/node/commit/bf3967fd3a)] - **test\_runner**: simplify hook running logic (Colin Ihrig) [#55963](https://github.com/nodejs/node/pull/55963)
* \[[`8c065dc61e`](https://github.com/nodejs/node/commit/8c065dc61e)] - **test\_runner**: mark context.plan() as stable (Colin Ihrig) [#55895](https://github.com/nodejs/node/pull/55895)
* \[[`8ff082cf48`](https://github.com/nodejs/node/commit/8ff082cf48)] - **test\_runner**: mark snapshot testing as stable (Colin Ihrig) [#55897](https://github.com/nodejs/node/pull/55897)
* \[[`7ae125cef4`](https://github.com/nodejs/node/commit/7ae125cef4)] - **tools**: fix `node:` enforcement for docs (Antoine du Hamel) [#56284](https://github.com/nodejs/node/pull/56284)
* \[[`0b489116a3`](https://github.com/nodejs/node/commit/0b489116a3)] - **tools**: update github\_reporter to 1.7.2 (Node.js GitHub Bot) [#56205](https://github.com/nodejs/node/pull/56205)
* \[[`5306819fac`](https://github.com/nodejs/node/commit/5306819fac)] - **tools**: add REPLACEME check to workflow (Mert Can Altin) [#56251](https://github.com/nodejs/node/pull/56251)
* \[[`4e3cab44cb`](https://github.com/nodejs/node/commit/4e3cab44cb)] - **tools**: use `github.actor` instead of bot username for release proposals (Antoine du Hamel) [#56232](https://github.com/nodejs/node/pull/56232)
* \[[`3e8938463a`](https://github.com/nodejs/node/commit/3e8938463a)] - _**Revert**_ "**tools**: disable automated libuv updates" (Luigi Pinca) [#56223](https://github.com/nodejs/node/pull/56223)
* \[[`98ea499e36`](https://github.com/nodejs/node/commit/98ea499e36)] - **tools**: update gyp-next to 0.19.1 (Anna Henningsen) [#56111](https://github.com/nodejs/node/pull/56111)
* \[[`2e76cd2a8b`](https://github.com/nodejs/node/commit/2e76cd2a8b)] - **tools**: fix release proposal linter to support more than 1 folk preparing (Antoine du Hamel) [#56203](https://github.com/nodejs/node/pull/56203)
* \[[`9fa0e41665`](https://github.com/nodejs/node/commit/9fa0e41665)] - **tools**: enable linter for `tools/icu/**` (Livia Medeiros) [#56176](https://github.com/nodejs/node/pull/56176)
* \[[`d6e1efcc59`](https://github.com/nodejs/node/commit/d6e1efcc59)] - **tools**: use commit title as PR title when creating release proposal (Antoine du Hamel) [#56165](https://github.com/nodejs/node/pull/56165)
* \[[`a88e4ce55e`](https://github.com/nodejs/node/commit/a88e4ce55e)] - **tools**: update gyp-next to 0.19.0 (Node.js GitHub Bot) [#56158](https://github.com/nodejs/node/pull/56158)
* \[[`bd0760efbc`](https://github.com/nodejs/node/commit/bd0760efbc)] - **tools**: bump the eslint group in /tools/eslint with 4 updates (dependabot\[bot]) [#56099](https://github.com/nodejs/node/pull/56099)
* \[[`c5b1cf4b12`](https://github.com/nodejs/node/commit/c5b1cf4b12)] - **tools**: improve release proposal PR opening (Antoine du Hamel) [#56161](https://github.com/nodejs/node/pull/56161)
* \[[`12baefb13d`](https://github.com/nodejs/node/commit/12baefb13d)] - **tools**: update `create-release-proposal` workflow (Antoine du Hamel) [#56054](https://github.com/nodejs/node/pull/56054)
* \[[`e6e1495f1a`](https://github.com/nodejs/node/commit/e6e1495f1a)] - **tools**: fix update-undici script (Michaël Zasso) [#56069](https://github.com/nodejs/node/pull/56069)
* \[[`ed635c90da`](https://github.com/nodejs/node/commit/ed635c90da)] - **tools**: allow dispatch of `tools.yml` from forks (Antoine du Hamel) [#56008](https://github.com/nodejs/node/pull/56008)
* \[[`1e628d1f37`](https://github.com/nodejs/node/commit/1e628d1f37)] - **tools**: fix nghttp3 updater script (Antoine du Hamel) [#56007](https://github.com/nodejs/node/pull/56007)
* \[[`1af3599b7e`](https://github.com/nodejs/node/commit/1af3599b7e)] - **tools**: filter release keys to reduce interactivity (Antoine du Hamel) [#55950](https://github.com/nodejs/node/pull/55950)
* \[[`1893be4a9c`](https://github.com/nodejs/node/commit/1893be4a9c)] - **tools**: update WPT updater (Antoine du Hamel) [#56003](https://github.com/nodejs/node/pull/56003)
* \[[`f89bd2ba8a`](https://github.com/nodejs/node/commit/f89bd2ba8a)] - **tools**: add WPT updater for specific subsystems (Mert Can Altin) [#54460](https://github.com/nodejs/node/pull/54460)
* \[[`61901372d5`](https://github.com/nodejs/node/commit/61901372d5)] - **tools**: use tokenless Codecov uploads (Michaël Zasso) [#55943](https://github.com/nodejs/node/pull/55943)
* \[[`312bb4dff8`](https://github.com/nodejs/node/commit/312bb4dff8)] - **tools**: lint js in `doc/**/*.md` (Livia Medeiros) [#55904](https://github.com/nodejs/node/pull/55904)
* \[[`7b476f637c`](https://github.com/nodejs/node/commit/7b476f637c)] - **tools**: add linter for release commit proposals (Antoine du Hamel) [#55923](https://github.com/nodejs/node/pull/55923)
* \[[`22d7017191`](https://github.com/nodejs/node/commit/22d7017191)] - **tools**: fix riscv64 build failed (Lu Yahan) [#52888](https://github.com/nodejs/node/pull/52888)
* \[[`f4f777f4d2`](https://github.com/nodejs/node/commit/f4f777f4d2)] - **tools**: bump cross-spawn from 7.0.3 to 7.0.5 in /tools/eslint (dependabot\[bot]) [#55894](https://github.com/nodejs/node/pull/55894)
* \[[`a648e4c44a`](https://github.com/nodejs/node/commit/a648e4c44a)] - **util**: harden more built-in classes against prototype pollution (Antoine du Hamel) [#56225](https://github.com/nodejs/node/pull/56225)
* \[[`4a1b51b5a9`](https://github.com/nodejs/node/commit/4a1b51b5a9)] - **util**: fix Latin1 decoding to return string output (Mert Can Altin) [#56222](https://github.com/nodejs/node/pull/56222)
* \[[`9e98e86604`](https://github.com/nodejs/node/commit/9e98e86604)] - **util**: do not rely on mutable `Object` and `Function`' `constructor` prop (Antoine du Hamel) [#56188](https://github.com/nodejs/node/pull/56188)
* \[[`374eb415fd`](https://github.com/nodejs/node/commit/374eb415fd)] - **util**: add fast path for Latin1 decoding (Mert Can Altin) [#55275](https://github.com/nodejs/node/pull/55275)
* \[[`bcfe9c80fc`](https://github.com/nodejs/node/commit/bcfe9c80fc)] - **(SEMVER-MINOR)** **util**: add sourcemap support to getCallSites (Marco Ippolito) [#55589](https://github.com/nodejs/node/pull/55589)
* \[[`2aa77c8a8f`](https://github.com/nodejs/node/commit/2aa77c8a8f)] - **v8,tools**: expose experimental wasm revectorize feature (Yolanda-Chen) [#54896](https://github.com/nodejs/node/pull/54896)
* \[[`bfd11d7661`](https://github.com/nodejs/node/commit/bfd11d7661)] - **worker**: fix crash when a worker joins after exit (Stephen Belanger) [#56191](https://github.com/nodejs/node/pull/56191)

<a id="22.12.0"></a>

## 2024-12-03, Version 22.12.0 'Jod' (LTS), @ruyadorno

### Notable Changes

#### require(esm) is now enabled by default

Support for loading native ES modules using require() had been available on v20.x and v22.x under the command line flag --experimental-require-module, and available by default on v23.x. In this release, it is now no longer behind a flag on v22.x.

This feature is still experimental, and we are looking for user feedback to make more final tweaks before fully stabilizing it. For this reason, on v23.x, when the Node.js instance encounters a native ES module in require() for the first time, it will emit an experimental warning unless `require()` comes from a path that contains `node_modules`. If there happens to be any regressions caused by this feature, users can report it to the Node.js issue tracker. Meanwhile this feature can also be disabled using `--no-experimental-require-module` as a workaround.

With this feature enabled, Node.js will no longer throw `ERR_REQUIRE_ESM` if `require()` is used to load a ES module. It can, however, throw `ERR_REQUIRE_ASYNC_MODULE` if the ES module being loaded or its dependencies contain top-level `await`. When the ES module is loaded successfully by `require()`, the returned object will either be a ES module namespace object similar to what's returned by `import()`, or what gets exported as `"module.exports"` in the ES module.

Users can check `process.features.require_module` to see whether `require(esm)` is enabled in the current Node.js instance. For packages, the `"module-sync"` exports condition can be used as a way to detect `require(esm)` support in the current Node.js instance and allow both `require()` and `import` to load the same native ES module. See [the documentation](https://nodejs.org/docs/latest/api/modules.html#loading-ecmascript-modules-using-require) for more details about this feature.

Contributed by Joyee Cheung in [#55085](https://github.com/nodejs/node/pull/55085)

#### Added resizable `ArrayBuffer` support in `Buffer`

When a `Buffer` is created using a resizable `ArrayBuffer`, the `Buffer` length will now correctly change as the underlying `ArrayBuffer` size is changed.

```js
const ab = new ArrayBuffer(10, { maxByteLength: 20 });
const buffer = Buffer.from(ab);
console.log(buffer.byteLength); // 10
ab.resize(15);
console.log(buffer.byteLength); // 15
ab.resize(5);
console.log(buffer.byteLength); // 5
```

Contributed by James Snell in [#55377](https://github.com/nodejs/node/pull/55377)

#### Update root certificates to NSS 3.104

This is the version of NSS that shipped in Firefox 131.0 on 2024-10-01.

Certificates added:

* FIRMAPROFESIONAL CA ROOT-A WEB
* TWCA CYBER Root CA
* SecureSign Root CA12
* SecureSign Root CA14
* SecureSign Root CA15

Contributed by Richard Lau in [#55681](https://github.com/nodejs/node/pull/55681)

#### SQLite Session Extension

Basic support for the [SQLite Session Extension](https://www.sqlite.org/sessionintro.html)
got added to the experimental `node:sqlite` module.

```js
const sourceDb = new DatabaseSync(':memory:');
const targetDb = new DatabaseSync(':memory:');

sourceDb.exec('CREATE TABLE data(key INTEGER PRIMARY KEY, value TEXT)');
targetDb.exec('CREATE TABLE data(key INTEGER PRIMARY KEY, value TEXT)');

const session = sourceDb.createSession();

const insert = sourceDb.prepare('INSERT INTO data (key, value) VALUES (?, ?)');
insert.run(1, 'hello');
insert.run(2, 'world');

const changeset = session.changeset();
targetDb.applyChangeset(changeset);
// Now that the changeset has been applied, targetDb contains the same data as sourceDb.
```

Of note to distributors when dynamically linking with SQLite (using the `--shared-sqlite`
flag): compiling SQLite with `SQLITE_ENABLE_SESSION` and `SQLITE_ENABLE_PREUPDATE_HOOK`
defines is now required.

Contributed by Bart Louwers in [#54181](https://github.com/nodejs/node/pull/54181).

#### Other Notable Changes

* \[[`4920869935`](https://github.com/nodejs/node/commit/4920869935)] - **(SEMVER-MINOR)** **assert**: make assertion\_error use Myers diff algorithm (Giovanni Bucci) [#54862](https://github.com/nodejs/node/pull/54862)
* \[[`ccffd3b819`](https://github.com/nodejs/node/commit/ccffd3b819)] - **doc**: enforce strict policy to semver-major releases (Rafael Gonzaga) [#55732](https://github.com/nodejs/node/pull/55732)
* \[[`acc6806900`](https://github.com/nodejs/node/commit/acc6806900)] - **doc**: add jazelly to collaborators (Jason Zhang) [#55531](https://github.com/nodejs/node/pull/55531)
* \[[`88d91e8bc2`](https://github.com/nodejs/node/commit/88d91e8bc2)] - **esm**: mark import attributes and JSON module as stable (Nicolò Ribaudo) [#55333](https://github.com/nodejs/node/pull/55333)
* \[[`98bfc7dce5`](https://github.com/nodejs/node/commit/98bfc7dce5)] - **(SEMVER-MINOR)** **http**: add diagnostic channel `http.client.request.created` (Marco Ippolito) [#55586](https://github.com/nodejs/node/pull/55586)
* \[[`337f61fb25`](https://github.com/nodejs/node/commit/337f61fb25)] - **(SEMVER-MINOR)** **lib**: add UV\_UDP\_REUSEPORT for udp (theanarkh) [#55403](https://github.com/nodejs/node/pull/55403)
* \[[`1628c48ad6`](https://github.com/nodejs/node/commit/1628c48ad6)] - **(SEMVER-MINOR)** **net**: add UV\_TCP\_REUSEPORT for tcp (theanarkh) [#55408](https://github.com/nodejs/node/pull/55408)
* \[[`457e73f4c9`](https://github.com/nodejs/node/commit/457e73f4c9)] - **(SEMVER-MINOR)** **sqlite**: add support for SQLite Session Extension (Bart Louwers) [#54181](https://github.com/nodejs/node/pull/54181)

### Commits

* \[[`f6885e1c68`](https://github.com/nodejs/node/commit/f6885e1c68)] - **assert**: fix the string length check for printing the simple diff (Giovanni Bucci) [#55474](https://github.com/nodejs/node/pull/55474)
* \[[`907484f04d`](https://github.com/nodejs/node/commit/907484f04d)] - **assert**: fix deepEqual always return true on URL (Xuguang Mei) [#50853](https://github.com/nodejs/node/pull/50853)
* \[[`301844e249`](https://github.com/nodejs/node/commit/301844e249)] - **assert**: differentiate cases where `cause` is `undefined` or missing (Antoine du Hamel) [#55738](https://github.com/nodejs/node/pull/55738)
* \[[`89ccd3e3f4`](https://github.com/nodejs/node/commit/89ccd3e3f4)] - **assert**: fix `deepStrictEqual` on errors when `cause` is not undefined (Edigleysson Silva (Edy)) [#55406](https://github.com/nodejs/node/pull/55406)
* \[[`4920869935`](https://github.com/nodejs/node/commit/4920869935)] - **(SEMVER-MINOR)** **assert**: make assertion\_error use Myers diff algorithm (Giovanni Bucci) [#54862](https://github.com/nodejs/node/pull/54862)
* \[[`c67aec368e`](https://github.com/nodejs/node/commit/c67aec368e)] - **benchmark**: add `test-reporters` (Aviv Keller) [#55757](https://github.com/nodejs/node/pull/55757)
* \[[`49774cc2c0`](https://github.com/nodejs/node/commit/49774cc2c0)] - **benchmark**: add `test_runner/mock-fn` (Aviv Keller) [#55771](https://github.com/nodejs/node/pull/55771)
* \[[`4caaeb47b2`](https://github.com/nodejs/node/commit/4caaeb47b2)] - **benchmark**: add nodeTiming.uvmetricsinfo bench (RafaelGSS) [#55614](https://github.com/nodejs/node/pull/55614)
* \[[`cac58564a1`](https://github.com/nodejs/node/commit/cac58564a1)] - **benchmark**: add --runs support to run.js (Rafael Gonzaga) [#55158](https://github.com/nodejs/node/pull/55158)
* \[[`5c3ee886fc`](https://github.com/nodejs/node/commit/5c3ee886fc)] - **benchmark**: adjust byte size for buffer-copy (Rafael Gonzaga) [#55295](https://github.com/nodejs/node/pull/55295)
* \[[`6023e1bdb2`](https://github.com/nodejs/node/commit/6023e1bdb2)] - **(SEMVER-MINOR)** **buffer**: make Buffer work with resizable ArrayBuffer (James M Snell) [#55377](https://github.com/nodejs/node/pull/55377)
* \[[`a6c00c2204`](https://github.com/nodejs/node/commit/a6c00c2204)] - **build**: add create release proposal action (Rafael Gonzaga) [#55690](https://github.com/nodejs/node/pull/55690)
* \[[`b4e413933b`](https://github.com/nodejs/node/commit/b4e413933b)] - **build**: implement node\_use\_amaro flag in GN build (Cheng) [#55798](https://github.com/nodejs/node/pull/55798)
* \[[`d1db202d4a`](https://github.com/nodejs/node/commit/d1db202d4a)] - **build**: apply cpp linting and formatting to ncrypto (Aviv Keller) [#55362](https://github.com/nodejs/node/pull/55362)
* \[[`8c670496da`](https://github.com/nodejs/node/commit/8c670496da)] - **build**: use rclone instead of aws CLI (Michaël Zasso) [#55617](https://github.com/nodejs/node/pull/55617)
* \[[`827e2065bd`](https://github.com/nodejs/node/commit/827e2065bd)] - **build**: stop pre-compiling `lint-md` (Aviv Keller) [#55266](https://github.com/nodejs/node/pull/55266)
* \[[`c3ca978d9c`](https://github.com/nodejs/node/commit/c3ca978d9c)] - **build**: fix building with system icu 76 (Michael Cho) [#55563](https://github.com/nodejs/node/pull/55563)
* \[[`23e3287bbe`](https://github.com/nodejs/node/commit/23e3287bbe)] - **build**: fix GN arg used in generate\_config\_gypi.py (Shelley Vohr) [#55530](https://github.com/nodejs/node/pull/55530)
* \[[`2b561abb0d`](https://github.com/nodejs/node/commit/2b561abb0d)] - **build**: fix GN build for sqlite and nghttp2 (Shelley Vohr) [#55529](https://github.com/nodejs/node/pull/55529)
* \[[`7008f29d79`](https://github.com/nodejs/node/commit/7008f29d79)] - **build**: fix GN build for cares/uv deps (Cheng) [#55477](https://github.com/nodejs/node/pull/55477)
* \[[`6ee94a394f`](https://github.com/nodejs/node/commit/6ee94a394f)] - **build**: fix uninstall script for AIX 7.1 (Cloorc) [#55438](https://github.com/nodejs/node/pull/55438)
* \[[`edbbd4a374`](https://github.com/nodejs/node/commit/edbbd4a374)] - **build**: conditionally compile bundled sqlite (Richard Lau) [#55409](https://github.com/nodejs/node/pull/55409)
* \[[`3d8e3a657c`](https://github.com/nodejs/node/commit/3d8e3a657c)] - **build**: tidy up cares.gyp (Richard Lau) [#55445](https://github.com/nodejs/node/pull/55445)
* \[[`f0c12e8fcb`](https://github.com/nodejs/node/commit/f0c12e8fcb)] - **build**: synchronize list of c-ares source files (Richard Lau) [#55445](https://github.com/nodejs/node/pull/55445)
* \[[`8daa8a62f8`](https://github.com/nodejs/node/commit/8daa8a62f8)] - **build**: fix path concatenation (Mohammed Keyvanzadeh) [#55387](https://github.com/nodejs/node/pull/55387)
* \[[`12faf0466e`](https://github.com/nodejs/node/commit/12faf0466e)] - **build**: fix make errors that occur in Makefile (minkyu\_kim) [#55287](https://github.com/nodejs/node/pull/55287)
* \[[`a21be0294d`](https://github.com/nodejs/node/commit/a21be0294d)] - **build,win**: enable pch for clang-cl (Stefan Stojanovic) [#55249](https://github.com/nodejs/node/pull/55249)
* \[[`7ed058cd00`](https://github.com/nodejs/node/commit/7ed058cd00)] - **cli**: add `--heap-prof` flag available to `NODE_OPTIONS` (Juan José) [#54259](https://github.com/nodejs/node/pull/54259)
* \[[`c26b1bfe6a`](https://github.com/nodejs/node/commit/c26b1bfe6a)] - **crypto**: allow length=0 for HKDF and PBKDF2 in SubtleCrypto.deriveBits (Filip Skokan) [#55866](https://github.com/nodejs/node/pull/55866)
* \[[`a1201d0392`](https://github.com/nodejs/node/commit/a1201d0392)] - **crypto**: update root certificates to NSS 3.104 (Richard Lau) [#55681](https://github.com/nodejs/node/pull/55681)
* \[[`20483aab7a`](https://github.com/nodejs/node/commit/20483aab7a)] - **crypto**: fix `RSA_PKCS1_PADDING` error message (Richard Lau) [#55629](https://github.com/nodejs/node/pull/55629)
* \[[`d345662d50`](https://github.com/nodejs/node/commit/d345662d50)] - **crypto**: include openssl/rand.h explicitly (Shelley Vohr) [#55425](https://github.com/nodejs/node/pull/55425)
* \[[`166ab3209d`](https://github.com/nodejs/node/commit/166ab3209d)] - **deps**: update simdutf to 5.6.1 (Node.js GitHub Bot) [#55850](https://github.com/nodejs/node/pull/55850)
* \[[`934979e12e`](https://github.com/nodejs/node/commit/934979e12e)] - **deps**: update undici to 6.21.0 (Node.js GitHub Bot) [#55851](https://github.com/nodejs/node/pull/55851)
* \[[`af77f66424`](https://github.com/nodejs/node/commit/af77f66424)] - **deps**: update c-ares to v1.34.3 (Node.js GitHub Bot) [#55803](https://github.com/nodejs/node/pull/55803)
* \[[`948a88d2f4`](https://github.com/nodejs/node/commit/948a88d2f4)] - **deps**: update icu to 76.1 (Node.js GitHub Bot) [#55551](https://github.com/nodejs/node/pull/55551)
* \[[`fa4c58a983`](https://github.com/nodejs/node/commit/fa4c58a983)] - **deps**: update acorn to 8.14.0 (Node.js GitHub Bot) [#55699](https://github.com/nodejs/node/pull/55699)
* \[[`c91155f22e`](https://github.com/nodejs/node/commit/c91155f22e)] - **deps**: update sqlite to 3.47.0 (Node.js GitHub Bot) [#55557](https://github.com/nodejs/node/pull/55557)
* \[[`d1cb7af95c`](https://github.com/nodejs/node/commit/d1cb7af95c)] - **deps**: update amaro to 0.2.0 (Node.js GitHub Bot) [#55601](https://github.com/nodejs/node/pull/55601)
* \[[`655e5600cb`](https://github.com/nodejs/node/commit/655e5600cb)] - **deps**: update nghttp2 to 1.64.0 (Node.js GitHub Bot) [#55559](https://github.com/nodejs/node/pull/55559)
* \[[`992450c469`](https://github.com/nodejs/node/commit/992450c469)] - **deps**: update acorn to 8.13.0 (Node.js GitHub Bot) [#55558](https://github.com/nodejs/node/pull/55558)
* \[[`abd2bd4f64`](https://github.com/nodejs/node/commit/abd2bd4f64)] - **deps**: update undici to 6.20.1 (Node.js GitHub Bot) [#55503](https://github.com/nodejs/node/pull/55503)
* \[[`7dc2c2edad`](https://github.com/nodejs/node/commit/7dc2c2edad)] - **deps**: update googletest to df1544b (Node.js GitHub Bot) [#55465](https://github.com/nodejs/node/pull/55465)
* \[[`fa9329c024`](https://github.com/nodejs/node/commit/fa9329c024)] - **deps**: update c-ares to v1.34.2 (Node.js GitHub Bot) [#55463](https://github.com/nodejs/node/pull/55463)
* \[[`41a2bcd335`](https://github.com/nodejs/node/commit/41a2bcd335)] - **deps**: update ada to 2.9.1 (Node.js GitHub Bot) [#54679](https://github.com/nodejs/node/pull/54679)
* \[[`a3b793defd`](https://github.com/nodejs/node/commit/a3b793defd)] - **deps**: update simdutf to 5.6.0 (Node.js GitHub Bot) [#55379](https://github.com/nodejs/node/pull/55379)
* \[[`551b8f897d`](https://github.com/nodejs/node/commit/551b8f897d)] - **deps**: update c-ares to v1.34.1 (Node.js GitHub Bot) [#55369](https://github.com/nodejs/node/pull/55369)
* \[[`26861eaf4e`](https://github.com/nodejs/node/commit/26861eaf4e)] - _**Revert**_ "**deps**: disable io\_uring support in libuv by default" (Santiago Gimeno) [#55114](https://github.com/nodejs/node/pull/55114)
* \[[`41c50bc15e`](https://github.com/nodejs/node/commit/41c50bc15e)] - **deps**: update libuv to 1.49.1 (Santiago Gimeno) [#55114](https://github.com/nodejs/node/pull/55114)
* \[[`26fcc04084`](https://github.com/nodejs/node/commit/26fcc04084)] - **deps**: update amaro to 0.1.9 (Node.js GitHub Bot) [#55348](https://github.com/nodejs/node/pull/55348)
* \[[`0ee6715921`](https://github.com/nodejs/node/commit/0ee6715921)] - **diagnostics\_channel**: fix unsubscribe during publish (simon-id) [#55116](https://github.com/nodejs/node/pull/55116)
* \[[`bf68733e7f`](https://github.com/nodejs/node/commit/bf68733e7f)] - **dns**: stop using deprecated `ares_query` (Aviv Keller) [#55430](https://github.com/nodejs/node/pull/55430)
* \[[`ef6707eb9b`](https://github.com/nodejs/node/commit/ef6707eb9b)] - **dns**: honor the order option (Luigi Pinca) [#55392](https://github.com/nodejs/node/pull/55392)
* \[[`0f3810f3e5`](https://github.com/nodejs/node/commit/0f3810f3e5)] - **doc**: add added tag and fix typo sqlite.md (Bart Louwers) [#56012](https://github.com/nodejs/node/pull/56012)
* \[[`d1bd0ef1b7`](https://github.com/nodejs/node/commit/d1bd0ef1b7)] - **doc**: remove non-working example (Antoine du Hamel) [#55856](https://github.com/nodejs/node/pull/55856)
* \[[`824ac650ed`](https://github.com/nodejs/node/commit/824ac650ed)] - **doc**: add `node:sqlite` to mandatory `node:` prefix list (翠 / green) [#55846](https://github.com/nodejs/node/pull/55846)
* \[[`b3ea42d887`](https://github.com/nodejs/node/commit/b3ea42d887)] - **doc**: add `-S` flag release preparation example (Antoine du Hamel) [#55836](https://github.com/nodejs/node/pull/55836)
* \[[`0bd5d8b9d9`](https://github.com/nodejs/node/commit/0bd5d8b9d9)] - **doc**: clarify UV\_THREADPOOL\_SIZE env var usage (Preveen P) [#55832](https://github.com/nodejs/node/pull/55832)
* \[[`27b0236a99`](https://github.com/nodejs/node/commit/27b0236a99)] - **doc**: add notable-change mention to sec release (Rafael Gonzaga) [#55830](https://github.com/nodejs/node/pull/55830)
* \[[`476075bada`](https://github.com/nodejs/node/commit/476075bada)] - **doc**: fix history info for `URL.prototype.toJSON` (Antoine du Hamel) [#55818](https://github.com/nodejs/node/pull/55818)
* \[[`2743b7b1d3`](https://github.com/nodejs/node/commit/2743b7b1d3)] - **doc**: correct max-semi-space-size statement (Joe Bowbeer) [#55812](https://github.com/nodejs/node/pull/55812)
* \[[`3013870093`](https://github.com/nodejs/node/commit/3013870093)] - **doc**: update unflag info of `import.meta.resolve` (skyclouds2001) [#55810](https://github.com/nodejs/node/pull/55810)
* \[[`27bcd103e7`](https://github.com/nodejs/node/commit/27bcd103e7)] - **doc**: run license-builder (github-actions\[bot]) [#55813](https://github.com/nodejs/node/pull/55813)
* \[[`72d4b30ead`](https://github.com/nodejs/node/commit/72d4b30ead)] - **doc**: clarify triager role (Gireesh Punathil) [#55775](https://github.com/nodejs/node/pull/55775)
* \[[`a30defe9dd`](https://github.com/nodejs/node/commit/a30defe9dd)] - **doc**: clarify removal of experimental API does not require a deprecation (Antoine du Hamel) [#55746](https://github.com/nodejs/node/pull/55746)
* \[[`ccffd3b819`](https://github.com/nodejs/node/commit/ccffd3b819)] - **doc**: enforce strict policy to semver-major releases (Rafael Gonzaga) [#55732](https://github.com/nodejs/node/pull/55732)
* \[[`b6d2a4e816`](https://github.com/nodejs/node/commit/b6d2a4e816)] - **doc**: add path aliases typescript doc (Carlos Espa) [#55766](https://github.com/nodejs/node/pull/55766)
* \[[`a435affa11`](https://github.com/nodejs/node/commit/a435affa11)] - **doc**: add esm example in `path.md` (Aviv Keller) [#55745](https://github.com/nodejs/node/pull/55745)
* \[[`91443c2711`](https://github.com/nodejs/node/commit/91443c2711)] - **doc**: consistent use of word child process (Gireesh Punathil) [#55654](https://github.com/nodejs/node/pull/55654)
* \[[`83fb0079d4`](https://github.com/nodejs/node/commit/83fb0079d4)] - **doc**: clarity to available addon options (Preveen P) [#55715](https://github.com/nodejs/node/pull/55715)
* \[[`6ca851457a`](https://github.com/nodejs/node/commit/6ca851457a)] - **doc**: update `--max-semi-space-size` description (Joe Bowbeer) [#55495](https://github.com/nodejs/node/pull/55495)
* \[[`e17fffc0ff`](https://github.com/nodejs/node/commit/e17fffc0ff)] - **doc**: broken `PerformanceObserver` code sample (Dom Harrington) [#54227](https://github.com/nodejs/node/pull/54227)
* \[[`8bd5777f0f`](https://github.com/nodejs/node/commit/8bd5777f0f)] - **doc**: add write flag when open file as the demo code's intention (robberfree) [#54626](https://github.com/nodejs/node/pull/54626)
* \[[`f1e0e0ba55`](https://github.com/nodejs/node/commit/f1e0e0ba55)] - **doc**: remove mention of ECDH-ES in crypto.diffieHellman (Filip Skokan) [#55611](https://github.com/nodejs/node/pull/55611)
* \[[`1d60b7ec97`](https://github.com/nodejs/node/commit/1d60b7ec97)] - **doc**: improve c++ embedder API doc (Gireesh Punathil) [#55597](https://github.com/nodejs/node/pull/55597)
* \[[`bbf51d7000`](https://github.com/nodejs/node/commit/bbf51d7000)] - **doc**: capitalize "MIT License" (Aviv Keller) [#55575](https://github.com/nodejs/node/pull/55575)
* \[[`0e69f6d123`](https://github.com/nodejs/node/commit/0e69f6d123)] - **doc**: add suggested tsconfig for type stripping (Marco Ippolito) [#55534](https://github.com/nodejs/node/pull/55534)
* \[[`67beb37f50`](https://github.com/nodejs/node/commit/67beb37f50)] - **doc**: add esm examples to node:string\_decoder (Alfredo González) [#55507](https://github.com/nodejs/node/pull/55507)
* \[[`acc6806900`](https://github.com/nodejs/node/commit/acc6806900)] - **doc**: add jazelly to collaborators (Jason Zhang) [#55531](https://github.com/nodejs/node/pull/55531)
* \[[`a6b3ed54ae`](https://github.com/nodejs/node/commit/a6b3ed54ae)] - **doc**: changed the command used to verify SHASUMS256 (adriancuadrado) [#55420](https://github.com/nodejs/node/pull/55420)
* \[[`0ad7ca4f1d`](https://github.com/nodejs/node/commit/0ad7ca4f1d)] - **doc**: move dual package shipping docs to separate repo (Joyee Cheung) [#55444](https://github.com/nodejs/node/pull/55444)
* \[[`e99a98ddfd`](https://github.com/nodejs/node/commit/e99a98ddfd)] - **doc**: add note about stdio streams in child\_process (Ederin (Ed) Igharoro) [#55322](https://github.com/nodejs/node/pull/55322)
* \[[`20302851a9`](https://github.com/nodejs/node/commit/20302851a9)] - **doc**: add `isBigIntObject` to documentation (leviscar) [#55450](https://github.com/nodejs/node/pull/55450)
* \[[`50d983e80b`](https://github.com/nodejs/node/commit/50d983e80b)] - **doc**: remove outdated remarks about `highWaterMark` in fs (Ian Kerins) [#55462](https://github.com/nodejs/node/pull/55462)
* \[[`07c2fb2045`](https://github.com/nodejs/node/commit/07c2fb2045)] - **doc**: move Danielle Adams key to old gpg keys (RafaelGSS) [#55399](https://github.com/nodejs/node/pull/55399)
* \[[`41b045170d`](https://github.com/nodejs/node/commit/41b045170d)] - **doc**: move Bryan English key to old gpg keys (RafaelGSS) [#55399](https://github.com/nodejs/node/pull/55399)
* \[[`13724dcc20`](https://github.com/nodejs/node/commit/13724dcc20)] - **doc**: move Beth Griggs keys to old gpg keys (RafaelGSS) [#55399](https://github.com/nodejs/node/pull/55399)
* \[[`0230fb1ead`](https://github.com/nodejs/node/commit/0230fb1ead)] - **doc**: spell out condition restrictions (Jan Martin) [#55187](https://github.com/nodejs/node/pull/55187)
* \[[`66e41f044d`](https://github.com/nodejs/node/commit/66e41f044d)] - **doc**: add instructions for WinGet build (Hüseyin Açacak) [#55356](https://github.com/nodejs/node/pull/55356)
* \[[`23d89da3f1`](https://github.com/nodejs/node/commit/23d89da3f1)] - **doc**: add missing return values in buffer docs (Karl Horky) [#55273](https://github.com/nodejs/node/pull/55273)
* \[[`6e7b33a0ef`](https://github.com/nodejs/node/commit/6e7b33a0ef)] - **doc**: fix ambasador markdown list (Rafael Gonzaga) [#55361](https://github.com/nodejs/node/pull/55361)
* \[[`d8c552a060`](https://github.com/nodejs/node/commit/d8c552a060)] - **doc**: edit onboarding guide to clarify when mailmap addition is needed (Antoine du Hamel) [#55334](https://github.com/nodejs/node/pull/55334)
* \[[`c7f82ec978`](https://github.com/nodejs/node/commit/c7f82ec978)] - **doc**: fix the return type of outgoingMessage.setHeaders() (Jimmy Leung) [#55290](https://github.com/nodejs/node/pull/55290)
* \[[`f1b9791694`](https://github.com/nodejs/node/commit/f1b9791694)] - **doc**: update `require(ESM)` history and stability status (Antoine du Hamel) [#55199](https://github.com/nodejs/node/pull/55199)
* \[[`9ffd2dd43b`](https://github.com/nodejs/node/commit/9ffd2dd43b)] - **doc**: consolidate history table of CustomEvent (Edigleysson Silva (Edy)) [#55758](https://github.com/nodejs/node/pull/55758)
* \[[`64fb9e6516`](https://github.com/nodejs/node/commit/64fb9e6516)] - **doc**: add history entries for JSON modules stabilization (Antoine du Hamel) [#55855](https://github.com/nodejs/node/pull/55855)
* \[[`ae2ae2fef1`](https://github.com/nodejs/node/commit/ae2ae2fef1)] - **esm**: fix import.meta.resolve crash (Marco Ippolito) [#55777](https://github.com/nodejs/node/pull/55777)
* \[[`15dd43dd6e`](https://github.com/nodejs/node/commit/15dd43dd6e)] - **esm**: add a fallback when importer in not a file (Antoine du Hamel) [#55471](https://github.com/nodejs/node/pull/55471)
* \[[`aed758d270`](https://github.com/nodejs/node/commit/aed758d270)] - **esm**: fix inconsistency with `importAssertion` in `resolve` hook (Wei Zhu) [#55365](https://github.com/nodejs/node/pull/55365)
* \[[`88d91e8bc2`](https://github.com/nodejs/node/commit/88d91e8bc2)] - **esm**: mark import attributes and JSON module as stable (Nicolò Ribaudo) [#55333](https://github.com/nodejs/node/pull/55333)
* \[[`a2c8de7fba`](https://github.com/nodejs/node/commit/a2c8de7fba)] - **events**: add hasEventListener util for validate (Sunghoon) [#55230](https://github.com/nodejs/node/pull/55230)
* \[[`4f84cdc8a2`](https://github.com/nodejs/node/commit/4f84cdc8a2)] - **events**: optimize EventTarget.addEventListener (Robert Nagy) [#55312](https://github.com/nodejs/node/pull/55312)
* \[[`c17601557b`](https://github.com/nodejs/node/commit/c17601557b)] - **fs**: prevent unwanted `dependencyOwners` removal (Carlos Espa) [#55565](https://github.com/nodejs/node/pull/55565)
* \[[`4dd609c685`](https://github.com/nodejs/node/commit/4dd609c685)] - **fs**: fix bufferSize option for opendir recursive (Ethan Arrowood) [#55744](https://github.com/nodejs/node/pull/55744)
* \[[`d695bd4c4f`](https://github.com/nodejs/node/commit/d695bd4c4f)] - **fs**: pass correct path to `DirentFromStats` during `glob` (Aviv Keller) [#55071](https://github.com/nodejs/node/pull/55071)
* \[[`5357338b8e`](https://github.com/nodejs/node/commit/5357338b8e)] - **fs**: use `wstring` on Windows paths (jazelly) [#55171](https://github.com/nodejs/node/pull/55171)
* \[[`0a7f301a36`](https://github.com/nodejs/node/commit/0a7f301a36)] - **http**: add diagnostic channel `http.server.response.created` (Marco Ippolito) [#55622](https://github.com/nodejs/node/pull/55622)
* \[[`98bfc7dce5`](https://github.com/nodejs/node/commit/98bfc7dce5)] - **(SEMVER-MINOR)** **http**: add diagnostic channel `http.client.request.created` (Marco Ippolito) [#55586](https://github.com/nodejs/node/pull/55586)
* \[[`d2430ee363`](https://github.com/nodejs/node/commit/d2430ee363)] - **http2**: fix client async storage persistence (Orgad Shaneh) [#55460](https://github.com/nodejs/node/pull/55460)
* \[[`753cbede2a`](https://github.com/nodejs/node/commit/753cbede2a)] - **lib**: remove startsWith/endsWith primordials for char checks (Gürgün Dayıoğlu) [#55407](https://github.com/nodejs/node/pull/55407)
* \[[`6e3e99c81e`](https://github.com/nodejs/node/commit/6e3e99c81e)] - **lib**: prefer logical assignment (Aviv Keller) [#55044](https://github.com/nodejs/node/pull/55044)
* \[[`03902ebb74`](https://github.com/nodejs/node/commit/03902ebb74)] - **lib**: replace `createDeferredPromise` util with `Promise.withResolvers` (Yagiz Nizipli) [#54836](https://github.com/nodejs/node/pull/54836)
* \[[`ee17fcd6f3`](https://github.com/nodejs/node/commit/ee17fcd6f3)] - **lib**: prefer symbol to number in webidl `type` function (Antoine du Hamel) [#55737](https://github.com/nodejs/node/pull/55737)
* \[[`18f0f07e92`](https://github.com/nodejs/node/commit/18f0f07e92)] - **lib**: implement webidl dictionary converter and use it in structuredClone (Jason Zhang) [#55489](https://github.com/nodejs/node/pull/55489)
* \[[`bcead24e24`](https://github.com/nodejs/node/commit/bcead24e24)] - **lib**: prefer number to string in webidl `type` function (Jason Zhang) [#55489](https://github.com/nodejs/node/pull/55489)
* \[[`d48c5da039`](https://github.com/nodejs/node/commit/d48c5da039)] - **lib**: convert transfer sequence to array in js (Jason Zhang) [#55317](https://github.com/nodejs/node/pull/55317)
* \[[`cefce4cbb0`](https://github.com/nodejs/node/commit/cefce4cbb0)] - **lib**: remove unnecessary optional chaining (Gürgün Dayıoğlu) [#55728](https://github.com/nodejs/node/pull/55728)
* \[[`f2561fdeec`](https://github.com/nodejs/node/commit/f2561fdeec)] - **lib**: use `Promise.withResolvers()` in timers (Yagiz Nizipli) [#55720](https://github.com/nodejs/node/pull/55720)
* \[[`337f61fb25`](https://github.com/nodejs/node/commit/337f61fb25)] - **(SEMVER-MINOR)** **lib**: add UV\_UDP\_REUSEPORT for udp (theanarkh) [#55403](https://github.com/nodejs/node/pull/55403)
* \[[`4f89059f63`](https://github.com/nodejs/node/commit/4f89059f63)] - **lib**: add flag to drop connection when running in cluster mode (theanarkh) [#54927](https://github.com/nodejs/node/pull/54927)
* \[[`29f7325e73`](https://github.com/nodejs/node/commit/29f7325e73)] - **lib**: test\_runner#mock:timers respeced timeout\_max behaviour (BadKey) [#55375](https://github.com/nodejs/node/pull/55375)
* \[[`68bcec64b8`](https://github.com/nodejs/node/commit/68bcec64b8)] - **lib**: remove settled dependant signals when they are GCed (Edigleysson Silva (Edy)) [#55354](https://github.com/nodejs/node/pull/55354)
* \[[`3f8a5d8a28`](https://github.com/nodejs/node/commit/3f8a5d8a28)] - **meta**: bump actions/setup-python from 5.2.0 to 5.3.0 (dependabot\[bot]) [#55688](https://github.com/nodejs/node/pull/55688)
* \[[`644ad5d60d`](https://github.com/nodejs/node/commit/644ad5d60d)] - **meta**: bump actions/setup-node from 4.0.4 to 4.1.0 (dependabot\[bot]) [#55687](https://github.com/nodejs/node/pull/55687)
* \[[`334fa69c31`](https://github.com/nodejs/node/commit/334fa69c31)] - **meta**: bump rtCamp/action-slack-notify from 2.3.0 to 2.3.2 (dependabot\[bot]) [#55686](https://github.com/nodejs/node/pull/55686)
* \[[`fb3fa8bee2`](https://github.com/nodejs/node/commit/fb3fa8bee2)] - **meta**: bump actions/upload-artifact from 4.4.0 to 4.4.3 (dependabot\[bot]) [#55685](https://github.com/nodejs/node/pull/55685)
* \[[`1aca3a8289`](https://github.com/nodejs/node/commit/1aca3a8289)] - **meta**: bump actions/cache from 4.0.2 to 4.1.2 (dependabot\[bot]) [#55684](https://github.com/nodejs/node/pull/55684)
* \[[`a6c73eb9c2`](https://github.com/nodejs/node/commit/a6c73eb9c2)] - **meta**: bump actions/checkout from 4.2.0 to 4.2.2 (dependabot\[bot]) [#55683](https://github.com/nodejs/node/pull/55683)
* \[[`06445bc4e3`](https://github.com/nodejs/node/commit/06445bc4e3)] - **meta**: bump github/codeql-action from 3.26.10 to 3.27.0 (dependabot\[bot]) [#55682](https://github.com/nodejs/node/pull/55682)
* \[[`37bafce2d8`](https://github.com/nodejs/node/commit/37bafce2d8)] - **meta**: make review-wanted message minimal (Aviv Keller) [#55607](https://github.com/nodejs/node/pull/55607)
* \[[`4cca54b161`](https://github.com/nodejs/node/commit/4cca54b161)] - **meta**: show PR/issue title on review-wanted (Aviv Keller) [#55606](https://github.com/nodejs/node/pull/55606)
* \[[`68decbf935`](https://github.com/nodejs/node/commit/68decbf935)] - **meta**: move one or more collaborators to emeritus (Node.js GitHub Bot) [#55381](https://github.com/nodejs/node/pull/55381)
* \[[`07fc40d823`](https://github.com/nodejs/node/commit/07fc40d823)] - **meta**: assign CODEOWNERS for /deps/ncrypto/\* (Filip Skokan) [#55426](https://github.com/nodejs/node/pull/55426)
* \[[`139e8f1579`](https://github.com/nodejs/node/commit/139e8f1579)] - **meta**: change color to blue notify review-wanted (Rafael Gonzaga) [#55423](https://github.com/nodejs/node/pull/55423)
* \[[`c0614dc92c`](https://github.com/nodejs/node/commit/c0614dc92c)] - **meta**: bump codecov/codecov-action from 4.5.0 to 4.6.0 (dependabot\[bot]) [#55222](https://github.com/nodejs/node/pull/55222)
* \[[`47b6c6748b`](https://github.com/nodejs/node/commit/47b6c6748b)] - **meta**: bump github/codeql-action from 3.26.6 to 3.26.10 (dependabot\[bot]) [#55221](https://github.com/nodejs/node/pull/55221)
* \[[`6c836aa97e`](https://github.com/nodejs/node/commit/6c836aa97e)] - **meta**: bump step-security/harden-runner from 2.9.1 to 2.10.1 (dependabot\[bot]) [#55220](https://github.com/nodejs/node/pull/55220)
* \[[`c81c818a21`](https://github.com/nodejs/node/commit/c81c818a21)] - **module**: throw ERR\_NO\_TYPESCRIPT when compiled without amaro (Marco Ippolito) [#55332](https://github.com/nodejs/node/pull/55332)
* \[[`d6d1479fcc`](https://github.com/nodejs/node/commit/d6d1479fcc)] - **module**: simplify --inspect-brk handling (Joyee Cheung) [#55679](https://github.com/nodejs/node/pull/55679)
* \[[`91fdec3a52`](https://github.com/nodejs/node/commit/91fdec3a52)] - **module**: fix error thrown from require(esm) hitting TLA repeatedly (Joyee Cheung) [#55520](https://github.com/nodejs/node/pull/55520)
* \[[`cb527a925d`](https://github.com/nodejs/node/commit/cb527a925d)] - **module**: do not warn when require(esm) comes from node\_modules (Joyee Cheung) [#55960](https://github.com/nodejs/node/pull/55960)
* \[[`16119f206f`](https://github.com/nodejs/node/commit/16119f206f)] - **module**: trim off internal stack frames for require(esm) warnings (Joyee Cheung) [#55496](https://github.com/nodejs/node/pull/55496)
* \[[`28b5b9a57d`](https://github.com/nodejs/node/commit/28b5b9a57d)] - **module**: allow ESM that failed to be required to be re-imported (Joyee Cheung) [#55502](https://github.com/nodejs/node/pull/55502)
* \[[`6ac3400960`](https://github.com/nodejs/node/commit/6ac3400960)] - **module**: include module information in require(esm) warning (Joyee Cheung) [#55397](https://github.com/nodejs/node/pull/55397)
* \[[`fcdd6167d8`](https://github.com/nodejs/node/commit/fcdd6167d8)] - **module**: check --experimental-require-module separately from detection (Joyee Cheung) [#55250](https://github.com/nodejs/node/pull/55250)
* \[[`d8c34ced43`](https://github.com/nodejs/node/commit/d8c34ced43)] - **module**: use kNodeModulesRE to detect node\_modules (Joyee Cheung) [#55243](https://github.com/nodejs/node/pull/55243)
* \[[`545c069eb5`](https://github.com/nodejs/node/commit/545c069eb5)] - **module**: support 'module.exports' interop export in require(esm) (Guy Bedford) [#54563](https://github.com/nodejs/node/pull/54563)
* \[[`58d6871c45`](https://github.com/nodejs/node/commit/58d6871c45)] - **(SEMVER-MINOR)** **module**: unflag --experimental-require-module (Joyee Cheung) [#55085](https://github.com/nodejs/node/pull/55085)
* \[[`1628c48ad6`](https://github.com/nodejs/node/commit/1628c48ad6)] - **(SEMVER-MINOR)** **net**: add UV\_TCP\_REUSEPORT for tcp (theanarkh) [#55408](https://github.com/nodejs/node/pull/55408)
* \[[`a5590083cd`](https://github.com/nodejs/node/commit/a5590083cd)] - **node-api**: add napi\_create\_buffer\_from\_arraybuffer method (Mert Can Altin) [#54505](https://github.com/nodejs/node/pull/54505)
* \[[`21ec855feb`](https://github.com/nodejs/node/commit/21ec855feb)] - **os**: improve path check with direct index access (Mert Can Altin) [#55434](https://github.com/nodejs/node/pull/55434)
* \[[`1fdaa15226`](https://github.com/nodejs/node/commit/1fdaa15226)] - **report**: fix network queries in getReport libuv with exclude-network (Adrien Foulon) [#55602](https://github.com/nodejs/node/pull/55602)
* \[[`457e73f4c9`](https://github.com/nodejs/node/commit/457e73f4c9)] - **(SEMVER-MINOR)** **sqlite**: add support for SQLite Session Extension (Bart Louwers) [#54181](https://github.com/nodejs/node/pull/54181)
* \[[`428701a6d8`](https://github.com/nodejs/node/commit/428701a6d8)] - **sqlite**: improve error handling using MaybeLocal (Tobias Nießen) [#55571](https://github.com/nodejs/node/pull/55571)
* \[[`4e5878536a`](https://github.com/nodejs/node/commit/4e5878536a)] - **sqlite**: add readOnly option (Tobias Nießen) [#55567](https://github.com/nodejs/node/pull/55567)
* \[[`8c35ad12de`](https://github.com/nodejs/node/commit/8c35ad12de)] - **sqlite**: refactor open options (Tobias Nießen) [#55442](https://github.com/nodejs/node/pull/55442)
* \[[`c3c403040a`](https://github.com/nodejs/node/commit/c3c403040a)] - **sqlite**: cache column names in stmt.all() (Fedor Indutny) [#55373](https://github.com/nodejs/node/pull/55373)
* \[[`6858f7a4d3`](https://github.com/nodejs/node/commit/6858f7a4d3)] - **src**: use env strings to create sqlite results (Michaël Zasso) [#55785](https://github.com/nodejs/node/pull/55785)
* \[[`db01eaf318`](https://github.com/nodejs/node/commit/db01eaf318)] - **src**: improve `node:os` userInfo performance (Yagiz Nizipli) [#55719](https://github.com/nodejs/node/pull/55719)
* \[[`383d28489d`](https://github.com/nodejs/node/commit/383d28489d)] - **src**: provide workaround for container-overflow (Daniel Lemire) [#55591](https://github.com/nodejs/node/pull/55591)
* \[[`3477b6b4a5`](https://github.com/nodejs/node/commit/3477b6b4a5)] - **src**: move more key related stuff to ncrypto (James M Snell) [#55368](https://github.com/nodejs/node/pull/55368)
* \[[`38c047e38f`](https://github.com/nodejs/node/commit/38c047e38f)] - **src**: refactor ECDHBitsJob signature (Filip Skokan) [#55610](https://github.com/nodejs/node/pull/55610)
* \[[`acbb62902a`](https://github.com/nodejs/node/commit/acbb62902a)] - **src**: fix dns crash when failed to create NodeAresTask (theanarkh) [#55521](https://github.com/nodejs/node/pull/55521)
* \[[`547cab9433`](https://github.com/nodejs/node/commit/547cab9433)] - **src**: use NewFromUtf8Literal in NODE\_DEFINE\_CONSTANT (Charles Kerr) [#55581](https://github.com/nodejs/node/pull/55581)
* \[[`231fe7b953`](https://github.com/nodejs/node/commit/231fe7b953)] - **src**: do not run IsWindowsBatchFile on non-windows (Yagiz Nizipli) [#55560](https://github.com/nodejs/node/pull/55560)
* \[[`bde374ee6a`](https://github.com/nodejs/node/commit/bde374ee6a)] - **src**: remove icu based `ToASCII` and `ToUnicode` (Yagiz Nizipli) [#55156](https://github.com/nodejs/node/pull/55156)
* \[[`6ad23e74be`](https://github.com/nodejs/node/commit/6ad23e74be)] - **src**: fix winapi\_strerror error string (Hüseyin Açacak) [#55207](https://github.com/nodejs/node/pull/55207)
* \[[`63bc40550b`](https://github.com/nodejs/node/commit/63bc40550b)] - **src**: remove uv\_\_node\_patch\_is\_using\_io\_uring (Santiago Gimeno) [#55114](https://github.com/nodejs/node/pull/55114)
* \[[`2af72a7671`](https://github.com/nodejs/node/commit/2af72a7671)] - **src**: implement IsInsideNodeModules() in C++ (Joyee Cheung) [#55286](https://github.com/nodejs/node/pull/55286)
* \[[`e14fb2defb`](https://github.com/nodejs/node/commit/e14fb2defb)] - **src,lib**: optimize nodeTiming.uvMetricsInfo (RafaelGSS) [#55614](https://github.com/nodejs/node/pull/55614)
* \[[`e14dba3ee5`](https://github.com/nodejs/node/commit/e14dba3ee5)] - **src,lib**: introduce `util.getSystemErrorMessage(err)` (Juan José) [#54075](https://github.com/nodejs/node/pull/54075)
* \[[`8f59c41d52`](https://github.com/nodejs/node/commit/8f59c41d52)] - **stream**: propagate AbortSignal reason (Marvin ROGER) [#55473](https://github.com/nodejs/node/pull/55473)
* \[[`7acb96362c`](https://github.com/nodejs/node/commit/7acb96362c)] - **test**: increase coverage of `pathToFileURL` (Antoine du Hamel) [#55493](https://github.com/nodejs/node/pull/55493)
* \[[`5861135ddb`](https://github.com/nodejs/node/commit/5861135ddb)] - **test**: improve test coverage for child process message sending (Juan José) [#55710](https://github.com/nodejs/node/pull/55710)
* \[[`554d4ace2f`](https://github.com/nodejs/node/commit/554d4ace2f)] - **test**: ensure that test priority is not higher than current priority (Livia Medeiros) [#55739](https://github.com/nodejs/node/pull/55739)
* \[[`b0ce62a9bd`](https://github.com/nodejs/node/commit/b0ce62a9bd)] - **test**: add buffer to fs\_permission tests (Rafael Gonzaga) [#55734](https://github.com/nodejs/node/pull/55734)
* \[[`9d9ad81d54`](https://github.com/nodejs/node/commit/9d9ad81d54)] - **test**: improve test coverage for `ServerResponse` (Juan José) [#55711](https://github.com/nodejs/node/pull/55711)
* \[[`273f84e01c`](https://github.com/nodejs/node/commit/273f84e01c)] - **test**: update `performance-timeline` wpt (RedYetiDev) [#55197](https://github.com/nodejs/node/pull/55197)
* \[[`89c9c46185`](https://github.com/nodejs/node/commit/89c9c46185)] - **test**: ignore unrelated events in FW watch tests (Carlos Espa) [#55605](https://github.com/nodejs/node/pull/55605)
* \[[`fc69080669`](https://github.com/nodejs/node/commit/fc69080669)] - **test**: refactor some esm tests (Antoine du Hamel) [#55472](https://github.com/nodejs/node/pull/55472)
* \[[`a80c166733`](https://github.com/nodejs/node/commit/a80c166733)] - **test**: split up test-runner-mock-timers test (Julian Gassner) [#55506](https://github.com/nodejs/node/pull/55506)
* \[[`8c2fc11f7c`](https://github.com/nodejs/node/commit/8c2fc11f7c)] - **test**: remove unneeded listeners (Luigi Pinca) [#55486](https://github.com/nodejs/node/pull/55486)
* \[[`1c5872dbde`](https://github.com/nodejs/node/commit/1c5872dbde)] - **test**: avoid `apply()` calls with large amount of elements (Livia Medeiros) [#55501](https://github.com/nodejs/node/pull/55501)
* \[[`2194eb4909`](https://github.com/nodejs/node/commit/2194eb4909)] - **test**: increase test coverage for `http.OutgoingMessage.appendHeader()` (Juan José) [#55467](https://github.com/nodejs/node/pull/55467)
* \[[`ad7e81379a`](https://github.com/nodejs/node/commit/ad7e81379a)] - **test**: make test-node-output-v8-warning more flexible (Shelley Vohr) [#55401](https://github.com/nodejs/node/pull/55401)
* \[[`6aeeaa719b`](https://github.com/nodejs/node/commit/6aeeaa719b)] - **test**: fix addons and node-api test assumptions (Antoine du Hamel) [#55441](https://github.com/nodejs/node/pull/55441)
* \[[`73ab14fd8f`](https://github.com/nodejs/node/commit/73ab14fd8f)] - **test**: update wpt test for webmessaging/broadcastchannel (devstone) [#55205](https://github.com/nodejs/node/pull/55205)
* \[[`ded1b68d10`](https://github.com/nodejs/node/commit/ded1b68d10)] - **test**: deflake `test-cluster-shared-handle-bind-privileged-port` (Aviv Keller) [#55378](https://github.com/nodejs/node/pull/55378)
* \[[`0e873c3031`](https://github.com/nodejs/node/commit/0e873c3031)] - **test**: update `console` wpt (Aviv Keller) [#55192](https://github.com/nodejs/node/pull/55192)
* \[[`832300533b`](https://github.com/nodejs/node/commit/832300533b)] - **test**: remove duplicate tests (Luigi Pinca) [#55393](https://github.com/nodejs/node/pull/55393)
* \[[`310a734c1b`](https://github.com/nodejs/node/commit/310a734c1b)] - **test**: update test\_util.cc for coverage (minkyu\_kim) [#55291](https://github.com/nodejs/node/pull/55291)
* \[[`254badd480`](https://github.com/nodejs/node/commit/254badd480)] - **test**: update `compression` wpt (Aviv Keller) [#55191](https://github.com/nodejs/node/pull/55191)
* \[[`c52a808ac9`](https://github.com/nodejs/node/commit/c52a808ac9)] - **test,crypto**: update WebCryptoAPI WPT (Filip Skokan) [#55703](https://github.com/nodejs/node/pull/55703)
* \[[`445d117b67`](https://github.com/nodejs/node/commit/445d117b67)] - **test,crypto**: update WebCryptoAPI WPT (Filip Skokan) [#55512](https://github.com/nodejs/node/pull/55512)
* \[[`cd0d748ede`](https://github.com/nodejs/node/commit/cd0d748ede)] - **test,crypto**: make crypto tests work with BoringSSL (Shelley Vohr) [#55491](https://github.com/nodejs/node/pull/55491)
* \[[`8bac7c27c8`](https://github.com/nodejs/node/commit/8bac7c27c8)] - **test,crypto**: update WebCryptoAPI WPT (Filip Skokan) [#55427](https://github.com/nodejs/node/pull/55427)
* \[[`363e7d5a76`](https://github.com/nodejs/node/commit/363e7d5a76)] - **test\_runner**: error on mocking an already mocked date (Aviv Keller) [#55858](https://github.com/nodejs/node/pull/55858)
* \[[`f41d329e98`](https://github.com/nodejs/node/commit/f41d329e98)] - **test\_runner**: add support for scheduler.wait on mock timers (Erick Wendel) [#55244](https://github.com/nodejs/node/pull/55244)
* \[[`b9200c33ae`](https://github.com/nodejs/node/commit/b9200c33ae)] - **test\_runner**: require `--enable-source-maps` for sourcemap coverage (Aviv Keller) [#55359](https://github.com/nodejs/node/pull/55359)
* \[[`f11d93d8ef`](https://github.com/nodejs/node/commit/f11d93d8ef)] - **tools**: enforce ordering of error codes in `errors.md` (Antoine du Hamel) [#55324](https://github.com/nodejs/node/pull/55324)
* \[[`85ca31a90a`](https://github.com/nodejs/node/commit/85ca31a90a)] - **tools**: bump @eslint/plugin-kit from 0.2.0 to 0.2.3 in /tools/eslint (dependabot\[bot]) [#55875](https://github.com/nodejs/node/pull/55875)
* \[[`506aac567b`](https://github.com/nodejs/node/commit/506aac567b)] - **tools**: fix exclude labels for commit-queue (Richard Lau) [#55809](https://github.com/nodejs/node/pull/55809)
* \[[`14ffac9995`](https://github.com/nodejs/node/commit/14ffac9995)] - **tools**: make commit-queue check blocked label (Marco Ippolito) [#55781](https://github.com/nodejs/node/pull/55781)
* \[[`eb22ec87e6`](https://github.com/nodejs/node/commit/eb22ec87e6)] - **tools**: remove non-existent file from eslint config (Aviv Keller) [#55772](https://github.com/nodejs/node/pull/55772)
* \[[`5844565fb2`](https://github.com/nodejs/node/commit/5844565fb2)] - **tools**: fix c-ares updater script for Node.js 18 (Richard Lau) [#55717](https://github.com/nodejs/node/pull/55717)
* \[[`0a79ebd257`](https://github.com/nodejs/node/commit/0a79ebd257)] - **tools**: update ESLint to 9.14.0 (dependabot\[bot]) [#55689](https://github.com/nodejs/node/pull/55689)
* \[[`12543d560a`](https://github.com/nodejs/node/commit/12543d560a)] - **tools**: use `util.parseArgs` in `lint-md` (Aviv Keller) [#55694](https://github.com/nodejs/node/pull/55694)
* \[[`d95aa244c2`](https://github.com/nodejs/node/commit/d95aa244c2)] - **tools**: fix root certificate updater (Richard Lau) [#55681](https://github.com/nodejs/node/pull/55681)
* \[[`3626891f8e`](https://github.com/nodejs/node/commit/3626891f8e)] - **tools**: compact jq output in daily-wpt-fyi.yml action (Filip Skokan) [#55695](https://github.com/nodejs/node/pull/55695)
* \[[`02c902e68a`](https://github.com/nodejs/node/commit/02c902e68a)] - **tools**: run daily WPT.fyi report on all supported releases (Filip Skokan) [#55619](https://github.com/nodejs/node/pull/55619)
* \[[`456b02351b`](https://github.com/nodejs/node/commit/456b02351b)] - **tools**: lint README lists more strictly (Antoine du Hamel) [#55625](https://github.com/nodejs/node/pull/55625)
* \[[`83a5983c7d`](https://github.com/nodejs/node/commit/83a5983c7d)] - **tools**: update lint-md-dependencies (Node.js GitHub Bot) [#55470](https://github.com/nodejs/node/pull/55470)
* \[[`72b4a8df6a`](https://github.com/nodejs/node/commit/72b4a8df6a)] - **tools**: update gyp-next to 0.18.3 (Node.js GitHub Bot) [#55464](https://github.com/nodejs/node/pull/55464)
* \[[`6b6e6a5590`](https://github.com/nodejs/node/commit/6b6e6a5590)] - **tools**: add script to synch c-ares source lists (Richard Lau) [#55445](https://github.com/nodejs/node/pull/55445)
* \[[`a6c444291b`](https://github.com/nodejs/node/commit/a6c444291b)] - **tools**: fix typos (Nathan Baulch) [#55061](https://github.com/nodejs/node/pull/55061)
* \[[`d5e915ba5d`](https://github.com/nodejs/node/commit/d5e915ba5d)] - **tools**: add `polyfilled` option to `prefer-primordials` rule (Antoine du Hamel) [#55318](https://github.com/nodejs/node/pull/55318)
* \[[`c8e7f767b7`](https://github.com/nodejs/node/commit/c8e7f767b7)] - **typings**: add missing type of `ArrayBufferPrototypeGetByteLength` (Wuli Zuo) [#55439](https://github.com/nodejs/node/pull/55439)
* \[[`6317f77942`](https://github.com/nodejs/node/commit/6317f77942)] - **url**: refactor `pathToFileURL` to native (Antoine du Hamel) [#55476](https://github.com/nodejs/node/pull/55476)
* \[[`5418d40256`](https://github.com/nodejs/node/commit/5418d40256)] - **url**: handle "unsafe" characters properly in `pathToFileURL` (Antoine du Hamel) [#54545](https://github.com/nodejs/node/pull/54545)
* \[[`fce8c32c19`](https://github.com/nodejs/node/commit/fce8c32c19)] - **util**: do not mark experimental feature as deprecated (Antoine du Hamel) [#55740](https://github.com/nodejs/node/pull/55740)
* \[[`940d22ffe1`](https://github.com/nodejs/node/commit/940d22ffe1)] - **(SEMVER-MINOR)** **util**: fix util.getCallSites plurality (Chengzhong Wu) [#55626](https://github.com/nodejs/node/pull/55626)
* \[[`42ac0c2af3`](https://github.com/nodejs/node/commit/42ac0c2af3)] - **util**: do not catch on circular `@@toStringTag` errors (Aviv Keller) [#55544](https://github.com/nodejs/node/pull/55544)

<a id="22.11.0"></a>

## 2024-10-29, Version 22.11.0 'Jod' (LTS), @richardlau

### Notable Changes

This release marks the transition of Node.js 22.x into Long Term Support (LTS)
with the codename 'Jod'. The 22.x release line now moves into "Active LTS"
and will remain so until October 2025. After that time, it will move into
"Maintenance" until end of life in April 2027.

Other than updating metadata, such as the `process.release` object, to reflect
that the release is LTS, no further changes from Node.js 22.10.0 are included.

### OpenSSL 3.x

Official binaries for Node.js 22.x currently include OpenSSL 3.0.x (more
specifically, the [quictls OpenSSL fork](https://github.com/quictls/openssl)).
OpenSSL 3.0.x is the currently designated long term support version that is
scheduled to be supported until 7th September 2026, which is within the expected
lifetime of Node.js 22.x. We are expecting upstream OpenSSL to announce a
successor long term support version prior to that date and since OpenSSL now
follows a semantic versioning-like versioning scheme we expect to be able to
update to the next long term supported version of OpenSSL during the lifetime of
Node.js 22.x.

<a id="22.10.0"></a>

## 2024-10-16, Version 22.10.0 (Current), @aduh95

### Notable Changes

#### New `"module-sync"` exports condition

This release introduces a `"module-sync"` exports condition that's enabled when
`require(esm)` is enabled, so packages can supply a synchronous ES module to the
Node.js module loader, no matter if it's being required or imported. This is
similar to the `"module"` condition that bundlers have been using to support
`require(esm)` in Node.js, and allows dual-package authors to opt into ESM-first
only on newer versions of Node.js that supports `require(esm)` to avoid the
dual-package hazard.

```json
{
  "type": "module",
  "exports": {
    "node": {
      // On new version of Node.js, both require() and import get
      // the ESM version
      "module-sync": "./index.js",
      // On older version of Node.js, where "module-sync" and require(esm) are
      // not supported, use the CJS version to avoid dual-package hazard.
      // When package authors think it's time to drop support for older versions of
      // Node.js, they can remove the exports conditions and just use "main": "index.js".
      "default": "./dist/index.cjs"
    },
    // On any other environment, use the ESM version.
    "default": "./index.js"
  }
}
```

Or if the package is only meant to be run on Node.js and wants to fallback to
CJS on older versions that don't have `require(esm)`:

```json
{
  "type": "module",
  "exports": {
    // On new version of Node.js, both require() and import get the ESM version
    "module-sync": "./index.js",
    // On older version of Node.js, where "module-sync" and require(esm) are
    // not supported, use the CJS version to avoid dual-package hazard.
    // When package authors think it's time to drop support for older versions of
    // Node.js, they can remove the exports conditions and just use "main": "index.js".
    "default": "./dist/index.cjs"
  }
}
```

**For package authors**: this only serves as a feature-detection mechanism for
packages that wish to support both CJS and ESM users during the period when some
active Node.js LTS versions support  `require(esm)` while some older ones don't.
When all active Node.js LTS lines support `require(esm)`, packages can simplify
their distributions by bumping the major version, dropping their CJS exports,
and removing the `module-sync` exports condition (with only `main` or `default`
targetting the ESM exports). If the package needs to support both bundlers and
being run unbundled on Node.js during the transition period, use both
`module-sync` and `module` and point them to the same ESM file. If the package
already doesn't want to support older versions of Node.js that doesn't support
`require(esm)`, don't use this export condition.

**For bundlers/tools**: they should avoid implementing this stop-gap condition.
Most existing bundlers implement the de-facto bundler standard
[`module`](https://webpack.js.org/guides/package-exports/#providing-commonjs-and-esm-version-stateless)
exports condition, and that should be enough to support users who want to bundle
ESM from CJS consumers. Users who want both bundlers and Node.js to recognize
the ESM exports can use both `module`/`module-sync` conditions during the
transition period, and can drop `module-sync`+`module` when they no longer need
to support older versions of Node.js. If tools do want to support this
condition, it's recommended to make the resolution rules in the graph pointed by
this condition match the Node.js native ESM rules to avoid divergence.

We ended up implementing a condition with a different name instead of reusing
`"module"`, because existing code in the ecosystem using the `"module"`
condition sometimes also expect the module resolution for these ESM files to
work in CJS style, which is supported by bundlers, but the native Node.js loader
has intentionally made ESM resolution different from CJS resolution (e.g.
forbidding `import './noext'` or `import './directory'`), so it would be
breaking to implement a `"module"` condition without implementing the forbidden
ESM resolution rules. For now, this just implements a new condition as
semver-minor so it can be backported to older LTS.

Contributed by Joyee Cheung in [#54648](https://github.com/nodejs/node/pull/54648).

#### `node --run` is now stable

This CLI flag runs a specified command from a `package.json`'s `"scripts"` object.

For the following `package.json`:

```json
{
  "scripts": {
    "test": "node --test-reporter junit --test ./test"
  }
}
```

You can run `node --run test` and that would start the test suite.

Contributed by Yagiz Nizipli in [#53763](https://github.com/nodejs/node/pull/53763).

#### Other notable changes

* \[[`f0b441230a`](https://github.com/nodejs/node/commit/f0b441230a)] - **(SEMVER-MINOR)** **crypto**: add `KeyObject.prototype.toCryptoKey` (Filip Skokan) [#55262](https://github.com/nodejs/node/pull/55262)
* \[[`349d2ed07b`](https://github.com/nodejs/node/commit/349d2ed07b)] - **(SEMVER-MINOR)** **crypto**: add Date fields for `validTo` and `validFrom` (Andrew Moon) [#54159](https://github.com/nodejs/node/pull/54159)
* \[[`bebc95ed58`](https://github.com/nodejs/node/commit/bebc95ed58)] - **doc**: add abmusse to collaborators (Abdirahim Musse) [#55086](https://github.com/nodejs/node/pull/55086)
* \[[`914db60159`](https://github.com/nodejs/node/commit/914db60159)] - **(SEMVER-MINOR)** **http2**: expose `nghttp2_option_set_stream_reset_rate_limit` as an option (Maël Nison) [#54875](https://github.com/nodejs/node/pull/54875)
* \[[`f7c3b03759`](https://github.com/nodejs/node/commit/f7c3b03759)] - **(SEMVER-MINOR)** **lib**: propagate aborted state to dependent signals before firing events (jazelly) [#54826](https://github.com/nodejs/node/pull/54826)
* \[[`32261fc98a`](https://github.com/nodejs/node/commit/32261fc98a)] - **(SEMVER-MINOR)** **module**: support loading entrypoint as url (RedYetiDev) [#54933](https://github.com/nodejs/node/pull/54933)
* \[[`06957ff355`](https://github.com/nodejs/node/commit/06957ff355)] - **(SEMVER-MINOR)** **module**: implement `flushCompileCache()` (Joyee Cheung) [#54971](https://github.com/nodejs/node/pull/54971)
* \[[`2dcf70c347`](https://github.com/nodejs/node/commit/2dcf70c347)] - **(SEMVER-MINOR)** **module**: throw when invalid argument is passed to `enableCompileCache()` (Joyee Cheung) [#54971](https://github.com/nodejs/node/pull/54971)
* \[[`f9b19d7c44`](https://github.com/nodejs/node/commit/f9b19d7c44)] - **(SEMVER-MINOR)** **module**: write compile cache to temporary file and then rename it (Joyee Cheung) [#54971](https://github.com/nodejs/node/pull/54971)
* \[[`e95163b170`](https://github.com/nodejs/node/commit/e95163b170)] - **(SEMVER-MINOR)** **process**: add `process.features.require_module` (Joyee Cheung) [#55241](https://github.com/nodejs/node/pull/55241)
* \[[`4050f68e5d`](https://github.com/nodejs/node/commit/4050f68e5d)] - **(SEMVER-MINOR)** **process**: add `process.features.typescript` (Aviv Keller) [#54295](https://github.com/nodejs/node/pull/54295)
* \[[`86f7cb802d`](https://github.com/nodejs/node/commit/86f7cb802d)] - **(SEMVER-MINOR)** **test\_runner**: support custom arguments in `run()` (Aviv Keller) [#55126](https://github.com/nodejs/node/pull/55126)
* \[[`b62f2f8259`](https://github.com/nodejs/node/commit/b62f2f8259)] - **(SEMVER-MINOR)** **test\_runner**: add `'test:summary'` event (Colin Ihrig) [#54851](https://github.com/nodejs/node/pull/54851)
* \[[`d7c708aec5`](https://github.com/nodejs/node/commit/d7c708aec5)] - **(SEMVER-MINOR)** **test\_runner**: add support for coverage via `run()` (Chemi Atlow) [#53937](https://github.com/nodejs/node/pull/53937)
* \[[`5fda4a1498`](https://github.com/nodejs/node/commit/5fda4a1498)] - **(SEMVER-MINOR)** **worker**: add `markAsUncloneable` api (Jason Zhang) [#55234](https://github.com/nodejs/node/pull/55234)

### Commits

* \[[`e3619510c8`](https://github.com/nodejs/node/commit/e3619510c8)] - **assert**: show the diff when deep comparing data with a custom message (Giovanni) [#54759](https://github.com/nodejs/node/pull/54759)
* \[[`39c7a9e70c`](https://github.com/nodejs/node/commit/39c7a9e70c)] - **benchmark**: adjust config for deepEqual object (Rafael Gonzaga) [#55254](https://github.com/nodejs/node/pull/55254)
* \[[`263526d5d0`](https://github.com/nodejs/node/commit/263526d5d0)] - **benchmark**: rewrite detect-esm-syntax benchmark (Joyee Cheung) [#55238](https://github.com/nodejs/node/pull/55238)
* \[[`cd0795fb00`](https://github.com/nodejs/node/commit/cd0795fb00)] - **benchmark**: add no-warnings to process.has bench (Rafael Gonzaga) [#55159](https://github.com/nodejs/node/pull/55159)
* \[[`4352d9cc31`](https://github.com/nodejs/node/commit/4352d9cc31)] - **benchmark**: create benchmark for typescript (Marco Ippolito) [#54904](https://github.com/nodejs/node/pull/54904)
* \[[`452bc9b48d`](https://github.com/nodejs/node/commit/452bc9b48d)] - **benchmark**: add webstorage benchmark (jakecastelli) [#55040](https://github.com/nodejs/node/pull/55040)
* \[[`d4d5ba3a9b`](https://github.com/nodejs/node/commit/d4d5ba3a9b)] - **benchmark**: include ascii to fs/readfile (Rafael Gonzaga) [#54988](https://github.com/nodejs/node/pull/54988)
* \[[`23b628db65`](https://github.com/nodejs/node/commit/23b628db65)] - **benchmark**: add dotenv benchmark (Aviv Keller) [#54278](https://github.com/nodejs/node/pull/54278)
* \[[`b1ebb0d8ca`](https://github.com/nodejs/node/commit/b1ebb0d8ca)] - **buffer**: coerce extrema to int in `blob.slice` (Antoine du Hamel) [#55141](https://github.com/nodejs/node/pull/55141)
* \[[`3a6e72483f`](https://github.com/nodejs/node/commit/3a6e72483f)] - **buffer**: extract Blob's .arrayBuffer() & webidl changes (Matthew Aitken) [#53372](https://github.com/nodejs/node/pull/53372)
* \[[`d109f1c4ff`](https://github.com/nodejs/node/commit/d109f1c4ff)] - **buffer**: use simdutf convert\_latin1\_to\_utf8\_safe (Robert Nagy) [#54798](https://github.com/nodejs/node/pull/54798)
* \[[`77f8a3f9c2`](https://github.com/nodejs/node/commit/77f8a3f9c2)] - **build**: fix notify-on-review-wanted action (Rafael Gonzaga) [#55304](https://github.com/nodejs/node/pull/55304)
* \[[`0d93b1ed0c`](https://github.com/nodejs/node/commit/0d93b1ed0c)] - **build**: fix not valid json in coverage (jakecastelli) [#55179](https://github.com/nodejs/node/pull/55179)
* \[[`f89664d890`](https://github.com/nodejs/node/commit/f89664d890)] - **build**: include `.nycrc` in coverage workflows (Wuli Zuo) [#55210](https://github.com/nodejs/node/pull/55210)
* \[[`d7a9df6417`](https://github.com/nodejs/node/commit/d7a9df6417)] - **build**: notify via slack when review-wanted (Rafael Gonzaga) [#55102](https://github.com/nodejs/node/pull/55102)
* \[[`68822cc861`](https://github.com/nodejs/node/commit/68822cc861)] - **build**: add more information to Makefile help (Aviv Keller) [#53381](https://github.com/nodejs/node/pull/53381)
* \[[`f3ca9c669b`](https://github.com/nodejs/node/commit/f3ca9c669b)] - **build**: update ruff and add `lint-py-fix` (Aviv Keller) [#54410](https://github.com/nodejs/node/pull/54410)
* \[[`d99ae548d7`](https://github.com/nodejs/node/commit/d99ae548d7)] - **build**: remove -v flag to reduce noise (iwuliz) [#55025](https://github.com/nodejs/node/pull/55025)
* \[[`d3dfbe7ff9`](https://github.com/nodejs/node/commit/d3dfbe7ff9)] - **build**: display free disk space after build in the test-macOS workflow (iwuliz) [#55025](https://github.com/nodejs/node/pull/55025)
* \[[`3077f6a5b7`](https://github.com/nodejs/node/commit/3077f6a5b7)] - **build**: support up to python 3.13 in android-configure (Aviv Keller) [#54529](https://github.com/nodejs/node/pull/54529)
* \[[`a929c71281`](https://github.com/nodejs/node/commit/a929c71281)] - **build**: add the option to generate compile\_commands.json in vcbuild.bat (Segev Finer) [#52279](https://github.com/nodejs/node/pull/52279)
* \[[`a81f368b99`](https://github.com/nodejs/node/commit/a81f368b99)] - **build**: fix eslint makefile target (Aviv Keller) [#54999](https://github.com/nodejs/node/pull/54999)
* \[[`c8b7a645ae`](https://github.com/nodejs/node/commit/c8b7a645ae)] - _**Revert**_ "**build**: upgrade clang-format to v18" (Chengzhong Wu) [#54994](https://github.com/nodejs/node/pull/54994)
* \[[`7861ca5dc3`](https://github.com/nodejs/node/commit/7861ca5dc3)] - **build**: print `Running XYZ linter...` for py and yml (Aviv Keller) [#54386](https://github.com/nodejs/node/pull/54386)
* \[[`aaea3944e5`](https://github.com/nodejs/node/commit/aaea3944e5)] - **build,win**: add winget config to set up env (Hüseyin Açacak) [#54729](https://github.com/nodejs/node/pull/54729)
* \[[`30d47220bb`](https://github.com/nodejs/node/commit/30d47220bb)] - **build,win**: float VS 17.11 compilation patch (Stefan Stojanovic) [#54970](https://github.com/nodejs/node/pull/54970)
* \[[`048a1ab350`](https://github.com/nodejs/node/commit/048a1ab350)] - **cli**: ensure --run has proper pwd (Yagiz Nizipli) [#54949](https://github.com/nodejs/node/pull/54949)
* \[[`a97841ee10`](https://github.com/nodejs/node/commit/a97841ee10)] - **cli**: fix spacing for port range error (Aviv Keller) [#54495](https://github.com/nodejs/node/pull/54495)
* \[[`1dcc5eedff`](https://github.com/nodejs/node/commit/1dcc5eedff)] - _**Revert**_ "**console**: colorize console error and warn" (Aviv Keller) [#54677](https://github.com/nodejs/node/pull/54677)
* \[[`f0b441230a`](https://github.com/nodejs/node/commit/f0b441230a)] - **(SEMVER-MINOR)** **crypto**: add KeyObject.prototype.toCryptoKey (Filip Skokan) [#55262](https://github.com/nodejs/node/pull/55262)
* \[[`d3f8c35320`](https://github.com/nodejs/node/commit/d3f8c35320)] - **crypto**: ensure invalid SubtleCrypto JWK data import results in DataError (Filip Skokan) [#55041](https://github.com/nodejs/node/pull/55041)
* \[[`349d2ed07b`](https://github.com/nodejs/node/commit/349d2ed07b)] - **(SEMVER-MINOR)** **crypto**: add Date fields for `validTo` and `validFrom` (Andrew Moon) [#54159](https://github.com/nodejs/node/pull/54159)
* \[[`34ca36a397`](https://github.com/nodejs/node/commit/34ca36a397)] - **deps**: update undici to 6.20.0 (Node.js GitHub Bot) [#55329](https://github.com/nodejs/node/pull/55329)
* \[[`f703652e84`](https://github.com/nodejs/node/commit/f703652e84)] - **deps**: upgrade npm to 10.9.0 (npm team) [#55255](https://github.com/nodejs/node/pull/55255)
* \[[`b533a51856`](https://github.com/nodejs/node/commit/b533a51856)] - **deps**: V8: backport 0d5d6e71bbb0 (Yagiz Nizipli) [#55115](https://github.com/nodejs/node/pull/55115)
* \[[`2f65b3fd07`](https://github.com/nodejs/node/commit/2f65b3fd07)] - **deps**: V8: partially cherry-pick 8953e49478 (Ben Noordhuis) [#55274](https://github.com/nodejs/node/pull/55274)
* \[[`bb9f77d53a`](https://github.com/nodejs/node/commit/bb9f77d53a)] - **deps**: update archs files for openssl-3.0.15+quic1 (Node.js GitHub Bot) [#55184](https://github.com/nodejs/node/pull/55184)
* \[[`63d51c82fe`](https://github.com/nodejs/node/commit/63d51c82fe)] - **deps**: upgrade openssl sources to quictls/openssl-3.0.15+quic1 (Node.js GitHub Bot) [#55184](https://github.com/nodejs/node/pull/55184)
* \[[`29e6484f3c`](https://github.com/nodejs/node/commit/29e6484f3c)] - **deps**: update archs files for openssl-3.0.14+quic1 (Node.js GitHub Bot) [#54336](https://github.com/nodejs/node/pull/54336)
* \[[`283927ec88`](https://github.com/nodejs/node/commit/283927ec88)] - **deps**: upgrade openssl sources to quictls/openssl-3.0.14+quic1 (Node.js GitHub Bot) [#54336](https://github.com/nodejs/node/pull/54336)
* \[[`b0636a1e88`](https://github.com/nodejs/node/commit/b0636a1e88)] - **deps**: update timezone to 2024b (Node.js GitHub Bot) [#55056](https://github.com/nodejs/node/pull/55056)
* \[[`173464d76f`](https://github.com/nodejs/node/commit/173464d76f)] - **deps**: update acorn-walk to 8.3.4 (Node.js GitHub Bot) [#54950](https://github.com/nodejs/node/pull/54950)
* \[[`0d4536543b`](https://github.com/nodejs/node/commit/0d4536543b)] - **deps**: update corepack to 0.29.4 (Node.js GitHub Bot) [#54845](https://github.com/nodejs/node/pull/54845)
* \[[`1de5512383`](https://github.com/nodejs/node/commit/1de5512383)] - **deps**: V8: cherry-pick 217457d0a560 (Michaël Zasso) [#54883](https://github.com/nodejs/node/pull/54883)
* \[[`1921d7a37c`](https://github.com/nodejs/node/commit/1921d7a37c)] - **doc**: add release key for aduh95 (Antoine du Hamel) [#55349](https://github.com/nodejs/node/pull/55349)
* \[[`d8e42be1b2`](https://github.com/nodejs/node/commit/d8e42be1b2)] - **doc**: move `ERR_INVALID_PERFORMANCE_MARK` to legacy errors (Antoine du Hamel) [#55247](https://github.com/nodejs/node/pull/55247)
* \[[`5ea8aa183c`](https://github.com/nodejs/node/commit/5ea8aa183c)] - **doc**: fix Markdown linter (Antoine du Hamel) [#55344](https://github.com/nodejs/node/pull/55344)
* \[[`873588888d`](https://github.com/nodejs/node/commit/873588888d)] - _**Revert**_ "**doc**: update test context.assert" (Antoine du Hamel) [#55344](https://github.com/nodejs/node/pull/55344)
* \[[`707e7cc702`](https://github.com/nodejs/node/commit/707e7cc702)] - **doc**: add pmarchini to collaborators (Pietro Marchini) [#55331](https://github.com/nodejs/node/pull/55331)
* \[[`b03272b9a1`](https://github.com/nodejs/node/commit/b03272b9a1)] - **doc**: fix `events.once()` example using `AbortSignal` (Ivo Janssen) [#55144](https://github.com/nodejs/node/pull/55144)
* \[[`85b765953d`](https://github.com/nodejs/node/commit/85b765953d)] - **doc**: add onboarding details for ambassador program (Marco Ippolito) [#55284](https://github.com/nodejs/node/pull/55284)
* \[[`5d41b8a8b0`](https://github.com/nodejs/node/commit/5d41b8a8b0)] - **doc**: update `require(ESM)` history and stability status (Antoine du Hamel) [#55199](https://github.com/nodejs/node/pull/55199)
* \[[`195df659e9`](https://github.com/nodejs/node/commit/195df659e9)] - **doc**: move `ERR_NAPI_TSFN_START/STOP_IDLE_LOOP` to legacy errors (Antoine du Hamel) [#55248](https://github.com/nodejs/node/pull/55248)
* \[[`8eae0d3f3c`](https://github.com/nodejs/node/commit/8eae0d3f3c)] - **doc**: fix initial default value of autoSelectFamily (Ihor Rohovets) [#55245](https://github.com/nodejs/node/pull/55245)
* \[[`297cb0da5a`](https://github.com/nodejs/node/commit/297cb0da5a)] - **doc**: tweak onboarding instructions (Michael Dawson) [#55212](https://github.com/nodejs/node/pull/55212)
* \[[`7ddbfe8c2b`](https://github.com/nodejs/node/commit/7ddbfe8c2b)] - **doc**: update test context.assert (Pietro Marchini) [#55186](https://github.com/nodejs/node/pull/55186)
* \[[`8a57550d20`](https://github.com/nodejs/node/commit/8a57550d20)] - **doc**: fix unordered error anchors (Antoine du Hamel) [#55242](https://github.com/nodejs/node/pull/55242)
* \[[`286ea4ed3d`](https://github.com/nodejs/node/commit/286ea4ed3d)] - **doc**: mention addons to experimental permission (Rafael Gonzaga) [#55166](https://github.com/nodejs/node/pull/55166)
* \[[`7c9ceabf38`](https://github.com/nodejs/node/commit/7c9ceabf38)] - **doc**: use correct dash in stability status (Antoine du Hamel) [#55200](https://github.com/nodejs/node/pull/55200)
* \[[`781ffd8ba1`](https://github.com/nodejs/node/commit/781ffd8ba1)] - **doc**: fix link in `test/README.md` (Livia Medeiros) [#55165](https://github.com/nodejs/node/pull/55165)
* \[[`61b9ed3bf2`](https://github.com/nodejs/node/commit/61b9ed3bf2)] - **doc**: add esm examples to node:net (Alfredo González) [#55134](https://github.com/nodejs/node/pull/55134)
* \[[`bb3499038d`](https://github.com/nodejs/node/commit/bb3499038d)] - **doc**: remove outdated https import reference (Edigleysson Silva (Edy)) [#55111](https://github.com/nodejs/node/pull/55111)
* \[[`6cc49518c7`](https://github.com/nodejs/node/commit/6cc49518c7)] - **doc**: move the YAML changes element (sendoru) [#55112](https://github.com/nodejs/node/pull/55112)
* \[[`b12b4a23e4`](https://github.com/nodejs/node/commit/b12b4a23e4)] - **doc**: remove random horizontal separators in `process.md` (Antoine du Hamel) [#55149](https://github.com/nodejs/node/pull/55149)
* \[[`7186ede388`](https://github.com/nodejs/node/commit/7186ede388)] - **doc**: put --env-file-if-exists=config right under --env-file=config (Edigleysson Silva (Edy)) [#55131](https://github.com/nodejs/node/pull/55131)
* \[[`8ad0dfff10`](https://github.com/nodejs/node/commit/8ad0dfff10)] - **doc**: fix the require resolve algorithm in `modules.md` (chirsz) [#55117](https://github.com/nodejs/node/pull/55117)
* \[[`fd40f0873f`](https://github.com/nodejs/node/commit/fd40f0873f)] - **doc**: update style guide (Aviv Keller) [#53223](https://github.com/nodejs/node/pull/53223)
* \[[`12c9d9780f`](https://github.com/nodejs/node/commit/12c9d9780f)] - **doc**: add missing `:` to `run()`'s `globPatterns` (Aviv Keller) [#55135](https://github.com/nodejs/node/pull/55135)
* \[[`73b05cfb04`](https://github.com/nodejs/node/commit/73b05cfb04)] - **doc**: correct `cleanup` option in stream.(promises.)finished (René) [#55043](https://github.com/nodejs/node/pull/55043)
* \[[`bebc95ed58`](https://github.com/nodejs/node/commit/bebc95ed58)] - **doc**: add abmusse to collaborators (Abdirahim Musse) [#55086](https://github.com/nodejs/node/pull/55086)
* \[[`a97c80c6ae`](https://github.com/nodejs/node/commit/a97c80c6ae)] - **doc**: add note about `--expose-internals` (Aviv Keller) [#52861](https://github.com/nodejs/node/pull/52861)
* \[[`89aeae63bd`](https://github.com/nodejs/node/commit/89aeae63bd)] - **doc**: remove `parseREPLKeyword` from REPL documentation (Aviv Keller) [#54749](https://github.com/nodejs/node/pull/54749)
* \[[`b3e0490b8b`](https://github.com/nodejs/node/commit/b3e0490b8b)] - **doc**: add missing EventSource docs to globals (Matthew Aitken) [#55022](https://github.com/nodejs/node/pull/55022)
* \[[`516c775fa5`](https://github.com/nodejs/node/commit/516c775fa5)] - **doc**: cover --experimental-test-module-mocks flag (Jonathan Sharpe) [#55021](https://github.com/nodejs/node/pull/55021)
* \[[`4244f1a269`](https://github.com/nodejs/node/commit/4244f1a269)] - **doc**: add more details for localStorage and sessionStorage (Batuhan Tomo) [#53881](https://github.com/nodejs/node/pull/53881)
* \[[`39a728c2e3`](https://github.com/nodejs/node/commit/39a728c2e3)] - **doc**: change backporting guide with updated info (Aviv Keller) [#53746](https://github.com/nodejs/node/pull/53746)
* \[[`3a5fe95ad7`](https://github.com/nodejs/node/commit/3a5fe95ad7)] - **doc**: add missing definitions to `internal-api.md` (Aviv Keller) [#53303](https://github.com/nodejs/node/pull/53303)
* \[[`f2d74a26a3`](https://github.com/nodejs/node/commit/f2d74a26a3)] - **doc**: fix history of `process.features` (Antoine du Hamel) [#54982](https://github.com/nodejs/node/pull/54982)
* \[[`29866ca438`](https://github.com/nodejs/node/commit/29866ca438)] - **doc**: fix typo callsite.lineNumber (Rafael Gonzaga) [#54969](https://github.com/nodejs/node/pull/54969)
* \[[`c1d73abd29`](https://github.com/nodejs/node/commit/c1d73abd29)] - **doc**: update documentation for externalizing deps (Michael Dawson) [#54792](https://github.com/nodejs/node/pull/54792)
* \[[`eca9668231`](https://github.com/nodejs/node/commit/eca9668231)] - **doc**: add documentation for process.features (Marco Ippolito) [#54897](https://github.com/nodejs/node/pull/54897)
* \[[`0fb446e207`](https://github.com/nodejs/node/commit/0fb446e207)] - **esm**: do not interpret `"main"` as a URL (Antoine du Hamel) [#55003](https://github.com/nodejs/node/pull/55003)
* \[[`be2fe4b249`](https://github.com/nodejs/node/commit/be2fe4b249)] - **events**: allow null/undefined eventInitDict (Matthew Aitken) [#54643](https://github.com/nodejs/node/pull/54643)
* \[[`cb47e169a0`](https://github.com/nodejs/node/commit/cb47e169a0)] - **events**: return `currentTarget` when dispatching (Matthew Aitken) [#54642](https://github.com/nodejs/node/pull/54642)
* \[[`dbfae3fe14`](https://github.com/nodejs/node/commit/dbfae3fe14)] - **fs**: acknowledge `signal` option in `filehandle.createReadStream()` (Livia Medeiros) [#55148](https://github.com/nodejs/node/pull/55148)
* \[[`1c94725c07`](https://github.com/nodejs/node/commit/1c94725c07)] - **fs**: check subdir correctly in cpSync (Jason Zhang) [#55033](https://github.com/nodejs/node/pull/55033)
* \[[`79ffefab2a`](https://github.com/nodejs/node/commit/79ffefab2a)] - **fs**: convert to u8 string for filesystem path (Jason Zhang) [#54653](https://github.com/nodejs/node/pull/54653)
* \[[`914db60159`](https://github.com/nodejs/node/commit/914db60159)] - **(SEMVER-MINOR)** **http2**: expose nghttp2\_option\_set\_stream\_reset\_rate\_limit as an option (Maël Nison) [#54875](https://github.com/nodejs/node/pull/54875)
* \[[`08b5e6c794`](https://github.com/nodejs/node/commit/08b5e6c794)] - **lib**: fix module print timing when specifier includes `"` (Antoine du Hamel) [#55150](https://github.com/nodejs/node/pull/55150)
* \[[`bf7d7aef4b`](https://github.com/nodejs/node/commit/bf7d7aef4b)] - **lib**: fix typos (Nathan Baulch) [#55065](https://github.com/nodejs/node/pull/55065)
* \[[`d803355d92`](https://github.com/nodejs/node/commit/d803355d92)] - **lib**: prefer optional chaining (Aviv Keller) [#55045](https://github.com/nodejs/node/pull/55045)
* \[[`d4873bcd6d`](https://github.com/nodejs/node/commit/d4873bcd6d)] - **lib**: remove lib/internal/idna.js (Yagiz Nizipli) [#55050](https://github.com/nodejs/node/pull/55050)
* \[[`f7c3b03759`](https://github.com/nodejs/node/commit/f7c3b03759)] - **(SEMVER-MINOR)** **lib**: propagate aborted state to dependent signals before firing events (jazelly) [#54826](https://github.com/nodejs/node/pull/54826)
* \[[`397ae418db`](https://github.com/nodejs/node/commit/397ae418db)] - **lib**: the REPL should survive deletion of Array.prototype methods (Jordan Harband) [#31457](https://github.com/nodejs/node/pull/31457)
* \[[`566179c9ec`](https://github.com/nodejs/node/commit/566179c9ec)] - **lib, tools**: remove duplicate requires (Aviv Keller) [#54987](https://github.com/nodejs/node/pull/54987)
* \[[`c9a1bbbef2`](https://github.com/nodejs/node/commit/c9a1bbbef2)] - **meta**: move one or more collaborators to emeritus (Node.js GitHub Bot) [#55300](https://github.com/nodejs/node/pull/55300)
* \[[`d7b73bbd1d`](https://github.com/nodejs/node/commit/d7b73bbd1d)] - **meta**: bump mozilla-actions/sccache-action from 0.0.5 to 0.0.6 (dependabot\[bot]) [#55225](https://github.com/nodejs/node/pull/55225)
* \[[`0f4269faa9`](https://github.com/nodejs/node/commit/0f4269faa9)] - **meta**: bump actions/checkout from 4.1.7 to 4.2.0 (dependabot\[bot]) [#55224](https://github.com/nodejs/node/pull/55224)
* \[[`33be1990d8`](https://github.com/nodejs/node/commit/33be1990d8)] - **meta**: bump actions/setup-node from 4.0.3 to 4.0.4 (dependabot\[bot]) [#55223](https://github.com/nodejs/node/pull/55223)
* \[[`f5b4ae5bf8`](https://github.com/nodejs/node/commit/f5b4ae5bf8)] - **meta**: bump peter-evans/create-pull-request from 7.0.1 to 7.0.5 (dependabot\[bot]) [#55219](https://github.com/nodejs/node/pull/55219)
* \[[`1985d9016e`](https://github.com/nodejs/node/commit/1985d9016e)] - **meta**: add mailmap entry for abmusse (Abdirahim Musse) [#55182](https://github.com/nodejs/node/pull/55182)
* \[[`93b215d5e6`](https://github.com/nodejs/node/commit/93b215d5e6)] - **meta**: add more information about nightly releases (Aviv Keller) [#55084](https://github.com/nodejs/node/pull/55084)
* \[[`aeae5973c3`](https://github.com/nodejs/node/commit/aeae5973c3)] - **meta**: add `linux` to OS labels in collaborator guide (Aviv Keller) [#54986](https://github.com/nodejs/node/pull/54986)
* \[[`4fb2c3baa8`](https://github.com/nodejs/node/commit/4fb2c3baa8)] - **meta**: remove never-used workflow trigger (Aviv Keller) [#54983](https://github.com/nodejs/node/pull/54983)
* \[[`e1f36d0da8`](https://github.com/nodejs/node/commit/e1f36d0da8)] - **meta**: remove unneeded ignore rules from ruff (Aviv Keller) [#54360](https://github.com/nodejs/node/pull/54360)
* \[[`ce0d0c1ec8`](https://github.com/nodejs/node/commit/ce0d0c1ec8)] - **meta**: remove `build-windows.yml` (Aviv Keller) [#54662](https://github.com/nodejs/node/pull/54662)
* \[[`ca67c97f33`](https://github.com/nodejs/node/commit/ca67c97f33)] - **meta**: add links to alternative issue trackers (Aviv Keller) [#54401](https://github.com/nodejs/node/pull/54401)
* \[[`6fcac73738`](https://github.com/nodejs/node/commit/6fcac73738)] - **module**: wrap swc error in ERR\_INVALID\_TYPESCRIPT\_SYNTAX (Marco Ippolito) [#55316](https://github.com/nodejs/node/pull/55316)
* \[[`0412ac8bf3`](https://github.com/nodejs/node/commit/0412ac8bf3)] - **module**: add internal type def for `flushCompileCache` (Jacob Smith) [#55226](https://github.com/nodejs/node/pull/55226)
* \[[`32261fc98a`](https://github.com/nodejs/node/commit/32261fc98a)] - **(SEMVER-MINOR)** **module**: support loading entrypoint as url (RedYetiDev) [#54933](https://github.com/nodejs/node/pull/54933)
* \[[`111261e245`](https://github.com/nodejs/node/commit/111261e245)] - **(SEMVER-MINOR)** **module**: implement the "module-sync" exports condition (Joyee Cheung) [#54648](https://github.com/nodejs/node/pull/54648)
* \[[`b6fc9adf5b`](https://github.com/nodejs/node/commit/b6fc9adf5b)] - **module**: remove duplicated import (Aviv Keller) [#54942](https://github.com/nodejs/node/pull/54942)
* \[[`06957ff355`](https://github.com/nodejs/node/commit/06957ff355)] - **(SEMVER-MINOR)** **module**: implement flushCompileCache() (Joyee Cheung) [#54971](https://github.com/nodejs/node/pull/54971)
* \[[`2dcf70c347`](https://github.com/nodejs/node/commit/2dcf70c347)] - **(SEMVER-MINOR)** **module**: throw when invalid argument is passed to enableCompileCache() (Joyee Cheung) [#54971](https://github.com/nodejs/node/pull/54971)
* \[[`f9b19d7c44`](https://github.com/nodejs/node/commit/f9b19d7c44)] - **(SEMVER-MINOR)** **module**: write compile cache to temporary file and then rename it (Joyee Cheung) [#54971](https://github.com/nodejs/node/pull/54971)
* \[[`1d169764db`](https://github.com/nodejs/node/commit/1d169764db)] - **module**: report unfinished TLA in ambiguous modules (Antoine du Hamel) [#54980](https://github.com/nodejs/node/pull/54980)
* \[[`c89c93496d`](https://github.com/nodejs/node/commit/c89c93496d)] - **module**: refator ESM loader for adding future synchronous hooks (Joyee Cheung) [#54769](https://github.com/nodejs/node/pull/54769)
* \[[`108cef22e6`](https://github.com/nodejs/node/commit/108cef22e6)] - **module**: remove bogus assertion in CJS entrypoint handling with --import (Joyee Cheung) [#54592](https://github.com/nodejs/node/pull/54592)
* \[[`67ecb10c78`](https://github.com/nodejs/node/commit/67ecb10c78)] - **module**: fix discrepancy between .ts and .js (Marco Ippolito) [#54461](https://github.com/nodejs/node/pull/54461)
* \[[`3300d5990f`](https://github.com/nodejs/node/commit/3300d5990f)] - **os**: use const with early return for path (Trivikram Kamat) [#54959](https://github.com/nodejs/node/pull/54959)
* \[[`90cce6ec7c`](https://github.com/nodejs/node/commit/90cce6ec7c)] - **path**: remove repetitive conditional operator in `posix.resolve` (Wiyeong Seo) [#54835](https://github.com/nodejs/node/pull/54835)
* \[[`cbfc980f89`](https://github.com/nodejs/node/commit/cbfc980f89)] - **perf\_hooks**: add missing type argument to getEntriesByName (Luke Taher) [#54767](https://github.com/nodejs/node/pull/54767)
* \[[`e95163b170`](https://github.com/nodejs/node/commit/e95163b170)] - **(SEMVER-MINOR)** **process**: add process.features.require\_module (Joyee Cheung) [#55241](https://github.com/nodejs/node/pull/55241)
* \[[`0655d3a384`](https://github.com/nodejs/node/commit/0655d3a384)] - **process**: fix `process.features.typescript` when Amaro is unavailable (Antoine du Hamel) [#55323](https://github.com/nodejs/node/pull/55323)
* \[[`4050f68e5d`](https://github.com/nodejs/node/commit/4050f68e5d)] - **(SEMVER-MINOR)** **process**: add `process.features.typescript` (Aviv Keller) [#54295](https://github.com/nodejs/node/pull/54295)
* \[[`75073c50ae`](https://github.com/nodejs/node/commit/75073c50ae)] - **quic**: start adding in the internal quic js api (James M Snell) [#53256](https://github.com/nodejs/node/pull/53256)
* \[[`538b1eb5b0`](https://github.com/nodejs/node/commit/538b1eb5b0)] - **repl**: catch `\v` and `\r` in new-line detection (Aviv Keller) [#54512](https://github.com/nodejs/node/pull/54512)
* \[[`57a9d3f15e`](https://github.com/nodejs/node/commit/57a9d3f15e)] - **sqlite**: disable DQS misfeature by default (Tobias Nießen) [#55297](https://github.com/nodejs/node/pull/55297)
* \[[`c126543374`](https://github.com/nodejs/node/commit/c126543374)] - **sqlite**: make sourceSQL and expandedSQL string-valued properties (Tobias Nießen) [#54721](https://github.com/nodejs/node/pull/54721)
* \[[`67f5f46c56`](https://github.com/nodejs/node/commit/67f5f46c56)] - **sqlite**: enable foreign key constraints by default (Tobias Nießen) [#54777](https://github.com/nodejs/node/pull/54777)
* \[[`09999491bf`](https://github.com/nodejs/node/commit/09999491bf)] - **src**: handle errors correctly in webstorage (Michaël Zasso) [#54544](https://github.com/nodejs/node/pull/54544)
* \[[`295c17c4ea`](https://github.com/nodejs/node/commit/295c17c4ea)] - **src**: make minor tweaks to quic c++ for c++20 (James M Snell) [#53256](https://github.com/nodejs/node/pull/53256)
* \[[`b1d47d06f9`](https://github.com/nodejs/node/commit/b1d47d06f9)] - **src**: apply getCallSite optimization (RafaelGSS) [#55174](https://github.com/nodejs/node/pull/55174)
* \[[`d6bcc44829`](https://github.com/nodejs/node/commit/d6bcc44829)] - **src**: modernize likely/unlikely hints (Yagiz Nizipli) [#55155](https://github.com/nodejs/node/pull/55155)
* \[[`1af5ad61ca`](https://github.com/nodejs/node/commit/1af5ad61ca)] - **src**: fixup Error.stackTraceLimit during snapshot building (Joyee Cheung) [#55121](https://github.com/nodejs/node/pull/55121)
* \[[`b229083235`](https://github.com/nodejs/node/commit/b229083235)] - **src**: parse --stack-trace-limit and use it in --trace-\* flags (Joyee Cheung) [#55121](https://github.com/nodejs/node/pull/55121)
* \[[`942ad54e08`](https://github.com/nodejs/node/commit/942ad54e08)] - **src**: move more key handling to ncrypto (James M Snell) [#55108](https://github.com/nodejs/node/pull/55108)
* \[[`0bb5584288`](https://github.com/nodejs/node/commit/0bb5584288)] - **src**: add receiver to fast api callback methods (Carlos Espa) [#54408](https://github.com/nodejs/node/pull/54408)
* \[[`706e9611f0`](https://github.com/nodejs/node/commit/706e9611f0)] - **src**: fix typos (Nathan Baulch) [#55064](https://github.com/nodejs/node/pull/55064)
* \[[`a96d5d1bcc`](https://github.com/nodejs/node/commit/a96d5d1bcc)] - **src**: move more stuff over to use Maybe\<void> (James M Snell) [#54831](https://github.com/nodejs/node/pull/54831)
* \[[`ee0a98b5a2`](https://github.com/nodejs/node/commit/ee0a98b5a2)] - **src**: decode native error messages as UTF-8 (Joyee Cheung) [#55024](https://github.com/nodejs/node/pull/55024)
* \[[`1fc8edecf8`](https://github.com/nodejs/node/commit/1fc8edecf8)] - **src**: update clang-tidy and focus on modernization (Yagiz Nizipli) [#53757](https://github.com/nodejs/node/pull/53757)
* \[[`3a1485a1a3`](https://github.com/nodejs/node/commit/3a1485a1a3)] - **src**: move evp stuff to ncrypto (James M Snell) [#54911](https://github.com/nodejs/node/pull/54911)
* \[[`9ae80e1e4d`](https://github.com/nodejs/node/commit/9ae80e1e4d)] - **src**: revert filesystem::path changes (Yagiz Nizipli) [#55015](https://github.com/nodejs/node/pull/55015)
* \[[`465d05018a`](https://github.com/nodejs/node/commit/465d05018a)] - **src**: mark node --run as stable (Yagiz Nizipli) [#53763](https://github.com/nodejs/node/pull/53763)
* \[[`ef546c872c`](https://github.com/nodejs/node/commit/ef546c872c)] - **src**: cleanup per env handles directly without a list (Chengzhong Wu) [#54993](https://github.com/nodejs/node/pull/54993)
* \[[`0876f78411`](https://github.com/nodejs/node/commit/0876f78411)] - **src**: add unistd.h import if node posix credentials is defined (Jonas) [#54528](https://github.com/nodejs/node/pull/54528)
* \[[`284db53866`](https://github.com/nodejs/node/commit/284db53866)] - **src**: remove duplicate code setting AF\_INET (He Yang) [#54939](https://github.com/nodejs/node/pull/54939)
* \[[`f332c4c4fc`](https://github.com/nodejs/node/commit/f332c4c4fc)] - **src**: use `Maybe<void>` where bool isn't needed (Michaël Zasso) [#54575](https://github.com/nodejs/node/pull/54575)
* \[[`c7ed2ff920`](https://github.com/nodejs/node/commit/c7ed2ff920)] - **stream**: handle undefined chunks correctly in decode stream (devstone) [#55153](https://github.com/nodejs/node/pull/55153)
* \[[`a9675a0cbc`](https://github.com/nodejs/node/commit/a9675a0cbc)] - **stream**: treat null asyncIterator as undefined (Jason Zhang) [#55119](https://github.com/nodejs/node/pull/55119)
* \[[`bf69ae1406`](https://github.com/nodejs/node/commit/bf69ae1406)] - **stream**: set stream prototype to closest transferable superclass (Jason Zhang) [#55067](https://github.com/nodejs/node/pull/55067)
* \[[`3273707a3a`](https://github.com/nodejs/node/commit/3273707a3a)] - **test**: fix tests when Amaro is unavailable (Richard Lau) [#55320](https://github.com/nodejs/node/pull/55320)
* \[[`ff3cc3b2ab`](https://github.com/nodejs/node/commit/ff3cc3b2ab)] - **test**: use more informative errors in `test-runner-cli` (Antoine du Hamel) [#55321](https://github.com/nodejs/node/pull/55321)
* \[[`17d2f9de6d`](https://github.com/nodejs/node/commit/17d2f9de6d)] - **test**: make `test-loaders-workers-spawned` less flaky (Antoine du Hamel) [#55172](https://github.com/nodejs/node/pull/55172)
* \[[`1b1104e69b`](https://github.com/nodejs/node/commit/1b1104e69b)] - **test**: add resource to internal module stat test (RafaelGSS) [#55157](https://github.com/nodejs/node/pull/55157)
* \[[`b36f8c2146`](https://github.com/nodejs/node/commit/b36f8c2146)] - **test**: update multiple assert tests to use node:test (James M Snell) [#54585](https://github.com/nodejs/node/pull/54585)
* \[[`1b30f7fdd6`](https://github.com/nodejs/node/commit/1b30f7fdd6)] - **test**: move coverage source map tests to new file (Aviv Keller) [#55123](https://github.com/nodejs/node/pull/55123)
* \[[`ce67e7b5b3`](https://github.com/nodejs/node/commit/ce67e7b5b3)] - **test**: adding more tests for strip-types (Kevin Toshihiro Uehara) [#54929](https://github.com/nodejs/node/pull/54929)
* \[[`a57c8ba3ef`](https://github.com/nodejs/node/commit/a57c8ba3ef)] - **test**: update wpt test for encoding (devstone) [#55151](https://github.com/nodejs/node/pull/55151)
* \[[`65fbe94d45`](https://github.com/nodejs/node/commit/65fbe94d45)] - **test**: add `escapePOSIXShell` util (Antoine du Hamel) [#55125](https://github.com/nodejs/node/pull/55125)
* \[[`cc8838252e`](https://github.com/nodejs/node/commit/cc8838252e)] - **test**: remove unnecessary `await` in test-watch-mode (Wuli) [#55142](https://github.com/nodejs/node/pull/55142)
* \[[`9aeba48bf0`](https://github.com/nodejs/node/commit/9aeba48bf0)] - **test**: fix typos (Nathan Baulch) [#55063](https://github.com/nodejs/node/pull/55063)
* \[[`0999b5e493`](https://github.com/nodejs/node/commit/0999b5e493)] - **test**: remove duplicated test descriptions (Christos Koutsiaris) [#54140](https://github.com/nodejs/node/pull/54140)
* \[[`e99d4a4cb8`](https://github.com/nodejs/node/commit/e99d4a4cb8)] - **test**: deflake test/pummel/test-timers.js (jakecastelli) [#55098](https://github.com/nodejs/node/pull/55098)
* \[[`fb8470afd7`](https://github.com/nodejs/node/commit/fb8470afd7)] - **test**: deflake test-http-remove-header-stays-removed (Luigi Pinca) [#55004](https://github.com/nodejs/node/pull/55004)
* \[[`e879c5edf2`](https://github.com/nodejs/node/commit/e879c5edf2)] - **test**: fix test-tls-junk-closes-server (Michael Dawson) [#55089](https://github.com/nodejs/node/pull/55089)
* \[[`b885f0583c`](https://github.com/nodejs/node/commit/b885f0583c)] - **test**: fix more tests that fail when path contains a space (Antoine du Hamel) [#55088](https://github.com/nodejs/node/pull/55088)
* \[[`85f1187942`](https://github.com/nodejs/node/commit/85f1187942)] - **test**: fix `assertSnapshot` when path contains a quote (Antoine du Hamel) [#55087](https://github.com/nodejs/node/pull/55087)
* \[[`fdae57f1e1`](https://github.com/nodejs/node/commit/fdae57f1e1)] - **test**: fix some tests when path contains `%` (Antoine du Hamel) [#55082](https://github.com/nodejs/node/pull/55082)
* \[[`36c9ea8912`](https://github.com/nodejs/node/commit/36c9ea8912)] - _**Revert**_ "**test**: mark test-fs-watch-non-recursive flaky on Windows" (Luigi Pinca) [#55079](https://github.com/nodejs/node/pull/55079)
* \[[`80da5993cc`](https://github.com/nodejs/node/commit/80da5993cc)] - **test**: remove interval and give more time to unsync (Pietro Marchini) [#55006](https://github.com/nodejs/node/pull/55006)
* \[[`93c23e74b3`](https://github.com/nodejs/node/commit/93c23e74b3)] - **test**: deflake test-inspector-strip-types (Luigi Pinca) [#55058](https://github.com/nodejs/node/pull/55058)
* \[[`43bbca2c08`](https://github.com/nodejs/node/commit/43bbca2c08)] - **test**: make `test-runner-assert` more robust (Aviv Keller) [#55036](https://github.com/nodejs/node/pull/55036)
* \[[`268f1ec08f`](https://github.com/nodejs/node/commit/268f1ec08f)] - **test**: update tls test to support OpenSSL32 (Michael Dawson) [#55030](https://github.com/nodejs/node/pull/55030)
* \[[`a50dd21423`](https://github.com/nodejs/node/commit/a50dd21423)] - **test**: do not assume `process.execPath` contains no spaces (Antoine du Hamel) [#55028](https://github.com/nodejs/node/pull/55028)
* \[[`c56e324cb8`](https://github.com/nodejs/node/commit/c56e324cb8)] - **test**: fix `test-vm-context-dont-contextify` when path contains a space (Antoine du Hamel) [#55026](https://github.com/nodejs/node/pull/55026)
* \[[`6d42e44264`](https://github.com/nodejs/node/commit/6d42e44264)] - **test**: adjust tls-set-ciphers for OpenSSL32 (Michael Dawson) [#55016](https://github.com/nodejs/node/pull/55016)
* \[[`22e601a76c`](https://github.com/nodejs/node/commit/22e601a76c)] - **test**: add `util.stripVTControlCharacters` test (RedYetiDev) [#54865](https://github.com/nodejs/node/pull/54865)
* \[[`a6796696d7`](https://github.com/nodejs/node/commit/a6796696d7)] - **test**: improve coverage for timer promises schedular (Aviv Keller) [#53370](https://github.com/nodejs/node/pull/53370)
* \[[`9506f77b3e`](https://github.com/nodejs/node/commit/9506f77b3e)] - **test**: remove `getCallSite` from common (RedYetiDev) [#54947](https://github.com/nodejs/node/pull/54947)
* \[[`20d3a806ea`](https://github.com/nodejs/node/commit/20d3a806ea)] - **test**: remove unused common utilities (RedYetiDev) [#54825](https://github.com/nodejs/node/pull/54825)
* \[[`341b6d9b94`](https://github.com/nodejs/node/commit/341b6d9b94)] - **test**: deflake test-http-header-overflow (Luigi Pinca) [#54978](https://github.com/nodejs/node/pull/54978)
* \[[`1e53c10853`](https://github.com/nodejs/node/commit/1e53c10853)] - **test**: fix `soucre` to `source` (Aviv Keller) [#55038](https://github.com/nodejs/node/pull/55038)
* \[[`6843ca7e0d`](https://github.com/nodejs/node/commit/6843ca7e0d)] - **test**: add asserts to validate test assumptions (Michael Dawson) [#54997](https://github.com/nodejs/node/pull/54997)
* \[[`98ff615c5e`](https://github.com/nodejs/node/commit/98ff615c5e)] - **test**: add runner watch mode isolation tests (Pietro Marchini) [#54888](https://github.com/nodejs/node/pull/54888)
* \[[`327a8f7b59`](https://github.com/nodejs/node/commit/327a8f7b59)] - **test**: fix invalid wasm test (Aviv Keller) [#54935](https://github.com/nodejs/node/pull/54935)
* \[[`5b012f544c`](https://github.com/nodejs/node/commit/5b012f544c)] - **test**: move test-http-max-sockets to parallel (Luigi Pinca) [#54977](https://github.com/nodejs/node/pull/54977)
* \[[`22b413910e`](https://github.com/nodejs/node/commit/22b413910e)] - **test**: remove test-http-max-sockets flaky designation (Luigi Pinca) [#54976](https://github.com/nodejs/node/pull/54976)
* \[[`62b8640550`](https://github.com/nodejs/node/commit/62b8640550)] - **test**: refactor test-whatwg-webstreams-encoding to be shorter (David Dong) [#54569](https://github.com/nodejs/node/pull/54569)
* \[[`1f11d68173`](https://github.com/nodejs/node/commit/1f11d68173)] - **test**: adjust key sizes to support OpenSSL32 (Michael Dawson) [#54972](https://github.com/nodejs/node/pull/54972)
* \[[`90a87ca8f7`](https://github.com/nodejs/node/commit/90a87ca8f7)] - **test**: update test to support OpenSSL32 (Michael Dawson) [#54968](https://github.com/nodejs/node/pull/54968)
* \[[`9b7834536a`](https://github.com/nodejs/node/commit/9b7834536a)] - **test**: update DOM events web platform tests (Matthew Aitken) [#54642](https://github.com/nodejs/node/pull/54642)
* \[[`1c001550a2`](https://github.com/nodejs/node/commit/1c001550a2)] - **test,crypto**: update WebCryptoAPI WPT (Filip Skokan) [#55029](https://github.com/nodejs/node/pull/55029)
* \[[`800f7c44ed`](https://github.com/nodejs/node/commit/800f7c44ed)] - **test\_runner**: throw on invalid source map (Aviv Keller) [#55055](https://github.com/nodejs/node/pull/55055)
* \[[`0f7e3f017f`](https://github.com/nodejs/node/commit/0f7e3f017f)] - **test\_runner**: assert entry is a valid object (Edigleysson Silva (Edy)) [#55231](https://github.com/nodejs/node/pull/55231)
* \[[`c308862d2e`](https://github.com/nodejs/node/commit/c308862d2e)] - **test\_runner**: avoid spread operator on arrays (Antoine du Hamel) [#55143](https://github.com/nodejs/node/pull/55143)
* \[[`12401972b7`](https://github.com/nodejs/node/commit/12401972b7)] - **test\_runner**: support typescript files in default glob (Aviv Keller) [#55081](https://github.com/nodejs/node/pull/55081)
* \[[`19cfa3140f`](https://github.com/nodejs/node/commit/19cfa3140f)] - **test\_runner**: close and flush destinations on forced exit (Colin Ihrig) [#55099](https://github.com/nodejs/node/pull/55099)
* \[[`86f7cb802d`](https://github.com/nodejs/node/commit/86f7cb802d)] - **(SEMVER-MINOR)** **test\_runner**: support custom arguments in `run()` (Aviv Keller) [#55126](https://github.com/nodejs/node/pull/55126)
* \[[`7eaeba499a`](https://github.com/nodejs/node/commit/7eaeba499a)] - **test\_runner**: fix mocking modules with quote in their URL (Antoine du Hamel) [#55083](https://github.com/nodejs/node/pull/55083)
* \[[`8818c6c88a`](https://github.com/nodejs/node/commit/8818c6c88a)] - **test\_runner**: report error on missing sourcemap source (Aviv Keller) [#55037](https://github.com/nodejs/node/pull/55037)
* \[[`b62f2f8259`](https://github.com/nodejs/node/commit/b62f2f8259)] - **(SEMVER-MINOR)** **test\_runner**: add 'test:summary' event (Colin Ihrig) [#54851](https://github.com/nodejs/node/pull/54851)
* \[[`449dad0db0`](https://github.com/nodejs/node/commit/449dad0db0)] - **test\_runner**: use `test:` symbol on second print of parent test (RedYetiDev) [#54956](https://github.com/nodejs/node/pull/54956)
* \[[`4b962a78c7`](https://github.com/nodejs/node/commit/4b962a78c7)] - **test\_runner**: replace ansi clear with ansi reset (Pietro Marchini) [#55013](https://github.com/nodejs/node/pull/55013)
* \[[`d7c708aec5`](https://github.com/nodejs/node/commit/d7c708aec5)] - **(SEMVER-MINOR)** **test\_runner**: add support for coverage via run() (Chemi Atlow) [#53937](https://github.com/nodejs/node/pull/53937)
* \[[`93c6c90219`](https://github.com/nodejs/node/commit/93c6c90219)] - **test\_runner**: support typescript module mocking (Marco Ippolito) [#54878](https://github.com/nodejs/node/pull/54878)
* \[[`1daec9a63f`](https://github.com/nodejs/node/commit/1daec9a63f)] - **test\_runner**: avoid coverage report partial file names (Pietro Marchini) [#54379](https://github.com/nodejs/node/pull/54379)
* \[[`d51e5a8667`](https://github.com/nodejs/node/commit/d51e5a8667)] - **tools**: enforce errors to not be documented in legacy section (Aviv Keller) [#55218](https://github.com/nodejs/node/pull/55218)
* \[[`6a7d201b80`](https://github.com/nodejs/node/commit/6a7d201b80)] - **tools**: update gyp-next to 0.18.2 (Node.js GitHub Bot) [#55160](https://github.com/nodejs/node/pull/55160)
* \[[`c988e7e2e5`](https://github.com/nodejs/node/commit/c988e7e2e5)] - **tools**: bump the eslint group in /tools/eslint with 4 updates (dependabot\[bot]) [#55227](https://github.com/nodejs/node/pull/55227)
* \[[`7982d3d4ed`](https://github.com/nodejs/node/commit/7982d3d4ed)] - **tools**: only check teams on the default branch (Antoine du Hamel) [#55124](https://github.com/nodejs/node/pull/55124)
* \[[`60a35eddb0`](https://github.com/nodejs/node/commit/60a35eddb0)] - **tools**: make `choco install` script more readable (Aviv Keller) [#54002](https://github.com/nodejs/node/pull/54002)
* \[[`b7b1fa6dd3`](https://github.com/nodejs/node/commit/b7b1fa6dd3)] - **tools**: bump Rollup from 4.18.1 to 4.22.4 for `lint-md` (dependabot\[bot]) [#55093](https://github.com/nodejs/node/pull/55093)
* \[[`3304bf387f`](https://github.com/nodejs/node/commit/3304bf387f)] - **tools**: unlock versions of irrelevant DB deps (Michaël Zasso) [#55042](https://github.com/nodejs/node/pull/55042)
* \[[`65c376a819`](https://github.com/nodejs/node/commit/65c376a819)] - **tools**: remove redudant code from eslint require rule (Aviv Keller) [#54892](https://github.com/nodejs/node/pull/54892)
* \[[`295f684b69`](https://github.com/nodejs/node/commit/295f684b69)] - **tools**: update error message for ICU in license-builder (Aviv Keller) [#54742](https://github.com/nodejs/node/pull/54742)
* \[[`ce4b6e403d`](https://github.com/nodejs/node/commit/ce4b6e403d)] - **tools**: refactor js2c.cc to use c++20 (Yagiz Nizipli) [#54849](https://github.com/nodejs/node/pull/54849)
* \[[`31f0ef6ea3`](https://github.com/nodejs/node/commit/31f0ef6ea3)] - **tools**: bump the eslint group in /tools/eslint with 7 updates (dependabot\[bot]) [#54821](https://github.com/nodejs/node/pull/54821)
* \[[`676d0a09a0`](https://github.com/nodejs/node/commit/676d0a09a0)] - **tools**: update github\_reporter to 1.7.1 (Node.js GitHub Bot) [#54951](https://github.com/nodejs/node/pull/54951)
* \[[`0f01f38aea`](https://github.com/nodejs/node/commit/0f01f38aea)] - **tty**: fix links for terminal colors (Aviv Keller) [#54596](https://github.com/nodejs/node/pull/54596)
* \[[`d264639f5f`](https://github.com/nodejs/node/commit/d264639f5f)] - **util**: update ansi regex (Aviv Keller) [#54865](https://github.com/nodejs/node/pull/54865)
* \[[`ea7aaf37bf`](https://github.com/nodejs/node/commit/ea7aaf37bf)] - **v8**: out of bounds copy (Robert Nagy) [#55261](https://github.com/nodejs/node/pull/55261)
* \[[`fa695facf5`](https://github.com/nodejs/node/commit/fa695facf5)] - **watch**: preserve output when gracefully restarted (Théo LUDWIG) [#54323](https://github.com/nodejs/node/pull/54323)
* \[[`5fda4a1498`](https://github.com/nodejs/node/commit/5fda4a1498)] - **(SEMVER-MINOR)** **worker**: add `markAsUncloneable` api (Jason Zhang) [#55234](https://github.com/nodejs/node/pull/55234)
* \[[`d65334c454`](https://github.com/nodejs/node/commit/d65334c454)] - **worker**: throw InvalidStateError in postMessage after close (devstone) [#55206](https://github.com/nodejs/node/pull/55206)
* \[[`fc90d7c63a`](https://github.com/nodejs/node/commit/fc90d7c63a)] - **worker**: handle `--input-type` more consistently (Antoine du Hamel) [#54979](https://github.com/nodejs/node/pull/54979)
* \[[`a9fa2da870`](https://github.com/nodejs/node/commit/a9fa2da870)] - **zlib**: throw brotli initialization error from c++ (Yagiz Nizipli) [#54698](https://github.com/nodejs/node/pull/54698)
* \[[`9abd1c7288`](https://github.com/nodejs/node/commit/9abd1c7288)] - **zlib**: remove prototype primordials usage (Yagiz Nizipli) [#54695](https://github.com/nodejs/node/pull/54695)

<a id="22.9.0"></a>

## 2024-09-17, Version 22.9.0 (Current), @RafaelGSS

### New API to retrieve execution Stack Trace

A new API `getCallSite` has been introduced to the `util` module. This API allows users
to retrieve the stacktrace of the current execution. Example:

```js
const util = require('node:util');

function exampleFunction() {
  const callSites = util.getCallSite();

  console.log('Call Sites:');
  callSites.forEach((callSite, index) => {
    console.log(`CallSite ${index + 1}:`);
    console.log(`Function Name: ${callSite.functionName}`);
    console.log(`Script Name: ${callSite.scriptName}`);
    console.log(`Line Number: ${callSite.lineNumber}`);
    console.log(`Column Number: ${callSite.column}`);
  });
  // CallSite 1:
  // Function Name: exampleFunction
  // Script Name: /home/example.js
  // Line Number: 5
  // Column Number: 26

  // CallSite 2:
  // Function Name: anotherFunction
  // Script Name: /home/example.js
  // Line Number: 22
  // Column Number: 3

  // ...
}

// A function to simulate another stack layer
function anotherFunction() {
  exampleFunction();
}

anotherFunction();
```

Thanks to Rafael Gonzaga for making this work on [#54380](https://github.com/nodejs/node/pull/54380).

### Disable V8 Maglev

We have seen several crashes/unexpected JS behaviors with maglev on v22
(which ships V8 v12.4). The bugs lie in the codegen so it would be difficult for
users to work around them or even figure out where the bugs are coming from.
Some bugs are fixed in the upstream while some others probably remain.

As v22 will get stuck with V8 v12.4 as LTS, it will be increasingly difficult to
backport patches for them even if the bugs are fixed. So disable it by default
on v22 to reduce the churn and troubles for users.

Thanks to Joyee Cheung for making this work on [#54384](https://github.com/nodejs/node/pull/54384)

### Exposes X509\_V\_FLAG\_PARTIAL\_CHAIN to tls.createSecureContext

This releases introduces a new option to the API `tls.createSecureContext`. For
now on users can use `tls.createSecureContext({ allowPartialTrustChain: true })`
to treat intermediate (non-self-signed) certificates in the trust CA certificate
list as trusted.

Thanks to Anna Henningsen for making this work on [#54790](https://github.com/nodejs/node/pull/54790)

### Other Notable Changes

* \[[`5c9599af5a`](https://github.com/nodejs/node/commit/5c9599af5a)] - **src**: create handle scope in FastInternalModuleStat (Joyee Cheung) [#54384](https://github.com/nodejs/node/pull/54384)
* \[[`e2307d87e8`](https://github.com/nodejs/node/commit/e2307d87e8)] - **(SEMVER-MINOR)** **stream**: relocate the status checking code in the onwritecomplete (YoonSoo\_Shin) [#54032](https://github.com/nodejs/node/pull/54032)

### Deprecations

* \[[`8433032948`](https://github.com/nodejs/node/commit/8433032948)] - **repl**: doc-deprecate instantiating `node:repl` classes without `new` (Aviv Keller) [#54842](https://github.com/nodejs/node/pull/54842)
* \[[`8c4c85cf31`](https://github.com/nodejs/node/commit/8c4c85cf31)] - **zlib**: deprecate instantiating classes without new (Yagiz Nizipli) [#54708](https://github.com/nodejs/node/pull/54708)

### Commits

* \[[`027b0ffe84`](https://github.com/nodejs/node/commit/027b0ffe84)] - **async\_hooks**: add an InactiveAsyncContextFrame class (Bryan English) [#54510](https://github.com/nodejs/node/pull/54510)
* \[[`022767028e`](https://github.com/nodejs/node/commit/022767028e)] - **benchmark**: --no-warnings to avoid DEP/ExpWarn log (Rafael Gonzaga) [#54928](https://github.com/nodejs/node/pull/54928)
* \[[`af1988c147`](https://github.com/nodejs/node/commit/af1988c147)] - **benchmark**: add buffer.isAscii benchmark (RafaelGSS) [#54740](https://github.com/nodejs/node/pull/54740)
* \[[`40c6849964`](https://github.com/nodejs/node/commit/40c6849964)] - **benchmark**: add buffer.isUtf8 bench (RafaelGSS) [#54740](https://github.com/nodejs/node/pull/54740)
* \[[`237d7dfbde`](https://github.com/nodejs/node/commit/237d7dfbde)] - **benchmark**: add access async version to bench (Rafael Gonzaga) [#54747](https://github.com/nodejs/node/pull/54747)
* \[[`ebe91db827`](https://github.com/nodejs/node/commit/ebe91db827)] - **benchmark**: enhance dc publish benchmark (Rafael Gonzaga) [#54745](https://github.com/nodejs/node/pull/54745)
* \[[`060164485b`](https://github.com/nodejs/node/commit/060164485b)] - **benchmark**: add match and doesNotMatch bench (RafaelGSS) [#54734](https://github.com/nodejs/node/pull/54734)
* \[[`2844180c7e`](https://github.com/nodejs/node/commit/2844180c7e)] - **benchmark**: add rejects and doesNotReject bench (RafaelGSS) [#54734](https://github.com/nodejs/node/pull/54734)
* \[[`af7689ed02`](https://github.com/nodejs/node/commit/af7689ed02)] - **benchmark**: add throws and doesNotThrow bench (RafaelGSS) [#54734](https://github.com/nodejs/node/pull/54734)
* \[[`456a1fe222`](https://github.com/nodejs/node/commit/456a1fe222)] - **benchmark**: add strictEqual and notStrictEqual bench (RafaelGSS) [#54734](https://github.com/nodejs/node/pull/54734)
* \[[`721c63c858`](https://github.com/nodejs/node/commit/721c63c858)] - **benchmark**: adds groups to better separate benchmarks (Giovanni Bucci) [#54393](https://github.com/nodejs/node/pull/54393)
* \[[`68e45b406e`](https://github.com/nodejs/node/commit/68e45b406e)] - **benchmark,doc**: add CPU scaling governor to perf (Rafael Gonzaga) [#54723](https://github.com/nodejs/node/pull/54723)
* \[[`d19efd7a50`](https://github.com/nodejs/node/commit/d19efd7a50)] - **benchmark,doc**: mention bar.R to the list of scripts (Rafael Gonzaga) [#54722](https://github.com/nodejs/node/pull/54722)
* \[[`1fb67afa2f`](https://github.com/nodejs/node/commit/1fb67afa2f)] - **buffer**: fix out of range for toString (Jason Zhang) [#54553](https://github.com/nodejs/node/pull/54553)
* \[[`85b5ed5d41`](https://github.com/nodejs/node/commit/85b5ed5d41)] - **buffer**: re-enable Fast API for Buffer.write (Robert Nagy) [#54526](https://github.com/nodejs/node/pull/54526)
* \[[`9a075279ec`](https://github.com/nodejs/node/commit/9a075279ec)] - **build**: upgrade clang-format to v18 (Aviv Keller) [#53957](https://github.com/nodejs/node/pull/53957)
* \[[`69ec9d8d2b`](https://github.com/nodejs/node/commit/69ec9d8d2b)] - **build**: fix conflicting V8 object print flags (Daeyeon Jeong) [#54785](https://github.com/nodejs/node/pull/54785)
* \[[`948bba396c`](https://github.com/nodejs/node/commit/948bba396c)] - **build**: do not build with code cache for core coverage collection (Joyee Cheung) [#54633](https://github.com/nodejs/node/pull/54633)
* \[[`6200cf4fb6`](https://github.com/nodejs/node/commit/6200cf4fb6)] - **build**: don't store eslint locally (Aviv Keller) [#54231](https://github.com/nodejs/node/pull/54231)
* \[[`3b5ed97fe9`](https://github.com/nodejs/node/commit/3b5ed97fe9)] - **build**: turn off `-Wrestrict` (Richard Lau) [#54737](https://github.com/nodejs/node/pull/54737)
* \[[`e38e305a35`](https://github.com/nodejs/node/commit/e38e305a35)] - **build,win**: enable clang-cl compilation (Stefan Stojanovic) [#54655](https://github.com/nodejs/node/pull/54655)
* \[[`5bba0781b0`](https://github.com/nodejs/node/commit/5bba0781b0)] - **crypto**: reject dh,x25519,x448 in {Sign,Verify}Final (Huáng Jùnliàng) [#53774](https://github.com/nodejs/node/pull/53774)
* \[[`3981853c00`](https://github.com/nodejs/node/commit/3981853c00)] - **crypto**: return a clearer error when loading an unsupported pkcs12 (Tim Perry) [#54485](https://github.com/nodejs/node/pull/54485)
* \[[`02ac5376b9`](https://github.com/nodejs/node/commit/02ac5376b9)] - **crypto**: remove unused `kHashTypes` internal (Antoine du Hamel) [#54627](https://github.com/nodejs/node/pull/54627)
* \[[`323d9da3c9`](https://github.com/nodejs/node/commit/323d9da3c9)] - **deps**: update cjs-module-lexer to 1.4.1 (Node.js GitHub Bot) [#54846](https://github.com/nodejs/node/pull/54846)
* \[[`bf4bf7cc6b`](https://github.com/nodejs/node/commit/bf4bf7cc6b)] - **deps**: update simdutf to 5.5.0 (Node.js GitHub Bot) [#54434](https://github.com/nodejs/node/pull/54434)
* \[[`61047dd130`](https://github.com/nodejs/node/commit/61047dd130)] - **deps**: upgrade npm to 10.8.3 (npm team) [#54619](https://github.com/nodejs/node/pull/54619)
* \[[`2351da5034`](https://github.com/nodejs/node/commit/2351da5034)] - **deps**: update cjs-module-lexer to 1.4.0 (Node.js GitHub Bot) [#54713](https://github.com/nodejs/node/pull/54713)
* \[[`0659516823`](https://github.com/nodejs/node/commit/0659516823)] - **deps**: allow amaro to be externalizable (Michael Dawson) [#54646](https://github.com/nodejs/node/pull/54646)
* \[[`6a32645dbc`](https://github.com/nodejs/node/commit/6a32645dbc)] - **deps**: fix sign-compare warning in ncrypto (Cheng) [#54624](https://github.com/nodejs/node/pull/54624)
* \[[`8f62f19197`](https://github.com/nodejs/node/commit/8f62f19197)] - **doc**: fix broken Android building link (Niklas Wenzel) [#54922](https://github.com/nodejs/node/pull/54922)
* \[[`440c256d76`](https://github.com/nodejs/node/commit/440c256d76)] - **doc**: add support link for aduh95 (Antoine du Hamel) [#54866](https://github.com/nodejs/node/pull/54866)
* \[[`56aca2a1ca`](https://github.com/nodejs/node/commit/56aca2a1ca)] - **doc**: run license-builder (github-actions\[bot]) [#54854](https://github.com/nodejs/node/pull/54854)
* \[[`8931f569c6`](https://github.com/nodejs/node/commit/8931f569c6)] - **doc**: experimental flag for global accessible APIs (Chengzhong Wu) [#54330](https://github.com/nodejs/node/pull/54330)
* \[[`6f8a6e9eb6`](https://github.com/nodejs/node/commit/6f8a6e9eb6)] - **doc**: add `ERR_INVALID_ADDRESS` to `errors.md` (Aviv Keller) [#54661](https://github.com/nodejs/node/pull/54661)
* \[[`c1b92e05e7`](https://github.com/nodejs/node/commit/c1b92e05e7)] - **doc**: add support link for mcollina (Matteo Collina) [#54786](https://github.com/nodejs/node/pull/54786)
* \[[`1def18122a`](https://github.com/nodejs/node/commit/1def18122a)] - **doc**: mark `--conditions` CLI flag as stable (Guy Bedford) [#54209](https://github.com/nodejs/node/pull/54209)
* \[[`b8ae36b6c3`](https://github.com/nodejs/node/commit/b8ae36b6c3)] - **doc**: fix typo in recognizing-contributors (Tobias Nießen) [#54822](https://github.com/nodejs/node/pull/54822)
* \[[`2c2ae80924`](https://github.com/nodejs/node/commit/2c2ae80924)] - **doc**: clarify `--max-old-space-size` and `--max-semi-space-size` units (Alexandre ABRIOUX) [#54477](https://github.com/nodejs/node/pull/54477)
* \[[`5bd4be5ce7`](https://github.com/nodejs/node/commit/5bd4be5ce7)] - **doc**: replace --allow-fs-read by --allow-fs-write in related section (M1CK431) [#54427](https://github.com/nodejs/node/pull/54427)
* \[[`c0f3e4603f`](https://github.com/nodejs/node/commit/c0f3e4603f)] - **doc**: add support link for marco-ippolito (Marco Ippolito) [#54789](https://github.com/nodejs/node/pull/54789)
* \[[`dc69eb8276`](https://github.com/nodejs/node/commit/dc69eb8276)] - **doc**: fix typo in module.md (Tobias Nießen) [#54794](https://github.com/nodejs/node/pull/54794)
* \[[`de225f5db9`](https://github.com/nodejs/node/commit/de225f5db9)] - **doc**: specify that preloaded modules affect subprocesses (Aviv Keller) [#52939](https://github.com/nodejs/node/pull/52939)
* \[[`62b0007cbe`](https://github.com/nodejs/node/commit/62b0007cbe)] - **doc**: clarify expandedSQL behavior (Tobias Nießen) [#54685](https://github.com/nodejs/node/pull/54685)
* \[[`1c7bdf95db`](https://github.com/nodejs/node/commit/1c7bdf95db)] - **doc**: render type references in SQLite docs (Tobias Nießen) [#54684](https://github.com/nodejs/node/pull/54684)
* \[[`5555095531`](https://github.com/nodejs/node/commit/5555095531)] - **doc**: fix typo (Michael Dawson) [#54640](https://github.com/nodejs/node/pull/54640)
* \[[`754baa4efa`](https://github.com/nodejs/node/commit/754baa4efa)] - **doc**: fix webcrypto.md AES-GCM backticks (Filip Skokan) [#54621](https://github.com/nodejs/node/pull/54621)
* \[[`5bfb4bcf45`](https://github.com/nodejs/node/commit/5bfb4bcf45)] - **doc**: add documentation about os.tmpdir() overrides (Joyee Cheung) [#54613](https://github.com/nodejs/node/pull/54613)
* \[[`22d873208e`](https://github.com/nodejs/node/commit/22d873208e)] - **doc, build**: fixup build docs (Aviv Keller) [#54899](https://github.com/nodejs/node/pull/54899)
* \[[`5e081a12b6`](https://github.com/nodejs/node/commit/5e081a12b6)] - **doc, child\_process**: add esm snippets (Aviv Keller) [#53616](https://github.com/nodejs/node/pull/53616)
* \[[`2b68c30a26`](https://github.com/nodejs/node/commit/2b68c30a26)] - **doc, meta**: fix broken link in `onboarding.md` (Aviv Keller) [#54886](https://github.com/nodejs/node/pull/54886)
* \[[`a624002fff`](https://github.com/nodejs/node/commit/a624002fff)] - **esm**: throw `ERR_REQUIRE_ESM` instead of `ERR_INTERNAL_ASSERTION` (Antoine du Hamel) [#54868](https://github.com/nodejs/node/pull/54868)
* \[[`31d4ef91ee`](https://github.com/nodejs/node/commit/31d4ef91ee)] - **esm**: fix support for `URL` instances in `import.meta.resolve` (Antoine du Hamel) [#54690](https://github.com/nodejs/node/pull/54690)
* \[[`40ba89e452`](https://github.com/nodejs/node/commit/40ba89e452)] - **esm**: use Undici/`fetch` `data:` URL parser (Matthew Aitken) [#54748](https://github.com/nodejs/node/pull/54748)
* \[[`93116dd7b1`](https://github.com/nodejs/node/commit/93116dd7b1)] - **fs**: translate error code properly in cpSync (Jason Zhang) [#54906](https://github.com/nodejs/node/pull/54906)
* \[[`375cbb592e`](https://github.com/nodejs/node/commit/375cbb592e)] - **fs**: refactor rimraf to avoid using primordials (Yagiz Nizipli) [#54834](https://github.com/nodejs/node/pull/54834)
* \[[`ee89c3149e`](https://github.com/nodejs/node/commit/ee89c3149e)] - **fs**: respect dereference when copy symlink directory (Jason Zhang) [#54732](https://github.com/nodejs/node/pull/54732)
* \[[`7123bf7ca4`](https://github.com/nodejs/node/commit/7123bf7ca4)] - **http**: reduce likelihood of race conditions on keep-alive timeout (jazelly) [#54863](https://github.com/nodejs/node/pull/54863)
* \[[`04ef3e4afd`](https://github.com/nodejs/node/commit/04ef3e4afd)] - **https**: only use default ALPNProtocols when appropriate (Brian White) [#54411](https://github.com/nodejs/node/pull/54411)
* \[[`dc5593ba1e`](https://github.com/nodejs/node/commit/dc5593ba1e)] - **lib**: remove unnecessary async (jakecastelli) [#54829](https://github.com/nodejs/node/pull/54829)
* \[[`2b9a6373da`](https://github.com/nodejs/node/commit/2b9a6373da)] - **lib**: make WeakRef safe in abort\_controller (jazelly) [#54791](https://github.com/nodejs/node/pull/54791)
* \[[`5f02e1b850`](https://github.com/nodejs/node/commit/5f02e1b850)] - **lib**: move `Symbol[Async]Dispose` polyfills to `internal/util` (Antoine du Hamel) [#54853](https://github.com/nodejs/node/pull/54853)
* \[[`fc78ced7e4`](https://github.com/nodejs/node/commit/fc78ced7e4)] - **lib**: convert signals to array before validation (Jason Zhang) [#54714](https://github.com/nodejs/node/pull/54714)
* \[[`21fef34a53`](https://github.com/nodejs/node/commit/21fef34a53)] - **lib**: add note about removing `node:sys` module (Rafael Gonzaga) [#54743](https://github.com/nodejs/node/pull/54743)
* \[[`a37d805489`](https://github.com/nodejs/node/commit/a37d805489)] - **(SEMVER-MINOR)** **lib**: add util.getCallSite() API (Rafael Gonzaga) [#54380](https://github.com/nodejs/node/pull/54380)
* \[[`2a1f56cce6`](https://github.com/nodejs/node/commit/2a1f56cce6)] - **lib**: ensure no holey array in fixed\_queue (Jason Zhang) [#54537](https://github.com/nodejs/node/pull/54537)
* \[[`540b1dbaf6`](https://github.com/nodejs/node/commit/540b1dbaf6)] - **lib**: refactor SubtleCrypto experimental warnings (Filip Skokan) [#54620](https://github.com/nodejs/node/pull/54620)
* \[[`b59c8b88c7`](https://github.com/nodejs/node/commit/b59c8b88c7)] - **lib,src**: use built-in array buffer detach, transfer (Yagiz Nizipli) [#54837](https://github.com/nodejs/node/pull/54837)
* \[[`c1cc046de9`](https://github.com/nodejs/node/commit/c1cc046de9)] - **meta**: bump peter-evans/create-pull-request from 6.1.0 to 7.0.1 (dependabot\[bot]) [#54820](https://github.com/nodejs/node/pull/54820)
* \[[`82c08ef483`](https://github.com/nodejs/node/commit/82c08ef483)] - **meta**: add `Windows ARM64` to flaky-tests list (Aviv Keller) [#54693](https://github.com/nodejs/node/pull/54693)
* \[[`df30e8efa1`](https://github.com/nodejs/node/commit/df30e8efa1)] - **meta**: ping @nodejs/performance on bench changes (Rafael Gonzaga) [#54752](https://github.com/nodejs/node/pull/54752)
* \[[`bdd9fbb905`](https://github.com/nodejs/node/commit/bdd9fbb905)] - **meta**: bump actions/setup-python from 5.1.1 to 5.2.0 (Rich Trott) [#54691](https://github.com/nodejs/node/pull/54691)
* \[[`19574a8403`](https://github.com/nodejs/node/commit/19574a8403)] - **meta**: update sccache to v0.8.1 (Aviv Keller) [#54720](https://github.com/nodejs/node/pull/54720)
* \[[`9ebcfb2b28`](https://github.com/nodejs/node/commit/9ebcfb2b28)] - **meta**: bump step-security/harden-runner from 2.9.0 to 2.9.1 (dependabot\[bot]) [#54704](https://github.com/nodejs/node/pull/54704)
* \[[`ea58feb959`](https://github.com/nodejs/node/commit/ea58feb959)] - **meta**: bump actions/upload-artifact from 4.3.4 to 4.4.0 (dependabot\[bot]) [#54703](https://github.com/nodejs/node/pull/54703)
* \[[`c6bd9e443e`](https://github.com/nodejs/node/commit/c6bd9e443e)] - **meta**: bump github/codeql-action from 3.25.15 to 3.26.6 (dependabot\[bot]) [#54702](https://github.com/nodejs/node/pull/54702)
* \[[`79b358af2e`](https://github.com/nodejs/node/commit/79b358af2e)] - **meta**: fix links in `SECURITY.md` (Aviv Keller) [#54696](https://github.com/nodejs/node/pull/54696)
* \[[`6c8a20d650`](https://github.com/nodejs/node/commit/6c8a20d650)] - **meta**: fix `contributing` codeowners (Aviv Keller) [#54641](https://github.com/nodejs/node/pull/54641)
* \[[`b7284ed099`](https://github.com/nodejs/node/commit/b7284ed099)] - **module**: do not warn for typeless package.json when there isn't one (Joyee Cheung) [#54045](https://github.com/nodejs/node/pull/54045)
* \[[`ddd24a6e63`](https://github.com/nodejs/node/commit/ddd24a6e63)] - **node-api**: add external buffer creation benchmark (Chengzhong Wu) [#54877](https://github.com/nodejs/node/pull/54877)
* \[[`4a7576efae`](https://github.com/nodejs/node/commit/4a7576efae)] - **node-api**: add support for UTF-8 and Latin-1 property keys (Mert Can Altin) [#52984](https://github.com/nodejs/node/pull/52984)
* \[[`461e523498`](https://github.com/nodejs/node/commit/461e523498)] - **os**: improve `tmpdir` performance (Yagiz Nizipli) [#54709](https://github.com/nodejs/node/pull/54709)
* \[[`94fb7ab2e7`](https://github.com/nodejs/node/commit/94fb7ab2e7)] - **path**: remove `StringPrototypeCharCodeAt` from `posix.extname` (Aviv Keller) [#54546](https://github.com/nodejs/node/pull/54546)
* \[[`67b1d4cb45`](https://github.com/nodejs/node/commit/67b1d4cb45)] - **repl**: avoid interpreting 'npm' as a command when errors are recoverable (Shima Ryuhei) [#54848](https://github.com/nodejs/node/pull/54848)
* \[[`8433032948`](https://github.com/nodejs/node/commit/8433032948)] - **repl**: doc-deprecate instantiating `node:repl` classes without `new` (Aviv Keller) [#54842](https://github.com/nodejs/node/pull/54842)
* \[[`7766349dd0`](https://github.com/nodejs/node/commit/7766349dd0)] - **sqlite**: fix segfault in expandedSQL (Tobias Nießen) [#54687](https://github.com/nodejs/node/pull/54687)
* \[[`4c1b98ba2b`](https://github.com/nodejs/node/commit/4c1b98ba2b)] - **sqlite**: remove unnecessary auto assignment (Tobias Nießen) [#54686](https://github.com/nodejs/node/pull/54686)
* \[[`77d162adb6`](https://github.com/nodejs/node/commit/77d162adb6)] - **src**: add `--env-file-if-exists` flag (Bosco Domingo) [#53060](https://github.com/nodejs/node/pull/53060)
* \[[`424bdc03b4`](https://github.com/nodejs/node/commit/424bdc03b4)] - **src**: add Cleanable class to Environment (Gabriel Schulhof) [#54880](https://github.com/nodejs/node/pull/54880)
* \[[`fbd08e3a9f`](https://github.com/nodejs/node/commit/fbd08e3a9f)] - **src**: switch crypto APIs to use Maybe\<void> (James M Snell) [#54775](https://github.com/nodejs/node/pull/54775)
* \[[`5e72bd3545`](https://github.com/nodejs/node/commit/5e72bd3545)] - **src**: eliminate ManagedEVPPkey (James M Snell) [#54751](https://github.com/nodejs/node/pull/54751)
* \[[`97cbcfbb43`](https://github.com/nodejs/node/commit/97cbcfbb43)] - **src**: fix unhandled error in structuredClone (Daeyeon Jeong) [#54764](https://github.com/nodejs/node/pull/54764)
* \[[`b89cd8d19a`](https://github.com/nodejs/node/commit/b89cd8d19a)] - **src**: move hkdf, scrypto, pbkdf2 impl to ncrypto (James M Snell) [#54651](https://github.com/nodejs/node/pull/54651)
* \[[`5c9599af5a`](https://github.com/nodejs/node/commit/5c9599af5a)] - **src**: create handle scope in FastInternalModuleStat (Joyee Cheung) [#54384](https://github.com/nodejs/node/pull/54384)
* \[[`e2307d87e8`](https://github.com/nodejs/node/commit/e2307d87e8)] - **(SEMVER-MINOR)** **stream**: relocate the status checking code in the onwritecomplete (YoonSoo\_Shin) [#54032](https://github.com/nodejs/node/pull/54032)
* \[[`ff54cabef6`](https://github.com/nodejs/node/commit/ff54cabef6)] - **test**: adjust test-tls-junk-server for OpenSSL32 (Michael Dawson) [#54926](https://github.com/nodejs/node/pull/54926)
* \[[`23fb03beed`](https://github.com/nodejs/node/commit/23fb03beed)] - **test**: remove duplicate skip AIX (Wuli) [#54917](https://github.com/nodejs/node/pull/54917)
* \[[`2b5e70816a`](https://github.com/nodejs/node/commit/2b5e70816a)] - **test**: adjust tls test for OpenSSL32 (Michael Dawson) [#54909](https://github.com/nodejs/node/pull/54909)
* \[[`cefa692dcb`](https://github.com/nodejs/node/commit/cefa692dcb)] - **test**: fix test-http2-socket-close.js (Hüseyin Açacak) [#54900](https://github.com/nodejs/node/pull/54900)
* \[[`097f6d3e7e`](https://github.com/nodejs/node/commit/097f6d3e7e)] - **test**: improve test-internal-fs-syncwritestream (Sunghoon) [#54671](https://github.com/nodejs/node/pull/54671)
* \[[`ed736a689f`](https://github.com/nodejs/node/commit/ed736a689f)] - **test**: deflake test-dns (Luigi Pinca) [#54902](https://github.com/nodejs/node/pull/54902)
* \[[`bb4849f595`](https://github.com/nodejs/node/commit/bb4849f595)] - **test**: fix test test-tls-dhe for OpenSSL32 (Michael Dawson) [#54903](https://github.com/nodejs/node/pull/54903)
* \[[`d9264bceca`](https://github.com/nodejs/node/commit/d9264bceca)] - **test**: use correct file naming syntax for `util-parse-env` (Aviv Keller) [#53705](https://github.com/nodejs/node/pull/53705)
* \[[`115a7ca42a`](https://github.com/nodejs/node/commit/115a7ca42a)] - **test**: add missing await (Luigi Pinca) [#54828](https://github.com/nodejs/node/pull/54828)
* \[[`7a1d633d77`](https://github.com/nodejs/node/commit/7a1d633d77)] - **test**: move more url tests to `node:test` (Yagiz Nizipli) [#54636](https://github.com/nodejs/node/pull/54636)
* \[[`ee385d62b9`](https://github.com/nodejs/node/commit/ee385d62b9)] - **test**: strip color chars in `test-runner-run` (Giovanni Bucci) [#54552](https://github.com/nodejs/node/pull/54552)
* \[[`2efec6221c`](https://github.com/nodejs/node/commit/2efec6221c)] - **test**: deflake test-http2-misbehaving-multiplex (Luigi Pinca) [#54872](https://github.com/nodejs/node/pull/54872)
* \[[`b198a91404`](https://github.com/nodejs/node/commit/b198a91404)] - **test**: remove dead code in test-http2-misbehaving-multiplex (Luigi Pinca) [#54860](https://github.com/nodejs/node/pull/54860)
* \[[`194cb83f39`](https://github.com/nodejs/node/commit/194cb83f39)] - **test**: reduce test-esm-loader-hooks-inspect-wait flakiness (Luigi Pinca) [#54827](https://github.com/nodejs/node/pull/54827)
* \[[`4b53558e8b`](https://github.com/nodejs/node/commit/4b53558e8b)] - **test**: reduce the allocation size in test-worker-arraybuffer-zerofill (James M Snell) [#54839](https://github.com/nodejs/node/pull/54839)
* \[[`c968d65d6d`](https://github.com/nodejs/node/commit/c968d65d6d)] - **test**: fix test-tls-client-mindhsize for OpenSSL32 (Michael Dawson) [#54739](https://github.com/nodejs/node/pull/54739)
* \[[`b998bb0933`](https://github.com/nodejs/node/commit/b998bb0933)] - **test**: remove need to make fs call for zlib test (Yagiz Nizipli) [#54814](https://github.com/nodejs/node/pull/54814)
* \[[`f084ea2e01`](https://github.com/nodejs/node/commit/f084ea2e01)] - **test**: use platform timeout (jakecastelli) [#54591](https://github.com/nodejs/node/pull/54591)
* \[[`b10e434cf3`](https://github.com/nodejs/node/commit/b10e434cf3)] - **test**: add platform timeout support for riscv64 (jakecastelli) [#54591](https://github.com/nodejs/node/pull/54591)
* \[[`b875f2d7de`](https://github.com/nodejs/node/commit/b875f2d7de)] - **test**: reduce stack size for test-error-serdes (James M Snell) [#54840](https://github.com/nodejs/node/pull/54840)
* \[[`d1a411480a`](https://github.com/nodejs/node/commit/d1a411480a)] - **test**: reduce fs calls in test-fs-existssync-false (Yagiz Nizipli) [#54815](https://github.com/nodejs/node/pull/54815)
* \[[`b96ee30a09`](https://github.com/nodejs/node/commit/b96ee30a09)] - **test**: use `node:test` in `test-cli-syntax.bad` (Aviv Keller) [#54513](https://github.com/nodejs/node/pull/54513)
* \[[`5278b8b7a1`](https://github.com/nodejs/node/commit/5278b8b7a1)] - **test**: move test-http-server-request-timeouts-mixed (James M Snell) [#54841](https://github.com/nodejs/node/pull/54841)
* \[[`8345a60d3a`](https://github.com/nodejs/node/commit/8345a60d3a)] - **test**: fix Windows async-context-frame memory failure (Stephen Belanger) [#54823](https://github.com/nodejs/node/pull/54823)
* \[[`cad404e1a1`](https://github.com/nodejs/node/commit/cad404e1a1)] - **test**: fix volatile for CauseSegfault with clang (Ivan Trubach) [#54325](https://github.com/nodejs/node/pull/54325)
* \[[`41682c7286`](https://github.com/nodejs/node/commit/41682c7286)] - **test**: set `test-http2-socket-close` as flaky (Yagiz Nizipli) [#54802](https://github.com/nodejs/node/pull/54802)
* \[[`1e1ac48711`](https://github.com/nodejs/node/commit/1e1ac48711)] - **test**: set `test-worker-arraybuffer-zerofill` as flaky (Yagiz Nizipli) [#54802](https://github.com/nodejs/node/pull/54802)
* \[[`56238debff`](https://github.com/nodejs/node/commit/56238debff)] - **test**: set `test-runner-run-watch` as flaky (Yagiz Nizipli) [#54802](https://github.com/nodejs/node/pull/54802)
* \[[`8291de1540`](https://github.com/nodejs/node/commit/8291de1540)] - **test**: set `test-http-server-request-timeouts-mixed` as flaky (Yagiz Nizipli) [#54802](https://github.com/nodejs/node/pull/54802)
* \[[`32d340e6b3`](https://github.com/nodejs/node/commit/32d340e6b3)] - **test**: set `test-single-executable-application-empty` as flaky (Yagiz Nizipli) [#54802](https://github.com/nodejs/node/pull/54802)
* \[[`6a2da4c4ca`](https://github.com/nodejs/node/commit/6a2da4c4ca)] - **test**: set `test-macos-app-sandbox` as flaky (Yagiz Nizipli) [#54802](https://github.com/nodejs/node/pull/54802)
* \[[`2f408847a0`](https://github.com/nodejs/node/commit/2f408847a0)] - **test**: set `test-fs-utimes` as flaky (Yagiz Nizipli) [#54802](https://github.com/nodejs/node/pull/54802)
* \[[`e3b7c40ffc`](https://github.com/nodejs/node/commit/e3b7c40ffc)] - **test**: set `test-runner-run-watch` as flaky (Yagiz Nizipli) [#54802](https://github.com/nodejs/node/pull/54802)
* \[[`d2ede46946`](https://github.com/nodejs/node/commit/d2ede46946)] - **test**: set `test-sqlite-statement-sync` as flaky (Yagiz Nizipli) [#54802](https://github.com/nodejs/node/pull/54802)
* \[[`b9f3385808`](https://github.com/nodejs/node/commit/b9f3385808)] - **test**: set `test-writewrap` as flaky (Yagiz Nizipli) [#54802](https://github.com/nodejs/node/pull/54802)
* \[[`d55fec8f40`](https://github.com/nodejs/node/commit/d55fec8f40)] - **test**: set `test-async-context-frame` as flaky (Yagiz Nizipli) [#54802](https://github.com/nodejs/node/pull/54802)
* \[[`3dfb525f3e`](https://github.com/nodejs/node/commit/3dfb525f3e)] - **test**: set `test-esm-loader-hooks-inspect-wait` as flaky (Yagiz Nizipli) [#54802](https://github.com/nodejs/node/pull/54802)
* \[[`b0458a88b4`](https://github.com/nodejs/node/commit/b0458a88b4)] - **test**: set `test-http2-large-file` as flaky (Yagiz Nizipli) [#54802](https://github.com/nodejs/node/pull/54802)
* \[[`5f6f8757e5`](https://github.com/nodejs/node/commit/5f6f8757e5)] - **test**: set `test-runner-watch-mode-complex` as flaky (Yagiz Nizipli) [#54802](https://github.com/nodejs/node/pull/54802)
* \[[`4231af336d`](https://github.com/nodejs/node/commit/4231af336d)] - **test**: set `test-performance-function` as flaky (Yagiz Nizipli) [#54802](https://github.com/nodejs/node/pull/54802)
* \[[`45ef2a868e`](https://github.com/nodejs/node/commit/45ef2a868e)] - **test**: set `test-debugger-heap-profiler` as flaky (Yagiz Nizipli) [#54802](https://github.com/nodejs/node/pull/54802)
* \[[`b5137f6405`](https://github.com/nodejs/node/commit/b5137f6405)] - **test**: fix `test-process-load-env-file` when path contains `'` (Antoine du Hamel) [#54511](https://github.com/nodejs/node/pull/54511)
* \[[`960116905a`](https://github.com/nodejs/node/commit/960116905a)] - **test**: refactor fs-watch tests due to macOS issue (Santiago Gimeno) [#54498](https://github.com/nodejs/node/pull/54498)
* \[[`f074d74bf3`](https://github.com/nodejs/node/commit/f074d74bf3)] - **test**: refactor `test-esm-type-field-errors` (Giovanni Bucci) [#54368](https://github.com/nodejs/node/pull/54368)
* \[[`67e30deced`](https://github.com/nodejs/node/commit/67e30deced)] - **test**: move more zlib tests to node:test (Yagiz Nizipli) [#54609](https://github.com/nodejs/node/pull/54609)
* \[[`fdb65111a3`](https://github.com/nodejs/node/commit/fdb65111a3)] - **test**: improve output of child process utilities (Joyee Cheung) [#54622](https://github.com/nodejs/node/pull/54622)
* \[[`55a12a4190`](https://github.com/nodejs/node/commit/55a12a4190)] - **test,crypto**: update WebCryptoAPI WPT (Filip Skokan) [#54925](https://github.com/nodejs/node/pull/54925)
* \[[`de0f445a7f`](https://github.com/nodejs/node/commit/de0f445a7f)] - **test\_runner**: reimplement `assert.ok` to allow stack parsing (Aviv Keller) [#54776](https://github.com/nodejs/node/pull/54776)
* \[[`a52c199d9d`](https://github.com/nodejs/node/commit/a52c199d9d)] - **(SEMVER-MINOR)** **test\_runner**: report coverage thresholds in `test:coverage` (Aviv Keller) [#54813](https://github.com/nodejs/node/pull/54813)
* \[[`6552fddef5`](https://github.com/nodejs/node/commit/6552fddef5)] - **test\_runner**: update kPatterns (Pietro Marchini) [#54728](https://github.com/nodejs/node/pull/54728)
* \[[`3396a4954d`](https://github.com/nodejs/node/commit/3396a4954d)] - **test\_runner**: detect only tests when isolation is off (Colin Ihrig) [#54832](https://github.com/nodejs/node/pull/54832)
* \[[`021f59b6bc`](https://github.com/nodejs/node/commit/021f59b6bc)] - **test\_runner**: apply filtering when tests begin (Colin Ihrig) [#54832](https://github.com/nodejs/node/pull/54832)
* \[[`36da793350`](https://github.com/nodejs/node/commit/36da793350)] - **test\_runner**: allow `--import` with no isolation (Aviv Keller) [#54697](https://github.com/nodejs/node/pull/54697)
* \[[`de73d1ee4b`](https://github.com/nodejs/node/commit/de73d1ee4b)] - **test\_runner**: improve code coverage cleanup (Colin Ihrig) [#54856](https://github.com/nodejs/node/pull/54856)
* \[[`3d478728f2`](https://github.com/nodejs/node/commit/3d478728f2)] - **timers**: avoid generating holey internal arrays (Gürgün Dayıoğlu) [#54771](https://github.com/nodejs/node/pull/54771)
* \[[`b3d567ae0f`](https://github.com/nodejs/node/commit/b3d567ae0f)] - **timers**: document ref option for scheduler.wait (Paolo Insogna) [#54605](https://github.com/nodejs/node/pull/54605)
* \[[`c2bf0134ce`](https://github.com/nodejs/node/commit/c2bf0134ce)] - **(SEMVER-MINOR)** **tls**: add `allowPartialTrustChain` flag (Anna Henningsen) [#54790](https://github.com/nodejs/node/pull/54790)
* \[[`608a611132`](https://github.com/nodejs/node/commit/608a611132)] - **tools**: add readability/fn\_size to filter (Rafael Gonzaga) [#54744](https://github.com/nodejs/node/pull/54744)
* \[[`93fab49099`](https://github.com/nodejs/node/commit/93fab49099)] - **tools**: add util scripts to land and rebase PRs (Antoine du Hamel) [#54656](https://github.com/nodejs/node/pull/54656)
* \[[`d6df542ff8`](https://github.com/nodejs/node/commit/d6df542ff8)] - **tools**: remove readability/fn\_size rule (Rafael Gonzaga) [#54663](https://github.com/nodejs/node/pull/54663)
* \[[`689d127ee7`](https://github.com/nodejs/node/commit/689d127ee7)] - **typings**: fix TypedArray to a global type (1ilsang) [#54063](https://github.com/nodejs/node/pull/54063)
* \[[`071dff1d34`](https://github.com/nodejs/node/commit/071dff1d34)] - **typings**: correct param type of `SafePromisePrototypeFinally` (Wuli) [#54727](https://github.com/nodejs/node/pull/54727)
* \[[`5243e3240c`](https://github.com/nodejs/node/commit/5243e3240c)] - _**Revert**_ "**v8**: enable maglev on supported architectures" (Joyee Cheung) [#54384](https://github.com/nodejs/node/pull/54384)
* \[[`ade9da5b3a`](https://github.com/nodejs/node/commit/ade9da5b3a)] - **vm**: add vm proto property lookup test (Chengzhong Wu) [#54606](https://github.com/nodejs/node/pull/54606)
* \[[`8385958b60`](https://github.com/nodejs/node/commit/8385958b60)] - **zlib**: add typings for better dx (Yagiz Nizipli) [#54699](https://github.com/nodejs/node/pull/54699)
* \[[`8c4c85cf31`](https://github.com/nodejs/node/commit/8c4c85cf31)] - **zlib**: deprecate instantiating classes without new (Yagiz Nizipli) [#54708](https://github.com/nodejs/node/pull/54708)

<a id="22.8.0"></a>

## 2024-09-03, Version 22.8.0 (Current), @RafaelGSS

### New JS API for compile cache

This release adds a new API `module.enableCompileCache()` that can be used to enable on-disk code caching of all modules loaded after this API is called.
Previously this could only be enabled by the `NODE_COMPILE_CACHE` environment variable, so it could only set by end-users.
This API allows tooling and library authors to enable caching of their own code.
This is a built-in alternative to the [v8-compile-cache](https://www.npmjs.com/package/v8-compile-cache)/[v8-compile-cache-lib ](https://www.npmjs.com/package/v8-compile-cache-lib) packages,
but have [better performance](https://github.com/nodejs/node/issues/47472#issuecomment-1970331362) and supports ESM.

Thanks to Joyee Cheung for working on this.

### New option for vm.createContext() to create a context with a freezable globalThis

Node.js implements a flavor of `vm.createContext()` and friends that creates a context without contextifying its global
object when vm.constants.DONT\_CONTEXTIFY is used. This is suitable when users want to freeze the context
(impossible when the global is contextified i.e. has interceptors installed) or speed up the global access if they
don't need the interceptor behavior.

Thanks to Joyee Cheung for working on this.

### Support for coverage thresholds

Node.js now supports requiring code coverage to meet a specific threshold before the process exits successfully.
To use this feature, you need to enable the `--experimental-test-coverage` flag.

You can set thresholds for the following types of coverage:

* **Branch coverage**: Use `--test-coverage-branches=<threshold>`
* **Function coverage**: Use `--test-coverage-functions=<threshold>`
* **Line coverage**: Use `--test-coverage-lines=<threshold>`

`<threshold>` should be an integer between 0 and 100. If an invalid value is provided, a `TypeError` will be thrown.

If the code coverage fails to meet the specified thresholds for any category, the process will exit with code `1`.

For instance, to enforce a minimum of 80% line coverage and 60% branch coverage, you can run:

```console
$ node --experimental-test-coverage --test-coverage-lines=80 --test-coverage-branches=60 example.js
```

Thanks Aviv Keller for working on this.

### Other Notable Changes

* \[[`1f2cc2fa47`](https://github.com/nodejs/node/commit/1f2cc2fa47)] - **(SEMVER-MINOR)** **src,lib**: add performance.uvMetricsInfo (Rafael Gonzaga) [#54413](https://github.com/nodejs/node/pull/54413)
* \[[`1e01bdc0d0`](https://github.com/nodejs/node/commit/1e01bdc0d0)] - **(SEMVER-MINOR)** **net**: exclude ipv6 loopback addresses from server.listen (Giovanni Bucci) [#54264](https://github.com/nodejs/node/pull/54264)
* \[[`97fa075c2e`](https://github.com/nodejs/node/commit/97fa075c2e)] - **(SEMVER-MINOR)** **test\_runner**: support running tests in process (Colin Ihrig) [#53927](https://github.com/nodejs/node/pull/53927)
* \[[`858b583c88`](https://github.com/nodejs/node/commit/858b583c88)] - **(SEMVER-MINOR)** **test\_runner**: defer inheriting hooks until run() (Colin Ihrig) [#53927](https://github.com/nodejs/node/pull/53927)

### Commits

* \[[`94985df9d6`](https://github.com/nodejs/node/commit/94985df9d6)] - **benchmark**: fix benchmark for file path and URL conversion (Early Riser) [#54190](https://github.com/nodejs/node/pull/54190)
* \[[`ac178b094b`](https://github.com/nodejs/node/commit/ac178b094b)] - **buffer**: truncate instead of throw when writing beyond buffer (Robert Nagy) [#54524](https://github.com/nodejs/node/pull/54524)
* \[[`afd8c1eb4f`](https://github.com/nodejs/node/commit/afd8c1eb4f)] - **buffer**: allow invalid encoding in from (Robert Nagy) [#54533](https://github.com/nodejs/node/pull/54533)
* \[[`6f0cf35cd3`](https://github.com/nodejs/node/commit/6f0cf35cd3)] - **build**: reclaim disk space on macOS GHA runner (jakecastelli) [#54658](https://github.com/nodejs/node/pull/54658)
* \[[`467ac3aec4`](https://github.com/nodejs/node/commit/467ac3aec4)] - **build**: don't clean obj.target directory if it doesn't exist (Joyee Cheung) [#54337](https://github.com/nodejs/node/pull/54337)
* \[[`71fdf961df`](https://github.com/nodejs/node/commit/71fdf961df)] - **build**: update required python version to 3.8 (Aviv Keller) [#54358](https://github.com/nodejs/node/pull/54358)
* \[[`73604cf1c5`](https://github.com/nodejs/node/commit/73604cf1c5)] - **deps**: update nghttp2 to 1.63.0 (Node.js GitHub Bot) [#54589](https://github.com/nodejs/node/pull/54589)
* \[[`b00c087285`](https://github.com/nodejs/node/commit/b00c087285)] - **deps**: V8: cherry-pick e74d0f437fcd (Joyee Cheung) [#54279](https://github.com/nodejs/node/pull/54279)
* \[[`33a6b3c7a9`](https://github.com/nodejs/node/commit/33a6b3c7a9)] - **deps**: backport ICU-22787 to fix ClangCL on Windows (Stefan Stojanovic) [#54502](https://github.com/nodejs/node/pull/54502)
* \[[`fe56949cbb`](https://github.com/nodejs/node/commit/fe56949cbb)] - **deps**: update c-ares to v1.33.1 (Node.js GitHub Bot) [#54549](https://github.com/nodejs/node/pull/54549)
* \[[`290f6ce619`](https://github.com/nodejs/node/commit/290f6ce619)] - **deps**: update amaro to 0.1.8 (Node.js GitHub Bot) [#54520](https://github.com/nodejs/node/pull/54520)
* \[[`b5843568b4`](https://github.com/nodejs/node/commit/b5843568b4)] - **deps**: update amaro to 0.1.7 (Node.js GitHub Bot) [#54473](https://github.com/nodejs/node/pull/54473)
* \[[`9c709209b4`](https://github.com/nodejs/node/commit/9c709209b4)] - **deps**: update undici to 6.19.8 (Node.js GitHub Bot) [#54456](https://github.com/nodejs/node/pull/54456)
* \[[`a5ce24181b`](https://github.com/nodejs/node/commit/a5ce24181b)] - **deps**: sqlite: fix Windows compilation (Colin Ihrig) [#54433](https://github.com/nodejs/node/pull/54433)
* \[[`3caf29ea88`](https://github.com/nodejs/node/commit/3caf29ea88)] - **deps**: update sqlite to 3.46.1 (Node.js GitHub Bot) [#54433](https://github.com/nodejs/node/pull/54433)
* \[[`68758d4b08`](https://github.com/nodejs/node/commit/68758d4b08)] - **doc**: add support me link for anonrig (Yagiz Nizipli) [#54611](https://github.com/nodejs/node/pull/54611)
* \[[`f5c5529266`](https://github.com/nodejs/node/commit/f5c5529266)] - **doc**: add alert on REPL from TCP socket (Rafael Gonzaga) [#54594](https://github.com/nodejs/node/pull/54594)
* \[[`bf824483cd`](https://github.com/nodejs/node/commit/bf824483cd)] - **doc**: fix typo in styleText description (Rafael Gonzaga) [#54616](https://github.com/nodejs/node/pull/54616)
* \[[`825d933fd4`](https://github.com/nodejs/node/commit/825d933fd4)] - **doc**: add getHeapStatistics() property descriptions (Benji Marinacci) [#54584](https://github.com/nodejs/node/pull/54584)
* \[[`80e5150160`](https://github.com/nodejs/node/commit/80e5150160)] - **doc**: fix module compile cache description (沈鸿飞) [#54625](https://github.com/nodejs/node/pull/54625)
* \[[`7fd033fe56`](https://github.com/nodejs/node/commit/7fd033fe56)] - **doc**: run license-builder (github-actions\[bot]) [#54562](https://github.com/nodejs/node/pull/54562)
* \[[`c499913732`](https://github.com/nodejs/node/commit/c499913732)] - **doc**: fix information about including coverage files (Aviv Keller) [#54527](https://github.com/nodejs/node/pull/54527)
* \[[`c3dc83befc`](https://github.com/nodejs/node/commit/c3dc83befc)] - **doc**: support collaborators - talk amplification (Michael Dawson) [#54508](https://github.com/nodejs/node/pull/54508)
* \[[`fc57beaad3`](https://github.com/nodejs/node/commit/fc57beaad3)] - **doc**: add note about shasum generation failure (Marco Ippolito) [#54487](https://github.com/nodejs/node/pull/54487)
* \[[`1800a58f49`](https://github.com/nodejs/node/commit/1800a58f49)] - **doc**: update websocket flag description to reflect stable API status (Yelim Koo) [#54482](https://github.com/nodejs/node/pull/54482)
* \[[`61affd77a7`](https://github.com/nodejs/node/commit/61affd77a7)] - **doc**: fix capitalization in module.md (shallow-beach) [#54488](https://github.com/nodejs/node/pull/54488)
* \[[`25419915c7`](https://github.com/nodejs/node/commit/25419915c7)] - **doc**: add esm examples to node:https (Alfredo González) [#54399](https://github.com/nodejs/node/pull/54399)
* \[[`83b5efeb54`](https://github.com/nodejs/node/commit/83b5efeb54)] - **doc**: reserve ABI 130 for Electron 33 (Calvin) [#54383](https://github.com/nodejs/node/pull/54383)
* \[[`6ccbd32ae8`](https://github.com/nodejs/node/commit/6ccbd32ae8)] - **doc, meta**: add missing `,` to `BUILDING.md` (Aviv Keller) [#54409](https://github.com/nodejs/node/pull/54409)
* \[[`fc08a9b0cd`](https://github.com/nodejs/node/commit/fc08a9b0cd)] - **fs**: refactor handleTimestampsAndMode to remove redundant call (HEESEUNG) [#54369](https://github.com/nodejs/node/pull/54369)
* \[[`4a664b5fcb`](https://github.com/nodejs/node/commit/4a664b5fcb)] - **lib**: respect terminal capabilities on styleText (Rafael Gonzaga) [#54389](https://github.com/nodejs/node/pull/54389)
* \[[`a9ce2b6a28`](https://github.com/nodejs/node/commit/a9ce2b6a28)] - **lib**: fix emit warning for debuglog.time when disabled (Vinicius Lourenço) [#54275](https://github.com/nodejs/node/pull/54275)
* \[[`b5a23c9783`](https://github.com/nodejs/node/commit/b5a23c9783)] - **meta**: remind users to use a supported version in bug reports (Aviv Keller) [#54481](https://github.com/nodejs/node/pull/54481)
* \[[`0d7171d8e9`](https://github.com/nodejs/node/commit/0d7171d8e9)] - **meta**: add more labels to dep-updaters (Aviv Keller) [#54454](https://github.com/nodejs/node/pull/54454)
* \[[`c4996c189f`](https://github.com/nodejs/node/commit/c4996c189f)] - **meta**: run coverage-windows when `vcbuild.bat` updated (Aviv Keller) [#54412](https://github.com/nodejs/node/pull/54412)
* \[[`3cf645768e`](https://github.com/nodejs/node/commit/3cf645768e)] - **module**: use amaro default transform values (Marco Ippolito) [#54517](https://github.com/nodejs/node/pull/54517)
* \[[`336496b90e`](https://github.com/nodejs/node/commit/336496b90e)] - **module**: add sourceURL magic comment hinting generated source (Chengzhong Wu) [#54402](https://github.com/nodejs/node/pull/54402)
* \[[`04f83b50ad`](https://github.com/nodejs/node/commit/04f83b50ad)] - _**Revert**_ "**net**: validate host name for server listen" (jakecastelli) [#54554](https://github.com/nodejs/node/pull/54554)
* \[[`1e01bdc0d0`](https://github.com/nodejs/node/commit/1e01bdc0d0)] - **(SEMVER-MINOR)** **net**: exclude ipv6 loopback addresses from server.listen (Giovanni Bucci) [#54264](https://github.com/nodejs/node/pull/54264)
* \[[`3cd10a3f40`](https://github.com/nodejs/node/commit/3cd10a3f40)] - **node-api**: remove RefBase and CallbackWrapper (Vladimir Morozov) [#53590](https://github.com/nodejs/node/pull/53590)
* \[[`72c554abab`](https://github.com/nodejs/node/commit/72c554abab)] - **sqlite**: return results with null prototype (Michaël Zasso) [#54350](https://github.com/nodejs/node/pull/54350)
* \[[`e071651bb2`](https://github.com/nodejs/node/commit/e071651bb2)] - **src**: disable fast methods for `buffer.write` (Michaël Zasso) [#54565](https://github.com/nodejs/node/pull/54565)
* \[[`f8cbbc685a`](https://github.com/nodejs/node/commit/f8cbbc685a)] - **src**: use v8::Isolate::GetDefaultLocale() to compute navigator.language (Joyee Cheung) [#54279](https://github.com/nodejs/node/pull/54279)
* \[[`4baf4637eb`](https://github.com/nodejs/node/commit/4baf4637eb)] - **(SEMVER-MINOR)** **src**: add JS APIs for compile cache and NODE\_DISABLE\_COMPILE\_CACHE (Joyee Cheung) [#54501](https://github.com/nodejs/node/pull/54501)
* \[[`101e299656`](https://github.com/nodejs/node/commit/101e299656)] - **src**: move more crypto\_dh.cc code to ncrypto (James M Snell) [#54459](https://github.com/nodejs/node/pull/54459)
* \[[`e6e1f4e8bd`](https://github.com/nodejs/node/commit/e6e1f4e8bd)] - **src**: remove redundant AESCipherMode (Tobias Nießen) [#54438](https://github.com/nodejs/node/pull/54438)
* \[[`1ff3f63f5e`](https://github.com/nodejs/node/commit/1ff3f63f5e)] - **src**: handle errors correctly in `permission.cc` (Michaël Zasso) [#54541](https://github.com/nodejs/node/pull/54541)
* \[[`4938188682`](https://github.com/nodejs/node/commit/4938188682)] - **src**: return `v8::Object` from error constructors (Michaël Zasso) [#54541](https://github.com/nodejs/node/pull/54541)
* \[[`4578e9485b`](https://github.com/nodejs/node/commit/4578e9485b)] - **src**: use better return types in KVStore (Michaël Zasso) [#54539](https://github.com/nodejs/node/pull/54539)
* \[[`7d9e994791`](https://github.com/nodejs/node/commit/7d9e994791)] - **src**: change SetEncodedValue to return Maybe\<void> (Tobias Nießen) [#54443](https://github.com/nodejs/node/pull/54443)
* \[[`eef303028f`](https://github.com/nodejs/node/commit/eef303028f)] - **src**: remove cached data tag from snapshot metadata (Joyee Cheung) [#54122](https://github.com/nodejs/node/pull/54122)
* \[[`3a74c400d5`](https://github.com/nodejs/node/commit/3a74c400d5)] - **src**: improve `buffer.transcode` performance (Yagiz Nizipli) [#54153](https://github.com/nodejs/node/pull/54153)
* \[[`909c5320fd`](https://github.com/nodejs/node/commit/909c5320fd)] - **src**: move more crypto code to ncrypto (James M Snell) [#54320](https://github.com/nodejs/node/pull/54320)
* \[[`9ba75faf5f`](https://github.com/nodejs/node/commit/9ba75faf5f)] - **(SEMVER-MINOR)** **src,lib**: add performance.uvMetricsInfo (Rafael Gonzaga) [#54413](https://github.com/nodejs/node/pull/54413)
* \[[`fffc300c6d`](https://github.com/nodejs/node/commit/fffc300c6d)] - **stream**: change stream to use index instead of `for...of` (Wiyeong Seo) [#54474](https://github.com/nodejs/node/pull/54474)
* \[[`a4a6ef8d29`](https://github.com/nodejs/node/commit/a4a6ef8d29)] - **test**: fix test-tls-client-auth test for OpenSSL32 (Michael Dawson) [#54610](https://github.com/nodejs/node/pull/54610)
* \[[`76345a5d7c`](https://github.com/nodejs/node/commit/76345a5d7c)] - **test**: update TLS test for OpenSSL 3.2 (Richard Lau) [#54612](https://github.com/nodejs/node/pull/54612)
* \[[`522d5a359d`](https://github.com/nodejs/node/commit/522d5a359d)] - **test**: run V8 Fast API tests in release mode too (Michaël Zasso) [#54570](https://github.com/nodejs/node/pull/54570)
* \[[`edbecf5209`](https://github.com/nodejs/node/commit/edbecf5209)] - **test**: increase key size for ca2-cert.pem (Michael Dawson) [#54599](https://github.com/nodejs/node/pull/54599)
* \[[`bc976cfc93`](https://github.com/nodejs/node/commit/bc976cfc93)] - **test**: update test-abortsignal-cloneable to use node:test (James M Snell) [#54581](https://github.com/nodejs/node/pull/54581)
* \[[`9f1ce732a8`](https://github.com/nodejs/node/commit/9f1ce732a8)] - **test**: update test-assert-typedarray-deepequal to use node:test (James M Snell) [#54585](https://github.com/nodejs/node/pull/54585)
* \[[`c74f2aeb92`](https://github.com/nodejs/node/commit/c74f2aeb92)] - **test**: update test-assert to use node:test (James M Snell) [#54585](https://github.com/nodejs/node/pull/54585)
* \[[`a0be95e4cc`](https://github.com/nodejs/node/commit/a0be95e4cc)] - **test**: merge ongc and gcutil into gc.js (tannal) [#54355](https://github.com/nodejs/node/pull/54355)
* \[[`c10aff665e`](https://github.com/nodejs/node/commit/c10aff665e)] - **test**: move a couple of tests over to using node:test (James M Snell) [#54582](https://github.com/nodejs/node/pull/54582)
* \[[`dbbc790949`](https://github.com/nodejs/node/commit/dbbc790949)] - **test**: update test-aborted-util to use node:test (James M Snell) [#54578](https://github.com/nodejs/node/pull/54578)
* \[[`64442fce6b`](https://github.com/nodejs/node/commit/64442fce6b)] - **test**: refactor test-abortcontroller to use node:test (James M Snell) [#54574](https://github.com/nodejs/node/pull/54574)
* \[[`72345dee1c`](https://github.com/nodejs/node/commit/72345dee1c)] - **test**: fix embedding test for Windows (Vladimir Morozov) [#53659](https://github.com/nodejs/node/pull/53659)
* \[[`846e2b2896`](https://github.com/nodejs/node/commit/846e2b2896)] - **test**: refactor test\_runner tests to change default reporter (Colin Ihrig) [#54547](https://github.com/nodejs/node/pull/54547)
* \[[`b5eb24c86a`](https://github.com/nodejs/node/commit/b5eb24c86a)] - **test**: force spec reporter in test-runner-watch-mode.mjs (Colin Ihrig) [#54538](https://github.com/nodejs/node/pull/54538)
* \[[`66ae9f4c0a`](https://github.com/nodejs/node/commit/66ae9f4c0a)] - **test**: use valid hostnames (Luigi Pinca) [#54556](https://github.com/nodejs/node/pull/54556)
* \[[`02d664b75f`](https://github.com/nodejs/node/commit/02d664b75f)] - **test**: fix improper path to URL conversion (Antoine du Hamel) [#54509](https://github.com/nodejs/node/pull/54509)
* \[[`8a4f8a9eff`](https://github.com/nodejs/node/commit/8a4f8a9eff)] - **test**: add tests for runner coverage with different stdout column widths (Pietro Marchini) [#54494](https://github.com/nodejs/node/pull/54494)
* \[[`b0ed8dbb2f`](https://github.com/nodejs/node/commit/b0ed8dbb2f)] - **test**: prevent V8 from writing into the system's tmpdir (Michaël Zasso) [#54395](https://github.com/nodejs/node/pull/54395)
* \[[`5ee234a5a6`](https://github.com/nodejs/node/commit/5ee234a5a6)] - **test,crypto**: update WebCryptoAPI WPT (Filip Skokan) [#54593](https://github.com/nodejs/node/pull/54593)
* \[[`a4bebf8559`](https://github.com/nodejs/node/commit/a4bebf8559)] - **test\_runner**: ensure test watcher picks up new test files (Pietro Marchini) [#54225](https://github.com/nodejs/node/pull/54225)
* \[[`d4310fe9c1`](https://github.com/nodejs/node/commit/d4310fe9c1)] - **(SEMVER-MINOR)** **test\_runner**: add support for coverage thresholds (Aviv Keller) [#54429](https://github.com/nodejs/node/pull/54429)
* \[[`0cf78aa24b`](https://github.com/nodejs/node/commit/0cf78aa24b)] - **test\_runner**: refactor `mock_loader` (Antoine du Hamel) [#54223](https://github.com/nodejs/node/pull/54223)
* \[[`97fa075c2e`](https://github.com/nodejs/node/commit/97fa075c2e)] - **(SEMVER-MINOR)** **test\_runner**: support running tests in process (Colin Ihrig) [#53927](https://github.com/nodejs/node/pull/53927)
* \[[`858b583c88`](https://github.com/nodejs/node/commit/858b583c88)] - **(SEMVER-MINOR)** **test\_runner**: defer inheriting hooks until run() (Colin Ihrig) [#53927](https://github.com/nodejs/node/pull/53927)
* \[[`45b0250692`](https://github.com/nodejs/node/commit/45b0250692)] - **test\_runner**: account for newline in source maps (Colin Ihrig) [#54444](https://github.com/nodejs/node/pull/54444)
* \[[`1c29e74d30`](https://github.com/nodejs/node/commit/1c29e74d30)] - **test\_runner**: make `mock.module`'s `specifier` consistent with `import()` (Antoine du Hamel) [#54416](https://github.com/nodejs/node/pull/54416)
* \[[`cbe30a02a3`](https://github.com/nodejs/node/commit/cbe30a02a3)] - **test\_runner**: finish build phase before running tests (Colin Ihrig) [#54423](https://github.com/nodejs/node/pull/54423)
* \[[`8a4b26f00c`](https://github.com/nodejs/node/commit/8a4b26f00c)] - **timers**: fix validation (Paolo Insogna) [#54404](https://github.com/nodejs/node/pull/54404)
* \[[`38798140c4`](https://github.com/nodejs/node/commit/38798140c4)] - **tools**: remove unused python files (Aviv Keller) [#53928](https://github.com/nodejs/node/pull/53928)
* \[[`da6c61def8`](https://github.com/nodejs/node/commit/da6c61def8)] - **tools**: add swc license (Marco Ippolito) [#54462](https://github.com/nodejs/node/pull/54462)
* \[[`16d4c437e1`](https://github.com/nodejs/node/commit/16d4c437e1)] - **typings**: provide internal types for wasi bindings (Andrew Moon) [#54119](https://github.com/nodejs/node/pull/54119)
* \[[`fe5666f006`](https://github.com/nodejs/node/commit/fe5666f006)] - **vm**: return all own names and symbols in property enumerator interceptor (Chengzhong Wu) [#54522](https://github.com/nodejs/node/pull/54522)
* \[[`db80eac496`](https://github.com/nodejs/node/commit/db80eac496)] - **(SEMVER-MINOR)** **vm**: introduce vanilla contexts via vm.constants.DONT\_CONTEXTIFY (Joyee Cheung) [#54394](https://github.com/nodejs/node/pull/54394)
* \[[`8ffdd1e2b2`](https://github.com/nodejs/node/commit/8ffdd1e2b2)] - **zlib**: simplify validators (Yagiz Nizipli) [#54442](https://github.com/nodejs/node/pull/54442)

<a id="22.7.0"></a>

## 2024-08-22, Version 22.7.0 (Current), @RafaelGSS

### Experimental transform types support

With the new flag `--experimental-transform-types` it is possible to enable the
transformation of TypeScript-only syntax into JavaScript code.

This feature allows Node.js to support TypeScript syntax such as `Enum` and `namespace`.

Thanks to Marco Ippolito for making this work on [#54283](https://github.com/nodejs/node/pull/54283).

### Module syntax detection is now enabled by default.

Module syntax detection (the `--experimental-detect-module` flag) is now
enabled by default. Use `--no-experimental-detect-module` to disable it if
needed.

Syntax detection attempts to run ambiguous files as CommonJS, and if the module
fails to parse as CommonJS due to ES module syntax, Node.js tries again and runs
the file as an ES module.
Ambiguous files are those with a `.js` or no extension, where the nearest parent
`package.json` has no `"type"` field (either `"type": "module"` or
`"type": "commonjs"`).
Syntax detection should have no performance impact on CommonJS modules, but it
incurs a slight performance penalty for ES modules; add `"type": "module"` to
the nearest parent `package.json` file to eliminate the performance cost.
A use case unlocked by this feature is the ability to use ES module syntax in
extensionless scripts with no nearby `package.json`.

Thanks to Geoffrey Booth for making this work on [#53619](https://github.com/nodejs/node/pull/53619).

### Performance Improvements to Buffer

Performance of Node.js Buffers have been optimized through multiple PR's with significant
improvements to the `Buffer.copy` and `Buffer.write` methods. These are used throughout
the codebase and should give a nice boost across the board.

Thanks to Robert Nagy for making this work on [#54311](https://github.com/nodejs/node/pull/54311),
[#54324](https://github.com/nodejs/node/pull/54324), and [#54087](https://github.com/nodejs/node/pull/54087).

### Other Notable Changes

* \[[`911de7dd6d`](https://github.com/nodejs/node/commit/911de7dd6d)] - **(SEMVER-MINOR)** **inspector**: support `Network.loadingFailed` event (Kohei Ueno) [#54246](https://github.com/nodejs/node/pull/54246)
* \[[`9ee4b16bd8`](https://github.com/nodejs/node/commit/9ee4b16bd8)] - **(SEMVER-MINOR)** **lib**: rewrite AsyncLocalStorage without async\_hooks (Stephen Belanger) [#48528](https://github.com/nodejs/node/pull/48528)

### Commits

* \[[`c6544ff5a6`](https://github.com/nodejs/node/commit/c6544ff5a6)] - **benchmark**: use assert.ok searchparams (Rafael Gonzaga) [#54334](https://github.com/nodejs/node/pull/54334)
* \[[`51b8576897`](https://github.com/nodejs/node/commit/51b8576897)] - **benchmark**: add stream.compose benchmark (jakecastelli) [#54308](https://github.com/nodejs/node/pull/54308)
* \[[`c166036515`](https://github.com/nodejs/node/commit/c166036515)] - **benchmark**: rename count to n (Rafael Gonzaga) [#54271](https://github.com/nodejs/node/pull/54271)
* \[[`1be0ee76ef`](https://github.com/nodejs/node/commit/1be0ee76ef)] - **benchmark**: change assert() to assert.ok() (Rafael Gonzaga) [#54254](https://github.com/nodejs/node/pull/54254)
* \[[`4dd229f546`](https://github.com/nodejs/node/commit/4dd229f546)] - **benchmark**: support --help in CLI (Aviv Keller) [#53358](https://github.com/nodejs/node/pull/53358)
* \[[`a5a320cd5b`](https://github.com/nodejs/node/commit/a5a320cd5b)] - **benchmark**: remove force option as force defaults to true (Yelim Koo) [#54203](https://github.com/nodejs/node/pull/54203)
* \[[`db0a80a0eb`](https://github.com/nodejs/node/commit/db0a80a0eb)] - **benchmark**: use assert.ok instead of assert (Rafael Gonzaga) [#54176](https://github.com/nodejs/node/pull/54176)
* \[[`8ba53ae7b7`](https://github.com/nodejs/node/commit/8ba53ae7b7)] - **buffer**: properly apply dst offset and src length on fast path (Robert Nagy) [#54391](https://github.com/nodejs/node/pull/54391)
* \[[`a5a60e6823`](https://github.com/nodejs/node/commit/a5a60e6823)] - **buffer**: use fast API for writing one-byte strings (Robert Nagy) [#54311](https://github.com/nodejs/node/pull/54311)
* \[[`7b641bc2bd`](https://github.com/nodejs/node/commit/7b641bc2bd)] - **buffer**: optimize byteLength for short strings (Robert Nagy) [#54345](https://github.com/nodejs/node/pull/54345)
* \[[`28ca678f81`](https://github.com/nodejs/node/commit/28ca678f81)] - **buffer**: optimize byteLength for common encodings (Robert Nagy) [#54342](https://github.com/nodejs/node/pull/54342)
* \[[`12785559be`](https://github.com/nodejs/node/commit/12785559be)] - **buffer**: optimize createFromString (Robert Nagy) [#54324](https://github.com/nodejs/node/pull/54324)
* \[[`f7f7b0c498`](https://github.com/nodejs/node/commit/f7f7b0c498)] - **buffer**: optimize for common encodings (Robert Nagy) [#54319](https://github.com/nodejs/node/pull/54319)
* \[[`37631f826b`](https://github.com/nodejs/node/commit/37631f826b)] - **buffer**: add JSDoc to blob bytes method (Roberto Simonini) [#54117](https://github.com/nodejs/node/pull/54117)
* \[[`ab6fae9dbf`](https://github.com/nodejs/node/commit/ab6fae9dbf)] - **buffer**: faster type check (Robert Nagy) [#54088](https://github.com/nodejs/node/pull/54088)
* \[[`9f8f26eb2f`](https://github.com/nodejs/node/commit/9f8f26eb2f)] - **buffer**: use native copy impl (Robert Nagy) [#54087](https://github.com/nodejs/node/pull/54087)
* \[[`019ebf03c1`](https://github.com/nodejs/node/commit/019ebf03c1)] - **buffer**: use faster integer argument check (Robert Nagy) [#54089](https://github.com/nodejs/node/pull/54089)
* \[[`c640a2f24c`](https://github.com/nodejs/node/commit/c640a2f24c)] - **build**: always disable strict aliasing (Michaël Zasso) [#54339](https://github.com/nodejs/node/pull/54339)
* \[[`6aa1d9e855`](https://github.com/nodejs/node/commit/6aa1d9e855)] - **build**: update `ruff` to `0.5.2` (Aviv Keller) [#53909](https://github.com/nodejs/node/pull/53909)
* \[[`350e699443`](https://github.com/nodejs/node/commit/350e699443)] - **build**: support `lint-js-fix` in `vcbuild.bat` (Aviv Keller) [#53695](https://github.com/nodejs/node/pull/53695)
* \[[`98fed763f7`](https://github.com/nodejs/node/commit/98fed763f7)] - **build**: add `--without-amaro` build flag (Antoine du Hamel) [#54136](https://github.com/nodejs/node/pull/54136)
* \[[`1ca598c5ce`](https://github.com/nodejs/node/commit/1ca598c5ce)] - **cli**: allow `--test-[name/skip]-pattern` in `NODE_OPTIONS` (Aviv Keller) [#53001](https://github.com/nodejs/node/pull/53001)
* \[[`37960a67ae`](https://github.com/nodejs/node/commit/37960a67ae)] - **console**: use validateOneOf for colorMode validation (HEESEUNG) [#54245](https://github.com/nodejs/node/pull/54245)
* \[[`d52f515bab`](https://github.com/nodejs/node/commit/d52f515bab)] - **crypto**: include NODE\_EXTRA\_CA\_CERTS in all secure contexts by default (Eric Bickle) [#44529](https://github.com/nodejs/node/pull/44529)
* \[[`b6a3e61353`](https://github.com/nodejs/node/commit/b6a3e61353)] - **deps**: update amaro to 0.1.6 (Node.js GitHub Bot) [#54374](https://github.com/nodejs/node/pull/54374)
* \[[`0d716ad3f3`](https://github.com/nodejs/node/commit/0d716ad3f3)] - **deps**: update simdutf to 5.3.4 (Node.js GitHub Bot) [#54312](https://github.com/nodejs/node/pull/54312)
* \[[`18bfea5f33`](https://github.com/nodejs/node/commit/18bfea5f33)] - **deps**: update zlib to 1.3.0.1-motley-71660e1 (Node.js GitHub Bot) [#53464](https://github.com/nodejs/node/pull/53464)
* \[[`d0c23f332f`](https://github.com/nodejs/node/commit/d0c23f332f)] - **deps**: update zlib to 1.3.0.1-motley-c2469fd (Node.js GitHub Bot) [#53464](https://github.com/nodejs/node/pull/53464)
* \[[`e7db63972c`](https://github.com/nodejs/node/commit/e7db63972c)] - **deps**: update zlib to 1.3.0.1-motley-68e57e6 (Node.js GitHub Bot) [#53464](https://github.com/nodejs/node/pull/53464)
* \[[`713ae95555`](https://github.com/nodejs/node/commit/713ae95555)] - **deps**: update zlib to 1.3.0.1-motley-8b7eff8 (Node.js GitHub Bot) [#53464](https://github.com/nodejs/node/pull/53464)
* \[[`758c9df36e`](https://github.com/nodejs/node/commit/758c9df36e)] - **deps**: update zlib to 1.3.0.1-motley-e432200 (Node.js GitHub Bot) [#53464](https://github.com/nodejs/node/pull/53464)
* \[[`fe7e6c9563`](https://github.com/nodejs/node/commit/fe7e6c9563)] - **deps**: update zlib to 1.3.0.1-motley-887bb57 (Node.js GitHub Bot) [#53464](https://github.com/nodejs/node/pull/53464)
* \[[`35722b7bca`](https://github.com/nodejs/node/commit/35722b7bca)] - **deps**: update simdjson to 3.10.0 (Node.js GitHub Bot) [#54197](https://github.com/nodejs/node/pull/54197)
* \[[`a2a41557db`](https://github.com/nodejs/node/commit/a2a41557db)] - **deps**: fix GN build warning in ncrypto (Cheng) [#54222](https://github.com/nodejs/node/pull/54222)
* \[[`869da204d7`](https://github.com/nodejs/node/commit/869da204d7)] - **deps**: update c-ares to v1.33.0 (Node.js GitHub Bot) [#54198](https://github.com/nodejs/node/pull/54198)
* \[[`e0d503a715`](https://github.com/nodejs/node/commit/e0d503a715)] - **deps**: update nbytes to 0.1.1 (Node.js GitHub Bot) [#54277](https://github.com/nodejs/node/pull/54277)
* \[[`b0c768dae1`](https://github.com/nodejs/node/commit/b0c768dae1)] - **deps**: update undici to 6.19.7 (Node.js GitHub Bot) [#54286](https://github.com/nodejs/node/pull/54286)
* \[[`ef9a950cb9`](https://github.com/nodejs/node/commit/ef9a950cb9)] - **deps**: update acorn to 8.12.1 (Node.js GitHub Bot) [#53465](https://github.com/nodejs/node/pull/53465)
* \[[`1597a1139a`](https://github.com/nodejs/node/commit/1597a1139a)] - **deps**: update undici to 6.19.5 (Node.js GitHub Bot) [#54076](https://github.com/nodejs/node/pull/54076)
* \[[`103e4db3e0`](https://github.com/nodejs/node/commit/103e4db3e0)] - **deps**: update simdutf to 5.3.1 (Node.js GitHub Bot) [#54196](https://github.com/nodejs/node/pull/54196)
* \[[`9f115ba9e9`](https://github.com/nodejs/node/commit/9f115ba9e9)] - **doc**: fix error description of the max header size (Egawa Ryo) [#54125](https://github.com/nodejs/node/pull/54125)
* \[[`f967ab3810`](https://github.com/nodejs/node/commit/f967ab3810)] - **doc**: add git node security --cleanup (Rafael Gonzaga) [#54381](https://github.com/nodejs/node/pull/54381)
* \[[`8883c01afa`](https://github.com/nodejs/node/commit/8883c01afa)] - **doc**: add note on weakness of permission model (Tobias Nießen) [#54268](https://github.com/nodejs/node/pull/54268)
* \[[`824bd58bc5`](https://github.com/nodejs/node/commit/824bd58bc5)] - **doc**: add versions when `--watch-preserve-output` was added (Théo LUDWIG) [#54328](https://github.com/nodejs/node/pull/54328)
* \[[`33795cfd49`](https://github.com/nodejs/node/commit/33795cfd49)] - **doc**: replace v19 mention in Current release (Rafael Gonzaga) [#54361](https://github.com/nodejs/node/pull/54361)
* \[[`aa6e770ea5`](https://github.com/nodejs/node/commit/aa6e770ea5)] - **doc**: correct peformance entry types (Jason Zhang) [#54263](https://github.com/nodejs/node/pull/54263)
* \[[`4b099ce1bd`](https://github.com/nodejs/node/commit/4b099ce1bd)] - **doc**: fix typo in method name in the sea doc (Eliyah Sundström) [#54027](https://github.com/nodejs/node/pull/54027)
* \[[`8a8d1d2281`](https://github.com/nodejs/node/commit/8a8d1d2281)] - **doc**: mark process.nextTick legacy (Marco Ippolito) [#51280](https://github.com/nodejs/node/pull/51280)
* \[[`6f4b5d998e`](https://github.com/nodejs/node/commit/6f4b5d998e)] - **doc**: add esm examples to node:http2 (Alfredo González) [#54292](https://github.com/nodejs/node/pull/54292)
* \[[`1535469c12`](https://github.com/nodejs/node/commit/1535469c12)] - **doc**: explicitly mention node:fs module restriction (Rafael Gonzaga) [#54269](https://github.com/nodejs/node/pull/54269)
* \[[`26c37f7910`](https://github.com/nodejs/node/commit/26c37f7910)] - **doc**: remove module-based permission doc (Rafael Gonzaga) [#54266](https://github.com/nodejs/node/pull/54266)
* \[[`971b9f31f5`](https://github.com/nodejs/node/commit/971b9f31f5)] - **doc**: update `buffer.constants.MAX_LENGTH` size (Samuli Asmala) [#54207](https://github.com/nodejs/node/pull/54207)
* \[[`3106149965`](https://github.com/nodejs/node/commit/3106149965)] - **doc**: warn for windows build bug (Jason Zhang) [#54217](https://github.com/nodejs/node/pull/54217)
* \[[`55f8ac3e89`](https://github.com/nodejs/node/commit/55f8ac3e89)] - **doc**: make some parameters optional in `tracingChannel.traceCallback` (Deokjin Kim) [#54068](https://github.com/nodejs/node/pull/54068)
* \[[`e3e2f22cab`](https://github.com/nodejs/node/commit/e3e2f22cab)] - **doc**: add esm examples to node:dns (Alfredo González) [#54172](https://github.com/nodejs/node/pull/54172)
* \[[`0429b1eb9d`](https://github.com/nodejs/node/commit/0429b1eb9d)] - **doc**: add KevinEady as a triager (Chengzhong Wu) [#54179](https://github.com/nodejs/node/pull/54179)
* \[[`4bfa7d8e54`](https://github.com/nodejs/node/commit/4bfa7d8e54)] - **doc**: add esm examples to node:console (Alfredo González) [#54108](https://github.com/nodejs/node/pull/54108)
* \[[`2f5309fc22`](https://github.com/nodejs/node/commit/2f5309fc22)] - **doc**: fix sea assets example (Sadzurami) [#54192](https://github.com/nodejs/node/pull/54192)
* \[[`88aef5a39d`](https://github.com/nodejs/node/commit/88aef5a39d)] - **doc**: add links to security steward companies (Aviv Keller) [#52981](https://github.com/nodejs/node/pull/52981)
* \[[`5175903c23`](https://github.com/nodejs/node/commit/5175903c23)] - **doc**: move `onread` option from `socket.connect()` to `new net.socket()` (sendoru) [#54194](https://github.com/nodejs/node/pull/54194)
* \[[`144637e845`](https://github.com/nodejs/node/commit/144637e845)] - **doc**: move release key for Myles Borins (Richard Lau) [#54059](https://github.com/nodejs/node/pull/54059)
* \[[`358fdacec6`](https://github.com/nodejs/node/commit/358fdacec6)] - **doc**: refresh instructions for building node from source (Liran Tal) [#53768](https://github.com/nodejs/node/pull/53768)
* \[[`11fdaa6ad2`](https://github.com/nodejs/node/commit/11fdaa6ad2)] - **doc**: add documentation for blob.bytes() method (jaexxin) [#54114](https://github.com/nodejs/node/pull/54114)
* \[[`db3b0df42c`](https://github.com/nodejs/node/commit/db3b0df42c)] - **doc**: add missing new lines to custom test reporter examples (Eddie Abbondanzio) [#54152](https://github.com/nodejs/node/pull/54152)
* \[[`1cafefd2cf`](https://github.com/nodejs/node/commit/1cafefd2cf)] - **doc**: fix worker threadId/destination typo (Thomas Hunter II) [#53933](https://github.com/nodejs/node/pull/53933)
* \[[`7772b46038`](https://github.com/nodejs/node/commit/7772b46038)] - **doc**: update list of Triagers on the `README.md` (Antoine du Hamel) [#54138](https://github.com/nodejs/node/pull/54138)
* \[[`af99ba3dc9`](https://github.com/nodejs/node/commit/af99ba3dc9)] - **doc**: remove unused imports from worker\_threads.md (Yelim Koo) [#54147](https://github.com/nodejs/node/pull/54147)
* \[[`826edc4341`](https://github.com/nodejs/node/commit/826edc4341)] - **doc**: expand troubleshooting section (Liran Tal) [#53808](https://github.com/nodejs/node/pull/53808)
* \[[`923195b624`](https://github.com/nodejs/node/commit/923195b624)] - **doc**: clarify `useCodeCache` setting for cross-platform SEA generation (Yelim Koo) [#53994](https://github.com/nodejs/node/pull/53994)
* \[[`7c305a4900`](https://github.com/nodejs/node/commit/7c305a4900)] - **doc, meta**: replace command with link to keys (Aviv Keller) [#53745](https://github.com/nodejs/node/pull/53745)
* \[[`6f986e0ee6`](https://github.com/nodejs/node/commit/6f986e0ee6)] - **doc, test**: simplify test README table (Aviv Keller) [#53971](https://github.com/nodejs/node/pull/53971)
* \[[`112228c15a`](https://github.com/nodejs/node/commit/112228c15a)] - **fs**: remove unnecessary option argument validation (Jonas) [#53958](https://github.com/nodejs/node/pull/53958)
* \[[`911de7dd6d`](https://github.com/nodejs/node/commit/911de7dd6d)] - **(SEMVER-MINOR)** **inspector**: support `Network.loadingFailed` event (Kohei Ueno) [#54246](https://github.com/nodejs/node/pull/54246)
* \[[`1e825915d5`](https://github.com/nodejs/node/commit/1e825915d5)] - **inspector**: provide detailed info to fix DevTools frontend errors (Kohei Ueno) [#54156](https://github.com/nodejs/node/pull/54156)
* \[[`417120a3a3`](https://github.com/nodejs/node/commit/417120a3a3)] - **lib**: replace spread operator with primordials function (YoonSoo\_Shin) [#54053](https://github.com/nodejs/node/pull/54053)
* \[[`09f411e6f6`](https://github.com/nodejs/node/commit/09f411e6f6)] - **lib**: avoid for of loop and remove unnecessary variable in zlib (YoonSoo\_Shin) [#54258](https://github.com/nodejs/node/pull/54258)
* \[[`b8970570b0`](https://github.com/nodejs/node/commit/b8970570b0)] - **lib**: improve async\_context\_frame structure (Stephen Belanger) [#54239](https://github.com/nodejs/node/pull/54239)
* \[[`783322fa16`](https://github.com/nodejs/node/commit/783322fa16)] - **lib**: fix unhandled errors in webstream adapters (Fedor Indutny) [#54206](https://github.com/nodejs/node/pull/54206)
* \[[`425b9562b9`](https://github.com/nodejs/node/commit/425b9562b9)] - **lib**: fix typos in comments within internal/streams (YoonSoo\_Shin) [#54093](https://github.com/nodejs/node/pull/54093)
* \[[`9ee4b16bd8`](https://github.com/nodejs/node/commit/9ee4b16bd8)] - **(SEMVER-MINOR)** **lib**: rewrite AsyncLocalStorage without async\_hooks (Stephen Belanger) [#48528](https://github.com/nodejs/node/pull/48528)
* \[[`8c9a4ae12b`](https://github.com/nodejs/node/commit/8c9a4ae12b)] - **lib,permission**: support Buffer to permission.has (Rafael Gonzaga) [#54104](https://github.com/nodejs/node/pull/54104)
* \[[`c8e358c96c`](https://github.com/nodejs/node/commit/c8e358c96c)] - **meta**: add test-permission-\* CODEOWNERS (Rafael Gonzaga) [#54267](https://github.com/nodejs/node/pull/54267)
* \[[`581c155cf8`](https://github.com/nodejs/node/commit/581c155cf8)] - **meta**: move one or more collaborators to emeritus (Node.js GitHub Bot) [#54210](https://github.com/nodejs/node/pull/54210)
* \[[`3f0d7344e3`](https://github.com/nodejs/node/commit/3f0d7344e3)] - **meta**: add module label for the lib/internal/modules folder (Aviv Keller) [#52858](https://github.com/nodejs/node/pull/52858)
* \[[`0157ec6bbd`](https://github.com/nodejs/node/commit/0157ec6bbd)] - **meta**: bump `actions/upload-artifact` from 4.3.3 to 4.3.4 (dependabot\[bot]) [#54166](https://github.com/nodejs/node/pull/54166)
* \[[`7fa95d2360`](https://github.com/nodejs/node/commit/7fa95d2360)] - **meta**: bump `actions/download-artifact` from 4.1.7 to 4.1.8 (dependabot\[bot]) [#54167](https://github.com/nodejs/node/pull/54167)
* \[[`acc5b9a0c5`](https://github.com/nodejs/node/commit/acc5b9a0c5)] - **meta**: bump actions/setup-python from 5.1.0 to 5.1.1 (dependabot\[bot]) [#54165](https://github.com/nodejs/node/pull/54165)
* \[[`dede30a8d0`](https://github.com/nodejs/node/commit/dede30a8d0)] - **meta**: bump `step-security/harden-runner` from 2.8.1 to 2.9.0 (dependabot\[bot]) [#54169](https://github.com/nodejs/node/pull/54169)
* \[[`b733854eac`](https://github.com/nodejs/node/commit/b733854eac)] - **meta**: bump `actions/setup-node` from 4.0.2 to 4.0.3 (dependabot\[bot]) [#54170](https://github.com/nodejs/node/pull/54170)
* \[[`6a9f168cc6`](https://github.com/nodejs/node/commit/6a9f168cc6)] - **meta**: bump `github/codeql-action` from 3.25.11 to 3.25.15 (dependabot\[bot]) [#54168](https://github.com/nodejs/node/pull/54168)
* \[[`9bbd85e4fe`](https://github.com/nodejs/node/commit/9bbd85e4fe)] - **meta**: bump `ossf/scorecard-action` from 2.3.3 to 2.4.0 (dependabot\[bot]) [#54171](https://github.com/nodejs/node/pull/54171)
* \[[`33633eebd9`](https://github.com/nodejs/node/commit/33633eebd9)] - **meta**: add typescript team to codeowners (Marco Ippolito) [#54101](https://github.com/nodejs/node/pull/54101)
* \[[`240d9296c1`](https://github.com/nodejs/node/commit/240d9296c1)] - **(SEMVER-MINOR)** **module**: add --experimental-transform-types flag (Marco Ippolito) [#54283](https://github.com/nodejs/node/pull/54283)
* \[[`66dcb2a571`](https://github.com/nodejs/node/commit/66dcb2a571)] - **(SEMVER-MINOR)** **module**: unflag detect-module (Geoffrey Booth) [#53619](https://github.com/nodejs/node/pull/53619)
* \[[`100225fbe1`](https://github.com/nodejs/node/commit/100225fbe1)] - **module**: do not attempt to strip type when there's no source (Antoine du Hamel) [#54287](https://github.com/nodejs/node/pull/54287)
* \[[`1ba2000703`](https://github.com/nodejs/node/commit/1ba2000703)] - **module**: refactor ts parser loading (Marco Ippolito) [#54243](https://github.com/nodejs/node/pull/54243)
* \[[`13cc480030`](https://github.com/nodejs/node/commit/13cc480030)] - **module**: remove outdated comment (Michaël Zasso) [#54118](https://github.com/nodejs/node/pull/54118)
* \[[`e676d98435`](https://github.com/nodejs/node/commit/e676d98435)] - **module,win**: fix long path resolve (Hüseyin Açacak) [#53294](https://github.com/nodejs/node/pull/53294)
* \[[`9aec536083`](https://github.com/nodejs/node/commit/9aec536083)] - **path**: change `posix.join` to use array (Wiyeong Seo) [#54331](https://github.com/nodejs/node/pull/54331)
* \[[`8a770cf5c9`](https://github.com/nodejs/node/commit/8a770cf5c9)] - **path**: fix relative on Windows (Hüseyin Açacak) [#53991](https://github.com/nodejs/node/pull/53991)
* \[[`267cd7f361`](https://github.com/nodejs/node/commit/267cd7f361)] - **path**: use the correct name in `validateString` (Benjamin Pasero) [#53669](https://github.com/nodejs/node/pull/53669)
* \[[`31adeea855`](https://github.com/nodejs/node/commit/31adeea855)] - **sea**: don't set code cache flags when snapshot is used (Joyee Cheung) [#54120](https://github.com/nodejs/node/pull/54120)
* \[[`7f1bf1ce24`](https://github.com/nodejs/node/commit/7f1bf1ce24)] - **sqlite**: split up large test file (Colin Ihrig) [#54014](https://github.com/nodejs/node/pull/54014)
* \[[`94e2ea6f5c`](https://github.com/nodejs/node/commit/94e2ea6f5c)] - **sqlite**: ensure statement finalization on db close (Colin Ihrig) [#54014](https://github.com/nodejs/node/pull/54014)
* \[[`e077ff1f38`](https://github.com/nodejs/node/commit/e077ff1f38)] - **src**: update compile cache storage structure (Joyee Cheung) [#54291](https://github.com/nodejs/node/pull/54291)
* \[[`4e4d1def7e`](https://github.com/nodejs/node/commit/4e4d1def7e)] - **src**: refactor http parser binding initialization (Joyee Cheung) [#54276](https://github.com/nodejs/node/pull/54276)
* \[[`409d9eb09b`](https://github.com/nodejs/node/commit/409d9eb09b)] - **src**: shift even moar x509 to ncrypto (James M Snell) [#54340](https://github.com/nodejs/node/pull/54340)
* \[[`f87aa27274`](https://github.com/nodejs/node/commit/f87aa27274)] - **src**: don't match after `--` in `Dotenv::GetPathFromArgs` (Aviv Keller) [#54237](https://github.com/nodejs/node/pull/54237)
* \[[`b6927dd981`](https://github.com/nodejs/node/commit/b6927dd981)] - **src**: move some X509Certificate stuff to ncrypto (James M Snell) [#54241](https://github.com/nodejs/node/pull/54241)
* \[[`a394219fa5`](https://github.com/nodejs/node/commit/a394219fa5)] - **src**: skip inspector wait in internal workers (Chengzhong Wu) [#54219](https://github.com/nodejs/node/pull/54219)
* \[[`8daeccfe92`](https://github.com/nodejs/node/commit/8daeccfe92)] - **src**: shift more crypto impl details to ncrypto (James M Snell) [#54028](https://github.com/nodejs/node/pull/54028)
* \[[`e619133ac9`](https://github.com/nodejs/node/commit/e619133ac9)] - **src**: move spkac methods to ncrypto (James M Snell) [#53985](https://github.com/nodejs/node/pull/53985)
* \[[`b52c2fff75`](https://github.com/nodejs/node/commit/b52c2fff75)] - **src**: account for OpenSSL unexpected version (Shelley Vohr) [#54038](https://github.com/nodejs/node/pull/54038)
* \[[`0b16af1689`](https://github.com/nodejs/node/commit/0b16af1689)] - **src,test**: track `URL.canParse` fast API calls (Michaël Zasso) [#54356](https://github.com/nodejs/node/pull/54356)
* \[[`2be78b03c3`](https://github.com/nodejs/node/commit/2be78b03c3)] - **src,test**: ensure that V8 fast APIs are called (Michaël Zasso) [#54317](https://github.com/nodejs/node/pull/54317)
* \[[`9297d29cdb`](https://github.com/nodejs/node/commit/9297d29cdb)] - **stream**: make checking pendingcb on WritableStream backward compatible (jakecastelli) [#54142](https://github.com/nodejs/node/pull/54142)
* \[[`2a6a12e493`](https://github.com/nodejs/node/commit/2a6a12e493)] - **stream**: throw TypeError when criteria fulfilled in getIterator (jakecastelli) [#53825](https://github.com/nodejs/node/pull/53825)
* \[[`7f68cc0f7f`](https://github.com/nodejs/node/commit/7f68cc0f7f)] - **test**: make snapshot comparison more flexible (Shelley Vohr) [#54375](https://github.com/nodejs/node/pull/54375)
* \[[`3df7938832`](https://github.com/nodejs/node/commit/3df7938832)] - **test**: make sure current run result is pushed and reset (jakecastelli) [#54332](https://github.com/nodejs/node/pull/54332)
* \[[`3e25be7b28`](https://github.com/nodejs/node/commit/3e25be7b28)] - **test**: use relative paths in test-cli-permission tests (sendoru) [#54188](https://github.com/nodejs/node/pull/54188)
* \[[`f49f1bb3e9`](https://github.com/nodejs/node/commit/f49f1bb3e9)] - **test**: unmark test-sqlite as flaky (Colin Ihrig) [#54014](https://github.com/nodejs/node/pull/54014)
* \[[`2f68a74702`](https://github.com/nodejs/node/commit/2f68a74702)] - **test**: fix timeout not being cleared (Isaac-yz-Liu) [#54242](https://github.com/nodejs/node/pull/54242)
* \[[`f5cfa4454e`](https://github.com/nodejs/node/commit/f5cfa4454e)] - **test**: refactor `test-runner-module-mocking` (Antoine du Hamel) [#54233](https://github.com/nodejs/node/pull/54233)
* \[[`b85b13b418`](https://github.com/nodejs/node/commit/b85b13b418)] - **test**: use assert.{s,deepS}trictEqual() (Luigi Pinca) [#54208](https://github.com/nodejs/node/pull/54208)
* \[[`6bcbfcd7bc`](https://github.com/nodejs/node/commit/6bcbfcd7bc)] - **test**: add subtests to test-node-run (sungpaks) [#54204](https://github.com/nodejs/node/pull/54204)
* \[[`dafe97548f`](https://github.com/nodejs/node/commit/dafe97548f)] - **test**: set test-structuredclone-jstransferable non-flaky (Stefan Stojanovic) [#54115](https://github.com/nodejs/node/pull/54115)
* \[[`be61793db5`](https://github.com/nodejs/node/commit/be61793db5)] - **test**: update wpt test for streams (devstone) [#54129](https://github.com/nodejs/node/pull/54129)
* \[[`670c796449`](https://github.com/nodejs/node/commit/670c796449)] - **test**: fix typo in test (Sonny) [#54137](https://github.com/nodejs/node/pull/54137)
* \[[`1a15f3f613`](https://github.com/nodejs/node/commit/1a15f3f613)] - **test**: add initial pull delay and prototype pollution prevention tests (Sonny) [#54061](https://github.com/nodejs/node/pull/54061)
* \[[`5dbff81b71`](https://github.com/nodejs/node/commit/5dbff81b71)] - **test**: add coverage for webstorage quota (jakecastelli) [#53964](https://github.com/nodejs/node/pull/53964)
* \[[`141e9fe7cc`](https://github.com/nodejs/node/commit/141e9fe7cc)] - **test\_runner**: use validateStringArray for `timers.enable()` (Deokjin Kim) [#49534](https://github.com/nodejs/node/pull/49534)
* \[[`e70711e190`](https://github.com/nodejs/node/commit/e70711e190)] - **test\_runner**: report failures in filtered suites (Colin Ihrig) [#54387](https://github.com/nodejs/node/pull/54387)
* \[[`7766c1dc9b`](https://github.com/nodejs/node/commit/7766c1dc9b)] - **test\_runner**: remove parseCommandLine() from test.js (Colin Ihrig) [#54353](https://github.com/nodejs/node/pull/54353)
* \[[`961cbf0be0`](https://github.com/nodejs/node/commit/961cbf0be0)] - **test\_runner**: refactor hook creation (Colin Ihrig) [#54353](https://github.com/nodejs/node/pull/54353)
* \[[`69c78ca2f5`](https://github.com/nodejs/node/commit/69c78ca2f5)] - **test\_runner**: return setup() from parseCommandLine() (Colin Ihrig) [#54353](https://github.com/nodejs/node/pull/54353)
* \[[`ed1ede8c26`](https://github.com/nodejs/node/commit/ed1ede8c26)] - **test\_runner**: pass global options to createTestTree() (Colin Ihrig) [#54353](https://github.com/nodejs/node/pull/54353)
* \[[`1e88045a69`](https://github.com/nodejs/node/commit/1e88045a69)] - **test\_runner**: pass harness object as option to root test (Colin Ihrig) [#54353](https://github.com/nodejs/node/pull/54353)
* \[[`e3378f0679`](https://github.com/nodejs/node/commit/e3378f0679)] - **test\_runner**: use run() argument names in parseCommandLine() (Colin Ihrig) [#54353](https://github.com/nodejs/node/pull/54353)
* \[[`676bbd5c09`](https://github.com/nodejs/node/commit/676bbd5c09)] - **test\_runner**: fix delete test file cause dependency file not watched (jakecastelli) [#53533](https://github.com/nodejs/node/pull/53533)
* \[[`fe793a6103`](https://github.com/nodejs/node/commit/fe793a6103)] - **test\_runner**: do not expose internal loader (Antoine du Hamel) [#54106](https://github.com/nodejs/node/pull/54106)
* \[[`7fad771bbf`](https://github.com/nodejs/node/commit/7fad771bbf)] - **test\_runner**: fix erroneous diagnostic warning when only: false (Pietro Marchini) [#54116](https://github.com/nodejs/node/pull/54116)
* \[[`dc465736fb`](https://github.com/nodejs/node/commit/dc465736fb)] - **test\_runner**: make mock\_loader not confuse CJS and ESM resolution (Sung Ye In) [#53846](https://github.com/nodejs/node/pull/53846)
* \[[`5a1afb2139`](https://github.com/nodejs/node/commit/5a1afb2139)] - **test\_runner**: remove outdated comment (Colin Ihrig) [#54146](https://github.com/nodejs/node/pull/54146)
* \[[`20a01fcc39`](https://github.com/nodejs/node/commit/20a01fcc39)] - **test\_runner**: run after hooks even if test is aborted (Colin Ihrig) [#54151](https://github.com/nodejs/node/pull/54151)
* \[[`df428adb6c`](https://github.com/nodejs/node/commit/df428adb6c)] - **tools**: remove header from c-ares license (Aviv Keller) [#54335](https://github.com/nodejs/node/pull/54335)
* \[[`b659fc0f2b`](https://github.com/nodejs/node/commit/b659fc0f2b)] - **tools**: add find pyenv path on windows (Marco Ippolito) [#54314](https://github.com/nodejs/node/pull/54314)
* \[[`b93c6d9f38`](https://github.com/nodejs/node/commit/b93c6d9f38)] - **tools**: make undici updater build wasm from src (Michael Dawson) [#54128](https://github.com/nodejs/node/pull/54128)
* \[[`3835131559`](https://github.com/nodejs/node/commit/3835131559)] - **tools**: add workflow to ensure `README` lists are in sync with gh teams (Antoine du Hamel) [#53901](https://github.com/nodejs/node/pull/53901)
* \[[`e218b7ca8a`](https://github.com/nodejs/node/commit/e218b7ca8a)] - **tools**: add strip-types to label system (Marco Ippolito) [#54185](https://github.com/nodejs/node/pull/54185)
* \[[`8b35f0e601`](https://github.com/nodejs/node/commit/8b35f0e601)] - **tools**: update eslint to 9.8.0 (Node.js GitHub Bot) [#54073](https://github.com/nodejs/node/pull/54073)
* \[[`d83421fbe5`](https://github.com/nodejs/node/commit/d83421fbe5)] - **tty**: initialize winSize array with values (Michaël Zasso) [#54281](https://github.com/nodejs/node/pull/54281)
* \[[`a4768374f2`](https://github.com/nodejs/node/commit/a4768374f2)] - **typings**: add util.styleText type definition (Rafael Gonzaga) [#54252](https://github.com/nodejs/node/pull/54252)
* \[[`a4aecd2755`](https://github.com/nodejs/node/commit/a4aecd2755)] - **typings**: add missing binding function `writeFileUtf8()` (Jungku Lee) [#54110](https://github.com/nodejs/node/pull/54110)
* \[[`0bed600df9`](https://github.com/nodejs/node/commit/0bed600df9)] - **url**: modify pathToFileURL to handle extended UNC path (Early Riser) [#54262](https://github.com/nodejs/node/pull/54262)
* \[[`037672f15d`](https://github.com/nodejs/node/commit/037672f15d)] - **url**: improve resolveObject with ObjectAssign (Early Riser) [#54092](https://github.com/nodejs/node/pull/54092)
* \[[`4d8b53e475`](https://github.com/nodejs/node/commit/4d8b53e475)] - **watch**: reload changes in contents of --env-file (Marek Piechut) [#54109](https://github.com/nodejs/node/pull/54109)

<a id="22.6.0"></a>

## 2024-08-06, Version 22.6.0 (Current), @RafaelGSS

### Experimental TypeScript support via strip types

Node.js introduces the `--experimental-strip-types` flag for initial TypeScript support.
This feature strips type annotations from .ts files, allowing them to run
without transforming TypeScript-specific syntax. Current limitations include:

* Supports only inline type annotations, not features like `enums` or `namespaces`.
* Requires explicit file extensions in import and require statements.
* Enforces the use of the type keyword for type imports to avoid runtime errors.
* Disabled for TypeScript in _node\_modules_ by default.

Thanks [Marco Ippolito](https://github.com/marco-ippolito) for working on this.

### Experimental Network Inspection Support in Node.js

This update introduces the initial support for network inspection in Node.js.
Currently, this is an experimental feature, so you need to enable it using the `--experimental-network-inspection` flag.
With this feature enabled, you can inspect network activities occurring within a JavaScript application.

To use network inspection, start your Node.js application with the following command:

```console
$ node --inspect-wait --experimental-network-inspection index.js
```

Please note that the network inspection capabilities are in active development.
We are actively working on enhancing this feature and will continue to expand its functionality in future updates.

* Network inspection is limited to the `http` and `https` modules only.
* The Network tab in Chrome DevTools will not be available until the
  [feature request on the Chrome DevTools side](https://issues.chromium.org/issues/353924015) is addressed.

Thanks [Kohei Ueno](https://github.com/cola119) for working on this.

### Other Notable Changes

* \[[`15a94e67b1`](https://github.com/nodejs/node/commit/15a94e67b1)] - **lib,src**: drop --experimental-network-imports (Rafael Gonzaga) [#53822](https://github.com/nodejs/node/pull/53822)
* \[[`68e444d2d8`](https://github.com/nodejs/node/commit/68e444d2d8)] - **(SEMVER-MINOR)** **http**: add diagnostics channel `http.client.request.error` (Kohei Ueno) [#54054](https://github.com/nodejs/node/pull/54054)
* \[[`2d982d3dee`](https://github.com/nodejs/node/commit/2d982d3dee)] - **(SEMVER-MINOR)** **deps**: V8: backport 7857eb34db42 (Stephen Belanger) [#53997](https://github.com/nodejs/node/pull/53997)
* \[[`15816bd0dd`](https://github.com/nodejs/node/commit/15816bd0dd)] - **(SEMVER-MINOR)** **stream**: expose DuplexPair API (Austin Wright) [#34111](https://github.com/nodejs/node/pull/34111)
* \[[`893c864542`](https://github.com/nodejs/node/commit/893c864542)] - **(SEMVER-MINOR)** **test\_runner**: fix support watch with run(), add globPatterns option (Matteo Collina) [#53866](https://github.com/nodejs/node/pull/53866)
* \[[`048d421ad1`](https://github.com/nodejs/node/commit/048d421ad1)] - **meta**: add jake to collaborators (jakecastelli) [#54004](https://github.com/nodejs/node/pull/54004)
* \[[`6ad6e01bf3`](https://github.com/nodejs/node/commit/6ad6e01bf3)] - **(SEMVER-MINOR)** **test\_runner**: refactor snapshots to get file from context (Colin Ihrig) [#53853](https://github.com/nodejs/node/pull/53853)
* \[[`698e44f8e7`](https://github.com/nodejs/node/commit/698e44f8e7)] - **(SEMVER-MINOR)** **test\_runner**: add context.filePath (Colin Ihrig) [#53853](https://github.com/nodejs/node/pull/53853)

### Commits

* \[[`063f46dc2a`](https://github.com/nodejs/node/commit/063f46dc2a)] - **assert**: use isError instead of instanceof in innerOk (Pietro Marchini) [#53980](https://github.com/nodejs/node/pull/53980)
* \[[`10bea42f81`](https://github.com/nodejs/node/commit/10bea42f81)] - **build**: update gcovr to 7.2 and codecov config (Benjamin E. Coe) [#54019](https://github.com/nodejs/node/pull/54019)
* \[[`7c417c6cf4`](https://github.com/nodejs/node/commit/7c417c6cf4)] - **build**: avoid compiling with VS v17.10 (Hüseyin Açacak) [#53863](https://github.com/nodejs/node/pull/53863)
* \[[`ee97c045b4`](https://github.com/nodejs/node/commit/ee97c045b4)] - **build**: ensure v8\_pointer\_compression\_sandbox is enabled on 64bit (Shelley Vohr) [#53884](https://github.com/nodejs/node/pull/53884)
* \[[`bfbed0afd5`](https://github.com/nodejs/node/commit/bfbed0afd5)] - **build**: fix conflict gyp configs (Chengzhong Wu) [#53605](https://github.com/nodejs/node/pull/53605)
* \[[`0f1fe63e32`](https://github.com/nodejs/node/commit/0f1fe63e32)] - **build**: trigger coverage ci when updating codecov (Yagiz Nizipli) [#53929](https://github.com/nodejs/node/pull/53929)
* \[[`ad62b945f0`](https://github.com/nodejs/node/commit/ad62b945f0)] - **build**: update codecov coverage build count (Yagiz Nizipli) [#53929](https://github.com/nodejs/node/pull/53929)
* \[[`3c40868fd3`](https://github.com/nodejs/node/commit/3c40868fd3)] - **build**: disable test-asan workflow (Michaël Zasso) [#53844](https://github.com/nodejs/node/pull/53844)
* \[[`2a62d6ca57`](https://github.com/nodejs/node/commit/2a62d6ca57)] - **build, tools**: drop leading `/` from `r2dir` (Richard Lau) [#53951](https://github.com/nodejs/node/pull/53951)
* \[[`9c7b009f47`](https://github.com/nodejs/node/commit/9c7b009f47)] - **build,tools**: simplify upload of shasum signatures (Michaël Zasso) [#53892](https://github.com/nodejs/node/pull/53892)
* \[[`057bd44f9f`](https://github.com/nodejs/node/commit/057bd44f9f)] - **child\_process**: fix incomplete prototype pollution hardening (Liran Tal) [#53781](https://github.com/nodejs/node/pull/53781)
* \[[`66f7c595c7`](https://github.com/nodejs/node/commit/66f7c595c7)] - **cli**: document `--inspect` port `0` behavior (Aviv Keller) [#53782](https://github.com/nodejs/node/pull/53782)
* \[[`fad3e74b47`](https://github.com/nodejs/node/commit/fad3e74b47)] - **console**: fix issues with frozen intrinsics (Vinicius Lourenço) [#54070](https://github.com/nodejs/node/pull/54070)
* \[[`e685ecd7ae`](https://github.com/nodejs/node/commit/e685ecd7ae)] - **deps**: update corepack to 0.29.3 (Node.js GitHub Bot) [#54072](https://github.com/nodejs/node/pull/54072)
* \[[`e5f7250e6d`](https://github.com/nodejs/node/commit/e5f7250e6d)] - **deps**: update amaro to 0.0.6 (Node.js GitHub Bot) [#54199](https://github.com/nodejs/node/pull/54199)
* \[[`2c1e9082e8`](https://github.com/nodejs/node/commit/2c1e9082e8)] - **deps**: update amaro to 0.0.5 (Node.js GitHub Bot) [#54199](https://github.com/nodejs/node/pull/54199)
* \[[`2d982d3dee`](https://github.com/nodejs/node/commit/2d982d3dee)] - **(SEMVER-MINOR)** **deps**: V8: backport 7857eb34db42 (Stephen Belanger) [#53997](https://github.com/nodejs/node/pull/53997)
* \[[`1061898462`](https://github.com/nodejs/node/commit/1061898462)] - **deps**: update c-ares to v1.32.3 (Node.js GitHub Bot) [#54020](https://github.com/nodejs/node/pull/54020)
* \[[`f4a7ac5e18`](https://github.com/nodejs/node/commit/f4a7ac5e18)] - **deps**: V8: cherry-pick 35888fee7bba (Joyee Cheung) [#53728](https://github.com/nodejs/node/pull/53728)
* \[[`1176310226`](https://github.com/nodejs/node/commit/1176310226)] - **deps**: add gn build files for ncrypto (Cheng) [#53940](https://github.com/nodejs/node/pull/53940)
* \[[`7a1d5a4f84`](https://github.com/nodejs/node/commit/7a1d5a4f84)] - **deps**: update c-ares to v1.32.2 (Node.js GitHub Bot) [#53865](https://github.com/nodejs/node/pull/53865)
* \[[`66f6a2aec9`](https://github.com/nodejs/node/commit/66f6a2aec9)] - **deps**: V8: cherry-pick 9812cb486e2b (Michaël Zasso) [#53966](https://github.com/nodejs/node/pull/53966)
* \[[`8e66a18ef0`](https://github.com/nodejs/node/commit/8e66a18ef0)] - **deps**: start working on ncrypto dep (James M Snell) [#53803](https://github.com/nodejs/node/pull/53803)
* \[[`c114082b12`](https://github.com/nodejs/node/commit/c114082b12)] - **deps**: fix include\_dirs of nbytes (Cheng) [#53862](https://github.com/nodejs/node/pull/53862)
* \[[`b7315281be`](https://github.com/nodejs/node/commit/b7315281be)] - **doc**: move numCPUs require to top of file in cluster CJS example (Alfredo González) [#53932](https://github.com/nodejs/node/pull/53932)
* \[[`8e7c30c2a4`](https://github.com/nodejs/node/commit/8e7c30c2a4)] - **doc**: update security-release process to automated one (Rafael Gonzaga) [#53877](https://github.com/nodejs/node/pull/53877)
* \[[`52a4206be2`](https://github.com/nodejs/node/commit/52a4206be2)] - **doc**: fix typo in technical-priorities.md (YoonSoo\_Shin) [#54094](https://github.com/nodejs/node/pull/54094)
* \[[`30e18a04a3`](https://github.com/nodejs/node/commit/30e18a04a3)] - **doc**: fix typo in diagnostic tooling support tiers document (Taejin Kim) [#54058](https://github.com/nodejs/node/pull/54058)
* \[[`58aebfd31e`](https://github.com/nodejs/node/commit/58aebfd31e)] - **doc**: move GeoffreyBooth to TSC regular member (Geoffrey Booth) [#54047](https://github.com/nodejs/node/pull/54047)
* \[[`c1634c7213`](https://github.com/nodejs/node/commit/c1634c7213)] - **doc**: correct typescript stdin support (Marco Ippolito) [#54036](https://github.com/nodejs/node/pull/54036)
* \[[`64812d5c22`](https://github.com/nodejs/node/commit/64812d5c22)] - **doc**: fix typo in recognizing-contributors (Marco Ippolito) [#53990](https://github.com/nodejs/node/pull/53990)
* \[[`6b35994b6f`](https://github.com/nodejs/node/commit/6b35994b6f)] - **doc**: fix documentation for `--run` (Aviv Keller) [#53976](https://github.com/nodejs/node/pull/53976)
* \[[`04d203a233`](https://github.com/nodejs/node/commit/04d203a233)] - **doc**: update boxstarter README (Aviv Keller) [#53785](https://github.com/nodejs/node/pull/53785)
* \[[`86fa46db1c`](https://github.com/nodejs/node/commit/86fa46db1c)] - **doc**: add info about prefix-only modules to `module.builtinModules` (Grigory) [#53954](https://github.com/nodejs/node/pull/53954)
* \[[`defdc3c568`](https://github.com/nodejs/node/commit/defdc3c568)] - **doc**: remove `scroll-behavior: smooth;` (Cloyd Lau) [#53942](https://github.com/nodejs/node/pull/53942)
* \[[`e907236dd9`](https://github.com/nodejs/node/commit/e907236dd9)] - **doc**: move --test-coverage-{ex,in}clude to proper location (Colin Ihrig) [#53926](https://github.com/nodejs/node/pull/53926)
* \[[`8bf9960b98`](https://github.com/nodejs/node/commit/8bf9960b98)] - **doc**: add `--experimental-sqlite` note (Aviv Keller) [#53907](https://github.com/nodejs/node/pull/53907)
* \[[`d7615004d8`](https://github.com/nodejs/node/commit/d7615004d8)] - **doc**: update `api_assets` README for new files (Aviv Keller) [#53676](https://github.com/nodejs/node/pull/53676)
* \[[`63cf715aa0`](https://github.com/nodejs/node/commit/63cf715aa0)] - **doc**: add MattiasBuelens to collaborators (Mattias Buelens) [#53895](https://github.com/nodejs/node/pull/53895)
* \[[`5b8dd78112`](https://github.com/nodejs/node/commit/5b8dd78112)] - **doc**: fix release date for 22.5.0 (Antoine du Hamel) [#53889](https://github.com/nodejs/node/pull/53889)
* \[[`dd2c0f349a`](https://github.com/nodejs/node/commit/dd2c0f349a)] - **doc**: fix casing of GitHub handle for two collaborators (Antoine du Hamel) [#53857](https://github.com/nodejs/node/pull/53857)
* \[[`b47c2308e1`](https://github.com/nodejs/node/commit/b47c2308e1)] - **doc**: update release-post nodejs.org script (Rafael Gonzaga) [#53762](https://github.com/nodejs/node/pull/53762)
* \[[`88539527d5`](https://github.com/nodejs/node/commit/88539527d5)] - **doc, test**: tracing channel hasSubscribers getter (Thomas Hunter II) [#52908](https://github.com/nodejs/node/pull/52908)
* \[[`44a08f75b0`](https://github.com/nodejs/node/commit/44a08f75b0)] - **doc,tools**: enforce use of `node:` prefix (Antoine du Hamel) [#53950](https://github.com/nodejs/node/pull/53950)
* \[[`87bab76df2`](https://github.com/nodejs/node/commit/87bab76df2)] - **doc,tty**: add documentation for ReadStream and WriteStream (jakecastelli) [#53567](https://github.com/nodejs/node/pull/53567)
* \[[`dcca9ba560`](https://github.com/nodejs/node/commit/dcca9ba560)] - **esm**: refactor `get_format` (Antoine du Hamel) [#53872](https://github.com/nodejs/node/pull/53872)
* \[[`5e03c17aae`](https://github.com/nodejs/node/commit/5e03c17aae)] - **fs**: optimize `fs.cpSync` js calls (Yagiz Nizipli) [#53614](https://github.com/nodejs/node/pull/53614)
* \[[`e0054ee0a7`](https://github.com/nodejs/node/commit/e0054ee0a7)] - **fs**: ensure consistency for mkdtemp in both fs and fs/promises (YieldRay) [#53776](https://github.com/nodejs/node/pull/53776)
* \[[`8086337ea9`](https://github.com/nodejs/node/commit/8086337ea9)] - **fs**: remove unnecessary option argument validation (Jonas) [#53861](https://github.com/nodejs/node/pull/53861)
* \[[`b377b93a3f`](https://github.com/nodejs/node/commit/b377b93a3f)] - **fs**: correctly pass dirent to exclude `withFileTypes` (RedYetiDev) [#53823](https://github.com/nodejs/node/pull/53823)
* \[[`68e444d2d8`](https://github.com/nodejs/node/commit/68e444d2d8)] - **(SEMVER-MINOR)** **http**: add diagnostics channel `http.client.request.error` (Kohei Ueno) [#54054](https://github.com/nodejs/node/pull/54054)
* \[[`de1fbc292f`](https://github.com/nodejs/node/commit/de1fbc292f)] - **(SEMVER-MINOR)** **inspector**: add initial support for network inspection (Kohei Ueno) [#53593](https://github.com/nodejs/node/pull/53593)
* \[[`744df0be24`](https://github.com/nodejs/node/commit/744df0be24)] - **lib**: support dynamic trace events on debugWithTimer (Vinicius Lourenço) [#53913](https://github.com/nodejs/node/pull/53913)
* \[[`546dab29c1`](https://github.com/nodejs/node/commit/546dab29c1)] - **lib**: optimize copyError with ObjectAssign in primordials (HEESEUNG) [#53999](https://github.com/nodejs/node/pull/53999)
* \[[`494df9835a`](https://github.com/nodejs/node/commit/494df9835a)] - **lib**: improve cluster/primary code (Ehsan Khakifirooz) [#53756](https://github.com/nodejs/node/pull/53756)
* \[[`03f353293b`](https://github.com/nodejs/node/commit/03f353293b)] - **lib**: improve error message when index not found on cjs (Vinicius Lourenço) [#53859](https://github.com/nodejs/node/pull/53859)
* \[[`d8375d6236`](https://github.com/nodejs/node/commit/d8375d6236)] - **lib**: decorate async stack trace in source maps (Chengzhong Wu) [#53860](https://github.com/nodejs/node/pull/53860)
* \[[`15a94e67b1`](https://github.com/nodejs/node/commit/15a94e67b1)] - **lib,src**: drop --experimental-network-imports (Rafael Gonzaga) [#53822](https://github.com/nodejs/node/pull/53822)
* \[[`a6eedc401d`](https://github.com/nodejs/node/commit/a6eedc401d)] - **meta**: add `sqlite` to js subsystems (Alex Yang) [#53911](https://github.com/nodejs/node/pull/53911)
* \[[`21098856de`](https://github.com/nodejs/node/commit/21098856de)] - **meta**: move tsc member to emeritus (Michael Dawson) [#54029](https://github.com/nodejs/node/pull/54029)
* \[[`048d421ad1`](https://github.com/nodejs/node/commit/048d421ad1)] - **meta**: add jake to collaborators (jakecastelli) [#54004](https://github.com/nodejs/node/pull/54004)
* \[[`20a8c96c41`](https://github.com/nodejs/node/commit/20a8c96c41)] - **meta**: remove license for hljs (Aviv Keller) [#53970](https://github.com/nodejs/node/pull/53970)
* \[[`2fd4ac4859`](https://github.com/nodejs/node/commit/2fd4ac4859)] - **meta**: make more bug-report information required (Aviv Keller) [#53718](https://github.com/nodejs/node/pull/53718)
* \[[`b312ec0b0c`](https://github.com/nodejs/node/commit/b312ec0b0c)] - **meta**: reword linter messages (Aviv Keller) [#53949](https://github.com/nodejs/node/pull/53949)
* \[[`d2526126a9`](https://github.com/nodejs/node/commit/d2526126a9)] - **meta**: store actions secrets in environment (Aviv Keller) [#53930](https://github.com/nodejs/node/pull/53930)
* \[[`1688f00dce`](https://github.com/nodejs/node/commit/1688f00dce)] - **meta**: move anonrig to tsc voting members (Yagiz Nizipli) [#53888](https://github.com/nodejs/node/pull/53888)
* \[[`c20e8418de`](https://github.com/nodejs/node/commit/c20e8418de)] - **module**: fix strip-types interaction with detect-module (Marco Ippolito) [#54164](https://github.com/nodejs/node/pull/54164)
* \[[`ab1f0b415f`](https://github.com/nodejs/node/commit/ab1f0b415f)] - **module**: fix extensionless typescript in cjs loader (Marco Ippolito) [#54062](https://github.com/nodejs/node/pull/54062)
* \[[`92439fc160`](https://github.com/nodejs/node/commit/92439fc160)] - **(SEMVER-MINOR)** **module**: add --experimental-strip-types (Marco Ippolito) [#53725](https://github.com/nodejs/node/pull/53725)
* \[[`f755d31bec`](https://github.com/nodejs/node/commit/f755d31bec)] - **node-api**: add property keys benchmark (Chengzhong Wu) [#54012](https://github.com/nodejs/node/pull/54012)
* \[[`7382eefae5`](https://github.com/nodejs/node/commit/7382eefae5)] - **node-api**: rename nogc to basic (Gabriel Schulhof) [#53830](https://github.com/nodejs/node/pull/53830)
* \[[`2c4470625b`](https://github.com/nodejs/node/commit/2c4470625b)] - **process**: unify experimental warning messages (Aviv Keller) [#53704](https://github.com/nodejs/node/pull/53704)
* \[[`98a7ad2e0d`](https://github.com/nodejs/node/commit/98a7ad2e0d)] - **src**: expose LookupAndCompile with parameters (Shelley Vohr) [#53886](https://github.com/nodejs/node/pull/53886)
* \[[`dd3c66be0a`](https://github.com/nodejs/node/commit/dd3c66be0a)] - **src**: simplify AESCipherTraits::AdditionalConfig (Tobias Nießen) [#53890](https://github.com/nodejs/node/pull/53890)
* \[[`ee82f224ff`](https://github.com/nodejs/node/commit/ee82f224ff)] - **src**: remove redundant RsaPointer (use RSAPointer) (James M Snell) [#54003](https://github.com/nodejs/node/pull/54003)
* \[[`2d77bd2929`](https://github.com/nodejs/node/commit/2d77bd2929)] - **src**: fix -Wshadow warning (Shelley Vohr) [#53885](https://github.com/nodejs/node/pull/53885)
* \[[`bd4a9ffe8c`](https://github.com/nodejs/node/commit/bd4a9ffe8c)] - **src**: start using ncrypto for CSPRNG calls (James M Snell) [#53984](https://github.com/nodejs/node/pull/53984)
* \[[`3fdcf7a47d`](https://github.com/nodejs/node/commit/3fdcf7a47d)] - **src**: return `undefined` if no rows are returned in SQLite (Deokjin Kim) [#53981](https://github.com/nodejs/node/pull/53981)
* \[[`ca6854443d`](https://github.com/nodejs/node/commit/ca6854443d)] - **src**: fix slice of slice of file-backed Blob (Josh Lee) [#53972](https://github.com/nodejs/node/pull/53972)
* \[[`c457f9ed5a`](https://github.com/nodejs/node/commit/c457f9ed5a)] - **src**: cache invariant code motion (Rafael Gonzaga) [#53879](https://github.com/nodejs/node/pull/53879)
* \[[`fd0da6c2cf`](https://github.com/nodejs/node/commit/fd0da6c2cf)] - **src**: avoid strcmp in ImportJWKAsymmetricKey (Tobias Nießen) [#53813](https://github.com/nodejs/node/pull/53813)
* \[[`fbf74bcf99`](https://github.com/nodejs/node/commit/fbf74bcf99)] - **src**: switch from ToLocalChecked to ToLocal in node\_webstorage (James M Snell) [#53959](https://github.com/nodejs/node/pull/53959)
* \[[`04bb6778e5`](https://github.com/nodejs/node/commit/04bb6778e5)] - **src**: move `ToNamespacedPath` call of webstorage (Yagiz Nizipli) [#53875](https://github.com/nodejs/node/pull/53875)
* \[[`9ffaf763e9`](https://github.com/nodejs/node/commit/9ffaf763e9)] - **src**: use Maybe\<void> in SecureContext (Tobias Nießen) [#53883](https://github.com/nodejs/node/pull/53883)
* \[[`a94c3ae06f`](https://github.com/nodejs/node/commit/a94c3ae06f)] - **src**: replace ToLocalChecked uses with ToLocal in node-file (James M Snell) [#53869](https://github.com/nodejs/node/pull/53869)
* \[[`55461be05f`](https://github.com/nodejs/node/commit/55461be05f)] - **src**: refactor webstorage implementation (Yagiz Nizipli) [#53876](https://github.com/nodejs/node/pull/53876)
* \[[`c53cf449a6`](https://github.com/nodejs/node/commit/c53cf449a6)] - **src**: fix env-file flag to ignore spaces before quotes (Mohit Malhotra) [#53786](https://github.com/nodejs/node/pull/53786)
* \[[`bac3a485f6`](https://github.com/nodejs/node/commit/bac3a485f6)] - **src**: fix potential segmentation fault in SQLite (Tobias Nießen) [#53850](https://github.com/nodejs/node/pull/53850)
* \[[`df5083e5f9`](https://github.com/nodejs/node/commit/df5083e5f9)] - **src,lib**: expose getCategoryEnabledBuffer to use on node.http (Vinicius Lourenço) [#53602](https://github.com/nodejs/node/pull/53602)
* \[[`8664b9ad60`](https://github.com/nodejs/node/commit/8664b9ad60)] - **src,test**: disallow unsafe integer coercion in SQLite (Tobias Nießen) [#53851](https://github.com/nodejs/node/pull/53851)
* \[[`15816bd0dd`](https://github.com/nodejs/node/commit/15816bd0dd)] - **(SEMVER-MINOR)** **stream**: expose DuplexPair API (Austin Wright) [#34111](https://github.com/nodejs/node/pull/34111)
* \[[`718f6bc78c`](https://github.com/nodejs/node/commit/718f6bc78c)] - **test**: do not swallow uncaughtException errors in exit code tests (Meghan Denny) [#54039](https://github.com/nodejs/node/pull/54039)
* \[[`c6656c9251`](https://github.com/nodejs/node/commit/c6656c9251)] - **test**: move shared module to `test/common` (Rich Trott) [#54042](https://github.com/nodejs/node/pull/54042)
* \[[`e471e32d46`](https://github.com/nodejs/node/commit/e471e32d46)] - **test**: skip sea tests with more accurate available disk space estimation (Chengzhong Wu) [#53996](https://github.com/nodejs/node/pull/53996)
* \[[`61971ec929`](https://github.com/nodejs/node/commit/61971ec929)] - **test**: remove unnecessary console log (KAYYY) [#53812](https://github.com/nodejs/node/pull/53812)
* \[[`1344bd2d6f`](https://github.com/nodejs/node/commit/1344bd2d6f)] - **test**: add comments and rename test for timer robustness (Rich Trott) [#54008](https://github.com/nodejs/node/pull/54008)
* \[[`da3573409c`](https://github.com/nodejs/node/commit/da3573409c)] - **test**: add test for one arg timers to increase coverage (Carlos Espa) [#54007](https://github.com/nodejs/node/pull/54007)
* \[[`fc67abd97e`](https://github.com/nodejs/node/commit/fc67abd97e)] - **test**: mark 'test/parallel/test-sqlite.js' as flaky (Colin Ihrig) [#54031](https://github.com/nodejs/node/pull/54031)
* \[[`aa0ac3b57c`](https://github.com/nodejs/node/commit/aa0ac3b57c)] - **test**: mark test-pipe-file-to-http as flaky (jakecastelli) [#53751](https://github.com/nodejs/node/pull/53751)
* \[[`52bc8ec360`](https://github.com/nodejs/node/commit/52bc8ec360)] - **test**: compare paths on Windows without considering case (Early Riser) [#53993](https://github.com/nodejs/node/pull/53993)
* \[[`7e8a609579`](https://github.com/nodejs/node/commit/7e8a609579)] - **test**: skip sea tests in large debug builds (Chengzhong Wu) [#53918](https://github.com/nodejs/node/pull/53918)
* \[[`30a94ca0c4`](https://github.com/nodejs/node/commit/30a94ca0c4)] - **test**: skip --title check on IBM i (Abdirahim Musse) [#53952](https://github.com/nodejs/node/pull/53952)
* \[[`5cea7ed706`](https://github.com/nodejs/node/commit/5cea7ed706)] - **test**: reduce flakiness of `test-assert-esm-cjs-message-verify` (Antoine du Hamel) [#53967](https://github.com/nodejs/node/pull/53967)
* \[[`58cb0dd8a6`](https://github.com/nodejs/node/commit/58cb0dd8a6)] - **test**: use `PYTHON` executable from env in `assertSnapshot` (Antoine du Hamel) [#53938](https://github.com/nodejs/node/pull/53938)
* \[[`c247582591`](https://github.com/nodejs/node/commit/c247582591)] - **test**: deflake test-blob-file-backed (Luigi Pinca) [#53920](https://github.com/nodejs/node/pull/53920)
* \[[`3999021653`](https://github.com/nodejs/node/commit/3999021653)] - **test\_runner**: switched to internal readline interface (Emil Tayeb) [#54000](https://github.com/nodejs/node/pull/54000)
* \[[`3fb97a90ee`](https://github.com/nodejs/node/commit/3fb97a90ee)] - **test\_runner**: remove redundant bootstrap boolean (Colin Ihrig) [#54013](https://github.com/nodejs/node/pull/54013)
* \[[`edd80e2bdc`](https://github.com/nodejs/node/commit/edd80e2bdc)] - **test\_runner**: do not throw on mocked clearTimeout() (Aksinya Bykova) [#54005](https://github.com/nodejs/node/pull/54005)
* \[[`893c864542`](https://github.com/nodejs/node/commit/893c864542)] - **(SEMVER-MINOR)** **test\_runner**: fix support watch with run(), add globPatterns option (Matteo Collina) [#53866](https://github.com/nodejs/node/pull/53866)
* \[[`4887213f2e`](https://github.com/nodejs/node/commit/4887213f2e)] - **test\_runner**: added colors to dot reporter (Giovanni) [#53450](https://github.com/nodejs/node/pull/53450)
* \[[`c4848c53e6`](https://github.com/nodejs/node/commit/c4848c53e6)] - **test\_runner**: cleanup global event listeners after run (Eddie Abbondanzio) [#53878](https://github.com/nodejs/node/pull/53878)
* \[[`876e7b3226`](https://github.com/nodejs/node/commit/876e7b3226)] - **test\_runner**: refactor coverage to pass in config options (Colin Ihrig) [#53931](https://github.com/nodejs/node/pull/53931)
* \[[`f45edb4b5e`](https://github.com/nodejs/node/commit/f45edb4b5e)] - **test\_runner**: refactor and simplify internals (Colin Ihrig) [#53921](https://github.com/nodejs/node/pull/53921)
* \[[`6ad6e01bf3`](https://github.com/nodejs/node/commit/6ad6e01bf3)] - **(SEMVER-MINOR)** **test\_runner**: refactor snapshots to get file from context (Colin Ihrig) [#53853](https://github.com/nodejs/node/pull/53853)
* \[[`698e44f8e7`](https://github.com/nodejs/node/commit/698e44f8e7)] - **(SEMVER-MINOR)** **test\_runner**: add context.filePath (Colin Ihrig) [#53853](https://github.com/nodejs/node/pull/53853)
* \[[`97da7ca11b`](https://github.com/nodejs/node/commit/97da7ca11b)] - **test\_runner**: consolidate option parsing (Colin Ihrig) [#53849](https://github.com/nodejs/node/pull/53849)
* \[[`43afcbf9dd`](https://github.com/nodejs/node/commit/43afcbf9dd)] - **tools**: fix `SLACK_TITLE` in invalid commit workflow (Antoine du Hamel) [#53912](https://github.com/nodejs/node/pull/53912)
* \[[`eed0963391`](https://github.com/nodejs/node/commit/eed0963391)] - **typings**: apply lint (1ilsang) [#54065](https://github.com/nodejs/node/pull/54065)
* \[[`e8ea49b256`](https://github.com/nodejs/node/commit/e8ea49b256)] - **typings**: fix typo on quic onSessionDatagram (1ilsang) [#54064](https://github.com/nodejs/node/pull/54064)

<a id="22.5.1"></a>

## 2024-07-19, Version 22.5.1 (Current), @richardlau

### Notable Changes

This release fixes a regression introduced in Node.js 22.5.0. The problem is known to display the following symptoms:

* Crash with `FATAL ERROR: v8::Object::GetCreationContextChecked No creation context available` [#53902](https://github.com/nodejs/node/issues/53902)
* npm errors with `npm error Exit handler never called!` [npm/cli#7657](https://github.com/npm/cli/issues/7657)
* yarn hangs or outputs `Usage Error: Couldn't find the node_modules state file - running an install might help (findPackageLocation)` [yarnpkg/berry#6398](https://github.com/yarnpkg/berry/issues/6398)

### Commits

* \[[`e2deeedc6e`](https://github.com/nodejs/node/commit/e2deeedc6e)] - _**Revert**_ "**fs**: add v8 fast api to closeSync" (Aviv Keller) [#53904](https://github.com/nodejs/node/pull/53904)

<a id="22.5.0"></a>

## 2024-07-17, Version 22.5.0 (Current), @RafaelGSS prepared by @aduh95

### Notable Changes

* \[[`1367c5558e`](https://github.com/nodejs/node/commit/1367c5558e)] - **(SEMVER-MINOR)** **http**: expose websockets (Natalia Venditto) [#53721](https://github.com/nodejs/node/pull/53721)
* \[[`b31394920d`](https://github.com/nodejs/node/commit/b31394920d)] - **(SEMVER-MINOR)** **lib**: add `node:sqlite` module (Colin Ihrig) [#53752](https://github.com/nodejs/node/pull/53752)
* \[[`aa7df9551d`](https://github.com/nodejs/node/commit/aa7df9551d)] - **module**: add `__esModule` to `require()`'d ESM (Joyee Cheung) [#52166](https://github.com/nodejs/node/pull/52166)
* \[[`8743c4d65a`](https://github.com/nodejs/node/commit/8743c4d65a)] - **(SEMVER-MINOR)** **path**: add `matchesGlob` method (Aviv Keller) [#52881](https://github.com/nodejs/node/pull/52881)
* \[[`77936c3d24`](https://github.com/nodejs/node/commit/77936c3d24)] - **(SEMVER-MINOR)** **process**: port on-exit-leak-free to core (Vinicius Lourenço) [#53239](https://github.com/nodejs/node/pull/53239)
* \[[`82d88a83f8`](https://github.com/nodejs/node/commit/82d88a83f8)] - **(SEMVER-MINOR)** **stream**: pipeline wait for close before calling the callback (jakecastelli) [#53462](https://github.com/nodejs/node/pull/53462)
* \[[`3a0fcbb17a`](https://github.com/nodejs/node/commit/3a0fcbb17a)] - **test\_runner**: support glob matching coverage files (Aviv Keller) [#53553](https://github.com/nodejs/node/pull/53553)
* \[[`22ca334090`](https://github.com/nodejs/node/commit/22ca334090)] - **(SEMVER-MINOR)** **worker**: add `postMessageToThread` (Paolo Insogna) [#53682](https://github.com/nodejs/node/pull/53682)

### Commits

* \[[`eb4e370933`](https://github.com/nodejs/node/commit/eb4e370933)] - **benchmark**: add require-esm benchmark (Joyee Cheung) [#52166](https://github.com/nodejs/node/pull/52166)
* \[[`4d4a8338db`](https://github.com/nodejs/node/commit/4d4a8338db)] - **benchmark**: add cpSync benchmark (Yagiz Nizipli) [#53612](https://github.com/nodejs/node/pull/53612)
* \[[`3d60b38afa`](https://github.com/nodejs/node/commit/3d60b38afa)] - **build**: fix build warning of c-ares under GN build (Cheng) [#53750](https://github.com/nodejs/node/pull/53750)
* \[[`a45c801048`](https://github.com/nodejs/node/commit/a45c801048)] - **build**: fix build error in sqlite under GN build (Cheng) [#53686](https://github.com/nodejs/node/pull/53686)
* \[[`40032eb623`](https://github.com/nodejs/node/commit/40032eb623)] - **build**: add gn files for deps/nbytes (Cheng) [#53685](https://github.com/nodejs/node/pull/53685)
* \[[`082799debb`](https://github.com/nodejs/node/commit/082799debb)] - **build**: fix mac build error of c-ares under GN (Cheng) [#53687](https://github.com/nodejs/node/pull/53687)
* \[[`b05394ea6a`](https://github.com/nodejs/node/commit/b05394ea6a)] - **build**: add version-specific library path for AIX (Richard Lau) [#53585](https://github.com/nodejs/node/pull/53585)
* \[[`6237172eaf`](https://github.com/nodejs/node/commit/6237172eaf)] - **cli**: update `node.1` to reflect Atom's sunset (Aviv Keller) [#53734](https://github.com/nodejs/node/pull/53734)
* \[[`5697938cb7`](https://github.com/nodejs/node/commit/5697938cb7)] - **crypto**: avoid std::function (Tobias Nießen) [#53683](https://github.com/nodejs/node/pull/53683)
* \[[`3cc01aa314`](https://github.com/nodejs/node/commit/3cc01aa314)] - **crypto**: make deriveBits length parameter optional and nullable (Filip Skokan) [#53601](https://github.com/nodejs/node/pull/53601)
* \[[`f82e20fdea`](https://github.com/nodejs/node/commit/f82e20fdea)] - **crypto**: avoid taking ownership of OpenSSL objects (Tobias Nießen) [#53460](https://github.com/nodejs/node/pull/53460)
* \[[`ad1e5610ec`](https://github.com/nodejs/node/commit/ad1e5610ec)] - **deps**: update googletest to 4b21f1a (Node.js GitHub Bot) [#53842](https://github.com/nodejs/node/pull/53842)
* \[[`d285d610a0`](https://github.com/nodejs/node/commit/d285d610a0)] - **deps**: update minimatch to 10.0.1 (Node.js GitHub Bot) [#53841](https://github.com/nodejs/node/pull/53841)
* \[[`70f5209c9f`](https://github.com/nodejs/node/commit/70f5209c9f)] - **deps**: update corepack to 0.29.2 (Node.js GitHub Bot) [#53838](https://github.com/nodejs/node/pull/53838)
* \[[`4930e12a45`](https://github.com/nodejs/node/commit/4930e12a45)] - **deps**: update simdutf to 5.3.0 (Node.js GitHub Bot) [#53837](https://github.com/nodejs/node/pull/53837)
* \[[`d346833364`](https://github.com/nodejs/node/commit/d346833364)] - **deps**: update ada to 2.9.0 (Node.js GitHub Bot) [#53748](https://github.com/nodejs/node/pull/53748)
* \[[`ab8abb5367`](https://github.com/nodejs/node/commit/ab8abb5367)] - **deps**: upgrade npm to 10.8.2 (npm team) [#53799](https://github.com/nodejs/node/pull/53799)
* \[[`1ad664905a`](https://github.com/nodejs/node/commit/1ad664905a)] - **deps**: update nbytes and add update script (Yagiz Nizipli) [#53790](https://github.com/nodejs/node/pull/53790)
* \[[`a66f11e798`](https://github.com/nodejs/node/commit/a66f11e798)] - **deps**: update googletest to 34ad51b (Node.js GitHub Bot) [#53157](https://github.com/nodejs/node/pull/53157)
* \[[`9bf61d6a0d`](https://github.com/nodejs/node/commit/9bf61d6a0d)] - **deps**: update googletest to 305e5a2 (Node.js GitHub Bot) [#53157](https://github.com/nodejs/node/pull/53157)
* \[[`8542ace488`](https://github.com/nodejs/node/commit/8542ace488)] - **deps**: V8: cherry-pick 9ebca66a5740 (Chengzhong Wu) [#53755](https://github.com/nodejs/node/pull/53755)
* \[[`29a734c21d`](https://github.com/nodejs/node/commit/29a734c21d)] - **deps**: V8: cherry-pick e061cf9970d9 (Joyee Cheung) [#53755](https://github.com/nodejs/node/pull/53755)
* \[[`c7624af44a`](https://github.com/nodejs/node/commit/c7624af44a)] - **deps**: update c-ares to v1.32.1 (Node.js GitHub Bot) [#53753](https://github.com/nodejs/node/pull/53753)
* \[[`bbcec9e129`](https://github.com/nodejs/node/commit/bbcec9e129)] - **deps**: update minimatch to 9.0.5 (Node.js GitHub Bot) [#53646](https://github.com/nodejs/node/pull/53646)
* \[[`76032fd980`](https://github.com/nodejs/node/commit/76032fd980)] - **deps**: update c-ares to v1.32.0 (Node.js GitHub Bot) [#53722](https://github.com/nodejs/node/pull/53722)
* \[[`26386046ad`](https://github.com/nodejs/node/commit/26386046ad)] - **doc**: move MylesBorins to emeritus (Myles Borins) [#53760](https://github.com/nodejs/node/pull/53760)
* \[[`362875bda0`](https://github.com/nodejs/node/commit/362875bda0)] - **doc**: add Rafael to the last security release (Rafael Gonzaga) [#53769](https://github.com/nodejs/node/pull/53769)
* \[[`a1a5ad848d`](https://github.com/nodejs/node/commit/a1a5ad848d)] - **doc**: use mock.callCount() in examples (Sébastien Règne) [#53754](https://github.com/nodejs/node/pull/53754)
* \[[`bb960c5471`](https://github.com/nodejs/node/commit/bb960c5471)] - **doc**: clarify authenticity of plaintexts in update (Tobias Nießen) [#53784](https://github.com/nodejs/node/pull/53784)
* \[[`5dd3018eb4`](https://github.com/nodejs/node/commit/5dd3018eb4)] - **doc**: add option to have support me link (Michael Dawson) [#53312](https://github.com/nodejs/node/pull/53312)
* \[[`0f95ad3d7d`](https://github.com/nodejs/node/commit/0f95ad3d7d)] - **doc**: add OpenSSL security level to TLS docs (Afanasii Kurakin) [#53647](https://github.com/nodejs/node/pull/53647)
* \[[`2d92ec2831`](https://github.com/nodejs/node/commit/2d92ec2831)] - **doc**: update `scroll-padding-top` to 4rem (Cloyd Lau) [#53662](https://github.com/nodejs/node/pull/53662)
* \[[`933359a786`](https://github.com/nodejs/node/commit/933359a786)] - **doc**: mention v8.setFlagsFromString to pm (Rafael Gonzaga) [#53731](https://github.com/nodejs/node/pull/53731)
* \[[`e17c2618e3`](https://github.com/nodejs/node/commit/e17c2618e3)] - **doc**: remove the last \<pre> tag (Claudio W) [#53741](https://github.com/nodejs/node/pull/53741)
* \[[`7f18a5f47a`](https://github.com/nodejs/node/commit/7f18a5f47a)] - **doc**: exclude voting and regular TSC from spotlight (Michael Dawson) [#53694](https://github.com/nodejs/node/pull/53694)
* \[[`df3dcd1bd1`](https://github.com/nodejs/node/commit/df3dcd1bd1)] - **doc**: fix releases guide for recent Git versions (Michaël Zasso) [#53709](https://github.com/nodejs/node/pull/53709)
* \[[`50987ea833`](https://github.com/nodejs/node/commit/50987ea833)] - **doc**: require `node:process` in assert doc examples (Alfredo González) [#53702](https://github.com/nodejs/node/pull/53702)
* \[[`fa58d01497`](https://github.com/nodejs/node/commit/fa58d01497)] - **doc**: add additional explanation to the wildcard section in permissions (jakecastelli) [#53664](https://github.com/nodejs/node/pull/53664)
* \[[`28bf1e48ef`](https://github.com/nodejs/node/commit/28bf1e48ef)] - **doc**: mark NODE\_MODULE\_VERSION for Node.js 22.0.0 (Michaël Zasso) [#53650](https://github.com/nodejs/node/pull/53650)
* \[[`1cc0b41f00`](https://github.com/nodejs/node/commit/1cc0b41f00)] - **doc**: include node.module\_timer on available categories (Vinicius Lourenço) [#53638](https://github.com/nodejs/node/pull/53638)
* \[[`d224e9eab5`](https://github.com/nodejs/node/commit/d224e9eab5)] - **doc**: fix module customization hook examples (Elliot Goodrich) [#53637](https://github.com/nodejs/node/pull/53637)
* \[[`2cf60964e6`](https://github.com/nodejs/node/commit/2cf60964e6)] - **doc**: fix doc for correct usage with plan & TestContext (Emil Tayeb) [#53615](https://github.com/nodejs/node/pull/53615)
* \[[`6df86ae056`](https://github.com/nodejs/node/commit/6df86ae056)] - **doc**: remove some news issues that are no longer (Michael Dawson) [#53608](https://github.com/nodejs/node/pull/53608)
* \[[`42b9408f3e`](https://github.com/nodejs/node/commit/42b9408f3e)] - **doc**: add issue for news from ambassadors (Michael Dawson) [#53607](https://github.com/nodejs/node/pull/53607)
* \[[`2d1ff91953`](https://github.com/nodejs/node/commit/2d1ff91953)] - **doc**: add esm example for os (Leonardo Peixoto) [#53604](https://github.com/nodejs/node/pull/53604)
* \[[`de99d69d75`](https://github.com/nodejs/node/commit/de99d69d75)] - **doc**: clarify usage of coverage reporters (Eliphaz Bouye) [#53523](https://github.com/nodejs/node/pull/53523)
* \[[`519c328dcf`](https://github.com/nodejs/node/commit/519c328dcf)] - **doc**: document addition testing options (Aviv Keller) [#53569](https://github.com/nodejs/node/pull/53569)
* \[[`c6166cdfe4`](https://github.com/nodejs/node/commit/c6166cdfe4)] - **doc**: clarify that fs.exists() may return false for existing symlink (Tobias Nießen) [#53566](https://github.com/nodejs/node/pull/53566)
* \[[`9139ab2848`](https://github.com/nodejs/node/commit/9139ab2848)] - **doc**: note http.closeAllConnections excludes upgraded sockets (Rob Hogan) [#53560](https://github.com/nodejs/node/pull/53560)
* \[[`19b3718ee1`](https://github.com/nodejs/node/commit/19b3718ee1)] - **doc, meta**: add PTAL to glossary (Aviv Keller) [#53770](https://github.com/nodejs/node/pull/53770)
* \[[`80c1f5ce8a`](https://github.com/nodejs/node/commit/80c1f5ce8a)] - **doc, typings**: events.once accepts symbol event type (René) [#53542](https://github.com/nodejs/node/pull/53542)
* \[[`1a21e0f61e`](https://github.com/nodejs/node/commit/1a21e0f61e)] - **esm**: improve `defaultResolve` performance (Yagiz Nizipli) [#53711](https://github.com/nodejs/node/pull/53711)
* \[[`262f2cb3b6`](https://github.com/nodejs/node/commit/262f2cb3b6)] - **esm**: remove unnecessary toNamespacedPath calls (Yagiz Nizipli) [#53656](https://github.com/nodejs/node/pull/53656)
* \[[`e29c9453a9`](https://github.com/nodejs/node/commit/e29c9453a9)] - **esm**: move hooks test with others (Geoffrey Booth) [#53558](https://github.com/nodejs/node/pull/53558)
* \[[`8368555289`](https://github.com/nodejs/node/commit/8368555289)] - **fs**: add v8 fast api to closeSync (Yagiz Nizipli) [#53627](https://github.com/nodejs/node/pull/53627)
* \[[`628a539810`](https://github.com/nodejs/node/commit/628a539810)] - **fs**: reduce throwing unnecessary errors on glob (Yagiz Nizipli) [#53632](https://github.com/nodejs/node/pull/53632)
* \[[`076e82ca40`](https://github.com/nodejs/node/commit/076e82ca40)] - **fs**: move `ToNamespacedPath` dir calls to c++ (Yagiz Nizipli) [#53630](https://github.com/nodejs/node/pull/53630)
* \[[`128e514d81`](https://github.com/nodejs/node/commit/128e514d81)] - **fs**: improve error performance of `fs.dir` (Yagiz Nizipli) [#53667](https://github.com/nodejs/node/pull/53667)
* \[[`603c2c5c08`](https://github.com/nodejs/node/commit/603c2c5c08)] - **fs**: fix typings (Yagiz Nizipli) [#53626](https://github.com/nodejs/node/pull/53626)
* \[[`1367c5558e`](https://github.com/nodejs/node/commit/1367c5558e)] - **(SEMVER-MINOR)** **http**: expose websockets (Natalia Venditto) [#53721](https://github.com/nodejs/node/pull/53721)
* \[[`7debb6c36e`](https://github.com/nodejs/node/commit/7debb6c36e)] - **http**: remove prototype primordials (Antoine du Hamel) [#53698](https://github.com/nodejs/node/pull/53698)
* \[[`b13aea5698`](https://github.com/nodejs/node/commit/b13aea5698)] - **http, readline**: replace sort with toSorted (Benjamin Gruenbaum) [#53623](https://github.com/nodejs/node/pull/53623)
* \[[`1397f5d9f4`](https://github.com/nodejs/node/commit/1397f5d9f4)] - **http2**: remove prototype primordials (Antoine du Hamel) [#53696](https://github.com/nodejs/node/pull/53696)
* \[[`f57d3cee2c`](https://github.com/nodejs/node/commit/f57d3cee2c)] - **lib**: make navigator not runtime-lookup process.version/arch/platform (Jordan Harband) [#53765](https://github.com/nodejs/node/pull/53765)
* \[[`0a01abbd45`](https://github.com/nodejs/node/commit/0a01abbd45)] - **lib**: refactor `platform` utility methods (Daniel Bayley) [#53817](https://github.com/nodejs/node/pull/53817)
* \[[`afe7f4f819`](https://github.com/nodejs/node/commit/afe7f4f819)] - **lib**: remove path.resolve from permissions.js (Rafael Gonzaga) [#53729](https://github.com/nodejs/node/pull/53729)
* \[[`cbe77b30ca`](https://github.com/nodejs/node/commit/cbe77b30ca)] - **lib**: move `ToNamespacedPath` call to c++ (Yagiz Nizipli) [#53654](https://github.com/nodejs/node/pull/53654)
* \[[`0f146aac2c`](https://github.com/nodejs/node/commit/0f146aac2c)] - **lib**: make navigator properties lazy (James M Snell) [#53649](https://github.com/nodejs/node/pull/53649)
* \[[`0540308bd7`](https://github.com/nodejs/node/commit/0540308bd7)] - **lib**: add toJSON to PerformanceMeasure (theanarkh) [#53603](https://github.com/nodejs/node/pull/53603)
* \[[`b31394920d`](https://github.com/nodejs/node/commit/b31394920d)] - **(SEMVER-MINOR)** **lib,src,test,doc**: add node:sqlite module (Colin Ihrig) [#53752](https://github.com/nodejs/node/pull/53752)
* \[[`1a7c2dc5ea`](https://github.com/nodejs/node/commit/1a7c2dc5ea)] - **meta**: remove redudant logging from dep updaters (Aviv Keller) [#53783](https://github.com/nodejs/node/pull/53783)
* \[[`ac5d7b709d`](https://github.com/nodejs/node/commit/ac5d7b709d)] - **meta**: change email address of anonrig (Yagiz Nizipli) [#53829](https://github.com/nodejs/node/pull/53829)
* \[[`085ec5533c`](https://github.com/nodejs/node/commit/085ec5533c)] - **meta**: add `node_sqlite.c` to PR label config (Aviv Keller) [#53797](https://github.com/nodejs/node/pull/53797)
* \[[`c68d873e99`](https://github.com/nodejs/node/commit/c68d873e99)] - **meta**: move one or more collaborators to emeritus (Node.js GitHub Bot) [#53758](https://github.com/nodejs/node/pull/53758)
* \[[`5ae8ea489d`](https://github.com/nodejs/node/commit/5ae8ea489d)] - **meta**: use HTML entities in commit-queue comment (Aviv Keller) [#53744](https://github.com/nodejs/node/pull/53744)
* \[[`ecd8fceb68`](https://github.com/nodejs/node/commit/ecd8fceb68)] - **meta**: move regular TSC member to emeritus (Michael Dawson) [#53693](https://github.com/nodejs/node/pull/53693)
* \[[`05058f9809`](https://github.com/nodejs/node/commit/05058f9809)] - **meta**: bump codecov/codecov-action from 4.4.1 to 4.5.0 (dependabot\[bot]) [#53675](https://github.com/nodejs/node/pull/53675)
* \[[`e272ffa3d6`](https://github.com/nodejs/node/commit/e272ffa3d6)] - **meta**: bump mozilla-actions/sccache-action from 0.0.4 to 0.0.5 (dependabot\[bot]) [#53674](https://github.com/nodejs/node/pull/53674)
* \[[`a39407560c`](https://github.com/nodejs/node/commit/a39407560c)] - **meta**: bump github/codeql-action from 3.25.7 to 3.25.11 (dependabot\[bot]) [#53673](https://github.com/nodejs/node/pull/53673)
* \[[`e4ce92ee31`](https://github.com/nodejs/node/commit/e4ce92ee31)] - **meta**: bump actions/checkout from 4.1.6 to 4.1.7 (dependabot\[bot]) [#53672](https://github.com/nodejs/node/pull/53672)
* \[[`4cf98febe7`](https://github.com/nodejs/node/commit/4cf98febe7)] - **meta**: bump peter-evans/create-pull-request from 6.0.5 to 6.1.0 (dependabot\[bot]) [#53671](https://github.com/nodejs/node/pull/53671)
* \[[`c28af95bf5`](https://github.com/nodejs/node/commit/c28af95bf5)] - **meta**: bump step-security/harden-runner from 2.8.0 to 2.8.1 (dependabot\[bot]) [#53670](https://github.com/nodejs/node/pull/53670)
* \[[`dd2157bc83`](https://github.com/nodejs/node/commit/dd2157bc83)] - **meta**: move member from TSC regular to emeriti (Michael Dawson) [#53599](https://github.com/nodejs/node/pull/53599)
* \[[`508abfe178`](https://github.com/nodejs/node/commit/508abfe178)] - **meta**: warnings bypass deprecation cycle (Benjamin Gruenbaum) [#53513](https://github.com/nodejs/node/pull/53513)
* \[[`3c5ec839e3`](https://github.com/nodejs/node/commit/3c5ec839e3)] - **meta**: prevent constant references to issues in versioning (Aviv Keller) [#53564](https://github.com/nodejs/node/pull/53564)
* \[[`aa7df9551d`](https://github.com/nodejs/node/commit/aa7df9551d)] - **module**: add \_\_esModule to require()'d ESM (Joyee Cheung) [#52166](https://github.com/nodejs/node/pull/52166)
* \[[`8743c4d65a`](https://github.com/nodejs/node/commit/8743c4d65a)] - **(SEMVER-MINOR)** **path**: add `matchesGlob` method (Aviv Keller) [#52881](https://github.com/nodejs/node/pull/52881)
* \[[`77936c3d24`](https://github.com/nodejs/node/commit/77936c3d24)] - **(SEMVER-MINOR)** **process**: port on-exit-leak-free to core (Vinicius Lourenço) [#53239](https://github.com/nodejs/node/pull/53239)
* \[[`5e4ca9fbb6`](https://github.com/nodejs/node/commit/5e4ca9fbb6)] - **src**: update outdated references to spec sections (Tobias Nießen) [#53832](https://github.com/nodejs/node/pull/53832)
* \[[`c22d9d5167`](https://github.com/nodejs/node/commit/c22d9d5167)] - **src**: use Maybe\<void> in ManagedEVPPKey (Tobias Nießen) [#53811](https://github.com/nodejs/node/pull/53811)
* \[[`d41ed44f49`](https://github.com/nodejs/node/commit/d41ed44f49)] - **src**: move `loadEnvFile` toNamespacedPath call (Yagiz Nizipli) [#53658](https://github.com/nodejs/node/pull/53658)
* \[[`dc99dd391f`](https://github.com/nodejs/node/commit/dc99dd391f)] - **src**: fix error handling in ExportJWKAsymmetricKey (Tobias Nießen) [#53767](https://github.com/nodejs/node/pull/53767)
* \[[`ab1e03e8cd`](https://github.com/nodejs/node/commit/ab1e03e8cd)] - **src**: use Maybe\<void> in node::crypto::error (Tobias Nießen) [#53766](https://github.com/nodejs/node/pull/53766)
* \[[`9bde9b254d`](https://github.com/nodejs/node/commit/9bde9b254d)] - **src**: fix implementation of `PropertySetterCallback` (Igor Sheludko) [#53576](https://github.com/nodejs/node/pull/53576)
* \[[`021e2cf40f`](https://github.com/nodejs/node/commit/021e2cf40f)] - **src**: remove unused ContextifyContext::WeakCallback (Chengzhong Wu) [#53517](https://github.com/nodejs/node/pull/53517)
* \[[`87121a17c4`](https://github.com/nodejs/node/commit/87121a17c4)] - **src**: fix typo in node.h (Daeyeon Jeong) [#53759](https://github.com/nodejs/node/pull/53759)
* \[[`94c7054c8d`](https://github.com/nodejs/node/commit/94c7054c8d)] - **src**: document the Node.js context embedder data (Joyee Cheung) [#53611](https://github.com/nodejs/node/pull/53611)
* \[[`c181940e83`](https://github.com/nodejs/node/commit/c181940e83)] - **src**: zero-initialize data that are copied into the snapshot (Joyee Cheung) [#53563](https://github.com/nodejs/node/pull/53563)
* \[[`8cda2db64c`](https://github.com/nodejs/node/commit/8cda2db64c)] - _**Revert**_ "**src**: make sure that memcpy-ed structs in snapshot have no padding" (Joyee Cheung) [#53563](https://github.com/nodejs/node/pull/53563)
* \[[`81767f6089`](https://github.com/nodejs/node/commit/81767f6089)] - **src**: fix Worker termination when '--inspect-brk' is passed (Daeyeon Jeong) [#53724](https://github.com/nodejs/node/pull/53724)
* \[[`a9db553935`](https://github.com/nodejs/node/commit/a9db553935)] - **src**: refactor embedded entrypoint loading (Joyee Cheung) [#53573](https://github.com/nodejs/node/pull/53573)
* \[[`3ab8aba478`](https://github.com/nodejs/node/commit/3ab8aba478)] - **src**: do not get string\_view from temp string (Cheng) [#53688](https://github.com/nodejs/node/pull/53688)
* \[[`664bf6c28f`](https://github.com/nodejs/node/commit/664bf6c28f)] - **src**: replace `kPathSeparator` with std::filesystem (Yagiz Nizipli) [#53063](https://github.com/nodejs/node/pull/53063)
* \[[`cc1f49751a`](https://github.com/nodejs/node/commit/cc1f49751a)] - **src**: move `FromNamespacedPath` to path.cc (Yagiz Nizipli) [#53540](https://github.com/nodejs/node/pull/53540)
* \[[`e43a4e07ec`](https://github.com/nodejs/node/commit/e43a4e07ec)] - **src**: use `starts_with` in node\_dotenv.cc (Yagiz Nizipli) [#53539](https://github.com/nodejs/node/pull/53539)
* \[[`19488fd4ce`](https://github.com/nodejs/node/commit/19488fd4ce)] - **src,test**: further cleanup references to osx (Daniel Bayley) [#53820](https://github.com/nodejs/node/pull/53820)
* \[[`4bf62f6cbd`](https://github.com/nodejs/node/commit/4bf62f6cbd)] - **stream**: improve inspector ergonomics (Benjamin Gruenbaum) [#53800](https://github.com/nodejs/node/pull/53800)
* \[[`82d88a83f8`](https://github.com/nodejs/node/commit/82d88a83f8)] - **(SEMVER-MINOR)** **stream**: pipeline wait for close before calling the callback (jakecastelli) [#53462](https://github.com/nodejs/node/pull/53462)
* \[[`53a7dd7790`](https://github.com/nodejs/node/commit/53a7dd7790)] - **test**: update wpt test (Mert Can Altin) [#53814](https://github.com/nodejs/node/pull/53814)
* \[[`bc480902ab`](https://github.com/nodejs/node/commit/bc480902ab)] - **test**: update WPT WebIDL interfaces (Filip Skokan) [#53720](https://github.com/nodejs/node/pull/53720)
* \[[`d13153d90f`](https://github.com/nodejs/node/commit/d13153d90f)] - **test**: un-set inspector-async-hook-setup-at-inspect-brk as flaky (Abdirahim Musse) [#53692](https://github.com/nodejs/node/pull/53692)
* \[[`ac9c2e6bf2`](https://github.com/nodejs/node/commit/ac9c2e6bf2)] - **test**: use python3 instead of python in pummel test (Mathis Wiehl) [#53057](https://github.com/nodejs/node/pull/53057)
* \[[`bac28678e6`](https://github.com/nodejs/node/commit/bac28678e6)] - **test**: do not assume cwd in snapshot tests (Antoine du Hamel) [#53146](https://github.com/nodejs/node/pull/53146)
* \[[`41e106c0c6`](https://github.com/nodejs/node/commit/41e106c0c6)] - **test**: use `Set.difference()` (Richard Lau) [#53597](https://github.com/nodejs/node/pull/53597)
* \[[`8aab680f66`](https://github.com/nodejs/node/commit/8aab680f66)] - **test**: fix OpenSSL version checks (Richard Lau) [#53503](https://github.com/nodejs/node/pull/53503)
* \[[`6aa4f0f266`](https://github.com/nodejs/node/commit/6aa4f0f266)] - **test**: refactor, add assertion to http-request-end (jakecastelli) [#53411](https://github.com/nodejs/node/pull/53411)
* \[[`fbc5cbb617`](https://github.com/nodejs/node/commit/fbc5cbb617)] - **test\_runner**: remove plan option from run() (Colin Ihrig) [#53834](https://github.com/nodejs/node/pull/53834)
* \[[`c590828ad8`](https://github.com/nodejs/node/commit/c590828ad8)] - **test\_runner**: fix escaping in snapshot tests (Julian Kniephoff) [#53833](https://github.com/nodejs/node/pull/53833)
* \[[`3a0fcbb17a`](https://github.com/nodejs/node/commit/3a0fcbb17a)] - **test\_runner**: support glob matching coverage files (Aviv Keller) [#53553](https://github.com/nodejs/node/pull/53553)
* \[[`e6a1eeb73d`](https://github.com/nodejs/node/commit/e6a1eeb73d)] - **test\_runner**: support module detection in module mocks (Geoffrey Booth) [#53642](https://github.com/nodejs/node/pull/53642)
* \[[`4d777de7d4`](https://github.com/nodejs/node/commit/4d777de7d4)] - **tls**: add setKeyCert() to tls.Socket (Brian White) [#53636](https://github.com/nodejs/node/pull/53636)
* \[[`ab9adfc42a`](https://github.com/nodejs/node/commit/ab9adfc42a)] - **tls**: remove prototype primordials (Antoine du Hamel) [#53699](https://github.com/nodejs/node/pull/53699)
* \[[`03d378ffb9`](https://github.com/nodejs/node/commit/03d378ffb9)] - **tools**: update lint-md-dependencies (Node.js GitHub Bot) [#53840](https://github.com/nodejs/node/pull/53840)
* \[[`06377b1b11`](https://github.com/nodejs/node/commit/06377b1b11)] - **tools**: update eslint to 9.7.0 (Node.js GitHub Bot) [#53839](https://github.com/nodejs/node/pull/53839)
* \[[`d6629a2d84`](https://github.com/nodejs/node/commit/d6629a2d84)] - **tools**: use v8\_features.json to populate config.gypi (Cheng) [#53749](https://github.com/nodejs/node/pull/53749)
* \[[`d3653fe8ac`](https://github.com/nodejs/node/commit/d3653fe8ac)] - **tools**: update eslint to 9.6.0 (Node.js GitHub Bot) [#53645](https://github.com/nodejs/node/pull/53645)
* \[[`1e930e93d4`](https://github.com/nodejs/node/commit/1e930e93d4)] - **tools**: update lint-md-dependencies to unified\@11.0.5 (Node.js GitHub Bot) [#53555](https://github.com/nodejs/node/pull/53555)
* \[[`317a13b30f`](https://github.com/nodejs/node/commit/317a13b30f)] - **tools**: replace reference to NodeMainInstance with SnapshotBuilder (codediverdev) [#53544](https://github.com/nodejs/node/pull/53544)
* \[[`0e25faea0a`](https://github.com/nodejs/node/commit/0e25faea0a)] - **typings**: add `fs_dir` types (Yagiz Nizipli) [#53631](https://github.com/nodejs/node/pull/53631)
* \[[`7637f291be`](https://github.com/nodejs/node/commit/7637f291be)] - **url**: fix typo (KAYYY) [#53827](https://github.com/nodejs/node/pull/53827)
* \[[`2c6548afd1`](https://github.com/nodejs/node/commit/2c6548afd1)] - **url**: reduce unnecessary string copies (Yagiz Nizipli) [#53628](https://github.com/nodejs/node/pull/53628)
* \[[`0f2b57d1bc`](https://github.com/nodejs/node/commit/0f2b57d1bc)] - **url**: make URL.parse enumerable (Filip Skokan) [#53720](https://github.com/nodejs/node/pull/53720)
* \[[`1300169f80`](https://github.com/nodejs/node/commit/1300169f80)] - **url**: add missing documentation for `URL.parse()` (Yagiz Nizipli) [#53733](https://github.com/nodejs/node/pull/53733)
* \[[`c55e72ed8b`](https://github.com/nodejs/node/commit/c55e72ed8b)] - **util**: fix crashing when emitting new Buffer() deprecation warning #53075 (Aras Abbasi) [#53089](https://github.com/nodejs/node/pull/53089)
* \[[`5aa216320e`](https://github.com/nodejs/node/commit/5aa216320e)] - **v8**: move `ToNamespacedPath` to c++ (Yagiz Nizipli) [#53655](https://github.com/nodejs/node/pull/53655)
* \[[`9fd976b09d`](https://github.com/nodejs/node/commit/9fd976b09d)] - **vm,src**: add property query interceptors (Chengzhong Wu) [#53517](https://github.com/nodejs/node/pull/53517)
* \[[`22ca334090`](https://github.com/nodejs/node/commit/22ca334090)] - **(SEMVER-MINOR)** **worker**: add postMessageToThread (Paolo Insogna) [#53682](https://github.com/nodejs/node/pull/53682)
* \[[`5aecbefbd5`](https://github.com/nodejs/node/commit/5aecbefbd5)] - **worker**: allow copied NODE\_OPTIONS in the env setting (Joyee Cheung) [#53596](https://github.com/nodejs/node/pull/53596)

<a id="22.4.1"></a>

## 2024-07-08, Version 22.4.1 (Current), @RafaelGSS

This is a security release.

### Notable Changes

* CVE-2024-36138 - Bypass incomplete fix of CVE-2024-27980 (High)
* CVE-2024-22020 - Bypass network import restriction via data URL (Medium)
* CVE-2024-22018 - fs.lstat bypasses permission model (Low)
* CVE-2024-36137 - fs.fchown/fchmod bypasses permission model (Low)
* CVE-2024-37372 - Permission model improperly processes UNC paths (Low)

### Commits

* \[[`110902ff5e`](https://github.com/nodejs/node/commit/110902ff5e)] - **lib,esm**: handle bypass network-import via data: (RafaelGSS) [nodejs-private/node-private#522](https://github.com/nodejs-private/node-private/pull/522)
* \[[`0a0de3d491`](https://github.com/nodejs/node/commit/0a0de3d491)] - **lib,permission**: support fs.lstat (RafaelGSS)
* \[[`93574335ff`](https://github.com/nodejs/node/commit/93574335ff)] - **lib,permission**: disable fchmod/fchown when pm enabled (RafaelGSS) [nodejs-private/node-private#584](https://github.com/nodejs-private/node-private/pull/584)
* \[[`09899e6302`](https://github.com/nodejs/node/commit/09899e6302)] - **src**: handle permissive extension on cmd check (RafaelGSS) [nodejs-private/node-private#596](https://github.com/nodejs-private/node-private/pull/596)
* \[[`5d9c811634`](https://github.com/nodejs/node/commit/5d9c811634)] - **src,permission**: fix UNC path resolution (RafaelGSS) [nodejs-private/node-private#581](https://github.com/nodejs-private/node-private/pull/581)

<a id="22.4.0"></a>

## 2024-07-02, Version 22.4.0 (Current), @targos

### Notable Changes

#### Experimental Web Storage API

* \[[`9e30724b53`](https://github.com/nodejs/node/commit/9e30724b53)] - **(SEMVER-MINOR)** **deps,lib,src**: add experimental web storage (Colin Ihrig) [#52435](https://github.com/nodejs/node/pull/52435)

#### API stability updates

* \[[`201266706b`](https://github.com/nodejs/node/commit/201266706b)] - **doc**: move `node --run` stability to rc (Yagiz Nizipli) [#53433](https://github.com/nodejs/node/pull/53433)
* \[[`16c0884d48`](https://github.com/nodejs/node/commit/16c0884d48)] - **doc**: mark WebSocket as stable (Matthew Aitken) [#53352](https://github.com/nodejs/node/pull/53352)
* \[[`cf375e73c1`](https://github.com/nodejs/node/commit/cf375e73c1)] - **doc**: mark --heap-prof and related flags stable (Joyee Cheung) [#53343](https://github.com/nodejs/node/pull/53343)
* \[[`0160745057`](https://github.com/nodejs/node/commit/0160745057)] - **doc**: mark --cpu-prof and related flags stable (Joyee Cheung) [#53343](https://github.com/nodejs/node/pull/53343)

#### Other Notable Changes

* \[[`df4762722c`](https://github.com/nodejs/node/commit/df4762722c)] - **doc**: doc-only deprecate OpenSSL engine-based APIs (Richard Lau) [#53329](https://github.com/nodejs/node/pull/53329)
* \[[`ad5282e196`](https://github.com/nodejs/node/commit/ad5282e196)] - **inspector**: fix disable async hooks on `Debugger.setAsyncCallStackDepth` (Joyee Cheung) [#53473](https://github.com/nodejs/node/pull/53473)
* \[[`e95af740fc`](https://github.com/nodejs/node/commit/e95af740fc)] - **(SEMVER-MINOR)** **lib**: add diagnostics\_channel events to module loading (RafaelGSS) [#44340](https://github.com/nodejs/node/pull/44340)
* \[[`50733a1abe`](https://github.com/nodejs/node/commit/50733a1abe)] - **(SEMVER-MINOR)** **util**: support `--no-` for argument with boolean type for parseArgs (Zhenwei Jin) [#53107](https://github.com/nodejs/node/pull/53107)

### Commits

* \[[`9f32002397`](https://github.com/nodejs/node/commit/9f32002397)] - **assert,util**: correct comparison when both contain same reference (Daniel Lemire) [#53431](https://github.com/nodejs/node/pull/53431)
* \[[`dfdc062111`](https://github.com/nodejs/node/commit/dfdc062111)] - **buffer**: make indexOf(byte) faster (Tobias Nießen) [#53455](https://github.com/nodejs/node/pull/53455)
* \[[`1de437527e`](https://github.com/nodejs/node/commit/1de437527e)] - **build**: configure with shared sqlite3 (Chengzhong Wu) [#53519](https://github.com/nodejs/node/pull/53519)
* \[[`c7d44ba1f3`](https://github.com/nodejs/node/commit/c7d44ba1f3)] - **build**: find version of Clang installed on Windows (Stefan Stojanovic) [#53228](https://github.com/nodejs/node/pull/53228)
* \[[`36aad8b204`](https://github.com/nodejs/node/commit/36aad8b204)] - **build**: fix spacing before NINJA\_ARGS (jakecastelli) [#53181](https://github.com/nodejs/node/pull/53181)
* \[[`82092cdaa3`](https://github.com/nodejs/node/commit/82092cdaa3)] - **crypto**: improve GetECGroupBits signature (Tobias Nießen) [#53364](https://github.com/nodejs/node/pull/53364)
* \[[`073c231607`](https://github.com/nodejs/node/commit/073c231607)] - **deps**: update c-ares to v1.31.0 (Node.js GitHub Bot) [#53554](https://github.com/nodejs/node/pull/53554)
* \[[`977beab729`](https://github.com/nodejs/node/commit/977beab729)] - **(SEMVER-MINOR)** **deps**: sqlite: fix Windows compilation (Colin Ihrig) [#52435](https://github.com/nodejs/node/pull/52435)
* \[[`e69b8d202c`](https://github.com/nodejs/node/commit/e69b8d202c)] - **deps**: update undici to 6.19.2 (Node.js GitHub Bot) [#53468](https://github.com/nodejs/node/pull/53468)
* \[[`c4a7e051c8`](https://github.com/nodejs/node/commit/c4a7e051c8)] - **deps**: update undici to 6.19.1 (Node.js GitHub Bot) [#53468](https://github.com/nodejs/node/pull/53468)
* \[[`fa34f8fcf0`](https://github.com/nodejs/node/commit/fa34f8fcf0)] - **deps**: update undici to 6.19.1 (Node.js GitHub Bot) [#53468](https://github.com/nodejs/node/pull/53468)
* \[[`0b40bfad43`](https://github.com/nodejs/node/commit/0b40bfad43)] - **deps**: update undici to 6.19.0 (Node.js GitHub Bot) [#53468](https://github.com/nodejs/node/pull/53468)
* \[[`1877f22a79`](https://github.com/nodejs/node/commit/1877f22a79)] - **deps**: update simdjson to 3.9.4 (Node.js GitHub Bot) [#53467](https://github.com/nodejs/node/pull/53467)
* \[[`1b84964b8d`](https://github.com/nodejs/node/commit/1b84964b8d)] - **deps**: patch V8 to 12.4.254.21 (Node.js GitHub Bot) [#53470](https://github.com/nodejs/node/pull/53470)
* \[[`6acadeb59b`](https://github.com/nodejs/node/commit/6acadeb59b)] - **deps**: update acorn-walk to 8.3.3 (Node.js GitHub Bot) [#53466](https://github.com/nodejs/node/pull/53466)
* \[[`7a7f438841`](https://github.com/nodejs/node/commit/7a7f438841)] - **deps**: update zlib to 1.3.0.1-motley-209717d (Node.js GitHub Bot) [#53156](https://github.com/nodejs/node/pull/53156)
* \[[`bf891bf64c`](https://github.com/nodejs/node/commit/bf891bf64c)] - **deps**: update c-ares to v1.30.0 (Node.js GitHub Bot) [#53416](https://github.com/nodejs/node/pull/53416)
* \[[`bd68888261`](https://github.com/nodejs/node/commit/bd68888261)] - **deps**: V8: cherry-pick a3cc8522a4c8 (kxxt) [#53412](https://github.com/nodejs/node/pull/53412)
* \[[`2defaaf771`](https://github.com/nodejs/node/commit/2defaaf771)] - **deps**: V8: cherry-pick 6ea594ff7132 (kxxt) [#53412](https://github.com/nodejs/node/pull/53412)
* \[[`9e30724b53`](https://github.com/nodejs/node/commit/9e30724b53)] - **(SEMVER-MINOR)** **deps,lib,src**: add experimental web storage (Colin Ihrig) [#52435](https://github.com/nodejs/node/pull/52435)
* \[[`608cc05de1`](https://github.com/nodejs/node/commit/608cc05de1)] - **doc**: recommend not using libuv node-api function (Michael Dawson) [#53521](https://github.com/nodejs/node/pull/53521)
* \[[`30858eca59`](https://github.com/nodejs/node/commit/30858eca59)] - **doc**: add additional guidance for PRs to deps (Michael Dawson) [#53499](https://github.com/nodejs/node/pull/53499)
* \[[`a5852cc710`](https://github.com/nodejs/node/commit/a5852cc710)] - **doc**: only apply content-visibility on all.html (Filip Skokan) [#53510](https://github.com/nodejs/node/pull/53510)
* \[[`befabe5c58`](https://github.com/nodejs/node/commit/befabe5c58)] - **doc**: update the description of the return type for options.filter (Zhenwei Jin) [#52742](https://github.com/nodejs/node/pull/52742)
* \[[`5ed1a036ba`](https://github.com/nodejs/node/commit/5ed1a036ba)] - **doc**: remove first timer badge (Aviv Keller) [#53338](https://github.com/nodejs/node/pull/53338)
* \[[`201266706b`](https://github.com/nodejs/node/commit/201266706b)] - **doc**: move `node --run` stability to rc (Yagiz Nizipli) [#53433](https://github.com/nodejs/node/pull/53433)
* \[[`46a7681cc4`](https://github.com/nodejs/node/commit/46a7681cc4)] - **doc**: add Buffer.from(string) to functions that use buffer pool (Christian Bates-White) [#52801](https://github.com/nodejs/node/pull/52801)
* \[[`ec5364f6de`](https://github.com/nodejs/node/commit/ec5364f6de)] - **doc**: add initial text for ambassadors program (Michael Dawson) [#52857](https://github.com/nodejs/node/pull/52857)
* \[[`fa113b8fc7`](https://github.com/nodejs/node/commit/fa113b8fc7)] - **doc**: fix typo (EhsanKhaki) [#53397](https://github.com/nodejs/node/pull/53397)
* \[[`d9182d0086`](https://github.com/nodejs/node/commit/d9182d0086)] - **doc**: define more cases for stream event emissions (Aviv Keller) [#53317](https://github.com/nodejs/node/pull/53317)
* \[[`923d24b6f2`](https://github.com/nodejs/node/commit/923d24b6f2)] - **doc**: remove mentions of policy model from security info (Aviv Keller) [#53249](https://github.com/nodejs/node/pull/53249)
* \[[`48f78cd31b`](https://github.com/nodejs/node/commit/48f78cd31b)] - **doc**: fix mistakes in the module `load` hook api (István Donkó) [#53349](https://github.com/nodejs/node/pull/53349)
* \[[`16c0884d48`](https://github.com/nodejs/node/commit/16c0884d48)] - **doc**: mark WebSocket as stable (Matthew Aitken) [#53352](https://github.com/nodejs/node/pull/53352)
* \[[`df4762722c`](https://github.com/nodejs/node/commit/df4762722c)] - **doc**: doc-only deprecate OpenSSL engine-based APIs (Richard Lau) [#53329](https://github.com/nodejs/node/pull/53329)
* \[[`cf375e73c1`](https://github.com/nodejs/node/commit/cf375e73c1)] - **doc**: mark --heap-prof and related flags stable (Joyee Cheung) [#53343](https://github.com/nodejs/node/pull/53343)
* \[[`0160745057`](https://github.com/nodejs/node/commit/0160745057)] - **doc**: mark --cpu-prof and related flags stable (Joyee Cheung) [#53343](https://github.com/nodejs/node/pull/53343)
* \[[`6e12d9f049`](https://github.com/nodejs/node/commit/6e12d9f049)] - **doc**: remove IRC from man page (Tobias Nießen) [#53344](https://github.com/nodejs/node/pull/53344)
* \[[`24c7a9415b`](https://github.com/nodejs/node/commit/24c7a9415b)] - **doc, http**: add `rejectNonStandardBodyWrites` option, clear its behaviour (jakecastelli) [#53396](https://github.com/nodejs/node/pull/53396)
* \[[`ec38f3dc6a`](https://github.com/nodejs/node/commit/ec38f3dc6a)] - **doc, meta**: organize contributing to Node-API guide (Aviv Keller) [#53243](https://github.com/nodejs/node/pull/53243)
* \[[`cf5a973c42`](https://github.com/nodejs/node/commit/cf5a973c42)] - **doc, meta**: use markdown rather than HTML in CONTRIBUTING.md (Aviv Keller) [#53235](https://github.com/nodejs/node/pull/53235)
* \[[`105b006fd2`](https://github.com/nodejs/node/commit/105b006fd2)] - **fs**: move `ToNamespacedPath` to c++ (Yagiz Nizipli) [#52135](https://github.com/nodejs/node/pull/52135)
* \[[`568377f7f0`](https://github.com/nodejs/node/commit/568377f7f0)] - **fs**: do not crash if the watched file is removed while setting up watch (Matteo Collina) [#53452](https://github.com/nodejs/node/pull/53452)
* \[[`fad179307c`](https://github.com/nodejs/node/commit/fad179307c)] - **fs**: add fast api for `InternalModuleStat` (Yagiz Nizipli) [#51344](https://github.com/nodejs/node/pull/51344)
* \[[`41100b65f6`](https://github.com/nodejs/node/commit/41100b65f6)] - **http2**: reject failed http2.connect when used with promisify (ehsankhfr) [#53475](https://github.com/nodejs/node/pull/53475)
* \[[`ad5282e196`](https://github.com/nodejs/node/commit/ad5282e196)] - **inspector**: fix disable async hooks on Debugger.setAsyncCallStackDepth (Joyee Cheung) [#53473](https://github.com/nodejs/node/pull/53473)
* \[[`b5fc227344`](https://github.com/nodejs/node/commit/b5fc227344)] - **lib**: fix typo in comment (codediverdev) [#53543](https://github.com/nodejs/node/pull/53543)
* \[[`e95af740fc`](https://github.com/nodejs/node/commit/e95af740fc)] - **(SEMVER-MINOR)** **lib**: add diagnostics\_channel events to module loading (RafaelGSS) [#44340](https://github.com/nodejs/node/pull/44340)
* \[[`123910f1de`](https://github.com/nodejs/node/commit/123910f1de)] - **lib**: remove the unused code (theanarkh) [#53463](https://github.com/nodejs/node/pull/53463)
* \[[`452011b719`](https://github.com/nodejs/node/commit/452011b719)] - **lib**: speed up MessageEvent creation internally (Matthew Aitken) [#52951](https://github.com/nodejs/node/pull/52951)
* \[[`710cf7758c`](https://github.com/nodejs/node/commit/710cf7758c)] - **lib**: reduce amount of caught URL errors (Yagiz Nizipli) [#52658](https://github.com/nodejs/node/pull/52658)
* \[[`45b59e58d1`](https://github.com/nodejs/node/commit/45b59e58d1)] - **lib**: fix naming convention of `Symbol` (Deokjin Kim) [#53387](https://github.com/nodejs/node/pull/53387)
* \[[`515dd24ee7`](https://github.com/nodejs/node/commit/515dd24ee7)] - **lib**: fix timer leak (theanarkh) [#53337](https://github.com/nodejs/node/pull/53337)
* \[[`77166137be`](https://github.com/nodejs/node/commit/77166137be)] - **meta**: use correct source for workflow in PR (Aviv Keller) [#53490](https://github.com/nodejs/node/pull/53490)
* \[[`d1c10fee53`](https://github.com/nodejs/node/commit/d1c10fee53)] - **meta**: move one or more collaborators to emeritus (Node.js GitHub Bot) [#53480](https://github.com/nodejs/node/pull/53480)
* \[[`a5026386bf`](https://github.com/nodejs/node/commit/a5026386bf)] - **meta**: fix typo in dependency updates (Aviv Keller) [#53471](https://github.com/nodejs/node/pull/53471)
* \[[`0b9191da99`](https://github.com/nodejs/node/commit/0b9191da99)] - **meta**: bump step-security/harden-runner from 2.7.1 to 2.8.0 (dependabot\[bot]) [#53245](https://github.com/nodejs/node/pull/53245)
* \[[`49cfb9d001`](https://github.com/nodejs/node/commit/49cfb9d001)] - **src**: reset `process.versions` during pre-execution (Richard Lau) [#53444](https://github.com/nodejs/node/pull/53444)
* \[[`15df4edd22`](https://github.com/nodejs/node/commit/15df4edd22)] - **src**: use `args.This()` instead of `Holder` (Michaël Zasso) [#53474](https://github.com/nodejs/node/pull/53474)
* \[[`e16a04e852`](https://github.com/nodejs/node/commit/e16a04e852)] - **src**: fix dynamically linked OpenSSL version (Richard Lau) [#53456](https://github.com/nodejs/node/pull/53456)
* \[[`5961253824`](https://github.com/nodejs/node/commit/5961253824)] - **src**: remove `base64` from `process.versions` (Richard Lau) [#53442](https://github.com/nodejs/node/pull/53442)
* \[[`11dd15c0b5`](https://github.com/nodejs/node/commit/11dd15c0b5)] - **src**: remove `SetEncoding` from StringEncoder (Yagiz Nizipli) [#53441](https://github.com/nodejs/node/pull/53441)
* \[[`0c7e69acd2`](https://github.com/nodejs/node/commit/0c7e69acd2)] - **src**: simplify `size() == 0` checks (Yagiz Nizipli) [#53440](https://github.com/nodejs/node/pull/53440)
* \[[`f077afafda`](https://github.com/nodejs/node/commit/f077afafda)] - **src**: add utilities to help debugging reproducibility of snapshots (Joyee Cheung) [#50983](https://github.com/nodejs/node/pull/50983)
* \[[`004b9ea4c4`](https://github.com/nodejs/node/commit/004b9ea4c4)] - **src**: make sure that memcpy-ed structs in snapshot have no padding (Joyee Cheung) [#50983](https://github.com/nodejs/node/pull/50983)
* \[[`bfc5236423`](https://github.com/nodejs/node/commit/bfc5236423)] - **src**: return non-empty data in context data serializer (Joyee Cheung) [#50983](https://github.com/nodejs/node/pull/50983)
* \[[`955454ba4d`](https://github.com/nodejs/node/commit/955454ba4d)] - **src**: fix typo in env.cc (EhsanKhaki) [#53418](https://github.com/nodejs/node/pull/53418)
* \[[`7d8787768c`](https://github.com/nodejs/node/commit/7d8787768c)] - **src**: avoid strcmp in favor of operator== (Tobias Nießen) [#53439](https://github.com/nodejs/node/pull/53439)
* \[[`599e7c3d8e`](https://github.com/nodejs/node/commit/599e7c3d8e)] - **src**: remove ArrayBufferAllocator::Reallocate override (Shu-yu Guo) [#52910](https://github.com/nodejs/node/pull/52910)
* \[[`f9075ff38e`](https://github.com/nodejs/node/commit/f9075ff38e)] - **src**: print v8::OOMDetails::detail when it's available (Joyee Cheung) [#53360](https://github.com/nodejs/node/pull/53360)
* \[[`4704270443`](https://github.com/nodejs/node/commit/4704270443)] - **src**: fix IsIPAddress for IPv6 (Hüseyin Açacak) [#53400](https://github.com/nodejs/node/pull/53400)
* \[[`63f62d76de`](https://github.com/nodejs/node/commit/63f62d76de)] - **src**: fix permission inspector crash (theanarkh) [#53389](https://github.com/nodejs/node/pull/53389)
* \[[`70bbc02dac`](https://github.com/nodejs/node/commit/70bbc02dac)] - **src, deps**: add nbytes library (James M Snell) [#53507](https://github.com/nodejs/node/pull/53507)
* \[[`8b877099d0`](https://github.com/nodejs/node/commit/8b877099d0)] - **stream**: update outdated highwatermark doc (Jay Kim) [#53494](https://github.com/nodejs/node/pull/53494)
* \[[`eded1e9768`](https://github.com/nodejs/node/commit/eded1e9768)] - **stream**: support dispose in writable (Benjamin Gruenbaum) [#48547](https://github.com/nodejs/node/pull/48547)
* \[[`b3372a8b0e`](https://github.com/nodejs/node/commit/b3372a8b0e)] - **stream**: callback should be called when pendingcb is 0 (jakecastelli) [#53438](https://github.com/nodejs/node/pull/53438)
* \[[`f4efb7f625`](https://github.com/nodejs/node/commit/f4efb7f625)] - **stream**: make sure \_destroy is called (jakecastelli) [#53213](https://github.com/nodejs/node/pull/53213)
* \[[`7dde37591c`](https://github.com/nodejs/node/commit/7dde37591c)] - **stream**: prevent stream unexpected pause when highWaterMark set to 0 (jakecastelli) [#53261](https://github.com/nodejs/node/pull/53261)
* \[[`6e66d9763f`](https://github.com/nodejs/node/commit/6e66d9763f)] - **test**: mark `test-benchmark-crypto` as flaky (Antoine du Hamel) [#52955](https://github.com/nodejs/node/pull/52955)
* \[[`1eebcbf9bf`](https://github.com/nodejs/node/commit/1eebcbf9bf)] - **test**: skip reproducible snapshot test on 32-bit (Michaël Zasso) [#53592](https://github.com/nodejs/node/pull/53592)
* \[[`91b2850303`](https://github.com/nodejs/node/commit/91b2850303)] - **test**: extend env for `test-node-output-errors` (Richard Lau) [#53535](https://github.com/nodejs/node/pull/53535)
* \[[`bcad560726`](https://github.com/nodejs/node/commit/bcad560726)] - **test**: update `compression` web-platform tests (Yagiz Nizipli) [#53478](https://github.com/nodejs/node/pull/53478)
* \[[`b8f436c755`](https://github.com/nodejs/node/commit/b8f436c755)] - **test**: update encoding web-platform tests (Yagiz Nizipli) [#53477](https://github.com/nodejs/node/pull/53477)
* \[[`d2c169a4f6`](https://github.com/nodejs/node/commit/d2c169a4f6)] - **test**: update `url` web-platform tests (Yagiz Nizipli) [#53472](https://github.com/nodejs/node/pull/53472)
* \[[`513e6aa4c7`](https://github.com/nodejs/node/commit/513e6aa4c7)] - **test**: check against run-time OpenSSL version (Richard Lau) [#53456](https://github.com/nodejs/node/pull/53456)
* \[[`602b9d63c4`](https://github.com/nodejs/node/commit/602b9d63c4)] - **test**: update tests for OpenSSL 3.0.14 (Richard Lau) [#53373](https://github.com/nodejs/node/pull/53373)
* \[[`4a3525bb08`](https://github.com/nodejs/node/commit/4a3525bb08)] - **test**: fix test-http-server-keepalive-req-gc (Etienne Pierre-doray) [#53292](https://github.com/nodejs/node/pull/53292)
* \[[`7349edb28b`](https://github.com/nodejs/node/commit/7349edb28b)] - **test**: update TLS tests for OpenSSL 3.2 (Richard Lau) [#53384](https://github.com/nodejs/node/pull/53384)
* \[[`a11a05763d`](https://github.com/nodejs/node/commit/a11a05763d)] - **tls**: check result of SSL\_CTX\_set\_\*\_proto\_version (Tobias Nießen) [#53459](https://github.com/nodejs/node/pull/53459)
* \[[`4b47f89eb2`](https://github.com/nodejs/node/commit/4b47f89eb2)] - **tls**: avoid taking ownership of OpenSSL objects (Tobias Nießen) [#53436](https://github.com/nodejs/node/pull/53436)
* \[[`ac8adeb99f`](https://github.com/nodejs/node/commit/ac8adeb99f)] - **tls**: use SSL\_get\_peer\_tmp\_key (Tobias Nießen) [#53366](https://github.com/nodejs/node/pull/53366)
* \[[`d5c380bb09`](https://github.com/nodejs/node/commit/d5c380bb09)] - **tools**: lock versions of irrelevant DB deps (Michaël Zasso) [#53546](https://github.com/nodejs/node/pull/53546)
* \[[`71321bb249`](https://github.com/nodejs/node/commit/71321bb249)] - **tools**: fix skip detection of test runner output (Richard Lau) [#53545](https://github.com/nodejs/node/pull/53545)
* \[[`ca198f4125`](https://github.com/nodejs/node/commit/ca198f4125)] - **tools**: update eslint to 9.5.0 (Node.js GitHub Bot) [#53515](https://github.com/nodejs/node/pull/53515)
* \[[`30fdd482a1`](https://github.com/nodejs/node/commit/30fdd482a1)] - **tools**: move ESLint to tools/eslint (Michaël Zasso) [#53413](https://github.com/nodejs/node/pull/53413)
* \[[`fe85e05ba9`](https://github.com/nodejs/node/commit/fe85e05ba9)] - **tools**: fix c-ares update script (Marco Ippolito) [#53414](https://github.com/nodejs/node/pull/53414)
* \[[`8eb7bdf81b`](https://github.com/nodejs/node/commit/8eb7bdf81b)] - **tools**: update lint-md-dependencies (Node.js GitHub Bot) [#53158](https://github.com/nodejs/node/pull/53158)
* \[[`9ece63d415`](https://github.com/nodejs/node/commit/9ece63d415)] - **tools**: do not run Corepack code before it's reviewed (Antoine du Hamel) [#53405](https://github.com/nodejs/node/pull/53405)
* \[[`ab2021492b`](https://github.com/nodejs/node/commit/ab2021492b)] - **tools**: move ESLint tools to tools/eslint (Michaël Zasso) [#53393](https://github.com/nodejs/node/pull/53393)
* \[[`78a9037a6d`](https://github.com/nodejs/node/commit/78a9037a6d)] - **tools**: use Ubuntu 24.04 and Clang on GitHub actions (Michaël Zasso) [#53212](https://github.com/nodejs/node/pull/53212)
* \[[`855eb25dad`](https://github.com/nodejs/node/commit/855eb25dad)] - **tools**: add stream label on PR when related files being changed in lib (jakecastelli) [#53269](https://github.com/nodejs/node/pull/53269)
* \[[`50733a1abe`](https://github.com/nodejs/node/commit/50733a1abe)] - **(SEMVER-MINOR)** **util**: support `--no-` for argument with boolean type for parseArgs (Zhenwei Jin) [#53107](https://github.com/nodejs/node/pull/53107)

<a id="22.3.0"></a>

## 2024-06-11, Version 22.3.0 (Current), @RafaelGSS

### Notable Changes

* \[[`5a41bcf9ca`](https://github.com/nodejs/node/commit/5a41bcf9ca)] - **(SEMVER-MINOR)** **src**: traverse parent folders while running `--run` (Yagiz Nizipli) [#53154](https://github.com/nodejs/node/pull/53154)
* \[[`1d5934524b`](https://github.com/nodejs/node/commit/1d5934524b)] - **(SEMVER-MINOR)** **buffer**: add .bytes() method to Blob (Matthew Aitken) [#53221](https://github.com/nodejs/node/pull/53221)
* \[[`75e5612fae`](https://github.com/nodejs/node/commit/75e5612fae)] - **(SEMVER-MINOR)** **src,permission**: --allow-wasi & prevent WASI exec (Rafael Gonzaga) [#53124](https://github.com/nodejs/node/pull/53124)
* \[[`b5c30e2f5e`](https://github.com/nodejs/node/commit/b5c30e2f5e)] - **(SEMVER-MINOR)** **module**: print amount of load time of a cjs module (Vinicius Lourenço) [#52213](https://github.com/nodejs/node/pull/52213)
* \[[`8c6dffc269`](https://github.com/nodejs/node/commit/8c6dffc269)] - **(SEMVER-MINOR)** **test\_runner**: add snapshot testing (Colin Ihrig) [#53169](https://github.com/nodejs/node/pull/53169)
* \[[`048478d351`](https://github.com/nodejs/node/commit/048478d351)] - **(SEMVER-MINOR)** **doc**: add context.assert docs (Colin Ihrig) [#53169](https://github.com/nodejs/node/pull/53169)
* \[[`f6d2af8ee7`](https://github.com/nodejs/node/commit/f6d2af8ee7)] - **(SEMVER-MINOR)** **test\_runner**: add context.fullName (Colin Ihrig) [#53169](https://github.com/nodejs/node/pull/53169)
* \[[`a0766bdf0e`](https://github.com/nodejs/node/commit/a0766bdf0e)] - **(SEMVER-MINOR)** **net**: add new net.server.listen tracing channel (Paolo Insogna) [#53136](https://github.com/nodejs/node/pull/53136)
* \[[`374743cd4e`](https://github.com/nodejs/node/commit/374743cd4e)] - **(SEMVER-MINOR)** **process**: add process.getBuiltinModule(id) (Joyee Cheung) [#52762](https://github.com/nodejs/node/pull/52762)
* \[[`1eb55f3550`](https://github.com/nodejs/node/commit/1eb55f3550)] - **(SEMVER-MINOR)** **doc**: improve explanation about built-in modules (Joyee Cheung) [#52762](https://github.com/nodejs/node/pull/52762)
* \[[`6165894774`](https://github.com/nodejs/node/commit/6165894774)] - **fs**: mark recursive cp methods as stable (Théo LUDWIG) [#53127](https://github.com/nodejs/node/pull/53127)
* \[[`db5dd0c6df`](https://github.com/nodejs/node/commit/db5dd0c6df)] - **doc**: add StefanStojanovic to collaborators (StefanStojanovic) [#53118](https://github.com/nodejs/node/pull/53118)
* \[[`cfcde78513`](https://github.com/nodejs/node/commit/cfcde78513)] - **(SEMVER-MINOR)** **cli**: add `NODE_RUN_PACKAGE_JSON_PATH` env (Yagiz Nizipli) [#53058](https://github.com/nodejs/node/pull/53058)
* \[[`7a67ecf161`](https://github.com/nodejs/node/commit/7a67ecf161)] - **(SEMVER-MINOR)** **test\_runner**: support module mocking (Colin Ihrig) [#52848](https://github.com/nodejs/node/pull/52848)
* \[[`ee56aecced`](https://github.com/nodejs/node/commit/ee56aecced)] - **(SEMVER-MINOR)** **lib**: add EventSource Client (Aras Abbasi) [#51575](https://github.com/nodejs/node/pull/51575)
* \[[`6413769bc7`](https://github.com/nodejs/node/commit/6413769bc7)] - **(SEMVER-MINOR)** **lib**: replace MessageEvent with undici's (Matthew Aitken) [#52370](https://github.com/nodejs/node/pull/52370)
* \[[`c70b2f7a76`](https://github.com/nodejs/node/commit/c70b2f7a76)] - **(SEMVER-MINOR)** **cli**: add `NODE_RUN_SCRIPT_NAME` env to `node --run` (Yagiz Nizipli) [#53032](https://github.com/nodejs/node/pull/53032)
* \[[`badec0c38b`](https://github.com/nodejs/node/commit/badec0c38b)] - **doc**: add Marco Ippolito to TSC (Rafael Gonzaga) [#53008](https://github.com/nodejs/node/pull/53008)

### Commits

* \[[`feb0ba2860`](https://github.com/nodejs/node/commit/feb0ba2860)] - **benchmark**: fix napi/ref addon (Michaël Zasso) [#53233](https://github.com/nodejs/node/pull/53233)
* \[[`bb844de4e1`](https://github.com/nodejs/node/commit/bb844de4e1)] - **benchmark**: fix api restriction for the permission category (Ryan Tsien) [#51528](https://github.com/nodejs/node/pull/51528)
* \[[`1d5934524b`](https://github.com/nodejs/node/commit/1d5934524b)] - **(SEMVER-MINOR)** **buffer**: add .bytes() method to Blob (Matthew Aitken) [#53221](https://github.com/nodejs/node/pull/53221)
* \[[`d87f9af5aa`](https://github.com/nodejs/node/commit/d87f9af5aa)] - **buffer**: make compare/equals faster (Tobias Nießen) [#52993](https://github.com/nodejs/node/pull/52993)
* \[[`ec83431d71`](https://github.com/nodejs/node/commit/ec83431d71)] - **build**: generate binlog in out directories (Chengzhong Wu) [#53325](https://github.com/nodejs/node/pull/53325)
* \[[`0976439417`](https://github.com/nodejs/node/commit/0976439417)] - **build**: fix --v8-lite-mode build (Daeyeon Jeong) [#52725](https://github.com/nodejs/node/pull/52725)
* \[[`350c733ae6`](https://github.com/nodejs/node/commit/350c733ae6)] - **build**: support python 3.13 (Chengzhong Wu) [#53190](https://github.com/nodejs/node/pull/53190)
* \[[`74cefa55a2`](https://github.com/nodejs/node/commit/74cefa55a2)] - **build**: update ruff to v0.4.5 (Yagiz Nizipli) [#53180](https://github.com/nodejs/node/pull/53180)
* \[[`33242ff042`](https://github.com/nodejs/node/commit/33242ff042)] - **build**: add `--skip-tests` to `test-ci-js` target (Antoine du Hamel) [#53105](https://github.com/nodejs/node/pull/53105)
* \[[`edcadf7f8a`](https://github.com/nodejs/node/commit/edcadf7f8a)] - **build**: fix building embedtest in GN build (Cheng) [#53145](https://github.com/nodejs/node/pull/53145)
* \[[`d711942fce`](https://github.com/nodejs/node/commit/d711942fce)] - **build**: use broader detection for 'help' (Aviv Keller) [#53045](https://github.com/nodejs/node/pull/53045)
* \[[`ca655b61a7`](https://github.com/nodejs/node/commit/ca655b61a7)] - **build**: fix -j propagation to ninja (Tobias Nießen) [#53088](https://github.com/nodejs/node/pull/53088)
* \[[`5fba67ff9f`](https://github.com/nodejs/node/commit/5fba67ff9f)] - **build**: exit on unsupported host OS for Android (Mohammed Keyvanzadeh) [#52882](https://github.com/nodejs/node/pull/52882)
* \[[`b7d7e9a084`](https://github.com/nodejs/node/commit/b7d7e9a084)] - **build**: fix `--enable-d8` builds (Richard Lau) [#53106](https://github.com/nodejs/node/pull/53106)
* \[[`14547c5d32`](https://github.com/nodejs/node/commit/14547c5d32)] - **build**: fix ./configure --help format error (Zhenwei Jin) [#53066](https://github.com/nodejs/node/pull/53066)
* \[[`f9490806d3`](https://github.com/nodejs/node/commit/f9490806d3)] - **build**: set "clang" in config.gypi in GN build (Cheng) [#53004](https://github.com/nodejs/node/pull/53004)
* \[[`638b510ce7`](https://github.com/nodejs/node/commit/638b510ce7)] - **cli**: add `--expose-gc` flag available to `NODE_OPTIONS` (Juan José) [#53078](https://github.com/nodejs/node/pull/53078)
* \[[`cfcde78513`](https://github.com/nodejs/node/commit/cfcde78513)] - **(SEMVER-MINOR)** **cli**: add `NODE_RUN_PACKAGE_JSON_PATH` env (Yagiz Nizipli) [#53058](https://github.com/nodejs/node/pull/53058)
* \[[`c70b2f7a76`](https://github.com/nodejs/node/commit/c70b2f7a76)] - **(SEMVER-MINOR)** **cli**: add `NODE_RUN_SCRIPT_NAME` env to `node --run` (Yagiz Nizipli) [#53032](https://github.com/nodejs/node/pull/53032)
* \[[`34f20983fd`](https://github.com/nodejs/node/commit/34f20983fd)] - **crypto**: fix propagation of "memory limit exceeded" (Tobias Nießen) [#53300](https://github.com/nodejs/node/pull/53300)
* \[[`fef067f4f4`](https://github.com/nodejs/node/commit/fef067f4f4)] - **deps**: update nghttp2 to 1.62.1 (Node.js GitHub Bot) [#52966](https://github.com/nodejs/node/pull/52966)
* \[[`fc949928ac`](https://github.com/nodejs/node/commit/fc949928ac)] - **deps**: update nghttp2 to 1.62.0 (Node.js GitHub Bot) [#52966](https://github.com/nodejs/node/pull/52966)
* \[[`4a17dda8dc`](https://github.com/nodejs/node/commit/4a17dda8dc)] - **deps**: update undici to 6.18.2 (Node.js GitHub Bot) [#53255](https://github.com/nodejs/node/pull/53255)
* \[[`e45cc2a551`](https://github.com/nodejs/node/commit/e45cc2a551)] - **deps**: update ada to 2.8.0 (Node.js GitHub Bot) [#53254](https://github.com/nodejs/node/pull/53254)
* \[[`77907a2619`](https://github.com/nodejs/node/commit/77907a2619)] - **deps**: update corepack to 0.28.2 (Node.js GitHub Bot) [#53253](https://github.com/nodejs/node/pull/53253)
* \[[`b688050778`](https://github.com/nodejs/node/commit/b688050778)] - **deps**: update simdjson to 3.9.3 (Node.js GitHub Bot) [#53252](https://github.com/nodejs/node/pull/53252)
* \[[`6303f19cbe`](https://github.com/nodejs/node/commit/6303f19cbe)] - **deps**: patch V8 to 12.4.254.20 (Node.js GitHub Bot) [#53159](https://github.com/nodejs/node/pull/53159)
* \[[`257004c68f`](https://github.com/nodejs/node/commit/257004c68f)] - **deps**: update c-ares to 1.29.0 (Node.js GitHub Bot) [#53155](https://github.com/nodejs/node/pull/53155)
* \[[`0b375a3e36`](https://github.com/nodejs/node/commit/0b375a3e36)] - **deps**: upgrade npm to 10.8.1 (npm team) [#53207](https://github.com/nodejs/node/pull/53207)
* \[[`728c861b1c`](https://github.com/nodejs/node/commit/728c861b1c)] - **deps**: fix FP16 bitcasts.h (Stefan Stojanovic) [#53134](https://github.com/nodejs/node/pull/53134)
* \[[`52a78737b1`](https://github.com/nodejs/node/commit/52a78737b1)] - **deps**: patch V8 to 12.4.254.19 (Node.js GitHub Bot) [#53094](https://github.com/nodejs/node/pull/53094)
* \[[`4d27b32e58`](https://github.com/nodejs/node/commit/4d27b32e58)] - **deps**: update undici to 6.18.1 (Node.js GitHub Bot) [#53073](https://github.com/nodejs/node/pull/53073)
* \[[`b94199240b`](https://github.com/nodejs/node/commit/b94199240b)] - **deps**: update undici to 6.18.0 (Node.js GitHub Bot) [#53073](https://github.com/nodejs/node/pull/53073)
* \[[`793af1b3e7`](https://github.com/nodejs/node/commit/793af1b3e7)] - **deps**: update undici to 6.17.0 (Node.js GitHub Bot) [#53034](https://github.com/nodejs/node/pull/53034)
* \[[`fe00becc03`](https://github.com/nodejs/node/commit/fe00becc03)] - **deps**: update undici to 6.16.1 (Node.js GitHub Bot) [#52948](https://github.com/nodejs/node/pull/52948)
* \[[`96f72ae54f`](https://github.com/nodejs/node/commit/96f72ae54f)] - **deps**: update undici to 6.15.0 (Matthew Aitken) [#52763](https://github.com/nodejs/node/pull/52763)
* \[[`af60fbb12b`](https://github.com/nodejs/node/commit/af60fbb12b)] - **deps**: update googletest to 33af80a (Node.js GitHub Bot) [#53053](https://github.com/nodejs/node/pull/53053)
* \[[`7b929df489`](https://github.com/nodejs/node/commit/7b929df489)] - **deps**: patch V8 to 12.4.254.18 (Node.js GitHub Bot) [#53054](https://github.com/nodejs/node/pull/53054)
* \[[`626037c0fc`](https://github.com/nodejs/node/commit/626037c0fc)] - **deps**: update zlib to 1.3.0.1-motley-4f653ff (Node.js GitHub Bot) [#53052](https://github.com/nodejs/node/pull/53052)
* \[[`6d8589e558`](https://github.com/nodejs/node/commit/6d8589e558)] - **deps**: patch V8 to 12.4.254.17 (Node.js GitHub Bot) [#52980](https://github.com/nodejs/node/pull/52980)
* \[[`fd91eaab34`](https://github.com/nodejs/node/commit/fd91eaab34)] - **deps**: upgrade npm to 10.8.0 (npm team) [#53014](https://github.com/nodejs/node/pull/53014)
* \[[`133cae0732`](https://github.com/nodejs/node/commit/133cae0732)] - **doc**: fix broken link in `static-analysis.md` (Richard Lau) [#53345](https://github.com/nodejs/node/pull/53345)
* \[[`7bc5f964fd`](https://github.com/nodejs/node/commit/7bc5f964fd)] - **doc**: indicate requirement on VS 17.6 or newer (Chengzhong Wu) [#53301](https://github.com/nodejs/node/pull/53301)
* \[[`8c71522ced`](https://github.com/nodejs/node/commit/8c71522ced)] - **doc**: remove cases for keys not containing "\*" in PATTERN\_KEY\_COMPARE (Maarten Zuidhoorn) [#53215](https://github.com/nodejs/node/pull/53215)
* \[[`718a3ab1ab`](https://github.com/nodejs/node/commit/718a3ab1ab)] - **doc**: add err param to fs.cp callback (Feng Yu) [#53234](https://github.com/nodejs/node/pull/53234)
* \[[`d89bde26ff`](https://github.com/nodejs/node/commit/d89bde26ff)] - **doc**: add `err` param to fs.copyFile callback (Feng Yu) [#53234](https://github.com/nodejs/node/pull/53234)
* \[[`91971ee344`](https://github.com/nodejs/node/commit/91971ee344)] - **doc**: reserve 128 for Electron 32 (Keeley Hammond) [#53203](https://github.com/nodejs/node/pull/53203)
* \[[`812f0e9e14`](https://github.com/nodejs/node/commit/812f0e9e14)] - **doc**: add note to ninjia build for macOS using -jn flag (jakecastelli) [#53187](https://github.com/nodejs/node/pull/53187)
* \[[`048478d351`](https://github.com/nodejs/node/commit/048478d351)] - **(SEMVER-MINOR)** **doc**: add context.assert docs (Colin Ihrig) [#53169](https://github.com/nodejs/node/pull/53169)
* \[[`c391923445`](https://github.com/nodejs/node/commit/c391923445)] - **doc**: include ESM import for HTTP (Aviv Keller) [#53165](https://github.com/nodejs/node/pull/53165)
* \[[`1eb55f3550`](https://github.com/nodejs/node/commit/1eb55f3550)] - **(SEMVER-MINOR)** **doc**: improve explanation about built-in modules (Joyee Cheung) [#52762](https://github.com/nodejs/node/pull/52762)
* \[[`67a766f7d4`](https://github.com/nodejs/node/commit/67a766f7d4)] - **doc**: fix minor grammar and style issues in SECURITY.md (Rich Trott) [#53168](https://github.com/nodejs/node/pull/53168)
* \[[`afbfe8922a`](https://github.com/nodejs/node/commit/afbfe8922a)] - **doc**: mention pm is not enforced when using fd (Rafael Gonzaga) [#53125](https://github.com/nodejs/node/pull/53125)
* \[[`1702d2632e`](https://github.com/nodejs/node/commit/1702d2632e)] - **doc**: fix format in `esm.md` (Pop Moore) [#53170](https://github.com/nodejs/node/pull/53170)
* \[[`070577e7d7`](https://github.com/nodejs/node/commit/070577e7d7)] - **doc**: fix wrong variable name in example of `timers.tick()` (Deokjin Kim) [#53147](https://github.com/nodejs/node/pull/53147)
* \[[`7147c1df1f`](https://github.com/nodejs/node/commit/7147c1df1f)] - **doc**: fix wrong function name in example of `context.plan()` (Deokjin Kim) [#53140](https://github.com/nodejs/node/pull/53140)
* \[[`cf47384148`](https://github.com/nodejs/node/commit/cf47384148)] - **doc**: add note for windows users and symlinks (Aviv Keller) [#53117](https://github.com/nodejs/node/pull/53117)
* \[[`088dff1074`](https://github.com/nodejs/node/commit/088dff1074)] - **doc**: move all TLS-PSK documentation to its section (Alba Mendez) [#35717](https://github.com/nodejs/node/pull/35717)
* \[[`db5dd0c6df`](https://github.com/nodejs/node/commit/db5dd0c6df)] - **doc**: add StefanStojanovic to collaborators (StefanStojanovic) [#53118](https://github.com/nodejs/node/pull/53118)
* \[[`0f0bc98ad7`](https://github.com/nodejs/node/commit/0f0bc98ad7)] - **doc**: improve ninja build for --built-in-modules-path (jakecastelli) [#53007](https://github.com/nodejs/node/pull/53007)
* \[[`4c65c52d30`](https://github.com/nodejs/node/commit/4c65c52d30)] - **doc**: avoid hiding by navigation bar in anchor jumping (Cloyd Lau) [#45131](https://github.com/nodejs/node/pull/45131)
* \[[`63fcbcfd62`](https://github.com/nodejs/node/commit/63fcbcfd62)] - **doc**: remove unavailable youtube link in pull requests (Deokjin Kim) [#52982](https://github.com/nodejs/node/pull/52982)
* \[[`77fd504636`](https://github.com/nodejs/node/commit/77fd504636)] - **doc**: add missing supported timer values in `timers.enable()` (Deokjin Kim) [#52969](https://github.com/nodejs/node/pull/52969)
* \[[`6708536b03`](https://github.com/nodejs/node/commit/6708536b03)] - **fs**: fix cp dir/non-dir mismatch error messages (Mathis Wiehl) [#53150](https://github.com/nodejs/node/pull/53150)
* \[[`6165894774`](https://github.com/nodejs/node/commit/6165894774)] - **fs**: mark recursive cp methods as stable (Théo LUDWIG) [#53127](https://github.com/nodejs/node/pull/53127)
* \[[`7940db7be1`](https://github.com/nodejs/node/commit/7940db7be1)] - **fs**: remove basename in favor of std::filesystem (Yagiz Nizipli) [#53062](https://github.com/nodejs/node/pull/53062)
* \[[`505e9a425b`](https://github.com/nodejs/node/commit/505e9a425b)] - **lib**: fix misleading argument of validateUint32 (Tobias Nießen) [#53307](https://github.com/nodejs/node/pull/53307)
* \[[`98ae1ebdd6`](https://github.com/nodejs/node/commit/98ae1ebdd6)] - **lib**: fix the name of the fetch global function (Gabriel Bota) [#53227](https://github.com/nodejs/node/pull/53227)
* \[[`fe007cd1b4`](https://github.com/nodejs/node/commit/fe007cd1b4)] - **lib**: allow CJS source map cache to be reclaimed (Chengzhong Wu) [#51711](https://github.com/nodejs/node/pull/51711)
* \[[`040be4a7b4`](https://github.com/nodejs/node/commit/040be4a7b4)] - **lib**: do not call callback if socket is closed (theanarkh) [#52829](https://github.com/nodejs/node/pull/52829)
* \[[`ee56aecced`](https://github.com/nodejs/node/commit/ee56aecced)] - **(SEMVER-MINOR)** **lib**: add EventSource Client (Aras Abbasi) [#51575](https://github.com/nodejs/node/pull/51575)
* \[[`6413769bc7`](https://github.com/nodejs/node/commit/6413769bc7)] - **(SEMVER-MINOR)** **lib**: replace MessageEvent with undici's (Matthew Aitken) [#52370](https://github.com/nodejs/node/pull/52370)
* \[[`879679e5a3`](https://github.com/nodejs/node/commit/879679e5a3)] - **lib,doc**: replace references to import assertions (Michaël Zasso) [#52998](https://github.com/nodejs/node/pull/52998)
* \[[`062a0c6f67`](https://github.com/nodejs/node/commit/062a0c6f67)] - **meta**: bump ossf/scorecard-action from 2.3.1 to 2.3.3 (dependabot\[bot]) [#53248](https://github.com/nodejs/node/pull/53248)
* \[[`e59b744b30`](https://github.com/nodejs/node/commit/e59b744b30)] - **meta**: bump actions/checkout from 4.1.4 to 4.1.6 (dependabot\[bot]) [#53247](https://github.com/nodejs/node/pull/53247)
* \[[`96924f48a0`](https://github.com/nodejs/node/commit/96924f48a0)] - **meta**: bump github/codeql-action from 3.25.3 to 3.25.7 (dependabot\[bot]) [#53246](https://github.com/nodejs/node/pull/53246)
* \[[`b7f5662dee`](https://github.com/nodejs/node/commit/b7f5662dee)] - **meta**: bump codecov/codecov-action from 4.3.1 to 4.4.1 (dependabot\[bot]) [#53244](https://github.com/nodejs/node/pull/53244)
* \[[`e079967eb4`](https://github.com/nodejs/node/commit/e079967eb4)] - **meta**: remove `initializeCommand` from devcontainer (Aviv Keller) [#53137](https://github.com/nodejs/node/pull/53137)
* \[[`3afeced572`](https://github.com/nodejs/node/commit/3afeced572)] - **meta**: move one or more collaborators to emeritus (Node.js GitHub Bot) [#53065](https://github.com/nodejs/node/pull/53065)
* \[[`4b9cdea8a6`](https://github.com/nodejs/node/commit/4b9cdea8a6)] - _**Revert**_ "**module**: have a single hooks thread for all workers" (Matteo Collina) [#53183](https://github.com/nodejs/node/pull/53183)
* \[[`b5c30e2f5e`](https://github.com/nodejs/node/commit/b5c30e2f5e)] - **(SEMVER-MINOR)** **module**: print amount of load time of a cjs module (Vinicius Lourenço) [#52213](https://github.com/nodejs/node/pull/52213)
* \[[`4cdb05a7a2`](https://github.com/nodejs/node/commit/4cdb05a7a2)] - **module**: do not set CJS variables for Worker eval (Antoine du Hamel) [#53050](https://github.com/nodejs/node/pull/53050)
* \[[`a0766bdf0e`](https://github.com/nodejs/node/commit/a0766bdf0e)] - **(SEMVER-MINOR)** **net**: add new net.server.listen tracing channel (Paolo Insogna) [#53136](https://github.com/nodejs/node/pull/53136)
* \[[`374743cd4e`](https://github.com/nodejs/node/commit/374743cd4e)] - **(SEMVER-MINOR)** **process**: add process.getBuiltinModule(id) (Joyee Cheung) [#52762](https://github.com/nodejs/node/pull/52762)
* \[[`e66eb376a0`](https://github.com/nodejs/node/commit/e66eb376a0)] - **repl**: fix await object patterns without values (Luke Haas) [#53331](https://github.com/nodejs/node/pull/53331)
* \[[`cb1329a8cf`](https://github.com/nodejs/node/commit/cb1329a8cf)] - **src**: use v8::(Des|S)erializeInternalFieldsCallback (Joyee Cheung) [#53217](https://github.com/nodejs/node/pull/53217)
* \[[`1886fe99af`](https://github.com/nodejs/node/commit/1886fe99af)] - **src**: use \_\_FUNCSIG\_\_ on Windows in backtrace (Joyee Cheung) [#53135](https://github.com/nodejs/node/pull/53135)
* \[[`3bfce6c816`](https://github.com/nodejs/node/commit/3bfce6c816)] - **src**: use new V8 API to define stream accessor (Igor Sheludko) [#53084](https://github.com/nodejs/node/pull/53084)
* \[[`11f790d911`](https://github.com/nodejs/node/commit/11f790d911)] - **src**: do not use deprecated V8 API (ishell) [#53084](https://github.com/nodejs/node/pull/53084)
* \[[`6b1731cbcc`](https://github.com/nodejs/node/commit/6b1731cbcc)] - **src**: convert all endian checks to constexpr (Tobias Nießen) [#52974](https://github.com/nodejs/node/pull/52974)
* \[[`7aa9519ad4`](https://github.com/nodejs/node/commit/7aa9519ad4)] - **src**: fix external module env and kDisableNodeOptionsEnv (Rafael Gonzaga) [#52905](https://github.com/nodejs/node/pull/52905)
* \[[`838fe59787`](https://github.com/nodejs/node/commit/838fe59787)] - **src**: fix execArgv in worker (theanarkh) [#53029](https://github.com/nodejs/node/pull/53029)
* \[[`4a2c6ff05d`](https://github.com/nodejs/node/commit/4a2c6ff05d)] - **src**: reduce unnecessary `GetCwd` calls (Yagiz Nizipli) [#53064](https://github.com/nodejs/node/pull/53064)
* \[[`ec44965b49`](https://github.com/nodejs/node/commit/ec44965b49)] - **src**: simplify node modules traverse path (Yagiz Nizipli) [#53061](https://github.com/nodejs/node/pull/53061)
* \[[`190129b48e`](https://github.com/nodejs/node/commit/190129b48e)] - **src**: remove unused `base64_table_url` (Yagiz Nizipli) [#53040](https://github.com/nodejs/node/pull/53040)
* \[[`d750a3c5c4`](https://github.com/nodejs/node/commit/d750a3c5c4)] - **src**: remove calls to recently deprecated V8 APIs (Adam Klein) [#52996](https://github.com/nodejs/node/pull/52996)
* \[[`f1890abb18`](https://github.com/nodejs/node/commit/f1890abb18)] - **src**: replace deprecated GetImportAssertions V8 API (Michaël Zasso) [#52997](https://github.com/nodejs/node/pull/52997)
* \[[`4347bd2acb`](https://github.com/nodejs/node/commit/4347bd2acb)] - **src**: improve node::Dotenv declarations (Tobias Nießen) [#52973](https://github.com/nodejs/node/pull/52973)
* \[[`e26166f30b`](https://github.com/nodejs/node/commit/e26166f30b)] - **src,permission**: handle process.chdir on pm (Rafael Gonzaga) [#53175](https://github.com/nodejs/node/pull/53175)
* \[[`75e5612fae`](https://github.com/nodejs/node/commit/75e5612fae)] - **(SEMVER-MINOR)** **src,permission**: --allow-wasi & prevent WASI exec (Rafael Gonzaga) [#53124](https://github.com/nodejs/node/pull/53124)
* \[[`7c66b27407`](https://github.com/nodejs/node/commit/7c66b27407)] - **stream**: micro-optimize writable condition (Orgad Shaneh) [#53189](https://github.com/nodejs/node/pull/53189)
* \[[`a656cf6bc8`](https://github.com/nodejs/node/commit/a656cf6bc8)] - **stream**: fix memory usage regression in writable (Orgad Shaneh) [#53188](https://github.com/nodejs/node/pull/53188)
* \[[`0e85a84fdc`](https://github.com/nodejs/node/commit/0e85a84fdc)] - **test**: fix test when compiled without engine support (Richard Lau) [#53232](https://github.com/nodejs/node/pull/53232)
* \[[`cebbd83e47`](https://github.com/nodejs/node/commit/cebbd83e47)] - **test**: update TLS trace tests for OpenSSL >= 3.2 (Richard Lau) [#53229](https://github.com/nodejs/node/pull/53229)
* \[[`45c1eb19f1`](https://github.com/nodejs/node/commit/45c1eb19f1)] - _**Revert**_ "**test**: skip v8-updates/test-linux-perf-logger" (Luke Albao) [#52869](https://github.com/nodejs/node/pull/52869)
* \[[`c1138db3c1`](https://github.com/nodejs/node/commit/c1138db3c1)] - **test**: unskip v8-updates/test-linux-perf-logger (Luke Albao) [#52869](https://github.com/nodejs/node/pull/52869)
* \[[`65b64cf0f1`](https://github.com/nodejs/node/commit/65b64cf0f1)] - **test**: fix Windows native test suites (Stefan Stojanovic) [#53173](https://github.com/nodejs/node/pull/53173)
* \[[`9a47792cd1`](https://github.com/nodejs/node/commit/9a47792cd1)] - **test**: skip `test-setproctitle` when `ps` is not available (Antoine du Hamel) [#53104](https://github.com/nodejs/node/pull/53104)
* \[[`a371dea699`](https://github.com/nodejs/node/commit/a371dea699)] - **test**: increase allocation so it fails for the test (Adam Majer) [#53099](https://github.com/nodejs/node/pull/53099)
* \[[`3ce7a9a1b5`](https://github.com/nodejs/node/commit/3ce7a9a1b5)] - **test**: remove timers from test-tls-socket-close (Luigi Pinca) [#53019](https://github.com/nodejs/node/pull/53019)
* \[[`494fa542af`](https://github.com/nodejs/node/commit/494fa542af)] - **test**: replace `.substr` with `.slice` (Antoine du Hamel) [#53070](https://github.com/nodejs/node/pull/53070)
* \[[`3f7d55b7db`](https://github.com/nodejs/node/commit/3f7d55b7db)] - **test**: add AbortController to knownGlobals (Luigi Pinca) [#53020](https://github.com/nodejs/node/pull/53020)
* \[[`c61f909ab6`](https://github.com/nodejs/node/commit/c61f909ab6)] - **test,doc**: enable running embedtest for Windows (Vladimir Morozov) [#52646](https://github.com/nodejs/node/pull/52646)
* \[[`2d1ecbf827`](https://github.com/nodejs/node/commit/2d1ecbf827)] - **test\_runner**: calculate executed lines using source map (Moshe Atlow) [#53315](https://github.com/nodejs/node/pull/53315)
* \[[`d4f5f80f6c`](https://github.com/nodejs/node/commit/d4f5f80f6c)] - **test\_runner**: handle file rename and deletion under watch mode (jakecastelli) [#53114](https://github.com/nodejs/node/pull/53114)
* \[[`07c601e32f`](https://github.com/nodejs/node/commit/07c601e32f)] - **test\_runner**: refactor to use min/max of `validateInteger` (Deokjin Kim) [#53148](https://github.com/nodejs/node/pull/53148)
* \[[`8c6dffc269`](https://github.com/nodejs/node/commit/8c6dffc269)] - **(SEMVER-MINOR)** **test\_runner**: add snapshot testing (Colin Ihrig) [#53169](https://github.com/nodejs/node/pull/53169)
* \[[`f6d2af8ee7`](https://github.com/nodejs/node/commit/f6d2af8ee7)] - **(SEMVER-MINOR)** **test\_runner**: add context.fullName (Colin Ihrig) [#53169](https://github.com/nodejs/node/pull/53169)
* \[[`7a67ecf161`](https://github.com/nodejs/node/commit/7a67ecf161)] - **(SEMVER-MINOR)** **test\_runner**: support module mocking (Colin Ihrig) [#52848](https://github.com/nodejs/node/pull/52848)
* \[[`3ff174f2bf`](https://github.com/nodejs/node/commit/3ff174f2bf)] - **test\_runner**: fix t.assert methods (Colin Ihrig) [#53049](https://github.com/nodejs/node/pull/53049)
* \[[`e2211a07c2`](https://github.com/nodejs/node/commit/e2211a07c2)] - **test\_runner**: avoid error when coverage line not found (Moshe Atlow) [#53000](https://github.com/nodejs/node/pull/53000)
* \[[`c249289121`](https://github.com/nodejs/node/commit/c249289121)] - **test\_runner,doc**: align documentation with actual stdout/stderr behavior (Moshe Atlow) [#53131](https://github.com/nodejs/node/pull/53131)
* \[[`5110b19a07`](https://github.com/nodejs/node/commit/5110b19a07)] - **tls**: fix negative sessionTimeout handling (Tobias Nießen) [#53002](https://github.com/nodejs/node/pull/53002)
* \[[`0ecb770331`](https://github.com/nodejs/node/commit/0ecb770331)] - **tools**: remove no-goma arg from make-v8 script (Michaël Zasso) [#53336](https://github.com/nodejs/node/pull/53336)
* \[[`e7f3a3c296`](https://github.com/nodejs/node/commit/e7f3a3c296)] - **tools**: use sccache Github action (Moshe Atlow) [#53316](https://github.com/nodejs/node/pull/53316)
* \[[`98cc094bc5`](https://github.com/nodejs/node/commit/98cc094bc5)] - **tools**: update eslint to 9.4.0 (Node.js GitHub Bot) [#53298](https://github.com/nodejs/node/pull/53298)
* \[[`6409b1fe65`](https://github.com/nodejs/node/commit/6409b1fe65)] - **tools**: update gyp-next to 0.18.1 (Node.js GitHub Bot) [#53251](https://github.com/nodejs/node/pull/53251)
* \[[`86e80dcb9b`](https://github.com/nodejs/node/commit/86e80dcb9b)] - **tools**: move webcrypto into no-restricted-properties (Zihong Qu) [#53023](https://github.com/nodejs/node/pull/53023)
* \[[`6022346f0e`](https://github.com/nodejs/node/commit/6022346f0e)] - **tools**: update error message for Type Error (Aviv Keller) [#53047](https://github.com/nodejs/node/pull/53047)
* \[[`c1b3e0ed6f`](https://github.com/nodejs/node/commit/c1b3e0ed6f)] - _**Revert**_ "**tools**: add --certify-safe to nci-ci" (Antoine du Hamel) [#53098](https://github.com/nodejs/node/pull/53098)
* \[[`9f764a873c`](https://github.com/nodejs/node/commit/9f764a873c)] - **tools**: update ESLint to v9 and use flat config (Michaël Zasso) [#52780](https://github.com/nodejs/node/pull/52780)
* \[[`2859f4c027`](https://github.com/nodejs/node/commit/2859f4c027)] - **watch**: fix variable naming (jakecastelli) [#53101](https://github.com/nodejs/node/pull/53101)

<a id="22.2.0"></a>

## 2024-05-15, Version 22.2.0 (Current), @targos

### Notable Changes

* \[[`fb85d38e80`](https://github.com/nodejs/node/commit/fb85d38e80)] - **(SEMVER-MINOR)** **cli**: allow running wasm in limited vmem with --disable-wasm-trap-handler (Joyee Cheung) [#52766](https://github.com/nodejs/node/pull/52766)
* \[[`23a0d3339f`](https://github.com/nodejs/node/commit/23a0d3339f)] - **doc**: add pimterry to collaborators (Tim Perry) [#52874](https://github.com/nodejs/node/pull/52874)
* \[[`7d7a762156`](https://github.com/nodejs/node/commit/7d7a762156)] - **(SEMVER-MINOR)** **fs**: allow 'withFileTypes' to be used with globs (Aviv Keller) [#52837](https://github.com/nodejs/node/pull/52837)
* \[[`8748dd6477`](https://github.com/nodejs/node/commit/8748dd6477)] - **(SEMVER-MINOR)** **inspector**: introduce the `--inspect-wait` flag (Kohei Ueno) [#52734](https://github.com/nodejs/node/pull/52734)
* \[[`9a7ae9b6c4`](https://github.com/nodejs/node/commit/9a7ae9b6c4)] - **lib,src**: remove --experimental-policy (Rafael Gonzaga) [#52583](https://github.com/nodejs/node/pull/52583)
* \[[`1f7c2a93fc`](https://github.com/nodejs/node/commit/1f7c2a93fc)] - **(SEMVER-MINOR)** **perf\_hooks**: add `deliveryType` and `responseStatus` fields (Matthew Aitken) [#51589](https://github.com/nodejs/node/pull/51589)
* \[[`2f59529dc5`](https://github.com/nodejs/node/commit/2f59529dc5)] - **(SEMVER-MINOR)** **test\_runner**: support test plans (Colin Ihrig) [#52860](https://github.com/nodejs/node/pull/52860)
* \[[`6b4dac3eb5`](https://github.com/nodejs/node/commit/6b4dac3eb5)] - **(SEMVER-MINOR)** **zlib**: expose zlib.crc32() (Joyee Cheung) [#52692](https://github.com/nodejs/node/pull/52692)

### Commits

* \[[`0f5716c364`](https://github.com/nodejs/node/commit/0f5716c364)] - **assert**: add deep equal check for more Error type (Zhenwei Jin) [#51805](https://github.com/nodejs/node/pull/51805)
* \[[`2c7d7caa8a`](https://github.com/nodejs/node/commit/2c7d7caa8a)] - **benchmark**: filter non-present deps from `start-cli-version` (Adam Majer) [#51746](https://github.com/nodejs/node/pull/51746)
* \[[`5db4c54bd6`](https://github.com/nodejs/node/commit/5db4c54bd6)] - **bootstrap**: print `--help` message using `console.log` (Jacob Hummer) [#51463](https://github.com/nodejs/node/pull/51463)
* \[[`67fcb6b85e`](https://github.com/nodejs/node/commit/67fcb6b85e)] - **buffer**: even faster atob (Daniel Lemire) [#52443](https://github.com/nodejs/node/pull/52443)
* \[[`a5d63f9052`](https://github.com/nodejs/node/commit/a5d63f9052)] - **buffer**: use size\_t instead of uint32\_t to avoid segmentation fault (Xavier Stouder) [#48033](https://github.com/nodejs/node/pull/48033)
* \[[`f1bc994826`](https://github.com/nodejs/node/commit/f1bc994826)] - **buffer**: remove lines setting indexes to integer value (Zhenwei Jin) [#52588](https://github.com/nodejs/node/pull/52588)
* \[[`a97ff753ab`](https://github.com/nodejs/node/commit/a97ff753ab)] - **build**: add option to enable clang-cl on Windows (Michaël Zasso) [#52870](https://github.com/nodejs/node/pull/52870)
* \[[`f96466a92c`](https://github.com/nodejs/node/commit/f96466a92c)] - **build**: enable building with shared uvwasi lib (Pooja D P) [#43987](https://github.com/nodejs/node/pull/43987)
* \[[`b463385aa8`](https://github.com/nodejs/node/commit/b463385aa8)] - **build**: remove deprecated calls for argument groups (Mohammed Keyvanzadeh) [#52913](https://github.com/nodejs/node/pull/52913)
* \[[`daeb7dbb3e`](https://github.com/nodejs/node/commit/daeb7dbb3e)] - **build**: sync V8 warning cflags with BUILD.gn (Michaël Zasso) [#52873](https://github.com/nodejs/node/pull/52873)
* \[[`eed967430d`](https://github.com/nodejs/node/commit/eed967430d)] - **build**: harmonize Clang checks (Michaël Zasso) [#52873](https://github.com/nodejs/node/pull/52873)
* \[[`e4b187433d`](https://github.com/nodejs/node/commit/e4b187433d)] - **build**: compile with C++20 support (Michaël Zasso) [#52838](https://github.com/nodejs/node/pull/52838)
* \[[`aea6ca25ba`](https://github.com/nodejs/node/commit/aea6ca25ba)] - **build**: drop base64 dep in GN build (Cheng) [#52856](https://github.com/nodejs/node/pull/52856)
* \[[`7f866a8225`](https://github.com/nodejs/node/commit/7f866a8225)] - **build**: make simdjson a public dep in GN build (Cheng) [#52755](https://github.com/nodejs/node/pull/52755)
* \[[`e1bd53c098`](https://github.com/nodejs/node/commit/e1bd53c098)] - **build**: define `NOMINMAX` in common.gypi (Chengzhong Wu) [#52794](https://github.com/nodejs/node/pull/52794)
* \[[`18c530f8f7`](https://github.com/nodejs/node/commit/18c530f8f7)] - **build, tools**: copy release assets to staging R2 bucket once built (flakey5) [#51394](https://github.com/nodejs/node/pull/51394)
* \[[`fb85d38e80`](https://github.com/nodejs/node/commit/fb85d38e80)] - **(SEMVER-MINOR)** **cli**: allow running wasm in limited vmem with --disable-wasm-trap-handler (Joyee Cheung) [#52766](https://github.com/nodejs/node/pull/52766)
* \[[`11e978916f`](https://github.com/nodejs/node/commit/11e978916f)] - **cluster**: replace `forEach` with `for-of` loop (Jérôme Benoit) [#50317](https://github.com/nodejs/node/pull/50317)
* \[[`db76c58d68`](https://github.com/nodejs/node/commit/db76c58d68)] - **console**: colorize console error and warn (Jithil P Ponnan) [#51629](https://github.com/nodejs/node/pull/51629)
* \[[`0d040a3035`](https://github.com/nodejs/node/commit/0d040a3035)] - **crypto**: fix duplicated switch-case return values (Mustafa Ateş UZUN) [#49030](https://github.com/nodejs/node/pull/49030)
* \[[`ab7219f0b2`](https://github.com/nodejs/node/commit/ab7219f0b2)] - **deps**: update googletest to fa6de7f (Node.js GitHub Bot) [#52949](https://github.com/nodejs/node/pull/52949)
* \[[`4ab096eccc`](https://github.com/nodejs/node/commit/4ab096eccc)] - **deps**: update simdjson to 3.9.2 (Node.js GitHub Bot) [#52947](https://github.com/nodejs/node/pull/52947)
* \[[`89f275b1df`](https://github.com/nodejs/node/commit/89f275b1df)] - **deps**: update corepack to 0.28.1 (Node.js GitHub Bot) [#52946](https://github.com/nodejs/node/pull/52946)
* \[[`fc568b4b42`](https://github.com/nodejs/node/commit/fc568b4b42)] - **deps**: update simdutf to 5.2.8 (Node.js GitHub Bot) [#52727](https://github.com/nodejs/node/pull/52727)
* \[[`e399360182`](https://github.com/nodejs/node/commit/e399360182)] - **deps**: update simdutf to 5.2.6 (Node.js GitHub Bot) [#52727](https://github.com/nodejs/node/pull/52727)
* \[[`232831f013`](https://github.com/nodejs/node/commit/232831f013)] - **deps**: enable unbundling of simdjson, simdutf, ada (Daniel Lemire) [#52924](https://github.com/nodejs/node/pull/52924)
* \[[`7ca83a5abc`](https://github.com/nodejs/node/commit/7ca83a5abc)] - **deps**: update googletest to 2d16ed0 (Node.js GitHub Bot) [#51657](https://github.com/nodejs/node/pull/51657)
* \[[`3b15eb5911`](https://github.com/nodejs/node/commit/3b15eb5911)] - **deps**: update googletest to d83fee1 (Node.js GitHub Bot) [#51657](https://github.com/nodejs/node/pull/51657)
* \[[`4190d70035`](https://github.com/nodejs/node/commit/4190d70035)] - **deps**: update googletest to 5a37b51 (Node.js GitHub Bot) [#51657](https://github.com/nodejs/node/pull/51657)
* \[[`7a166a2871`](https://github.com/nodejs/node/commit/7a166a2871)] - **deps**: update googletest to 5197b1a (Node.js GitHub Bot) [#51657](https://github.com/nodejs/node/pull/51657)
* \[[`812dbd749f`](https://github.com/nodejs/node/commit/812dbd749f)] - **deps**: update googletest to eff443c (Node.js GitHub Bot) [#51657](https://github.com/nodejs/node/pull/51657)
* \[[`cb3ae4b9ef`](https://github.com/nodejs/node/commit/cb3ae4b9ef)] - **deps**: update googletest to c231e6f (Node.js GitHub Bot) [#51657](https://github.com/nodejs/node/pull/51657)
* \[[`d97317aaa1`](https://github.com/nodejs/node/commit/d97317aaa1)] - **deps**: update googletest to e4fdb87 (Node.js GitHub Bot) [#51657](https://github.com/nodejs/node/pull/51657)
* \[[`ad8ca1259f`](https://github.com/nodejs/node/commit/ad8ca1259f)] - **deps**: update googletest to 5df0241 (Node.js GitHub Bot) [#51657](https://github.com/nodejs/node/pull/51657)
* \[[`828f0d7096`](https://github.com/nodejs/node/commit/828f0d7096)] - **deps**: update googletest to b75ecf1 (Node.js GitHub Bot) [#51657](https://github.com/nodejs/node/pull/51657)
* \[[`3b60dbcf7b`](https://github.com/nodejs/node/commit/3b60dbcf7b)] - **deps**: update googletest to 4565741 (Node.js GitHub Bot) [#51657](https://github.com/nodejs/node/pull/51657)
* \[[`37098eb880`](https://github.com/nodejs/node/commit/37098eb880)] - **deps**: update simdjson to 3.9.1 (Node.js GitHub Bot) [#52397](https://github.com/nodejs/node/pull/52397)
* \[[`a13cf1c049`](https://github.com/nodejs/node/commit/a13cf1c049)] - **deps**: update uvwasi to 0.0.21 (Node.js GitHub Bot) [#52863](https://github.com/nodejs/node/pull/52863)
* \[[`faf8ada719`](https://github.com/nodejs/node/commit/faf8ada719)] - **deps**: V8: cherry-pick f6bef09b3b0a (Richard Lau) [#52802](https://github.com/nodejs/node/pull/52802)
* \[[`8e5844c2a4`](https://github.com/nodejs/node/commit/8e5844c2a4)] - **doc**: remove reference to AUTHORS file (Marco Ippolito) [#52960](https://github.com/nodejs/node/pull/52960)
* \[[`1f3634e30f`](https://github.com/nodejs/node/commit/1f3634e30f)] - **doc**: update hljs with the latest styles (Aviv Keller) [#52911](https://github.com/nodejs/node/pull/52911)
* \[[`9102255749`](https://github.com/nodejs/node/commit/9102255749)] - **doc**: mention quicker way to build docs (Alex Crawford) [#52937](https://github.com/nodejs/node/pull/52937)
* \[[`15db3ef5fb`](https://github.com/nodejs/node/commit/15db3ef5fb)] - **doc**: mention push.followTags config (Rafael Gonzaga) [#52906](https://github.com/nodejs/node/pull/52906)
* \[[`80fa675af2`](https://github.com/nodejs/node/commit/80fa675af2)] - **doc**: document pipeline with `end` option (Alois Klink) [#48970](https://github.com/nodejs/node/pull/48970)
* \[[`c0000f4118`](https://github.com/nodejs/node/commit/c0000f4118)] - **doc**: add example for `execFileSync` method and ref to stdio (Evan Shortiss) [#39412](https://github.com/nodejs/node/pull/39412)
* \[[`e0148e2653`](https://github.com/nodejs/node/commit/e0148e2653)] - **doc**: add examples and notes to http server.close et al (mary marchini) [#49091](https://github.com/nodejs/node/pull/49091)
* \[[`030f56ee6d`](https://github.com/nodejs/node/commit/030f56ee6d)] - **doc**: fix `dns.lookup` family `0` and `all` descriptions (Adam Jones) [#51653](https://github.com/nodejs/node/pull/51653)
* \[[`a6d624cd5a`](https://github.com/nodejs/node/commit/a6d624cd5a)] - **doc**: update `fs.realpath` documentation (sinkhaha) [#48170](https://github.com/nodejs/node/pull/48170)
* \[[`5dab187ca8`](https://github.com/nodejs/node/commit/5dab187ca8)] - **doc**: update fs read documentation for clarity (Mert Can Altin) [#52453](https://github.com/nodejs/node/pull/52453)
* \[[`5d3ee7205d`](https://github.com/nodejs/node/commit/5d3ee7205d)] - **doc**: watermark string behavior (Benjamin Gruenbaum) [#52842](https://github.com/nodejs/node/pull/52842)
* \[[`2dd8f092a8`](https://github.com/nodejs/node/commit/2dd8f092a8)] - **doc**: exclude commits with baking-for-lts (Marco Ippolito) [#52896](https://github.com/nodejs/node/pull/52896)
* \[[`0c2539b913`](https://github.com/nodejs/node/commit/0c2539b913)] - **doc**: add names next to release key bash commands (Aviv Keller) [#52878](https://github.com/nodejs/node/pull/52878)
* \[[`23a0d3339f`](https://github.com/nodejs/node/commit/23a0d3339f)] - **doc**: add pimterry to collaborators (Tim Perry) [#52874](https://github.com/nodejs/node/pull/52874)
* \[[`15aad62e0c`](https://github.com/nodejs/node/commit/15aad62e0c)] - **doc**: update BUILDING.md previous versions links (Michaël Zasso) [#52852](https://github.com/nodejs/node/pull/52852)
* \[[`f770a993d4`](https://github.com/nodejs/node/commit/f770a993d4)] - **doc**: add more definitions to GLOSSARY.md (Aviv Keller) [#52798](https://github.com/nodejs/node/pull/52798)
* \[[`f35b838a65`](https://github.com/nodejs/node/commit/f35b838a65)] - **doc**: make docs more welcoming and descriptive for newcomers (Serkan Özel) [#38056](https://github.com/nodejs/node/pull/38056)
* \[[`562a019a14`](https://github.com/nodejs/node/commit/562a019a14)] - **doc**: add OpenSSL errors to API docs (John Lamp) [#34213](https://github.com/nodejs/node/pull/34213)
* \[[`0cb7cf7aa9`](https://github.com/nodejs/node/commit/0cb7cf7aa9)] - **doc**: fix grammatical mistake (codershiba) [#52808](https://github.com/nodejs/node/pull/52808)
* \[[`a0147ff8d0`](https://github.com/nodejs/node/commit/a0147ff8d0)] - **doc**: simplify copy-pasting of `branch-diff` commands (Antoine du Hamel) [#52757](https://github.com/nodejs/node/pull/52757)
* \[[`fce31fc829`](https://github.com/nodejs/node/commit/fce31fc829)] - **doc**: add test\_runner to subsystem (Raz Luvaton) [#52774](https://github.com/nodejs/node/pull/52774)
* \[[`ca5607bbc8`](https://github.com/nodejs/node/commit/ca5607bbc8)] - **events**: update MaxListenersExceededWarning message log (sinkhaha) [#51921](https://github.com/nodejs/node/pull/51921)
* \[[`96566fc696`](https://github.com/nodejs/node/commit/96566fc696)] - **events**: add stop propagation flag to `Event.stopImmediatePropagation` (Mickael Meausoone) [#39463](https://github.com/nodejs/node/pull/39463)
* \[[`5ee69243ed`](https://github.com/nodejs/node/commit/5ee69243ed)] - **events**: replace NodeCustomEvent with CustomEvent (Feng Yu) [#43876](https://github.com/nodejs/node/pull/43876)
* \[[`f076e721cb`](https://github.com/nodejs/node/commit/f076e721cb)] - **fs**: keep fs.promises.readFile read until EOF is reached (Zhenwei Jin) [#52178](https://github.com/nodejs/node/pull/52178)
* \[[`7d7a762156`](https://github.com/nodejs/node/commit/7d7a762156)] - **(SEMVER-MINOR)** **fs**: allow 'withFileTypes' to be used with globs (Aviv Keller) [#52837](https://github.com/nodejs/node/pull/52837)
* \[[`ad9c4bddb1`](https://github.com/nodejs/node/commit/ad9c4bddb1)] - **http**: correctly translate HTTP method (Paolo Insogna) [#52701](https://github.com/nodejs/node/pull/52701)
* \[[`8748dd6477`](https://github.com/nodejs/node/commit/8748dd6477)] - **(SEMVER-MINOR)** **inspector**: introduce the `--inspect-wait` flag (Kohei Ueno) [#52734](https://github.com/nodejs/node/pull/52734)
* \[[`9a7ae9b6c4`](https://github.com/nodejs/node/commit/9a7ae9b6c4)] - **lib,src**: remove --experimental-policy (Rafael Gonzaga) [#52583](https://github.com/nodejs/node/pull/52583)
* \[[`a850219600`](https://github.com/nodejs/node/commit/a850219600)] - **meta**: move `@anonrig` to TSC regular member (Yagiz Nizipli) [#52932](https://github.com/nodejs/node/pull/52932)
* \[[`4dc8a387b3`](https://github.com/nodejs/node/commit/4dc8a387b3)] - **meta**: add mailmap entry for legendecas (Chengzhong Wu) [#52795](https://github.com/nodejs/node/pull/52795)
* \[[`d10182d81d`](https://github.com/nodejs/node/commit/d10182d81d)] - **meta**: bump actions/checkout from 4.1.1 to 4.1.4 (dependabot\[bot]) [#52787](https://github.com/nodejs/node/pull/52787)
* \[[`48d0ac0665`](https://github.com/nodejs/node/commit/48d0ac0665)] - **meta**: bump github/codeql-action from 3.24.9 to 3.25.3 (dependabot\[bot]) [#52786](https://github.com/nodejs/node/pull/52786)
* \[[`7c7a25150e`](https://github.com/nodejs/node/commit/7c7a25150e)] - **meta**: bump actions/upload-artifact from 4.3.1 to 4.3.3 (dependabot\[bot]) [#52785](https://github.com/nodejs/node/pull/52785)
* \[[`d9abf18342`](https://github.com/nodejs/node/commit/d9abf18342)] - **meta**: bump actions/download-artifact from 4.1.4 to 4.1.7 (dependabot\[bot]) [#52784](https://github.com/nodejs/node/pull/52784)
* \[[`590e5c6c45`](https://github.com/nodejs/node/commit/590e5c6c45)] - **meta**: bump codecov/codecov-action from 4.1.1 to 4.3.1 (dependabot\[bot]) [#52783](https://github.com/nodejs/node/pull/52783)
* \[[`b3d1720515`](https://github.com/nodejs/node/commit/b3d1720515)] - **meta**: bump step-security/harden-runner from 2.7.0 to 2.7.1 (dependabot\[bot]) [#52782](https://github.com/nodejs/node/pull/52782)
* \[[`f74beb53de`](https://github.com/nodejs/node/commit/f74beb53de)] - **module**: cache synchronous module jobs before linking (Joyee Cheung) [#52868](https://github.com/nodejs/node/pull/52868)
* \[[`8fbf6628d6`](https://github.com/nodejs/node/commit/8fbf6628d6)] - **module**: have a single hooks thread for all workers (Gabriel Bota) [#52706](https://github.com/nodejs/node/pull/52706)
* \[[`609d90bb4b`](https://github.com/nodejs/node/commit/609d90bb4b)] - **path**: fix toNamespacedPath on Windows (Hüseyin Açacak) [#52915](https://github.com/nodejs/node/pull/52915)
* \[[`1f7c2a93fc`](https://github.com/nodejs/node/commit/1f7c2a93fc)] - **(SEMVER-MINOR)** **perf\_hooks**: add `deliveryType` and `responseStatus` fields (Matthew Aitken) [#51589](https://github.com/nodejs/node/pull/51589)
* \[[`0bbc62c42a`](https://github.com/nodejs/node/commit/0bbc62c42a)] - **process**: improve event-loop (Aras Abbasi) [#52108](https://github.com/nodejs/node/pull/52108)
* \[[`619ac79abb`](https://github.com/nodejs/node/commit/619ac79abb)] - **quic**: address coverity warning (Michael Dawson) [#52824](https://github.com/nodejs/node/pull/52824)
* \[[`04de5766ee`](https://github.com/nodejs/node/commit/04de5766ee)] - **repl**: fix disruptive autocomplete without inspector (Nitzan Uziely) [#40661](https://github.com/nodejs/node/pull/40661)
* \[[`663bb973ab`](https://github.com/nodejs/node/commit/663bb973ab)] - **src**: fix Worker termination in `inspector.waitForDebugger` (Daeyeon Jeong) [#52527](https://github.com/nodejs/node/pull/52527)
* \[[`fca38b2d6e`](https://github.com/nodejs/node/commit/fca38b2d6e)] - **src**: use `S_ISDIR` to check if the file is a directory (theanarkh) [#52164](https://github.com/nodejs/node/pull/52164)
* \[[`b228db579f`](https://github.com/nodejs/node/commit/b228db579f)] - **src**: allow preventing debug signal handler start (Shelley Vohr) [#46681](https://github.com/nodejs/node/pull/46681)
* \[[`ace65a9aac`](https://github.com/nodejs/node/commit/ace65a9aac)] - **src**: make sure pass the `argv` to worker threads (theanarkh) [#52827](https://github.com/nodejs/node/pull/52827)
* \[[`75004d32ab`](https://github.com/nodejs/node/commit/75004d32ab)] - **src**: fix typo Unabled -> Unable (Simon Siefke) [#52820](https://github.com/nodejs/node/pull/52820)
* \[[`c40a8273ef`](https://github.com/nodejs/node/commit/c40a8273ef)] - **src**: avoid unused variable 'error' warning (Michaël Zasso) [#52886](https://github.com/nodejs/node/pull/52886)
* \[[`d169d0f181`](https://github.com/nodejs/node/commit/d169d0f181)] - **src**: fix positional args in task runner (Yagiz Nizipli) [#52810](https://github.com/nodejs/node/pull/52810)
* \[[`9c76c95c10`](https://github.com/nodejs/node/commit/9c76c95c10)] - **src**: only apply fix in main thread (Paolo Insogna) [#52702](https://github.com/nodejs/node/pull/52702)
* \[[`e1cba97df3`](https://github.com/nodejs/node/commit/e1cba97df3)] - **src**: fix test local edge case (Paolo Insogna) [#52702](https://github.com/nodejs/node/pull/52702)
* \[[`dc41c135d7`](https://github.com/nodejs/node/commit/dc41c135d7)] - **src**: reduce unnecessary serialization of CLI options in C++ (Joyee Cheung) [#52451](https://github.com/nodejs/node/pull/52451)
* \[[`fb24c4475c`](https://github.com/nodejs/node/commit/fb24c4475c)] - **src**: rewrite task runner in c++ (Yagiz Nizipli) [#52609](https://github.com/nodejs/node/pull/52609)
* \[[`323f95de9e`](https://github.com/nodejs/node/commit/323f95de9e)] - **src**: migrate to new V8 interceptors API (Michaël Zasso) [#52745](https://github.com/nodejs/node/pull/52745)
* \[[`850ff02931`](https://github.com/nodejs/node/commit/850ff02931)] - **src,permission**: resolve path on fs\_permission (Rafael Gonzaga) [#52761](https://github.com/nodejs/node/pull/52761)
* \[[`8d3b0b7ade`](https://github.com/nodejs/node/commit/8d3b0b7ade)] - **stream**: use `ByteLengthQueuingStrategy` when not in `objectMode` (Jason) [#48847](https://github.com/nodejs/node/pull/48847)
* \[[`fa715437b0`](https://github.com/nodejs/node/commit/fa715437b0)] - **stream**: fix util.inspect for compression/decompressionStream (Mert Can Altin) [#52283](https://github.com/nodejs/node/pull/52283)
* \[[`b0e6a6b3d5`](https://github.com/nodejs/node/commit/b0e6a6b3d5)] - **string\_decoder**: throw an error when writing a too long buffer (zhenweijin) [#52215](https://github.com/nodejs/node/pull/52215)
* \[[`e016e952e6`](https://github.com/nodejs/node/commit/e016e952e6)] - **test**: add `Debugger.setInstrumentationBreakpoint` known issue (Konstantin Ulitin) [#31137](https://github.com/nodejs/node/pull/31137)
* \[[`a589de0886`](https://github.com/nodejs/node/commit/a589de0886)] - **test**: use `for-of` instead of `forEach` (Gibby Free) [#49790](https://github.com/nodejs/node/pull/49790)
* \[[`578868ddf8`](https://github.com/nodejs/node/commit/578868ddf8)] - **test**: verify request payload is uploaded consistently (Austin Wright) [#34066](https://github.com/nodejs/node/pull/34066)
* \[[`c676e522e6`](https://github.com/nodejs/node/commit/c676e522e6)] - **test**: add fuzzer for native/js string conversion (Adam Korczynski) [#51120](https://github.com/nodejs/node/pull/51120)
* \[[`5f6415b41d`](https://github.com/nodejs/node/commit/5f6415b41d)] - **test**: add fuzzer for `ClientHelloParser` (AdamKorcz) [#51088](https://github.com/nodejs/node/pull/51088)
* \[[`4d50d51a5e`](https://github.com/nodejs/node/commit/4d50d51a5e)] - **test**: fix broken env fuzzer by initializing process (AdamKorcz) [#51080](https://github.com/nodejs/node/pull/51080)
* \[[`cd00cdcbc8`](https://github.com/nodejs/node/commit/cd00cdcbc8)] - **test**: replace `forEach()` in `test-stream-pipe-unpipe-stream` (Dario) [#50786](https://github.com/nodejs/node/pull/50786)
* \[[`5469adf458`](https://github.com/nodejs/node/commit/5469adf458)] - **test**: test pipeline `end` on transform streams (Alois Klink) [#48970](https://github.com/nodejs/node/pull/48970)
* \[[`ea6070b0e8`](https://github.com/nodejs/node/commit/ea6070b0e8)] - **test**: improve coverage of lib/readline.js (Rongjian Zhang) [#38646](https://github.com/nodejs/node/pull/38646)
* \[[`4f96b00307`](https://github.com/nodejs/node/commit/4f96b00307)] - **test**: updated for each to for of in test file (lyannel) [#50308](https://github.com/nodejs/node/pull/50308)
* \[[`5d91cf1976`](https://github.com/nodejs/node/commit/5d91cf1976)] - **test**: move `test-http-server-request-timeouts-mixed` to sequential (Madhuri) [#45722](https://github.com/nodejs/node/pull/45722)
* \[[`f47e8fccbb`](https://github.com/nodejs/node/commit/f47e8fccbb)] - **test**: fix DNS cancel tests (Szymon Marczak) [#44432](https://github.com/nodejs/node/pull/44432)
* \[[`0b073f885a`](https://github.com/nodejs/node/commit/0b073f885a)] - **test**: add http agent to `executionAsyncResource` (psj-tar-gz) [#34966](https://github.com/nodejs/node/pull/34966)
* \[[`fbce3178ba`](https://github.com/nodejs/node/commit/fbce3178ba)] - **test**: reduce memory usage of test-worker-stdio (Adam Majer) [#37769](https://github.com/nodejs/node/pull/37769)
* \[[`1f8eaec454`](https://github.com/nodejs/node/commit/1f8eaec454)] - **test**: add common.expectRequiredModule() (Joyee Cheung) [#52868](https://github.com/nodejs/node/pull/52868)
* \[[`5e731da572`](https://github.com/nodejs/node/commit/5e731da572)] - **test**: skip unstable shadow realm gc tests (Chengzhong Wu) [#52855](https://github.com/nodejs/node/pull/52855)
* \[[`30a35ae522`](https://github.com/nodejs/node/commit/30a35ae522)] - **test**: crypto-rsa-dsa testing for dynamic openssl (Michael Dawson) [#52781](https://github.com/nodejs/node/pull/52781)
* \[[`968fe6a8b1`](https://github.com/nodejs/node/commit/968fe6a8b1)] - **test**: skip some console tests on dumb terminal (Adam Majer) [#37770](https://github.com/nodejs/node/pull/37770)
* \[[`1448959e0d`](https://github.com/nodejs/node/commit/1448959e0d)] - **test**: skip v8-updates/test-linux-perf-logger (Michaël Zasso) [#52821](https://github.com/nodejs/node/pull/52821)
* \[[`30a4248b48`](https://github.com/nodejs/node/commit/30a4248b48)] - **test**: add env variable test for --run (Yagiz Nizipli) [#52811](https://github.com/nodejs/node/pull/52811)
* \[[`edb4ed3bc9`](https://github.com/nodejs/node/commit/edb4ed3bc9)] - **test**: drop test-crypto-timing-safe-equal-benchmarks (Rafael Gonzaga) [#52751](https://github.com/nodejs/node/pull/52751)
* \[[`944ae598b5`](https://github.com/nodejs/node/commit/944ae598b5)] - **test, crypto**: use correct object on assert (响马) [#51820](https://github.com/nodejs/node/pull/51820)
* \[[`a814e720fa`](https://github.com/nodejs/node/commit/a814e720fa)] - **test\_runner**: fix watch mode race condition (Moshe Atlow) [#52954](https://github.com/nodejs/node/pull/52954)
* \[[`2f59529dc5`](https://github.com/nodejs/node/commit/2f59529dc5)] - **(SEMVER-MINOR)** **test\_runner**: support test plans (Colin Ihrig) [#52860](https://github.com/nodejs/node/pull/52860)
* \[[`3267b3c063`](https://github.com/nodejs/node/commit/3267b3c063)] - **test\_runner**: display failed test stack trace with dot reporter (Mihir Bhansali) [#52655](https://github.com/nodejs/node/pull/52655)
* \[[`b96868b4e7`](https://github.com/nodejs/node/commit/b96868b4e7)] - **test\_runner**: preserve hook promise when executed twice (Moshe Atlow) [#52791](https://github.com/nodejs/node/pull/52791)
* \[[`74341ba3c9`](https://github.com/nodejs/node/commit/74341ba3c9)] - **tools**: fix v8-update workflow (Michaël Zasso) [#52957](https://github.com/nodejs/node/pull/52957)
* \[[`afe39ed0df`](https://github.com/nodejs/node/commit/afe39ed0df)] - **tools**: add --certify-safe to nci-ci (Matteo Collina) [#52940](https://github.com/nodejs/node/pull/52940)
* \[[`bb97e1ccdd`](https://github.com/nodejs/node/commit/bb97e1ccdd)] - **tools**: fix doc update action (Marco Ippolito) [#52890](https://github.com/nodejs/node/pull/52890)
* \[[`c6043fe6c8`](https://github.com/nodejs/node/commit/c6043fe6c8)] - **tools**: fix get\_asan\_state() in tools/test.py (Joyee Cheung) [#52766](https://github.com/nodejs/node/pull/52766)
* \[[`6e71accc5f`](https://github.com/nodejs/node/commit/6e71accc5f)] - **tools**: support max\_virtual\_memory test configuration (Joyee Cheung) [#52766](https://github.com/nodejs/node/pull/52766)
* \[[`1600bdac60`](https://github.com/nodejs/node/commit/1600bdac60)] - **tools**: support != in test status files (Joyee Cheung) [#52766](https://github.com/nodejs/node/pull/52766)
* \[[`8ce23dc9f3`](https://github.com/nodejs/node/commit/8ce23dc9f3)] - **tools**: update gyp-next to 0.18.0 (Node.js GitHub Bot) [#52835](https://github.com/nodejs/node/pull/52835)
* \[[`c5f832adc0`](https://github.com/nodejs/node/commit/c5f832adc0)] - **tools**: update gyp-next to 0.17.0 (Node.js GitHub Bot) [#52835](https://github.com/nodejs/node/pull/52835)
* \[[`646a094782`](https://github.com/nodejs/node/commit/646a094782)] - **tools**: prepare custom rules for ESLint v9 (Michaël Zasso) [#52889](https://github.com/nodejs/node/pull/52889)
* \[[`505566347d`](https://github.com/nodejs/node/commit/505566347d)] - **tools**: update lint-md-dependencies to rollup\@4.17.2 (Node.js GitHub Bot) [#52836](https://github.com/nodejs/node/pull/52836)
* \[[`466e0c1321`](https://github.com/nodejs/node/commit/466e0c1321)] - **tools**: update `gr2m/create-or-update-pull-request-action` (Antoine du Hamel) [#52843](https://github.com/nodejs/node/pull/52843)
* \[[`ce7a751ad1`](https://github.com/nodejs/node/commit/ce7a751ad1)] - **tools**: use sccache GitHub action (Michaël Zasso) [#52839](https://github.com/nodejs/node/pull/52839)
* \[[`1ee38a5ec1`](https://github.com/nodejs/node/commit/1ee38a5ec1)] - **tools**: specify a commit-message for V8 update workflow (Antoine du Hamel) [#52844](https://github.com/nodejs/node/pull/52844)
* \[[`317998a1e8`](https://github.com/nodejs/node/commit/317998a1e8)] - **tools**: fix V8 update workflow (Antoine du Hamel) [#52822](https://github.com/nodejs/node/pull/52822)
* \[[`ef6a2101e2`](https://github.com/nodejs/node/commit/ef6a2101e2)] - **url,tools,benchmark**: replace deprecated `substr()` (Jungku Lee) [#51546](https://github.com/nodejs/node/pull/51546)
* \[[`0deef2d2b1`](https://github.com/nodejs/node/commit/0deef2d2b1)] - **util**: fix `%s` format behavior with `Symbol.toPrimitive` (Chenyu Yang) [#50992](https://github.com/nodejs/node/pull/50992)
* \[[`a42b93b9aa`](https://github.com/nodejs/node/commit/a42b93b9aa)] - **util**: improve `isInsideNodeModules` (uzlopak) [#52147](https://github.com/nodejs/node/pull/52147)
* \[[`d71e16154a`](https://github.com/nodejs/node/commit/d71e16154a)] - **watch**: allow listening for grouped changes (Matthieu Sieben) [#52722](https://github.com/nodejs/node/pull/52722)
* \[[`e895f7cf32`](https://github.com/nodejs/node/commit/e895f7cf32)] - **watch**: enable passthrough ipc in watch mode (Zack) [#50890](https://github.com/nodejs/node/pull/50890)
* \[[`f5d925706a`](https://github.com/nodejs/node/commit/f5d925706a)] - **watch**: fix arguments parsing (Moshe Atlow) [#52760](https://github.com/nodejs/node/pull/52760)
* \[[`6b4dac3eb5`](https://github.com/nodejs/node/commit/6b4dac3eb5)] - **(SEMVER-MINOR)** **zlib**: expose zlib.crc32() (Joyee Cheung) [#52692](https://github.com/nodejs/node/pull/52692)

<a id="22.1.0"></a>

## 2024-05-02, Version 22.1.0 (Current), @targos prepared by @aduh95

### module: implement `NODE_COMPILE_CACHE` for automatic on-disk code caching

This patch implements automatic on-disk code caching that can be enabled
via an environment variable `NODE_COMPILE_CACHE=/path/to/cache/dir`.

When set, whenever Node.js compiles a CommonJS or a ECMAScript Module,
it will use on-disk [V8 code cache](https://v8.dev/blog/code-caching-for-devs)
persisted in the specified directory
to speed up the compilation. This may slow down the first load of a
module graph, but subsequent loads of the same module graph may get
a significant speedup if the contents of the modules do not change.
Locally, this speeds up loading of `test/fixtures/snapshot/typescript.js`
from \~130ms to \~80ms.

To clean up the generated code cache, simply remove the directory.
It will be recreated the next time the same directory is used for
`NODE_COMPILE_CACHE`.

Compilation cache generated by one version of Node.js may not be used
by a different version of Node.js. Cache generated by different versions
of Node.js will be stored separately if the same directory is used
to persist the cache, so they can co-exist.

Caveat: currently when using this with V8 JavaScript code coverage, the
coverage being collected by V8 may be less precise in functions that are
deserialized from the code cache. It's recommended to turn this off when
running tests to generate precise coverage.

Contributed by Joyee Cheung in [#52535](https://github.com/nodejs/node/pull/52535).

### Other Notable Changes

* \[[`44ee04cf9f`](https://github.com/nodejs/node/commit/44ee04cf9f)] - **buffer**: improve `base64` and `base64url` performance (Yagiz Nizipli) [#52428](https://github.com/nodejs/node/pull/52428)
* \[[`3c37ce5710`](https://github.com/nodejs/node/commit/3c37ce5710)] - **(SEMVER-MINOR)** **dns**: add order option and support ipv6first (Paolo Insogna) [#52492](https://github.com/nodejs/node/pull/52492)
* \[[`3026401be1`](https://github.com/nodejs/node/commit/3026401be1)] - **events,doc**: mark CustomEvent as stable (Daeyeon Jeong) [#52618](https://github.com/nodejs/node/pull/52618)
* \[[`64428dc1c9`](https://github.com/nodejs/node/commit/64428dc1c9)] - **(SEMVER-MINOR)** **lib, url**: add a `windows` option to path parsing (Aviv Keller) [#52509](https://github.com/nodejs/node/pull/52509)
* \[[`d79ae74f71`](https://github.com/nodejs/node/commit/d79ae74f71)] - **(SEMVER-MINOR)** **net**: add CLI option for autoSelectFamilyAttemptTimeout (Paolo Insogna) [#52474](https://github.com/nodejs/node/pull/52474)
* \[[`43fa6a1a45`](https://github.com/nodejs/node/commit/43fa6a1a45)] - **(SEMVER-MINOR)** **src**: add `string_view` overload to snapshot FromBlob (Anna Henningsen) [#52595](https://github.com/nodejs/node/pull/52595)
* \[[`c6fe433d42`](https://github.com/nodejs/node/commit/c6fe433d42)] - **src,permission**: throw async errors on async APIs (Rafael Gonzaga) [#52730](https://github.com/nodejs/node/pull/52730)
* \[[`e247a61d15`](https://github.com/nodejs/node/commit/e247a61d15)] - **(SEMVER-MINOR)** **test\_runner**: add --test-skip-pattern cli option (Aviv Keller) [#52529](https://github.com/nodejs/node/pull/52529)
* \[[`9b18df9dcb`](https://github.com/nodejs/node/commit/9b18df9dcb)] - **(SEMVER-MINOR)** **url**: implement parse method for safer URL parsing (Ali Hassan) [#52280](https://github.com/nodejs/node/pull/52280)

### Commits

* \[[`35643c18c0`](https://github.com/nodejs/node/commit/35643c18c0)] - **benchmark**: reduce the buffer size for blob (Debadree Chatterjee) [#52548](https://github.com/nodejs/node/pull/52548)
* \[[`7cdfe8a3fc`](https://github.com/nodejs/node/commit/7cdfe8a3fc)] - **benchmark**: inherit stdio/stderr instead of pipe (Ali Hassan) [#52456](https://github.com/nodejs/node/pull/52456)
* \[[`7b82c17f22`](https://github.com/nodejs/node/commit/7b82c17f22)] - **benchmark**: add ipc support to spawn stdio config (Ali Hassan) [#52456](https://github.com/nodejs/node/pull/52456)
* \[[`dfda6fed61`](https://github.com/nodejs/node/commit/dfda6fed61)] - **buffer**: add missing ARG\_TYPE(ArrayBuffer) for isUtf8 (Jungku Lee) [#52477](https://github.com/nodejs/node/pull/52477)
* \[[`44ee04cf9f`](https://github.com/nodejs/node/commit/44ee04cf9f)] - **buffer**: improve `base64` and `base64url` performance (Yagiz Nizipli) [#52428](https://github.com/nodejs/node/pull/52428)
* \[[`c64a1a3b89`](https://github.com/nodejs/node/commit/c64a1a3b89)] - **build**: fix typo in node.gyp (Michaël Zasso) [#52719](https://github.com/nodejs/node/pull/52719)
* \[[`4f713fbc2e`](https://github.com/nodejs/node/commit/4f713fbc2e)] - **build**: fix headers install for shared mode on Win (Segev Finer) [#52442](https://github.com/nodejs/node/pull/52442)
* \[[`4baeb7b21d`](https://github.com/nodejs/node/commit/4baeb7b21d)] - **build**: fix arm64 cross-compilation bug on non-arm machines (Mahdi Sharifi) [#52559](https://github.com/nodejs/node/pull/52559)
* \[[`d5cd468ce8`](https://github.com/nodejs/node/commit/d5cd468ce8)] - **build,tools,node-api**: fix building node-api tests for Windows Debug (Vladimir Morozov) [#52632](https://github.com/nodejs/node/pull/52632)
* \[[`910533fcfd`](https://github.com/nodejs/node/commit/910533fcfd)] - **crypto**: simplify assertions in Safe\*Print (David Benjamin) [#49709](https://github.com/nodejs/node/pull/49709)
* \[[`61e1ac0b8c`](https://github.com/nodejs/node/commit/61e1ac0b8c)] - **crypto**: enable NODE\_EXTRA\_CA\_CERTS with BoringSSL (Shelley Vohr) [#52217](https://github.com/nodejs/node/pull/52217)
* \[[`6e98eee256`](https://github.com/nodejs/node/commit/6e98eee256)] - **deps**: upgrade npm to 10.7.0 (npm team) [#52767](https://github.com/nodejs/node/pull/52767)
* \[[`27a5f9418c`](https://github.com/nodejs/node/commit/27a5f9418c)] - **deps**: V8: cherry-pick 500de8bd371b (Richard Lau) [#52676](https://github.com/nodejs/node/pull/52676)
* \[[`3b422ddcea`](https://github.com/nodejs/node/commit/3b422ddcea)] - **deps**: update corepack to 0.28.0 (Node.js GitHub Bot) [#52616](https://github.com/nodejs/node/pull/52616)
* \[[`d40e4d4c42`](https://github.com/nodejs/node/commit/d40e4d4c42)] - **deps**: update ada to 2.7.8 (Node.js GitHub Bot) [#52517](https://github.com/nodejs/node/pull/52517)
* \[[`5b52a4870a`](https://github.com/nodejs/node/commit/5b52a4870a)] - **deps**: update icu to 75.1 (Node.js GitHub Bot) [#52573](https://github.com/nodejs/node/pull/52573)
* \[[`80cbe72c1f`](https://github.com/nodejs/node/commit/80cbe72c1f)] - **deps**: update undici to 6.13.0 (Node.js GitHub Bot) [#52493](https://github.com/nodejs/node/pull/52493)
* \[[`9a44059055`](https://github.com/nodejs/node/commit/9a44059055)] - **deps**: update zlib to 1.3.0.1-motley-7d77fb7 (Node.js GitHub Bot) [#52516](https://github.com/nodejs/node/pull/52516)
* \[[`d67a9a5360`](https://github.com/nodejs/node/commit/d67a9a5360)] - **deps**: update minimatch to 9.0.4 (Node.js GitHub Bot) [#52524](https://github.com/nodejs/node/pull/52524)
* \[[`8738b89971`](https://github.com/nodejs/node/commit/8738b89971)] - **deps**: upgrade npm to 10.5.2 (npm team) [#52458](https://github.com/nodejs/node/pull/52458)
* \[[`8e4fd2842b`](https://github.com/nodejs/node/commit/8e4fd2842b)] - **deps,src**: simplify base64 encoding (Daniel Lemire) [#52714](https://github.com/nodejs/node/pull/52714)
* \[[`3c37ce5710`](https://github.com/nodejs/node/commit/3c37ce5710)] - **(SEMVER-MINOR)** **dns**: add order option and support ipv6first (Paolo Insogna) [#52492](https://github.com/nodejs/node/pull/52492)
* \[[`3987a28a9e`](https://github.com/nodejs/node/commit/3987a28a9e)] - **doc**: update process.versions properties (ishabi) [#52736](https://github.com/nodejs/node/pull/52736)
* \[[`c0b58e07f1`](https://github.com/nodejs/node/commit/c0b58e07f1)] - **doc**: remove mold use on mac for speeding up build (Cong Zhang) [#52252](https://github.com/nodejs/node/pull/52252)
* \[[`9a032cf6e2`](https://github.com/nodejs/node/commit/9a032cf6e2)] - **doc**: remove relative limitation to pm (Rafael Gonzaga) [#52648](https://github.com/nodejs/node/pull/52648)
* \[[`90c6e77238`](https://github.com/nodejs/node/commit/90c6e77238)] - **doc**: fix info string causing duplicated code blocks (Mathieu Leenhardt) [#52660](https://github.com/nodejs/node/pull/52660)
* \[[`4d577fa048`](https://github.com/nodejs/node/commit/4d577fa048)] - **doc**: add .gitattributes for md files (Hüseyin Açacak) [#52161](https://github.com/nodejs/node/pull/52161)
* \[[`04c8e110e5`](https://github.com/nodejs/node/commit/04c8e110e5)] - **doc**: run license-builder (github-actions\[bot]) [#52631](https://github.com/nodejs/node/pull/52631)
* \[[`3552829594`](https://github.com/nodejs/node/commit/3552829594)] - **doc**: add info on contributor spotlight program (Michael Dawson) [#52598](https://github.com/nodejs/node/pull/52598)
* \[[`eeb80ad836`](https://github.com/nodejs/node/commit/eeb80ad836)] - **doc**: correct unsafe URL example in http docs (Malte Legenhausen) [#52555](https://github.com/nodejs/node/pull/52555)
* \[[`c83526a688`](https://github.com/nodejs/node/commit/c83526a688)] - **doc**: replace U+00A0 with U+0020 (Luigi Pinca) [#52590](https://github.com/nodejs/node/pull/52590)
* \[[`31831e9db8`](https://github.com/nodejs/node/commit/31831e9db8)] - **doc**: sort options alphabetically (Luigi Pinca) [#52589](https://github.com/nodejs/node/pull/52589)
* \[[`a93f5d4aaa`](https://github.com/nodejs/node/commit/a93f5d4aaa)] - **doc**: correct stream.finished changes (KaKa) [#52551](https://github.com/nodejs/node/pull/52551)
* \[[`27ffa35540`](https://github.com/nodejs/node/commit/27ffa35540)] - **doc**: add RedYetiDev to triage team (Aviv Keller) [#52556](https://github.com/nodejs/node/pull/52556)
* \[[`63cc2b870e`](https://github.com/nodejs/node/commit/63cc2b870e)] - **doc**: fix issue detected in markdown lint update (Rich Trott) [#52566](https://github.com/nodejs/node/pull/52566)
* \[[`7e93c4892b`](https://github.com/nodejs/node/commit/7e93c4892b)] - **doc**: update test runner coverage limitations (Moshe Atlow) [#52515](https://github.com/nodejs/node/pull/52515)
* \[[`3026401be1`](https://github.com/nodejs/node/commit/3026401be1)] - **events,doc**: mark CustomEvent as stable (Daeyeon Jeong) [#52618](https://github.com/nodejs/node/pull/52618)
* \[[`c6e0fe2f22`](https://github.com/nodejs/node/commit/c6e0fe2f22)] - **fs**: allow setting Stat date properties (Nicolò Ribaudo) [#52708](https://github.com/nodejs/node/pull/52708)
* \[[`f23fa1de72`](https://github.com/nodejs/node/commit/f23fa1de72)] - **fs**: fix read / readSync positional offset types (Ruy Adorno) [#52603](https://github.com/nodejs/node/pull/52603)
* \[[`a7e03d301a`](https://github.com/nodejs/node/commit/a7e03d301a)] - **fs**: fixes recursive fs.watch crash on Linux when deleting files (Matteo Collina) [#52349](https://github.com/nodejs/node/pull/52349)
* \[[`d5ecb6cd00`](https://github.com/nodejs/node/commit/d5ecb6cd00)] - **http2**: fix excessive CPU usage when using `allowHTTP1=true` (Eugene) [#52713](https://github.com/nodejs/node/pull/52713)
* \[[`d1adc9b140`](https://github.com/nodejs/node/commit/d1adc9b140)] - **lib**: enforce ASCII order in error code imports (Antoine du Hamel) [#52625](https://github.com/nodejs/node/pull/52625)
* \[[`9ffdcade37`](https://github.com/nodejs/node/commit/9ffdcade37)] - **lib**: use predefined variable instead of bit operation (Deokjin Kim) [#52580](https://github.com/nodejs/node/pull/52580)
* \[[`fdcde845ee`](https://github.com/nodejs/node/commit/fdcde845ee)] - **lib**: refactor lazy loading of undici for fetch method (Victor Chen) [#52275](https://github.com/nodejs/node/pull/52275)
* \[[`f6145aa2ca`](https://github.com/nodejs/node/commit/f6145aa2ca)] - **lib**: convert WeakMaps in cjs loader with private symbol properties (Chengzhong Wu) [#52095](https://github.com/nodejs/node/pull/52095)
* \[[`014bf01efc`](https://github.com/nodejs/node/commit/014bf01efc)] - **lib**: replace string prototype usage with alternatives (Aviv Keller) [#52440](https://github.com/nodejs/node/pull/52440)
* \[[`dc399ddd03`](https://github.com/nodejs/node/commit/dc399ddd03)] - **lib, doc**: rename readme.md to README.md (Aviv Keller) [#52471](https://github.com/nodejs/node/pull/52471)
* \[[`64428dc1c9`](https://github.com/nodejs/node/commit/64428dc1c9)] - **(SEMVER-MINOR)** **lib, url**: add a `windows` option to path parsing (Aviv Keller) [#52509](https://github.com/nodejs/node/pull/52509)
* \[[`9b2b6abb62`](https://github.com/nodejs/node/commit/9b2b6abb62)] - **lib,src**: iterate module requests of a module wrap in JS (Chengzhong Wu) [#52058](https://github.com/nodejs/node/pull/52058)
* \[[`896a80e366`](https://github.com/nodejs/node/commit/896a80e366)] - **meta**: standardize regex (Aviv Keller) [#52693](https://github.com/nodejs/node/pull/52693)
* \[[`20c07e922e`](https://github.com/nodejs/node/commit/20c07e922e)] - **meta**: move one or more collaborators to emeritus (Node.js GitHub Bot) [#52633](https://github.com/nodejs/node/pull/52633)
* \[[`e70d8a4fa9`](https://github.com/nodejs/node/commit/e70d8a4fa9)] - **meta**: move one or more collaborators to emeritus (Node.js GitHub Bot) [#52457](https://github.com/nodejs/node/pull/52457)
* \[[`20ab8f2a88`](https://github.com/nodejs/node/commit/20ab8f2a88)] - **module**: support ESM detection in the CJS loader (Joyee Cheung) [#52047](https://github.com/nodejs/node/pull/52047)
* \[[`544c602b75`](https://github.com/nodejs/node/commit/544c602b75)] - **module**: skip NODE\_COMPILE\_CACHE when policy is enabled (Joyee Cheung) [#52577](https://github.com/nodejs/node/pull/52577)
* \[[`3df3afc284`](https://github.com/nodejs/node/commit/3df3afc284)] - **module**: detect ESM syntax by trying to recompile as SourceTextModule (Joyee Cheung) [#52413](https://github.com/nodejs/node/pull/52413)
* \[[`4d77fd2c46`](https://github.com/nodejs/node/commit/4d77fd2c46)] - **(SEMVER-MINOR)** **module**: implement NODE\_COMPILE\_CACHE for automatic on-disk code caching (Joyee Cheung) [#52535](https://github.com/nodejs/node/pull/52535)
* \[[`9794d21b07`](https://github.com/nodejs/node/commit/9794d21b07)] - **module**: fix submodules loaded by require() and import() (Joyee Cheung) [#52487](https://github.com/nodejs/node/pull/52487)
* \[[`b00766d9e7`](https://github.com/nodejs/node/commit/b00766d9e7)] - **module**: tidy code and comments (Jacob Smith) [#52437](https://github.com/nodejs/node/pull/52437)
* \[[`d79ae74f71`](https://github.com/nodejs/node/commit/d79ae74f71)] - **(SEMVER-MINOR)** **net**: add CLI option for autoSelectFamilyAttemptTimeout (Paolo Insogna) [#52474](https://github.com/nodejs/node/pull/52474)
* \[[`b17cfea289`](https://github.com/nodejs/node/commit/b17cfea289)] - **node-api**: address coverity report (Michael Dawson) [#52584](https://github.com/nodejs/node/pull/52584)
* \[[`1fca8baac1`](https://github.com/nodejs/node/commit/1fca8baac1)] - **node-api**: copy external type tags when they are set (Niels Martignène) [#52426](https://github.com/nodejs/node/pull/52426)
* \[[`d086ab42a1`](https://github.com/nodejs/node/commit/d086ab42a1)] - **quic**: address recent coverity warnings (Michael Dawson) [#52647](https://github.com/nodejs/node/pull/52647)
* \[[`fb4edf70cf`](https://github.com/nodejs/node/commit/fb4edf70cf)] - **quic**: rework TLSContext, additional cleanups (James M Snell) [#51340](https://github.com/nodejs/node/pull/51340)
* \[[`0c58d0319b`](https://github.com/nodejs/node/commit/0c58d0319b)] - **src**: remove misplaced windows code under posix guard in node.cc (Ali Hassan) [#52545](https://github.com/nodejs/node/pull/52545)
* \[[`e20d2f1de3`](https://github.com/nodejs/node/commit/e20d2f1de3)] - **src**: cast to v8::Value before using v8::EmbedderGraph::V8Node (Joyee Cheung) [#52638](https://github.com/nodejs/node/pull/52638)
* \[[`43fa6a1a45`](https://github.com/nodejs/node/commit/43fa6a1a45)] - **(SEMVER-MINOR)** **src**: add `string_view` overload to snapshot FromBlob (Anna Henningsen) [#52595](https://github.com/nodejs/node/pull/52595)
* \[[`a56faff4d0`](https://github.com/nodejs/node/commit/a56faff4d0)] - **src**: parse inspector profiles with simdjson (Joyee Cheung) [#51783](https://github.com/nodejs/node/pull/51783)
* \[[`ac04c6434a`](https://github.com/nodejs/node/commit/ac04c6434a)] - **src**: remove regex usage for env file parsing (IlyasShabi) [#52406](https://github.com/nodejs/node/pull/52406)
* \[[`f283d27285`](https://github.com/nodejs/node/commit/f283d27285)] - **src**: fix loadEnvFile ENOENT error (mathis-west-1) [#52438](https://github.com/nodejs/node/pull/52438)
* \[[`c6fe433d42`](https://github.com/nodejs/node/commit/c6fe433d42)] - **src,permission**: throw async errors on async APIs (Rafael Gonzaga) [#52730](https://github.com/nodejs/node/pull/52730)
* \[[`9f9eca965a`](https://github.com/nodejs/node/commit/9f9eca965a)] - **stream**: update ongoing promise in async iterator return() method (Mattias Buelens) [#52657](https://github.com/nodejs/node/pull/52657)
* \[[`d568a9a38e`](https://github.com/nodejs/node/commit/d568a9a38e)] - **test**: mark `test-error-serdes` as flaky (Antoine du Hamel) [#52739](https://github.com/nodejs/node/pull/52739)
* \[[`45f7002b90`](https://github.com/nodejs/node/commit/45f7002b90)] - **test**: mark test as flaky (Michael Dawson) [#52671](https://github.com/nodejs/node/pull/52671)
* \[[`10596e20e8`](https://github.com/nodejs/node/commit/10596e20e8)] - **test**: fix backtick usage in docs (Aviv Keller) [#52643](https://github.com/nodejs/node/pull/52643)
* \[[`b2f754c9f1`](https://github.com/nodejs/node/commit/b2f754c9f1)] - **test**: skip test-fs-watch-recursive-delete.js on IBM i (Abdirahim Musse) [#52645](https://github.com/nodejs/node/pull/52645)
* \[[`ed080d868d`](https://github.com/nodejs/node/commit/ed080d868d)] - **test**: ensure that all worker servers are ready (Luigi Pinca) [#52563](https://github.com/nodejs/node/pull/52563)
* \[[`c8c61737e4`](https://github.com/nodejs/node/commit/c8c61737e4)] - **test**: fix test-tls-ticket-cluster.js (Hüseyin Açacak) [#52431](https://github.com/nodejs/node/pull/52431)
* \[[`18aa5d6640`](https://github.com/nodejs/node/commit/18aa5d6640)] - **test**: split wasi poll test for windows (Hüseyin Açacak) [#52538](https://github.com/nodejs/node/pull/52538)
* \[[`e34e0a9ba1`](https://github.com/nodejs/node/commit/e34e0a9ba1)] - **test**: write tests for assertIsArray http2 util (Sinan Sonmez (Chaush)) [#52511](https://github.com/nodejs/node/pull/52511)
* \[[`e247a61d15`](https://github.com/nodejs/node/commit/e247a61d15)] - **(SEMVER-MINOR)** **test\_runner**: add --test-skip-pattern cli option (Aviv Keller) [#52529](https://github.com/nodejs/node/pull/52529)
* \[[`e066ba2ae4`](https://github.com/nodejs/node/commit/e066ba2ae4)] - **test\_runner**: better error handing for test hook (Alex Yang) [#52401](https://github.com/nodejs/node/pull/52401)
* \[[`328755341d`](https://github.com/nodejs/node/commit/328755341d)] - **test\_runner**: don't exceed call stack when filtering (Colin Ihrig) [#52488](https://github.com/nodejs/node/pull/52488)
* \[[`b4ccb6c626`](https://github.com/nodejs/node/commit/b4ccb6c626)] - **test\_runner**: move end of work check to finalize() (Colin Ihrig) [#52488](https://github.com/nodejs/node/pull/52488)
* \[[`2ef9380472`](https://github.com/nodejs/node/commit/2ef9380472)] - **tools**: update lint-md-dependencies to rollup\@4.17.0 (Node.js GitHub Bot) [#52729](https://github.com/nodejs/node/pull/52729)
* \[[`db421bdefc`](https://github.com/nodejs/node/commit/db421bdefc)] - **tools**: take co-authors into account in `find-inactive-collaborators` (Antoine du Hamel) [#52669](https://github.com/nodejs/node/pull/52669)
* \[[`01103a85cb`](https://github.com/nodejs/node/commit/01103a85cb)] - **tools**: fix invalid escape sequence in mkssldef (Michaël Zasso) [#52624](https://github.com/nodejs/node/pull/52624)
* \[[`382d951b01`](https://github.com/nodejs/node/commit/382d951b01)] - **tools**: update lint-md-dependencies to rollup\@4.15.0 (Node.js GitHub Bot) [#52617](https://github.com/nodejs/node/pull/52617)
* \[[`f9ddd77ff3`](https://github.com/nodejs/node/commit/f9ddd77ff3)] - **tools**: add lint rule to keep primordials in ASCII order (Antoine du Hamel) [#52592](https://github.com/nodejs/node/pull/52592)
* \[[`552642a498`](https://github.com/nodejs/node/commit/552642a498)] - **tools**: update lint-md-dependencies (Rich Trott) [#52581](https://github.com/nodejs/node/pull/52581)
* \[[`df61feb655`](https://github.com/nodejs/node/commit/df61feb655)] - **tools**: fix heading spaces for osx-entitlements.plist (Jackson Tian) [#52561](https://github.com/nodejs/node/pull/52561)
* \[[`6b4bbfbb1f`](https://github.com/nodejs/node/commit/6b4bbfbb1f)] - **tools**: update lint-md-dependencies to rollup\@4.14.2 vfile-reporter\@8.1.1 (Node.js GitHub Bot) [#52518](https://github.com/nodejs/node/pull/52518)
* \[[`4e5ce3afb7`](https://github.com/nodejs/node/commit/4e5ce3afb7)] - **tools**: use stylistic ESLint plugin for formatting (Michaël Zasso) [#50714](https://github.com/nodejs/node/pull/50714)
* \[[`15c5686381`](https://github.com/nodejs/node/commit/15c5686381)] - **tools**: update minimatch index path (Marco Ippolito) [#52523](https://github.com/nodejs/node/pull/52523)
* \[[`8ae1507ae1`](https://github.com/nodejs/node/commit/8ae1507ae1)] - **tools**: add a linter for README lists (Antoine du Hamel) [#52476](https://github.com/nodejs/node/pull/52476)
* \[[`0b970316bc`](https://github.com/nodejs/node/commit/0b970316bc)] - **typings**: fix invalid JSDoc declarations (Yagiz Nizipli) [#52659](https://github.com/nodejs/node/pull/52659)
* \[[`9b18df9dcb`](https://github.com/nodejs/node/commit/9b18df9dcb)] - **(SEMVER-MINOR)** **url**: implement parse method for safer URL parsing (Ali Hassan) [#52280](https://github.com/nodejs/node/pull/52280)
* \[[`d33131af3a`](https://github.com/nodejs/node/commit/d33131af3a)] - **vm**: fix ASCII-betical order (Aviv Keller) [#52686](https://github.com/nodejs/node/pull/52686)

<a id="22.0.0"></a>

## 2024-04-24, Version 22.0.0 (Current), @RafaelGSS and @marco-ippolito

We're excited to announce the release of Node.js 22!
Highlights include require()ing ESM graphs, WebSocket client, updates of the V8 JavaScript engine, and more!
As a reminder, Node.js 22 will enter long-term support (LTS) in October, but until then, it will be the "Current" release for the next six months.
We encourage you to explore the new features and benefits offered by this latest release and evaluate their potential impact on your applications.

### Other Notable Changes

* \[[`25c79f3331`](https://github.com/nodejs/node/commit/25c79f3331)] - **esm**: drop support for import assertions (Nicolò Ribaudo) [#52104](https://github.com/nodejs/node/pull/52104)
* \[[`818c10e86d`](https://github.com/nodejs/node/commit/818c10e86d)] - **lib**: improve perf of `AbortSignal` creation (Raz Luvaton) [#52408](https://github.com/nodejs/node/pull/52408)
* \[[`4f68c7c1c9`](https://github.com/nodejs/node/commit/4f68c7c1c9)] - **watch**: mark as stable (Moshe Atlow) [#52074](https://github.com/nodejs/node/pull/52074)
* \[[`02b0bc01fe`](https://github.com/nodejs/node/commit/02b0bc01fe)] - **(SEMVER-MAJOR)** **deps**: update V8 to 12.4.254.14 (Michaël Zasso) [#52465](https://github.com/nodejs/node/pull/52465)
* \[[`c975384264`](https://github.com/nodejs/node/commit/c975384264)] - **(SEMVER-MAJOR)** **lib**: enable WebSocket by default (Aras Abbasi) [#51594](https://github.com/nodejs/node/pull/51594)
* \[[`1abff07392`](https://github.com/nodejs/node/commit/1abff07392)] - **(SEMVER-MAJOR)** **stream**: bump default highWaterMark (Robert Nagy) [#52037](https://github.com/nodejs/node/pull/52037)
* \[[`1a5acd0638`](https://github.com/nodejs/node/commit/1a5acd0638)] - **(SEMVER-MAJOR)** **v8**: enable maglev on supported architectures (Keyhan Vakil) [#51360](https://github.com/nodejs/node/pull/51360)
* \[[`128c60d906`](https://github.com/nodejs/node/commit/128c60d906)] - **(SEMVER-MINOR)** **cli**: implement `node --run <script-in-package-json>` (Yagiz Nizipli) [#52190](https://github.com/nodejs/node/pull/52190)
* \[[`151d365ad1`](https://github.com/nodejs/node/commit/151d365ad1)] - **(SEMVER-MINOR)** **fs**: expose glob and globSync (Moshe Atlow) [#51912](https://github.com/nodejs/node/pull/51912)
* \[[`5f7fad2605`](https://github.com/nodejs/node/commit/5f7fad2605)] - **(SEMVER-MINOR)** **module**: support require()ing synchronous ESM graphs (Joyee Cheung) [#51977](https://github.com/nodejs/node/pull/51977)

### Semver-Major Commits

* \[[`2b1e7c2fcb`](https://github.com/nodejs/node/commit/2b1e7c2fcb)] - **(SEMVER-MAJOR)** **build**: compile with C++20 support on Windows (StefanStojanovic) [#52465](https://github.com/nodejs/node/pull/52465)
* \[[`12d00f1479`](https://github.com/nodejs/node/commit/12d00f1479)] - **(SEMVER-MAJOR)** **build**: reset embedder string to "-node.0" (Michaël Zasso) [#52465](https://github.com/nodejs/node/pull/52465)
* \[[`5f08e11a3c`](https://github.com/nodejs/node/commit/5f08e11a3c)] - **(SEMVER-MAJOR)** **build**: reset embedder string to "-node.0" (Michaël Zasso) [#52293](https://github.com/nodejs/node/pull/52293)
* \[[`94f0369d1d`](https://github.com/nodejs/node/commit/94f0369d1d)] - **(SEMVER-MAJOR)** **build**: reset embedder string to "-node.0" (Michaël Zasso) [#51362](https://github.com/nodejs/node/pull/51362)
* \[[`58674cd1d8`](https://github.com/nodejs/node/commit/58674cd1d8)] - **(SEMVER-MAJOR)** **build**: reset embedder string to "-node.0" (Michaël Zasso) [#50115](https://github.com/nodejs/node/pull/50115)
* \[[`60e836427e`](https://github.com/nodejs/node/commit/60e836427e)] - **(SEMVER-MAJOR)** **console**: treat non-strings as separate argument in console.assert() (Jacob Hummer) [#49722](https://github.com/nodejs/node/pull/49722)
* \[[`d62ab3a1ef`](https://github.com/nodejs/node/commit/d62ab3a1ef)] - **(SEMVER-MAJOR)** **crypto**: runtime deprecate hmac constructor (Marco Ippolito) [#52071](https://github.com/nodejs/node/pull/52071)
* \[[`de0602d190`](https://github.com/nodejs/node/commit/de0602d190)] - **(SEMVER-MAJOR)** **crypto**: runtime deprecate Hash constructor (Marco Ippolito) [#51880](https://github.com/nodejs/node/pull/51880)
* \[[`215f4d04b7`](https://github.com/nodejs/node/commit/215f4d04b7)] - **(SEMVER-MAJOR)** **crypto**: move createCipher and createDecipher to eol (Marco Ippolito) [#50973](https://github.com/nodejs/node/pull/50973)
* \[[`30801b8aaf`](https://github.com/nodejs/node/commit/30801b8aaf)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick cd10ad7cdbe5 (Joyee Cheung) [#52465](https://github.com/nodejs/node/pull/52465)
* \[[`521b629ab1`](https://github.com/nodejs/node/commit/521b629ab1)] - **(SEMVER-MAJOR)** **deps**: V8: revert CL 5331688 (Michaël Zasso) [#52465](https://github.com/nodejs/node/pull/52465)
* \[[`3795e97e6c`](https://github.com/nodejs/node/commit/3795e97e6c)] - **(SEMVER-MAJOR)** **deps**: patch V8 to support compilation with MSVC (StefanStojanovic) [#52465](https://github.com/nodejs/node/pull/52465)
* \[[`5bde9e677d`](https://github.com/nodejs/node/commit/5bde9e677d)] - **(SEMVER-MAJOR)** **deps**: silence internal V8 deprecation warning (Michaël Zasso) [#52465](https://github.com/nodejs/node/pull/52465)
* \[[`46e628c6f2`](https://github.com/nodejs/node/commit/46e628c6f2)] - **(SEMVER-MAJOR)** **deps**: patch V8 to avoid duplicated zlib symbol (Michaël Zasso) [#52465](https://github.com/nodejs/node/pull/52465)
* \[[`f824e40a82`](https://github.com/nodejs/node/commit/f824e40a82)] - **(SEMVER-MAJOR)** **deps**: remove usage of a C++20 feature from V8 (Michaël Zasso) [#52465](https://github.com/nodejs/node/pull/52465)
* \[[`d2c84c9a13`](https://github.com/nodejs/node/commit/d2c84c9a13)] - **(SEMVER-MAJOR)** **deps**: avoid compilation error with ASan (Michaël Zasso) [#52465](https://github.com/nodejs/node/pull/52465)
* \[[`95d6045bdb`](https://github.com/nodejs/node/commit/95d6045bdb)] - **(SEMVER-MAJOR)** **deps**: disable V8 concurrent sparkplug compilation (Michaël Zasso) [#52465](https://github.com/nodejs/node/pull/52465)
* \[[`00f55f5743`](https://github.com/nodejs/node/commit/00f55f5743)] - **(SEMVER-MAJOR)** **deps**: silence irrelevant V8 warning (Michaël Zasso) [#52465](https://github.com/nodejs/node/pull/52465)
* \[[`764085aa66`](https://github.com/nodejs/node/commit/764085aa66)] - **(SEMVER-MAJOR)** **deps**: always define V8\_EXPORT\_PRIVATE as no-op (Michaël Zasso) [#52465](https://github.com/nodejs/node/pull/52465)
* \[[`02b0bc01fe`](https://github.com/nodejs/node/commit/02b0bc01fe)] - **(SEMVER-MAJOR)** **deps**: update V8 to 12.4.254.14 (Michaël Zasso) [#52465](https://github.com/nodejs/node/pull/52465)
* \[[`0ec50a19dd`](https://github.com/nodejs/node/commit/0ec50a19dd)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick cd10ad7cdbe5 (Joyee Cheung) [#52293](https://github.com/nodejs/node/pull/52293)
* \[[`021b0b7dee`](https://github.com/nodejs/node/commit/021b0b7dee)] - **(SEMVER-MAJOR)** **deps**: V8: backport c4be0a97f981 (Richard Lau) [#52293](https://github.com/nodejs/node/pull/52293)
* \[[`681aaf85c7`](https://github.com/nodejs/node/commit/681aaf85c7)] - **(SEMVER-MAJOR)** **deps**: silence internal V8 deprecation warning (Michaël Zasso) [#52293](https://github.com/nodejs/node/pull/52293)
* \[[`c563a1c4e4`](https://github.com/nodejs/node/commit/c563a1c4e4)] - **(SEMVER-MAJOR)** **deps**: patch V8 to support compilation with MSVC (Stefan Stojanovic) [#52293](https://github.com/nodejs/node/pull/52293)
* \[[`11e94b9987`](https://github.com/nodejs/node/commit/11e94b9987)] - **(SEMVER-MAJOR)** **deps**: patch V8 to avoid duplicated zlib symbol (Michaël Zasso) [#52293](https://github.com/nodejs/node/pull/52293)
* \[[`856163e23c`](https://github.com/nodejs/node/commit/856163e23c)] - **(SEMVER-MAJOR)** **deps**: remove usage of a C++20 feature from V8 (Michaël Zasso) [#52293](https://github.com/nodejs/node/pull/52293)
* \[[`b530214127`](https://github.com/nodejs/node/commit/b530214127)] - **(SEMVER-MAJOR)** **deps**: avoid compilation error with ASan (Michaël Zasso) [#52293](https://github.com/nodejs/node/pull/52293)
* \[[`8054f69dd9`](https://github.com/nodejs/node/commit/8054f69dd9)] - **(SEMVER-MAJOR)** **deps**: disable V8 concurrent sparkplug compilation (Michaël Zasso) [#52293](https://github.com/nodejs/node/pull/52293)
* \[[`dee908be42`](https://github.com/nodejs/node/commit/dee908be42)] - **(SEMVER-MAJOR)** **deps**: silence irrelevant V8 warning (Michaël Zasso) [#52293](https://github.com/nodejs/node/pull/52293)
* \[[`cf069414ee`](https://github.com/nodejs/node/commit/cf069414ee)] - **(SEMVER-MAJOR)** **deps**: always define V8\_EXPORT\_PRIVATE as no-op (Michaël Zasso) [#52293](https://github.com/nodejs/node/pull/52293)
* \[[`cc5792dd85`](https://github.com/nodejs/node/commit/cc5792dd85)] - **(SEMVER-MAJOR)** **deps**: update V8 to 12.3.219.16 (Michaël Zasso) [#52293](https://github.com/nodejs/node/pull/52293)
* \[[`61a0d3b4c4`](https://github.com/nodejs/node/commit/61a0d3b4c4)] - **(SEMVER-MAJOR)** **deps**: V8: backport c4be0a97f981 (Richard Lau) [#51362](https://github.com/nodejs/node/pull/51362)
* \[[`f55380a725`](https://github.com/nodejs/node/commit/f55380a725)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick f8d5e576b814 (Richard Lau) [#51362](https://github.com/nodejs/node/pull/51362)
* \[[`b9d806a2dd`](https://github.com/nodejs/node/commit/b9d806a2dd)] - **(SEMVER-MAJOR)** **deps**: patch V8 to support compilation with MSVC (StefanStojanovic) [#51362](https://github.com/nodejs/node/pull/51362)
* \[[`63b58bc17b`](https://github.com/nodejs/node/commit/63b58bc17b)] - **(SEMVER-MAJOR)** **deps**: patch V8 to avoid duplicated zlib symbol (Michaël Zasso) [#51362](https://github.com/nodejs/node/pull/51362)
* \[[`86056353c4`](https://github.com/nodejs/node/commit/86056353c4)] - **(SEMVER-MAJOR)** **deps**: remove usage of a C++20 feature from V8 (Michaël Zasso) [#51362](https://github.com/nodejs/node/pull/51362)
* \[[`2e0efc1c8d`](https://github.com/nodejs/node/commit/2e0efc1c8d)] - **(SEMVER-MAJOR)** **deps**: avoid compilation error with ASan (Michaël Zasso) [#51362](https://github.com/nodejs/node/pull/51362)
* \[[`59e6f62e34`](https://github.com/nodejs/node/commit/59e6f62e34)] - **(SEMVER-MAJOR)** **deps**: disable V8 concurrent sparkplug compilation (Michaël Zasso) [#51362](https://github.com/nodejs/node/pull/51362)
* \[[`0423f7e27e`](https://github.com/nodejs/node/commit/0423f7e27e)] - **(SEMVER-MAJOR)** **deps**: silence irrelevant V8 warning (Michaël Zasso) [#51362](https://github.com/nodejs/node/pull/51362)
* \[[`f36620806d`](https://github.com/nodejs/node/commit/f36620806d)] - **(SEMVER-MAJOR)** **deps**: always define V8\_EXPORT\_PRIVATE as no-op (Michaël Zasso) [#51362](https://github.com/nodejs/node/pull/51362)
* \[[`09a8440b45`](https://github.com/nodejs/node/commit/09a8440b45)] - **(SEMVER-MAJOR)** **deps**: update V8 to 12.2.281.27 (Michaël Zasso) [#51362](https://github.com/nodejs/node/pull/51362)
* \[[`0da3beebfc`](https://github.com/nodejs/node/commit/0da3beebfc)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick de611e69ad51 (Keyhan Vakil) [#50115](https://github.com/nodejs/node/pull/50115)
* \[[`b982335637`](https://github.com/nodejs/node/commit/b982335637)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick 0fd478bcdabd (Joyee Cheung) [#50115](https://github.com/nodejs/node/pull/50115)
* \[[`481a90116c`](https://github.com/nodejs/node/commit/481a90116c)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick 0f9ebbc672c7 (Chengzhong Wu) [#50115](https://github.com/nodejs/node/pull/50115)
* \[[`782addbdc3`](https://github.com/nodejs/node/commit/782addbdc3)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick 8f0b94671ddb (Lu Yahan) [#50115](https://github.com/nodejs/node/pull/50115)
* \[[`b682e7f540`](https://github.com/nodejs/node/commit/b682e7f540)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick f7d000a7ae7b (Luke Albao) [#50115](https://github.com/nodejs/node/pull/50115)
* \[[`a60090c52f`](https://github.com/nodejs/node/commit/a60090c52f)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick 25902244ad1a (Joyee Cheung) [#50115](https://github.com/nodejs/node/pull/50115)
* \[[`8441d1fc18`](https://github.com/nodejs/node/commit/8441d1fc18)] - **(SEMVER-MAJOR)** **deps**: patch V8 to avoid duplicated zlib symbol (Michaël Zasso) [#50115](https://github.com/nodejs/node/pull/50115)
* \[[`e8e9bbd7a9`](https://github.com/nodejs/node/commit/e8e9bbd7a9)] - **(SEMVER-MAJOR)** **deps**: remove usage of a C++20 feature from V8 (Michaël Zasso) [#50115](https://github.com/nodejs/node/pull/50115)
* \[[`785d5cd006`](https://github.com/nodejs/node/commit/785d5cd006)] - **(SEMVER-MAJOR)** **deps**: avoid compilation error with ASan (Michaël Zasso) [#50115](https://github.com/nodejs/node/pull/50115)
* \[[`7071c1dafd`](https://github.com/nodejs/node/commit/7071c1dafd)] - **(SEMVER-MAJOR)** **deps**: disable V8 concurrent sparkplug compilation (Michaël Zasso) [#50115](https://github.com/nodejs/node/pull/50115)
* \[[`d1d60b297d`](https://github.com/nodejs/node/commit/d1d60b297d)] - **(SEMVER-MAJOR)** **deps**: silence irrelevant V8 warning (Michaël Zasso) [#50115](https://github.com/nodejs/node/pull/50115)
* \[[`5b240c62f9`](https://github.com/nodejs/node/commit/5b240c62f9)] - **(SEMVER-MAJOR)** **deps**: always define V8\_EXPORT\_PRIVATE as no-op (Michaël Zasso) [#50115](https://github.com/nodejs/node/pull/50115)
* \[[`d8c97e4857`](https://github.com/nodejs/node/commit/d8c97e4857)] - **(SEMVER-MAJOR)** **deps**: update V8 to 11.9.169.7 (Michaël Zasso) [#50115](https://github.com/nodejs/node/pull/50115)
* \[[`b9df88a8c2`](https://github.com/nodejs/node/commit/b9df88a8c2)] - **(SEMVER-MAJOR)** **doc**: runtime deprecate flag --trace-atomics-wait (marco-ippolito) [#51179](https://github.com/nodejs/node/pull/51179)
* \[[`9ba5df30b4`](https://github.com/nodejs/node/commit/9ba5df30b4)] - **(SEMVER-MAJOR)** **doc**: bump FreeBSD experimental support to 13.2 (Michaël Zasso) [#51231](https://github.com/nodejs/node/pull/51231)
* \[[`900d79caf2`](https://github.com/nodejs/node/commit/900d79caf2)] - **(SEMVER-MAJOR)** **doc**: add migration paths for deprecated utils (Marco Ippolito) [#50488](https://github.com/nodejs/node/pull/50488)
* \[[`8206f6bb7f`](https://github.com/nodejs/node/commit/8206f6bb7f)] - **(SEMVER-MAJOR)** **fs**: runtime deprecate fs.Stats constructor (Marco Ippolito) [#52067](https://github.com/nodejs/node/pull/52067)
* \[[`c14133503a`](https://github.com/nodejs/node/commit/c14133503a)] - **(SEMVER-MAJOR)** **fs**: use private fields instead of symbols for `Dir` (Jungku Lee) [#51037](https://github.com/nodejs/node/pull/51037)
* \[[`abbdc3efaa`](https://github.com/nodejs/node/commit/abbdc3efaa)] - **(SEMVER-MAJOR)** **fs**: make stats date fields lazy (Yagiz Nizipli) [#50908](https://github.com/nodejs/node/pull/50908)
* \[[`4b76ccea95`](https://github.com/nodejs/node/commit/4b76ccea95)] - **(SEMVER-MAJOR)** **http**: preserve raw header duplicates in writeHead after setHeader calls (Tim Perry) [#50394](https://github.com/nodejs/node/pull/50394)
* \[[`c975384264`](https://github.com/nodejs/node/commit/c975384264)] - **(SEMVER-MAJOR)** **lib**: enable WebSocket by default (Aras Abbasi) [#51594](https://github.com/nodejs/node/pull/51594)
* \[[`351495e938`](https://github.com/nodejs/node/commit/351495e938)] - **(SEMVER-MAJOR)** **lib,test**: handle new Iterator global (Michaël Zasso) [#51362](https://github.com/nodejs/node/pull/51362)
* \[[`a8b21fdc90`](https://github.com/nodejs/node/commit/a8b21fdc90)] - **(SEMVER-MAJOR)** **process**: wait for `'exit'` before printing result (Antoine du Hamel) [#52172](https://github.com/nodejs/node/pull/52172)
* \[[`582ff5037c`](https://github.com/nodejs/node/commit/582ff5037c)] - **(SEMVER-MAJOR)** **src**: update NODE\_MODULE\_VERSION to 127 (Michaël Zasso) [#52465](https://github.com/nodejs/node/pull/52465)
* \[[`c5c4b50260`](https://github.com/nodejs/node/commit/c5c4b50260)] - **(SEMVER-MAJOR)** **src**: update NODE\_MODULE\_VERSION to 126 (Michaël Zasso) [#52293](https://github.com/nodejs/node/pull/52293)
* \[[`d248639285`](https://github.com/nodejs/node/commit/d248639285)] - **(SEMVER-MAJOR)** **src**: use supported API to get stalled TLA messages (Michaël Zasso) [#51362](https://github.com/nodejs/node/pull/51362)
* \[[`d34b02db4c`](https://github.com/nodejs/node/commit/d34b02db4c)] - **(SEMVER-MAJOR)** **src**: update default V8 platform to override functions with location (Etienne Pierre-Doray) [#51362](https://github.com/nodejs/node/pull/51362)
* \[[`d9c47e9b5f`](https://github.com/nodejs/node/commit/d9c47e9b5f)] - **(SEMVER-MAJOR)** **src**: add missing TryCatch (Michaël Zasso) [#51362](https://github.com/nodejs/node/pull/51362)
* \[[`5cddd3b2d8`](https://github.com/nodejs/node/commit/5cddd3b2d8)] - **(SEMVER-MAJOR)** **src**: update NODE\_MODULE\_VERSION to 124 (Michaël Zasso) [#51362](https://github.com/nodejs/node/pull/51362)
* \[[`1528846ada`](https://github.com/nodejs/node/commit/1528846ada)] - **(SEMVER-MAJOR)** **src**: use non-deprecated v8::Uint8Array::kMaxLength (Michaël Zasso) [#50115](https://github.com/nodejs/node/pull/50115)
* \[[`7166986626`](https://github.com/nodejs/node/commit/7166986626)] - **(SEMVER-MAJOR)** **src**: adapt to v8::Exception API change (Michaël Zasso) [#50115](https://github.com/nodejs/node/pull/50115)
* \[[`4782818020`](https://github.com/nodejs/node/commit/4782818020)] - **(SEMVER-MAJOR)** **src**: use non-deprecated version of CreateSyntheticModule (Michaël Zasso) [#50115](https://github.com/nodejs/node/pull/50115)
* \[[`2cff0ce411`](https://github.com/nodejs/node/commit/2cff0ce411)] - **(SEMVER-MAJOR)** **src**: update NODE\_MODULE\_VERSION to 122 (Michaël Zasso) [#50115](https://github.com/nodejs/node/pull/50115)
* \[[`1abff07392`](https://github.com/nodejs/node/commit/1abff07392)] - **(SEMVER-MAJOR)** **stream**: bump default highWaterMark (Robert Nagy) [#52037](https://github.com/nodejs/node/pull/52037)
* \[[`9efc84a2cb`](https://github.com/nodejs/node/commit/9efc84a2cb)] - **(SEMVER-MAJOR)** **test**: mark test-worker-arraybuffer-zerofill as flaky (Michaël Zasso) [#51362](https://github.com/nodejs/node/pull/51362)
* \[[`84c2e712eb`](https://github.com/nodejs/node/commit/84c2e712eb)] - **(SEMVER-MAJOR)** **test**: mark some GC-related tests as flaky (Michaël Zasso) [#51362](https://github.com/nodejs/node/pull/51362)
* \[[`cdc4437b87`](https://github.com/nodejs/node/commit/cdc4437b87)] - **(SEMVER-MAJOR)** **test**: allow slightly more diff in memory leak test (Michaël Zasso) [#51362](https://github.com/nodejs/node/pull/51362)
* \[[`515b007fae`](https://github.com/nodejs/node/commit/515b007fae)] - **(SEMVER-MAJOR)** **test**: replace always-opt flag with alway-turbofan (Michaël Zasso) [#50115](https://github.com/nodejs/node/pull/50115)
* \[[`2341805eb2`](https://github.com/nodejs/node/commit/2341805eb2)] - **(SEMVER-MAJOR)** **test**: remove tests that create very large buffers (Michaël Zasso) [#50115](https://github.com/nodejs/node/pull/50115)
* \[[`941cef5636`](https://github.com/nodejs/node/commit/941cef5636)] - **(SEMVER-MAJOR)** **test**: adapt to new V8 trusted memory spaces (Michaël Zasso) [#50115](https://github.com/nodejs/node/pull/50115)
* \[[`29de7f82cd`](https://github.com/nodejs/node/commit/29de7f82cd)] - **(SEMVER-MAJOR)** **test\_runner**: omit filtered test from output (Colin Ihrig) [#52221](https://github.com/nodejs/node/pull/52221)
* \[[`00dc6d9d97`](https://github.com/nodejs/node/commit/00dc6d9d97)] - **(SEMVER-MAJOR)** **test\_runner**: improve `--test-name-pattern` to allow matching single test (Michał Drobniak) [#51577](https://github.com/nodejs/node/pull/51577)
* \[[`5def8019d5`](https://github.com/nodejs/node/commit/5def8019d5)] - **(SEMVER-MAJOR)** **tools**: update V8 gypfiles for 12.4 (Michaël Zasso) [#52465](https://github.com/nodejs/node/pull/52465)
* \[[`c22793d050`](https://github.com/nodejs/node/commit/c22793d050)] - **(SEMVER-MAJOR)** **tools**: roughly port v8\_abseil to gyp (Michaël Zasso) [#51362](https://github.com/nodejs/node/pull/51362)
* \[[`ffb0302f0c`](https://github.com/nodejs/node/commit/ffb0302f0c)] - **(SEMVER-MAJOR)** **tools**: update V8 gypfiles for 12.2 (Michaël Zasso) [#51362](https://github.com/nodejs/node/pull/51362)
* \[[`aadea12440`](https://github.com/nodejs/node/commit/aadea12440)] - **(SEMVER-MAJOR)** **tools**: update V8 gypfiles for 12.1 (Michaël Zasso) [#51362](https://github.com/nodejs/node/pull/51362)
* \[[`7784773967`](https://github.com/nodejs/node/commit/7784773967)] - **(SEMVER-MAJOR)** **tools**: update V8 gypfiles for 12.0 (Michaël Zasso) [#51362](https://github.com/nodejs/node/pull/51362)
* \[[`9fe0424baa`](https://github.com/nodejs/node/commit/9fe0424baa)] - **(SEMVER-MAJOR)** **trace\_events**: use private fields instead of symbols for `Tracing` (Jungku Lee) [#51180](https://github.com/nodejs/node/pull/51180)
* \[[`e96cd25007`](https://github.com/nodejs/node/commit/e96cd25007)] - **(SEMVER-MAJOR)** **util**: runtime deprecate util.log (Marco Ippolito) [#50488](https://github.com/nodejs/node/pull/50488)
* \[[`6cf20d5e43`](https://github.com/nodejs/node/commit/6cf20d5e43)] - **(SEMVER-MAJOR)** **util**: runtime deprecate util.isUndefined (Marco Ippolito) [#50488](https://github.com/nodejs/node/pull/50488)
* \[[`09e424921f`](https://github.com/nodejs/node/commit/09e424921f)] - **(SEMVER-MAJOR)** **util**: runtime deprecate util.isSymbol (Marco Ippolito) [#50488](https://github.com/nodejs/node/pull/50488)
* \[[`80b6bfd4e9`](https://github.com/nodejs/node/commit/80b6bfd4e9)] - **(SEMVER-MAJOR)** **util**: runtime deprecate util.isString (Marco Ippolito) [#50488](https://github.com/nodejs/node/pull/50488)
* \[[`d419edded9`](https://github.com/nodejs/node/commit/d419edded9)] - **(SEMVER-MAJOR)** **util**: runtime deprecate util.isRegExp (Marco Ippolito) [#50488](https://github.com/nodejs/node/pull/50488)
* \[[`e0b8de78ed`](https://github.com/nodejs/node/commit/e0b8de78ed)] - **(SEMVER-MAJOR)** **util**: runtime deprecate util.isPrimitive (Marco Ippolito) [#50488](https://github.com/nodejs/node/pull/50488)
* \[[`5478e1129a`](https://github.com/nodejs/node/commit/5478e1129a)] - **(SEMVER-MAJOR)** **util**: runtime deprecate util.isObject (Marco Ippolito) [#50488](https://github.com/nodejs/node/pull/50488)
* \[[`b05b1dd541`](https://github.com/nodejs/node/commit/b05b1dd541)] - **(SEMVER-MAJOR)** **util**: runtime deprecate util.isNumber (Marco Ippolito) [#50488](https://github.com/nodejs/node/pull/50488)
* \[[`5af9bf5f6a`](https://github.com/nodejs/node/commit/5af9bf5f6a)] - **(SEMVER-MAJOR)** **util**: runtime deprecate util.isNullOrUndefined (Marco Ippolito) [#50488](https://github.com/nodejs/node/pull/50488)
* \[[`860a10e10e`](https://github.com/nodejs/node/commit/860a10e10e)] - **(SEMVER-MAJOR)** **util**: runtime deprecate util.isNull (Marco Ippolito) [#50488](https://github.com/nodejs/node/pull/50488)
* \[[`70330f5c2b`](https://github.com/nodejs/node/commit/70330f5c2b)] - **(SEMVER-MAJOR)** **util**: runtime deprecate util.isFunction (Marco Ippolito) [#50488](https://github.com/nodejs/node/pull/50488)
* \[[`7c69c33acc`](https://github.com/nodejs/node/commit/7c69c33acc)] - **(SEMVER-MAJOR)** **util**: runtime deprecate util.isError (Marco Ippolito) [#50488](https://github.com/nodejs/node/pull/50488)
* \[[`a0c5b871a9`](https://github.com/nodejs/node/commit/a0c5b871a9)] - **(SEMVER-MAJOR)** **util**: runtime deprecate util.isDate (Marco Ippolito) [#50488](https://github.com/nodejs/node/pull/50488)
* \[[`3c670cb15d`](https://github.com/nodejs/node/commit/3c670cb15d)] - **(SEMVER-MAJOR)** **util**: runtime deprecation util.isBuffer (Marco Ippolito) [#50488](https://github.com/nodejs/node/pull/50488)
* \[[`c17a448ca9`](https://github.com/nodejs/node/commit/c17a448ca9)] - **(SEMVER-MAJOR)** **util**: runtime deprecation util.isBoolean (Marco Ippolito) [#50488](https://github.com/nodejs/node/pull/50488)
* \[[`fbb2f891aa`](https://github.com/nodejs/node/commit/fbb2f891aa)] - **(SEMVER-MAJOR)** **util**: runtime deprecate util.isArray (Marco Ippolito) [#50488](https://github.com/nodejs/node/pull/50488)
* \[[`22d8062e42`](https://github.com/nodejs/node/commit/22d8062e42)] - **(SEMVER-MAJOR)** **util**: runtime deprecation util.\_extend (Marco Ippolito) [#50488](https://github.com/nodejs/node/pull/50488)
* \[[`1a5acd0638`](https://github.com/nodejs/node/commit/1a5acd0638)] - **(SEMVER-MAJOR)** **v8**: enable maglev on supported architectures (Keyhan Vakil) [#51360](https://github.com/nodejs/node/pull/51360)

### Semver-Minor Commits

* \[[`128c60d906`](https://github.com/nodejs/node/commit/128c60d906)] - **(SEMVER-MINOR)** **cli**: implement `node --run <script-in-package-json>` (Yagiz Nizipli) [#52190](https://github.com/nodejs/node/pull/52190)
* \[[`f69946b905`](https://github.com/nodejs/node/commit/f69946b905)] - **(SEMVER-MINOR)** **deps**: update simdutf to 5.0.0 (Daniel Lemire) [#52138](https://github.com/nodejs/node/pull/52138)
* \[[`828ad42eee`](https://github.com/nodejs/node/commit/828ad42eee)] - **(SEMVER-MINOR)** **deps**: update undici to 6.3.0 (Node.js GitHub Bot) [#51462](https://github.com/nodejs/node/pull/51462)
* \[[`05f8172188`](https://github.com/nodejs/node/commit/05f8172188)] - **(SEMVER-MINOR)** **deps**: update undici to 6.2.1 (Node.js GitHub Bot) [#51278](https://github.com/nodejs/node/pull/51278)
* \[[`a0c466810a`](https://github.com/nodejs/node/commit/a0c466810a)] - **(SEMVER-MINOR)** **doc**: deprecate fs.Stats public constructor (Marco Ippolito) [#51879](https://github.com/nodejs/node/pull/51879)
* \[[`151d365ad1`](https://github.com/nodejs/node/commit/151d365ad1)] - **(SEMVER-MINOR)** **fs**: expose glob and globSync (Moshe Atlow) [#51912](https://github.com/nodejs/node/pull/51912)
* \[[`5f7fad2605`](https://github.com/nodejs/node/commit/5f7fad2605)] - **(SEMVER-MINOR)** **module**: support require()ing synchronous ESM graphs (Joyee Cheung) [#51977](https://github.com/nodejs/node/pull/51977)
* \[[`009665fb56`](https://github.com/nodejs/node/commit/009665fb56)] - **(SEMVER-MINOR)** **report**: add `--report-exclude-network` option (Ethan Arrowood) [#51645](https://github.com/nodejs/node/pull/51645)
* \[[`80f86e5d02`](https://github.com/nodejs/node/commit/80f86e5d02)] - **(SEMVER-MINOR)** **src**: add C++ ProcessEmitWarningSync() (Joyee Cheung) [#51977](https://github.com/nodejs/node/pull/51977)
* \[[`78be0d0f1c`](https://github.com/nodejs/node/commit/78be0d0f1c)] - **(SEMVER-MINOR)** **src**: add uv\_get\_available\_memory to report and process (theanarkh) [#52023](https://github.com/nodejs/node/pull/52023)
* \[[`b34512e38e`](https://github.com/nodejs/node/commit/b34512e38e)] - **(SEMVER-MINOR)** **src**: preload function for Environment (Cheng Zhao) [#51539](https://github.com/nodejs/node/pull/51539)
* \[[`7d258db1d7`](https://github.com/nodejs/node/commit/7d258db1d7)] - **(SEMVER-MINOR)** **stream**: support typed arrays (IlyasShabi) [#51866](https://github.com/nodejs/node/pull/51866)
* \[[`5276c0d5d4`](https://github.com/nodejs/node/commit/5276c0d5d4)] - **(SEMVER-MINOR)** **test\_runner**: add suite() (Colin Ihrig) [#52127](https://github.com/nodejs/node/pull/52127)
* \[[`84de97a61e`](https://github.com/nodejs/node/commit/84de97a61e)] - **(SEMVER-MINOR)** **test\_runner**: support forced exit (Colin Ihrig) [#52038](https://github.com/nodejs/node/pull/52038)
* \[[`aac5ad901d`](https://github.com/nodejs/node/commit/aac5ad901d)] - **(SEMVER-MINOR)** **test\_runner**: add `test:complete` event to reflect execution order (Moshe Atlow) [#51909](https://github.com/nodejs/node/pull/51909)
* \[[`9a1e01c4ce`](https://github.com/nodejs/node/commit/9a1e01c4ce)] - **(SEMVER-MINOR)** **util**: support array of formats in util.styleText (Marco Ippolito) [#52040](https://github.com/nodejs/node/pull/52040)
* \[[`7f2d61f82a`](https://github.com/nodejs/node/commit/7f2d61f82a)] - **(SEMVER-MINOR)** **v8**: implement v8.queryObjects() for memory leak regression testing (Joyee Cheung) [#51927](https://github.com/nodejs/node/pull/51927)
* \[[`d1d5da22e4`](https://github.com/nodejs/node/commit/d1d5da22e4)] - **(SEMVER-MINOR)** **vm**: harden module type checks (Chengzhong Wu) [#52162](https://github.com/nodejs/node/pull/52162)

### Semver-Patch Commits

* \[[`a760dadec3`](https://github.com/nodejs/node/commit/a760dadec3)] - **benchmark**: add AbortSignal.abort benchmarks (Raz Luvaton) [#52408](https://github.com/nodejs/node/pull/52408)
* \[[`47c934e464`](https://github.com/nodejs/node/commit/47c934e464)] - **benchmark**: conditionally use spawn with taskset for cpu pinning (Ali Hassan) [#52253](https://github.com/nodejs/node/pull/52253)
* \[[`dde0cffb2e`](https://github.com/nodejs/node/commit/dde0cffb2e)] - **benchmark**: add toNamespacedPath bench (Rafael Gonzaga) [#52236](https://github.com/nodejs/node/pull/52236)
* \[[`bda66ad711`](https://github.com/nodejs/node/commit/bda66ad711)] - **benchmark**: add style-text benchmark (Rafael Gonzaga) [#52004](https://github.com/nodejs/node/pull/52004)
* \[[`21211a3fa9`](https://github.com/nodejs/node/commit/21211a3fa9)] - **buffer**: improve `btoa` performance (Yagiz Nizipli) [#52427](https://github.com/nodejs/node/pull/52427)
* \[[`6f504b71ac`](https://github.com/nodejs/node/commit/6f504b71ac)] - **buffer**: use simdutf for `atob` implementation (Yagiz Nizipli) [#52381](https://github.com/nodejs/node/pull/52381)
* \[[`0ce7365856`](https://github.com/nodejs/node/commit/0ce7365856)] - **build**: temporary disable ubsan (Rafael Gonzaga) [#52560](https://github.com/nodejs/node/pull/52560)
* \[[`4e278f0253`](https://github.com/nodejs/node/commit/4e278f0253)] - **build**: speed up compilation of some V8 files (Michaël Zasso) [#52083](https://github.com/nodejs/node/pull/52083)
* \[[`ba06c5c509`](https://github.com/nodejs/node/commit/ba06c5c509)] - **build,tools**: add test-ubsan ci (Rafael Gonzaga) [#46297](https://github.com/nodejs/node/pull/46297)
* \[[`562369f348`](https://github.com/nodejs/node/commit/562369f348)] - **child\_process**: use internal addAbortListener (Chemi Atlow) [#52081](https://github.com/nodejs/node/pull/52081)
* \[[`8f61b658de`](https://github.com/nodejs/node/commit/8f61b658de)] - **crypto**: deprecate implicitly shortened GCM tags (Tobias Nießen) [#52345](https://github.com/nodejs/node/pull/52345)
* \[[`08609b5222`](https://github.com/nodejs/node/commit/08609b5222)] - **crypto**: make timingSafeEqual faster for Uint8Array (Tobias Nießen) [#52341](https://github.com/nodejs/node/pull/52341)
* \[[`9f939f5af7`](https://github.com/nodejs/node/commit/9f939f5af7)] - **crypto**: reject Ed25519/Ed448 in Sign/Verify prototypes (Filip Skokan) [#52340](https://github.com/nodejs/node/pull/52340)
* \[[`2241e8c5b3`](https://github.com/nodejs/node/commit/2241e8c5b3)] - **crypto**: validate RSA-PSS saltLength in subtle.sign and subtle.verify (Filip Skokan) [#52262](https://github.com/nodejs/node/pull/52262)
* \[[`6dd1c75f4a`](https://github.com/nodejs/node/commit/6dd1c75f4a)] - **crypto**: fix `input` validation in `crypto.hash` (Antoine du Hamel) [#52070](https://github.com/nodejs/node/pull/52070)
* \[[`a1d48f4a26`](https://github.com/nodejs/node/commit/a1d48f4a26)] - **deps**: update simdutf to 5.2.4 (Node.js GitHub Bot) [#52473](https://github.com/nodejs/node/pull/52473)
* \[[`08ff4a0c9d`](https://github.com/nodejs/node/commit/08ff4a0c9d)] - **deps**: update nghttp2 to 1.61.0 (Node.js GitHub Bot) [#52395](https://github.com/nodejs/node/pull/52395)
* \[[`cf629366b9`](https://github.com/nodejs/node/commit/cf629366b9)] - **deps**: update simdutf to 5.2.3 (Yagiz Nizipli) [#52381](https://github.com/nodejs/node/pull/52381)
* \[[`ad86a12964`](https://github.com/nodejs/node/commit/ad86a12964)] - **deps**: upgrade npm to 10.5.1 (npm team) [#52351](https://github.com/nodejs/node/pull/52351)
* \[[`45cc32c9c6`](https://github.com/nodejs/node/commit/45cc32c9c6)] - **deps**: update c-ares to 1.28.1 (Node.js GitHub Bot) [#52285](https://github.com/nodejs/node/pull/52285)
* \[[`38161c38d9`](https://github.com/nodejs/node/commit/38161c38d9)] - **deps**: update zlib to 1.3.0.1-motley-24c07df (Node.js GitHub Bot) [#52199](https://github.com/nodejs/node/pull/52199)
* \[[`1264414700`](https://github.com/nodejs/node/commit/1264414700)] - **deps**: update simdjson to 3.8.0 (Node.js GitHub Bot) [#52124](https://github.com/nodejs/node/pull/52124)
* \[[`f6996ee150`](https://github.com/nodejs/node/commit/f6996ee150)] - **deps**: V8: backport c4be0a97f981 (Richard Lau) [#52183](https://github.com/nodejs/node/pull/52183)
* \[[`0d4bc4c40e`](https://github.com/nodejs/node/commit/0d4bc4c40e)] - **deps**: V8: cherry-pick f8d5e576b814 (Richard Lau) [#52183](https://github.com/nodejs/node/pull/52183)
* \[[`70a05103c8`](https://github.com/nodejs/node/commit/70a05103c8)] - **deps**: update zlib to 1.3.0.1-motley-24342f6 (Node.js GitHub Bot) [#52123](https://github.com/nodejs/node/pull/52123)
* \[[`4c3e9659ed`](https://github.com/nodejs/node/commit/4c3e9659ed)] - **deps**: update corepack to 0.26.0 (Node.js GitHub Bot) [#52027](https://github.com/nodejs/node/pull/52027)
* \[[`0b4cdb4b42`](https://github.com/nodejs/node/commit/0b4cdb4b42)] - **deps**: update ada to 2.7.7 (Node.js GitHub Bot) [#52028](https://github.com/nodejs/node/pull/52028)
* \[[`b241a1d0ae`](https://github.com/nodejs/node/commit/b241a1d0ae)] - **deps**: update simdutf to 4.0.9 (Node.js GitHub Bot) [#51655](https://github.com/nodejs/node/pull/51655)
* \[[`36dcd399c0`](https://github.com/nodejs/node/commit/36dcd399c0)] - **deps**: upgrade libuv to 1.48.0 (Santiago Gimeno) [#51697](https://github.com/nodejs/node/pull/51697)
* \[[`8cf313cd72`](https://github.com/nodejs/node/commit/8cf313cd72)] - **deps**: update undici to 6.6.0 (Node.js GitHub Bot) [#51630](https://github.com/nodejs/node/pull/51630)
* \[[`dd4767f99f`](https://github.com/nodejs/node/commit/dd4767f99f)] - **deps**: update undici to 6.4.0 (Node.js GitHub Bot) [#51527](https://github.com/nodejs/node/pull/51527)
* \[[`8362caa7d8`](https://github.com/nodejs/node/commit/8362caa7d8)] - **dgram**: use internal addAbortListener (Chemi Atlow) [#52081](https://github.com/nodejs/node/pull/52081)
* \[[`4f3cf4e89a`](https://github.com/nodejs/node/commit/4f3cf4e89a)] - **diagnostics\_channel**: early-exit tracing channel trace methods (Stephen Belanger) [#51915](https://github.com/nodejs/node/pull/51915)
* \[[`204018bba6`](https://github.com/nodejs/node/commit/204018bba6)] - **doc**: deprecate --experimental-policy (RafaelGSS) [#52602](https://github.com/nodejs/node/pull/52602)
* \[[`d32a914ac7`](https://github.com/nodejs/node/commit/d32a914ac7)] - **doc**: add lint-js-fix into BUILDING.md (jakecastelli) [#52290](https://github.com/nodejs/node/pull/52290)
* \[[`411503bacd`](https://github.com/nodejs/node/commit/411503bacd)] - **doc**: remove Internet Explorer mention in BUILDING.md (Rich Trott) [#52455](https://github.com/nodejs/node/pull/52455)
* \[[`e9ccf5aba2`](https://github.com/nodejs/node/commit/e9ccf5aba2)] - **doc**: accommodate upcoming stricter .md linting (Rich Trott) [#52454](https://github.com/nodejs/node/pull/52454)
* \[[`b4186ec2c1`](https://github.com/nodejs/node/commit/b4186ec2c1)] - **doc**: add Rafael to steward list (Rafael Gonzaga) [#52452](https://github.com/nodejs/node/pull/52452)
* \[[`7b01bfb2be`](https://github.com/nodejs/node/commit/7b01bfb2be)] - **doc**: correct naming convention in C++ style guide (Mohammed Keyvanzadeh) [#52424](https://github.com/nodejs/node/pull/52424)
* \[[`c82f3c9e80`](https://github.com/nodejs/node/commit/c82f3c9e80)] - **doc**: update `process.execArg` example to be more useful (Jacob Smith) [#52412](https://github.com/nodejs/node/pull/52412)
* \[[`655b327a4d`](https://github.com/nodejs/node/commit/655b327a4d)] - **doc**: call out http(s).globalAgent default (mathis-west-1) [#52392](https://github.com/nodejs/node/pull/52392)
* \[[`2c77be5488`](https://github.com/nodejs/node/commit/2c77be5488)] - **doc**: update the location of `build_with_cmake` (Emmanuel Ferdman) [#52356](https://github.com/nodejs/node/pull/52356)
* \[[`7dd514f2db`](https://github.com/nodejs/node/commit/7dd514f2db)] - **doc**: reserve 125 for Electron 31 (Shelley Vohr) [#52379](https://github.com/nodejs/node/pull/52379)
* \[[`756acd0877`](https://github.com/nodejs/node/commit/756acd0877)] - **doc**: use consistent plural form of "index" (Rich Trott) [#52373](https://github.com/nodejs/node/pull/52373)
* \[[`ba07e4e5e6`](https://github.com/nodejs/node/commit/ba07e4e5e6)] - **doc**: fix typo in cli.md (Daeyeon Jeong) [#52388](https://github.com/nodejs/node/pull/52388)
* \[[`461d9d665d`](https://github.com/nodejs/node/commit/461d9d665d)] - **doc**: add Rafael to sec release stewards (Rafael Gonzaga) [#52354](https://github.com/nodejs/node/pull/52354)
* \[[`d0c364a844`](https://github.com/nodejs/node/commit/d0c364a844)] - **doc**: document missing options of events.on (Chemi Atlow) [#52080](https://github.com/nodejs/node/pull/52080)
* \[[`a63261cf2c`](https://github.com/nodejs/node/commit/a63261cf2c)] - **doc**: add missing space (Augustin Mauroy) [#52360](https://github.com/nodejs/node/pull/52360)
* \[[`dd711d221a`](https://github.com/nodejs/node/commit/dd711d221a)] - **doc**: add tips about vcpkg cause build faild on windows (Cong Zhang) [#52181](https://github.com/nodejs/node/pull/52181)
* \[[`4df34cf6dd`](https://github.com/nodejs/node/commit/4df34cf6dd)] - **doc**: replace "below" with "following" (Rich Trott) [#52315](https://github.com/nodejs/node/pull/52315)
* \[[`d9aa33fdbf`](https://github.com/nodejs/node/commit/d9aa33fdbf)] - **doc**: fix email pattern to be wrapped with `<<` instead of single `<` (Raz Luvaton) [#52284](https://github.com/nodejs/node/pull/52284)
* \[[`903f28e684`](https://github.com/nodejs/node/commit/903f28e684)] - **doc**: update release gpg keyserver (marco-ippolito) [#52257](https://github.com/nodejs/node/pull/52257)
* \[[`fd55458770`](https://github.com/nodejs/node/commit/fd55458770)] - **doc**: add release key for marco-ippolito (marco-ippolito) [#52257](https://github.com/nodejs/node/pull/52257)
* \[[`27493a1dd7`](https://github.com/nodejs/node/commit/27493a1dd7)] - **doc**: fix arrow vertical alignment in HTML version (Akash Yeole) [#52193](https://github.com/nodejs/node/pull/52193)
* \[[`af48641993`](https://github.com/nodejs/node/commit/af48641993)] - **doc**: move TSC members from regular to emeritus (Michael Dawson) [#52209](https://github.com/nodejs/node/pull/52209)
* \[[`fa13ed6d79`](https://github.com/nodejs/node/commit/fa13ed6d79)] - **doc**: add section explaining todo tests (Colin Ihrig) [#52204](https://github.com/nodejs/node/pull/52204)
* \[[`312ebd97c2`](https://github.com/nodejs/node/commit/312ebd97c2)] - **doc**: edit `ChildProcess` `'message'` event docs (theanarkh) [#52154](https://github.com/nodejs/node/pull/52154)
* \[[`f1635f442f`](https://github.com/nodejs/node/commit/f1635f442f)] - **doc**: quote test\_runner glob parameters (Fabian Meyer) [#52201](https://github.com/nodejs/node/pull/52201)
* \[[`fc029181df`](https://github.com/nodejs/node/commit/fc029181df)] - **doc**: add mold to speeding up section (Cong Zhang) [#52179](https://github.com/nodejs/node/pull/52179)
* \[[`8bd3cb2f8c`](https://github.com/nodejs/node/commit/8bd3cb2f8c)] - **doc**: http event order correction (wh0) [#51464](https://github.com/nodejs/node/pull/51464)
* \[[`a7f170e45a`](https://github.com/nodejs/node/commit/a7f170e45a)] - **doc**: move gabrielschulhof to TSC emeritus (Gabriel Schulhof) [#52192](https://github.com/nodejs/node/pull/52192)
* \[[`305375ac16`](https://github.com/nodejs/node/commit/305375ac16)] - **doc**: fix `--env-file` docs for valid quotes for defining values (Gabriel Bota) [#52157](https://github.com/nodejs/node/pull/52157)
* \[[`3fcaf7b900`](https://github.com/nodejs/node/commit/3fcaf7b900)] - **doc**: clarify what is supported in NODE\_OPTIONS (Michael Dawson) [#52076](https://github.com/nodejs/node/pull/52076)
* \[[`4fe87357f3`](https://github.com/nodejs/node/commit/4fe87357f3)] - **doc**: fix typos in maintaining-dependencies.md (RoboSchmied) [#52160](https://github.com/nodejs/node/pull/52160)
* \[[`f1949ac1ae`](https://github.com/nodejs/node/commit/f1949ac1ae)] - **doc**: add spec for contains module syntax (Geoffrey Booth) [#52059](https://github.com/nodejs/node/pull/52059)
* \[[`707155424b`](https://github.com/nodejs/node/commit/707155424b)] - **doc**: optimize the doc about Unix abstract socket (theanarkh) [#52043](https://github.com/nodejs/node/pull/52043)
* \[[`8a191e4e6a`](https://github.com/nodejs/node/commit/8a191e4e6a)] - **doc**: update pnpm link (Superchupu) [#52113](https://github.com/nodejs/node/pull/52113)
* \[[`454d0806a1`](https://github.com/nodejs/node/commit/454d0806a1)] - **doc**: remove ableist language from crypto (Jamie King) [#52063](https://github.com/nodejs/node/pull/52063)
* \[[`dafe004703`](https://github.com/nodejs/node/commit/dafe004703)] - **doc**: update collaborator email (Ruy Adorno) [#52088](https://github.com/nodejs/node/pull/52088)
* \[[`8824adb031`](https://github.com/nodejs/node/commit/8824adb031)] - **doc**: state that removing npm is a non-goal (Geoffrey Booth) [#51951](https://github.com/nodejs/node/pull/51951)
* \[[`b360532f1a`](https://github.com/nodejs/node/commit/b360532f1a)] - **doc**: mention NodeSource in RafaelGSS steward list (Rafael Gonzaga) [#52057](https://github.com/nodejs/node/pull/52057)
* \[[`57d2e4881c`](https://github.com/nodejs/node/commit/57d2e4881c)] - **doc**: remove ArrayBuffer from crypto.hash() data parameter type (fengmk2) [#52069](https://github.com/nodejs/node/pull/52069)
* \[[`e11c1d2315`](https://github.com/nodejs/node/commit/e11c1d2315)] - **doc**: add some commonly used lables up gront (Michael Dawson) [#52006](https://github.com/nodejs/node/pull/52006)
* \[[`8f9f5db1e8`](https://github.com/nodejs/node/commit/8f9f5db1e8)] - **doc**: document that `const c2 = vm.createContext(c1); c1 === c2` is true (Daniel Kaplan) [#51960](https://github.com/nodejs/node/pull/51960)
* \[[`d78a565713`](https://github.com/nodejs/node/commit/d78a565713)] - **doc**: clarify what moderation issues are for (Antoine du Hamel) [#51990](https://github.com/nodejs/node/pull/51990)
* \[[`4cac07c931`](https://github.com/nodejs/node/commit/4cac07c931)] - **doc**: add Hemanth HM mention to v21.7.0 changelog (Rafael Gonzaga) [#52008](https://github.com/nodejs/node/pull/52008)
* \[[`73025c4dec`](https://github.com/nodejs/node/commit/73025c4dec)] - **doc**: add UlisesGascon as a collaborator (Ulises Gascón) [#51991](https://github.com/nodejs/node/pull/51991)
* \[[`999c6b34fb`](https://github.com/nodejs/node/commit/999c6b34fb)] - **doc**: test for cli options (Aras Abbasi) [#51623](https://github.com/nodejs/node/pull/51623)
* \[[`edd6190836`](https://github.com/nodejs/node/commit/edd6190836)] - **doc**: deprecate hmac public constructor (Marco Ippolito) [#51881](https://github.com/nodejs/node/pull/51881)
* \[[`25c79f3331`](https://github.com/nodejs/node/commit/25c79f3331)] - **esm**: drop support for import assertions (Nicolò Ribaudo) [#52104](https://github.com/nodejs/node/pull/52104)
* \[[`d619aab575`](https://github.com/nodejs/node/commit/d619aab575)] - **events**: rename high & low watermark for consistency (Chemi Atlow) [#52080](https://github.com/nodejs/node/pull/52080)
* \[[`e263946c2e`](https://github.com/nodejs/node/commit/e263946c2e)] - **events**: extract addAbortListener for safe internal use (Chemi Atlow) [#52081](https://github.com/nodejs/node/pull/52081)
* \[[`40ef2da8d6`](https://github.com/nodejs/node/commit/40ef2da8d6)] - **events**: remove abort listener from signal in `on` (Neal Beeken) [#51091](https://github.com/nodejs/node/pull/51091)
* \[[`61e5de1268`](https://github.com/nodejs/node/commit/61e5de1268)] - **fs**: refactor maybeCallback function (Yagiz Nizipli) [#52129](https://github.com/nodejs/node/pull/52129)
* \[[`39f1b899cd`](https://github.com/nodejs/node/commit/39f1b899cd)] - **fs**: fix edge case in readFileSync utf8 fast path (Richard Lau) [#52101](https://github.com/nodejs/node/pull/52101)
* \[[`639c096004`](https://github.com/nodejs/node/commit/639c096004)] - **fs**: validate fd from cpp on `fchown` (Yagiz Nizipli) [#52051](https://github.com/nodejs/node/pull/52051)
* \[[`9ac1fe05d7`](https://github.com/nodejs/node/commit/9ac1fe05d7)] - **fs**: validate fd from cpp on `close` (Yagiz Nizipli) [#52051](https://github.com/nodejs/node/pull/52051)
* \[[`3ec20f25df`](https://github.com/nodejs/node/commit/3ec20f25df)] - **fs**: validate file mode from cpp (Yagiz Nizipli) [#52050](https://github.com/nodejs/node/pull/52050)
* \[[`8c0b723ccb`](https://github.com/nodejs/node/commit/8c0b723ccb)] - **fs,permission**: make handling of buffers consistent (Tobias Nießen) [#52348](https://github.com/nodejs/node/pull/52348)
* \[[`3fc8d2200e`](https://github.com/nodejs/node/commit/3fc8d2200e)] - **http2**: fix h2-over-h2 connection proxying (Tim Perry) [#52368](https://github.com/nodejs/node/pull/52368)
* \[[`b9d8a14a03`](https://github.com/nodejs/node/commit/b9d8a14a03)] - **http2**: use internal addAbortListener (Chemi Atlow) [#52081](https://github.com/nodejs/node/pull/52081)
* \[[`818c10e86d`](https://github.com/nodejs/node/commit/818c10e86d)] - **lib**: improve perf of `AbortSignal` creation (Raz Luvaton) [#52408](https://github.com/nodejs/node/pull/52408)
* \[[`3f5ff8dc20`](https://github.com/nodejs/node/commit/3f5ff8dc20)] - **lib**: .load .save add proper error message when no file passed (Thomas Mauran) [#52225](https://github.com/nodejs/node/pull/52225)
* \[[`0a252c23d9`](https://github.com/nodejs/node/commit/0a252c23d9)] - **lib**: fix type error for \_refreshLine (Jackson Tian) [#52133](https://github.com/nodejs/node/pull/52133)
* \[[`14de082ab4`](https://github.com/nodejs/node/commit/14de082ab4)] - **lib**: emit listening event once when call listen twice (theanarkh) [#52119](https://github.com/nodejs/node/pull/52119)
* \[[`4e9ce7c035`](https://github.com/nodejs/node/commit/4e9ce7c035)] - **lib**: make sure clear the old timer in http server (theanarkh) [#52118](https://github.com/nodejs/node/pull/52118)
* \[[`20525f14b9`](https://github.com/nodejs/node/commit/20525f14b9)] - **lib**: fix listen with handle in cluster worker (theanarkh) [#52056](https://github.com/nodejs/node/pull/52056)
* \[[`8df54481f4`](https://github.com/nodejs/node/commit/8df54481f4)] - **meta**: bump actions/download-artifact from 4.1.3 to 4.1.4 (dependabot\[bot]) [#52314](https://github.com/nodejs/node/pull/52314)
* \[[`bcc102147a`](https://github.com/nodejs/node/commit/bcc102147a)] - **meta**: bump rtCamp/action-slack-notify from 2.2.1 to 2.3.0 (dependabot\[bot]) [#52313](https://github.com/nodejs/node/pull/52313)
* \[[`4e7e0ef9c3`](https://github.com/nodejs/node/commit/4e7e0ef9c3)] - **meta**: bump github/codeql-action from 3.24.6 to 3.24.9 (dependabot\[bot]) [#52312](https://github.com/nodejs/node/pull/52312)
* \[[`14a39881b8`](https://github.com/nodejs/node/commit/14a39881b8)] - **meta**: bump actions/cache from 4.0.1 to 4.0.2 (dependabot\[bot]) [#52311](https://github.com/nodejs/node/pull/52311)
* \[[`2f8f90dadb`](https://github.com/nodejs/node/commit/2f8f90dadb)] - **meta**: bump actions/setup-python from 5.0.0 to 5.1.0 (dependabot\[bot]) [#52310](https://github.com/nodejs/node/pull/52310)
* \[[`95efdaf01a`](https://github.com/nodejs/node/commit/95efdaf01a)] - **meta**: bump codecov/codecov-action from 4.1.0 to 4.1.1 (dependabot\[bot]) [#52308](https://github.com/nodejs/node/pull/52308)
* \[[`24c1a8e739`](https://github.com/nodejs/node/commit/24c1a8e739)] - **meta**: move one or more collaborators to emeritus (Node.js GitHub Bot) [#52300](https://github.com/nodejs/node/pull/52300)
* \[[`60dcfad91e`](https://github.com/nodejs/node/commit/60dcfad91e)] - **meta**: pass Codecov upload token to codecov action (Michaël Zasso) [#51982](https://github.com/nodejs/node/pull/51982)
* \[[`db1746182b`](https://github.com/nodejs/node/commit/db1746182b)] - **module**: disallow CJS <-> ESM edges in a cycle from require(esm) (Joyee Cheung) [#52264](https://github.com/nodejs/node/pull/52264)
* \[[`d6b57f6629`](https://github.com/nodejs/node/commit/d6b57f6629)] - **module**: centralize SourceTextModule compilation for builtin loader (Joyee Cheung) [#52291](https://github.com/nodejs/node/pull/52291)
* \[[`f4a0a3b04b`](https://github.com/nodejs/node/commit/f4a0a3b04b)] - **module**: warn on detection in typeless package (Geoffrey Booth) [#52168](https://github.com/nodejs/node/pull/52168)
* \[[`8bc745944e`](https://github.com/nodejs/node/commit/8bc745944e)] - **module**: eliminate performance cost of detection for cjs entry (Geoffrey Booth) [#52093](https://github.com/nodejs/node/pull/52093)
* \[[`63d04d4d80`](https://github.com/nodejs/node/commit/63d04d4d80)] - **module**: fix detect-module not retrying as esm for cjs-only errors (Geoffrey Booth) [#52024](https://github.com/nodejs/node/pull/52024)
* \[[`575ced8139`](https://github.com/nodejs/node/commit/575ced8139)] - **module**: print location of unsettled top-level await in entry points (Joyee Cheung) [#51999](https://github.com/nodejs/node/pull/51999)
* \[[`075c95f61f`](https://github.com/nodejs/node/commit/075c95f61f)] - **module**: refactor ESM loader initialization and entry point handling (Joyee Cheung) [#51999](https://github.com/nodejs/node/pull/51999)
* \[[`45f0dd0192`](https://github.com/nodejs/node/commit/45f0dd0192)] - **module,win**: fix long path resolve (Stefan Stojanovic) [#51097](https://github.com/nodejs/node/pull/51097)
* \[[`d89fc73d45`](https://github.com/nodejs/node/commit/d89fc73d45)] - **net**: use internal addAbortListener (Chemi Atlow) [#52081](https://github.com/nodejs/node/pull/52081)
* \[[`f0e6acde2d`](https://github.com/nodejs/node/commit/f0e6acde2d)] - **node-api**: make tsfn accept napi\_finalize once more (Gabriel Schulhof) [#51801](https://github.com/nodejs/node/pull/51801)
* \[[`ff93f3e1a8`](https://github.com/nodejs/node/commit/ff93f3e1a8)] - **readline**: use internal addAbortListener (Chemi Atlow) [#52081](https://github.com/nodejs/node/pull/52081)
* \[[`4a6ca7a1d4`](https://github.com/nodejs/node/commit/4a6ca7a1d4)] - **src**: remove erroneous CVE-2024-27980 revert option (Tobias Nießen) [#52543](https://github.com/nodejs/node/pull/52543)
* \[[`64b67779f7`](https://github.com/nodejs/node/commit/64b67779f7)] - **src**: disallow direct .bat and .cmd file spawning (Ben Noordhuis) [nodejs-private/node-private#560](https://github.com/nodejs-private/node-private/pull/560)
* \[[`9ef724bc81`](https://github.com/nodejs/node/commit/9ef724bc81)] - **src**: update branch name in node\_revert.h (Tobias Nießen) [#52390](https://github.com/nodejs/node/pull/52390)
* \[[`ec1550407b`](https://github.com/nodejs/node/commit/ec1550407b)] - **src**: stop using `v8::BackingStore::Reallocate` (Michaël Zasso) [#52292](https://github.com/nodejs/node/pull/52292)
* \[[`681b0a3df3`](https://github.com/nodejs/node/commit/681b0a3df3)] - **src**: address coverity warning in module\_wrap.cc (Michael Dawson) [#52143](https://github.com/nodejs/node/pull/52143)
* \[[`04319228e0`](https://github.com/nodejs/node/commit/04319228e0)] - **src**: fix move after use reported by coverity (Michael Dawson) [#52141](https://github.com/nodejs/node/pull/52141)
* \[[`0eb2b727f6`](https://github.com/nodejs/node/commit/0eb2b727f6)] - **src**: return a number from process.constrainedMemory() constantly (Chengzhong Wu) [#52039](https://github.com/nodejs/node/pull/52039)
* \[[`bec9b5fccc`](https://github.com/nodejs/node/commit/bec9b5fccc)] - **src**: use dedicated routine to compile function for builtin CJS loader (Joyee Cheung) [#52016](https://github.com/nodejs/node/pull/52016)
* \[[`1f193165b9`](https://github.com/nodejs/node/commit/1f193165b9)] - **src**: fix reading empty string views in Blob\[De]serializer (Joyee Cheung) [#52000](https://github.com/nodejs/node/pull/52000)
* \[[`fb356b3305`](https://github.com/nodejs/node/commit/fb356b3305)] - **src**: refactor out FormatErrorMessage for error formatting (Joyee Cheung) [#51999](https://github.com/nodejs/node/pull/51999)
* \[[`1a8ae9d6c0`](https://github.com/nodejs/node/commit/1a8ae9d6c0)] - **src**: use callback-based array iteration in Blob (Joyee Cheung) [#51758](https://github.com/nodejs/node/pull/51758)
* \[[`5cd2ec8bd5`](https://github.com/nodejs/node/commit/5cd2ec8bd5)] - **src**: implement v8 array iteration using the new callback-based API (Joyee Cheung) [#51758](https://github.com/nodejs/node/pull/51758)
* \[[`89a26b451e`](https://github.com/nodejs/node/commit/89a26b451e)] - **src**: fix node\_version.h (Joyee Cheung) [#50375](https://github.com/nodejs/node/pull/50375)
* \[[`c02de658a1`](https://github.com/nodejs/node/commit/c02de658a1)] - **stream**: make Duplex inherit destroy from Writable (Luigi Pinca) [#52318](https://github.com/nodejs/node/pull/52318)
* \[[`63391e749d`](https://github.com/nodejs/node/commit/63391e749d)] - **stream**: add `new` when constructing `ERR_MULTIPLE_CALLBACK` (haze) [#52110](https://github.com/nodejs/node/pull/52110)
* \[[`a9528e87b9`](https://github.com/nodejs/node/commit/a9528e87b9)] - **stream**: use internal addAbortListener (Chemi Atlow) [#52081](https://github.com/nodejs/node/pull/52081)
* \[[`ee4fa77624`](https://github.com/nodejs/node/commit/ee4fa77624)] - **test**: fix watch test with require not testing pid (Raz Luvaton) [#52353](https://github.com/nodejs/node/pull/52353)
* \[[`05cb16dc1a`](https://github.com/nodejs/node/commit/05cb16dc1a)] - **test**: simplify ASan build checks (Michaël Zasso) [#52430](https://github.com/nodejs/node/pull/52430)
* \[[`eb53121b77`](https://github.com/nodejs/node/commit/eb53121b77)] - **test**: fix Windows compiler warnings in overlapped-checker (Michaël Zasso) [#52405](https://github.com/nodejs/node/pull/52405)
* \[[`7dfa4750af`](https://github.com/nodejs/node/commit/7dfa4750af)] - **test**: add test for skip+todo combinations (Colin Ihrig) [#52204](https://github.com/nodejs/node/pull/52204)
* \[[`5905596719`](https://github.com/nodejs/node/commit/5905596719)] - **test**: fix incorrect test fixture (Colin Ihrig) [#52185](https://github.com/nodejs/node/pull/52185)
* \[[`bae14b7914`](https://github.com/nodejs/node/commit/bae14b7914)] - **test**: do not set concurrency on parallelized runs (Antoine du Hamel) [#52177](https://github.com/nodejs/node/pull/52177)
* \[[`0b676736a0`](https://github.com/nodejs/node/commit/0b676736a0)] - **test**: add missing cctest/test\_path.cc (Yagiz Nizipli) [#52148](https://github.com/nodejs/node/pull/52148)
* \[[`c714cda9a7`](https://github.com/nodejs/node/commit/c714cda9a7)] - **test**: add `spawnSyncAndAssert` util (Antoine du Hamel) [#52132](https://github.com/nodejs/node/pull/52132)
* \[[`978d5a26c9`](https://github.com/nodejs/node/commit/978d5a26c9)] - **test**: reduce flakiness of test-runner-output.mjs (Colin Ihrig) [#52146](https://github.com/nodejs/node/pull/52146)
* \[[`afaf889775`](https://github.com/nodejs/node/commit/afaf889775)] - **test**: skip test for dynamically linked OpenSSL (Richard Lau) [#52542](https://github.com/nodejs/node/pull/52542)
* \[[`be75821a12`](https://github.com/nodejs/node/commit/be75821a12)] - **test**: add test for using `--print` with promises (Antoine du Hamel) [#52137](https://github.com/nodejs/node/pull/52137)
* \[[`4e109e5958`](https://github.com/nodejs/node/commit/4e109e5958)] - **test**: un-set test-emit-after-on-destroyed as flaky (Abdirahim Musse) [#51995](https://github.com/nodejs/node/pull/51995)
* \[[`3f8cc88009`](https://github.com/nodejs/node/commit/3f8cc88009)] - **test\_runner**: fix clearing final timeout in own callback (Ben Richeson) [#52332](https://github.com/nodejs/node/pull/52332)
* \[[`52f8dcfccc`](https://github.com/nodejs/node/commit/52f8dcfccc)] - **test\_runner**: make end of work check stricter (Colin Ihrig) [#52326](https://github.com/nodejs/node/pull/52326)
* \[[`433bd1b04d`](https://github.com/nodejs/node/commit/433bd1b04d)] - **test\_runner**: fix recursive run (Moshe Atlow) [#52322](https://github.com/nodejs/node/pull/52322)
* \[[`e57992ffb2`](https://github.com/nodejs/node/commit/e57992ffb2)] - **test\_runner**: hide new line when no error in spec reporter (Moshe Atlow) [#52297](https://github.com/nodejs/node/pull/52297)
* \[[`ac9e5e7527`](https://github.com/nodejs/node/commit/ac9e5e7527)] - **test\_runner**: improve describe.only behavior (Moshe Atlow) [#52296](https://github.com/nodejs/node/pull/52296)
* \[[`2c024cd24d`](https://github.com/nodejs/node/commit/2c024cd24d)] - **test\_runner**: disable highWatermark on TestsStream (Colin Ihrig) [#52287](https://github.com/nodejs/node/pull/52287)
* \[[`7c02486f1f`](https://github.com/nodejs/node/commit/7c02486f1f)] - **test\_runner**: run afterEach hooks in correct order (Colin Ihrig) [#52239](https://github.com/nodejs/node/pull/52239)
* \[[`6af4049810`](https://github.com/nodejs/node/commit/6af4049810)] - **test\_runner**: simplify test end time tracking (Colin Ihrig) [#52182](https://github.com/nodejs/node/pull/52182)
* \[[`878047be0b`](https://github.com/nodejs/node/commit/878047be0b)] - **test\_runner**: simplify test start time tracking (Colin Ihrig) [#52182](https://github.com/nodejs/node/pull/52182)
* \[[`4648c83dbc`](https://github.com/nodejs/node/commit/4648c83dbc)] - **test\_runner**: don't await the same promise for each test (Colin Ihrig) [#52185](https://github.com/nodejs/node/pull/52185)
* \[[`f9755f6f79`](https://github.com/nodejs/node/commit/f9755f6f79)] - **test\_runner**: emit diagnostics when watch mode drains (Moshe Atlow) [#52130](https://github.com/nodejs/node/pull/52130)
* \[[`4ba9f45d99`](https://github.com/nodejs/node/commit/4ba9f45d99)] - **test\_runner**: ignore todo flag when running suites (Colin Ihrig) [#52117](https://github.com/nodejs/node/pull/52117)
* \[[`6f4d6011ea`](https://github.com/nodejs/node/commit/6f4d6011ea)] - **test\_runner**: skip each hooks for skipped tests (Colin Ihrig) [#52115](https://github.com/nodejs/node/pull/52115)
* \[[`05db979c01`](https://github.com/nodejs/node/commit/05db979c01)] - **test\_runner**: run top level tests in a microtask (Colin Ihrig) [#52092](https://github.com/nodejs/node/pull/52092)
* \[[`97b2c5344d`](https://github.com/nodejs/node/commit/97b2c5344d)] - **test\_runner**: remove redundant report call (Colin Ihrig) [#52089](https://github.com/nodejs/node/pull/52089)
* \[[`780d030bdf`](https://github.com/nodejs/node/commit/780d030bdf)] - **test\_runner**: use internal addAbortListener (Chemi Atlow) [#52081](https://github.com/nodejs/node/pull/52081)
* \[[`814fa1ae74`](https://github.com/nodejs/node/commit/814fa1ae74)] - **test\_runner**: use source maps when reporting coverage (Moshe Atlow) [#52060](https://github.com/nodejs/node/pull/52060)
* \[[`3c5764a0e2`](https://github.com/nodejs/node/commit/3c5764a0e2)] - **test\_runner**: handle undefined test locations (Colin Ihrig) [#52036](https://github.com/nodejs/node/pull/52036)
* \[[`328642bbb9`](https://github.com/nodejs/node/commit/328642bbb9)] - **test\_runner**: use paths for test locations (Colin Ihrig) [#52010](https://github.com/nodejs/node/pull/52010)
* \[[`6d625fe616`](https://github.com/nodejs/node/commit/6d625fe616)] - **test\_runner**: support source mapped test locations (Colin Ihrig) [#52010](https://github.com/nodejs/node/pull/52010)
* \[[`592c6907bf`](https://github.com/nodejs/node/commit/592c6907bf)] - **test\_runner**: avoid overwriting root start time (Colin Ihrig) [#52020](https://github.com/nodejs/node/pull/52020)
* \[[`29b231763e`](https://github.com/nodejs/node/commit/29b231763e)] - **test\_runner**: abort unfinished tests on async error (Colin Ihrig) [#51996](https://github.com/nodejs/node/pull/51996)
* \[[`5d13419dbd`](https://github.com/nodejs/node/commit/5d13419dbd)] - **test\_runner**: run before hook immediately if test started (Moshe Atlow) [#52003](https://github.com/nodejs/node/pull/52003)
* \[[`8451990668`](https://github.com/nodejs/node/commit/8451990668)] - **test\_runner**: add support for null and date value output (Malthe Borch) [#51920](https://github.com/nodejs/node/pull/51920)
* \[[`423ad47e0f`](https://github.com/nodejs/node/commit/423ad47e0f)] - **tools**: change inactive limit to 12 months (Yagiz Nizipli) [#52425](https://github.com/nodejs/node/pull/52425)
* \[[`0d1e64f64c`](https://github.com/nodejs/node/commit/0d1e64f64c)] - **tools**: update stale bot messaging (Wes Todd) [#52423](https://github.com/nodejs/node/pull/52423)
* \[[`5bae73df90`](https://github.com/nodejs/node/commit/5bae73df90)] - **tools**: update lint-md-dependencies to rollup\@4.14.0 (Node.js GitHub Bot) [#52398](https://github.com/nodejs/node/pull/52398)
* \[[`468cb99ba4`](https://github.com/nodejs/node/commit/468cb99ba4)] - **tools**: update Ruff to v0.3.4 (Michaël Zasso) [#52302](https://github.com/nodejs/node/pull/52302)
* \[[`67b9dda003`](https://github.com/nodejs/node/commit/67b9dda003)] - **tools**: run test-ubsan on ubuntu-latest (Michaël Zasso) [#52375](https://github.com/nodejs/node/pull/52375)
* \[[`f1f32d89e0`](https://github.com/nodejs/node/commit/f1f32d89e0)] - **tools**: update lint-md-dependencies to rollup\@4.13.2 (Node.js GitHub Bot) [#52286](https://github.com/nodejs/node/pull/52286)
* \[[`d7aa8fc9da`](https://github.com/nodejs/node/commit/d7aa8fc9da)] - _**Revert**_ "**tools**: run `build-windows` workflow only on source changes" (Michaël Zasso) [#52320](https://github.com/nodejs/node/pull/52320)
* \[[`a3b1fc3f27`](https://github.com/nodejs/node/commit/a3b1fc3f27)] - **tools**: use Python 3.12 in GitHub Actions workflows (Michaël Zasso) [#52301](https://github.com/nodejs/node/pull/52301)
* \[[`021cf91208`](https://github.com/nodejs/node/commit/021cf91208)] - **tools**: allow local updates for llhttp (Paolo Insogna) [#52085](https://github.com/nodejs/node/pull/52085)
* \[[`4d8602046e`](https://github.com/nodejs/node/commit/4d8602046e)] - **tools**: install npm PowerShell scripts on Windows (Luke Karrys) [#52009](https://github.com/nodejs/node/pull/52009)
* \[[`081319d762`](https://github.com/nodejs/node/commit/081319d762)] - **tools**: update lint-md-dependencies to rollup\@4.13.0 (Node.js GitHub Bot) [#52122](https://github.com/nodejs/node/pull/52122)
* \[[`c43a944231`](https://github.com/nodejs/node/commit/c43a944231)] - **tools**: fix error reported by coverity in js2c.cc (Michael Dawson) [#52142](https://github.com/nodejs/node/pull/52142)
* \[[`f05b241f07`](https://github.com/nodejs/node/commit/f05b241f07)] - **tools**: sync ubsan workflow with asan (Michaël Zasso) [#52152](https://github.com/nodejs/node/pull/52152)
* \[[`a21b15a14e`](https://github.com/nodejs/node/commit/a21b15a14e)] - **tools**: update github\_reporter to 1.7.0 (Node.js GitHub Bot) [#52121](https://github.com/nodejs/node/pull/52121)
* \[[`d60a871db2`](https://github.com/nodejs/node/commit/d60a871db2)] - **tools**: remove gyp-next .github folder (Marco Ippolito) [#52064](https://github.com/nodejs/node/pull/52064)
* \[[`6ad5353764`](https://github.com/nodejs/node/commit/6ad5353764)] - **tools**: update gyp-next to 0.16.2 (Node.js GitHub Bot) [#52062](https://github.com/nodejs/node/pull/52062)
* \[[`dab85bdc06`](https://github.com/nodejs/node/commit/dab85bdc06)] - **tools**: install manpage to share/man for FreeBSD (Po-Chuan Hsieh) [#51791](https://github.com/nodejs/node/pull/51791)
* \[[`cde37e7b63`](https://github.com/nodejs/node/commit/cde37e7b63)] - **tools**: automate gyp-next update (Marco Ippolito) [#52014](https://github.com/nodejs/node/pull/52014)
* \[[`925a464cb8`](https://github.com/nodejs/node/commit/925a464cb8)] - **url**: remove #context from URLSearchParams (Matt Cowley) [#51520](https://github.com/nodejs/node/pull/51520)
* \[[`893e2cf22b`](https://github.com/nodejs/node/commit/893e2cf22b)] - **watch**: fix some node argument not passed to watched process (Raz Luvaton) [#52358](https://github.com/nodejs/node/pull/52358)
* \[[`fec7e505fc`](https://github.com/nodejs/node/commit/fec7e505fc)] - **watch**: use internal addAbortListener (Chemi Atlow) [#52081](https://github.com/nodejs/node/pull/52081)
* \[[`4f68c7c1c9`](https://github.com/nodejs/node/commit/4f68c7c1c9)] - **watch**: mark as stable (Moshe Atlow) [#52074](https://github.com/nodejs/node/pull/52074)
* \[[`257f32296d`](https://github.com/nodejs/node/commit/257f32296d)] - **watch**: batch file restarts (Moshe Atlow) [#51992](https://github.com/nodejs/node/pull/51992)
