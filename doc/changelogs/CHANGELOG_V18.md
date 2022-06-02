# Node.js 18 ChangeLog

<!--lint disable maximum-line-length no-literal-urls prohibited-strings-->

<table>
<tr>
<th>Current</th>
</tr>
<tr>
<td>
<a href="#18.3.0">18.3.0</a><br/>
<a href="#18.2.0">18.2.0</a><br/>
<a href="#18.1.0">18.1.0</a><br/>
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

<a id="18.3.0"></a>

## 2022-06-01, Version 18.3.0 (Current), @bengl

### Notable Changes

* \[[`dc3b91f351`](https://github.com/nodejs/node/commit/dc3b91f351)] - **deps**: update undici to 5.4.0  (Node.js GitHub Bot) [#43262](https://github.com/nodejs/node/pull/43262)
* \[[`d6cf409d78`](https://github.com/nodejs/node/commit/d6cf409d78)] - **(SEMVER-MINOR)** **util**: add parseArgs module (Benjamin Coe) [#42675](https://github.com/nodejs/node/pull/42675)
* \[[`9539cfa358`](https://github.com/nodejs/node/commit/9539cfa358)] - **(SEMVER-MINOR)** **http**: add uniqueHeaders option to request and createServer (Paolo Insogna) [#41397](https://github.com/nodejs/node/pull/41397)
* \[[`41fdc2617d`](https://github.com/nodejs/node/commit/41fdc2617d)] - **deps**: upgrade npm to 8.11.0 (npm team) [#43210](https://github.com/nodejs/node/pull/43210)
* \[[`0000654e47`](https://github.com/nodejs/node/commit/0000654e47)] - **deps**: patch V8 to 10.2.154.4 (Michaël Zasso) [#43067](https://github.com/nodejs/node/pull/43067)
* \[[`b3c8e609fd`](https://github.com/nodejs/node/commit/b3c8e609fd)] - **(SEMVER-MINOR)** **deps**: update V8 to 10.2.154.2 (Michaël Zasso) [#42740](https://github.com/nodejs/node/pull/42740)
* \[[`3e89b7336d`](https://github.com/nodejs/node/commit/3e89b7336d)] - **(SEMVER-MINOR)** **fs**: make params in writing methods optional (LiviaMedeiros) [#42601](https://github.com/nodejs/node/pull/42601)
* \[[`9539cfa358`](https://github.com/nodejs/node/commit/9539cfa358)] - **(SEMVER-MINOR)** **http**: add uniqueHeaders option to request and createServer (Paolo Insogna) [#41397](https://github.com/nodejs/node/pull/41397)
* \[[`8f5b4570e5`](https://github.com/nodejs/node/commit/8f5b4570e5)] - **(SEMVER-MINOR)** **net**: add ability to reset a tcp socket (pupilTong) [#43112](https://github.com/nodejs/node/pull/43112)
* \[[`5eff7b4a6a`](https://github.com/nodejs/node/commit/5eff7b4a6a)] - **(SEMVER-MINOR)** _**Revert**_ "**build**: make x86 Windows support temporarily experimental" (Michaël Zasso) [#42740](https://github.com/nodejs/node/pull/42740)
  * This means 32-bit Windows binaries are back with this release.

### Commits

* \[[`aefc9dda9a`](https://github.com/nodejs/node/commit/aefc9dda9a)] - **benchmark**: add node-error benchmark (RafaelGSS) [#43077](https://github.com/nodejs/node/pull/43077)
* \[[`85d81a764f`](https://github.com/nodejs/node/commit/85d81a764f)] - **bootstrap**: include code cache in the embedded snapshot (Joyee Cheung) [#43023](https://github.com/nodejs/node/pull/43023)
* \[[`5eff7b4a6a`](https://github.com/nodejs/node/commit/5eff7b4a6a)] - **(SEMVER-MINOR)** _**Revert**_ "**build**: make x86 Windows support temporarily experimental" (Michaël Zasso) [#42740](https://github.com/nodejs/node/pull/42740)
* \[[`d6634707c5`](https://github.com/nodejs/node/commit/d6634707c5)] - **(SEMVER-MINOR)** **build**: run V8 tests with detected Python version (Richard Lau) [#42740](https://github.com/nodejs/node/pull/42740)
* \[[`b8b5e6df67`](https://github.com/nodejs/node/commit/b8b5e6df67)] - **(SEMVER-MINOR)** **build**: reset embedder string to "-node.0" (Michaël Zasso) [#42740](https://github.com/nodejs/node/pull/42740)
* \[[`285ef30877`](https://github.com/nodejs/node/commit/285ef30877)] - **console**: fix console.dir crash on a revoked proxy (Daeyeon Jeong) [#43100](https://github.com/nodejs/node/pull/43100)
* \[[`920d8c5300`](https://github.com/nodejs/node/commit/920d8c5300)] - **crypto**: align webcrypto RSA key import/export with other implementations (Filip Skokan) [#42816](https://github.com/nodejs/node/pull/42816)
* \[[`c76caadf2e`](https://github.com/nodejs/node/commit/c76caadf2e)] - **debugger**: throw a more useful error when the frame is missing (Kohei Ueno) [#42776](https://github.com/nodejs/node/pull/42776)
* \[[`dc3b91f351`](https://github.com/nodejs/node/commit/dc3b91f351)] - **deps**: update undici to 5.4.0  (Node.js GitHub Bot) [#43262](https://github.com/nodejs/node/pull/43262)
* \[[`35250bf2f6`](https://github.com/nodejs/node/commit/35250bf2f6)] - **deps**: regenerate OpenSSL archs files (Daniel Bevenius) [#42973](https://github.com/nodejs/node/pull/42973)
* \[[`ecacc3a727`](https://github.com/nodejs/node/commit/ecacc3a727)] - **deps**: exclude linker scripts for windows builds (Daniel Bevenius) [#42973](https://github.com/nodejs/node/pull/42973)
* \[[`41fdc2617d`](https://github.com/nodejs/node/commit/41fdc2617d)] - **deps**: upgrade npm to 8.11.0 (npm team) [#43210](https://github.com/nodejs/node/pull/43210)
* \[[`87b248e27e`](https://github.com/nodejs/node/commit/87b248e27e)] - **deps**: update undici to 5.3.0 (Node.js GitHub Bot) [#43197](https://github.com/nodejs/node/pull/43197)
* \[[`d42de132a7`](https://github.com/nodejs/node/commit/d42de132a7)] - **deps**: upgrade npm to 8.10.0 (npm team) [#43061](https://github.com/nodejs/node/pull/43061)
* \[[`0000654e47`](https://github.com/nodejs/node/commit/0000654e47)] - **deps**: patch V8 to 10.2.154.4 (Michaël Zasso) [#43067](https://github.com/nodejs/node/pull/43067)
* \[[`742ffefb44`](https://github.com/nodejs/node/commit/742ffefb44)] - **(SEMVER-MINOR)** **deps**: make V8 10.2 ABI-compatible with 10.1 (Michaël Zasso) [#42740](https://github.com/nodejs/node/pull/42740)
* \[[`c626a533c7`](https://github.com/nodejs/node/commit/c626a533c7)] - **deps**: make V8 compilable with older glibc (Michaël Zasso) [#42657](https://github.com/nodejs/node/pull/42657)
* \[[`76546b12a2`](https://github.com/nodejs/node/commit/76546b12a2)] - **deps**: V8: fix v8-cppgc.h for MSVC (Jiawen Geng) [#42657](https://github.com/nodejs/node/pull/42657)
* \[[`45e1fd4473`](https://github.com/nodejs/node/commit/45e1fd4473)] - **deps**: silence V8's warning on CompileFunction (Michaël Zasso) [#40907](https://github.com/nodejs/node/pull/40907)
* \[[`2130891a9a`](https://github.com/nodejs/node/commit/2130891a9a)] - **(SEMVER-MINOR)** **deps**: disable trap handler for Windows cross-compiler (Michaël Zasso) [#40488](https://github.com/nodejs/node/pull/40488)
* \[[`e678b6c63d`](https://github.com/nodejs/node/commit/e678b6c63d)] - **deps**: fix V8 build issue with inline methods (Jiawen Geng) [#38807](https://github.com/nodejs/node/pull/38807)
* \[[`f83323f304`](https://github.com/nodejs/node/commit/f83323f304)] - **deps**: V8: forward declaration of `Rtl*FunctionTable` (Refael Ackermann) [#32116](https://github.com/nodejs/node/pull/32116)
* \[[`4be7584ca6`](https://github.com/nodejs/node/commit/4be7584ca6)] - **deps**: V8: un-cherry-pick bd019bd (Refael Ackermann) [#32116](https://github.com/nodejs/node/pull/32116)
* \[[`b3c8e609fd`](https://github.com/nodejs/node/commit/b3c8e609fd)] - **(SEMVER-MINOR)** **deps**: update V8 to 10.2.154.2 (Michaël Zasso) [#42740](https://github.com/nodejs/node/pull/42740)
* \[[`0afdc3e9a8`](https://github.com/nodejs/node/commit/0afdc3e9a8)] - **doc**: use serial comma in errors docs (Tobias Nießen) [#43242](https://github.com/nodejs/node/pull/43242)
* \[[`00989338b3`](https://github.com/nodejs/node/commit/00989338b3)] - **doc**: add note regarding `%Array.prototype.concat%` in `primordials.md` (Antoine du Hamel) [#43166](https://github.com/nodejs/node/pull/43166)
* \[[`badf72dd0a`](https://github.com/nodejs/node/commit/badf72dd0a)] - **doc**: use serial comma in worker\_threads docs (Tobias Nießen) [#43220](https://github.com/nodejs/node/pull/43220)
* \[[`776e746f0a`](https://github.com/nodejs/node/commit/776e746f0a)] - **doc**: fix napi version for node\_api\_symbol\_for (Danielle Adams) [#42878](https://github.com/nodejs/node/pull/42878)
* \[[`76b46801f8`](https://github.com/nodejs/node/commit/76b46801f8)] - **doc**: document `signal` option for `EventTarget#addEventListener` (Antoine du Hamel) [#43170](https://github.com/nodejs/node/pull/43170)
* \[[`3082c75efd`](https://github.com/nodejs/node/commit/3082c75efd)] - **doc**: make minor adjustments (LiviaMedeiros) [#43175](https://github.com/nodejs/node/pull/43175)
* \[[`598c1f102e`](https://github.com/nodejs/node/commit/598c1f102e)] - **doc**: use serial comma in dgram docs (Tobias Nießen) [#43191](https://github.com/nodejs/node/pull/43191)
* \[[`b772c13a62`](https://github.com/nodejs/node/commit/b772c13a62)] - **doc**: use serial comma in process docs (Tobias Nießen) [#43179](https://github.com/nodejs/node/pull/43179)
* \[[`f90a3d7a54`](https://github.com/nodejs/node/commit/f90a3d7a54)] - **doc**: improved parallel specification (mawaregetsuka) [#42679](https://github.com/nodejs/node/pull/42679)
* \[[`71074eedef`](https://github.com/nodejs/node/commit/71074eedef)] - **doc**: improve callback params for `fs.mkdir` (Daeyeon Jeong) [#43016](https://github.com/nodejs/node/pull/43016)
* \[[`2b8b224077`](https://github.com/nodejs/node/commit/2b8b224077)] - **doc**: remove outdated footnote (Python 2 --> 3 for V8 tests) (DeeDeeG) [#43105](https://github.com/nodejs/node/pull/43105)
* \[[`945f228cf1`](https://github.com/nodejs/node/commit/945f228cf1)] - **doc**: add release key for RafaelGSS (Rafael Gonzaga) [#43131](https://github.com/nodejs/node/pull/43131)
* \[[`56fc712f93`](https://github.com/nodejs/node/commit/56fc712f93)] - **doc**: use serial comma in assert docs (Tobias Nießen) [#43154](https://github.com/nodejs/node/pull/43154)
* \[[`093a3cf2f1`](https://github.com/nodejs/node/commit/093a3cf2f1)] - **doc**: fix errors in Performance hooks doc (OneNail) [#43152](https://github.com/nodejs/node/pull/43152)
* \[[`4d9f43a8a7`](https://github.com/nodejs/node/commit/4d9f43a8a7)] - **doc**: use serial comma in dns docs (Tobias Nießen) [#43145](https://github.com/nodejs/node/pull/43145)
* \[[`4e9393e32f`](https://github.com/nodejs/node/commit/4e9393e32f)] - **doc**: use ASCII apostrophes consistently (Tobias Nießen) [#43114](https://github.com/nodejs/node/pull/43114)
* \[[`b3181851d7`](https://github.com/nodejs/node/commit/b3181851d7)] - **doc**: add strategic initiative for shadow realm (Chengzhong Wu) [#43037](https://github.com/nodejs/node/pull/43037)
* \[[`6f48dfb499`](https://github.com/nodejs/node/commit/6f48dfb499)] - **fs**: add trailing commas (LiviaMedeiros) [#43127](https://github.com/nodejs/node/pull/43127)
* \[[`3e89b7336d`](https://github.com/nodejs/node/commit/3e89b7336d)] - **(SEMVER-MINOR)** **fs**: make params in writing methods optional (LiviaMedeiros) [#42601](https://github.com/nodejs/node/pull/42601)
* \[[`9539cfa358`](https://github.com/nodejs/node/commit/9539cfa358)] - **(SEMVER-MINOR)** **http**: add uniqueHeaders option to request and createServer (Paolo Insogna) [#41397](https://github.com/nodejs/node/pull/41397)
* \[[`b187060f76`](https://github.com/nodejs/node/commit/b187060f76)] - **http2**: set origin name correctly when servername is empty (ofirbarak) [#42838](https://github.com/nodejs/node/pull/42838)
* \[[`a07f5f28f3`](https://github.com/nodejs/node/commit/a07f5f28f3)] - **http2**: improve tests and docs (Daeyeon Jeong) [#42858](https://github.com/nodejs/node/pull/42858)
* \[[`701d40496d`](https://github.com/nodejs/node/commit/701d40496d)] - **lib**: refactor `validateInt32` and `validateUint32` (mawaregetsuka) [#43071](https://github.com/nodejs/node/pull/43071)
* \[[`09da76493a`](https://github.com/nodejs/node/commit/09da76493a)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#43231](https://github.com/nodejs/node/pull/43231)
* \[[`1da6b7c32b`](https://github.com/nodejs/node/commit/1da6b7c32b)] - **meta**: add mailmap entry for legendecas (Chengzhong Wu) [#43156](https://github.com/nodejs/node/pull/43156)
* \[[`1be254b24e`](https://github.com/nodejs/node/commit/1be254b24e)] - **meta**: add mailmap entry for npm team (Luigi Pinca) [#43143](https://github.com/nodejs/node/pull/43143)
* \[[`7d8d9d625a`](https://github.com/nodejs/node/commit/7d8d9d625a)] - **meta**: add mailmap entry for Morgan Roderick (Luigi Pinca) [#43144](https://github.com/nodejs/node/pull/43144)
* \[[`8f5b4570e5`](https://github.com/nodejs/node/commit/8f5b4570e5)] - **(SEMVER-MINOR)** **net**: add ability to reset a tcp socket (pupilTong) [#43112](https://github.com/nodejs/node/pull/43112)
* \[[`11c783fa63`](https://github.com/nodejs/node/commit/11c783fa63)] - **net**: remoteAddress always undefined called before connected (OneNail) [#43011](https://github.com/nodejs/node/pull/43011)
* \[[`9c4e070567`](https://github.com/nodejs/node/commit/9c4e070567)] - **(SEMVER-MINOR)** **node-api**: emit uncaught-exception on unhandled tsfn callbacks (Chengzhong Wu) [#36510](https://github.com/nodejs/node/pull/36510)
* \[[`54d68b1afd`](https://github.com/nodejs/node/commit/54d68b1afd)] - **perf\_hooks**: fix start\_time of perf\_hooks (theanarkh) [#43069](https://github.com/nodejs/node/pull/43069)
* \[[`baa5d0005a`](https://github.com/nodejs/node/commit/baa5d0005a)] - **src**: refactor GetCipherValue and related functions (Tobias Nießen) [#43171](https://github.com/nodejs/node/pull/43171)
* \[[`6ee9fb14e4`](https://github.com/nodejs/node/commit/6ee9fb14e4)] - **src**: make SecureContext fields private (Tobias Nießen) [#43173](https://github.com/nodejs/node/pull/43173)
* \[[`1e2552ba6c`](https://github.com/nodejs/node/commit/1e2552ba6c)] - **src**: reuse GetServerName (Tobias Nießen) [#43168](https://github.com/nodejs/node/pull/43168)
* \[[`d8a14a2486`](https://github.com/nodejs/node/commit/d8a14a2486)] - **src**: fix static analysis warning and use smart ptr (Tobias Nießen) [#43117](https://github.com/nodejs/node/pull/43117)
* \[[`72a767b7ca`](https://github.com/nodejs/node/commit/72a767b7ca)] - **src**: remove SecureContext::operator\* (Tobias Nießen) [#43121](https://github.com/nodejs/node/pull/43121)
* \[[`82fb037388`](https://github.com/nodejs/node/commit/82fb037388)] - **src**: move context snapshot index to SnapshotData (Joyee Cheung) [#43023](https://github.com/nodejs/node/pull/43023)
* \[[`9577878258`](https://github.com/nodejs/node/commit/9577878258)] - **src**: replace TraceEventScope with sync events (Chengzhong Wu) [#42977](https://github.com/nodejs/node/pull/42977)
* \[[`41b69e3cf4`](https://github.com/nodejs/node/commit/41b69e3cf4)] - **src,lib**: migrate to console on context's extra binding (Chengzhong Wu) [#43142](https://github.com/nodejs/node/pull/43142)
* \[[`3e1ed1ee0c`](https://github.com/nodejs/node/commit/3e1ed1ee0c)] - **test**: improve code coverage for inspector connection errors (Kohei Ueno) [#42310](https://github.com/nodejs/node/pull/42310)
* \[[`d7ca2234dc`](https://github.com/nodejs/node/commit/d7ca2234dc)] - **test**: use mustSucceed instead of mustCall with assert.ifError (MURAKAMI Masahiko) [#43188](https://github.com/nodejs/node/pull/43188)
* \[[`9cc5ce6a24`](https://github.com/nodejs/node/commit/9cc5ce6a24)] - **test**: improve readline/emitKeypressEvents.js coverage (OneNail) [#42908](https://github.com/nodejs/node/pull/42908)
* \[[`447bbd0d66`](https://github.com/nodejs/node/commit/447bbd0d66)] - **tls**: fix convertALPNProtocols accepting ArrayBufferViews (LiviaMedeiros) [#43211](https://github.com/nodejs/node/pull/43211)
* \[[`df691464aa`](https://github.com/nodejs/node/commit/df691464aa)] - **tools**: update lint-md-dependencies to rollup\@2.75.1 (Node.js GitHub Bot) [#43230](https://github.com/nodejs/node/pull/43230)
* \[[`d9fb25c936`](https://github.com/nodejs/node/commit/d9fb25c936)] - **tools**: refactor build-addons.js to ESM (Feng Yu) [#43099](https://github.com/nodejs/node/pull/43099)
* \[[`0efeceb4a6`](https://github.com/nodejs/node/commit/0efeceb4a6)] - **tools**: update lint-md-dependencies to rollup\@2.74.1 (Node.js GitHub Bot) [#43172](https://github.com/nodejs/node/pull/43172)
* \[[`ed352a945c`](https://github.com/nodejs/node/commit/ed352a945c)] - **tools**: update eslint to 8.16.0 (Node.js GitHub Bot) [#43174](https://github.com/nodejs/node/pull/43174)
* \[[`1183058908`](https://github.com/nodejs/node/commit/1183058908)] - **tools**: refactor update-authors.js to ESM (Feng Yu) [#43098](https://github.com/nodejs/node/pull/43098)
* \[[`5228028962`](https://github.com/nodejs/node/commit/5228028962)] - **(SEMVER-MINOR)** **tools**: update V8 gypfiles for 10.2 (Michaël Zasso) [#42740](https://github.com/nodejs/node/pull/42740)
* \[[`d6cf409d78`](https://github.com/nodejs/node/commit/d6cf409d78)] - **(SEMVER-MINOR)** **util**: add parseArgs module (Benjamin Coe) [#42675](https://github.com/nodejs/node/pull/42675)
* \[[`d91b489784`](https://github.com/nodejs/node/commit/d91b489784)] - **worker**: fix heap snapshot crash on exit (Chengzhong Wu) [#43123](https://github.com/nodejs/node/pull/43123)

<a id="18.2.0"></a>

## 2022-05-17, Version 18.2.0 (Current), @BethGriggs prepared by @RafaelGSS

### Notable Changes

#### OpenSSL 3.0.3

This update can be treated as a security release as the issues addressed in OpenSSL 3.0.3 slightly affect Node.js 18.
See <https://nodejs.org/en/blog/vulnerability/openssl-fixes-in-regular-releases-may2022/> for more information on how the May 2022 OpenSSL releases affect other Node.js release lines.

* \[[`8e54c19a6e`](https://github.com/nodejs/node/commit/8e54c19a6e)] - **deps**: update archs files for quictls/openssl-3.0.3+quic (RafaelGSS) [#43022](https://github.com/nodejs/node/pull/43022)
* \[[`6365bf808e`](https://github.com/nodejs/node/commit/6365bf808e)] - **deps**: upgrade openssl sources to quictls/openssl-3.0.3 (RafaelGSS) [#43022](https://github.com/nodejs/node/pull/43022)

#### Other Notable Changes

* \[[`13c333e533`](https://github.com/nodejs/node/commit/13c333e533)] - _**Revert**_ "**deps**: add template for generated headers" (Daniel Bevenius) [#42978](https://github.com/nodejs/node/pull/42978)
* \[[`d128356a7f`](https://github.com/nodejs/node/commit/d128356a7f)] - **deps**: update undici to 5.2.0 (Node.js GitHub Bot) [#43059](https://github.com/nodejs/node/pull/43059)
* \[[`2df1624f80`](https://github.com/nodejs/node/commit/2df1624f80)] - **deps**: upgrade npm to 8.9.0 (npm team) [#42968](https://github.com/nodejs/node/pull/42968)
* \[[`6365bf808e`](https://github.com/nodejs/node/commit/6365bf808e)] - **deps**: upgrade openssl sources to quictls/openssl-3.0.3 (RafaelGSS) [#43022](https://github.com/nodejs/node/pull/43022)
* \[[`4a3f678e70`](https://github.com/nodejs/node/commit/4a3f678e70)] - **doc**: add LiviaMedeiros to collaborators (LiviaMedeiros) [#43039](https://github.com/nodejs/node/pull/43039)
* \[[`686c4c1f6f`](https://github.com/nodejs/node/commit/686c4c1f6f)] - **doc**: add release key for Juan Arboleda (Juan José) [#42961](https://github.com/nodejs/node/pull/42961)
* \[[`784d84cf34`](https://github.com/nodejs/node/commit/784d84cf34)] - **(SEMVER-MINOR)** **fs**: add `read(buffer[, options])` versions (LiviaMedeiros) [#42768](https://github.com/nodejs/node/pull/42768)
* \[[`2f192c4be0`](https://github.com/nodejs/node/commit/2f192c4be0)] - **(SEMVER-MINOR)** **http**: added connection closing methods (Paolo Insogna) [#42812](https://github.com/nodejs/node/pull/42812)
* \[[`c92e291beb`](https://github.com/nodejs/node/commit/c92e291beb)] - **(SEMVER-MINOR)** **perf\_hooks**: add PerformanceResourceTiming (RafaelGSS) [#42725](https://github.com/nodejs/node/pull/42725)

### Commits

* \[[`7cac7bb806`](https://github.com/nodejs/node/commit/7cac7bb806)] - **assert**: fix CallTracker wraps the function causes the length to be lost (OneNail) [#42909](https://github.com/nodejs/node/pull/42909)
* \[[`e74a8da287`](https://github.com/nodejs/node/commit/e74a8da287)] - **assert**: make `assert.fail` less affected by prototype tampering (Antoine du Hamel) [#42918](https://github.com/nodejs/node/pull/42918)
* \[[`1146806673`](https://github.com/nodejs/node/commit/1146806673)] - **bootstrap**: stop delaying instantiation of maps in per-context scripts (Darshan Sen) [#42934](https://github.com/nodejs/node/pull/42934)
* \[[`a20310d171`](https://github.com/nodejs/node/commit/a20310d171)] - **bootstrap**: use a context snapshotted with primordials in workers (Joyee Cheung) [#42867](https://github.com/nodejs/node/pull/42867)
* \[[`9ee7d9eb15`](https://github.com/nodejs/node/commit/9ee7d9eb15)] - **bootstrap**: fix wasm\_web\_api external reference registration (Joyee Cheung) [#42903](https://github.com/nodejs/node/pull/42903)
* \[[`cec678a00e`](https://github.com/nodejs/node/commit/cec678a00e)] - **build**: set ASAN workaround (Richard Lau) [#43085](https://github.com/nodejs/node/pull/43085)
* \[[`7c4df42caa`](https://github.com/nodejs/node/commit/7c4df42caa)] - **build**: disable windows-2022 temporarily (Jiawen Geng) [#43093](https://github.com/nodejs/node/pull/43093)
* \[[`0eb32ed976`](https://github.com/nodejs/node/commit/0eb32ed976)] - **build**: fix various shared library build issues (William Marlow) [#41850](https://github.com/nodejs/node/pull/41850)
* \[[`48f4a714b2`](https://github.com/nodejs/node/commit/48f4a714b2)] - **build**: fix indeterminacy of icu\_locales value (Sergey Nazaryev) [#42865](https://github.com/nodejs/node/pull/42865)
* \[[`19c060fd84`](https://github.com/nodejs/node/commit/19c060fd84)] - **crypto**: adjust minimum length in generateKey('hmac', ...) (LiviaMedeiros) [#42944](https://github.com/nodejs/node/pull/42944)
* \[[`183bcc0699`](https://github.com/nodejs/node/commit/183bcc0699)] - **crypto**: clean up parameter validation in HKDF (Tobias Nießen) [#42924](https://github.com/nodejs/node/pull/42924)
* \[[`946f57c7bc`](https://github.com/nodejs/node/commit/946f57c7bc)] - **debugger**: fix inconsistent inspector output of exec new Map() (Kohei Ueno) [#42423](https://github.com/nodejs/node/pull/42423)
* \[[`d128356a7f`](https://github.com/nodejs/node/commit/d128356a7f)] - **deps**: update undici to 5.2.0 (Node.js GitHub Bot) [#43059](https://github.com/nodejs/node/pull/43059)
* \[[`a9703a55ef`](https://github.com/nodejs/node/commit/a9703a55ef)] - **deps**: remove opensslconf template headers (Daniel Bevenius) [#43035](https://github.com/nodejs/node/pull/43035)
* \[[`a4a4f7134b`](https://github.com/nodejs/node/commit/a4a4f7134b)] - **deps**: fix llhttp version number (Michael Dawson) [#43029](https://github.com/nodejs/node/pull/43029)
* \[[`8e54c19a6e`](https://github.com/nodejs/node/commit/8e54c19a6e)] - **deps**: update archs files for quictls/openssl-3.0.3+quic (RafaelGSS) [#43022](https://github.com/nodejs/node/pull/43022)
* \[[`6365bf808e`](https://github.com/nodejs/node/commit/6365bf808e)] - **deps**: upgrade openssl sources to quictls/openssl-3.0.3 (RafaelGSS) [#43022](https://github.com/nodejs/node/pull/43022)
* \[[`e8121ae7fe`](https://github.com/nodejs/node/commit/e8121ae7fe)] - **deps**: regenerate OpenSSL archs files (Daniel Bevenius) [#42978](https://github.com/nodejs/node/pull/42978)
* \[[`13c333e533`](https://github.com/nodejs/node/commit/13c333e533)] - _**Revert**_ "**deps**: add template for generated headers" (Daniel Bevenius) [#42978](https://github.com/nodejs/node/pull/42978)
* \[[`2df1624f80`](https://github.com/nodejs/node/commit/2df1624f80)] - **deps**: upgrade npm to 8.9.0 (npm team) [#42968](https://github.com/nodejs/node/pull/42968)
* \[[`f53ed9d1bb`](https://github.com/nodejs/node/commit/f53ed9d1bb)] - **doc**: use serial comma in fs docs (Tobias Nießen) [#43104](https://github.com/nodejs/node/pull/43104)
* \[[`839824aca8`](https://github.com/nodejs/node/commit/839824aca8)] - **doc**: use serial comma in events docs (Tobias Nießen) [#43113](https://github.com/nodejs/node/pull/43113)
* \[[`9629c74080`](https://github.com/nodejs/node/commit/9629c74080)] - **doc**: use serial comma in modules docs (Tobias Nießen) [#43103](https://github.com/nodejs/node/pull/43103)
* \[[`76096c2d4a`](https://github.com/nodejs/node/commit/76096c2d4a)] - **doc**: use serial comma in util docs (Tobias Nießen) [#43063](https://github.com/nodejs/node/pull/43063)
* \[[`1e9de0dd5a`](https://github.com/nodejs/node/commit/1e9de0dd5a)] - **doc**: remove git:// protocol, adjust nits in onboarding.md (LiviaMedeiros) [#43045](https://github.com/nodejs/node/pull/43045)
* \[[`eb630d7ef9`](https://github.com/nodejs/node/commit/eb630d7ef9)] - **doc**: add maintaining info for shared libary option (Michael Dawson) [#42517](https://github.com/nodejs/node/pull/42517)
* \[[`3816a97bae`](https://github.com/nodejs/node/commit/3816a97bae)] - **doc**: add detail for how to update llhttp (Michael Dawson) [#43028](https://github.com/nodejs/node/pull/43028)
* \[[`330e267a57`](https://github.com/nodejs/node/commit/330e267a57)] - **doc**: use serial comma in buffer docs (Tobias Nießen) [#43048](https://github.com/nodejs/node/pull/43048)
* \[[`0957212390`](https://github.com/nodejs/node/commit/0957212390)] - **doc**: use consistent method symbol (Paolo Insogna) [#42974](https://github.com/nodejs/node/pull/42974)
* \[[`22cb7104cb`](https://github.com/nodejs/node/commit/22cb7104cb)] - **doc**: add Rafael to the security steward for NearForm (Matteo Collina) [#42966](https://github.com/nodejs/node/pull/42966)
* \[[`ef177da3f1`](https://github.com/nodejs/node/commit/ef177da3f1)] - **doc**: mark some node-api functions as experimental (NickNaso) [#42987](https://github.com/nodejs/node/pull/42987)
* \[[`4a3f678e70`](https://github.com/nodejs/node/commit/4a3f678e70)] - **doc**: add LiviaMedeiros to collaborators (LiviaMedeiros) [#43039](https://github.com/nodejs/node/pull/43039)
* \[[`c988a0ed26`](https://github.com/nodejs/node/commit/c988a0ed26)] - **doc**: use serial comma in http docs (Tobias Nießen) [#43026](https://github.com/nodejs/node/pull/43026)
* \[[`4de918b4c1`](https://github.com/nodejs/node/commit/4de918b4c1)] - **doc**: add the preferred name for @himself65 (Himself65) [#43024](https://github.com/nodejs/node/pull/43024)
* \[[`686c4c1f6f`](https://github.com/nodejs/node/commit/686c4c1f6f)] - **doc**: add release key for Juan Arboleda (Juan José) [#42961](https://github.com/nodejs/node/pull/42961)
* \[[`64e0aa116d`](https://github.com/nodejs/node/commit/64e0aa116d)] - **doc**: rename N-API to Node-API in test/README.md (Daeyeon Jeong) [#42946](https://github.com/nodejs/node/pull/42946)
* \[[`65d64553c0`](https://github.com/nodejs/node/commit/65d64553c0)] - **doc**: use serial comma in tls docs (Tobias Nießen) [#43001](https://github.com/nodejs/node/pull/43001)
* \[[`840e61e745`](https://github.com/nodejs/node/commit/840e61e745)] - **doc**: improve commit message example for releases (Juan José) [#42954](https://github.com/nodejs/node/pull/42954)
* \[[`ba3ad7c665`](https://github.com/nodejs/node/commit/ba3ad7c665)] - **doc**: use serial comma in cluster docs (Tobias Nießen) [#42989](https://github.com/nodejs/node/pull/42989)
* \[[`3ab3086008`](https://github.com/nodejs/node/commit/3ab3086008)] - **doc**: fix errors in Web Streams doc (OneNail) [#42862](https://github.com/nodejs/node/pull/42862)
* \[[`1fbfee2497`](https://github.com/nodejs/node/commit/1fbfee2497)] - **doc**: fix examples in cluster.md (OneNail) [#42889](https://github.com/nodejs/node/pull/42889)
* \[[`1237c742f4`](https://github.com/nodejs/node/commit/1237c742f4)] - **doc**: add additional step to security release process (Michael Dawson) [#42916](https://github.com/nodejs/node/pull/42916)
* \[[`88692d8fd6`](https://github.com/nodejs/node/commit/88692d8fd6)] - **doc**: add section regarding property definition in `primordials.md` (Antoine du Hamel) [#42921](https://github.com/nodejs/node/pull/42921)
* \[[`924670f3af`](https://github.com/nodejs/node/commit/924670f3af)] - **doc**: clarify some default values in `fs.md` (LiviaMedeiros) [#42892](https://github.com/nodejs/node/pull/42892)
* \[[`becca06f9b`](https://github.com/nodejs/node/commit/becca06f9b)] - **fs**: remove unnecessary ?? operator (Morgan Roderick) [#43073](https://github.com/nodejs/node/pull/43073)
* \[[`784d84cf34`](https://github.com/nodejs/node/commit/784d84cf34)] - **(SEMVER-MINOR)** **fs**: add `read(buffer[, options])` versions (LiviaMedeiros) [#42768](https://github.com/nodejs/node/pull/42768)
* \[[`2f192c4be0`](https://github.com/nodejs/node/commit/2f192c4be0)] - **(SEMVER-MINOR)** **http**: added connection closing methods (Paolo Insogna) [#42812](https://github.com/nodejs/node/pull/42812)
* \[[`bfbf965eb0`](https://github.com/nodejs/node/commit/bfbf965eb0)] - **http2**: compat support for array headers (OneNail) [#42901](https://github.com/nodejs/node/pull/42901)
* \[[`46a44b3011`](https://github.com/nodejs/node/commit/46a44b3011)] - **lib**: move WebAssembly Web API into separate file (Tobias Nießen) [#42993](https://github.com/nodejs/node/pull/42993)
* \[[`c64b8d3282`](https://github.com/nodejs/node/commit/c64b8d3282)] - **lib,test**: enable wasm/webapi/empty-body WPT (Tobias Nießen) [#42960](https://github.com/nodejs/node/pull/42960)
* \[[`ddd271ec2b`](https://github.com/nodejs/node/commit/ddd271ec2b)] - **meta**: add mailmap entry for ShogunPanda (Paolo Insogna) [#43094](https://github.com/nodejs/node/pull/43094)
* \[[`174ff972f0`](https://github.com/nodejs/node/commit/174ff972f0)] - **meta**: update .mailmap for recent README name change (Rich Trott) [#43027](https://github.com/nodejs/node/pull/43027)
* \[[`16df8ad7c3`](https://github.com/nodejs/node/commit/16df8ad7c3)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#43004](https://github.com/nodejs/node/pull/43004)
* \[[`0ec32d0715`](https://github.com/nodejs/node/commit/0ec32d0715)] - **meta**: move one or more collaborators to emeritus (Node.js GitHub Bot) [#42937](https://github.com/nodejs/node/pull/42937)
* \[[`037ff3da6d`](https://github.com/nodejs/node/commit/037ff3da6d)] - **node-api**: explicitly set \_\_cdecl for API functions (Vladimir Morozov) [#42780](https://github.com/nodejs/node/pull/42780)
* \[[`e2462a2f98`](https://github.com/nodejs/node/commit/e2462a2f98)] - **node-api**: fix napi\_get\_all\_property\_names (Vladimir Morozov) [#42463](https://github.com/nodejs/node/pull/42463)
* \[[`c92e291beb`](https://github.com/nodejs/node/commit/c92e291beb)] - **(SEMVER-MINOR)** **perf\_hooks**: add PerformanceResourceTiming (RafaelGSS) [#42725](https://github.com/nodejs/node/pull/42725)
* \[[`c535db1195`](https://github.com/nodejs/node/commit/c535db1195)] - **src**: delete AllocatedBuffer (Darshan Sen) [#43008](https://github.com/nodejs/node/pull/43008)
* \[[`5dc79298e1`](https://github.com/nodejs/node/commit/5dc79298e1)] - **src**: remove unnecessary comment (Kohei Ueno) [#42952](https://github.com/nodejs/node/pull/42952)
* \[[`38e4c15534`](https://github.com/nodejs/node/commit/38e4c15534)] - **src**: always signal V8 for intercepted properties (Michaël Zasso) [#42963](https://github.com/nodejs/node/pull/42963)
* \[[`cacff07e75`](https://github.com/nodejs/node/commit/cacff07e75)] - **src**: fix memory leak for v8.serialize (liuxingbaoyu) [#42695](https://github.com/nodejs/node/pull/42695)
* \[[`8cfc18e4db`](https://github.com/nodejs/node/commit/8cfc18e4db)] - **src,crypto**: remove uses of AllocatedBuffer from crypto\_rsa.cc (Darshan Sen) [#42852](https://github.com/nodejs/node/pull/42852)
* \[[`0670843b24`](https://github.com/nodejs/node/commit/0670843b24)] - **test**: fix dangerous .map in `test/parallel/test-http-set-trailers.js` (LiviaMedeiros) [#43087](https://github.com/nodejs/node/pull/43087)
* \[[`9eb8bf1d26`](https://github.com/nodejs/node/commit/9eb8bf1d26)] - **test**: reduce flakiness of `test-fs-read-position-validation.mjs` (LiviaMedeiros) [#42999](https://github.com/nodejs/node/pull/42999)
* \[[`41d2f6e8c5`](https://github.com/nodejs/node/commit/41d2f6e8c5)] - **test**: rename handlewrap.hasref tests (Daeyeon Jeong) [#42754](https://github.com/nodejs/node/pull/42754)
* \[[`e058f47277`](https://github.com/nodejs/node/commit/e058f47277)] - **test**: improve observable ICU behaviour coverage (LiviaMedeiros) [#42683](https://github.com/nodejs/node/pull/42683)
* \[[`d23debb4cb`](https://github.com/nodejs/node/commit/d23debb4cb)] - **test**: validate webstream encoder/decoder inspector (Yoshiki Kurihara) [#42747](https://github.com/nodejs/node/pull/42747)
* \[[`b1c18edaa9`](https://github.com/nodejs/node/commit/b1c18edaa9)] - **test**: use`mustSucceed` instead of `mustCall` with `assert.ifError` (MURAKAMI Masahiko) [#42806](https://github.com/nodejs/node/pull/42806)
* \[[`2dc795687a`](https://github.com/nodejs/node/commit/2dc795687a)] - **test**: improve `lib/internal/webstreams/readablestream.js` coverage (MURAKAMI Masahiko) [#42823](https://github.com/nodejs/node/pull/42823)
* \[[`d746207dc2`](https://github.com/nodejs/node/commit/d746207dc2)] - **test**: fix test-crypto-fips.js under shared OpenSSL (Vita Batrla) [#42947](https://github.com/nodejs/node/pull/42947)
* \[[`56c47b5101`](https://github.com/nodejs/node/commit/56c47b5101)] - **test**: use consistent timeouts (Paolo Insogna) [#42893](https://github.com/nodejs/node/pull/42893)
* \[[`68ed3c88d9`](https://github.com/nodejs/node/commit/68ed3c88d9)] - **test**: add test for position validation in fs.read() and fs.readSync() (LiviaMedeiros) [#42837](https://github.com/nodejs/node/pull/42837)
* \[[`72b90fd5f5`](https://github.com/nodejs/node/commit/72b90fd5f5)] - **test**: reduce impact of flaky HTTP server tests (Tobias Nießen) [#42926](https://github.com/nodejs/node/pull/42926)
* \[[`531a0a9980`](https://github.com/nodejs/node/commit/531a0a9980)] - **tools**: update lint-md-dependencies to rollup\@2.73.0 (Node.js GitHub Bot) [#43107](https://github.com/nodejs/node/pull/43107)
* \[[`64daaca46d`](https://github.com/nodejs/node/commit/64daaca46d)] - **tools**: update eslint to 8.15.0 (Node.js GitHub Bot) [#43005](https://github.com/nodejs/node/pull/43005)
* \[[`79872382ef`](https://github.com/nodejs/node/commit/79872382ef)] - **tools**: refactor lint-sh.js to esm module (Feng Yu) [#42942](https://github.com/nodejs/node/pull/42942)
* \[[`265ecdfe07`](https://github.com/nodejs/node/commit/265ecdfe07)] - **tools**: update lint-md-dependencies (Node.js GitHub Bot) [#43003](https://github.com/nodejs/node/pull/43003)
* \[[`e9e1f1e194`](https://github.com/nodejs/node/commit/e9e1f1e194)] - **typings**: fix `os.cpus` invalid return type (Himself65) [#43006](https://github.com/nodejs/node/pull/43006)
* \[[`55ef6e81cb`](https://github.com/nodejs/node/commit/55ef6e81cb)] - **wasm**: add missing init reported by coverity (Michael Dawson) [#42897](https://github.com/nodejs/node/pull/42897)
* \[[`5470578008`](https://github.com/nodejs/node/commit/5470578008)] - **worker**: fix stream racing with terminate (Keyhan Vakil) [#42874](https://github.com/nodejs/node/pull/42874)

<a id="18.1.0"></a>

## 2022-05-03, Version 18.1.0 (Current), @targos

### Notable Changes

* \[[`c46e7bbf69`](https://github.com/nodejs/node/commit/c46e7bbf69)] - **doc**: add @kuriyosh to collaborators (Yoshiki Kurihara) [#42824](https://github.com/nodejs/node/pull/42824)
* \[[`b0f7c4c8f9`](https://github.com/nodejs/node/commit/b0f7c4c8f9)] - **(SEMVER-MINOR)** **lib,src**: implement WebAssembly Web API (Tobias Nießen) [#42701](https://github.com/nodejs/node/pull/42701)
* \[[`78a860ae58`](https://github.com/nodejs/node/commit/78a860ae58)] - **(SEMVER-MINOR)** **test\_runner**: add initial CLI runner (Colin Ihrig) [#42658](https://github.com/nodejs/node/pull/42658)
* \[[`bf9240ae8c`](https://github.com/nodejs/node/commit/bf9240ae8c)] - **(SEMVER-MINOR)** **worker**: add hasRef() to MessagePort (Darshan Sen) [#42849](https://github.com/nodejs/node/pull/42849)

### Commits

* \[[`4694f5bb96`](https://github.com/nodejs/node/commit/4694f5bb96)] - **async\_hooks**: avoid decrementing iterator after erase (Gabriel Bota) [#42749](https://github.com/nodejs/node/pull/42749)
* \[[`459546b4f0`](https://github.com/nodejs/node/commit/459546b4f0)] - **benchmark**: fix misc/startup failure (Antoine du Hamel) [#42746](https://github.com/nodejs/node/pull/42746)
* \[[`6bd24204ea`](https://github.com/nodejs/node/commit/6bd24204ea)] - **bootstrap**: use the isolate snapshot in workers (Joyee Cheung) [#42702](https://github.com/nodejs/node/pull/42702)
* \[[`29c8411f99`](https://github.com/nodejs/node/commit/29c8411f99)] - **bootstrap**: move embedded snapshot to SnapshotBuilder (Joyee Cheung) [#42702](https://github.com/nodejs/node/pull/42702)
* \[[`4050b0d64f`](https://github.com/nodejs/node/commit/4050b0d64f)] - **build**: enable V8's shared read-only heap (Michaël Zasso) [#42809](https://github.com/nodejs/node/pull/42809)
* \[[`f9994e2029`](https://github.com/nodejs/node/commit/f9994e2029)] - **build**: improve reliability of find\_python.cmd script (Luigi Pinca) [#42810](https://github.com/nodejs/node/pull/42810)
* \[[`5d15eb1a14`](https://github.com/nodejs/node/commit/5d15eb1a14)] - **build**: fix format-cpp (Darshan Sen) [#42764](https://github.com/nodejs/node/pull/42764)
* \[[`7c973474bf`](https://github.com/nodejs/node/commit/7c973474bf)] - **build**: improve the format-cpp error message (Darshan Sen) [#42765](https://github.com/nodejs/node/pull/42765)
* \[[`7681e60829`](https://github.com/nodejs/node/commit/7681e60829)] - **crypto**: validate `this` in all webcrypto methods and getters (Filip Skokan) [#42815](https://github.com/nodejs/node/pull/42815)
* \[[`2a4c8263c3`](https://github.com/nodejs/node/commit/2a4c8263c3)] - **deps**: update undici to 5.1.1 (Michaël Zasso) [#42939](https://github.com/nodejs/node/pull/42939)
* \[[`1102922ef9`](https://github.com/nodejs/node/commit/1102922ef9)] - **deps**: upgrade npm to 8.8.0 (npm team) [#42886](https://github.com/nodejs/node/pull/42886)
* \[[`279892987b`](https://github.com/nodejs/node/commit/279892987b)] - **deps**: remove linux-ppc64 architecture (Daniel Bevenius) [#42616](https://github.com/nodejs/node/pull/42616)
* \[[`4abe9879ae`](https://github.com/nodejs/node/commit/4abe9879ae)] - **deps**: remove linux-ppc architecture (Daniel Bevenius) [#42616](https://github.com/nodejs/node/pull/42616)
* \[[`8dc71f2266`](https://github.com/nodejs/node/commit/8dc71f2266)] - **deps**: remove aix-gcc architecture (Daniel Bevenius) [#42616](https://github.com/nodejs/node/pull/42616)
* \[[`6dc1f82384`](https://github.com/nodejs/node/commit/6dc1f82384)] - **deps**: remove archs/aix64-gcc (Daniel Bevenius) [#42616](https://github.com/nodejs/node/pull/42616)
* \[[`e8734a4771`](https://github.com/nodejs/node/commit/e8734a4771)] - **deps**: add note about removing asm archs (Daniel Bevenius) [#42616](https://github.com/nodejs/node/pull/42616)
* \[[`7fae2c9d6e`](https://github.com/nodejs/node/commit/7fae2c9d6e)] - **deps**: add template for generated headers (Daniel Bevenius) [#42616](https://github.com/nodejs/node/pull/42616)
* \[[`294664e32c`](https://github.com/nodejs/node/commit/294664e32c)] - **deps**: upgrade npm to 8.7.0 (npm team) [#42744](https://github.com/nodejs/node/pull/42744)
* \[[`60e461c45d`](https://github.com/nodejs/node/commit/60e461c45d)] - **doc**: reword "test directory" (LiviaMedeiros) [#42817](https://github.com/nodejs/node/pull/42817)
* \[[`227a45ba60`](https://github.com/nodejs/node/commit/227a45ba60)] - **doc**: remove legacy `-J` test.py option from BUILDING.md (LiviaMedeiros) [#42817](https://github.com/nodejs/node/pull/42817)
* \[[`e313dc6ed9`](https://github.com/nodejs/node/commit/e313dc6ed9)] - **doc**: http2.createServer `options` as optional (Daeyeon Jeong) [#42832](https://github.com/nodejs/node/pull/42832)
* \[[`8f2b2280cd`](https://github.com/nodejs/node/commit/8f2b2280cd)] - **doc**: record March 2022 security release steward (Richard Lau) [#42876](https://github.com/nodejs/node/pull/42876)
* \[[`e15d22c024`](https://github.com/nodejs/node/commit/e15d22c024)] - **doc**: initial version of security-model-strategy.md (Michael Dawson) [#42709](https://github.com/nodejs/node/pull/42709)
* \[[`fe65996790`](https://github.com/nodejs/node/commit/fe65996790)] - **doc**: clarify guide on testing internal errors (Livia Medeiros) [#42813](https://github.com/nodejs/node/pull/42813)
* \[[`2f849a460f`](https://github.com/nodejs/node/commit/2f849a460f)] - **doc**: fix markdown formatting in primordials.md (Tobias Nießen) [#42877](https://github.com/nodejs/node/pull/42877)
* \[[`cd2f5a4fd4`](https://github.com/nodejs/node/commit/cd2f5a4fd4)] - **doc**: add primordials guidelines (Antoine du Hamel) [#38635](https://github.com/nodejs/node/pull/38635)
* \[[`2d76f72665`](https://github.com/nodejs/node/commit/2d76f72665)] - **doc**: elevate node-clinic diagnostic tier (RafaelGSS) [#42802](https://github.com/nodejs/node/pull/42802)
* \[[`9b61ac2617`](https://github.com/nodejs/node/commit/9b61ac2617)] - **doc**: update WebAssembly strategy with Wasm Web API (Tobias Nießen) [#42836](https://github.com/nodejs/node/pull/42836)
* \[[`c6c1dc5833`](https://github.com/nodejs/node/commit/c6c1dc5833)] - **doc**: order `vm.Module` linker arguments correctly (Simen Bekkhus) [#42797](https://github.com/nodejs/node/pull/42797)
* \[[`c46e7bbf69`](https://github.com/nodejs/node/commit/c46e7bbf69)] - **doc**: add @kuriyosh to collaborators (Yoshiki Kurihara) [#42824](https://github.com/nodejs/node/pull/42824)
* \[[`59da1339b4`](https://github.com/nodejs/node/commit/59da1339b4)] - **doc**: add maintaining-webassembly.md (Michael Dawson) [#42660](https://github.com/nodejs/node/pull/42660)
* \[[`d9f3f05cab`](https://github.com/nodejs/node/commit/d9f3f05cab)] - **doc**: fix outdated documentation for `family` property (Antoine du Hamel) [#42789](https://github.com/nodejs/node/pull/42789)
* \[[`6fa080cb48`](https://github.com/nodejs/node/commit/6fa080cb48)] - **doc**: delete heapdump from diagnostic tooling support tiers (Tony Gorez) [#42783](https://github.com/nodejs/node/pull/42783)
* \[[`c32f76d49e`](https://github.com/nodejs/node/commit/c32f76d49e)] - **doc**: fix example in assert.md (Livia Medeiros) [#42786](https://github.com/nodejs/node/pull/42786)
* \[[`6225370b2e`](https://github.com/nodejs/node/commit/6225370b2e)] - **doc**: fix version history for Loaders API (Antoine du Hamel) [#42778](https://github.com/nodejs/node/pull/42778)
* \[[`3d65a3b13e`](https://github.com/nodejs/node/commit/3d65a3b13e)] - **doc**: add `node:` prefix for all core modules (Antoine du Hamel) [#42752](https://github.com/nodejs/node/pull/42752)
* \[[`46c880b99b`](https://github.com/nodejs/node/commit/46c880b99b)] - **doc**: clarify core modules that can be loaded without a prefix (Antoine du Hamel) [#42753](https://github.com/nodejs/node/pull/42753)
* \[[`025b3e786a`](https://github.com/nodejs/node/commit/025b3e786a)] - **doc**: consolidate use of multiple-byte units (Antoine du Hamel) [#42587](https://github.com/nodejs/node/pull/42587)
* \[[`962d80b7a1`](https://github.com/nodejs/node/commit/962d80b7a1)] - **doc**: add documentation for inherited methods (Luigi Pinca) [#42691](https://github.com/nodejs/node/pull/42691)
* \[[`222b3e6674`](https://github.com/nodejs/node/commit/222b3e6674)] - **doc**: close tag in n-api.md (Livia Medeiros) [#42751](https://github.com/nodejs/node/pull/42751)
* \[[`4c30936065`](https://github.com/nodejs/node/commit/4c30936065)] - **doc**: copyedit http.OutgoingMessage documentation (Luigi Pinca) [#42733](https://github.com/nodejs/node/pull/42733)
* \[[`d77c59d0f2`](https://github.com/nodejs/node/commit/d77c59d0f2)] - **doc**: improve fragment (`:target`) anchors behavior on HTML version (Antoine du Hamel) [#42739](https://github.com/nodejs/node/pull/42739)
* \[[`c50309cb39`](https://github.com/nodejs/node/commit/c50309cb39)] - **doc**: fix `added:` info for `outgoingMessage.writable*` (Luigi Pinca) [#42737](https://github.com/nodejs/node/pull/42737)
* \[[`6b7c35e807`](https://github.com/nodejs/node/commit/6b7c35e807)] - **doc**: delete mdb\_v8 from diagnostic tooling support tiers (Tony Gorez) [#42626](https://github.com/nodejs/node/pull/42626)
* \[[`2a07a9fc3a`](https://github.com/nodejs/node/commit/2a07a9fc3a)] - **doc**: document the 'close' and 'finish' events (Luigi Pinca) [#42704](https://github.com/nodejs/node/pull/42704)
* \[[`ef5ab8179b`](https://github.com/nodejs/node/commit/ef5ab8179b)] - **doc**: fix `added:` info for `outgoingMessage.{,un}cork()` (Luigi Pinca) [#42711](https://github.com/nodejs/node/pull/42711)
* \[[`a6e1e7a5d7`](https://github.com/nodejs/node/commit/a6e1e7a5d7)] - **doc,test**: add tests and docs for duplex.fromWeb and duplex.toWeb (Erick Wendel) [#42738](https://github.com/nodejs/node/pull/42738)
* \[[`336242a7c6`](https://github.com/nodejs/node/commit/336242a7c6)] - **errors,console**: refactor to use ES2021 syntax (小菜) [#42872](https://github.com/nodejs/node/pull/42872)
* \[[`0e16120d0d`](https://github.com/nodejs/node/commit/0e16120d0d)] - **errors,vm**: update error and use cause (Gus Caplan) [#42820](https://github.com/nodejs/node/pull/42820)
* \[[`a0638a23b0`](https://github.com/nodejs/node/commit/a0638a23b0)] - **esm**: fix imports from non-file module (Antoine du Hamel) [#42881](https://github.com/nodejs/node/pull/42881)
* \[[`dab15f69e3`](https://github.com/nodejs/node/commit/dab15f69e3)] - **esm**: graduate top-level-await to stable (Antoine du Hamel) [#42875](https://github.com/nodejs/node/pull/42875)
* \[[`48bbb73f36`](https://github.com/nodejs/node/commit/48bbb73f36)] - **fs**: fix mkdirSync so ENOSPC is correctly reported (Santiago Gimeno) [#42811](https://github.com/nodejs/node/pull/42811)
* \[[`d33cbabd79`](https://github.com/nodejs/node/commit/d33cbabd79)] - **lib**: remove experimental warning from FormData (Xuguang Mei) [#42807](https://github.com/nodejs/node/pull/42807)
* \[[`ad8269450a`](https://github.com/nodejs/node/commit/ad8269450a)] - **lib,src**: use Response URL as WebAssembly location (Tobias Nießen) [#42842](https://github.com/nodejs/node/pull/42842)
* \[[`b0f7c4c8f9`](https://github.com/nodejs/node/commit/b0f7c4c8f9)] - **(SEMVER-MINOR)** **lib,src**: implement WebAssembly Web API (Tobias Nießen) [#42701](https://github.com/nodejs/node/pull/42701)
* \[[`fdc65032a7`](https://github.com/nodejs/node/commit/fdc65032a7)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#42848](https://github.com/nodejs/node/pull/42848)
* \[[`33ac027fdf`](https://github.com/nodejs/node/commit/33ac027fdf)] - **meta**: move one or more collaborators to emeritus (Node.js GitHub Bot) [#42769](https://github.com/nodejs/node/pull/42769)
* \[[`14893c5984`](https://github.com/nodejs/node/commit/14893c5984)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#42760](https://github.com/nodejs/node/pull/42760)
* \[[`2f38b4812c`](https://github.com/nodejs/node/commit/2f38b4812c)] - **meta**: move mmarchini to emeritus (mary marchini) [#42750](https://github.com/nodejs/node/pull/42750)
* \[[`718d11fdb0`](https://github.com/nodejs/node/commit/718d11fdb0)] - **perf\_hooks**: return different functions in timerify (Himself65) [#42854](https://github.com/nodejs/node/pull/42854)
* \[[`e8083664e1`](https://github.com/nodejs/node/commit/e8083664e1)] - **src**: turn SSL\_CTX\_new CHECK/segfault into JS exception (Anna Henningsen) [#42799](https://github.com/nodejs/node/pull/42799)
* \[[`37ca1102c4`](https://github.com/nodejs/node/commit/37ca1102c4)] - **src**: make --no-node-snapshot a per-process option (Joyee Cheung) [#42864](https://github.com/nodejs/node/pull/42864)
* \[[`1976284a92`](https://github.com/nodejs/node/commit/1976284a92)] - **src**: define fs.constants.S\_IWUSR & S\_IRUSR for Win (Liviu Ionescu) [#42757](https://github.com/nodejs/node/pull/42757)
* \[[`b7e9dd0278`](https://github.com/nodejs/node/commit/b7e9dd0278)] - **src**: use `node:` prefix in example (Antoine du Hamel) [#42794](https://github.com/nodejs/node/pull/42794)
* \[[`1a7af6364d`](https://github.com/nodejs/node/commit/1a7af6364d)] - **src**: large page attributing an id on Linux (David CARLIER) [#42644](https://github.com/nodejs/node/pull/42644)
* \[[`494650c09f`](https://github.com/nodejs/node/commit/494650c09f)] - **src,crypto**: remove uses of AllocatedBuffer from crypto\_ec.cc (Darshan Sen) [#42766](https://github.com/nodejs/node/pull/42766)
* \[[`8e0e576669`](https://github.com/nodejs/node/commit/8e0e576669)] - **test**: add tests for extracting function name (Kohei Ueno) [#42399](https://github.com/nodejs/node/pull/42399)
* \[[`fbda87d966`](https://github.com/nodejs/node/commit/fbda87d966)] - **test**: simplify test-gc-{http-client,net}-\* (Luigi Pinca) [#42782](https://github.com/nodejs/node/pull/42782)
* \[[`3c796f8328`](https://github.com/nodejs/node/commit/3c796f8328)] - **test**: fix `parallel/test-dgram-udp6-link-local-address` (Antoine du Hamel) [#42795](https://github.com/nodejs/node/pull/42795)
* \[[`b85a11c28e`](https://github.com/nodejs/node/commit/b85a11c28e)] - **test**: improve lib/internal/test\_runner/test.js coverage (MURAKAMI Masahiko) [#42745](https://github.com/nodejs/node/pull/42745)
* \[[`59c07a99fb`](https://github.com/nodejs/node/commit/59c07a99fb)] - **test**: check ecdsa psychic signature (Filip Skokan) [#42863](https://github.com/nodejs/node/pull/42863)
* \[[`0725064695`](https://github.com/nodejs/node/commit/0725064695)] - **test**: fix port in net-perf\_hooks (Livia Medeiros) [#42761](https://github.com/nodejs/node/pull/42761)
* \[[`7b701442de`](https://github.com/nodejs/node/commit/7b701442de)] - **test**: skip test that cannot pass under --node-builtin-modules-path (Geoffrey Booth) [#42834](https://github.com/nodejs/node/pull/42834)
* \[[`37364abc58`](https://github.com/nodejs/node/commit/37364abc58)] - **test**: fix flaky HTTP server tests (Tobias Nießen) [#42846](https://github.com/nodejs/node/pull/42846)
* \[[`8476ffb85a`](https://github.com/nodejs/node/commit/8476ffb85a)] - **test**: use `assert.match()` instead of `assert(regex.test())` (Antoine du Hamel) [#42803](https://github.com/nodejs/node/pull/42803)
* \[[`d311916f37`](https://github.com/nodejs/node/commit/d311916f37)] - **test**: fix calculations in test-worker-resource-limits (Joyee Cheung) [#42702](https://github.com/nodejs/node/pull/42702)
* \[[`deb3cf49c7`](https://github.com/nodejs/node/commit/deb3cf49c7)] - **test**: remove the legacy url parser function (Kohei Ueno) [#42656](https://github.com/nodejs/node/pull/42656)
* \[[`be44b1ffcb`](https://github.com/nodejs/node/commit/be44b1ffcb)] - **test**: improve test coverage of internal/blob (Yoshiki Kurihara) [#41513](https://github.com/nodejs/node/pull/41513)
* \[[`78a860ae58`](https://github.com/nodejs/node/commit/78a860ae58)] - **(SEMVER-MINOR)** **test\_runner**: add initial CLI runner (Colin Ihrig) [#42658](https://github.com/nodejs/node/pull/42658)
* \[[`1e7479d34c`](https://github.com/nodejs/node/commit/1e7479d34c)] - **tools**: update lint-md-dependencies (Node.js GitHub Bot) [#42932](https://github.com/nodejs/node/pull/42932)
* \[[`c3c5fe78dc`](https://github.com/nodejs/node/commit/c3c5fe78dc)] - **tools**: bump jsdoccomment from 0.22.1 to 0.29.0 (Rich Trott) [#42857](https://github.com/nodejs/node/pull/42857)
* \[[`97fc00a06e`](https://github.com/nodejs/node/commit/97fc00a06e)] - **tools**: update eslint to 8.14.0 (Node.js GitHub Bot) [#42845](https://github.com/nodejs/node/pull/42845)
* \[[`93fd77a16f`](https://github.com/nodejs/node/commit/93fd77a16f)] - **tools**: update doc to highlight.js\@11.5.1 (Node.js GitHub Bot) [#42758](https://github.com/nodejs/node/pull/42758)
* \[[`47c04813f7`](https://github.com/nodejs/node/commit/47c04813f7)] - **tools**: update lint-md-dependencies (Node.js GitHub Bot) [#42759](https://github.com/nodejs/node/pull/42759)
* \[[`18ae2c39d5`](https://github.com/nodejs/node/commit/18ae2c39d5)] - **tools**: lint osx shell scripts (Livia Medeiros) [#42712](https://github.com/nodejs/node/pull/42712)
* \[[`4af0fbd41e`](https://github.com/nodejs/node/commit/4af0fbd41e)] - **v8**: export cpu\_profiler\_metadata\_size in getHeapCodeStatistics (theanarkh) [#42818](https://github.com/nodejs/node/pull/42818)
* \[[`a19fb609d8`](https://github.com/nodejs/node/commit/a19fb609d8)] - **v8**: export more fields in getHeapStatistics (theanarkh) [#42784](https://github.com/nodejs/node/pull/42784)
* \[[`1b5856a2a9`](https://github.com/nodejs/node/commit/1b5856a2a9)] - **wasi**: remove unecessary null check (Michael Dawson) [#42819](https://github.com/nodejs/node/pull/42819)
* \[[`bf9240ae8c`](https://github.com/nodejs/node/commit/bf9240ae8c)] - **(SEMVER-MINOR)** **worker**: add hasRef() to MessagePort (Darshan Sen) [#42849](https://github.com/nodejs/node/pull/42849)
* \[[`c3922afa1c`](https://github.com/nodejs/node/commit/c3922afa1c)] - **worker**: add hasRef() to the handle object (Darshan Sen) [#42756](https://github.com/nodejs/node/pull/42756)

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
