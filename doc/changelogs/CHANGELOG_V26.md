# Node.js 26 ChangeLog

<!--lint disable maximum-line-length no-literal-urls prohibited-strings-->

<table>
<tr>
<th>Current</th>
</tr>
<tr>
<td>
<a href="#26.0.0">26.0.0</a><br/>
</td>
</tr>
</table>

* Other Versions
  * [25.x](CHANGELOG_V25.md)
  * [24.x](CHANGELOG_V24.md)
  * [23.x](CHANGELOG_V23.md)
  * [22.x](CHANGELOG_V22.md)
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

<a id="26.0.0"></a>

## 2026-04-28, Version 26.0.0 (Current), @RafaelGSS

We're excited to announce the release of Node.js 26! Highlights include the Temporal API enabled by default,
updates to the V8 JavaScript engine to 14.6, Undici to 8.0, and several important deprecations and removals
as we continue to modernize the platform.

As a reminder, Node.js 26 will enter long-term support (LTS) in October, but until then, it will be the "Current" release for the next six months.
We encourage you to explore the new features and benefits offered by this latest release and evaluate their potential impact on your applications.

### Notable Changes

#### Temporal API

The Temporal API is now enabled by default in Node.js 26. Temporal is a modern date/time API for JavaScript
that provides a more robust and feature-rich alternative to the legacy `Date` object.

