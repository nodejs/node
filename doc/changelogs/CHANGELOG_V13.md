# Node.js 13 ChangeLog

<!--lint disable prohibited-strings-->
<!--lint disable maximum-line-length-->

<table>
<tr>
<th>Current</th>
</tr>
<tr>
<td>
<a href="#13.3.0">13.3.0</a><br/>
<a href="#13.2.0">13.2.0</a><br/>
<a href="#13.1.0">13.1.0</a><br/>
<a href="#13.0.1">13.0.1</a><br/>
<a href="#13.0.0">13.0.0</a><br/>
</td>
</tr>
</table>

* Other Versions
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

<a id="13.3.0"></a>
## 2019-12-03, Version 13.3.0 (Current), @BridgeAR

### Notable Changes

* **fs**:
  * Reworked experimental recursive `rmdir()`  (cjihrig) [#30644](https://github.com/nodejs/node/pull/30644)
    * The `maxBusyTries` option is renamed to `maxRetries`, and its default is
      set to 0. The `emfileWait` option has been removed, and `EMFILE` errors
      use the same retry logic as other errors. The `retryDelay` option is now
      supported. `ENFILE` errors are now retried.
* **http**:
  * Make maximum header size configurable per-stream or per-server (Anna Henningsen) [#30570](https://github.com/nodejs/node/pull/30570)
* **http2**:
  * Make maximum tolerated rejected streams configurable (Denys Otrishko) [#30534](https://github.com/nodejs/node/pull/30534)
  * Allow to configure maximum tolerated invalid frames (Denys Otrishko) [#30534](https://github.com/nodejs/node/pull/30534)
* **wasi**:
  * Introduce initial WASI support (cjihrig) [#30258](https://github.com/nodejs/node/pull/30258)

### Commits

* [[`4cd4e7c17a`](https://github.com/nodejs/node/commit/4cd4e7c17a)] - **benchmark,doc,lib,test**: prepare for padding lint rule (Rich Trott) [#30696](https://github.com/nodejs/node/pull/30696)
* [[`63eb4fee46`](https://github.com/nodejs/node/commit/63eb4fee46)] - **buffer**: fix 6-byte writeUIntBE() range check (Brian White) [#30459](https://github.com/nodejs/node/pull/30459)
* [[`e8af569200`](https://github.com/nodejs/node/commit/e8af569200)] - **buffer**: release buffers with free callbacks on env exit (Anna Henningsen) [#30551](https://github.com/nodejs/node/pull/30551)
* [[`648766bccf`](https://github.com/nodejs/node/commit/648766bccf)] - **build**: do not build mksnapshot and mkcodecache for --shared (Joyee Cheung) [#30647](https://github.com/nodejs/node/pull/30647)
* [[`6545314a4f`](https://github.com/nodejs/node/commit/6545314a4f)] - **build**: add --without-node-code-cache configure option (Joyee Cheung) [#30647](https://github.com/nodejs/node/pull/30647)
* [[`80ada94cd3`](https://github.com/nodejs/node/commit/80ada94cd3)] - **build**: use Node.js instead of Node in configure (Tobias Nießen) [#30642](https://github.com/nodejs/node/pull/30642)
* [[`0aae502c67`](https://github.com/nodejs/node/commit/0aae502c67)] - **build,win**: propagate error codes in vcbuild (João Reis) [#30724](https://github.com/nodejs/node/pull/30724)
* [[`6a53152b42`](https://github.com/nodejs/node/commit/6a53152b42)] - **build,win**: add test-ci-native and test-ci-js (João Reis) [#30724](https://github.com/nodejs/node/pull/30724)
* [[`30a4f68a15`](https://github.com/nodejs/node/commit/30a4f68a15)] - **child_process**: document kill() return value (cjihrig) [#30669](https://github.com/nodejs/node/pull/30669)
* [[`dae36a9692`](https://github.com/nodejs/node/commit/dae36a9692)] - **child_process**: replace var with let/const (dnlup) [#30389](https://github.com/nodejs/node/pull/30389)
* [[`4b13bca31a`](https://github.com/nodejs/node/commit/4b13bca31a)] - **child_process**: replace var with const/let in internal/child\_process.js (Luis Camargo) [#30414](https://github.com/nodejs/node/pull/30414)
* [[`378c54fe97`](https://github.com/nodejs/node/commit/378c54fe97)] - **cluster**: replace vars in child.js (EmaSuriano) [#30383](https://github.com/nodejs/node/pull/30383)
* [[`708e67a732`](https://github.com/nodejs/node/commit/708e67a732)] - **cluster**: replace var with let (Herrmann, Rene R. (656)) [#30425](https://github.com/nodejs/node/pull/30425)
* [[`55fbe45f69`](https://github.com/nodejs/node/commit/55fbe45f69)] - **cluster**: replace var by let in shared\_handle.js (poutch) [#30402](https://github.com/nodejs/node/pull/30402)
* [[`4affc30a12`](https://github.com/nodejs/node/commit/4affc30a12)] - **crypto**: automatically manage memory for ECDSA\_SIG (Tobias Nießen) [#30641](https://github.com/nodejs/node/pull/30641)
* [[`55c2ac70b7`](https://github.com/nodejs/node/commit/55c2ac70b7)] - **crypto**: remove redundant validateUint32 argument (Tobias Nießen) [#30579](https://github.com/nodejs/node/pull/30579)
* [[`0ba877a541`](https://github.com/nodejs/node/commit/0ba877a541)] - **deps**: V8: cherry-pick 0dfd9ea51241 (bcoe) [#30713](https://github.com/nodejs/node/pull/30713)
* [[`b470354057`](https://github.com/nodejs/node/commit/b470354057)] - **deps**: patch V8 to 7.9.317.25 (Myles Borins) [#30679](https://github.com/nodejs/node/pull/30679)
* [[`d257448bca`](https://github.com/nodejs/node/commit/d257448bca)] - **deps**: update llhttp to 2.0.1 (Fedor Indutny) [#30553](https://github.com/nodejs/node/pull/30553)
* [[`456d250d2d`](https://github.com/nodejs/node/commit/456d250d2d)] - **deps**: V8: backport 93f189f19a03 (Michaël Zasso) [#30681](https://github.com/nodejs/node/pull/30681)
* [[`aa01ebdbca`](https://github.com/nodejs/node/commit/aa01ebdbca)] - **deps**: V8: cherry-pick ca5b0ec (Anna Henningsen) [#30708](https://github.com/nodejs/node/pull/30708)
* [[`f37450f580`](https://github.com/nodejs/node/commit/f37450f580)] - **dns**: use length for building TXT string (Anna Henningsen) [#30690](https://github.com/nodejs/node/pull/30690)
* [[`3d302ff276`](https://github.com/nodejs/node/commit/3d302ff276)] - **doc**: fix typographical error (Rich Trott) [#30735](https://github.com/nodejs/node/pull/30735)
* [[`19b31c1bc5`](https://github.com/nodejs/node/commit/19b31c1bc5)] - **doc**: revise REPL uncaught exception text (Rich Trott) [#30729](https://github.com/nodejs/node/pull/30729)
* [[`61af1fcaa1`](https://github.com/nodejs/node/commit/61af1fcaa1)] - **doc**: update signature algorithm in release doc (Myles Borins) [#30673](https://github.com/nodejs/node/pull/30673)
* [[`a8002d92ab`](https://github.com/nodejs/node/commit/a8002d92ab)] - **doc**: update README.md to fix active/maint times (Michael Dawson) [#30707](https://github.com/nodejs/node/pull/30707)
* [[`f46df0b496`](https://github.com/nodejs/node/commit/f46df0b496)] - **doc**: update socket.bufferSize text (Rich Trott) [#30723](https://github.com/nodejs/node/pull/30723)
* [[`cbd50262c0`](https://github.com/nodejs/node/commit/cbd50262c0)] - **doc**: note that buf.buffer's contents might differ (AJ Jordan) [#29651](https://github.com/nodejs/node/pull/29651)
* [[`a25626c1ed`](https://github.com/nodejs/node/commit/a25626c1ed)] - **doc**: clarify IncomingMessage.destroy() description (Sam Foxman) [#30255](https://github.com/nodejs/node/pull/30255)
* [[`8fcb450934`](https://github.com/nodejs/node/commit/8fcb450934)] - **doc**: fixed a typo in process.md (Harendra Singh) [#30277](https://github.com/nodejs/node/pull/30277)
* [[`ad9f737e44`](https://github.com/nodejs/node/commit/ad9f737e44)] - **doc**: documenting a bit more FreeBSD case (David Carlier) [#30325](https://github.com/nodejs/node/pull/30325)
* [[`40b762177f`](https://github.com/nodejs/node/commit/40b762177f)] - **doc**: add missing 'added' versions to module.builtinModules (Thomas Watson) [#30562](https://github.com/nodejs/node/pull/30562)
* [[`aca0119089`](https://github.com/nodejs/node/commit/aca0119089)] - **doc**: fix worker.resourceLimits indentation (Daniel Nalborczyk) [#30663](https://github.com/nodejs/node/pull/30663)
* [[`43e78578a6`](https://github.com/nodejs/node/commit/43e78578a6)] - **doc**: fix worker.resourceLimits type (Daniel Nalborczyk) [#30664](https://github.com/nodejs/node/pull/30664)
* [[`20dbce17d5`](https://github.com/nodejs/node/commit/20dbce17d5)] - **doc**: avoid proposal syntax in code example (Alex Zherdev) [#30685](https://github.com/nodejs/node/pull/30685)
* [[`1e7c567734`](https://github.com/nodejs/node/commit/1e7c567734)] - **doc**: address nits for src/README.md (Anna Henningsen) [#30693](https://github.com/nodejs/node/pull/30693)
* [[`87136c9bde`](https://github.com/nodejs/node/commit/87136c9bde)] - **doc**: revise socket.connect() note (Rich Trott) [#30691](https://github.com/nodejs/node/pull/30691)
* [[`fcde49700c`](https://github.com/nodejs/node/commit/fcde49700c)] - **doc**: remove "this API is unstable" note for v8 serdes API (bruce-one) [#30631](https://github.com/nodejs/node/pull/30631)
* [[`809a2b056b`](https://github.com/nodejs/node/commit/809a2b056b)] - **doc**: fixup incorrect flag name reference (Guy Bedford) [#30651](https://github.com/nodejs/node/pull/30651)
* [[`3d978839c1`](https://github.com/nodejs/node/commit/3d978839c1)] - **doc**: minor updates to releases.md (Beth Griggs) [#30636](https://github.com/nodejs/node/pull/30636)
* [[`e9f031c741`](https://github.com/nodejs/node/commit/e9f031c741)] - **doc**: add 13 and 12 to previous versions (Andrew Hughes) [#30590](https://github.com/nodejs/node/pull/30590)
* [[`8ab18b6b6f`](https://github.com/nodejs/node/commit/8ab18b6b6f)] - **doc**: update AUTHORS list (Gus Caplan) [#30672](https://github.com/nodejs/node/pull/30672)
* [[`329a821d25`](https://github.com/nodejs/node/commit/329a821d25)] - **doc**: add explanation why keep var with for loop (Lucas Recknagel) [#30380](https://github.com/nodejs/node/pull/30380)
* [[`426ca263c8`](https://github.com/nodejs/node/commit/426ca263c8)] - **doc**: document "Resume Build" limitation (Richard Lau) [#30604](https://github.com/nodejs/node/pull/30604)
* [[`00f7cc65a1`](https://github.com/nodejs/node/commit/00f7cc65a1)] - **doc**: add note of caution about non-conforming streams (Robert Nagy) [#29895](https://github.com/nodejs/node/pull/29895)
* [[`7d98a59c39`](https://github.com/nodejs/node/commit/7d98a59c39)] - **doc**: add note about debugging worker\_threads (Denys Otrishko) [#30594](https://github.com/nodejs/node/pull/30594)
* [[`8ef629a78a`](https://github.com/nodejs/node/commit/8ef629a78a)] - **doc**: simplify "is recommended" language in assert documentation (Rich Trott) [#30558](https://github.com/nodejs/node/pull/30558)
* [[`19d192d1f0`](https://github.com/nodejs/node/commit/19d192d1f0)] - **doc**: fix a typo in a date for version 13.2.0 (Kirlat) [#30587](https://github.com/nodejs/node/pull/30587)
* [[`b67759a93c`](https://github.com/nodejs/node/commit/b67759a93c)] - **doc,deps**: document how to maintain ICU in Node.js (Steven R. Loomis) [#30607](https://github.com/nodejs/node/pull/30607)
* [[`bfcc9142f3`](https://github.com/nodejs/node/commit/bfcc9142f3)] - **doc,n-api**: mark napi\_detach\_arraybuffer as experimental (legendecas) [#30703](https://github.com/nodejs/node/pull/30703)
* [[`365f0ab09b`](https://github.com/nodejs/node/commit/365f0ab09b)] - **esm**: data URLs should ignore unknown parameters (Bradley Farias) [#30593](https://github.com/nodejs/node/pull/30593)
* [[`0285aa0967`](https://github.com/nodejs/node/commit/0285aa0967)] - **events**: improve performance caused by primordials (guzhizhou) [#30577](https://github.com/nodejs/node/pull/30577)
* [[`3475f9b82c`](https://github.com/nodejs/node/commit/3475f9b82c)] - **fs**: add ENFILE to rimraf retry logic (cjihrig) [#30644](https://github.com/nodejs/node/pull/30644)
* [[`f725953433`](https://github.com/nodejs/node/commit/f725953433)] - **fs**: add retryDelay option to rimraf (cjihrig) [#30644](https://github.com/nodejs/node/pull/30644)
* [[`51bc379243`](https://github.com/nodejs/node/commit/51bc379243)] - **fs**: remove rimraf's emfileWait option (cjihrig) [#30644](https://github.com/nodejs/node/pull/30644)
* [[`612a3a2e6c`](https://github.com/nodejs/node/commit/612a3a2e6c)] - **fs**: make rimraf default to 0 retries (cjihrig) [#30644](https://github.com/nodejs/node/pull/30644)
* [[`fa1f87b199`](https://github.com/nodejs/node/commit/fa1f87b199)] - **fs**: rename rimraf's maxBusyTries to maxRetries (cjihrig) [#30644](https://github.com/nodejs/node/pull/30644)
* [[`8ee27ffe77`](https://github.com/nodejs/node/commit/8ee27ffe77)] - **fs**: change var to let (Àlvar Pérez) [#30407](https://github.com/nodejs/node/pull/30407)
* [[`850c2a72ea`](https://github.com/nodejs/node/commit/850c2a72ea)] - **fs**: cover fs.opendir ERR\_INVALID\_CALLBACK (Vladislav Botvin) [#30307](https://github.com/nodejs/node/pull/30307)
* [[`62574087ea`](https://github.com/nodejs/node/commit/62574087ea)] - **(SEMVER-MINOR)** **http**: make maximum header size configurable per-stream or per-server (Anna Henningsen) [#30570](https://github.com/nodejs/node/pull/30570)
* [[`1d1d136806`](https://github.com/nodejs/node/commit/1d1d136806)] - **http**: set socket.server unconditionally (Anna Henningsen) [#30571](https://github.com/nodejs/node/pull/30571)
* [[`6848bfbf65`](https://github.com/nodejs/node/commit/6848bfbf65)] - **http**: replace var with let (Guilherme Goncalves) [#30421](https://github.com/nodejs/node/pull/30421)
* [[`8256d38349`](https://github.com/nodejs/node/commit/8256d38349)] - **http**: destructure primordials in lib/\_http\_server.js (Artem Maksimov) [#30315](https://github.com/nodejs/node/pull/30315)
* [[`3b169f1dbd`](https://github.com/nodejs/node/commit/3b169f1dbd)] - **http**: improve performance caused by primordials (Lucas Recknagel) [#30416](https://github.com/nodejs/node/pull/30416)
* [[`6f313f9ab0`](https://github.com/nodejs/node/commit/6f313f9ab0)] - **http2**: fix session memory accounting after pausing (Michael Lehenbauer) [#30684](https://github.com/nodejs/node/pull/30684)
* [[`7d37bcebea`](https://github.com/nodejs/node/commit/7d37bcebea)] - **(SEMVER-MINOR)** **http2**: make maximum tolerated rejected streams configurable (Denys Otrishko) [#30534](https://github.com/nodejs/node/pull/30534)
* [[`092a3c28aa`](https://github.com/nodejs/node/commit/092a3c28aa)] - **(SEMVER-MINOR)** **http2**: allow to configure maximum tolerated invalid frames (Denys Otrishko) [#30534](https://github.com/nodejs/node/pull/30534)
* [[`e92afd998f`](https://github.com/nodejs/node/commit/e92afd998f)] - **(SEMVER-MINOR)** **http2**: replace direct array usage with struct for js\_fields\_ (Denys Otrishko) [#30534](https://github.com/nodejs/node/pull/30534)
* [[`30ef8e4cbd`](https://github.com/nodejs/node/commit/30ef8e4cbd)] - **http2**: change var to let compact.js (Maria Emmanouil) [#30392](https://github.com/nodejs/node/pull/30392)
* [[`1a2ed4a5f4`](https://github.com/nodejs/node/commit/1a2ed4a5f4)] - **http2**: core.js replace var with let (Daniel Schuech) [#30403](https://github.com/nodejs/node/pull/30403)
* [[`f7ca7e6677`](https://github.com/nodejs/node/commit/f7ca7e6677)] - **http2**: replace var with let/const (Paolo Ceschi Berrini) [#30417](https://github.com/nodejs/node/pull/30417)
* [[`6322611077`](https://github.com/nodejs/node/commit/6322611077)] - **inspector**: properly shut down uv\_async\_t (Anna Henningsen) [#30612](https://github.com/nodejs/node/pull/30612)
* [[`de3a1c3019`](https://github.com/nodejs/node/commit/de3a1c3019)] - **lib**: enforce use of primordial Number (Sebastien Ahkrin) [#30700](https://github.com/nodejs/node/pull/30700)
* [[`5a9340d723`](https://github.com/nodejs/node/commit/5a9340d723)] - **lib**: use static Number properties from primordials (Michaël Zasso) [#30686](https://github.com/nodejs/node/pull/30686)
* [[`892bde635e`](https://github.com/nodejs/node/commit/892bde635e)] - **lib**: enforce use of Boolean from primordials (Michaël Zasso) [#30698](https://github.com/nodejs/node/pull/30698)
* [[`ae2c7d0b02`](https://github.com/nodejs/node/commit/ae2c7d0b02)] - **lib**: replace Date.now function by primordial DateNow (Tchoupinax) [#30689](https://github.com/nodejs/node/pull/30689)
* [[`c09e3deac5`](https://github.com/nodejs/node/commit/c09e3deac5)] - **lib**: replace ArrayBuffer.isView by primordial ArrayBuffer (Vincent Dhennin) [#30692](https://github.com/nodejs/node/pull/30692)
* [[`5ef4dceb95`](https://github.com/nodejs/node/commit/5ef4dceb95)] - **lib**: enforce use of Array from primordials (Michaël Zasso) [#30635](https://github.com/nodejs/node/pull/30635)
* [[`a4dfe3b7dc`](https://github.com/nodejs/node/commit/a4dfe3b7dc)] - **lib**: flatten access to primordials (Michaël Zasso) [#30610](https://github.com/nodejs/node/pull/30610)
* [[`b545b91de5`](https://github.com/nodejs/node/commit/b545b91de5)] - **lib**: use let instead of var (Shubham Chaturvedi) [#30375](https://github.com/nodejs/node/pull/30375)
* [[`5120926337`](https://github.com/nodejs/node/commit/5120926337)] - **lib**: replace var with let/const (jens-cappelle) [#30391](https://github.com/nodejs/node/pull/30391)
* [[`b18b056d64`](https://github.com/nodejs/node/commit/b18b056d64)] - **lib**: replace var w/ let (Chris Oyler) [#30386](https://github.com/nodejs/node/pull/30386)
* [[`3796885096`](https://github.com/nodejs/node/commit/3796885096)] - **lib**: replace var with let/const (Tijl Claessens) [#30390](https://github.com/nodejs/node/pull/30390)
* [[`ffe3040659`](https://github.com/nodejs/node/commit/ffe3040659)] - **lib**: adding perf notes js\_stream\_socket.js (ryan jarvinen) [#30415](https://github.com/nodejs/node/pull/30415)
* [[`797b938c49`](https://github.com/nodejs/node/commit/797b938c49)] - **lib**: replace var with let (Dennis Saenger) [#30396](https://github.com/nodejs/node/pull/30396)
* [[`0b64e45e41`](https://github.com/nodejs/node/commit/0b64e45e41)] - **lib**: main\_thread\_only change var to let (matijagaspar) [#30398](https://github.com/nodejs/node/pull/30398)
* [[`d024630f44`](https://github.com/nodejs/node/commit/d024630f44)] - **lib**: change var to let in stream\_base\_commons (Kyriakos Markakis) [#30426](https://github.com/nodejs/node/pull/30426)
* [[`3c041edbe7`](https://github.com/nodejs/node/commit/3c041edbe7)] - **lib**: use let instead of var (Semir Ajruli) [#30424](https://github.com/nodejs/node/pull/30424)
* [[`d277c375fd`](https://github.com/nodejs/node/commit/d277c375fd)] - **lib**: changed var to let (Oliver Belaifa) [#30427](https://github.com/nodejs/node/pull/30427)
* [[`0fd89cc0f1`](https://github.com/nodejs/node/commit/0fd89cc0f1)] - **lib**: replace var with let/const (Dries Stelten) [#30409](https://github.com/nodejs/node/pull/30409)
* [[`bdba03e3ed`](https://github.com/nodejs/node/commit/bdba03e3ed)] - **lib**: change var to let (Dimitris Ktistakis) [#30408](https://github.com/nodejs/node/pull/30408)
* [[`48fef42ca9`](https://github.com/nodejs/node/commit/48fef42ca9)] - **lib**: replace var with let/const (Tembrechts) [#30404](https://github.com/nodejs/node/pull/30404)
* [[`502173b54e`](https://github.com/nodejs/node/commit/502173b54e)] - **lib**: replace var to let in cli\_table.js (Jing Lin) [#30400](https://github.com/nodejs/node/pull/30400)
* [[`2cf8a7f117`](https://github.com/nodejs/node/commit/2cf8a7f117)] - **module**: fix specifier resolution algorithm (Rongjian Zhang) [#30574](https://github.com/nodejs/node/pull/30574)
* [[`be9788bf20`](https://github.com/nodejs/node/commit/be9788bf20)] - **n-api**: detach external ArrayBuffers on env exit (Anna Henningsen) [#30551](https://github.com/nodejs/node/pull/30551)
* [[`8171cef921`](https://github.com/nodejs/node/commit/8171cef921)] - **(SEMVER-MINOR)** **n-api**: implement napi\_is\_detached\_arraybuffer (Denys Otrishko) [#30613](https://github.com/nodejs/node/pull/30613)
* [[`cc5875b2e6`](https://github.com/nodejs/node/commit/cc5875b2e6)] - **n-api**: add missed nullptr check in napi\_has\_own\_property (Denys Otrishko) [#30626](https://github.com/nodejs/node/pull/30626)
* [[`017280e6e2`](https://github.com/nodejs/node/commit/017280e6e2)] - **net**: replaced vars to lets and consts (nathias) [#30401](https://github.com/nodejs/node/pull/30401)
* [[`56248a827a`](https://github.com/nodejs/node/commit/56248a827a)] - **process**: replace var with let/const (Jesper Ek) [#30382](https://github.com/nodejs/node/pull/30382)
* [[`5c40b2f9ac`](https://github.com/nodejs/node/commit/5c40b2f9ac)] - **process**: replace vars in per\_thread.js (EmaSuriano) [#30385](https://github.com/nodejs/node/pull/30385)
* [[`c50bbf58da`](https://github.com/nodejs/node/commit/c50bbf58da)] - **readline**: change var to let (dnlup) [#30435](https://github.com/nodejs/node/pull/30435)
* [[`b91d22cc8d`](https://github.com/nodejs/node/commit/b91d22cc8d)] - **repl**: fix referrer for dynamic import (Corey Farrell) [#30609](https://github.com/nodejs/node/pull/30609)
* [[`4e5818a456`](https://github.com/nodejs/node/commit/4e5818a456)] - **repl**: change var to let (Oliver Belaifa) [#30428](https://github.com/nodejs/node/pull/30428)
* [[`e65ad865c6`](https://github.com/nodejs/node/commit/e65ad865c6)] - **src**: change header file in node\_stat\_watcher.cc (Reza Fatahi) [#29976](https://github.com/nodejs/node/pull/29976)
* [[`be84ceefb8`](https://github.com/nodejs/node/commit/be84ceefb8)] - **src**: clean up node\_file.h (Anna Henningsen) [#30530](https://github.com/nodejs/node/pull/30530)
* [[`bccfd124b0`](https://github.com/nodejs/node/commit/bccfd124b0)] - **src**: remove unused variable in node\_dir.cc (gengjiawen) [#30267](https://github.com/nodejs/node/pull/30267)
* [[`fc11db18fe`](https://github.com/nodejs/node/commit/fc11db18fe)] - **src**: inline SetSNICallback (Anna Henningsen) [#30548](https://github.com/nodejs/node/pull/30548)
* [[`7bd587ef0c`](https://github.com/nodejs/node/commit/7bd587ef0c)] - **src**: use BaseObjectPtr to store SNI context (Anna Henningsen) [#30548](https://github.com/nodejs/node/pull/30548)
* [[`8ec0d75de7`](https://github.com/nodejs/node/commit/8ec0d75de7)] - **src**: cleanup unused headers (Alexandre Ferrando) [#30328](https://github.com/nodejs/node/pull/30328)
* [[`6c249c0982`](https://github.com/nodejs/node/commit/6c249c0982)] - **src**: run native immediates during Environment cleanup (Anna Henningsen) [#30666](https://github.com/nodejs/node/pull/30666)
* [[`bea25016d1`](https://github.com/nodejs/node/commit/bea25016d1)] - **src**: no SetImmediate from destructor in stream\_pipe code (Anna Henningsen) [#30666](https://github.com/nodejs/node/pull/30666)
* [[`94357db815`](https://github.com/nodejs/node/commit/94357db815)] - **src**: add more `can_call_into_js()` guards (Anna Henningsen) [#30666](https://github.com/nodejs/node/pull/30666)
* [[`d54432f974`](https://github.com/nodejs/node/commit/d54432f974)] - **src**: keep object alive in stream\_pipe code (Anna Henningsen) [#30666](https://github.com/nodejs/node/pull/30666)
* [[`d194c0ff37`](https://github.com/nodejs/node/commit/d194c0ff37)] - **src**: replaced var with let (Aldo Ambrosioni) [#30397](https://github.com/nodejs/node/pull/30397)
* [[`44f28ea155`](https://github.com/nodejs/node/commit/44f28ea155)] - **src**: fix -Wsign-compare warnings (cjihrig) [#30565](https://github.com/nodejs/node/pull/30565)
* [[`1916acb3cb`](https://github.com/nodejs/node/commit/1916acb3cb)] - **src**: fix signal handler crash on close (Shelley Vohr) [#30582](https://github.com/nodejs/node/pull/30582)
* [[`9e9e48bf7e`](https://github.com/nodejs/node/commit/9e9e48bf7e)] - **src**: use uv\_async\_t for WeakRefs (Anna Henningsen) [#30616](https://github.com/nodejs/node/pull/30616)
* [[`9d8d2e1f45`](https://github.com/nodejs/node/commit/9d8d2e1f45)] - **src,doc**: fix broken links (cjihrig) [#30662](https://github.com/nodejs/node/pull/30662)
* [[`f135c38796`](https://github.com/nodejs/node/commit/f135c38796)] - **src,doc**: add C++ internals documentation (Anna Henningsen) [#30552](https://github.com/nodejs/node/pull/30552)
* [[`e968e26dbd`](https://github.com/nodejs/node/commit/e968e26dbd)] - **stream**: improve performance for sync write finishes (Anna Henningsen) [#30710](https://github.com/nodejs/node/pull/30710)
* [[`49e047f7a1`](https://github.com/nodejs/node/commit/49e047f7a1)] - **test**: add coverage for ERR\_TLS\_INVALID\_PROTOCOL\_VERSION (Rich Trott) [#30741](https://github.com/nodejs/node/pull/30741)
* [[`81d81a5904`](https://github.com/nodejs/node/commit/81d81a5904)] - **test**: add an indicator `isIBMi` (Xu Meng) [#30714](https://github.com/nodejs/node/pull/30714)
* [[`37c70ee198`](https://github.com/nodejs/node/commit/37c70ee198)] - **test**: use arrow functions in async-hooks tests (garygsc) [#30137](https://github.com/nodejs/node/pull/30137)
* [[`b5c7dad95a`](https://github.com/nodejs/node/commit/b5c7dad95a)] - **test**: fix test-benchmark-streams (Rich Trott) [#30757](https://github.com/nodejs/node/pull/30757)
* [[`1e199ceb71`](https://github.com/nodejs/node/commit/1e199ceb71)] - **test**: move test-http-max-http-headers to parallel (Rich Trott) [#30712](https://github.com/nodejs/node/pull/30712)
* [[`1918b4e84f`](https://github.com/nodejs/node/commit/1918b4e84f)] - **test**: correct header length subtraction (Rich Trott) [#30712](https://github.com/nodejs/node/pull/30712)
* [[`1222be81e3`](https://github.com/nodejs/node/commit/1222be81e3)] - **test**: remove unused callback argument (Rich Trott) [#30712](https://github.com/nodejs/node/pull/30712)
* [[`d69b9b753a`](https://github.com/nodejs/node/commit/d69b9b753a)] - **test**: simplify forEach() usage (Rich Trott) [#30712](https://github.com/nodejs/node/pull/30712)
* [[`01ab031cca`](https://github.com/nodejs/node/commit/01ab031cca)] - **test**: remove unused callback argument (Rich Trott) [#30712](https://github.com/nodejs/node/pull/30712)
* [[`93707c4916`](https://github.com/nodejs/node/commit/93707c4916)] - **test**: increase coverage for trace\_events.js (Rich Trott) [#30705](https://github.com/nodejs/node/pull/30705)
* [[`4800b623ed`](https://github.com/nodejs/node/commit/4800b623ed)] - **test**: use arrow functions in addons tests (garygsc) [#30131](https://github.com/nodejs/node/pull/30131)
* [[`ba0115fe6f`](https://github.com/nodejs/node/commit/ba0115fe6f)] - **test**: refactor createHook test (Jeny) [#30568](https://github.com/nodejs/node/pull/30568)
* [[`099d3fdf87`](https://github.com/nodejs/node/commit/099d3fdf87)] - **test**: port worker + buffer test to N-API (Anna Henningsen) [#30551](https://github.com/nodejs/node/pull/30551)
* [[`83861fb333`](https://github.com/nodejs/node/commit/83861fb333)] - **test**: revert 6d022c13 (Anna Henningsen) [#30708](https://github.com/nodejs/node/pull/30708)
* [[`a3b758d634`](https://github.com/nodejs/node/commit/a3b758d634)] - **test**: move test-https-server-consumed-timeout to parallel (Rich Trott) [#30677](https://github.com/nodejs/node/pull/30677)
* [[`00f532f15e`](https://github.com/nodejs/node/commit/00f532f15e)] - **test**: remove unnecessary common.platformTimeout() call (Rich Trott) [#30677](https://github.com/nodejs/node/pull/30677)
* [[`ecb902f33c`](https://github.com/nodejs/node/commit/ecb902f33c)] - **test**: do not skip test-http-server-consumed-timeout (Rich Trott) [#30677](https://github.com/nodejs/node/pull/30677)
* [[`49458deb4f`](https://github.com/nodejs/node/commit/49458deb4f)] - **test**: remove unused function argument from http test (Rich Trott) [#30677](https://github.com/nodejs/node/pull/30677)
* [[`a2f440d326`](https://github.com/nodejs/node/commit/a2f440d326)] - **test**: add logging in case of infinite loop (Rich Trott) [#30649](https://github.com/nodejs/node/pull/30649)
* [[`3e3ad396bd`](https://github.com/nodejs/node/commit/3e3ad396bd)] - **test**: remove destructuring from test-inspector-contexts (Rich Trott) [#30649](https://github.com/nodejs/node/pull/30649)
* [[`3571e132a7`](https://github.com/nodejs/node/commit/3571e132a7)] - **test**: check for session.post() errors in test-insepctor-context (Rich Trott) [#30649](https://github.com/nodejs/node/pull/30649)
* [[`37696320a2`](https://github.com/nodejs/node/commit/37696320a2)] - **test**: add mustCall() to test-inspector-contexts (Rich Trott) [#30649](https://github.com/nodejs/node/pull/30649)
* [[`0972fa3c16`](https://github.com/nodejs/node/commit/0972fa3c16)] - **test**: add regression test for signal handler removal in exit (Anna Henningsen) [#30589](https://github.com/nodejs/node/pull/30589)
* [[`5ecfd947e2`](https://github.com/nodejs/node/commit/5ecfd947e2)] - **(SEMVER-MINOR)** **test**: update and harden http2-reset-flood (Denys Otrishko) [#30534](https://github.com/nodejs/node/pull/30534)
* [[`70d6fa122a`](https://github.com/nodejs/node/commit/70d6fa122a)] - **test**: skip test-domain-error-types in debug mode temporariliy (Rich Trott) [#30629](https://github.com/nodejs/node/pull/30629)
* [[`949f2ad528`](https://github.com/nodejs/node/commit/949f2ad528)] - **test**: move test-worker-prof to sequential (Rich Trott) [#30628](https://github.com/nodejs/node/pull/30628)
* [[`d4b61709f1`](https://github.com/nodejs/node/commit/d4b61709f1)] - **test**: dir class initialisation w/o handler (Dmitriy Kikinskiy) [#30313](https://github.com/nodejs/node/pull/30313)
* [[`60b17b4fe6`](https://github.com/nodejs/node/commit/60b17b4fe6)] - **test**: change object assign by spread operator (poutch) [#30438](https://github.com/nodejs/node/pull/30438)
* [[`97e627335f`](https://github.com/nodejs/node/commit/97e627335f)] - **test**: use useful message argument in test function (Rich Trott) [#30618](https://github.com/nodejs/node/pull/30618)
* [[`d651c7dd6b`](https://github.com/nodejs/node/commit/d651c7dd6b)] - **test**: test for minimum ICU version consistency (Richard Lau) [#30608](https://github.com/nodejs/node/pull/30608)
* [[`dade9069c3`](https://github.com/nodejs/node/commit/dade9069c3)] - **test**: code&learn var to let update (Nazar Malyy) [#30436](https://github.com/nodejs/node/pull/30436)
* [[`e401e8c8ed`](https://github.com/nodejs/node/commit/e401e8c8ed)] - **test**: change object assign to spread object (poutch) [#30422](https://github.com/nodejs/node/pull/30422)
* [[`2ecc735c48`](https://github.com/nodejs/node/commit/2ecc735c48)] - **test**: use spread instead of Object.assign (dnlup) [#30419](https://github.com/nodejs/node/pull/30419)
* [[`d8da9dacab`](https://github.com/nodejs/node/commit/d8da9dacab)] - **test**: changed var to let in module-errors (Jamar Torres) [#30413](https://github.com/nodejs/node/pull/30413)
* [[`9dab32f340`](https://github.com/nodejs/node/commit/9dab32f340)] - **test**: use spread instead of object.assign (Shubham Chaturvedi) [#30412](https://github.com/nodejs/node/pull/30412)
* [[`7e7a8165a8`](https://github.com/nodejs/node/commit/7e7a8165a8)] - **test**: replace var with let in pre\_execution.js (Vladimir Adamic) [#30411](https://github.com/nodejs/node/pull/30411)
* [[`8a9ee48797`](https://github.com/nodejs/node/commit/8a9ee48797)] - **test**: change var to let in test-trace-events (Jon Church) [#30406](https://github.com/nodejs/node/pull/30406)
* [[`d6a448825c`](https://github.com/nodejs/node/commit/d6a448825c)] - **test**: dns utils replace var (Osmond van Hemert) [#30405](https://github.com/nodejs/node/pull/30405)
* [[`01e0571e94`](https://github.com/nodejs/node/commit/01e0571e94)] - **test**: test cover cases when trace is empty (telenord) [#30311](https://github.com/nodejs/node/pull/30311)
* [[`f8dfa2d704`](https://github.com/nodejs/node/commit/f8dfa2d704)] - **test**: switch to object spread in common/benchmark.js (palmires) [#30309](https://github.com/nodejs/node/pull/30309)
* [[`36671f9bf8`](https://github.com/nodejs/node/commit/36671f9bf8)] - **test**: add common.mustCall() to stream test (Rich Trott) [#30561](https://github.com/nodejs/node/pull/30561)
* [[`106235fe91`](https://github.com/nodejs/node/commit/106235fe91)] - **test**: move explanatory comment to expected location in file (Rich Trott) [#30561](https://github.com/nodejs/node/pull/30561)
* [[`081b4e2496`](https://github.com/nodejs/node/commit/081b4e2496)] - **test**: move stream test to parallel (Rich Trott) [#30561](https://github.com/nodejs/node/pull/30561)
* [[`103d01e057`](https://github.com/nodejs/node/commit/103d01e057)] - **test**: remove string literal as message in strictEqual() in stream test (Rich Trott) [#30561](https://github.com/nodejs/node/pull/30561)
* [[`ebba3228e2`](https://github.com/nodejs/node/commit/ebba3228e2)] - **test**: use arrow function for callback in stream test (Rich Trott) [#30561](https://github.com/nodejs/node/pull/30561)
* [[`e122d397c0`](https://github.com/nodejs/node/commit/e122d397c0)] - **test**: replace setTimeout with setImmediate in stream test (Rich Trott) [#30561](https://github.com/nodejs/node/pull/30561)
* [[`20ee4997f3`](https://github.com/nodejs/node/commit/20ee4997f3)] - **test**: refactor test-dgram-multicast-set-interface-lo.js (Taylor Gagne) [#30536](https://github.com/nodejs/node/pull/30536)
* [[`7aa1df7076`](https://github.com/nodejs/node/commit/7aa1df7076)] - **tls**: introduce ERR\_TLS\_INVALID\_CONTEXT (Rich Trott) [#30718](https://github.com/nodejs/node/pull/30718)
* [[`0b0f0237c1`](https://github.com/nodejs/node/commit/0b0f0237c1)] - **tls**: add memory tracking support to SSLWrap (Anna Henningsen) [#30548](https://github.com/nodejs/node/pull/30548)
* [[`89e2c71b27`](https://github.com/nodejs/node/commit/89e2c71b27)] - **tls**: allow empty subject even with altNames defined (Jason Macgowan) [#22906](https://github.com/nodejs/node/pull/22906)
* [[`941a91daed`](https://github.com/nodejs/node/commit/941a91daed)] - **tools**: enforce blank line between functions (Rich Trott) [#30696](https://github.com/nodejs/node/pull/30696)
* [[`5a6f836a15`](https://github.com/nodejs/node/commit/5a6f836a15)] - **tools**: add unified plugin changing links for html docs (Marek Łabuz) [#29946](https://github.com/nodejs/node/pull/29946)
* [[`84f7b5c752`](https://github.com/nodejs/node/commit/84f7b5c752)] - **tools**: enable more eslint rules (cjihrig) [#30598](https://github.com/nodejs/node/pull/30598)
* [[`5522467cf5`](https://github.com/nodejs/node/commit/5522467cf5)] - **tools**: update ESLint to 6.7.1 (cjihrig) [#30598](https://github.com/nodejs/node/pull/30598)
* [[`1f10681496`](https://github.com/nodejs/node/commit/1f10681496)] - **tty**: truecolor check moved before 256 check (Duncan Healy) [#30474](https://github.com/nodejs/node/pull/30474)
* [[`6a0dd1cbbd`](https://github.com/nodejs/node/commit/6a0dd1cbbd)] - **util**: fix .format() not always calling toString when it should be (Ruben Bridgewater) [#30343](https://github.com/nodejs/node/pull/30343)
* [[`1040e7222f`](https://github.com/nodejs/node/commit/1040e7222f)] - **util**: fix inspection of errors with tampered name or stack property (Ruben Bridgewater) [#30576](https://github.com/nodejs/node/pull/30576)
* [[`18e9b56bf6`](https://github.com/nodejs/node/commit/18e9b56bf6)] - **util**: use let instead of var for util/inspect.js (Luciano) [#30399](https://github.com/nodejs/node/pull/30399)
* [[`9ec53cf5c1`](https://github.com/nodejs/node/commit/9ec53cf5c1)] - **(SEMVER-MINOR)** **wasi**: introduce initial WASI support (cjihrig) [#30258](https://github.com/nodejs/node/pull/30258)

<a id="13.2.0"></a>
## 2019-11-21, Version 13.2.0 (Current), @MylesBorins

### Notable Changes

* **addons**:
  * Deprecate one- and two-argument `AtExit()`. Use the three-argument variant of `AtExit()` or `AddEnvironmentCleanupHook()` instead (Anna Henningsen) [#30227](https://github.com/nodejs/node/pull/30227)
* **child_process,cluster**:
  * The `serialization` option is added that allows child process IPC to use the V8 serialization API (to e.g., pass through data types like sets or maps) (Anna Henningsen) [#30162](https://github.com/nodejs/node/pull/30162)
* **deps**:
  * Update V8 to 7.9
  * Update `npm` to 6.13.1 (Ruy Adorno) [#30271](https://github.com/nodejs/node/pull/30271)
* **embedder**:
  * Exposes the ability to pass cli flags / options through an API as embedder (Shelley Vohr) [#30466](https://github.com/nodejs/node/pull/30466)
  * Allow adding linked bindings to Environment (Anna Henningsen) [#30274](https://github.com/nodejs/node/pull/30274)
* **esm**:
  * Unflag `--experimental-modules` (Guy Bedford) [#29866](https://github.com/nodejs/node/pull/29866)
* **stream**:
  * Add `writable.writableCorked` property (Robert Nagy) [#29012](https://github.com/nodejs/node/pull/29012)
* **worker**:
  * Allow specifying resource limits (Anna Henningsen) [#26628](https://github.com/nodejs/node/pull/26628)
* **v8**:
  * The Serialization API is now stable (Anna Henningsen) [#30234](https://github.com/nodejs/node/pull/30234)

### Commits

* [[`b76c13ec86`](https://github.com/nodejs/node/commit/b76c13ec86)] - **assert**: replace var with let in lib/assert.js (PerfectPan) [#30261](https://github.com/nodejs/node/pull/30261)
* [[`7f49816e8a`](https://github.com/nodejs/node/commit/7f49816e8a)] - **benchmark**: use let instead of var in async\_hooks (dnlup) [#30470](https://github.com/nodejs/node/pull/30470)
* [[`0130d2b6e0`](https://github.com/nodejs/node/commit/0130d2b6e0)] - **benchmark**: use let instead of var in assert (dnlup) [#30450](https://github.com/nodejs/node/pull/30450)
* [[`9cae205f4d`](https://github.com/nodejs/node/commit/9cae205f4d)] - **buffer**: change var to let (Vladislav Botvin) [#30292](https://github.com/nodejs/node/pull/30292)
* [[`b5198cd3b0`](https://github.com/nodejs/node/commit/b5198cd3b0)] - **(SEMVER-MINOR)** **build**: reset embedder string to "-node.0" (Michaël Zasso) [#30513](https://github.com/nodejs/node/pull/30513)
* [[`f4f210adc1`](https://github.com/nodejs/node/commit/f4f210adc1)] - **build**: store cache on timed out builds on Travis (Richard Lau) [#30469](https://github.com/nodejs/node/pull/30469)
* [[`277e5fadf8`](https://github.com/nodejs/node/commit/277e5fadf8)] - **(SEMVER-MINOR)** **build,tools**: update V8 gypfiles for V8 7.9 (Michaël Zasso) [#30020](https://github.com/nodejs/node/pull/30020)
* [[`e51beef8d4`](https://github.com/nodejs/node/commit/e51beef8d4)] - **(SEMVER-MINOR)** **child_process,cluster**: allow using V8 serialization API (Anna Henningsen) [#30162](https://github.com/nodejs/node/pull/30162)
* [[`6bf0e40bad`](https://github.com/nodejs/node/commit/6bf0e40bad)] - **cluster**: destruct primordials in lib/internal/cluster/worker.js (peze) [#30246](https://github.com/nodejs/node/pull/30246)
* [[`18ec8a84be`](https://github.com/nodejs/node/commit/18ec8a84be)] - **(SEMVER-MINOR)** **crypto**: add support for IEEE-P1363 DSA signatures (Tobias Nießen) [#29292](https://github.com/nodejs/node/pull/29292)
* [[`39d0a25ddd`](https://github.com/nodejs/node/commit/39d0a25ddd)] - **crypto**: fix key requirements in asymmetric cipher (Tobias Nießen) [#30249](https://github.com/nodejs/node/pull/30249)
* [[`8c2e2ce6bf`](https://github.com/nodejs/node/commit/8c2e2ce6bf)] - **crypto**: update root certificates (AshCripps) [#30195](https://github.com/nodejs/node/pull/30195)
* [[`4f282f52f0`](https://github.com/nodejs/node/commit/4f282f52f0)] - **deps**: patch V8 to 7.9.317.23 (Myles Borins) [#30560](https://github.com/nodejs/node/pull/30560)
* [[`9b71534d23`](https://github.com/nodejs/node/commit/9b71534d23)] - **deps**: upgrade npm to 6.13.1 (claudiahdz) [#30533](https://github.com/nodejs/node/pull/30533)
* [[`f17c794faf`](https://github.com/nodejs/node/commit/f17c794faf)] - **(SEMVER-MINOR)** **deps**: patch V8 to be API/ABI compatible with 7.8 (from 7.9) (Michaël Zasso) [#30513](https://github.com/nodejs/node/pull/30513)
* [[`5a1ad570ea`](https://github.com/nodejs/node/commit/5a1ad570ea)] - **deps**: V8: cherry-pick a7dffcd767be (Christian Clauss) [#30218](https://github.com/nodejs/node/pull/30218)
* [[`2c6cf902b0`](https://github.com/nodejs/node/commit/2c6cf902b0)] - **(SEMVER-MINOR)** **deps**: V8: cherry-pick 50031fae736f (Michaël Zasso) [#30020](https://github.com/nodejs/node/pull/30020)
* [[`1e5e8c3922`](https://github.com/nodejs/node/commit/1e5e8c3922)] - **deps**: V8: cherry-pick e5dbc95 (Gabriel Schulhof) [#30130](https://github.com/nodejs/node/pull/30130)
* [[`9c356ba91c`](https://github.com/nodejs/node/commit/9c356ba91c)] - **(SEMVER-MINOR)** **deps**: V8: backport 5e755c6ee6d3 (Michaël Zasso) [#30020](https://github.com/nodejs/node/pull/30020)
* [[`fe99841c88`](https://github.com/nodejs/node/commit/fe99841c88)] - **(SEMVER-MINOR)** **deps**: V8: backport 07ee86a5a28b (Michaël Zasso) [#30020](https://github.com/nodejs/node/pull/30020)
* [[`5131bbe477`](https://github.com/nodejs/node/commit/5131bbe477)] - **(SEMVER-MINOR)** **deps**: V8: cherry-pick 777fa98 (Michaël Zasso) [#30020](https://github.com/nodejs/node/pull/30020)
* [[`824e8b6f9b`](https://github.com/nodejs/node/commit/824e8b6f9b)] - **(SEMVER-MINOR)** **deps**: V8: cherry-pick 7228ef8 (Michaël Zasso) [#30020](https://github.com/nodejs/node/pull/30020)
* [[`4c7acc256a`](https://github.com/nodejs/node/commit/4c7acc256a)] - **(SEMVER-MINOR)** **deps**: V8: cherry-pick 6b0a953 (Michaël Zasso) [#30020](https://github.com/nodejs/node/pull/30020)
* [[`ebef1b2308`](https://github.com/nodejs/node/commit/ebef1b2308)] - **(SEMVER-MINOR)** **deps**: V8: cherry-pick bba5f1f (Michaël Zasso) [#30020](https://github.com/nodejs/node/pull/30020)
* [[`28ca44c724`](https://github.com/nodejs/node/commit/28ca44c724)] - **(SEMVER-MINOR)** **deps**: V8: cherry-pick cfe9172 (Michaël Zasso) [#30020](https://github.com/nodejs/node/pull/30020)
* [[`ba4abfd198`](https://github.com/nodejs/node/commit/ba4abfd198)] - **(SEMVER-MINOR)** **deps**: V8: cherry-pick 3e82c8d (Michaël Zasso) [#30020](https://github.com/nodejs/node/pull/30020)
* [[`2abdcbbe5e`](https://github.com/nodejs/node/commit/2abdcbbe5e)] - **(SEMVER-MINOR)** **deps**: V8: cherry-pick f2d92ec (Michaël Zasso) [#30020](https://github.com/nodejs/node/pull/30020)
* [[`46383616e3`](https://github.com/nodejs/node/commit/46383616e3)] - **(SEMVER-MINOR)** **deps**: make v8.h compatible with VS2015 (Joao Reis) [#30020](https://github.com/nodejs/node/pull/30020)
* [[`5bc35732aa`](https://github.com/nodejs/node/commit/5bc35732aa)] - **(SEMVER-MINOR)** **deps**: V8: forward declaration of `Rtl\*FunctionTable` (Refael Ackermann) [#27375](https://github.com/nodejs/node/pull/27375)
* [[`627a804627`](https://github.com/nodejs/node/commit/627a804627)] - **(SEMVER-MINOR)** **deps**: V8: patch register-arm64.h (Refael Ackermann) [#27375](https://github.com/nodejs/node/pull/27375)
* [[`13e6b0b82a`](https://github.com/nodejs/node/commit/13e6b0b82a)] - **(SEMVER-MINOR)** **deps**: update V8's postmortem script (Colin Ihrig) [#30020](https://github.com/nodejs/node/pull/30020)
* [[`a4a6565348`](https://github.com/nodejs/node/commit/a4a6565348)] - **(SEMVER-MINOR)** **deps**: update V8's postmortem script (Colin Ihrig) [#29694](https://github.com/nodejs/node/pull/29694)
* [[`4182e3bad7`](https://github.com/nodejs/node/commit/4182e3bad7)] - **(SEMVER-MINOR)** **deps**: patch V8 to run on older XCode versions (Ujjwal Sharma) [#29694](https://github.com/nodejs/node/pull/29694)
* [[`6566c15157`](https://github.com/nodejs/node/commit/6566c15157)] - **(SEMVER-MINOR)** **deps**: V8: silence irrelevant warnings (Michaël Zasso) [#26685](https://github.com/nodejs/node/pull/26685)
* [[`6018db2ef9`](https://github.com/nodejs/node/commit/6018db2ef9)] - **(SEMVER-MINOR)** **deps**: V8: un-cherry-pick bd019bd (Refael Ackermann) [#26685](https://github.com/nodejs/node/pull/26685)
* [[`605cb9f0fc`](https://github.com/nodejs/node/commit/605cb9f0fc)] - **(SEMVER-MINOR)** **deps**: update V8 to 7.9.317.22 (Michaël Zasso) [#30513](https://github.com/nodejs/node/pull/30513)
* [[`b82f63d9ca`](https://github.com/nodejs/node/commit/b82f63d9ca)] - **deps**: update nghttp2 to 1.40.0 (gengjiawen) [#30493](https://github.com/nodejs/node/pull/30493)
* [[`401d2e9115`](https://github.com/nodejs/node/commit/401d2e9115)] - **deps**: update npm to 6.13.0 (Ruy Adorno) [#30271](https://github.com/nodejs/node/pull/30271)
* [[`f8ee70c94d`](https://github.com/nodejs/node/commit/f8ee70c94d)] - **dgram**: remove listeners on bind error (Anna Henningsen) [#30210](https://github.com/nodejs/node/pull/30210)
* [[`0433d7995a`](https://github.com/nodejs/node/commit/0433d7995a)] - **dgram**: reset bind state before emitting error (Anna Henningsen) [#30210](https://github.com/nodejs/node/pull/30210)
* [[`0f8662d615`](https://github.com/nodejs/node/commit/0f8662d615)] - **dns**: switch var to const/let (Dmitriy Kikinskiy) [#30302](https://github.com/nodejs/node/pull/30302)
* [[`ab887bd5f6`](https://github.com/nodejs/node/commit/ab887bd5f6)] - **doc**: add mention for using promisify on class methods (Denys Otrishko) [#30355](https://github.com/nodejs/node/pull/30355)
* [[`9940116aba`](https://github.com/nodejs/node/commit/9940116aba)] - **doc**: explain GIT\_REMOTE\_REF in COLLABORATOR\_GUIDE (Denys Otrishko) [#30371](https://github.com/nodejs/node/pull/30371)
* [[`027bde563d`](https://github.com/nodejs/node/commit/027bde563d)] - **doc**: fix overriding of prefix option (Luigi Pinca) [#30518](https://github.com/nodejs/node/pull/30518)
* [[`b7757533bc`](https://github.com/nodejs/node/commit/b7757533bc)] - **doc**: update http.md mention of socket (Jesse O'Connor) [#30155](https://github.com/nodejs/node/pull/30155)
* [[`7f664e454b`](https://github.com/nodejs/node/commit/7f664e454b)] - **doc**: adds NO\_COLOR to assert doc page (Shobhit Chittora) [#30483](https://github.com/nodejs/node/pull/30483)
* [[`fba2f9a3d6`](https://github.com/nodejs/node/commit/fba2f9a3d6)] - **doc**: document timed out Travis CI builds (Richard Lau) [#30469](https://github.com/nodejs/node/pull/30469)
* [[`c40e242b32`](https://github.com/nodejs/node/commit/c40e242b32)] - **doc**: replace const / var with let (Duncan Healy) [#30446](https://github.com/nodejs/node/pull/30446)
* [[`a93345b7cd`](https://github.com/nodejs/node/commit/a93345b7cd)] - **doc**: update outdated commonjs compat info (Geoffrey Booth) [#30512](https://github.com/nodejs/node/pull/30512)
* [[`b590533253`](https://github.com/nodejs/node/commit/b590533253)] - **doc**: esm: improve dual package hazard docs (Geoffrey Booth) [#30345](https://github.com/nodejs/node/pull/30345)
* [[`d631a0a3e4`](https://github.com/nodejs/node/commit/d631a0a3e4)] - **doc**: update 8.x to 10.x in backporting guide (garygsc) [#30481](https://github.com/nodejs/node/pull/30481)
* [[`7e603bed52`](https://github.com/nodejs/node/commit/7e603bed52)] - **doc**: createRequire can take import.meta.url directly (Geoffrey Booth) [#30495](https://github.com/nodejs/node/pull/30495)
* [[`e4a296ce8d`](https://github.com/nodejs/node/commit/e4a296ce8d)] - **doc**: add entry to url.parse() changes metadata (Luigi Pinca) [#30348](https://github.com/nodejs/node/pull/30348)
* [[`64cf00b0b9`](https://github.com/nodejs/node/commit/64cf00b0b9)] - **doc**: simplify text in pull-requests.md (Rich Trott) [#30458](https://github.com/nodejs/node/pull/30458)
* [[`1e2672012f`](https://github.com/nodejs/node/commit/1e2672012f)] - **doc**: remove "multiple variants" from BUILDING.md (Rich Trott) [#30366](https://github.com/nodejs/node/pull/30366)
* [[`2d16a74ff9`](https://github.com/nodejs/node/commit/2d16a74ff9)] - **doc**: remove "maintenance is supported by" text in BUILDING.md (Rich Trott) [#30365](https://github.com/nodejs/node/pull/30365)
* [[`c832565290`](https://github.com/nodejs/node/commit/c832565290)] - **doc**: add lookup to http.request() options (Luigi Pinca) [#30353](https://github.com/nodejs/node/pull/30353)
* [[`b8afe57e85`](https://github.com/nodejs/node/commit/b8afe57e85)] - **doc**: fix up N-API doc (Michael Dawson) [#30254](https://github.com/nodejs/node/pull/30254)
* [[`b558d941bd`](https://github.com/nodejs/node/commit/b558d941bd)] - **doc**: fix some recent doc nits (vsemozhetbyt) [#30341](https://github.com/nodejs/node/pull/30341)
* [[`1133981eac`](https://github.com/nodejs/node/commit/1133981eac)] - **doc**: add link to node-code-ide-configs in testing (Trivikram Kamat) [#24012](https://github.com/nodejs/node/pull/24012)
* [[`041f3a306e`](https://github.com/nodejs/node/commit/041f3a306e)] - **doc**: update divergent specifier hazard guidance (Geoffrey Booth) [#30051](https://github.com/nodejs/node/pull/30051)
* [[`085af30361`](https://github.com/nodejs/node/commit/085af30361)] - **doc**: include --experimental-resolve-self in manpage (Guy Bedford) [#29978](https://github.com/nodejs/node/pull/29978)
* [[`31a3b724f0`](https://github.com/nodejs/node/commit/31a3b724f0)] - **doc**: update GOVERNANCE.md (Rich Trott) [#30259](https://github.com/nodejs/node/pull/30259)
* [[`15a7032d44`](https://github.com/nodejs/node/commit/15a7032d44)] - **doc**: move inactive Collaborators to emeriti (Rich Trott) [#30243](https://github.com/nodejs/node/pull/30243)
* [[`fabc489dba`](https://github.com/nodejs/node/commit/fabc489dba)] - **doc**: update examples in writing-tests.md (garygsc) [#30126](https://github.com/nodejs/node/pull/30126)
* [[`1836eae7a6`](https://github.com/nodejs/node/commit/1836eae7a6)] - **doc, console**: remove non-existant methods from docs (Simon Schick) [#30346](https://github.com/nodejs/node/pull/30346)
* [[`7ad2e024dd`](https://github.com/nodejs/node/commit/7ad2e024dd)] - **doc,meta**: allow Travis results for doc/comment changes (Rich Trott) [#30330](https://github.com/nodejs/node/pull/30330)
* [[`2deea28070`](https://github.com/nodejs/node/commit/2deea28070)] - **doc,meta**: remove wait period for npm pull requests (Rich Trott) [#30329](https://github.com/nodejs/node/pull/30329)
* [[`7e0f90e286`](https://github.com/nodejs/node/commit/7e0f90e286)] - **domain**: rename var to let and const (Maria Stogova) [#30312](https://github.com/nodejs/node/pull/30312)
* [[`c2c74fc93e`](https://github.com/nodejs/node/commit/c2c74fc93e)] - **encoding**: make TextDecoder handle BOM correctly (Anna Henningsen) [#30132](https://github.com/nodejs/node/pull/30132)
* [[`f9eab48dd0`](https://github.com/nodejs/node/commit/f9eab48dd0)] - **esm**: disable non-js exts outside package scopes (Guy Bedford) [#30501](https://github.com/nodejs/node/pull/30501)
* [[`3d8cdf191d`](https://github.com/nodejs/node/commit/3d8cdf191d)] - **esm**: unflag --experimental-modules (Guy Bedford) [#29866](https://github.com/nodejs/node/pull/29866)
* [[`293e8a2384`](https://github.com/nodejs/node/commit/293e8a2384)] - **esm**: exit the process with an error if loader has an issue (Michaël Zasso) [#30219](https://github.com/nodejs/node/pull/30219)
* [[`45fd44c6ec`](https://github.com/nodejs/node/commit/45fd44c6ec)] - **fs**: change var to let (Nadya) [#30318](https://github.com/nodejs/node/pull/30318)
* [[`bb6f944607`](https://github.com/nodejs/node/commit/bb6f944607)] - **fs**: add noop stub for FSWatcher.prototype.start (Lucas Holmquist) [#30160](https://github.com/nodejs/node/pull/30160)
* [[`4fe62c1620`](https://github.com/nodejs/node/commit/4fe62c1620)] - **http**: revise \_http\_server.js (telenord) [#30279](https://github.com/nodejs/node/pull/30279)
* [[`62e15a793a`](https://github.com/nodejs/node/commit/62e15a793a)] - **http**: outgoing cork (Robert Nagy) [#29053](https://github.com/nodejs/node/pull/29053)
* [[`50f9476a44`](https://github.com/nodejs/node/commit/50f9476a44)] - **http**: http\_common rename var to let and const (telenord) [#30288](https://github.com/nodejs/node/pull/30288)
* [[`b8aceace95`](https://github.com/nodejs/node/commit/b8aceace95)] - **http**: http\_incoming rename var to let and const (telenord) [#30285](https://github.com/nodejs/node/pull/30285)
* [[`a37ade8648`](https://github.com/nodejs/node/commit/a37ade8648)] - **http**: replace vars with lets and consts in lib/\_http\_agent.js (palmires) [#30301](https://github.com/nodejs/node/pull/30301)
* [[`e59cc8aad8`](https://github.com/nodejs/node/commit/e59cc8aad8)] - **http,async_hooks**: keep resource object alive from socket (Anna Henningsen) [#30196](https://github.com/nodejs/node/pull/30196)
* [[`1b84175924`](https://github.com/nodejs/node/commit/1b84175924)] - **http2**: remove duplicated assertIsObject (Yongsheng Zhang) [#30541](https://github.com/nodejs/node/pull/30541)
* [[`666588143e`](https://github.com/nodejs/node/commit/666588143e)] - **http2**: use custom BaseObject smart pointers (Anna Henningsen) [#30374](https://github.com/nodejs/node/pull/30374)
* [[`f25b00aaca`](https://github.com/nodejs/node/commit/f25b00aaca)] - **(SEMVER-MINOR)** **https**: add client support for TLS keylog events (Sam Roberts) [#30053](https://github.com/nodejs/node/pull/30053)
* [[`88da3af6f6`](https://github.com/nodejs/node/commit/88da3af6f6)] - **https**: change var to let in lib/https.js (galina.prokofeva) [#30320](https://github.com/nodejs/node/pull/30320)
* [[`f15a3b0281`](https://github.com/nodejs/node/commit/f15a3b0281)] - **lib**: replace var with let (David OLIVIER) [#30381](https://github.com/nodejs/node/pull/30381)
* [[`31a63ab1ec`](https://github.com/nodejs/node/commit/31a63ab1ec)] - **lib**: replace var with let and const in readline.js (VinceOPS) [#30377](https://github.com/nodejs/node/pull/30377)
* [[`3eeeea419d`](https://github.com/nodejs/node/commit/3eeeea419d)] - **lib**: change var to let/const in internal/querystring.js (Artem Maksimov) [#30286](https://github.com/nodejs/node/pull/30286)
* [[`f10608655b`](https://github.com/nodejs/node/commit/f10608655b)] - **lib**: change var to let in internal/streams (Kyriakos Markakis) [#30430](https://github.com/nodejs/node/pull/30430)
* [[`3ce6e15844`](https://github.com/nodejs/node/commit/3ce6e15844)] - **lib**: replace var with let/const (Kenza Houmani) [#30440](https://github.com/nodejs/node/pull/30440)
* [[`d37d340472`](https://github.com/nodejs/node/commit/d37d340472)] - **lib**: change var to let in string\_decoder (mkdorff) [#30393](https://github.com/nodejs/node/pull/30393)
* [[`9a1c16eda4`](https://github.com/nodejs/node/commit/9a1c16eda4)] - **lib**: replaced var to let in lib/v8.js (Vadim Gorbachev) [#30305](https://github.com/nodejs/node/pull/30305)
* [[`3e4a6a5968`](https://github.com/nodejs/node/commit/3e4a6a5968)] - **lib**: change var to let in lib/\_stream\_duplex.js (Ilia Safronov) [#30297](https://github.com/nodejs/node/pull/30297)
* [[`c7c566023f`](https://github.com/nodejs/node/commit/c7c566023f)] - **module**: reduce circular dependency of internal/modules/cjs/loader (Joyee Cheung) [#30349](https://github.com/nodejs/node/pull/30349)
* [[`e98d89cef9`](https://github.com/nodejs/node/commit/e98d89cef9)] - **module**: conditional exports with flagged conditions (Guy Bedford) [#29978](https://github.com/nodejs/node/pull/29978)
* [[`caedcd9ef9`](https://github.com/nodejs/node/commit/caedcd9ef9)] - **module**: fix for empty object in InternalModuleReadJSON (Guy Bedford) [#30256](https://github.com/nodejs/node/pull/30256)
* [[`66e1adf200`](https://github.com/nodejs/node/commit/66e1adf200)] - **net**: destructure primordials (Guilherme Goncalves) [#30447](https://github.com/nodejs/node/pull/30447)
* [[`9230ffffd0`](https://github.com/nodejs/node/commit/9230ffffd0)] - **net**: replaced vars to lets and consts (alexahdp) [#30287](https://github.com/nodejs/node/pull/30287)
* [[`9248c8b960`](https://github.com/nodejs/node/commit/9248c8b960)] - **path**: replace var with let in lib/path.js (peze) [#30260](https://github.com/nodejs/node/pull/30260)
* [[`e363f8e17f`](https://github.com/nodejs/node/commit/e363f8e17f)] - **process**: add coverage tests for sourceMapFromDataUrl method (Nolik) [#30319](https://github.com/nodejs/node/pull/30319)
* [[`7b4187413e`](https://github.com/nodejs/node/commit/7b4187413e)] - **process**: make source map getter resistant against prototype tampering (Anna Henningsen) [#30228](https://github.com/nodejs/node/pull/30228)
* [[`183464a24d`](https://github.com/nodejs/node/commit/183464a24d)] - **querystring**: replace var with let/const (Raoul Jaeckel) [#30429](https://github.com/nodejs/node/pull/30429)
* [[`7188b9599d`](https://github.com/nodejs/node/commit/7188b9599d)] - **src**: fix -Winconsistent-missing-override warning (Colin Ihrig) [#30549](https://github.com/nodejs/node/pull/30549)
* [[`966404fd24`](https://github.com/nodejs/node/commit/966404fd24)] - **src**: add file name to 'Module did not self-register' error (Jeremy Apthorp) [#30125](https://github.com/nodejs/node/pull/30125)
* [[`21dd6019ec`](https://github.com/nodejs/node/commit/21dd6019ec)] - **(SEMVER-MINOR)** **src**: expose ArrayBuffer version of Buffer::New() (Anna Henningsen) [#30476](https://github.com/nodejs/node/pull/30476)
* [[`2e43686c5a`](https://github.com/nodejs/node/commit/2e43686c5a)] - **src**: mark ArrayBuffers with free callbacks as untransferable (Anna Henningsen) [#30475](https://github.com/nodejs/node/pull/30475)
* [[`564c18e214`](https://github.com/nodejs/node/commit/564c18e214)] - **src**: remove HandleWrap instances from list once closed (Anna Henningsen) [#30374](https://github.com/nodejs/node/pull/30374)
* [[`4222f2400a`](https://github.com/nodejs/node/commit/4222f2400a)] - **src**: remove keep alive option from SetImmediate() (Anna Henningsen) [#30374](https://github.com/nodejs/node/pull/30374)
* [[`940a2972b2`](https://github.com/nodejs/node/commit/940a2972b2)] - **src**: use BaseObjectPtr for keeping channel alive in dns bindings (Anna Henningsen) [#30374](https://github.com/nodejs/node/pull/30374)
* [[`a2dbadc1ce`](https://github.com/nodejs/node/commit/a2dbadc1ce)] - **src**: introduce custom smart pointers for `BaseObject`s (Anna Henningsen) [#30374](https://github.com/nodejs/node/pull/30374)
* [[`1a92c88418`](https://github.com/nodejs/node/commit/1a92c88418)] - **src**: migrate off ArrayBuffer::GetContents (Anna Henningsen) [#30339](https://github.com/nodejs/node/pull/30339)
* [[`0d5de1a20e`](https://github.com/nodejs/node/commit/0d5de1a20e)] - **(SEMVER-MINOR)** **src**: remove custom tracking for SharedArrayBuffers (Anna Henningsen) [#30020](https://github.com/nodejs/node/pull/30020)
* [[`f0ff2ed9d5`](https://github.com/nodejs/node/commit/f0ff2ed9d5)] - **(SEMVER-MINOR)** **src**: update v8abbr.h for V8 update (Colin Ihrig) [#30020](https://github.com/nodejs/node/pull/30020)
* [[`2c8276eda8`](https://github.com/nodejs/node/commit/2c8276eda8)] - **(SEMVER-MINOR)** **src**: expose ability to set options (Shelley Vohr) [#30466](https://github.com/nodejs/node/pull/30466)
* [[`592d51cb23`](https://github.com/nodejs/node/commit/592d51cb23)] - **src**: enhance feature access `CHECK`s during bootstrap (Anna Henningsen) [#30452](https://github.com/nodejs/node/pull/30452)
* [[`d648c933b5`](https://github.com/nodejs/node/commit/d648c933b5)] - **src**: lib/internal/timers.js var -\> let/const (Nikolay Krashnikov) [#30314](https://github.com/nodejs/node/pull/30314)
* [[`70ad676023`](https://github.com/nodejs/node/commit/70ad676023)] - **src**: persist strings that are used multiple times in the environment (Vadim Gorbachev) [#30321](https://github.com/nodejs/node/pull/30321)
* [[`b744070d74`](https://github.com/nodejs/node/commit/b744070d74)] - **(SEMVER-MINOR)** **src**: allow adding linked bindings to Environment (Anna Henningsen) [#30274](https://github.com/nodejs/node/pull/30274)
* [[`058a8d5363`](https://github.com/nodejs/node/commit/058a8d5363)] - **src**: do not use `std::function` for `OnScopeLeave` (Anna Henningsen) [#30134](https://github.com/nodejs/node/pull/30134)
* [[`906d279e69`](https://github.com/nodejs/node/commit/906d279e69)] - **src**: run RunBeforeExitCallbacks as part of EmitBeforeExit (Anna Henningsen) [#30229](https://github.com/nodejs/node/pull/30229)
* [[`66b3619b4e`](https://github.com/nodejs/node/commit/66b3619b4e)] - **src**: use unique\_ptr for InitializeInspector() (Anna Henningsen) [#30229](https://github.com/nodejs/node/pull/30229)
* [[`db7deb6e7a`](https://github.com/nodejs/node/commit/db7deb6e7a)] - **src**: make WaitForInspectorDisconnect an exit hook (Anna Henningsen) [#30229](https://github.com/nodejs/node/pull/30229)
* [[`cd233e3f16`](https://github.com/nodejs/node/commit/cd233e3f16)] - **src**: make EndStartedProfilers an exit hook (Anna Henningsen) [#30229](https://github.com/nodejs/node/pull/30229)
* [[`8234d04b56`](https://github.com/nodejs/node/commit/8234d04b56)] - **src**: track no of active JS signal handlers (Anna Henningsen) [#30229](https://github.com/nodejs/node/pull/30229)
* [[`0072a8eddf`](https://github.com/nodejs/node/commit/0072a8eddf)] - **src**: remove AsyncScope and AsyncCallbackScope (Anna Henningsen) [#30236](https://github.com/nodejs/node/pull/30236)
* [[`e3371f0c93`](https://github.com/nodejs/node/commit/e3371f0c93)] - **src**: use callback scope for main script (Anna Henningsen) [#30236](https://github.com/nodejs/node/pull/30236)
* [[`cd6d6215cc`](https://github.com/nodejs/node/commit/cd6d6215cc)] - **(SEMVER-MINOR)** **src**: deprecate two- and one-argument AtExit() (Anna Henningsen) [#30227](https://github.com/nodejs/node/pull/30227)
* [[`5f4535a97c`](https://github.com/nodejs/node/commit/5f4535a97c)] - **src**: make AtExit() callbacks run in reverse order (Anna Henningsen) [#30230](https://github.com/nodejs/node/pull/30230)
* [[`44968f0edc`](https://github.com/nodejs/node/commit/44968f0edc)] - **src**: remove unimplemented method from node.h (Anna Henningsen) [#30098](https://github.com/nodejs/node/pull/30098)
* [[`4524c7ad36`](https://github.com/nodejs/node/commit/4524c7ad36)] - **stream**: replace var with let (daern91) [#30379](https://github.com/nodejs/node/pull/30379)
* [[`41720d78c9`](https://github.com/nodejs/node/commit/41720d78c9)] - **stream**: add writableCorked to Duplex (Anna Henningsen) [#29053](https://github.com/nodejs/node/pull/29053)
* [[`7cbdac9a71`](https://github.com/nodejs/node/commit/7cbdac9a71)] - **stream**: increase MAX\_HWM (Robert Nagy) [#29938](https://github.com/nodejs/node/pull/29938)
* [[`c254d7469d`](https://github.com/nodejs/node/commit/c254d7469d)] - **(SEMVER-MINOR)** **stream**: add writableCorked property (Robert Nagy) [#29012](https://github.com/nodejs/node/pull/29012)
* [[`cb9c64a6e0`](https://github.com/nodejs/node/commit/cb9c64a6e0)] - **test**: move test not requiring internet from internet to parallel (Rich Trott) [#30545](https://github.com/nodejs/node/pull/30545)
* [[`902c6702df`](https://github.com/nodejs/node/commit/902c6702df)] - **test**: use reserved .invalid TLD for invalid address in test (Rich Trott) [#30545](https://github.com/nodejs/node/pull/30545)
* [[`92f766bd83`](https://github.com/nodejs/node/commit/92f766bd83)] - **test**: improve assertion message in internet dgram test (Rich Trott) [#30545](https://github.com/nodejs/node/pull/30545)
* [[`a5f25ecf07`](https://github.com/nodejs/node/commit/a5f25ecf07)] - **test**: cover 'close' method in Dir class (Artem Maksimov) [#30310](https://github.com/nodejs/node/pull/30310)
* [[`45e57303f3`](https://github.com/nodejs/node/commit/45e57303f3)] - **test**: add test for options validation of createServer (Yongsheng Zhang) [#30541](https://github.com/nodejs/node/pull/30541)
* [[`6be03981b2`](https://github.com/nodejs/node/commit/6be03981b2)] - **test**: clean up http-set-trailers (Denys Otrishko) [#30522](https://github.com/nodejs/node/pull/30522)
* [[`2952c5d72b`](https://github.com/nodejs/node/commit/2952c5d72b)] - **(SEMVER-MINOR)** **test**: increase limit again for network space overhead test (Michaël Zasso) [#30020](https://github.com/nodejs/node/pull/30020)
* [[`4131b14011`](https://github.com/nodejs/node/commit/4131b14011)] - **(SEMVER-MINOR)** **test**: update test-postmortem-metadata.js (Colin Ihrig) [#30020](https://github.com/nodejs/node/pull/30020)
* [[`c464ede598`](https://github.com/nodejs/node/commit/c464ede598)] - **test**: handle undefined default\_configuration (Shelley Vohr) [#30465](https://github.com/nodejs/node/pull/30465)
* [[`5ec550de02`](https://github.com/nodejs/node/commit/5ec550de02)] - **test**: Change from var to const (Jure Stepisnik) [#30431](https://github.com/nodejs/node/pull/30431)
* [[`13bac0ac0f`](https://github.com/nodejs/node/commit/13bac0ac0f)] - **test**: changed var to let in test-repl-editor (JL Phillips) [#30443](https://github.com/nodejs/node/pull/30443)
* [[`0d12e9cc29`](https://github.com/nodejs/node/commit/0d12e9cc29)] - **test**: improve test-fs-open (Artem Maksimov) [#30280](https://github.com/nodejs/node/pull/30280)
* [[`89bc2526ab`](https://github.com/nodejs/node/commit/89bc2526ab)] - **test**: change var to let (nathias) [#30444](https://github.com/nodejs/node/pull/30444)
* [[`fa071efea4`](https://github.com/nodejs/node/commit/fa071efea4)] - **test**: changed var to const in test (Kerry Mahne) [#30434](https://github.com/nodejs/node/pull/30434)
* [[`13a22432fc`](https://github.com/nodejs/node/commit/13a22432fc)] - **test**: var to const in test-repl-multiline.js (SoulMonk) [#30433](https://github.com/nodejs/node/pull/30433)
* [[`109da52141`](https://github.com/nodejs/node/commit/109da52141)] - **test**: deflake test-http-dump-req-when-res-ends.js (Luigi Pinca) [#30360](https://github.com/nodejs/node/pull/30360)
* [[`72bbd5cdb0`](https://github.com/nodejs/node/commit/72bbd5cdb0)] - **test**: change var to const in parallel/test-stream-transform-final\* (Kenza Houmani) [#30448](https://github.com/nodejs/node/pull/30448)
* [[`cd82e4d9d8`](https://github.com/nodejs/node/commit/cd82e4d9d8)] - **test**: replace Object.assign with object spread (Grigoriy Levanov) [#30306](https://github.com/nodejs/node/pull/30306)
* [[`aec695eb6c`](https://github.com/nodejs/node/commit/aec695eb6c)] - **test**: fix Python unittests in ./test and ./tools (Christian Clauss) [#30340](https://github.com/nodejs/node/pull/30340)
* [[`ea0c1a67c5`](https://github.com/nodejs/node/commit/ea0c1a67c5)] - **test**: mark test-http-dump-req-when-res-ends as flaky on windows (AshCripps) [#30316](https://github.com/nodejs/node/pull/30316)
* [[`308f5e4710`](https://github.com/nodejs/node/commit/308f5e4710)] - **test**: fix test-benchmark-cluster (Rich Trott) [#30342](https://github.com/nodejs/node/pull/30342)
* [[`bb0727a132`](https://github.com/nodejs/node/commit/bb0727a132)] - **test**: do not run release-npm test without crypto (Michaël Zasso) [#30265](https://github.com/nodejs/node/pull/30265)
* [[`ab5bca379f`](https://github.com/nodejs/node/commit/ab5bca379f)] - **test**: remove AtExit() addon test (Anna Henningsen) [#30275](https://github.com/nodejs/node/pull/30275)
* [[`de68720908`](https://github.com/nodejs/node/commit/de68720908)] - **test**: deflake test-tls-close-notify.js (Luigi Pinca) [#30202](https://github.com/nodejs/node/pull/30202)
* [[`8fe684961b`](https://github.com/nodejs/node/commit/8fe684961b)] - ***Revert*** "**test**: test configure ninja" (Anna Henningsen) [#30295](https://github.com/nodejs/node/pull/30295)
* [[`0dedecc7e0`](https://github.com/nodejs/node/commit/0dedecc7e0)] - **test**: test configure ninja (Patrick Housley) [#30033](https://github.com/nodejs/node/pull/30033)
* [[`01fa18c99c`](https://github.com/nodejs/node/commit/01fa18c99c)] - **(SEMVER-MINOR)** **tls**: cli option to enable TLS key logging to file (Sam Roberts) [#30055](https://github.com/nodejs/node/pull/30055)
* [[`5869f2bee7`](https://github.com/nodejs/node/commit/5869f2bee7)] - **tls**: change loop var to let (Xavier Redondo) [#30445](https://github.com/nodejs/node/pull/30445)
* [[`26a9bdfca3`](https://github.com/nodejs/node/commit/26a9bdfca3)] - **tls**: replace var with let (Daniil Pletnev) [#30308](https://github.com/nodejs/node/pull/30308)
* [[`bad0b66580`](https://github.com/nodejs/node/commit/bad0b66580)] - **tls**: replace var with let and const (Nolik) [#30299](https://github.com/nodejs/node/pull/30299)
* [[`ae5aa3ee83`](https://github.com/nodejs/node/commit/ae5aa3ee83)] - **tls**: refactor tls\_wrap.cc (Artem Maksimov) [#30303](https://github.com/nodejs/node/pull/30303)
* [[`80b1717c0f`](https://github.com/nodejs/node/commit/80b1717c0f)] - **tools**: fix build at non-English windows (Rongjian Zhang) [#30492](https://github.com/nodejs/node/pull/30492)
* [[`642b0b883f`](https://github.com/nodejs/node/commit/642b0b883f)] - **tools**: update tzdata to 2019c (Albert Wang) [#30356](https://github.com/nodejs/node/pull/30356)
* [[`3a44adebf8`](https://github.com/nodejs/node/commit/3a44adebf8)] - **tools**: pull xcode\_emulation.py from node-gyp (Christian Clauss) [#30272](https://github.com/nodejs/node/pull/30272)
* [[`92fa4e0096`](https://github.com/nodejs/node/commit/92fa4e0096)] - **tools**: make doctool work if no internet available (Richard Lau) [#30214](https://github.com/nodejs/node/pull/30214)
* [[`0f9f18aabe`](https://github.com/nodejs/node/commit/0f9f18aabe)] - **tools**: update certdata.txt (AshCripps) [#30195](https://github.com/nodejs/node/pull/30195)
* [[`dbdc3818e0`](https://github.com/nodejs/node/commit/dbdc3818e0)] - **tools**: check-imports using utf-8 (Christian Clauss) [#30220](https://github.com/nodejs/node/pull/30220)
* [[`3b45f8fd9c`](https://github.com/nodejs/node/commit/3b45f8fd9c)] - **url**: replace var with let in lib/url.js (xefimx) [#30281](https://github.com/nodejs/node/pull/30281)
* [[`35dc84859f`](https://github.com/nodejs/node/commit/35dc84859f)] - **util**: replace var with let (Susana Ferreira) [#30439](https://github.com/nodejs/node/pull/30439)
* [[`3727a6572b`](https://github.com/nodejs/node/commit/3727a6572b)] - **v8**: mark serdes API as stable (Anna Henningsen) [#30234](https://github.com/nodejs/node/pull/30234)
* [[`9b11bdb001`](https://github.com/nodejs/node/commit/9b11bdb001)] - **v8**: inspect unserializable objects (Anna Henningsen) [#30167](https://github.com/nodejs/node/pull/30167)
* [[`2ec40c265a`](https://github.com/nodejs/node/commit/2ec40c265a)] - **(SEMVER-MINOR)** **worker**: allow specifying resource limits (Anna Henningsen) [#26628](https://github.com/nodejs/node/pull/26628)

<a id="13.1.0"></a>
## 2019-11-05, Version 13.1.0 (Current), @targos

### Notable Changes

* **cli**:
  * Added a new flag (`--trace-uncaught`) that makes Node.js print the stack
    trace at the time of throwing uncaught exceptions, rather than at the
    creation of the `Error` object, if there is any. This is disabled by default
    because it affects GC behavior (Anna Henningsen) [#30025](https://github.com/nodejs/node/pull/30025).
* **crypto**:
  * Added `Hash.prototype.copy()` method. It returns a new `Hash` object with
    its internal state cloned from the original one (Ben Noordhuis) [#29910](https://github.com/nodejs/node/pull/29910).
* **dgram**:
  * Added source-specific multicast support. This adds methods to Datagram
    sockets to support [RFC 4607](https://tools.ietf.org/html/rfc4607) for IPv4
    and IPv6 (Lucas Pardue) [#15735](https://github.com/nodejs/node/pull/15735).
* **fs**:
  * Added a `bufferSize` option to `fs.opendir()`. It allows to control the
    number of entries that are buffered internally when reading from the
    directory (Anna Henningsen) [#30114](https://github.com/nodejs/node/pull/30114).
* **meta**:
  * Added [Chengzhong Wu](https://github.com/legendecas) to collaborators [#30115](https://github.com/nodejs/node/pull/30115).

### Commits

* [[`445837851b`](https://github.com/nodejs/node/commit/445837851b)] - **async_hooks**: only emit `after` for AsyncResource if stack not empty (Anna Henningsen) [#30087](https://github.com/nodejs/node/pull/30087)
* [[`8860bd68b6`](https://github.com/nodejs/node/commit/8860bd68b6)] - **buffer**: improve performance caused by primordials (Jizu Sun) [#30235](https://github.com/nodejs/node/pull/30235)
* [[`1bded9841c`](https://github.com/nodejs/node/commit/1bded9841c)] - **build**: fix detection of Visual Studio 2017 (Richard Lau) [#30119](https://github.com/nodejs/node/pull/30119)
* [[`49e7f042f9`](https://github.com/nodejs/node/commit/49e7f042f9)] - **build**: add workaround for WSL (gengjiawen) [#30221](https://github.com/nodejs/node/pull/30221)
* [[`03827ddf38`](https://github.com/nodejs/node/commit/03827ddf38)] - **build**: allow Python 3.8 (Michaël Zasso) [#30194](https://github.com/nodejs/node/pull/30194)
* [[`54698113c0`](https://github.com/nodejs/node/commit/54698113c0)] - **build**: find Python syntax errors in dependencies (Christian Clauss) [#30143](https://github.com/nodejs/node/pull/30143)
* [[`b255688d5f`](https://github.com/nodejs/node/commit/b255688d5f)] - **build**: fix pkg-config search for libnghttp2 (Ben Noordhuis) [#30145](https://github.com/nodejs/node/pull/30145)
* [[`8980d8c25f`](https://github.com/nodejs/node/commit/8980d8c25f)] - **build**: vcbuild uses default Python, not Py2 (João Reis) [#30091](https://github.com/nodejs/node/pull/30091)
* [[`cedad02406`](https://github.com/nodejs/node/commit/cedad02406)] - **build**: prefer python 3 over 2 for configure (Sam Roberts) [#30091](https://github.com/nodejs/node/pull/30091)
* [[`5ba842b8f9`](https://github.com/nodejs/node/commit/5ba842b8f9)] - **build**: python3 support for configure (Rod Vagg) [#30047](https://github.com/nodejs/node/pull/30047)
* [[`d05f67caef`](https://github.com/nodejs/node/commit/d05f67caef)] - **cli**: whitelist new V8 flag in NODE\_OPTIONS (Shelley Vohr) [#30094](https://github.com/nodejs/node/pull/30094)
* [[`5ca58646c1`](https://github.com/nodejs/node/commit/5ca58646c1)] - **(SEMVER-MINOR)** **cli**: add --trace-uncaught flag (Anna Henningsen) [#30025](https://github.com/nodejs/node/pull/30025)
* [[`8b75aabee9`](https://github.com/nodejs/node/commit/8b75aabee9)] - **crypto**: guard with OPENSSL\_NO\_GOST (Shelley Vohr) [#30050](https://github.com/nodejs/node/pull/30050)
* [[`1d03df4c5e`](https://github.com/nodejs/node/commit/1d03df4c5e)] - **(SEMVER-MINOR)** **crypto**: add Hash.prototype.copy() method (Ben Noordhuis) [#29910](https://github.com/nodejs/node/pull/29910)
* [[`46c9194ec8`](https://github.com/nodejs/node/commit/46c9194ec8)] - **deps**: V8: cherry-pick a7dffcd767be (Christian Clauss) [#30218](https://github.com/nodejs/node/pull/30218)
* [[`104bfb9a38`](https://github.com/nodejs/node/commit/104bfb9a38)] - **deps**: V8: cherry-pick e5dbc95 (Gabriel Schulhof) [#30130](https://github.com/nodejs/node/pull/30130)
* [[`e3124481c2`](https://github.com/nodejs/node/commit/e3124481c2)] - **deps**: update npm to 6.12.1 (Michael Perrotte) [#30164](https://github.com/nodejs/node/pull/30164)
* [[`f3d00c594d`](https://github.com/nodejs/node/commit/f3d00c594d)] - **deps**: V8: backport 777fa98 (Michaël Zasso) [#30062](https://github.com/nodejs/node/pull/30062)
* [[`1cfa98c23e`](https://github.com/nodejs/node/commit/1cfa98c23e)] - **deps**: V8: cherry-pick c721203 (Michaël Zasso) [#30065](https://github.com/nodejs/node/pull/30065)
* [[`0d9ae1b8f6`](https://github.com/nodejs/node/commit/0d9ae1b8f6)] - **deps**: V8: cherry-pick ed40ab1 (Michaël Zasso) [#30064](https://github.com/nodejs/node/pull/30064)
* [[`a63f7e73c4`](https://github.com/nodejs/node/commit/a63f7e73c4)] - **(SEMVER-MINOR)** **dgram**: add source-specific multicast support (Lucas Pardue) [#15735](https://github.com/nodejs/node/pull/15735)
* [[`fc407bb555`](https://github.com/nodejs/node/commit/fc407bb555)] - **doc**: add missing hash for header link (Nick Schonning) [#30188](https://github.com/nodejs/node/pull/30188)
* [[`201a60e6ba`](https://github.com/nodejs/node/commit/201a60e6ba)] - **doc**: linkify `.setupMaster()` in cluster doc (Trivikram Kamat) [#30204](https://github.com/nodejs/node/pull/30204)
* [[`b7070f315f`](https://github.com/nodejs/node/commit/b7070f315f)] - **doc**: explain http2 aborted event callback (dev-313) [#30179](https://github.com/nodejs/node/pull/30179)
* [[`f8fb2c06c5`](https://github.com/nodejs/node/commit/f8fb2c06c5)] - **doc**: linkify `.fork()` in cluster documentation (Anna Henningsen) [#30163](https://github.com/nodejs/node/pull/30163)
* [[`ae81360214`](https://github.com/nodejs/node/commit/ae81360214)] - **doc**: update AUTHORS list (Michaël Zasso) [#30142](https://github.com/nodejs/node/pull/30142)
* [[`1499a72a1f`](https://github.com/nodejs/node/commit/1499a72a1f)] - **doc**: improve doc Http2Session:Timeout (dev-313) [#30161](https://github.com/nodejs/node/pull/30161)
* [[`3709b5cc7e`](https://github.com/nodejs/node/commit/3709b5cc7e)] - **doc**: move inactive Collaborators to emeriti (Rich Trott) [#30177](https://github.com/nodejs/node/pull/30177)
* [[`a48d17900b`](https://github.com/nodejs/node/commit/a48d17900b)] - **doc**: add options description for send APIs (dev-313) [#29868](https://github.com/nodejs/node/pull/29868)
* [[`dfb4a24695`](https://github.com/nodejs/node/commit/dfb4a24695)] - **doc**: fix an error in resolution algorithm steps (Alex Zherdev) [#29940](https://github.com/nodejs/node/pull/29940)
* [[`403a648a16`](https://github.com/nodejs/node/commit/403a648a16)] - **doc**: fix numbering in require algorithm (Jan Krems) [#30117](https://github.com/nodejs/node/pull/30117)
* [[`e4ab6fced1`](https://github.com/nodejs/node/commit/e4ab6fced1)] - **doc**: remove incorrect and outdated example (Tobias Nießen) [#30138](https://github.com/nodejs/node/pull/30138)
* [[`3c23224a76`](https://github.com/nodejs/node/commit/3c23224a76)] - **doc**: adjust code sample for stream.finished (Cotton Hou) [#29983](https://github.com/nodejs/node/pull/29983)
* [[`d91d270416`](https://github.com/nodejs/node/commit/d91d270416)] - **doc**: claim NODE\_MODULE\_VERSION=80 for Electron 9 (Samuel Attard) [#30052](https://github.com/nodejs/node/pull/30052)
* [[`621eaf9ed5`](https://github.com/nodejs/node/commit/621eaf9ed5)] - **doc**: remove "it is important to" phrasing (Rich Trott) [#30108](https://github.com/nodejs/node/pull/30108)
* [[`9a71091098`](https://github.com/nodejs/node/commit/9a71091098)] - **doc**: revise os.md (Rich Trott) [#30102](https://github.com/nodejs/node/pull/30102)
* [[`381c6cd0d2`](https://github.com/nodejs/node/commit/381c6cd0d2)] - **doc**: delete "a number of" things in the docs (Rich Trott) [#30103](https://github.com/nodejs/node/pull/30103)
* [[`45c70a9793`](https://github.com/nodejs/node/commit/45c70a9793)] - **doc**: remove dashes (Rich Trott) [#30101](https://github.com/nodejs/node/pull/30101)
* [[`ea9d125536`](https://github.com/nodejs/node/commit/ea9d125536)] - **doc**: add legendecas to collaborators (legendecas) [#30115](https://github.com/nodejs/node/pull/30115)
* [[`39070bbed0`](https://github.com/nodejs/node/commit/39070bbed0)] - **doc**: make YAML matter consistent in crypto.md (Rich Trott) [#30016](https://github.com/nodejs/node/pull/30016)
* [[`978946e38b`](https://github.com/nodejs/node/commit/978946e38b)] - **doc,meta**: prefer aliases and stubs over Runtime Deprecations (Rich Trott) [#30153](https://github.com/nodejs/node/pull/30153)
* [[`32a538901f`](https://github.com/nodejs/node/commit/32a538901f)] - **doc,n-api**: sort bottom-of-the-page references (Gabriel Schulhof) [#30124](https://github.com/nodejs/node/pull/30124)
* [[`07b5584a3f`](https://github.com/nodejs/node/commit/07b5584a3f)] - **(SEMVER-MINOR)** **fs**: add `bufferSize` option to `fs.opendir()` (Anna Henningsen) [#30114](https://github.com/nodejs/node/pull/30114)
* [[`2505f678ef`](https://github.com/nodejs/node/commit/2505f678ef)] - **http**: support readable hwm in IncomingMessage (Colin Ihrig) [#30135](https://github.com/nodejs/node/pull/30135)
* [[`f01c5c51b0`](https://github.com/nodejs/node/commit/f01c5c51b0)] - **inspector**: turn platform tasks that outlive Agent into no-ops (Anna Henningsen) [#30031](https://github.com/nodejs/node/pull/30031)
* [[`050efebf24`](https://github.com/nodejs/node/commit/050efebf24)] - **meta**: use contact\_links instead of issue templates (Michaël Zasso) [#30172](https://github.com/nodejs/node/pull/30172)
* [[`edfbee3727`](https://github.com/nodejs/node/commit/edfbee3727)] - **module**: resolve self-references (Jan Krems) [#29327](https://github.com/nodejs/node/pull/29327)
* [[`93b1bb8cb5`](https://github.com/nodejs/node/commit/93b1bb8cb5)] - **n-api,doc**: add info about building n-api addons (Jim Schlight) [#30032](https://github.com/nodejs/node/pull/30032)
* [[`cc1cd2b3c5`](https://github.com/nodejs/node/commit/cc1cd2b3c5)] - **src**: isolate-\>Dispose() order consistency (Shelley Vohr) [#30181](https://github.com/nodejs/node/pull/30181)
* [[`a0df91cce1`](https://github.com/nodejs/node/commit/a0df91cce1)] - **(SEMVER-MINOR)** **src**: expose granular SetIsolateUpForNode (Shelley Vohr) [#30150](https://github.com/nodejs/node/pull/30150)
* [[`ec7b69ff05`](https://github.com/nodejs/node/commit/ec7b69ff05)] - **src**: change env.h includes for forward declarations (Alexandre Ferrando) [#30133](https://github.com/nodejs/node/pull/30133)
* [[`98c8f76dd1`](https://github.com/nodejs/node/commit/98c8f76dd1)] - **src**: split up InitializeContext (Shelley Vohr) [#30067](https://github.com/nodejs/node/pull/30067)
* [[`d78e3176dd`](https://github.com/nodejs/node/commit/d78e3176dd)] - **src**: fix crash with SyntheticModule#setExport (Michaël Zasso) [#30062](https://github.com/nodejs/node/pull/30062)
* [[`fd0aded233`](https://github.com/nodejs/node/commit/fd0aded233)] - **src**: allow inspector without v8 platform (Shelley Vohr) [#30049](https://github.com/nodejs/node/pull/30049)
* [[`87f14e13b3`](https://github.com/nodejs/node/commit/87f14e13b3)] - **stream**: extract Readable.from in its own file (Matteo Collina) [#30140](https://github.com/nodejs/node/pull/30140)
* [[`1d9f4278dd`](https://github.com/nodejs/node/commit/1d9f4278dd)] - **test**: use arrow functions for callbacks (Minuk Park) [#30069](https://github.com/nodejs/node/pull/30069)
* [[`a03809d7dd`](https://github.com/nodejs/node/commit/a03809d7dd)] - **test**: verify npm compatibility with releases (Michaël Zasso) [#30082](https://github.com/nodejs/node/pull/30082)
* [[`68e4b5a1fc`](https://github.com/nodejs/node/commit/68e4b5a1fc)] - **tools**: fix Python 3 deprecation warning in test.py (Loris Zinsou) [#30208](https://github.com/nodejs/node/pull/30208)
* [[`348ec693ac`](https://github.com/nodejs/node/commit/348ec693ac)] - **tools**: fix Python 3 syntax error in mac\_tool.py (Christian Clauss) [#30146](https://github.com/nodejs/node/pull/30146)
* [[`e2fb353df3`](https://github.com/nodejs/node/commit/e2fb353df3)] - **tools**: use print() function in buildbot\_run.py (Christian Clauss) [#30148](https://github.com/nodejs/node/pull/30148)
* [[`bcbcce5983`](https://github.com/nodejs/node/commit/bcbcce5983)] - **tools**: undefined name opts -\> args in gyptest.py (Christian Clauss) [#30144](https://github.com/nodejs/node/pull/30144)
* [[`14981f5bba`](https://github.com/nodejs/node/commit/14981f5bba)] - **tools**: git rm -r tools/v8\_gypfiles/broken (Christian Clauss) [#30149](https://github.com/nodejs/node/pull/30149)
* [[`d549a34597`](https://github.com/nodejs/node/commit/d549a34597)] - **tools**: update ESLint to 6.6.0 (Colin Ihrig) [#30123](https://github.com/nodejs/node/pull/30123)
* [[`a3757546e8`](https://github.com/nodejs/node/commit/a3757546e8)] - **tools**: doc: improve async workflow of generate.js (Theotime Poisseau) [#30106](https://github.com/nodejs/node/pull/30106)

<a id="13.0.1"></a>
## 2019-10-23, Version 13.0.1 (Current), @targos

### Notable Changes

* **deps**:
  * Fixed a bug in npm 6.12.0 where warnings are emitted on Node.js 13.x (Jordan Harband) [#30079](https://github.com/nodejs/node/pull/30079).
* **esm**:
  * Changed file extension resolution order of `--es-module-specifier-resolution=node`
    to match that of the CommonJS loader (Myles Borins) [#29974](https://github.com/nodejs/node/pull/29974).

### Commits

* [[`19a983c615`](https://github.com/nodejs/node/commit/19a983c615)] - **build**: make linter failures fail `test-doc` target (Richard Lau) [#30012](https://github.com/nodejs/node/pull/30012)
* [[`13f3d6c680`](https://github.com/nodejs/node/commit/13f3d6c680)] - **build**: log the found compiler version if too old (Richard Lau) [#30028](https://github.com/nodejs/node/pull/30028)
* [[`a25d2fcf8b`](https://github.com/nodejs/node/commit/a25d2fcf8b)] - **build**: make configure --without-snapshot a no-op (Michaël Zasso) [#30021](https://github.com/nodejs/node/pull/30021)
* [[`e04d0584a5`](https://github.com/nodejs/node/commit/e04d0584a5)] - **build**: default Windows build to Visual Studio 2019 (Michaël Zasso) [#30022](https://github.com/nodejs/node/pull/30022)
* [[`ccf58835c7`](https://github.com/nodejs/node/commit/ccf58835c7)] - **build**: use python3 to build and test on Travis (Christian Clauss) [#29451](https://github.com/nodejs/node/pull/29451)
* [[`b92afcd90c`](https://github.com/nodejs/node/commit/b92afcd90c)] - **build**: fix version checks in configure.py (Michaël Zasso) [#29965](https://github.com/nodejs/node/pull/29965)
* [[`2dc4da0d8b`](https://github.com/nodejs/node/commit/2dc4da0d8b)] - **build**: build benchmark addons like test addons (Richard Lau) [#29995](https://github.com/nodejs/node/pull/29995)
* [[`2f36976594`](https://github.com/nodejs/node/commit/2f36976594)] - **deps**: npm: patch support for 13.x (Jordan Harband) [#30079](https://github.com/nodejs/node/pull/30079)
* [[`9d332ab4ce`](https://github.com/nodejs/node/commit/9d332ab4ce)] - **deps**: upgrade to libuv 1.33.1 (Colin Ihrig) [#29996](https://github.com/nodejs/node/pull/29996)
* [[`89b9115c4d`](https://github.com/nodejs/node/commit/89b9115c4d)] - **doc**: --enable-source-maps and prepareStackTrace are incompatible (Benjamin Coe) [#30046](https://github.com/nodejs/node/pull/30046)
* [[`35bffcdd9d`](https://github.com/nodejs/node/commit/35bffcdd9d)] - **doc**: join parts of disrupt section in cli.md (vsemozhetbyt) [#30038](https://github.com/nodejs/node/pull/30038)
* [[`0299767508`](https://github.com/nodejs/node/commit/0299767508)] - **doc**: update collaborator email address (Minwoo Jung) [#30007](https://github.com/nodejs/node/pull/30007)
* [[`ff4f2999e6`](https://github.com/nodejs/node/commit/ff4f2999e6)] - **doc**: fix tls version typo (akitsu-sanae) [#29984](https://github.com/nodejs/node/pull/29984)
* [[`62b4ca6e32`](https://github.com/nodejs/node/commit/62b4ca6e32)] - **doc**: clarify readable.unshift null/EOF (Robert Nagy) [#29950](https://github.com/nodejs/node/pull/29950)
* [[`dc83ff9056`](https://github.com/nodejs/node/commit/dc83ff9056)] - **doc**: remove unused Markdown reference links (Nick Schonning) [#29961](https://github.com/nodejs/node/pull/29961)
* [[`d80ece68ac`](https://github.com/nodejs/node/commit/d80ece68ac)] - **doc**: re-enable passing remark-lint rule (Nick Schonning) [#29961](https://github.com/nodejs/node/pull/29961)
* [[`828e171107`](https://github.com/nodejs/node/commit/828e171107)] - **doc**: add server header into the discarded list of http message.headers (Huachao Mao) [#29962](https://github.com/nodejs/node/pull/29962)
* [[`9729c5da8a`](https://github.com/nodejs/node/commit/9729c5da8a)] - **esm**: modify resolution order for specifier flag (Myles Borins) [#29974](https://github.com/nodejs/node/pull/29974)
* [[`cfd45ebf94`](https://github.com/nodejs/node/commit/cfd45ebf94)] - **module**: refactor modules bootstrap (Bradley Farias) [#29937](https://github.com/nodejs/node/pull/29937)
* [[`d561321e4a`](https://github.com/nodejs/node/commit/d561321e4a)] - **src**: remove unnecessary std::endl usage (Daniel Bevenius) [#30003](https://github.com/nodejs/node/pull/30003)
* [[`ed80c233cd`](https://github.com/nodejs/node/commit/ed80c233cd)] - **src**: make implementing CancelPendingDelayedTasks for platform optional (Anna Henningsen) [#30034](https://github.com/nodejs/node/pull/30034)
* [[`8fcc039de9`](https://github.com/nodejs/node/commit/8fcc039de9)] - **src**: expose ListNode\<T\>::prev\_ on postmortem metadata (legendecas) [#30027](https://github.com/nodejs/node/pull/30027)
* [[`0c88dc1932`](https://github.com/nodejs/node/commit/0c88dc1932)] - **src**: fewer uses of NODE\_USE\_V8\_PLATFORM (Shelley Vohr) [#30029](https://github.com/nodejs/node/pull/30029)
* [[`972144073b`](https://github.com/nodejs/node/commit/972144073b)] - **src**: remove unused iomanip include (Daniel Bevenius) [#30004](https://github.com/nodejs/node/pull/30004)
* [[`b019ccd59d`](https://github.com/nodejs/node/commit/b019ccd59d)] - **src**: initialize openssl only once (Sam Roberts) [#29999](https://github.com/nodejs/node/pull/29999)
* [[`3eae670470`](https://github.com/nodejs/node/commit/3eae670470)] - **src**: refine maps parsing for large pages (Gabriel Schulhof) [#29973](https://github.com/nodejs/node/pull/29973)
* [[`f3712dfe83`](https://github.com/nodejs/node/commit/f3712dfe83)] - **stream**: simplify uint8ArrayToBuffer helper (Luigi Pinca) [#30041](https://github.com/nodejs/node/pull/30041)
* [[`46aa4810ad`](https://github.com/nodejs/node/commit/46aa4810ad)] - **stream**: remove dead code (Luigi Pinca) [#30041](https://github.com/nodejs/node/pull/30041)
* [[`f155dfeecb`](https://github.com/nodejs/node/commit/f155dfeecb)] - **test**: expand Worker test for non-shared ArrayBuffer (Anna Henningsen) [#30044](https://github.com/nodejs/node/pull/30044)
* [[`e110d81b17`](https://github.com/nodejs/node/commit/e110d81b17)] - **test**: fix test runner for Python 3 on Windows (Michaël Zasso) [#30023](https://github.com/nodejs/node/pull/30023)
* [[`c096f251e4`](https://github.com/nodejs/node/commit/c096f251e4)] - **test**: remove common.skipIfInspectorEnabled() (Rich Trott) [#29993](https://github.com/nodejs/node/pull/29993)
* [[`b1b8663a23`](https://github.com/nodejs/node/commit/b1b8663a23)] - **test**: add cb error test for fs.close() (Matteo Rossi) [#29970](https://github.com/nodejs/node/pull/29970)

<a id="13.0.0"></a>
## 2019-10-22, Version 13.0.0 (Current), @BethGriggs

### Notable Changes

* **assert**:
  * If the validation function passed to `assert.throws()` or `assert.rejects()`
    returns a value other than `true`, an assertion error will be thrown instead
    of the original error to highlight the programming mistake (Ruben Bridgewater) [#28263](https://github.com/nodejs/node/pull/28263).
  * If a constructor function is passed to validate the instance of errors
    thrown in `assert.throws()` or `assert.reject()`, an assertion error will be
    thrown instead of the original error (Ruben Bridgewater) [#28263](https://github.com/nodejs/node/pull/28263).
* **build**:
  * Node.js releases are now built with default full-icu support. This means
    that all locales supported by ICU are now included and Intl-related APIs may
    return different values than before (Richard Lau) [#29887](https://github.com/nodejs/node/pull/29887).
  * The minimum Xcode version supported for macOS was increased to 10. It is
    still possible to build Node.js with Xcode 8 but this may no longer be the
    case in a future v13.x release (Michael Dawson) [#29622](https://github.com/nodejs/node/pull/29622).
* **child_process**:
  * `ChildProcess._channel` (DEP0129) is now a Runtime deprecation (cjihrig) [#27949](https://github.com/nodejs/node/pull/27949).
* **console**:
  * The output `console.timeEnd()` and `console.timeLog()` will now
    automatically select a suitable time unit instead of always using
    milliseconds (Xavier Stouder) [#29251](https://github.com/nodejs/node/pull/29251).
* **deps**:
  * The V8 engine was updated to version 7.8. This includes performance
    improvements to object destructuring, memory usage and WebAssembly startup
    time (Myles Borins) [#29694](https://github.com/nodejs/node/pull/29694).
* **domain**:
  * The domain's error handler is now executed with the active domain set to the
    domain's parent to prevent inner recursion (Julien Gilli) [#26211](https://github.com/nodejs/node/pull/26211).
* **fs**:
  * The undocumented method `FSWatcher.prototype.start()` was removed (Lucas Holmquist) [#29905](https://github.com/nodejs/node/pull/29905).
  * Calling the `open()` method on a `ReadStream` or `WriteStream` now emits a
    runtime deprecation warning. The methods are supposed to be internal and
    should not be called by user code (Robert Nagy) [#29061](https://github.com/nodejs/node/pull/29061).
  * `fs.read/write`, `fs.readSync/writeSync` and `fd.read/write` now accept any
    safe integer as their `offset` parameter. The value of `offset` is also no
    longer coerced, so a valid type must be passed to the functions (Zach Bjornson) [#26572](https://github.com/nodejs/node/pull/26572).
* **http**:
  * Aborted requests no longer emit the `end` or `error` events after `aborted`
    (Robert Nagy) [#27984](https://github.com/nodejs/node/pull/27984), [#20077](https://github.com/nodejs/node/pull/20077).
  * Data will no longer be emitted after a socket error (Robert Nagy) [#28711](https://github.com/nodejs/node/pull/28711).
  * The legacy HTTP parser (previously available under the
    `--http-parser=legacy` flag) was removed (Anna Henningsen) [#29589](https://github.com/nodejs/node/pull/29589).
  * The `host` option for HTTP requests is now validated to be a string value (Giorgos Ntemiris) [#29568](https://github.com/nodejs/node/pull/29568).
  * The `request.connection` and `response.connection` properties are now
    runtime deprecated. The equivalent `request.socket` and `response.socket`
    should be used instead (Robert Nagy) [#29015](https://github.com/nodejs/node/pull/29015).
* **http, http2**:
  * The default server timeout was removed (Ali Ijaz Sheikh) [#27558](https://github.com/nodejs/node/pull/27558).
  * Brought 425 status code name into accordance with RFC 8470. The name changed
    from "Unordered Collection" to "Too Early" (Sergei Osipov) [#29880](https://github.com/nodejs/node/pull/29880).
* **lib**:
  * The `error.errno` property will now always be a number. To get the string
    value, use `error.code` instead (Joyee Cheung) [#28140](https://github.com/nodejs/node/pull/28140).
* **module**:
  * `module.createRequireFromPath()` is deprecated. Use `module.createRequire()`
    instead (cjihrig) [#27951](https://github.com/nodejs/node/pull/27951).
* **src**:
  * Changing the value of `process.env.TZ` will now clear the tz cache. This
    affects the default time zone used by methods such as
    `Date.prototype.toString` (Ben Noordhuis) [#20026](https://github.com/nodejs/node/pull/20026).
* **stream**:
  * The timing and behavior of streams was consolidated for a number of edge
    cases. Please look at the individual commits below for more information.

### Semver-Major Commits

* [[`5981fb7faa`](https://github.com/nodejs/node/commit/5981fb7faa)] - **(SEMVER-MAJOR)** **assert**: fix line number calculation after V8 upgrade (Michaël Zasso) [#29694](https://github.com/nodejs/node/pull/29694)
* [[`48d1ea5e7f`](https://github.com/nodejs/node/commit/48d1ea5e7f)] - **(SEMVER-MAJOR)** **assert**: special handle identical error names in instance checks (Ruben Bridgewater) [#28263](https://github.com/nodejs/node/pull/28263)
* [[`97c52ca5dc`](https://github.com/nodejs/node/commit/97c52ca5dc)] - **(SEMVER-MAJOR)** **assert**: add more information to AssertionErrors (Ruben Bridgewater) [#28263](https://github.com/nodejs/node/pull/28263)
* [[`5700cd17dd`](https://github.com/nodejs/node/commit/5700cd17dd)] - **(SEMVER-MAJOR)** **assert**: do not repeat .throws() code (Ruben Bridgewater) [#28263](https://github.com/nodejs/node/pull/28263)
* [[`d47b6786c9`](https://github.com/nodejs/node/commit/d47b6786c9)] - **(SEMVER-MAJOR)** **assert**: wrap validation function errors (Ruben Bridgewater) [#28263](https://github.com/nodejs/node/pull/28263)
* [[`0b3242c3ce`](https://github.com/nodejs/node/commit/0b3242c3ce)] - **(SEMVER-MAJOR)** **assert**: fix generatedMessage property (Ruben Bridgewater) [#28263](https://github.com/nodejs/node/pull/28263)
* [[`ace3f16917`](https://github.com/nodejs/node/commit/ace3f16917)] - **(SEMVER-MAJOR)** **assert**: improve class instance errors (Ruben Bridgewater) [#28263](https://github.com/nodejs/node/pull/28263)
* [[`0376b5b7ba`](https://github.com/nodejs/node/commit/0376b5b7ba)] - **(SEMVER-MAJOR)** **benchmark**: use test/common/tmpdir consistently (João Reis) [#28858](https://github.com/nodejs/node/pull/28858)
* [[`4885e50f7e`](https://github.com/nodejs/node/commit/4885e50f7e)] - **(SEMVER-MAJOR)** **build**: make full-icu the default for releases (Richard Lau) [#29887](https://github.com/nodejs/node/pull/29887)
* [[`60a3bd93ce`](https://github.com/nodejs/node/commit/60a3bd93ce)] - **(SEMVER-MAJOR)** **build**: reset embedder string to "-node.0" (Myles Borins) [#29694](https://github.com/nodejs/node/pull/29694)
* [[`9f830f37da`](https://github.com/nodejs/node/commit/9f830f37da)] - **(SEMVER-MAJOR)** **build**: update minimum Xcode version for macOS (Michael Dawson) [#29622](https://github.com/nodejs/node/pull/29622)
* [[`66eaeac1df`](https://github.com/nodejs/node/commit/66eaeac1df)] - **(SEMVER-MAJOR)** **build**: reset embedder string to "-node.0" (Michaël Zasso) [#28016](https://github.com/nodejs/node/pull/28016)
* [[`d05668d688`](https://github.com/nodejs/node/commit/d05668d688)] - **(SEMVER-MAJOR)** **child_process**: runtime deprecate \_channel (cjihrig) [#27949](https://github.com/nodejs/node/pull/27949)
* [[`4f9cd2770a`](https://github.com/nodejs/node/commit/4f9cd2770a)] - **(SEMVER-MAJOR)** **child_process**: simplify spawn argument parsing (cjihrig) [#27854](https://github.com/nodejs/node/pull/27854)
* [[`66043e1812`](https://github.com/nodejs/node/commit/66043e1812)] - **(SEMVER-MAJOR)** **console**: display timeEnd with suitable time unit (Xavier Stouder) [#29251](https://github.com/nodejs/node/pull/29251)
* [[`80f2b67367`](https://github.com/nodejs/node/commit/80f2b67367)] - **(SEMVER-MAJOR)** **deps**: patch V8 to 7.8.279.14 (Myles Borins) [#29694](https://github.com/nodejs/node/pull/29694)
* [[`eeafb263f4`](https://github.com/nodejs/node/commit/eeafb263f4)] - **(SEMVER-MAJOR)** **deps**: patch V8 to 7.8.279.12 (Myles Borins) [#29694](https://github.com/nodejs/node/pull/29694)
* [[`ddfc3b0a76`](https://github.com/nodejs/node/commit/ddfc3b0a76)] - **(SEMVER-MAJOR)** **deps**: patch V8 to 7.8.279.10 (Myles Borins) [#29694](https://github.com/nodejs/node/pull/29694)
* [[`8d05991d10`](https://github.com/nodejs/node/commit/8d05991d10)] - **(SEMVER-MAJOR)** **deps**: update V8's postmortem script (cjihrig) [#29694](https://github.com/nodejs/node/pull/29694)
* [[`858602445b`](https://github.com/nodejs/node/commit/858602445b)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick 716875d (Myles Borins) [#29694](https://github.com/nodejs/node/pull/29694)
* [[`f7f6c928c1`](https://github.com/nodejs/node/commit/f7f6c928c1)] - **(SEMVER-MAJOR)** **deps**: update V8 to 7.8.279.9 (Myles Borins) [#29694](https://github.com/nodejs/node/pull/29694)
* [[`84d3243ce9`](https://github.com/nodejs/node/commit/84d3243ce9)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick b33af60 (Michaël Zasso) [#28016](https://github.com/nodejs/node/pull/28016)
* [[`2dcc3665ab`](https://github.com/nodejs/node/commit/2dcc3665ab)] - **(SEMVER-MAJOR)** **deps**: update V8 to 7.6.303.28 (Michaël Zasso) [#28016](https://github.com/nodejs/node/pull/28016)
* [[`eef1b5aa0f`](https://github.com/nodejs/node/commit/eef1b5aa0f)] - **(SEMVER-MAJOR)** **doc**: make `AssertionError` a link (Ruben Bridgewater) [#28263](https://github.com/nodejs/node/pull/28263)
* [[`8fd7184959`](https://github.com/nodejs/node/commit/8fd7184959)] - **(SEMVER-MAJOR)** **doc**: update assert.throws() examples (Ruben Bridgewater) [#28263](https://github.com/nodejs/node/pull/28263)
* [[`80d9b1c712`](https://github.com/nodejs/node/commit/80d9b1c712)] - **(SEMVER-MAJOR)** **doc**: wrap long line (cjihrig) [#27951](https://github.com/nodejs/node/pull/27951)
* [[`43a5170858`](https://github.com/nodejs/node/commit/43a5170858)] - **(SEMVER-MAJOR)** **domain**: error handler runs outside of its domain (Julien Gilli) [#26211](https://github.com/nodejs/node/pull/26211)
* [[`7eacb74389`](https://github.com/nodejs/node/commit/7eacb74389)] - **(SEMVER-MAJOR)** **fs**: make FSWatcher.start private (Lucas Holmquist) [#29905](https://github.com/nodejs/node/pull/29905)
* [[`773769df60`](https://github.com/nodejs/node/commit/773769df60)] - **(SEMVER-MAJOR)** **fs**: add runtime deprecate for file stream open() (Robert Nagy) [#29061](https://github.com/nodejs/node/pull/29061)
* [[`5e3b4d6ed9`](https://github.com/nodejs/node/commit/5e3b4d6ed9)] - **(SEMVER-MAJOR)** **fs**: allow int64 offset in fs.write/writeSync/fd.write (Zach Bjornson) [#26572](https://github.com/nodejs/node/pull/26572)
* [[`a3c0014e73`](https://github.com/nodejs/node/commit/a3c0014e73)] - **(SEMVER-MAJOR)** **fs**: use IsSafeJsInt instead of IsNumber for ftruncate (Zach Bjornson) [#26572](https://github.com/nodejs/node/pull/26572)
* [[`0bbda5e5ae`](https://github.com/nodejs/node/commit/0bbda5e5ae)] - **(SEMVER-MAJOR)** **fs**: allow int64 offset in fs.read/readSync/fd.read (Zach Bjornson) [#26572](https://github.com/nodejs/node/pull/26572)
* [[`eadc3850fe`](https://github.com/nodejs/node/commit/eadc3850fe)] - **(SEMVER-MAJOR)** **fs**: close file descriptor of promisified truncate (João Reis) [#28858](https://github.com/nodejs/node/pull/28858)
* [[`5f80df8820`](https://github.com/nodejs/node/commit/5f80df8820)] - **(SEMVER-MAJOR)** **http**: do not emit end after aborted (Robert Nagy) [#27984](https://github.com/nodejs/node/pull/27984)
* [[`e573c39b88`](https://github.com/nodejs/node/commit/e573c39b88)] - **(SEMVER-MAJOR)** **http**: don't emit 'data' after 'error' (Robert Nagy) [#28711](https://github.com/nodejs/node/pull/28711)
* [[`ac59dc42ed`](https://github.com/nodejs/node/commit/ac59dc42ed)] - **(SEMVER-MAJOR)** **http**: remove legacy parser (Anna Henningsen) [#29589](https://github.com/nodejs/node/pull/29589)
* [[`2daf883a18`](https://github.com/nodejs/node/commit/2daf883a18)] - **(SEMVER-MAJOR)** **http**: throw if 'host' agent header is not a string value (Giorgos Ntemiris) [#29568](https://github.com/nodejs/node/pull/29568)
* [[`0daec61b9b`](https://github.com/nodejs/node/commit/0daec61b9b)] - **(SEMVER-MAJOR)** **http**: replace superfluous connection property with getter/setter (Robert Nagy) [#29015](https://github.com/nodejs/node/pull/29015)
* [[`461bf36d70`](https://github.com/nodejs/node/commit/461bf36d70)] - **(SEMVER-MAJOR)** **http**: fix test where aborted should not be emitted (Robert Nagy) [#20077](https://github.com/nodejs/node/pull/20077)
* [[`d5577f0395`](https://github.com/nodejs/node/commit/d5577f0395)] - **(SEMVER-MAJOR)** **http**: remove default 'timeout' listener on upgrade (Luigi Pinca) [#26030](https://github.com/nodejs/node/pull/26030)
* [[`c30ef3cbd2`](https://github.com/nodejs/node/commit/c30ef3cbd2)] - **(SEMVER-MAJOR)** **http, http2**: remove default server timeout (Ali Ijaz Sheikh) [#27558](https://github.com/nodejs/node/pull/27558)
* [[`4e782c9deb`](https://github.com/nodejs/node/commit/4e782c9deb)] - **(SEMVER-MAJOR)** **http2**: remove security revert flags (Anna Henningsen) [#29141](https://github.com/nodejs/node/pull/29141)
* [[`41637a530e`](https://github.com/nodejs/node/commit/41637a530e)] - **(SEMVER-MAJOR)** **http2**: remove callback-based padding (Anna Henningsen) [#29144](https://github.com/nodejs/node/pull/29144)
* [[`91a4cb7175`](https://github.com/nodejs/node/commit/91a4cb7175)] - **(SEMVER-MAJOR)** **lib**: rename validateInteger to validateSafeInteger (Zach Bjornson) [#26572](https://github.com/nodejs/node/pull/26572)
* [[`1432065e9d`](https://github.com/nodejs/node/commit/1432065e9d)] - **(SEMVER-MAJOR)** **lib**: correct error.errno to always be numeric (Joyee Cheung) [#28140](https://github.com/nodejs/node/pull/28140)
* [[`702331be90`](https://github.com/nodejs/node/commit/702331be90)] - **(SEMVER-MAJOR)** **lib**: no need to strip BOM or shebang for scripts (Refael Ackermann) [#27375](https://github.com/nodejs/node/pull/27375)
* [[`e2c0c0c680`](https://github.com/nodejs/node/commit/e2c0c0c680)] - **(SEMVER-MAJOR)** **lib**: rework logic of stripping BOM+Shebang from commonjs (Gus Caplan) [#27768](https://github.com/nodejs/node/pull/27768)
* [[`14701e539c`](https://github.com/nodejs/node/commit/14701e539c)] - **(SEMVER-MAJOR)** **module**: runtime deprecate createRequireFromPath() (cjihrig) [#27951](https://github.com/nodejs/node/pull/27951)
* [[`04633eeeb9`](https://github.com/nodejs/node/commit/04633eeeb9)] - **(SEMVER-MAJOR)** **readline**: error on falsy values for callback (Sam Roberts) [#28109](https://github.com/nodejs/node/pull/28109)
* [[`3eea43af07`](https://github.com/nodejs/node/commit/3eea43af07)] - **(SEMVER-MAJOR)** **repl**: close file descriptor of history file (João Reis) [#28858](https://github.com/nodejs/node/pull/28858)
* [[`458a38c904`](https://github.com/nodejs/node/commit/458a38c904)] - **(SEMVER-MAJOR)** **src**: bring 425 status code name into accordance with RFC 8470 (Sergei Osipov) [#29880](https://github.com/nodejs/node/pull/29880)
* [[`7fcc1f7047`](https://github.com/nodejs/node/commit/7fcc1f7047)] - **(SEMVER-MAJOR)** **src**: update NODE\_MODULE\_VERSION to 79 (Myles Borins) [#29694](https://github.com/nodejs/node/pull/29694)
* [[`4b7be335b9`](https://github.com/nodejs/node/commit/4b7be335b9)] - **(SEMVER-MAJOR)** **src**: update NODE\_MODULE\_VERSION to 78 (Michaël Zasso) [#28918](https://github.com/nodejs/node/pull/28918)
* [[`a0e2c6d284`](https://github.com/nodejs/node/commit/a0e2c6d284)] - **(SEMVER-MAJOR)** **src**: add error codes to errors thrown in C++ (Yaniv Friedensohn) [#27700](https://github.com/nodejs/node/pull/27700)
* [[`94e980c9d3`](https://github.com/nodejs/node/commit/94e980c9d3)] - **(SEMVER-MAJOR)** **src**: use non-deprecated overload of V8::SetFlagsFromString (Michaël Zasso) [#28016](https://github.com/nodejs/node/pull/28016)
* [[`655e0dc01a`](https://github.com/nodejs/node/commit/655e0dc01a)] - **(SEMVER-MAJOR)** **src**: update NODE\_MODULE\_VERSION to 77 (Michaël Zasso) [#28016](https://github.com/nodejs/node/pull/28016)
* [[`e3cd79ef8e`](https://github.com/nodejs/node/commit/e3cd79ef8e)] - **(SEMVER-MAJOR)** **src**: update NODE\_MODULE\_VERSION to 74 (Refael Ackermann) [#27375](https://github.com/nodejs/node/pull/27375)
* [[`eba348b6ae`](https://github.com/nodejs/node/commit/eba348b6ae)] - **(SEMVER-MAJOR)** **src**: make process.env.TZ setter clear tz cache (Ben Noordhuis) [#20026](https://github.com/nodejs/node/pull/20026)
* [[`f2061930c8`](https://github.com/nodejs/node/commit/f2061930c8)] - **(SEMVER-MAJOR)** **src**: enable V8's WASM trap handlers (Gus Caplan) [#27246](https://github.com/nodejs/node/pull/27246)
* [[`f8f6a21580`](https://github.com/nodejs/node/commit/f8f6a21580)] - **(SEMVER-MAJOR)** **stream**: throw unhandled error for readable with autoDestroy (Robert Nagy) [#29806](https://github.com/nodejs/node/pull/29806)
* [[`f663b31cc2`](https://github.com/nodejs/node/commit/f663b31cc2)] - **(SEMVER-MAJOR)** **stream**: always invoke callback before emitting error (Robert Nagy) [#29293](https://github.com/nodejs/node/pull/29293)
* [[`aa32e13968`](https://github.com/nodejs/node/commit/aa32e13968)] - **(SEMVER-MAJOR)** **stream**: do not flush destroyed writable (Robert Nagy) [#29028](https://github.com/nodejs/node/pull/29028)
* [[`ba3be578d8`](https://github.com/nodejs/node/commit/ba3be578d8)] - **(SEMVER-MAJOR)** **stream**: don't emit finish on error (Robert Nagy) [#28979](https://github.com/nodejs/node/pull/28979)
* [[`db706da235`](https://github.com/nodejs/node/commit/db706da235)] - **(SEMVER-MAJOR)** **stream**: disallow stream methods on finished stream (Robert Nagy) [#28687](https://github.com/nodejs/node/pull/28687)
* [[`188896ea3e`](https://github.com/nodejs/node/commit/188896ea3e)] - **(SEMVER-MAJOR)** **stream**: do not emit after 'error' (Robert Nagy) [#28708](https://github.com/nodejs/node/pull/28708)
* [[`4a2bd69db9`](https://github.com/nodejs/node/commit/4a2bd69db9)] - **(SEMVER-MAJOR)** **stream**: fix destroy() behavior (Robert Nagy) [#29058](https://github.com/nodejs/node/pull/29058)
* [[`824dc576db`](https://github.com/nodejs/node/commit/824dc576db)] - **(SEMVER-MAJOR)** **stream**: simplify `.pipe()` and `.unpipe()` in Readable (Weijia Wang) [#28583](https://github.com/nodejs/node/pull/28583)
* [[`8ef68e66d0`](https://github.com/nodejs/node/commit/8ef68e66d0)] - **(SEMVER-MAJOR)** **test**: clean tmpdir on process exit (João Reis) [#28858](https://github.com/nodejs/node/pull/28858)
* [[`d3f20a4725`](https://github.com/nodejs/node/commit/d3f20a4725)] - **(SEMVER-MAJOR)** **test**: use unique tmpdirs for each test (João Reis) [#28858](https://github.com/nodejs/node/pull/28858)
* [[`174723354e`](https://github.com/nodejs/node/commit/174723354e)] - **(SEMVER-MAJOR)** **tools**: patch V8 to run on older XCode versions (Ujjwal Sharma) [#29694](https://github.com/nodejs/node/pull/29694)
* [[`1676502318`](https://github.com/nodejs/node/commit/1676502318)] - **(SEMVER-MAJOR)** **tools**: update V8 gypfiles (Michaël Zasso) [#29694](https://github.com/nodejs/node/pull/29694)
* [[`1a25e901b7`](https://github.com/nodejs/node/commit/1a25e901b7)] - **(SEMVER-MAJOR)** **tools**: support full-icu by default (Steven R. Loomis) [#29522](https://github.com/nodejs/node/pull/29522)
* [[`2664dacf7e`](https://github.com/nodejs/node/commit/2664dacf7e)] - **(SEMVER-MAJOR)** **util**: validate formatWithOptions inspectOptions (Ruben Bridgewater) [#29824](https://github.com/nodejs/node/pull/29824)

### Semver-Minor Commits

* [[`8915b15f8c`](https://github.com/nodejs/node/commit/8915b15f8c)] - **(SEMVER-MINOR)** **http**: add reusedSocket property on client request (themez) [#29715](https://github.com/nodejs/node/pull/29715)
* [[`6afed1dc85`](https://github.com/nodejs/node/commit/6afed1dc85)] - **(SEMVER-MINOR)** **n-api**: add `napi\_detach\_arraybuffer` (legendecas) [#29768](https://github.com/nodejs/node/pull/29768)
* [[`c0305af2c4`](https://github.com/nodejs/node/commit/c0305af2c4)] - **(SEMVER-MINOR)** **repl**: check for NODE\_REPL\_EXTERNAL\_MODULE (Gus Caplan) [#29778](https://github.com/nodejs/node/pull/29778)

### Semver-Patch Commits

* [[`e6c389cb3c`](https://github.com/nodejs/node/commit/e6c389cb3c)] - **benchmark**: remove double word "then" in comments (Nick Schonning) [#29823](https://github.com/nodejs/node/pull/29823)
* [[`1294c7e485`](https://github.com/nodejs/node/commit/1294c7e485)] - **benchmark**: add benchmark for vm.createContext (Joyee Cheung) [#29845](https://github.com/nodejs/node/pull/29845)
* [[`6f814013f4`](https://github.com/nodejs/node/commit/6f814013f4)] - **build**: fix version checks in gyp files (Ben Noordhuis) [#29931](https://github.com/nodejs/node/pull/29931)
* [[`6c205aba00`](https://github.com/nodejs/node/commit/6c205aba00)] - **build**: always use strings for compiler version in gyp files (Michaël Zasso) [#29897](https://github.com/nodejs/node/pull/29897)
* [[`be926c7e21`](https://github.com/nodejs/node/commit/be926c7e21)] - **build**: find Python 3 or Python 2 in configure (cclauss) [#25878](https://github.com/nodejs/node/pull/25878)
* [[`16f673ebcc`](https://github.com/nodejs/node/commit/16f673ebcc)] - **build**: re-enable openssl arm for arm64 (Edward Vielmetti) [#28180](https://github.com/nodejs/node/pull/28180)
* [[`204248a0c3`](https://github.com/nodejs/node/commit/204248a0c3)] - **console**: update time formatting (Ruben Bridgewater) [#29629](https://github.com/nodejs/node/pull/29629)
* [[`c64ed10d80`](https://github.com/nodejs/node/commit/c64ed10d80)] - **crypto**: reject public keys properly (Tobias Nießen) [#29913](https://github.com/nodejs/node/pull/29913)
* [[`7de5a55710`](https://github.com/nodejs/node/commit/7de5a55710)] - **deps**: patch V8 to 7.8.279.17 (Michaël Zasso) [#29928](https://github.com/nodejs/node/pull/29928)
* [[`a350d8b780`](https://github.com/nodejs/node/commit/a350d8b780)] - **deps**: V8: cherry-pick 53e62af (Michaël Zasso) [#29898](https://github.com/nodejs/node/pull/29898)
* [[`6b962ddf01`](https://github.com/nodejs/node/commit/6b962ddf01)] - **deps**: patch V8 to 7.8.279.15 (Michaël Zasso) [#29899](https://github.com/nodejs/node/pull/29899)
* [[`efa6bead1d`](https://github.com/nodejs/node/commit/efa6bead1d)] - **doc**: add missing deprecation code (cjihrig) [#29969](https://github.com/nodejs/node/pull/29969)
* [[`c4de76f7a6`](https://github.com/nodejs/node/commit/c4de76f7a6)] - **doc**: update vm.md for link linting (Rich Trott) [#29982](https://github.com/nodejs/node/pull/29982)
* [[`ed5eaa0495`](https://github.com/nodejs/node/commit/ed5eaa0495)] - **doc**: prepare miscellaneous docs for new markdown lint rules (Rich Trott) [#29963](https://github.com/nodejs/node/pull/29963)
* [[`039eb56249`](https://github.com/nodejs/node/commit/039eb56249)] - **doc**: fix some recent nits in fs.md (Vse Mozhet Byt) [#29906](https://github.com/nodejs/node/pull/29906)
* [[`7812a615ab`](https://github.com/nodejs/node/commit/7812a615ab)] - **doc**: fs dir modifications may not be reflected by dir.read (Anna Henningsen) [#29893](https://github.com/nodejs/node/pull/29893)
* [[`37321a9e11`](https://github.com/nodejs/node/commit/37321a9e11)] - **doc**: add missing deprecation number (cjihrig) [#29183](https://github.com/nodejs/node/pull/29183)
* [[`791409a9ce`](https://github.com/nodejs/node/commit/791409a9ce)] - **doc**: fixup changelog for v10.16.3 (Andrew Hughes) [#29159](https://github.com/nodejs/node/pull/29159)
* [[`02b3722b30`](https://github.com/nodejs/node/commit/02b3722b30)] - **doc,meta**: reduce npm PR wait period to one week (Rich Trott) [#29922](https://github.com/nodejs/node/pull/29922)
* [[`fce1a5198a`](https://github.com/nodejs/node/commit/fce1a5198a)] - **domain**: do not import util for a simple type check (Ruben Bridgewater) [#29825](https://github.com/nodejs/node/pull/29825)
* [[`b798f64566`](https://github.com/nodejs/node/commit/b798f64566)] - **esm**: unflag --experimental-exports (Guy Bedford) [#29867](https://github.com/nodejs/node/pull/29867)
* [[`5c93aab278`](https://github.com/nodejs/node/commit/5c93aab278)] - **fs**: buffer dir entries in opendir() (Anna Henningsen) [#29893](https://github.com/nodejs/node/pull/29893)
* [[`624fa4147a`](https://github.com/nodejs/node/commit/624fa4147a)] - **http2**: fix file close error condition at respondWithFd (Anna Henningsen) [#29884](https://github.com/nodejs/node/pull/29884)
* [[`d5c3837061`](https://github.com/nodejs/node/commit/d5c3837061)] - **lib**: remove the comment of base64 validation (Maledong) [#29201](https://github.com/nodejs/node/pull/29201)
* [[`3238232fc4`](https://github.com/nodejs/node/commit/3238232fc4)] - **lib**: rename validateSafeInteger to validateInteger (cjihrig) [#29184](https://github.com/nodejs/node/pull/29184)
* [[`aca1c283bd`](https://github.com/nodejs/node/commit/aca1c283bd)] - **module**: warn on require of .js inside type: module (Guy Bedford) [#29909](https://github.com/nodejs/node/pull/29909)
* [[`1447a79dc4`](https://github.com/nodejs/node/commit/1447a79dc4)] - **net**: treat ENOTCONN at shutdown as success (Anna Henningsen) [#29912](https://github.com/nodejs/node/pull/29912)
* [[`4ca61f40fe`](https://github.com/nodejs/node/commit/4ca61f40fe)] - **process**: add lineLength to source-map-cache (bcoe) [#29863](https://github.com/nodejs/node/pull/29863)
* [[`545f7282d1`](https://github.com/nodejs/node/commit/545f7282d1)] - **src**: implement v8 host weakref hooks (Gus Caplan) [#29874](https://github.com/nodejs/node/pull/29874)
* [[`53ca0b9ae1`](https://github.com/nodejs/node/commit/53ca0b9ae1)] - **src**: render N-API weak callbacks as cleanup hooks (Gabriel Schulhof) [#28428](https://github.com/nodejs/node/pull/28428)
* [[`075c7ebeb5`](https://github.com/nodejs/node/commit/075c7ebeb5)] - **src**: fix largepages regression (Gabriel Schulhof) [#29914](https://github.com/nodejs/node/pull/29914)
* [[`179f4232ed`](https://github.com/nodejs/node/commit/179f4232ed)] - **src**: remove unused using declarations in worker.cc (Daniel Bevenius) [#29883](https://github.com/nodejs/node/pull/29883)
* [[`264cb79bc2`](https://github.com/nodejs/node/commit/264cb79bc2)] - **src**: silence compiler warning node\_process\_methods (Daniel Bevenius) [#28261](https://github.com/nodejs/node/pull/28261)
* [[`89b32378c8`](https://github.com/nodejs/node/commit/89b32378c8)] - **src**: forbid reset\_handler for SIGSEGV handling (Anna Henningsen) [#27775](https://github.com/nodejs/node/pull/27775)
* [[`e256204776`](https://github.com/nodejs/node/commit/e256204776)] - **src**: reset SIGSEGV handler before crashing (Anna Henningsen) [#27775](https://github.com/nodejs/node/pull/27775)
* [[`e6b3ec3d3c`](https://github.com/nodejs/node/commit/e6b3ec3d3c)] - **src**: do not use posix feature macro in node.h (Anna Henningsen) [#27775](https://github.com/nodejs/node/pull/27775)
* [[`6e796581fc`](https://github.com/nodejs/node/commit/6e796581fc)] - **src**: remove freebsd SA\_RESETHAND workaround (Ben Noordhuis) [#27780](https://github.com/nodejs/node/pull/27780)
* [[`8709a408d2`](https://github.com/nodejs/node/commit/8709a408d2)] - **stream**: use more accurate end-of-stream writable and readable detection (Robert Nagy) [#29409](https://github.com/nodejs/node/pull/29409)
* [[`698a29420f`](https://github.com/nodejs/node/commit/698a29420f)] - **stream**: fix readable state `awaitDrain` increase in recursion (ran) [#27572](https://github.com/nodejs/node/pull/27572)
* [[`033037cec9`](https://github.com/nodejs/node/commit/033037cec9)] - **stream**: avoid unecessary nextTick (Robert Nagy) [#29194](https://github.com/nodejs/node/pull/29194)
* [[`f4f856b238`](https://github.com/nodejs/node/commit/f4f856b238)] - **test**: fix flaky doctool and test (Rich Trott) [#29979](https://github.com/nodejs/node/pull/29979)
* [[`7991b57cfd`](https://github.com/nodejs/node/commit/7991b57cfd)] - **test**: fix fs benchmark test (Rich Trott) [#29967](https://github.com/nodejs/node/pull/29967)
* [[`2bb93e1108`](https://github.com/nodejs/node/commit/2bb93e1108)] - **test**: set LC\_ALL to known good value (Ben Noordhuis) [#28096](https://github.com/nodejs/node/pull/28096)
* [[`039cfdc838`](https://github.com/nodejs/node/commit/039cfdc838)] - **test**: add addon tests for `RegisterSignalHandler()` (Anna Henningsen) [#27775](https://github.com/nodejs/node/pull/27775)
* [[`90b5f1b107`](https://github.com/nodejs/node/commit/90b5f1b107)] - **tools**: update remark-preset-lint-node to 1.10.1 (Rich Trott) [#29982](https://github.com/nodejs/node/pull/29982)
* [[`ea3d5ff785`](https://github.com/nodejs/node/commit/ea3d5ff785)] - **tools**: fix test runner in presence of NODE\_REPL\_EXTERNAL\_MODULE (Gus Caplan) [#29956](https://github.com/nodejs/node/pull/29956)
* [[`8728f8660a`](https://github.com/nodejs/node/commit/8728f8660a)] - **tools**: fix GYP MSVS solution generator for Python 3 (Michaël Zasso) [#29897](https://github.com/nodejs/node/pull/29897)
* [[`66b953207d`](https://github.com/nodejs/node/commit/66b953207d)] - **tools**: port Python 3 compat patches from node-gyp to gyp (Michaël Zasso) [#29897](https://github.com/nodejs/node/pull/29897)
* [[`a0c6cf8eb1`](https://github.com/nodejs/node/commit/a0c6cf8eb1)] - **tools**: update remark-preset-lint-node to 1.10.0 (Rich Trott) [#29594](https://github.com/nodejs/node/pull/29594)
* [[`1e01f3f022`](https://github.com/nodejs/node/commit/1e01f3f022)] - **tools**: apply more stringent blank-line linting for markdown files (Rich Trott) [#29447](https://github.com/nodejs/node/pull/29447)
* [[`f9caee986c`](https://github.com/nodejs/node/commit/f9caee986c)] - **vm**: add Synthetic modules (Gus Caplan) [#29864](https://github.com/nodejs/node/pull/29864)
