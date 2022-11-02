# Node.js 19 ChangeLog

<!--lint disable maximum-line-length no-literal-urls prohibited-strings-->

<table>
<tr>
<th>Current</th>
</tr>
<tr>
<td>
<b><a href="#19.0.1">19.0.1</a></b><br/>
<a href="#19.0.0">19.0.0</a><br/>
</td>
</tr>
</table>

* Other Versions
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

<a id="19.0.1"></a>

## 2022-11-04, Version 19.0.1 (Current), @RafaelGSS

This is a security release.

### Notable changes

The following CVEs are fixed in this release:

* **[CVE-2022-3602](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-3602)**: X.509 Email Address 4-byte Buffer Overflow (High)
* **[CVE-2022-3786](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-3786)**: X.509 Email Address Variable Length Buffer Overflow (High)
* **[CVE-2022-43548](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-43548)**: DNS rebinding in --inspect via invalid octal IP address (Medium)

More detailed information on each of the vulnerabilities can be found in [November 2022 Security Releases](https://nodejs.org/en/blog/vulnerability/november-2022-security-releases/) blog post.

### Commits

* \[[`e58e8d70a8`](https://github.com/nodejs/node/commit/e58e8d70a8)] - **deps**: update archs files for quictls/openssl-3.0.7+quic (RafaelGSS) [#45286](https://github.com/nodejs/node/pull/45286)
* \[[`85f4548d57`](https://github.com/nodejs/node/commit/85f4548d57)] - **deps**: upgrade openssl sources to quictls/openssl-3.0.7+quic (RafaelGSS) [#45286](https://github.com/nodejs/node/pull/45286)
* \[[`43403f56f7`](https://github.com/nodejs/node/commit/43403f56f7)] - **inspector**: harden IP address validation again (Tobias Nießen) [nodejs-private/node-private#354](https://github.com/nodejs-private/node-private/pull/354)

<a id="19.0.0"></a>

## 2022-10-18, Version 19.0.0 (Current), @RafaelGSS and @ruyadorno

Node.js 19 is here! Highlights include the update of the V8 JavaScript engine to 10.7, HTTP(s)/1.1 KeepAlive enabled by default, and ESM Resolution adjusts.

Node.js 19 will replace Node.js 18 as our ‘Current’ release line when Node.js 18 enters long-term support (LTS) later this month.
As per the release schedule, Node.js 19 will be ‘Current' release for the next 6 months, until April 2023.

### Notable Changes

#### Deprecations and Removals

* \[[`7dd2f41c73`](https://github.com/nodejs/node/commit/7dd2f41c73)] - **(SEMVER-MAJOR)** **module**: runtime deprecate exports double slash maps (Guy Bedford) [#44495](https://github.com/nodejs/node/pull/44495)
* \[[`ada2d053ae`](https://github.com/nodejs/node/commit/ada2d053ae)] - **(SEMVER-MAJOR)** **process**: runtime deprecate coercion to integer in `process.exit()` (Daeyeon Jeong) [#44711](https://github.com/nodejs/node/pull/44711)

#### HTTP(S)/1.1 KeepAlive by default

Starting with this release, Node.js sets `keepAlive` to true by default. This means that any outgoing HTTP(s) connection will automatically use HTTP 1.1 Keep-Alive. The default waiting window is 5 seconds.
Enable keep-alive will deliver better throughput as connections are reused by default.

Additionally the agent is now able to parse the response `Keep-Alive` which the servers might send. This header instructs the client on how much to stay connected.
On the other side, the Node.js HTTP server will now automatically disconnect idle clients (which are using HTTP Keep-Alive to reuse the connection) when `close()` is invoked).

Node.js HTTP(S)/1.1 requests may experience a better throughput/performance by default.

Contributed by Paolo Insogna in [#43522](https://github.com/nodejs/node/pull/43522)

#### DTrace/SystemTap/ETW Support were removed

The main reason is the lack of resources from Node.js team. The complexity to keep the support up-to-date has been proved not worth it without a clear plan to support those tools. Hence, [an issue was raised](https://github.com/nodejs/node/issues/44550) in the Node.js repository to assess a better support, for `DTrace` in specific.

Contributed by Ben Noordhuis in [#43651](https://github.com/nodejs/node/pull/43651) and [#43652](https://github.com/nodejs/node/pull/43652)

#### V8 10.7

The V8 engine is updated to version 10.7, which is part of Chromium 107.
This version include a new feature to the JavaScript API: `Intl.NumberFormat`.

`Intl.NumberFormat` v3 API is a new [TC39 ECMA402 stage 3 proposal](https://github.com/tc39/proposal-intl-numberformat-v3)
extend the pre-existing `Intl.NumberFormat`.

The V8 update was a contribution by Michaël Zasso in [#44741](https://github.com/nodejs/node/pull/44741).

#### llhttp 8.1.0

llhttp has been updated to version 8.1.0.Collectively, this version brings many update to the llhttp API, introducing new callbacks and allow all callback to be pausable.

Contributed by Paolo Insogna in [#44967](https://github.com/nodejs/node/pull/44967)

#### Other Notable Changes

* \[[`46a3afb579`](https://github.com/nodejs/node/commit/46a3afb579)] - **doc**: graduate webcrypto to stable (Filip Skokan) [#44897](https://github.com/nodejs/node/pull/44897)
* \[[`f594cc85b7`](https://github.com/nodejs/node/commit/f594cc85b7)] - **esm**: remove specifier resolution flag (Geoffrey Booth) [#44859](https://github.com/nodejs/node/pull/44859)

### Semver-Major Commits

* \[[`53f73d1cfe`](https://github.com/nodejs/node/commit/53f73d1cfe)] - **(SEMVER-MAJOR)** **build**: enable V8's trap handler on Windows (Michaël Zasso) [#44741](https://github.com/nodejs/node/pull/44741)
* \[[`06aaf8a1c4`](https://github.com/nodejs/node/commit/06aaf8a1c4)] - **(SEMVER-MAJOR)** **build**: reset embedder string to "-node.0" (Michaël Zasso) [#44741](https://github.com/nodejs/node/pull/44741)
* \[[`aa3a572e6b`](https://github.com/nodejs/node/commit/aa3a572e6b)] - **(SEMVER-MAJOR)** **build**: remove dtrace & etw support (Ben Noordhuis) [#43652](https://github.com/nodejs/node/pull/43652)
* \[[`38f1e2793c`](https://github.com/nodejs/node/commit/38f1e2793c)] - **(SEMVER-MAJOR)** **build**: remove systemtap support (Ben Noordhuis) [#43651](https://github.com/nodejs/node/pull/43651)
* \[[`2849283c4c`](https://github.com/nodejs/node/commit/2849283c4c)] - **(SEMVER-MAJOR)** **crypto**: remove non-standard `webcrypto.Crypto.prototype.CryptoKey` (Antoine du Hamel) [#42083](https://github.com/nodejs/node/pull/42083)
* \[[`a1653ac715`](https://github.com/nodejs/node/commit/a1653ac715)] - **(SEMVER-MAJOR)** **crypto**: do not allow to call setFips from the worker thread (Sergey Petushkov) [#43624](https://github.com/nodejs/node/pull/43624)
* \[[`fd36a8dadb`](https://github.com/nodejs/node/commit/fd36a8dadb)] - **(SEMVER-MAJOR)** **deps**: update llhttp to 8.1.0 (Paolo Insogna) [#44967](https://github.com/nodejs/node/pull/44967)
* \[[`89ecdddaab`](https://github.com/nodejs/node/commit/89ecdddaab)] - **(SEMVER-MAJOR)** **deps**: bump minimum ICU version to 71 (Michaël Zasso) [#44741](https://github.com/nodejs/node/pull/44741)
* \[[`66fe446efd`](https://github.com/nodejs/node/commit/66fe446efd)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick 0cccb6f27d78 (Michaël Zasso) [#44741](https://github.com/nodejs/node/pull/44741)
* \[[`88ed027d57`](https://github.com/nodejs/node/commit/88ed027d57)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick 7ddb8399f9f1 (Michaël Zasso) [#44741](https://github.com/nodejs/node/pull/44741)
* \[[`26c651c34e`](https://github.com/nodejs/node/commit/26c651c34e)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick 1b3a4f0c34a1 (Michaël Zasso) [#44741](https://github.com/nodejs/node/pull/44741)
* \[[`c8ff2dfd11`](https://github.com/nodejs/node/commit/c8ff2dfd11)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick b161a0823165 (Michaël Zasso) [#44741](https://github.com/nodejs/node/pull/44741)
* \[[`7a8fa2d517`](https://github.com/nodejs/node/commit/7a8fa2d517)] - **(SEMVER-MAJOR)** **deps**: fix V8 build on Windows with MSVC (Michaël Zasso) [#44741](https://github.com/nodejs/node/pull/44741)
* \[[`83b0aaa800`](https://github.com/nodejs/node/commit/83b0aaa800)] - **(SEMVER-MAJOR)** **deps**: fix V8 build on SmartOS (Michaël Zasso) [#44741](https://github.com/nodejs/node/pull/44741)
* \[[`7a952e8ea5`](https://github.com/nodejs/node/commit/7a952e8ea5)] - **(SEMVER-MAJOR)** **deps**: silence irrelevant V8 warning (Michaël Zasso) [#44741](https://github.com/nodejs/node/pull/44741)
* \[[`6bd756d7c6`](https://github.com/nodejs/node/commit/6bd756d7c6)] - **(SEMVER-MAJOR)** **deps**: update V8 to 10.7.193.13 (Michaël Zasso) [#44741](https://github.com/nodejs/node/pull/44741)
* \[[`03fb789fb9`](https://github.com/nodejs/node/commit/03fb789fb9)] - **(SEMVER-MAJOR)** **events**: add null check for the signal of EventTarget (Masashi Hirano) [#43153](https://github.com/nodejs/node/pull/43153)
* \[[`a4fa526ddc`](https://github.com/nodejs/node/commit/a4fa526ddc)] - **(SEMVER-MAJOR)** **fs**: add directory autodetection to fsPromises.symlink() (Livia Medeiros) [#42894](https://github.com/nodejs/node/pull/42894)
* \[[`bb4891d8d4`](https://github.com/nodejs/node/commit/bb4891d8d4)] - **(SEMVER-MAJOR)** **fs**: add validateBuffer to improve error (Hirotaka Tagawa / wafuwafu13) [#44769](https://github.com/nodejs/node/pull/44769)
* \[[`950a4411fa`](https://github.com/nodejs/node/commit/950a4411fa)] - **(SEMVER-MAJOR)** **fs**: remove coercion to string in writing methods (Livia Medeiros) [#42796](https://github.com/nodejs/node/pull/42796)
* \[[`41a6d82968`](https://github.com/nodejs/node/commit/41a6d82968)] - **(SEMVER-MAJOR)** **fs**: harden fs.readSync(buffer, options) typecheck (LiviaMedeiros) [#42772](https://github.com/nodejs/node/pull/42772)
* \[[`2275faac2b`](https://github.com/nodejs/node/commit/2275faac2b)] - **(SEMVER-MAJOR)** **fs**: harden fs.read(params, callback) typecheck (LiviaMedeiros) [#42772](https://github.com/nodejs/node/pull/42772)
* \[[`29953a0b88`](https://github.com/nodejs/node/commit/29953a0b88)] - **(SEMVER-MAJOR)** **fs**: harden filehandle.read(params) typecheck (LiviaMedeiros) [#42772](https://github.com/nodejs/node/pull/42772)
* \[[`4267b92604`](https://github.com/nodejs/node/commit/4267b92604)] - **(SEMVER-MAJOR)** **http**: use Keep-Alive by default in global agents (Paolo Insogna) [#43522](https://github.com/nodejs/node/pull/43522)
* \[[`0324529e0f`](https://github.com/nodejs/node/commit/0324529e0f)] - **(SEMVER-MAJOR)** **inspector**: introduce inspector/promises API (Erick Wendel) [#44250](https://github.com/nodejs/node/pull/44250)
* \[[`80270994d6`](https://github.com/nodejs/node/commit/80270994d6)] - **(SEMVER-MAJOR)** **lib**: enable global CustomEvent by default (Daeyeon Jeong) [#44860](https://github.com/nodejs/node/pull/44860)
* \[[`f529f73bd7`](https://github.com/nodejs/node/commit/f529f73bd7)] - **(SEMVER-MAJOR)** **lib**: brand check event handler property receivers (Chengzhong Wu) [#44483](https://github.com/nodejs/node/pull/44483)
* \[[`6de2673a9f`](https://github.com/nodejs/node/commit/6de2673a9f)] - **(SEMVER-MAJOR)** **lib**: enable global WebCrypto by default (Antoine du Hamel) [#42083](https://github.com/nodejs/node/pull/42083)
* \[[`73ba8830d5`](https://github.com/nodejs/node/commit/73ba8830d5)] - **(SEMVER-MAJOR)** **lib**: use private field in AbortController (Joyee Cheung) [#43820](https://github.com/nodejs/node/pull/43820)
* \[[`7dd2f41c73`](https://github.com/nodejs/node/commit/7dd2f41c73)] - **(SEMVER-MAJOR)** **module**: runtime deprecate exports double slash maps (Guy Bedford) [#44495](https://github.com/nodejs/node/pull/44495)
* \[[`22c39b1ddd`](https://github.com/nodejs/node/commit/22c39b1ddd)] - **(SEMVER-MAJOR)** **path**: the dot will be added(path.format) if it is not specified in `ext` (theanarkh) [#44349](https://github.com/nodejs/node/pull/44349)
* \[[`587367d107`](https://github.com/nodejs/node/commit/587367d107)] - **(SEMVER-MAJOR)** **perf\_hooks**: expose webperf global scope interfaces (Chengzhong Wu) [#44483](https://github.com/nodejs/node/pull/44483)
* \[[`364c0e196c`](https://github.com/nodejs/node/commit/364c0e196c)] - **(SEMVER-MAJOR)** **perf\_hooks**: fix webperf idlharness (Chengzhong Wu) [#44483](https://github.com/nodejs/node/pull/44483)
* \[[`ada2d053ae`](https://github.com/nodejs/node/commit/ada2d053ae)] - **(SEMVER-MAJOR)** **process**: runtime deprecate coercion to integer in `process.exit()` (Daeyeon Jeong) [#44711](https://github.com/nodejs/node/pull/44711)
* \[[`e0ab8dd637`](https://github.com/nodejs/node/commit/e0ab8dd637)] - **(SEMVER-MAJOR)** **process**: make process.config read only (Sergey Petushkov) [#43627](https://github.com/nodejs/node/pull/43627)
* \[[`481a959adb`](https://github.com/nodejs/node/commit/481a959adb)] - **(SEMVER-MAJOR)** **readline**: remove `question` method from `InterfaceConstructor` (Antoine du Hamel) [#44606](https://github.com/nodejs/node/pull/44606)
* \[[`c9602ce212`](https://github.com/nodejs/node/commit/c9602ce212)] - **(SEMVER-MAJOR)** **src**: use new v8::OOMErrorCallback API (Michaël Zasso) [#44741](https://github.com/nodejs/node/pull/44741)
* \[[`19a70c11e4`](https://github.com/nodejs/node/commit/19a70c11e4)] - **(SEMVER-MAJOR)** **src**: override CreateJob instead of PostJob (Clemens Backes) [#44741](https://github.com/nodejs/node/pull/44741)
* \[[`fd52c62bee`](https://github.com/nodejs/node/commit/fd52c62bee)] - **(SEMVER-MAJOR)** **src**: use V8\_ENABLE\_SANDBOX macro (Michaël Zasso) [#44741](https://github.com/nodejs/node/pull/44741)
* \[[`c10988db44`](https://github.com/nodejs/node/commit/c10988db44)] - **(SEMVER-MAJOR)** **src**: use non-deprecated V8 inspector API (Michaël Zasso) [#44741](https://github.com/nodejs/node/pull/44741)
* \[[`3efe901dd6`](https://github.com/nodejs/node/commit/3efe901dd6)] - **(SEMVER-MAJOR)** **src**: update NODE\_MODULE\_VERSION to 111 (Michaël Zasso) [#44741](https://github.com/nodejs/node/pull/44741)
* \[[`77e585657f`](https://github.com/nodejs/node/commit/77e585657f)] - **(SEMVER-MAJOR)** **src**: turn embedder api overload into default argument (Alena Khineika) [#43629](https://github.com/nodejs/node/pull/43629)
* \[[`dabda03ea9`](https://github.com/nodejs/node/commit/dabda03ea9)] - **(SEMVER-MAJOR)** **src**: per-environment time origin value (Chengzhong Wu) [#43781](https://github.com/nodejs/node/pull/43781)
* \[[`2e49b99cc2`](https://github.com/nodejs/node/commit/2e49b99cc2)] - **(SEMVER-MAJOR)** **src,test**: disable freezing V8 flags on initialization (Clemens Backes) [#44741](https://github.com/nodejs/node/pull/44741)
* \[[`2b32985c62`](https://github.com/nodejs/node/commit/2b32985c62)] - **(SEMVER-MAJOR)** **stream**: use null for the error argument (Luigi Pinca) [#44312](https://github.com/nodejs/node/pull/44312)
* \[[`36805e8524`](https://github.com/nodejs/node/commit/36805e8524)] - **(SEMVER-MAJOR)** **test**: adapt test-repl for V8 update (Michaël Zasso) [#44741](https://github.com/nodejs/node/pull/44741)
* \[[`96ef25793d`](https://github.com/nodejs/node/commit/96ef25793d)] - **(SEMVER-MAJOR)** **test**: adapt test-repl-pretty-\*stack to V8 changes (Michaël Zasso) [#44741](https://github.com/nodejs/node/pull/44741)
* \[[`71c193e581`](https://github.com/nodejs/node/commit/71c193e581)] - **(SEMVER-MAJOR)** **test**: adapt to new JSON SyntaxError messages (Michaël Zasso) [#44741](https://github.com/nodejs/node/pull/44741)
* \[[`b5f1564880`](https://github.com/nodejs/node/commit/b5f1564880)] - **(SEMVER-MAJOR)** **test**: rename always-opt flag to always-turbofan (Michaël Zasso) [#44741](https://github.com/nodejs/node/pull/44741)
* \[[`1acf0339dd`](https://github.com/nodejs/node/commit/1acf0339dd)] - **(SEMVER-MAJOR)** **test**: fix test-hash-seed for new V8 versions (Michaël Zasso) [#44741](https://github.com/nodejs/node/pull/44741)
* \[[`57ff476c33`](https://github.com/nodejs/node/commit/57ff476c33)] - **(SEMVER-MAJOR)** **test**: remove duplicate test (Luigi Pinca) [#44051](https://github.com/nodejs/node/pull/44051)
* \[[`77def91bf9`](https://github.com/nodejs/node/commit/77def91bf9)] - **(SEMVER-MAJOR)** **tls,http2**: send fatal alert on ALPN mismatch (Tobias Nießen) [#44031](https://github.com/nodejs/node/pull/44031)
* \[[`4860ad99b9`](https://github.com/nodejs/node/commit/4860ad99b9)] - **(SEMVER-MAJOR)** **tools**: update V8 gypfiles for 10.7 (Michaël Zasso) [#44741](https://github.com/nodejs/node/pull/44741)

### Semver-Minor Commits

* \[[`af0921d877`](https://github.com/nodejs/node/commit/af0921d877)] - **(SEMVER-MINOR)** **esm**: add `--import` flag (Moshe Atlow) [#43942](https://github.com/nodejs/node/pull/43942)
* \[[`0633e9a0b5`](https://github.com/nodejs/node/commit/0633e9a0b5)] - **(SEMVER-MINOR)** **lib**: add diagnostics channel for process and worker (theanarkh) [#44045](https://github.com/nodejs/node/pull/44045)
* \[[`ca5be26b31`](https://github.com/nodejs/node/commit/ca5be26b31)] - **(SEMVER-MINOR)** **src**: add support for externally shared js builtins (Michael Dawson) [#44376](https://github.com/nodejs/node/pull/44376)
* \[[`e86a638305`](https://github.com/nodejs/node/commit/e86a638305)] - **(SEMVER-MINOR)** **src**: add initial shadow realm support (Chengzhong Wu) [#42869](https://github.com/nodejs/node/pull/42869)
* \[[`71ca6d7d6a`](https://github.com/nodejs/node/commit/71ca6d7d6a)] - **(SEMVER-MINOR)** **util**: add `maxArrayLength` option to Set and Map (Kohei Ueno) [#43576](https://github.com/nodejs/node/pull/43576)

### Semver-Patch Commits

* \[[`78508028e3`](https://github.com/nodejs/node/commit/78508028e3)] - **bootstrap**: generate bootstrapper arguments in BuiltinLoader (Joyee Cheung) [#44488](https://github.com/nodejs/node/pull/44488)
* \[[`5291096ca2`](https://github.com/nodejs/node/commit/5291096ca2)] - **bootstrap**: check more metadata when loading the snapshot (Joyee Cheung) [#44132](https://github.com/nodejs/node/pull/44132)
* \[[`d0f73d383d`](https://github.com/nodejs/node/commit/d0f73d383d)] - **build**: go faster, drop -fno-omit-frame-pointer (Ben Noordhuis) [#44452](https://github.com/nodejs/node/pull/44452)
* \[[`214354fc9f`](https://github.com/nodejs/node/commit/214354fc9f)] - **crypto**: fix webcrypto HMAC "get key length" in deriveKey and generateKey (Filip Skokan) [#44917](https://github.com/nodejs/node/pull/44917)
* \[[`40a0757b21`](https://github.com/nodejs/node/commit/40a0757b21)] - **crypto**: remove webcrypto HKDF and PBKDF2 default-applied lengths (Filip Skokan) [#44945](https://github.com/nodejs/node/pull/44945)
* \[[`eeec3eb16a`](https://github.com/nodejs/node/commit/eeec3eb16a)] - **crypto**: simplify webcrypto ECDH deriveBits (Filip Skokan) [#44946](https://github.com/nodejs/node/pull/44946)
* \[[`0be1c57281`](https://github.com/nodejs/node/commit/0be1c57281)] - **deps**: V8: cherry-pick c2792e58035f (Jiawen Geng) [#44961](https://github.com/nodejs/node/pull/44961)
* \[[`488474618c`](https://github.com/nodejs/node/commit/488474618c)] - **deps**: V8: cherry-pick c3dffe6e2bda (Michaël Zasso) [#44958](https://github.com/nodejs/node/pull/44958)
* \[[`34ba631a0b`](https://github.com/nodejs/node/commit/34ba631a0b)] - **deps**: V8: cherry-pick e7f0f26f5ef3 (Michaël Zasso) [#44958](https://github.com/nodejs/node/pull/44958)
* \[[`690a837f4f`](https://github.com/nodejs/node/commit/690a837f4f)] - **deps**: V8: cherry-pick 3d59a3c2c164 (Michaël Zasso) [#44958](https://github.com/nodejs/node/pull/44958)
* \[[`bab8b3aad6`](https://github.com/nodejs/node/commit/bab8b3aad6)] - **deps**: V8: cherry-pick 8b8703953616 (Michaël Zasso) [#44958](https://github.com/nodejs/node/pull/44958)
* \[[`37e5152245`](https://github.com/nodejs/node/commit/37e5152245)] - **doc**: add notable changes to latest v18.x release changelog (Danielle Adams) [#44996](https://github.com/nodejs/node/pull/44996)
* \[[`19a909902a`](https://github.com/nodejs/node/commit/19a909902a)] - **doc**: deprecate url.parse() (Rich Trott) [#44919](https://github.com/nodejs/node/pull/44919)
* \[[`6686d9000b`](https://github.com/nodejs/node/commit/6686d9000b)] - **doc**: fix backticks in fs API docs (Livia Medeiros) [#44962](https://github.com/nodejs/node/pull/44962)
* \[[`46a3afb579`](https://github.com/nodejs/node/commit/46a3afb579)] - **doc**: graduate webcrypto to stable (Filip Skokan) [#44897](https://github.com/nodejs/node/pull/44897)
* \[[`6e3c55cc35`](https://github.com/nodejs/node/commit/6e3c55cc35)] - **doc**: fix v16.17.1 security release changelog (Ruy Adorno) [#44759](https://github.com/nodejs/node/pull/44759)
* \[[`77cb88b91c`](https://github.com/nodejs/node/commit/77cb88b91c)] - **doc**: mark `--import` as experimental (Moshe Atlow) [#44067](https://github.com/nodejs/node/pull/44067)
* \[[`46dcfb3c7b`](https://github.com/nodejs/node/commit/46dcfb3c7b)] - **doc,crypto**: update webcrypto docs for global access (Filip Skokan) [#44723](https://github.com/nodejs/node/pull/44723)
* \[[`f594cc85b7`](https://github.com/nodejs/node/commit/f594cc85b7)] - **esm**: remove specifier resolution flag (Geoffrey Booth) [#44859](https://github.com/nodejs/node/pull/44859)
* \[[`3c040348fe`](https://github.com/nodejs/node/commit/3c040348fe)] - _**Revert**_ "**esm**: convert `resolve` hook to synchronous" (Jacob Smith) [#43526](https://github.com/nodejs/node/pull/43526)
* \[[`90b634a5a5`](https://github.com/nodejs/node/commit/90b634a5a5)] - **esm**: convert `resolve` hook to synchronous (Jacob Smith) [#43363](https://github.com/nodejs/node/pull/43363)
* \[[`7c06eab1dc`](https://github.com/nodejs/node/commit/7c06eab1dc)] - _**Revert**_ "**http**: do not leak error listeners" (Luigi Pinca) [#44921](https://github.com/nodejs/node/pull/44921)
* \[[`464d1c1558`](https://github.com/nodejs/node/commit/464d1c1558)] - **lib**: reset `RegExp` statics before running user code (Antoine du Hamel) [#43741](https://github.com/nodejs/node/pull/43741)
* \[[`15f10515e3`](https://github.com/nodejs/node/commit/15f10515e3)] - **module**: fix segment deprecation for imports field (Guy Bedford) [#44883](https://github.com/nodejs/node/pull/44883)
* \[[`7cdf745fdd`](https://github.com/nodejs/node/commit/7cdf745fdd)] - **perf\_hooks**: convert maxSize to IDL value in setResourceTimingBufferSize (Chengzhong Wu) [#44902](https://github.com/nodejs/node/pull/44902)
* \[[`be525d7d04`](https://github.com/nodejs/node/commit/be525d7d04)] - **src**: consolidate exit codes in the code base (Joyee Cheung) [#44746](https://github.com/nodejs/node/pull/44746)
* \[[`d5ce285c8b`](https://github.com/nodejs/node/commit/d5ce285c8b)] - **src**: refactor BaseObject methods (Joyee Cheung) [#44796](https://github.com/nodejs/node/pull/44796)
* \[[`717465433c`](https://github.com/nodejs/node/commit/717465433c)] - **src**: create BaseObject with node::Realm (Chengzhong Wu) [#44348](https://github.com/nodejs/node/pull/44348)
* \[[`45f2258f74`](https://github.com/nodejs/node/commit/45f2258f74)] - **src**: restore IS\_RELEASE to 0 (Bryan English) [#44758](https://github.com/nodejs/node/pull/44758)
* \[[`1f54fc25cb`](https://github.com/nodejs/node/commit/1f54fc25cb)] - **src**: use automatic memory mgmt in SecretKeyGen (Tobias Nießen) [#44479](https://github.com/nodejs/node/pull/44479)
* \[[`7371d335ac`](https://github.com/nodejs/node/commit/7371d335ac)] - **src**: use V8 entropy source if RAND\_bytes() != 1 (Tobias Nießen) [#44493](https://github.com/nodejs/node/pull/44493)
* \[[`81d9cdb8cd`](https://github.com/nodejs/node/commit/81d9cdb8cd)] - **src**: introduce node::Realm (Chengzhong Wu) [#44179](https://github.com/nodejs/node/pull/44179)
* \[[`ad41c919df`](https://github.com/nodejs/node/commit/ad41c919df)] - **src**: remove v8abbr.h (Tobias Nießen) [#44402](https://github.com/nodejs/node/pull/44402)
* \[[`fddc701d3c`](https://github.com/nodejs/node/commit/fddc701d3c)] - **src**: support diagnostics channel in the snapshot (Joyee Cheung) [#44193](https://github.com/nodejs/node/pull/44193)
* \[[`d70aab663c`](https://github.com/nodejs/node/commit/d70aab663c)] - **src**: support WeakReference in snapshot (Joyee Cheung) [#44193](https://github.com/nodejs/node/pull/44193)
* \[[`4ca398a617`](https://github.com/nodejs/node/commit/4ca398a617)] - **src**: iterate over base objects to prepare for snapshot (Joyee Cheung) [#44192](https://github.com/nodejs/node/pull/44192)
* \[[`8b0e5b19bd`](https://github.com/nodejs/node/commit/8b0e5b19bd)] - **src**: fix cppgc incompatibility in v8 (Shelley Vohr) [#43521](https://github.com/nodejs/node/pull/43521)
* \[[`3fdf6cfad9`](https://github.com/nodejs/node/commit/3fdf6cfad9)] - **stream**: fix `size` function returned from QueuingStrategies (Daeyeon Jeong) [#44867](https://github.com/nodejs/node/pull/44867)
* \[[`331088f4a4`](https://github.com/nodejs/node/commit/331088f4a4)] - _**Revert**_ "**tools**: refactor `tools/license2rtf` to ESM" (Richard Lau) [#43214](https://github.com/nodejs/node/pull/43214)
* \[[`30cb1bf8b8`](https://github.com/nodejs/node/commit/30cb1bf8b8)] - **tools**: refactor `tools/license2rtf` to ESM (Feng Yu) [#43101](https://github.com/nodejs/node/pull/43101)
* \[[`a3ff4bfc66`](https://github.com/nodejs/node/commit/a3ff4bfc66)] - **url**: revert "validate ipv4 part length" (Antoine du Hamel) [#42940](https://github.com/nodejs/node/pull/42940)
* \[[`87d0d7a069`](https://github.com/nodejs/node/commit/87d0d7a069)] - **url**: validate ipv4 part length (Yagiz Nizipli) [#42915](https://github.com/nodejs/node/pull/42915)
* \[[`5b1bcf82f1`](https://github.com/nodejs/node/commit/5b1bcf82f1)] - **vm**: make ContextifyContext a BaseObject (Joyee Cheung) [#44796](https://github.com/nodejs/node/pull/44796)