Contributed by Richard Lau in [#61806](https://github.com/nodejs/node/pull/61806).

#### V8 14.6

The V8 engine is updated to version 14.6.202.33, which is part of Chromium 134.

This version also includes:

* Upsert (<https://github.com/tc39/proposal-upsert>): `[Weak]Map.prototype.getOrInsert()`, `[Weak]Map.prototype.getOrInsertComputed()`
* Iterator sequencing (<https://github.com/tc39/proposal-iterator-sequencing>): `Iterator.concat()`

Contributed by Michaël Zasso in [#61898](https://github.com/nodejs/node/pull/61898).

#### Undici 8

Undici has been updated to version 8.0.2, bringing new features and improvements to Node.js's HTTP client implementation.

#### Deprecations and Removals

* \[[`dff46c07c3`](https://github.com/nodejs/node/commit/dff46c07c3)] - **(SEMVER-MAJOR)** **crypto**: move DEP0182 to End-of-Life (Tobias Nießen) [#61084](https://github.com/nodejs/node/pull/61084)
* \[[`93c25815ee`](https://github.com/nodejs/node/commit/93c25815ee)] - **(SEMVER-MAJOR)** **http**: move writeHeader to end-of-life (Sebastian Beltran) [#60635](https://github.com/nodejs/node/pull/60635)

`http.Server.prototype.writeHeader()` is now fully removed. Use `http.Server.prototype.writeHead()` instead.

* \[[`c755b0113c`](https://github.com/nodejs/node/commit/c755b0113c)] - **(SEMVER-MAJOR)** **stream**: move \_stream\_\* to end-of-life (Sebastian Beltran) [#60657](https://github.com/nodejs/node/pull/60657)

The legacy `_stream_wrap`, `_stream_readable`, `_stream_writable`, `_stream_duplex`, `_stream_transform`, and `_stream_passthrough` modules are now fully removed.

* \[[`adac077484`](https://github.com/nodejs/node/commit/adac077484)] - **(SEMVER-MAJOR)** **crypto**: runtime-deprecate DEP0203 and DEP0204 (Filip Skokan) [#62453](https://github.com/nodejs/node/pull/62453)
* \[[`ac6375417a`](https://github.com/nodejs/node/commit/ac6375417a)] - **(SEMVER-MAJOR)** **stream**: promote DEP0201 to runtime deprecation (René) [#62173](https://github.com/nodejs/node/pull/62173)
* \[[`98907f560f`](https://github.com/nodejs/node/commit/98907f560f)] - **(SEMVER-MAJOR)** **module**: runtime-deprecate module.register() (Geoffrey Booth) [#62401](https://github.com/nodejs/node/pull/62401)
* \[[`89f4b6cddb`](https://github.com/nodejs/node/commit/89f4b6cddb)] - **(SEMVER-MAJOR)** **module**: remove --experimental-transform-types (Marco Ippolito) [#61803](https://github.com/nodejs/node/pull/61803)

### Semver-Major Commits

* \[[`d3f79aa65d`](https://github.com/nodejs/node/commit/d3f79aa65d)] - **(SEMVER-MAJOR)** **assert**: allow printf-style messages as assertion error (Ruben Bridgewater) [#58849](https://github.com/nodejs/node/pull/58849)
* \[[`f6ce381fec`](https://github.com/nodejs/node/commit/f6ce381fec)] - **(SEMVER-MAJOR)** **build**: bump GCC requirement to 13.2 (Michaël Zasso) [#62555](https://github.com/nodejs/node/pull/62555)
* \[[`bff81fca46`](https://github.com/nodejs/node/commit/bff81fca46)] - **(SEMVER-MAJOR)** **build**: enable Temporal by default (Richard Lau) [#61806](https://github.com/nodejs/node/pull/61806)
* \[[`6ddb1643e1`](https://github.com/nodejs/node/commit/6ddb1643e1)] - **(SEMVER-MAJOR)** **build**: enable V8\_VERIFY\_WRITE\_BARRIERS in debug build (Joyee Cheung) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`a8ab08b373`](https://github.com/nodejs/node/commit/a8ab08b373)] - **(SEMVER-MAJOR)** **build**: reset embedder string to "-node.0" (Michaël Zasso) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`0998c37eb6`](https://github.com/nodejs/node/commit/0998c37eb6)] - **(SEMVER-MAJOR)** **build**: target Power 9 for AIX/IBM i (Richard Lau) [#62296](https://github.com/nodejs/node/pull/62296)
* \[[`d73c49e849`](https://github.com/nodejs/node/commit/d73c49e849)] - **(SEMVER-MAJOR)** **build**: drop support for Python 3.9 (Mike McCready) [#61177](https://github.com/nodejs/node/pull/61177)
* \[[`3c92ee1008`](https://github.com/nodejs/node/commit/3c92ee1008)] - **(SEMVER-MAJOR)** **build**: enable maglev for Linux on s390x (Richard Lau) [#60863](https://github.com/nodejs/node/pull/60863)
* \[[`908c468828`](https://github.com/nodejs/node/commit/908c468828)] - **(SEMVER-MAJOR)** **build**: reset embedder string to "-node.0" (Michaël Zasso) [#60488](https://github.com/nodejs/node/pull/60488)
* \[[`6380fbb5ee`](https://github.com/nodejs/node/commit/6380fbb5ee)] - **(SEMVER-MAJOR)** **build**: reset embedder string to "-node.0" (Michaël Zasso) [#60111](https://github.com/nodejs/node/pull/60111)
* \[[`089d6c77e7`](https://github.com/nodejs/node/commit/089d6c77e7)] - **(SEMVER-MAJOR)** **(CVE-2026-21717)** **build,test**: test array index hash collision (Joyee Cheung) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`f9bd0165c4`](https://github.com/nodejs/node/commit/f9bd0165c4)] - **(SEMVER-MAJOR)** **build,win**: fix Temporal build (StefanStojanovic) [#61806](https://github.com/nodejs/node/pull/61806)
* \[[`6cc4cf8fe8`](https://github.com/nodejs/node/commit/6cc4cf8fe8)] - **(SEMVER-MAJOR)** **crypto**: unify asymmetric key import through KeyObjectHandle::Init (Filip Skokan) [#62499](https://github.com/nodejs/node/pull/62499)
* \[[`adac077484`](https://github.com/nodejs/node/commit/adac077484)] - **(SEMVER-MAJOR)** **crypto**: runtime-deprecate DEP0203 and DEP0204 (Filip Skokan) [#62453](https://github.com/nodejs/node/pull/62453)
* \[[`74509b166a`](https://github.com/nodejs/node/commit/74509b166a)] - **(SEMVER-MAJOR)** **crypto**: decorate async crypto job errors with OpenSSL error details (Filip Skokan) [#62348](https://github.com/nodejs/node/pull/62348)
* \[[`da5843b91d`](https://github.com/nodejs/node/commit/da5843b91d)] - **(SEMVER-MAJOR)** **crypto**: default ML-KEM and ML-DSA pkcs8 export to seed-only format (Filip Skokan) [#62178](https://github.com/nodejs/node/pull/62178)
* \[[`dff46c07c3`](https://github.com/nodejs/node/commit/dff46c07c3)] - **(SEMVER-MAJOR)** **crypto**: move DEP0182 to End-of-Life (Tobias Nießen) [#61084](https://github.com/nodejs/node/pull/61084)
* \[[`94cd600542`](https://github.com/nodejs/node/commit/94cd600542)] - **(SEMVER-MAJOR)** **crypto**: fix DOMException name for non-extractable key error (Filip Skokan) [#60830](https://github.com/nodejs/node/pull/60830)
* \[[`15d406c1b1`](https://github.com/nodejs/node/commit/15d406c1b1)] - **(SEMVER-MAJOR)** **deps**: fix V8 race condition for AIX (Abdirahim Musse) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`46852d2d7a`](https://github.com/nodejs/node/commit/46852d2d7a)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick cd2c216e7658 (LuYahan) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`784431d6fc`](https://github.com/nodejs/node/commit/784431d6fc)] - **(SEMVER-MAJOR)** **deps**: V8: backport 088b7112e7ab (Igor Sheludko) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`3839c4a756`](https://github.com/nodejs/node/commit/3839c4a756)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick 00f6e834029f (Joyee Cheung) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`44f64f1dd9`](https://github.com/nodejs/node/commit/44f64f1dd9)] - **(SEMVER-MAJOR)** **deps**: V8: backport bef0d9c1bc90 (Joyee Cheung) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`1f8f288e22`](https://github.com/nodejs/node/commit/1f8f288e22)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick cf1bce40a5ef (Richard Lau) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`d7eccac9ad`](https://github.com/nodejs/node/commit/d7eccac9ad)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick daf4656ba85e (Milad Fa) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`3ee1ea7d0b`](https://github.com/nodejs/node/commit/3ee1ea7d0b)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick d83f479604c8 (Joyee Cheung) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`80907c0239`](https://github.com/nodejs/node/commit/80907c0239)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick edeb0a4fa181 (Joyee Cheung) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`5e0dc169e9`](https://github.com/nodejs/node/commit/5e0dc169e9)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick aa0b288f87cc (Richard Lau) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`8c1f7adbcd`](https://github.com/nodejs/node/commit/8c1f7adbcd)] - **(SEMVER-MAJOR)** **deps**: patch V8 to fix Windows build (StefanStojanovic) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`3cbd3404d9`](https://github.com/nodejs/node/commit/3cbd3404d9)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick highway\@989a498fdf3 (Richard Lau) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`9f2b7d4031`](https://github.com/nodejs/node/commit/9f2b7d4031)] - **(SEMVER-MAJOR)** **deps**: support madvise(3C) across ALL illumos revisions (Dan McDonald) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`947ec32118`](https://github.com/nodejs/node/commit/947ec32118)] - **(SEMVER-MAJOR)** **deps**: patch V8 for illumos (Dan McDonald) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`0660b942b2`](https://github.com/nodejs/node/commit/0660b942b2)] - **(SEMVER-MAJOR)** **deps**: remove problematic comment from v8-internal (Michaël Zasso) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`bef7b31a3f`](https://github.com/nodejs/node/commit/bef7b31a3f)] - **(SEMVER-MAJOR)** **deps**: define V8\_PRESERVE\_MOST as no-op on Windows (Stefan Stojanovic) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`a10bf1e6ce`](https://github.com/nodejs/node/commit/a10bf1e6ce)] - **(SEMVER-MAJOR)** **deps**: patch V8 to avoid duplicated zlib symbol (Michaël Zasso) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`cc547428e1`](https://github.com/nodejs/node/commit/cc547428e1)] - **(SEMVER-MAJOR)** **deps**: update V8 to 14.6.202.33 (Michaël Zasso) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`b81d2cbcae`](https://github.com/nodejs/node/commit/b81d2cbcae)] - **(SEMVER-MAJOR)** **deps**: update undici to 8.0.2 (Node.js GitHub Bot) [#62384](https://github.com/nodejs/node/pull/62384)
* \[[`bf5c6a8bd4`](https://github.com/nodejs/node/commit/bf5c6a8bd4)] - **(SEMVER-MAJOR)** **deps**: V8: backport 151d0a44a1b2 (Abdirahim Musse) [#60488](https://github.com/nodejs/node/pull/60488)
* \[[`b59af772dc`](https://github.com/nodejs/node/commit/b59af772dc)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick 47800791b35c (Jakob Kummerow) [#60488](https://github.com/nodejs/node/pull/60488)
* \[[`5e41e5228a`](https://github.com/nodejs/node/commit/5e41e5228a)] - **(SEMVER-MAJOR)** **deps**: patch V8 for illumos (Dan McDonald) [#59805](https://github.com/nodejs/node/pull/59805)
* \[[`2243e58e43`](https://github.com/nodejs/node/commit/2243e58e43)] - **(SEMVER-MAJOR)** **deps**: use std::map in MSVC STL for EphemeronRememberedSet (Joyee Cheung) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`4157964c42`](https://github.com/nodejs/node/commit/4157964c42)] - **(SEMVER-MAJOR)** **deps**: remove problematic comment from v8-internal (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`7c8483a4e9`](https://github.com/nodejs/node/commit/7c8483a4e9)] - **(SEMVER-MAJOR)** **deps**: patch V8 to avoid duplicated zlib symbol (Michaël Zasso) [#54077](https://github.com/nodejs/node/pull/54077)
* \[[`53379f3706`](https://github.com/nodejs/node/commit/53379f3706)] - **(SEMVER-MAJOR)** **deps**: update V8 to 14.3.127.12 (Michaël Zasso) [#60488](https://github.com/nodejs/node/pull/60488)
* \[[`f819aec288`](https://github.com/nodejs/node/commit/f819aec288)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick ff34ae20c8e3 (Chengzhong Wu) [#60111](https://github.com/nodejs/node/pull/60111)
* \[[`1acd8df36f`](https://github.com/nodejs/node/commit/1acd8df36f)] - **(SEMVER-MAJOR)** **deps**: V8: backport fed47445bbdd (Abdirahim Musse) [#60111](https://github.com/nodejs/node/pull/60111)
* \[[`46f72577a4`](https://github.com/nodejs/node/commit/46f72577a4)] - **(SEMVER-MAJOR)** **deps**: patch V8 for illumos (Dan McDonald) [#59805](https://github.com/nodejs/node/pull/59805)
* \[[`39eb88eaa8`](https://github.com/nodejs/node/commit/39eb88eaa8)] - **(SEMVER-MAJOR)** **deps**: use std::map in MSVC STL for EphemeronRememberedSet (Joyee Cheung) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`ea3d14eadb`](https://github.com/nodejs/node/commit/ea3d14eadb)] - **(SEMVER-MAJOR)** **deps**: remove problematic comment from v8-internal (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`7bc0f245b4`](https://github.com/nodejs/node/commit/7bc0f245b4)] - **(SEMVER-MAJOR)** **deps**: patch V8 to avoid duplicated zlib symbol (Michaël Zasso) [#54077](https://github.com/nodejs/node/pull/54077)
* \[[`c2843b722c`](https://github.com/nodejs/node/commit/c2843b722c)] - **(SEMVER-MAJOR)** **deps**: update V8 to 14.2.231.9 (Michaël Zasso) [#60111](https://github.com/nodejs/node/pull/60111)
* \[[`b4ea323833`](https://github.com/nodejs/node/commit/b4ea323833)] - **(SEMVER-MAJOR)** **diagnostics\_channel**: ensure tracePromise consistency with non-Promises (René) [#61766](https://github.com/nodejs/node/pull/61766)
* \[[`0c08835f71`](https://github.com/nodejs/node/commit/0c08835f71)] - **(SEMVER-MAJOR)** **doc**: remove extensionless CJS exception for type:module packages (Matteo Collina) [#62176](https://github.com/nodejs/node/pull/62176)
* \[[`ef0f0b0865`](https://github.com/nodejs/node/commit/ef0f0b0865)] - **(SEMVER-MAJOR)** **doc**: update supported Windows SDK version to 11 (Mike McCready) [#61973](https://github.com/nodejs/node/pull/61973)
* \[[`a00d95c73d`](https://github.com/nodejs/node/commit/a00d95c73d)] - **(SEMVER-MAJOR)** **doc**: drop p8 and z13 support (Milad Fa) [#61005](https://github.com/nodejs/node/pull/61005)
* \[[`93c25815ee`](https://github.com/nodejs/node/commit/93c25815ee)] - **(SEMVER-MAJOR)** **http**: move writeHeader to end-of-life (Sebastian Beltran) [#60635](https://github.com/nodejs/node/pull/60635)
* \[[`4346c0f7a7`](https://github.com/nodejs/node/commit/4346c0f7a7)] - **(SEMVER-MAJOR)** **http**: fix handling of HTTP upgrades with bodies (Tim Perry) [#60016](https://github.com/nodejs/node/pull/60016)
* \[[`fa70327610`](https://github.com/nodejs/node/commit/fa70327610)] - **(SEMVER-MAJOR)** **lib**: return undefined for localStorage without file (Matteo Collina) [#61333](https://github.com/nodejs/node/pull/61333)
* \[[`b328bf74bd`](https://github.com/nodejs/node/commit/b328bf74bd)] - **(SEMVER-MAJOR)** **lib,src**: implement QuotaExceededError as DOMException-derived interface (Filip Skokan) [#62293](https://github.com/nodejs/node/pull/62293)
* \[[`98907f560f`](https://github.com/nodejs/node/commit/98907f560f)] - **(SEMVER-MAJOR)** **module**: runtime-deprecate module.register() (Geoffrey Booth) [#62401](https://github.com/nodejs/node/pull/62401)
* \[[`89f4b6cddb`](https://github.com/nodejs/node/commit/89f4b6cddb)] - **(SEMVER-MAJOR)** **module**: remove --experimental-transform-types (Marco Ippolito) [#61803](https://github.com/nodejs/node/pull/61803)
* \[[`5334433437`](https://github.com/nodejs/node/commit/5334433437)] - **(SEMVER-MAJOR)** **src**: replace uses of deprecated v8::External APIs (gahaas) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`46e75f4874`](https://github.com/nodejs/node/commit/46e75f4874)] - **(SEMVER-MAJOR)** **src**: stop using `v8::PropertyCallbackInfo<T>::This()` (Igor Sheludko) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`54fefda0aa`](https://github.com/nodejs/node/commit/54fefda0aa)] - **(SEMVER-MAJOR)** **src**: avoid deprecated Wasm API (Clemens Backes) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`840f509bd1`](https://github.com/nodejs/node/commit/840f509bd1)] - **(SEMVER-MAJOR)** **src**: avoid deprecated `FixedArray::Get` (Clemens Backes) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`75c3bcc3ec`](https://github.com/nodejs/node/commit/75c3bcc3ec)] - **(SEMVER-MAJOR)** **src**: update NODE\_MODULE\_VERSION to 147 (Michaël Zasso) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`8480f87375`](https://github.com/nodejs/node/commit/8480f87375)] - **(SEMVER-MAJOR)** **src**: remove deprecated and unused isolate fields (Michaël Zasso) [#60488](https://github.com/nodejs/node/pull/60488)
* \[[`70b6bd8e19`](https://github.com/nodejs/node/commit/70b6bd8e19)] - **(SEMVER-MAJOR)** **src**: update NODE\_MODULE\_VERSION to 144 (Michaël Zasso) [#60488](https://github.com/nodejs/node/pull/60488)
* \[[`7d2bc5249b`](https://github.com/nodejs/node/commit/7d2bc5249b)] - **(SEMVER-MAJOR)** **src**: include `node_api_types.h` instead of `node_api.h` in `node.h` (Anna Henningsen) [#60496](https://github.com/nodejs/node/pull/60496)
* \[[`91ab1101bc`](https://github.com/nodejs/node/commit/91ab1101bc)] - **(SEMVER-MAJOR)** **src**: update NODE\_MODULE\_VERSION to 142 (Michaël Zasso) [#60111](https://github.com/nodejs/node/pull/60111)
* \[[`ac6375417a`](https://github.com/nodejs/node/commit/ac6375417a)] - **(SEMVER-MAJOR)** **stream**: promote DEP0201 to runtime deprecation (René) [#62173](https://github.com/nodejs/node/pull/62173)
* \[[`c755b0113c`](https://github.com/nodejs/node/commit/c755b0113c)] - **(SEMVER-MAJOR)** **stream**: move \_stream\_\* to end-of-life (Sebastian Beltran) [#60657](https://github.com/nodejs/node/pull/60657)
* \[[`fadb214d95`](https://github.com/nodejs/node/commit/fadb214d95)] - **(SEMVER-MAJOR)** **stream**: readable read one buffer at a time (Robert Nagy) [#60441](https://github.com/nodejs/node/pull/60441)
* \[[`4fe325d93d`](https://github.com/nodejs/node/commit/4fe325d93d)] - **(SEMVER-MAJOR)** **stream**: preserve AsyncLocalStorage on finished only when needed (avcribl) [#59873](https://github.com/nodejs/node/pull/59873)
* \[[`7682e7e9c5`](https://github.com/nodejs/node/commit/7682e7e9c5)] - **(SEMVER-MAJOR)** **test**: skip wasm allocation tests in workers (Michaël Zasso) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`ebfaf25870`](https://github.com/nodejs/node/commit/ebfaf25870)] - **(SEMVER-MAJOR)** **test**: update wpt Wasm jsapi expectations (Michaël Zasso) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`ece6a17574`](https://github.com/nodejs/node/commit/ece6a17574)] - **(SEMVER-MAJOR)** **test**: support presence of Temporal global (Michaël Zasso) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`75b8d7a912`](https://github.com/nodejs/node/commit/75b8d7a912)] - **(SEMVER-MAJOR)** **test**: add type tags to uses of v8::External (gahaas) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`092a448ad0`](https://github.com/nodejs/node/commit/092a448ad0)] - **(SEMVER-MAJOR)** **test**: fix test-linux-perf-logger for V8 14.3 (Michaël Zasso) [#60488](https://github.com/nodejs/node/pull/60488)
* \[[`8eb9c8f794`](https://github.com/nodejs/node/commit/8eb9c8f794)] - **(SEMVER-MAJOR)** **tools**: remove v8\_initializers\_slow workaround from v8.gyp (Michaël Zasso) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`a34fe77fe7`](https://github.com/nodejs/node/commit/a34fe77fe7)] - **(SEMVER-MAJOR)** **tools**: add Rust args to `tools/make-v8.sh` (Richard Lau) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`f4666bd6e3`](https://github.com/nodejs/node/commit/f4666bd6e3)] - **(SEMVER-MAJOR)** **tools**: update V8 gypfiles for 14.6 (Michaël Zasso) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`3c23d217a6`](https://github.com/nodejs/node/commit/3c23d217a6)] - **(SEMVER-MAJOR)** **tools**: update V8 gypfiles for 14.5 (Michaël Zasso) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`e508489e37`](https://github.com/nodejs/node/commit/e508489e37)] - **(SEMVER-MAJOR)** **tools**: update V8 gypfiles for 14.4 (Michaël Zasso) [#61898](https://github.com/nodejs/node/pull/61898)
* \[[`dc97b507d0`](https://github.com/nodejs/node/commit/dc97b507d0)] - **(SEMVER-MAJOR)** **util**: mark proxied objects as such when inspecting them (Ruben Bridgewater) [#61029](https://github.com/nodejs/node/pull/61029)
* \[[`ddbe1365ff`](https://github.com/nodejs/node/commit/ddbe1365ff)] - **(SEMVER-MAJOR)** **util**: reduce TextEncoder.encodeInto function size (Yagiz Nizipli) [#60339](https://github.com/nodejs/node/pull/60339)

### Semver-Minor Commits

* \[[`d4fa60cf9f`](https://github.com/nodejs/node/commit/d4fa60cf9f)] - **(SEMVER-MINOR)** **crypto**: add raw key formats support to the KeyObject APIs (Filip Skokan) [#62240](https://github.com/nodejs/node/pull/62240)

### Semver-Patch Commits

* \[[`a4edab8dfb`](https://github.com/nodejs/node/commit/a4edab8dfb)] - **build**: use `CARGO` environment variable if set (Richard Lau) [#62421](https://github.com/nodejs/node/pull/62421)
* \[[`ecf8721076`](https://github.com/nodejs/node/commit/ecf8721076)] - **build**: add weak symbol detection to export script (Abdirahim Musse) [#62656](https://github.com/nodejs/node/pull/62656)
* \[[`5b9f811662`](https://github.com/nodejs/node/commit/5b9f811662)] - **build**: filter hidden visibility symbols on AIX (Abdirahim Musse) [#62656](https://github.com/nodejs/node/pull/62656)
* \[[`2e724793e6`](https://github.com/nodejs/node/commit/2e724793e6)] - **build**: aix add conditonal flags for clang builds (Abdirahim Musse) [#62656](https://github.com/nodejs/node/pull/62656)
* \[[`f212aee483`](https://github.com/nodejs/node/commit/f212aee483)] - **build**: enable temporal on GHA macOS build (Chengzhong Wu) [#61691](https://github.com/nodejs/node/pull/61691)
* \[[`159ae48f8c`](https://github.com/nodejs/node/commit/159ae48f8c)] - **build**: add `cargo` and `rustc` checks for Temporal (Richard Lau) [#61467](https://github.com/nodejs/node/pull/61467)
* \[[`a004535617`](https://github.com/nodejs/node/commit/a004535617)] - **build**: add temporal to linux GHA build (Chengzhong Wu) [#60942](https://github.com/nodejs/node/pull/60942)
* \[[`9df9b66c18`](https://github.com/nodejs/node/commit/9df9b66c18)] - **crypto**: add support for Ed25519 context parameter (Filip Skokan) [#62474](https://github.com/nodejs/node/pull/62474)
* \[[`c3042c605b`](https://github.com/nodejs/node/commit/c3042c605b)] - **crypto**: recognize raw formats in keygen (Filip Skokan) [#62480](https://github.com/nodejs/node/pull/62480)
* \[[`ce0f498def`](https://github.com/nodejs/node/commit/ce0f498def)] - **deps**: V8: cherry-pick fcf8b990c73c (Abdirahim Musse) [#62894](https://github.com/nodejs/node/pull/62894)
* \[[`b7fab70d56`](https://github.com/nodejs/node/commit/b7fab70d56)] - _**Revert**_ "**deps**: V8: cherry-pick 7107287" (Richard Lau) [#62894](https://github.com/nodejs/node/pull/62894)
* \[[`d936c30fb4`](https://github.com/nodejs/node/commit/d936c30fb4)] - **deps**: V8: cherry-pick 7107287 (Abdirahim Musse) [#62656](https://github.com/nodejs/node/pull/62656)
* \[[`c91d00b6d4`](https://github.com/nodejs/node/commit/c91d00b6d4)] - **deps**: fix aix implicit declaration in OpenSSL (Abdirahim Musse) [#62656](https://github.com/nodejs/node/pull/62656)
* \[[`0474a27c06`](https://github.com/nodejs/node/commit/0474a27c06)] - **deps**: libuv: revert 3a9a6e3e6b (Antoine du Hamel) [#62511](https://github.com/nodejs/node/pull/62511)
* \[[`7547e795ef`](https://github.com/nodejs/node/commit/7547e795ef)] - **deps**: update icu to 78.3 (Node.js GitHub Bot) [#62324](https://github.com/nodejs/node/pull/62324)
* \[[`5bebd7eaea`](https://github.com/nodejs/node/commit/5bebd7eaea)] - **deps**: update libuv to 1.52.1 (Node.js GitHub Bot) [#61829](https://github.com/nodejs/node/pull/61829)
* \[[`87d7db1918`](https://github.com/nodejs/node/commit/87d7db1918)] - **deps**: patch V8 to 14.3.127.18 (Node.js GitHub Bot) [#61421](https://github.com/nodejs/node/pull/61421)
* \[[`9d27d9a393`](https://github.com/nodejs/node/commit/9d27d9a393)] - **deps**: patch V8 to 14.3.127.17 (Node.js GitHub Bot) [#61058](https://github.com/nodejs/node/pull/61058)
* \[[`bfc729cf19`](https://github.com/nodejs/node/commit/bfc729cf19)] - **deps**: patch V8 to 14.3.127.16 (Node.js GitHub Bot) [#60819](https://github.com/nodejs/node/pull/60819)
* \[[`8716146d5b`](https://github.com/nodejs/node/commit/8716146d5b)] - **deps**: patch V8 to 14.3.127.14 (Node.js GitHub Bot) [#60743](https://github.com/nodejs/node/pull/60743)
* \[[`da71ab6895`](https://github.com/nodejs/node/commit/da71ab6895)] - **deps**: V8: cherry-pick highway\@989a498fdf3 (Richard Lau) [#60682](https://github.com/nodejs/node/pull/60682)
* \[[`72d719dc00`](https://github.com/nodejs/node/commit/72d719dc00)] - **deps**: support madvise(3C) across ALL illumos revisions (Dan McDonald) [#58237](https://github.com/nodejs/node/pull/58237)
* \[[`ecca2b0d64`](https://github.com/nodejs/node/commit/ecca2b0d64)] - **deps**: define V8\_PRESERVE\_MOST as no-op on Windows (Stefan Stojanovic) [#56238](https://github.com/nodejs/node/pull/56238)
* \[[`baefd4d5e2`](https://github.com/nodejs/node/commit/baefd4d5e2)] - **deps**: patch V8 to 14.2.231.17 (Node.js GitHub Bot) [#60647](https://github.com/nodejs/node/pull/60647)
* \[[`76d6be5fc5`](https://github.com/nodejs/node/commit/76d6be5fc5)] - **deps**: patch V8 to 14.2.231.16 (Node.js GitHub Bot) [#60544](https://github.com/nodejs/node/pull/60544)
* \[[`e0ca993514`](https://github.com/nodejs/node/commit/e0ca993514)] - **deps**: patch V8 to 14.2.231.14 (Node.js GitHub Bot) [#60413](https://github.com/nodejs/node/pull/60413)
* \[[`de8386de4d`](https://github.com/nodejs/node/commit/de8386de4d)] - **deps**: V8: cherry-pick f93055fbd5aa (Olivier Flückiger) [#60105](https://github.com/nodejs/node/pull/60105)
* \[[`710105bab5`](https://github.com/nodejs/node/commit/710105bab5)] - **deps**: support madvise(3C) across ALL illumos revisions (Dan McDonald) [#58237](https://github.com/nodejs/node/pull/58237)
* \[[`6e5f3b9fe1`](https://github.com/nodejs/node/commit/6e5f3b9fe1)] - **deps**: define V8\_PRESERVE\_MOST as no-op on Windows (Stefan Stojanovic) [#56238](https://github.com/nodejs/node/pull/56238)
* \[[`b2c5235254`](https://github.com/nodejs/node/commit/b2c5235254)] - **doc**: fix stray carriage return in packages.md (Filip Skokan) [#62350](https://github.com/nodejs/node/pull/62350)
* \[[`f38a739623`](https://github.com/nodejs/node/commit/f38a739623)] - **doc**: reserve NMV 146 for Electron 42 (Niklas Wenzel) [#62124](https://github.com/nodejs/node/pull/62124)
* \[[`a57893b799`](https://github.com/nodejs/node/commit/a57893b799)] - **doc**: add Temporal section to Table of Contents (Richard Lau) [#61805](https://github.com/nodejs/node/pull/61805)
* \[[`d4cc54b8c8`](https://github.com/nodejs/node/commit/d4cc54b8c8)] - **doc**: fix v24 changelog after security release (Marco Ippolito) [#61371](https://github.com/nodejs/node/pull/61371)
* \[[`659fd01b3e`](https://github.com/nodejs/node/commit/659fd01b3e)] - **doc**: fix v22 changelog after security release (Marco Ippolito) [#61371](https://github.com/nodejs/node/pull/61371)
* \[[`6c96a63891`](https://github.com/nodejs/node/commit/6c96a63891)] - **doc**: fix v20 changelog after security release (Marco Ippolito) [#61371](https://github.com/nodejs/node/pull/61371)
* \[[`a18f8c1693`](https://github.com/nodejs/node/commit/a18f8c1693)] - **doc**: reserve NMV 145 for Electron 41 (Niklas Wenzel) [#61291](https://github.com/nodejs/node/pull/61291)
* \[[`253b16fe14`](https://github.com/nodejs/node/commit/253b16fe14)] - **doc**: add note about rust toolchain version requirement (Chengzhong Wu) [#60942](https://github.com/nodejs/node/pull/60942)
* \[[`0177491df2`](https://github.com/nodejs/node/commit/0177491df2)] - **doc**: restore REPLACEME on assert change (Michaël Zasso) [#60848](https://github.com/nodejs/node/pull/60848)
* \[[`dec0213c83`](https://github.com/nodejs/node/commit/dec0213c83)] - **doc**: add known issue to v24.11.0 release notes (Richard Lau) [#60467](https://github.com/nodejs/node/pull/60467)
* \[[`f7ca0ae765`](https://github.com/nodejs/node/commit/f7ca0ae765)] - **doc**: remove Corepack documentation page (Antoine du Hamel) [#57663](https://github.com/nodejs/node/pull/57663)
* \[[`a7d9c49490`](https://github.com/nodejs/node/commit/a7d9c49490)] - **doc**: reserve NMV 143 for Electron 40 (Shelley Vohr) [#60386](https://github.com/nodejs/node/pull/60386)
* \[[`04a086a1f4`](https://github.com/nodejs/node/commit/04a086a1f4)] - **esm**: use wasm version of cjs-module-lexer (Joyee Cheung) [#60663](https://github.com/nodejs/node/pull/60663)
* \[[`a27052f2e0`](https://github.com/nodejs/node/commit/a27052f2e0)] - _**Revert**_ "**inspector**: fix compressed responses" (Antoine du Hamel) [#61502](https://github.com/nodejs/node/pull/61502)
* \[[`186c7a9c74`](https://github.com/nodejs/node/commit/186c7a9c74)] - **inspector**: fix compressed responses (Ruben Nogueira) [#61226](https://github.com/nodejs/node/pull/61226)
* \[[`012bf70908`](https://github.com/nodejs/node/commit/012bf70908)] - **process**: optimize asyncHandledRejections by using FixedQueue (Gürgün Dayıoğlu) [#60854](https://github.com/nodejs/node/pull/60854)
* \[[`1a88acbfa2`](https://github.com/nodejs/node/commit/1a88acbfa2)] - **quic**: fixup linting/formatting issues (James M Snell) [#62387](https://github.com/nodejs/node/pull/62387)
* \[[`79b960a2bc`](https://github.com/nodejs/node/commit/79b960a2bc)] - **quic**: update http3 impl details (James M Snell) [#62387](https://github.com/nodejs/node/pull/62387)
* \[[`57186e5827`](https://github.com/nodejs/node/commit/57186e5827)] - **quic**: fix a handful of bugs and missing functionality (James M Snell) [#62387](https://github.com/nodejs/node/pull/62387)
* \[[`637bda0238`](https://github.com/nodejs/node/commit/637bda0238)] - **sqlite**: enable Percentile extension (Jurj Andrei George) [#61295](https://github.com/nodejs/node/pull/61295)
* \[[`e619adfb86`](https://github.com/nodejs/node/commit/e619adfb86)] - **src**: workaround AIX libc++ std::filesystem bug (Richard Lau) [#62788](https://github.com/nodejs/node/pull/62788)
* \[[`79262ff860`](https://github.com/nodejs/node/commit/79262ff860)] - **src**: do not enable wasm trap handler if there's not enough vmem (Joyee Cheung) [#62132](https://github.com/nodejs/node/pull/62132)
* \[[`2422ed8b5b`](https://github.com/nodejs/node/commit/2422ed8b5b)] - **src**: remove redundant `experimental_transform_types` from node\_options.h (沈鸿 飞) [#62058](https://github.com/nodejs/node/pull/62058)
* \[[`a86db6be70`](https://github.com/nodejs/node/commit/a86db6be70)] - **src**: simplify handling of kNoAuthTagLength (Tobias Nießen) [#61192](https://github.com/nodejs/node/pull/61192)
* \[[`d546e7fd0b`](https://github.com/nodejs/node/commit/d546e7fd0b)] - **src**: tag more v8 aligned pointer slots (Chengzhong Wu) [#60666](https://github.com/nodejs/node/pull/60666)
* \[[`b8e264d3c3`](https://github.com/nodejs/node/commit/b8e264d3c3)] - **src**: tag v8 aligned pointer slots with embedder data type tags (Chengzhong Wu) [#60602](https://github.com/nodejs/node/pull/60602)
* \[[`cd391b5f11`](https://github.com/nodejs/node/commit/cd391b5f11)] - **test**: wpt for Wasm jsapi including new ESM Integration tests (Guy Bedford) [#59034](https://github.com/nodejs/node/pull/59034)
* \[[`1baafcc882`](https://github.com/nodejs/node/commit/1baafcc882)] - **test**: update WPT resources, interfaces and WebCryptoAPI (Node.js GitHub Bot) [#62389](https://github.com/nodejs/node/pull/62389)
* \[[`6a84d4a17c`](https://github.com/nodejs/node/commit/6a84d4a17c)] - **tools**: update nixpkgs-unstable to 832efc09b4caf6b4569fbf9dc01bec3082a (Node.js GitHub Bot) [#62486](https://github.com/nodejs/node/pull/62486)
* \[[`a98d9f6ad7`](https://github.com/nodejs/node/commit/a98d9f6ad7)] - **tools**: update nixpkgs-unstable to 9cf7092bdd603554bd8b63c216e8943cf9b (Node.js GitHub Bot) [#62383](https://github.com/nodejs/node/pull/62383)
* \[[`f6d02af01f`](https://github.com/nodejs/node/commit/f6d02af01f)] - **tools**: update nixpkgs-unstable to f82ce7af0b79ac154b12e27ed800aeb9741 (Node.js GitHub Bot) [#62258](https://github.com/nodejs/node/pull/62258)
* \[[`5b5f069a27`](https://github.com/nodejs/node/commit/5b5f069a27)] - **tools**: bump nixpkgs-unstable pin to e38213b91d3786389a446dfce4ff5a8aaf6 (Node.js GitHub Bot) [#62052](https://github.com/nodejs/node/pull/62052)
* \[[`13eb80f3b7`](https://github.com/nodejs/node/commit/13eb80f3b7)] - **tools**: update nixpkgs-unstable to d1c15b7d5806069da59e819999d70e1cec0 (Node.js GitHub Bot) [#61931](https://github.com/nodejs/node/pull/61931)
* \[[`4d1557a744`](https://github.com/nodejs/node/commit/4d1557a744)] - **tools**: update nixpkgs-unstable to 2343bbb58f99267223bc2aac4fc9ea301a1 (Node.js GitHub Bot) [#61831](https://github.com/nodejs/node/pull/61831)
* \[[`ecd979c95a`](https://github.com/nodejs/node/commit/ecd979c95a)] - **tools**: update nixpkgs-unstable to ae67888ff7ef9dff69b3cf0cc0fbfbcd3a7 (Node.js GitHub Bot) [#61733](https://github.com/nodejs/node/pull/61733)
* \[[`7de56bdee2`](https://github.com/nodejs/node/commit/7de56bdee2)] - **tools**: update nixpkgs-unstable to 6308c3b21396534d8aaeac46179c14c439a (Node.js GitHub Bot) [#61606](https://github.com/nodejs/node/pull/61606)
* \[[`e33ce7a6fe`](https://github.com/nodejs/node/commit/e33ce7a6fe)] - **tools**: update nixpkgs-unstable to ab9fbbcf4858bd6d40ba2bbec37ceb4ab6e (Node.js GitHub Bot) [#61513](https://github.com/nodejs/node/pull/61513)
* \[[`ba05a66774`](https://github.com/nodejs/node/commit/ba05a66774)] - **tools**: update nixpkgs-unstable to be5afa0fcb31f0a96bf9ecba05a516c66fc (Node.js GitHub Bot) [#61420](https://github.com/nodejs/node/pull/61420)
* \[[`bb5d066989`](https://github.com/nodejs/node/commit/bb5d066989)] - **tools**: update nixpkgs-unstable to 3146c6aa9995e7351a398e17470e15305e6 (Node.js GitHub Bot) [#61340](https://github.com/nodejs/node/pull/61340)
* \[[`d050aa87e8`](https://github.com/nodejs/node/commit/d050aa87e8)] - **tools**: update nixpkgs-unstable to 16c7794d0a28b5a37904d55bcca36003b91 (Node.js GitHub Bot) [#61272](https://github.com/nodejs/node/pull/61272)
* \[[`2696391b18`](https://github.com/nodejs/node/commit/2696391b18)] - **tools**: update nixpkgs-unstable to 3edc4a30ed3903fdf6f90c837f961fa6b49 (Node.js GitHub Bot) [#61188](https://github.com/nodejs/node/pull/61188)
* \[[`c5d3f5f9c8`](https://github.com/nodejs/node/commit/c5d3f5f9c8)] - **tools**: update nixpkgs-unstable to 7d853e518814cca2a657b72eeba67ae20eb (Node.js GitHub Bot) [#61137](https://github.com/nodejs/node/pull/61137)
* \[[`dcb9573d0f`](https://github.com/nodejs/node/commit/dcb9573d0f)] - **tools**: update nixpkgs-unstable to f997fa0f94fb1ce55bccb97f60d41412ae8 (Node.js GitHub Bot) [#61057](https://github.com/nodejs/node/pull/61057)
* \[[`bd426739dc`](https://github.com/nodejs/node/commit/bd426739dc)] - **tools**: update nixpkgs-unstable to a672be65651c80d3f592a89b3945466584a (Node.js GitHub Bot) [#60980](https://github.com/nodejs/node/pull/60980)
* \[[`85852a3221`](https://github.com/nodejs/node/commit/85852a3221)] - **tools**: update nixpkgs-unstable to 59b6c96beacc898566c9be1052ae806f383 (Node.js GitHub Bot) [#60900](https://github.com/nodejs/node/pull/60900)
* \[[`1e7eb90b39`](https://github.com/nodejs/node/commit/1e7eb90b39)] - **tools**: update nixpkgs-unstable to a8d610af3f1a5fb71e23e08434d8d61a466 (Node.js GitHub Bot) [#60818](https://github.com/nodejs/node/pull/60818)
* \[[`fb6b83c9ef`](https://github.com/nodejs/node/commit/fb6b83c9ef)] - **tools**: lint Temporal global (René) [#60793](https://github.com/nodejs/node/pull/60793)
* \[[`adb40439ca`](https://github.com/nodejs/node/commit/adb40439ca)] - **tools**: update nixpkgs-unstable to 71cf367cc2c168b0c2959835659c38f0a34 (Node.js GitHub Bot) [#60742](https://github.com/nodejs/node/pull/60742)
* \[[`8a76958005`](https://github.com/nodejs/node/commit/8a76958005)] - **tools**: update nixpkgs-unstable to ffcdcf99d65c61956d882df249a9be53e59 (Node.js GitHub Bot) [#60315](https://github.com/nodejs/node/pull/60315)
* \[[`9120924de1`](https://github.com/nodejs/node/commit/9120924de1)] - **util**: fix nested proxy inspection (Ruben Bridgewater) [#61077](https://github.com/nodejs/node/pull/61077)
