# Node.js 21 ChangeLog

<!--lint disable maximum-line-length no-literal-urls prohibited-strings-->

<table>
<tr>
<th>Current</th>
</tr>
<tr>
<td>
<a href="#21.1.0">21.1.0</a><br/>
<a href="#21.0.0">21.0.0</a><br/>
</td>
</tr>
</table>

* Other Versions
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

<a id="21.1.0"></a>

## 2023-10-24, Version 21.1.0 (Current), @targos

### Notable Changes

#### Automatically detect and run ESM syntax

The new flag `--experimental-detect-module` can be used to automatically run
ES modules when their syntax can be detected. For “ambiguous” files, which are
`.js` or extensionless files with no `package.json` with a `type` field, Node.js
will parse the file to detect ES module syntax; if found, it will run the file
as an ES module, otherwise it will run the file as a CommonJS module.
The same applies to string input via `--eval` or `STDIN`.

We hope to make detection enabled by default in a future version of Node.js.
Detection increases startup time, so we encourage everyone — especially package
authors — to add a `type` field to `package.json`, even for the default
`"type": "commonjs"`. The presence of a `type` field, or explicit extensions
such as `.mjs` or `.cjs`, will opt out of detection.

Contributed by Geoffrey Booth in [#50096](https://github.com/nodejs/node/pull/50096).

### vm: fix V8 compilation cache support for vm.Script

Previously repeated compilation of the same source code using `vm.Script`
stopped hitting the V8 compilation cache after v16.x when support for
`importModuleDynamically` was added to `vm.Script`, resulting in a performance
regression that blocked users (in particular Jest users) from upgrading from
v16.x.

The recent fixes landed in v21.1.0 allow the compilation cache to be hit again
for `vm.Script` when `--experimental-vm-modules` is not used even in the
presence of the `importModuleDynamically` option, so that users affected by the
performance regression can now upgrade. Ongoing work is also being done to
enable compilation cache support for `vm.CompileFunction`.

Contributed by Joyee Cheung in [#50137](https://github.com/nodejs/node/pull/50137).

#### Other Notable Changes

* \[[`3729e33358`](https://github.com/nodejs/node/commit/3729e33358)] - **doc**: add H4ad to collaborators (Vinícius Lourenço) [#50217](https://github.com/nodejs/node/pull/50217)
* \[[`18862e4d5d`](https://github.com/nodejs/node/commit/18862e4d5d)] - **(SEMVER-MINOR)** **fs**: add `flush` option to `appendFile()` functions (Colin Ihrig) [#50095](https://github.com/nodejs/node/pull/50095)
* \[[`5a52c518ef`](https://github.com/nodejs/node/commit/5a52c518ef)] - **(SEMVER-MINOR)** **lib**: add `navigator.userAgent` (Yagiz Nizipli) [#50200](https://github.com/nodejs/node/pull/50200)
* \[[`789372a072`](https://github.com/nodejs/node/commit/789372a072)] - **(SEMVER-MINOR)** **stream**: allow pass stream class to `stream.compose` (Alex Yang) [#50187](https://github.com/nodejs/node/pull/50187)
* \[[`f3a9ea0bc4`](https://github.com/nodejs/node/commit/f3a9ea0bc4)] - **stream**: improve performance of readable stream reads (Raz Luvaton) [#50173](https://github.com/nodejs/node/pull/50173)

### Commits

* \[[`9cd68b9083`](https://github.com/nodejs/node/commit/9cd68b9083)] - **buffer**: remove unnecessary assignment in fromString (Tobias Nießen) [#50199](https://github.com/nodejs/node/pull/50199)
* \[[`a362c276ec`](https://github.com/nodejs/node/commit/a362c276ec)] - **crypto**: ensure valid point on elliptic curve in SubtleCrypto.importKey (Filip Skokan) [#50234](https://github.com/nodejs/node/pull/50234)
* \[[`f4da308f8d`](https://github.com/nodejs/node/commit/f4da308f8d)] - **deps**: V8: cherry-pick f7d000a7ae7b (Luke Albao) [#50302](https://github.com/nodejs/node/pull/50302)
* \[[`269e268c38`](https://github.com/nodejs/node/commit/269e268c38)] - **deps**: update ada to 2.7.2 (Node.js GitHub Bot) [#50338](https://github.com/nodejs/node/pull/50338)
* \[[`03a31ce41e`](https://github.com/nodejs/node/commit/03a31ce41e)] - **deps**: update corepack to 0.22.0 (Node.js GitHub Bot) [#50325](https://github.com/nodejs/node/pull/50325)
* \[[`000531781b`](https://github.com/nodejs/node/commit/000531781b)] - **deps**: update undici to 5.26.4 (Node.js GitHub Bot) [#50274](https://github.com/nodejs/node/pull/50274)
* \[[`f050668c14`](https://github.com/nodejs/node/commit/f050668c14)] - **deps**: update c-ares to 1.20.1 (Node.js GitHub Bot) [#50082](https://github.com/nodejs/node/pull/50082)
* \[[`ba258b682b`](https://github.com/nodejs/node/commit/ba258b682b)] - **deps**: update c-ares to 1.20.0 (Node.js GitHub Bot) [#50082](https://github.com/nodejs/node/pull/50082)
* \[[`571f7ef1fa`](https://github.com/nodejs/node/commit/571f7ef1fa)] - **deps**: patch V8 to 11.8.172.15 (Michaël Zasso) [#50114](https://github.com/nodejs/node/pull/50114)
* \[[`943047e800`](https://github.com/nodejs/node/commit/943047e800)] - **deps**: V8: cherry-pick 25902244ad1a (Joyee Cheung) [#50156](https://github.com/nodejs/node/pull/50156)
* \[[`db2a1cf1cb`](https://github.com/nodejs/node/commit/db2a1cf1cb)] - **doc**: fix `navigator.hardwareConcurrency` example (Tobias Nießen) [#50278](https://github.com/nodejs/node/pull/50278)
* \[[`6e537aeb44`](https://github.com/nodejs/node/commit/6e537aeb44)] - **doc**: explain how to disable navigator (Geoffrey Booth) [#50310](https://github.com/nodejs/node/pull/50310)
* \[[`c40de82d62`](https://github.com/nodejs/node/commit/c40de82d62)] - **doc**: add loong64 info into platform list (Shi Pujin) [#50086](https://github.com/nodejs/node/pull/50086)
* \[[`1c21a1880b`](https://github.com/nodejs/node/commit/1c21a1880b)] - **doc**: update release process LTS step (Richard Lau) [#50299](https://github.com/nodejs/node/pull/50299)
* \[[`2473aa3672`](https://github.com/nodejs/node/commit/2473aa3672)] - **doc**: fix release process table of contents (Richard Lau) [#50216](https://github.com/nodejs/node/pull/50216)
* \[[`ce9d84eae3`](https://github.com/nodejs/node/commit/ce9d84eae3)] - **doc**: update api `stream.compose` (Alex Yang) [#50206](https://github.com/nodejs/node/pull/50206)
* \[[`dacee4d9b5`](https://github.com/nodejs/node/commit/dacee4d9b5)] - **doc**: add ReflectConstruct to known perf issues (Vinicius Lourenço) [#50111](https://github.com/nodejs/node/pull/50111)
* \[[`82363be2ac`](https://github.com/nodejs/node/commit/82363be2ac)] - **doc**: fix typo in dgram docs (Peter Johnson) [#50211](https://github.com/nodejs/node/pull/50211)
* \[[`8c1a46c751`](https://github.com/nodejs/node/commit/8c1a46c751)] - **doc**: fix H4ad collaborator sort (Vinicius Lourenço) [#50218](https://github.com/nodejs/node/pull/50218)
* \[[`3729e33358`](https://github.com/nodejs/node/commit/3729e33358)] - **doc**: add H4ad to collaborators (Vinícius Lourenço) [#50217](https://github.com/nodejs/node/pull/50217)
* \[[`bac872cbd0`](https://github.com/nodejs/node/commit/bac872cbd0)] - **doc**: update release-stewards with last sec-release (Rafael Gonzaga) [#50179](https://github.com/nodejs/node/pull/50179)
* \[[`06b7724f14`](https://github.com/nodejs/node/commit/06b7724f14)] - **doc**: add command to keep major branch sync (Rafael Gonzaga) [#50102](https://github.com/nodejs/node/pull/50102)
* \[[`47633ab086`](https://github.com/nodejs/node/commit/47633ab086)] - **doc**: add loong64 to list of architectures (Shi Pujin) [#50172](https://github.com/nodejs/node/pull/50172)
* \[[`1f40ca1b91`](https://github.com/nodejs/node/commit/1f40ca1b91)] - **doc**: update security release process (Michael Dawson) [#50166](https://github.com/nodejs/node/pull/50166)
* \[[`998feda118`](https://github.com/nodejs/node/commit/998feda118)] - **esm**: do not give wrong hints when detecting file format (Antoine du Hamel) [#50314](https://github.com/nodejs/node/pull/50314)
* \[[`e375063e01`](https://github.com/nodejs/node/commit/e375063e01)] - **(SEMVER-MINOR)** **esm**: detect ESM syntax in ambiguous JavaScript (Geoffrey Booth) [#50096](https://github.com/nodejs/node/pull/50096)
* \[[`c76eb27971`](https://github.com/nodejs/node/commit/c76eb27971)] - **esm**: improve check for ESM syntax (Geoffrey Booth) [#50127](https://github.com/nodejs/node/pull/50127)
* \[[`7740bf820c`](https://github.com/nodejs/node/commit/7740bf820c)] - **esm**: rename error code related to import attributes (Antoine du Hamel) [#50181](https://github.com/nodejs/node/pull/50181)
* \[[`0cc176ef25`](https://github.com/nodejs/node/commit/0cc176ef25)] - **fs**: improve error performance for `readSync` (Jungku Lee) [#50033](https://github.com/nodejs/node/pull/50033)
* \[[`5942edb774`](https://github.com/nodejs/node/commit/5942edb774)] - **fs**: improve error performance for `fsyncSync` (Jungku Lee) [#49880](https://github.com/nodejs/node/pull/49880)
* \[[`6ec5abadc0`](https://github.com/nodejs/node/commit/6ec5abadc0)] - **fs**: improve error performance for `mkdirSync` (CanadaHonk) [#49847](https://github.com/nodejs/node/pull/49847)
* \[[`c5ff000cb1`](https://github.com/nodejs/node/commit/c5ff000cb1)] - **fs**: improve error performance of `realpathSync` (Yagiz Nizipli) [#49962](https://github.com/nodejs/node/pull/49962)
* \[[`6eeaa02f5c`](https://github.com/nodejs/node/commit/6eeaa02f5c)] - **fs**: improve error performance of `lchownSync` (Yagiz Nizipli) [#49962](https://github.com/nodejs/node/pull/49962)
* \[[`dc9ac8d41c`](https://github.com/nodejs/node/commit/dc9ac8d41c)] - **fs**: improve error performance of `symlinkSync` (Yagiz Nizipli) [#49962](https://github.com/nodejs/node/pull/49962)
* \[[`bc6f279261`](https://github.com/nodejs/node/commit/bc6f279261)] - **fs**: improve error performance of `readlinkSync` (Yagiz Nizipli) [#49962](https://github.com/nodejs/node/pull/49962)
* \[[`275987841e`](https://github.com/nodejs/node/commit/275987841e)] - **fs**: improve error performance of `mkdtempSync` (Yagiz Nizipli) [#49962](https://github.com/nodejs/node/pull/49962)
* \[[`81f15274e2`](https://github.com/nodejs/node/commit/81f15274e2)] - **fs**: improve error performance of `linkSync` (Yagiz Nizipli) [#49962](https://github.com/nodejs/node/pull/49962)
* \[[`f766c04856`](https://github.com/nodejs/node/commit/f766c04856)] - **fs**: improve error performance of `chownSync` (Yagiz Nizipli) [#49962](https://github.com/nodejs/node/pull/49962)
* \[[`610036c67d`](https://github.com/nodejs/node/commit/610036c67d)] - **fs**: improve error performance of `renameSync` (Yagiz Nizipli) [#49962](https://github.com/nodejs/node/pull/49962)
* \[[`18862e4d5d`](https://github.com/nodejs/node/commit/18862e4d5d)] - **(SEMVER-MINOR)** **fs**: add flush option to appendFile() functions (Colin Ihrig) [#50095](https://github.com/nodejs/node/pull/50095)
* \[[`3f8cbb15cb`](https://github.com/nodejs/node/commit/3f8cbb15cb)] - **http2**: allow streams to complete gracefully after goaway (Michael Lumish) [#50202](https://github.com/nodejs/node/pull/50202)
* \[[`1464eba1a0`](https://github.com/nodejs/node/commit/1464eba1a0)] - **lib**: improve performance of validateStringArray and validateBooleanArray (Aras Abbasi) [#49756](https://github.com/nodejs/node/pull/49756)
* \[[`5a52c518ef`](https://github.com/nodejs/node/commit/5a52c518ef)] - **(SEMVER-MINOR)** **lib**: add `navigator.userAgent` (Yagiz Nizipli) [#50200](https://github.com/nodejs/node/pull/50200)
* \[[`b6021ab8f6`](https://github.com/nodejs/node/commit/b6021ab8f6)] - **lib**: reduce overhead of blob clone (Vinicius Lourenço) [#50110](https://github.com/nodejs/node/pull/50110)
* \[[`be19d9baa1`](https://github.com/nodejs/node/commit/be19d9baa1)] - **meta**: move Trott to TSC regular member (Rich Trott) [#50297](https://github.com/nodejs/node/pull/50297)
* \[[`91e373f8e9`](https://github.com/nodejs/node/commit/91e373f8e9)] - **node-api**: return napi\_exception\_pending on proxy handlers (Chengzhong Wu) [#48607](https://github.com/nodejs/node/pull/48607)
* \[[`531a3ae4b5`](https://github.com/nodejs/node/commit/531a3ae4b5)] - **stream**: simplify prefinish (Robert Nagy) [#50204](https://github.com/nodejs/node/pull/50204)
* \[[`514ac86579`](https://github.com/nodejs/node/commit/514ac86579)] - **stream**: reduce scope of readable bitmap details (Robert Nagy) [#49963](https://github.com/nodejs/node/pull/49963)
* \[[`789372a072`](https://github.com/nodejs/node/commit/789372a072)] - **(SEMVER-MINOR)** **stream**: allow pass stream class to `stream.compose` (Alex Yang) [#50187](https://github.com/nodejs/node/pull/50187)
* \[[`f3a9ea0bc4`](https://github.com/nodejs/node/commit/f3a9ea0bc4)] - **stream**: call helper function from push and unshift (Raz Luvaton) [#50173](https://github.com/nodejs/node/pull/50173)
* \[[`a9ca7b32e7`](https://github.com/nodejs/node/commit/a9ca7b32e7)] - **test**: improve watch mode test (Moshe Atlow) [#50319](https://github.com/nodejs/node/pull/50319)
* \[[`63b7059efd`](https://github.com/nodejs/node/commit/63b7059efd)] - **test**: set `test-watch-mode-inspect` as flaky (Yagiz Nizipli) [#50259](https://github.com/nodejs/node/pull/50259)
* \[[`7f87084b05`](https://github.com/nodejs/node/commit/7f87084b05)] - _**Revert**_ "**test**: set `test-esm-loader-resolve-type` as flaky" (Antoine du Hamel) [#50315](https://github.com/nodejs/node/pull/50315)
* \[[`4d390e2de4`](https://github.com/nodejs/node/commit/4d390e2de4)] - **test**: replace forEach with for..of in test-http-perf\_hooks.js (Niya Shiyas) [#49818](https://github.com/nodejs/node/pull/49818)
* \[[`67c599ec39`](https://github.com/nodejs/node/commit/67c599ec39)] - **test**: replace forEach with for..of in test-net-isipv4.js (Niya Shiyas) [#49822](https://github.com/nodejs/node/pull/49822)
* \[[`19d3ce2494`](https://github.com/nodejs/node/commit/19d3ce2494)] - **test**: deflake `test-esm-loader-resolve-type` (Antoine du Hamel) [#50273](https://github.com/nodejs/node/pull/50273)
* \[[`2d8d6c5701`](https://github.com/nodejs/node/commit/2d8d6c5701)] - **test**: replace forEach with for..of in test-http2-server (Niya Shiyas) [#49819](https://github.com/nodejs/node/pull/49819)
* \[[`af31d51e5a`](https://github.com/nodejs/node/commit/af31d51e5a)] - **test**: replace forEach with for..of in test-http2-client-destroy.js (Niya Shiyas) [#49820](https://github.com/nodejs/node/pull/49820)
* \[[`465ad2a5ce`](https://github.com/nodejs/node/commit/465ad2a5ce)] - **test**: update `url` web platform tests (Yagiz Nizipli) [#50264](https://github.com/nodejs/node/pull/50264)
* \[[`3b80a6894c`](https://github.com/nodejs/node/commit/3b80a6894c)] - **test**: set `test-emit-after-on-destroyed` as flaky (Yagiz Nizipli) [#50246](https://github.com/nodejs/node/pull/50246)
* \[[`57adbdd156`](https://github.com/nodejs/node/commit/57adbdd156)] - **test**: set inspector async stack test as flaky (Yagiz Nizipli) [#50244](https://github.com/nodejs/node/pull/50244)
* \[[`6507f66404`](https://github.com/nodejs/node/commit/6507f66404)] - **test**: set test-worker-nearheaplimit-deadlock flaky (StefanStojanovic) [#50277](https://github.com/nodejs/node/pull/50277)
* \[[`21a6ba548d`](https://github.com/nodejs/node/commit/21a6ba548d)] - **test**: set `test-cli-node-options` as flaky (Yagiz Nizipli) [#50296](https://github.com/nodejs/node/pull/50296)
* \[[`c55f8f30cb`](https://github.com/nodejs/node/commit/c55f8f30cb)] - **test**: reduce the number of requests and parsers (Luigi Pinca) [#50240](https://github.com/nodejs/node/pull/50240)
* \[[`5129bedfa2`](https://github.com/nodejs/node/commit/5129bedfa2)] - **test**: set crypto-timing test as flaky (Yagiz Nizipli) [#50232](https://github.com/nodejs/node/pull/50232)
* \[[`9bc5ab5e07`](https://github.com/nodejs/node/commit/9bc5ab5e07)] - **test**: set `test-structuredclone-*` as flaky (Yagiz Nizipli) [#50261](https://github.com/nodejs/node/pull/50261)
* \[[`317e447ddc`](https://github.com/nodejs/node/commit/317e447ddc)] - **test**: deflake `test-loaders-workers-spawned` (Antoine du Hamel) [#50251](https://github.com/nodejs/node/pull/50251)
* \[[`0c710daae2`](https://github.com/nodejs/node/commit/0c710daae2)] - **test**: improve code coverage of diagnostics\_channel (Jithil P Ponnan) [#50053](https://github.com/nodejs/node/pull/50053)
* \[[`7c6e4d7ec3`](https://github.com/nodejs/node/commit/7c6e4d7ec3)] - **test**: set `test-esm-loader-resolve-type` as flaky (Yagiz Nizipli) [#50226](https://github.com/nodejs/node/pull/50226)
* \[[`c8744909b0`](https://github.com/nodejs/node/commit/c8744909b0)] - **test**: set inspector async hook test as flaky (Yagiz Nizipli) [#50252](https://github.com/nodejs/node/pull/50252)
* \[[`3e38001739`](https://github.com/nodejs/node/commit/3e38001739)] - **test**: skip test-benchmark-os.js on IBM i (Abdirahim Musse) [#50208](https://github.com/nodejs/node/pull/50208)
* \[[`dd66fdfb7b`](https://github.com/nodejs/node/commit/dd66fdfb7b)] - **test**: set parallel http server test as flaky (Yagiz Nizipli) [#50227](https://github.com/nodejs/node/pull/50227)
* \[[`a38d1311bf`](https://github.com/nodejs/node/commit/a38d1311bf)] - **test**: set test-worker-nearheaplimit-deadlock flaky (Stefan Stojanovic) [#50238](https://github.com/nodejs/node/pull/50238)
* \[[`8efb75fd80`](https://github.com/nodejs/node/commit/8efb75fd80)] - **test**: set `test-runner-watch-mode` as flaky (Yagiz Nizipli) [#50221](https://github.com/nodejs/node/pull/50221)
* \[[`143ddded74`](https://github.com/nodejs/node/commit/143ddded74)] - **test**: set sea snapshot tests as flaky (Yagiz Nizipli) [#50223](https://github.com/nodejs/node/pull/50223)
* \[[`ae905a8f35`](https://github.com/nodejs/node/commit/ae905a8f35)] - **test**: fix defect path traversal tests (Tobias Nießen) [#50124](https://github.com/nodejs/node/pull/50124)
* \[[`ce27ee701b`](https://github.com/nodejs/node/commit/ce27ee701b)] - **tls**: reduce TLS 'close' event listener warnings (Tim Perry) [#50136](https://github.com/nodejs/node/pull/50136)
* \[[`ab4bae8e1f`](https://github.com/nodejs/node/commit/ab4bae8e1f)] - **tools**: drop support for osx notarization with gon (Ulises Gascón) [#50291](https://github.com/nodejs/node/pull/50291)
* \[[`5df3d5abcc`](https://github.com/nodejs/node/commit/5df3d5abcc)] - **tools**: update comment in `update-uncidi.sh` and `acorn_version.h` (Jungku Lee) [#50175](https://github.com/nodejs/node/pull/50175)
* \[[`bf7b94f0b3`](https://github.com/nodejs/node/commit/bf7b94f0b3)] - **tools**: refactor checkimports.py (Mohammed Keyvanzadeh) [#50011](https://github.com/nodejs/node/pull/50011)
* \[[`5dc454a837`](https://github.com/nodejs/node/commit/5dc454a837)] - **util**: remove internal mime fns from benchmarks (Aras Abbasi) [#50201](https://github.com/nodejs/node/pull/50201)
* \[[`8f7eb15603`](https://github.com/nodejs/node/commit/8f7eb15603)] - **vm**: use import attributes instead of import assertions (Antoine du Hamel) [#50141](https://github.com/nodejs/node/pull/50141)
* \[[`dda33c2bf1`](https://github.com/nodejs/node/commit/dda33c2bf1)] - **vm**: reject in importModuleDynamically without --experimental-vm-modules (Joyee Cheung) [#50137](https://github.com/nodejs/node/pull/50137)
* \[[`3999362c59`](https://github.com/nodejs/node/commit/3999362c59)] - **vm**: use internal versions of compileFunction and Script (Joyee Cheung) [#50137](https://github.com/nodejs/node/pull/50137)
* \[[`a54179f0e0`](https://github.com/nodejs/node/commit/a54179f0e0)] - **vm**: unify host-defined option generation in vm.compileFunction (Joyee Cheung) [#50137](https://github.com/nodejs/node/pull/50137)
* \[[`87be790fa9`](https://github.com/nodejs/node/commit/87be790fa9)] - **worker**: handle detached `MessagePort` from a different context (Juan José) [#49150](https://github.com/nodejs/node/pull/49150)

<a id="21.0.0"></a>

## 2023-10-17, Version 21.0.0 (Current), @RafaelGSS and @targos

We're excited to announce the release of Node.js 21! Highlights include updates of the V8 JavaScript engine to 11.8,
stable `fetch` and `WebStreams`, a new experimental flag to change the interpretation of ambiguous code
from CommonJS to ES modules (`--experimental-default-type`), many updates to our test runner, and more!

Node.js 21 will replace Node.js 20 as our ‘Current’ release line when Node.js 20 enters long-term support (LTS) later this month.
As per the release schedule, Node.js 21 will be ‘Current' release for the next 6 months, until April 2024.

### Other Notable Changes

* \[[`740ca5423a`](https://github.com/nodejs/node/commit/740ca5423a)] - **doc**: promote fetch/webstreams from experimental to stable (Steven) [#45684](https://github.com/nodejs/node/pull/45684)
* \[[`85301803e1`](https://github.com/nodejs/node/commit/85301803e1)] - **esm**: --experimental-default-type flag to flip module defaults (Geoffrey Booth) [#49869](https://github.com/nodejs/node/pull/49869)
* \[[`705e623ac4`](https://github.com/nodejs/node/commit/705e623ac4)] - **esm**: remove `globalPreload` hook (superseded by `initialize`) (Jacob Smith) [#49144](https://github.com/nodejs/node/pull/49144)
* \[[`e01c1d700d`](https://github.com/nodejs/node/commit/e01c1d700d)] - **fs**: add flush option to writeFile() functions (Colin Ihrig) [#50009](https://github.com/nodejs/node/pull/50009)
* \[[`1948dce707`](https://github.com/nodejs/node/commit/1948dce707)] - **(SEMVER-MAJOR)** **fs**: add globSync implementation (Moshe Atlow) [#47653](https://github.com/nodejs/node/pull/47653)
* \[[`e28dbe1c2b`](https://github.com/nodejs/node/commit/e28dbe1c2b)] - **(SEMVER-MINOR)** **lib**: add WebSocket client (Matthew Aitken) [#49830](https://github.com/nodejs/node/pull/49830)
* \[[`95b8f5dcab`](https://github.com/nodejs/node/commit/95b8f5dcab)] - **stream**: optimize Writable (Robert Nagy) [#50012](https://github.com/nodejs/node/pull/50012)
* \[[`7cd4e70948`](https://github.com/nodejs/node/commit/7cd4e70948)] - **(SEMVER-MAJOR)** **test\_runner**: support passing globs (Moshe Atlow) [#47653](https://github.com/nodejs/node/pull/47653)
* \[[`1d220b55ac`](https://github.com/nodejs/node/commit/1d220b55ac)] - **vm**: use default HDO when importModuleDynamically is not set (Joyee Cheung) [#49950](https://github.com/nodejs/node/pull/49950)

### Semver-Major Commits

* \[[`ac2a68c76b`](https://github.com/nodejs/node/commit/ac2a68c76b)] - **(SEMVER-MAJOR)** **build**: drop support for Visual Studio 2019 (Michaël Zasso) [#49051](https://github.com/nodejs/node/pull/49051)
* \[[`4e3983031a`](https://github.com/nodejs/node/commit/4e3983031a)] - **(SEMVER-MAJOR)** **build**: bump supported macOS and Xcode versions (Michaël Zasso) [#49164](https://github.com/nodejs/node/pull/49164)
* \[[`5a0777776d`](https://github.com/nodejs/node/commit/5a0777776d)] - **(SEMVER-MAJOR)** **crypto**: do not overwrite \_writableState.defaultEncoding (Tobias Nießen) [#49140](https://github.com/nodejs/node/pull/49140)
* \[[`162a0652ab`](https://github.com/nodejs/node/commit/162a0652ab)] - **(SEMVER-MAJOR)** **deps**: bump minimum ICU version to 73 (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`17a74ddd3d`](https://github.com/nodejs/node/commit/17a74ddd3d)] - **(SEMVER-MAJOR)** **deps**: update V8 to 11.8.172.13 (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`e9ff81016d`](https://github.com/nodejs/node/commit/e9ff81016d)] - **(SEMVER-MAJOR)** **deps**: update llhttp to 9.1.2 (Paolo Insogna) [#48981](https://github.com/nodejs/node/pull/48981)
* \[[`7ace5aba75`](https://github.com/nodejs/node/commit/7ace5aba75)] - **(SEMVER-MAJOR)** **events**: validate options of `on` and `once` (Deokjin Kim) [#46018](https://github.com/nodejs/node/pull/46018)
* \[[`b3ec13d449`](https://github.com/nodejs/node/commit/b3ec13d449)] - **(SEMVER-MAJOR)** **fs**: adjust `position` validation in reading methods (Livia Medeiros) [#42835](https://github.com/nodejs/node/pull/42835)
* \[[`1948dce707`](https://github.com/nodejs/node/commit/1948dce707)] - **(SEMVER-MAJOR)** **fs**: add globSync implementation (Moshe Atlow) [#47653](https://github.com/nodejs/node/pull/47653)
* \[[`d68d0eacaa`](https://github.com/nodejs/node/commit/d68d0eacaa)] - **(SEMVER-MAJOR)** **http**: reduce parts in chunked response when corking (Robert Nagy) [#50167](https://github.com/nodejs/node/pull/50167)
* \[[`c5b0b894ed`](https://github.com/nodejs/node/commit/c5b0b894ed)] - **(SEMVER-MAJOR)** **lib**: mark URL/URLSearchParams as uncloneable and untransferable (Chengzhong Wu) [#47497](https://github.com/nodejs/node/pull/47497)
* \[[`3205b1936a`](https://github.com/nodejs/node/commit/3205b1936a)] - **(SEMVER-MAJOR)** **lib**: remove aix directory case for package reader (Yagiz Nizipli) [#48605](https://github.com/nodejs/node/pull/48605)
* \[[`b40f0c3074`](https://github.com/nodejs/node/commit/b40f0c3074)] - **(SEMVER-MAJOR)** **lib**: add `navigator.hardwareConcurrency` (Yagiz Nizipli) [#47769](https://github.com/nodejs/node/pull/47769)
* \[[`4b08c4c047`](https://github.com/nodejs/node/commit/4b08c4c047)] - **(SEMVER-MAJOR)** **lib**: runtime deprecate punycode (Yagiz Nizipli) [#47202](https://github.com/nodejs/node/pull/47202)
* \[[`3ce51ae9c0`](https://github.com/nodejs/node/commit/3ce51ae9c0)] - **(SEMVER-MAJOR)** **module**: harmonize error code between ESM and CJS (Antoine du Hamel) [#48606](https://github.com/nodejs/node/pull/48606)
* \[[`7202859402`](https://github.com/nodejs/node/commit/7202859402)] - **(SEMVER-MAJOR)** **net**: do not treat `server.maxConnections=0` as `Infinity` (ignoramous) [#48276](https://github.com/nodejs/node/pull/48276)
* \[[`c15bafdaf4`](https://github.com/nodejs/node/commit/c15bafdaf4)] - **(SEMVER-MAJOR)** **net**: only defer \_final call when connecting (Jason Zhang) [#47385](https://github.com/nodejs/node/pull/47385)
* \[[`6ffacbf0f9`](https://github.com/nodejs/node/commit/6ffacbf0f9)] - **(SEMVER-MAJOR)** **node-api**: rename internal NAPI\_VERSION definition (Chengzhong Wu) [#48501](https://github.com/nodejs/node/pull/48501)
* \[[`11af089b14`](https://github.com/nodejs/node/commit/11af089b14)] - **(SEMVER-MAJOR)** **src**: update NODE\_MODULE\_VERSION to 120 (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`d920b7c94b`](https://github.com/nodejs/node/commit/d920b7c94b)] - **(SEMVER-MAJOR)** **src**: throw DOMException on cloning non-serializable objects (Chengzhong Wu) [#47839](https://github.com/nodejs/node/pull/47839)
* \[[`64549731b6`](https://github.com/nodejs/node/commit/64549731b6)] - **(SEMVER-MAJOR)** **src**: throw DataCloneError on transfering untransferable objects (Chengzhong Wu) [#47604](https://github.com/nodejs/node/pull/47604)
* \[[`dac8de689b`](https://github.com/nodejs/node/commit/dac8de689b)] - **(SEMVER-MAJOR)** **stream**: use private properties for strategies (Yagiz Nizipli) [#47218](https://github.com/nodejs/node/pull/47218)
* \[[`1fa084ecdf`](https://github.com/nodejs/node/commit/1fa084ecdf)] - **(SEMVER-MAJOR)** **stream**: use private properties for encoding (Yagiz Nizipli) [#47218](https://github.com/nodejs/node/pull/47218)
* \[[`4e93247079`](https://github.com/nodejs/node/commit/4e93247079)] - **(SEMVER-MAJOR)** **stream**: use private properties for compression (Yagiz Nizipli) [#47218](https://github.com/nodejs/node/pull/47218)
* \[[`527589b755`](https://github.com/nodejs/node/commit/527589b755)] - **(SEMVER-MAJOR)** **test\_runner**: disallow array in `run` options (Raz Luvaton) [#49935](https://github.com/nodejs/node/pull/49935)
* \[[`7cd4e70948`](https://github.com/nodejs/node/commit/7cd4e70948)] - **(SEMVER-MAJOR)** **test\_runner**: support passing globs (Moshe Atlow) [#47653](https://github.com/nodejs/node/pull/47653)
* \[[`2ef170254b`](https://github.com/nodejs/node/commit/2ef170254b)] - **(SEMVER-MAJOR)** **tls**: use `validateNumber` for `options.minDHSize` (Deokjin Kim) [#49973](https://github.com/nodejs/node/pull/49973)
* \[[`092fb9f541`](https://github.com/nodejs/node/commit/092fb9f541)] - **(SEMVER-MAJOR)** **tls**: use validateFunction for `options.checkServerIdentity` (Deokjin Kim) [#49896](https://github.com/nodejs/node/pull/49896)
* \[[`ccca547e28`](https://github.com/nodejs/node/commit/ccca547e28)] - **(SEMVER-MAJOR)** **util**: runtime deprecate `promisify`-ing a function returning a `Promise` (Antoine du Hamel) [#49609](https://github.com/nodejs/node/pull/49609)
* \[[`4038cf0513`](https://github.com/nodejs/node/commit/4038cf0513)] - **(SEMVER-MAJOR)** **vm**: freeze `dependencySpecifiers` array (Antoine du Hamel) [#49720](https://github.com/nodejs/node/pull/49720)

### Semver-Minor Commits

* \[[`3227d7327c`](https://github.com/nodejs/node/commit/3227d7327c)] - **(SEMVER-MINOR)** **deps**: update uvwasi to 0.0.19 (Node.js GitHub Bot) [#49908](https://github.com/nodejs/node/pull/49908)
* \[[`e28dbe1c2b`](https://github.com/nodejs/node/commit/e28dbe1c2b)] - **(SEMVER-MINOR)** **lib**: add WebSocket client (Matthew Aitken) [#49830](https://github.com/nodejs/node/pull/49830)
* \[[`9f9c58212e`](https://github.com/nodejs/node/commit/9f9c58212e)] - **(SEMVER-MINOR)** **test\_runner, cli**: add --test-concurrency flag (Colin Ihrig) [#49996](https://github.com/nodejs/node/pull/49996)
* \[[`d37b0d267f`](https://github.com/nodejs/node/commit/d37b0d267f)] - **(SEMVER-MINOR)** **wasi**: updates required for latest uvwasi version (Michael Dawson) [#49908](https://github.com/nodejs/node/pull/49908)

### Semver-Patch Commits

* \[[`33c87ec096`](https://github.com/nodejs/node/commit/33c87ec096)] - **benchmark**: fix race condition on fs benchs (Vinicius Lourenço) [#50035](https://github.com/nodejs/node/pull/50035)
* \[[`3c0ec61c4b`](https://github.com/nodejs/node/commit/3c0ec61c4b)] - **benchmark**: add warmup to accessSync bench (Rafael Gonzaga) [#50073](https://github.com/nodejs/node/pull/50073)
* \[[`1a839f388e`](https://github.com/nodejs/node/commit/1a839f388e)] - **benchmark**: improved config for blob,file benchmark (Vinícius Lourenço) [#49730](https://github.com/nodejs/node/pull/49730)
* \[[`86fe5a80f3`](https://github.com/nodejs/node/commit/86fe5a80f3)] - **benchmark**: added new benchmarks for blob (Vinícius Lourenço) [#49730](https://github.com/nodejs/node/pull/49730)
* \[[`6322d4f587`](https://github.com/nodejs/node/commit/6322d4f587)] - **build**: fix IBM i build with Python 3.9 (Richard Lau) [#48056](https://github.com/nodejs/node/pull/48056)
* \[[`17c55d176b`](https://github.com/nodejs/node/commit/17c55d176b)] - **build**: reset embedder string to "-node.0" (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`f10928f926`](https://github.com/nodejs/node/commit/f10928f926)] - **crypto**: use X509\_ALGOR accessors instead of reaching into X509\_ALGOR (David Benjamin) [#50057](https://github.com/nodejs/node/pull/50057)
* \[[`136a96722a`](https://github.com/nodejs/node/commit/136a96722a)] - **crypto**: account for disabled SharedArrayBuffer (Shelley Vohr) [#50034](https://github.com/nodejs/node/pull/50034)
* \[[`17b9925393`](https://github.com/nodejs/node/commit/17b9925393)] - **crypto**: return clear errors when loading invalid PFX data (Tim Perry) [#49566](https://github.com/nodejs/node/pull/49566)
* \[[`ca25d564c6`](https://github.com/nodejs/node/commit/ca25d564c6)] - **deps**: upgrade npm to 10.2.0 (npm team) [#50027](https://github.com/nodejs/node/pull/50027)
* \[[`f23a9353ae`](https://github.com/nodejs/node/commit/f23a9353ae)] - **deps**: update corepack to 0.21.0 (Node.js GitHub Bot) [#50088](https://github.com/nodejs/node/pull/50088)
* \[[`ceedb3a509`](https://github.com/nodejs/node/commit/ceedb3a509)] - **deps**: update simdutf to 3.2.18 (Node.js GitHub Bot) [#50091](https://github.com/nodejs/node/pull/50091)
* \[[`0522ac086c`](https://github.com/nodejs/node/commit/0522ac086c)] - **deps**: update zlib to 1.2.13.1-motley-fef5869 (Node.js GitHub Bot) [#50085](https://github.com/nodejs/node/pull/50085)
* \[[`4f8c5829da`](https://github.com/nodejs/node/commit/4f8c5829da)] - **deps**: update googletest to 2dd1c13 (Node.js GitHub Bot) [#50081](https://github.com/nodejs/node/pull/50081)
* \[[`588784ea30`](https://github.com/nodejs/node/commit/588784ea30)] - **deps**: update undici to 5.25.4 (Node.js GitHub Bot) [#50025](https://github.com/nodejs/node/pull/50025)
* \[[`c9eef0c3c4`](https://github.com/nodejs/node/commit/c9eef0c3c4)] - **deps**: update googletest to e47544a (Node.js GitHub Bot) [#49982](https://github.com/nodejs/node/pull/49982)
* \[[`23cb478398`](https://github.com/nodejs/node/commit/23cb478398)] - **deps**: update ada to 2.6.10 (Node.js GitHub Bot) [#49984](https://github.com/nodejs/node/pull/49984)
* \[[`61411bb323`](https://github.com/nodejs/node/commit/61411bb323)] - **deps**: fix call to undeclared functions 'ntohl' and 'htons' (MatteoBax) [#49979](https://github.com/nodejs/node/pull/49979)
* \[[`49cf182e30`](https://github.com/nodejs/node/commit/49cf182e30)] - **deps**: update ada to 2.6.9 (Node.js GitHub Bot) [#49340](https://github.com/nodejs/node/pull/49340)
* \[[`ceb6df0f22`](https://github.com/nodejs/node/commit/ceb6df0f22)] - **deps**: update ada to 2.6.8 (Node.js GitHub Bot) [#49340](https://github.com/nodejs/node/pull/49340)
* \[[`b73e18b5dc`](https://github.com/nodejs/node/commit/b73e18b5dc)] - **deps**: update ada to 2.6.7 (Node.js GitHub Bot) [#49340](https://github.com/nodejs/node/pull/49340)
* \[[`baf2256617`](https://github.com/nodejs/node/commit/baf2256617)] - **deps**: update ada to 2.6.5 (Node.js GitHub Bot) [#49340](https://github.com/nodejs/node/pull/49340)
* \[[`a20a328a9b`](https://github.com/nodejs/node/commit/a20a328a9b)] - **deps**: update ada to 2.6.3 (Node.js GitHub Bot) [#49340](https://github.com/nodejs/node/pull/49340)
* \[[`3838b579e4`](https://github.com/nodejs/node/commit/3838b579e4)] - **deps**: V8: cherry-pick 8ec2651fbdd8 (Abdirahim Musse) [#49862](https://github.com/nodejs/node/pull/49862)
* \[[`668437ccad`](https://github.com/nodejs/node/commit/668437ccad)] - **deps**: V8: cherry-pick b60a03df4ceb (Joyee Cheung) [#49491](https://github.com/nodejs/node/pull/49491)
* \[[`f970087147`](https://github.com/nodejs/node/commit/f970087147)] - **deps**: V8: backport 93b1a74cbc9b (Joyee Cheung) [#49419](https://github.com/nodejs/node/pull/49419)
* \[[`4531c154e5`](https://github.com/nodejs/node/commit/4531c154e5)] - **deps**: V8: cherry-pick 8ec2651fbdd8 (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`9ad0e2cacc`](https://github.com/nodejs/node/commit/9ad0e2cacc)] - **deps**: V8: cherry-pick 89b3702c92b0 (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`dfc9c86868`](https://github.com/nodejs/node/commit/dfc9c86868)] - **deps**: V8: cherry-pick de9a5de2274f (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`186b36efba`](https://github.com/nodejs/node/commit/186b36efba)] - **deps**: V8: cherry-pick b5b5d6c31bb0 (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`867586ce95`](https://github.com/nodejs/node/commit/867586ce95)] - **deps**: V8: cherry-pick 93b1a74cbc9b (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`4ad3479ba7`](https://github.com/nodejs/node/commit/4ad3479ba7)] - **deps**: V8: cherry-pick 1a3ecc2483b2 (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`660f902f16`](https://github.com/nodejs/node/commit/660f902f16)] - **deps**: patch V8 to avoid duplicated zlib symbol (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`f7c1d410ad`](https://github.com/nodejs/node/commit/f7c1d410ad)] - **deps**: remove usage of a C++20 feature from V8 (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`9c4030bfb9`](https://github.com/nodejs/node/commit/9c4030bfb9)] - **deps**: avoid compilation error with ASan (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`5f05cc15e6`](https://github.com/nodejs/node/commit/5f05cc15e6)] - **deps**: disable V8 concurrent sparkplug compilation (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`42cd952dbd`](https://github.com/nodejs/node/commit/42cd952dbd)] - **deps**: silence irrelevant V8 warning (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`88cf90f9c4`](https://github.com/nodejs/node/commit/88cf90f9c4)] - **deps**: always define V8\_EXPORT\_PRIVATE as no-op (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`8609915951`](https://github.com/nodejs/node/commit/8609915951)] - **doc**: improve ccache explanation (Chengzhong Wu) [#50133](https://github.com/nodejs/node/pull/50133)
* \[[`91d21324a9`](https://github.com/nodejs/node/commit/91d21324a9)] - **doc**: move danielleadams to TSC non-voting member (Danielle Adams) [#50142](https://github.com/nodejs/node/pull/50142)
* \[[`34fa7043a2`](https://github.com/nodejs/node/commit/34fa7043a2)] - **doc**: fix description of `fs.readdir` `recursive` option (RamdohokarAngha) [#48902](https://github.com/nodejs/node/pull/48902)
* \[[`81e4d2ec2f`](https://github.com/nodejs/node/commit/81e4d2ec2f)] - **doc**: mention files read before env setup (Rafael Gonzaga) [#50072](https://github.com/nodejs/node/pull/50072)
* \[[`0ce37ed8e9`](https://github.com/nodejs/node/commit/0ce37ed8e9)] - **doc**: move permission model to Active Development (Rafael Gonzaga) [#50068](https://github.com/nodejs/node/pull/50068)
* \[[`3c430212c3`](https://github.com/nodejs/node/commit/3c430212c3)] - **doc**: add command to get patch minors and majors (Rafael Gonzaga) [#50067](https://github.com/nodejs/node/pull/50067)
* \[[`e43bf4c31d`](https://github.com/nodejs/node/commit/e43bf4c31d)] - **doc**: use precise promise terminology in fs (Benjamin Gruenbaum) [#50029](https://github.com/nodejs/node/pull/50029)
* \[[`d3a5f1fb5f`](https://github.com/nodejs/node/commit/d3a5f1fb5f)] - **doc**: use precise terminology in test runner (Benjamin Gruenbaum) [#50028](https://github.com/nodejs/node/pull/50028)
* \[[`24dea2348d`](https://github.com/nodejs/node/commit/24dea2348d)] - **doc**: clarify explaination text on how to run the example (Anshul Sinha) [#39020](https://github.com/nodejs/node/pull/39020)
* \[[`f3ed57bd8b`](https://github.com/nodejs/node/commit/f3ed57bd8b)] - **doc**: reserve 119 for Electron 28 (David Sanders) [#50020](https://github.com/nodejs/node/pull/50020)
* \[[`85c09f178c`](https://github.com/nodejs/node/commit/85c09f178c)] - **doc**: update Collaborator pronouns (Tierney Cyren) [#50005](https://github.com/nodejs/node/pull/50005)
* \[[`099e2f7bce`](https://github.com/nodejs/node/commit/099e2f7bce)] - **doc**: update link to Abstract Modules Records spec (Rich Trott) [#49961](https://github.com/nodejs/node/pull/49961)
* \[[`47b2883673`](https://github.com/nodejs/node/commit/47b2883673)] - **doc**: updated building docs for windows (Claudio W) [#49767](https://github.com/nodejs/node/pull/49767)
* \[[`7b624c30b2`](https://github.com/nodejs/node/commit/7b624c30b2)] - **doc**: update CHANGELOG\_V20 about vm fixes (Joyee Cheung) [#49951](https://github.com/nodejs/node/pull/49951)
* \[[`1dc0667aa6`](https://github.com/nodejs/node/commit/1dc0667aa6)] - **doc**: document dangerous symlink behavior (Tobias Nießen) [#49154](https://github.com/nodejs/node/pull/49154)
* \[[`bc056c2426`](https://github.com/nodejs/node/commit/bc056c2426)] - **doc**: add main ARIA landmark to API docs (Rich Trott) [#49882](https://github.com/nodejs/node/pull/49882)
* \[[`f416a0f555`](https://github.com/nodejs/node/commit/f416a0f555)] - **doc**: add navigation ARIA landmark to doc ToC (Rich Trott) [#49882](https://github.com/nodejs/node/pull/49882)
* \[[`740ca5423a`](https://github.com/nodejs/node/commit/740ca5423a)] - **doc**: promote fetch/webstreams from experimental to stable (Steven) [#45684](https://github.com/nodejs/node/pull/45684)
* \[[`f802aa0645`](https://github.com/nodejs/node/commit/f802aa0645)] - **doc**: fix 'partial' typo (Colin Ihrig) [#48657](https://github.com/nodejs/node/pull/48657)
* \[[`6fda81d4f5`](https://github.com/nodejs/node/commit/6fda81d4f5)] - **doc**: mention `Navigator` is a partial implementation (Moshe Atlow) [#48656](https://github.com/nodejs/node/pull/48656)
* \[[`6aa2aeedcb`](https://github.com/nodejs/node/commit/6aa2aeedcb)] - **doc**: mark Node.js 19 as End-of-Life (Richard Lau) [#48283](https://github.com/nodejs/node/pull/48283)
* \[[`0ee9c83ffc`](https://github.com/nodejs/node/commit/0ee9c83ffc)] - **errors**: improve performance of determine-specific-type (Aras Abbasi) [#49696](https://github.com/nodejs/node/pull/49696)
* \[[`4f84a3d200`](https://github.com/nodejs/node/commit/4f84a3d200)] - **errors**: improve formatList in errors.js (Aras Abbasi) [#49642](https://github.com/nodejs/node/pull/49642)
* \[[`cc725a653a`](https://github.com/nodejs/node/commit/cc725a653a)] - **errors**: improve performance of instantiation (Aras Abbasi) [#49654](https://github.com/nodejs/node/pull/49654)
* \[[`d1ef6aa2db`](https://github.com/nodejs/node/commit/d1ef6aa2db)] - **esm**: use import attributes instead of import assertions (Antoine du Hamel) [#50140](https://github.com/nodejs/node/pull/50140)
* \[[`19b470f866`](https://github.com/nodejs/node/commit/19b470f866)] - **esm**: bypass CommonJS loader under --default-type (Geoffrey Booth) [#49986](https://github.com/nodejs/node/pull/49986)
* \[[`9c683204db`](https://github.com/nodejs/node/commit/9c683204db)] - **esm**: unflag extensionless javascript and wasm in module scope (Geoffrey Booth) [#49974](https://github.com/nodejs/node/pull/49974)
* \[[`05be31d5de`](https://github.com/nodejs/node/commit/05be31d5de)] - **esm**: improve `getFormatOfExtensionlessFile` speed (Yagiz Nizipli) [#49965](https://github.com/nodejs/node/pull/49965)
* \[[`aadfea4979`](https://github.com/nodejs/node/commit/aadfea4979)] - **esm**: improve JSDoc annotation of internal functions (Antoine du Hamel) [#49959](https://github.com/nodejs/node/pull/49959)
* \[[`7f0e36af52`](https://github.com/nodejs/node/commit/7f0e36af52)] - **esm**: fix cache collision on JSON files using file: URL (Antoine du Hamel) [#49887](https://github.com/nodejs/node/pull/49887)
* \[[`85301803e1`](https://github.com/nodejs/node/commit/85301803e1)] - **esm**: --experimental-default-type flag to flip module defaults (Geoffrey Booth) [#49869](https://github.com/nodejs/node/pull/49869)
* \[[`f42a103991`](https://github.com/nodejs/node/commit/f42a103991)] - **esm**: require braces for modules code (Geoffrey Booth) [#49657](https://github.com/nodejs/node/pull/49657)
* \[[`705e623ac4`](https://github.com/nodejs/node/commit/705e623ac4)] - **esm**: remove `globalPreload` hook (superseded by `initialize`) (Jacob Smith) [#49144](https://github.com/nodejs/node/pull/49144)
* \[[`18a818744f`](https://github.com/nodejs/node/commit/18a818744f)] - **fs**: improve error performance of `readdirSync` (Yagiz Nizipli) [#50131](https://github.com/nodejs/node/pull/50131)
* \[[`d3985296a9`](https://github.com/nodejs/node/commit/d3985296a9)] - **fs**: fix `unlinkSync` typings (Yagiz Nizipli) [#49859](https://github.com/nodejs/node/pull/49859)
* \[[`6bc7fa7906`](https://github.com/nodejs/node/commit/6bc7fa7906)] - **fs**: improve error perf of sync `chmod`+`fchmod` (CanadaHonk) [#49859](https://github.com/nodejs/node/pull/49859)
* \[[`6bd77db41f`](https://github.com/nodejs/node/commit/6bd77db41f)] - **fs**: improve error perf of sync `*times` (CanadaHonk) [#49864](https://github.com/nodejs/node/pull/49864)
* \[[`bf0f0789da`](https://github.com/nodejs/node/commit/bf0f0789da)] - **fs**: improve error performance of writevSync (IlyasShabi) [#50038](https://github.com/nodejs/node/pull/50038)
* \[[`8a49735bae`](https://github.com/nodejs/node/commit/8a49735bae)] - **fs**: add flush option to createWriteStream() (Colin Ihrig) [#50093](https://github.com/nodejs/node/pull/50093)
* \[[`ed49722a8a`](https://github.com/nodejs/node/commit/ed49722a8a)] - **fs**: improve error performance for `ftruncateSync` (André Alves) [#50032](https://github.com/nodejs/node/pull/50032)
* \[[`e01c1d700d`](https://github.com/nodejs/node/commit/e01c1d700d)] - **fs**: add flush option to writeFile() functions (Colin Ihrig) [#50009](https://github.com/nodejs/node/pull/50009)
* \[[`f7a160d5b4`](https://github.com/nodejs/node/commit/f7a160d5b4)] - **fs**: improve error performance for `fdatasyncSync` (Jungku Lee) [#49898](https://github.com/nodejs/node/pull/49898)
* \[[`813713f211`](https://github.com/nodejs/node/commit/813713f211)] - **fs**: throw errors from sync branches instead of separate implementations (Joyee Cheung) [#49913](https://github.com/nodejs/node/pull/49913)
* \[[`b866e38192`](https://github.com/nodejs/node/commit/b866e38192)] - **http**: refactor to make servername option normalization testable (Rongjian Zhang) [#38733](https://github.com/nodejs/node/pull/38733)
* \[[`2990390359`](https://github.com/nodejs/node/commit/2990390359)] - **inspector**: simplify dispatchProtocolMessage (Daniel Lemire) [#49780](https://github.com/nodejs/node/pull/49780)
* \[[`d4c5fe488e`](https://github.com/nodejs/node/commit/d4c5fe488e)] - **lib**: fix compileFunction throws range error for negative numbers (Jithil P Ponnan) [#49855](https://github.com/nodejs/node/pull/49855)
* \[[`589ac5004c`](https://github.com/nodejs/node/commit/589ac5004c)] - **lib**: faster internal createBlob (Vinícius Lourenço) [#49730](https://github.com/nodejs/node/pull/49730)
* \[[`952cf0d17a`](https://github.com/nodejs/node/commit/952cf0d17a)] - **lib**: reduce overhead of validateObject (Vinicius Lourenço) [#49928](https://github.com/nodejs/node/pull/49928)
* \[[`fa250fdec1`](https://github.com/nodejs/node/commit/fa250fdec1)] - **lib**: make fetch sync and return a Promise (Matthew Aitken) [#49936](https://github.com/nodejs/node/pull/49936)
* \[[`1b96975f27`](https://github.com/nodejs/node/commit/1b96975f27)] - **lib**: fix `primordials` typings (Sam Verschueren) [#49895](https://github.com/nodejs/node/pull/49895)
* \[[`6aa7101960`](https://github.com/nodejs/node/commit/6aa7101960)] - **lib**: update params in jsdoc for `HTTPRequestOptions` (Jungku Lee) [#49872](https://github.com/nodejs/node/pull/49872)
* \[[`a4fdb1abe0`](https://github.com/nodejs/node/commit/a4fdb1abe0)] - **lib,test**: do not hardcode Buffer.kMaxLength (Michaël Zasso) [#49876](https://github.com/nodejs/node/pull/49876)
* \[[`fd21429ef5`](https://github.com/nodejs/node/commit/fd21429ef5)] - **lib**: update usage of always on Atomics API (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`bac85be22d`](https://github.com/nodejs/node/commit/bac85be22d)] - **meta**: ping TSC for offboarding (Tobias Nießen) [#50147](https://github.com/nodejs/node/pull/50147)
* \[[`609b13e6c2`](https://github.com/nodejs/node/commit/609b13e6c2)] - **meta**: bump actions/upload-artifact from 3.1.2 to 3.1.3 (dependabot\[bot]) [#50000](https://github.com/nodejs/node/pull/50000)
* \[[`3825464ef4`](https://github.com/nodejs/node/commit/3825464ef4)] - **meta**: bump actions/cache from 3.3.1 to 3.3.2 (dependabot\[bot]) [#50003](https://github.com/nodejs/node/pull/50003)
* \[[`49f0f9ca11`](https://github.com/nodejs/node/commit/49f0f9ca11)] - **meta**: bump github/codeql-action from 2.21.5 to 2.21.9 (dependabot\[bot]) [#50002](https://github.com/nodejs/node/pull/50002)
* \[[`f156427244`](https://github.com/nodejs/node/commit/f156427244)] - **meta**: bump actions/checkout from 3.6.0 to 4.1.0 (dependabot\[bot]) [#50001](https://github.com/nodejs/node/pull/50001)
* \[[`0fe673c7e6`](https://github.com/nodejs/node/commit/0fe673c7e6)] - **meta**: update website team with new name (Rich Trott) [#49883](https://github.com/nodejs/node/pull/49883)
* \[[`51f4ff2450`](https://github.com/nodejs/node/commit/51f4ff2450)] - **module**: move helpers out of cjs loader (Geoffrey Booth) [#49912](https://github.com/nodejs/node/pull/49912)
* \[[`7517c9f95b`](https://github.com/nodejs/node/commit/7517c9f95b)] - **module, esm**: jsdoc for modules files (Geoffrey Booth) [#49523](https://github.com/nodejs/node/pull/49523)
* \[[`b55adfb4f1`](https://github.com/nodejs/node/commit/b55adfb4f1)] - **node-api**: update headers for better wasm support (Toyo Li) [#49037](https://github.com/nodejs/node/pull/49037)
* \[[`b38e312486`](https://github.com/nodejs/node/commit/b38e312486)] - **node-api**: run finalizers directly from GC (Vladimir Morozov) [#42651](https://github.com/nodejs/node/pull/42651)
* \[[`0f0dd1a493`](https://github.com/nodejs/node/commit/0f0dd1a493)] - **os**: cache homedir, remove getCheckedFunction (Aras Abbasi) [#50037](https://github.com/nodejs/node/pull/50037)
* \[[`0e507d30ac`](https://github.com/nodejs/node/commit/0e507d30ac)] - **perf\_hooks**: reduce overhead of new user timings (Vinicius Lourenço) [#49914](https://github.com/nodejs/node/pull/49914)
* \[[`328bdac7f0`](https://github.com/nodejs/node/commit/328bdac7f0)] - **perf\_hooks**: reducing overhead of performance observer entry list (Vinicius Lourenço) [#50008](https://github.com/nodejs/node/pull/50008)
* \[[`e6e320ecc7`](https://github.com/nodejs/node/commit/e6e320ecc7)] - **perf\_hooks**: reduce overhead of new resource timings (Vinicius Lourenço) [#49837](https://github.com/nodejs/node/pull/49837)
* \[[`971af4b211`](https://github.com/nodejs/node/commit/971af4b211)] - **quic**: fix up coverity warning in quic/session.cc (Michael Dawson) [#49865](https://github.com/nodejs/node/pull/49865)
* \[[`546797f2b1`](https://github.com/nodejs/node/commit/546797f2b1)] - **quic**: prevent copying ngtcp2\_cid (Tobias Nießen) [#48561](https://github.com/nodejs/node/pull/48561)
* \[[`ac6f594c97`](https://github.com/nodejs/node/commit/ac6f594c97)] - **quic**: address new coverity warning (Michael Dawson) [#48384](https://github.com/nodejs/node/pull/48384)
* \[[`4ee8ef269b`](https://github.com/nodejs/node/commit/4ee8ef269b)] - **quic**: prevent copying ngtcp2\_cid\_token (Tobias Nießen) [#48370](https://github.com/nodejs/node/pull/48370)
* \[[`6d2811fbf2`](https://github.com/nodejs/node/commit/6d2811fbf2)] - **quic**: add additional implementation (James M Snell) [#47927](https://github.com/nodejs/node/pull/47927)
* \[[`0b3fcfcf35`](https://github.com/nodejs/node/commit/0b3fcfcf35)] - **quic**: fix typo in endpoint.h (Tobias Nießen) [#47911](https://github.com/nodejs/node/pull/47911)
* \[[`76044c4e2b`](https://github.com/nodejs/node/commit/76044c4e2b)] - **quic**: add additional QUIC implementation (James M Snell) [#47603](https://github.com/nodejs/node/pull/47603)
* \[[`78a15702dd`](https://github.com/nodejs/node/commit/78a15702dd)] - **src**: avoid making JSTransferable wrapper object weak (Chengzhong Wu) [#50026](https://github.com/nodejs/node/pull/50026)
* \[[`387e2929fe`](https://github.com/nodejs/node/commit/387e2929fe)] - **src**: generate default snapshot with --predictable (Joyee Cheung) [#48749](https://github.com/nodejs/node/pull/48749)
* \[[`1643adf771`](https://github.com/nodejs/node/commit/1643adf771)] - **src**: fix TLSWrap lifetime bug in ALPN callback (Ben Noordhuis) [#49635](https://github.com/nodejs/node/pull/49635)
* \[[`66776d8665`](https://github.com/nodejs/node/commit/66776d8665)] - **src**: set port in node\_options to uint16\_t (Yagiz Nizipli) [#49151](https://github.com/nodejs/node/pull/49151)
* \[[`55ff64001a`](https://github.com/nodejs/node/commit/55ff64001a)] - **src**: name scoped lock (Mohammed Keyvanzadeh) [#50010](https://github.com/nodejs/node/pull/50010)
* \[[`b903a710f4`](https://github.com/nodejs/node/commit/b903a710f4)] - **src**: use exact return value for `uv_os_getenv` (Yagiz Nizipli) [#49149](https://github.com/nodejs/node/pull/49149)
* \[[`43500fa646`](https://github.com/nodejs/node/commit/43500fa646)] - **src**: move const variable in `node_file.h` to `node_file.cc` (Jungku Lee) [#49688](https://github.com/nodejs/node/pull/49688)
* \[[`36ab510da7`](https://github.com/nodejs/node/commit/36ab510da7)] - **src**: remove unused variable (Michaël Zasso) [#49665](https://github.com/nodejs/node/pull/49665)
* \[[`23d65e7281`](https://github.com/nodejs/node/commit/23d65e7281)] - **src**: revert `IS_RELEASE` to 0 (Rafael Gonzaga) [#49084](https://github.com/nodejs/node/pull/49084)
* \[[`38dee8a1c0`](https://github.com/nodejs/node/commit/38dee8a1c0)] - **src**: distinguish HTML transferable and cloneable (Chengzhong Wu) [#47956](https://github.com/nodejs/node/pull/47956)
* \[[`586fcff061`](https://github.com/nodejs/node/commit/586fcff061)] - **src**: fix logically dead code reported by Coverity (Mohammed Keyvanzadeh) [#48589](https://github.com/nodejs/node/pull/48589)
* \[[`7f2c810814`](https://github.com/nodejs/node/commit/7f2c810814)] - **src,tools**: initialize cppgc (Daryl Haresign) [#45704](https://github.com/nodejs/node/pull/45704)
* \[[`aad8002b88`](https://github.com/nodejs/node/commit/aad8002b88)] - **stream**: use private symbol for bitmap state (Robert Nagy) [#49993](https://github.com/nodejs/node/pull/49993)
* \[[`a85e4186e5`](https://github.com/nodejs/node/commit/a85e4186e5)] - **stream**: reduce overhead of transfer (Vinicius Lourenço) [#50107](https://github.com/nodejs/node/pull/50107)
* \[[`e9bda11761`](https://github.com/nodejs/node/commit/e9bda11761)] - **stream**: lazy allocate back pressure buffer (Robert Nagy) [#50013](https://github.com/nodejs/node/pull/50013)
* \[[`557044af40`](https://github.com/nodejs/node/commit/557044af40)] - **stream**: avoid unnecessary drain for sync stream (Robert Nagy) [#50014](https://github.com/nodejs/node/pull/50014)
* \[[`95b8f5dcab`](https://github.com/nodejs/node/commit/95b8f5dcab)] - **stream**: optimize Writable (Robert Nagy) [#50012](https://github.com/nodejs/node/pull/50012)
* \[[`5de25deeb9`](https://github.com/nodejs/node/commit/5de25deeb9)] - **stream**: avoid tick in writable hot path (Robert Nagy) [#49966](https://github.com/nodejs/node/pull/49966)
* \[[`53b5545672`](https://github.com/nodejs/node/commit/53b5545672)] - **stream**: writable state bitmap (Robert Nagy) [#49899](https://github.com/nodejs/node/pull/49899)
* \[[`d4e99b1a66`](https://github.com/nodejs/node/commit/d4e99b1a66)] - **stream**: remove asIndexedPairs (Chemi Atlow) [#48150](https://github.com/nodejs/node/pull/48150)
* \[[`41e4174945`](https://github.com/nodejs/node/commit/41e4174945)] - **test**: replace forEach with for..of in test-net-isipv6.js (Niya Shiyas) [#49823](https://github.com/nodejs/node/pull/49823)
* \[[`f0e720a7fa`](https://github.com/nodejs/node/commit/f0e720a7fa)] - **test**: add EOVERFLOW as an allowed error (Abdirahim Musse) [#50128](https://github.com/nodejs/node/pull/50128)
* \[[`224f3ae974`](https://github.com/nodejs/node/commit/224f3ae974)] - **test**: reduce number of repetition in test-heapdump-shadowrealm.js (Chengzhong Wu) [#50104](https://github.com/nodejs/node/pull/50104)
* \[[`76004f3e56`](https://github.com/nodejs/node/commit/76004f3e56)] - **test**: replace forEach with for..of in test-parse-args.mjs (Niya Shiyas) [#49824](https://github.com/nodejs/node/pull/49824)
* \[[`fce8fbadcd`](https://github.com/nodejs/node/commit/fce8fbadcd)] - **test**: replace forEach with for..of in test-process-env (Niya Shiyas) [#49825](https://github.com/nodejs/node/pull/49825)
* \[[`24492476a7`](https://github.com/nodejs/node/commit/24492476a7)] - **test**: replace forEach with for..of in test-http-url (Niya Shiyas) [#49840](https://github.com/nodejs/node/pull/49840)
* \[[`2fe511ba23`](https://github.com/nodejs/node/commit/2fe511ba23)] - **test**: replace forEach() in test-net-perf\_hooks with for of (Narcisa Codreanu) [#49831](https://github.com/nodejs/node/pull/49831)
* \[[`42c37f28e6`](https://github.com/nodejs/node/commit/42c37f28e6)] - **test**: change forEach to for...of (Tiffany Lastimosa) [#49799](https://github.com/nodejs/node/pull/49799)
* \[[`6c9625dca4`](https://github.com/nodejs/node/commit/6c9625dca4)] - **test**: update skip for moved `test-wasm-web-api` (Richard Lau) [#49958](https://github.com/nodejs/node/pull/49958)
* \[[`f05d6d090c`](https://github.com/nodejs/node/commit/f05d6d090c)] - _**Revert**_ "**test**: mark test-runner-output as flaky" (Luigi Pinca) [#49905](https://github.com/nodejs/node/pull/49905)
* \[[`035e06317a`](https://github.com/nodejs/node/commit/035e06317a)] - **test**: disambiguate AIX and IBM i (Richard Lau) [#48056](https://github.com/nodejs/node/pull/48056)
* \[[`4d0aeed4a6`](https://github.com/nodejs/node/commit/4d0aeed4a6)] - **test**: deflake test-perf-hooks.js (Joyee Cheung) [#49892](https://github.com/nodejs/node/pull/49892)
* \[[`853f57239c`](https://github.com/nodejs/node/commit/853f57239c)] - **test**: migrate message error tests from Python to JS (Yiyun Lei) [#49721](https://github.com/nodejs/node/pull/49721)
* \[[`a71e3a65bb`](https://github.com/nodejs/node/commit/a71e3a65bb)] - **test**: fix edge snapshot stack traces (Geoffrey Booth) [#49659](https://github.com/nodejs/node/pull/49659)
* \[[`6b76b7782c`](https://github.com/nodejs/node/commit/6b76b7782c)] - **test**: skip v8-updates/test-linux-perf (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`c13c98dd38`](https://github.com/nodejs/node/commit/c13c98dd38)] - **test**: skip test-tick-processor-arguments on SmartOS (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`738aa304b3`](https://github.com/nodejs/node/commit/738aa304b3)] - **test**: adapt REPL test to V8 changes (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`de5c009252`](https://github.com/nodejs/node/commit/de5c009252)] - **test**: adapt test-fs-write to V8 internal changes (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`8c36168b42`](https://github.com/nodejs/node/commit/8c36168b42)] - **test**: update flag to disable SharedArrayBuffer (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`6ccb15f7ef`](https://github.com/nodejs/node/commit/6ccb15f7ef)] - **test**: adapt debugger tests to V8 11.4 (Philip Pfaffe) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`c5de3b49e8`](https://github.com/nodejs/node/commit/c5de3b49e8)] - **test,crypto**: update WebCryptoAPI WPT (Filip Skokan) [#50039](https://github.com/nodejs/node/pull/50039)
* \[[`4b35a9cfda`](https://github.com/nodejs/node/commit/4b35a9cfda)] - **test\_runner**: add test location for FileTests (Colin Ihrig) [#49999](https://github.com/nodejs/node/pull/49999)
* \[[`c935d4c8fa`](https://github.com/nodejs/node/commit/c935d4c8fa)] - **test\_runner**: replace spurious if with else (Colin Ihrig) [#49943](https://github.com/nodejs/node/pull/49943)
* \[[`a4c7f81241`](https://github.com/nodejs/node/commit/a4c7f81241)] - **test\_runner**: catch reporter errors (Moshe Atlow) [#49646](https://github.com/nodejs/node/pull/49646)
* \[[`bb52656fc6`](https://github.com/nodejs/node/commit/bb52656fc6)] - _**Revert**_ "**test\_runner**: run global after() hook earlier" (Joyee Cheung) [#49110](https://github.com/nodejs/node/pull/49110)
* \[[`6346bdc526`](https://github.com/nodejs/node/commit/6346bdc526)] - **test\_runner**: run global after() hook earlier (Colin Ihrig) [#49059](https://github.com/nodejs/node/pull/49059)
* \[[`0d8faf2952`](https://github.com/nodejs/node/commit/0d8faf2952)] - **test\_runner,test**: fix flaky test-runner-cli-concurrency.js (Colin Ihrig) [#50108](https://github.com/nodejs/node/pull/50108)
* \[[`b1ada0ad55`](https://github.com/nodejs/node/commit/b1ada0ad55)] - **tls**: handle cases where the raw socket is destroyed (Luigi Pinca) [#49980](https://github.com/nodejs/node/pull/49980)
* \[[`fae1af0a75`](https://github.com/nodejs/node/commit/fae1af0a75)] - **tls**: ciphers allow bang syntax (Chemi Atlow) [#49712](https://github.com/nodejs/node/pull/49712)
* \[[`766198b9e1`](https://github.com/nodejs/node/commit/766198b9e1)] - **tools**: fix comments referencing dep\_updaters scripts (Keksonoid) [#50165](https://github.com/nodejs/node/pull/50165)
* \[[`760b5dd259`](https://github.com/nodejs/node/commit/760b5dd259)] - **tools**: remove no-return-await lint rule (翠 / green) [#50118](https://github.com/nodejs/node/pull/50118)
* \[[`a0a5b751fb`](https://github.com/nodejs/node/commit/a0a5b751fb)] - **tools**: update lint-md-dependencies (Node.js GitHub Bot) [#50083](https://github.com/nodejs/node/pull/50083)
* \[[`69fb55e6b9`](https://github.com/nodejs/node/commit/69fb55e6b9)] - **tools**: update eslint to 8.51.0 (Node.js GitHub Bot) [#50084](https://github.com/nodejs/node/pull/50084)
* \[[`f73650ea52`](https://github.com/nodejs/node/commit/f73650ea52)] - **tools**: remove genv8constants.py (Ben Noordhuis) [#50023](https://github.com/nodejs/node/pull/50023)
* \[[`581434e54f`](https://github.com/nodejs/node/commit/581434e54f)] - **tools**: update eslint to 8.50.0 (Node.js GitHub Bot) [#49989](https://github.com/nodejs/node/pull/49989)
* \[[`344d3c4b7c`](https://github.com/nodejs/node/commit/344d3c4b7c)] - **tools**: update lint-md-dependencies (Node.js GitHub Bot) [#49983](https://github.com/nodejs/node/pull/49983)
* \[[`7f06c270c6`](https://github.com/nodejs/node/commit/7f06c270c6)] - **tools**: add navigation ARIA landmark to generated API ToC (Rich Trott) [#49882](https://github.com/nodejs/node/pull/49882)
* \[[`e97d25687b`](https://github.com/nodejs/node/commit/e97d25687b)] - **tools**: use osx notarytool for future releases (Ulises Gascon) [#48701](https://github.com/nodejs/node/pull/48701)
* \[[`3f1936f698`](https://github.com/nodejs/node/commit/3f1936f698)] - **tools**: update github\_reporter to 1.5.3 (Node.js GitHub Bot) [#49877](https://github.com/nodejs/node/pull/49877)
* \[[`8568de3da6`](https://github.com/nodejs/node/commit/8568de3da6)] - **tools**: add new V8 headers to distribution (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`86cb23d09f`](https://github.com/nodejs/node/commit/86cb23d09f)] - **tools**: update V8 gypfiles for 11.8 (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`9c6219c7e2`](https://github.com/nodejs/node/commit/9c6219c7e2)] - **tools**: update V8 gypfiles for 11.7 (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`73ddf50163`](https://github.com/nodejs/node/commit/73ddf50163)] - **tools**: update V8 gypfiles for 11.6 (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`817ef255ea`](https://github.com/nodejs/node/commit/817ef255ea)] - **tools**: update V8 gypfiles for 11.5 (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`f34a3a9861`](https://github.com/nodejs/node/commit/f34a3a9861)] - **tools**: update V8 gypfiles for 11.4 (Michaël Zasso) [#49639](https://github.com/nodejs/node/pull/49639)
* \[[`9df864ddeb`](https://github.com/nodejs/node/commit/9df864ddeb)] - **typings**: use `Symbol.dispose` and `Symbol.asyncDispose` in types (Niklas Mollenhauer) [#50123](https://github.com/nodejs/node/pull/50123)
* \[[`54bb691c0b`](https://github.com/nodejs/node/commit/54bb691c0b)] - **util**: lazy parse mime parameters (Aras Abbasi) [#49889](https://github.com/nodejs/node/pull/49889)
* \[[`1d220b55ac`](https://github.com/nodejs/node/commit/1d220b55ac)] - **vm**: use default HDO when importModuleDynamically is not set (Joyee Cheung) [#49950](https://github.com/nodejs/node/pull/49950)
* \[[`c1a3a98560`](https://github.com/nodejs/node/commit/c1a3a98560)] - **wasi**: address coverity warning (Michael Dawson) [#49866](https://github.com/nodejs/node/pull/49866)
* \[[`9cb8eb7177`](https://github.com/nodejs/node/commit/9cb8eb7177)] - **wasi**: fix up wasi tests for ibmi (Michael Dawson) [#49953](https://github.com/nodejs/node/pull/49953)
* \[[`16ac5e1ca8`](https://github.com/nodejs/node/commit/16ac5e1ca8)] - **zlib**: fix discovery of cpu-features.h for android (MatteoBax) [#49828](https://github.com/nodejs/node/pull/49828)
