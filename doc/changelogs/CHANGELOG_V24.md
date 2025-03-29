# Node.js 24 ChangeLog

<!--lint disable maximum-line-length no-literal-urls prohibited-strings-->

<table>
<tr>
<th>Current</th>
</tr>
<tr>
<td>
<a href="#24.0.0">24.0.0</a><br/>
</td>
</tr>
</table>

* Other Versions
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

<a id="24.0.0"></a>

## 2025-04-23, Version 24.0.0 (Current), @RafaelGSS & @juanarbol

// TODO: Intro

### Other Notable Changes

// TODO

### Semver-Major Commits

* \[[`1ba47324e0`](https://github.com/nodejs/node/commit/1ba47324e0)] - **(SEMVER-MAJOR)** **buffer**: make `buflen` in integer range (zhenweijin) [#51821](https://github.com/nodejs/node/pull/51821)
* \[[`8b40221777`](https://github.com/nodejs/node/commit/8b40221777)] - **(SEMVER-MAJOR)** **build**: bump supported macOS version to 13.5 (Michaël Zasso) [#57115](https://github.com/nodejs/node/pull/57115)
* \[[`ed52ab913b`](https://github.com/nodejs/node/commit/ed52ab913b)] - **(SEMVER-MAJOR)** **build**: increase minimum Xcode version to 16.1 (Michaël Zasso) [#56824](https://github.com/nodejs/node/pull/56824)
* \[[`89f661dd66`](https://github.com/nodejs/node/commit/89f661dd66)] - **(SEMVER-MAJOR)** **build**: link V8 with atomic library (Michaël Zasso) [#55014](https://github.com/nodejs/node/pull/55014)
* \[[`44b0e423dc`](https://github.com/nodejs/node/commit/44b0e423dc)] - **(SEMVER-MAJOR)** **build**: remove support for ppc 32-bit (Michaël Zasso) [#55014](https://github.com/nodejs/node/pull/55014)
* \[[`1f654e655c`](https://github.com/nodejs/node/commit/1f654e655c)] - **(SEMVER-MAJOR)** **build**: reset embedder string to "-node.0" (Michaël Zasso) [#55014](https://github.com/nodejs/node/pull/55014)
* \[[`1b5b019de1`](https://github.com/nodejs/node/commit/1b5b019de1)] - **(SEMVER-MAJOR)** **child\_process**: deprecate passing `args` to `spawn` and `execFile` (Daniel Venable) [#57199](https://github.com/nodejs/node/pull/57199)
* \[[`52d39441d0`](https://github.com/nodejs/node/commit/52d39441d0)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick f915fa4c9f41 (Olivier Flückiger) [#55014](https://github.com/nodejs/node/pull/55014)
* \[[`99ffe3555a`](https://github.com/nodejs/node/commit/99ffe3555a)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick 0d5d6e71bbb0 (Yagiz Nizipli) [#55014](https://github.com/nodejs/node/pull/55014)
* \[[`5d8011d91c`](https://github.com/nodejs/node/commit/5d8011d91c)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick 0c11feeeca4a (Michaël Zasso) [#55014](https://github.com/nodejs/node/pull/55014)
* \[[`d85d2f8350`](https://github.com/nodejs/node/commit/d85d2f8350)] - **(SEMVER-MAJOR)** **deps**: define V8\_PRESERVE\_MOST as no-op on Windows (Stefan Stojanovic) [#55014](https://github.com/nodejs/node/pull/55014)
* \[[`e8f55f7b7a`](https://github.com/nodejs/node/commit/e8f55f7b7a)] - **(SEMVER-MAJOR)** **deps**: always define V8\_NODISCARD as no-op (Michaël Zasso) [#55014](https://github.com/nodejs/node/pull/55014)
* \[[`b3c1b63a5d`](https://github.com/nodejs/node/commit/b3c1b63a5d)] - **(SEMVER-MAJOR)** **deps**: fix FP16 bitcasts.h (Stefan Stojanovic) [#55014](https://github.com/nodejs/node/pull/55014)
* \[[`d0361f0bba`](https://github.com/nodejs/node/commit/d0361f0bba)] - **(SEMVER-MAJOR)** **deps**: patch V8 to support compilation with MSVC (StefanStojanovic) [#55014](https://github.com/nodejs/node/pull/55014)
* \[[`a4e0fce896`](https://github.com/nodejs/node/commit/a4e0fce896)] - **(SEMVER-MAJOR)** **deps**: patch V8 to avoid duplicated zlib symbol (Michaël Zasso) [#55014](https://github.com/nodejs/node/pull/55014)
* \[[`4f8fd566cc`](https://github.com/nodejs/node/commit/4f8fd566cc)] - **(SEMVER-MAJOR)** **deps**: disable V8 concurrent sparkplug compilation (Michaël Zasso) [#55014](https://github.com/nodejs/node/pull/55014)
* \[[`1142f78f1d`](https://github.com/nodejs/node/commit/1142f78f1d)] - **(SEMVER-MAJOR)** **deps**: always define V8\_EXPORT\_PRIVATE as no-op (Michaël Zasso) [#55014](https://github.com/nodejs/node/pull/55014)
* \[[`5edec0e39a`](https://github.com/nodejs/node/commit/5edec0e39a)] - **(SEMVER-MAJOR)** **deps**: update V8 to 13.0.245.25 (Michaël Zasso) [#55014](https://github.com/nodejs/node/pull/55014)
* \[[`25b22e4754`](https://github.com/nodejs/node/commit/25b22e4754)] - **(SEMVER-MAJOR)** **deps**: upgrade npm to 11.0.0 (npm team) [#56274](https://github.com/nodejs/node/pull/56274)
* \[[`47b80c293d`](https://github.com/nodejs/node/commit/47b80c293d)] - **(SEMVER-MAJOR)** **deps**: update undici to 7.0.0 (Node.js GitHub Bot) [#56070](https://github.com/nodejs/node/pull/56070)
* \[[`a4f556fc36`](https://github.com/nodejs/node/commit/a4f556fc36)] - **(SEMVER-MAJOR)** **fs**: remove ability to call truncate with fd (Yagiz Nizipli) [#57567](https://github.com/nodejs/node/pull/57567)
* \[[`25dd206c29`](https://github.com/nodejs/node/commit/25dd206c29)] - **(SEMVER-MAJOR)** **fs**: remove `fs.F_OK`, `fs.R_OK`, `fs.W_OK`, `fs.X_OK` (Livia Medeiros) [#55862](https://github.com/nodejs/node/pull/55862)
* \[[`529b56ef9d`](https://github.com/nodejs/node/commit/529b56ef9d)] - **(SEMVER-MAJOR)** **fs**: deprecate passing invalid types in `fs.existsSync` (Carlos Espa) [#55753](https://github.com/nodejs/node/pull/55753)
* \[[`b02cd411c2`](https://github.com/nodejs/node/commit/b02cd411c2)] - **(SEMVER-MAJOR)** **fs**: runtime deprecate `fs.F_OK`, `fs.R_OK`, `fs.W_OK`, `fs.X_OK` (Livia Medeiros) [#49686](https://github.com/nodejs/node/pull/49686)
* \[[`d9540b51eb`](https://github.com/nodejs/node/commit/d9540b51eb)] - **(SEMVER-MAJOR)** **fs**: remove `dirent.path` (Antoine du Hamel) [#55548](https://github.com/nodejs/node/pull/55548)
* \[[`da906646c0`](https://github.com/nodejs/node/commit/da906646c0)] - **(SEMVER-MAJOR)** **lib**: remove obsolete Cipher export (James M Snell) [#57266](https://github.com/nodejs/node/pull/57266)
* \[[`c864dea910`](https://github.com/nodejs/node/commit/c864dea910)] - **(SEMVER-MAJOR)** **lib**: unexpose six process bindings (Michaël Zasso) [#57149](https://github.com/nodejs/node/pull/57149)
* \[[`51ae57673d`](https://github.com/nodejs/node/commit/51ae57673d)] - **(SEMVER-MAJOR)** **lib**: make ALS default to AsyncContextFrame (Stephen Belanger) [#55552](https://github.com/nodejs/node/pull/55552)
* \[[`019efe1453`](https://github.com/nodejs/node/commit/019efe1453)] - **(SEMVER-MAJOR)** **lib**: runtime deprecate SlowBuffer (Rafael Gonzaga) [#55175](https://github.com/nodejs/node/pull/55175)
* \[[`ace5548ff0`](https://github.com/nodejs/node/commit/ace5548ff0)] - **(SEMVER-MAJOR)** **net**: make \_setSimultaneousAccepts() end-of-life deprecated (Yagiz Nizipli) [#57550](https://github.com/nodejs/node/pull/57550)
* \[[`0368f2f662`](https://github.com/nodejs/node/commit/0368f2f662)] - **(SEMVER-MAJOR)** **repl**: runtime deprecate instantiating without new (Aviv Keller) [#54869](https://github.com/nodejs/node/pull/54869)
* \[[`7e8752006a`](https://github.com/nodejs/node/commit/7e8752006a)] - **(SEMVER-MAJOR)** **src**: update GetForegroundTaskRunner override (Etienne Pierre-doray) [#55014](https://github.com/nodejs/node/pull/55014)
* \[[`7917b67313`](https://github.com/nodejs/node/commit/7917b67313)] - **(SEMVER-MAJOR)** **src**: update NODE\_MODULE\_VERSION to 134 (Michaël Zasso) [#55014](https://github.com/nodejs/node/pull/55014)
* \[[`bf3bc4ec2f`](https://github.com/nodejs/node/commit/bf3bc4ec2f)] - **(SEMVER-MAJOR)** **src**: drop --experimental-permission in favour of --permission (Rafael Gonzaga) [#56240](https://github.com/nodejs/node/pull/56240)
* \[[`58982d712b`](https://github.com/nodejs/node/commit/58982d712b)] - **(SEMVER-MAJOR)** **src**: add async context frame to AsyncResource (Gerhard Stöbich) [#56082](https://github.com/nodejs/node/pull/56082)
* \[[`03dcd7077a`](https://github.com/nodejs/node/commit/03dcd7077a)] - **(SEMVER-MAJOR)** **src**: nuke deprecated and un-used enum members in `OptionEnvvarSettings` (Juan José) [#53079](https://github.com/nodejs/node/pull/53079)
* \[[`fd8de670da`](https://github.com/nodejs/node/commit/fd8de670da)] - **(SEMVER-MAJOR)** **stream**: catch and forward error from dest.write (jakecastelli) [#55270](https://github.com/nodejs/node/pull/55270)
* \[[`6857dbc018`](https://github.com/nodejs/node/commit/6857dbc018)] - **(SEMVER-MAJOR)** **test**: disable fast API call count checks (Michaël Zasso) [#55014](https://github.com/nodejs/node/pull/55014)
* \[[`1a2eb15bc6`](https://github.com/nodejs/node/commit/1a2eb15bc6)] - **(SEMVER-MAJOR)** **test\_runner**: remove promises returned by t.test() (Colin Ihrig) [#56664](https://github.com/nodejs/node/pull/56664)
* \[[`96718268fe`](https://github.com/nodejs/node/commit/96718268fe)] - **(SEMVER-MAJOR)** **test\_runner**: remove promises returned by test() (Colin Ihrig) [#56664](https://github.com/nodejs/node/pull/56664)
* \[[`aa3523ec22`](https://github.com/nodejs/node/commit/aa3523ec22)] - **(SEMVER-MAJOR)** **test\_runner**: automatically wait for subtests to finish (Colin Ihrig) [#56664](https://github.com/nodejs/node/pull/56664)
* \[[`6b0af1748c`](https://github.com/nodejs/node/commit/6b0af1748c)] - **(SEMVER-MAJOR)** **timers**: check for immediate instance in clearImmediate (Gürgün Dayıoğlu) [#57069](https://github.com/nodejs/node/pull/57069)
* \[[`5d7091f1bc`](https://github.com/nodejs/node/commit/5d7091f1bc)] - **(SEMVER-MAJOR)** **timers**: set several methods EOL (Yagiz Nizipli) [#56966](https://github.com/nodejs/node/pull/56966)
* \[[`a2a53cb728`](https://github.com/nodejs/node/commit/a2a53cb728)] - **(SEMVER-MAJOR)** **tls**: remove deprecated tls.createSecurePair (Jonas) [#57361](https://github.com/nodejs/node/pull/57361)
* \[[`388d67033d`](https://github.com/nodejs/node/commit/388d67033d)] - **(SEMVER-MAJOR)** **tls**: make server.prototype.setOptions end-of-life (Yagiz Nizipli) [#57339](https://github.com/nodejs/node/pull/57339)
* \[[`6f965260dd`](https://github.com/nodejs/node/commit/6f965260dd)] - **(SEMVER-MAJOR)** **tools**: update V8 gypfiles for 13.0 (Michaël Zasso) [#55014](https://github.com/nodejs/node/pull/55014)
* \[[`d1f8ccb10d`](https://github.com/nodejs/node/commit/d1f8ccb10d)] - **(SEMVER-MAJOR)** **url**: expose urlpattern as global (Jonas) [#56950](https://github.com/nodejs/node/pull/56950)
* \[[`11fbdd8c9d`](https://github.com/nodejs/node/commit/11fbdd8c9d)] - **(SEMVER-MAJOR)** **url**: runtime deprecate url.parse (Yagiz Nizipli) [#55017](https://github.com/nodejs/node/pull/55017)
* \[[`4ee87b8bc3`](https://github.com/nodejs/node/commit/4ee87b8bc3)] - **(SEMVER-MAJOR)** **zlib**: deprecate classes usage without `new` (Yagiz Nizipli) [#55718](https://github.com/nodejs/node/pull/55718)

### Semver-Minor Commits

TBD

### Semver-Patch Commits

TBD
