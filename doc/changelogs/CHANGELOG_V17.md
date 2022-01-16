# Node.js 17 ChangeLog

<!--lint disable maximum-line-length no-literal-urls prohibited-strings-->

<table>
<tr>
<th>Current</th>
</tr>
<tr>
<td>
<a href="#17.4.0">17.4.0</a><br/>
<a href="#17.3.1">17.3.1</a><br/>
<a href="#17.3.0">17.3.0</a><br/>
<a href="#17.2.0">17.2.0</a><br/>
<a href="#17.1.0">17.1.0</a><br/>
<a href="#17.0.1">17.0.1</a><br/>
<a href="#17.0.0">17.0.0</a><br/>
</td>
</tr>
</table>

* Other Versions
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

<a id="17.4.0"></a>

## 2022-01-18, Version 17.4.0 (Current), @targos

### Notable Changes

* \[[`ef6f98c2e3`](https://github.com/nodejs/node/commit/ef6f98c2e3)] - **(SEMVER-MINOR)** **child\_process**: add support for URL to `cp.fork` (Antoine du Hamel) [#41225](https://github.com/nodejs/node/pull/41225)
* \[[`d62fe315c2`](https://github.com/nodejs/node/commit/d62fe315c2)] - **(SEMVER-MINOR)** **crypto**: alias webcrypto.subtle and webcrypto.getRandomValues on crypto (James M Snell) [#41266](https://github.com/nodejs/node/pull/41266)
* \[[`fcb37e9ce5`](https://github.com/nodejs/node/commit/fcb37e9ce5)] - **doc**: add Mesteery to collaborators (Mestery) [#41543](https://github.com/nodejs/node/pull/41543)
* \[[`4079fc42b7`](https://github.com/nodejs/node/commit/4079fc42b7)] - **(SEMVER-MINOR)** **events**: graduate capturerejections to supported (James M Snell) [#41267](https://github.com/nodejs/node/pull/41267)
* \[[`fe21607901`](https://github.com/nodejs/node/commit/fe21607901)] - **(SEMVER-MINOR)** **events**: add EventEmitterAsyncResource to core (James M Snell) [#41246](https://github.com/nodejs/node/pull/41246)
* \[[`6d8eb6ace6`](https://github.com/nodejs/node/commit/6d8eb6ace6)] - **(SEMVER-MINOR)** **loader**: return package format from defaultResolve if known (Gabriel Bota) [#40980](https://github.com/nodejs/node/pull/40980)
* \[[`27c619140a`](https://github.com/nodejs/node/commit/27c619140a)] - **(SEMVER-MINOR)** **perf\_hooks**: multiple fixes for Histogram (James M Snell) [#41153](https://github.com/nodejs/node/pull/41153)
* \[[`09c25bb224`](https://github.com/nodejs/node/commit/09c25bb224)] - **(SEMVER-MINOR)** **stream**: add filter method to readable (Benjamin Gruenbaum, Robert Nagy) [#41354](https://github.com/nodejs/node/pull/41354)
* \[[`1150963217`](https://github.com/nodejs/node/commit/1150963217)] - **(SEMVER-MINOR)** **stream**: add isReadable helper (Robert Nagy) [#41199](https://github.com/nodejs/node/pull/41199)
* \[[`9f5a873965`](https://github.com/nodejs/node/commit/9f5a873965)] - **(SEMVER-MINOR)** **stream**: add map method to Readable (Benjamin Gruenbaum, Robert Nagy) [#40815](https://github.com/nodejs/node/pull/40815)

### Commits

* \[[`314102b14d`](https://github.com/nodejs/node/commit/314102b14d)] - **async\_hooks**: add missing initialization (Michael Dawson) [#41288](https://github.com/nodejs/node/pull/41288)
* \[[`56345a3f63`](https://github.com/nodejs/node/commit/56345a3f63)] - **async\_hooks**: fix AsyncLocalStorage in unhandledRejection cases (Bradley Farias) [#41202](https://github.com/nodejs/node/pull/41202)
* \[[`fa84353952`](https://github.com/nodejs/node/commit/fa84353952)] - **benchmark**: simplify http benchmarker regular expression (Rich Trott) [#38206](https://github.com/nodejs/node/pull/38206)
* \[[`88d760c559`](https://github.com/nodejs/node/commit/88d760c559)] - **benchmark**: fix benchmark/run.js handling of --set (Rich Trott) [#41334](https://github.com/nodejs/node/pull/41334)
* \[[`dcf1ea0a3f`](https://github.com/nodejs/node/commit/dcf1ea0a3f)] - **benchmark,test**: use Object.hasOwn() where applicable (Rich Trott) [#41229](https://github.com/nodejs/node/pull/41229)
* \[[`4958c800da`](https://github.com/nodejs/node/commit/4958c800da)] - **build**: fix npm version detection (Michaël Zasso) [#41575](https://github.com/nodejs/node/pull/41575)
* \[[`e8538c3751`](https://github.com/nodejs/node/commit/e8538c3751)] - **build**: fix node build failures in WSL Ubuntu (MrJithil) [#41476](https://github.com/nodejs/node/pull/41476)
* \[[`3d88ea195e`](https://github.com/nodejs/node/commit/3d88ea195e)] - **build**: fix workflow access to git history (Rich Trott) [#41472](https://github.com/nodejs/node/pull/41472)
* \[[`b0f0ad1004`](https://github.com/nodejs/node/commit/b0f0ad1004)] - **build**: start build on z/OS (alexcfyung) [#41273](https://github.com/nodejs/node/pull/41273)
* \[[`80a3766ac7`](https://github.com/nodejs/node/commit/80a3766ac7)] - **build**: use list for mutable retval rather than tuple (Rich Trott) [#41372](https://github.com/nodejs/node/pull/41372)
* \[[`afe1e00509`](https://github.com/nodejs/node/commit/afe1e00509)] - **build**: remove Python 2 workaround (Rich Trott) [#41357](https://github.com/nodejs/node/pull/41357)
* \[[`011c5f6498`](https://github.com/nodejs/node/commit/011c5f6498)] - **build**: improve readability of texts in workflows (Mestery) [#40988](https://github.com/nodejs/node/pull/40988)
* \[[`2233f31069`](https://github.com/nodejs/node/commit/2233f31069)] - **build,tools,win**: trim unused VCBUILD\_PYTHON\_LOCATION variable (David Sanders) [#41235](https://github.com/nodejs/node/pull/41235)
* \[[`d9465ae614`](https://github.com/nodejs/node/commit/d9465ae614)] - **child\_process**: queue pending messages (Erick Wendel) [#41221](https://github.com/nodejs/node/pull/41221)
* \[[`ed41fd110d`](https://github.com/nodejs/node/commit/ed41fd110d)] - **child\_process**: revise argument processing (Rich Trott) [#41280](https://github.com/nodejs/node/pull/41280)
* \[[`ef6f98c2e3`](https://github.com/nodejs/node/commit/ef6f98c2e3)] - **(SEMVER-MINOR)** **child\_process**: add support for URL to `cp.fork` (Antoine du Hamel) [#41225](https://github.com/nodejs/node/pull/41225)
* \[[`d62fe315c2`](https://github.com/nodejs/node/commit/d62fe315c2)] - **(SEMVER-MINOR)** **crypto**: alias webcrypto.subtle and webcrypto.getRandomValues on crypto (James M Snell) [#41266](https://github.com/nodejs/node/pull/41266)
* \[[`8ea56a9606`](https://github.com/nodejs/node/commit/8ea56a9606)] - **crypto**: fix error capture when loading engine (Tobias Nießen) [#41187](https://github.com/nodejs/node/pull/41187)
* \[[`f5b8aee1a1`](https://github.com/nodejs/node/commit/f5b8aee1a1)] - **deps**: upgrade npm to 8.3.1 (npm-robot) [#41503](https://github.com/nodejs/node/pull/41503)
* \[[`af3acecd7e`](https://github.com/nodejs/node/commit/af3acecd7e)] - **deps**: V8: cherry-pick 3b6b21f595f6 (Michaël Zasso) [#41457](https://github.com/nodejs/node/pull/41457)
* \[[`02ca5d7c7c`](https://github.com/nodejs/node/commit/02ca5d7c7c)] - **deps**: upgrade to libuv 1.43.0 (Colin Ihrig) [#41398](https://github.com/nodejs/node/pull/41398)
* \[[`48e4780fd7`](https://github.com/nodejs/node/commit/48e4780fd7)] - **doc**: remove statement about client private keys (Tobias Nießen) [#41505](https://github.com/nodejs/node/pull/41505)
* \[[`ba7160e815`](https://github.com/nodejs/node/commit/ba7160e815)] - **doc**: fix typo in `onboarding.md` (Antoine du Hamel) [#41544](https://github.com/nodejs/node/pull/41544)
* \[[`fcb37e9ce5`](https://github.com/nodejs/node/commit/fcb37e9ce5)] - **doc**: add Mesteery to collaborators (Mestery) [#41543](https://github.com/nodejs/node/pull/41543)
* \[[`abbfed8789`](https://github.com/nodejs/node/commit/abbfed8789)] - **doc**: add missing word in readable.read() text (Rich Trott) [#41524](https://github.com/nodejs/node/pull/41524)
* \[[`712dfdc11f`](https://github.com/nodejs/node/commit/712dfdc11f)] - **doc**: add missing YAML tag in `esm.md` (Antoine du Hamel) [#41516](https://github.com/nodejs/node/pull/41516)
* \[[`f443a4e8fa`](https://github.com/nodejs/node/commit/f443a4e8fa)] - **doc**: expand fs.access() mode parameter docs (Colin Ihrig) [#41484](https://github.com/nodejs/node/pull/41484)
* \[[`5c0c459976`](https://github.com/nodejs/node/commit/5c0c459976)] - **doc**: correct checkHost behavior with wildcards etc (Tobias Nießen) [#41468](https://github.com/nodejs/node/pull/41468)
* \[[`c632241440`](https://github.com/nodejs/node/commit/c632241440)] - **doc**: remove extraneous colon in legacy subject (Tobias Nießen) [#41477](https://github.com/nodejs/node/pull/41477)
* \[[`b7b0631b10`](https://github.com/nodejs/node/commit/b7b0631b10)] - **doc**: remove SameValue comparison reference (Rich Trott) [#41460](https://github.com/nodejs/node/pull/41460)
* \[[`524103d6bf`](https://github.com/nodejs/node/commit/524103d6bf)] - **doc**: update mailmap entries for mhdawson (Michael Dawson) [#41437](https://github.com/nodejs/node/pull/41437)
* \[[`62aa190c01`](https://github.com/nodejs/node/commit/62aa190c01)] - **doc**: add guidance on order vulns are listed in (Michael Dawson) [#41429](https://github.com/nodejs/node/pull/41429)
* \[[`d721a758b2`](https://github.com/nodejs/node/commit/d721a758b2)] - **doc**: update output in inspector examples (David Sanders) [#41390](https://github.com/nodejs/node/pull/41390)
* \[[`60025bde16`](https://github.com/nodejs/node/commit/60025bde16)] - **doc**: add note regarding unfinished TLA (Antoine du Hamel) [#41434](https://github.com/nodejs/node/pull/41434)
* \[[`10bdb5969e`](https://github.com/nodejs/node/commit/10bdb5969e)] - **doc**: add reference for `===` operator in assert.md (Rich Trott) [#41442](https://github.com/nodejs/node/pull/41442)
* \[[`edc6a7af42`](https://github.com/nodejs/node/commit/edc6a7af42)] - **doc**: clarify `uncaughtException` `origin` for ESM (Antoine du Hamel) [#41339](https://github.com/nodejs/node/pull/41339)
* \[[`4a369d03b4`](https://github.com/nodejs/node/commit/4a369d03b4)] - **doc**: revise HTTPRequestOptions text (Rich Trott) [#41407](https://github.com/nodejs/node/pull/41407)
* \[[`f43bfe2e16`](https://github.com/nodejs/node/commit/f43bfe2e16)] - **doc**: add reference for == and != operators (Rich Trott) [#41413](https://github.com/nodejs/node/pull/41413)
* \[[`d3111bf0cc`](https://github.com/nodejs/node/commit/d3111bf0cc)] - **doc**: add @RaisinTen to the TSC (Michael Dawson) [#41419](https://github.com/nodejs/node/pull/41419)
* \[[`e6bed4e972`](https://github.com/nodejs/node/commit/e6bed4e972)] - **doc**: update Abstract Equality Comparison text in assert.md (Rich Trott) [#41375](https://github.com/nodejs/node/pull/41375)
* \[[`19db19bb80`](https://github.com/nodejs/node/commit/19db19bb80)] - **doc**: fix example commands for `REPLACEME` updates (Richard Lau) [#41269](https://github.com/nodejs/node/pull/41269)
* \[[`16c0bea91d`](https://github.com/nodejs/node/commit/16c0bea91d)] - **doc**: document that `require.main` may be `undefined` (Antoine du Hamel) [#41384](https://github.com/nodejs/node/pull/41384)
* \[[`014d4836ec`](https://github.com/nodejs/node/commit/014d4836ec)] - **doc**: clarify entry point behavior when using loader hooks (Antoine du Hamel) [#41304](https://github.com/nodejs/node/pull/41304)
* \[[`6460b1b32d`](https://github.com/nodejs/node/commit/6460b1b32d)] - **doc**: clarify `require` behavior with non `.js` extensions (Antoine du Hamel) [#41345](https://github.com/nodejs/node/pull/41345)
* \[[`0d18a8c232`](https://github.com/nodejs/node/commit/0d18a8c232)] - **doc**: revise frozen-intrinsics text (Rich Trott) [#41342](https://github.com/nodejs/node/pull/41342)
* \[[`c267bb2192`](https://github.com/nodejs/node/commit/c267bb2192)] - **doc**: fix example description for worker\_threads (Dmitry Petrov) [#41341](https://github.com/nodejs/node/pull/41341)
* \[[`ffe17a84f2`](https://github.com/nodejs/node/commit/ffe17a84f2)] - **doc**: make pull-request guide default branch agnostic (Antoine du Hamel) [#41299](https://github.com/nodejs/node/pull/41299)
* \[[`5cfc547997`](https://github.com/nodejs/node/commit/5cfc547997)] - **doc**: fix sync comment in observer snippet (Eric Jacobson) [#41262](https://github.com/nodejs/node/pull/41262)
* \[[`3a80104b29`](https://github.com/nodejs/node/commit/3a80104b29)] - **doc**: remove section about amending commits in PR guide (Thiago Santos) [#41287](https://github.com/nodejs/node/pull/41287)
* \[[`23f97ec04e`](https://github.com/nodejs/node/commit/23f97ec04e)] - **doc**: remove legacy in-page links in v8.md (Rich Trott) [#41291](https://github.com/nodejs/node/pull/41291)
* \[[`e819685cec`](https://github.com/nodejs/node/commit/e819685cec)] - **doc**: include stack trace difference in ES modules (Marcos Bérgamo) [#41157](https://github.com/nodejs/node/pull/41157)
* \[[`dac8407944`](https://github.com/nodejs/node/commit/dac8407944)] - **doc**: fix example in node-api docs (Michael Dawson) [#41264](https://github.com/nodejs/node/pull/41264)
* \[[`29563abd85`](https://github.com/nodejs/node/commit/29563abd85)] - **doc**: add usage recommendation for writable.\_destroy (Rafael Gonzaga) [#41040](https://github.com/nodejs/node/pull/41040)
* \[[`e27e8272f7`](https://github.com/nodejs/node/commit/e27e8272f7)] - **doc**: make function signature comply with JSDoc comment (Rich Trott) [#41242](https://github.com/nodejs/node/pull/41242)
* \[[`d83a02994c`](https://github.com/nodejs/node/commit/d83a02994c)] - **doc**: align maxHeaderSize default with current value (Gil Pedersen) [#41183](https://github.com/nodejs/node/pull/41183)
* \[[`730e25d7dd`](https://github.com/nodejs/node/commit/730e25d7dd)] - **doc**: add unhandledRejection to strict mode (Colin Ihrig) [#41194](https://github.com/nodejs/node/pull/41194)
* \[[`74742c3618`](https://github.com/nodejs/node/commit/74742c3618)] - **doc**: adding estimated execution time (mawaregetsuka) [#41142](https://github.com/nodejs/node/pull/41142)
* \[[`34ef5a7d4d`](https://github.com/nodejs/node/commit/34ef5a7d4d)] - **doc**: fix syntax error in nested conditions example (Mateusz Burzyński) [#41205](https://github.com/nodejs/node/pull/41205)
* \[[`c9a4603913`](https://github.com/nodejs/node/commit/c9a4603913)] - **esm**: make `process.exit()` default to exit code 0 (Gang Chen) [#41388](https://github.com/nodejs/node/pull/41388)
* \[[`8a94ca7a69`](https://github.com/nodejs/node/commit/8a94ca7a69)] - **esm**: refactor esm tests out of test/message (Geoffrey Booth) [#41352](https://github.com/nodejs/node/pull/41352)
* \[[`5ebe086ea6`](https://github.com/nodejs/node/commit/5ebe086ea6)] - **esm**: reconcile JSDoc vs. actual parameter name (Rich Trott) [#41238](https://github.com/nodejs/node/pull/41238)
* \[[`9fe304b8e8`](https://github.com/nodejs/node/commit/9fe304b8e8)] - **events**: clarify JSDoc entries (Rich Trott) [#41311](https://github.com/nodejs/node/pull/41311)
* \[[`4079fc42b7`](https://github.com/nodejs/node/commit/4079fc42b7)] - **(SEMVER-MINOR)** **events**: graduate capturerejections to supported (James M Snell) [#41267](https://github.com/nodejs/node/pull/41267)
* \[[`e3a0a9cb3a`](https://github.com/nodejs/node/commit/e3a0a9cb3a)] - **events**: add jsdoc details for Event and EventTarget (James M Snell) [#41274](https://github.com/nodejs/node/pull/41274)
* \[[`fe21607901`](https://github.com/nodejs/node/commit/fe21607901)] - **(SEMVER-MINOR)** **events**: add EventEmitterAsyncResource to core (James M Snell) [#41246](https://github.com/nodejs/node/pull/41246)
* \[[`d4a6f2caf1`](https://github.com/nodejs/node/commit/d4a6f2caf1)] - **fs**: use async directory processing in cp() (Colin Ihrig) [#41351](https://github.com/nodejs/node/pull/41351)
* \[[`0951bd94db`](https://github.com/nodejs/node/commit/0951bd94db)] - **fs**: correct param names in JSDoc comments (Rich Trott) [#41237](https://github.com/nodejs/node/pull/41237)
* \[[`1d75436a1c`](https://github.com/nodejs/node/commit/1d75436a1c)] - **http**: remove duplicate code (Shaw) [#39239](https://github.com/nodejs/node/pull/39239)
* \[[`0aacd4926d`](https://github.com/nodejs/node/commit/0aacd4926d)] - **http2**: handle existing socket data when creating HTTP/2 server sessions (Tim Perry) [#41185](https://github.com/nodejs/node/pull/41185)
* \[[`24fbbf2747`](https://github.com/nodejs/node/commit/24fbbf2747)] - **lib**: remove spurious JSDoc entry (Rich Trott) [#41240](https://github.com/nodejs/node/pull/41240)
* \[[`e457ec05d6`](https://github.com/nodejs/node/commit/e457ec05d6)] - **lib**: fix checking syntax of esm module (Qingyu Deng) [#41198](https://github.com/nodejs/node/pull/41198)
* \[[`f176124e8b`](https://github.com/nodejs/node/commit/f176124e8b)] - **lib,tools**: remove empty lines between JSDoc tags (Rich Trott) [#41147](https://github.com/nodejs/node/pull/41147)
* \[[`68fd2ac999`](https://github.com/nodejs/node/commit/68fd2ac999)] - **loader**: fix package resolution for edge case (Gabriel Bota) [#41218](https://github.com/nodejs/node/pull/41218)
* \[[`6d8eb6ace6`](https://github.com/nodejs/node/commit/6d8eb6ace6)] - **(SEMVER-MINOR)** **loader**: return package format from defaultResolve if known (Gabriel Bota) [#40980](https://github.com/nodejs/node/pull/40980)
* \[[`a6146c7e27`](https://github.com/nodejs/node/commit/a6146c7e27)] - **meta**: move one or more collaborators to emeritus (Node.js GitHub Bot) [#41456](https://github.com/nodejs/node/pull/41456)
* \[[`07353e9b8b`](https://github.com/nodejs/node/commit/07353e9b8b)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#41475](https://github.com/nodejs/node/pull/41475)
* \[[`e1ff4521d7`](https://github.com/nodejs/node/commit/e1ff4521d7)] - **meta**: correct my name in AUTHORS (Jacob Smith) [#41444](https://github.com/nodejs/node/pull/41444)
* \[[`da1d5d6563`](https://github.com/nodejs/node/commit/da1d5d6563)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#41449](https://github.com/nodejs/node/pull/41449)
* \[[`0f9afa58d5`](https://github.com/nodejs/node/commit/0f9afa58d5)] - **meta**: add required fields in issue templates (Rich Trott) [#41378](https://github.com/nodejs/node/pull/41378)
* \[[`da04408075`](https://github.com/nodejs/node/commit/da04408075)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#41374](https://github.com/nodejs/node/pull/41374)
* \[[`1f6c4e819b`](https://github.com/nodejs/node/commit/1f6c4e819b)] - **meta**: replace API docs issue template with form (Rich Trott) [#41348](https://github.com/nodejs/node/pull/41348)
* \[[`253c3e5488`](https://github.com/nodejs/node/commit/253c3e5488)] - **meta**: move one or more collaborators to emeritus (Node.js GitHub Bot) [#41336](https://github.com/nodejs/node/pull/41336)
* \[[`3e188cacc2`](https://github.com/nodejs/node/commit/3e188cacc2)] - **meta**: replace feature request template with form (Rich Trott) [#41317](https://github.com/nodejs/node/pull/41317)
* \[[`e339220511`](https://github.com/nodejs/node/commit/e339220511)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#41322](https://github.com/nodejs/node/pull/41322)
* \[[`d0d595f8f2`](https://github.com/nodejs/node/commit/d0d595f8f2)] - **meta**: update node-api team name (Richard Lau) [#41268](https://github.com/nodejs/node/pull/41268)
* \[[`a53fa2010b`](https://github.com/nodejs/node/commit/a53fa2010b)] - **meta**: move one or more collaborators to emeritus (Node.js GitHub Bot) [#41248](https://github.com/nodejs/node/pull/41248)
* \[[`edefb41ec1`](https://github.com/nodejs/node/commit/edefb41ec1)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#41234](https://github.com/nodejs/node/pull/41234)
* \[[`6da7909797`](https://github.com/nodejs/node/commit/6da7909797)] - **meta**: remove community-committee from CODEOWNERS (Rich Trott) [#41169](https://github.com/nodejs/node/pull/41169)
* \[[`5fe011a24d`](https://github.com/nodejs/node/commit/5fe011a24d)] - **node-api**: add missing initialization of last error (Michael Dawson) [#41290](https://github.com/nodejs/node/pull/41290)
* \[[`27c619140a`](https://github.com/nodejs/node/commit/27c619140a)] - **(SEMVER-MINOR)** **perf\_hooks**: multiple fixes for Histogram (James M Snell) [#41153](https://github.com/nodejs/node/pull/41153)
* \[[`e60187d8ab`](https://github.com/nodejs/node/commit/e60187d8ab)] - **policy**: add missing JSDoc @param entry (Rich Trott) [#41239](https://github.com/nodejs/node/pull/41239)
* \[[`ec9071f55d`](https://github.com/nodejs/node/commit/ec9071f55d)] - **src**: use `std::optional` for Worker thread id (Anna Henningsen) [#41453](https://github.com/nodejs/node/pull/41453)
* \[[`6aec92f959`](https://github.com/nodejs/node/commit/6aec92f959)] - **src**: gracefully handle errors in GetX509NameObject (Tobias Nießen) [#41490](https://github.com/nodejs/node/pull/41490)
* \[[`7ac126b75c`](https://github.com/nodejs/node/commit/7ac126b75c)] - **src**: fix out-of-bounds check of serialization indices (JoostK) [#41452](https://github.com/nodejs/node/pull/41452)
* \[[`93b3664b9a`](https://github.com/nodejs/node/commit/93b3664b9a)] - **src**: do IWYU for some STL includes (David Sanders) [#41236](https://github.com/nodejs/node/pull/41236)
* \[[`337ebfcd53`](https://github.com/nodejs/node/commit/337ebfcd53)] - **src**: split out async stack corruption detection from inline fn (Anna Henningsen) [#41331](https://github.com/nodejs/node/pull/41331)
* \[[`250e197a62`](https://github.com/nodejs/node/commit/250e197a62)] - **src**: store native async execution resources as `v8::Local` (Anna Henningsen) [#41331](https://github.com/nodejs/node/pull/41331)
* \[[`6187e81a8e`](https://github.com/nodejs/node/commit/6187e81a8e)] - **src**: guard slightly costly check in MakeCallback more strongly (Anna Henningsen) [#41331](https://github.com/nodejs/node/pull/41331)
* \[[`51d86fe6a0`](https://github.com/nodejs/node/commit/51d86fe6a0)] - **stream**: remove always-false condition check (Rich Trott) [#41488](https://github.com/nodejs/node/pull/41488)
* \[[`b08138f367`](https://github.com/nodejs/node/commit/b08138f367)] - **stream**: fix error-path function call (Rich Trott) [#41433](https://github.com/nodejs/node/pull/41433)
* \[[`d79f8c2987`](https://github.com/nodejs/node/commit/d79f8c2987)] - **stream**: remove unused function argument (Rich Trott) [#41403](https://github.com/nodejs/node/pull/41403)
* \[[`09c25bb224`](https://github.com/nodejs/node/commit/09c25bb224)] - **(SEMVER-MINOR)** **stream**: add filter method to readable (Benjamin Gruenbaum) [#41354](https://github.com/nodejs/node/pull/41354)
* \[[`1150963217`](https://github.com/nodejs/node/commit/1150963217)] - **(SEMVER-MINOR)** **stream**: add isReadable helper (Robert Nagy) [#41199](https://github.com/nodejs/node/pull/41199)
* \[[`9f5a873965`](https://github.com/nodejs/node/commit/9f5a873965)] - **(SEMVER-MINOR)** **stream**: add map method to Readable (Benjamin Gruenbaum) [#40815](https://github.com/nodejs/node/pull/40815)
* \[[`3dc65646c8`](https://github.com/nodejs/node/commit/3dc65646c8)] - **stream**: fix enqueue race condition on esm modules (Rafael Gonzaga) [#40901](https://github.com/nodejs/node/pull/40901)
* \[[`09f2fd36a4`](https://github.com/nodejs/node/commit/09f2fd36a4)] - **test**: improve test coverage of dns/promises (Yoshiki Kurihara) [#41425](https://github.com/nodejs/node/pull/41425)
* \[[`106ef0cef4`](https://github.com/nodejs/node/commit/106ef0cef4)] - **test**: remove broken wiki link from test/common doc (Yoshiki Kurihara) [#41426](https://github.com/nodejs/node/pull/41426)
* \[[`9d8d7c63cb`](https://github.com/nodejs/node/commit/9d8d7c63cb)] - **test**: do not OR F\_OK in fs.access() test (Colin Ihrig) [#41484](https://github.com/nodejs/node/pull/41484)
* \[[`3e2154deda`](https://github.com/nodejs/node/commit/3e2154deda)] - **test**: mark test-performance-eventloopdelay flaky (Michael Dawson) [#41409](https://github.com/nodejs/node/pull/41409)
* \[[`e808ee68d0`](https://github.com/nodejs/node/commit/e808ee68d0)] - **test**: mark test-repl-sigint-nested-eval as flaky (Michael Dawson) [#41302](https://github.com/nodejs/node/pull/41302)
* \[[`f97f6c585d`](https://github.com/nodejs/node/commit/f97f6c585d)] - **test**: use spawnSync() full name in test-stdio-pipe-stderr (Rich Trott) [#41332](https://github.com/nodejs/node/pull/41332)
* \[[`75c565bf18`](https://github.com/nodejs/node/commit/75c565bf18)] - **test**: improve expectWarning error message (Rich Trott) [#41326](https://github.com/nodejs/node/pull/41326)
* \[[`c136d597f0`](https://github.com/nodejs/node/commit/c136d597f0)] - **test**: use spawnSync() full name (Rich Trott) [#41327](https://github.com/nodejs/node/pull/41327)
* \[[`b2a87f770d`](https://github.com/nodejs/node/commit/b2a87f770d)] - **test**: add comments explaining \_setSimultaneousAccepts deprecation tests (Yoshiki Kurihara) [#41307](https://github.com/nodejs/node/pull/41307)
* \[[`fac0871102`](https://github.com/nodejs/node/commit/fac0871102)] - **test**: mark test-worker-take-heapsnapshot flaky (Michael Dawson) [#41253](https://github.com/nodejs/node/pull/41253)
* \[[`90617b9303`](https://github.com/nodejs/node/commit/90617b9303)] - **test**: mark wpt/test-user-timing test flaky (Michael Dawson) [#41203](https://github.com/nodejs/node/pull/41203)
* \[[`8f08328a01`](https://github.com/nodejs/node/commit/8f08328a01)] - **test**: correct param name in JSDoc comment (Rich Trott) [#41241](https://github.com/nodejs/node/pull/41241)
* \[[`367ab2a55e`](https://github.com/nodejs/node/commit/367ab2a55e)] - **test**: mark test-crypto-keygen slow on windows (Michael Dawson) [#41207](https://github.com/nodejs/node/pull/41207)
* \[[`f067876338`](https://github.com/nodejs/node/commit/f067876338)] - **test**: improve test coverage of dns/promises (Yoshiki Kurihara) [#41133](https://github.com/nodejs/node/pull/41133)
* \[[`2e92f6f5d9`](https://github.com/nodejs/node/commit/2e92f6f5d9)] - **timers**: use ref counts to count timers (Darshan Sen) [#41231](https://github.com/nodejs/node/pull/41231)
* \[[`3c8b25bec8`](https://github.com/nodejs/node/commit/3c8b25bec8)] - **tls**: use optional chaining to simplify checks (Antoine du Hamel) [#41337](https://github.com/nodejs/node/pull/41337)
* \[[`a11ff31bca`](https://github.com/nodejs/node/commit/a11ff31bca)] - **tls**: permit null as a pfx value (CallMeLaNN) [#41170](https://github.com/nodejs/node/pull/41170)
* \[[`5129b7c802`](https://github.com/nodejs/node/commit/5129b7c802)] - **tools**: fix small not-quite-a-bug in find-inactive-tsc.mjs (Rich Trott) [#41469](https://github.com/nodejs/node/pull/41469)
* \[[`258ee4ba64`](https://github.com/nodejs/node/commit/258ee4ba64)] - **tools**: enable ESLint recommended configuration (Rich Trott) [#41463](https://github.com/nodejs/node/pull/41463)
* \[[`090a674a81`](https://github.com/nodejs/node/commit/090a674a81)] - **tools**: enable ESLint no-constant-condition rule (Rich Trott) [#41463](https://github.com/nodejs/node/pull/41463)
* \[[`1f4369a106`](https://github.com/nodejs/node/commit/1f4369a106)] - **tools**: enable ESLint require-yield rule (Rich Trott) [#41463](https://github.com/nodejs/node/pull/41463)
* \[[`8090ce7a6c`](https://github.com/nodejs/node/commit/8090ce7a6c)] - **tools**: enable ESLint no-sparse-arrays rule (Rich Trott) [#41463](https://github.com/nodejs/node/pull/41463)
* \[[`afa4f37faf`](https://github.com/nodejs/node/commit/afa4f37faf)] - **tools**: enable ESLint no-loss-of-precision rule (Rich Trott) [#41463](https://github.com/nodejs/node/pull/41463)
* \[[`ec337b2019`](https://github.com/nodejs/node/commit/ec337b2019)] - **tools**: replace for loop with map() (Rich Trott) [#41451](https://github.com/nodejs/node/pull/41451)
* \[[`c91ac205a5`](https://github.com/nodejs/node/commit/c91ac205a5)] - **tools**: use GITHUB\_ACTIONS env var in inactivity scripts (Rich Trott) [#41422](https://github.com/nodejs/node/pull/41422)
* \[[`4a57d476a8`](https://github.com/nodejs/node/commit/4a57d476a8)] - **tools**: replace while+exec() with matchAll() (Rich Trott) [#41406](https://github.com/nodejs/node/pull/41406)
* \[[`583f8d969a`](https://github.com/nodejs/node/commit/583f8d969a)] - **tools**: fix argv bug in find-inactive-tsc.mjs (Rich Trott) [#41394](https://github.com/nodejs/node/pull/41394)
* \[[`dcada80f30`](https://github.com/nodejs/node/commit/dcada80f30)] - **tools**: remove conditional assignment in custom ESLint rule (Rich Trott) [#41325](https://github.com/nodejs/node/pull/41325)
* \[[`e15e1cb030`](https://github.com/nodejs/node/commit/e15e1cb030)] - **tools**: update lint-md-dependencies to @rollup/plugin-node-resolve\@13.1.2 (Node.js GitHub Bot) [#41369](https://github.com/nodejs/node/pull/41369)
* \[[`07683021b7`](https://github.com/nodejs/node/commit/07683021b7)] - **tools**: update doc to rehype-raw\@6.1.1 (Node.js GitHub Bot) [#41367](https://github.com/nodejs/node/pull/41367)
* \[[`bd8b95a5e8`](https://github.com/nodejs/node/commit/bd8b95a5e8)] - **tools**: remove last of error-masking in commit-queue.sh (Rich Trott) [#41356](https://github.com/nodejs/node/pull/41356)
* \[[`9284d24df6`](https://github.com/nodejs/node/commit/9284d24df6)] - **tools**: update eslint to 8.6.0 (Node.js GitHub Bot) [#41368](https://github.com/nodejs/node/pull/41368)
* \[[`5fc886f68e`](https://github.com/nodejs/node/commit/5fc886f68e)] - **tools**: do not mask errors on multiple commit retrieval (Rich Trott) [#41340](https://github.com/nodejs/node/pull/41340)
* \[[`0ca7cda962`](https://github.com/nodejs/node/commit/0ca7cda962)] - **tools**: enable jsdoc/check-param-names lint rule (Rich Trott) [#41311](https://github.com/nodejs/node/pull/41311)
* \[[`75ff8e6505`](https://github.com/nodejs/node/commit/75ff8e6505)] - **tools**: improve section tag additions in HTML doc generator (Rich Trott) [#41318](https://github.com/nodejs/node/pull/41318)
* \[[`9c4124706e`](https://github.com/nodejs/node/commit/9c4124706e)] - **tools**: simplify commit-queue.sh merge command (Rich Trott) [#41314](https://github.com/nodejs/node/pull/41314)
* \[[`137c814848`](https://github.com/nodejs/node/commit/137c814848)] - **tools**: update lint-md-dependencies to rollup\@2.62.0 (Node.js GitHub Bot) [#41315](https://github.com/nodejs/node/pull/41315)
* \[[`58da5d9b43`](https://github.com/nodejs/node/commit/58da5d9b43)] - **tools**: use Object.hasOwn() in alljson.mjs (Rich Trott) [#41306](https://github.com/nodejs/node/pull/41306)
* \[[`c12cbf2020`](https://github.com/nodejs/node/commit/c12cbf2020)] - **tools**: avoid generating duplicate id attributes (Rich Trott) [#41291](https://github.com/nodejs/node/pull/41291)
* \[[`80a114d1b7`](https://github.com/nodejs/node/commit/80a114d1b7)] - **tools**: be intentional about masking possible error in start-ci.sh (Rich Trott) [#41284](https://github.com/nodejs/node/pull/41284)
* \[[`198528426d`](https://github.com/nodejs/node/commit/198528426d)] - **tools**: use {N} for spaces in regex (Rich Trott) [#41295](https://github.com/nodejs/node/pull/41295)
* \[[`46b364a684`](https://github.com/nodejs/node/commit/46b364a684)] - **tools**: consolidate update-authors.js logic (Rich Trott) [#41255](https://github.com/nodejs/node/pull/41255)
* \[[`c546cef4bc`](https://github.com/nodejs/node/commit/c546cef4bc)] - **tools**: update doc dependency mdast-util-gfm-table to 1.0.2 (Rich Trott) [#41260](https://github.com/nodejs/node/pull/41260)
* \[[`60c059e4bc`](https://github.com/nodejs/node/commit/60c059e4bc)] - **tools**: make license-builder.sh comply with shellcheck 0.8.0 (Rich Trott) [#41258](https://github.com/nodejs/node/pull/41258)
* \[[`62e28f19f7`](https://github.com/nodejs/node/commit/62e28f19f7)] - **tools**: use arrow function for callback in lint-sh.js (Rich Trott) [#41256](https://github.com/nodejs/node/pull/41256)
* \[[`e2df381da9`](https://github.com/nodejs/node/commit/e2df381da9)] - **tools**: add double-quotes to make-v8.sh (Rich Trott) [#41257](https://github.com/nodejs/node/pull/41257)
* \[[`dae2e5fffa`](https://github.com/nodejs/node/commit/dae2e5fffa)] - **tools**: enable prefer-object-has-own lint rule (Rich Trott) [#41245](https://github.com/nodejs/node/pull/41245)
* \[[`aa7d14768d`](https://github.com/nodejs/node/commit/aa7d14768d)] - **tools**: update eslint to 8.5.0 (Node.js GitHub Bot) [#41228](https://github.com/nodejs/node/pull/41228)
* \[[`0c14e7e7c8`](https://github.com/nodejs/node/commit/0c14e7e7c8)] - **tools**: enable jsdoc/tag-lines ESLint rule (Rich Trott) [#41147](https://github.com/nodejs/node/pull/41147)
* \[[`c486da1715`](https://github.com/nodejs/node/commit/c486da1715)] - **tools**: update lint-md-dependencies to @rollup/plugin-node-resolve\@13.1.1 (Node.js GitHub Bot) [#41227](https://github.com/nodejs/node/pull/41227)
* \[[`82f492bbb0`](https://github.com/nodejs/node/commit/82f492bbb0)] - **tools**: fix CQ and auto-start-ci jobs (Antoine du Hamel) [#41230](https://github.com/nodejs/node/pull/41230)
* \[[`c44185ca37`](https://github.com/nodejs/node/commit/c44185ca37)] - **tools**: fix GitHub Actions status when CQ is empty (Antoine du Hamel) [#41193](https://github.com/nodejs/node/pull/41193)
* \[[`800640adf9`](https://github.com/nodejs/node/commit/800640adf9)] - **tools,benchmark,lib,test**: enable no-case-declarations lint rule (Rich Trott) [#41385](https://github.com/nodejs/node/pull/41385)
* \[[`4518fdda24`](https://github.com/nodejs/node/commit/4518fdda24)] - **tools,lib,test**: enable ESLint no-regex-spaces rule (Rich Trott) [#41463](https://github.com/nodejs/node/pull/41463)
* \[[`c8e8fc0ecb`](https://github.com/nodejs/node/commit/c8e8fc0ecb)] - **typings**: add types for symbol and accessor properties on `primordials` (ExE Boss) [#40992](https://github.com/nodejs/node/pull/40992)
* \[[`d733b56101`](https://github.com/nodejs/node/commit/d733b56101)] - **typings**: add JSDoc for `string_decoder` (Qingyu Deng) [#38229](https://github.com/nodejs/node/pull/38229)
* \[[`01ad8debd3`](https://github.com/nodejs/node/commit/01ad8debd3)] - **url,lib**: pass urlsearchparams-constructor.any.js (Khaidi Chu) [#41197](https://github.com/nodejs/node/pull/41197)
* \[[`5ed8a1c017`](https://github.com/nodejs/node/commit/5ed8a1c017)] - **util**: do not reduce to a single line if not appropriate using inspect (Ruben Bridgewater) [#41083](https://github.com/nodejs/node/pull/41083)
* \[[`ab5e94c832`](https://github.com/nodejs/node/commit/ab5e94c832)] - **util**: display a present-but-undefined error cause (Jordan Harband) [#41247](https://github.com/nodejs/node/pull/41247)

<a id="17.3.1"></a>

## 2022-01-10, Version 17.3.1 (Current), @BethGriggs

This is a security release.

### Notable changes

#### Improper handling of URI Subject Alternative Names (Medium)(CVE-2021-44531)

Accepting arbitrary Subject Alternative Name (SAN) types, unless a PKI is specifically defined to use a particular SAN type, can result in bypassing name-constrained intermediates. Node.js was accepting URI SAN types, which PKIs are often not defined to use. Additionally, when a protocol allows URI SANs, Node.js did not match the URI correctly.

Versions of Node.js with the fix for this disable the URI SAN type when checking a certificate against a hostname. This behavior can be reverted through the `--security-revert` command-line option.

More details will be available at [CVE-2021-44531](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2021-44531) after publication.

#### Certificate Verification Bypass via String Injection (Medium)(CVE-2021-44532)

Node.js converts SANs (Subject Alternative Names) to a string format. It uses this string to check peer certificates against hostnames when validating connections. The string format was subject to an injection vulnerability when name constraints were used within a certificate chain, allowing the bypass of these name constraints.

Versions of Node.js with the fix for this escape SANs containing the problematic characters in order to prevent the injection. This behavior can be reverted through the `--security-revert` command-line option.

More details will be available at [CVE-2021-44532](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2021-44532) after publication.

#### Incorrect handling of certificate subject and issuer fields (Medium)(CVE-2021-44533)

Node.js did not handle multi-value Relative Distinguished Names correctly. Attackers could craft certificate subjects containing a single-value Relative Distinguished Name that would be interpreted as a multi-value Relative Distinguished Name, for example, in order to inject a Common Name that would allow bypassing the certificate subject verification.

Affected versions of Node.js do not accept multi-value Relative Distinguished Names and are thus not vulnerable to such attacks themselves. However, third-party code that uses node's ambiguous presentation of certificate subjects may be vulnerable.

More details will be available at [CVE-2021-44533](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2021-44533) after publication.

#### Prototype pollution via `console.table` properties (Low)(CVE-2022-21824)

Due to the formatting logic of the `console.table()` function it was not safe to allow user controlled input to be passed to the `properties` parameter while simultaneously passing a plain object with at least one property as the first parameter, which could be `__proto__`. The prototype pollution has very limited control, in that it only allows an empty string to be assigned numerical keys of the object prototype.

Versions of Node.js with the fix for this use a null protoype for the object these properties are being assigned to.

More details will be available at [CVE-2022-21824](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-21824) after publication.

Thanks to Patrik Oldsberg (rugvip) for reporting this vulnerability.

### Commits

* \[[`2a0515f73c`](https://github.com/nodejs/node/commit/2a0515f73c)] - **console**: fix prototype pollution via console.table (Tobias Nießen) [nodejs-private/node-private#307](https://github.com/nodejs-private/node-private/pull/307)
* \[[`2e2c45553d`](https://github.com/nodejs/node/commit/2e2c45553d)] - **crypto,tls**: implement safe x509 GeneralName format (Tobias Nießen) [nodejs-private/node-private#300](https://github.com/nodejs-private/node-private/pull/300)
* \[[`df3141f59b`](https://github.com/nodejs/node/commit/df3141f59b)] - **src**: add cve reverts and associated tests (Michael Dawson) [nodejs-private/node-private#300](https://github.com/nodejs-private/node-private/pull/300)
* \[[`5398548746`](https://github.com/nodejs/node/commit/5398548746)] - **src**: remove unused x509 functions (Tobias Nießen) [nodejs-private/node-private#300](https://github.com/nodejs-private/node-private/pull/300)
* \[[`1f7fdff64a`](https://github.com/nodejs/node/commit/1f7fdff64a)] - **tls**: fix handling of x509 subject and issuer (Tobias Nießen) [nodejs-private/node-private#300](https://github.com/nodejs-private/node-private/pull/300)
* \[[`b11b4cc69d`](https://github.com/nodejs/node/commit/b11b4cc69d)] - **tls**: drop support for URI alternative names (Tobias Nießen) [nodejs-private/node-private#300](https://github.com/nodejs-private/node-private/pull/300)

<a id="17.3.0"></a>

## 2021-12-17, Version 17.3.0 (Current), @danielleadams

### Notable changes

#### OpenSSL-3.0.1

OpenSSL-3.0.1 contains a fix for CVE-2021-4044: Invalid handling of X509\_verify\_cert() internal errors in libssl (Moderate). This is a vulnerability in OpenSSL that may be exploited through Node.js. More information can be read here: <https://www.openssl.org/news/secadv/20211214.txt>.

Contributed by Richard Lau [#41177](https://github.com/nodejs/node/pull/41177).

#### Other Notable Changes

* **lib**:
  * make AbortSignal cloneable/transferable (James M Snell) [#41050](https://github.com/nodejs/node/pull/41050)
* **deps**:
  * upgrade npm to 8.3.0 (npm team) [#41127](https://github.com/nodejs/node/pull/41127)
* **doc**:
  * add @bnb as a collaborator (Tierney Cyren) [#41100](https://github.com/nodejs/node/pull/41100)
* **process**:
  * add `getActiveResourcesInfo()` (Darshan Sen) [#40813](https://github.com/nodejs/node/pull/40813)
* **timers**:
  * add experimental scheduler api (James M Snell) [#40909](https://github.com/nodejs/node/pull/40909)

### Commits

* \[[`99fb6d48eb`](https://github.com/nodejs/node/commit/99fb6d48eb)] - **assert**: prefer reference comparison over string comparison (Darshan Sen) [#41015](https://github.com/nodejs/node/pull/41015)
* \[[`a7dfa43dc7`](https://github.com/nodejs/node/commit/a7dfa43dc7)] - **assert**: use stricter stack frame detection in .ifError() (Ruben Bridgewater) [#41006](https://github.com/nodejs/node/pull/41006)
* \[[`28761de6d4`](https://github.com/nodejs/node/commit/28761de6d4)] - **buffer**: fix `Blob` constructor on various `TypedArray`s (Irakli Gozalishvili) [#40706](https://github.com/nodejs/node/pull/40706)
* \[[`8fcb71a5ab`](https://github.com/nodejs/node/commit/8fcb71a5ab)] - **build**: update openssl config generator Dockerfile (Richard Lau) [#41177](https://github.com/nodejs/node/pull/41177)
* \[[`3a9ffa86db`](https://github.com/nodejs/node/commit/3a9ffa86db)] - **build**: use '<(python)' instead of 'python' (Cheng Zhao) [#41146](https://github.com/nodejs/node/pull/41146)
* \[[`85f1537c28`](https://github.com/nodejs/node/commit/85f1537c28)] - **build**: fix comment-labeled workflow (Mestery) [#41176](https://github.com/nodejs/node/pull/41176)
* \[[`61c53a667a`](https://github.com/nodejs/node/commit/61c53a667a)] - **build**: use gh cli in workflows file (Mestery) [#40985](https://github.com/nodejs/node/pull/40985)
* \[[`1fc6fd66ff`](https://github.com/nodejs/node/commit/1fc6fd66ff)] - **build**: fix commit-queue-rebase functionality (Rich Trott) [#41140](https://github.com/nodejs/node/pull/41140)
* \[[`831face7d1`](https://github.com/nodejs/node/commit/831face7d1)] - **build**: skip documentation generation if no ICU (Rich Trott) [#41091](https://github.com/nodejs/node/pull/41091)
* \[[`c776c9236e`](https://github.com/nodejs/node/commit/c776c9236e)] - **build**: re-enable V8 concurrent marking (Michaël Zasso) [#41013](https://github.com/nodejs/node/pull/41013)
* \[[`2125449f89`](https://github.com/nodejs/node/commit/2125449f89)] - **build**: add `--without-corepack` (Jonah Snider) [#41060](https://github.com/nodejs/node/pull/41060)
* \[[`6327685363`](https://github.com/nodejs/node/commit/6327685363)] - **build**: fail early in test-macos.yml (Rich Trott) [#41035](https://github.com/nodejs/node/pull/41035)
* \[[`ee4186b305`](https://github.com/nodejs/node/commit/ee4186b305)] - **build**: add tools/doc to tools.yml updates (Rich Trott) [#41036](https://github.com/nodejs/node/pull/41036)
* \[[`db30bc97d0`](https://github.com/nodejs/node/commit/db30bc97d0)] - **build**: update Actions versions (Mestery) [#40987](https://github.com/nodejs/node/pull/40987)
* \[[`db9cef3c4f`](https://github.com/nodejs/node/commit/db9cef3c4f)] - **build**: set persist-credentials: false on workflows (Rich Trott) [#40972](https://github.com/nodejs/node/pull/40972)
* \[[`29739f813f`](https://github.com/nodejs/node/commit/29739f813f)] - **build**: add OpenSSL gyp artifacts to .gitignore (Luigi Pinca) [#40967](https://github.com/nodejs/node/pull/40967)
* \[[`1b8baf0e4f`](https://github.com/nodejs/node/commit/1b8baf0e4f)] - **build**: remove legacy -J test.py option from Makefile/vcbuild (Rich Trott) [#40945](https://github.com/nodejs/node/pull/40945)
* \[[`5c27ec8385`](https://github.com/nodejs/node/commit/5c27ec8385)] - **build**: ignore unrelated workflow changes in slow Actions tests (Rich Trott) [#40928](https://github.com/nodejs/node/pull/40928)
* \[[`8957c9bd1c`](https://github.com/nodejs/node/commit/8957c9bd1c)] - **build,tools**: automate enforcement of emeritus criteria (Rich Trott) [#41155](https://github.com/nodejs/node/pull/41155)
* \[[`e924dc7982`](https://github.com/nodejs/node/commit/e924dc7982)] - **cluster**: use linkedlist for round\_robin\_handle (twchn) [#40615](https://github.com/nodejs/node/pull/40615)
* \[[`c757fa513e`](https://github.com/nodejs/node/commit/c757fa513e)] - **crypto**: add missing null check (Michael Dawson) [#40598](https://github.com/nodejs/node/pull/40598)
* \[[`35fe14454b`](https://github.com/nodejs/node/commit/35fe14454b)] - **deps**: update archs files for quictls/openssl-3.0.1+quic (Richard Lau) [#41177](https://github.com/nodejs/node/pull/41177)
* \[[`0b2103419f`](https://github.com/nodejs/node/commit/0b2103419f)] - **deps**: upgrade openssl sources to quictls/openssl-3.0.1+quic (Richard Lau) [#41177](https://github.com/nodejs/node/pull/41177)
* \[[`fae4945ab3`](https://github.com/nodejs/node/commit/fae4945ab3)] - **deps**: upgrade npm to 8.3.0 (npm team) [#41127](https://github.com/nodejs/node/pull/41127)
* \[[`3a1d952e68`](https://github.com/nodejs/node/commit/3a1d952e68)] - **deps**: upgrade npm to 8.2.0 (npm team) [#41065](https://github.com/nodejs/node/pull/41065)
* \[[`627b5bb718`](https://github.com/nodejs/node/commit/627b5bb718)] - **deps**: update Acorn to v8.6.0 (Michaël Zasso) [#40993](https://github.com/nodejs/node/pull/40993)
* \[[`a2fb12f9c6`](https://github.com/nodejs/node/commit/a2fb12f9c6)] - **deps**: patch V8 to 9.6.180.15 (Michaël Zasso) [#40949](https://github.com/nodejs/node/pull/40949)
* \[[`93111e4662`](https://github.com/nodejs/node/commit/93111e4662)] - **doc**: fix closing parenthesis (AlphaDio) [#41190](https://github.com/nodejs/node/pull/41190)
* \[[`f883bf3d12`](https://github.com/nodejs/node/commit/f883bf3d12)] - **doc**: add security steward on/offboarding steps (Michael Dawson) [#41129](https://github.com/nodejs/node/pull/41129)
* \[[`1274a25b14`](https://github.com/nodejs/node/commit/1274a25b14)] - **doc**: align module resolve algorithm with implementation (Qingyu Deng) [#38837](https://github.com/nodejs/node/pull/38837)
* \[[`34c6c59014`](https://github.com/nodejs/node/commit/34c6c59014)] - **doc**: update nodejs-sec managers (Michael Dawson) [#41128](https://github.com/nodejs/node/pull/41128)
* \[[`db26bdb011`](https://github.com/nodejs/node/commit/db26bdb011)] - **doc**: move style guide to findable location (Rich Trott) [#41119](https://github.com/nodejs/node/pull/41119)
* \[[`4369c6d9f6`](https://github.com/nodejs/node/commit/4369c6d9f6)] - **doc**: fix comments in test-fs-watch.js (jakub-g) [#41046](https://github.com/nodejs/node/pull/41046)
* \[[`93f5bd34e9`](https://github.com/nodejs/node/commit/93f5bd34e9)] - **doc**: document support building with Python 3.10 on Windows (Christian Clauss) [#41098](https://github.com/nodejs/node/pull/41098)
* \[[`d8fa227c26`](https://github.com/nodejs/node/commit/d8fa227c26)] - **doc**: add note about pip being required (Piotr Rybak) [#40669](https://github.com/nodejs/node/pull/40669)
* \[[`95691801f3`](https://github.com/nodejs/node/commit/95691801f3)] - **doc**: remove OpenJSF Slack nodejs from support doc (Rich Trott) [#41108](https://github.com/nodejs/node/pull/41108)
* \[[`e3ac384d78`](https://github.com/nodejs/node/commit/e3ac384d78)] - **doc**: simplify major release preparation (Bethany Nicolle Griggs) [#40816](https://github.com/nodejs/node/pull/40816)
* \[[`3406910040`](https://github.com/nodejs/node/commit/3406910040)] - **doc**: clarify escaping for ES modules (notroid5) [#41074](https://github.com/nodejs/node/pull/41074)
* \[[`668284b5a1`](https://github.com/nodejs/node/commit/668284b5a1)] - **doc**: add @bnb as a collaborator (Tierney Cyren) [#41100](https://github.com/nodejs/node/pull/41100)
* \[[`94d09113a2`](https://github.com/nodejs/node/commit/94d09113a2)] - **doc**: add explicit declaration of fd with null val (Henadzi) [#40704](https://github.com/nodejs/node/pull/40704)
* \[[`b353ded677`](https://github.com/nodejs/node/commit/b353ded677)] - **doc**: expand entries for isIP(), isIPv4(), and isIPv6() (Rich Trott) [#41028](https://github.com/nodejs/node/pull/41028)
* \[[`f18aa14b1d`](https://github.com/nodejs/node/commit/f18aa14b1d)] - **doc**: link to commit queue guide (Geoffrey Booth) [#41030](https://github.com/nodejs/node/pull/41030)
* \[[`681edbe75f`](https://github.com/nodejs/node/commit/681edbe75f)] - **doc**: specify that `message.socket` can be nulled (Luigi Pinca) [#41014](https://github.com/nodejs/node/pull/41014)
* \[[`7c41f32f06`](https://github.com/nodejs/node/commit/7c41f32f06)] - **doc**: fix JSDoc in ESM loaders examples (Mestery) [#40984](https://github.com/nodejs/node/pull/40984)
* \[[`61b2e2ef9e`](https://github.com/nodejs/node/commit/61b2e2ef9e)] - **doc**: remove legacy -J test.py option from BUILDING.md (Rich Trott) [#40945](https://github.com/nodejs/node/pull/40945)
* \[[`c9b09d124e`](https://github.com/nodejs/node/commit/c9b09d124e)] - **doc,lib,tools**: align multiline comments (Rich Trott) [#41109](https://github.com/nodejs/node/pull/41109)
* \[[`12023dff4b`](https://github.com/nodejs/node/commit/12023dff4b)] - **(SEMVER-MINOR)** **errors**: add support for cause in aborterror (James M Snell) [#41008](https://github.com/nodejs/node/pull/41008)
* \[[`b0b7943e8f`](https://github.com/nodejs/node/commit/b0b7943e8f)] - **(SEMVER-MINOR)** **esm**: working mock test (Bradley Farias) [#39240](https://github.com/nodejs/node/pull/39240)
* \[[`37dbc3b9e9`](https://github.com/nodejs/node/commit/37dbc3b9e9)] - **(SEMVER-MINOR)** **events**: propagate abortsignal reason in new AbortError ctor in events (James M Snell) [#41008](https://github.com/nodejs/node/pull/41008)
* \[[`1b8d4e4867`](https://github.com/nodejs/node/commit/1b8d4e4867)] - **(SEMVER-MINOR)** **events**: propagate weak option for kNewListener (James M Snell) [#40899](https://github.com/nodejs/node/pull/40899)
* \[[`bbdcd0513b`](https://github.com/nodejs/node/commit/bbdcd0513b)] - **(SEMVER-MINOR)** **fs**: accept URL as argument for `fs.rm` and `fs.rmSync` (Antoine du Hamel) [#41132](https://github.com/nodejs/node/pull/41132)
* \[[`46108f8d50`](https://github.com/nodejs/node/commit/46108f8d50)] - **fs**: fix error codes for `fs.cp` (Antoine du Hamel) [#41106](https://github.com/nodejs/node/pull/41106)
* \[[`e25671cddb`](https://github.com/nodejs/node/commit/e25671cddb)] - **fs**: fix `length` option being ignored during `read()` (Shinho Ahn) [#40906](https://github.com/nodejs/node/pull/40906)
* \[[`6eda874be0`](https://github.com/nodejs/node/commit/6eda874be0)] - **(SEMVER-MINOR)** **fs**: propagate abortsignal reason in new AbortSignal constructors (James M Snell) [#41008](https://github.com/nodejs/node/pull/41008)
* \[[`70ed4ef248`](https://github.com/nodejs/node/commit/70ed4ef248)] - **http**: don't write empty data on req/res end() (Santiago Gimeno) [#41116](https://github.com/nodejs/node/pull/41116)
* \[[`4b3bf7e818`](https://github.com/nodejs/node/commit/4b3bf7e818)] - **(SEMVER-MINOR)** **http2**: propagate abortsignal reason in new AbortError constructor (James M Snell) [#41008](https://github.com/nodejs/node/pull/41008)
* \[[`8d87303f76`](https://github.com/nodejs/node/commit/8d87303f76)] - **inspector**: add missing initialization (Michael Dawson) [#41022](https://github.com/nodejs/node/pull/41022)
* \[[`b191e66ddf`](https://github.com/nodejs/node/commit/b191e66ddf)] - **lib**: include return types in JSDoc (Rich Trott) [#41130](https://github.com/nodejs/node/pull/41130)
* \[[`348707fca6`](https://github.com/nodejs/node/commit/348707fca6)] - **(SEMVER-MINOR)** **lib**: make AbortSignal cloneable/transferable (James M Snell) [#41050](https://github.com/nodejs/node/pull/41050)
* \[[`4ba883d384`](https://github.com/nodejs/node/commit/4ba883d384)] - **(SEMVER-MINOR)** **lib**: add abortSignal.throwIfAborted() (James M Snell) [#40951](https://github.com/nodejs/node/pull/40951)
* \[[`cc3e430c11`](https://github.com/nodejs/node/commit/cc3e430c11)] - **lib**: use consistent types in JSDoc @returns (Rich Trott) [#41089](https://github.com/nodejs/node/pull/41089)
* \[[`a1ed7f2810`](https://github.com/nodejs/node/commit/a1ed7f2810)] - **(SEMVER-MINOR)** **lib**: propagate abortsignal reason in new AbortError constructor in blob (James M Snell) [#41008](https://github.com/nodejs/node/pull/41008)
* \[[`1572db3e86`](https://github.com/nodejs/node/commit/1572db3e86)] - **lib**: do not lazy load EOL in blob (Ruben Bridgewater) [#41004](https://github.com/nodejs/node/pull/41004)
* \[[`62c4b4c85b`](https://github.com/nodejs/node/commit/62c4b4c85b)] - **(SEMVER-MINOR)** **lib**: add AbortSignal.timeout (James M Snell) [#40899](https://github.com/nodejs/node/pull/40899)
* \[[`f0d874342d`](https://github.com/nodejs/node/commit/f0d874342d)] - **lib,test,tools**: use consistent JSDoc types (Rich Trott) [#40989](https://github.com/nodejs/node/pull/40989)
* \[[`03e6771137`](https://github.com/nodejs/node/commit/03e6771137)] - **meta**: move one or more collaborators to emeritus (Node.js GitHub Bot) [#41154](https://github.com/nodejs/node/pull/41154)
* \[[`e26c187b85`](https://github.com/nodejs/node/commit/e26c187b85)] - **meta**: move to emeritus automatically after 18 months (Rich Trott) [#41155](https://github.com/nodejs/node/pull/41155)
* \[[`b89fb3ef0a`](https://github.com/nodejs/node/commit/b89fb3ef0a)] - **meta**: move silverwind to emeriti (Roman Reiss) [#41171](https://github.com/nodejs/node/pull/41171)
* \[[`0fc148321f`](https://github.com/nodejs/node/commit/0fc148321f)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#41144](https://github.com/nodejs/node/pull/41144)
* \[[`d6d1d6647c`](https://github.com/nodejs/node/commit/d6d1d6647c)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#41088](https://github.com/nodejs/node/pull/41088)
* \[[`f30d6bcaff`](https://github.com/nodejs/node/commit/f30d6bcaff)] - **meta**: move one or more TSC members to emeritus (Node.js GitHub Bot) [#40908](https://github.com/nodejs/node/pull/40908)
* \[[`033a646d82`](https://github.com/nodejs/node/commit/033a646d82)] - **meta**: increase security policy response targets (Matteo Collina) [#40968](https://github.com/nodejs/node/pull/40968)
* \[[`6b6e1d054e`](https://github.com/nodejs/node/commit/6b6e1d054e)] - **node-api,doc**: document parms which can be optional (Michael Dawson) [#41021](https://github.com/nodejs/node/pull/41021)
* \[[`93ea1666f6`](https://github.com/nodejs/node/commit/93ea1666f6)] - **perf\_hooks**: use spec-compliant `structuredClone` (Michaël Zasso) [#40904](https://github.com/nodejs/node/pull/40904)
* \[[`d8a2125900`](https://github.com/nodejs/node/commit/d8a2125900)] - **(SEMVER-MINOR)** **process**: add `getActiveResourcesInfo()` (Darshan Sen) [#40813](https://github.com/nodejs/node/pull/40813)
* \[[`67124ac63a`](https://github.com/nodejs/node/commit/67124ac63a)] - **(SEMVER-MINOR)** **readline**: propagate signal.reason in awaitable question (James M Snell) [#41008](https://github.com/nodejs/node/pull/41008)
* \[[`8fac878ff5`](https://github.com/nodejs/node/commit/8fac878ff5)] - **readline**: skip escaping characters again (Ruben Bridgewater) [#41005](https://github.com/nodejs/node/pull/41005)
* \[[`d3de937782`](https://github.com/nodejs/node/commit/d3de937782)] - **src**: fix limit calculation (Michael Dawson) [#41026](https://github.com/nodejs/node/pull/41026)
* \[[`6f0ec9835a`](https://github.com/nodejs/node/commit/6f0ec9835a)] - **src**: use a higher limit in the NearHeapLimitCallback (Joyee Cheung) [#41041](https://github.com/nodejs/node/pull/41041)
* \[[`90097ab891`](https://github.com/nodejs/node/commit/90097ab891)] - **src,crypto**: remove uses of `AllocatedBuffer` from `crypto_sig` (Darshan Sen) [#40895](https://github.com/nodejs/node/pull/40895)
* \[[`b59c513c31`](https://github.com/nodejs/node/commit/b59c513c31)] - **stream**: add isErrored helper (Robert Nagy) [#41121](https://github.com/nodejs/node/pull/41121)
* \[[`1787bfab68`](https://github.com/nodejs/node/commit/1787bfab68)] - **stream**: allow readable to end early without error (Robert Nagy) [#40881](https://github.com/nodejs/node/pull/40881)
* \[[`01e8c15c8a`](https://github.com/nodejs/node/commit/01e8c15c8a)] - **(SEMVER-MINOR)** **stream**: use cause options in AbortError constructors (James M Snell) [#41008](https://github.com/nodejs/node/pull/41008)
* \[[`0e21c64ae9`](https://github.com/nodejs/node/commit/0e21c64ae9)] - **stream**: remove whatwg streams experimental warning (James M Snell) [#40971](https://github.com/nodejs/node/pull/40971)
* \[[`513305c7d7`](https://github.com/nodejs/node/commit/513305c7d7)] - **stream**: cleanup eos (Robert Nagy) [#40998](https://github.com/nodejs/node/pull/40998)
* \[[`da8baf4bbb`](https://github.com/nodejs/node/commit/da8baf4bbb)] - **test**: do not load absolute path crypto engines twice (Richard Lau) [#41177](https://github.com/nodejs/node/pull/41177)
* \[[`1f6a9c3e31`](https://github.com/nodejs/node/commit/1f6a9c3e31)] - **test**: skip ESLint tests if no Intl (Rich Trott) [#41105](https://github.com/nodejs/node/pull/41105)
* \[[`ce656a80b5`](https://github.com/nodejs/node/commit/ce656a80b5)] - **test**: add missing JSDoc parameter name (Rich Trott) [#41057](https://github.com/nodejs/node/pull/41057)
* \[[`fb8f2e9643`](https://github.com/nodejs/node/commit/fb8f2e9643)] - **test**: deflake test-trace-atomics-wait (Luigi Pinca) [#41018](https://github.com/nodejs/node/pull/41018)
* \[[`de1748aca4`](https://github.com/nodejs/node/commit/de1748aca4)] - **test**: add auth option case for url.format (Hirotaka Tagawa / wafuwafu13) [#40516](https://github.com/nodejs/node/pull/40516)
* \[[`943547a0eb`](https://github.com/nodejs/node/commit/943547a0eb)] - _**Revert**_ "**test**: skip different params test for OpenSSL 3.x" (Daniel Bevenius) [#40640](https://github.com/nodejs/node/pull/40640)
* \[[`0caa3483d2`](https://github.com/nodejs/node/commit/0caa3483d2)] - **(SEMVER-MINOR)** **timers**: add experimental scheduler api (James M Snell) [#40909](https://github.com/nodejs/node/pull/40909)
* \[[`e795547651`](https://github.com/nodejs/node/commit/e795547651)] - **(SEMVER-MINOR)** **timers**: propagate signal.reason in awaitable timers (James M Snell) [#41008](https://github.com/nodejs/node/pull/41008)
* \[[`a77cae1ef7`](https://github.com/nodejs/node/commit/a77cae1ef7)] - **tls**: improve handling of shutdown (Jameson Nash) [#36111](https://github.com/nodejs/node/pull/36111)
* \[[`db410e7d3e`](https://github.com/nodejs/node/commit/db410e7d3e)] - **tools**: update doc to remark-rehype\@10.1.0 (Node.js GitHub Bot) [#41149](https://github.com/nodejs/node/pull/41149)
* \[[`e3870f3f17`](https://github.com/nodejs/node/commit/e3870f3f17)] - **tools**: update lint-md-dependencies to rollup\@2.61.1 vfile-reporter\@7.0.3 (Node.js GitHub Bot) [#41150](https://github.com/nodejs/node/pull/41150)
* \[[`6fc92bd191`](https://github.com/nodejs/node/commit/6fc92bd191)] - **tools**: enable jsdoc/require-returns-type ESLint rule (Rich Trott) [#41130](https://github.com/nodejs/node/pull/41130)
* \[[`70e6fe860a`](https://github.com/nodejs/node/commit/70e6fe860a)] - **tools**: update ESLint to 8.4.1 (Rich Trott) [#41114](https://github.com/nodejs/node/pull/41114)
* \[[`78894fa888`](https://github.com/nodejs/node/commit/78894fa888)] - **tools**: enable JSDoc check-alignment lint rule (Rich Trott) [#41109](https://github.com/nodejs/node/pull/41109)
* \[[`40a773aa29`](https://github.com/nodejs/node/commit/40a773aa29)] - **tools**: strip comments from lint-md rollup output (Rich Trott) [#41092](https://github.com/nodejs/node/pull/41092)
* \[[`7b606cfef6`](https://github.com/nodejs/node/commit/7b606cfef6)] - **tools**: update highlight.js to 11.3.1 (Rich Trott) [#41091](https://github.com/nodejs/node/pull/41091)
* \[[`52633a9e95`](https://github.com/nodejs/node/commit/52633a9e95)] - **tools**: enable jsdoc/require-returns-check lint rule (Rich Trott) [#41089](https://github.com/nodejs/node/pull/41089)
* \[[`dc0405e7fb`](https://github.com/nodejs/node/commit/dc0405e7fb)] - **tools**: update ESLint to 8.4.0 (Luigi Pinca) [#41085](https://github.com/nodejs/node/pull/41085)
* \[[`855f15d059`](https://github.com/nodejs/node/commit/855f15d059)] - **tools**: enable jsdoc/require-param-name lint rule (Rich Trott) [#41057](https://github.com/nodejs/node/pull/41057)
* \[[`78265e095a`](https://github.com/nodejs/node/commit/78265e095a)] - **tools**: use jsdoc recommended rules (Rich Trott) [#41057](https://github.com/nodejs/node/pull/41057)
* \[[`9cfdf15da6`](https://github.com/nodejs/node/commit/9cfdf15da6)] - **tools**: rollback highlight.js (Richard Lau) [#41078](https://github.com/nodejs/node/pull/41078)
* \[[`fe3e09bb4b`](https://github.com/nodejs/node/commit/fe3e09bb4b)] - **tools**: remove Babel from license-builder.sh (Rich Trott) [#41049](https://github.com/nodejs/node/pull/41049)
* \[[`62e0aa9725`](https://github.com/nodejs/node/commit/62e0aa9725)] - **tools**: udpate packages in tools/doc (Rich Trott) [#41036](https://github.com/nodejs/node/pull/41036)
* \[[`a959f4fa72`](https://github.com/nodejs/node/commit/a959f4fa72)] - **tools**: install and enable JSDoc linting in ESLint (Rich Trott) [#41027](https://github.com/nodejs/node/pull/41027)
* \[[`661960e471`](https://github.com/nodejs/node/commit/661960e471)] - **tools**: include JSDoc in ESLint updating tool (Rich Trott) [#41027](https://github.com/nodejs/node/pull/41027)
* \[[`e2922714ee`](https://github.com/nodejs/node/commit/e2922714ee)] - **tools**: ignore unrelated workflow changes in slow Actions tests (Antoine du Hamel) [#40990](https://github.com/nodejs/node/pull/40990)
* \[[`6525226ff7`](https://github.com/nodejs/node/commit/6525226ff7)] - **tools**: remove unneeded tool in update-eslint.sh (Rich Trott) [#40995](https://github.com/nodejs/node/pull/40995)
* \[[`5400b7963d`](https://github.com/nodejs/node/commit/5400b7963d)] - **tools**: consolidate ESLint dependencies (Rich Trott) [#40995](https://github.com/nodejs/node/pull/40995)
* \[[`86d5af14bc`](https://github.com/nodejs/node/commit/86d5af14bc)] - **tools**: update ESLint update script to consolidate dependencies (Rich Trott) [#40995](https://github.com/nodejs/node/pull/40995)
* \[[`8427099f66`](https://github.com/nodejs/node/commit/8427099f66)] - **tools**: run ESLint update to minimize diff on subsequent update (Rich Trott) [#40995](https://github.com/nodejs/node/pull/40995)
* \[[`82daaa9914`](https://github.com/nodejs/node/commit/82daaa9914)] - **tools,test**: make -J behavior default for test.py (Rich Trott) [#40945](https://github.com/nodejs/node/pull/40945)
* \[[`db77780cb9`](https://github.com/nodejs/node/commit/db77780cb9)] - **url**: detect hostname more reliably in url.parse() (Rich Trott) [#41031](https://github.com/nodejs/node/pull/41031)
* \[[`66b5083c1e`](https://github.com/nodejs/node/commit/66b5083c1e)] - **util**: serialize falsy cause values while inspecting errors (Ruben Bridgewater) [#41097](https://github.com/nodejs/node/pull/41097)
* \[[`09d29ca8d9`](https://github.com/nodejs/node/commit/09d29ca8d9)] - **util**: make sure error causes of any type may be inspected (Ruben Bridgewater) [#41097](https://github.com/nodejs/node/pull/41097)
* \[[`f5ff88b3cb`](https://github.com/nodejs/node/commit/f5ff88b3cb)] - **(SEMVER-MINOR)** **util**: pass through the inspect function to custom inspect functions (Ruben Bridgewater) [#41019](https://github.com/nodejs/node/pull/41019)
* \[[`a0326f0941`](https://github.com/nodejs/node/commit/a0326f0941)] - **util**: escape lone surrogate code points using .inspect() (Ruben Bridgewater) [#41001](https://github.com/nodejs/node/pull/41001)
* \[[`91df200ad6`](https://github.com/nodejs/node/commit/91df200ad6)] - **(SEMVER-MINOR)** **util**: add numericSeparator to util.inspect (Ruben Bridgewater) [#41003](https://github.com/nodejs/node/pull/41003)
* \[[`da87413257`](https://github.com/nodejs/node/commit/da87413257)] - **(SEMVER-MINOR)** **util**: always visualize cause property in errors during inspection (Ruben Bridgewater) [#41002](https://github.com/nodejs/node/pull/41002)

<a id="17.2.0"></a>

## 2021-11-30, Version 17.2.0 (Current), @targos

### Notable Changes

* \[[`06916490af`](https://github.com/nodejs/node/commit/06916490af)] - **(SEMVER-MINOR)** **async\_hooks**: expose async\_wrap providers (Rafael Gonzaga) [#40760](https://github.com/nodejs/node/pull/40760)
* \[[`371ee64c92`](https://github.com/nodejs/node/commit/371ee64c92)] - **(SEMVER-MINOR)** **deps**: update V8 to 9.6.180.14 (Michaël Zasso) [#40488](https://github.com/nodejs/node/pull/40488)
* \[[`675c210b04`](https://github.com/nodejs/node/commit/675c210b04)] - **(SEMVER-MINOR)** **lib**: add reason to AbortSignal (James M Snell) [#40807](https://github.com/nodejs/node/pull/40807)
* \[[`0de2850680`](https://github.com/nodejs/node/commit/0de2850680)] - **(SEMVER-MINOR)** **src**: add x509.fingerprint512 to crypto module (3nprob) [#39809](https://github.com/nodejs/node/pull/39809)
* \[[`fa9b5c35d2`](https://github.com/nodejs/node/commit/fa9b5c35d2)] - **stream**: deprecate thenable support (Antoine du Hamel) [#40860](https://github.com/nodejs/node/pull/40860)
* \[[`534409d4e7`](https://github.com/nodejs/node/commit/534409d4e7)] - **stream**: fix finished regression when working with legacy Stream (Matteo Collina) [#40858](https://github.com/nodejs/node/pull/40858)

### Commits

* \[[`48157c44c2`](https://github.com/nodejs/node/commit/48157c44c2)] - _**Revert**_ "**async\_hooks**: merge resource\_symbol with owner\_symbol" (Darshan Sen) [#40741](https://github.com/nodejs/node/pull/40741)
* \[[`4a971f67e4`](https://github.com/nodejs/node/commit/4a971f67e4)] - **async\_hooks**: eliminate require side effects (Stephen Belanger) [#40782](https://github.com/nodejs/node/pull/40782)
* \[[`06916490af`](https://github.com/nodejs/node/commit/06916490af)] - **(SEMVER-MINOR)** **async\_hooks**: expose async\_wrap providers (Rafael Gonzaga) [#40760](https://github.com/nodejs/node/pull/40760)
* \[[`65b33ba510`](https://github.com/nodejs/node/commit/65b33ba510)] - **build**: remove extraneous quotation marks from commit body (Rich Trott) [#40963](https://github.com/nodejs/node/pull/40963)
* \[[`05d652a555`](https://github.com/nodejs/node/commit/05d652a555)] - **build**: fix branch name for lint-md-dependencies update (Rich Trott) [#40924](https://github.com/nodejs/node/pull/40924)
* \[[`1482c4415f`](https://github.com/nodejs/node/commit/1482c4415f)] - **build**: fix `make` invocation in tools.yml (Rich Trott) [#40890](https://github.com/nodejs/node/pull/40890)
* \[[`69de8c8143`](https://github.com/nodejs/node/commit/69de8c8143)] - **build**: reset embedder string to "-node.0" (Michaël Zasso) [#40488](https://github.com/nodejs/node/pull/40488)
* \[[`e793331322`](https://github.com/nodejs/node/commit/e793331322)] - **build**: fix tools.yml errors (Rich Trott) [#40870](https://github.com/nodejs/node/pull/40870)
* \[[`51ac59b047`](https://github.com/nodejs/node/commit/51ac59b047)] - **build**: add GitHub Action to update tools modules (Rich Trott) [#40644](https://github.com/nodejs/node/pull/40644)
* \[[`a8cc8b6554`](https://github.com/nodejs/node/commit/a8cc8b6554)] - **crypto**: trim input for NETSCAPE\_SPKI\_b64\_decode (Shelley Vohr) [#40757](https://github.com/nodejs/node/pull/40757)
* \[[`2979c58fb0`](https://github.com/nodejs/node/commit/2979c58fb0)] - **crypto**: throw errors in SignTraits::DeriveBits (Tobias Nießen) [#40796](https://github.com/nodejs/node/pull/40796)
* \[[`7f5931d03f`](https://github.com/nodejs/node/commit/7f5931d03f)] - **crypto**: fix build without scrypt (Martin Jansa) [#40613](https://github.com/nodejs/node/pull/40613)
* \[[`90f35fc329`](https://github.com/nodejs/node/commit/90f35fc329)] - **deps**: upgrade npm to 8.1.4 (npm team) [#40865](https://github.com/nodejs/node/pull/40865)
* \[[`d461603d71`](https://github.com/nodejs/node/commit/d461603d71)] - **deps**: V8: cherry-pick cced52a97ee9 (Ray Wang) [#40656](https://github.com/nodejs/node/pull/40656)
* \[[`d6ae50ff96`](https://github.com/nodejs/node/commit/d6ae50ff96)] - **deps**: V8: cherry-pick 7ae0b77628f6 (Ray Wang) [#40882](https://github.com/nodejs/node/pull/40882)
* \[[`e60053deee`](https://github.com/nodejs/node/commit/e60053deee)] - **deps**: V8: cherry-pick 2a0bc36dec12 (Michaël Zasso) [#40178](https://github.com/nodejs/node/pull/40178)
* \[[`94acaae3d4`](https://github.com/nodejs/node/commit/94acaae3d4)] - **deps**: V8: patch jinja2 for Python 3.10 compat (Michaël Zasso) [#40296](https://github.com/nodejs/node/pull/40296)
* \[[`37c9828d39`](https://github.com/nodejs/node/commit/37c9828d39)] - **deps**: disable trap handler for Windows cross-compiler (Michaël Zasso) [#40488](https://github.com/nodejs/node/pull/40488)
* \[[`dfb97fb501`](https://github.com/nodejs/node/commit/dfb97fb501)] - **deps**: silence irrelevant V8 warning (Michaël Zasso) [#38990](https://github.com/nodejs/node/pull/38990)
* \[[`9ecf4be0c2`](https://github.com/nodejs/node/commit/9ecf4be0c2)] - **deps**: silence irrelevant V8 warning (Michaël Zasso) [#37587](https://github.com/nodejs/node/pull/37587)
* \[[`82a8736dec`](https://github.com/nodejs/node/commit/82a8736dec)] - **deps**: fix V8 build issue with inline methods (Jiawen Geng) [#38807](https://github.com/nodejs/node/pull/38807)
* \[[`eab5ef5505`](https://github.com/nodejs/node/commit/eab5ef5505)] - **deps**: make v8.h compatible with VS2015 (Joao Reis) [#32116](https://github.com/nodejs/node/pull/32116)
* \[[`d6e5dd56ff`](https://github.com/nodejs/node/commit/d6e5dd56ff)] - **deps**: V8: forward declaration of `Rtl*FunctionTable` (Refael Ackermann) [#32116](https://github.com/nodejs/node/pull/32116)
* \[[`29a6e15480`](https://github.com/nodejs/node/commit/29a6e15480)] - **deps**: V8: patch register-arm64.h (Refael Ackermann) [#32116](https://github.com/nodejs/node/pull/32116)
* \[[`1ff83c4945`](https://github.com/nodejs/node/commit/1ff83c4945)] - **deps**: V8: un-cherry-pick bd019bd (Refael Ackermann) [#32116](https://github.com/nodejs/node/pull/32116)
* \[[`371ee64c92`](https://github.com/nodejs/node/commit/371ee64c92)] - **(SEMVER-MINOR)** **deps**: update V8 to 9.6.180.14 (Michaël Zasso) [#40488](https://github.com/nodejs/node/pull/40488)
* \[[`6506a42c16`](https://github.com/nodejs/node/commit/6506a42c16)] - **deps**: V8: cherry-pick cced52a97ee9 (Ray Wang) [#40656](https://github.com/nodejs/node/pull/40656)
* \[[`a612ecb42f`](https://github.com/nodejs/node/commit/a612ecb42f)] - **deps**: V8: cherry-pick 7ae0b77628f6 (Ray Wang) [#40882](https://github.com/nodejs/node/pull/40882)
* \[[`b46184df5e`](https://github.com/nodejs/node/commit/b46184df5e)] - **deps**: update to uvwasi 0.0.12 (Colin Ihrig) [#40847](https://github.com/nodejs/node/pull/40847)
* \[[`fa37428472`](https://github.com/nodejs/node/commit/fa37428472)] - **deps**: add -fno-strict-aliasing flag to libuv (Daniel Bevenius) [#40631](https://github.com/nodejs/node/pull/40631)
* \[[`9711ccfe08`](https://github.com/nodejs/node/commit/9711ccfe08)] - **deps**: upgrade npm to 8.1.3 (npm team) [#40726](https://github.com/nodejs/node/pull/40726)
* \[[`8e525634c6`](https://github.com/nodejs/node/commit/8e525634c6)] - **deps**: update ICU to 70.1 (Michaël Zasso) [#40658](https://github.com/nodejs/node/pull/40658)
* \[[`3bfc9f5b47`](https://github.com/nodejs/node/commit/3bfc9f5b47)] - **doc**: add information on suppressing initial break in debugger (Rich Trott) [#40960](https://github.com/nodejs/node/pull/40960)
* \[[`8966ab3c57`](https://github.com/nodejs/node/commit/8966ab3c57)] - **doc**: improve grammar in API description (Tobias Nießen) [#40959](https://github.com/nodejs/node/pull/40959)
* \[[`130777a61c`](https://github.com/nodejs/node/commit/130777a61c)] - **doc**: update BUILDING.md section on running tests (Rich Trott) [#40933](https://github.com/nodejs/node/pull/40933)
* \[[`6e9b6652e5`](https://github.com/nodejs/node/commit/6e9b6652e5)] - **doc**: remove experimental abortcontroller flag (FrankQiu) [#38968](https://github.com/nodejs/node/pull/38968)
* \[[`b92416ff02`](https://github.com/nodejs/node/commit/b92416ff02)] - **doc**: fix spelling of 'WebAssembly' (Geoffrey Booth) [#40785](https://github.com/nodejs/node/pull/40785)
* \[[`cf495a6293`](https://github.com/nodejs/node/commit/cf495a6293)] - **doc**: clarify more optional parameters in node-api (Michael Dawson) [#40888](https://github.com/nodejs/node/pull/40888)
* \[[`694012b392`](https://github.com/nodejs/node/commit/694012b392)] - **doc**: define "types", "deno" community conditions (Guy Bedford) [#40708](https://github.com/nodejs/node/pull/40708)
* \[[`4c47b0150b`](https://github.com/nodejs/node/commit/4c47b0150b)] - **doc**: document optional params in napi\_get\_cb\_info (Michael Dawson) [#40821](https://github.com/nodejs/node/pull/40821)
* \[[`dfdf68f4d0`](https://github.com/nodejs/node/commit/dfdf68f4d0)] - **doc**: improve README.md lede section (Rich Trott) [#40837](https://github.com/nodejs/node/pull/40837)
* \[[`9c200e1de4`](https://github.com/nodejs/node/commit/9c200e1de4)] - **doc**: add pref to using draft PR versus WIP label (Michael Dawson) [#40824](https://github.com/nodejs/node/pull/40824)
* \[[`fe2cd09750`](https://github.com/nodejs/node/commit/fe2cd09750)] - **doc**: fix `added:` info for `Readable.fromWeb()` (Luigi Pinca) [#40820](https://github.com/nodejs/node/pull/40820)
* \[[`c91a9ab095`](https://github.com/nodejs/node/commit/c91a9ab095)] - **doc**: tweak guidance for modules in core (Michael Dawson) [#40601](https://github.com/nodejs/node/pull/40601)
* \[[`2ea08e9b55`](https://github.com/nodejs/node/commit/2ea08e9b55)] - **doc**: claim ABI version for Electron 18 (Keeley Hammond) [#40768](https://github.com/nodejs/node/pull/40768)
* \[[`8166b07ddc`](https://github.com/nodejs/node/commit/8166b07ddc)] - **doc**: fix transform stream example (Evan Lucas) [#40777](https://github.com/nodejs/node/pull/40777)
* \[[`5ceb06cddf`](https://github.com/nodejs/node/commit/5ceb06cddf)] - **doc**: fix linter-enforced formatting in crypto.md (Mohammed Keyvanzadeh) [#40780](https://github.com/nodejs/node/pull/40780)
* \[[`d3070d8eea`](https://github.com/nodejs/node/commit/d3070d8eea)] - **doc**: fix corepack grammar for `--force` flag (Steven) [#40762](https://github.com/nodejs/node/pull/40762)
* \[[`9271f23e3a`](https://github.com/nodejs/node/commit/9271f23e3a)] - **doc**: update maintaining ICU guide (Michaël Zasso) [#40658](https://github.com/nodejs/node/pull/40658)
* \[[`20d7d657bb`](https://github.com/nodejs/node/commit/20d7d657bb)] - **doc**: clarify getAuthTag with authTagLength (Tobias Nießen) [#40713](https://github.com/nodejs/node/pull/40713)
* \[[`75288fbc6b`](https://github.com/nodejs/node/commit/75288fbc6b)] - **doc**: fix order of announce work (Michael Dawson) [#40725](https://github.com/nodejs/node/pull/40725)
* \[[`429915aa6c`](https://github.com/nodejs/node/commit/429915aa6c)] - **doc**: add initial list of technical priorities (Michael Dawson) [#40235](https://github.com/nodejs/node/pull/40235)
* \[[`a5a1691514`](https://github.com/nodejs/node/commit/a5a1691514)] - **fs**: nullish coalescing to respect zero positional reads (Omar El-Mihilmy) [#40716](https://github.com/nodejs/node/pull/40716)
* \[[`bddb4c69b7`](https://github.com/nodejs/node/commit/bddb4c69b7)] - **http**: add missing initialization (Michael Dawson) [#40555](https://github.com/nodejs/node/pull/40555)
* \[[`80ce97f514`](https://github.com/nodejs/node/commit/80ce97f514)] - **http**: change totalSocketCount only on socket creation/close (Subhi Al Hasan) [#40572](https://github.com/nodejs/node/pull/40572)
* \[[`675c210b04`](https://github.com/nodejs/node/commit/675c210b04)] - **(SEMVER-MINOR)** **lib**: add reason to AbortSignal (James M Snell) [#40807](https://github.com/nodejs/node/pull/40807)
* \[[`b614b17525`](https://github.com/nodejs/node/commit/b614b17525)] - _**Revert**_ "**lib**: use helper for readability" (Darshan Sen) [#40741](https://github.com/nodejs/node/pull/40741)
* \[[`10a842d2d1`](https://github.com/nodejs/node/commit/10a842d2d1)] - **lib**: fix typos in lib code comments (Yoshiki) [#40792](https://github.com/nodejs/node/pull/40792)
* \[[`3ec78d1570`](https://github.com/nodejs/node/commit/3ec78d1570)] - **meta**: add feature request label for issue template (Mestery) [#40970](https://github.com/nodejs/node/pull/40970)
* \[[`9c897b69a1`](https://github.com/nodejs/node/commit/9c897b69a1)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#40900](https://github.com/nodejs/node/pull/40900)
* \[[`dfc6fc987a`](https://github.com/nodejs/node/commit/dfc6fc987a)] - **meta**: update name and email (Viero Fernando) [#40848](https://github.com/nodejs/node/pull/40848)
* \[[`813cf746a8`](https://github.com/nodejs/node/commit/813cf746a8)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#40809](https://github.com/nodejs/node/pull/40809)
* \[[`e451ec9ec1`](https://github.com/nodejs/node/commit/e451ec9ec1)] - **meta**: edit GOVERNANCE.md for minor updates (Rich Trott) [#40798](https://github.com/nodejs/node/pull/40798)
* \[[`2536be7528`](https://github.com/nodejs/node/commit/2536be7528)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#40748](https://github.com/nodejs/node/pull/40748)
* \[[`3c8aa211cd`](https://github.com/nodejs/node/commit/3c8aa211cd)] - **module**: import assertions improvements (Geoffrey Booth) [#40785](https://github.com/nodejs/node/pull/40785)
* \[[`3a4f3873be`](https://github.com/nodejs/node/commit/3a4f3873be)] - **process**: support hrtime in the snapshot (Joyee Cheung) [#40649](https://github.com/nodejs/node/pull/40649)
* \[[`1555dbdddc`](https://github.com/nodejs/node/commit/1555dbdddc)] - **repl**: fix and extend require/import tab complete (Mestery) [#40216](https://github.com/nodejs/node/pull/40216)
* \[[`c2259c974b`](https://github.com/nodejs/node/commit/c2259c974b)] - **src**: reset error struct if error code is napi\_ok (JckXia) [#40552](https://github.com/nodejs/node/pull/40552)
* \[[`3be49d6853`](https://github.com/nodejs/node/commit/3be49d6853)] - **src**: prevent extra copies of `TimerWrap::TimerCb` (Darshan Sen) [#40665](https://github.com/nodejs/node/pull/40665)
* \[[`0de2850680`](https://github.com/nodejs/node/commit/0de2850680)] - **(SEMVER-MINOR)** **src**: add x509.fingerprint512 to crypto module (3nprob) [#39809](https://github.com/nodejs/node/pull/39809)
* \[[`924d29e88f`](https://github.com/nodejs/node/commit/924d29e88f)] - **src**: add and fix some preprocessor comments (Tobias Nießen) [#40701](https://github.com/nodejs/node/pull/40701)
* \[[`acbd8220a7`](https://github.com/nodejs/node/commit/acbd8220a7)] - **src,crypto**: use `ByteSource::ToBuffer()` in `crypto_dh` (Darshan Sen) [#40903](https://github.com/nodejs/node/pull/40903)
* \[[`82b422663d`](https://github.com/nodejs/node/commit/82b422663d)] - **src,crypto**: remove `AllocatedBuffer`s from `crypto_spkac` (Darshan Sen) [#40752](https://github.com/nodejs/node/pull/40752)
* \[[`415b42fbdf`](https://github.com/nodejs/node/commit/415b42fbdf)] - **src,crypto**: refactor `crypto_tls.*` (Darshan Sen) [#40675](https://github.com/nodejs/node/pull/40675)
* \[[`88b57bc9d3`](https://github.com/nodejs/node/commit/88b57bc9d3)] - **src,doc**: add SyntaxError napi support (Idan Attias) [#40736](https://github.com/nodejs/node/pull/40736)
* \[[`70bd90e039`](https://github.com/nodejs/node/commit/70bd90e039)] - **stream**: stricter isReadableNodeStream (Robert Nagy) [#40941](https://github.com/nodejs/node/pull/40941)
* \[[`86d1c0b19d`](https://github.com/nodejs/node/commit/86d1c0b19d)] - **stream**: drain Transform with 0 highWaterMark (Robert Nagy) [#40947](https://github.com/nodejs/node/pull/40947)
* \[[`fa9b5c35d2`](https://github.com/nodejs/node/commit/fa9b5c35d2)] - **stream**: deprecate thenable support (Antoine du Hamel) [#40860](https://github.com/nodejs/node/pull/40860)
* \[[`dc99ae3bd6`](https://github.com/nodejs/node/commit/dc99ae3bd6)] - **stream**: fix the ReadableStreamBYOBReader error message (James M Snell) [#40846](https://github.com/nodejs/node/pull/40846)
* \[[`650c9bd022`](https://github.com/nodejs/node/commit/650c9bd022)] - **stream**: pipeline with end option (Robert Nagy) [#40886](https://github.com/nodejs/node/pull/40886)
* \[[`534409d4e7`](https://github.com/nodejs/node/commit/534409d4e7)] - **stream**: fix finished regression when working with legacy Stream (Matteo Collina) [#40858](https://github.com/nodejs/node/pull/40858)
* \[[`16ee8423d6`](https://github.com/nodejs/node/commit/16ee8423d6)] - **stream**: pipeline should drain empty readable (Robert Nagy) [#40654](https://github.com/nodejs/node/pull/40654)
* \[[`7d127d2fc1`](https://github.com/nodejs/node/commit/7d127d2fc1)] - **stream**: fix non readable Duplex readableAborted (Robert Nagy) [#40801](https://github.com/nodejs/node/pull/40801)
* \[[`7c4f9a34ae`](https://github.com/nodejs/node/commit/7c4f9a34ae)] - **stream**: allow calling callback before promise (Robert Nagy) [#40772](https://github.com/nodejs/node/pull/40772)
* \[[`62171eb516`](https://github.com/nodejs/node/commit/62171eb516)] - **test**: mark test-fs-watch-non-recursive flaky on Windows (Luigi Pinca) [#40916](https://github.com/nodejs/node/pull/40916)
* \[[`ae12a609a5`](https://github.com/nodejs/node/commit/ae12a609a5)] - **test**: improve test coverage of readline/promises (Yoshiki Kurihara) [#40876](https://github.com/nodejs/node/pull/40876)
* \[[`4288c6a26e`](https://github.com/nodejs/node/commit/4288c6a26e)] - **test**: deflake cluster-concurrent-disconnect (Luigi Pinca) [#40877](https://github.com/nodejs/node/pull/40877)
* \[[`009ab4d9bb`](https://github.com/nodejs/node/commit/009ab4d9bb)] - **test**: deflake fs-promises-watch (Luigi Pinca) [#40863](https://github.com/nodejs/node/pull/40863)
* \[[`522a30f469`](https://github.com/nodejs/node/commit/522a30f469)] - **test**: fix argument order in assertion (Luigi Pinca) [#40842](https://github.com/nodejs/node/pull/40842)
* \[[`b190b0e77b`](https://github.com/nodejs/node/commit/b190b0e77b)] - **test**: deflake http2-pipe-named-pipe (Luigi Pinca) [#40842](https://github.com/nodejs/node/pull/40842)
* \[[`c485460793`](https://github.com/nodejs/node/commit/c485460793)] - **test**: use descriptive name for destination file (Luigi Pinca) [#40842](https://github.com/nodejs/node/pull/40842)
* \[[`43e86508ae`](https://github.com/nodejs/node/commit/43e86508ae)] - **test**: add AsyncLocalStorage tests using udp, tcp and tls sockets (Darshan Sen) [#40741](https://github.com/nodejs/node/pull/40741)
* \[[`896073be47`](https://github.com/nodejs/node/commit/896073be47)] - **test**: deflake child-process-pipe-dataflow (Luigi Pinca) [#40838](https://github.com/nodejs/node/pull/40838)
* \[[`150c0f1b76`](https://github.com/nodejs/node/commit/150c0f1b76)] - **test**: add tests for invalid UTF-8 (git-srinivas) [#40351](https://github.com/nodejs/node/pull/40351)
* \[[`3d896231cd`](https://github.com/nodejs/node/commit/3d896231cd)] - **test**: fix flaky test-policy-integrity (Rich Trott) [#40763](https://github.com/nodejs/node/pull/40763)
* \[[`dc9e68899d`](https://github.com/nodejs/node/commit/dc9e68899d)] - **test**: add semicolons for linter update (Rich Trott) [#40720](https://github.com/nodejs/node/pull/40720)
* \[[`747247bc59`](https://github.com/nodejs/node/commit/747247bc59)] - **tools**: update gyp-next to v0.10.1 (Michaël Zasso) [#40934](https://github.com/nodejs/node/pull/40934)
* \[[`6ebbd0d9dc`](https://github.com/nodejs/node/commit/6ebbd0d9dc)] - **tools**: fix commit-lint GH Actions CI (Antoine du Hamel) [#40845](https://github.com/nodejs/node/pull/40845)
* \[[`0201f3f604`](https://github.com/nodejs/node/commit/0201f3f604)] - **tools**: ping TSC members identified as inactive (Rich Trott) [#40915](https://github.com/nodejs/node/pull/40915)
* \[[`22265e3fb6`](https://github.com/nodejs/node/commit/22265e3fb6)] - **tools**: update lint-md-dependencies to rollup\@2.60.1 (Node.js GitHub Bot) [#40929](https://github.com/nodejs/node/pull/40929)
* \[[`3d593aa4ed`](https://github.com/nodejs/node/commit/3d593aa4ed)] - **tools**: update lint-md dependencies (Rich Trott) [#40894](https://github.com/nodejs/node/pull/40894)
* \[[`e674f61720`](https://github.com/nodejs/node/commit/e674f61720)] - **tools**: update ESLint to 8.3.0 (Luigi Pinca) [#40917](https://github.com/nodejs/node/pull/40917)
* \[[`878ba91bf6`](https://github.com/nodejs/node/commit/878ba91bf6)] - **tools**: add find-inactive-tsc (Rich Trott) [#40884](https://github.com/nodejs/node/pull/40884)
* \[[`d97ad3080f`](https://github.com/nodejs/node/commit/d97ad3080f)] - **tools**: disable trap handler for Windows cross-compiler (Michaël Zasso) [#40488](https://github.com/nodejs/node/pull/40488)
* \[[`e3f8988118`](https://github.com/nodejs/node/commit/e3f8988118)] - **tools**: update V8 gypfiles for 9.6 (Michaël Zasso) [#40488](https://github.com/nodejs/node/pull/40488)
* \[[`de4d0180dc`](https://github.com/nodejs/node/commit/de4d0180dc)] - **tools**: update @babel/eslint-parser to 7.16.3 (Node.js GitHub Bot) [#40889](https://github.com/nodejs/node/pull/40889)
* \[[`727b34ec95`](https://github.com/nodejs/node/commit/727b34ec95)] - **tools**: only validate first commit message of a PR (Antoine du Hamel) [#40740](https://github.com/nodejs/node/pull/40740)
* \[[`5b08e908ea`](https://github.com/nodejs/node/commit/5b08e908ea)] - **tools**: update babel-eslint-parser to 7.16.0 (Rich Trott) [#40720](https://github.com/nodejs/node/pull/40720)
* \[[`30623c283a`](https://github.com/nodejs/node/commit/30623c283a)] - **tools**: improve update scripts (Rich Trott) [#40644](https://github.com/nodejs/node/pull/40644)

<a id="17.1.0"></a>

## 2021-11-09, Version 17.1.0 (Current), @targos

### Notable Changes

* \[[`89b34ecffb`](https://github.com/nodejs/node/commit/89b34ecffb)] - **doc**: add VoltrexMaster to collaborators (voltrexmaster) [#40566](https://github.com/nodejs/node/pull/40566)
* \[[`95e4d29eb4`](https://github.com/nodejs/node/commit/95e4d29eb4)] - **(SEMVER-MINOR)** **esm**: add support for JSON import assertion (Antoine du Hamel) [#40250](https://github.com/nodejs/node/pull/40250)
* \[[`1ddbae2d76`](https://github.com/nodejs/node/commit/1ddbae2d76)] - **(SEMVER-MINOR)** **lib**: add unsubscribe method to non-active DC channels (simon-id) [#40433](https://github.com/nodejs/node/pull/40433)
* \[[`aa61551b49`](https://github.com/nodejs/node/commit/aa61551b49)] - **(SEMVER-MINOR)** **lib**: add return value for DC channel.unsubscribe (simon-id) [#40433](https://github.com/nodejs/node/pull/40433)
* \[[`fbeb895ca6`](https://github.com/nodejs/node/commit/fbeb895ca6)] - **(SEMVER-MINOR)** **v8**: multi-tenant promise hook api (Stephen Belanger) [#39283](https://github.com/nodejs/node/pull/39283)

### Commits

* \[[`8a00dc5add`](https://github.com/nodejs/node/commit/8a00dc5add)] - **build**: skip long-running Actions for README-only modifications (Rich Trott) [#40571](https://github.com/nodejs/node/pull/40571)
* \[[`9f46fca124`](https://github.com/nodejs/node/commit/9f46fca124)] - **build**: disable v8 pointer compression on 32bit archs (Cheng Zhao) [#40418](https://github.com/nodejs/node/pull/40418)
* \[[`5bef74395d`](https://github.com/nodejs/node/commit/5bef74395d)] - **deps**: patch V8 to 9.5.172.25 (Michaël Zasso) [#40604](https://github.com/nodejs/node/pull/40604)
* \[[`3805b806ee`](https://github.com/nodejs/node/commit/3805b806ee)] - **deps**: upgrade npm to 8.1.2 (npm team) [#40643](https://github.com/nodejs/node/pull/40643)
* \[[`c003ba131b`](https://github.com/nodejs/node/commit/c003ba131b)] - **deps**: update c-ares to 1.18.1 (Richard Lau) [#40660](https://github.com/nodejs/node/pull/40660)
* \[[`841f35cc52`](https://github.com/nodejs/node/commit/841f35cc52)] - **deps**: upgrade npm to 8.1.1 (npm team) [#40554](https://github.com/nodejs/node/pull/40554)
* \[[`8d16f0d2d3`](https://github.com/nodejs/node/commit/8d16f0d2d3)] - **deps**: V8: cherry-pick 422dc378a1da (Ray Wang) [#40450](https://github.com/nodejs/node/pull/40450)
* \[[`cdf5c44d62`](https://github.com/nodejs/node/commit/cdf5c44d62)] - **deps**: add riscv64 config into openssl gypi (Lu Yahan) [#40473](https://github.com/nodejs/node/pull/40473)
* \[[`2b9fcdfe26`](https://github.com/nodejs/node/commit/2b9fcdfe26)] - **deps**: attempt to suppress macro-redefined warning (Daniel Bevenius) [#40518](https://github.com/nodejs/node/pull/40518)
* \[[`d2839bfaa9`](https://github.com/nodejs/node/commit/d2839bfaa9)] - **deps**: regenerate OpenSSL arch files (Daniel Bevenius) [#40518](https://github.com/nodejs/node/pull/40518)
* \[[`5df8ce5cbe`](https://github.com/nodejs/node/commit/5df8ce5cbe)] - **deps,build,tools**: fix openssl-is-fips for ninja builds (Daniel Bevenius) [#40518](https://github.com/nodejs/node/pull/40518)
* \[[`79bf429405`](https://github.com/nodejs/node/commit/79bf429405)] - **dgram**: fix send with out of bounds offset + length (Nitzan Uziely) [#40568](https://github.com/nodejs/node/pull/40568)
* \[[`c29658fda7`](https://github.com/nodejs/node/commit/c29658fda7)] - **doc**: update cjs-module-lexer repo link (Guy Bedford) [#40707](https://github.com/nodejs/node/pull/40707)
* \[[`e374f3ddd9`](https://github.com/nodejs/node/commit/e374f3ddd9)] - **doc**: fix lint re-enabling comment in README.md (Rich Trott) [#40647](https://github.com/nodejs/node/pull/40647)
* \[[`ecccf48106`](https://github.com/nodejs/node/commit/ecccf48106)] - **doc**: format v8.md in preparation for stricter linting (Rich Trott) [#40647](https://github.com/nodejs/node/pull/40647)
* \[[`95a7117037`](https://github.com/nodejs/node/commit/95a7117037)] - **doc**: final round of markdown format changes (Rich Trott) [#40645](https://github.com/nodejs/node/pull/40645)
* \[[`c104f5a9ab`](https://github.com/nodejs/node/commit/c104f5a9ab)] - **doc**: remove `--experimental-modules` documentation (FrankQiu) [#38974](https://github.com/nodejs/node/pull/38974)
* \[[`ac81f89bbf`](https://github.com/nodejs/node/commit/ac81f89bbf)] - **doc**: update tracking issues of startup performance (Joyee Cheung) [#40629](https://github.com/nodejs/node/pull/40629)
* \[[`65effa11fc`](https://github.com/nodejs/node/commit/65effa11fc)] - **doc**: fix markdown syntax and HTML tag misses (ryan) [#40608](https://github.com/nodejs/node/pull/40608)
* \[[`c78d708a16`](https://github.com/nodejs/node/commit/c78d708a16)] - **doc**: use 'GitHub Actions workflow' instead (Mestery) [#40586](https://github.com/nodejs/node/pull/40586)
* \[[`71bac70bf2`](https://github.com/nodejs/node/commit/71bac70bf2)] - **doc**: ref OpenSSL legacy provider from crypto docs (Tobias Nießen) [#40593](https://github.com/nodejs/node/pull/40593)
* \[[`8f410229ac`](https://github.com/nodejs/node/commit/8f410229ac)] - **doc**: add node: url scheme (Daniel Nalborczyk) [#40573](https://github.com/nodejs/node/pull/40573)
* \[[`35dbed21f2`](https://github.com/nodejs/node/commit/35dbed21f2)] - **doc**: call cwd function (Daniel Nalborczyk) [#40573](https://github.com/nodejs/node/pull/40573)
* \[[`4870a23ccc`](https://github.com/nodejs/node/commit/4870a23ccc)] - **doc**: remove unused imports (Daniel Nalborczyk) [#40573](https://github.com/nodejs/node/pull/40573)
* \[[`5951ccc12e`](https://github.com/nodejs/node/commit/5951ccc12e)] - **doc**: simplify CHANGELOG.md (Rich Trott) [#40475](https://github.com/nodejs/node/pull/40475)
* \[[`6ae134ecb7`](https://github.com/nodejs/node/commit/6ae134ecb7)] - **doc**: correct esm spec scope lookup definition (Guy Bedford) [#40592](https://github.com/nodejs/node/pull/40592)
* \[[`09239216f6`](https://github.com/nodejs/node/commit/09239216f6)] - **doc**: update CHANGELOG.md for Node.js 16.13.0 (Richard Lau) [#40617](https://github.com/nodejs/node/pull/40617)
* \[[`46ec5ac4df`](https://github.com/nodejs/node/commit/46ec5ac4df)] - **doc**: add info on project's usage of coverity (Michael Dawson) [#40506](https://github.com/nodejs/node/pull/40506)
* \[[`7eb1a44410`](https://github.com/nodejs/node/commit/7eb1a44410)] - **doc**: fix typo in changelogs (Luigi Pinca) [#40585](https://github.com/nodejs/node/pull/40585)
* \[[`132f6cba05`](https://github.com/nodejs/node/commit/132f6cba05)] - **doc**: update onboarding task (Rich Trott) [#40570](https://github.com/nodejs/node/pull/40570)
* \[[`5e2d0ed61e`](https://github.com/nodejs/node/commit/5e2d0ed61e)] - **doc**: simplify ccache instructions (Rich Trott) [#40550](https://github.com/nodejs/node/pull/40550)
* \[[`c1c1738bfc`](https://github.com/nodejs/node/commit/c1c1738bfc)] - **doc**: fix macOS environment variables for ccache (Rich Trott) [#40550](https://github.com/nodejs/node/pull/40550)
* \[[`6e3e50f2ab`](https://github.com/nodejs/node/commit/6e3e50f2ab)] - **doc**: improve async\_context introduction (Michaël Zasso) [#40560](https://github.com/nodejs/node/pull/40560)
* \[[`1587fe62d4`](https://github.com/nodejs/node/commit/1587fe62d4)] - **doc**: use GFM footnotes in webcrypto.md (Rich Trott) [#40477](https://github.com/nodejs/node/pull/40477)
* \[[`305c022db4`](https://github.com/nodejs/node/commit/305c022db4)] - **doc**: describe buffer limit of v8.serialize (Ray Wang) [#40243](https://github.com/nodejs/node/pull/40243)
* \[[`6e39e0e10a`](https://github.com/nodejs/node/commit/6e39e0e10a)] - **doc**: run license-builder (Rich Trott) [#40540](https://github.com/nodejs/node/pull/40540)
* \[[`556e49ccb5`](https://github.com/nodejs/node/commit/556e49ccb5)] - **doc**: use GFM footnotes in maintaining-V8.md (#40476) (Rich Trott) [#40476](https://github.com/nodejs/node/pull/40476)
* \[[`9c6a9fd5b1`](https://github.com/nodejs/node/commit/9c6a9fd5b1)] - **doc**: use GFM footnotes in BUILDING.md (Rich Trott) [#40474](https://github.com/nodejs/node/pull/40474)
* \[[`fd946215cc`](https://github.com/nodejs/node/commit/fd946215cc)] - **doc**: fix `fs.symlink` code example (Juan José Arboleda) [#40414](https://github.com/nodejs/node/pull/40414)
* \[[`404730ac1b`](https://github.com/nodejs/node/commit/404730ac1b)] - **doc**: update for changed `--dns-result-order` default (Richard Lau) [#40538](https://github.com/nodejs/node/pull/40538)
* \[[`acc22c7c4a`](https://github.com/nodejs/node/commit/acc22c7c4a)] - **doc**: add missing entry in `globals.md` (Antoine du Hamel) [#40531](https://github.com/nodejs/node/pull/40531)
* \[[`0375d958ef`](https://github.com/nodejs/node/commit/0375d958ef)] - **doc**: explain backport labels (Stephen Belanger) [#40520](https://github.com/nodejs/node/pull/40520)
* \[[`4993d87c49`](https://github.com/nodejs/node/commit/4993d87c49)] - **doc**: fix entry for Slack channel in onboarding.md (Rich Trott) [#40563](https://github.com/nodejs/node/pull/40563)
* \[[`89b34ecffb`](https://github.com/nodejs/node/commit/89b34ecffb)] - **doc**: add VoltrexMaster to collaborators (voltrexmaster) [#40566](https://github.com/nodejs/node/pull/40566)
* \[[`6357ef15d0`](https://github.com/nodejs/node/commit/6357ef15d0)] - **doc**: document considerations for inclusion in core (Rich Trott) [#40338](https://github.com/nodejs/node/pull/40338)
* \[[`ed04827373`](https://github.com/nodejs/node/commit/ed04827373)] - **doc**: update link in onboarding doc (Rich Trott) [#40539](https://github.com/nodejs/node/pull/40539)
* \[[`34e244b8e9`](https://github.com/nodejs/node/commit/34e244b8e9)] - **doc**: clarify behavior of napi\_extended\_error\_info (Michael Dawson) [#40458](https://github.com/nodejs/node/pull/40458)
* \[[`5a588ff047`](https://github.com/nodejs/node/commit/5a588ff047)] - **doc**: add updating expected assets to release guide (Richard Lau) [#40470](https://github.com/nodejs/node/pull/40470)
* \[[`95e4d29eb4`](https://github.com/nodejs/node/commit/95e4d29eb4)] - **(SEMVER-MINOR)** **esm**: add support for JSON import assertion (Antoine du Hamel) [#40250](https://github.com/nodejs/node/pull/40250)
* \[[`825a683423`](https://github.com/nodejs/node/commit/825a683423)] - **http**: response should always emit 'close' (Robert Nagy) [#40543](https://github.com/nodejs/node/pull/40543)
* \[[`81cd7f3751`](https://github.com/nodejs/node/commit/81cd7f3751)] - **lib**: fix regular expression to detect \`/\` and \`\\\` (Francesco Trotta) [#40325](https://github.com/nodejs/node/pull/40325)
* \[[`1ddbae2d76`](https://github.com/nodejs/node/commit/1ddbae2d76)] - **(SEMVER-MINOR)** **lib**: add unsubscribe method to non-active DC channels (simon-id) [#40433](https://github.com/nodejs/node/pull/40433)
* \[[`aa61551b49`](https://github.com/nodejs/node/commit/aa61551b49)] - **(SEMVER-MINOR)** **lib**: add return value for DC channel.unsubscribe (simon-id) [#40433](https://github.com/nodejs/node/pull/40433)
* \[[`d97872dd98`](https://github.com/nodejs/node/commit/d97872dd98)] - **meta**: use form schema for flaky test template (Michaël Zasso) [#40737](https://github.com/nodejs/node/pull/40737)
* \[[`c2fabdbce8`](https://github.com/nodejs/node/commit/c2fabdbce8)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#40668](https://github.com/nodejs/node/pull/40668)
* \[[`aa98c6bdce`](https://github.com/nodejs/node/commit/aa98c6bdce)] - **meta**: consolidate AUTHORS entries for brettkiefer (Rich Trott) [#40599](https://github.com/nodejs/node/pull/40599)
* \[[`18296c3d8e`](https://github.com/nodejs/node/commit/18296c3d8e)] - **meta**: consolidate AUTHORS entries for alexzherdev (Rich Trott) [#40620](https://github.com/nodejs/node/pull/40620)
* \[[`88d812793d`](https://github.com/nodejs/node/commit/88d812793d)] - **meta**: consolidate AUTHORS entries for Azard (Rich Trott) [#40619](https://github.com/nodejs/node/pull/40619)
* \[[`d81b65ca0e`](https://github.com/nodejs/node/commit/d81b65ca0e)] - **meta**: move Fishrock123 to emeritus (Jeremiah Senkpiel) [#40596](https://github.com/nodejs/node/pull/40596)
* \[[`ec02e7b68a`](https://github.com/nodejs/node/commit/ec02e7b68a)] - **meta**: consolidate AUTHORS entries for clakech (Rich Trott) [#40589](https://github.com/nodejs/node/pull/40589)
* \[[`08e7a2ff24`](https://github.com/nodejs/node/commit/08e7a2ff24)] - **meta**: consolidate AUTHORS entries for darai0512 (Rich Trott) [#40569](https://github.com/nodejs/node/pull/40569)
* \[[`488ee51f90`](https://github.com/nodejs/node/commit/488ee51f90)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#40580](https://github.com/nodejs/node/pull/40580)
* \[[`ba9a6c5d06`](https://github.com/nodejs/node/commit/ba9a6c5d06)] - **meta**: consolidate AUTHORS entries for dfabulich (Rich Trott) [#40527](https://github.com/nodejs/node/pull/40527)
* \[[`bd06e9945e`](https://github.com/nodejs/node/commit/bd06e9945e)] - **meta**: move one or more collaborators to emeritus (Node.js GitHub Bot) [#40464](https://github.com/nodejs/node/pull/40464)
* \[[`7cee125dcf`](https://github.com/nodejs/node/commit/7cee125dcf)] - **meta**: add Richard Lau to TSC list in README.md (Rich Trott) [#40523](https://github.com/nodejs/node/pull/40523)
* \[[`6a771f8bcb`](https://github.com/nodejs/node/commit/6a771f8bcb)] - **meta**: consolidate AUTHORS entries for dguo (Rich Trott) [#40517](https://github.com/nodejs/node/pull/40517)
* \[[`e4c740646d`](https://github.com/nodejs/node/commit/e4c740646d)] - **meta**: consolidate AUTHORS entries for cxreg (Rich Trott) [#40490](https://github.com/nodejs/node/pull/40490)
* \[[`075cfbf941`](https://github.com/nodejs/node/commit/075cfbf941)] - **module**: resolver & spec hardening /w refactoring (Guy Bedford) [#40510](https://github.com/nodejs/node/pull/40510)
* \[[`b320387c83`](https://github.com/nodejs/node/commit/b320387c83)] - **policy**: fix message for invalid manifest specifier (Rich Trott) [#40574](https://github.com/nodejs/node/pull/40574)
* \[[`ea968d54c5`](https://github.com/nodejs/node/commit/ea968d54c5)] - **process**: refactor execution (Voltrex) [#40664](https://github.com/nodejs/node/pull/40664)
* \[[`fb7c437b0b`](https://github.com/nodejs/node/commit/fb7c437b0b)] - **src**: make LoadEnvironment with string work with builtin modules path (Michaël Zasso) [#40607](https://github.com/nodejs/node/pull/40607)
* \[[`e9388c87bf`](https://github.com/nodejs/node/commit/e9388c87bf)] - **src**: remove usage of `AllocatedBuffer` from `node_http2` (Darshan Sen) [#40584](https://github.com/nodejs/node/pull/40584)
* \[[`7a22f913b0`](https://github.com/nodejs/node/commit/7a22f913b0)] - **src**: fix #endif description in crypto\_keygen.h (Tobias Nießen) [#40639](https://github.com/nodejs/node/pull/40639)
* \[[`396342e26d`](https://github.com/nodejs/node/commit/396342e26d)] - **src**: throw error instead of assertion (Ray Wang) [#40243](https://github.com/nodejs/node/pull/40243)
* \[[`accab383a1`](https://github.com/nodejs/node/commit/accab383a1)] - **src**: register external references in os bindings (Joyee Cheung) [#40239](https://github.com/nodejs/node/pull/40239)
* \[[`a11f9ea4f0`](https://github.com/nodejs/node/commit/a11f9ea4f0)] - **src**: register external references in crypto bindings (Joyee Cheung) [#40239](https://github.com/nodejs/node/pull/40239)
* \[[`ef1ace7e88`](https://github.com/nodejs/node/commit/ef1ace7e88)] - **src,crypto**: use `std::variant` in DH params (Darshan Sen) [#40457](https://github.com/nodejs/node/pull/40457)
* \[[`4433852f62`](https://github.com/nodejs/node/commit/4433852f62)] - **src,crypto**: remove `AllocatedBuffer` from `crypto_cipher.cc` (Darshan Sen) [#40400](https://github.com/nodejs/node/pull/40400)
* \[[`814126c3ed`](https://github.com/nodejs/node/commit/814126c3ed)] - **src,fs**: remove `ToLocalChecked()` call from `fs::AfterMkdirp()` (Darshan Sen) [#40386](https://github.com/nodejs/node/pull/40386)
* \[[`d4b45cc249`](https://github.com/nodejs/node/commit/d4b45cc249)] - **src,stream**: remove `*Check*()` calls from non-`Initialize()` functions (Darshan Sen) [#40425](https://github.com/nodejs/node/pull/40425)
* \[[`bac7fe0797`](https://github.com/nodejs/node/commit/bac7fe0797)] - **stream**: remove no longer necessary ComposeDuplex (Robert Nagy) [#40545](https://github.com/nodejs/node/pull/40545)
* \[[`e58cce49fd`](https://github.com/nodejs/node/commit/e58cce49fd)] - **test**: disable warnings to fix flaky test (Antoine du Hamel) [#40739](https://github.com/nodejs/node/pull/40739)
* \[[`8c103ab2ff`](https://github.com/nodejs/node/commit/8c103ab2ff)] - **test**: skip macos sandbox test with builtin modules path (Michaël Zasso) [#40607](https://github.com/nodejs/node/pull/40607)
* \[[`ac3bc6eed0`](https://github.com/nodejs/node/commit/ac3bc6eed0)] - **test**: add semicolon after chunk size (Luigi Pinca) [#40487](https://github.com/nodejs/node/pull/40487)
* \[[`95fe9bb922`](https://github.com/nodejs/node/commit/95fe9bb922)] - **test**: deflake http2-cancel-while-client-reading (Luigi Pinca) [#40659](https://github.com/nodejs/node/pull/40659)
* \[[`dfd0215266`](https://github.com/nodejs/node/commit/dfd0215266)] - **test**: avoid deep comparisons with literals (Tobias Nießen) [#40634](https://github.com/nodejs/node/pull/40634)
* \[[`5020f634b8`](https://github.com/nodejs/node/commit/5020f634b8)] - **test**: mark test-policy-integrity flaky on Windows (Rich Trott) [#40684](https://github.com/nodejs/node/pull/40684)
* \[[`8fa1c61e40`](https://github.com/nodejs/node/commit/8fa1c61e40)] - **test**: fix test-datetime-change-notify after daylight change (Piotr Rybak) [#40684](https://github.com/nodejs/node/pull/40684)
* \[[`179a5c5436`](https://github.com/nodejs/node/commit/179a5c5436)] - **test**: test `crypto.setEngine()` using an actual engine (Darshan Sen) [#40481](https://github.com/nodejs/node/pull/40481)
* \[[`cf6ded4db5`](https://github.com/nodejs/node/commit/cf6ded4db5)] - **test**: use conventional argument order in assertion (Tobias Nießen) [#40591](https://github.com/nodejs/node/pull/40591)
* \[[`aefb097d6a`](https://github.com/nodejs/node/commit/aefb097d6a)] - **test**: fix test description (Luigi Pinca) [#40486](https://github.com/nodejs/node/pull/40486)
* \[[`126e669b84`](https://github.com/nodejs/node/commit/126e669b84)] - **test,doc**: correct documentation for runBenchmark() (Rich Trott) [#40683](https://github.com/nodejs/node/pull/40683)
* \[[`1844463ce2`](https://github.com/nodejs/node/commit/1844463ce2)] - **test,tools**: increase pummel/benchmark test timeout from 4x to 6x (Rich Trott) [#40684](https://github.com/nodejs/node/pull/40684)
* \[[`f731f5ffb5`](https://github.com/nodejs/node/commit/f731f5ffb5)] - **test,tools**: increase timeout for benchmark tests (Rich Trott) [#40684](https://github.com/nodejs/node/pull/40684)
* \[[`bbc10f1849`](https://github.com/nodejs/node/commit/bbc10f1849)] - **tools**: simplify and fix commit queue (Michaël Zasso) [#40742](https://github.com/nodejs/node/pull/40742)
* \[[`a3df50d810`](https://github.com/nodejs/node/commit/a3df50d810)] - **tools**: ensure the PR was not pushed before merging (Antoine du Hamel) [#40747](https://github.com/nodejs/node/pull/40747)
* \[[`306d953c15`](https://github.com/nodejs/node/commit/306d953c15)] - **tools**: update ESLint to 8.2.0 (Luigi Pinca) [#40734](https://github.com/nodejs/node/pull/40734)
* \[[`b7e736843c`](https://github.com/nodejs/node/commit/b7e736843c)] - **tools**: use GitHub Squash and Merge feature when using CQ (Antoine du Hamel) [#40666](https://github.com/nodejs/node/pull/40666)
* \[[`50d102ec08`](https://github.com/nodejs/node/commit/50d102ec08)] - **tools**: fix bug in `prefer-primordials` ESLint rule (Antoine du Hamel) [#40628](https://github.com/nodejs/node/pull/40628)
* \[[`ec2cadef85`](https://github.com/nodejs/node/commit/ec2cadef85)] - **tools**: add script to update c-ares (Richard Lau) [#40660](https://github.com/nodejs/node/pull/40660)
* \[[`5daa313215`](https://github.com/nodejs/node/commit/5daa313215)] - **tools**: notify user if format-md needs to be run (Rich Trott) [#40647](https://github.com/nodejs/node/pull/40647)
* \[[`0787c781ce`](https://github.com/nodejs/node/commit/0787c781ce)] - **tools**: abort CQ session when landing several commits (Antoine du Hamel) [#40577](https://github.com/nodejs/node/pull/40577)
* \[[`ddc44ddfd9`](https://github.com/nodejs/node/commit/ddc44ddfd9)] - **tools**: fix commit-lint workflow (Antoine du Hamel) [#40673](https://github.com/nodejs/node/pull/40673)
* \[[`47eddd7076`](https://github.com/nodejs/node/commit/47eddd7076)] - **tools**: avoid unnecessary escaping in markdown formatter (Rich Trott) [#40645](https://github.com/nodejs/node/pull/40645)
* \[[`c700de3705`](https://github.com/nodejs/node/commit/c700de3705)] - **tools**: avoid fetch extra commits when validating commit messages (Antoine du Hamel) [#39128](https://github.com/nodejs/node/pull/39128)
* \[[`716963484b`](https://github.com/nodejs/node/commit/716963484b)] - **tools**: update ESLint to 8.1.0 (Luigi Pinca) [#40582](https://github.com/nodejs/node/pull/40582)
* \[[`9cb2116608`](https://github.com/nodejs/node/commit/9cb2116608)] - **tools**: fix formatting of warning message in update-authors.js (Rich Trott) [#40600](https://github.com/nodejs/node/pull/40600)
* \[[`507f1dbc8d`](https://github.com/nodejs/node/commit/507f1dbc8d)] - **tools**: udpate doc tools to accommodate GFM footnotes (Rich Trott) [#40477](https://github.com/nodejs/node/pull/40477)
* \[[`c2265a92c3`](https://github.com/nodejs/node/commit/c2265a92c3)] - **tools**: update license-builder.sh for OpenSSL (Rich Trott) [#40540](https://github.com/nodejs/node/pull/40540)
* \[[`16624b404c`](https://github.com/nodejs/node/commit/16624b404c)] - **tools,meta**: remove exclusions from AUTHORS (Rich Trott) [#40648](https://github.com/nodejs/node/pull/40648)
* \[[`a95e344fe5`](https://github.com/nodejs/node/commit/a95e344fe5)] - **tty**: support more CI services in `getColorDepth` (Richie Bendall) [#40385](https://github.com/nodejs/node/pull/40385)
* \[[`b4194ff349`](https://github.com/nodejs/node/commit/b4194ff349)] - **typings**: add more bindings typings (Mestery) [#40415](https://github.com/nodejs/node/pull/40415)
* \[[`da859b56cb`](https://github.com/nodejs/node/commit/da859b56cb)] - **typings**: add JSDoc typings for inspector (Voltrex) [#38390](https://github.com/nodejs/node/pull/38390)
* \[[`90aa96dc44`](https://github.com/nodejs/node/commit/90aa96dc44)] - **typings**: improve internal bindings typings (Mestery) [#40411](https://github.com/nodejs/node/pull/40411)
* \[[`1e9f3cc522`](https://github.com/nodejs/node/commit/1e9f3cc522)] - **typings**: separate `internalBinding` typings (Mestery) [#40409](https://github.com/nodejs/node/pull/40409)
* \[[`fbeb895ca6`](https://github.com/nodejs/node/commit/fbeb895ca6)] - **(SEMVER-MINOR)** **v8**: multi-tenant promise hook api (Stephen Belanger) [#39283](https://github.com/nodejs/node/pull/39283)

<a id="17.0.1"></a>

## 2021-10-20, Version 17.0.1 (Current), @targos

### Notable Changes

#### Fixed distribution for native addon builds

This release fixes an issue introduced in Node.js v17.0.0, where some V8 headers
were missing from the distributed tarball, making it impossible to build native
addons. These headers are now included. [#40526](https://github.com/nodejs/node/pull/40526)

#### Fixed stream issues

* Fixed a regression in `stream.promises.pipeline`, which was introduced in version
  16.10.0, is fixed. It is now possible again to pass an array of streams to the
  function. [#40193](https://github.com/nodejs/node/pull/40193)
* Fixed a bug in `stream.Duplex.from`, which didn't work properly when an async
  generator function was passed to it. [#40499](https://github.com/nodejs/node/pull/40499)

### Commits

* \[[`3f033556c3`](https://github.com/nodejs/node/commit/3f033556c3)] - **build**: include missing V8 headers in distribution (Michaël Zasso) [#40526](https://github.com/nodejs/node/pull/40526)
* \[[`adbd92ef1d`](https://github.com/nodejs/node/commit/adbd92ef1d)] - **crypto**: avoid double free (Michael Dawson) [#40380](https://github.com/nodejs/node/pull/40380)
* \[[`8dce85aadc`](https://github.com/nodejs/node/commit/8dce85aadc)] - **doc**: format doc/api/\*.md with markdown formatter (Rich Trott) [#40403](https://github.com/nodejs/node/pull/40403)
* \[[`977016a72f`](https://github.com/nodejs/node/commit/977016a72f)] - **doc**: specify that maxFreeSockets is per host (Luigi Pinca) [#40483](https://github.com/nodejs/node/pull/40483)
* \[[`f9f2442739`](https://github.com/nodejs/node/commit/f9f2442739)] - **src**: add missing inialization in agent.h (Michael Dawson) [#40379](https://github.com/nodejs/node/pull/40379)
* \[[`111f0bd9b6`](https://github.com/nodejs/node/commit/111f0bd9b6)] - **stream**: fix fromAsyncGen (Robert Nagy) [#40499](https://github.com/nodejs/node/pull/40499)
* \[[`b84f101049`](https://github.com/nodejs/node/commit/b84f101049)] - **stream**: support array of streams in promises pipeline (Mestery) [#40193](https://github.com/nodejs/node/pull/40193)
* \[[`3f7c503b69`](https://github.com/nodejs/node/commit/3f7c503b69)] - **test**: adjust CLI flags test to ignore blank lines in doc (Rich Trott) [#40403](https://github.com/nodejs/node/pull/40403)
* \[[`7c42d9fcc6`](https://github.com/nodejs/node/commit/7c42d9fcc6)] - **test**: split test-crypto-dh.js (Joyee Cheung) [#40451](https://github.com/nodejs/node/pull/40451)

<a id="17.0.0"></a>

## 2021-10-19, Version 17.0.0 (Current), @BethGriggs

### Notable Changes

#### Deprecations and Removals

* \[[`f182b9b29f`](https://github.com/nodejs/node/commit/f182b9b29f)] - **(SEMVER-MAJOR)** **dns**: runtime deprecate type coercion of `dns.lookup` options (Antoine du Hamel) [#39793](https://github.com/nodejs/node/pull/39793)
* \[[`4b030d0573`](https://github.com/nodejs/node/commit/4b030d0573)] - **doc**: deprecate (doc-only) http abort related (dr-js) [#36670](https://github.com/nodejs/node/pull/36670)
* \[[`36e2ffe6dc`](https://github.com/nodejs/node/commit/36e2ffe6dc)] - **(SEMVER-MAJOR)** **module**: subpath folder mappings EOL (Guy Bedford) [#40121](https://github.com/nodejs/node/pull/40121)
* \[[`64287e4d45`](https://github.com/nodejs/node/commit/64287e4d45)] - **(SEMVER-MAJOR)** **module**: runtime deprecate trailing slash patterns (Guy Bedford) [#40117](https://github.com/nodejs/node/pull/40117)

#### OpenSSL 3.0

Node.js now includes OpenSSL 3.0, specifically [quictls/openssl](https://github.com/quictls/openssl) which provides QUIC support. With OpenSSL 3.0 FIPS support is again available using the new FIPS module. For details about how to build Node.js with FIPS support please see [BUILDING.md](https://github.com/nodejs/node/blob/master/BUILDING.md#building-nodejs-with-fips-compliant-openssl).

While OpenSSL 3.0 APIs should be mostly compatible with those provided by OpenSSL 1.1.1, we do anticipate some ecosystem impact due to tightened restrictions on the allowed algorithms and key sizes.

If you hit an `ERR_OSSL_EVP_UNSUPPORTED` error in your application with Node.js 17, it’s likely that your application or a module you’re using is attempting to use an algorithm or key size which is no longer allowed by default with OpenSSL 3.0. A command-line option, `--openssl-legacy-provider`, has been added to revert to the legacy provider as a temporary workaround for these tightened restrictions.

For details about all the features in OpenSSL 3.0 please see the [OpenSSL 3.0 release blog](https://www.openssl.org/blog/blog/2021/09/07/OpenSSL3.Final).

Contributed in <https://github.com/nodejs/node/pull/38512>, <https://github.com/nodejs/node/pull/40478>

#### V8 9.5

The V8 JavaScript engine is updated to V8 9.5. This release comes with additional supported types for the `Intl.DisplayNames` API and Extended `timeZoneName` options in the `Intl.DateTimeFormat` API.

You can read more details in the V8 9.5 release post - <https://v8.dev/blog/v8-release-95>.

Contributed by Michaël Zasso - <https://github.com/nodejs/node/pull/40178>

#### Readline Promise API

The `readline` module provides an interface for reading data from a Readable
stream (such as `process.stdin`) one line at a time.

The following simple example illustrates the basic use of the `readline` module:

```mjs
import * as readline from 'node:readline/promises';
import { stdin as input, stdout as output } from 'process';

const rl = readline.createInterface({ input, output });

const answer = await rl.question('What do you think of Node.js? ');

console.log(`Thank you for your valuable feedback: ${answer}`);

rl.close();
```

Contributed by Antoine du Hamel - <https://github.com/nodejs/node/pull/37947>

#### Other Notable Changes

* \[[`1b2749ecbe`](https://github.com/nodejs/node/commit/1b2749ecbe)] - **(SEMVER-MAJOR)** **dns**: default to verbatim=true in dns.lookup() (treysis) [#39987](https://github.com/nodejs/node/pull/39987)
* \[[`59d3d542d6`](https://github.com/nodejs/node/commit/59d3d542d6)] - **(SEMVER-MAJOR)** **errors**: print Node.js version on fatal exceptions that cause exit (Divlo) [#38332](https://github.com/nodejs/node/pull/38332)
* \[[`a35b7e0427`](https://github.com/nodejs/node/commit/a35b7e0427)] - **deps**: upgrade npm to 8.1.0 (npm team) [#40463](https://github.com/nodejs/node/pull/40463)
* \[[`6cd12be347`](https://github.com/nodejs/node/commit/6cd12be347)] - **(SEMVER-MINOR)** **fs**: add FileHandle.prototype.readableWebStream() (James M Snell) [#39331](https://github.com/nodejs/node/pull/39331)
* \[[`d0a898681f`](https://github.com/nodejs/node/commit/d0a898681f)] - **(SEMVER-MAJOR)** **lib**: add structuredClone() global (Ethan Arrowood) [#39759](https://github.com/nodejs/node/pull/39759)
* \[[`e4b1fb5e64`](https://github.com/nodejs/node/commit/e4b1fb5e64)] - **(SEMVER-MAJOR)** **lib**: expose `DOMException` as global (Khaidi Chu) [#39176](https://github.com/nodejs/node/pull/39176)
* \[[`0738a2b7bd`](https://github.com/nodejs/node/commit/0738a2b7bd)] - **(SEMVER-MAJOR)** **stream**: finished should error on errored stream (Robert Nagy) [#39235](https://github.com/nodejs/node/pull/39235)

### Semver-Major Commits

* \[[`9dfa30bdd5`](https://github.com/nodejs/node/commit/9dfa30bdd5)] - **(SEMVER-MAJOR)** **build**: compile with C++17 (MSVC) (Richard Lau) [#38807](https://github.com/nodejs/node/pull/38807)
* \[[`9f0bc602e4`](https://github.com/nodejs/node/commit/9f0bc602e4)] - **(SEMVER-MAJOR)** **build**: compile with --gnu++17 (Richard Lau) [#38807](https://github.com/nodejs/node/pull/38807)
* \[[`62719c5fd2`](https://github.com/nodejs/node/commit/62719c5fd2)] - **(SEMVER-MAJOR)** **deps**: update V8 to 9.5.172.19 (Michaël Zasso) [#40178](https://github.com/nodejs/node/pull/40178)
* \[[`66da32c045`](https://github.com/nodejs/node/commit/66da32c045)] - **(SEMVER-MAJOR)** **deps,test,src,doc,tools**: update to OpenSSL 3.0 (Daniel Bevenius) [#38512](https://github.com/nodejs/node/pull/38512)
* \[[`40c6e838df`](https://github.com/nodejs/node/commit/40c6e838df)] - **(SEMVER-MAJOR)** **dgram**: tighten `address` validation in `socket.send` (Voltrex) [#39190](https://github.com/nodejs/node/pull/39190)
* \[[`f182b9b29f`](https://github.com/nodejs/node/commit/f182b9b29f)] - **(SEMVER-MAJOR)** **dns**: runtime deprecate type coercion of `dns.lookup` options (Antoine du Hamel) [#39793](https://github.com/nodejs/node/pull/39793)
* \[[`1b2749ecbe`](https://github.com/nodejs/node/commit/1b2749ecbe)] - **(SEMVER-MAJOR)** **dns**: default to verbatim=true in dns.lookup() (treysis) [#39987](https://github.com/nodejs/node/pull/39987)
* \[[`ae876d420c`](https://github.com/nodejs/node/commit/ae876d420c)] - **(SEMVER-MAJOR)** **doc**: update minimum supported FreeBSD to 12.2 (Michaël Zasso) [#40179](https://github.com/nodejs/node/pull/40179)
* \[[`59d3d542d6`](https://github.com/nodejs/node/commit/59d3d542d6)] - **(SEMVER-MAJOR)** **errors**: print Node.js version on fatal exceptions that cause exit (Divlo) [#38332](https://github.com/nodejs/node/pull/38332)
* \[[`f9447b71a6`](https://github.com/nodejs/node/commit/f9447b71a6)] - **(SEMVER-MAJOR)** **fs**: fix rmsync error swallowing (Nitzan Uziely) [#38684](https://github.com/nodejs/node/pull/38684)
* \[[`f27b7cf95c`](https://github.com/nodejs/node/commit/f27b7cf95c)] - **(SEMVER-MAJOR)** **fs**: aggregate errors in fsPromises to avoid error swallowing (Nitzan Uziely) [#38259](https://github.com/nodejs/node/pull/38259)
* \[[`d0a898681f`](https://github.com/nodejs/node/commit/d0a898681f)] - **(SEMVER-MAJOR)** **lib**: add structuredClone() global (Ethan Arrowood) [#39759](https://github.com/nodejs/node/pull/39759)
* \[[`e4b1fb5e64`](https://github.com/nodejs/node/commit/e4b1fb5e64)] - **(SEMVER-MAJOR)** **lib**: expose `DOMException` as global (Khaidi Chu) [#39176](https://github.com/nodejs/node/pull/39176)
* \[[`36e2ffe6dc`](https://github.com/nodejs/node/commit/36e2ffe6dc)] - **(SEMVER-MAJOR)** **module**: subpath folder mappings EOL (Guy Bedford) [#40121](https://github.com/nodejs/node/pull/40121)
* \[[`64287e4d45`](https://github.com/nodejs/node/commit/64287e4d45)] - **(SEMVER-MAJOR)** **module**: runtime deprecate trailing slash patterns (Guy Bedford) [#40117](https://github.com/nodejs/node/pull/40117)
* \[[`707dd77d86`](https://github.com/nodejs/node/commit/707dd77d86)] - **(SEMVER-MAJOR)** **readline**: validate `AbortSignal`s and remove unused event listeners (Antoine du Hamel) [#37947](https://github.com/nodejs/node/pull/37947)
* \[[`8122d243ae`](https://github.com/nodejs/node/commit/8122d243ae)] - **(SEMVER-MAJOR)** **readline**: introduce promise-based API (Antoine du Hamel) [#37947](https://github.com/nodejs/node/pull/37947)
* \[[`592d1c3d44`](https://github.com/nodejs/node/commit/592d1c3d44)] - **(SEMVER-MAJOR)** **readline**: refactor `Interface` to ES2015 class (Antoine du Hamel) [#37947](https://github.com/nodejs/node/pull/37947)
* \[[`3f619407fe`](https://github.com/nodejs/node/commit/3f619407fe)] - **(SEMVER-MAJOR)** **src**: allow CAP\_NET\_BIND\_SERVICE in SafeGetenv (Daniel Bevenius) [#37727](https://github.com/nodejs/node/pull/37727)
* \[[`0a7f850123`](https://github.com/nodejs/node/commit/0a7f850123)] - **(SEMVER-MAJOR)** **src**: return Maybe from a couple of functions (Darshan Sen) [#39603](https://github.com/nodejs/node/pull/39603)
* \[[`bdaf51bae7`](https://github.com/nodejs/node/commit/bdaf51bae7)] - **(SEMVER-MAJOR)** **src**: allow custom PageAllocator in NodePlatform (Shelley Vohr) [#38362](https://github.com/nodejs/node/pull/38362)
* \[[`0c6f345cda`](https://github.com/nodejs/node/commit/0c6f345cda)] - **(SEMVER-MAJOR)** **stream**: fix highwatermark threshold and add the missing error (Rongjian Zhang) [#38700](https://github.com/nodejs/node/pull/38700)
* \[[`0e841b45c2`](https://github.com/nodejs/node/commit/0e841b45c2)] - **(SEMVER-MAJOR)** **stream**: don't emit 'data' after 'error' or 'close' (Robert Nagy) [#39639](https://github.com/nodejs/node/pull/39639)
* \[[`ef992f6de9`](https://github.com/nodejs/node/commit/ef992f6de9)] - **(SEMVER-MAJOR)** **stream**: do not emit `end` on readable error (Szymon Marczak) [#39607](https://github.com/nodejs/node/pull/39607)
* \[[`efd40eadab`](https://github.com/nodejs/node/commit/efd40eadab)] - **(SEMVER-MAJOR)** **stream**: forward errored to callback (Robert Nagy) [#39364](https://github.com/nodejs/node/pull/39364)
* \[[`09d8c0c8d2`](https://github.com/nodejs/node/commit/09d8c0c8d2)] - **(SEMVER-MAJOR)** **stream**: destroy readable on read error (Robert Nagy) [#39342](https://github.com/nodejs/node/pull/39342)
* \[[`a5dec3a470`](https://github.com/nodejs/node/commit/a5dec3a470)] - **(SEMVER-MAJOR)** **stream**: validate abort signal (Robert Nagy) [#39346](https://github.com/nodejs/node/pull/39346)
* \[[`bb275ef2a4`](https://github.com/nodejs/node/commit/bb275ef2a4)] - **(SEMVER-MAJOR)** **stream**: unify stream utils (Robert Nagy) [#39294](https://github.com/nodejs/node/pull/39294)
* \[[`b2ae12d422`](https://github.com/nodejs/node/commit/b2ae12d422)] - **(SEMVER-MAJOR)** **stream**: throw on premature close in Readable\[AsyncIterator] (Darshan Sen) [#39117](https://github.com/nodejs/node/pull/39117)
* \[[`0738a2b7bd`](https://github.com/nodejs/node/commit/0738a2b7bd)] - **(SEMVER-MAJOR)** **stream**: finished should error on errored stream (Robert Nagy) [#39235](https://github.com/nodejs/node/pull/39235)
* \[[`954217adda`](https://github.com/nodejs/node/commit/954217adda)] - **(SEMVER-MAJOR)** **stream**: error Duplex write/read if not writable/readable (Robert Nagy) [#34385](https://github.com/nodejs/node/pull/34385)
* \[[`f4609bdf3f`](https://github.com/nodejs/node/commit/f4609bdf3f)] - **(SEMVER-MAJOR)** **stream**: bypass legacy destroy for pipeline and async iteration (Robert Nagy) [#38505](https://github.com/nodejs/node/pull/38505)
* \[[`e1e669b109`](https://github.com/nodejs/node/commit/e1e669b109)] - **(SEMVER-MAJOR)** **url**: throw invalid this on detached accessors (James M Snell) [#39752](https://github.com/nodejs/node/pull/39752)
* \[[`70157b9cb7`](https://github.com/nodejs/node/commit/70157b9cb7)] - **(SEMVER-MAJOR)** **url**: forbid certain confusable changes from being introduced by toASCII (Timothy Gu) [#38631](https://github.com/nodejs/node/pull/38631)

### Semver-Minor Commits

* \[[`6cd12be347`](https://github.com/nodejs/node/commit/6cd12be347)] - **(SEMVER-MINOR)** **fs**: add FileHandle.prototype.readableWebStream() (James M Snell) [#39331](https://github.com/nodejs/node/pull/39331)
* \[[`341312d78a`](https://github.com/nodejs/node/commit/341312d78a)] - **(SEMVER-MINOR)** **readline**: add `autoCommit` option (Antoine du Hamel) [#37947](https://github.com/nodejs/node/pull/37947)
* \[[`1d2f37d970`](https://github.com/nodejs/node/commit/1d2f37d970)] - **(SEMVER-MINOR)** **src**: add --openssl-legacy-provider option (Daniel Bevenius) [#40478](https://github.com/nodejs/node/pull/40478)
* \[[`3b72788afb`](https://github.com/nodejs/node/commit/3b72788afb)] - **(SEMVER-MINOR)** **src**: add flags for controlling process behavior (Cheng Zhao) [#40339](https://github.com/nodejs/node/pull/40339)
* \[[`8306051001`](https://github.com/nodejs/node/commit/8306051001)] - **(SEMVER-MINOR)** **stream**: add readableDidRead (Robert Nagy) [#36820](https://github.com/nodejs/node/pull/36820)
* \[[`08ffbd115e`](https://github.com/nodejs/node/commit/08ffbd115e)] - **(SEMVER-MINOR)** **vm**: add support for import assertions in dynamic imports (Antoine du Hamel) [#40249](https://github.com/nodejs/node/pull/40249)

### Semver-Patch Commits

* \[[`ed01811e71`](https://github.com/nodejs/node/commit/ed01811e71)] - **benchmark**: increase crypto DSA keygen params (Brian White) [#40416](https://github.com/nodejs/node/pull/40416)
* \[[`cb93fdbba5`](https://github.com/nodejs/node/commit/cb93fdbba5)] - **build**: reset embedder string to "-node.0" (Michaël Zasso) [#40178](https://github.com/nodejs/node/pull/40178)
* \[[`ed76b49834`](https://github.com/nodejs/node/commit/ed76b49834)] - **build**: fix actions pull request's branch (Mestery) [#40494](https://github.com/nodejs/node/pull/40494)
* \[[`6baea14506`](https://github.com/nodejs/node/commit/6baea14506)] - **build**: avoid run find inactive authors on forked repo (Jiawen Geng) [#40465](https://github.com/nodejs/node/pull/40465)
* \[[`f9996d5b80`](https://github.com/nodejs/node/commit/f9996d5b80)] - **build**: include new public V8 headers in distribution (Michaël Zasso) [#40423](https://github.com/nodejs/node/pull/40423)
* \[[`983b757f3f`](https://github.com/nodejs/node/commit/983b757f3f)] - **build**: update codeowners-validator to 0.6 (FrankQiu) [#40307](https://github.com/nodejs/node/pull/40307)
* \[[`73c3885e10`](https://github.com/nodejs/node/commit/73c3885e10)] - **build**: remove duplicate check for authors.yml (Rich Trott) [#40393](https://github.com/nodejs/node/pull/40393)
* \[[`92090d3435`](https://github.com/nodejs/node/commit/92090d3435)] - **build**: make scripts in gyp run with right python (Cheng Zhao) [#39730](https://github.com/nodejs/node/pull/39730)
* \[[`28f711b552`](https://github.com/nodejs/node/commit/28f711b552)] - **crypto**: remove incorrect constructor invocation (gc) [#40300](https://github.com/nodejs/node/pull/40300)
* \[[`228e703ded`](https://github.com/nodejs/node/commit/228e703ded)] - **deps**: workaround debug link error on Windows (Richard Lau) [#38807](https://github.com/nodejs/node/pull/38807)
* \[[`a35b7e0427`](https://github.com/nodejs/node/commit/a35b7e0427)] - **deps**: upgrade npm to 8.1.0 (npm team) [#40463](https://github.com/nodejs/node/pull/40463)
* \[[`d434c5382a`](https://github.com/nodejs/node/commit/d434c5382a)] - **deps**: regenerate OpenSSL arch files (Daniel Bevenius) [#40478](https://github.com/nodejs/node/pull/40478)
* \[[`2cebd5f02b`](https://github.com/nodejs/node/commit/2cebd5f02b)] - **deps**: add missing legacyprov.c source (Daniel Bevenius) [#40478](https://github.com/nodejs/node/pull/40478)
* \[[`bf82dcd5ba`](https://github.com/nodejs/node/commit/bf82dcd5ba)] - **deps**: patch V8 to 9.5.172.21 (Michaël Zasso) [#40432](https://github.com/nodejs/node/pull/40432)
* \[[`795108a63d`](https://github.com/nodejs/node/commit/795108a63d)] - **deps**: V8: make V8 9.5 ABI-compatible with 9.6 (Michaël Zasso) [#40422](https://github.com/nodejs/node/pull/40422)
* \[[`5d7bd8616e`](https://github.com/nodejs/node/commit/5d7bd8616e)] - **deps**: suppress zlib compiler warnings (Daniel Bevenius) [#40343](https://github.com/nodejs/node/pull/40343)
* \[[`fe84cd453d`](https://github.com/nodejs/node/commit/fe84cd453d)] - **deps**: upgrade Corepack to 0.10 (Maël Nison) [#40374](https://github.com/nodejs/node/pull/40374)
* \[[`2d503ed3ff`](https://github.com/nodejs/node/commit/2d503ed3ff)] - **deps**: V8: backport 239898ef8c77 (Felix Yan) [#39827](https://github.com/nodejs/node/pull/39827)
* \[[`c9296b190f`](https://github.com/nodejs/node/commit/c9296b190f)] - **deps**: V8: cherry-pick 2a0bc36dec12 (Michaël Zasso) [#40178](https://github.com/nodejs/node/pull/40178)
* \[[`5b358370ad`](https://github.com/nodejs/node/commit/5b358370ad)] - **deps**: V8: cherry-pick cf21eb36b975 (Michaël Zasso) [#40178](https://github.com/nodejs/node/pull/40178)
* \[[`228e703ded`](https://github.com/nodejs/node/commit/228e703ded)] - **deps**: workaround debug link error on Windows (Richard Lau) [#38807](https://github.com/nodejs/node/pull/38807)
* \[[`cca9b95523`](https://github.com/nodejs/node/commit/cca9b95523)] - **dgram**: add `nread` assertion to `UDPWrap::OnRecv` (Darshan Sen) [#40295](https://github.com/nodejs/node/pull/40295)
* \[[`7c77db0243`](https://github.com/nodejs/node/commit/7c77db0243)] - **dns**: refactor and use validators (Voltrex) [#40022](https://github.com/nodejs/node/pull/40022)
* \[[`a278117f28`](https://github.com/nodejs/node/commit/a278117f28)] - **doc**: update Collaborator guide to reflect GitHub web UI update (Antoine du Hamel) [#40456](https://github.com/nodejs/node/pull/40456)
* \[[`4cf5563147`](https://github.com/nodejs/node/commit/4cf5563147)] - **doc**: indicate n-api out params that may be NULL (Isaac Brodsky) [#40371](https://github.com/nodejs/node/pull/40371)
* \[[`15ce81a464`](https://github.com/nodejs/node/commit/15ce81a464)] - **doc**: remove ESLint comments which were breaking the CJS/ESM toggles (Mark Skelton) [#40408](https://github.com/nodejs/node/pull/40408)
* \[[`54a85d6bb5`](https://github.com/nodejs/node/commit/54a85d6bb5)] - **doc**: add pronouns for tniessen to README (Tobias Nießen) [#40412](https://github.com/nodejs/node/pull/40412)
* \[[`40db88b7b5`](https://github.com/nodejs/node/commit/40db88b7b5)] - **doc**: format changelogs (Rich Trott) [#40388](https://github.com/nodejs/node/pull/40388)
* \[[`4f68839910`](https://github.com/nodejs/node/commit/4f68839910)] - **doc**: fix missing variable in deepStrictEqual example (OliverOdo) [#40396](https://github.com/nodejs/node/pull/40396)
* \[[`ca6adcf37e`](https://github.com/nodejs/node/commit/ca6adcf37e)] - **doc**: fix asyncLocalStorage.run() description (Constantine Kim) [#40381](https://github.com/nodejs/node/pull/40381)
* \[[`7dd3adf6dd`](https://github.com/nodejs/node/commit/7dd3adf6dd)] - **doc**: fix typos in n-api docs (Ignacio Carbajo) [#40402](https://github.com/nodejs/node/pull/40402)
* \[[`eb65871ab4`](https://github.com/nodejs/node/commit/eb65871ab4)] - **doc**: format doc/guides using format-md task (Rich Trott) [#40358](https://github.com/nodejs/node/pull/40358)
* \[[`0d50dfdf61`](https://github.com/nodejs/node/commit/0d50dfdf61)] - **doc**: improve phrasing in fs.md (Arslan Ali) [#40255](https://github.com/nodejs/node/pull/40255)
* \[[`7723148758`](https://github.com/nodejs/node/commit/7723148758)] - **doc**: add link to core promises tracking issue (Michael Dawson) [#40355](https://github.com/nodejs/node/pull/40355)
* \[[`ccee352630`](https://github.com/nodejs/node/commit/ccee352630)] - **doc**: esm resolver spec refactoring for deprecations (Guy Bedford) [#40314](https://github.com/nodejs/node/pull/40314)
* \[[`1fc1b0f5f2`](https://github.com/nodejs/node/commit/1fc1b0f5f2)] - **doc**: claim ABI version for Electron v17 (Milan Burda) [#40320](https://github.com/nodejs/node/pull/40320)
* \[[`0d2b6aca60`](https://github.com/nodejs/node/commit/0d2b6aca60)] - **doc**: assign missing deprecation number (Michaël Zasso) [#40324](https://github.com/nodejs/node/pull/40324)
* \[[`4bd8e0efa0`](https://github.com/nodejs/node/commit/4bd8e0efa0)] - **doc**: fix typo in ESM example (Tobias Nießen) [#40275](https://github.com/nodejs/node/pull/40275)
* \[[`03d25fe816`](https://github.com/nodejs/node/commit/03d25fe816)] - **doc**: fix typo in esm.md (Mason Malone) [#40273](https://github.com/nodejs/node/pull/40273)
* \[[`6199441b00`](https://github.com/nodejs/node/commit/6199441b00)] - **doc**: correct ESM load hook table header (Jacob) [#40234](https://github.com/nodejs/node/pull/40234)
* \[[`78962d1ca1`](https://github.com/nodejs/node/commit/78962d1ca1)] - **doc**: mark readline promise implementation as experimental (Antoine du Hamel) [#40211](https://github.com/nodejs/node/pull/40211)
* \[[`4b030d0573`](https://github.com/nodejs/node/commit/4b030d0573)] - **doc**: deprecate (doc-only) http abort related (dr-js) [#36670](https://github.com/nodejs/node/pull/36670)
* \[[`bbd4c6eee9`](https://github.com/nodejs/node/commit/bbd4c6eee9)] - **doc**: claim ABI version for Electron v15 and v16 (Samuel Attard) [#39950](https://github.com/nodejs/node/pull/39950)
* \[[`3e774a0500`](https://github.com/nodejs/node/commit/3e774a0500)] - **doc**: fix history for `fs.WriteStream` `open` event (Antoine du Hamel) [#39972](https://github.com/nodejs/node/pull/39972)
* \[[`6fdd5827f0`](https://github.com/nodejs/node/commit/6fdd5827f0)] - **doc**: anchor link parity between markdown and html-generated docs (foxxyz) [#39304](https://github.com/nodejs/node/pull/39304)
* \[[`7b7a0331f4`](https://github.com/nodejs/node/commit/7b7a0331f4)] - **doc**: reset added: version to REPLACEME (Luigi Pinca) [#39901](https://github.com/nodejs/node/pull/39901)
* \[[`58257b7c61`](https://github.com/nodejs/node/commit/58257b7c61)] - **doc**: fix typo in webstreams.md (Luigi Pinca) [#39898](https://github.com/nodejs/node/pull/39898)
* \[[`df22736d80`](https://github.com/nodejs/node/commit/df22736d80)] - **esm**: consolidate ESM loader hooks (Jacob) [#37468](https://github.com/nodejs/node/pull/37468)
* \[[`ac4f5e2437`](https://github.com/nodejs/node/commit/ac4f5e2437)] - **lib**: refactor to use let (gdccwxx) [#40364](https://github.com/nodejs/node/pull/40364)
* \[[`3d11bafaa0`](https://github.com/nodejs/node/commit/3d11bafaa0)] - **lib**: make structuredClone spec compliant (voltrexmaster) [#40251](https://github.com/nodejs/node/pull/40251)
* \[[`48655e17e1`](https://github.com/nodejs/node/commit/48655e17e1)] - **lib,url**: correct URL's argument to pass idlharness (Khaidi Chu) [#39848](https://github.com/nodejs/node/pull/39848)
* \[[`c0a70203de`](https://github.com/nodejs/node/commit/c0a70203de)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#40485](https://github.com/nodejs/node/pull/40485)
* \[[`cbc7b5d424`](https://github.com/nodejs/node/commit/cbc7b5d424)] - **meta**: consolidate AUTHORS entries for emanuelbuholzer (Rich Trott) [#40469](https://github.com/nodejs/node/pull/40469)
* \[[`881174e016`](https://github.com/nodejs/node/commit/881174e016)] - **meta**: consolidate AUTHORS entries for ebickle (Rich Trott) [#40447](https://github.com/nodejs/node/pull/40447)
* \[[`b80b85e130`](https://github.com/nodejs/node/commit/b80b85e130)] - **meta**: add `typings` to label-pr-config (Mestery) [#40401](https://github.com/nodejs/node/pull/40401)
* \[[`95cf944736`](https://github.com/nodejs/node/commit/95cf944736)] - **meta**: consolidate AUTHORS entries for evantorrie (Rich Trott) [#40430](https://github.com/nodejs/node/pull/40430)
* \[[`c350c217f4`](https://github.com/nodejs/node/commit/c350c217f4)] - **meta**: consolidate AUTHORS entries for gabrielschulhof (Rich Trott) [#40420](https://github.com/nodejs/node/pull/40420)
* \[[`a9411891cf`](https://github.com/nodejs/node/commit/a9411891cf)] - **meta**: consolidate AUTHORS information for geirha (Rich Trott) [#40406](https://github.com/nodejs/node/pull/40406)
* \[[`0cc37209fa`](https://github.com/nodejs/node/commit/0cc37209fa)] - **meta**: consolidate duplicate AUTHORS entries for hassaanp (Rich Trott) [#40391](https://github.com/nodejs/node/pull/40391)
* \[[`49b7ec96a4`](https://github.com/nodejs/node/commit/49b7ec96a4)] - **meta**: update AUTHORS (Node.js GitHub Bot) [#40392](https://github.com/nodejs/node/pull/40392)
* \[[`a3c0713d9e`](https://github.com/nodejs/node/commit/a3c0713d9e)] - **meta**: consolidate AUTHORS entry for thw0rted (Rich Trott) [#40387](https://github.com/nodejs/node/pull/40387)
* \[[`eaa59571e0`](https://github.com/nodejs/node/commit/eaa59571e0)] - **meta**: update label-pr-config (Mestery) [#40199](https://github.com/nodejs/node/pull/40199)
* \[[`6a205d7a56`](https://github.com/nodejs/node/commit/6a205d7a56)] - **meta**: use .mailmap to consolidate AUTHORS entries for ide (Rich Trott) [#40367](https://github.com/nodejs/node/pull/40367)
* \[[`f570109094`](https://github.com/nodejs/node/commit/f570109094)] - **net**: check if option is undefined (Daijiro Wachi) [#40344](https://github.com/nodejs/node/pull/40344)
* \[[`119558b6a2`](https://github.com/nodejs/node/commit/119558b6a2)] - **net**: remove unused ObjectKeys (Daijiro Wachi) [#40344](https://github.com/nodejs/node/pull/40344)
* \[[`c7cd8ef6c6`](https://github.com/nodejs/node/commit/c7cd8ef6c6)] - **net**: check objectMode first and then readble || writable (Daijiro Wachi) [#40344](https://github.com/nodejs/node/pull/40344)
* \[[`46446623f5`](https://github.com/nodejs/node/commit/46446623f5)] - **net**: throw error to object mode in Socket (Daijiro Wachi) [#40344](https://github.com/nodejs/node/pull/40344)
* \[[`38aa7cc7c7`](https://github.com/nodejs/node/commit/38aa7cc7c7)] - **src**: get embedder options on-demand (Joyee Cheung) [#40357](https://github.com/nodejs/node/pull/40357)
* \[[`ad4e70c817`](https://github.com/nodejs/node/commit/ad4e70c817)] - **src**: ensure V8 initialized before marking milestone (Shelley Vohr) [#40405](https://github.com/nodejs/node/pull/40405)
* \[[`a784258444`](https://github.com/nodejs/node/commit/a784258444)] - **src**: remove usage of `AllocatedBuffer` from `stream_*` (Darshan Sen) [#40293](https://github.com/nodejs/node/pull/40293)
* \[[`f11493dfc9`](https://github.com/nodejs/node/commit/f11493dfc9)] - **src**: add missing initialization (Michael Dawson) [#40370](https://github.com/nodejs/node/pull/40370)
* \[[`5e248eceb6`](https://github.com/nodejs/node/commit/5e248eceb6)] - **src**: update NODE\_MODULE\_VERSION to 102 (Michaël Zasso) [#40178](https://github.com/nodejs/node/pull/40178)
* \[[`3f0b62375b`](https://github.com/nodejs/node/commit/3f0b62375b)] - **stream**: convert premature close to AbortError (Robert Nagy) [#39524](https://github.com/nodejs/node/pull/39524)
* \[[`79f4d5a345`](https://github.com/nodejs/node/commit/79f4d5a345)] - **stream**: fix toWeb typo (Robert Nagy) [#39496](https://github.com/nodejs/node/pull/39496)
* \[[`44ee6c2623`](https://github.com/nodejs/node/commit/44ee6c2623)] - **stream**: call done() in consistent fashion (Rich Trott) [#39475](https://github.com/nodejs/node/pull/39475)
* \[[`09ad64d66d`](https://github.com/nodejs/node/commit/09ad64d66d)] - **stream**: add CompressionStream and DecompressionStream (James M Snell) [#39348](https://github.com/nodejs/node/pull/39348)
* \[[`a99c230305`](https://github.com/nodejs/node/commit/a99c230305)] - **stream**: implement streams to webstreams adapters (James M Snell) [#39134](https://github.com/nodejs/node/pull/39134)
* \[[`a5ba28dda2`](https://github.com/nodejs/node/commit/a5ba28dda2)] - **stream**: fix performance regression (Brian White) [#39254](https://github.com/nodejs/node/pull/39254)
* \[[`ce00381751`](https://github.com/nodejs/node/commit/ce00381751)] - **stream**: use finished for async iteration (Robert Nagy) [#39282](https://github.com/nodejs/node/pull/39282)
* \[[`e0faf8c3e9`](https://github.com/nodejs/node/commit/e0faf8c3e9)] - **test**: replace common port with specific number (Daijiro Wachi) [#40344](https://github.com/nodejs/node/pull/40344)
* \[[`8068f40313`](https://github.com/nodejs/node/commit/8068f40313)] - **test**: fix typos in whatwg-webstreams explanations (Tobias Nießen) [#40389](https://github.com/nodejs/node/pull/40389)
* \[[`eafdeab97b`](https://github.com/nodejs/node/commit/eafdeab97b)] - **test**: add test for readStream.path when fd is specified (Qingyu Deng) [#40359](https://github.com/nodejs/node/pull/40359)
* \[[`24f045dae2`](https://github.com/nodejs/node/commit/24f045dae2)] - **test**: replace .then chains with await (gdccwxx) [#40348](https://github.com/nodejs/node/pull/40348)
* \[[`5b4ba52786`](https://github.com/nodejs/node/commit/5b4ba52786)] - **test**: fix "test/common/debugger" identify async function (gdccwxx) [#40348](https://github.com/nodejs/node/pull/40348)
* \[[`1d84e916d6`](https://github.com/nodejs/node/commit/1d84e916d6)] - **test**: improve test coverage of `fs.ReadStream` with `FileHandle` (Antoine du Hamel) [#40018](https://github.com/nodejs/node/pull/40018)
* \[[`b63e449b2e`](https://github.com/nodejs/node/commit/b63e449b2e)] - **test**: pass URL's toascii.window\.js WPT (Khaidi Chu) [#39910](https://github.com/nodejs/node/pull/39910)
* \[[`842fd234b7`](https://github.com/nodejs/node/commit/842fd234b7)] - **test**: adapt test-repl to V8 9.5 (Michaël Zasso) [#40178](https://github.com/nodejs/node/pull/40178)
* \[[`d7b9b9f8d7`](https://github.com/nodejs/node/commit/d7b9b9f8d7)] - **test**: remove test-v8-untrusted-code-mitigations (Ross McIlroy) [#40178](https://github.com/nodejs/node/pull/40178)
* \[[`7624917069`](https://github.com/nodejs/node/commit/7624917069)] - **tools**: update tools/lint-md dependencies to support GFM footnotes (Rich Trott) [#40445](https://github.com/nodejs/node/pull/40445)
* \[[`350a95b89f`](https://github.com/nodejs/node/commit/350a95b89f)] - **tools**: update lint-md dependencies (Rich Trott) [#40404](https://github.com/nodejs/node/pull/40404)
* \[[`012152d7d6`](https://github.com/nodejs/node/commit/012152d7d6)] - **tools**: udpate @babel/eslint-parser (Rich Trott) [#40394](https://github.com/nodejs/node/pull/40394)
* \[[`43c780e741`](https://github.com/nodejs/node/commit/43c780e741)] - **tools**: remove @babel/plugin-syntax-import-assertions (Rich Trott) [#40394](https://github.com/nodejs/node/pull/40394)
* \[[`b39db95737`](https://github.com/nodejs/node/commit/b39db95737)] - **tools**: remove @bable/plugin-syntax-class-properties (Rich Trott) [#40394](https://github.com/nodejs/node/pull/40394)
* \[[`a6fd39f44f`](https://github.com/nodejs/node/commit/a6fd39f44f)] - **tools**: remove @babel/plugin-syntax-top-level-await (Rich Trott) [#40394](https://github.com/nodejs/node/pull/40394)
* \[[`8ca76eba73`](https://github.com/nodejs/node/commit/8ca76eba73)] - **tools**: update ESLint to 8.0.0 (Rich Trott) [#40394](https://github.com/nodejs/node/pull/40394)
* \[[`dd8e219d71`](https://github.com/nodejs/node/commit/dd8e219d71)] - **tools**: prepare ESLint rules for 8.0.0 requirements (Rich Trott) [#40394](https://github.com/nodejs/node/pull/40394)
* \[[`0a1b399781`](https://github.com/nodejs/node/commit/0a1b399781)] - **tools**: fix ESLint update scripts (Rich Trott) [#40394](https://github.com/nodejs/node/pull/40394)
* \[[`d6d6b050ff`](https://github.com/nodejs/node/commit/d6d6b050ff)] - **tools**: warn about duplicates when generating AUTHORS file (Rich Trott) [#40304](https://github.com/nodejs/node/pull/40304)
* \[[`1fd984581c`](https://github.com/nodejs/node/commit/1fd984581c)] - **tools**: update V8 gypfiles for 9.5 (Michaël Zasso) [#40178](https://github.com/nodejs/node/pull/40178)
* \[[`a8a86387fa`](https://github.com/nodejs/node/commit/a8a86387fa)] - **tty**: enable buffering (Robert Nagy) [#39253](https://github.com/nodejs/node/pull/39253)
* \[[`9467cbadcb`](https://github.com/nodejs/node/commit/9467cbadcb)] - **typings**: define types for os binding (Michaël Zasso) [#40222](https://github.com/nodejs/node/pull/40222)
* \[[`70a5b86049`](https://github.com/nodejs/node/commit/70a5b86049)] - **typings**: add missing types to options and util bindings (Michaël Zasso) [#40222](https://github.com/nodejs/node/pull/40222)
* \[[`3815a21beb`](https://github.com/nodejs/node/commit/3815a21beb)] - **typings**: define types for timers binding (Michaël Zasso) [#40222](https://github.com/nodejs/node/pull/40222)
* \[[`9e64336fbf`](https://github.com/nodejs/node/commit/9e64336fbf)] - **typings**: fix declaration of primordials (Michaël Zasso) [#40222](https://github.com/nodejs/node/pull/40222)
* \[[`f581f6da94`](https://github.com/nodejs/node/commit/f581f6da94)] - **url**: fix performance regression (Brian White) [#39778](https://github.com/nodejs/node/pull/39778)
* \[[`02de40246f`](https://github.com/nodejs/node/commit/02de40246f)] - **v8**: remove --harmony-top-level-await (Geoffrey Booth) [#40226](https://github.com/nodejs/node/pull/40226)
