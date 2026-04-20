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

## 2026-04-22, Version 26.0.0 (Current), @RafaelGSS

TBD

### Notable Changes

TBD

### Deprecations and Removals

TBD

### Semver-Major Commits

* \[[`d3f79aa65d`](https://github.com/nodejs/node/commit/d3f79aa65d)] - **(SEMVER-MAJOR)** **assert**: allow printf-style messages as assertion error (Ruben Bridgewater) [#58849](https://github.com/nodejs/node/pull/58849)
* \[[`0998c37eb6`](https://github.com/nodejs/node/commit/0998c37eb6)] - **(SEMVER-MAJOR)** **build**: target Power 9 for AIX/IBM i (Richard Lau) [#62296](https://github.com/nodejs/node/pull/62296)
* \[[`d73c49e849`](https://github.com/nodejs/node/commit/d73c49e849)] - **(SEMVER-MAJOR)** **build**: drop support for Python 3.9 (Mike McCready) [#61177](https://github.com/nodejs/node/pull/61177)
* \[[`3c92ee1008`](https://github.com/nodejs/node/commit/3c92ee1008)] - **(SEMVER-MAJOR)** **build**: enable maglev for Linux on s390x (Richard Lau) [#60863](https://github.com/nodejs/node/pull/60863)
* \[[`908c468828`](https://github.com/nodejs/node/commit/908c468828)] - **(SEMVER-MAJOR)** **build**: reset embedder string to "-node.0" (Michaël Zasso) [#60488](https://github.com/nodejs/node/pull/60488)
* \[[`6380fbb5ee`](https://github.com/nodejs/node/commit/6380fbb5ee)] - **(SEMVER-MAJOR)** **build**: reset embedder string to "-node.0" (Michaël Zasso) [#60111](https://github.com/nodejs/node/pull/60111)
* \[[`adac077484`](https://github.com/nodejs/node/commit/adac077484)] - **(SEMVER-MAJOR)** **crypto**: runtime-deprecate DEP0203 and DEP0204 (Filip Skokan) [#62453](https://github.com/nodejs/node/pull/62453)
* \[[`74509b166a`](https://github.com/nodejs/node/commit/74509b166a)] - **(SEMVER-MAJOR)** **crypto**: decorate async crypto job errors with OpenSSL error details (Filip Skokan) [#62348](https://github.com/nodejs/node/pull/62348)
* \[[`da5843b91d`](https://github.com/nodejs/node/commit/da5843b91d)] - **(SEMVER-MAJOR)** **crypto**: default ML-KEM and ML-DSA pkcs8 export to seed-only format (Filip Skokan) [#62178](https://github.com/nodejs/node/pull/62178)
* \[[`dff46c07c3`](https://github.com/nodejs/node/commit/dff46c07c3)] - **(SEMVER-MAJOR)** **crypto**: move DEP0182 to End-of-Life (Tobias Nießen) [#61084](https://github.com/nodejs/node/pull/61084)
* \[[`94cd600542`](https://github.com/nodejs/node/commit/94cd600542)] - **(SEMVER-MAJOR)** **crypto**: fix DOMException name for non-extractable key error (Filip Skokan) [#60830](https://github.com/nodejs/node/pull/60830)
* \[[`bf5c6a8bd4`](https://github.com/nodejs/node/commit/bf5c6a8bd4)] - **(SEMVER-MAJOR)** **deps**: V8: backport 151d0a44a1b2 (Abdirahim Musse) [#60488](https://github.com/nodejs/node/pull/60488)
* \[[`b59af772dc`](https://github.com/nodejs/node/commit/b59af772dc)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick 47800791b35c (Jakob Kummerow) [#60488](https://github.com/nodejs/node/pull/60488)
* \[[`5e41e5228a`](https://github.com/nodejs/node/commit/5e41e5228a)] - **(SEMVER-MAJOR)** **deps**: patch V8 for illumos (Dan McDonald) [#59805](https://github.com/nodejs/node/pull/59805)
* \[[`2243e58e43`](https://github.com/nodejs/node/commit/2243e58e43)] - **(SEMVER-MAJOR)** **deps**: use std::map in MSVC STL for EphemeronRememberedSet (Joyee Cheung) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`4157964c42`](https://github.com/nodejs/node/commit/4157964c42)] - **(SEMVER-MAJOR)** **deps**: remove problematic comment from v8-internal (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`7c8483a4e9`](https://github.com/nodejs/node/commit/7c8483a4e9)] - **(SEMVER-MAJOR)** **deps**: patch V8 to avoid duplicated zlib symbol (Michaël Zasso) [#54077](https://github.com/nodejs/node/pull/54077)
* \[[`53379f3706`](https://github.com/nodejs/node/commit/53379f3706)] - **(SEMVER-MAJOR)** **deps**: update V8 to 14.3.127.12 (Michaël Zasso) [#60488](https://github.com/nodejs/node/pull/60488)
* \[[`f9a83ffcd9`](https://github.com/nodejs/node/commit/f9a83ffcd9)] - **(SEMVER-MAJOR)** **deps**: V8: backport fe81545e6d14 (Caio Lima) [#60429](https://github.com/nodejs/node/pull/60429)
* \[[`f819aec288`](https://github.com/nodejs/node/commit/f819aec288)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick ff34ae20c8e3 (Chengzhong Wu) [#60111](https://github.com/nodejs/node/pull/60111)
* \[[`1acd8df36f`](https://github.com/nodejs/node/commit/1acd8df36f)] - **(SEMVER-MAJOR)** **deps**: V8: backport fed47445bbdd (Abdirahim Musse) [#60111](https://github.com/nodejs/node/pull/60111)
* \[[`46f72577a4`](https://github.com/nodejs/node/commit/46f72577a4)] - **(SEMVER-MAJOR)** **deps**: patch V8 for illumos (Dan McDonald) [#59805](https://github.com/nodejs/node/pull/59805)
* \[[`39eb88eaa8`](https://github.com/nodejs/node/commit/39eb88eaa8)] - **(SEMVER-MAJOR)** **deps**: use std::map in MSVC STL for EphemeronRememberedSet (Joyee Cheung) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`ea3d14eadb`](https://github.com/nodejs/node/commit/ea3d14eadb)] - **(SEMVER-MAJOR)** **deps**: remove problematic comment from v8-internal (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`7bc0f245b4`](https://github.com/nodejs/node/commit/7bc0f245b4)] - **(SEMVER-MAJOR)** **deps**: patch V8 to avoid duplicated zlib symbol (Michaël Zasso) [#54077](https://github.com/nodejs/node/pull/54077)
* \[[`c2843b722c`](https://github.com/nodejs/node/commit/c2843b722c)] - **(SEMVER-MAJOR)** **deps**: update V8 to 14.2.231.9 (Michaël Zasso) [#60111](https://github.com/nodejs/node/pull/60111)
* \[[`b4ea323833`](https://github.com/nodejs/node/commit/b4ea323833)] - **(SEMVER-MAJOR)** **diagnostics\_channel**: ensure tracePromise consistency with non-Promises (René) [#61766](https://github.com/nodejs/node/pull/61766)
* \[[`ef0f0b0865`](https://github.com/nodejs/node/commit/ef0f0b0865)] - **(SEMVER-MAJOR)** **doc**: update supported Windows SDK version to 11 (Mike McCready) [#61973](https://github.com/nodejs/node/pull/61973)
* \[[`a00d95c73d`](https://github.com/nodejs/node/commit/a00d95c73d)] - **(SEMVER-MAJOR)** **doc**: drop p8 and z13 support (Milad Fa) [#61005](https://github.com/nodejs/node/pull/61005)
* \[[`93c25815ee`](https://github.com/nodejs/node/commit/93c25815ee)] - **(SEMVER-MAJOR)** **http**: move writeHeader to end-of-life (Sebastian Beltran) [#60635](https://github.com/nodejs/node/pull/60635)
* \[[`4346c0f7a7`](https://github.com/nodejs/node/commit/4346c0f7a7)] - **(SEMVER-MAJOR)** **http**: fix handling of HTTP upgrades with bodies (Tim Perry) [#60016](https://github.com/nodejs/node/pull/60016)
* \[[`fa70327610`](https://github.com/nodejs/node/commit/fa70327610)] - **(SEMVER-MAJOR)** **lib**: return undefined for localStorage without file (Matteo Collina) [#61333](https://github.com/nodejs/node/pull/61333)
* \[[`b328bf74bd`](https://github.com/nodejs/node/commit/b328bf74bd)] - **(SEMVER-MAJOR)** **lib,src**: implement QuotaExceededError as DOMException-derived interface (Filip Skokan) [#62293](https://github.com/nodejs/node/pull/62293)
* \[[`89f4b6cddb`](https://github.com/nodejs/node/commit/89f4b6cddb)] - **(SEMVER-MAJOR)** **module**: remove --experimental-transform-types (Marco Ippolito) [#61803](https://github.com/nodejs/node/pull/61803)
* \[[`8480f87375`](https://github.com/nodejs/node/commit/8480f87375)] - **(SEMVER-MAJOR)** **src**: remove deprecated and unused isolate fields (Michaël Zasso) [#60488](https://github.com/nodejs/node/pull/60488)
* \[[`70b6bd8e19`](https://github.com/nodejs/node/commit/70b6bd8e19)] - **(SEMVER-MAJOR)** **src**: update NODE\_MODULE\_VERSION to 144 (Michaël Zasso) [#60488](https://github.com/nodejs/node/pull/60488)
* \[[`7d2bc5249b`](https://github.com/nodejs/node/commit/7d2bc5249b)] - **(SEMVER-MAJOR)** **src**: include `node_api_types.h` instead of `node_api.h` in `node.h` (Anna Henningsen) [#60496](https://github.com/nodejs/node/pull/60496)
* \[[`91ab1101bc`](https://github.com/nodejs/node/commit/91ab1101bc)] - **(SEMVER-MAJOR)** **src**: update NODE\_MODULE\_VERSION to 142 (Michaël Zasso) [#60111](https://github.com/nodejs/node/pull/60111)
* \[[`ac6375417a`](https://github.com/nodejs/node/commit/ac6375417a)] - **(SEMVER-MAJOR)** **stream**: promote DEP0201 to runtime deprecation (René) [#62173](https://github.com/nodejs/node/pull/62173)
* \[[`c755b0113c`](https://github.com/nodejs/node/commit/c755b0113c)] - **(SEMVER-MAJOR)** **stream**: move \_stream\_\* to end-of-life (Sebastian Beltran) [#60657](https://github.com/nodejs/node/pull/60657)
* \[[`fadb214d95`](https://github.com/nodejs/node/commit/fadb214d95)] - **(SEMVER-MAJOR)** **stream**: readable read one buffer at a time (Robert Nagy) [#60441](https://github.com/nodejs/node/pull/60441)
* \[[`4fe325d93d`](https://github.com/nodejs/node/commit/4fe325d93d)] - **(SEMVER-MAJOR)** **stream**: preserve AsyncLocalStorage on finished only when needed (avcribl) [#59873](https://github.com/nodejs/node/pull/59873)
* \[[`092a448ad0`](https://github.com/nodejs/node/commit/092a448ad0)] - **(SEMVER-MAJOR)** **test**: fix test-linux-perf-logger for V8 14.3 (Michaël Zasso) [#60488](https://github.com/nodejs/node/pull/60488)
* \[[`dc97b507d0`](https://github.com/nodejs/node/commit/dc97b507d0)] - **(SEMVER-MAJOR)** **util**: mark proxied objects as such when inspecting them (Ruben Bridgewater) [#61029](https://github.com/nodejs/node/pull/61029)
* \[[`ddbe1365ff`](https://github.com/nodejs/node/commit/ddbe1365ff)] - **(SEMVER-MAJOR)** **util**: reduce TextEncoder.encodeInto function size (Yagiz Nizipli) [#60339](https://github.com/nodejs/node/pull/60339)

### Semver-Minor Commits

TBD

### Semver-Patch Commits

TBD
