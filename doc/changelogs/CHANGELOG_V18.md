# Node.js 18 ChangeLog

<!--lint disable maximum-line-length no-literal-urls prohibited-strings-->

<table>
<tr>
<th>Current</th>
</tr>
<tr>
<td>
<a href="#18.11.0">18.11.0</a><br/>
<a href="#18.10.0">18.10.0</a><br/>
<a href="#18.9.1">18.9.1</a><br/>
<a href="#18.9.0">18.9.0</a><br/>
<a href="#18.8.0">18.8.0</a><br/>
<a href="#18.7.0">18.7.0</a><br/>
<a href="#18.6.0">18.6.0</a><br/>
<a href="#18.5.0">18.5.0</a><br/>
<a href="#18.4.0">18.4.0</a><br/>
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

<a id="18.11.0"></a>

## 2022-10-13, Version 18.11.0 (Current), @danielleadams

### watch mode (experimental)

Running in 'watch' mode using `node --watch` restarts the process when an imported file is changed.

Contributed by Moshe Atlow in [#44366](https://github.com/nodejs/node/pull/44366)

### Other notable changes

* **fs**:
  * (SEMVER-MINOR) add `FileHandle.prototype.readLines` (Antoine du Hamel) [#42590](https://github.com/nodejs/node/pull/42590)
* **http**:
  * (SEMVER-MINOR) add writeEarlyHints function to ServerResponse (Wing) [#44180](https://github.com/nodejs/node/pull/44180)
* **http2**:
  * (SEMVER-MINOR) make early hints generic (Yagiz Nizipli) [#44820](https://github.com/nodejs/node/pull/44820)
* **lib**:
  * (SEMVER-MINOR) refactor transferable AbortSignal (flakey5) [#44048](https://github.com/nodejs/node/pull/44048)
* **src**:
  * (SEMVER-MINOR) add detailed embedder process initialization API (Anna Henningsen) [#44121](https://github.com/nodejs/node/pull/44121)
* **util**:
  * (SEMVER-MINOR) add default value option to parsearg (Manuel Spigolon) [#44631](https://github.com/nodejs/node/pull/44631)

### Commits

* \[[`27b4b782ce`](https://github.com/nodejs/node/commit/27b4b782ce)] - **benchmark**: add vm context global proxy benchmark (Joyee Cheung) [#44796](https://github.com/nodejs/node/pull/44796)
* \[[`4e82521af1`](https://github.com/nodejs/node/commit/4e82521af1)] - **bootstrap**: update comments in bootstrap/node.js (Joyee Cheung) [#44726](https://github.com/nodejs/node/pull/44726)
* \[[`725be0ea50`](https://github.com/nodejs/node/commit/725be0ea50)] - **buffer**: initialize TextDecoder once on blob.text() (Yagiz Nizipli) [#44787](https://github.com/nodejs/node/pull/44787)
* \[[`653c3b1f62`](https://github.com/nodejs/node/commit/653c3b1f62)] - **buffer,lib**: update atob to align wpt's base64.json (Khaidi Chu) [#43901](https://github.com/nodejs/node/pull/43901)
* \[[`37808b3355`](https://github.com/nodejs/node/commit/37808b3355)] - **build**: convert V8 test JSON to JUnit XML (Keyhan Vakil) [#44049](https://github.com/nodejs/node/pull/44049)
* \[[`f92871a52b`](https://github.com/nodejs/node/commit/f92871a52b)] - **build**: update timezone-update.yml (Alex) [#44717](https://github.com/nodejs/node/pull/44717)
* \[[`f85d3471ee`](https://github.com/nodejs/node/commit/f85d3471ee)] - **child\_process**: remove lookup of undefined property (Colin Ihrig) [#44766](https://github.com/nodejs/node/pull/44766)
* \[[`2f5f41c315`](https://github.com/nodejs/node/commit/2f5f41c315)] - **(SEMVER-MINOR)** **cli**: add `--watch` (Moshe Atlow) [#44366](https://github.com/nodejs/node/pull/44366)
* \[[`7fb9cc70f3`](https://github.com/nodejs/node/commit/7fb9cc70f3)] - **cluster**: use inspector utils (Moshe Atlow) [#44592](https://github.com/nodejs/node/pull/44592)
* \[[`99a2c16040`](https://github.com/nodejs/node/commit/99a2c16040)] - **crypto**: add causes to applicable webcrypto's OperationError (Filip Skokan) [#44890](https://github.com/nodejs/node/pull/44890)
* \[[`e0fbba0939`](https://github.com/nodejs/node/commit/e0fbba0939)] - **crypto**: use EVP\_PKEY\_CTX\_set\_dsa\_paramgen\_q\_bits when available (David Benjamin) [#44561](https://github.com/nodejs/node/pull/44561)
* \[[`a90386b0a1`](https://github.com/nodejs/node/commit/a90386b0a1)] - **deps**: update undici to 5.11.0 (Node.js GitHub Bot) [#44929](https://github.com/nodejs/node/pull/44929)
* \[[`aa68d40fbf`](https://github.com/nodejs/node/commit/aa68d40fbf)] - **deps**: update corepack to 0.14.2 (Node.js GitHub Bot) [#44775](https://github.com/nodejs/node/pull/44775)
* \[[`c892f35815`](https://github.com/nodejs/node/commit/c892f35815)] - **deps**: V8: fix debug build (Ben Noordhuis) [#44392](https://github.com/nodejs/node/pull/44392)
* \[[`91514393dc`](https://github.com/nodejs/node/commit/91514393dc)] - **dns**: support dns module in the snapshot (Joyee Cheung) [#44633](https://github.com/nodejs/node/pull/44633)
* \[[`ce3cb29319`](https://github.com/nodejs/node/commit/ce3cb29319)] - **doc**: add fsPromises.readFile() example (Tierney Cyren) [#40237](https://github.com/nodejs/node/pull/40237)
* \[[`97df9b84a2`](https://github.com/nodejs/node/commit/97df9b84a2)] - **doc**: improve building doc for Android (BuShe Pie) [#44888](https://github.com/nodejs/node/pull/44888)
* \[[`8c69da893b`](https://github.com/nodejs/node/commit/8c69da893b)] - **doc**: mention `corepack prepare` supports tag or range (Michael Rienstra) [#44646](https://github.com/nodejs/node/pull/44646)
* \[[`842bc64833`](https://github.com/nodejs/node/commit/842bc64833)] - **doc**: remove Legacy status from querystring (Rich Trott) [#44912](https://github.com/nodejs/node/pull/44912)
* \[[`ddb5402f5f`](https://github.com/nodejs/node/commit/ddb5402f5f)] - **doc**: fix label name in collaborator guide (Rich Trott) [#44920](https://github.com/nodejs/node/pull/44920)
* \[[`d08b024a3d`](https://github.com/nodejs/node/commit/d08b024a3d)] - **doc**: fix typo in Node.js 12 changelog (Lorand Horvath) [#42880](https://github.com/nodejs/node/pull/42880)
* \[[`b6b9c427c5`](https://github.com/nodejs/node/commit/b6b9c427c5)] - **doc**: move release keys we don't use anymore in README (Rich Trott) [#44899](https://github.com/nodejs/node/pull/44899)
* \[[`e92b074b32`](https://github.com/nodejs/node/commit/e92b074b32)] - **doc**: fix grammar in dns docs (#44850) (Colin Ihrig) [#44850](https://github.com/nodejs/node/pull/44850)
* \[[`780144c339`](https://github.com/nodejs/node/commit/780144c339)] - **doc**: remove unnecessary leading commas (Colin Ihrig) [#44854](https://github.com/nodejs/node/pull/44854)
* \[[`6ae9bc8fbc`](https://github.com/nodejs/node/commit/6ae9bc8fbc)] - **doc**: add extra step for reporter pre-approval (Rafael Gonzaga) [#44806](https://github.com/nodejs/node/pull/44806)
* \[[`ccf31d8bca`](https://github.com/nodejs/node/commit/ccf31d8bca)] - **doc**: add anchor link for --preserve-symlinks (Kohei Ueno) [#44858](https://github.com/nodejs/node/pull/44858)
* \[[`7c5c19ee54`](https://github.com/nodejs/node/commit/7c5c19ee54)] - **doc**: update node prefix require.cache example (Simone Busoli) [#44724](https://github.com/nodejs/node/pull/44724)
* \[[`2a5bce6318`](https://github.com/nodejs/node/commit/2a5bce6318)] - **doc**: include last security release date (Vladimir de Turckheim) [#44794](https://github.com/nodejs/node/pull/44794)
* \[[`4efaf4265c`](https://github.com/nodejs/node/commit/4efaf4265c)] - **doc**: remove "currently" and comma splice from child\_process.md (Rich Trott) [#44789](https://github.com/nodejs/node/pull/44789)
* \[[`3627616b40`](https://github.com/nodejs/node/commit/3627616b40)] - **doc,crypto**: mark experimental algorithms more visually (Filip Skokan) [#44892](https://github.com/nodejs/node/pull/44892)
* \[[`3c653cf23a`](https://github.com/nodejs/node/commit/3c653cf23a)] - **doc,crypto**: add missing CFRG curve algorithms to supported lists (Filip Skokan) [#44876](https://github.com/nodejs/node/pull/44876)
* \[[`70f55020d3`](https://github.com/nodejs/node/commit/70f55020d3)] - **doc,crypto**: add null length to crypto.subtle.deriveBits (Filip Skokan) [#44876](https://github.com/nodejs/node/pull/44876)
* \[[`910fbd0ece`](https://github.com/nodejs/node/commit/910fbd0ece)] - **esm**: fix duplicated test (Geoffrey Booth) [#44779](https://github.com/nodejs/node/pull/44779)
* \[[`bc00f3bde1`](https://github.com/nodejs/node/commit/bc00f3bde1)] - **fs**: fix opts.filter issue in cp async (Tho) [#44922](https://github.com/nodejs/node/pull/44922)
* \[[`11d1c23fa0`](https://github.com/nodejs/node/commit/11d1c23fa0)] - **(SEMVER-MINOR)** **fs**: add `FileHandle.prototype.readLines` (Antoine du Hamel) [#42590](https://github.com/nodejs/node/pull/42590)
* \[[`67fb76519a`](https://github.com/nodejs/node/commit/67fb76519a)] - **fs**: improve promise based readFile performance for big files (Ruben Bridgewater) [#44295](https://github.com/nodejs/node/pull/44295)
* \[[`dc6379bdc2`](https://github.com/nodejs/node/commit/dc6379bdc2)] - **fs**: don't hard code name in validatePosition() (Colin Ihrig) [#44767](https://github.com/nodejs/node/pull/44767)
* \[[`eb19b1e97c`](https://github.com/nodejs/node/commit/eb19b1e97c)] - **http**: be more aggressive to reply 400, 408 and 431 (ywave620) [#44818](https://github.com/nodejs/node/pull/44818)
* \[[`4c869c8d9e`](https://github.com/nodejs/node/commit/4c869c8d9e)] - **(SEMVER-MINOR)** **http**: add writeEarlyHints function to ServerResponse (Wing) [#44180](https://github.com/nodejs/node/pull/44180)
* \[[`9c7e66478c`](https://github.com/nodejs/node/commit/9c7e66478c)] - **(SEMVER-MINOR)** **http2**: make early hints generic (Yagiz Nizipli) [#44820](https://github.com/nodejs/node/pull/44820)
* \[[`3f20e5b15c`](https://github.com/nodejs/node/commit/3f20e5b15c)] - **(SEMVER-MINOR)** **lib**: refactor transferable AbortSignal (flakey5) [#44048](https://github.com/nodejs/node/pull/44048)
* \[[`ada7d82b16`](https://github.com/nodejs/node/commit/ada7d82b16)] - **lib**: require JSDoc in internal validators code (Rich Trott) [#44896](https://github.com/nodejs/node/pull/44896)
* \[[`67eaa303af`](https://github.com/nodejs/node/commit/67eaa303af)] - **lib**: add cause to DOMException (flakey5) [#44703](https://github.com/nodejs/node/pull/44703)
* \[[`0db86ee98e`](https://github.com/nodejs/node/commit/0db86ee98e)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#44930](https://github.com/nodejs/node/pull/44930)
* \[[`2efe4d985b`](https://github.com/nodejs/node/commit/2efe4d985b)] - **meta**: label test.js and test.md with test\_runner label (Moshe Atlow) [#44863](https://github.com/nodejs/node/pull/44863)
* \[[`fd9feb3a6c`](https://github.com/nodejs/node/commit/fd9feb3a6c)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#44857](https://github.com/nodejs/node/pull/44857)
* \[[`a854bb39c9`](https://github.com/nodejs/node/commit/a854bb39c9)] - **node-api**: create reference only when needed (Gerhard Stöbich) [#44827](https://github.com/nodejs/node/pull/44827)
* \[[`fd5c26b8db`](https://github.com/nodejs/node/commit/fd5c26b8db)] - **path**: change basename() argument from ext to suffix (Rich Trott) [#44774](https://github.com/nodejs/node/pull/44774)
* \[[`803fbfb168`](https://github.com/nodejs/node/commit/803fbfb168)] - **process**: fix uid/gid validation to avoid crash (Tobias Nießen) [#44910](https://github.com/nodejs/node/pull/44910)
* \[[`9f2dd48fc3`](https://github.com/nodejs/node/commit/9f2dd48fc3)] - **src**: remove uid\_t/gid\_t casts (Tobias Nießen) [#44914](https://github.com/nodejs/node/pull/44914)
* \[[`3abb607f3a`](https://github.com/nodejs/node/commit/3abb607f3a)] - **src**: remove UncheckedMalloc(0) workaround (Tobias Nießen) [#44543](https://github.com/nodejs/node/pull/44543)
* \[[`0606f9298f`](https://github.com/nodejs/node/commit/0606f9298f)] - **src**: deduplicate setting RSA OAEP label (Tobias Nießen) [#44849](https://github.com/nodejs/node/pull/44849)
* \[[`daf3152f7e`](https://github.com/nodejs/node/commit/daf3152f7e)] - **src**: implement GetDetachedness() in MemoryRetainerNode (Joyee Cheung) [#44803](https://github.com/nodejs/node/pull/44803)
* \[[`7ca77dd4ef`](https://github.com/nodejs/node/commit/7ca77dd4ef)] - **src**: avoid X509\_free in loops in crypto\_x509.cc (Tobias Nießen) [#44855](https://github.com/nodejs/node/pull/44855)
* \[[`781ad96227`](https://github.com/nodejs/node/commit/781ad96227)] - **src**: use OnScopeLeave instead of multiple free() (Tobias Nießen) [#44852](https://github.com/nodejs/node/pull/44852)
* \[[`b27b336a7a`](https://github.com/nodejs/node/commit/b27b336a7a)] - **src**: remove ParseIP() in cares\_wrap.cc (Tobias Nießen) [#44771](https://github.com/nodejs/node/pull/44771)
* \[[`f99f5d3c01`](https://github.com/nodejs/node/commit/f99f5d3c01)] - **(SEMVER-MINOR)** **src**: add detailed embedder process initialization API (Anna Henningsen) [#44121](https://github.com/nodejs/node/pull/44121)
* \[[`281fd7a09a`](https://github.com/nodejs/node/commit/281fd7a09a)] - **src,stream**: improve DoWrite() and Write() (ywave620) [#44434](https://github.com/nodejs/node/pull/44434)
* \[[`a33cc22bf7`](https://github.com/nodejs/node/commit/a33cc22bf7)] - **src,worker**: fix race of WorkerHeapSnapshotTaker (ywave620) [#44745](https://github.com/nodejs/node/pull/44745)
* \[[`f300f197da`](https://github.com/nodejs/node/commit/f300f197da)] - **stream**: handle enqueuing chunks when a pending BYOB pull request exists (Daeyeon Jeong) [#44770](https://github.com/nodejs/node/pull/44770)
* \[[`9ac029ea11`](https://github.com/nodejs/node/commit/9ac029ea11)] - **test**: bump memory limit for abort fatal error (Danielle Adams) [#44984](https://github.com/nodejs/node/pull/44984)
* \[[`b9b671f25f`](https://github.com/nodejs/node/commit/b9b671f25f)] - **test**: debug watch mode inspect (Moshe Atlow) [#44861](https://github.com/nodejs/node/pull/44861)
* \[[`2308b71d09`](https://github.com/nodejs/node/commit/2308b71d09)] - **test**: don't clobber RegExp.$\_ on startup (Ben Noordhuis) [#44864](https://github.com/nodejs/node/pull/44864)
* \[[`fe91bebb67`](https://github.com/nodejs/node/commit/fe91bebb67)] - **test**: loosen test for negative timestamps in `test-fs-stat-date` (Livia Medeiros) [#44707](https://github.com/nodejs/node/pull/44707)
* \[[`a080608552`](https://github.com/nodejs/node/commit/a080608552)] - **test**: check `--test` is disallowed in NODE\_OPTIONS (Kohei Ueno) [#44846](https://github.com/nodejs/node/pull/44846)
* \[[`dc2af265d7`](https://github.com/nodejs/node/commit/dc2af265d7)] - **test**: improve lib/internal/source\_map/source\_map.js coverage (MURAKAMI Masahiko) [#42771](https://github.com/nodejs/node/pull/42771)
* \[[`60a05d6dea`](https://github.com/nodejs/node/commit/60a05d6dea)] - **test**: skip some binding tests on IBMi PASE (Richard Lau) [#44810](https://github.com/nodejs/node/pull/44810)
* \[[`8dacedaa3d`](https://github.com/nodejs/node/commit/8dacedaa3d)] - **test**: remove unused variable in addon test (Joyee Cheung) [#44809](https://github.com/nodejs/node/pull/44809)
* \[[`c54cee1c3f`](https://github.com/nodejs/node/commit/c54cee1c3f)] - **test**: check server status in test-tls-psk-client (Richard Lau) [#44824](https://github.com/nodejs/node/pull/44824)
* \[[`ee3c6a4dc5`](https://github.com/nodejs/node/commit/ee3c6a4dc5)] - **test**: use async/await in test-debugger-exceptions (pete3249) [#44690](https://github.com/nodejs/node/pull/44690)
* \[[`9f14625fe5`](https://github.com/nodejs/node/commit/9f14625fe5)] - **test**: use async/await in test-debugger-help (Chandana) [#44686](https://github.com/nodejs/node/pull/44686)
* \[[`8033ad846b`](https://github.com/nodejs/node/commit/8033ad846b)] - **test**: update test-debugger-scripts to use await/async (mmeenapriya) [#44692](https://github.com/nodejs/node/pull/44692)
* \[[`f4f08be384`](https://github.com/nodejs/node/commit/f4f08be384)] - **test**: use await in test-debugger-invalid-json (Anjana Krishnakumar Vellore) [#44689](https://github.com/nodejs/node/pull/44689)
* \[[`d2f36169f3`](https://github.com/nodejs/node/commit/d2f36169f3)] - **test**: use async/await in test-debugger-random-port-with-inspect-port (Monu-Chaudhary) [#44695](https://github.com/nodejs/node/pull/44695)
* \[[`ddf029725b`](https://github.com/nodejs/node/commit/ddf029725b)] - **test**: use async/await in test-debugger-heap-profiler (Brinda Ashar) [#44693](https://github.com/nodejs/node/pull/44693)
* \[[`117f068250`](https://github.com/nodejs/node/commit/117f068250)] - **test**: use async/await in test-debugger-auto-resume (samyuktaprabhu) [#44675](https://github.com/nodejs/node/pull/44675)
* \[[`143c428cae`](https://github.com/nodejs/node/commit/143c428cae)] - **test**: migrated from Promise chains to Async/Await (Rathi N Das) [#44674](https://github.com/nodejs/node/pull/44674)
* \[[`e609a3309c`](https://github.com/nodejs/node/commit/e609a3309c)] - **test**: change promises to async/await in test-debugger-backtrace.js (Juliet Zhang) [#44677](https://github.com/nodejs/node/pull/44677)
* \[[`eeabd23ca6`](https://github.com/nodejs/node/commit/eeabd23ca6)] - **test**: use async/await in test-debugger-sb-before-load (Hope Olaidé) [#44697](https://github.com/nodejs/node/pull/44697)
* \[[`5c63d1464e`](https://github.com/nodejs/node/commit/5c63d1464e)] - **test**: add extra tests for basename with ext option (Connor Burton) [#44772](https://github.com/nodejs/node/pull/44772)
* \[[`f8b2d7a059`](https://github.com/nodejs/node/commit/f8b2d7a059)] - **test**: refactor to async/await (Divya Mohan) [#44694](https://github.com/nodejs/node/pull/44694)
* \[[`9864bde9ab`](https://github.com/nodejs/node/commit/9864bde9ab)] - **test**: modify test-debugger-custom-port.js to use async-await (Priya Shastri) [#44680](https://github.com/nodejs/node/pull/44680)
* \[[`af30823881`](https://github.com/nodejs/node/commit/af30823881)] - **test**: upgrade all 1024 bit RSA keys to 2048 bits (Momtchil Momtchev) [#44498](https://github.com/nodejs/node/pull/44498)
* \[[`0fb669e31f`](https://github.com/nodejs/node/commit/0fb669e31f)] - **test**: update test-debugger-breakpoint-exists.js to use async/await (Archana Kamath) [#44682](https://github.com/nodejs/node/pull/44682)
* \[[`cca253503e`](https://github.com/nodejs/node/commit/cca253503e)] - **test**: use async/await in test-debugger-preserve-breaks (poorvitusam) [#44696](https://github.com/nodejs/node/pull/44696)
* \[[`0b2e8b1681`](https://github.com/nodejs/node/commit/0b2e8b1681)] - **test**: use async/await in test-debugger-profile (surbhirjain) [#44684](https://github.com/nodejs/node/pull/44684)
* \[[`4db72a65cf`](https://github.com/nodejs/node/commit/4db72a65cf)] - **test**: change the promises to async/await in test-debugger-exec-scope.js (Ankita Khiratkar) [#44685](https://github.com/nodejs/node/pull/44685)
* \[[`56c9c98963`](https://github.com/nodejs/node/commit/56c9c98963)] - **test**: fix test-runner-inspect (Moshe Atlow) [#44620](https://github.com/nodejs/node/pull/44620)
* \[[`36227ed862`](https://github.com/nodejs/node/commit/36227ed862)] - **test**: fix watch mode test flake (Moshe Atlow) [#44739](https://github.com/nodejs/node/pull/44739)
* \[[`3abd71a0ea`](https://github.com/nodejs/node/commit/3abd71a0ea)] - **test**: deflake watch mode tests (Moshe Atlow) [#44621](https://github.com/nodejs/node/pull/44621)
* \[[`0c9f38f2be`](https://github.com/nodejs/node/commit/0c9f38f2be)] - **test**: split watch mode inspector tests to sequential (Moshe Atlow) [#44551](https://github.com/nodejs/node/pull/44551)
* \[[`d762a34128`](https://github.com/nodejs/node/commit/d762a34128)] - **test\_runner**: add --test-name-pattern CLI flag (Colin Ihrig)
* \[[`c7ece464a1`](https://github.com/nodejs/node/commit/c7ece464a1)] - **test\_runner**: remove runtime experimental warning (Colin Ihrig) [#44844](https://github.com/nodejs/node/pull/44844)
* \[[`3c1e9d41c8`](https://github.com/nodejs/node/commit/3c1e9d41c8)] - **test\_runner**: support using `--inspect` with `--test` (Moshe Atlow) [#44520](https://github.com/nodejs/node/pull/44520)
* \[[`4bdef48732`](https://github.com/nodejs/node/commit/4bdef48732)] - **tools**: remove faulty early termination logic from update-timezone.mjs (Darshan Sen) [#44870](https://github.com/nodejs/node/pull/44870)
* \[[`19d8574996`](https://github.com/nodejs/node/commit/19d8574996)] - **tools**: fix timezone update tool (Darshan Sen) [#44870](https://github.com/nodejs/node/pull/44870)
* \[[`ad8b8ae7d3`](https://github.com/nodejs/node/commit/ad8b8ae7d3)] - **tools**: update eslint to 8.25.0 (Node.js GitHub Bot) [#44931](https://github.com/nodejs/node/pull/44931)
* \[[`fd99b17a4d`](https://github.com/nodejs/node/commit/fd99b17a4d)] - **tools**: make `utils.SearchFiles` deterministic (Bruno Pitrus) [#44496](https://github.com/nodejs/node/pull/44496)
* \[[`131adece37`](https://github.com/nodejs/node/commit/131adece37)] - **tools**: fix typo in tools/update-authors.mjs (Darshan Sen) [#44780](https://github.com/nodejs/node/pull/44780)
* \[[`ab22777e65`](https://github.com/nodejs/node/commit/ab22777e65)] - **tools**: refactor deprecated format in no-unescaped-regexp-dot (Madhuri) [#44763](https://github.com/nodejs/node/pull/44763)
* \[[`3ad0fae89d`](https://github.com/nodejs/node/commit/3ad0fae89d)] - **tools**: update eslint-check.js to object style (andiemontoyeah) [#44706](https://github.com/nodejs/node/pull/44706)
* \[[`e9d572a9bd`](https://github.com/nodejs/node/commit/e9d572a9bd)] - **tools**: update eslint to 8.24.0 (Node.js GitHub Bot) [#44778](https://github.com/nodejs/node/pull/44778)
* \[[`984b0b4a6c`](https://github.com/nodejs/node/commit/984b0b4a6c)] - **tools**: update lint-md-dependencies to rollup\@2.79.1 (Node.js GitHub Bot) [#44776](https://github.com/nodejs/node/pull/44776)
* \[[`db5aeed702`](https://github.com/nodejs/node/commit/db5aeed702)] - **(SEMVER-MINOR)** **util**: add default value option to parsearg (Manuel Spigolon) [#44631](https://github.com/nodejs/node/pull/44631)
* \[[`576ccdf125`](https://github.com/nodejs/node/commit/576ccdf125)] - **util**: increase robustness with primordials (Jordan Harband) [#41212](https://github.com/nodejs/node/pull/41212)

<a id="18.10.0"></a>

## 2022-09-28, Version 18.10.0 (Current), @RafaelGSS

### Notable changes

* **doc**:
  * (SEMVER-MINOR) deprecate modp1, modp2, and modp5 groups (Tobias Nießen) [#44588](https://github.com/nodejs/node/pull/44588)
  * add legendecas to TSC list (Michael Dawson) [#44662](https://github.com/nodejs/node/pull/44662)
  * move `policy` docs to the `permissions` scope (Rafael Gonzaga) [#44222](https://github.com/nodejs/node/pull/44222)
* **gyp**:
  * libnode for ios app embedding (chexiongsheng) [#44210](https://github.com/nodejs/node/pull/44210)
* **http**:
  * (SEMVER-MINOR) throw error on content-length mismatch (sidwebworks) [#44588](https://github.com/nodejs/node/pull/44378)
* **stream**:
  * (SEMVER-MINOR) add `ReadableByteStream.tee()` (Daeyeon Jeong) [#44505](https://github.com/nodejs/node/pull/44505)

### Commits

* \[[`f497368679`](https://github.com/nodejs/node/commit/f497368679)] - **benchmark**: fix startup benchmark (Evan Lucas) [#44727](https://github.com/nodejs/node/pull/44727)
* \[[`0c9a94684e`](https://github.com/nodejs/node/commit/0c9a94684e)] - **benchmark**: add stream destroy benchmark (SindreXie) [#44533](https://github.com/nodejs/node/pull/44533)
* \[[`9c5c1459a8`](https://github.com/nodejs/node/commit/9c5c1459a8)] - **bootstrap**: clean up inspector console methods during serialization (Joyee Cheung) [#44279](https://github.com/nodejs/node/pull/44279)
* \[[`19f67dba8a`](https://github.com/nodejs/node/commit/19f67dba8a)] - **bootstrap**: remove unused global parameter in per-context scripts (Joyee Cheung) [#44472](https://github.com/nodejs/node/pull/44472)
* \[[`9da11426f6`](https://github.com/nodejs/node/commit/9da11426f6)] - **build**: remove redundant entry in crypto (Jiawen Geng) [#44604](https://github.com/nodejs/node/pull/44604)
* \[[`70898b4e67`](https://github.com/nodejs/node/commit/70898b4e67)] - **build**: rewritten the Android build system (BuShe Pie) [#44207](https://github.com/nodejs/node/pull/44207)
* \[[`a733f7faac`](https://github.com/nodejs/node/commit/a733f7faac)] - _**Revert**_ "**build**: go faster, drop -fno-omit-frame-pointer" (Ben Noordhuis) [#44566](https://github.com/nodejs/node/pull/44566)
* \[[`1315a83333`](https://github.com/nodejs/node/commit/1315a83333)] - **build**: fix bad upstream merge (Stephen Gallagher) [#44642](https://github.com/nodejs/node/pull/44642)
* \[[`993bd9b134`](https://github.com/nodejs/node/commit/993bd9b134)] - **crypto**: restrict PBKDF2 args to signed int (Tobias Nießen) [#44575](https://github.com/nodejs/node/pull/44575)
* \[[`ca5fb67b4e`](https://github.com/nodejs/node/commit/ca5fb67b4e)] - **deps**: update to ngtcp2 0.8.1 and nghttp3 0.7.0 (Tobias Nießen) [#44622](https://github.com/nodejs/node/pull/44622)
* \[[`8da1d6ebc4`](https://github.com/nodejs/node/commit/8da1d6ebc4)] - **deps**: update corepack to 0.14.1 (Node.js GitHub Bot) [#44704](https://github.com/nodejs/node/pull/44704)
* \[[`d36c4a3088`](https://github.com/nodejs/node/commit/d36c4a3088)] - **deps**: update ngtcp2 update instructions (Tobias Nießen) [#44619](https://github.com/nodejs/node/pull/44619)
* \[[`7129106aa0`](https://github.com/nodejs/node/commit/7129106aa0)] - **deps**: upgrade npm to 8.19.2 (npm team) [#44632](https://github.com/nodejs/node/pull/44632)
* \[[`3cc8f4bb56`](https://github.com/nodejs/node/commit/3cc8f4bb56)] - **deps**: update to uvwasi 0.0.13 (Colin Ihrig) [#44524](https://github.com/nodejs/node/pull/44524)
* \[[`4686579d4b`](https://github.com/nodejs/node/commit/4686579d4b)] - **dns**: remove unnecessary parameter from validateOneOf (Yagiz Nizipli) [#44635](https://github.com/nodejs/node/pull/44635)
* \[[`729dd95f1f`](https://github.com/nodejs/node/commit/729dd95f1f)] - **dns**: refactor default resolver (Joyee Cheung) [#44541](https://github.com/nodejs/node/pull/44541)
* \[[`6dc038262a`](https://github.com/nodejs/node/commit/6dc038262a)] - **doc**: mention git node backport (RafaelGSS) [#44764](https://github.com/nodejs/node/pull/44764)
* \[[`fd971f5176`](https://github.com/nodejs/node/commit/fd971f5176)] - **doc**: ensure to revert node\_version changes (Rafael Gonzaga) [#44760](https://github.com/nodejs/node/pull/44760)
* \[[`f274b08f8e`](https://github.com/nodejs/node/commit/f274b08f8e)] - **doc**: fix description for `napi_get_cb_info()` in `n-api.md` (Daeyeon Jeong) [#44761](https://github.com/nodejs/node/pull/44761)
* \[[`2502f2353d`](https://github.com/nodejs/node/commit/2502f2353d)] - **doc**: update the deprecation for exit code to clarify its scope (Daeyeon Jeong) [#44714](https://github.com/nodejs/node/pull/44714)
* \[[`064543d0ae`](https://github.com/nodejs/node/commit/064543d0ae)] - **doc**: update guidance for adding new modules (Michael Dawson) [#44576](https://github.com/nodejs/node/pull/44576)
* \[[`33a2f17534`](https://github.com/nodejs/node/commit/33a2f17534)] - **doc**: add registry number for Electron 22 (Keeley Hammond) [#44748](https://github.com/nodejs/node/pull/44748)
* \[[`10a0d75c26`](https://github.com/nodejs/node/commit/10a0d75c26)] - **doc**: include code examples for webstreams consumers (Lucas Santos) [#44387](https://github.com/nodejs/node/pull/44387)
* \[[`4dbe4a010c`](https://github.com/nodejs/node/commit/4dbe4a010c)] - **doc**: mention where to push security commits (RafaelGSS) [#44691](https://github.com/nodejs/node/pull/44691)
* \[[`82cb8151ad`](https://github.com/nodejs/node/commit/82cb8151ad)] - **doc**: remove extra space on threadpool usage (Connor Burton) [#44734](https://github.com/nodejs/node/pull/44734)
* \[[`6ef9af2748`](https://github.com/nodejs/node/commit/6ef9af2748)] - **doc**: make legacy banner slightly less bright (Rich Trott) [#44665](https://github.com/nodejs/node/pull/44665)
* \[[`b209c83e66`](https://github.com/nodejs/node/commit/b209c83e66)] - **doc**: improve building doc for Windows Powershell (Brian Muenzenmeyer) [#44625](https://github.com/nodejs/node/pull/44625)
* \[[`05b17e9250`](https://github.com/nodejs/node/commit/05b17e9250)] - **doc**: maintain only one list of MODP groups (Tobias Nießen) [#44644](https://github.com/nodejs/node/pull/44644)
* \[[`ec1cbdb69b`](https://github.com/nodejs/node/commit/ec1cbdb69b)] - **doc**: add legendecas to TSC list (Michael Dawson) [#44662](https://github.com/nodejs/node/pull/44662)
* \[[`9341fb4446`](https://github.com/nodejs/node/commit/9341fb4446)] - **doc**: remove comma in README.md (Taha-Chaudhry) [#44599](https://github.com/nodejs/node/pull/44599)
* \[[`3dabb44dda`](https://github.com/nodejs/node/commit/3dabb44dda)] - **doc**: use serial comma in report docs (Daeyeon Jeong) [#44608](https://github.com/nodejs/node/pull/44608)
* \[[`226d90a95a`](https://github.com/nodejs/node/commit/226d90a95a)] - **doc**: use serial comma in stream docs (Daeyeon Jeong) [#44609](https://github.com/nodejs/node/pull/44609)
* \[[`3f710fa636`](https://github.com/nodejs/node/commit/3f710fa636)] - **doc**: remove empty line in YAML block (Claudio Wunder) [#44617](https://github.com/nodejs/node/pull/44617)
* \[[`4ad1b0abc3`](https://github.com/nodejs/node/commit/4ad1b0abc3)] - **(SEMVER-MINOR)** **doc**: deprecate modp1, modp2, and modp5 groups (Tobias Nießen) [#44588](https://github.com/nodejs/node/pull/44588)
* \[[`2d92610525`](https://github.com/nodejs/node/commit/2d92610525)] - **doc**: remove old OpenSSL ENGINE constants (Tobias Nießen) [#44589](https://github.com/nodejs/node/pull/44589)
* \[[`03705639c4`](https://github.com/nodejs/node/commit/03705639c4)] - **doc**: fix heading levels for test runner hooks (Fabian Meyer) [#44603](https://github.com/nodejs/node/pull/44603)
* \[[`6c557346a7`](https://github.com/nodejs/node/commit/6c557346a7)] - **doc**: fix errors in http.md (Luigi Pinca) [#44587](https://github.com/nodejs/node/pull/44587)
* \[[`48d944b71c`](https://github.com/nodejs/node/commit/48d944b71c)] - **doc**: fix vm.Script createCachedData example (Chengzhong Wu) [#44487](https://github.com/nodejs/node/pull/44487)
* \[[`2813323120`](https://github.com/nodejs/node/commit/2813323120)] - **doc**: mention how to get commit release (Rafael Gonzaga) [#44572](https://github.com/nodejs/node/pull/44572)
* \[[`ea7b44d474`](https://github.com/nodejs/node/commit/ea7b44d474)] - **doc**: fix link in `process.md` (Antoine du Hamel) [#44594](https://github.com/nodejs/node/pull/44594)
* \[[`39b65d2fb7`](https://github.com/nodejs/node/commit/39b65d2fb7)] - **doc**: do not use weak MODP group in example (Tobias Nießen) [#44585](https://github.com/nodejs/node/pull/44585)
* \[[`f5549afd90`](https://github.com/nodejs/node/commit/f5549afd90)] - **doc**: remove ebpf from supported tooling list (Rafael Gonzaga) [#44549](https://github.com/nodejs/node/pull/44549)
* \[[`a3360b1f4f`](https://github.com/nodejs/node/commit/a3360b1f4f)] - **doc**: emphasize that createCipher is never secure (Tobias Nießen) [#44538](https://github.com/nodejs/node/pull/44538)
* \[[`4e6f7862ba`](https://github.com/nodejs/node/commit/4e6f7862ba)] - **doc**: document attribute Script.cachedDataRejected (Chengzhong Wu) [#44451](https://github.com/nodejs/node/pull/44451)
* \[[`01e584ecab`](https://github.com/nodejs/node/commit/01e584ecab)] - **doc**: move policy docs to the permissions scope (Rafael Gonzaga) [#44222](https://github.com/nodejs/node/pull/44222)
* \[[`57dac53c22`](https://github.com/nodejs/node/commit/57dac53c22)] - **doc,crypto**: cleanup removed pbkdf2 behaviours (Filip Skokan) [#44733](https://github.com/nodejs/node/pull/44733)
* \[[`c209bd6fb9`](https://github.com/nodejs/node/commit/c209bd6fb9)] - **doc,inspector**: document changes of inspector.close (Chengzhong Wu) [#44628](https://github.com/nodejs/node/pull/44628)
* \[[`9b3b7d6978`](https://github.com/nodejs/node/commit/9b3b7d6978)] - **esm,loader**: tidy ESMLoader internals (Jacob Smith) [#44701](https://github.com/nodejs/node/pull/44701)
* \[[`daf63d2fa3`](https://github.com/nodejs/node/commit/daf63d2fa3)] - **fs**: fix typo in mkdir example (SergeyTsukanov) [#44791](https://github.com/nodejs/node/pull/44791)
* \[[`85ab2f857f`](https://github.com/nodejs/node/commit/85ab2f857f)] - **fs**: remove unused option in `fs.fstatSync()` (Livia Medeiros) [#44613](https://github.com/nodejs/node/pull/44613)
* \[[`a6091f5496`](https://github.com/nodejs/node/commit/a6091f5496)] - **gyp**: libnode for ios app embedding (chexiongsheng) [#44210](https://github.com/nodejs/node/pull/44210)
* \[[`f158656e4c`](https://github.com/nodejs/node/commit/f158656e4c)] - **(SEMVER-MINOR)** **http**: throw error on content-length mismatch (sidwebworks) [#44378](https://github.com/nodejs/node/pull/44378)
* \[[`1b160517f5`](https://github.com/nodejs/node/commit/1b160517f5)] - **inspector**: expose inspector.close on workers (Chengzhong Wu) [#44489](https://github.com/nodejs/node/pull/44489)
* \[[`a2eb55a2c9`](https://github.com/nodejs/node/commit/a2eb55a2c9)] - **lib**: don't match `sourceMappingURL` in strings (Alan Agius) [#44658](https://github.com/nodejs/node/pull/44658)
* \[[`2baf532518`](https://github.com/nodejs/node/commit/2baf532518)] - **lib**: fix reference leak (falsandtru) [#44499](https://github.com/nodejs/node/pull/44499)
* \[[`d8d34ae6bc`](https://github.com/nodejs/node/commit/d8d34ae6bc)] - **lib**: reset `RegExp` statics before running user code (Antoine du Hamel) [#44247](https://github.com/nodejs/node/pull/44247)
* \[[`eb3635184b`](https://github.com/nodejs/node/commit/eb3635184b)] - **lib,test**: fix bug in InternalSocketAddress (Tobias Nießen) [#44618](https://github.com/nodejs/node/pull/44618)
* \[[`74dc4d198f`](https://github.com/nodejs/node/commit/74dc4d198f)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#44777](https://github.com/nodejs/node/pull/44777)
* \[[`97d2ed7296`](https://github.com/nodejs/node/commit/97d2ed7296)] - **meta**: add mailmap entry for dnlup (Rich Trott) [#44716](https://github.com/nodejs/node/pull/44716)
* \[[`35fbd2cc14`](https://github.com/nodejs/node/commit/35fbd2cc14)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#44705](https://github.com/nodejs/node/pull/44705)
* \[[`c5c1bc40a2`](https://github.com/nodejs/node/commit/c5c1bc40a2)] - **meta**: move dnlup to emeriti (dnlup) [#44667](https://github.com/nodejs/node/pull/44667)
* \[[`c62dfe0427`](https://github.com/nodejs/node/commit/c62dfe0427)] - **meta**: update test\_runner in label-pr-config (Shrujal Shah) [#44615](https://github.com/nodejs/node/pull/44615)
* \[[`fe56efd0bc`](https://github.com/nodejs/node/commit/fe56efd0bc)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#44591](https://github.com/nodejs/node/pull/44591)
* \[[`4436ffb536`](https://github.com/nodejs/node/commit/4436ffb536)] - **module**: open stat/readPackage to mutations (Maël Nison) [#44537](https://github.com/nodejs/node/pull/44537)
* \[[`f8ec946c82`](https://github.com/nodejs/node/commit/f8ec946c82)] - **module**: exports & imports map invalid slash deprecation (Guy Bedford) [#44477](https://github.com/nodejs/node/pull/44477)
* \[[`64cb43a2b6`](https://github.com/nodejs/node/commit/64cb43a2b6)] - **node-api**: add deprecation code of uncaught exception (Chengzhong Wu) [#44624](https://github.com/nodejs/node/pull/44624)
* \[[`ce1704c2c7`](https://github.com/nodejs/node/commit/ce1704c2c7)] - **src**: avoid using v8 on Isolate termination (Santiago Gimeno) [#44669](https://github.com/nodejs/node/pull/44669)
* \[[`3036b85d71`](https://github.com/nodejs/node/commit/3036b85d71)] - **src**: remove \<unistd.h> from node\_os.cc (Tobias Nießen) [#44668](https://github.com/nodejs/node/pull/44668)
* \[[`29f57b7899`](https://github.com/nodejs/node/commit/29f57b7899)] - **src**: avoid copy when creating Blob (Tobias Nießen) [#44616](https://github.com/nodejs/node/pull/44616)
* \[[`75cfb13ea6`](https://github.com/nodejs/node/commit/75cfb13ea6)] - **src**: make ReqWrap weak (Rafael Gonzaga) [#44074](https://github.com/nodejs/node/pull/44074)
* \[[`c12abb5ece`](https://github.com/nodejs/node/commit/c12abb5ece)] - **src**: make NearHeapLimitCallback() more robust (Joyee Cheung) [#44581](https://github.com/nodejs/node/pull/44581)
* \[[`81ea507e8e`](https://github.com/nodejs/node/commit/81ea507e8e)] - **src**: dump isolate stats when process exits (daomingq) [#44534](https://github.com/nodejs/node/pull/44534)
* \[[`687844822f`](https://github.com/nodejs/node/commit/687844822f)] - **src**: consolidate environment cleanup queue (Chengzhong Wu) [#44379](https://github.com/nodejs/node/pull/44379)
* \[[`3d42aaaac0`](https://github.com/nodejs/node/commit/3d42aaaac0)] - **stream**: handle a pending pull request from a released reader (Daeyeon Jeong) [#44702](https://github.com/nodejs/node/pull/44702)
* \[[`73ad9db6c5`](https://github.com/nodejs/node/commit/73ad9db6c5)] - **stream**: refactor use es2020 statement (SindreXie) [#44533](https://github.com/nodejs/node/pull/44533)
* \[[`0af6e420b3`](https://github.com/nodejs/node/commit/0af6e420b3)] - **stream**: remove `abortReason` from `WritableStreamDefaultController` (Daeyeon Jeong) [#44540](https://github.com/nodejs/node/pull/44540)
* \[[`2f2f8d5821`](https://github.com/nodejs/node/commit/2f2f8d5821)] - **(SEMVER-MINOR)** **stream**: add `ReadableByteStream.tee()` (Daeyeon Jeong) [#44505](https://github.com/nodejs/node/pull/44505)
* \[[`667e8bf3fb`](https://github.com/nodejs/node/commit/667e8bf3fb)] - **stream**: fix `writableStream.abort()` (Daeyeon Jeong) [#44327](https://github.com/nodejs/node/pull/44327)
* \[[`3112d5dae0`](https://github.com/nodejs/node/commit/3112d5dae0)] - **test**: verify napi\_remove\_wrap with napi\_delete\_reference (Chengzhong Wu) [#44754](https://github.com/nodejs/node/pull/44754)
* \[[`b512436841`](https://github.com/nodejs/node/commit/b512436841)] - **test**: change promises to async/await (Madhulika Sharma) [#44683](https://github.com/nodejs/node/pull/44683)
* \[[`858631f720`](https://github.com/nodejs/node/commit/858631f720)] - **test**: use async/await in test-debugger-invalid-args (Nupur Chauhan) [#44678](https://github.com/nodejs/node/pull/44678)
* \[[`6c9ded810c`](https://github.com/nodejs/node/commit/6c9ded810c)] - **test**: update test-debugger-low-level to use await/async (Meghana Ramesh) [#44688](https://github.com/nodejs/node/pull/44688)
* \[[`945aa74e57`](https://github.com/nodejs/node/commit/945aa74e57)] - **test**: check that sysconf returns a positive value (Tobias Nießen) [#44666](https://github.com/nodejs/node/pull/44666)
* \[[`79f0f48a6f`](https://github.com/nodejs/node/commit/79f0f48a6f)] - **test**: change promise to async/await in debugger-watcher (“Pooja) [#44687](https://github.com/nodejs/node/pull/44687)
* \[[`a56cb65bd6`](https://github.com/nodejs/node/commit/a56cb65bd6)] - **test**: fix addon tests compilation with OpenSSL 1.1.1 (Adam Majer) [#44725](https://github.com/nodejs/node/pull/44725)
* \[[`8a68a80a06`](https://github.com/nodejs/node/commit/8a68a80a06)] - **test**: fix test-performance-measure (smitley) [#44637](https://github.com/nodejs/node/pull/44637)
* \[[`55de0136b3`](https://github.com/nodejs/node/commit/55de0136b3)] - **test**: improve lib/readline.js coverage (MURAKAMI Masahiko) [#42686](https://github.com/nodejs/node/pull/42686)
* \[[`a3095d217f`](https://github.com/nodejs/node/commit/a3095d217f)] - **test**: fix `test-repl` not validating leaked globals properly (Antoine du Hamel) [#44640](https://github.com/nodejs/node/pull/44640)
* \[[`7db2974692`](https://github.com/nodejs/node/commit/7db2974692)] - **test**: ignore stale process cleanup failures on Windows (Joyee Cheung) [#44480](https://github.com/nodejs/node/pull/44480)
* \[[`6c35f338c3`](https://github.com/nodejs/node/commit/6c35f338c3)] - **test**: use python3 instead of python (Luigi Pinca) [#44545](https://github.com/nodejs/node/pull/44545)
* \[[`20e04c6d44`](https://github.com/nodejs/node/commit/20e04c6d44)] - **test**: fix DebugSymbolsTest.ReqWrapList on PPC64LE (Daniel Bevenius) [#44341](https://github.com/nodejs/node/pull/44341)
* \[[`eb25fe73b0`](https://github.com/nodejs/node/commit/eb25fe73b0)] - **test**: add more cases for parse-encoding (Tony Gorez) [#44427](https://github.com/nodejs/node/pull/44427)
* \[[`5ab3bc9419`](https://github.com/nodejs/node/commit/5ab3bc9419)] - **test\_runner**: include stack of uncaught exceptions (Moshe Atlow) [#44614](https://github.com/nodejs/node/pull/44614)
* \[[`752e1472e1`](https://github.com/nodejs/node/commit/752e1472e1)] - **tls**: fix out-of-bounds read in ClientHelloParser (Tobias Nießen) [#44580](https://github.com/nodejs/node/pull/44580)
* \[[`0cddb0af99`](https://github.com/nodejs/node/commit/0cddb0af99)] - **tools**: add update-llhttp.sh (Paolo Insogna) [#44652](https://github.com/nodejs/node/pull/44652)
* \[[`ef0dc47df9`](https://github.com/nodejs/node/commit/ef0dc47df9)] - **tools**: fix typo in update-nghttp2.sh (Luigi Pinca) [#44664](https://github.com/nodejs/node/pull/44664)
* \[[`0df181a5a1`](https://github.com/nodejs/node/commit/0df181a5a1)] - **tools**: add timezone update workflow (Lenvin Gonsalves) [#43988](https://github.com/nodejs/node/pull/43988)
* \[[`dd4348900d`](https://github.com/nodejs/node/commit/dd4348900d)] - **tools**: update eslint to 8.23.1 (Node.js GitHub Bot) [#44639](https://github.com/nodejs/node/pull/44639)
* \[[`b9cfb71e12`](https://github.com/nodejs/node/commit/b9cfb71e12)] - **tools**: update lint-md-dependencies to @rollup/plugin-node-resolve\@14.1.0 (Node.js GitHub Bot) [#44638](https://github.com/nodejs/node/pull/44638)
* \[[`5ae142d7ad`](https://github.com/nodejs/node/commit/5ae142d7ad)] - **tools**: update gyp-next to v0.13.0 (Jiawen Geng) [#44605](https://github.com/nodejs/node/pull/44605)
* \[[`5dd86c3faf`](https://github.com/nodejs/node/commit/5dd86c3faf)] - **tools**: update lint-md-dependencies to @rollup/plugin-node-resolve\@14.0.1 (Node.js GitHub Bot) [#44590](https://github.com/nodejs/node/pull/44590)
* \[[`caad4748cf`](https://github.com/nodejs/node/commit/caad4748cf)] - **tools**: increase timeout of running WPT (Joyee Cheung) [#44574](https://github.com/nodejs/node/pull/44574)
* \[[`5db9779f14`](https://github.com/nodejs/node/commit/5db9779f14)] - **tools**: fix shebang to use python3 by default (Himself65) [#44531](https://github.com/nodejs/node/pull/44531)
* \[[`9aa6a560e9`](https://github.com/nodejs/node/commit/9aa6a560e9)] - **v8**: add setHeapSnapshotNearHeapLimit (theanarkh) [#44420](https://github.com/nodejs/node/pull/44420)
* \[[`360b74e94f`](https://github.com/nodejs/node/commit/360b74e94f)] - **win**: fix fs.realpath.native for long paths (StefanStojanovic) [#44536](https://github.com/nodejs/node/pull/44536)

<a id="18.9.1"></a>

## 2022-09-23, Version 18.9.1 (Current), @RafaelGSS

This is a security release.

### Notable changes

The following CVEs are fixed in this release:

* **[CVE-2022-32212](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-32212)**: DNS rebinding in --inspect on macOS (High)
  * Insufficient fix for macOS devices on v18.5.0
* **[CVE-2022-32222](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-32222)**: Node 18 reads openssl.cnf from /home/iojs/build/ upon startup on MacOS (Medium)
* **[CVE-2022-32213](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-32213)**: HTTP Request Smuggling - Flawed Parsing of Transfer-Encoding (Medium)
  * Insufficient fix on v18.5.0
* **[CVE-2022-32215](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-32215)**: HTTP Request Smuggling - Incorrect Parsing of Multi-line Transfer-Encoding (Medium)
  * Insufficient fix on v18.5.0
* **[CVE-2022-35256](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-35256)**: HTTP Request Smuggling - Incorrect Parsing of Header Fields (Medium)
* **[CVE-2022-35255](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-35255)**: Weak randomness in WebCrypto keygen

More detailed information on each of the vulnerabilities can be found in [September 22nd 2022 Security Releases](https://nodejs.org/en/blog/vulnerability/september-2022-security-releases/) blog post.

#### llhttp updated to 6.0.10

`llhttp` is updated to 6.0.10 which includes fixes for the following vulnerabilities.

* **HTTP Request Smuggling - CVE-2022-32213 bypass via obs-fold mechanic (Medium)([CVE-2022-32213](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-32213) )**: The `llhttp` parser in the `http` module does not correctly parse and validate Transfer-Encoding headers. This can lead to HTTP Request Smuggling (HRS).
* **HTTP Request Smuggling - Incorrect Parsing of Multi-line Transfer-Encoding (Medium)([CVE-2022-32215](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-32215))**: The `llhttp` parser in the `http` module does not correctly handle multi-line Transfer-Encoding headers. This can lead to HTTP Request Smuggling (HRS).
* **HTTP Request Smuggling - Incorrect Parsing of Header Fields (Medium)([CVE-35256](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-35256))**: The llhttp parser in the `http` does not correctly handle header fields that are not terminated with CLRF. This can lead to HTTP Request Smuggling (HRS).

### Commits

* \[[`0c2a5723be`](https://github.com/nodejs/node/commit/0c2a5723be)] - **crypto**: fix weak randomness in WebCrypto keygen (Ben Noordhuis) [nodejs-private/node-private#](https://github.com/nodejs-private/node-private/pull/346)
* \[[`ffb6f4d51d`](https://github.com/nodejs/node/commit/ffb6f4d51d)] - **deps**: MacOS - fix location of OpenSSL config file (Michael Dawson) [nodejs-private/node-private#345](https://github.com/nodejs-private/node-private/pull/345)
* \[[`01bffcdd93`](https://github.com/nodejs/node/commit/01bffcdd93)] - **http**: disable chunked encoding when OBS fold is used (Paolo Insogna) [nodejs-private/node-private#341](https://github.com/nodejs-private/node-private/pull/341)
* \[[`2c379d341d`](https://github.com/nodejs/node/commit/2c379d341d)] - **src**: fix IPv4 non routable validation (RafaelGSS) [nodejs-private/node-private#337](https://github.com/nodejs-private/node-private/pull/337)

<a id="18.9.0"></a>

## 2022-09-08, Version 18.9.0 (Current), @RafaelGSS

### Notable changes

* **doc**
  * add daeyeon to collaborators (Daeyeon Jeong) [#44355](https://github.com/nodejs/node/pull/44355)
* **lib**
  * (SEMVER-MINOR) add diagnostics channel for process and worker (theanarkh) [#44045](https://github.com/nodejs/node/pull/44045)
* **os**
  * (SEMVER-MINOR) add machine method (theanarkh) [#44416](https://github.com/nodejs/node/pull/44416)
* **report**
  * (SEMVER-MINOR) expose report public native apis (Chengzhong Wu) [#44255](https://github.com/nodejs/node/pull/44255)
* **src**
  * (SEMVER-MINOR) expose environment RequestInterrupt api (Chengzhong Wu) [#44362](https://github.com/nodejs/node/pull/44362)
* **vm**
  * include vm context in the embedded snapshot (Joyee Cheung) [#44252](https://github.com/nodejs/node/pull/44252)

### Commits

* \[[`e27e709d3c`](https://github.com/nodejs/node/commit/e27e709d3c)] - **build**: add --libdir flag to configure (Stephen Gallagher) [#44361](https://github.com/nodejs/node/pull/44361)
* \[[`30da2b4d89`](https://github.com/nodejs/node/commit/30da2b4d89)] - **build**: added NINJA env to customize ninja binary (Jeff Dickey) [#44293](https://github.com/nodejs/node/pull/44293)
* \[[`3c5354869e`](https://github.com/nodejs/node/commit/3c5354869e)] - **cluster**: fix cluster rr distribute error (theanarkh) [#44202](https://github.com/nodejs/node/pull/44202)
* \[[`5cefd02618`](https://github.com/nodejs/node/commit/5cefd02618)] - **crypto**: handle invalid prepareAsymmetricKey JWK inputs (Filip Skokan) [#44475](https://github.com/nodejs/node/pull/44475)
* \[[`c868e36385`](https://github.com/nodejs/node/commit/c868e36385)] - **crypto**: add digest name to INVALID\_DIGEST errors (Tobias Nießen) [#44468](https://github.com/nodejs/node/pull/44468)
* \[[`35cbe1ad85`](https://github.com/nodejs/node/commit/35cbe1ad85)] - **crypto**: use actual option name in error message (Tobias Nießen) [#44455](https://github.com/nodejs/node/pull/44455)
* \[[`c3dbe18e4c`](https://github.com/nodejs/node/commit/c3dbe18e4c)] - **crypto**: simplify control flow in HKDF (Tobias Nießen) [#44272](https://github.com/nodejs/node/pull/44272)
* \[[`28781a1f7e`](https://github.com/nodejs/node/commit/28781a1f7e)] - **crypto**: improve RSA-PSS digest error messages (Tobias Nießen) [#44307](https://github.com/nodejs/node/pull/44307)
* \[[`b1eafe14fd`](https://github.com/nodejs/node/commit/b1eafe14fd)] - **debugger**: decrease timeout used to wait for the port to be free (Joyee Cheung) [#44359](https://github.com/nodejs/node/pull/44359)
* \[[`8ef5c40a83`](https://github.com/nodejs/node/commit/8ef5c40a83)] - **deps**: update corepack to 0.14.0 (Node.js GitHub Bot) [#44509](https://github.com/nodejs/node/pull/44509)
* \[[`cf19a79dfc`](https://github.com/nodejs/node/commit/cf19a79dfc)] - **deps**: upgrade npm to 8.19.1 (npm team) [#44486](https://github.com/nodejs/node/pull/44486)
* \[[`c5630ad1a7`](https://github.com/nodejs/node/commit/c5630ad1a7)] - **deps**: V8: backport ff8d67c88449 (Michaël Zasso) [#44423](https://github.com/nodejs/node/pull/44423)
* \[[`255e7fbd08`](https://github.com/nodejs/node/commit/255e7fbd08)] - **deps**: update Acorn to v8.8.0 (Michaël Zasso) [#44437](https://github.com/nodejs/node/pull/44437)
* \[[`754d26a53e`](https://github.com/nodejs/node/commit/754d26a53e)] - **deps**: patch V8 to 10.2.154.15 (Michaël Zasso) [#44294](https://github.com/nodejs/node/pull/44294)
* \[[`1b50ff2600`](https://github.com/nodejs/node/commit/1b50ff2600)] - **deps**: update icu tzdata to 2022b (Matías Zúñiga) [#44283](https://github.com/nodejs/node/pull/44283)
* \[[`1e451dca99`](https://github.com/nodejs/node/commit/1e451dca99)] - **deps**: upgrade llhttp to 6.0.9 (Paolo Insogna) [#44344](https://github.com/nodejs/node/pull/44344)
* \[[`57da3db522`](https://github.com/nodejs/node/commit/57da3db522)] - **deps**: update undici to 5.9.1 (Node.js GitHub Bot) [#44319](https://github.com/nodejs/node/pull/44319)
* \[[`1c87a7e8f6`](https://github.com/nodejs/node/commit/1c87a7e8f6)] - **doc**: add missing parenthesis in TLSSocket section (Tobias Nießen) [#44512](https://github.com/nodejs/node/pull/44512)
* \[[`05006eddb2`](https://github.com/nodejs/node/commit/05006eddb2)] - **doc**: do not use "Returns:" for crypto.constants (Tobias Nießen) [#44481](https://github.com/nodejs/node/pull/44481)
* \[[`54b6ed58bc`](https://github.com/nodejs/node/commit/54b6ed58bc)] - **doc**: use serial comma in addons docs (Tobias Nießen) [#44482](https://github.com/nodejs/node/pull/44482)
* \[[`11452a97b3`](https://github.com/nodejs/node/commit/11452a97b3)] - **doc**: add --update-assert-snapshot to node.1 (Colin Ihrig) [#44429](https://github.com/nodejs/node/pull/44429)
* \[[`ae028e8ac3`](https://github.com/nodejs/node/commit/ae028e8ac3)] - **doc**: improve assert.snapshot() docs (Colin Ihrig) [#44429](https://github.com/nodejs/node/pull/44429)
* \[[`71c869688a`](https://github.com/nodejs/node/commit/71c869688a)] - **doc**: add missing imports in events sample code (Brian Evans) [#44337](https://github.com/nodejs/node/pull/44337)
* \[[`92046e8027`](https://github.com/nodejs/node/commit/92046e8027)] - **doc**: apply scroll-margin-top to h2, h3 elements (metonym) [#44414](https://github.com/nodejs/node/pull/44414)
* \[[`3e6cde5931`](https://github.com/nodejs/node/commit/3e6cde5931)] - **doc**: fix spacing issue in `--build-snapshot` help text (Shohei YOSHIDA) [#44435](https://github.com/nodejs/node/pull/44435)
* \[[`8e41dbb81b`](https://github.com/nodejs/node/commit/8e41dbb81b)] - **doc**: mention cherry-pick edge-case on release (RafaelGSS) [#44408](https://github.com/nodejs/node/pull/44408)
* \[[`cef30f9afc`](https://github.com/nodejs/node/commit/cef30f9afc)] - **doc**: note on release guide to update `main` branch (Ruy Adorno) [#44384](https://github.com/nodejs/node/pull/44384)
* \[[`21437f7a7f`](https://github.com/nodejs/node/commit/21437f7a7f)] - **doc**: fix release guide example consistency (Ruy Adorno) [#44385](https://github.com/nodejs/node/pull/44385)
* \[[`ed52bd0a18`](https://github.com/nodejs/node/commit/ed52bd0a18)] - **doc**: fix style of n-api.md (theanarkh) [#44377](https://github.com/nodejs/node/pull/44377)
* \[[`65c1f4015f`](https://github.com/nodejs/node/commit/65c1f4015f)] - **doc**: add history for net.createServer() options (Luigi Pinca) [#44326](https://github.com/nodejs/node/pull/44326)
* \[[`4a0f750a6c`](https://github.com/nodejs/node/commit/4a0f750a6c)] - **doc**: add daeyeon to collaborators (Daeyeon Jeong) [#44355](https://github.com/nodejs/node/pull/44355)
* \[[`8cc5556f76`](https://github.com/nodejs/node/commit/8cc5556f76)] - **doc**: fix typo in test runner code examples (Moshe Atlow) [#44351](https://github.com/nodejs/node/pull/44351)
* \[[`b660b7467d`](https://github.com/nodejs/node/commit/b660b7467d)] - **doc,worker**: document resourceLimits overrides (Keyhan Vakil) [#43992](https://github.com/nodejs/node/pull/43992)
* \[[`2ed3b30696`](https://github.com/nodejs/node/commit/2ed3b30696)] - **inspector**: prevent integer overflow in open() (Tobias Nießen) [#44367](https://github.com/nodejs/node/pull/44367)
* \[[`b8f08e5e7e`](https://github.com/nodejs/node/commit/b8f08e5e7e)] - **lib**: codify findSourceMap return value when not found (Chengzhong Wu) [#44397](https://github.com/nodejs/node/pull/44397)
* \[[`a86ef1ba3e`](https://github.com/nodejs/node/commit/a86ef1ba3e)] - **lib**: use safe `Promise` alternatives when available (Antoine du Hamel) [#43476](https://github.com/nodejs/node/pull/43476)
* \[[`e519ac7842`](https://github.com/nodejs/node/commit/e519ac7842)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#44511](https://github.com/nodejs/node/pull/44511)
* \[[`c03f28b960`](https://github.com/nodejs/node/commit/c03f28b960)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#44422](https://github.com/nodejs/node/pull/44422)
* \[[`ef08cbddac`](https://github.com/nodejs/node/commit/ef08cbddac)] - **node-api**: avoid calling virtual methods in base's dtor (Chengzhong Wu) [#44424](https://github.com/nodejs/node/pull/44424)
* \[[`256340197c`](https://github.com/nodejs/node/commit/256340197c)] - **node-api**: cleanup redundant static modifiers (Chengzhong Wu) [#44301](https://github.com/nodejs/node/pull/44301)
* \[[`6714736706`](https://github.com/nodejs/node/commit/6714736706)] - **(SEMVER-MINOR)** **os**: add machine method (theanarkh) [#44416](https://github.com/nodejs/node/pull/44416)
* \[[`807b1e5533`](https://github.com/nodejs/node/commit/807b1e5533)] - **report**: get stack trace with cross origin contexts (Chengzhong Wu) [#44398](https://github.com/nodejs/node/pull/44398)
* \[[`b17cc877d0`](https://github.com/nodejs/node/commit/b17cc877d0)] - **report**: fix missing section javascriptHeap on OOMError (Chengzhong Wu) [#44398](https://github.com/nodejs/node/pull/44398)
* \[[`1f23c17ae0`](https://github.com/nodejs/node/commit/1f23c17ae0)] - **(SEMVER-MINOR)** **report**: expose report public native apis (Chengzhong Wu) [#44255](https://github.com/nodejs/node/pull/44255)
* \[[`df259005d9`](https://github.com/nodejs/node/commit/df259005d9)] - **report**: add queue info for udp (theanarkh) [#44345](https://github.com/nodejs/node/pull/44345)
* \[[`fc17b808c9`](https://github.com/nodejs/node/commit/fc17b808c9)] - **src**: rename misleading arg in ClientHelloParser (Tobias Nießen) [#44500](https://github.com/nodejs/node/pull/44500)
* \[[`125ab7da2a`](https://github.com/nodejs/node/commit/125ab7da2a)] - **src**: improve error handling in CloneSSLCerts (Tobias Nießen) [#44410](https://github.com/nodejs/node/pull/44410)
* \[[`aa34f7347b`](https://github.com/nodejs/node/commit/aa34f7347b)] - **src**: fix incorrect comments in crypto (Tobias Nießen) [#44470](https://github.com/nodejs/node/pull/44470)
* \[[`18b720805f`](https://github.com/nodejs/node/commit/18b720805f)] - **src**: avoid casting std::trunc(... / ...) to size\_t (Tobias Nießen) [#44467](https://github.com/nodejs/node/pull/44467)
* \[[`4331bbe2af`](https://github.com/nodejs/node/commit/4331bbe2af)] - **(SEMVER-MINOR)** **src**: expose environment RequestInterrupt api (Chengzhong Wu) [#44362](https://github.com/nodejs/node/pull/44362)
* \[[`c5413a1146`](https://github.com/nodejs/node/commit/c5413a1146)] - **src**: simplify enable\_if logic of `ToStringHelper::BaseConvert` (Feng Yu) [#44306](https://github.com/nodejs/node/pull/44306)
* \[[`dcc1cf4f4e`](https://github.com/nodejs/node/commit/dcc1cf4f4e)] - **src**: add error handling to `uv_uptime` call (Juan José Arboleda) [#44386](https://github.com/nodejs/node/pull/44386)
* \[[`fd611cc272`](https://github.com/nodejs/node/commit/fd611cc272)] - **src**: remove base64\_select\_table and base64\_table (Tobias Nießen) [#44425](https://github.com/nodejs/node/pull/44425)
* \[[`4776b4767b`](https://github.com/nodejs/node/commit/4776b4767b)] - **src**: fix uv\_err\_name memory leak (theanarkh) [#44421](https://github.com/nodejs/node/pull/44421)
* \[[`8db2e66d3a`](https://github.com/nodejs/node/commit/8db2e66d3a)] - **src**: make Endianness an enum class (Tobias Nießen) [#44411](https://github.com/nodejs/node/pull/44411)
* \[[`048e440878`](https://github.com/nodejs/node/commit/048e440878)] - **src**: fix ssize\_t error from nghttp2.h (Darshan Sen) [#44393](https://github.com/nodejs/node/pull/44393)
* \[[`dc1c95ede3`](https://github.com/nodejs/node/commit/dc1c95ede3)] - **src**: trace fs async api (theanarkh) [#44057](https://github.com/nodejs/node/pull/44057)
* \[[`0f4e98ba2c`](https://github.com/nodejs/node/commit/0f4e98ba2c)] - **src**: restore context default IsCodeGenerationFromStringsAllowed value (Chengzhong Wu) [#44324](https://github.com/nodejs/node/pull/44324)
* \[[`05fb650b54`](https://github.com/nodejs/node/commit/05fb650b54)] - **src**: simplify and optimize GetOpenSSLVersion() (Tobias Nießen) [#44395](https://github.com/nodejs/node/pull/44395)
* \[[`7f16177f96`](https://github.com/nodejs/node/commit/7f16177f96)] - **src**: use `if constexpr` where appropriate (Anna Henningsen) [#44291](https://github.com/nodejs/node/pull/44291)
* \[[`2be8acad18`](https://github.com/nodejs/node/commit/2be8acad18)] - **src**: simplify ECDH::GetCurves() (Tobias Nießen) [#44309](https://github.com/nodejs/node/pull/44309)
* \[[`3eb7918f8e`](https://github.com/nodejs/node/commit/3eb7918f8e)] - **src**: make minor improvements to EnabledDebugList (Tobias Nießen) [#44350](https://github.com/nodejs/node/pull/44350)
* \[[`88d9566593`](https://github.com/nodejs/node/commit/88d9566593)] - **src**: remove KeyObjectData::symmetric\_key\_len\_ (Tobias Nießen) [#44346](https://github.com/nodejs/node/pull/44346)
* \[[`768c9cb872`](https://github.com/nodejs/node/commit/768c9cb872)] - **src**: fix multiple format string bugs (Tobias Nießen) [#44314](https://github.com/nodejs/node/pull/44314)
* \[[`6857ee8299`](https://github.com/nodejs/node/commit/6857ee8299)] - **src**: make minor improvements to SecureBuffer (Tobias Nießen) [#44302](https://github.com/nodejs/node/pull/44302)
* \[[`2facf8b8e0`](https://github.com/nodejs/node/commit/2facf8b8e0)] - **stream**: fix setting abort reason in `ReadableStream.pipeTo()` (Daeyeon Jeong) [#44418](https://github.com/nodejs/node/pull/44418)
* \[[`65134d696b`](https://github.com/nodejs/node/commit/65134d696b)] - **stream**: fix `ReadableStreamReader.releaseLock()` (Daeyeon Jeong) [#44292](https://github.com/nodejs/node/pull/44292)
* \[[`4c33e5d4ce`](https://github.com/nodejs/node/commit/4c33e5d4ce)] - **test**: avoid race in file write stream handle tests (Joyee Cheung) [#44380](https://github.com/nodejs/node/pull/44380)
* \[[`0d77342a39`](https://github.com/nodejs/node/commit/0d77342a39)] - **test**: style updates for assert.snapshot() (Colin Ihrig) [#44429](https://github.com/nodejs/node/pull/44429)
* \[[`e36ed44b26`](https://github.com/nodejs/node/commit/e36ed44b26)] - **test**: deflake child process exec timeout tests (Joyee Cheung) [#44390](https://github.com/nodejs/node/pull/44390)
* \[[`0af15c71fb`](https://github.com/nodejs/node/commit/0af15c71fb)] - **test**: make the vm timeout escape tests more lenient (Joyee Cheung) [#44433](https://github.com/nodejs/node/pull/44433)
* \[[`0f071b800e`](https://github.com/nodejs/node/commit/0f071b800e)] - **test**: split heap prof tests (Joyee Cheung) [#44388](https://github.com/nodejs/node/pull/44388)
* \[[`2dd88b8425`](https://github.com/nodejs/node/commit/2dd88b8425)] - **test**: fix multiple incorrect mustNotCall() uses (Tobias Nießen) [#44022](https://github.com/nodejs/node/pull/44022)
* \[[`4ae1f4990c`](https://github.com/nodejs/node/commit/4ae1f4990c)] - **test**: split report OOM tests (Joyee Cheung) [#44389](https://github.com/nodejs/node/pull/44389)
* \[[`3a5fdacdc2`](https://github.com/nodejs/node/commit/3a5fdacdc2)] - **test**: fix WPT runner result (Daeyeon Jeong) [#44238](https://github.com/nodejs/node/pull/44238)
* \[[`e001aafee3`](https://github.com/nodejs/node/commit/e001aafee3)] - **test**: raise sleep times in child process tests (Joyee Cheung) [#44375](https://github.com/nodejs/node/pull/44375)
* \[[`8e2dcafc24`](https://github.com/nodejs/node/commit/8e2dcafc24)] - **test**: remove duplicate test (Luigi Pinca) [#44313](https://github.com/nodejs/node/pull/44313)
* \[[`c65d7fb1fa`](https://github.com/nodejs/node/commit/c65d7fb1fa)] - **test**: add OpenSSL 3.x providers test (Richard Lau) [#44148](https://github.com/nodejs/node/pull/44148)
* \[[`11e9d6e173`](https://github.com/nodejs/node/commit/11e9d6e173)] - **test**: make tmpdir.js importable from esm (Geoffrey Booth) [#44322](https://github.com/nodejs/node/pull/44322)
* \[[`a35c2f9ef4`](https://github.com/nodejs/node/commit/a35c2f9ef4)] - **test\_runner**: fix `duration_ms` to be milliseconds (Moshe Atlow) [#44450](https://github.com/nodejs/node/pull/44450)
* \[[`8175c65b4d`](https://github.com/nodejs/node/commit/8175c65b4d)] - **test\_runner**: support programmatically running `--test` (Moshe Atlow) [#44241](https://github.com/nodejs/node/pull/44241)
* \[[`1cdccbc845`](https://github.com/nodejs/node/commit/1cdccbc845)] - **tls**: remove SecureContext setFreeListLength (Tobias Nießen) [#44300](https://github.com/nodejs/node/pull/44300)
* \[[`70399166f3`](https://github.com/nodejs/node/commit/70399166f3)] - **tls**: use OpenSSL constant for client random size (Tobias Nießen) [#44305](https://github.com/nodejs/node/pull/44305)
* \[[`6fe189b62a`](https://github.com/nodejs/node/commit/6fe189b62a)] - **tools**: update lint-md-dependencies to rollup\@2.79.0 (Node.js GitHub Bot) [#44510](https://github.com/nodejs/node/pull/44510)
* \[[`1e62bb14dd`](https://github.com/nodejs/node/commit/1e62bb14dd)] - **tools**: fix typo in `avoid-prototype-pollution` lint rule (Antoine du Hamel) [#44446](https://github.com/nodejs/node/pull/44446)
* \[[`78c6827688`](https://github.com/nodejs/node/commit/78c6827688)] - **tools**: don't use f-strings in test.py (Santiago Gimeno) [#44407](https://github.com/nodejs/node/pull/44407)
* \[[`443730c419`](https://github.com/nodejs/node/commit/443730c419)] - **tools**: update doc to unist-util-visit\@4.1.1 (Node.js GitHub Bot) [#44370](https://github.com/nodejs/node/pull/44370)
* \[[`96df99375e`](https://github.com/nodejs/node/commit/96df99375e)] - **tools**: update eslint to 8.23.0 (Node.js GitHub Bot) [#44419](https://github.com/nodejs/node/pull/44419)
* \[[`b6709544e9`](https://github.com/nodejs/node/commit/b6709544e9)] - **tools**: refactor `avoid-prototype-pollution` lint rule (Antoine du Hamel) [#43476](https://github.com/nodejs/node/pull/43476)
* \[[`8b0a4afcae`](https://github.com/nodejs/node/commit/8b0a4afcae)] - **tty**: fix TypeError when stream is closed (Antoine du Hamel) [#43803](https://github.com/nodejs/node/pull/43803)
* \[[`c4a45a93f3`](https://github.com/nodejs/node/commit/c4a45a93f3)] - **vm**: avoid unnecessary property getter interceptor calls (Joyee Cheung) [#44252](https://github.com/nodejs/node/pull/44252)
* \[[`736a04aa13`](https://github.com/nodejs/node/commit/736a04aa13)] - **vm**: include vm context in the embedded snapshot (Joyee Cheung) [#44252](https://github.com/nodejs/node/pull/44252)
* \[[`bce827e5d1`](https://github.com/nodejs/node/commit/bce827e5d1)] - **vm**: make ContextifyContext template context-independent (Joyee Cheung) [#44252](https://github.com/nodejs/node/pull/44252)

<a id="18.8.0"></a>

## 2022-08-24, Version 18.8.0 (Current), @ruyadorno

### Notable changes

#### bootstrap: implement run-time user-land snapshots via --build-snapshot and --snapshot-blob

This patch introduces `--build-snapshot` and `--snapshot-blob` options for creating and using user land snapshots.

To generate a snapshot using snapshot.js as an entry point and write the snapshot blob to snapshot.blob:

```bash
echo "globalThis.foo = 'I am from the snapshot'" > snapshot.js
node --snapshot-blob snapshot.blob --build-snapshot snapshot.js
```

To restore application state from snapshot.blob, with index.js as the entry point script for the deserialized application:

```bash
echo "console.log(globalThis.foo)" > index.js
node --snapshot-blob snapshot.blob index.js
# => I am from the snapshot
```

Users can also use the `v8.startupSnapshot` API to specify an entry point at snapshot building time, thus avoiding the need of an additional entry script at deserialization time:

```bash
echo "require('v8').startupSnapshot.setDeserializeMainFunction(() => console.log('I am from the snapshot'))" > snapshot.js
node --snapshot-blob snapshot.blob --build-snapshot snapshot.js
node --snapshot-blob snapshot.blob
# => I am from the snapshot
```

Contributed by Joyee Cheung in [#38905](https://github.com/nodejs/node/pull/38905)

### Other notable changes

* **crypto**:
  * (SEMVER-MINOR) allow zero-length IKM in HKDF and in webcrypto PBKDF2 (Filip Skokan) [#44201](https://github.com/nodejs/node/pull/44201)
  * (SEMVER-MINOR) allow zero-length secret KeyObject (Filip Skokan) [#44201](https://github.com/nodejs/node/pull/44201)
* **deps**:
  * upgrade npm to 8.18.0 (npm team) [#44263](https://github.com/nodejs/node/pull/44263) - Adds a new `npm query` command
* **doc**:
  * add Erick Wendel to collaborators (Erick Wendel) [#44088](https://github.com/nodejs/node/pull/44088)
  * add theanarkh to collaborators (theanarkh) [#44131](https://github.com/nodejs/node/pull/44131)
  * add MoLow to collaborators (Moshe Atlow) [#44214](https://github.com/nodejs/node/pull/44214)
  * add cola119 to collaborators (cola119) [#44248](https://github.com/nodejs/node/pull/44248)
  * deprecate `--trace-atomics-wait` (Keyhan Vakil) [#44093](https://github.com/nodejs/node/pull/44093)
* **http**:
  * (SEMVER-MINOR) make idle http parser count configurable (theanarkh) [#43974](https://github.com/nodejs/node/pull/43974)
* **net**:
  * (SEMVER-MINOR) add local family (theanarkh) [#43975](https://github.com/nodejs/node/pull/43975)
* **src**:
  * (SEMVER-MINOR) print source map error source on demand (Chengzhong Wu) [#43875](https://github.com/nodejs/node/pull/43875)
* **tls**:
  * (SEMVER-MINOR) pass a valid socket on `tlsClientError` (Daeyeon Jeong) [#44021](https://github.com/nodejs/node/pull/44021)

### Commits

* \[[`0e20072e32`](https://github.com/nodejs/node/commit/0e20072e32)] - **assert**: add `getCalls` and `reset` to callTracker (Moshe Atlow) [#44191](https://github.com/nodejs/node/pull/44191)
* \[[`126fbbab74`](https://github.com/nodejs/node/commit/126fbbab74)] - **assert**: add assert.Snapshot (Moshe Atlow) [#44095](https://github.com/nodejs/node/pull/44095)
* \[[`87d7845b4f`](https://github.com/nodejs/node/commit/87d7845b4f)] - **bootstrap**: fixup Error.stackTraceLimit for user-land snapshot (Joyee Cheung) [#44203](https://github.com/nodejs/node/pull/44203)
* \[[`597a5171ee`](https://github.com/nodejs/node/commit/597a5171ee)] - **(SEMVER-MINOR)** **bootstrap**: clean up warning setup during serialization (Joyee Cheung) [#38905](https://github.com/nodejs/node/pull/38905)
* \[[`3561514ff5`](https://github.com/nodejs/node/commit/3561514ff5)] - **(SEMVER-MINOR)** **bootstrap**: implement --snapshot-blob and --build-snapshot (Joyee Cheung) [#38905](https://github.com/nodejs/node/pull/38905)
* \[[`123b2d6795`](https://github.com/nodejs/node/commit/123b2d6795)] - **bootstrap**: turn on FunctionCodeHandling::kKeep in the snapshot builder (Joyee Cheung) [#44104](https://github.com/nodejs/node/pull/44104)
* \[[`e7d101fbd4`](https://github.com/nodejs/node/commit/e7d101fbd4)] - **bootstrap**: support more builtins in the embedded code cache (Joyee Cheung) [#44018](https://github.com/nodejs/node/pull/44018)
* \[[`2ae2828040`](https://github.com/nodejs/node/commit/2ae2828040)] - **build**: enable pointer authentication for branch protection on arm64 (Jeremiah Gowdy) [#43200](https://github.com/nodejs/node/pull/43200)
* \[[`fecec4d3ba`](https://github.com/nodejs/node/commit/fecec4d3ba)] - **build**: add workflow to label flaky-test platform (Rafael Gonzaga) [#44042](https://github.com/nodejs/node/pull/44042)
* \[[`c975c4f674`](https://github.com/nodejs/node/commit/c975c4f674)] - **build**: optimized and fixed building configuration to Android (BuShe) [#44016](https://github.com/nodejs/node/pull/44016)
* \[[`ec1b31e6ad`](https://github.com/nodejs/node/commit/ec1b31e6ad)] - **build**: allow test-internet on forks if not scheduled (Rich Trott) [#44073](https://github.com/nodejs/node/pull/44073)
* \[[`ea48c5673b`](https://github.com/nodejs/node/commit/ea48c5673b)] - **build**: skip test-internet run on forks (Rich Trott) [#44054](https://github.com/nodejs/node/pull/44054)
* \[[`1c0d66e927`](https://github.com/nodejs/node/commit/1c0d66e927)] - **(SEMVER-MINOR)** **crypto**: allow zero-length IKM in HKDF and in webcrypto PBKDF2 (Filip Skokan) [#44201](https://github.com/nodejs/node/pull/44201)
* \[[`07d90c8a71`](https://github.com/nodejs/node/commit/07d90c8a71)] - **(SEMVER-MINOR)** **crypto**: allow zero-length secret KeyObject (Filip Skokan) [#44201](https://github.com/nodejs/node/pull/44201)
* \[[`ac2b10e0c7`](https://github.com/nodejs/node/commit/ac2b10e0c7)] - **crypto**: fix webcrypto deriveBits validations (Filip Skokan) [#44173](https://github.com/nodejs/node/pull/44173)
* \[[`4c902be5a5`](https://github.com/nodejs/node/commit/4c902be5a5)] - **crypto**: fix webcrypto EC key namedCurve validation errors (Filip Skokan) [#44172](https://github.com/nodejs/node/pull/44172)
* \[[`81e1ec4f6f`](https://github.com/nodejs/node/commit/81e1ec4f6f)] - **crypto**: fix webcrypto generateKey() AES key length validation error (Filip Skokan) [#44170](https://github.com/nodejs/node/pull/44170)
* \[[`ad8ef3a56c`](https://github.com/nodejs/node/commit/ad8ef3a56c)] - **crypto**: fix webcrypto operation errors to be OperationError (Filip Skokan) [#44171](https://github.com/nodejs/node/pull/44171)
* \[[`c270b9a0aa`](https://github.com/nodejs/node/commit/c270b9a0aa)] - **deps**: update corepack to 0.13.0 (Node.js GitHub Bot) [#44318](https://github.com/nodejs/node/pull/44318)
* \[[`bce8041d67`](https://github.com/nodejs/node/commit/bce8041d67)] - **deps**: upgrade npm to 8.18.0 (npm team) [#44263](https://github.com/nodejs/node/pull/44263)
* \[[`a26997263b`](https://github.com/nodejs/node/commit/a26997263b)] - **deps**: update corepack to 0.12.3 (Node.js GitHub Bot) [#44229](https://github.com/nodejs/node/pull/44229)
* \[[`b1590bbca2`](https://github.com/nodejs/node/commit/b1590bbca2)] - **deps**: upgrade npm to 8.17.0 (npm team) [#44205](https://github.com/nodejs/node/pull/44205)
* \[[`818271c1c3`](https://github.com/nodejs/node/commit/818271c1c3)] - **deps**: update undici to 5.8.2 (Node.js GitHub Bot) [#44187](https://github.com/nodejs/node/pull/44187)
* \[[`d09bc5402d`](https://github.com/nodejs/node/commit/d09bc5402d)] - **deps**: update undici to 5.8.1 (Node.js GitHub Bot) [#44158](https://github.com/nodejs/node/pull/44158)
* \[[`a92d90b482`](https://github.com/nodejs/node/commit/a92d90b482)] - **deps**: update corepack to 0.12.2 (Node.js GitHub Bot) [#44159](https://github.com/nodejs/node/pull/44159)
* \[[`52a516a281`](https://github.com/nodejs/node/commit/52a516a281)] - **deps**: V8: cherry-pick 9861ce1deae2 (Milad Fa) [#44115](https://github.com/nodejs/node/pull/44115)
* \[[`763b956f07`](https://github.com/nodejs/node/commit/763b956f07)] - **deps**: remove unnecessary file (Brian White) [#44133](https://github.com/nodejs/node/pull/44133)
* \[[`194587e767`](https://github.com/nodejs/node/commit/194587e767)] - **deps**: upgrade npm to 8.16.0 (npm team) [#44119](https://github.com/nodejs/node/pull/44119)
* \[[`116dcccc79`](https://github.com/nodejs/node/commit/116dcccc79)] - **deps**: upgrade base64 to dc6a41ce36e (Brian White) [#44032](https://github.com/nodejs/node/pull/44032)
* \[[`b7aaf3d4ca`](https://github.com/nodejs/node/commit/b7aaf3d4ca)] - **deps**: upgrade npm to 8.15.1 (npm team) [#44013](https://github.com/nodejs/node/pull/44013)
* \[[`a0c57837c4`](https://github.com/nodejs/node/commit/a0c57837c4)] - **deps**: cherry-pick 00704f5a from V8 upstream (Keyhan Vakil) [#43921](https://github.com/nodejs/node/pull/43921)
* \[[`19557ad6a4`](https://github.com/nodejs/node/commit/19557ad6a4)] - **dgram**: add dgram send queue info (theanarkh) [#44149](https://github.com/nodejs/node/pull/44149)
* \[[`a93371205b`](https://github.com/nodejs/node/commit/a93371205b)] - **doc**: fix optionality of callback arg of checkPrime (Tobias Nießen) [#44311](https://github.com/nodejs/node/pull/44311)
* \[[`d3f3bf602d`](https://github.com/nodejs/node/commit/d3f3bf602d)] - **doc**: fix typo (Hana) [#44262](https://github.com/nodejs/node/pull/44262)
* \[[`7a567875b0`](https://github.com/nodejs/node/commit/7a567875b0)] - **doc**: add TypeScript execution requirements (Michael Dawson) [#44030](https://github.com/nodejs/node/pull/44030)
* \[[`e8916fa758`](https://github.com/nodejs/node/commit/e8916fa758)] - **doc**: add cola119 to collaborators (cola119) [#44248](https://github.com/nodejs/node/pull/44248)
* \[[`8c1fe86026`](https://github.com/nodejs/node/commit/8c1fe86026)] - **doc**: fix added version for readable.closed/destroyed (Matthew Peveler) [#44033](https://github.com/nodejs/node/pull/44033)
* \[[`f39a0514d3`](https://github.com/nodejs/node/commit/f39a0514d3)] - **doc**: improved building doc for Android (BuShe) [#44166](https://github.com/nodejs/node/pull/44166)
* \[[`4d26cb9bb2`](https://github.com/nodejs/node/commit/4d26cb9bb2)] - **doc**: add MoLow to collaborators (Moshe Atlow) [#44214](https://github.com/nodejs/node/pull/44214)
* \[[`6bff14b6f1`](https://github.com/nodejs/node/commit/6bff14b6f1)] - **doc**: update tags in adding-new-napi-api.md (Chengzhong Wu) [#44190](https://github.com/nodejs/node/pull/44190)
* \[[`721639a1d4`](https://github.com/nodejs/node/commit/721639a1d4)] - **doc**: fix typo in diagnostics\_channel (Evan Lucas) [#44199](https://github.com/nodejs/node/pull/44199)
* \[[`0fffc24caa`](https://github.com/nodejs/node/commit/0fffc24caa)] - **doc**: add Retry CI in collaborator guide (Livia Medeiros) [#44130](https://github.com/nodejs/node/pull/44130)
* \[[`fb11643e31`](https://github.com/nodejs/node/commit/fb11643e31)] - **doc**: add performance note to `--enable-source-maps` docs (Saurabh Daware) [#43817](https://github.com/nodejs/node/pull/43817)
* \[[`cb7a9e78fd`](https://github.com/nodejs/node/commit/cb7a9e78fd)] - **doc**: remove unused code in call tracker example (Colin Ihrig) [#44127](https://github.com/nodejs/node/pull/44127)
* \[[`8c26daff7c`](https://github.com/nodejs/node/commit/8c26daff7c)] - **doc**: add theanarkh to collaborators (theanarkh) [#44131](https://github.com/nodejs/node/pull/44131)
* \[[`46f8fb8e53`](https://github.com/nodejs/node/commit/46f8fb8e53)] - **doc**: clarify tls.tlsSocket.getCipher().version (Adam Majer) [#44086](https://github.com/nodejs/node/pull/44086)
* \[[`02236032f0`](https://github.com/nodejs/node/commit/02236032f0)] - **doc**: update repository list in onboarding doc (Rich Trott) [#44089](https://github.com/nodejs/node/pull/44089)
* \[[`58f2739e32`](https://github.com/nodejs/node/commit/58f2739e32)] - **doc**: add Erick Wendel to collaborators (Erick Wendel) [#44088](https://github.com/nodejs/node/pull/44088)
* \[[`fe83d514b2`](https://github.com/nodejs/node/commit/fe83d514b2)] - **doc**: update collaborator email (Ruy Adorno) [#44044](https://github.com/nodejs/node/pull/44044)
* \[[`76011dd7f7`](https://github.com/nodejs/node/commit/76011dd7f7)] - **doc**: copyedit `test.md` (Antoine du Hamel) [#44061](https://github.com/nodejs/node/pull/44061)
* \[[`1d6029aa3d`](https://github.com/nodejs/node/commit/1d6029aa3d)] - **doc**: add kvakil to triagers (Keyhan Vakil) [#43996](https://github.com/nodejs/node/pull/43996)
* \[[`7f7a0eb2f5`](https://github.com/nodejs/node/commit/7f7a0eb2f5)] - **doc**: clarify part of onboarding guide regarding adding to teams (Darshan Sen) [#44024](https://github.com/nodejs/node/pull/44024)
* \[[`2ae5d853a7`](https://github.com/nodejs/node/commit/2ae5d853a7)] - **doc**: fix code examples in `crypto.md` (Antoine du Hamel) [#44053](https://github.com/nodejs/node/pull/44053)
* \[[`1b9537b6a5`](https://github.com/nodejs/node/commit/1b9537b6a5)] - **doc**: claim ABI version for Electron 21 (Keeley Hammond) [#44034](https://github.com/nodejs/node/pull/44034)
* \[[`d23dfa4dcb`](https://github.com/nodejs/node/commit/d23dfa4dcb)] - **doc**: remove old reference from crypto/README.md (Tobias Nießen) [#44012](https://github.com/nodejs/node/pull/44012)
* \[[`222ecd6e14`](https://github.com/nodejs/node/commit/222ecd6e14)] - **doc**: add missing env vars to man page (cola119) [#43492](https://github.com/nodejs/node/pull/43492)
* \[[`374b77619b`](https://github.com/nodejs/node/commit/374b77619b)] - **doc**: list supported MODP groups explicitly (Tobias Nießen) [#43986](https://github.com/nodejs/node/pull/43986)
* \[[`72a9ecf94f`](https://github.com/nodejs/node/commit/72a9ecf94f)] - **doc**: fix typo in packages.md (Dominic Saadi) [#44005](https://github.com/nodejs/node/pull/44005)
* \[[`1b328305f0`](https://github.com/nodejs/node/commit/1b328305f0)] - **doc**: fix typos in `test.md` (Antoine du Hamel) [#43997](https://github.com/nodejs/node/pull/43997)
* \[[`7af55dbc40`](https://github.com/nodejs/node/commit/7af55dbc40)] - **doc**: add missing test runner option (Moshe Atlow) [#43989](https://github.com/nodejs/node/pull/43989)
* \[[`e8441a2864`](https://github.com/nodejs/node/commit/e8441a2864)] - **doc,report**: document special filenames (Chengzhong Wu) [#44257](https://github.com/nodejs/node/pull/44257)
* \[[`da7bc5acdf`](https://github.com/nodejs/node/commit/da7bc5acdf)] - **doc,worker**: deprecate `--trace-atomics-wait` (Keyhan Vakil) [#44093](https://github.com/nodejs/node/pull/44093)
* \[[`37a9d7a754`](https://github.com/nodejs/node/commit/37a9d7a754)] - **errors**: refactor to use optional chaining (SindreXie) [#44184](https://github.com/nodejs/node/pull/44184)
* \[[`a6dccc969f`](https://github.com/nodejs/node/commit/a6dccc969f)] - **esm**: do not bind loader hook functions (Antoine du Hamel) [#44122](https://github.com/nodejs/node/pull/44122)
* \[[`5e9c197d85`](https://github.com/nodejs/node/commit/5e9c197d85)] - **esm**: fix loader hooks accepting too many arguments (Jacob Smith) [#44109](https://github.com/nodejs/node/pull/44109)
* \[[`e072c3aa70`](https://github.com/nodejs/node/commit/e072c3aa70)] - **esm**: move package config helpers (Geoffrey Booth) [#43967](https://github.com/nodejs/node/pull/43967)
* \[[`d57178cdfc`](https://github.com/nodejs/node/commit/d57178cdfc)] - **events**: use bitset to save memory (Basit Chonka) [#43700](https://github.com/nodejs/node/pull/43700)
* \[[`4ec3f671af`](https://github.com/nodejs/node/commit/4ec3f671af)] - **fs**: add encoding parameter to benchmarks (Yagiz Nizipli) [#44278](https://github.com/nodejs/node/pull/44278)
* \[[`851264ca90`](https://github.com/nodejs/node/commit/851264ca90)] - **http**: add max for http keepalive (theanarkh) [#44217](https://github.com/nodejs/node/pull/44217)
* \[[`340ca4d8fe`](https://github.com/nodejs/node/commit/340ca4d8fe)] - **http**: fix error message when specifying headerTimeout for createServer (Nick Sia) [#44163](https://github.com/nodejs/node/pull/44163)
* \[[`c340344641`](https://github.com/nodejs/node/commit/c340344641)] - **http**: trace http request / response (theanarkh) [#44102](https://github.com/nodejs/node/pull/44102)
* \[[`a2cd8b316c`](https://github.com/nodejs/node/commit/a2cd8b316c)] - **(SEMVER-MINOR)** **http**: make idle http parser count configurable (theanarkh) [#43974](https://github.com/nodejs/node/pull/43974)
* \[[`5dc39a10bd`](https://github.com/nodejs/node/commit/5dc39a10bd)] - **http**: reuse socket only when it is drained (ywave620) [#43902](https://github.com/nodejs/node/pull/43902)
* \[[`8c2d19b2d6`](https://github.com/nodejs/node/commit/8c2d19b2d6)] - **http**: do not leak error listeners (Paolo Insogna) [#43587](https://github.com/nodejs/node/pull/43587)
* \[[`1a44fbc19e`](https://github.com/nodejs/node/commit/1a44fbc19e)] - **lib**: add diagnostics channel and perf hooks detail (Danielle Adams) [#43984](https://github.com/nodejs/node/pull/43984)
* \[[`8cfc8b0e7b`](https://github.com/nodejs/node/commit/8cfc8b0e7b)] - **lib**: refactor to avoid prototype pollution (Antoine du Hamel) [#43474](https://github.com/nodejs/node/pull/43474)
* \[[`04007f2f51`](https://github.com/nodejs/node/commit/04007f2f51)] - **lib**: fix diagnostics channel (theanarkh) [#44154](https://github.com/nodejs/node/pull/44154)
* \[[`c02bbdd921`](https://github.com/nodejs/node/commit/c02bbdd921)] - **lib**: pass env variables to child process on z/OS (alexcfyung) [#42255](https://github.com/nodejs/node/pull/42255)
* \[[`617ea4af1c`](https://github.com/nodejs/node/commit/617ea4af1c)] - **lib**: add missing env vars to --help (cola119) [#43492](https://github.com/nodejs/node/pull/43492)
* \[[`94912bb09c`](https://github.com/nodejs/node/commit/94912bb09c)] - **lib**: add `Promise` methods to `avoid-prototype-pollution` lint rule (Antoine du Hamel) [#43849](https://github.com/nodejs/node/pull/43849)
* \[[`8977a87504`](https://github.com/nodejs/node/commit/8977a87504)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#44321](https://github.com/nodejs/node/pull/44321)
* \[[`f7be92fe86`](https://github.com/nodejs/node/commit/f7be92fe86)] - **meta**: update `web streams` in label-pr-config (Daeyeon Jeong) [#44235](https://github.com/nodejs/node/pull/44235)
* \[[`2c72ded880`](https://github.com/nodejs/node/commit/2c72ded880)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#44231](https://github.com/nodejs/node/pull/44231)
* \[[`c59dc7a4c1`](https://github.com/nodejs/node/commit/c59dc7a4c1)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#44161](https://github.com/nodejs/node/pull/44161)
* \[[`e0efd9af50`](https://github.com/nodejs/node/commit/e0efd9af50)] - **meta**: add codeowner for src/node\_snapshot\* (Chengzhong Wu) [#44113](https://github.com/nodejs/node/pull/44113)
* \[[`a996f53c78`](https://github.com/nodejs/node/commit/a996f53c78)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#44065](https://github.com/nodejs/node/pull/44065)
* \[[`697dbfb174`](https://github.com/nodejs/node/commit/697dbfb174)] - **meta**: shorten PowerShell snippet for bug-report template (NicoNekoru) [#44011](https://github.com/nodejs/node/pull/44011)
* \[[`05802c2877`](https://github.com/nodejs/node/commit/05802c2877)] - **module**: protect against prototype mutation (Antoine du Hamel) [#44007](https://github.com/nodejs/node/pull/44007)
* \[[`1b3fcf765f`](https://github.com/nodejs/node/commit/1b3fcf765f)] - **(SEMVER-MINOR)** **net**: create diagnostics channels lazily (Joyee Cheung) [#38905](https://github.com/nodejs/node/pull/38905)
* \[[`aa7c053926`](https://github.com/nodejs/node/commit/aa7c053926)] - **net**: remove unused callback (theanarkh) [#44204](https://github.com/nodejs/node/pull/44204)
* \[[`b6b632c09c`](https://github.com/nodejs/node/commit/b6b632c09c)] - **(SEMVER-MINOR)** **net**: add local family (theanarkh) [#43975](https://github.com/nodejs/node/pull/43975)
* \[[`c3d87564d4`](https://github.com/nodejs/node/commit/c3d87564d4)] - **net, dns**: socket should handle its output as input (Adam Majer) [#44083](https://github.com/nodejs/node/pull/44083)
* \[[`3ba75b341b`](https://github.com/nodejs/node/commit/3ba75b341b)] - **(SEMVER-MINOR)** **net,tls**: pass a valid socket on `tlsClientError` (Daeyeon Jeong) [#44021](https://github.com/nodejs/node/pull/44021)
* \[[`0e38fba552`](https://github.com/nodejs/node/commit/0e38fba552)] - **perf\_hooks**: add resourcetiming buffer limit (Chengzhong Wu) [#44220](https://github.com/nodejs/node/pull/44220)
* \[[`b9fd240f63`](https://github.com/nodejs/node/commit/b9fd240f63)] - **perf\_hooks**: fix gc elapsed time (theanarkh) [#44058](https://github.com/nodejs/node/pull/44058)
* \[[`8cf64998e2`](https://github.com/nodejs/node/commit/8cf64998e2)] - **report**: print javascript stack on fatal error (Chengzhong Wu) [#44242](https://github.com/nodejs/node/pull/44242)
* \[[`c842ab36b6`](https://github.com/nodejs/node/commit/c842ab36b6)] - **report**: skip report if uncaught exception is handled (Chengzhong Wu) [#44208](https://github.com/nodejs/node/pull/44208)
* \[[`ab73cc8706`](https://github.com/nodejs/node/commit/ab73cc8706)] - **src**: disambiguate terms used to refer to builtins and addons (Joyee Cheung) [#44135](https://github.com/nodejs/node/pull/44135)
* \[[`e9d19ac64c`](https://github.com/nodejs/node/commit/e9d19ac64c)] - **src**: use imported namespaces in `node_contextify.cc` (Juan José) [#44299](https://github.com/nodejs/node/pull/44299)
* \[[`3dadc95cd2`](https://github.com/nodejs/node/commit/3dadc95cd2)] - **src**: refactor to avoid using a moved object (Tobias Nießen) [#44269](https://github.com/nodejs/node/pull/44269)
* \[[`3765c6335b`](https://github.com/nodejs/node/commit/3765c6335b)] - **src**: extract common context embedder tag checks (Chengzhong Wu) [#44258](https://github.com/nodejs/node/pull/44258)
* \[[`d2dce59729`](https://github.com/nodejs/node/commit/d2dce59729)] - **src**: avoid copying BaseObjectPtrs in loop (Tobias Nießen) [#44270](https://github.com/nodejs/node/pull/44270)
* \[[`9614907104`](https://github.com/nodejs/node/commit/9614907104)] - **src**: remove usage on ScriptCompiler::CompileFunctionInContext (Chengzhong Wu) [#44198](https://github.com/nodejs/node/pull/44198)
* \[[`4e1ffd932e`](https://github.com/nodejs/node/commit/4e1ffd932e)] - **src**: fix --heapsnapshot-near-heap-limit error hint (Chengzhong Wu) [#44216](https://github.com/nodejs/node/pull/44216)
* \[[`960a20928f`](https://github.com/nodejs/node/commit/960a20928f)] - **src**: prevent copying ArrayBufferViewContents (Keyhan Vakil) [#44091](https://github.com/nodejs/node/pull/44091)
* \[[`4755ad5495`](https://github.com/nodejs/node/commit/4755ad5495)] - **src**: remove usages of GetBackingStore in crypto (Keyhan Vakil) [#44079](https://github.com/nodejs/node/pull/44079)
* \[[`a2022e5aff`](https://github.com/nodejs/node/commit/a2022e5aff)] - **src**: remove unowned usages of GetBackingStore (Keyhan Vakil) [#44080](https://github.com/nodejs/node/pull/44080)
* \[[`8e1b7e2b8f`](https://github.com/nodejs/node/commit/8e1b7e2b8f)] - **src**: remove usages of GetBackingStore in node-api (Keyhan Vakil) [#44075](https://github.com/nodejs/node/pull/44075)
* \[[`cddf3eda28`](https://github.com/nodejs/node/commit/cddf3eda28)] - **src**: remove usages of GetBackingStore in modules (Keyhan Vakil) [#44076](https://github.com/nodejs/node/pull/44076)
* \[[`a54e4d4170`](https://github.com/nodejs/node/commit/a54e4d4170)] - **src**: remove usages of GetBackingStore in WASI (Keyhan Vakil) [#44077](https://github.com/nodejs/node/pull/44077)
* \[[`38cdb1f9b6`](https://github.com/nodejs/node/commit/38cdb1f9b6)] - **src**: remove usages of GetBackingStore in startup (Keyhan Vakil) [#44078](https://github.com/nodejs/node/pull/44078)
* \[[`c4783e37d7`](https://github.com/nodejs/node/commit/c4783e37d7)] - **src**: nest namespace report in namespace node (Chengzhong Wu) [#44069](https://github.com/nodejs/node/pull/44069)
* \[[`04bcdf63a0`](https://github.com/nodejs/node/commit/04bcdf63a0)] - **src**: use a typed array internally for process.\_exiting (Darshan Sen) [#43883](https://github.com/nodejs/node/pull/43883)
* \[[`b90b8abdd5`](https://github.com/nodejs/node/commit/b90b8abdd5)] - **src**: fix bug in GetErrorSource() (Tobias Nießen) [#44019](https://github.com/nodejs/node/pull/44019)
* \[[`728e18e025`](https://github.com/nodejs/node/commit/728e18e025)] - **src**: fix to use replacement character (Kohei Ueno) [#43999](https://github.com/nodejs/node/pull/43999)
* \[[`cc6e0fc8ff`](https://github.com/nodejs/node/commit/cc6e0fc8ff)] - **src**: improve SPKAC::ExportChallenge() (Tobias Nießen) [#44002](https://github.com/nodejs/node/pull/44002)
* \[[`9763e2fba9`](https://github.com/nodejs/node/commit/9763e2fba9)] - **src**: fix typo in src/README.md (Anna Henningsen) [#44009](https://github.com/nodejs/node/pull/44009)
* \[[`460397709b`](https://github.com/nodejs/node/commit/460397709b)] - **src**: remove unnecessary cast in crypto\_sig.cc (Tobias Nießen) [#44001](https://github.com/nodejs/node/pull/44001)
* \[[`68ee8e9089`](https://github.com/nodejs/node/commit/68ee8e9089)] - **src**: split property helpers from node::Environment (Chengzhong Wu) [#44056](https://github.com/nodejs/node/pull/44056)
* \[[`9990dc7d18`](https://github.com/nodejs/node/commit/9990dc7d18)] - **src,buffer**: remove unused chars\_written parameter (Keyhan Vakil) [#44092](https://github.com/nodejs/node/pull/44092)
* \[[`ecf82186e0`](https://github.com/nodejs/node/commit/ecf82186e0)] - **src,fs**: refactor duplicated code in fs.readdir (Daeyeon Jeong) [#43204](https://github.com/nodejs/node/pull/43204)
* \[[`ee6412a992`](https://github.com/nodejs/node/commit/ee6412a992)] - **src,lib**: print prinstine source when source map source not found (Chengzhong Wu) [#44052](https://github.com/nodejs/node/pull/44052)
* \[[`4249276783`](https://github.com/nodejs/node/commit/4249276783)] - **(SEMVER-MINOR)** **src,lib**: print source map error source on demand (Chengzhong Wu) [#43875](https://github.com/nodejs/node/pull/43875)
* \[[`1dabdbf05c`](https://github.com/nodejs/node/commit/1dabdbf05c)] - **src,test**: fix typos (SADIK KUZU) [#44110](https://github.com/nodejs/node/pull/44110)
* \[[`a3ac445198`](https://github.com/nodejs/node/commit/a3ac445198)] - **stream**: fix `isDetachedBuffer` validations (Daeyeon Jeong) [#44114](https://github.com/nodejs/node/pull/44114)
* \[[`c079abe017`](https://github.com/nodejs/node/commit/c079abe017)] - **stream**: improve views validation on `BYOBRequest` (Daeyeon Jeong) [#44155](https://github.com/nodejs/node/pull/44155)
* \[[`2f904bc8bf`](https://github.com/nodejs/node/commit/2f904bc8bf)] - **stream**: update TextEncoderStream to align the latest spec (Kohei Ueno) [#44101](https://github.com/nodejs/node/pull/44101)
* \[[`40b817cfb9`](https://github.com/nodejs/node/commit/40b817cfb9)] - **(SEMVER-MINOR)** **test**: test snapshotting TypeScript compiler (Joyee Cheung) [#38905](https://github.com/nodejs/node/pull/38905)
* \[[`d4189ab609`](https://github.com/nodejs/node/commit/d4189ab609)] - **(SEMVER-MINOR)** **test**: add UMD module test with marked (Joyee Cheung) [#38905](https://github.com/nodejs/node/pull/38905)
* \[[`514e5162d2`](https://github.com/nodejs/node/commit/514e5162d2)] - **test**: deflake test-diagnostics-channel-net (Keyhan Vakil) [#44144](https://github.com/nodejs/node/pull/44144)
* \[[`a2707d0f48`](https://github.com/nodejs/node/commit/a2707d0f48)] - **test**: add coverage for invalid RSA-PSS digests (Tobias Nießen) [#44271](https://github.com/nodejs/node/pull/44271)
* \[[`7b6126a59a`](https://github.com/nodejs/node/commit/7b6126a59a)] - **test**: update Web Streams WPT (Daeyeon Jeong) [#44234](https://github.com/nodejs/node/pull/44234)
* \[[`a02492f96c`](https://github.com/nodejs/node/commit/a02492f96c)] - **test**: move "errors" test to "parallel" (Michaël Zasso) [#44233](https://github.com/nodejs/node/pull/44233)
* \[[`b4224dd192`](https://github.com/nodejs/node/commit/b4224dd192)] - **test**: reduce http-server-request-timeouts-mixed flakiness (Nick Sia) [#44169](https://github.com/nodejs/node/pull/44169)
* \[[`f5e2f6c362`](https://github.com/nodejs/node/commit/f5e2f6c362)] - **test**: remove cjs loader from stack traces (Geoffrey Booth) [#44197](https://github.com/nodejs/node/pull/44197)
* \[[`e37314497a`](https://github.com/nodejs/node/commit/e37314497a)] - **test**: add filesystem check to `test-fs-stat-date.mjs` (Livia Medeiros) [#44174](https://github.com/nodejs/node/pull/44174)
* \[[`9755b1f979`](https://github.com/nodejs/node/commit/9755b1f979)] - **test**: mark connection leak test flaky on IBM i (Richard Lau) [#44215](https://github.com/nodejs/node/pull/44215)
* \[[`beaf5f5776`](https://github.com/nodejs/node/commit/beaf5f5776)] - **test**: use `mustSucceed` instead of `mustCall` with `assert.ifError` (MURAKAMI Masahiko) [#44196](https://github.com/nodejs/node/pull/44196)
* \[[`11f74e72a7`](https://github.com/nodejs/node/commit/11f74e72a7)] - **test**: update WPT runner (Filip Skokan) [#43455](https://github.com/nodejs/node/pull/43455)
* \[[`b2a15b6275`](https://github.com/nodejs/node/commit/b2a15b6275)] - **test**: update wpt url status (Kohei Ueno) [#44175](https://github.com/nodejs/node/pull/44175)
* \[[`6b84451d70`](https://github.com/nodejs/node/commit/6b84451d70)] - **test**: update wasm/jsapi web platform tests (Yagiz Nizipli) [#44100](https://github.com/nodejs/node/pull/44100)
* \[[`537d52fa0f`](https://github.com/nodejs/node/commit/537d52fa0f)] - **test**: update hr-time web platform tests (Yagiz Nizipli) [#44100](https://github.com/nodejs/node/pull/44100)
* \[[`79445cb215`](https://github.com/nodejs/node/commit/79445cb215)] - **test**: update console web platform tests (Yagiz Nizipli) [#44100](https://github.com/nodejs/node/pull/44100)
* \[[`70267a0eeb`](https://github.com/nodejs/node/commit/70267a0eeb)] - **test**: move tests with many workers to sequential (Keyhan Vakil) [#44139](https://github.com/nodejs/node/pull/44139)
* \[[`86a7fb0c8a`](https://github.com/nodejs/node/commit/86a7fb0c8a)] - **test**: deflake gc-http-client tests by restricting number of requests (Nick Sia) [#44146](https://github.com/nodejs/node/pull/44146)
* \[[`e17117dfda`](https://github.com/nodejs/node/commit/e17117dfda)] - **test**: move test-vm-break-on-sigint to sequential (Keyhan Vakil) [#44140](https://github.com/nodejs/node/pull/44140)
* \[[`e5113fab05`](https://github.com/nodejs/node/commit/e5113fab05)] - **test**: remove test-http-client-response-timeout flaky designation (Luigi Pinca) [#44145](https://github.com/nodejs/node/pull/44145)
* \[[`f1b5f933d7`](https://github.com/nodejs/node/commit/f1b5f933d7)] - **test**: s390x z15 accelerated zlib fixes (Adam Majer) [#44117](https://github.com/nodejs/node/pull/44117)
* \[[`86bbd5e61a`](https://github.com/nodejs/node/commit/86bbd5e61a)] - **test**: tune down parallelism for some flaky tests (Keyhan Vakil) [#44090](https://github.com/nodejs/node/pull/44090)
* \[[`40e2ca7f66`](https://github.com/nodejs/node/commit/40e2ca7f66)] - **test**: fix `internet/test-inspector-help-page` (Daeyeon Jeong) [#44025](https://github.com/nodejs/node/pull/44025)
* \[[`b19564b9d2`](https://github.com/nodejs/node/commit/b19564b9d2)] - **test**: refactor ESM tests to improve performance (Jacob Smith) [#43784](https://github.com/nodejs/node/pull/43784)
* \[[`d964b308ae`](https://github.com/nodejs/node/commit/d964b308ae)] - **test**: remove test-gc-http-client-timeout from flaky list (Feng Yu) [#43971](https://github.com/nodejs/node/pull/43971)
* \[[`2cab7bb791`](https://github.com/nodejs/node/commit/2cab7bb791)] - **test**: reduce loop times for preventing test from timeout (theanarkh) [#43981](https://github.com/nodejs/node/pull/43981)
* \[[`9244d6d416`](https://github.com/nodejs/node/commit/9244d6d416)] - **test**: fix test-cluster-concurrent-disconnect (Daeyeon Jeong) [#43961](https://github.com/nodejs/node/pull/43961)
* \[[`3c8037a9fa`](https://github.com/nodejs/node/commit/3c8037a9fa)] - **test**: change misleading variable name (Tobias Nießen) [#43990](https://github.com/nodejs/node/pull/43990)
* \[[`82164344e2`](https://github.com/nodejs/node/commit/82164344e2)] - **test,crypto**: update WebCryptoAPI WPT (Filip Skokan) [#44223](https://github.com/nodejs/node/pull/44223)
* \[[`c0b160c842`](https://github.com/nodejs/node/commit/c0b160c842)] - **test\_runner**: fix test runner hooks failure stack (Moshe Atlow) [#44284](https://github.com/nodejs/node/pull/44284)
* \[[`8ed39397d5`](https://github.com/nodejs/node/commit/8ed39397d5)] - **test\_runner**: refactor to use more primordials (Antoine du Hamel) [#44062](https://github.com/nodejs/node/pull/44062)
* \[[`d8749c3b87`](https://github.com/nodejs/node/commit/d8749c3b87)] - **test\_runner**: verbous error when entire test tree is canceled (Moshe Atlow) [#44060](https://github.com/nodejs/node/pull/44060)
* \[[`0d007471fa`](https://github.com/nodejs/node/commit/0d007471fa)] - **test\_runner**: empty pending tests queue post running (Moshe Atlow) [#44059](https://github.com/nodejs/node/pull/44059)
* \[[`c3fa82f007`](https://github.com/nodejs/node/commit/c3fa82f007)] - **test\_runner**: add before/after/each hooks (Moshe Atlow) [#43730](https://github.com/nodejs/node/pull/43730)
* \[[`50c854bbfe`](https://github.com/nodejs/node/commit/50c854bbfe)] - **test\_runner**: fix top level `describe` queuing (Moshe Atlow) [#43998](https://github.com/nodejs/node/pull/43998)
* \[[`04fdc3e1fa`](https://github.com/nodejs/node/commit/04fdc3e1fa)] - **test\_runner**: graceful termination on `--test` only (Moshe Atlow) [#43977](https://github.com/nodejs/node/pull/43977)
* \[[`51a0310398`](https://github.com/nodejs/node/commit/51a0310398)] - **test\_runner**: validate `concurrency` option (Antoine du Hamel) [#43976](https://github.com/nodejs/node/pull/43976)
* \[[`ecf7b0720a`](https://github.com/nodejs/node/commit/ecf7b0720a)] - **tls**: use logical OR operator (Mohammed Keyvanzadeh) [#44236](https://github.com/nodejs/node/pull/44236)
* \[[`f7c1b838ba`](https://github.com/nodejs/node/commit/f7c1b838ba)] - **tools**: update lint-md-dependencies to rollup\@2.78.1 (Node.js GitHub Bot) [#44320](https://github.com/nodejs/node/pull/44320)
* \[[`36b39db74d`](https://github.com/nodejs/node/commit/36b39db74d)] - **tools**: update ESLint to 8.22.0 (Luigi Pinca) [#44243](https://github.com/nodejs/node/pull/44243)
* \[[`87f75a27fb`](https://github.com/nodejs/node/commit/87f75a27fb)] - **tools**: update lint-md-dependencies to rollup\@2.78.0 (Node.js GitHub Bot) [#44244](https://github.com/nodejs/node/pull/44244)
* \[[`a3cc8ce959`](https://github.com/nodejs/node/commit/a3cc8ce959)] - **tools**: update lint-md-dependencies to rollup\@2.77.3 (Node.js GitHub Bot) [#44230](https://github.com/nodejs/node/pull/44230)
* \[[`873941a43e`](https://github.com/nodejs/node/commit/873941a43e)] - **tools**: update eslint to 8.21.0 (Node.js GitHub Bot) [#44162](https://github.com/nodejs/node/pull/44162)
* \[[`6be7e6d136`](https://github.com/nodejs/node/commit/6be7e6d136)] - **tools**: update lint-md-dependencies to @rollup/plugin-commonjs\@22.0.2 (Node.js GitHub Bot) [#44160](https://github.com/nodejs/node/pull/44160)
* \[[`b252f389d7`](https://github.com/nodejs/node/commit/b252f389d7)] - **tools**: update undici CPE in vuln checking script (Facundo Tuesca) [#44128](https://github.com/nodejs/node/pull/44128)
* \[[`3eacf25789`](https://github.com/nodejs/node/commit/3eacf25789)] - **tools**: update lint-md-dependencies to rollup\@2.77.2 (Node.js GitHub Bot) [#44064](https://github.com/nodejs/node/pull/44064)
* \[[`1175d9036a`](https://github.com/nodejs/node/commit/1175d9036a)] - **tools**: add verbose flag to find-inactive-collaborators (Rich Trott) [#43964](https://github.com/nodejs/node/pull/43964)
* \[[`2cf3ce83d8`](https://github.com/nodejs/node/commit/2cf3ce83d8)] - **trace\_events**: add example (theanarkh) [#43253](https://github.com/nodejs/node/pull/43253)
* \[[`2efce0fe5b`](https://github.com/nodejs/node/commit/2efce0fe5b)] - **typings**: add JSDoc for `internal/validators` (Yagiz Nizipli) [#44181](https://github.com/nodejs/node/pull/44181)

<a id="18.7.0"></a>

## 2022-07-26, Version 18.7.0 (Current), @danielleadams

### Notable changes

* **doc**:
  * add F3n67u to collaborators (Feng Yu) [#43953](https://github.com/nodejs/node/pull/43953)
  * deprecate coercion to integer in process.exit (Daeyeon Jeong) [#43738](https://github.com/nodejs/node/pull/43738)
  * **(SEMVER-MINOR)** deprecate diagnostics\_channel object subscribe method (Stephen Belanger) [#42714](https://github.com/nodejs/node/pull/42714)
* **events**:
  * **(SEMVER-MINOR)** expose CustomEvent on global with CLI flag (Daeyeon Jeong) [#43885](https://github.com/nodejs/node/pull/43885)
  * **(SEMVER-MINOR)** add `CustomEvent` (Daeyeon Jeong) [#43514](https://github.com/nodejs/node/pull/43514)
* **http**:
  * **(SEMVER-MINOR)** add drop request event for http server (theanarkh) [#43806](https://github.com/nodejs/node/pull/43806)
* **lib**:
  * **(SEMVER-MINOR)** improved diagnostics\_channel subscribe/unsubscribe (Stephen Belanger) [#42714](https://github.com/nodejs/node/pull/42714)
* **util**:
  * **(SEMVER-MINOR)** add tokens to parseArgs (John Gee) [#43459](https://github.com/nodejs/node/pull/43459)

### Commits

* \[[`0aa255ab72`](https://github.com/nodejs/node/commit/0aa255ab72)] - **bootstrap**: handle snapshot errors gracefully (Joyee Cheung) [#43531](https://github.com/nodejs/node/pull/43531)
* \[[`0783ddf57e`](https://github.com/nodejs/node/commit/0783ddf57e)] - **buffer**: do not leak memory if buffer is too big (Keyhan Vakil) [#43938](https://github.com/nodejs/node/pull/43938)
* \[[`12657accdd`](https://github.com/nodejs/node/commit/12657accdd)] - **build**: add .gitattributes for npm and other shims (Hrishikesh Kadam) [#43879](https://github.com/nodejs/node/pull/43879)
* \[[`c2db4f4581`](https://github.com/nodejs/node/commit/c2db4f4581)] - **build**: make GitPod less noisy (Rich Trott) [#43829](https://github.com/nodejs/node/pull/43829)
* \[[`364deeadcd`](https://github.com/nodejs/node/commit/364deeadcd)] - **build**: add GitHub token permissions for workflows (Varun Sharma) [#43743](https://github.com/nodejs/node/pull/43743)
* \[[`8b83b4d5be`](https://github.com/nodejs/node/commit/8b83b4d5be)] - **child\_process**: do not need to count length when maxBuffer is Infinity (theanarkh) [#43822](https://github.com/nodejs/node/pull/43822)
* \[[`c1893b7a7c`](https://github.com/nodejs/node/commit/c1893b7a7c)] - **child\_process**: avoid repeated calls to `normalizeSpawnArguments` (木杉) [#43345](https://github.com/nodejs/node/pull/43345)
* \[[`7b276b89b9`](https://github.com/nodejs/node/commit/7b276b89b9)] - **cluster**: send connection to other server when worker drop it (theanarkh) [#43747](https://github.com/nodejs/node/pull/43747)
* \[[`e8c66f92a5`](https://github.com/nodejs/node/commit/e8c66f92a5)] - **crypto**: remove unneeded guard (Rich Trott) [#43856](https://github.com/nodejs/node/pull/43856)
* \[[`c95132e9ea`](https://github.com/nodejs/node/commit/c95132e9ea)] - **deps**: cherry-pick libuv/libuv\@3a7b955 (Ben Noordhuis) [#43950](https://github.com/nodejs/node/pull/43950)
* \[[`cc8d5426d2`](https://github.com/nodejs/node/commit/cc8d5426d2)] - **deps**: cherry-pick libuv/libuv\@abb109f (Ben Noordhuis) [#43950](https://github.com/nodejs/node/pull/43950)
* \[[`7762e463d6`](https://github.com/nodejs/node/commit/7762e463d6)] - **deps**: update corepack to 0.12.1 (Node.js GitHub Bot) [#43965](https://github.com/nodejs/node/pull/43965)
* \[[`1256c4dad5`](https://github.com/nodejs/node/commit/1256c4dad5)] - **deps**: update hast-util-raw (Moshe Atlow) [#43927](https://github.com/nodejs/node/pull/43927)
* \[[`aac97c2d2a`](https://github.com/nodejs/node/commit/aac97c2d2a)] - **deps**: update undici to 5.8.0 (Node.js GitHub Bot) [#43886](https://github.com/nodejs/node/pull/43886)
* \[[`cdff61917d`](https://github.com/nodejs/node/commit/cdff61917d)] - **deps**: clean archs files for OpenSSL (RafaelGSS) [#43735](https://github.com/nodejs/node/pull/43735)
* \[[`fc936a84e4`](https://github.com/nodejs/node/commit/fc936a84e4)] - **deps**: remove not used architectures (RafaelGSS) [#43735](https://github.com/nodejs/node/pull/43735)
* \[[`361a643d8b`](https://github.com/nodejs/node/commit/361a643d8b)] - **deps**: V8: backport f3cad8cec656 (Joyee Cheung) [#43531](https://github.com/nodejs/node/pull/43531)
* \[[`2e1732ebd0`](https://github.com/nodejs/node/commit/2e1732ebd0)] - **deps**: V8: backport 22698d267667 (Chengzhong Wu) [#43751](https://github.com/nodejs/node/pull/43751)
* \[[`979f469d3a`](https://github.com/nodejs/node/commit/979f469d3a)] - **deps**: upgrade npm to 8.15.0 (npm team) [#43917](https://github.com/nodejs/node/pull/43917)
* \[[`4096d81988`](https://github.com/nodejs/node/commit/4096d81988)] - **deps**: upgrade npm to 8.14.0 (npm team) [#43826](https://github.com/nodejs/node/pull/43826)
* \[[`2ec8092e2c`](https://github.com/nodejs/node/commit/2ec8092e2c)] - **deps,src**: use SIMD for normal base64 encoding (Brian White) [#39775](https://github.com/nodejs/node/pull/39775)
* \[[`67b4edde37`](https://github.com/nodejs/node/commit/67b4edde37)] - **dns**: fix getServers return undefined (jiahao.si) [#43922](https://github.com/nodejs/node/pull/43922)
* \[[`7c75539a88`](https://github.com/nodejs/node/commit/7c75539a88)] - **dns**: fix cares memory leak (theanarkh) [#43912](https://github.com/nodejs/node/pull/43912)
* \[[`1f80b88da5`](https://github.com/nodejs/node/commit/1f80b88da5)] - **doc**: update email and mailmap for BethGriggs (Beth Griggs) [#43985](https://github.com/nodejs/node/pull/43985)
* \[[`8a2a6e16eb`](https://github.com/nodejs/node/commit/8a2a6e16eb)] - **doc**: add 15.x - 18.x to Other Versions section (shhh7612) [#43940](https://github.com/nodejs/node/pull/43940)
* \[[`51cb0d42ca`](https://github.com/nodejs/node/commit/51cb0d42ca)] - **doc**: inspector.close undefined in worker threads (Keyhan Vakil) [#43867](https://github.com/nodejs/node/pull/43867)
* \[[`c789c0f5f7`](https://github.com/nodejs/node/commit/c789c0f5f7)] - **doc**: improve documentation for safe `Promise` statics alternatives (Antoine du Hamel) [#43759](https://github.com/nodejs/node/pull/43759)
* \[[`cb9b0e0011`](https://github.com/nodejs/node/commit/cb9b0e0011)] - **doc**: recommend git-node-v8 (Keyhan Vakil) [#43934](https://github.com/nodejs/node/pull/43934)
* \[[`d7e9bd1830`](https://github.com/nodejs/node/commit/d7e9bd1830)] - **doc**: clarify subprocess.stdout/in/err property (Kohei Ueno) [#43910](https://github.com/nodejs/node/pull/43910)
* \[[`808793ebb5`](https://github.com/nodejs/node/commit/808793ebb5)] - **doc**: fix typo in `src/crypto/README.md` (Jianru Lin) [#43968](https://github.com/nodejs/node/pull/43968)
* \[[`bbc455c4f9`](https://github.com/nodejs/node/commit/bbc455c4f9)] - **doc**: remind backporter about v8\_embedder\_string (Keyhan Vakil) [#43924](https://github.com/nodejs/node/pull/43924)
* \[[`a86b66c8b4`](https://github.com/nodejs/node/commit/a86b66c8b4)] - **doc**: fix typo in http.md (Airing) [#43933](https://github.com/nodejs/node/pull/43933)
* \[[`a96af37233`](https://github.com/nodejs/node/commit/a96af37233)] - **doc**: add F3n67u to collaborators (Feng Yu) [#43953](https://github.com/nodejs/node/pull/43953)
* \[[`aa7d4e59f7`](https://github.com/nodejs/node/commit/aa7d4e59f7)] - **doc**: improve test runner timeout docs (Tobias Nießen) [#43836](https://github.com/nodejs/node/pull/43836)
* \[[`80c2fa8212`](https://github.com/nodejs/node/commit/80c2fa8212)] - **doc**: mention Win 32-bit openssl build issue (RafaelGSS) [#43853](https://github.com/nodejs/node/pull/43853)
* \[[`8b8c55df7e`](https://github.com/nodejs/node/commit/8b8c55df7e)] - **doc**: add security release specifics to releases.md (Beth Griggs) [#43835](https://github.com/nodejs/node/pull/43835)
* \[[`42693aaf9f`](https://github.com/nodejs/node/commit/42693aaf9f)] - **doc**: add history info for `global.performance` (Antoine du Hamel) [#43841](https://github.com/nodejs/node/pull/43841)
* \[[`140d6af572`](https://github.com/nodejs/node/commit/140d6af572)] - **doc**: add platform-windows-arm to who to CC (Michael Dawson) [#43808](https://github.com/nodejs/node/pull/43808)
* \[[`976093efe3`](https://github.com/nodejs/node/commit/976093efe3)] - **doc**: document ES2022's Error "cause" property (James Ide) [#43830](https://github.com/nodejs/node/pull/43830)
* \[[`ec7e45e4a2`](https://github.com/nodejs/node/commit/ec7e45e4a2)] - **doc**: include make clean to openssl arch (RafaelGSS) [#43735](https://github.com/nodejs/node/pull/43735)
* \[[`d64dfd53c9`](https://github.com/nodejs/node/commit/d64dfd53c9)] - **doc**: add link to diagnostic tools (Rafael Gonzaga) [#43736](https://github.com/nodejs/node/pull/43736)
* \[[`2910136920`](https://github.com/nodejs/node/commit/2910136920)] - **doc**: update links to MDN page about dynamic imports (Jannis R) [#43847](https://github.com/nodejs/node/pull/43847)
* \[[`d88a9fae79`](https://github.com/nodejs/node/commit/d88a9fae79)] - **doc**: deprecate coercion to integer in process.exit (Daeyeon Jeong) [#43738](https://github.com/nodejs/node/pull/43738)
* \[[`fc843e103d`](https://github.com/nodejs/node/commit/fc843e103d)] - **doc**: add MoLow to triagers (Moshe Atlow) [#43799](https://github.com/nodejs/node/pull/43799)
* \[[`8c8c97da61`](https://github.com/nodejs/node/commit/8c8c97da61)] - **(SEMVER-MINOR)** **doc**: deprecate diagnostics\_channel object subscribe method (Stephen Belanger) [#42714](https://github.com/nodejs/node/pull/42714)
* \[[`9b53a694b5`](https://github.com/nodejs/node/commit/9b53a694b5)] - **doc**: revert anachronistic 'node:' module require()s in API history notes (DeeDeeG) [#43768](https://github.com/nodejs/node/pull/43768)
* \[[`2815bd3002`](https://github.com/nodejs/node/commit/2815bd3002)] - **doc**: clarify release process for new releasers (Rafael Gonzaga) [#43739](https://github.com/nodejs/node/pull/43739)
* \[[`50b3750e67`](https://github.com/nodejs/node/commit/50b3750e67)] - **doc**: fix typo in ngtcp2 readme (Dan Castillo) [#43767](https://github.com/nodejs/node/pull/43767)
* \[[`6bcd40dd85`](https://github.com/nodejs/node/commit/6bcd40dd85)] - **domain**: fix vm promise tracking while keeping isolation (Stephen Belanger) [#43556](https://github.com/nodejs/node/pull/43556)
* \[[`e89e0b470b`](https://github.com/nodejs/node/commit/e89e0b470b)] - **esm**: remove superfluous argument (Rich Trott) [#43884](https://github.com/nodejs/node/pull/43884)
* \[[`0d2921f396`](https://github.com/nodejs/node/commit/0d2921f396)] - **esm**: fix erroneous re-initialization of ESMLoader (Jacob Smith) [#43763](https://github.com/nodejs/node/pull/43763)
* \[[`9b5b8d78c3`](https://github.com/nodejs/node/commit/9b5b8d78c3)] - **esm**: throw on any non-2xx response (LiviaMedeiros) [#43742](https://github.com/nodejs/node/pull/43742)
* \[[`dfc4832ef1`](https://github.com/nodejs/node/commit/dfc4832ef1)] - **(SEMVER-MINOR)** **events**: expose CustomEvent on global with CLI flag (Daeyeon Jeong) [#43885](https://github.com/nodejs/node/pull/43885)
* \[[`e4473952ae`](https://github.com/nodejs/node/commit/e4473952ae)] - **(SEMVER-MINOR)** **events**: add `CustomEvent` (Daeyeon Jeong) [#43514](https://github.com/nodejs/node/pull/43514)
* \[[`100f6deb09`](https://github.com/nodejs/node/commit/100f6deb09)] - **fs**: use signed types for stat data (LiviaMedeiros) [#43714](https://github.com/nodejs/node/pull/43714)
* \[[`25ec71db63`](https://github.com/nodejs/node/commit/25ec71db63)] - **http**: fix http server connection list when close (theanarkh) [#43949](https://github.com/nodejs/node/pull/43949)
* \[[`ca658c8afe`](https://github.com/nodejs/node/commit/ca658c8afe)] - **(SEMVER-MINOR)** **http**: add drop request event for http server (theanarkh) [#43806](https://github.com/nodejs/node/pull/43806)
* \[[`9c699bd8a8`](https://github.com/nodejs/node/commit/9c699bd8a8)] - **http**: wait for pending responses in closeIdleConnections (Paolo Insogna) [#43890](https://github.com/nodejs/node/pull/43890)
* \[[`781d5e54e3`](https://github.com/nodejs/node/commit/781d5e54e3)] - **inspector**: set sampling interval before start (Shelley Vohr) [#43779](https://github.com/nodejs/node/pull/43779)
* \[[`0b5dbb2a56`](https://github.com/nodejs/node/commit/0b5dbb2a56)] - **lib**: refactor PriorityQueue to use private field (Finn Yu) [#43889](https://github.com/nodejs/node/pull/43889)
* \[[`324473ca32`](https://github.com/nodejs/node/commit/324473ca32)] - **(SEMVER-MINOR)** **lib**: improved diagnostics\_channel subscribe/unsubscribe (Stephen Belanger) [#42714](https://github.com/nodejs/node/pull/42714)
* \[[`5aa3b213ac`](https://github.com/nodejs/node/commit/5aa3b213ac)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#43966](https://github.com/nodejs/node/pull/43966)
* \[[`e707552357`](https://github.com/nodejs/node/commit/e707552357)] - **meta**: update `node-api` in label-pr-config (Daeyeon Jeong) [#43794](https://github.com/nodejs/node/pull/43794)
* \[[`8a8de94034`](https://github.com/nodejs/node/commit/8a8de94034)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#43872](https://github.com/nodejs/node/pull/43872)
* \[[`7d49fc766c`](https://github.com/nodejs/node/commit/7d49fc766c)] - **meta**: use platform dropdown on flaky template (Rafael Gonzaga) [#43855](https://github.com/nodejs/node/pull/43855)
* \[[`e4aa50fc3f`](https://github.com/nodejs/node/commit/e4aa50fc3f)] - **meta**: enable blank issues (Matteo Collina) [#43775](https://github.com/nodejs/node/pull/43775)
* \[[`ceb7c150ec`](https://github.com/nodejs/node/commit/ceb7c150ec)] - **meta**: move one or more collaborators to emeritus (Node.js GitHub Bot) [#43770](https://github.com/nodejs/node/pull/43770)
* \[[`29bcd47738`](https://github.com/nodejs/node/commit/29bcd47738)] - **net**: fix socket.\_getpeername (Daeyeon Jeong) [#43010](https://github.com/nodejs/node/pull/43010)
* \[[`380659daf1`](https://github.com/nodejs/node/commit/380659daf1)] - **process**: use `defineProperty` instead of assignment (Mark S. Miller) [#43907](https://github.com/nodejs/node/pull/43907)
* \[[`aba9c8ebea`](https://github.com/nodejs/node/commit/aba9c8ebea)] - **repl**: fix overzealous top-level await (Tobias Nießen) [#43827](https://github.com/nodejs/node/pull/43827)
* \[[`1deb6b73b7`](https://github.com/nodejs/node/commit/1deb6b73b7)] - **repl**: use `SafePromiseAll` and `SafePromiseRace` (Antoine du Hamel) [#43758](https://github.com/nodejs/node/pull/43758)
* \[[`bf8f2e23ff`](https://github.com/nodejs/node/commit/bf8f2e23ff)] - **src**: refactor DH groups to delete crypto\_groups.h (Tobias Nießen) [#43896](https://github.com/nodejs/node/pull/43896)
* \[[`9435fbf8cd`](https://github.com/nodejs/node/commit/9435fbf8cd)] - **src**: remove dead code in base64\_encode (Tobias Nießen) [#43979](https://github.com/nodejs/node/pull/43979)
* \[[`2c47e58ea0`](https://github.com/nodejs/node/commit/2c47e58ea0)] - **src**: fix regression that a source marker is lost (cola119) [#43086](https://github.com/nodejs/node/pull/43086)
* \[[`d084150320`](https://github.com/nodejs/node/commit/d084150320)] - **src**: per-isolate eternal template properties (Chengzhong Wu) [#43802](https://github.com/nodejs/node/pull/43802)
* \[[`9f9d00ccbb`](https://github.com/nodejs/node/commit/9f9d00ccbb)] - **src**: merge NativeModuleEnv into NativeModuleLoader (Joyee Cheung) [#43824](https://github.com/nodejs/node/pull/43824)
* \[[`bb512904e9`](https://github.com/nodejs/node/commit/bb512904e9)] - **src**: use named struct instead of typedef (Tobias Nießen) [#43881](https://github.com/nodejs/node/pull/43881)
* \[[`bb5511e8cc`](https://github.com/nodejs/node/commit/bb5511e8cc)] - **src**: use named enum instead of typedef (Tobias Nießen) [#43880](https://github.com/nodejs/node/pull/43880)
* \[[`5db0c8f667`](https://github.com/nodejs/node/commit/5db0c8f667)] - **src**: pass only Isolate\* and env\_vars to EnabledDebugList::Parse() (Darshan Sen) [#43668](https://github.com/nodejs/node/pull/43668)
* \[[`249365524e`](https://github.com/nodejs/node/commit/249365524e)] - **src**: fix node watchdog race condition (theanarkh) [#43780](https://github.com/nodejs/node/pull/43780)
* \[[`17cb27237d`](https://github.com/nodejs/node/commit/17cb27237d)] - **src**: deduplicate `SetALPN` implementations (Tobias Nießen) [#43756](https://github.com/nodejs/node/pull/43756)
* \[[`b4c75a96be`](https://github.com/nodejs/node/commit/b4c75a96be)] - **src**: fix `napi_check_object_type_tag()` (Daeyeon Jeong) [#43788](https://github.com/nodejs/node/pull/43788)
* \[[`8432d6596f`](https://github.com/nodejs/node/commit/8432d6596f)] - **src**: slim down env-inl.h (Ben Noordhuis) [#43745](https://github.com/nodejs/node/pull/43745)
* \[[`2266a4b6d6`](https://github.com/nodejs/node/commit/2266a4b6d6)] - **stream**: improve `respondWithNewView()` (Daeyeon Jeong) [#43866](https://github.com/nodejs/node/pull/43866)
* \[[`bf3991b406`](https://github.com/nodejs/node/commit/bf3991b406)] - **stream**: fix 0 transform hwm backpressure (Robert Nagy) [#43685](https://github.com/nodejs/node/pull/43685)
* \[[`a057510037`](https://github.com/nodejs/node/commit/a057510037)] - **stream**: initial approach to include strategy options on Readable.toWeb() (txxnano) [#43515](https://github.com/nodejs/node/pull/43515)
* \[[`198cf59d2c`](https://github.com/nodejs/node/commit/198cf59d2c)] - **test**: update WPT encoding tests (Kohei Ueno) [#43958](https://github.com/nodejs/node/pull/43958)
* \[[`f0ed1aed8d`](https://github.com/nodejs/node/commit/f0ed1aed8d)] - **test**: remove test-whatwg-events-add-event-listener-options-once.js (Feng Yu) [#43877](https://github.com/nodejs/node/pull/43877)
* \[[`88505556fe`](https://github.com/nodejs/node/commit/88505556fe)] - **test**: work scheduled in process.nextTick can keep the event loop alive (Andreu Botella) [#43787](https://github.com/nodejs/node/pull/43787)
* \[[`81a21946eb`](https://github.com/nodejs/node/commit/81a21946eb)] - **test**: simplify test-tls-set-secure-context (Tobias Nießen) [#43878](https://github.com/nodejs/node/pull/43878)
* \[[`61cd11a8a7`](https://github.com/nodejs/node/commit/61cd11a8a7)] - **test**: use `common.mustNotMutateObjectDeep()` in fs tests (LiviaMedeiros) [#43819](https://github.com/nodejs/node/pull/43819)
* \[[`b1081dbe12`](https://github.com/nodejs/node/commit/b1081dbe12)] - **test**: fix test http upload timeout (theanarkh) [#43935](https://github.com/nodejs/node/pull/43935)
* \[[`efd5e0e925`](https://github.com/nodejs/node/commit/efd5e0e925)] - **test**: simplify ReplStream.wait() (Tobias Nießen) [#43857](https://github.com/nodejs/node/pull/43857)
* \[[`ef21ad2996`](https://github.com/nodejs/node/commit/ef21ad2996)] - **test**: merge test-crypto-dh-hash with modp18 test (Tobias Nießen) [#43891](https://github.com/nodejs/node/pull/43891)
* \[[`e502c50a90`](https://github.com/nodejs/node/commit/e502c50a90)] - **test**: refactor `test/es-module/test-esm-resolve-type` (Antoine du Hamel) [#43178](https://github.com/nodejs/node/pull/43178)
* \[[`c782c3dc69`](https://github.com/nodejs/node/commit/c782c3dc69)] - **test**: ensure NODE\_EXTRA\_CA\_CERTS not set before tests (KrayzeeKev) [#43858](https://github.com/nodejs/node/pull/43858)
* \[[`bb6787cb57`](https://github.com/nodejs/node/commit/bb6787cb57)] - **test**: add check to test-fs-readfile-tostring-fail (Richard Lau) [#43850](https://github.com/nodejs/node/pull/43850)
* \[[`7571704186`](https://github.com/nodejs/node/commit/7571704186)] - **test**: complete TODO in test/wpt/test-url.js (Kohei Ueno) [#43797](https://github.com/nodejs/node/pull/43797)
* \[[`6f1d2dfb9d`](https://github.com/nodejs/node/commit/6f1d2dfb9d)] - **test**: add test on worker process.exit in async modules (Chengzhong Wu) [#43751](https://github.com/nodejs/node/pull/43751)
* \[[`776cc3abbd`](https://github.com/nodejs/node/commit/776cc3abbd)] - **test**: use `common.mustNotMutateObjectDeep()` in immutability tests (LiviaMedeiros) [#43196](https://github.com/nodejs/node/pull/43196)
* \[[`42f2deb3a0`](https://github.com/nodejs/node/commit/42f2deb3a0)] - **test**: add `common.mustNotMutateObjectDeep()` (LiviaMedeiros) [#43196](https://github.com/nodejs/node/pull/43196)
* \[[`f3fc51c508`](https://github.com/nodejs/node/commit/f3fc51c508)] - **test**: fix coverity warning in test (Michael Dawson) [#43631](https://github.com/nodejs/node/pull/43631)
* \[[`a9ecba2fa8`](https://github.com/nodejs/node/commit/a9ecba2fa8)] - **test**: mark test-http-client-response-timeout flaky (Tobias Nießen) [#43792](https://github.com/nodejs/node/pull/43792)
* \[[`cd0d9ddb7c`](https://github.com/nodejs/node/commit/cd0d9ddb7c)] - **test\_runner**: add support for boolean values for `concurrency` option (Lenvin Gonsalves) [#43887](https://github.com/nodejs/node/pull/43887)
* \[[`f98020138a`](https://github.com/nodejs/node/commit/f98020138a)] - **test\_runner**: validate `timeout` option (Antoine du Hamel) [#43843](https://github.com/nodejs/node/pull/43843)
* \[[`58d15b3687`](https://github.com/nodejs/node/commit/58d15b3687)] - **test\_runner**: pass signal on timeout (Moshe Atlow) [#43911](https://github.com/nodejs/node/pull/43911)
* \[[`8b0248506f`](https://github.com/nodejs/node/commit/8b0248506f)] - **test\_runner**: do not report an error when tests are passing (Antoine du Hamel) [#43919](https://github.com/nodejs/node/pull/43919)
* \[[`aa8053e1fa`](https://github.com/nodejs/node/commit/aa8053e1fa)] - **test\_runner**: recieve and pass AbortSignal (Moshe Atlow) [#43554](https://github.com/nodejs/node/pull/43554)
* \[[`f13e4c1be9`](https://github.com/nodejs/node/commit/f13e4c1be9)] - **test\_runner**: fix `it` concurrency (Moshe Atlow) [#43757](https://github.com/nodejs/node/pull/43757)
* \[[`e404a3ef6d`](https://github.com/nodejs/node/commit/e404a3ef6d)] - **test\_runner**: support timeout for tests (Moshe Atlow) [#43505](https://github.com/nodejs/node/pull/43505)
* \[[`f28198cc05`](https://github.com/nodejs/node/commit/f28198cc05)] - **test\_runner**: catch errors thrown within `describe` (Moshe Atlow) [#43729](https://github.com/nodejs/node/pull/43729)
* \[[`bfe0ac6cd0`](https://github.com/nodejs/node/commit/bfe0ac6cd0)] - **tools**: add more options to track flaky tests (Antoine du Hamel) [#43954](https://github.com/nodejs/node/pull/43954)
* \[[`17a4e5e775`](https://github.com/nodejs/node/commit/17a4e5e775)] - **tools**: add verbose flag to inactive TSC finder (Rich Trott) [#43913](https://github.com/nodejs/node/pull/43913)
* \[[`373304b0c7`](https://github.com/nodejs/node/commit/373304b0c7)] - **tools**: add support for using API key to vuln checking script (Facundo Tuesca) [#43909](https://github.com/nodejs/node/pull/43909)
* \[[`ed45088c14`](https://github.com/nodejs/node/commit/ed45088c14)] - **tools**: support versioned node shared libs on z/OS (alexcfyung) [#42256](https://github.com/nodejs/node/pull/42256)
* \[[`c9ecd6d21f`](https://github.com/nodejs/node/commit/c9ecd6d21f)] - **tools**: update doc to highlight.js\@11.6.0 (Node.js GitHub Bot) [#43870](https://github.com/nodejs/node/pull/43870)
* \[[`c92135aa0f`](https://github.com/nodejs/node/commit/c92135aa0f)] - **tools**: update lint-md-dependencies to rollup\@2.77.0 (Node.js GitHub Bot) [#43871](https://github.com/nodejs/node/pull/43871)
* \[[`e12bf40fd1`](https://github.com/nodejs/node/commit/e12bf40fd1)] - **tools**: update eslint to 8.20.0 (Node.js GitHub Bot) [#43873](https://github.com/nodejs/node/pull/43873)
* \[[`09fe9b30a9`](https://github.com/nodejs/node/commit/09fe9b30a9)] - **tools**: add script for vulnerability checking (Facundo Tuesca) [#43362](https://github.com/nodejs/node/pull/43362)
* \[[`19e8876877`](https://github.com/nodejs/node/commit/19e8876877)] - **trace\_events**: trace net connect event (theanarkh) [#43903](https://github.com/nodejs/node/pull/43903)
* \[[`1af7f24143`](https://github.com/nodejs/node/commit/1af7f24143)] - **util**: remove unicode support todo for perf implications (Rhys) [#43762](https://github.com/nodejs/node/pull/43762)
* \[[`acfc33ca8c`](https://github.com/nodejs/node/commit/acfc33ca8c)] - **(SEMVER-MINOR)** **util**: add tokens to parseArgs (John Gee) [#43459](https://github.com/nodejs/node/pull/43459)
* \[[`f32aec8a6d`](https://github.com/nodejs/node/commit/f32aec8a6d)] - **util**: refactor to use validateObject (Kohei Ueno) [#43769](https://github.com/nodejs/node/pull/43769)
* \[[`d7cfd0c5ba`](https://github.com/nodejs/node/commit/d7cfd0c5ba)] - **v8**: serialize BigInt64Array and BigUint64Array (Ben Noordhuis) [#43571](https://github.com/nodejs/node/pull/43571)

<a id="18.6.0"></a>

## 2022-07-13, Version 18.6.0 (Current), @targos

### Notable Changes

#### Experimental ESM Loader Hooks API

Node.js ESM Loader hooks now support multiple custom loaders, and composition is
achieved via "chaining": `foo-loader` calls `bar-loader` calls `qux-loader`
(a custom loader _must_ now signal a short circuit when intentionally not
calling the next). See the [ESM docs](https://nodejs.org/api/esm.html) for details.

Real-world use-cases are laid out for end-users with working examples in the
article [Custom ESM loaders: Who, what, when, where, why, how](https://dev.to/jakobjingleheimer/custom-esm-loaders-who-what-when-where-why-how-4i1o).

Contributed by Jacob Smith, Geoffrey Booth, and Bradley Farias - <https://github.com/nodejs/node/pull/42623>

### Commits

#### Semver-minor commits

* \[[`0bca7b722e`](https://github.com/nodejs/node/commit/0bca7b722e)] - **(SEMVER-MINOR)** **dns**: export error code constants from `dns/promises` (Feng Yu) [#43176](https://github.com/nodejs/node/pull/43176)
* \[[`da61e2330f`](https://github.com/nodejs/node/commit/da61e2330f)] - **(SEMVER-MINOR)** **esm**: add chaining to loaders (Jacob Smith) [#42623](https://github.com/nodejs/node/pull/42623)
* \[[`8c97f63401`](https://github.com/nodejs/node/commit/8c97f63401)] - **(SEMVER-MINOR)** **http**: add diagnostics channel for http client (theanarkh) [#43580](https://github.com/nodejs/node/pull/43580)
* \[[`b27856d3d4`](https://github.com/nodejs/node/commit/b27856d3d4)] - **(SEMVER-MINOR)** **http**: add perf\_hooks detail for http request and client (theanarkh) [#43361](https://github.com/nodejs/node/pull/43361)
* \[[`9d918d9923`](https://github.com/nodejs/node/commit/9d918d9923)] - **(SEMVER-MINOR)** **module**: add isBuiltIn method (hemanth.hm) [#43396](https://github.com/nodejs/node/pull/43396)
* \[[`a0e7b9983c`](https://github.com/nodejs/node/commit/a0e7b9983c)] - **(SEMVER-MINOR)** **net**: add drop event for net server (theanarkh) [#43582](https://github.com/nodejs/node/pull/43582)
* \[[`4bda6e02a3`](https://github.com/nodejs/node/commit/4bda6e02a3)] - **(SEMVER-MINOR)** **test\_runner**: expose `describe` and `it` (Moshe Atlow) [#43420](https://github.com/nodejs/node/pull/43420)
* \[[`34e83312a4`](https://github.com/nodejs/node/commit/34e83312a4)] - **(SEMVER-MINOR)** **v8**: add v8.startupSnapshot utils (Joyee Cheung) [#43329](https://github.com/nodejs/node/pull/43329)

#### Semver-patch commits

* \[[`ef174eac87`](https://github.com/nodejs/node/commit/ef174eac87)] - **assert**: callTracker throw a specific error message when possible (Moshe Atlow) [#43640](https://github.com/nodejs/node/pull/43640)
* \[[`07836637af`](https://github.com/nodejs/node/commit/07836637af)] - **bootstrap**: move global initialization to js (Alena Khineika) [#43625](https://github.com/nodejs/node/pull/43625)
* \[[`e9ee7e44be`](https://github.com/nodejs/node/commit/e9ee7e44be)] - **build,test**: increase stack size limit on Windows (Tobias Nießen) [#43632](https://github.com/nodejs/node/pull/43632)
* \[[`3ca9e653a6`](https://github.com/nodejs/node/commit/3ca9e653a6)] - **child\_process**: improve ipc write performance (rubikscraft) [#42931](https://github.com/nodejs/node/pull/42931)
* \[[`cad6d990ec`](https://github.com/nodejs/node/commit/cad6d990ec)] - **child\_process**: speed up 'advanced' ipc receiving (rubikscraft) [#42931](https://github.com/nodejs/node/pull/42931)
* \[[`ce3a22a9e3`](https://github.com/nodejs/node/commit/ce3a22a9e3)] - **cluster**: fix closing dgram sockets in cluster workers throws errors (Ouyang Yadong) [#43709](https://github.com/nodejs/node/pull/43709)
* \[[`5d8ee519db`](https://github.com/nodejs/node/commit/5d8ee519db)] - **cluster**: fix fd leak (theanarkh) [#43650](https://github.com/nodejs/node/pull/43650)
* \[[`fa5c4643e2`](https://github.com/nodejs/node/commit/fa5c4643e2)] - **cluster, net**: fix listen pipe with readable and writable in cluster (theanarkh) [#43634](https://github.com/nodejs/node/pull/43634)
* \[[`4df96b501d`](https://github.com/nodejs/node/commit/4df96b501d)] - **crypto**: don't disable TLS 1.3 without suites (Adam Majer) [#43427](https://github.com/nodejs/node/pull/43427)
* \[[`a43928ae78`](https://github.com/nodejs/node/commit/a43928ae78)] - **crypto**: use ByteSource::Builder in To\*Copy (Tobias Nießen) [#43477](https://github.com/nodejs/node/pull/43477)
* \[[`bb326f7ece`](https://github.com/nodejs/node/commit/bb326f7ece)] - **crypto**: handle webcrypto generateKey() usages edge case (Filip Skokan) [#43454](https://github.com/nodejs/node/pull/43454)
* \[[`9bd13bbb3a`](https://github.com/nodejs/node/commit/9bd13bbb3a)] - **crypto**: update Wrapping and unwrapping keys webcrypto example (Filip Skokan) [#43452](https://github.com/nodejs/node/pull/43452)
* \[[`679f19128e`](https://github.com/nodejs/node/commit/679f19128e)] - **crypto**: fix webcrypto generateKey() with empty usages (Filip Skokan) [#43431](https://github.com/nodejs/node/pull/43431)
* \[[`64a9dd7b83`](https://github.com/nodejs/node/commit/64a9dd7b83)] - **crypto**: fix webcrypto digest() invalid algorithm (Filip Skokan) [#43431](https://github.com/nodejs/node/pull/43431)
* \[[`dedb22e965`](https://github.com/nodejs/node/commit/dedb22e965)] - **crypto**: fix webcrypto RSA generateKey() use of publicExponent (Filip Skokan) [#43431](https://github.com/nodejs/node/pull/43431)
* \[[`018f61cb4f`](https://github.com/nodejs/node/commit/018f61cb4f)] - **crypto**: fix webcrypto AES-KW keys accepting encrypt/decrypt usages (Filip Skokan) [#43431](https://github.com/nodejs/node/pull/43431)
* \[[`3ee0bb8d03`](https://github.com/nodejs/node/commit/3ee0bb8d03)] - **crypto**: fix webcrypto deriveBits for non-byte lengths (Filip Skokan) [#43431](https://github.com/nodejs/node/pull/43431)
* \[[`7fc075b23a`](https://github.com/nodejs/node/commit/7fc075b23a)] - **deps**: update undici to 5.7.0 (Node.js GitHub Bot) [#43790](https://github.com/nodejs/node/pull/43790)
* \[[`d6a9e93426`](https://github.com/nodejs/node/commit/d6a9e93426)] - **deps**: patch V8 to 10.2.154.13 (Michaël Zasso) [#43727](https://github.com/nodejs/node/pull/43727)
* \[[`428d03cb94`](https://github.com/nodejs/node/commit/428d03cb94)] - **deps**: update corepack to 0.12.0 (Node.js GitHub Bot) [#43748](https://github.com/nodejs/node/pull/43748)
* \[[`74914698e5`](https://github.com/nodejs/node/commit/74914698e5)] - **deps**: upgrade npm to 8.13.2 (npm team) [#43622](https://github.com/nodejs/node/pull/43622)
* \[[`0636f86ecc`](https://github.com/nodejs/node/commit/0636f86ecc)] - **deps**: upgrade npm to 8.13.1 (npm team) [#43552](https://github.com/nodejs/node/pull/43552)
* \[[`2149acda60`](https://github.com/nodejs/node/commit/2149acda60)] - **dns**: make promise API fully constructed from `lib/internal/dns/promises` (Feng Yu) [#43227](https://github.com/nodejs/node/pull/43227)
* \[[`79ea19e5e2`](https://github.com/nodejs/node/commit/79ea19e5e2)] - **errors**: extract type detection & use in `ERR_INVALID_RETURN_VALUE` (Jacob Smith) [#43558](https://github.com/nodejs/node/pull/43558)
* \[[`80ced1ae31`](https://github.com/nodejs/node/commit/80ced1ae31)] - **esm**: treat `307` and `308` as redirects in HTTPS imports (Kid) [#43689](https://github.com/nodejs/node/pull/43689)
* \[[`953fefe77b`](https://github.com/nodejs/node/commit/953fefe77b)] - **esm**: restore `next<HookName>`'s `context` as optional arg (Jacob Smith) [#43553](https://github.com/nodejs/node/pull/43553)
* \[[`10bcad5c6e`](https://github.com/nodejs/node/commit/10bcad5c6e)] - **esm**: fix chain advances when loader calls next\<HookName> multiple times (Jacob Smith) [#43303](https://github.com/nodejs/node/pull/43303)
* \[[`50d64edd49`](https://github.com/nodejs/node/commit/50d64edd49)] - **esm**: refactor responseURL handling (Guy Bedford) [#43164](https://github.com/nodejs/node/pull/43164)
* \[[`254efd9e3b`](https://github.com/nodejs/node/commit/254efd9e3b)] - **esm**: fix http(s) import via custom loader (Jacob Smith) [#43130](https://github.com/nodejs/node/pull/43130)
* \[[`061ed0e76b`](https://github.com/nodejs/node/commit/061ed0e76b)] - **events**: improve `Event` compatibility (Daeyeon Jeong) [#43461](https://github.com/nodejs/node/pull/43461)
* \[[`66fb059547`](https://github.com/nodejs/node/commit/66fb059547)] - **events**: improve `EventListener` validation (Daeyeon Jeong) [#43491](https://github.com/nodejs/node/pull/43491)
* \[[`12a591a676`](https://github.com/nodejs/node/commit/12a591a676)] - **fs**: refactor realpath with Map and Set (LiviaMedeiros) [#43569](https://github.com/nodejs/node/pull/43569)
* \[[`df501316c1`](https://github.com/nodejs/node/commit/df501316c1)] - **fs**: don't end fs promises on Isolate termination (Santiago Gimeno) [#42910](https://github.com/nodejs/node/pull/42910)
* \[[`e6d4837fad`](https://github.com/nodejs/node/commit/e6d4837fad)] - **http**: fix failing test (Paolo Insogna) [#43641](https://github.com/nodejs/node/pull/43641)
* \[[`491c7619c4`](https://github.com/nodejs/node/commit/491c7619c4)] - **http**: defer reentrant execution of Parser::Execute (Paolo Insogna) [#43369](https://github.com/nodejs/node/pull/43369)
* \[[`d71ba322b0`](https://github.com/nodejs/node/commit/d71ba322b0)] - **http**: fix http agent keep alive (theanarkh) [#43380](https://github.com/nodejs/node/pull/43380)
* \[[`1f4f811de5`](https://github.com/nodejs/node/commit/1f4f811de5)] - **http2**: log debug only when in debug mode (Basit) [#43626](https://github.com/nodejs/node/pull/43626)
* \[[`c8cbec4cef`](https://github.com/nodejs/node/commit/c8cbec4cef)] - **lib**: make `validateObject` less affected by prototype tampering (Antoine du Hamel) [#42929](https://github.com/nodejs/node/pull/42929)
* \[[`dc484b6f6f`](https://github.com/nodejs/node/commit/dc484b6f6f)] - **lib**: implement safe alternatives to `Promise` static methods (Antoine du Hamel) [#43728](https://github.com/nodejs/node/pull/43728)
* \[[`2233567331`](https://github.com/nodejs/node/commit/2233567331)] - **lib**: use null-prototype objects for property descriptors (Antoine du Hamel) [#43473](https://github.com/nodejs/node/pull/43473)
* \[[`b9198d977f`](https://github.com/nodejs/node/commit/b9198d977f)] - **lib**: refactor to avoid unsafe regex primordials (Antoine du Hamel) [#43475](https://github.com/nodejs/node/pull/43475)
* \[[`deaf4bb5cd`](https://github.com/nodejs/node/commit/deaf4bb5cd)] - **lib**: fix TODO in `freeze_intrinsics` (Antoine du Hamel) [#43472](https://github.com/nodejs/node/pull/43472)
* \[[`61e6d7858a`](https://github.com/nodejs/node/commit/61e6d7858a)] - **lib,src**: add source map support for global eval (Chengzhong Wu) [#43428](https://github.com/nodejs/node/pull/43428)
* \[[`58646eaad6`](https://github.com/nodejs/node/commit/58646eaad6)] - **loader**: make `require.resolve` throw for unknown builtin modules (木杉) [#43336](https://github.com/nodejs/node/pull/43336)
* \[[`e914185c44`](https://github.com/nodejs/node/commit/e914185c44)] - **module**: cjs-module-lexer WebAssembly fallback (Guy Bedford) [#43612](https://github.com/nodejs/node/pull/43612)
* \[[`3ad4d37b3c`](https://github.com/nodejs/node/commit/3ad4d37b3c)] - **module**: also enable subpath imports in REPL (Ray) [#43450](https://github.com/nodejs/node/pull/43450)
* \[[`bf4ac4c55f`](https://github.com/nodejs/node/commit/bf4ac4c55f)] - **net**: remove redundant connecting assignment (Ouyang Yadong) [#43710](https://github.com/nodejs/node/pull/43710)
* \[[`ad1d0541c5`](https://github.com/nodejs/node/commit/ad1d0541c5)] - **net**: fix net keepalive and noDelay (theanarkh) [#43561](https://github.com/nodejs/node/pull/43561)
* \[[`f8bdc53e4f`](https://github.com/nodejs/node/commit/f8bdc53e4f)] - **net**: prevent /32 ipv4 mask from matching all ips (supriyo-biswas) [#43381](https://github.com/nodejs/node/pull/43381)
* \[[`47a252257b`](https://github.com/nodejs/node/commit/47a252257b)] - **net**: fix net.Server keepalive and noDelay (theanarkh) [#43497](https://github.com/nodejs/node/pull/43497)
* \[[`d834d216f2`](https://github.com/nodejs/node/commit/d834d216f2)] - **perf\_hooks**: add initiatorType getter (Rafael Gonzaga) [#43593](https://github.com/nodejs/node/pull/43593)
* \[[`02009b7069`](https://github.com/nodejs/node/commit/02009b7069)] - **perf\_hooks**: fix miscounted gc performance entry starttime (#43066) (Xuguang Mei) [#43066](https://github.com/nodejs/node/pull/43066)
* \[[`e9574f3009`](https://github.com/nodejs/node/commit/e9574f3009)] - **readline**: fix to not access a property on an undefined value (Kohei Ueno) [#43543](https://github.com/nodejs/node/pull/43543)
* \[[`fe1f740f61`](https://github.com/nodejs/node/commit/fe1f740f61)] - **src**: merge RunInThisContext() with RunInContext() (Daeyeon Jeong) [#43225](https://github.com/nodejs/node/pull/43225)
* \[[`0f6d19489a`](https://github.com/nodejs/node/commit/0f6d19489a)] - **src**: fix crash on FSReqPromise destructor (Santiago Gimeno) [#43533](https://github.com/nodejs/node/pull/43533)
* \[[`4e6a844207`](https://github.com/nodejs/node/commit/4e6a844207)] - **src**: delegate NodeArrayBufferAllocator to v8's allocator (Jeremy Rose) [#43594](https://github.com/nodejs/node/pull/43594)
* \[[`5ae30bf17a`](https://github.com/nodejs/node/commit/5ae30bf17a)] - **src**: remove a stale comment in `async_hooks` (Daeyeon Jeong) [#43317](https://github.com/nodejs/node/pull/43317)
* \[[`0b432b957e`](https://github.com/nodejs/node/commit/0b432b957e)] - **src**: fix compiler warning in src/heap\_utils.cc (Darshan Sen) [#43579](https://github.com/nodejs/node/pull/43579)
* \[[`d3fc791c3d`](https://github.com/nodejs/node/commit/d3fc791c3d)] - **src**: improve and update ByteSource description (Tobias Nießen) [#43478](https://github.com/nodejs/node/pull/43478)
* \[[`4e0afa4133`](https://github.com/nodejs/node/commit/4e0afa4133)] - **src**: remove CopyBuffer (Tobias Nießen) [#43463](https://github.com/nodejs/node/pull/43463)
* \[[`0659d5e3b0`](https://github.com/nodejs/node/commit/0659d5e3b0)] - **src**: change FormatSize to actually accept a size\_t (Tobias Nießen) [#43464](https://github.com/nodejs/node/pull/43464)
* \[[`66ee1f1e3c`](https://github.com/nodejs/node/commit/66ee1f1e3c)] - **src**: register StreamBase while registering LibuvStreamWrap (Darshan Sen) [#43321](https://github.com/nodejs/node/pull/43321)
* \[[`48ee6b9dc9`](https://github.com/nodejs/node/commit/48ee6b9dc9)] - **src,bootstrap**: remove NodeMainInstance::registry\_ (Darshan Sen) [#43392](https://github.com/nodejs/node/pull/43392)
* \[[`2e181f68a3`](https://github.com/nodejs/node/commit/2e181f68a3)] - **src,stream**: change return type to `Maybe` (Daeyeon Jeong) [#43575](https://github.com/nodejs/node/pull/43575)
* \[[`0f07abc80d`](https://github.com/nodejs/node/commit/0f07abc80d)] - **stream**: finish pipeline if dst closes before src (Robert Nagy) [#43701](https://github.com/nodejs/node/pull/43701)
* \[[`1617a4621e`](https://github.com/nodejs/node/commit/1617a4621e)] - **stream**: pass error on legacy destroy (Giacomo Gregoletto) [#43519](https://github.com/nodejs/node/pull/43519)
* \[[`40f51d8e83`](https://github.com/nodejs/node/commit/40f51d8e83)] - **test\_runner**: protect internals against prototype tampering (Antoine du Hamel) [#43578](https://github.com/nodejs/node/pull/43578)
* \[[`ddf7518520`](https://github.com/nodejs/node/commit/ddf7518520)] - **test\_runner**: cancel on termination (Moshe Atlow) [#43549](https://github.com/nodejs/node/pull/43549)
* \[[`e51d8c6004`](https://github.com/nodejs/node/commit/e51d8c6004)] - **test\_runner**: wait for stderr and stdout to complete (Moshe Atlow) [#43666](https://github.com/nodejs/node/pull/43666)
* \[[`dda64ddfbd`](https://github.com/nodejs/node/commit/dda64ddfbd)] - **test\_runner**: add Subtest to tap protocol output (Moshe Atlow) [#43417](https://github.com/nodejs/node/pull/43417)
* \[[`a1f1d3a7b3`](https://github.com/nodejs/node/commit/a1f1d3a7b3)] - **url**: update WHATWG URL parser to align with latest spec (Feng Yu) [#43190](https://github.com/nodejs/node/pull/43190)
* \[[`5a5c4be5a3`](https://github.com/nodejs/node/commit/5a5c4be5a3)] - **util**: add `AggregateError.prototype.errors` to inspect output (LiviaMedeiros) [#43646](https://github.com/nodejs/node/pull/43646)
* \[[`bdca4d3ccf`](https://github.com/nodejs/node/commit/bdca4d3ccf)] - **util**: remove unnecessary template string (Ruben Bridgewater) [#41082](https://github.com/nodejs/node/pull/41082)
* \[[`6b16836448`](https://github.com/nodejs/node/commit/6b16836448)] - **util**: mark cwd grey while inspecting errors (Ruben Bridgewater) [#41082](https://github.com/nodejs/node/pull/41082)
* \[[`baa22a7b7d`](https://github.com/nodejs/node/commit/baa22a7b7d)] - **util**: avoid inline access to Symbol.iterator (Kohei Ueno) [#43683](https://github.com/nodejs/node/pull/43683)
* \[[`a1f581a61e`](https://github.com/nodejs/node/commit/a1f581a61e)] - **util**: fix TypeError of symbol in template literals (cola119) [#42790](https://github.com/nodejs/node/pull/42790)
* \[[`ba9b2f021f`](https://github.com/nodejs/node/commit/ba9b2f021f)] - **wasi**: use WasmMemoryObject handle for perf (#43544) (snek) [#43544](https://github.com/nodejs/node/pull/43544)

#### Documentation commits

* \[[`e0769554a5`](https://github.com/nodejs/node/commit/e0769554a5)] - **doc**: remove bullet point referring to Node.js 12 (Luigi Pinca) [#43744](https://github.com/nodejs/node/pull/43744)
* \[[`7ffcd85ace`](https://github.com/nodejs/node/commit/7ffcd85ace)] - **doc**: include last security release date (Rafael Gonzaga) [#43774](https://github.com/nodejs/node/pull/43774)
* \[[`4569d6ebcb`](https://github.com/nodejs/node/commit/4569d6ebcb)] - **doc**: add details for July 2022 security releases (Beth Griggs) [#43733](https://github.com/nodejs/node/pull/43733)
* \[[`1bd56339c5`](https://github.com/nodejs/node/commit/1bd56339c5)] - **doc**: remove openssl 1.x reference (Rafael Gonzaga) [#43734](https://github.com/nodejs/node/pull/43734)
* \[[`bf62ffd848`](https://github.com/nodejs/node/commit/bf62ffd848)] - **doc**: remove node-report from support tiers (RafaelGSS) [#43737](https://github.com/nodejs/node/pull/43737)
* \[[`ca5af0dbf7`](https://github.com/nodejs/node/commit/ca5af0dbf7)] - **doc**: update changelog-maker to the new flags (RafaelGSS) [#43696](https://github.com/nodejs/node/pull/43696)
* \[[`088b9266d0`](https://github.com/nodejs/node/commit/088b9266d0)] - **doc**: remove extra 'in's (Colin Ihrig) [#43705](https://github.com/nodejs/node/pull/43705)
* \[[`7679c77347`](https://github.com/nodejs/node/commit/7679c77347)] - **doc**: add Geoffrey Booth to TSC (Rich Trott) [#43706](https://github.com/nodejs/node/pull/43706)
* \[[`d46261ceed`](https://github.com/nodejs/node/commit/d46261ceed)] - **doc**: improve readability of `dns.md` (0xSanyam) [#43694](https://github.com/nodejs/node/pull/43694)
* \[[`ca0fbfd87f`](https://github.com/nodejs/node/commit/ca0fbfd87f)] - **doc**: add note regarding special case of 0 stat.size (Douglas Wilson) [#43690](https://github.com/nodejs/node/pull/43690)
* \[[`267f66b5cc`](https://github.com/nodejs/node/commit/267f66b5cc)] - **doc**: fix default of duplex.allowHalfOpen (Vincent Weevers) [#43665](https://github.com/nodejs/node/pull/43665)
* \[[`46ad2061db`](https://github.com/nodejs/node/commit/46ad2061db)] - **doc**: fix typo in errors.md (Kazuma Ohashi) [#43677](https://github.com/nodejs/node/pull/43677)
* \[[`3a8edb363e`](https://github.com/nodejs/node/commit/3a8edb363e)] - **doc**: improve description of --input-type (cola119) [#43507](https://github.com/nodejs/node/pull/43507)
* \[[`b4b15b71d7`](https://github.com/nodejs/node/commit/b4b15b71d7)] - **doc**: add daeyeon to triagers (Daeyeon Jeong) [#43637](https://github.com/nodejs/node/pull/43637)
* \[[`cb77b3e3f7`](https://github.com/nodejs/node/commit/cb77b3e3f7)] - **doc**: remove appmetrics from tierlist (Tony Gorez) [#43608](https://github.com/nodejs/node/pull/43608)
* \[[`0fe825ac07`](https://github.com/nodejs/node/commit/0fe825ac07)] - **doc**: remove systemtap from tierlist (Tony Gorez) [#43605](https://github.com/nodejs/node/pull/43605)
* \[[`6fc5a13fe0`](https://github.com/nodejs/node/commit/6fc5a13fe0)] - **doc**: add single executable application initiative (Michael Dawson) [#43611](https://github.com/nodejs/node/pull/43611)
* \[[`350e6ae04c`](https://github.com/nodejs/node/commit/350e6ae04c)] - **doc**: remove windows xperf from tierlist (Tony Gorez) [#43607](https://github.com/nodejs/node/pull/43607)
* \[[`a6e98dfd65`](https://github.com/nodejs/node/commit/a6e98dfd65)] - **doc**: remove lttng from tierlist (Tony Gorez) [#43604](https://github.com/nodejs/node/pull/43604)
* \[[`22512427b3`](https://github.com/nodejs/node/commit/22512427b3)] - **doc**: remove dtrace from tierlist (Tony Gorez) [#43606](https://github.com/nodejs/node/pull/43606)
* \[[`a3659e3547`](https://github.com/nodejs/node/commit/a3659e3547)] - **doc**: promote 0x to tier 4 (Tony Gorez) [#43609](https://github.com/nodejs/node/pull/43609)
* \[[`6ede1c2162`](https://github.com/nodejs/node/commit/6ede1c2162)] - **doc**: include CVSS mention (Rafael Gonzaga) [#43602](https://github.com/nodejs/node/pull/43602)
* \[[`23c5de3579`](https://github.com/nodejs/node/commit/23c5de3579)] - **doc**: fix icu-small example (Michael Dawson) [#43591](https://github.com/nodejs/node/pull/43591)
* \[[`54a8a0c9c7`](https://github.com/nodejs/node/commit/54a8a0c9c7)] - **doc**: add `backport-open-vN.x` step to backporting guide (LiviaMedeiros) [#43590](https://github.com/nodejs/node/pull/43590)
* \[[`60b949d8ff`](https://github.com/nodejs/node/commit/60b949d8ff)] - **doc**: move MylesBorins to TSC Emeritus (Myles Borins) [#43524](https://github.com/nodejs/node/pull/43524)
* \[[`08ed28c31e`](https://github.com/nodejs/node/commit/08ed28c31e)] - **doc**: add Juan as a security steward (Michael Dawson) [#43512](https://github.com/nodejs/node/pull/43512)
* \[[`2e799bcd35`](https://github.com/nodejs/node/commit/2e799bcd35)] - **doc**: update link to MDN page about dynamic imports (James Scott-Brown) [#43530](https://github.com/nodejs/node/pull/43530)
* \[[`c8aafe2036`](https://github.com/nodejs/node/commit/c8aafe2036)] - **doc**: fix Visual Studio 2019 download link (Feng Yu) [#43236](https://github.com/nodejs/node/pull/43236)
* \[[`d0c78d21e0`](https://github.com/nodejs/node/commit/d0c78d21e0)] - **doc**: update link of `ICU data slicer` (Feng Yu) [#43483](https://github.com/nodejs/node/pull/43483)
* \[[`324728094c`](https://github.com/nodejs/node/commit/324728094c)] - **doc**: update v8 doc link to v8.dev (Feng Yu) [#43482](https://github.com/nodejs/node/pull/43482)
* \[[`b111331c9c`](https://github.com/nodejs/node/commit/b111331c9c)] - **doc**: add ESM version examples to events api doc (Feng Yu) [#43226](https://github.com/nodejs/node/pull/43226)
* \[[`038decfbc3`](https://github.com/nodejs/node/commit/038decfbc3)] - **doc**: update default branch name in `test/**` (Luigi Pinca) [#43445](https://github.com/nodejs/node/pull/43445)
* \[[`a23051af84`](https://github.com/nodejs/node/commit/a23051af84)] - **doc**: add new useful V8 option (JialuZhang-intel) [#42575](https://github.com/nodejs/node/pull/42575)
* \[[`7f406fd77b`](https://github.com/nodejs/node/commit/7f406fd77b)] - **doc**: remove branch name mention in `src/README.md` (Feng Yu) [#43442](https://github.com/nodejs/node/pull/43442)
* \[[`06fe60a6f9`](https://github.com/nodejs/node/commit/06fe60a6f9)] - **doc**: update default branch name in `Makefile` (Feng Yu) [#43441](https://github.com/nodejs/node/pull/43441)
* \[[`9d61da0aef`](https://github.com/nodejs/node/commit/9d61da0aef)] - **doc**: update main branch name in release guide (Richard Lau) [#43437](https://github.com/nodejs/node/pull/43437)
* \[[`739d3a35ed`](https://github.com/nodejs/node/commit/739d3a35ed)] - **doc**: update main branch name in onboarding.md (Feng Yu) [#43443](https://github.com/nodejs/node/pull/43443)
* \[[`e0fedcfb18`](https://github.com/nodejs/node/commit/e0fedcfb18)] - **doc**: fixup after rename of primary nodejs branch (Michael Dawson) [#43453](https://github.com/nodejs/node/pull/43453)
* \[[`429e0f433b`](https://github.com/nodejs/node/commit/429e0f433b)] - **doc**: update main branch name in doc/contributing/\* (Luigi Pinca) [#43438](https://github.com/nodejs/node/pull/43438)
* \[[`cbaf1207f4`](https://github.com/nodejs/node/commit/cbaf1207f4)] - **doc**: add code examples to node test runner (Wassim Chegham) [#43359](https://github.com/nodejs/node/pull/43359)
* \[[`462e526237`](https://github.com/nodejs/node/commit/462e526237)] - **doc,test**: clarify timingSafeEqual semantics (Tobias Nießen) [#43228](https://github.com/nodejs/node/pull/43228)

#### Other commits

* \[[`7ee0be71f9`](https://github.com/nodejs/node/commit/7ee0be71f9)] - **benchmark**: fix output regression (Brian White) [#43635](https://github.com/nodejs/node/pull/43635)
* \[[`d90a6f9bda`](https://github.com/nodejs/node/commit/d90a6f9bda)] - **benchmark**: fix fork detection (Paolo Insogna) [#43601](https://github.com/nodejs/node/pull/43601)
* \[[`f9c30abcdc`](https://github.com/nodejs/node/commit/f9c30abcdc)] - **benchmark**: forcefully close processes (Paolo Insogna) [#43557](https://github.com/nodejs/node/pull/43557)
* \[[`ebf962c053`](https://github.com/nodejs/node/commit/ebf962c053)] - **build**: enable GitPod prebuilds (Rich Trott) [#43698](https://github.com/nodejs/node/pull/43698)
* \[[`482bd53357`](https://github.com/nodejs/node/commit/482bd53357)] - **build**: clarify missing clang-format tool (Tobias Nießen) [#42762](https://github.com/nodejs/node/pull/42762)
* \[[`919c5ee5c2`](https://github.com/nodejs/node/commit/919c5ee5c2)] - **build**: update main branch name in GH workflow (Feng Yu) [#43481](https://github.com/nodejs/node/pull/43481)
* \[[`3b08dfdc5d`](https://github.com/nodejs/node/commit/3b08dfdc5d)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#43750](https://github.com/nodejs/node/pull/43750)
* \[[`508cbbcbf9`](https://github.com/nodejs/node/commit/508cbbcbf9)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#43660](https://github.com/nodejs/node/pull/43660)
* \[[`d650c9c6b0`](https://github.com/nodejs/node/commit/d650c9c6b0)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#43573](https://github.com/nodejs/node/pull/43573)
* \[[`b9204c9be8`](https://github.com/nodejs/node/commit/b9204c9be8)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#43480](https://github.com/nodejs/node/pull/43480)
* \[[`294f0ef4df`](https://github.com/nodejs/node/commit/294f0ef4df)] - **test**: mark test-net-connect-reset-until-connected flaky on freebsd (Feng Yu) [#43613](https://github.com/nodejs/node/pull/43613)
* \[[`f2f7d7b207`](https://github.com/nodejs/node/commit/f2f7d7b207)] - **test**: remove unnecessary .toString() calls in HTTP tests (Anna Henningsen) [#43731](https://github.com/nodejs/node/pull/43731)
* \[[`38e92fd88f`](https://github.com/nodejs/node/commit/38e92fd88f)] - **test**: mark test-gc-http-client-timeout as flaky on arm (Chengzhong Wu) [#43754](https://github.com/nodejs/node/pull/43754)
* \[[`b164848c55`](https://github.com/nodejs/node/commit/b164848c55)] - **test**: fix typo in file name (Antoine du Hamel) [#43764](https://github.com/nodejs/node/pull/43764)
* \[[`a0b799f645`](https://github.com/nodejs/node/commit/a0b799f645)] - **test**: add test for profile command of node inspect (Kohei Ueno) [#43058](https://github.com/nodejs/node/pull/43058)
* \[[`c4d88b3345`](https://github.com/nodejs/node/commit/c4d88b3345)] - **test**: use Object for `tests` variable in fs trace test (Feng Yu) [#43585](https://github.com/nodejs/node/pull/43585)
* \[[`c9f130e1cf`](https://github.com/nodejs/node/commit/c9f130e1cf)] - **test**: improve code coverage for performance\_entry (Kohei Ueno) [#43434](https://github.com/nodejs/node/pull/43434)
* \[[`0b4956079a`](https://github.com/nodejs/node/commit/0b4956079a)] - **test**: add test to ensure repl doesn't support --input-type (cola119) [#43507](https://github.com/nodejs/node/pull/43507)
* \[[`2adf4e7727`](https://github.com/nodejs/node/commit/2adf4e7727)] - **test**: fix flaky test-perf-hooks-histogram (Santiago Gimeno) [#43567](https://github.com/nodejs/node/pull/43567)
* \[[`043756d540`](https://github.com/nodejs/node/commit/043756d540)] - **test**: fill DOMException names (LiviaMedeiros) [#43615](https://github.com/nodejs/node/pull/43615)
* \[[`e718a6e913`](https://github.com/nodejs/node/commit/e718a6e913)] - **test**: fix Buffer.from(ArrayBufferView) call (LiviaMedeiros) [#43614](https://github.com/nodejs/node/pull/43614)
* \[[`ac72f4e812`](https://github.com/nodejs/node/commit/ac72f4e812)] - **test**: mark test-worker-http2-stream-terminate flaky on all platforms (Finn Yu) [#43620](https://github.com/nodejs/node/pull/43620)
* \[[`dabccef69f`](https://github.com/nodejs/node/commit/dabccef69f)] - **test**: mark flaky tests on smartos (Feng Yu) [#43596](https://github.com/nodejs/node/pull/43596)
* \[[`705e85e736`](https://github.com/nodejs/node/commit/705e85e736)] - **test**: improve code coverage for SourceMap class (italo jose) [#43285](https://github.com/nodejs/node/pull/43285)
* \[[`7c6f548382`](https://github.com/nodejs/node/commit/7c6f548382)] - **test**: mark test-http-server-request-timeouts-mixed flaky on macOS (F3n67u) [#43597](https://github.com/nodejs/node/pull/43597)
* \[[`bd91337988`](https://github.com/nodejs/node/commit/bd91337988)] - **test**: refactor to top-level await (Meek Simbule) [#43500](https://github.com/nodejs/node/pull/43500)
* \[[`9940dc38c1`](https://github.com/nodejs/node/commit/9940dc38c1)] - **test**: skip test-v8-serialize-leak on IBM i (Richard Lau) [#43511](https://github.com/nodejs/node/pull/43511)
* \[[`17b92f0679`](https://github.com/nodejs/node/commit/17b92f0679)] - **test**: use unique file names in fs trace test (Ben Noordhuis) [#43504](https://github.com/nodejs/node/pull/43504)
* \[[`7ca58b8ee7`](https://github.com/nodejs/node/commit/7ca58b8ee7)] - **test**: allow EOVERFLOW errors in fs position tests (Richard Lau) [#43510](https://github.com/nodejs/node/pull/43510)
* \[[`eece34cddb`](https://github.com/nodejs/node/commit/eece34cddb)] - **test**: add WPT tests for dom/events (Daiki Nishikawa) [#43151](https://github.com/nodejs/node/pull/43151)
* \[[`70d297c271`](https://github.com/nodejs/node/commit/70d297c271)] - **test**: replace gc(true) with gc({ type: 'minor' }) (Tobias Nießen) [#43493](https://github.com/nodejs/node/pull/43493)
* \[[`1022c0d0d4`](https://github.com/nodejs/node/commit/1022c0d0d4)] - **test**: fix flaky test-https-server-close- tests (Santiago Gimeno) [#43216](https://github.com/nodejs/node/pull/43216)
* \[[`a9ab41cb38`](https://github.com/nodejs/node/commit/a9ab41cb38)] - **test**: refactor to top-level await (Meek Simbule) [#43366](https://github.com/nodejs/node/pull/43366)
* \[[`b1a7798821`](https://github.com/nodejs/node/commit/b1a7798821)] - **test**: skip test-net-connect-reset-until-connected on SmartOS (Filip Skokan) [#43449](https://github.com/nodejs/node/pull/43449)
* \[[`3b0703fd0d`](https://github.com/nodejs/node/commit/3b0703fd0d)] - **test**: rename `test-eventtarget-whatwg-*.js` (Daeyeon Jeong) [#43467](https://github.com/nodejs/node/pull/43467)
* \[[`5c0a24d5be`](https://github.com/nodejs/node/commit/5c0a24d5be)] - **test**: mark test-worker-http2-stream-terminate flaky on Windows (Darshan Sen) [#43425](https://github.com/nodejs/node/pull/43425)
* \[[`48ae00c0b1`](https://github.com/nodejs/node/commit/48ae00c0b1)] - **test**: improve coverage for load hooks (Antoine du Hamel) [#43374](https://github.com/nodejs/node/pull/43374)
* \[[`2b55b606f7`](https://github.com/nodejs/node/commit/2b55b606f7)] - _**Revert**_ "**test**: mark test\_buffer/test\_finalizer flaky" (Matteo Collina) [#43418](https://github.com/nodejs/node/pull/43418)
* \[[`3948accbf4`](https://github.com/nodejs/node/commit/3948accbf4)] - **test**: make node-api/test\_buffer/test\_finalizer not flaky (Matteo Collina) [#43418](https://github.com/nodejs/node/pull/43418)
* \[[`c954bcd20b`](https://github.com/nodejs/node/commit/c954bcd20b)] - **tools**: remove rpm build scripts (Ben Noordhuis) [#43647](https://github.com/nodejs/node/pull/43647)
* \[[`8a06b7b9d0`](https://github.com/nodejs/node/commit/8a06b7b9d0)] - **tools**: update lint-md-dependencies to rollup\@2.76.0 (Node.js GitHub Bot) [#43749](https://github.com/nodejs/node/pull/43749)
* \[[`aafdf1239e`](https://github.com/nodejs/node/commit/aafdf1239e)] - **tools**: refactor `tools/license2rtf` to ESM (Feng Yu) [#43232](https://github.com/nodejs/node/pull/43232)
* \[[`99ffabf2dd`](https://github.com/nodejs/node/commit/99ffabf2dd)] - **tools**: update eslint to 8.19.0 (Node.js GitHub Bot) [#43662](https://github.com/nodejs/node/pull/43662)
* \[[`c6396c179f`](https://github.com/nodejs/node/commit/c6396c179f)] - **tools**: update lint-md-dependencies (Node.js GitHub Bot) [#43572](https://github.com/nodejs/node/pull/43572)
* \[[`8d14d6e215`](https://github.com/nodejs/node/commit/8d14d6e215)] - **tools**: fix CJS/ESM toggle on small screens (Antoine du Hamel) [#43506](https://github.com/nodejs/node/pull/43506)
* \[[`59d4da699e`](https://github.com/nodejs/node/commit/59d4da699e)] - **tools**: update eslint to 8.18.0 (Node.js GitHub Bot) [#43479](https://github.com/nodejs/node/pull/43479)
* \[[`752380a959`](https://github.com/nodejs/node/commit/752380a959)] - **tools**: update main branch name (Feng Yu) [#43440](https://github.com/nodejs/node/pull/43440)
* \[[`06c367ef8b`](https://github.com/nodejs/node/commit/06c367ef8b)] - **tools**: update lint-md-dependencies to rollup\@2.75.6 (Node.js GitHub Bot) [#43386](https://github.com/nodejs/node/pull/43386)

<a id="18.5.0"></a>

## 2022-07-07, Version 18.5.0 (Current), @RafaelGSS

This is a security release.

### Notable changes

The following CVEs are fixed in this release:

* **[CVE-2022-2097](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-2097)**: OpenSSL - AES OCB fails to encrypt some bytes (Medium)
* **[CVE-2022-32212](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-32212)**: DNS rebinding in --inspect via invalid IP addresses (High)
* **[CVE-2022-32213](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-32213)**: HTTP Request Smuggling - Flawed Parsing of Transfer-Encoding (Medium)
* **[CVE-2022-32214](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-32214)**: HTTP Request Smuggling - Improper Delimiting of Header Fields (Medium)
* **[CVE-2022-32215](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-32215)**: HTTP Request Smuggling - Incorrect Parsing of Multi-line Transfer-Encoding (Medium)
* **[CVE-2022-32222](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-32222)**: Attempt to read openssl.cnf from /home/iojs/build/ upon startup (Medium)
* **[CVE-2022-32223](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-32223)**: DLL Hijacking on Windows (High)

More detailed information on each of the vulnerabilities can be found in [July 7th 2022 Security Releases](https://nodejs.org/en/blog/vulnerability/july-2022-security-releases/) blog post.

#### llhttp updated to 6.0.7

`llhttp` is updated to 6.0.7 which includes fixes for the following vulnerabilities.

* **HTTP Request Smuggling - Flawed Parsing of Transfer-Encoding (Medium)([CVE-2022-32213](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-32214) )**: The `llhttp` parser in the `http` module does not correctly parse and validate Transfer-Encoding headers. This can lead to HTTP Request Smuggling (HRS).
* **HTTP Request Smuggling - Improper Delimiting of Header Fields (Medium)([CVE-2022-32214](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-32214))**: The `llhttp` parser in the `http` module does not strictly use the CRLF sequence to delimit HTTP requests. This can lead to HTTP Request Smuggling.
  * **Note**: This can be considered a breaking change due to disabling LF header delimiting. To enable LF header delimiting you can specify the `--insecure-http-parser` command-line flag, but note that this will additionally enable other insecure behaviours.
* **HTTP Request Smuggling - Incorrect Parsing of Multi-line Transfer-Encoding (Medium)([CVE-2022-32215](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-32215))**: The `llhttp` parser in the `http` module does not correctly handle multi-line Transfer-Encoding headers. This can lead to HTTP Request Smuggling (HRS).

Some of these fixes required breaking changes, so you may be impacted by this update.

#### Default OpenSSL Configuration

To resolve **[CVE-2022-32223](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-32223)**: DLL Hijacking on Windows (High), changes were made to how Node.js loads OpenSSL configuration by default.

**[CVE-2022-32223](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-32223)** could be exploited if the victim has the following dependencies on Windows machine:

* OpenSSL has been installed and `C:\Program Files\Common Files\SSL\openssl.cnf` exists.

Whenever the above conditions are present, `node.exe` will search for `providers.dll` in the current user directory. After that, `node.exe` will try to search for `providers.dll` by the DLL Search Order in Windows. An attacker can place the malicious file `providers.dll` under a variety of paths to exploit this vulnerability.

Node.js can use an OpenSSL configuration file by specifying the environment variable `OPENSSL_CONF`, or using the command-line option `--openssl-conf`, and if none of those are specified will default to reading the default OpenSSL configuration file `openssl.cnf`.

From this release, Node.js will only read a section that is, by default, named `nodejs_conf`. If you were previously relying on the configuration specified in the shared section of the `openssl.cnf` file, you may be affected by this change. You can fall back to the previous behavior to read the default section by:

* Specifying the `--openssl-shared-config` command-line flag; or
* Creating a new `nodejs_conf` section in that file and copying the contents of the default section into the new `nodejs_conf` section.

Note that when specifying `--openssl-shared-config` or defining `nodejs_conf` in your `openssl.cnf`, you should be cautious and review your configuration as it could lead to you being vulnerable to similar DLL exploit attacks.

### Commits

* \[[`dc7af13486`](https://github.com/nodejs/node/commit/dc7af13486)] - **deps**: update archs files for quictls/openssl-3.0.5+quic (RafaelGSS) [#43693](https://github.com/nodejs/node/pull/43693)
* \[[`fa72c534eb`](https://github.com/nodejs/node/commit/fa72c534eb)] - **deps**: upgrade openssl sources to quictls/openssl-3.0.5+quic (RafaelGSS) [#43693](https://github.com/nodejs/node/pull/43693)
* \[[`a5fc2deb43`](https://github.com/nodejs/node/commit/a5fc2deb43)] - **deps**: update default openssl.cnf directory (Michael Dawson) [nodejs-private/node-private#335](https://github.com/nodejs-private/node-private/pull/335)
* \[[`f2407748e3`](https://github.com/nodejs/node/commit/f2407748e3)] - **(SEMVER-MAJOR)** **http**: stricter Transfer-Encoding and header separator parsing (Paolo Insogna) [nodejs-private/node-private#315](https://github.com/nodejs-private/node-private/pull/315)
* \[[`e4af5eba95`](https://github.com/nodejs/node/commit/e4af5eba95)] - **src**: fix IPv4 validation in inspector\_socket (Tobias Nießen) [nodejs-private/node-private#320](https://github.com/nodejs-private/node-private/pull/320)
* \[[`3f0c3e142d`](https://github.com/nodejs/node/commit/3f0c3e142d)] - **(SEMVER-MAJOR)** **src,deps,build,test**: add OpenSSL config appname (Daniel Bevenius) [#43124](https://github.com/nodejs/node/pull/43124)
* \[[`9578158ff8`](https://github.com/nodejs/node/commit/9578158ff8)] - **(SEMVER-MINOR)** **src,doc,test**: add --openssl-shared-config option (Daniel Bevenius) [#43124](https://github.com/nodejs/node/pull/43124)

<a id="18.4.0"></a>

## 2022-06-16, Version 18.4.0 (Current), @danielleadams

### Notable Changes

* **crypto**:
  * remove Node.js-specific webcrypto extensions (Filip Skokan) [#43310](https://github.com/nodejs/node/pull/43310)
  * add CFRG curves to Web Crypto API (Filip Skokan) [#42507](https://github.com/nodejs/node/pull/42507)
* **dns**:
  * accept `'IPv4'` and `'IPv6'` for `family` (Antoine du Hamel) [#43054](https://github.com/nodejs/node/pull/43054)
* **report**:
  * add more heap infos in process report (theanarkh) [#43116](https://github.com/nodejs/node/pull/43116)

### Commits

* \[[`702bfa0b7c`](https://github.com/nodejs/node/commit/702bfa0b7c)] - **async\_hooks**: use `kEmptyObject` (LiviaMedeiros) [#43159](https://github.com/nodejs/node/pull/43159)
* \[[`f7c4015fd8`](https://github.com/nodejs/node/commit/f7c4015fd8)] - **bootstrap**: consolidate global properties definition (Chengzhong Wu) [#43357](https://github.com/nodejs/node/pull/43357)
* \[[`8d892f5259`](https://github.com/nodejs/node/commit/8d892f5259)] - **build**: add nonpm and nocorepack to vcbuild.bat (Darshan Sen) [#43219](https://github.com/nodejs/node/pull/43219)
* \[[`4109ddc005`](https://github.com/nodejs/node/commit/4109ddc005)] - **child\_process**: use `kEmptyObject` (LiviaMedeiros) [#43159](https://github.com/nodejs/node/pull/43159)
* \[[`7b5cb14f0c`](https://github.com/nodejs/node/commit/7b5cb14f0c)] - **cluster**: use `kEmptyObject` (LiviaMedeiros) [#43159](https://github.com/nodejs/node/pull/43159)
* \[[`9f1de2c005`](https://github.com/nodejs/node/commit/9f1de2c005)] - **crypto**: fix webcrypto import of cfrg raw public keys (Filip Skokan) [#43404](https://github.com/nodejs/node/pull/43404)
* \[[`7f02e22998`](https://github.com/nodejs/node/commit/7f02e22998)] - **crypto**: test webcrypto ec raw public key import (Filip Skokan) [#43405](https://github.com/nodejs/node/pull/43405)
* \[[`0a075cb548`](https://github.com/nodejs/node/commit/0a075cb548)] - **crypto**: fix webcrypto JWK EC and OKP import crv check (Filip Skokan) [#43346](https://github.com/nodejs/node/pull/43346)
* \[[`df0903c8e8`](https://github.com/nodejs/node/commit/df0903c8e8)] - **crypto**: use `kEmptyObject` (LiviaMedeiros) [#43159](https://github.com/nodejs/node/pull/43159)
* \[[`6d0053345e`](https://github.com/nodejs/node/commit/6d0053345e)] - **(SEMVER-MINOR)** **crypto**: remove Node.js-specific webcrypto extensions (Filip Skokan) [#43310](https://github.com/nodejs/node/pull/43310)
* \[[`28c034d6b5`](https://github.com/nodejs/node/commit/28c034d6b5)] - **(SEMVER-MINOR)** **crypto**: add CFRG curves to Web Crypto API (Filip Skokan) [#42507](https://github.com/nodejs/node/pull/42507)
* \[[`fe7fd85109`](https://github.com/nodejs/node/commit/fe7fd85109)] - **deps**: update Corepack to 0.11.2 (Maël Nison) [#43402](https://github.com/nodejs/node/pull/43402)
* \[[`517f17b214`](https://github.com/nodejs/node/commit/517f17b214)] - **deps**: update undici to 5.5.1 (Node.js GitHub Bot) [#43412](https://github.com/nodejs/node/pull/43412)
* \[[`f4c830fbe4`](https://github.com/nodejs/node/commit/f4c830fbe4)] - **deps**: upgrade npm to 8.12.1 (npm CLI robot) [#43301](https://github.com/nodejs/node/pull/43301)
* \[[`0bb84b09a5`](https://github.com/nodejs/node/commit/0bb84b09a5)] - **(SEMVER-MINOR)** **dns**: accept `'IPv4'` and `'IPv6'` for `family` (Antoine du Hamel) [#43054](https://github.com/nodejs/node/pull/43054)
* \[[`f91babe494`](https://github.com/nodejs/node/commit/f91babe494)] - **doc**: packages documentation updates for 12 EOL (Guy Bedford) [#43375](https://github.com/nodejs/node/pull/43375)
* \[[`066f963ec1`](https://github.com/nodejs/node/commit/066f963ec1)] - **doc**: add initial doc on how to update cjs-module-lexer (Michael Dawson) [#43255](https://github.com/nodejs/node/pull/43255)
* \[[`36e5684ae0`](https://github.com/nodejs/node/commit/36e5684ae0)] - **doc**: clarify use of deps/icu-small (Michael Dawson) [#43287](https://github.com/nodejs/node/pull/43287)
* \[[`b9634e7ef3`](https://github.com/nodejs/node/commit/b9634e7ef3)] - **doc**: remove llnode from diag tierlist (Tony Gorez) [#43289](https://github.com/nodejs/node/pull/43289)
* \[[`4caeb10e7b`](https://github.com/nodejs/node/commit/4caeb10e7b)] - **doc**: remove ETW from diag tierlist (Tony Gorez) [#43295](https://github.com/nodejs/node/pull/43295)
* \[[`41955e5ce5`](https://github.com/nodejs/node/commit/41955e5ce5)] - **doc**: use serial comma in report docs (Tobias Nießen) [#43394](https://github.com/nodejs/node/pull/43394)
* \[[`e30d4c1cb0`](https://github.com/nodejs/node/commit/e30d4c1cb0)] - **doc**: add fspromises mkdir example (Tierney Cyren) [#40843](https://github.com/nodejs/node/pull/40843)
* \[[`adec5fa929`](https://github.com/nodejs/node/commit/adec5fa929)] - **doc**: add F3n67u to triagers (Feng Yu) [#43350](https://github.com/nodejs/node/pull/43350)
* \[[`cc3505b192`](https://github.com/nodejs/node/commit/cc3505b192)] - **doc**: fix typo in globals.md (Daeyeon Jeong) [#43365](https://github.com/nodejs/node/pull/43365)
* \[[`052c8eaf6a`](https://github.com/nodejs/node/commit/052c8eaf6a)] - **doc**: use serial comma in webstreams docs (Tobias Nießen) [#43353](https://github.com/nodejs/node/pull/43353)
* \[[`b824a0b7d0`](https://github.com/nodejs/node/commit/b824a0b7d0)] - **doc**: fix specifier example in `esm.md` (hiroki osame) [#43351](https://github.com/nodejs/node/pull/43351)
* \[[`d558b3c028`](https://github.com/nodejs/node/commit/d558b3c028)] - **doc**: add undici to glossary (F3n67u) [#43327](https://github.com/nodejs/node/pull/43327)
* \[[`f9ad98f5cb`](https://github.com/nodejs/node/commit/f9ad98f5cb)] - **doc**: change glossary link in pull request guide to node's glossary doc (Feng Yu) [#43318](https://github.com/nodejs/node/pull/43318)
* \[[`02944a6783`](https://github.com/nodejs/node/commit/02944a6783)] - **doc**: fix typo in util.parseArgs usage example (Michael Ficarra) [#43332](https://github.com/nodejs/node/pull/43332)
* \[[`f2bc6a3f71`](https://github.com/nodejs/node/commit/f2bc6a3f71)] - **doc**: improve description of TZ (Tobias Nießen) [#43334](https://github.com/nodejs/node/pull/43334)
* \[[`9335ea6c35`](https://github.com/nodejs/node/commit/9335ea6c35)] - **doc**: use serial comma in net docs (Tobias Nießen) [#43335](https://github.com/nodejs/node/pull/43335)
* \[[`05f38c6c3e`](https://github.com/nodejs/node/commit/05f38c6c3e)] - **doc**: make clear the result of comparison between Symbol.for (Kohei Ueno) [#43309](https://github.com/nodejs/node/pull/43309)
* \[[`c9aed9de9f`](https://github.com/nodejs/node/commit/c9aed9de9f)] - **doc**: add missing require to stream api doc (Feng Yu) [#43237](https://github.com/nodejs/node/pull/43237)
* \[[`f3188c1c9c`](https://github.com/nodejs/node/commit/f3188c1c9c)] - **doc**: add CIGTM to `glossary.md` (Feng Yu) [#43316](https://github.com/nodejs/node/pull/43316)
* \[[`c572d2d115`](https://github.com/nodejs/node/commit/c572d2d115)] - **doc**: use serial comma in pull request doc (Feng Yu) [#43319](https://github.com/nodejs/node/pull/43319)
* \[[`8a4e1fa002`](https://github.com/nodejs/node/commit/8a4e1fa002)] - **doc**: use serial comma in ESM docs (Tobias Nießen) [#43322](https://github.com/nodejs/node/pull/43322)
* \[[`fff0560a66`](https://github.com/nodejs/node/commit/fff0560a66)] - **doc**: promote cdt to tier 3 (Tony Gorez) [#43290](https://github.com/nodejs/node/pull/43290)
* \[[`7d0f6da97f`](https://github.com/nodejs/node/commit/7d0f6da97f)] - **doc**: fix chromium document link in pull-requests.md (rikapo) [#43265](https://github.com/nodejs/node/pull/43265)
* \[[`4674b0d2a5`](https://github.com/nodejs/node/commit/4674b0d2a5)] - **doc**: fix 404 link of BUILDING.md (Feng Yu) [#43234](https://github.com/nodejs/node/pull/43234)
* \[[`ee392c5c0b`](https://github.com/nodejs/node/commit/ee392c5c0b)] - **doc**: update CHANGELOG\_V18.md (Filip Skokan) [#43298](https://github.com/nodejs/node/pull/43298)
* \[[`5a3a2a197f`](https://github.com/nodejs/node/commit/5a3a2a197f)] - **doc**: add src/crypto to CC list for nodejs/crypto (Tobias Nießen) [#43286](https://github.com/nodejs/node/pull/43286)
* \[[`69ce50396c`](https://github.com/nodejs/node/commit/69ce50396c)] - **doc**: use serial comma in console docs (Tobias Nießen) [#43257](https://github.com/nodejs/node/pull/43257)
* \[[`0c5092c51c`](https://github.com/nodejs/node/commit/0c5092c51c)] - **events**: fix adding abort listener in `events.once` (Daeyeon Jeong) [#43373](https://github.com/nodejs/node/pull/43373)
* \[[`fda2105481`](https://github.com/nodejs/node/commit/fda2105481)] - **events**: use `kEmptyObject` (LiviaMedeiros) [#43159](https://github.com/nodejs/node/pull/43159)
* \[[`63bf49b143`](https://github.com/nodejs/node/commit/63bf49b143)] - **fs**: use `kEmptyObject` (LiviaMedeiros) [#43159](https://github.com/nodejs/node/pull/43159)
* \[[`9b764531b9`](https://github.com/nodejs/node/commit/9b764531b9)] - **fs**: export constants from `fs/promises` (Feng Yu) [#43177](https://github.com/nodejs/node/pull/43177)
* \[[`a4409f85f8`](https://github.com/nodejs/node/commit/a4409f85f8)] - **http**: use `kEmptyObject` (LiviaMedeiros) [#43159](https://github.com/nodejs/node/pull/43159)
* \[[`2ffd54105a`](https://github.com/nodejs/node/commit/2ffd54105a)] - **http2**: use `kEmptyObject` (LiviaMedeiros) [#43159](https://github.com/nodejs/node/pull/43159)
* \[[`b468b8fe51`](https://github.com/nodejs/node/commit/b468b8fe51)] - **https**: use `kEmptyObject` (LiviaMedeiros) [#43159](https://github.com/nodejs/node/pull/43159)
* \[[`d2a98dc6cf`](https://github.com/nodejs/node/commit/d2a98dc6cf)] - **inspector**: add missing initialization (Michael Dawson) [#43254](https://github.com/nodejs/node/pull/43254)
* \[[`3b2f7eed39`](https://github.com/nodejs/node/commit/3b2f7eed39)] - **lib**: use `kEmptyObject` in various places (LiviaMedeiros) [#43159](https://github.com/nodejs/node/pull/43159)
* \[[`4a9511d971`](https://github.com/nodejs/node/commit/4a9511d971)] - **lib**: give names to promisified methods (LiviaMedeiros) [#43218](https://github.com/nodejs/node/pull/43218)
* \[[`b8644606eb`](https://github.com/nodejs/node/commit/b8644606eb)] - **lib**: use null-prototype objects for property descriptors (Antoine du Hamel) [#43270](https://github.com/nodejs/node/pull/43270)
* \[[`64edd6cbc3`](https://github.com/nodejs/node/commit/64edd6cbc3)] - **meta**: move one or more collaborators to emeritus (Node.js GitHub Bot) [#43399](https://github.com/nodejs/node/pull/43399)
* \[[`b05cea57ba`](https://github.com/nodejs/node/commit/b05cea57ba)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#43387](https://github.com/nodejs/node/pull/43387)
* \[[`a8ecec57e3`](https://github.com/nodejs/node/commit/a8ecec57e3)] - **meta**: move one or more collaborators to emeritus (#43183) (Node.js GitHub Bot) [#43183](https://github.com/nodejs/node/pull/43183)
* \[[`60dc36244a`](https://github.com/nodejs/node/commit/60dc36244a)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#43312](https://github.com/nodejs/node/pull/43312)
* \[[`9803b82ac7`](https://github.com/nodejs/node/commit/9803b82ac7)] - **net,dns**: move hasObserver out of perf function (theanarkh) [#43217](https://github.com/nodejs/node/pull/43217)
* \[[`112518fb1d`](https://github.com/nodejs/node/commit/112518fb1d)] - **perf\_hooks**: fix function wrapped by `timerify` to work correctly (Kohei Ueno) [#43330](https://github.com/nodejs/node/pull/43330)
* \[[`a3310d13bf`](https://github.com/nodejs/node/commit/a3310d13bf)] - **perf\_hooks**: use `kEmptyObject` (LiviaMedeiros) [#43159](https://github.com/nodejs/node/pull/43159)
* \[[`7e8a00a26d`](https://github.com/nodejs/node/commit/7e8a00a26d)] - **readline**: fix question stack overflow (Eugene Chapko) [#43320](https://github.com/nodejs/node/pull/43320)
* \[[`5e98cacf77`](https://github.com/nodejs/node/commit/5e98cacf77)] - **readline**: use `kEmptyObject` (LiviaMedeiros) [#43159](https://github.com/nodejs/node/pull/43159)
* \[[`66d956ca49`](https://github.com/nodejs/node/commit/66d956ca49)] - **repl**: make autocomplete case-insensitive (Sergey Petushkov) [#41632](https://github.com/nodejs/node/pull/41632)
* \[[`201f3d7f56`](https://github.com/nodejs/node/commit/201f3d7f56)] - **(SEMVER-MINOR)** **report**: add more heap infos in process report (theanarkh) [#43116](https://github.com/nodejs/node/pull/43116)
* \[[`a0568409b6`](https://github.com/nodejs/node/commit/a0568409b6)] - **src**: fix json utils escapes for U+000B (Chengzhong Wu) [#43206](https://github.com/nodejs/node/pull/43206)
* \[[`931ecfa033`](https://github.com/nodejs/node/commit/931ecfa033)] - **src**: fix memory leaks and refactor `ByteSource` (Tobias Nießen) [#43202](https://github.com/nodejs/node/pull/43202)
* \[[`5e65c1f3da`](https://github.com/nodejs/node/commit/5e65c1f3da)] - **src**: convey potential exceptions during StreamPipe construction (Darshan Sen) [#43240](https://github.com/nodejs/node/pull/43240)
* \[[`b200a5ff67`](https://github.com/nodejs/node/commit/b200a5ff67)] - **stream**: use `kEmptyObject` (LiviaMedeiros) [#43159](https://github.com/nodejs/node/pull/43159)
* \[[`1cc1a57cdb`](https://github.com/nodejs/node/commit/1cc1a57cdb)] - **test**: remove unused argument in test-util-inspect.js (Colin Ihrig) [#43395](https://github.com/nodejs/node/pull/43395)
* \[[`42c2115a82`](https://github.com/nodejs/node/commit/42c2115a82)] - **test**: mark test\_buffer/test\_finalizer flaky (Michael Dawson) [#43414](https://github.com/nodejs/node/pull/43414)
* \[[`71802c32d0`](https://github.com/nodejs/node/commit/71802c32d0)] - **test**: fix address in use error (Caleb Everett) [#43199](https://github.com/nodejs/node/pull/43199)
* \[[`e1b8c85a7a`](https://github.com/nodejs/node/commit/e1b8c85a7a)] - **test**: add test for short-option followed by its value (Kohei Ueno) [#43358](https://github.com/nodejs/node/pull/43358)
* \[[`f8d26c6011`](https://github.com/nodejs/node/commit/f8d26c6011)] - **test**: fix `common.mustNotCall` error message (Antoine du Hamel) [#42917](https://github.com/nodejs/node/pull/42917)
* \[[`18fffe6108`](https://github.com/nodejs/node/commit/18fffe6108)] - **test**: convert then to async/await (Meek Simbule) [#43292](https://github.com/nodejs/node/pull/43292)
* \[[`acd96d80eb`](https://github.com/nodejs/node/commit/acd96d80eb)] - **test**: add `BigInt`s to `common.getArrayBufferViews()` (LiviaMedeiros) [#43235](https://github.com/nodejs/node/pull/43235)
* \[[`e576a7fa50`](https://github.com/nodejs/node/commit/e576a7fa50)] - **test\_runner**: use `kEmptyObject` (LiviaMedeiros) [#43159](https://github.com/nodejs/node/pull/43159)
* \[[`fecad7a3a5`](https://github.com/nodejs/node/commit/fecad7a3a5)] - **timers**: use `kEmptyObject` (LiviaMedeiros) [#43159](https://github.com/nodejs/node/pull/43159)
* \[[`e31baca76a`](https://github.com/nodejs/node/commit/e31baca76a)] - **tls**: use `kEmptyObject` (LiviaMedeiros) [#43159](https://github.com/nodejs/node/pull/43159)
* \[[`7f8f61a749`](https://github.com/nodejs/node/commit/7f8f61a749)] - **tls**: fix performance regression in `convertALPNProtocols()` (LiviaMedeiros) [#43250](https://github.com/nodejs/node/pull/43250)
* \[[`ac9599a718`](https://github.com/nodejs/node/commit/ac9599a718)] - **tools**: report unsafe string and regex primordials as lint errors (Antoine du Hamel) [#43393](https://github.com/nodejs/node/pull/43393)
* \[[`b69d874592`](https://github.com/nodejs/node/commit/b69d874592)] - **tools**: fix `create-or-update-pull-request-action` hash on GHA (Antoine du Hamel) [#43378](https://github.com/nodejs/node/pull/43378)
* \[[`cf8a115983`](https://github.com/nodejs/node/commit/cf8a115983)] - **tools**: add `avoid-prototype-pollution` lint rule (Antoine du Hamel) [#43308](https://github.com/nodejs/node/pull/43308)
* \[[`8c0fe1e184`](https://github.com/nodejs/node/commit/8c0fe1e184)] - **tools**: fix find-inactive actions (LiviaMedeiros) [#43377](https://github.com/nodejs/node/pull/43377)
* \[[`7f45d69f83`](https://github.com/nodejs/node/commit/7f45d69f83)] - **tools**: update lint-md-dependencies to rollup\@2.75.5 (Node.js GitHub Bot) [#43313](https://github.com/nodejs/node/pull/43313)
* \[[`d5d0f01c5a`](https://github.com/nodejs/node/commit/d5d0f01c5a)] - **tools**: update eslint to 8.17.0 (Node.js GitHub Bot) [#43314](https://github.com/nodejs/node/pull/43314)
* \[[`f598fe1585`](https://github.com/nodejs/node/commit/f598fe1585)] - **tools**: use hashes instead of tags for external actions (#43284) (Antoine du Hamel) [#43284](https://github.com/nodejs/node/pull/43284)
* \[[`10f79947d9`](https://github.com/nodejs/node/commit/10f79947d9)] - **tools**: update `codecov/codecov-action` version (Antoine du Hamel) [#43297](https://github.com/nodejs/node/pull/43297)
* \[[`f93848fa50`](https://github.com/nodejs/node/commit/f93848fa50)] - **tools**: update lint-md-dependencies to rollup\@2.75.3 (Node.js GitHub Bot) [#43261](https://github.com/nodejs/node/pull/43261)
* \[[`b3d7dc1de8`](https://github.com/nodejs/node/commit/b3d7dc1de8)] - **tools**: update clang-format 1.7.0 to 1.8.0 (Darshan Sen) [#43241](https://github.com/nodejs/node/pull/43241)
* \[[`812140c65a`](https://github.com/nodejs/node/commit/812140c65a)] - **tools,doc**: add guards against prototype pollution when creating proxies (Antoine du Hamel) [#43391](https://github.com/nodejs/node/pull/43391)
* \[[`56b8cc5cef`](https://github.com/nodejs/node/commit/56b8cc5cef)] - **util**: freeze `kEnumerableProperty` (LiviaMedeiros) [#43390](https://github.com/nodejs/node/pull/43390)
* \[[`b187d55b6d`](https://github.com/nodejs/node/commit/b187d55b6d)] - **util**: add `kEmptyObject` to internal/util (LiviaMedeiros) [#43159](https://github.com/nodejs/node/pull/43159)
* \[[`024b396275`](https://github.com/nodejs/node/commit/024b396275)] - **vm**: use `kEmptyObject` (LiviaMedeiros) [#43159](https://github.com/nodejs/node/pull/43159)
* \[[`7fc432fa35`](https://github.com/nodejs/node/commit/7fc432fa35)] - **wasi**: use `kEmptyObject` (LiviaMedeiros) [#43159](https://github.com/nodejs/node/pull/43159)
* \[[`44b65d0ca7`](https://github.com/nodejs/node/commit/44b65d0ca7)] - **worker**: use `kEmptyObject` (LiviaMedeiros) [#43159](https://github.com/nodejs/node/pull/43159)

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
