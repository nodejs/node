# Node.js 14 ChangeLog

<!--lint disable prohibited-strings-->
<!--lint disable maximum-line-length-->

<table>
<tr>
<th>Current</th>
</tr>
<tr>
<td>
<a href="#14.0.0">14.0.0</a><br/>
</td>
</tr>
</table>

* Other Versions
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

<a id="14.0.0"></a>
## 2020-04-21, Version 14.0.0 (Current), @BethGriggs

### Notable Changes

#### Deprecations

* **(SEMVER-MAJOR)** **crypto**: move pbkdf2 without digest to EOL (James M Snell) [#31166](https://github.com/nodejs/node/pull/31166)
* **(SEMVER-MAJOR)** **fs**: deprecate closing FileHandle on garbage collection (James M Snell) [#28396](https://github.com/nodejs/node/pull/28396)
* **(SEMVER-MAJOR)** **http**: move OutboundMessage.prototype.flush to EOL (James M Snell) [#31164](https://github.com/nodejs/node/pull/31164)
* **(SEMVER-MAJOR)** **lib**: move GLOBAL and root aliases to EOL (James M Snell) [#31167](https://github.com/nodejs/node/pull/31167)
* **(SEMVER-MAJOR)** **os**: move tmpDir() to EOL (James M Snell) [#31169](https://github.com/nodejs/node/pull/31169)
* **(SEMVER-MAJOR)** **src**: remove deprecated wasm type check (Clemens Backes) [#32116](https://github.com/nodejs/node/pull/32116)
* **(SEMVER-MAJOR)** **stream**: move \_writableState.buffer to EOL (James M Snell) [#31165](https://github.com/nodejs/node/pull/31165)
* **(SEMVER-MINOR)** **doc**: deprecate process.mainModule (Antoine du HAMEL) [#32232](https://github.com/nodejs/node/pull/32232)
* **(SEMVER-MINOR)** **doc**: deprecate process.umask() with no arguments (Colin Ihrig) [#32499](https://github.com/nodejs/node/pull/32499)

#### ECMAScript Modules - Experimental Warning Removal

* **module**: remove experimental modules warning (Guy Bedford) [#31974](https://github.com/nodejs/node/pull/31974)

In Node.js 13 we removed the need to include the --experimental-modules flag, but when running EcmaScript Modules in Node.js, this would still result in a warning ExperimentalWarning: The ESM module loader is experimental.

As of Node.js 14 there is no longer this warning when using ESM in Node.js. However, the ESM implementation in Node.js remains experimental. As per our stability index: “The feature is not subject to Semantic Versioning rules. Non-backward compatible changes or removal may occur in any future release.” Users should be cautious when using the feature in production environments.

Please keep in mind that the implementation of ESM in Node.js differs from the developer experience you might be familiar with. Most transpilation workflows support features such as optional file extensions or JSON modules that the Node.js ESM implementation does not support. It is highly likely that modules from transpiled environments will require a certain degree of refactoring to work in Node.js. It is worth mentioning that many of our design decisions were made with two primary goals. Spec compliance and Web Compatibility. It is our belief that the current implementation offers a future proof model to authoring ESM modules that paves the path to Universal JavaScript. Please read more in our documentation.

The ESM implementation in Node.js is still experimental but we do believe that we are getting very close to being able to call ESM in Node.js “stable”. Removing the warning is a huge step in that direction.

#### New V8 ArrayBuffer API

* **src**: migrate to new V8 ArrayBuffer API (Thang Tran) [#30782](https://github.com/nodejs/node/pull/30782)

Multiple ArrayBuffers pointing to the same base address are no longer allowed by V8. This may impact native addons.

#### Toolchain and Compiler Upgrades

* **(SEMVER-MAJOR)** **build**: update macos deployment target to 10.13 for 14.x (AshCripps) [#32454](https://github.com/nodejs/node/pull/32454)
* **(SEMVER-MAJOR)** **doc**: update cross compiler machine for Linux armv7 (Richard Lau) [#32812](https://github.com/nodejs/node/pull/32812)
* **(SEMVER-MAJOR)** **doc**: update Centos/RHEL releases use devtoolset-8 (Richard Lau) [#32812](https://github.com/nodejs/node/pull/32812)
* **(SEMVER-MAJOR)** **doc**: remove SmartOS from official binaries (Richard Lau) [#32812](https://github.com/nodejs/node/pull/32812)
* **(SEMVER-MAJOR)** **win**: block running on EOL Windows versions (João Reis) [#31954](https://github.com/nodejs/node/pull/31954)

It is expected that there will be an ABI mismatch on ARM between the Node.js binary and native addons. Native addons are only broken if they
interact with `std::shared_ptr`. This is expected to be fixed in a later version of Node.js 14. - [#30786](https://github.com/nodejs/node/issues/30786)

#### Update to V8 8.1

* **(SEMVER-MAJOR)** **deps**: update V8 to 8.1.307.20 (Matheus Marchini) [#32116](https://github.com/nodejs/node/pull/32116)

#### Other Notable Changes:

* **cli, report**: move --report-on-fatalerror to stable (Colin Ihrig) [#32496](https://github.com/nodejs/node/pull/32496)
* **deps**: upgrade to libuv 1.37.0 (Colin Ihrig) [#32866](https://github.com/nodejs/node/pull/32866)
* **fs**: add fs/promises alias module (Gus Caplan) [#31553](https://github.com/nodejs/node/pull/31553)

### Semver-Major Commits

* [[`5360dd151d`](https://github.com/nodejs/node/commit/5360dd151d)] - **(SEMVER-MAJOR)** **assert**: handle (deep) equal(NaN, NaN) as being identical (Ruben Bridgewater) [#30766](https://github.com/nodejs/node/pull/30766)
* [[`a621608f12`](https://github.com/nodejs/node/commit/a621608f12)] - **(SEMVER-MAJOR)** **build**: update macos deployment target to 10.13 for 14.x (AshCripps) [#32454](https://github.com/nodejs/node/pull/32454)
* [[`e65bed1b7e`](https://github.com/nodejs/node/commit/e65bed1b7e)] - **(SEMVER-MAJOR)** **child_process**: create proper public API for `channel` (Anna Henningsen) [#30165](https://github.com/nodejs/node/pull/30165)
* [[`1b9a62cff4`](https://github.com/nodejs/node/commit/1b9a62cff4)] - **(SEMVER-MAJOR)** **crypto**: make DH error messages consistent (Tobias Nießen) [#31873](https://github.com/nodejs/node/pull/31873)
* [[`bffa5044c5`](https://github.com/nodejs/node/commit/bffa5044c5)] - **(SEMVER-MAJOR)** **crypto**: move pbkdf2 without digest to EOL (James M Snell) [#31166](https://github.com/nodejs/node/pull/31166)
* [[`10f5fa7513`](https://github.com/nodejs/node/commit/10f5fa7513)] - **(SEMVER-MAJOR)** **crypto**: forbid setting the PBKDF2 iter count to 0 (Tobias Nießen) [#30578](https://github.com/nodejs/node/pull/30578)
* [[`2883c855e0`](https://github.com/nodejs/node/commit/2883c855e0)] - **(SEMVER-MAJOR)** **deps**: update V8 to 8.1.307.20 (Matheus Marchini) [#32116](https://github.com/nodejs/node/pull/32116)
* [[`1b2e2944bc`](https://github.com/nodejs/node/commit/1b2e2944bc)] - **(SEMVER-MAJOR)** **dgram**: don't hide implicit bind errors (Colin Ihrig) [#31958](https://github.com/nodejs/node/pull/31958)
* [[`1a1ce93317`](https://github.com/nodejs/node/commit/1a1ce93317)] - **(SEMVER-MAJOR)** **doc**: update cross compiler machine for Linux armv7 (Richard Lau) [#32812](https://github.com/nodejs/node/pull/32812)
* [[`dad96e4fc1`](https://github.com/nodejs/node/commit/dad96e4fc1)] - **(SEMVER-MAJOR)** **doc**: update Centos/RHEL releases use devtoolset-8 (Richard Lau) [#32812](https://github.com/nodejs/node/pull/32812)
* [[`5317202aa1`](https://github.com/nodejs/node/commit/5317202aa1)] - **(SEMVER-MAJOR)** **doc**: remove SmartOS from official binaries (Richard Lau) [#32812](https://github.com/nodejs/node/pull/32812)
* [[`75ee5b2622`](https://github.com/nodejs/node/commit/75ee5b2622)] - **(SEMVER-MAJOR)** **doc**: deprecate process.umask() with no arguments (Colin Ihrig) [#32499](https://github.com/nodejs/node/pull/32499)
* [[`afe353061b`](https://github.com/nodejs/node/commit/afe353061b)] - **(SEMVER-MAJOR)** **doc**: fs.write is not longer coercing strings (Juan José Arboleda) [#31030](https://github.com/nodejs/node/pull/31030)
* [[`a45c1aa39f`](https://github.com/nodejs/node/commit/a45c1aa39f)] - **(SEMVER-MAJOR)** **doc**: fix mode and flags being mistaken in fs (Ruben Bridgewater) [#27044](https://github.com/nodejs/node/pull/27044)
* [[`331d636240`](https://github.com/nodejs/node/commit/331d636240)] - **(SEMVER-MAJOR)** **errors**: remove unused ERR\_SOCKET\_CANNOT\_SEND error (Colin Ihrig) [#31958](https://github.com/nodejs/node/pull/31958)
* [[`b8e41774d4`](https://github.com/nodejs/node/commit/b8e41774d4)] - **(SEMVER-MAJOR)** **fs**: add fs/promises alias module (Gus Caplan) [#31553](https://github.com/nodejs/node/pull/31553)
* [[`fb6df3bfac`](https://github.com/nodejs/node/commit/fb6df3bfac)] - **(SEMVER-MAJOR)** **fs**: validate the input data to be of expected types (Ruben Bridgewater) [#31030](https://github.com/nodejs/node/pull/31030)
* [[`2d8febceef`](https://github.com/nodejs/node/commit/2d8febceef)] - **(SEMVER-MAJOR)** **fs**: deprecate closing FileHandle on garbage collection (James M Snell) [#28396](https://github.com/nodejs/node/pull/28396)
* [[`67e067eb06`](https://github.com/nodejs/node/commit/67e067eb06)] - **(SEMVER-MAJOR)** **fs**: watch signals for recursive incompatibility (Eran Levin) [#29947](https://github.com/nodejs/node/pull/29947)
* [[`f0d2df41f8`](https://github.com/nodejs/node/commit/f0d2df41f8)] - **(SEMVER-MAJOR)** **fs**: change streams to always emit close by default (Robert Nagy) [#31408](https://github.com/nodejs/node/pull/31408)
* [[`a13500f503`](https://github.com/nodejs/node/commit/a13500f503)] - **(SEMVER-MAJOR)** **fs**: improve mode and flags validation (Ruben Bridgewater) [#27044](https://github.com/nodejs/node/pull/27044)
* [[`535e9571f5`](https://github.com/nodejs/node/commit/535e9571f5)] - **(SEMVER-MAJOR)** **fs**: make FSStatWatcher.start private (Lucas Holmquist) [#29971](https://github.com/nodejs/node/pull/29971)
* [[`c1b2f6afbe`](https://github.com/nodejs/node/commit/c1b2f6afbe)] - **(SEMVER-MAJOR)** **http**: detach socket from IncomingMessage on keep-alive (Robert Nagy) [#32153](https://github.com/nodejs/node/pull/32153)
* [[`173d044d09`](https://github.com/nodejs/node/commit/173d044d09)] - **(SEMVER-MAJOR)** **http**: align OutgoingMessage and ClientRequest destroy (Robert Nagy) [#32148](https://github.com/nodejs/node/pull/32148)
* [[`d3715c76b5`](https://github.com/nodejs/node/commit/d3715c76b5)] - **(SEMVER-MAJOR)** **http**: move OutboundMessage.prototype.flush to EOL (James M Snell) [#31164](https://github.com/nodejs/node/pull/31164)
* [[`c776a37791`](https://github.com/nodejs/node/commit/c776a37791)] - **(SEMVER-MAJOR)** **http**: end with data can cause write after end (Robert Nagy) [#28666](https://github.com/nodejs/node/pull/28666)
* [[`ff2ed3ec85`](https://github.com/nodejs/node/commit/ff2ed3ec85)] - **(SEMVER-MAJOR)** **http**: remove unused hasItems() from freelist (Rich Trott) [#30744](https://github.com/nodejs/node/pull/30744)
* [[`d247a8e1dc`](https://github.com/nodejs/node/commit/d247a8e1dc)] - **(SEMVER-MAJOR)** **http**: emit close on socket re-use (Robert Nagy) [#28685](https://github.com/nodejs/node/pull/28685)
* [[`6f0ec79e42`](https://github.com/nodejs/node/commit/6f0ec79e42)] - **(SEMVER-MAJOR)** **http,stream**: make virtual methods throw an error (Luigi Pinca) [#31912](https://github.com/nodejs/node/pull/31912)
* [[`ec0dd6fa1c`](https://github.com/nodejs/node/commit/ec0dd6fa1c)] - **(SEMVER-MAJOR)** **lib**: move GLOBAL and root aliases to EOL (James M Snell) [#31167](https://github.com/nodejs/node/pull/31167)
* [[`d7452b7140`](https://github.com/nodejs/node/commit/d7452b7140)] - **(SEMVER-MAJOR)** **module**: warn on using unfinished circular dependency (Anna Henningsen) [#29935](https://github.com/nodejs/node/pull/29935)
* [[`eeccd52b4e`](https://github.com/nodejs/node/commit/eeccd52b4e)] - **(SEMVER-MAJOR)** **net**: make readable/writable start as true (Robert Nagy) [#32272](https://github.com/nodejs/node/pull/32272)
* [[`ab4115f17c`](https://github.com/nodejs/node/commit/ab4115f17c)] - **(SEMVER-MAJOR)** **os**: move tmpDir() to EOL (James M Snell) [#31169](https://github.com/nodejs/node/pull/31169)
* [[`8c18e91c8a`](https://github.com/nodejs/node/commit/8c18e91c8a)] - **(SEMVER-MAJOR)** **process**: remove undocumented `now` argument from emitWarning() (Rich Trott) [#31643](https://github.com/nodejs/node/pull/31643)
* [[`84c426cb60`](https://github.com/nodejs/node/commit/84c426cb60)] - **(SEMVER-MAJOR)** **repl**: properly handle `repl.repl` (Ruben Bridgewater) [#30981](https://github.com/nodejs/node/pull/30981)
* [[`4f523c2c1a`](https://github.com/nodejs/node/commit/4f523c2c1a)] - **(SEMVER-MAJOR)** **src**: migrate to new V8 ArrayBuffer API (Thang Tran) [#30782](https://github.com/nodejs/node/pull/30782)
* [[`c712fb7cd6`](https://github.com/nodejs/node/commit/c712fb7cd6)] - **(SEMVER-MAJOR)** **src**: add abstract `IsolatePlatformDelegate` (Marcel Laverdet) [#30324](https://github.com/nodejs/node/pull/30324)
* [[`1428a92492`](https://github.com/nodejs/node/commit/1428a92492)] - **(SEMVER-MAJOR)** **stream**: make pipeline try to wait for 'close' (Robert Nagy) [#32158](https://github.com/nodejs/node/pull/32158)
* [[`388cef61e8`](https://github.com/nodejs/node/commit/388cef61e8)] - **(SEMVER-MAJOR)** **stream**: align stream.Duplex with net.Socket (Robert Nagy) [#32139](https://github.com/nodejs/node/pull/32139)
* [[`7cafd5f3a9`](https://github.com/nodejs/node/commit/7cafd5f3a9)] - **(SEMVER-MAJOR)** **stream**: fix finished w/ 'close' before 'end' (Robert Nagy) [#31545](https://github.com/nodejs/node/pull/31545)
* [[`311e12b962`](https://github.com/nodejs/node/commit/311e12b962)] - **(SEMVER-MAJOR)** **stream**: fix multiple destroy calls (Robert Nagy) [#29197](https://github.com/nodejs/node/pull/29197)
* [[`1f209129c7`](https://github.com/nodejs/node/commit/1f209129c7)] - **(SEMVER-MAJOR)** **stream**: throw invalid argument errors (Robert Nagy) [#31831](https://github.com/nodejs/node/pull/31831)
* [[`d016b9d708`](https://github.com/nodejs/node/commit/d016b9d708)] - **(SEMVER-MAJOR)** **stream**: finished callback for closed streams (Robert Nagy) [#31509](https://github.com/nodejs/node/pull/31509)
* [[`e559842188`](https://github.com/nodejs/node/commit/e559842188)] - **(SEMVER-MAJOR)** **stream**: make readable & writable computed (Robert Nagy) [#31197](https://github.com/nodejs/node/pull/31197)
* [[`907c07fa85`](https://github.com/nodejs/node/commit/907c07fa85)] - **(SEMVER-MAJOR)** **stream**: move \_writableState.buffer to EOL (James M Snell) [#31165](https://github.com/nodejs/node/pull/31165)
* [[`66f4e4edcb`](https://github.com/nodejs/node/commit/66f4e4edcb)] - **(SEMVER-MAJOR)** **stream**: do not emit 'end' after 'error' (Robert Nagy) [#31182](https://github.com/nodejs/node/pull/31182)
* [[`75b30c606c`](https://github.com/nodejs/node/commit/75b30c606c)] - **(SEMVER-MAJOR)** **stream**: emit 'error' asynchronously (Robert Nagy) [#29744](https://github.com/nodejs/node/pull/29744)
* [[`4bec6d13f9`](https://github.com/nodejs/node/commit/4bec6d13f9)] - **(SEMVER-MAJOR)** **stream**: enable autoDestroy by default (Robert Nagy) [#30623](https://github.com/nodejs/node/pull/30623)
* [[`20d009d2fd`](https://github.com/nodejs/node/commit/20d009d2fd)] - **(SEMVER-MAJOR)** **stream**: pipe should not swallow error (Robert Nagy) [#30993](https://github.com/nodejs/node/pull/30993)
* [[`67ed526ab0`](https://github.com/nodejs/node/commit/67ed526ab0)] - **(SEMVER-MAJOR)** **stream**: error state cleanup (Robert Nagy) [#30851](https://github.com/nodejs/node/pull/30851)
* [[`e902fadc5e`](https://github.com/nodejs/node/commit/e902fadc5e)] - **(SEMVER-MAJOR)** **stream**: do not throw multiple callback errors in writable (Robert Nagy) [#30614](https://github.com/nodejs/node/pull/30614)
* [[`e13a37e49d`](https://github.com/nodejs/node/commit/e13a37e49d)] - **(SEMVER-MAJOR)** **stream**: ensure finish is emitted in next tick (Robert Nagy) [#30733](https://github.com/nodejs/node/pull/30733)
* [[`9d09969f4c`](https://github.com/nodejs/node/commit/9d09969f4c)] - **(SEMVER-MAJOR)** **stream**: always invoke end callback (Robert Nagy) [#29747](https://github.com/nodejs/node/pull/29747)

* [[`0f78dcc86d`](https://github.com/nodejs/node/commit/0f78dcc86d)] - **(SEMVER-MAJOR)** **util**: escape C1 control characters and switch to hex format (Ruben Bridgewater) [#29826](https://github.com/nodejs/node/pull/29826)
* [[`cb8898c48f`](https://github.com/nodejs/node/commit/cb8898c48f)] - **(SEMVER-MAJOR)** **win**: block running on EOL Windows versions (João Reis) [#31954](https://github.com/nodejs/node/pull/31954)
* [[`a9401439c7`](https://github.com/nodejs/node/commit/a9401439c7)] - **(SEMVER-MAJOR)** **zlib**: align with streams (Robert Nagy) [#32220](https://github.com/nodejs/node/pull/32220)

### Semver-Minor Commits

* [[`63f0dd1ab9`](https://github.com/nodejs/node/commit/63f0dd1ab9)] - **(SEMVER-MINOR)** **async_hooks**: merge run and exit methods (Andrey Pechkurov) [#31950](https://github.com/nodejs/node/pull/31950)
* [[`a683e87cd0`](https://github.com/nodejs/node/commit/a683e87cd0)] - **(SEMVER-MINOR)** **async_hooks**: prevent sync methods of async storage exiting outer context (Stephen Belanger) [#31950](https://github.com/nodejs/node/pull/31950)
* [[`f571b294f5`](https://github.com/nodejs/node/commit/f571b294f5)] - **(SEMVER-MINOR)** **doc**: deprecate process.mainModule (Antoine du HAMEL) [#32232](https://github.com/nodejs/node/pull/32232)
* [[`e04f599258`](https://github.com/nodejs/node/commit/e04f599258)] - **(SEMVER-MINOR)** **doc**: add basic embedding example documentation (Anna Henningsen) [#30467](https://github.com/nodejs/node/pull/30467)
* [[`e93503be83`](https://github.com/nodejs/node/commit/e93503be83)] - **(SEMVER-MINOR)** **embedding**: provide hook for custom process.exit() behaviour (Anna Henningsen) [#32531](https://github.com/nodejs/node/pull/32531)
* [[`a8cf886de7`](https://github.com/nodejs/node/commit/a8cf886de7)] - **(SEMVER-MINOR)** **src**: shutdown platform from FreePlatform() (Anna Henningsen) [#30467](https://github.com/nodejs/node/pull/30467)
* [[`0e576740dc`](https://github.com/nodejs/node/commit/0e576740dc)] - **(SEMVER-MINOR)** **src**: fix what a dispose without checking (Jichan) [#30467](https://github.com/nodejs/node/pull/30467)
* [[`887b6a143b`](https://github.com/nodejs/node/commit/887b6a143b)] - **(SEMVER-MINOR)** **src**: allow non-Node.js TracingControllers (Anna Henningsen) [#30467](https://github.com/nodejs/node/pull/30467)
* [[`7e0264d932`](https://github.com/nodejs/node/commit/7e0264d932)] - **(SEMVER-MINOR)** **src**: add ability to look up platform based on `Environment\*` (Anna Henningsen) [#30467](https://github.com/nodejs/node/pull/30467)
* [[`d7f11077f1`](https://github.com/nodejs/node/commit/d7f11077f1)] - **(SEMVER-MINOR)** **src**: make InitializeNodeWithArgs() official public API (Anna Henningsen) [#30467](https://github.com/nodejs/node/pull/30467)
* [[`821e21de8c`](https://github.com/nodejs/node/commit/821e21de8c)] - **(SEMVER-MINOR)** **src**: add unique\_ptr equivalent of CreatePlatform (Anna Henningsen) [#30467](https://github.com/nodejs/node/pull/30467)
* [[`7dead8440c`](https://github.com/nodejs/node/commit/7dead8440c)] - **(SEMVER-MINOR)** **src**: add LoadEnvironment() variant taking a string (Anna Henningsen) [#30467](https://github.com/nodejs/node/pull/30467)
* [[`c44edec4da`](https://github.com/nodejs/node/commit/c44edec4da)] - **(SEMVER-MINOR)** **src**: provide a variant of LoadEnvironment taking a callback (Anna Henningsen) [#30467](https://github.com/nodejs/node/pull/30467)
* [[`a9fb51f9be`](https://github.com/nodejs/node/commit/a9fb51f9be)] - **(SEMVER-MINOR)** **src**: align worker and main thread code with embedder API (Anna Henningsen) [#30467](https://github.com/nodejs/node/pull/30467)
* [[`084c379648`](https://github.com/nodejs/node/commit/084c379648)] - **(SEMVER-MINOR)** **src**: associate is\_main\_thread() with worker\_context() (Anna Henningsen) [#30467](https://github.com/nodejs/node/pull/30467)
* [[`64c01222d9`](https://github.com/nodejs/node/commit/64c01222d9)] - **(SEMVER-MINOR)** **src**: move worker\_context from Environment to IsolateData (Anna Henningsen) [#30467](https://github.com/nodejs/node/pull/30467)
* [[`288382a4ce`](https://github.com/nodejs/node/commit/288382a4ce)] - **(SEMVER-MINOR)** **src**: fix memory leak in CreateEnvironment when bootstrap fails (Anna Henningsen) [#30467](https://github.com/nodejs/node/pull/30467)
* [[`d7bc5816a5`](https://github.com/nodejs/node/commit/d7bc5816a5)] - **(SEMVER-MINOR)** **src**: make `FreeEnvironment()` perform all necessary cleanup (Anna Henningsen) [#30467](https://github.com/nodejs/node/pull/30467)
* [[`43d32b073f`](https://github.com/nodejs/node/commit/43d32b073f)] - **(SEMVER-MINOR)** **src,test**: add full-featured embedder API test (Anna Henningsen) [#30467](https://github.com/nodejs/node/pull/30467)
* [[`2061c33670`](https://github.com/nodejs/node/commit/2061c33670)] - **(SEMVER-MINOR)** **test**: add extended embedder cctest (Anna Henningsen) [#30467](https://github.com/nodejs/node/pull/30467)
* [[`2561484dcb`](https://github.com/nodejs/node/commit/2561484dcb)] - **(SEMVER-MINOR)** **test**: re-enable cctest that was commented out (Anna Henningsen) [#30467](https://github.com/nodejs/node/pull/30467)

### Semver-Patch Commits

* [[`9b6e797379`](https://github.com/nodejs/node/commit/9b6e797379)] - ***Revert*** "**assert**: fix line number calculation after V8 upgrade" (Michaël Zasso) [#32116](https://github.com/nodejs/node/pull/32116)
* [[`c740fbda9d`](https://github.com/nodejs/node/commit/c740fbda9d)] - **buffer**: add type check in bidirectionalIndexOf (Gerhard Stoebich) [#32770](https://github.com/nodejs/node/pull/32770)
* [[`c8e3470e53`](https://github.com/nodejs/node/commit/c8e3470e53)] - **buffer**: mark pool ArrayBuffer as untransferable (Anna Henningsen) [#32759](https://github.com/nodejs/node/pull/32759)
* [[`f2c22db580`](https://github.com/nodejs/node/commit/f2c22db580)] - **build**: remove .git folders when testing V8 (Richard Lau) [#32877](https://github.com/nodejs/node/pull/32877)
* [[`c0f43bfda8`](https://github.com/nodejs/node/commit/c0f43bfda8)] - **build**: add configure flag to build V8 with DCHECKs (Anna Henningsen) [#32787](https://github.com/nodejs/node/pull/32787)
* [[`99e7f878ce`](https://github.com/nodejs/node/commit/99e7f878ce)] - **build**: re-enable ASAN Action using clang (Matheus Marchini) [#32776](https://github.com/nodejs/node/pull/32776)
* [[`3e55284e9b`](https://github.com/nodejs/node/commit/3e55284e9b)] - **build**: use same flags as V8 for ASAN (Matheus Marchini) [#32776](https://github.com/nodejs/node/pull/32776)
* [[`4e5ec41024`](https://github.com/nodejs/node/commit/4e5ec41024)] - **build**: add build from tarball (John Kleinschmidt) [#32129](https://github.com/nodejs/node/pull/32129)
* [[`6a349019da`](https://github.com/nodejs/node/commit/6a349019da)] - **build**: temporarily skip ASAN build (Matheus Marchini) [#32116](https://github.com/nodejs/node/pull/32116)
* [[`da92f15413`](https://github.com/nodejs/node/commit/da92f15413)] - **build**: reset embedder string to "-node.0" (Matheus Marchini) [#32116](https://github.com/nodejs/node/pull/32116)
* [[`e883059c24`](https://github.com/nodejs/node/commit/e883059c24)] - **cli, report**: move --report-on-fatalerror to stable (Colin Ihrig) [#32496](https://github.com/nodejs/node/pull/32496)
* [[`bf86f55e22`](https://github.com/nodejs/node/commit/bf86f55e22)] - **deps**: patch V8 to 8.1.307.30 (Michaël Zasso) [#32693](https://github.com/nodejs/node/pull/32693)
* [[`b5bbde8cf1`](https://github.com/nodejs/node/commit/b5bbde8cf1)] - **deps**: upgrade to libuv 1.37.0 (Colin Ihrig) [#32866](https://github.com/nodejs/node/pull/32866)
* [[`7afe24dba6`](https://github.com/nodejs/node/commit/7afe24dba6)] - **deps**: upgrade to libuv 1.36.0 (Colin Ihrig) [#32866](https://github.com/nodejs/node/pull/32866)
* [[`1cd235d1a0`](https://github.com/nodejs/node/commit/1cd235d1a0)] - **deps**: patch V8 to run on Xcode 8 (Matheus Marchini) [#32116](https://github.com/nodejs/node/pull/32116)
* [[`5d867badd0`](https://github.com/nodejs/node/commit/5d867badd0)] - **deps**: V8: silence irrelevant warnings (Michaël Zasso) [#32116](https://github.com/nodejs/node/pull/32116)
* [[`8d2c441e4d`](https://github.com/nodejs/node/commit/8d2c441e4d)] - **deps**: V8: cherry-pick 931bdbd76f5b (Matheus Marchini) [#32116](https://github.com/nodejs/node/pull/32116)
* [[`049160dfb6`](https://github.com/nodejs/node/commit/049160dfb6)] - **deps**: V8: cherry-pick 1e36e21acc40 (Matheus Marchini) [#32116](https://github.com/nodejs/node/pull/32116)
* [[`0220c298c5`](https://github.com/nodejs/node/commit/0220c298c5)] - **deps**: bump minimum icu version to 65 (Michaël Zasso) [#32116](https://github.com/nodejs/node/pull/32116)
* [[`f90eba1d91`](https://github.com/nodejs/node/commit/f90eba1d91)] - **deps**: make v8.h compatible with VS2015 (Joao Reis) [#32116](https://github.com/nodejs/node/pull/32116)
* [[`56b6a4f732`](https://github.com/nodejs/node/commit/56b6a4f732)] - **deps**: V8: forward declaration of `Rtl\*FunctionTable` (Refael Ackermann) [#32116](https://github.com/nodejs/node/pull/32116)
* [[`40c9419b35`](https://github.com/nodejs/node/commit/40c9419b35)] - **deps**: V8: patch register-arm64.h (Refael Ackermann) [#32116](https://github.com/nodejs/node/pull/32116)
* [[`55407ab73e`](https://github.com/nodejs/node/commit/55407ab73e)] - **deps**: patch V8 to run on older XCode versions (Ujjwal Sharma) [#32116](https://github.com/nodejs/node/pull/32116)
* [[`990bc9adb4`](https://github.com/nodejs/node/commit/990bc9adb4)] - **deps**: V8: un-cherry-pick bd019bd (Refael Ackermann) [#32116](https://github.com/nodejs/node/pull/32116)
* [[`17a6def4e8`](https://github.com/nodejs/node/commit/17a6def4e8)] - **deps**: update V8 dtrace & postmortem metadata (Colin Ihrig) [#32116](https://github.com/nodejs/node/pull/32116)
* [[`0f14123186`](https://github.com/nodejs/node/commit/0f14123186)] - **deps**: V8: stub backport fast API call changes (Anna Henningsen) [#32885](https://github.com/nodejs/node/pull/32885)
* [[`bf412ed77b`](https://github.com/nodejs/node/commit/bf412ed77b)] - **deps**: V8: stub backport d5b444bc5a84 (Anna Henningsen) [#32885](https://github.com/nodejs/node/pull/32885)
* [[`fdaa365b0b`](https://github.com/nodejs/node/commit/fdaa365b0b)] - **deps**: V8: stub backport 65238018ca4b and 8d08318e1a85 (Anna Henningsen) [#32885](https://github.com/nodejs/node/pull/32885)
* [[`8198e7882c`](https://github.com/nodejs/node/commit/8198e7882c)] - **deps**: V8: stub backport 9e52d5c5d717 (Anna Henningsen) [#32885](https://github.com/nodejs/node/pull/32885)
* [[`a27852ae7c`](https://github.com/nodejs/node/commit/a27852ae7c)] - **deps**: V8: cherry-pick 98b1ef80c722 (Anna Henningsen) [#32885](https://github.com/nodejs/node/pull/32885)
* [[`e8c7b7a2df`](https://github.com/nodejs/node/commit/e8c7b7a2df)] - **deps**: V8: cherry-pick b5c917ee80cb (Anna Henningsen) [#32885](https://github.com/nodejs/node/pull/32885)
* [[`552cee0cc0`](https://github.com/nodejs/node/commit/552cee0cc0)] - **deps**: V8: cherry-pick 700b1b97e9ab (Anna Henningsen) [#32885](https://github.com/nodejs/node/pull/32885)
* [[`9b7a1b048a`](https://github.com/nodejs/node/commit/9b7a1b048a)] - **deps**: V8: cherry-pick e8ba5699c648 (Anna Henningsen) [#32885](https://github.com/nodejs/node/pull/32885)
* [[`1f02617b05`](https://github.com/nodejs/node/commit/1f02617b05)] - **deps**: V8: cherry-pick 55a01ec7519a (Anna Henningsen) [#32885](https://github.com/nodejs/node/pull/32885)
* [[`da728c482c`](https://github.com/nodejs/node/commit/da728c482c)] - **deps**: V8: cherry-pick 9f0f2cb7f08d (Anna Henningsen) [#32885](https://github.com/nodejs/node/pull/32885)
* [[`2ee8b4a512`](https://github.com/nodejs/node/commit/2ee8b4a512)] - **deps**: V8: cherry-pick e395d1698453 (Anna Henningsen) [#32885](https://github.com/nodejs/node/pull/32885)
* [[`dfc66a6af4`](https://github.com/nodejs/node/commit/dfc66a6af4)] - **deps**: V8: cherry-pick d1253ae95b09 (Anna Henningsen) [#32885](https://github.com/nodejs/node/pull/32885)
* [[`c3ecbc758b`](https://github.com/nodejs/node/commit/c3ecbc758b)] - **deps**: V8: cherry-pick fa3e37e511ee (Anna Henningsen) [#32885](https://github.com/nodejs/node/pull/32885)
* [[`9568fbc7cd`](https://github.com/nodejs/node/commit/9568fbc7cd)] - **deps**: V8: cherry-pick f0057afc2fb6 (Anna Henningsen) [#32885](https://github.com/nodejs/node/pull/32885)
* [[`07d4372d5a`](https://github.com/nodejs/node/commit/07d4372d5a)] - **deps**: V8: cherry-pick 94723c197199 (Anna Henningsen) [#32885](https://github.com/nodejs/node/pull/32885)
* [[`4a11a54f9a`](https://github.com/nodejs/node/commit/4a11a54f9a)] - **deps**: V8: backport 844fe8f7d965 (Anna Henningsen) [#32885](https://github.com/nodejs/node/pull/32885)
* [[`1b7878558a`](https://github.com/nodejs/node/commit/1b7878558a)] - **deps**: V8: cherry-pick 2db93c023379 (Anna Henningsen) [#32885](https://github.com/nodejs/node/pull/32885)
* [[`122937fc67`](https://github.com/nodejs/node/commit/122937fc67)] - **deps**: V8: cherry-pick 4b1447e4bb0e (Anna Henningsen) [#32885](https://github.com/nodejs/node/pull/32885)
* [[`01573ba4ae`](https://github.com/nodejs/node/commit/01573ba4ae)] - **deps**: remove duplicated postmortem metadata entry (Matheus Marchini) [#32521](https://github.com/nodejs/node/pull/32521)
* [[`9290febefa`](https://github.com/nodejs/node/commit/9290febefa)] - **deps**: patch V8 to 8.1.307.26 (Matheus Marchini) [#32521](https://github.com/nodejs/node/pull/32521)
* [[`a9e4cec70d`](https://github.com/nodejs/node/commit/a9e4cec70d)] - ***Revert*** "**deps**: V8: cherry-pick f9257802c1c0" (Matheus Marchini) [#32521](https://github.com/nodejs/node/pull/32521)
* [[`77542a5d57`](https://github.com/nodejs/node/commit/77542a5d57)] - **deps**: revert whitespace changes on V8 (Matheus Marchini) [#32587](https://github.com/nodejs/node/pull/32587)
* [[`9add24ecd3`](https://github.com/nodejs/node/commit/9add24ecd3)] - **doc**: missing brackets (William Bonawentura) [#32657](https://github.com/nodejs/node/pull/32657)
* [[`1796cc0df5`](https://github.com/nodejs/node/commit/1796cc0df5)] - **doc**: improve consistency in usage of NULL (Michael Dawson) [#32726](https://github.com/nodejs/node/pull/32726)
* [[`2662b0c9e3`](https://github.com/nodejs/node/commit/2662b0c9e3)] - **doc**: improve net docs (Robert Nagy) [#32811](https://github.com/nodejs/node/pull/32811)
* [[`5d940de17b`](https://github.com/nodejs/node/commit/5d940de17b)] - **doc**: note that signatures of binary may be from subkeys (Reşat SABIQ) [#32591](https://github.com/nodejs/node/pull/32591)
* [[`3c8dd6d0c3`](https://github.com/nodejs/node/commit/3c8dd6d0c3)] - **doc**: add transform stream destroy() return value (Colin Ihrig) [#32788](https://github.com/nodejs/node/pull/32788)
* [[`39368b34eb`](https://github.com/nodejs/node/commit/39368b34eb)] - **doc**: updated guidance for n-api changes (Michael Dawson) [#32721](https://github.com/nodejs/node/pull/32721)
* [[`cba6e5dc09`](https://github.com/nodejs/node/commit/cba6e5dc09)] - **doc**: remove warning from `response.writeHead` (Cecchi MacNaughton) [#32700](https://github.com/nodejs/node/pull/32700)
* [[`8f7fd8d6aa`](https://github.com/nodejs/node/commit/8f7fd8d6aa)] - **doc**: improve AsyncLocalStorage sample (Andrey Pechkurov) [#32757](https://github.com/nodejs/node/pull/32757)
* [[`a7c75f956f`](https://github.com/nodejs/node/commit/a7c75f956f)] - **doc**: document `buffer.from` returns internal pool buffer (Harshitha KP) [#32703](https://github.com/nodejs/node/pull/32703)
* [[`f6a91156c7`](https://github.com/nodejs/node/commit/f6a91156c7)] - **doc**: add puzpuzpuz to collaborators (Andrey Pechkurov) [#32817](https://github.com/nodejs/node/pull/32817)
* [[`1db8da21f2`](https://github.com/nodejs/node/commit/1db8da21f2)] - **doc**: split process.umask() entry into two (Rich Trott) [#32711](https://github.com/nodejs/node/pull/32711)
* [[`6ade42bb3c`](https://github.com/nodejs/node/commit/6ade42bb3c)] - **doc**: stream.end(cb) cb can be invoked with error (Pranshu Srivastava) [#32238](https://github.com/nodejs/node/pull/32238)
* [[`edb3ffb003`](https://github.com/nodejs/node/commit/edb3ffb003)] - **doc**: fix os.version() Windows API (Colin Ihrig) [#32156](https://github.com/nodejs/node/pull/32156)
* [[`a777cfa843`](https://github.com/nodejs/node/commit/a777cfa843)] - **doc**: remove repetition (Luigi Pinca) [#31868](https://github.com/nodejs/node/pull/31868)
* [[`7c524fb092`](https://github.com/nodejs/node/commit/7c524fb092)] - **doc**: fix Writable.write callback description (Robert Nagy) [#31812](https://github.com/nodejs/node/pull/31812)
* [[`43fb664701`](https://github.com/nodejs/node/commit/43fb664701)] - **doc**: fix missing changelog corrections (Myles Borins) [#31854](https://github.com/nodejs/node/pull/31854)
* [[`a2d6f98e1a`](https://github.com/nodejs/node/commit/a2d6f98e1a)] - **doc**: fix typo (Colin Ihrig) [#31675](https://github.com/nodejs/node/pull/31675)
* [[`17e3f3be76`](https://github.com/nodejs/node/commit/17e3f3be76)] - **doc**: update pr-url for DEP0022 EOL (Colin Ihrig) [#31675](https://github.com/nodejs/node/pull/31675)
* [[`cd0f5a239e`](https://github.com/nodejs/node/commit/cd0f5a239e)] - **doc**: update pr-url for DEP0016 EOL (Colin Ihrig) [#31675](https://github.com/nodejs/node/pull/31675)
* [[`5170daaca5`](https://github.com/nodejs/node/commit/5170daaca5)] - **doc**: fix changelog for v10.18.1 (Andrew Hughes) [#31358](https://github.com/nodejs/node/pull/31358)
* [[`d845915d46`](https://github.com/nodejs/node/commit/d845915d46)] - **doc**: mark Node.js 8 End-of-Life in CHANGELOG (Beth Griggs) [#31152](https://github.com/nodejs/node/pull/31152)
* [[`009a9c475b`](https://github.com/nodejs/node/commit/009a9c475b)] - **doc,src,test**: assign missing deprecation code (Colin Ihrig) [#31674](https://github.com/nodejs/node/pull/31674)
* [[`ed4fbefb71`](https://github.com/nodejs/node/commit/ed4fbefb71)] - **fs**: use finished over destroy w/ cb (Robert Nagy) [#32809](https://github.com/nodejs/node/pull/32809)
* [[`3e9302b2b3`](https://github.com/nodejs/node/commit/3e9302b2b3)] - **fs**: validate the input data before opening file (Yongsheng Zhang) [#31731](https://github.com/nodejs/node/pull/31731)
* [[`1a3e358a1d`](https://github.com/nodejs/node/commit/1a3e358a1d)] - **http**: refactor agent 'free' handler (Robert Nagy) [#32801](https://github.com/nodejs/node/pull/32801)
* [[`399749e4d8`](https://github.com/nodejs/node/commit/399749e4d8)] - **lib**: created isValidCallback helper (Yash Ladha) [#32665](https://github.com/nodejs/node/pull/32665)
* [[`bc55b57e64`](https://github.com/nodejs/node/commit/bc55b57e64)] - **lib**: fix few comment typos in fs/watchers.js (Denys Otrishko) [#31705](https://github.com/nodejs/node/pull/31705)
* [[`f98668ade3`](https://github.com/nodejs/node/commit/f98668ade3)] - **module**: remove experimental modules warning (Guy Bedford) [#31974](https://github.com/nodejs/node/pull/31974)
* [[`fe1bda9aeb`](https://github.com/nodejs/node/commit/fe1bda9aeb)] - **module**: fix memory leak when require error occurs (Qinhui Chen) [#32837](https://github.com/nodejs/node/pull/32837)
* [[`076ba3150d`](https://github.com/nodejs/node/commit/076ba3150d)] - ***Revert*** "**n-api**: detect deadlocks in thread-safe function" (Gabriel Schulhof) [#32880](https://github.com/nodejs/node/pull/32880)
* [[`1092bb94f4`](https://github.com/nodejs/node/commit/1092bb94f4)] - **process**: suggest --trace-warnings when printing warning (Anna Henningsen) [#32797](https://github.com/nodejs/node/pull/32797)
* [[`d19a2c33b3`](https://github.com/nodejs/node/commit/d19a2c33b3)] - **src**: migrate measureMemory to new v8 api (gengjiawen) [#32116](https://github.com/nodejs/node/pull/32116)
* [[`a63db7fb5e`](https://github.com/nodejs/node/commit/a63db7fb5e)] - **src**: remove deprecated wasm type check (Clemens Backes) [#32116](https://github.com/nodejs/node/pull/32116)
* [[`c080b2d821`](https://github.com/nodejs/node/commit/c080b2d821)] - **src**: avoid calling deprecated method (Clemens Backes) [#32116](https://github.com/nodejs/node/pull/32116)
* [[`7ed0d1439e`](https://github.com/nodejs/node/commit/7ed0d1439e)] - **src**: remove use of deprecated Symbol::Name() (Colin Ihrig) [#32116](https://github.com/nodejs/node/pull/32116)
* [[`59eeb3b5b9`](https://github.com/nodejs/node/commit/59eeb3b5b9)] - **src**: stop overriding deprecated V8 methods (Clemens Backes) [#32116](https://github.com/nodejs/node/pull/32116)
* [[`339c192ddb`](https://github.com/nodejs/node/commit/339c192ddb)] - **src**: update NODE\_MODULE\_VERSION to 83 (Matheus Marchini) [#32116](https://github.com/nodejs/node/pull/32116)
* [[`6681a685a9`](https://github.com/nodejs/node/commit/6681a685a9)] - **src**: remove unused using in node\_worker.cc (Daniel Bevenius) [#32840](https://github.com/nodejs/node/pull/32840)
* [[`b9d9f91a80`](https://github.com/nodejs/node/commit/b9d9f91a80)] - **src**: use basename(argv0) for --trace-uncaught suggestion (Anna Henningsen) [#32798](https://github.com/nodejs/node/pull/32798)
* [[`24e1e28b38`](https://github.com/nodejs/node/commit/24e1e28b38)] - **src**: ignore GCC -Wcast-function-type for v8.h (Daniel Bevenius) [#32679](https://github.com/nodejs/node/pull/32679)
* [[`a946189ccd`](https://github.com/nodejs/node/commit/a946189ccd)] - **src**: add AliasedStruct utility (James M Snell) [#32778](https://github.com/nodejs/node/pull/32778)
* [[`457f1f1ed0`](https://github.com/nodejs/node/commit/457f1f1ed0)] - **src**: remove unused v8 Array namespace (Juan José Arboleda) [#32749](https://github.com/nodejs/node/pull/32749)
* [[`b68e26ee70`](https://github.com/nodejs/node/commit/b68e26ee70)] - **src**: flush V8 interrupts from Environment dtor (Anna Henningsen) [#32523](https://github.com/nodejs/node/pull/32523)
* [[`96bf137cca`](https://github.com/nodejs/node/commit/96bf137cca)] - **src**: use env-\>RequestInterrupt() for inspector MainThreadInterface (Anna Henningsen) [#32523](https://github.com/nodejs/node/pull/32523)
* [[`72da426780`](https://github.com/nodejs/node/commit/72da426780)] - **src**: use env-\>RequestInterrupt() for inspector io thread start (Anna Henningsen) [#32523](https://github.com/nodejs/node/pull/32523)
* [[`99c9b2368c`](https://github.com/nodejs/node/commit/99c9b2368c)] - **src**: fix cleanup hook removal for InspectorTimer (Anna Henningsen) [#32523](https://github.com/nodejs/node/pull/32523)
* [[`6dffd6b3de`](https://github.com/nodejs/node/commit/6dffd6b3de)] - **src**: make `Environment::interrupt\_data\_` atomic (Anna Henningsen) [#32523](https://github.com/nodejs/node/pull/32523)
* [[`8c5ad1392f`](https://github.com/nodejs/node/commit/8c5ad1392f)] - **src**: initialize inspector before RunBootstrapping() (Anna Henningsen) [#32672](https://github.com/nodejs/node/pull/32672)
* [[`eafd64b1c8`](https://github.com/nodejs/node/commit/eafd64b1c8)] - **src**: consistently declare BindingData class (Sam Roberts) [#32677](https://github.com/nodejs/node/pull/32677)
* [[`78c82a38ac`](https://github.com/nodejs/node/commit/78c82a38ac)] - **src**: move fs state out of Environment (Anna Henningsen) [#32538](https://github.com/nodejs/node/pull/32538)
* [[`7005670f34`](https://github.com/nodejs/node/commit/7005670f34)] - **src**: move http parser state out of Environment (Anna Henningsen) [#32538](https://github.com/nodejs/node/pull/32538)
* [[`19b671506c`](https://github.com/nodejs/node/commit/19b671506c)] - **src**: move v8 stats buffers out of Environment (Anna Henningsen) [#32538](https://github.com/nodejs/node/pull/32538)
* [[`4df24f040d`](https://github.com/nodejs/node/commit/4df24f040d)] - **src**: move HTTP/2 state out of Environment (Anna Henningsen) [#32538](https://github.com/nodejs/node/pull/32538)
* [[`1fc3de908e`](https://github.com/nodejs/node/commit/1fc3de908e)] - **src**: make creating per-binding data structures easier (Anna Henningsen) [#32538](https://github.com/nodejs/node/pull/32538)
* [[`0e9f9b7592`](https://github.com/nodejs/node/commit/0e9f9b7592)] - **src**: include AsyncWrap provider strings in snapshot (Anna Henningsen) [#32572](https://github.com/nodejs/node/pull/32572)
* [[`effebf87ab`](https://github.com/nodejs/node/commit/effebf87ab)] - **src**: remove unused v8 namespace (Juan José Arboleda) [#32375](https://github.com/nodejs/node/pull/32375)
* [[`d23eed256b`](https://github.com/nodejs/node/commit/d23eed256b)] - **src**: remove calls to deprecated ArrayBuffer methods (Michaël Zasso) [#32358](https://github.com/nodejs/node/pull/32358)
* [[`f3682102dc`](https://github.com/nodejs/node/commit/f3682102dc)] - **src**: give Http2Session JS fields their own backing store (Anna Henningsen) [#31648](https://github.com/nodejs/node/pull/31648)
* [[`90f7a5c010`](https://github.com/nodejs/node/commit/90f7a5c010)] - **src**: set arraybuffer\_untransferable\_private\_symbol (Thang Tran) [#31053](https://github.com/nodejs/node/pull/31053)
* [[`d06efafe6b`](https://github.com/nodejs/node/commit/d06efafe6b)] - **src**: explicitly allocate backing stores for v8 stat buffers (Anna Henningsen) [#30946](https://github.com/nodejs/node/pull/30946)
* [[`917fedd21a`](https://github.com/nodejs/node/commit/917fedd21a)] - **src**: unset NODE\_VERSION\_IS\_RELEASE from master (Michaël Zasso) [#30584](https://github.com/nodejs/node/pull/30584)
* [[`69f19f4ccd`](https://github.com/nodejs/node/commit/69f19f4ccd)] - **src**: remove uses of deprecated wasm TransferrableModule (Clemens Backes) [#30026](https://github.com/nodejs/node/pull/30026)
* [[`acac5df260`](https://github.com/nodejs/node/commit/acac5df260)] - **src,doc**: add documentation for per-binding state pattern (Anna Henningsen) [#32538](https://github.com/nodejs/node/pull/32538)
* [[`ad4c10e824`](https://github.com/nodejs/node/commit/ad4c10e824)] - **stream**: improve comments regarding end() errors (Robert Nagy) [#32839](https://github.com/nodejs/node/pull/32839)
* [[`6e5c23b6c8`](https://github.com/nodejs/node/commit/6e5c23b6c8)] - **stream**: update comment to indicate unused API (Robert Nagy) [#32808](https://github.com/nodejs/node/pull/32808)
* [[`21bd6679ce`](https://github.com/nodejs/node/commit/21bd6679ce)] - **stream**: fix finished typo (Robert Nagy) [#31881](https://github.com/nodejs/node/pull/31881)
* [[`85c6fcd1cd`](https://github.com/nodejs/node/commit/85c6fcd1cd)] - **stream**: avoid writing to writable (Robert Nagy) [#31805](https://github.com/nodejs/node/pull/31805)
* [[`0875837417`](https://github.com/nodejs/node/commit/0875837417)] - **stream**: fix async iterator destroyed error order (Robert Nagy) [#31700](https://github.com/nodejs/node/pull/31700)
* [[`b9a7625fdf`](https://github.com/nodejs/node/commit/b9a7625fdf)] - **stream**: removed outdated TODO (Robert Nagy) [#31701](https://github.com/nodejs/node/pull/31701)
* [[`68e1288e00`](https://github.com/nodejs/node/commit/68e1288e00)] - **test**: mark addons/zlib-bindings/test flaky on arm (Michaël Zasso) [#32885](https://github.com/nodejs/node/pull/32885)
* [[`a09bf3ad5f`](https://github.com/nodejs/node/commit/a09bf3ad5f)] - **test**: replace console.log/error() with debuglog (daemon1024) [#32692](https://github.com/nodejs/node/pull/32692)
* [[`d1b41bbd86`](https://github.com/nodejs/node/commit/d1b41bbd86)] - **test**: only detect uname on supported os (Xu Meng) [#32833](https://github.com/nodejs/node/pull/32833)
* [[`4bb29ed044`](https://github.com/nodejs/node/commit/4bb29ed044)] - **test**: mark cpu-prof-dir-worker flaky on all (Sam Roberts) [#32828](https://github.com/nodejs/node/pull/32828)
* [[`e18a40e42d`](https://github.com/nodejs/node/commit/e18a40e42d)] - **test**: replace equal with strictEqual (Jesus Hernandez) [#32727](https://github.com/nodejs/node/pull/32727)
* [[`320f297a35`](https://github.com/nodejs/node/commit/320f297a35)] - **test**: mark test-worker-prof flaky on arm (Sam Roberts) [#32826](https://github.com/nodejs/node/pull/32826)
* [[`4b5658b536`](https://github.com/nodejs/node/commit/4b5658b536)] - **test**: mark test-http2-reset-flood flaky on all (Sam Roberts) [#32825](https://github.com/nodejs/node/pull/32825)
* [[`ead51be541`](https://github.com/nodejs/node/commit/ead51be541)] - **test**: cover node entry type in perf\_hooks (Julian Duque) [#32751](https://github.com/nodejs/node/pull/32751)
* [[`9e5189a560`](https://github.com/nodejs/node/commit/9e5189a560)] - **test**: use symlinks to copy shells (John Kleinschmidt) [#32129](https://github.com/nodejs/node/pull/32129)
* [[`c5763e8dc1`](https://github.com/nodejs/node/commit/c5763e8dc1)] - **test**: wait for message from parent in embedding cctest (Anna Henningsen) [#32563](https://github.com/nodejs/node/pull/32563)
* [[`c3204a8787`](https://github.com/nodejs/node/commit/c3204a8787)] - **test**: use common.buildType in embedding test (Anna Henningsen) [#32422](https://github.com/nodejs/node/pull/32422)
* [[`f2cc28aec3`](https://github.com/nodejs/node/commit/f2cc28aec3)] - **test**: use InitializeNodeWithArgs in cctest (Anna Henningsen) [#32406](https://github.com/nodejs/node/pull/32406)
* [[`df1592d2e9`](https://github.com/nodejs/node/commit/df1592d2e9)] - **test**: async iterate destroyed stream (Robert Nagy) [#28995](https://github.com/nodejs/node/pull/28995)
* [[`5100e84f4b`](https://github.com/nodejs/node/commit/5100e84f4b)] - **test**: fix flaky test-fs-promises-file-handle-close (Anna Henningsen) [#31687](https://github.com/nodejs/node/pull/31687)
* [[`52944b834a`](https://github.com/nodejs/node/commit/52944b834a)] - **test**: remove test (Clemens Backes) [#32116](https://github.com/nodejs/node/pull/32116)
* [[`119fdf6813`](https://github.com/nodejs/node/commit/119fdf6813)] - **test**: remove checks for deserializing wasm (Matheus Marchini) [#32116](https://github.com/nodejs/node/pull/32116)
* [[`add5f6e5cd`](https://github.com/nodejs/node/commit/add5f6e5cd)] - **tls**: provide default cipher list from command line (Anna Henningsen) [#32760](https://github.com/nodejs/node/pull/32760)
* [[`405ae1909b`](https://github.com/nodejs/node/commit/405ae1909b)] - **tools**: update V8 gypfiles for 8.1 (Matheus Marchini) [#32116](https://github.com/nodejs/node/pull/32116)
* [[`7fe61222ef`](https://github.com/nodejs/node/commit/7fe61222ef)] - **worker**: mention argument name in type check message (Anna Henningsen) [#32815](https://github.com/nodejs/node/pull/32815)
* [[`7147df53e8`](https://github.com/nodejs/node/commit/7147df53e8)] - **worker**: fix type check in receiveMessageOnPort (Anna Henningsen) [#32745](https://github.com/nodejs/node/pull/32745)
* [[`0c545f0f72`](https://github.com/nodejs/node/commit/0c545f0f72)] - **zlib**: emits 'close' event after readable 'end' (Sergey Zelenov) [#32050](https://github.com/nodejs/node/pull/32050)
