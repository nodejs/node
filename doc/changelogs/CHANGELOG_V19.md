# Node.js 19 ChangeLog

<!--lint disable maximum-line-length no-literal-urls prohibited-strings-->

<table>
<tr>
<th>Current</th>
</tr>
<tr>
<td>
<b><a href="#19.2.0">19.2.0</a></b><br/>
<a href="#19.1.0">19.1.0</a><br/>
<a href="#19.0.1">19.0.1</a><br/>
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

<a id="19.2.0"></a>

## 2022-11-29, Version 19.2.0 (Current), @ruyadorno

### Notable changes

#### Time zone update

Time zone data has been updated to 2022f. This includes changes to Daylight Savings Time (DST) for Fiji and Mexico. For more information, see <https://mm.icann.org/pipermail/tz-announce/2022-October/000075.html>.

#### Other notable changes

* **buffer**
  * (SEMVER-MINOR) introduce `File` class (Khafra) [#45139](https://github.com/nodejs/node/pull/45139)
* **deps**
  * update V8 to 10.8.168.20 (Michaël Zasso) [#45230](https://github.com/nodejs/node/pull/45230)
* **doc**
  * deprecate use of invalid ports in `url.parse` (Antoine du Hamel) [#45576](https://github.com/nodejs/node/pull/45576)
* **util**
  * add fast path for utf8 encoding (Yagiz Nizipli) [#45412](https://github.com/nodejs/node/pull/45412)

### Commits

* \[[`7cff1e14ba`](https://github.com/nodejs/node/commit/7cff1e14ba)] - **(SEMVER-MINOR)** **async\_hooks**: add hook to stop propagation (Gerhard Stöbich) [#45386](https://github.com/nodejs/node/pull/45386)
* \[[`f08f6a64a3`](https://github.com/nodejs/node/commit/f08f6a64a3)] - **benchmark**: add v8 serialize benchmark (Yagiz Nizipli) [#45476](https://github.com/nodejs/node/pull/45476)
* \[[`26ad54c1a2`](https://github.com/nodejs/node/commit/26ad54c1a2)] - **benchmark**: add text-encoder benchmark (Yagiz Nizipli) [#45450](https://github.com/nodejs/node/pull/45450)
* \[[`6c56c9722b`](https://github.com/nodejs/node/commit/6c56c9722b)] - **(SEMVER-MINOR)** **buffer**: introduce File (Khafra) [#45139](https://github.com/nodejs/node/pull/45139)
* \[[`6e1e25d6dd`](https://github.com/nodejs/node/commit/6e1e25d6dd)] - **build**: avoid redefined macro (Michaël Zasso) [#45544](https://github.com/nodejs/node/pull/45544)
* \[[`5c9b2a7c82`](https://github.com/nodejs/node/commit/5c9b2a7c82)] - **build**: fix env.h for cpp20 (Jiawen Geng) [#45516](https://github.com/nodejs/node/pull/45516)
* \[[`54fd8a1966`](https://github.com/nodejs/node/commit/54fd8a1966)] - **build**: reset embedder string to "-node.0" (Michaël Zasso) [#45230](https://github.com/nodejs/node/pull/45230)
* \[[`0f3cf7e5ce`](https://github.com/nodejs/node/commit/0f3cf7e5ce)] - _**Revert**_ "**build**: remove precompiled header and debug information for host builds" (Stefan Stojanovic) [#45432](https://github.com/nodejs/node/pull/45432)
* \[[`62ef1eb4ff`](https://github.com/nodejs/node/commit/62ef1eb4ff)] - **build**: add --v8-disable-object-print flag (MURAKAMI Masahiko) [#45458](https://github.com/nodejs/node/pull/45458)
* \[[`1ce2f56cf6`](https://github.com/nodejs/node/commit/1ce2f56cf6)] - **build**: make scripts in gyp run with right python (Jiawen Geng) [#45435](https://github.com/nodejs/node/pull/45435)
* \[[`9ffe3c051a`](https://github.com/nodejs/node/commit/9ffe3c051a)] - **build,deps,src**: fix Intel VTune profiling support (Shi Lei) [#45248](https://github.com/nodejs/node/pull/45248)
* \[[`bd3accc7b2`](https://github.com/nodejs/node/commit/bd3accc7b2)] - **crypto**: clear OpenSSL error queue after calling X509\_check\_private\_key() (Filip Skokan) [#45495](https://github.com/nodejs/node/pull/45495)
* \[[`724addb293`](https://github.com/nodejs/node/commit/724addb293)] - **crypto**: update root certificates (Luigi Pinca) [#45490](https://github.com/nodejs/node/pull/45490)
* \[[`efe19eb7f5`](https://github.com/nodejs/node/commit/efe19eb7f5)] - **crypto**: clear OpenSSL error queue after calling X509\_verify() (Takuro Sato) [#45377](https://github.com/nodejs/node/pull/45377)
* \[[`f63ae525fa`](https://github.com/nodejs/node/commit/f63ae525fa)] - **deps**: V8: cherry-pick 2ada52cffbff (Michaël Zasso) [#45573](https://github.com/nodejs/node/pull/45573)
* \[[`43e002e3d4`](https://github.com/nodejs/node/commit/43e002e3d4)] - **deps**: update base64 to 0.5.0 (Facundo Tuesca) [#45509](https://github.com/nodejs/node/pull/45509)
* \[[`aaa4ac7735`](https://github.com/nodejs/node/commit/aaa4ac7735)] - **deps**: V8: cherry-pick 9df5ef70ff18 (Yagiz Nizipli) [#45230](https://github.com/nodejs/node/pull/45230)
* \[[`e70c3090ff`](https://github.com/nodejs/node/commit/e70c3090ff)] - **deps**: V8: cherry-pick f1c888e7093e (Michaël Zasso) [#45230](https://github.com/nodejs/node/pull/45230)
* \[[`51eb323c50`](https://github.com/nodejs/node/commit/51eb323c50)] - **deps**: V8: cherry-pick 92a7385171bb (Michaël Zasso) [#45230](https://github.com/nodejs/node/pull/45230)
* \[[`1370b1a769`](https://github.com/nodejs/node/commit/1370b1a769)] - **deps**: fix V8 build on Windows with MSVC (Michaël Zasso) [#45230](https://github.com/nodejs/node/pull/45230)
* \[[`3cd6367e6a`](https://github.com/nodejs/node/commit/3cd6367e6a)] - **deps**: silence irrelevant V8 warning (Michaël Zasso) [#45230](https://github.com/nodejs/node/pull/45230)
* \[[`9348bdd28d`](https://github.com/nodejs/node/commit/9348bdd28d)] - **deps**: V8: fix v8-cppgc.h for MSVC (Jiawen Geng) [#45230](https://github.com/nodejs/node/pull/45230)
* \[[`e9292544b0`](https://github.com/nodejs/node/commit/e9292544b0)] - **deps**: fix V8 build issue with inline methods (Jiawen Geng) [#45230](https://github.com/nodejs/node/pull/45230)
* \[[`a3b9967553`](https://github.com/nodejs/node/commit/a3b9967553)] - **deps**: update V8 to 10.8.168.20 (Michaël Zasso) [#45230](https://github.com/nodejs/node/pull/45230)
* \[[`117efe98b0`](https://github.com/nodejs/node/commit/117efe98b0)] - **deps**: V8: cherry-pick 9df5ef70ff18 (Yagiz Nizipli) [#45474](https://github.com/nodejs/node/pull/45474)
* \[[`628891d4dd`](https://github.com/nodejs/node/commit/628891d4dd)] - **deps**: update timezone to 2022f (Node.js GitHub Bot) [#45289](https://github.com/nodejs/node/pull/45289)
* \[[`45ba14b3be`](https://github.com/nodejs/node/commit/45ba14b3be)] - **deps**: fix zlib compilation for CPUs without SIMD features (Anna Henningsen) [#45387](https://github.com/nodejs/node/pull/45387)
* \[[`c41e67fe1d`](https://github.com/nodejs/node/commit/c41e67fe1d)] - **deps**: update zlib to upstream 8bbd6c31 (Luigi Pinca) [#45387](https://github.com/nodejs/node/pull/45387)
* \[[`413bf9ad39`](https://github.com/nodejs/node/commit/413bf9ad39)] - **deps**: patch V8 to 10.7.193.22 (Michaël Zasso) [#45460](https://github.com/nodejs/node/pull/45460)
* \[[`ad8da86b3f`](https://github.com/nodejs/node/commit/ad8da86b3f)] - **deps**: update acorn to 8.8.1 (Node.js GitHub Bot) [#45441](https://github.com/nodejs/node/pull/45441)
* \[[`17e6031bf0`](https://github.com/nodejs/node/commit/17e6031bf0)] - **deps**: V8: cherry-pick 031b98b25cba (Michaël Zasso) [#45375](https://github.com/nodejs/node/pull/45375)
* \[[`9e0e97c121`](https://github.com/nodejs/node/commit/9e0e97c121)] - **diagnostics\_channel**: built-in channels should remain experimental (Stephen Belanger) [#45423](https://github.com/nodejs/node/pull/45423)
* \[[`44886e55e1`](https://github.com/nodejs/node/commit/44886e55e1)] - **diagnostics\_channel**: mark as stable (Stephen Belanger) [#45290](https://github.com/nodejs/node/pull/45290)
* \[[`b6b5b51687`](https://github.com/nodejs/node/commit/b6b5b51687)] - **doc**: deprecate use of invalid ports in `url.parse` (Antoine du Hamel) [#45576](https://github.com/nodejs/node/pull/45576)
* \[[`d805d5a894`](https://github.com/nodejs/node/commit/d805d5a894)] - **doc**: clarify changes in readableFlowing (Kohei Ueno) [#45554](https://github.com/nodejs/node/pull/45554)
* \[[`015842f3d2`](https://github.com/nodejs/node/commit/015842f3d2)] - **doc**: use console.error for error case in http2 (Deokjin Kim) [#45577](https://github.com/nodejs/node/pull/45577)
* \[[`4345732900`](https://github.com/nodejs/node/commit/4345732900)] - **doc**: add version description about fsPromise.constants (chlorine) [#45556](https://github.com/nodejs/node/pull/45556)
* \[[`16643dbb19`](https://github.com/nodejs/node/commit/16643dbb19)] - **doc**: add missing documentation for paramEncoding (Tobias Nießen) [#45523](https://github.com/nodejs/node/pull/45523)
* \[[`246cd358b5`](https://github.com/nodejs/node/commit/246cd358b5)] - **doc**: fix typo in threat model (Tobias Nießen) [#45558](https://github.com/nodejs/node/pull/45558)
* \[[`5b1df22db0`](https://github.com/nodejs/node/commit/5b1df22db0)] - **doc**: add Node.js Threat Model (Rafael Gonzaga) [#45223](https://github.com/nodejs/node/pull/45223)
* \[[`19d8493c92`](https://github.com/nodejs/node/commit/19d8493c92)] - **doc**: run license-builder (github-actions\[bot]) [#45553](https://github.com/nodejs/node/pull/45553)
* \[[`6f0bc097ea`](https://github.com/nodejs/node/commit/6f0bc097ea)] - **doc**: add async\_hooks migration note (Geoffrey Booth) [#45335](https://github.com/nodejs/node/pull/45335)
* \[[`118de4b44c`](https://github.com/nodejs/node/commit/118de4b44c)] - **doc**: fix RESOLVE\_ESM\_MATCH in modules.md (翠 / green) [#45280](https://github.com/nodejs/node/pull/45280)
* \[[`4de67d1ef4`](https://github.com/nodejs/node/commit/4de67d1ef4)] - **doc**: add arm64 to os.machine() (Carter Snook) [#45374](https://github.com/nodejs/node/pull/45374)
* \[[`1812a89c00`](https://github.com/nodejs/node/commit/1812a89c00)] - **doc**: add lint rule to enforce trailing commas (Antoine du Hamel) [#45471](https://github.com/nodejs/node/pull/45471)
* \[[`4128c27f66`](https://github.com/nodejs/node/commit/4128c27f66)] - **doc**: include v19.1.0 in `CHANGELOG.md` (Rafael Gonzaga) [#45462](https://github.com/nodejs/node/pull/45462)
* \[[`94a6a97ec6`](https://github.com/nodejs/node/commit/94a6a97ec6)] - **doc**: adjust wording to eliminate awkward typography (Konv) [#45398](https://github.com/nodejs/node/pull/45398)
* \[[`a6fe707b62`](https://github.com/nodejs/node/commit/a6fe707b62)] - **doc**: fix typo in maintaining-dependencies.md (Tobias Nießen) [#45428](https://github.com/nodejs/node/pull/45428)
* \[[`8906a4e58e`](https://github.com/nodejs/node/commit/8906a4e58e)] - **esm**: add JSDoc property descriptions for loader (Rich Trott) [#45370](https://github.com/nodejs/node/pull/45370)
* \[[`4e5ad9df50`](https://github.com/nodejs/node/commit/4e5ad9df50)] - **esm**: add JSDoc property descriptions for fetch (Rich Trott) [#45370](https://github.com/nodejs/node/pull/45370)
* \[[`2b760c339e`](https://github.com/nodejs/node/commit/2b760c339e)] - **fs**: fix fs.rm support for loop symlinks (Nathanael Ruf) [#45439](https://github.com/nodejs/node/pull/45439)
* \[[`e0a271e41b`](https://github.com/nodejs/node/commit/e0a271e41b)] - **gyp**: fix v8 canary build on aix (Vasili Skurydzin) [#45496](https://github.com/nodejs/node/pull/45496)
* \[[`eac26c0793`](https://github.com/nodejs/node/commit/eac26c0793)] - _**Revert**_ "**http**: headers(Distinct), trailers(Distinct) setters to be no-op" (Rich Trott) [#45527](https://github.com/nodejs/node/pull/45527)
* \[[`f208db70a0`](https://github.com/nodejs/node/commit/f208db70a0)] - **http**: add debug log for ERR\_UNESCAPED\_CHARACTERS (Aidan Temple) [#45420](https://github.com/nodejs/node/pull/45420)
* \[[`b72b2bab72`](https://github.com/nodejs/node/commit/b72b2bab72)] - **http**: add JSDoc property descriptions (Rich Trott) [#45370](https://github.com/nodejs/node/pull/45370)
* \[[`4c9159a830`](https://github.com/nodejs/node/commit/4c9159a830)] - **lib**: improve transferable abort controller exec (Yagiz Nizipli) [#45525](https://github.com/nodejs/node/pull/45525)
* \[[`5745bcbb41`](https://github.com/nodejs/node/commit/5745bcbb41)] - **lib**: improve AbortController creation duration (Yagiz Nizipli) [#45525](https://github.com/nodejs/node/pull/45525)
* \[[`38767b42fb`](https://github.com/nodejs/node/commit/38767b42fb)] - **lib**: do not throw if global property is no longer configurable (Antoine du Hamel) [#45344](https://github.com/nodejs/node/pull/45344)
* \[[`0d1b1c5df0`](https://github.com/nodejs/node/commit/0d1b1c5df0)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#45531](https://github.com/nodejs/node/pull/45531)
* \[[`208ea1a58c`](https://github.com/nodejs/node/commit/208ea1a58c)] - **meta**: update VoltrexMaster's username (Mohammed Keyvanzadeh) [#45503](https://github.com/nodejs/node/pull/45503)
* \[[`d13ea68ef6`](https://github.com/nodejs/node/commit/d13ea68ef6)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#45443](https://github.com/nodejs/node/pull/45443)
* \[[`6704e7814f`](https://github.com/nodejs/node/commit/6704e7814f)] - **meta**: be more proactive about removing from teams (Rich Trott) [#45352](https://github.com/nodejs/node/pull/45352)
* \[[`6fdd202c57`](https://github.com/nodejs/node/commit/6fdd202c57)] - **module**: require.resolve.paths returns null with node schema (MURAKAMI Masahiko) [#45147](https://github.com/nodejs/node/pull/45147)
* \[[`38f1ede379`](https://github.com/nodejs/node/commit/38f1ede379)] - **node-api**: address coverity warning (Michael Dawson) [#45563](https://github.com/nodejs/node/pull/45563)
* \[[`4a4f2802ec`](https://github.com/nodejs/node/commit/4a4f2802ec)] - **node-api**: declare type napi\_cleanup\_hook (Chengzhong Wu) [#45391](https://github.com/nodejs/node/pull/45391)
* \[[`8ff16fd8c0`](https://github.com/nodejs/node/commit/8ff16fd8c0)] - **node-api**: fix immediate napi\_remove\_wrap test (Chengzhong Wu) [#45406](https://github.com/nodejs/node/pull/45406)
* \[[`e7a5b3347b`](https://github.com/nodejs/node/commit/e7a5b3347b)] - **src**: address coverity warning in node\_file.cc (Michael Dawson) [#45565](https://github.com/nodejs/node/pull/45565)
* \[[`128c9f6fac`](https://github.com/nodejs/node/commit/128c9f6fac)] - **src**: use qualified `std::move` call in node\_http2 (Michaël Zasso) [#45555](https://github.com/nodejs/node/pull/45555)
* \[[`57bca94cb1`](https://github.com/nodejs/node/commit/57bca94cb1)] - **src**: avoid unused variables and functions (Michaël Zasso) [#45542](https://github.com/nodejs/node/pull/45542)
* \[[`649b31f5e5`](https://github.com/nodejs/node/commit/649b31f5e5)] - **src**: add missing include for `std::all_of` (Michaël Zasso) [#45541](https://github.com/nodejs/node/pull/45541)
* \[[`56f22ea47c`](https://github.com/nodejs/node/commit/56f22ea47c)] - **src**: set an appropriate thread pool size if given `--v8-pool-size=0` (Daeyeon Jeong) [#45513](https://github.com/nodejs/node/pull/45513)
* \[[`cce9e11d2d`](https://github.com/nodejs/node/commit/cce9e11d2d)] - **src**: move FsStatsOffset and kFsStatsBufferLength to node\_file.h (Joyee Cheung) [#45498](https://github.com/nodejs/node/pull/45498)
* \[[`5e5bf0c236`](https://github.com/nodejs/node/commit/5e5bf0c236)] - **src**: don't run tasks on isolate termination (Santiago Gimeno) [#45444](https://github.com/nodejs/node/pull/45444)
* \[[`10e7c2a62c`](https://github.com/nodejs/node/commit/10e7c2a62c)] - **src**: remove the unused PackageConfig class (Joyee Cheung) [#45478](https://github.com/nodejs/node/pull/45478)
* \[[`459d4481d4`](https://github.com/nodejs/node/commit/459d4481d4)] - **src**: add --max-semi-space-size to the options allowed in NODE\_OPTIONS (Emanuel Hoogeveen) [#44436](https://github.com/nodejs/node/pull/44436)
* \[[`a483d1291e`](https://github.com/nodejs/node/commit/a483d1291e)] - **src**: condense experimental warning message (Rich Trott) [#45424](https://github.com/nodejs/node/pull/45424)
* \[[`42507e68ab`](https://github.com/nodejs/node/commit/42507e68ab)] - **src,node-api**: update `napi_is_detached_arraybuffer` (Daeyeon Jeong) [#45538](https://github.com/nodejs/node/pull/45538)
* \[[`f720c5880e`](https://github.com/nodejs/node/commit/f720c5880e)] - **stream**: use ArrayBufferPrototypeGetByteLength (Yagiz Nizipli) [#45528](https://github.com/nodejs/node/pull/45528)
* \[[`c00258e24b`](https://github.com/nodejs/node/commit/c00258e24b)] - **stream**: add primordials to adapters (Yagiz Nizipli) [#45511](https://github.com/nodejs/node/pull/45511)
* \[[`5274a8f7db`](https://github.com/nodejs/node/commit/5274a8f7db)] - **stream**: avoid premature close when will not emit close (Robert Nagy) [#45301](https://github.com/nodejs/node/pull/45301)
* \[[`496912d722`](https://github.com/nodejs/node/commit/496912d722)] - **stream**: fix typo in `adapters.js` (#45515) (Kohei Ueno) [#45515](https://github.com/nodejs/node/pull/45515)
* \[[`8d96e2c723`](https://github.com/nodejs/node/commit/8d96e2c723)] - **stream**: add fast path for utf8 (Yagiz Nizipli) [#45483](https://github.com/nodejs/node/pull/45483)
* \[[`c3fe9072c6`](https://github.com/nodejs/node/commit/c3fe9072c6)] - **test**: add trailing commas in event tests (Rich Trott) [#45466](https://github.com/nodejs/node/pull/45466)
* \[[`bb4c293873`](https://github.com/nodejs/node/commit/bb4c293873)] - **test**: add trailing commas in async-hooks tests (#45549) (Antoine du Hamel) [#45549](https://github.com/nodejs/node/pull/45549)
* \[[`731e8741b2`](https://github.com/nodejs/node/commit/731e8741b2)] - **test**: add trailing commas in addons test (#45548) (Antoine du Hamel) [#45548](https://github.com/nodejs/node/pull/45548)
* \[[`d6c68ce346`](https://github.com/nodejs/node/commit/d6c68ce346)] - **test**: add trailing commas in `test/common` (#45550) (Antoine du Hamel) [#45550](https://github.com/nodejs/node/pull/45550)
* \[[`c9ba0b738d`](https://github.com/nodejs/node/commit/c9ba0b738d)] - **test**: revise pull request guide text about code (Rich Trott) [#45519](https://github.com/nodejs/node/pull/45519)
* \[[`076e9eeaeb`](https://github.com/nodejs/node/commit/076e9eeaeb)] - **test**: fix test-trace-gc-flag (Tony Gorez) [#45230](https://github.com/nodejs/node/pull/45230)
* \[[`72f2df2802`](https://github.com/nodejs/node/commit/72f2df2802)] - **test**: adapt test-v8-stats for V8 update (Michaël Zasso) [#45230](https://github.com/nodejs/node/pull/45230)
* \[[`b491504d77`](https://github.com/nodejs/node/commit/b491504d77)] - **test**: enable the WPT for `structuredClone` (Daeyeon Jeong) [#45482](https://github.com/nodejs/node/pull/45482)
* \[[`1277ffcb55`](https://github.com/nodejs/node/commit/1277ffcb55)] - **test**: add lint rule to enforce trailing commas (Antoine du Hamel) [#45468](https://github.com/nodejs/node/pull/45468)
* \[[`45b54eec55`](https://github.com/nodejs/node/commit/45b54eec55)] - **test**: update uses of \_jabber.\_tcp.google.com (Colin Ihrig) [#45451](https://github.com/nodejs/node/pull/45451)
* \[[`51213c24bd`](https://github.com/nodejs/node/commit/51213c24bd)] - **test**: add test to validate changelogs for releases (Richard Lau) [#45325](https://github.com/nodejs/node/pull/45325)
* \[[`00a3b5f7d5`](https://github.com/nodejs/node/commit/00a3b5f7d5)] - **test**: remove flaky designation for test-worker-http2-stream-terminate (Rich Trott) [#45438](https://github.com/nodejs/node/pull/45438)
* \[[`4fe5c4e167`](https://github.com/nodejs/node/commit/4fe5c4e167)] - **test**: fix flaky test-repl-sigint-nested-eval (Rich Trott) [#45354](https://github.com/nodejs/node/pull/45354)
* \[[`f79dd65333`](https://github.com/nodejs/node/commit/f79dd65333)] - **test**: add a test to ensure the correctness of timezone upgrades (Darshan Sen) [#45299](https://github.com/nodejs/node/pull/45299)
* \[[`016749ba5d`](https://github.com/nodejs/node/commit/016749ba5d)] - **test\_runner**: add initial TAP parser (Wassim Chegham) [#43525](https://github.com/nodejs/node/pull/43525)
* \[[`e9760b4ae8`](https://github.com/nodejs/node/commit/e9760b4ae8)] - **test\_runner**: support watch mode (Moshe Atlow) [#45214](https://github.com/nodejs/node/pull/45214)
* \[[`160c88ec77`](https://github.com/nodejs/node/commit/160c88ec77)] - **tools**: have test-asan use ubuntu-20.04 (Filip Skokan) [#45581](https://github.com/nodejs/node/pull/45581)
* \[[`81f63c2b28`](https://github.com/nodejs/node/commit/81f63c2b28)] - **tools**: update eslint to 8.28.0 (Node.js GitHub Bot) [#45532](https://github.com/nodejs/node/pull/45532)
* \[[`f3f1aed01a`](https://github.com/nodejs/node/commit/f3f1aed01a)] - **tools**: add automation for updating libuv dependency (Facundo Tuesca) [#45362](https://github.com/nodejs/node/pull/45362)
* \[[`d4f30f07b3`](https://github.com/nodejs/node/commit/d4f30f07b3)] - **tools**: add missing step in update-base64.sh script (Facundo Tuesca) [#45509](https://github.com/nodejs/node/pull/45509)
* \[[`cca20330cf`](https://github.com/nodejs/node/commit/cca20330cf)] - **tools**: update certdata.txt (Luigi Pinca) [#45490](https://github.com/nodejs/node/pull/45490)
* \[[`39e873139b`](https://github.com/nodejs/node/commit/39e873139b)] - **tools**: include current release in the list of released versions (Antoine du Hamel) [#45463](https://github.com/nodejs/node/pull/45463)
* \[[`8a34ef4897`](https://github.com/nodejs/node/commit/8a34ef4897)] - **tools**: update lint-md-dependencies to rollup\@3.3.0 (Node.js GitHub Bot) [#45442](https://github.com/nodejs/node/pull/45442)
* \[[`bb36acff42`](https://github.com/nodejs/node/commit/bb36acff42)] - **tools**: do not run CQ on non-fast-tracked PRs open for less than 2 days (Moshe Atlow) [#45407](https://github.com/nodejs/node/pull/45407)
* \[[`93bc2ba509`](https://github.com/nodejs/node/commit/93bc2ba509)] - **tools**: simplify .eslintrc.js (Rich Trott) [#45397](https://github.com/nodejs/node/pull/45397)
* \[[`b7f8a44c64`](https://github.com/nodejs/node/commit/b7f8a44c64)] - **tools**: simplify regex in ESLint config (Rich Trott) [#45399](https://github.com/nodejs/node/pull/45399)
* \[[`36bf87fabf`](https://github.com/nodejs/node/commit/36bf87fabf)] - **tools**: enable jsdoc/require-property-description rule (Rich Trott) [#45370](https://github.com/nodejs/node/pull/45370)
* \[[`7c6281a7d2`](https://github.com/nodejs/node/commit/7c6281a7d2)] - **tools**: dynamically determine parallelism on GitHub Actions macOS (Rich Trott) [#45350](https://github.com/nodejs/node/pull/45350)
* \[[`f441b04c11`](https://github.com/nodejs/node/commit/f441b04c11)] - **trace\_events**: add new categories (theanarkh) [#45266](https://github.com/nodejs/node/pull/45266)
* \[[`6bdd2c3884`](https://github.com/nodejs/node/commit/6bdd2c3884)] - _**Revert**_ "**url**: improve port validation" (Rich Trott) [#45517](https://github.com/nodejs/node/pull/45517)
* \[[`bbba42fcb2`](https://github.com/nodejs/node/commit/bbba42fcb2)] - **url**: remove unnecessary object call to kFormat (Yagiz Nizipli) [#45492](https://github.com/nodejs/node/pull/45492)
* \[[`7c79ba7b27`](https://github.com/nodejs/node/commit/7c79ba7b27)] - **util**: add fast path for utf8 encoding (Yagiz Nizipli) [#45412](https://github.com/nodejs/node/pull/45412)
* \[[`f86f90f839`](https://github.com/nodejs/node/commit/f86f90f839)] - **util**: improve text decoder performance (Yagiz Nizipli) [#45388](https://github.com/nodejs/node/pull/45388)
* \[[`3263ceb21a`](https://github.com/nodejs/node/commit/3263ceb21a)] - **watch**: watch for missing dependencies (Moshe Atlow) [#45348](https://github.com/nodejs/node/pull/45348)

<a id="19.1.0"></a>

## 2022-11-14, Version 19.1.0 (Current), @RafaelGSS

### Notable changes

#### Support function mocking on Node.js test runner

The `node:test` module supports mocking during testing via a top-level `mock`
object.

```js
test('spies on an object method', (t) => {
  const number = {
    value: 5,
    add(a) {
      return this.value + a;
    },
  };
  t.mock.method(number, 'add');

  assert.strictEqual(number.add(3), 8);
  assert.strictEqual(number.add.mock.calls.length, 1);
});
```

Contributed by Colin Ihrig in [#45326](https://github.com/nodejs/node/pull/45326)

#### fs.watch recursive support on Linux

`fs.watch` supports recursive watch using the `recursive: true` option.

```js
const watcher = fs.watch(testDirectory, { recursive: true });
watcher.on('change', function(event, filename) {
});
```

Contributed by Yagiz Nizipli in [#45098](https://github.com/nodejs/node/pull/45098)

#### Other notable changes

* **deps**
  * update ICU to 72.1 (Michaël Zasso) [#45068](https://github.com/nodejs/node/pull/45068)
* **doc**
  * add lukekarrys to collaborators (Luke Karrys) [#45180](https://github.com/nodejs/node/pull/45180)
  * add anonrig to collaborators (Yagiz Nizipli) [#45002](https://github.com/nodejs/node/pull/45002)
* **lib**
  * drop fetch experimental warning (Matteo Collina) [#45287](https://github.com/nodejs/node/pull/45287)
* **util**
  * (SEMVER-MINOR) add MIME utilities (Bradley Farias) [#21128](https://github.com/nodejs/node/pull/21128)
  * improve textdecoder decode performance (Yagiz Nizipli) [#45294](https://github.com/nodejs/node/pull/45294)

### Commits

* \[[`c9cf399ec7`](https://github.com/nodejs/node/commit/c9cf399ec7)] - **benchmark**: add parameters to text-decoder benchmark (Yagiz Nizipli) [#45363](https://github.com/nodejs/node/pull/45363)
* \[[`79f6bb061d`](https://github.com/nodejs/node/commit/79f6bb061d)] - **benchmark**: fix text-decoder benchmark (Yagiz Nizipli) [#45363](https://github.com/nodejs/node/pull/45363)
* \[[`a27c994ced`](https://github.com/nodejs/node/commit/a27c994ced)] - **benchmark**: add blob benchmark (Yagiz Nizipli) [#44990](https://github.com/nodejs/node/pull/44990)
* \[[`c45b6aee78`](https://github.com/nodejs/node/commit/c45b6aee78)] - **bootstrap**: merge main thread and worker thread initializations (Joyee Cheung) [#44869](https://github.com/nodejs/node/pull/44869)
* \[[`33691208df`](https://github.com/nodejs/node/commit/33691208df)] - **buffer**: fix validation of options in `Blob` constructor (Antoine du Hamel) [#45156](https://github.com/nodejs/node/pull/45156)
* \[[`7b938df296`](https://github.com/nodejs/node/commit/7b938df296)] - **build**: support Python 3.11 (Luigi Pinca) [#45191](https://github.com/nodejs/node/pull/45191)
* \[[`75e0a2d109`](https://github.com/nodejs/node/commit/75e0a2d109)] - **build**: workaround for node-core-utils (Jiawen Geng) [#45199](https://github.com/nodejs/node/pull/45199)
* \[[`f598edbdf4`](https://github.com/nodejs/node/commit/f598edbdf4)] - **build**: fix icu-small build with ICU 72.1 (Steven R. Loomis) [#45195](https://github.com/nodejs/node/pull/45195)
* \[[`29b9f4f90c`](https://github.com/nodejs/node/commit/29b9f4f90c)] - **build**: remove unused language files (Ben Noordhuis) [#45138](https://github.com/nodejs/node/pull/45138)
* \[[`3a1ee940d1`](https://github.com/nodejs/node/commit/3a1ee940d1)] - **build**: add GitHub token to auto-start-ci workflow (Richard Lau) [#45185](https://github.com/nodejs/node/pull/45185)
* \[[`17349a2f42`](https://github.com/nodejs/node/commit/17349a2f42)] - **build**: restore Windows resource file (Richard Lau) [#45042](https://github.com/nodejs/node/pull/45042)
* \[[`24e24bd063`](https://github.com/nodejs/node/commit/24e24bd063)] - **build**: add version info to timezone update PR (Darshan Sen) [#45021](https://github.com/nodejs/node/pull/45021)
* \[[`8d7aa53e6b`](https://github.com/nodejs/node/commit/8d7aa53e6b)] - **build,win**: pass --debug-nghttp2 to configure (Santiago Gimeno) [#45209](https://github.com/nodejs/node/pull/45209)
* \[[`b2e60480f3`](https://github.com/nodejs/node/commit/b2e60480f3)] - **child\_process**: validate arguments for null bytes (Darshan Sen) [#44782](https://github.com/nodejs/node/pull/44782)
* \[[`1f0edde412`](https://github.com/nodejs/node/commit/1f0edde412)] - **crypto**: handle more webcrypto errors with OperationError (Filip Skokan) [#45320](https://github.com/nodejs/node/pull/45320)
* \[[`13fb05e12b`](https://github.com/nodejs/node/commit/13fb05e12b)] - **crypto**: handle unsupported AES ciphers in webcrypto (Filip Skokan) [#45321](https://github.com/nodejs/node/pull/45321)
* \[[`c168cbfbb3`](https://github.com/nodejs/node/commit/c168cbfbb3)] - **deps**: V8: cherry-pick 56816d76c121 (Shi Pujin) [#45353](https://github.com/nodejs/node/pull/45353)
* \[[`1432474abf`](https://github.com/nodejs/node/commit/1432474abf)] - **deps**: upgrade npm to 8.19.3 (npm team) [#45322](https://github.com/nodejs/node/pull/45322)
* \[[`f35d56200d`](https://github.com/nodejs/node/commit/f35d56200d)] - **deps**: update corepack to 0.15.1 (Node.js GitHub Bot) [#45331](https://github.com/nodejs/node/pull/45331)
* \[[`44de2321aa`](https://github.com/nodejs/node/commit/44de2321aa)] - **deps**: patch V8 to 10.7.193.20 (Michaël Zasso) [#45228](https://github.com/nodejs/node/pull/45228)
* \[[`bfe3819f08`](https://github.com/nodejs/node/commit/bfe3819f08)] - **deps**: upgrade to libuv 1.44.2 (Luigi Pinca) [#42340](https://github.com/nodejs/node/pull/42340)
* \[[`0d41df96b3`](https://github.com/nodejs/node/commit/0d41df96b3)] - **deps**: update corepack to 0.15.0 (Node.js GitHub Bot) [#45235](https://github.com/nodejs/node/pull/45235)
* \[[`0d241638ca`](https://github.com/nodejs/node/commit/0d241638ca)] - **deps**: update undici to 5.12.0 (Node.js GitHub Bot) [#45236](https://github.com/nodejs/node/pull/45236)
* \[[`f58996188a`](https://github.com/nodejs/node/commit/f58996188a)] - _**Revert**_ "**deps**: make V8 compilable with older glibc" (Michaël Zasso) [#45162](https://github.com/nodejs/node/pull/45162)
* \[[`8cda730e58`](https://github.com/nodejs/node/commit/8cda730e58)] - **deps**: update ICU to 72.1 (Michaël Zasso) [#45068](https://github.com/nodejs/node/pull/45068)
* \[[`0a6ed6f710`](https://github.com/nodejs/node/commit/0a6ed6f710)] - _**Revert**_ "**deps**: V8: forward declaration of `Rtl*FunctionTable`" (Michaël Zasso) [#45119](https://github.com/nodejs/node/pull/45119)
* \[[`2f7518ada2`](https://github.com/nodejs/node/commit/2f7518ada2)] - **deps**: update timezone (Node.js GitHub Bot) [#44950](https://github.com/nodejs/node/pull/44950)
* \[[`3bfba6df79`](https://github.com/nodejs/node/commit/3bfba6df79)] - **deps**: patch V8 to 10.7.193.16 (Michaël Zasso) [#45023](https://github.com/nodejs/node/pull/45023)
* \[[`b5baaa61b3`](https://github.com/nodejs/node/commit/b5baaa61b3)] - **dns**: fix port validation (Antoine du Hamel) [#45135](https://github.com/nodejs/node/pull/45135)
* \[[`0e9bad97cc`](https://github.com/nodejs/node/commit/0e9bad97cc)] - **doc**: allow for holidays in triage response (Michael Dawson) [#45267](https://github.com/nodejs/node/pull/45267)
* \[[`d4aabb9d3d`](https://github.com/nodejs/node/commit/d4aabb9d3d)] - **doc**: include last security release date (Juan José Arboleda) [#45368](https://github.com/nodejs/node/pull/45368)
* \[[`ba45373164`](https://github.com/nodejs/node/commit/ba45373164)] - **doc**: fix email for Ashley (Michael Dawson) [#45364](https://github.com/nodejs/node/pull/45364)
* \[[`d5e5c75b13`](https://github.com/nodejs/node/commit/d5e5c75b13)] - **doc**: fix test runner's only tests section header (Colin Ihrig) [#45343](https://github.com/nodejs/node/pull/45343)
* \[[`a7c5f31c47`](https://github.com/nodejs/node/commit/a7c5f31c47)] - **doc**: run license-builder (github-actions\[bot]) [#45349](https://github.com/nodejs/node/pull/45349)
* \[[`3de125743e`](https://github.com/nodejs/node/commit/3de125743e)] - **doc**: add more info for timer.setInterval (theanarkh) [#45232](https://github.com/nodejs/node/pull/45232)
* \[[`5a1252d9b4`](https://github.com/nodejs/node/commit/5a1252d9b4)] - **doc**: use module names in stability overview table (Filip Skokan) [#45312](https://github.com/nodejs/node/pull/45312)
* \[[`4d38bf2c5f`](https://github.com/nodejs/node/commit/4d38bf2c5f)] - **doc**: add `node:` prefix for examples (Daeyeon Jeong) [#45328](https://github.com/nodejs/node/pull/45328)
* \[[`b4b6b95f48`](https://github.com/nodejs/node/commit/b4b6b95f48)] - **doc**: update name of Node.js core Slack channel (Rich Trott) [#45293](https://github.com/nodejs/node/pull/45293)
* \[[`7d7e7c316b`](https://github.com/nodejs/node/commit/7d7e7c316b)] - **doc**: fix "task\_processor.js" typo (andreysoktoev) [#45257](https://github.com/nodejs/node/pull/45257)
* \[[`b9039a54af`](https://github.com/nodejs/node/commit/b9039a54af)] - **doc**: add history section to `fetch`-related globals (Antoine du Hamel) [#45198](https://github.com/nodejs/node/pull/45198)
* \[[`d9163f1632`](https://github.com/nodejs/node/commit/d9163f1632)] - **doc**: clarify moderation in `onboarding.md` (Benjamin Gruenbaum) [#41930](https://github.com/nodejs/node/pull/41930)
* \[[`c179c1478b`](https://github.com/nodejs/node/commit/c179c1478b)] - **doc**: change make lint to make lint-md (RafaelGSS) [#45197](https://github.com/nodejs/node/pull/45197)
* \[[`58bec56fab`](https://github.com/nodejs/node/commit/58bec56fab)] - **doc**: add more lts update steps to release guide (Ruy Adorno) [#45177](https://github.com/nodejs/node/pull/45177)
* \[[`8f8d7e76ac`](https://github.com/nodejs/node/commit/8f8d7e76ac)] - **doc**: add bmuenzenmeyer to triagers (Brian Muenzenmeyer) [#45155](https://github.com/nodejs/node/pull/45155)
* \[[`de2df550f6`](https://github.com/nodejs/node/commit/de2df550f6)] - **doc**: update process.release (Filip Skokan) [#45170](https://github.com/nodejs/node/pull/45170)
* \[[`916e8760ba`](https://github.com/nodejs/node/commit/916e8760ba)] - **doc**: add link to triage guide (Brian Muenzenmeyer) [#45154](https://github.com/nodejs/node/pull/45154)
* \[[`54d806853e`](https://github.com/nodejs/node/commit/54d806853e)] - **doc**: mark Node.js 12 as End-of-Life (Rafael Gonzaga) [#45186](https://github.com/nodejs/node/pull/45186)
* \[[`3a26347649`](https://github.com/nodejs/node/commit/3a26347649)] - **doc**: add lukekarrys to collaborators (Luke Karrys) [#45180](https://github.com/nodejs/node/pull/45180)
* \[[`85cb4d795c`](https://github.com/nodejs/node/commit/85cb4d795c)] - **doc**: update mark release line lts on release guide (Ruy Adorno) [#45101](https://github.com/nodejs/node/pull/45101)
* \[[`c23e023a2d`](https://github.com/nodejs/node/commit/c23e023a2d)] - **doc**: be more definite and present tense-y (Ben Noordhuis) [#45120](https://github.com/nodejs/node/pull/45120)
* \[[`519002152b`](https://github.com/nodejs/node/commit/519002152b)] - **doc**: add major version note to release guide (Ruy Adorno) [#45054](https://github.com/nodejs/node/pull/45054)
* \[[`809e8dcbd2`](https://github.com/nodejs/node/commit/809e8dcbd2)] - **doc**: fix v14.x link maintaining openssl guide (RafaelGSS) [#45071](https://github.com/nodejs/node/pull/45071)
* \[[`9d449d389d`](https://github.com/nodejs/node/commit/9d449d389d)] - **doc**: add note about latest GitHub release (Michaël Zasso) [#45111](https://github.com/nodejs/node/pull/45111)
* \[[`ee34a3a1bc`](https://github.com/nodejs/node/commit/ee34a3a1bc)] - **doc**: mention v18.x openssl maintaining guide (Rafael Gonzaga) [#45070](https://github.com/nodejs/node/pull/45070)
* \[[`3e4033a90d`](https://github.com/nodejs/node/commit/3e4033a90d)] - **doc**: fix display of "problematic" ASCII characters (John Gardner) [#44373](https://github.com/nodejs/node/pull/44373)
* \[[`533e38b0b8`](https://github.com/nodejs/node/commit/533e38b0b8)] - **doc**: mark Node.js v17.x as EOL (KaKa) [#45110](https://github.com/nodejs/node/pull/45110)
* \[[`93a34faa39`](https://github.com/nodejs/node/commit/93a34faa39)] - **doc**: update Node.js 16 End-of-Life date (Richard Lau) [#45103](https://github.com/nodejs/node/pull/45103)
* \[[`b4beddef79`](https://github.com/nodejs/node/commit/b4beddef79)] - **doc**: fix typo in parseArgs default value (Tobias Nießen) [#45083](https://github.com/nodejs/node/pull/45083)
* \[[`e8103fd33b`](https://github.com/nodejs/node/commit/e8103fd33b)] - **doc**: updated security stewards (Michael Dawson) [#45005](https://github.com/nodejs/node/pull/45005)
* \[[`5fbccae4f0`](https://github.com/nodejs/node/commit/5fbccae4f0)] - **doc**: fix http and http2 writeEarlyHints() parameter (Fabian Meyer) [#45000](https://github.com/nodejs/node/pull/45000)
* \[[`d47f83251a`](https://github.com/nodejs/node/commit/d47f83251a)] - **doc**: run license-builder (github-actions\[bot]) [#45034](https://github.com/nodejs/node/pull/45034)
* \[[`e6bbc5033d`](https://github.com/nodejs/node/commit/e6bbc5033d)] - **doc**: improve the workflow to test release binaries (Rafael Gonzaga) [#45004](https://github.com/nodejs/node/pull/45004)
* \[[`f0c18f04f0`](https://github.com/nodejs/node/commit/f0c18f04f0)] - **doc**: fix undici version in changelog (Michael Dawson) [#44982](https://github.com/nodejs/node/pull/44982)
* \[[`ffba3218ec`](https://github.com/nodejs/node/commit/ffba3218ec)] - **doc**: add info on fixup to security release process (Michael Dawson) [#44807](https://github.com/nodejs/node/pull/44807)
* \[[`edb92f4510`](https://github.com/nodejs/node/commit/edb92f4510)] - **doc**: add anonrig to collaborators (Yagiz Nizipli) [#45002](https://github.com/nodejs/node/pull/45002)
* \[[`58334a38e8`](https://github.com/nodejs/node/commit/58334a38e8)] - **doc, async\_hooks**: improve and add migration hints (Gerhard Stöbich) [#45369](https://github.com/nodejs/node/pull/45369)
* \[[`7225a7d46b`](https://github.com/nodejs/node/commit/7225a7d46b)] - **doc, http**: add Uint8Array as allowed type (Gerhard Stöbich) [#45167](https://github.com/nodejs/node/pull/45167)
* \[[`40a5e22328`](https://github.com/nodejs/node/commit/40a5e22328)] - **esm**: protect ESM loader from prototype pollution (Antoine du Hamel) [#45175](https://github.com/nodejs/node/pull/45175)
* \[[`2e5d8e7239`](https://github.com/nodejs/node/commit/2e5d8e7239)] - **esm**: protect ESM loader from prototype pollution (Antoine du Hamel) [#45044](https://github.com/nodejs/node/pull/45044)
* \[[`c3dd696081`](https://github.com/nodejs/node/commit/c3dd696081)] - **events**: add unique events benchmark (Yagiz Nizipli) [#44657](https://github.com/nodejs/node/pull/44657)
* \[[`daff3b8b09`](https://github.com/nodejs/node/commit/daff3b8b09)] - **fs**: update todo message (Yagiz Nizipli) [#45265](https://github.com/nodejs/node/pull/45265)
* \[[`670def3d6f`](https://github.com/nodejs/node/commit/670def3d6f)] - **fs**: fix opts.filter issue in cpSync (Tho) [#45143](https://github.com/nodejs/node/pull/45143)
* \[[`34bfef91a9`](https://github.com/nodejs/node/commit/34bfef91a9)] - **(SEMVER-MINOR)** **fs**: add recursive watch to linux (Yagiz Nizipli) [#45098](https://github.com/nodejs/node/pull/45098)
* \[[`d89ca1b443`](https://github.com/nodejs/node/commit/d89ca1b443)] - **fs**: trace more fs api (theanarkh) [#45095](https://github.com/nodejs/node/pull/45095)
* \[[`1a04881494`](https://github.com/nodejs/node/commit/1a04881494)] - **http**: headers(Distinct), trailers(Distinct) setters to be no-op (Madhuri) [#45176](https://github.com/nodejs/node/pull/45176)
* \[[`8abc3f732a`](https://github.com/nodejs/node/commit/8abc3f732a)] - **http**: add priority to common http headers (James M Snell) [#45045](https://github.com/nodejs/node/pull/45045)
* \[[`316354e3d3`](https://github.com/nodejs/node/commit/316354e3d3)] - **http2**: improve session close/destroy procedures (Santiago Gimeno) [#45115](https://github.com/nodejs/node/pull/45115)
* \[[`1635140952`](https://github.com/nodejs/node/commit/1635140952)] - **http2**: fix crash on Http2Stream::diagnostic\_name() (Santiago Gimeno) [#45123](https://github.com/nodejs/node/pull/45123)
* \[[`94b7f5338c`](https://github.com/nodejs/node/commit/94b7f5338c)] - **http2**: fix debugStream method (Santiago Gimeno) [#45129](https://github.com/nodejs/node/pull/45129)
* \[[`3db37e7d1b`](https://github.com/nodejs/node/commit/3db37e7d1b)] - **inspector**: refactor `inspector/promises` to be more robust (Antoine du Hamel) [#45041](https://github.com/nodejs/node/pull/45041)
* \[[`0478e4063f`](https://github.com/nodejs/node/commit/0478e4063f)] - **lib**: add options to the heap snapshot APIs (Joyee Cheung) [#44989](https://github.com/nodejs/node/pull/44989)
* \[[`a8e901555a`](https://github.com/nodejs/node/commit/a8e901555a)] - **lib**: fix JSDoc issues (Rich Trott) [#45243](https://github.com/nodejs/node/pull/45243)
* \[[`74352842bc`](https://github.com/nodejs/node/commit/74352842bc)] - **lib**: use process.nextTick() instead of setImmediate() (Luigi Pinca) [#42340](https://github.com/nodejs/node/pull/42340)
* \[[`9f3d2f6879`](https://github.com/nodejs/node/commit/9f3d2f6879)] - **lib**: drop fetch experimental warning (Matteo Collina) [#45287](https://github.com/nodejs/node/pull/45287)
* \[[`e2181e057b`](https://github.com/nodejs/node/commit/e2181e057b)] - **lib**: fix eslint early return (RafaelGSS) [#45409](https://github.com/nodejs/node/pull/45409)
* \[[`d1726692ee`](https://github.com/nodejs/node/commit/d1726692ee)] - **lib**: fix TypeError when converting a detached buffer source (Kohei Ueno) [#44020](https://github.com/nodejs/node/pull/44020)
* \[[`d7470ad986`](https://github.com/nodejs/node/commit/d7470ad986)] - **lib**: fix `AbortSignal.timeout` parameter validation (dnalborczyk) [#42856](https://github.com/nodejs/node/pull/42856)
* \[[`c7b7f2bec2`](https://github.com/nodejs/node/commit/c7b7f2bec2)] - **lib**: add lint rule to protect against `Object.prototype.then` pollution (Antoine du Hamel) [#45061](https://github.com/nodejs/node/pull/45061)
* \[[`9ed9aa8233`](https://github.com/nodejs/node/commit/9ed9aa8233)] - **lib**: add ability to add separate event name to defineEventHandler (James M Snell) [#45032](https://github.com/nodejs/node/pull/45032)
* \[[`8b4a41e23d`](https://github.com/nodejs/node/commit/8b4a41e23d)] - **lib**: fix typo in `pre_execution.js` (Antoine du Hamel) [#45039](https://github.com/nodejs/node/pull/45039)
* \[[`cc2393c9fe`](https://github.com/nodejs/node/commit/cc2393c9fe)] - **lib**: promise version of streams.finished call clean up (Naor Tedgi (Abu Emma)) [#44862](https://github.com/nodejs/node/pull/44862)
* \[[`17ef1bbc8e`](https://github.com/nodejs/node/commit/17ef1bbc8e)] - **lib**: make properties on Blob and URL enumerable (Khafra) [#44918](https://github.com/nodejs/node/pull/44918)
* \[[`8199841e9c`](https://github.com/nodejs/node/commit/8199841e9c)] - **lib**: support more attributes for early hint link (Yagiz Nizipli) [#44874](https://github.com/nodejs/node/pull/44874)
* \[[`88c3bb609b`](https://github.com/nodejs/node/commit/88c3bb609b)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#45333](https://github.com/nodejs/node/pull/45333)
* \[[`a866e8c163`](https://github.com/nodejs/node/commit/a866e8c163)] - **meta**: update collaborator email address in README (Rich Trott) [#45251](https://github.com/nodejs/node/pull/45251)
* \[[`bfbfacad79`](https://github.com/nodejs/node/commit/bfbfacad79)] - **meta**: fix email address typo in README (Rich Trott) [#45250](https://github.com/nodejs/node/pull/45250)
* \[[`0d58bb9531`](https://github.com/nodejs/node/commit/0d58bb9531)] - **meta**: remove dont-land-on-v12 auto labeling (Moshe Atlow) [#45233](https://github.com/nodejs/node/pull/45233)
* \[[`b41b5ba658`](https://github.com/nodejs/node/commit/b41b5ba658)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#45238](https://github.com/nodejs/node/pull/45238)
* \[[`ad9a5bb61f`](https://github.com/nodejs/node/commit/ad9a5bb61f)] - **meta**: move a collaborator to emeritus (Rich Trott) [#45160](https://github.com/nodejs/node/pull/45160)
* \[[`ec8683052b`](https://github.com/nodejs/node/commit/ec8683052b)] - **meta**: move one or more collaborators to emeritus (Node.js GitHub Bot) [#45036](https://github.com/nodejs/node/pull/45036)
* \[[`7900810fb3`](https://github.com/nodejs/node/commit/7900810fb3)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#45020](https://github.com/nodejs/node/pull/45020)
* \[[`738144c311`](https://github.com/nodejs/node/commit/738144c311)] - **module**: ensure relative requires work from deleted directories (Bradley Farias) [#42384](https://github.com/nodejs/node/pull/42384)
* \[[`36acf8a13e`](https://github.com/nodejs/node/commit/36acf8a13e)] - **net**: remove \_readableState from debug statement (Rich Trott) [#45063](https://github.com/nodejs/node/pull/45063)
* \[[`aaca54c5c0`](https://github.com/nodejs/node/commit/aaca54c5c0)] - **node-api**: handle no support for external buffers (Michael Dawson) [#45181](https://github.com/nodejs/node/pull/45181)
* \[[`2105f099ea`](https://github.com/nodejs/node/commit/2105f099ea)] - **node-api,test**: fix test\_reference\_double\_free crash (Vladimir Morozov) [#44927](https://github.com/nodejs/node/pull/44927)
* \[[`2fcf851a91`](https://github.com/nodejs/node/commit/2fcf851a91)] - **os**: convert uid and gid to 32-bit signed integers (Luigi Pinca) [#42340](https://github.com/nodejs/node/pull/42340)
* \[[`dfe4237d77`](https://github.com/nodejs/node/commit/dfe4237d77)] - **perf\_hooks**: align toStringTag with other Web Performance implementations (Daeyeon Jeong) [#45157](https://github.com/nodejs/node/pull/45157)
* \[[`9d15da3341`](https://github.com/nodejs/node/commit/9d15da3341)] - **report**: add more memory info (theanarkh) [#45254](https://github.com/nodejs/node/pull/45254)
* \[[`a2620acad7`](https://github.com/nodejs/node/commit/a2620acad7)] - **report**: add rss and use/kernel cpu usage fields (theanarkh) [#45043](https://github.com/nodejs/node/pull/45043)
* \[[`66e1dc4979`](https://github.com/nodejs/node/commit/66e1dc4979)] - **report,doc**: define report version semantics (Gireesh Punathil) [#45050](https://github.com/nodejs/node/pull/45050)
* \[[`86e22b4e19`](https://github.com/nodejs/node/commit/86e22b4e19)] - **src**: track contexts in the Environment instead of AsyncHooks (Joyee Cheung) [#45282](https://github.com/nodejs/node/pull/45282)
* \[[`326d19af3d`](https://github.com/nodejs/node/commit/326d19af3d)] - **src**: resolve TODO related to inspector CVEs (Tobias Nießen) [#45341](https://github.com/nodejs/node/pull/45341)
* \[[`4e45585ca2`](https://github.com/nodejs/node/commit/4e45585ca2)] - **src**: revert is\_release to 0 (RafaelGSS) [#45315](https://github.com/nodejs/node/pull/45315)
* \[[`5d480118fb`](https://github.com/nodejs/node/commit/5d480118fb)] - **src**: print nghttp2 logs when using --debug-nghttp2 (Santiago Gimeno) [#45209](https://github.com/nodejs/node/pull/45209)
* \[[`3e46ebda3c`](https://github.com/nodejs/node/commit/3e46ebda3c)] - **src**: trace threadpool event (theanarkh) [#44458](https://github.com/nodejs/node/pull/44458)
* \[[`97547bcd14`](https://github.com/nodejs/node/commit/97547bcd14)] - **src**: lock-free init\_process\_flags (Jérémy Lal) [#45221](https://github.com/nodejs/node/pull/45221)
* \[[`42db84913b`](https://github.com/nodejs/node/commit/42db84913b)] - **src**: call uv\_library\_shutdown before DisposePlatform (theanarkh) [#45226](https://github.com/nodejs/node/pull/45226)
* \[[`aa4152a1b6`](https://github.com/nodejs/node/commit/aa4152a1b6)] - **src**: fix `crypto.privateEncrypt` fails first time (liuxingbaoyu) [#42793](https://github.com/nodejs/node/pull/42793)
* \[[`243c141b69`](https://github.com/nodejs/node/commit/243c141b69)] - **src**: clarify OptionEnvvarSettings member names (Chengzhong Wu) [#45057](https://github.com/nodejs/node/pull/45057)
* \[[`5335e29ce7`](https://github.com/nodejs/node/commit/5335e29ce7)] - **src**: let http2 streams end after session close (Santiago Gimeno) [#45153](https://github.com/nodejs/node/pull/45153)
* \[[`8d5682266e`](https://github.com/nodejs/node/commit/8d5682266e)] - **src**: remap invalid file descriptors using `dup2` (Obiwac) [#44461](https://github.com/nodejs/node/pull/44461)
* \[[`4e14ed8878`](https://github.com/nodejs/node/commit/4e14ed8878)] - **src**: remove unused `contextify_global_private_symbol` (Daeyeon Jeong) [#45128](https://github.com/nodejs/node/pull/45128)
* \[[`a8412f5677`](https://github.com/nodejs/node/commit/a8412f5677)] - **src**: forbid running watch mode in REPL (Moshe Atlow) [#45058](https://github.com/nodejs/node/pull/45058)
* \[[`162bf0ddff`](https://github.com/nodejs/node/commit/162bf0ddff)] - **src**: fix test runner coverage (Moshe Atlow) [#45055](https://github.com/nodejs/node/pull/45055)
* \[[`e5b1179630`](https://github.com/nodejs/node/commit/e5b1179630)] - **src**: optimize ALPN callback (Ben Noordhuis) [#44875](https://github.com/nodejs/node/pull/44875)
* \[[`9dc21a1f86`](https://github.com/nodejs/node/commit/9dc21a1f86)] - **src**: simplify ALPN code, remove indirection (Ben Noordhuis) [#44875](https://github.com/nodejs/node/pull/44875)
* \[[`5fce8e3495`](https://github.com/nodejs/node/commit/5fce8e3495)] - **src**: iwyu in cleanup\_queue.cc (Shelley Vohr) [#44983](https://github.com/nodejs/node/pull/44983)
* \[[`824dcfc422`](https://github.com/nodejs/node/commit/824dcfc422)] - **src**: return void in InitializeInspector() (Joyee Cheung) [#44903](https://github.com/nodejs/node/pull/44903)
* \[[`7a31ae8ab1`](https://github.com/nodejs/node/commit/7a31ae8ab1)] - **src,lib**: retrieve parsed source map url from v8 (Chengzhong Wu) [#44798](https://github.com/nodejs/node/pull/44798)
* \[[`ccb1c1e9a2`](https://github.com/nodejs/node/commit/ccb1c1e9a2)] - **stream**: add compose operator (Raz Luvaton) [#44937](https://github.com/nodejs/node/pull/44937)
* \[[`e60d9053bc`](https://github.com/nodejs/node/commit/e60d9053bc)] - **stream**: fix duplexify premature destroy (Robert Nagy) [#45133](https://github.com/nodejs/node/pull/45133)
* \[[`bc0ae3e74e`](https://github.com/nodejs/node/commit/bc0ae3e74e)] - **stream**: fix web streams have no Symbol.toStringTag (Jithil P Ponnan) [#45117](https://github.com/nodejs/node/pull/45117)
* \[[`1655532fd2`](https://github.com/nodejs/node/commit/1655532fd2)] - **stream**: don't push null from closed promise #42694 (David Halls) [#45026](https://github.com/nodejs/node/pull/45026)
* \[[`717db1d46a`](https://github.com/nodejs/node/commit/717db1d46a)] - **test**: skip test-fs-largefile if not enough disk space (Rich Trott) [#45339](https://github.com/nodejs/node/pull/45339)
* \[[`4a80aff16e`](https://github.com/nodejs/node/commit/4a80aff16e)] - **test**: fix catching failed assertion (Pavel Horal) [#45222](https://github.com/nodejs/node/pull/45222)
* \[[`66e7821506`](https://github.com/nodejs/node/commit/66e7821506)] - **test**: defer invocation checks (Luigi Pinca) [#42340](https://github.com/nodejs/node/pull/42340)
* \[[`43db0fbd49`](https://github.com/nodejs/node/commit/43db0fbd49)] - **test**: fix test-socket-write-after-fin-error (Luigi Pinca) [#42340](https://github.com/nodejs/node/pull/42340)
* \[[`d5f4d98847`](https://github.com/nodejs/node/commit/d5f4d98847)] - **test**: make `test-eventemitter-asyncresource.js` shorter (Juan José) [#45146](https://github.com/nodejs/node/pull/45146)
* \[[`7428651100`](https://github.com/nodejs/node/commit/7428651100)] - **test**: convert test-debugger-pid to async/await (Luke Karrys) [#45179](https://github.com/nodejs/node/pull/45179)
* \[[`f10f2c1121`](https://github.com/nodejs/node/commit/f10f2c1121)] - **test**: fix textdecoder test for small-icu builds (Richard Lau) [#45225](https://github.com/nodejs/node/pull/45225)
* \[[`eed799bd31`](https://github.com/nodejs/node/commit/eed799bd31)] - **test**: improve test coverage in `test-event-capture-rejections.js` (Juan José) [#45148](https://github.com/nodejs/node/pull/45148)
* \[[`069747bfdd`](https://github.com/nodejs/node/commit/069747bfdd)] - **test**: fix timeout of test-heap-prof.js in riscv devices (Yu Gu) [#42674](https://github.com/nodejs/node/pull/42674)
* \[[`ddb7df76de`](https://github.com/nodejs/node/commit/ddb7df76de)] - **test**: deflake test-http2-empty-frame-without-eof (Santiago Gimeno) [#45212](https://github.com/nodejs/node/pull/45212)
* \[[`02ebde39d3`](https://github.com/nodejs/node/commit/02ebde39d3)] - **test**: use common/tmpdir in watch-mode ipc test (Richard Lau) [#45211](https://github.com/nodejs/node/pull/45211)
* \[[`f9bc40a1fc`](https://github.com/nodejs/node/commit/f9bc40a1fc)] - **test**: use uv\_sleep() where possible (Santiago Gimeno) [#45124](https://github.com/nodejs/node/pull/45124)
* \[[`3c7ea23b8f`](https://github.com/nodejs/node/commit/3c7ea23b8f)] - **test**: fix typo in `test/parallel/test-fs-rm.js` (Tim Shilov) [#44882](https://github.com/nodejs/node/pull/44882)
* \[[`b39dcde056`](https://github.com/nodejs/node/commit/b39dcde056)] - **test**: remove a snapshot blob from test-inspect-address-in-use.js (Daeyeon Jeong) [#45132](https://github.com/nodejs/node/pull/45132)
* \[[`fabed9bdc8`](https://github.com/nodejs/node/commit/fabed9bdc8)] - **test**: add test for Module.\_stat (Darshan Sen) [#44713](https://github.com/nodejs/node/pull/44713)
* \[[`2b3b291c97`](https://github.com/nodejs/node/commit/2b3b291c97)] - **test**: watch mode inspect restart repeatedly (Moshe Atlow) [#45060](https://github.com/nodejs/node/pull/45060)
* \[[`17e86e4188`](https://github.com/nodejs/node/commit/17e86e4188)] - **test**: remove experimental-wasm-threads flag (Michaël Zasso) [#45074](https://github.com/nodejs/node/pull/45074)
* \[[`f0480d68e9`](https://github.com/nodejs/node/commit/f0480d68e9)] - **test**: remove unnecessary noop function args to `mustCall()` (Antoine du Hamel) [#45047](https://github.com/nodejs/node/pull/45047)
* \[[`82e6043118`](https://github.com/nodejs/node/commit/82e6043118)] - **test**: mark test-watch-mode\* as flaky on all platforms (Pierrick Bouvier) [#45049](https://github.com/nodejs/node/pull/45049)
* \[[`26a2ae2489`](https://github.com/nodejs/node/commit/26a2ae2489)] - **test**: wrap missing `common.mustCall` (Moshe Atlow) [#45064](https://github.com/nodejs/node/pull/45064)
* \[[`8662399cda`](https://github.com/nodejs/node/commit/8662399cda)] - **test**: remove mentions of `--experimental-async-stack-tagging-api` flag (Simon) [#45051](https://github.com/nodejs/node/pull/45051)
* \[[`71b8d506ed`](https://github.com/nodejs/node/commit/71b8d506ed)] - **test**: improve assertions in `test-repl-unsupported-option.js` (Juan José) [#44953](https://github.com/nodejs/node/pull/44953)
* \[[`dbc696d363`](https://github.com/nodejs/node/commit/dbc696d363)] - **test**: remove unnecessary noop function args to mustCall() (Rich Trott) [#45027](https://github.com/nodejs/node/pull/45027)
* \[[`c1ca19fb06`](https://github.com/nodejs/node/commit/c1ca19fb06)] - **test**: update WPT resources (Khaidi Chu) [#44948](https://github.com/nodejs/node/pull/44948)
* \[[`43677e5a34`](https://github.com/nodejs/node/commit/43677e5a34)] - **test**: skip test depending on `overlapped-checker` when not available (Antoine du Hamel) [#45015](https://github.com/nodejs/node/pull/45015)
* \[[`3519d74e87`](https://github.com/nodejs/node/commit/3519d74e87)] - **test**: improve test coverage for `os` package (Juan José) [#44959](https://github.com/nodejs/node/pull/44959)
* \[[`ea0cfc9a83`](https://github.com/nodejs/node/commit/ea0cfc9a83)] - **test**: add test to improve coverage in http2-compat-serverresponse (Cesar Mario Diaz) [#44970](https://github.com/nodejs/node/pull/44970)
* \[[`482578682c`](https://github.com/nodejs/node/commit/482578682c)] - **test**: improve test coverage in `test-child-process-spawn-argv0.js` (Juan José) [#44955](https://github.com/nodejs/node/pull/44955)
* \[[`a618dc3c3e`](https://github.com/nodejs/node/commit/a618dc3c3e)] - **test**: use CHECK instead of EXPECT where necessary (Tobias Nießen) [#44795](https://github.com/nodejs/node/pull/44795)
* \[[`c59d3b76e6`](https://github.com/nodejs/node/commit/c59d3b76e6)] - **test**: refactor promises to async/await (Madhuri) [#44980](https://github.com/nodejs/node/pull/44980)
* \[[`36c5927c60`](https://github.com/nodejs/node/commit/36c5927c60)] - **test,crypto**: update WebCryptoAPI WPT (Filip Skokan) [#45165](https://github.com/nodejs/node/pull/45165)
* \[[`6158d740f3`](https://github.com/nodejs/node/commit/6158d740f3)] - **test\_runner**: support function mocking (Colin Ihrig) [#45326](https://github.com/nodejs/node/pull/45326)
* \[[`920804dc46`](https://github.com/nodejs/node/commit/920804dc46)] - **test\_runner**: avoid swallowing of asynchronously thrown errors (MURAKAMI Masahiko) [#45264](https://github.com/nodejs/node/pull/45264)
* \[[`8e7f9de45e`](https://github.com/nodejs/node/commit/8e7f9de45e)] - **test\_runner**: fix afterEach not running on test failures (Jithil P Ponnan) [#45204](https://github.com/nodejs/node/pull/45204)
* \[[`0040030443`](https://github.com/nodejs/node/commit/0040030443)] - **test\_runner**: report tap subtest in order (Moshe Atlow) [#45220](https://github.com/nodejs/node/pull/45220)
* \[[`afa8291c7c`](https://github.com/nodejs/node/commit/afa8291c7c)] - **test\_runner**: call {before,after}Each() on suites (Colin Ihrig) [#45161](https://github.com/nodejs/node/pull/45161)
* \[[`ff174b0937`](https://github.com/nodejs/node/commit/ff174b0937)] - **test\_runner**: add extra fields in AssertionError YAML (Bryan English) [#44952](https://github.com/nodejs/node/pull/44952)
* \[[`bf868fdfab`](https://github.com/nodejs/node/commit/bf868fdfab)] - **(SEMVER-MINOR)** **tls**: add "ca" property to certificate object (Ben Noordhuis) [#44935](https://github.com/nodejs/node/pull/44935)
* \[[`e8075fd1f8`](https://github.com/nodejs/node/commit/e8075fd1f8)] - **tools**: add automation for updating acorn dependency (Facundo Tuesca) [#45357](https://github.com/nodejs/node/pull/45357)
* \[[`9aa305ff3e`](https://github.com/nodejs/node/commit/9aa305ff3e)] - **tools**: add documentation regarding our api tooling (Claudio Wunder) [#45270](https://github.com/nodejs/node/pull/45270)
* \[[`76cbc07f9b`](https://github.com/nodejs/node/commit/76cbc07f9b)] - **tools**: allow scripts to run from anywhere (Luigi Pinca) [#45361](https://github.com/nodejs/node/pull/45361)
* \[[`aa875a4d6a`](https://github.com/nodejs/node/commit/aa875a4d6a)] - **tools**: update eslint to 8.27.0 (Node.js GitHub Bot) [#45358](https://github.com/nodejs/node/pull/45358)
* \[[`4b71db13ae`](https://github.com/nodejs/node/commit/4b71db13ae)] - **tools**: update eslint to 8.26.0 (Node.js GitHub Bot) [#45243](https://github.com/nodejs/node/pull/45243)
* \[[`63267dfefb`](https://github.com/nodejs/node/commit/63267dfefb)] - **tools**: update lint-md-dependencies to rollup\@3.2.5 (Node.js GitHub Bot) [#45332](https://github.com/nodejs/node/pull/45332)
* \[[`e275859138`](https://github.com/nodejs/node/commit/e275859138)] - **tools**: fix stability index generation (Antoine du Hamel) [#45346](https://github.com/nodejs/node/pull/45346)
* \[[`97fe8bacb1`](https://github.com/nodejs/node/commit/97fe8bacb1)] - **tools**: increase macOS cores to 3 on GitHub CI (Rich Trott) [#45340](https://github.com/nodejs/node/pull/45340)
* \[[`eda4ae51ca`](https://github.com/nodejs/node/commit/eda4ae51ca)] - **tools**: add automation for updating base64 dependency (Facundo Tuesca) [#45300](https://github.com/nodejs/node/pull/45300)
* \[[`2ee052f794`](https://github.com/nodejs/node/commit/2ee052f794)] - **tools**: fix `request-ci-failed` comment (Antoine du Hamel) [#45291](https://github.com/nodejs/node/pull/45291)
* \[[`e118dd88fd`](https://github.com/nodejs/node/commit/e118dd88fd)] - **tools**: refactor dynamic strings creation in shell scripts (Antoine du Hamel) [#45240](https://github.com/nodejs/node/pull/45240)
* \[[`ba89cea683`](https://github.com/nodejs/node/commit/ba89cea683)] - **tools**: update lint-md-dependencies (Node.js GitHub Bot) [#45237](https://github.com/nodejs/node/pull/45237)
* \[[`786f086800`](https://github.com/nodejs/node/commit/786f086800)] - **tools**: use Python 3.11 in GitHub Actions workflows (Luigi Pinca) [#45191](https://github.com/nodejs/node/pull/45191)
* \[[`0738d14fa4`](https://github.com/nodejs/node/commit/0738d14fa4)] - **tools**: fix `request-ci-failed` comment (Antoine du Hamel) [#45218](https://github.com/nodejs/node/pull/45218)
* \[[`49be13ccd8`](https://github.com/nodejs/node/commit/49be13ccd8)] - **tools**: keep Emeriti lists case-insensitive alphabetic (Rich Trott) [#45159](https://github.com/nodejs/node/pull/45159)
* \[[`6e30d2231b`](https://github.com/nodejs/node/commit/6e30d2231b)] - **tools**: update actions/setup-python to v4 (Yagiz Nizipli) [#45178](https://github.com/nodejs/node/pull/45178)
* \[[`a4158692d7`](https://github.com/nodejs/node/commit/a4158692d7)] - **tools**: update V8 gypfiles for RISC-V (Andreas Schwab) [#45149](https://github.com/nodejs/node/pull/45149)
* \[[`c43bc2169f`](https://github.com/nodejs/node/commit/c43bc2169f)] - **tools**: fix `create-or-update-pull-request-action` hash on GHA (Antoine du Hamel) [#45166](https://github.com/nodejs/node/pull/45166)
* \[[`2ccc03ec32`](https://github.com/nodejs/node/commit/2ccc03ec32)] - **tools**: update gr2m/create-or-update-pull-request-action (Luigi Pinca) [#45022](https://github.com/nodejs/node/pull/45022)
* \[[`a70b27629f`](https://github.com/nodejs/node/commit/a70b27629f)] - **tools**: do not use the set-output command in workflows (Luigi Pinca) [#45024](https://github.com/nodejs/node/pull/45024)
* \[[`025e616662`](https://github.com/nodejs/node/commit/025e616662)] - **tools**: update lint-md-dependencies (Node.js GitHub Bot) [#45019](https://github.com/nodejs/node/pull/45019)
* \[[`732f9a78d3`](https://github.com/nodejs/node/commit/732f9a78d3)] - **trace\_events**: fix getCategories (theanarkh) [#45092](https://github.com/nodejs/node/pull/45092)
* \[[`1bc84ce52c`](https://github.com/nodejs/node/commit/1bc84ce52c)] - **url**: remove \t \n \r in url.parse() similar to WHATWG (Rich Trott) [#45116](https://github.com/nodejs/node/pull/45116)
* \[[`84e7388160`](https://github.com/nodejs/node/commit/84e7388160)] - **url**: improve port validation (Rich Trott) [#45012](https://github.com/nodejs/node/pull/45012)
* \[[`02cff4a3d3`](https://github.com/nodejs/node/commit/02cff4a3d3)] - **url**: improve url.parse() compliance with WHATWG URL (Rich Trott) [#45011](https://github.com/nodejs/node/pull/45011)
* \[[`89390a6be2`](https://github.com/nodejs/node/commit/89390a6be2)] - **util**: improve text-decoder performance (Yagiz Nizipli) [#45363](https://github.com/nodejs/node/pull/45363)
* \[[`0deed8daeb`](https://github.com/nodejs/node/commit/0deed8daeb)] - **util**: improve textdecoder decode performance (Yagiz Nizipli) [#45294](https://github.com/nodejs/node/pull/45294)
* \[[`d41f8ffc36`](https://github.com/nodejs/node/commit/d41f8ffc36)] - **(SEMVER-MINOR)** **util**: add MIME utilities (#21128) (Bradley Farias) [#21128](https://github.com/nodejs/node/pull/21128)

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
