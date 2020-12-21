# Node.js 15 ChangeLog

<!--lint disable prohibited-strings-->
<!--lint disable maximum-line-length-->
<!--lint disable no-literal-urls-->

<table>
<tr>
<th>Current</th>
</tr>
<tr>
<td>
<a href="#15.5.0">15.5.0</a><br/>
<a href="#15.4.0">15.4.0</a><br/>
<a href="#15.3.0">15.3.0</a><br/>
<a href="#15.2.1">15.2.1</a><br/>
<a href="#15.2.0">15.2.0</a><br/>
<a href="#15.1.0">15.1.0</a><br/>
<a href="#15.0.1">15.0.1</a><br/>
<a href="#15.0.0">15.0.0</a><br/>
</td>
</tr>
</table>

* Other Versions
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

<a id="15.5.0"></a>
## 2020-12-22, Version 15.5.0 (Current), @targos

### Notable Changes

#### Extended support for `AbortSignal` in child_process and stream

The following APIs now support an `AbortSignal` in their options object:

* `child_process.spawn()`

Calling `.abort()` on the corresponding `AbortController` is similar to calling `.kill()` on the child process except the error passed to the callback will be an `AbortError`:

```js
const controller = new AbortController();
const { signal } = controller;
const grep = spawn('grep', ['ssh'], { signal });
grep.on('error', (err) => {
  // This will be called with err being an AbortError if the controller aborts
});
controller.abort(); // stops the process
```

* `new stream.Writable()` and `new stream.Readable()`

Calling `.abort()` on the corresponding `AbortController` will behave the same way as calling `.destroy(new AbortError())` on the stream:

```js
const { Readable } = require('stream');
const controller = new AbortController();
const read = new Readable({
  read(size) {
    // ...
  },
  signal: controller.signal
});
// Later, abort the operation closing the stream
controller.abort();
```

Contributed by Benjamin Gruenbaum [#36431](https://github.com/nodejs/node/pull/36431), [#36432](https://github.com/nodejs/node/pull/36432).

#### BigInt support in `querystring.stringify()`

If `querystring.stringify()` is called with an object that contains `BigInt` values, they will now be serialized to their decimal representation instead of the empty string:

```js
const querystring = require('querystring');
console.log(querystring.stringify({ bigint: 2n ** 64n }));
// Prints: bigint=18446744073709551616
```

Contributed by Darshan Sen [#36499](https://github.com/nodejs/node/pull/36499).

#### Additions to the C++ embedder APIs

A new `IsolateSettingsFlag` is available for those calling `SetIsolateUpForNode()`: `SHOULD_NOT_SET_PREPARE_STACK_TRACE_CALLBACK` can be used to prevent Node.js from setting a custom callback to prepare stack traces.

Contributed by Shelley Vohr [#36447](https://github.com/nodejs/node/pull/36447).

---

Added `node::GetEnvironmentIsolateData()` and `node::GetArrayBufferAllocator()` to respectively get the current `IsolateData*` and, from it, the current Node.js `ArrayBufferAllocator` if there is one.

Contributed by Anna Henningsen [#36441](https://github.com/nodejs/node/pull/36441).

#### New core collaborator

With this release, we welcome a new Node.js core collaborator:

* Pooja D P [@PoojaDurgad](https://github.com/PoojaDurgad) [#36511](https://github.com/nodejs/node/pull/36511)

### Commits

#### Semver-minor commits

* [[`e449571230`](https://github.com/nodejs/node/commit/e449571230)] - **(SEMVER-MINOR)** **child_process**: add signal support to spawn (Benjamin Gruenbaum) [#36432](https://github.com/nodejs/node/pull/36432)
* [[`25d7e90386`](https://github.com/nodejs/node/commit/25d7e90386)] - **(SEMVER-MINOR)** **http**: use `autoDestroy: true` in incoming message (Daniele Belardi) [#33035](https://github.com/nodejs/node/pull/33035)
* [[`5481be8cbd`](https://github.com/nodejs/node/commit/5481be8cbd)] - **(SEMVER-MINOR)** **lib**: support BigInt in querystring.stringify (raisinten) [#36499](https://github.com/nodejs/node/pull/36499)
* [[`036ed1fafc`](https://github.com/nodejs/node/commit/036ed1fafc)] - **(SEMVER-MINOR)** **src**: add way to get IsolateData and allocator from Environment (Anna Henningsen) [#36441](https://github.com/nodejs/node/pull/36441)
* [[`e23309486b`](https://github.com/nodejs/node/commit/e23309486b)] - **(SEMVER-MINOR)** **src**: allow preventing SetPrepareStackTraceCallback (Shelley Vohr) [#36447](https://github.com/nodejs/node/pull/36447)
* [[`6ecbc1dcb3`](https://github.com/nodejs/node/commit/6ecbc1dcb3)] - **(SEMVER-MINOR)** **stream**: support abortsignal in constructor (Benjamin Gruenbaum) [#36431](https://github.com/nodejs/node/pull/36431)

#### Semver-patch commits

* [[`1330995b80`](https://github.com/nodejs/node/commit/1330995b80)] - **build,lib,test**: change whitelist to allowlist (Michaël Zasso) [#36406](https://github.com/nodejs/node/pull/36406)
* [[`dc8d1a74a6`](https://github.com/nodejs/node/commit/dc8d1a74a6)] - **deps**: upgrade npm to 7.3.0 (Ruy Adorno) [#36572](https://github.com/nodejs/node/pull/36572)
* [[`b6a31f0a70`](https://github.com/nodejs/node/commit/b6a31f0a70)] - **deps**: update archs files for OpenSSL-1.1.1i (Myles Borins) [#36520](https://github.com/nodejs/node/pull/36520)
* [[`5b49807c3f`](https://github.com/nodejs/node/commit/5b49807c3f)] - **deps**: re-enable OPENSSL\_NO\_QUIC guards (James M Snell) [#36520](https://github.com/nodejs/node/pull/36520)
* [[`309e2971a2`](https://github.com/nodejs/node/commit/309e2971a2)] - **deps**: various quic patches from akamai/openssl (Todd Short) [#36520](https://github.com/nodejs/node/pull/36520)
* [[`27fb651cbc`](https://github.com/nodejs/node/commit/27fb651cbc)] - **deps**: upgrade openssl sources to 1.1.1i (Myles Borins) [#36520](https://github.com/nodejs/node/pull/36520)
* [[`1f43aadf90`](https://github.com/nodejs/node/commit/1f43aadf90)] - **deps**: update patch and docs for openssl update (Myles Borins) [#36520](https://github.com/nodejs/node/pull/36520)
* [[`752c94d202`](https://github.com/nodejs/node/commit/752c94d202)] - **deps**: fix npm doctor tests for pre-release node (nlf) [#36543](https://github.com/nodejs/node/pull/36543)
* [[`b0393fa2ed`](https://github.com/nodejs/node/commit/b0393fa2ed)] - **deps**: upgrade npm to 7.2.0 (Myles Borins) [#36543](https://github.com/nodejs/node/pull/36543)
* [[`cb4652e91d`](https://github.com/nodejs/node/commit/cb4652e91d)] - **deps**: update to c-ares 1.17.1 (Danny Sonnenschein) [#36207](https://github.com/nodejs/node/pull/36207)
* [[`21fbcb6f81`](https://github.com/nodejs/node/commit/21fbcb6f81)] - **deps**: V8: backport 4bf051d536a1 (Anna Henningsen) [#36482](https://github.com/nodejs/node/pull/36482)
* [[`30fe0ff681`](https://github.com/nodejs/node/commit/30fe0ff681)] - **deps**: upgrade npm to 7.1.2 (Darcy Clarke) [#36487](https://github.com/nodejs/node/pull/36487)
* [[`0baa610c3e`](https://github.com/nodejs/node/commit/0baa610c3e)] - **deps**: upgrade npm to 7.1.1 (Ruy Adorno) [#36459](https://github.com/nodejs/node/pull/36459)
* [[`5929b08851`](https://github.com/nodejs/node/commit/5929b08851)] - **deps**: upgrade npm to 7.1.0 (Ruy Adorno) [#36395](https://github.com/nodejs/node/pull/36395)
* [[`deaafd5788`](https://github.com/nodejs/node/commit/deaafd5788)] - **dns**: refactor to use more primordials (Antoine du Hamel) [#36314](https://github.com/nodejs/node/pull/36314)
* [[`e30af7be33`](https://github.com/nodejs/node/commit/e30af7be33)] - **fs**: refactor to use optional chaining (ZiJian Liu) [#36524](https://github.com/nodejs/node/pull/36524)
* [[`213dcd7930`](https://github.com/nodejs/node/commit/213dcd7930)] - **http**: add test for incomingmessage destroy (Daniele Belardi) [#33035](https://github.com/nodejs/node/pull/33035)
* [[`36b4ddd382`](https://github.com/nodejs/node/commit/36b4ddd382)] - **http**: use standard args order in IncomingMEssage onError (Daniele Belardi) [#33035](https://github.com/nodejs/node/pull/33035)
* [[`60b5e696fc`](https://github.com/nodejs/node/commit/60b5e696fc)] - **http**: remove trailing space (Daniele Belardi) [#33035](https://github.com/nodejs/node/pull/33035)
* [[`f11a648d8e`](https://github.com/nodejs/node/commit/f11a648d8e)] - **http**: add comments in \_http\_incoming (Daniele Belardi) [#33035](https://github.com/nodejs/node/pull/33035)
* [[`4b81d79b58`](https://github.com/nodejs/node/commit/4b81d79b58)] - **http**: fix lint error in incoming message (Daniele Belardi) [#33035](https://github.com/nodejs/node/pull/33035)
* [[`397e31e25f`](https://github.com/nodejs/node/commit/397e31e25f)] - **http**: reafactor incoming message destroy (Daniele Belardi) [#33035](https://github.com/nodejs/node/pull/33035)
* [[`9852ebca8d`](https://github.com/nodejs/node/commit/9852ebca8d)] - **http**: do not loop over prototype in Agent (Michaël Zasso) [#36410](https://github.com/nodejs/node/pull/36410)
* [[`e46a46a4cd`](https://github.com/nodejs/node/commit/e46a46a4cd)] - **inspector**: refactor to use more primordials (Antoine du Hamel) [#36356](https://github.com/nodejs/node/pull/36356)
* [[`728f512c7d`](https://github.com/nodejs/node/commit/728f512c7d)] - **lib**: make safe primordials safe to iterate (Antoine du Hamel) [#36391](https://github.com/nodejs/node/pull/36391)
* [[`f368d697cf`](https://github.com/nodejs/node/commit/f368d697cf)] - ***Revert*** "**perf_hooks**: make PerformanceObserver an AsyncResource" (Nicolai Stange) [#36343](https://github.com/nodejs/node/pull/36343)
* [[`e2ced0d401`](https://github.com/nodejs/node/commit/e2ced0d401)] - **perf_hooks**: invoke performance\_entry\_callback via MakeSyncCallback() (Nicolai Stange) [#36343](https://github.com/nodejs/node/pull/36343)
* [[`7c903ec6c8`](https://github.com/nodejs/node/commit/7c903ec6c8)] - **repl**: disable blocking completions by default (Anna Henningsen) [#36564](https://github.com/nodejs/node/pull/36564)
* [[`d38a0ec93e`](https://github.com/nodejs/node/commit/d38a0ec93e)] - **src**: remove unnecessary ToLocalChecked node\_errors (Daniel Bevenius) [#36547](https://github.com/nodejs/node/pull/36547)
* [[`bbc0d14cd2`](https://github.com/nodejs/node/commit/bbc0d14cd2)] - **src**: use correct microtask queue for checkpoints (Anna Henningsen) [#36581](https://github.com/nodejs/node/pull/36581)
* [[`7efb3111e8`](https://github.com/nodejs/node/commit/7efb3111e8)] - **src**: remove unnecessary ToLocalChecked call (Daniel Bevenius) [#36523](https://github.com/nodejs/node/pull/36523)
* [[`68687d3419`](https://github.com/nodejs/node/commit/68687d3419)] - **src**: remove empty name check in node\_env\_var.cc (raisinten) [#36133](https://github.com/nodejs/node/pull/36133)
* [[`1b4984de98`](https://github.com/nodejs/node/commit/1b4984de98)] - **src**: remove duplicate V macros in node\_v8.cc (Daniel Bevenius) [#36454](https://github.com/nodejs/node/pull/36454)
* [[`5ff7f42e65`](https://github.com/nodejs/node/commit/5ff7f42e65)] - **src**: use correct outer Context’s microtask queue (Anna Henningsen) [#36482](https://github.com/nodejs/node/pull/36482)
* [[`96c095f237`](https://github.com/nodejs/node/commit/96c095f237)] - **src**: guard against env != null in node\_errors.cc (Anna Henningsen) [#36414](https://github.com/nodejs/node/pull/36414)
* [[`4f3d7bb417`](https://github.com/nodejs/node/commit/4f3d7bb417)] - **src**: introduce convenience node::MakeSyncCallback() (Nicolai Stange) [#36343](https://github.com/nodejs/node/pull/36343)
* [[`e59788262c`](https://github.com/nodejs/node/commit/e59788262c)] - **src**: add typedef for CleanupHookCallback callback (Daniel Bevenius) [#36442](https://github.com/nodejs/node/pull/36442)
* [[`2a60e3b9df`](https://github.com/nodejs/node/commit/2a60e3b9df)] - **src**: fix indentation in memory\_tracker-inl.h (Daniel Bevenius) [#36425](https://github.com/nodejs/node/pull/36425)
* [[`210390f6fd`](https://github.com/nodejs/node/commit/210390f6fd)] - **src**: remove identical V macro (Daniel Bevenius) [#36427](https://github.com/nodejs/node/pull/36427)
* [[`02afe586aa`](https://github.com/nodejs/node/commit/02afe586aa)] - **src**: use using declarations consistently (Daniel Bevenius) [#36365](https://github.com/nodejs/node/pull/36365)
* [[`169406b7d7`](https://github.com/nodejs/node/commit/169406b7d7)] - **src**: add missing context scopes (Anna Henningsen) [#36413](https://github.com/nodejs/node/pull/36413)
* [[`3f33d0bcda`](https://github.com/nodejs/node/commit/3f33d0bcda)] - **stream**: fix pipe deadlock when starting with needDrain (Robert Nagy) [#36563](https://github.com/nodejs/node/pull/36563)
* [[`d8b5b9499c`](https://github.com/nodejs/node/commit/d8b5b9499c)] - **stream**: accept iterable as a valid first argument (ZiJian Liu) [#36479](https://github.com/nodejs/node/pull/36479)
* [[`58319d5336`](https://github.com/nodejs/node/commit/58319d5336)] - **tls**: forward new SecureContext options (Alba Mendez) [#36416](https://github.com/nodejs/node/pull/36416)
* [[`fa40366276`](https://github.com/nodejs/node/commit/fa40366276)] - **util**: simplify constructor retrieval in inspect() (Rich Trott) [#36466](https://github.com/nodejs/node/pull/36466)
* [[`cc544dbfaa`](https://github.com/nodejs/node/commit/cc544dbfaa)] - **util**: fix instanceof checks with null prototypes during inspection (Ruben Bridgewater) [#36178](https://github.com/nodejs/node/pull/36178)
* [[`13d6597b4b`](https://github.com/nodejs/node/commit/13d6597b4b)] - **util**: fix module prefixes during inspection (Ruben Bridgewater) [#36178](https://github.com/nodejs/node/pull/36178)
* [[`20ecc82569`](https://github.com/nodejs/node/commit/20ecc82569)] - **worker**: fix broadcast channel SharedArrayBuffer passing (Anna Henningsen) [#36501](https://github.com/nodejs/node/pull/36501)
* [[`56fe9bae26`](https://github.com/nodejs/node/commit/56fe9bae26)] - **worker**: refactor MessagePort entanglement management (Anna Henningsen) [#36345](https://github.com/nodejs/node/pull/36345)

#### Documentation commits

* [[`19c233232f`](https://github.com/nodejs/node/commit/19c233232f)] - **doc**: fix AbortSignal example for stream.Readable (Michaël Zasso) [#36596](https://github.com/nodejs/node/pull/36596)
* [[`9fbab3e2f5`](https://github.com/nodejs/node/commit/9fbab3e2f5)] - **doc**: update and run license-builder for Babel (Michaël Zasso) [#36504](https://github.com/nodejs/node/pull/36504)
* [[`a1ba6686a0`](https://github.com/nodejs/node/commit/a1ba6686a0)] - **doc**: add remark about Collaborators discussion page (FrankQiu) [#36420](https://github.com/nodejs/node/pull/36420)
* [[`c5602fb166`](https://github.com/nodejs/node/commit/c5602fb166)] - **doc**: simplify worker\_threads.md text (Rich Trott) [#36545](https://github.com/nodejs/node/pull/36545)
* [[`149f2cfac1`](https://github.com/nodejs/node/commit/149f2cfac1)] - **doc**: add two tips for speeding the dev builds (Momtchil Momtchev) [#36452](https://github.com/nodejs/node/pull/36452)
* [[`ad75c78c32`](https://github.com/nodejs/node/commit/ad75c78c32)] - **doc**: add note about timingSafeEqual for TypedArray (Tobias Nießen) [#36323](https://github.com/nodejs/node/pull/36323)
* [[`9830fe5c9e`](https://github.com/nodejs/node/commit/9830fe5c9e)] - **doc**: move Derek Lewis to emeritus (Rich Trott) [#36514](https://github.com/nodejs/node/pull/36514)
* [[`eb29a16bae`](https://github.com/nodejs/node/commit/eb29a16bae)] - **doc**: add issue reference to github pr template (Chinmoy Chakraborty) [#36440](https://github.com/nodejs/node/pull/36440)
* [[`f09985d42a`](https://github.com/nodejs/node/commit/f09985d42a)] - **doc**: update url.md (Rock) [#36147](https://github.com/nodejs/node/pull/36147)
* [[`c3ec90d23c`](https://github.com/nodejs/node/commit/c3ec90d23c)] - **doc**: make explicit reverting node\_version.h changes (Richard Lau) [#36461](https://github.com/nodejs/node/pull/36461)
* [[`7a34452b1d`](https://github.com/nodejs/node/commit/7a34452b1d)] - **doc**: add license info to the README (FrankQiu) [#36278](https://github.com/nodejs/node/pull/36278)
* [[`22f039339f`](https://github.com/nodejs/node/commit/22f039339f)] - **doc**: revise addon mulitple initializations text (Rich Trott) [#36457](https://github.com/nodejs/node/pull/36457)
* [[`25a245443a`](https://github.com/nodejs/node/commit/25a245443a)] - **doc**: add v15.4.0 link to CHANGELOG.md (Danielle Adams) [#36456](https://github.com/nodejs/node/pull/36456)
* [[`1ec8516fd6`](https://github.com/nodejs/node/commit/1ec8516fd6)] - **doc**: add PoojaDurgad to collaborators (Pooja D P) [#36511](https://github.com/nodejs/node/pull/36511)
* [[`98918110a1`](https://github.com/nodejs/node/commit/98918110a1)] - **doc**: edit addon text about event loop blocking (Rich Trott) [#36448](https://github.com/nodejs/node/pull/36448)
* [[`62bfe3d313`](https://github.com/nodejs/node/commit/62bfe3d313)] - **doc**: note v15.0.0 changed default --unhandled-rejections=throw (kai zhu) [#36361](https://github.com/nodejs/node/pull/36361)
* [[`129053fe4c`](https://github.com/nodejs/node/commit/129053fe4c)] - **doc**: update terminology (Michael Dawson) [#36475](https://github.com/nodejs/node/pull/36475)
* [[`e331de2571`](https://github.com/nodejs/node/commit/e331de2571)] - **doc**: reword POSIX threads text in addons.md (Rich Trott) [#36436](https://github.com/nodejs/node/pull/36436)
* [[`04f166389b`](https://github.com/nodejs/node/commit/04f166389b)] - **doc**: add RaisinTen as a triager (raisinten) [#36404](https://github.com/nodejs/node/pull/36404)
* [[`3341b2cb9d`](https://github.com/nodejs/node/commit/3341b2cb9d)] - **doc**: document ABORT\_ERR code (Benjamin Gruenbaum) [#36319](https://github.com/nodejs/node/pull/36319)
* [[`6a6b3af736`](https://github.com/nodejs/node/commit/6a6b3af736)] - **doc**: provide more context on techinical values (Michael Dawson) [#36201](https://github.com/nodejs/node/pull/36201)

#### Other commits

* [[`e1f00fd996`](https://github.com/nodejs/node/commit/e1f00fd996)] - **benchmark**: reduce code duplication (Rich Trott) [#36568](https://github.com/nodejs/node/pull/36568)
* [[`82a26268d7`](https://github.com/nodejs/node/commit/82a26268d7)] - **build**: do not run GitHub actions for draft PRs (Michaël Zasso) [#35910](https://github.com/nodejs/node/pull/35910)
* [[`95c80f5fb0`](https://github.com/nodejs/node/commit/95c80f5fb0)] - **build**: run some workflows only on nodejs/node (Michaël Zasso) [#36507](https://github.com/nodejs/node/pull/36507)
* [[`584ea8b26c`](https://github.com/nodejs/node/commit/584ea8b26c)] - **build**: fix make test-npm (Ruy Adorno) [#36369](https://github.com/nodejs/node/pull/36369)
* [[`01576fbc19`](https://github.com/nodejs/node/commit/01576fbc19)] - **test**: increase abort logic coverage (Moshe vilner) [#36586](https://github.com/nodejs/node/pull/36586)
* [[`22ac2279ee`](https://github.com/nodejs/node/commit/22ac2279ee)] - **test**: increase coverage for stream (ZiJian Liu) [#36538](https://github.com/nodejs/node/pull/36538)
* [[`9fc2479707`](https://github.com/nodejs/node/commit/9fc2479707)] - **test**: increase coverage for worker (ZiJian Liu) [#36491](https://github.com/nodejs/node/pull/36491)
* [[`81e603b7cf`](https://github.com/nodejs/node/commit/81e603b7cf)] - **test**: specify global object for globals (Rich Trott) [#36498](https://github.com/nodejs/node/pull/36498)
* [[`109ab787fd`](https://github.com/nodejs/node/commit/109ab787fd)] - **test**: increase coverage for fs/dir read (Zijian Liu) [#36388](https://github.com/nodejs/node/pull/36388)
* [[`9f2d3c291b`](https://github.com/nodejs/node/commit/9f2d3c291b)] - **test**: remove test-http2-client-upload as flaky (Rich Trott) [#36496](https://github.com/nodejs/node/pull/36496)
* [[`d299ceeac7`](https://github.com/nodejs/node/commit/d299ceeac7)] - **test**: increase coverage for net/blocklist (Zijian Liu) [#36405](https://github.com/nodejs/node/pull/36405)
* [[`f7635fd86d`](https://github.com/nodejs/node/commit/f7635fd86d)] - **test**: make executable name more general (Shelley Vohr) [#36489](https://github.com/nodejs/node/pull/36489)
* [[`acd78d9d25`](https://github.com/nodejs/node/commit/acd78d9d25)] - **test**: increased externalized string length (Shelley Vohr) [#36451](https://github.com/nodejs/node/pull/36451)
* [[`0f749a35ec`](https://github.com/nodejs/node/commit/0f749a35ec)] - **test**: add test for async contexts in PerformanceObserver (ZauberNerd) [#36343](https://github.com/nodejs/node/pull/36343)
* [[`dd705ad1f0`](https://github.com/nodejs/node/commit/dd705ad1f0)] - **test**: increase execFile abort coverage (Moshe vilner) [#36429](https://github.com/nodejs/node/pull/36429)
* [[`31b062d591`](https://github.com/nodejs/node/commit/31b062d591)] - **test**: fix flaky test-repl (Rich Trott) [#36415](https://github.com/nodejs/node/pull/36415)
* [[`023291b43c`](https://github.com/nodejs/node/commit/023291b43c)] - **test**: check null proto-of-proto in util.inspect() (Rich Trott) [#36399](https://github.com/nodejs/node/pull/36399)
* [[`d3d1f338c7`](https://github.com/nodejs/node/commit/d3d1f338c7)] - **test**: add SIGTRAP to test-signal-handler (Ash Cripps) [#36368](https://github.com/nodejs/node/pull/36368)
* [[`166aa8a7b5`](https://github.com/nodejs/node/commit/166aa8a7b5)] - **test**: fix child-process-pipe-dataflow (Santiago Gimeno) [#36366](https://github.com/nodejs/node/pull/36366)
* [[`ecbb757ae0`](https://github.com/nodejs/node/commit/ecbb757ae0)] - **tools**: fix make-v8.sh (Richard Lau) [#36594](https://github.com/nodejs/node/pull/36594)
* [[`e3c5adc6d0`](https://github.com/nodejs/node/commit/e3c5adc6d0)] - **tools**: fix release script sign function (Antoine du Hamel) [#36556](https://github.com/nodejs/node/pull/36556)
* [[`0d4d34748d`](https://github.com/nodejs/node/commit/0d4d34748d)] - **tools**: update ESLint to 7.16.0 (Yongsheng Zhang) [#36579](https://github.com/nodejs/node/pull/36579)
* [[`f3828c9dcb`](https://github.com/nodejs/node/commit/f3828c9dcb)] - **tools**: fix update-eslint.sh (Yongsheng Zhang) [#36579](https://github.com/nodejs/node/pull/36579)
* [[`27260c70b4`](https://github.com/nodejs/node/commit/27260c70b4)] - **tools**: fix release script (Antoine du Hamel) [#36540](https://github.com/nodejs/node/pull/36540)
* [[`c6700ad041`](https://github.com/nodejs/node/commit/c6700ad041)] - **tools**: remove unused variable in configure.py (Rich Trott) [#36525](https://github.com/nodejs/node/pull/36525)
* [[`7b8d373d5e`](https://github.com/nodejs/node/commit/7b8d373d5e)] - **tools**: lint shell scripts (Antoine du Hamel) [#36099](https://github.com/nodejs/node/pull/36099)
* [[`c6e65d09ef`](https://github.com/nodejs/node/commit/c6e65d09ef)] - **tools**: update ini in tools/node-lint-md-cli-rollup (Myles Borins) [#36474](https://github.com/nodejs/node/pull/36474)
* [[`7542a3bd55`](https://github.com/nodejs/node/commit/7542a3bd55)] - **tools**: enable no-unsafe-optional-chaining lint rule (Colin Ihrig) [#36411](https://github.com/nodejs/node/pull/36411)
* [[`26f8ccfbe6`](https://github.com/nodejs/node/commit/26f8ccfbe6)] - **tools**: update ESLint to 7.15.0 (Colin Ihrig) [#36411](https://github.com/nodejs/node/pull/36411)
* [[`8ecf2f9976`](https://github.com/nodejs/node/commit/8ecf2f9976)] - **tools**: update doc tool dependencies (Michaël Zasso) [#36407](https://github.com/nodejs/node/pull/36407)
* [[`040b39f076`](https://github.com/nodejs/node/commit/040b39f076)] - **tools**: enable no-unused-expressions lint rule (Michaël Zasso) [#36248](https://github.com/nodejs/node/pull/36248)

<a id="15.4.0"></a>
## 2020-12-09, Version 15.4.0 (Current), @danielleadams

### Notable Changes

* **child_processes**:
  * add AbortSignal support (Benjamin Gruenbaum) [#36308](https://github.com/nodejs/node/pull/36308)
* **deps**:
  * update ICU to 68.1 (Michaël Zasso) [#36187](https://github.com/nodejs/node/pull/36187)
* **events**:
  * support signal in EventTarget (Benjamin Gruenbaum) [#36258](https://github.com/nodejs/node/pull/36258)
  * graduate Event, EventTarget, AbortController (James M Snell) [#35949](https://github.com/nodejs/node/pull/35949)
* **http**:
  * enable call chaining with setHeader() (pooja d.p) [#35924](https://github.com/nodejs/node/pull/35924)
* **module**:
  * add isPreloading indicator (James M Snell) [#36263](https://github.com/nodejs/node/pull/36263)
* **stream**:
  * support abort signal (Benjamin Gruenbaum) [#36061](https://github.com/nodejs/node/pull/36061)
  * add FileHandle support to Read/WriteStream (Momtchil Momtchev) [#35922](https://github.com/nodejs/node/pull/35922)
* **worker**:
  * add experimental BroadcastChannel (James M Snell) [#36271](https://github.com/nodejs/node/pull/36271)

### Commits

* [[`e79bdc313a`](https://github.com/nodejs/node/commit/e79bdc313a)] - **assert**: refactor to use more primordials (Antoine du Hamel) [#36234](https://github.com/nodejs/node/pull/36234)
* [[`2344e3e360`](https://github.com/nodejs/node/commit/2344e3e360)] - **benchmark**: changed `fstat` to `fstatSync` (Narasimha Prasanna HN) [#36206](https://github.com/nodejs/node/pull/36206)
* [[`ca8db41151`](https://github.com/nodejs/node/commit/ca8db41151)] - **benchmark,child_process**: remove failing benchmark parameter (Antoine du Hamel) [#36295](https://github.com/nodejs/node/pull/36295)
* [[`9db9be774b`](https://github.com/nodejs/node/commit/9db9be774b)] - **buffer**: refactor to use primordials instead of Array#reduce (Antoine du Hamel) [#36392](https://github.com/nodejs/node/pull/36392)
* [[`8d8d2261a5`](https://github.com/nodejs/node/commit/8d8d2261a5)] - **buffer**: refactor to use more primordials (Antoine du Hamel) [#36166](https://github.com/nodejs/node/pull/36166)
* [[`74adc441c4`](https://github.com/nodejs/node/commit/74adc441c4)] - **build**: fix typo in Makefile (raisinten) [#36176](https://github.com/nodejs/node/pull/36176)
* [[`224a6471cc`](https://github.com/nodejs/node/commit/224a6471cc)] - **(SEMVER-MINOR)** **child_process**: add AbortSignal support (Benjamin Gruenbaum) [#36308](https://github.com/nodejs/node/pull/36308)
* [[`4ca1bd8806`](https://github.com/nodejs/node/commit/4ca1bd8806)] - **child_process**: refactor to use more primordials (Zijian Liu) [#36269](https://github.com/nodejs/node/pull/36269)
* [[`841e8f444e`](https://github.com/nodejs/node/commit/841e8f444e)] - **crypto**: fix "Invalid JWK" error messages (Filip Skokan) [#36200](https://github.com/nodejs/node/pull/36200)
* [[`278862aeb9`](https://github.com/nodejs/node/commit/278862aeb9)] - **deps**: upgrade npm to 7.0.15 (Ruy Adorno) [#36293](https://github.com/nodejs/node/pull/36293)
* [[`66bc2067ce`](https://github.com/nodejs/node/commit/66bc2067ce)] - **deps**: V8: cherry-pick 86991d0587a1 (Benjamin Coe) [#36254](https://github.com/nodejs/node/pull/36254)
* [[`095cef2c11`](https://github.com/nodejs/node/commit/095cef2c11)] - **deps**: update ICU to 68.1 (Michaël Zasso) [#36187](https://github.com/nodejs/node/pull/36187)
* [[`8d69d8387e`](https://github.com/nodejs/node/commit/8d69d8387e)] - **dgram**: refactor to use more primordials (Antoine du Hamel) [#36286](https://github.com/nodejs/node/pull/36286)
* [[`bef550a50c`](https://github.com/nodejs/node/commit/bef550a50c)] - **doc**: add Powershell oneliner to get Windows version (Michael Bashurov) [#30289](https://github.com/nodejs/node/pull/30289)
* [[`2649c384c6`](https://github.com/nodejs/node/commit/2649c384c6)] - **doc**: add version metadata to timers/promises (Colin Ihrig) [#36378](https://github.com/nodejs/node/pull/36378)
* [[`0401ffbfb6`](https://github.com/nodejs/node/commit/0401ffbfb6)] - **doc**: add process for handling premature disclosure (Michael Dawson) [#36155](https://github.com/nodejs/node/pull/36155)
* [[`3e5fcda13e`](https://github.com/nodejs/node/commit/3e5fcda13e)] - **doc**: add table header in intl.md (Rich Trott) [#36261](https://github.com/nodejs/node/pull/36261)
* [[`65d89fdd69`](https://github.com/nodejs/node/commit/65d89fdd69)] - **doc**: adding example to Buffer.isBuffer method (naortedgi) [#36233](https://github.com/nodejs/node/pull/36233)
* [[`03cf8dbc0e`](https://github.com/nodejs/node/commit/03cf8dbc0e)] - **doc**: fix typo in events.md (Luigi Pinca) [#36231](https://github.com/nodejs/node/pull/36231)
* [[`b176d61e8c`](https://github.com/nodejs/node/commit/b176d61e8c)] - **doc**: fix --experimental-wasm-modules text location (Colin Ihrig) [#36220](https://github.com/nodejs/node/pull/36220)
* [[`44c4aaddad`](https://github.com/nodejs/node/commit/44c4aaddad)] - **doc**: stabilize subpath patterns (Guy Bedford) [#36177](https://github.com/nodejs/node/pull/36177)
* [[`fdf5d851d0`](https://github.com/nodejs/node/commit/fdf5d851d0)] - **doc**: add missing version to update cmd (Ruy Adorno) [#36204](https://github.com/nodejs/node/pull/36204)
* [[`186ad24fdf`](https://github.com/nodejs/node/commit/186ad24fdf)] - **doc**: cleanup events.md structure (James M Snell) [#36100](https://github.com/nodejs/node/pull/36100)
* [[`c14512b9a5`](https://github.com/nodejs/node/commit/c14512b9a5)] - **errors**: display original symbol name (Benjamin Coe) [#36042](https://github.com/nodejs/node/pull/36042)
* [[`855a85c124`](https://github.com/nodejs/node/commit/855a85c124)] - **(SEMVER-MINOR)** **events**: support signal in EventTarget (Benjamin Gruenbaum) [#36258](https://github.com/nodejs/node/pull/36258)
* [[`dc1930923b`](https://github.com/nodejs/node/commit/dc1930923b)] - **(SEMVER-MINOR)** **events**: graduate Event, EventTarget, AbortController (James M Snell) [#35949](https://github.com/nodejs/node/pull/35949)
* [[`537e5cbf51`](https://github.com/nodejs/node/commit/537e5cbf51)] - **fs**: move method definition from header (Yash Ladha) [#36256](https://github.com/nodejs/node/pull/36256)
* [[`744b8aa807`](https://github.com/nodejs/node/commit/744b8aa807)] - **fs**: pass ERR\_DIR\_CLOSED asynchronously to dir.close (Zijian Liu) [#36243](https://github.com/nodejs/node/pull/36243)
* [[`c04a2df185`](https://github.com/nodejs/node/commit/c04a2df185)] - **fs**: refactor to use more primordials (Antoine du Hamel) [#36196](https://github.com/nodejs/node/pull/36196)
* [[`58abdcaceb`](https://github.com/nodejs/node/commit/58abdcaceb)] - **(SEMVER-MINOR)** **http**: enable call chaining with setHeader() (pooja d.p) [#35924](https://github.com/nodejs/node/pull/35924)
* [[`cedf51f3ce`](https://github.com/nodejs/node/commit/cedf51f3ce)] - **http2**: refactor to use more primordials (Antoine du Hamel) [#36357](https://github.com/nodejs/node/pull/36357)
* [[`5f41f1b19e`](https://github.com/nodejs/node/commit/5f41f1b19e)] - **http2**: check write not scheduled in scope destructor (David Halls) [#36241](https://github.com/nodejs/node/pull/36241)
* [[`4127eb2405`](https://github.com/nodejs/node/commit/4127eb2405)] - **https**: add abortcontroller test (Benjamin Gruenbaum) [#36307](https://github.com/nodejs/node/pull/36307)
* [[`c2938bde6c`](https://github.com/nodejs/node/commit/c2938bde6c)] - **lib**: add uncurried accessor properties to `primordials` (ExE Boss) [#36329](https://github.com/nodejs/node/pull/36329)
* [[`f73a0a8069`](https://github.com/nodejs/node/commit/f73a0a8069)] - **lib**: fix typo in internal/errors.js (raisinten) [#36426](https://github.com/nodejs/node/pull/36426)
* [[`617cb58cc8`](https://github.com/nodejs/node/commit/617cb58cc8)] - **lib**: refactor primordials.uncurryThis (Antoine du Hamel) [#36221](https://github.com/nodejs/node/pull/36221)
* [[`cc18907ec4`](https://github.com/nodejs/node/commit/cc18907ec4)] - **module**: refactor to use more primordials (Antoine du Hamel) [#36348](https://github.com/nodejs/node/pull/36348)
* [[`d4de7c7eb9`](https://github.com/nodejs/node/commit/d4de7c7eb9)] - **(SEMVER-MINOR)** **module**: add isPreloading indicator (James M Snell) [#36263](https://github.com/nodejs/node/pull/36263)
* [[`8611b8f98a`](https://github.com/nodejs/node/commit/8611b8f98a)] - **net**: refactor to use more primordials (Antoine du Hamel) [#36303](https://github.com/nodejs/node/pull/36303)
* [[`2a24096720`](https://github.com/nodejs/node/commit/2a24096720)] - **os**: refactor to use more primordials (Antoine du Hamel) [#36284](https://github.com/nodejs/node/pull/36284)
* [[`0e7f0c6d27`](https://github.com/nodejs/node/commit/0e7f0c6d27)] - **path**: refactor to use more primordials (Antoine du Hamel) [#36302](https://github.com/nodejs/node/pull/36302)
* [[`ea46ca8cbf`](https://github.com/nodejs/node/commit/ea46ca8cbf)] - **perf_hooks**: refactor to use more primordials (Antoine du Hamel) [#36297](https://github.com/nodejs/node/pull/36297)
* [[`a9ac86d1ee`](https://github.com/nodejs/node/commit/a9ac86d1ee)] - **policy**: refactor to use more primordials (Antoine du Hamel) [#36210](https://github.com/nodejs/node/pull/36210)
* [[`39d0ceda48`](https://github.com/nodejs/node/commit/39d0ceda48)] - **process**: refactor to use more primordials (Antoine du Hamel) [#36212](https://github.com/nodejs/node/pull/36212)
* [[`ab084c199e`](https://github.com/nodejs/node/commit/ab084c199e)] - **querystring**: refactor to use more primordials (Antoine du Hamel) [#36315](https://github.com/nodejs/node/pull/36315)
* [[`d29199ef82`](https://github.com/nodejs/node/commit/d29199ef82)] - **quic**: refactor to use more primordials (Antoine du Hamel) [#36211](https://github.com/nodejs/node/pull/36211)
* [[`b885409e48`](https://github.com/nodejs/node/commit/b885409e48)] - **readline**: refactor to use more primordials (Antoine du Hamel) [#36296](https://github.com/nodejs/node/pull/36296)
* [[`9cb53f635a`](https://github.com/nodejs/node/commit/9cb53f635a)] - **repl**: refactor to use more primordials (Antoine du Hamel) [#36264](https://github.com/nodejs/node/pull/36264)
* [[`8dadaa652e`](https://github.com/nodejs/node/commit/8dadaa652e)] - **src**: remove some duplication in DeserializeProps (Daniel Bevenius) [#36336](https://github.com/nodejs/node/pull/36336)
* [[`a03aa0a6b2`](https://github.com/nodejs/node/commit/a03aa0a6b2)] - **src**: rename AliasedBufferInfo-\>AliasedBufferIndex (Daniel Bevenius) [#36339](https://github.com/nodejs/node/pull/36339)
* [[`e7b2d91e04`](https://github.com/nodejs/node/commit/e7b2d91e04)] - **src**: use transferred consistently (Daniel Bevenius) [#36340](https://github.com/nodejs/node/pull/36340)
* [[`6ebb98af11`](https://github.com/nodejs/node/commit/6ebb98af11)] - **src**: use ToLocal in DeserializeProperties (Daniel Bevenius) [#36279](https://github.com/nodejs/node/pull/36279)
* [[`47397ffd56`](https://github.com/nodejs/node/commit/47397ffd56)] - **src**: update node.rc file description (devsnek) [#36197](https://github.com/nodejs/node/pull/36197)
* [[`cfc8ec18db`](https://github.com/nodejs/node/commit/cfc8ec18db)] - **src**: fix label indentation (Rich Trott) [#36213](https://github.com/nodejs/node/pull/36213)
* [[`197ba21279`](https://github.com/nodejs/node/commit/197ba21279)] - **(SEMVER-MINOR)** **stream**: support abort signal (Benjamin Gruenbaum) [#36061](https://github.com/nodejs/node/pull/36061)
* [[`6033d30361`](https://github.com/nodejs/node/commit/6033d30361)] - **(SEMVER-MINOR)** **stream**: add FileHandle support to Read/WriteStream (Momtchil Momtchev) [#35922](https://github.com/nodejs/node/pull/35922)
* [[`a15addc153`](https://github.com/nodejs/node/commit/a15addc153)] - **string_decoder**: refactor to use more primordials (Antoine du Hamel) [#36358](https://github.com/nodejs/node/pull/36358)
* [[`b39d150e60`](https://github.com/nodejs/node/commit/b39d150e60)] - **test**: fix comment misspellings of transferred (Rich Trott) [#36360](https://github.com/nodejs/node/pull/36360)
* [[`a7e794d1bf`](https://github.com/nodejs/node/commit/a7e794d1bf)] - **test**: fix flaky test-http2-respond-file-error-pipe-offset (Rich Trott) [#36305](https://github.com/nodejs/node/pull/36305)
* [[`1091a658e1`](https://github.com/nodejs/node/commit/1091a658e1)] - **test**: fix bootstrap test (Benjamin Gruenbaum) [#36418](https://github.com/nodejs/node/pull/36418)
* [[`fbcb72a665`](https://github.com/nodejs/node/commit/fbcb72a665)] - **test**: increase coverage for readline (Zijian Liu) [#36389](https://github.com/nodejs/node/pull/36389)
* [[`22028aae54`](https://github.com/nodejs/node/commit/22028aae54)] - **test**: skip flaky parts of broadcastchannel test on Windows (Rich Trott) [#36386](https://github.com/nodejs/node/pull/36386)
* [[`faca2b829e`](https://github.com/nodejs/node/commit/faca2b829e)] - **test**: fix test-worker-broadcastchannel-wpt (Rich Trott) [#36353](https://github.com/nodejs/node/pull/36353)
* [[`ea09da492c`](https://github.com/nodejs/node/commit/ea09da492c)] - **test**: fix typo in comment (inokawa) [#36312](https://github.com/nodejs/node/pull/36312)
* [[`b61ca1bfe6`](https://github.com/nodejs/node/commit/b61ca1bfe6)] - **test**: replace anonymous functions by arrows (Aleksandr Krutko) [#36125](https://github.com/nodejs/node/pull/36125)
* [[`2c7358ef43`](https://github.com/nodejs/node/commit/2c7358ef43)] - **test**: fix flaky sequential/test-fs-watch (Rich Trott) [#36249](https://github.com/nodejs/node/pull/36249)
* [[`b613950016`](https://github.com/nodejs/node/commit/b613950016)] - **test**: increase coverage for util.inspect() (Rich Trott) [#36228](https://github.com/nodejs/node/pull/36228)
* [[`69a8f05488`](https://github.com/nodejs/node/commit/69a8f05488)] - **test**: improve test coverage SourceMap API (Juan José Arboleda) [#36089](https://github.com/nodejs/node/pull/36089)
* [[`44d6d0bf0d`](https://github.com/nodejs/node/commit/44d6d0bf0d)] - **test**: fix missed warning for non-experimental AbortController (James M Snell) [#36240](https://github.com/nodejs/node/pull/36240)
* [[`29b5236256`](https://github.com/nodejs/node/commit/29b5236256)] - **timers**: reject with AbortError on cancellation (Benjamin Gruenbaum) [#36317](https://github.com/nodejs/node/pull/36317)
* [[`b20409e985`](https://github.com/nodejs/node/commit/b20409e985)] - **tls**: refactor to use more primordials (Antoine du Hamel) [#36266](https://github.com/nodejs/node/pull/36266)
* [[`f317bba034`](https://github.com/nodejs/node/commit/f317bba034)] - **tls**: permit null as a cipher value (Rich Trott) [#36318](https://github.com/nodejs/node/pull/36318)
* [[`9ae59c847a`](https://github.com/nodejs/node/commit/9ae59c847a)] - **tools**: upgrade to @babel/eslint-parser 7.12.1 (Antoine du Hamel) [#36321](https://github.com/nodejs/node/pull/36321)
* [[`e798770803`](https://github.com/nodejs/node/commit/e798770803)] - **tools**: refloat 7 Node.js patches to cpplint.py (Rich Trott) [#36324](https://github.com/nodejs/node/pull/36324)
* [[`a8b95cfcb2`](https://github.com/nodejs/node/commit/a8b95cfcb2)] - **tools**: bump cpplint to 1.5.4 (Rich Trott) [#36324](https://github.com/nodejs/node/pull/36324)
* [[`754b7a76b1`](https://github.com/nodejs/node/commit/754b7a76b1)] - **tools**: remove bashisms from macOS release scripts (Antoine du Hamel) [#36121](https://github.com/nodejs/node/pull/36121)
* [[`2868ffb331`](https://github.com/nodejs/node/commit/2868ffb331)] - **tools**: remove bashisms from release script (Antoine du Hamel) [#36123](https://github.com/nodejs/node/pull/36123)
* [[`8cf1addaa8`](https://github.com/nodejs/node/commit/8cf1addaa8)] - **tools**: update stability index linking logic (Rich Trott) [#36280](https://github.com/nodejs/node/pull/36280)
* [[`d95ae65986`](https://github.com/nodejs/node/commit/d95ae65986)] - **tools**: update highlight.js to 10.1.2 (Myles Borins) [#36309](https://github.com/nodejs/node/pull/36309)
* [[`5935ccc11c`](https://github.com/nodejs/node/commit/5935ccc11c)] - **tools**: fix undeclared identifier FALSE (Antoine du Hamel) [#36276](https://github.com/nodejs/node/pull/36276)
* [[`a2da7ba914`](https://github.com/nodejs/node/commit/a2da7ba914)] - **tools**: use using-declaration consistently (Daniel Bevenius) [#36245](https://github.com/nodejs/node/pull/36245)
* [[`82c1e39c4a`](https://github.com/nodejs/node/commit/82c1e39c4a)] - **tools**: refloat 7 Node.js patches to cpplint.py (Rich Trott) [#36235](https://github.com/nodejs/node/pull/36235)
* [[`bcf7393412`](https://github.com/nodejs/node/commit/bcf7393412)] - **tools**: bump cpplint to 1.5.3 (Rich Trott) [#36235](https://github.com/nodejs/node/pull/36235)
* [[`be11976407`](https://github.com/nodejs/node/commit/be11976407)] - **tools**: enable no-nonoctal-decimal-escape lint rule (Colin Ihrig) [#36217](https://github.com/nodejs/node/pull/36217)
* [[`c86c2399a2`](https://github.com/nodejs/node/commit/c86c2399a2)] - **tools**: update ESLint to 7.14.0 (Colin Ihrig) [#36217](https://github.com/nodejs/node/pull/36217)
* [[`cfadd82cf3`](https://github.com/nodejs/node/commit/cfadd82cf3)] - **tools**: refloat 7 Node.js patches to cpplint.py (Rich Trott) [#36213](https://github.com/nodejs/node/pull/36213)
* [[`03e8aaf613`](https://github.com/nodejs/node/commit/03e8aaf613)] - **tools**: bump cpplint.py to 1.5.2 (Rich Trott) [#36213](https://github.com/nodejs/node/pull/36213)
* [[`6bc007fc94`](https://github.com/nodejs/node/commit/6bc007fc94)] - **tty**: refactor to use more primordials (Zijian Liu) [#36272](https://github.com/nodejs/node/pull/36272)
* [[`fbd5652943`](https://github.com/nodejs/node/commit/fbd5652943)] - **v8**: refactor to use more primordials (Antoine du Hamel) [#36285](https://github.com/nodejs/node/pull/36285)
* [[`8731a80439`](https://github.com/nodejs/node/commit/8731a80439)] - **vm**: add `SafeForTerminationScope`s for SIGINT interruptions (Anna Henningsen) [#36344](https://github.com/nodejs/node/pull/36344)
* [[`47345a1f84`](https://github.com/nodejs/node/commit/47345a1f84)] - **worker**: refactor to use more primordials (Antoine du Hamel) [#36393](https://github.com/nodejs/node/pull/36393)
* [[`21c4704c7b`](https://github.com/nodejs/node/commit/21c4704c7b)] - **worker**: refactor to use more primordials (Antoine du Hamel) [#36267](https://github.com/nodejs/node/pull/36267)
* [[`802d44b1a9`](https://github.com/nodejs/node/commit/802d44b1a9)] - **(SEMVER-MINOR)** **worker**: add experimental BroadcastChannel (James M Snell) [#36271](https://github.com/nodejs/node/pull/36271)
* [[`4b4caada9f`](https://github.com/nodejs/node/commit/4b4caada9f)] - **zlib**: refactor to use more primordials (Antoine du Hamel) [#36347](https://github.com/nodejs/node/pull/36347)

<a id="15.3.0"></a>
## 2020-11-24, Version 15.3.0 (Current), @codebytere

### Notable Changes

* [[`6349b1d673`](https://github.com/nodejs/node/commit/6349b1d673)] - **(SEMVER-MINOR)** **dns**: add a cancel() method to the promise Resolver (Szymon Marczak) [#33099](https://github.com/nodejs/node/pull/33099)
* [[`9ce9b016e6`](https://github.com/nodejs/node/commit/9ce9b016e6)] - **(SEMVER-MINOR)** **events**: add max listener warning for EventTarget (James M Snell) [#36001](https://github.com/nodejs/node/pull/36001)
* [[`8390f8a86b`](https://github.com/nodejs/node/commit/8390f8a86b)] - **(SEMVER-MINOR)** **http**: add support for abortsignal to http.request (Benjamin Gruenbaum) [#36048](https://github.com/nodejs/node/pull/36048)
* [[`9c6be3cc90`](https://github.com/nodejs/node/commit/9c6be3cc90)] - **(SEMVER-MINOR)** **http2**: allow setting the local window size of a session (Yongsheng Zhang) [#35978](https://github.com/nodejs/node/pull/35978)
* [[`15ff155c12`](https://github.com/nodejs/node/commit/15ff155c12)] - **(SEMVER-MINOR)** **lib**: add throws option to fs.f/l/statSync (Andrew Casey) [#33716](https://github.com/nodejs/node/pull/33716)
* [[`85c85d368a`](https://github.com/nodejs/node/commit/85c85d368a)] - **(SEMVER-MINOR)** **path**: add `path/posix` and `path/win32` alias modules (ExE Boss) [#34962](https://github.com/nodejs/node/pull/34962)
* [[`d1baae3640`](https://github.com/nodejs/node/commit/d1baae3640)] - **(SEMVER-MINOR)** **readline**: add getPrompt to get the current prompt (Mattias Runge-Broberg) [#33675](https://github.com/nodejs/node/pull/33675)
* [[`5729478509`](https://github.com/nodejs/node/commit/5729478509)] - **(SEMVER-MINOR)** **src**: add loop idle time in diagnostic report (Gireesh Punathil) [#35940](https://github.com/nodejs/node/pull/35940)
* [[`baa87c1a7d`](https://github.com/nodejs/node/commit/baa87c1a7d)] - **(SEMVER-MINOR)** **util**: add `util/types` alias module (ExE Boss) [#34055](https://github.com/nodejs/node/pull/34055)

### Commits

* [[`34aa0c868e`](https://github.com/nodejs/node/commit/34aa0c868e)] - **assert**: refactor to use more primordials (Antoine du Hamel) [#35998](https://github.com/nodejs/node/pull/35998)
* [[`28d710164a`](https://github.com/nodejs/node/commit/28d710164a)] - **async_hooks**: refactor to use more primordials (Antoine du Hamel) [#36168](https://github.com/nodejs/node/pull/36168)
* [[`1924255fdb`](https://github.com/nodejs/node/commit/1924255fdb)] - **async_hooks**: fix leak in AsyncLocalStorage exit (Stephen Belanger) [#35779](https://github.com/nodejs/node/pull/35779)
* [[`3ee556a867`](https://github.com/nodejs/node/commit/3ee556a867)] - **benchmark**: fix build warnings (Gabriel Schulhof) [#36157](https://github.com/nodejs/node/pull/36157)
* [[`fcc38a1312`](https://github.com/nodejs/node/commit/fcc38a1312)] - **build**: replace which with command -v (raisinten) [#36118](https://github.com/nodejs/node/pull/36118)
* [[`60874ba941`](https://github.com/nodejs/node/commit/60874ba941)] - **build**: try “python3” as a last resort for 3.x (Ole André Vadla Ravnås) [#35983](https://github.com/nodejs/node/pull/35983)
* [[`fbe210b2a1`](https://github.com/nodejs/node/commit/fbe210b2a1)] - **build**: conditionally clear vcinstalldir (Brian Ingenito) [#36009](https://github.com/nodejs/node/pull/36009)
* [[`56f83e6876`](https://github.com/nodejs/node/commit/56f83e6876)] - **build**: refactor configure.py to use argparse (raisinten) [#35755](https://github.com/nodejs/node/pull/35755)
* [[`0b70822461`](https://github.com/nodejs/node/commit/0b70822461)] - **child_process**: refactor to use more primordials (Antoine du Hamel) [#36003](https://github.com/nodejs/node/pull/36003)
* [[`e54108f2e4`](https://github.com/nodejs/node/commit/e54108f2e4)] - **cluster**: refactor to use more primordials (Antoine du Hamel) [#36011](https://github.com/nodejs/node/pull/36011)
* [[`272fc794b2`](https://github.com/nodejs/node/commit/272fc794b2)] - **crypto**: fix format warning in AdditionalConfig (raisinten) [#36060](https://github.com/nodejs/node/pull/36060)
* [[`63a138e02f`](https://github.com/nodejs/node/commit/63a138e02f)] - **crypto**: fix passing TypedArray to webcrypto AES methods (Antoine du Hamel) [#36087](https://github.com/nodejs/node/pull/36087)
* [[`4a88c73fa5`](https://github.com/nodejs/node/commit/4a88c73fa5)] - **deps**: upgrade npm to 7.0.14 (nlf) [#36238](https://github.com/nodejs/node/pull/36238)
* [[`d16e8622a7`](https://github.com/nodejs/node/commit/d16e8622a7)] - **deps**: upgrade npm to 7.0.13 (Ruy Adorno) [#36202](https://github.com/nodejs/node/pull/36202)
* [[`c23ee3744f`](https://github.com/nodejs/node/commit/c23ee3744f)] - **deps**: upgrade npm to 7.0.12 (Ruy Adorno) [#36153](https://github.com/nodejs/node/pull/36153)
* [[`0fcbb1c0d5`](https://github.com/nodejs/node/commit/0fcbb1c0d5)] - **deps**: V8: cherry-pick 3176bfd447a9 (Anna Henningsen) [#35612](https://github.com/nodejs/node/pull/35612)
* [[`27f1bc05fd`](https://github.com/nodejs/node/commit/27f1bc05fd)] - **deps**: upgrade npm to 7.0.11 (Darcy Clarke) [#36112](https://github.com/nodejs/node/pull/36112)
* [[`8ae3ffe2be`](https://github.com/nodejs/node/commit/8ae3ffe2be)] - **deps**: V8: cherry-pick 1d0f426311d4 (Ole André Vadla Ravnås) [#35986](https://github.com/nodejs/node/pull/35986)
* [[`4b7ba11d67`](https://github.com/nodejs/node/commit/4b7ba11d67)] - **deps**: V8: cherry-pick 4e077ff0444a (Ole André Vadla Ravnås) [#35986](https://github.com/nodejs/node/pull/35986)
* [[`098a5b1298`](https://github.com/nodejs/node/commit/098a5b1298)] - **deps**: V8: cherry-pick 086eecbd96b6 (Ole André Vadla Ravnås) [#35986](https://github.com/nodejs/node/pull/35986)
* [[`d2c757ab19`](https://github.com/nodejs/node/commit/d2c757ab19)] - **deps**: V8: cherry-pick 27e1ac1a79ff (Ole André Vadla Ravnås) [#35986](https://github.com/nodejs/node/pull/35986)
* [[`6349b1d673`](https://github.com/nodejs/node/commit/6349b1d673)] - **(SEMVER-MINOR)** **dns**: add a cancel() method to the promise Resolver (Szymon Marczak) [#33099](https://github.com/nodejs/node/pull/33099)
* [[`0fbade38ef`](https://github.com/nodejs/node/commit/0fbade38ef)] - **doc**: add arm64 macOS as experimental (Richard Lau) [#36189](https://github.com/nodejs/node/pull/36189)
* [[`42dfda8f78`](https://github.com/nodejs/node/commit/42dfda8f78)] - **doc**: remove stray comma in url.md (Rich Trott) [#36175](https://github.com/nodejs/node/pull/36175)
* [[`8bbdbccbb6`](https://github.com/nodejs/node/commit/8bbdbccbb6)] - **doc**: revise agent.destroy() text (Rich Trott) [#36163](https://github.com/nodejs/node/pull/36163)
* [[`545ac1fec5`](https://github.com/nodejs/node/commit/545ac1fec5)] - **doc**: fix punctuation in v8.md (Rich Trott) [#36192](https://github.com/nodejs/node/pull/36192)
* [[`a6a90af8c0`](https://github.com/nodejs/node/commit/a6a90af8c0)] - **doc**: add compatibility/interop technical value (Geoffrey Booth) [#35323](https://github.com/nodejs/node/pull/35323)
* [[`4ab4a99900`](https://github.com/nodejs/node/commit/4ab4a99900)] - **doc**: de-emphasize wrapping in napi\_define\_class (Gabriel Schulhof) [#36159](https://github.com/nodejs/node/pull/36159)
* [[`bb29508e8f`](https://github.com/nodejs/node/commit/bb29508e8f)] - **doc**: add link for v8.takeCoverage() (Rich Trott) [#36135](https://github.com/nodejs/node/pull/36135)
* [[`24065b92f1`](https://github.com/nodejs/node/commit/24065b92f1)] - **doc**: mark modules implementation as stable (Guy Bedford) [#35781](https://github.com/nodejs/node/pull/35781)
* [[`142cacdc63`](https://github.com/nodejs/node/commit/142cacdc63)] - **doc**: clarify text about process not responding (Rich Trott) [#36117](https://github.com/nodejs/node/pull/36117)
* [[`0ff384b0be`](https://github.com/nodejs/node/commit/0ff384b0be)] - **doc**: esm docs consolidation and reordering (Guy Bedford) [#36046](https://github.com/nodejs/node/pull/36046)
* [[`b17a83a00d`](https://github.com/nodejs/node/commit/b17a83a00d)] - **doc**: claim ABI version for Electron v13 (Shelley Vohr) [#36101](https://github.com/nodejs/node/pull/36101)
* [[`e8a8513b2c`](https://github.com/nodejs/node/commit/e8a8513b2c)] - **doc**: fix invalid link in worker\_threads.md (Rich Trott) [#36109](https://github.com/nodejs/node/pull/36109)
* [[`cd33594a0d`](https://github.com/nodejs/node/commit/cd33594a0d)] - **doc**: move shigeki to emeritus (Rich Trott) [#36093](https://github.com/nodejs/node/pull/36093)
* [[`eefc6aa6c9`](https://github.com/nodejs/node/commit/eefc6aa6c9)] - **doc**: document the error when cwd not exists in child\_process.spawn (FeelyChau) [#34505](https://github.com/nodejs/node/pull/34505)
* [[`841a2812d0`](https://github.com/nodejs/node/commit/841a2812d0)] - **doc**: fix typo in debugger.md (Rich Trott) [#36066](https://github.com/nodejs/node/pull/36066)
* [[`500e709439`](https://github.com/nodejs/node/commit/500e709439)] - **doc**: update list styles for remark-parse@9 rendering (Rich Trott) [#36049](https://github.com/nodejs/node/pull/36049)
* [[`a8dab217eb`](https://github.com/nodejs/node/commit/a8dab217eb)] - **doc,url**: fix url.hostname example (Rishabh Mehan) [#33735](https://github.com/nodejs/node/pull/33735)
* [[`e48ec703ba`](https://github.com/nodejs/node/commit/e48ec703ba)] - **domain**: improve deprecation warning text for DEP0097 (Anna Henningsen) [#36136](https://github.com/nodejs/node/pull/36136)
* [[`bcbf176c22`](https://github.com/nodejs/node/commit/bcbf176c22)] - **errors**: refactor to use more primordials (Antoine du Hamel) [#36167](https://github.com/nodejs/node/pull/36167)
* [[`66788970ac`](https://github.com/nodejs/node/commit/66788970ac)] - **esm**: refactor to use more primordials (Antoine du Hamel) [#36019](https://github.com/nodejs/node/pull/36019)
* [[`9ce9b016e6`](https://github.com/nodejs/node/commit/9ce9b016e6)] - **(SEMVER-MINOR)** **events**: add max listener warning for EventTarget (James M Snell) [#36001](https://github.com/nodejs/node/pull/36001)
* [[`1550073dbc`](https://github.com/nodejs/node/commit/1550073dbc)] - **events**: disabled manual construction AbortSignal (raisinten) [#36094](https://github.com/nodejs/node/pull/36094)
* [[`8a6cabbb23`](https://github.com/nodejs/node/commit/8a6cabbb23)] - **events**: port some wpt tests (Ethan Arrowood) [#34169](https://github.com/nodejs/node/pull/34169)
* [[`3691eccf0a`](https://github.com/nodejs/node/commit/3691eccf0a)] - **fs**: remove experimental from promises.rmdir recursive (Anders Kaseorg) [#36131](https://github.com/nodejs/node/pull/36131)
* [[`76b1863240`](https://github.com/nodejs/node/commit/76b1863240)] - **fs**: filehandle read now accepts object as argument (Nikola Glavina) [#34180](https://github.com/nodejs/node/pull/34180)
* [[`2fdf509268`](https://github.com/nodejs/node/commit/2fdf509268)] - **http**: fix typo in comment (Hollow Man) [#36193](https://github.com/nodejs/node/pull/36193)
* [[`8390f8a86b`](https://github.com/nodejs/node/commit/8390f8a86b)] - **(SEMVER-MINOR)** **http**: add support for abortsignal to http.request (Benjamin Gruenbaum) [#36048](https://github.com/nodejs/node/pull/36048)
* [[`387d92fd0e`](https://github.com/nodejs/node/commit/387d92fd0e)] - **http**: onFinish will not be triggered again when finished (rickyes) [#35845](https://github.com/nodejs/node/pull/35845)
* [[`48bf59bb8b`](https://github.com/nodejs/node/commit/48bf59bb8b)] - **http2**: add support for AbortSignal to http2Session.request (Madara Uchiha) [#36070](https://github.com/nodejs/node/pull/36070)
* [[`8a0c3b9c76`](https://github.com/nodejs/node/commit/8a0c3b9c76)] - **http2**: refactor to use more primordials (Antoine du Hamel) [#36142](https://github.com/nodejs/node/pull/36142)
* [[`f0aed8c01c`](https://github.com/nodejs/node/commit/f0aed8c01c)] - **http2**: add support for TypedArray to getUnpackedSettings (Antoine du Hamel) [#36141](https://github.com/nodejs/node/pull/36141)
* [[`9c6be3cc90`](https://github.com/nodejs/node/commit/9c6be3cc90)] - **(SEMVER-MINOR)** **http2**: allow setting the local window size of a session (Yongsheng Zhang) [#35978](https://github.com/nodejs/node/pull/35978)
* [[`0b40568afe`](https://github.com/nodejs/node/commit/0b40568afe)] - **http2**: delay session.receive() by a tick (Szymon Marczak) [#35985](https://github.com/nodejs/node/pull/35985)
* [[`1a4d43f840`](https://github.com/nodejs/node/commit/1a4d43f840)] - **lib**: refactor to use more primordials (Antoine du Hamel) [#36140](https://github.com/nodejs/node/pull/36140)
* [[`d6ea12e003`](https://github.com/nodejs/node/commit/d6ea12e003)] - **lib**: set abort-controller toStringTag (Benjamin Gruenbaum) [#36115](https://github.com/nodejs/node/pull/36115)
* [[`82f1cde57e`](https://github.com/nodejs/node/commit/82f1cde57e)] - **lib**: remove primordials.SafePromise (Antoine du Hamel) [#36149](https://github.com/nodejs/node/pull/36149)
* [[`15ff155c12`](https://github.com/nodejs/node/commit/15ff155c12)] - **(SEMVER-MINOR)** **lib**: add throws option to fs.f/l/statSync (Andrew Casey) [#33716](https://github.com/nodejs/node/pull/33716)
* [[`75707f45eb`](https://github.com/nodejs/node/commit/75707f45eb)] - **lib,tools**: enforce access to prototype from primordials (Antoine du Hamel) [#36025](https://github.com/nodejs/node/pull/36025)
* [[`79b2ba6744`](https://github.com/nodejs/node/commit/79b2ba6744)] - **n-api**: clean up binding creation (Gabriel Schulhof) [#36170](https://github.com/nodejs/node/pull/36170)
* [[`5698cc08f0`](https://github.com/nodejs/node/commit/5698cc08f0)] - **n-api**: fix test\_async\_context warnings (Gabriel Schulhof) [#36171](https://github.com/nodejs/node/pull/36171)
* [[`3d623d850c`](https://github.com/nodejs/node/commit/3d623d850c)] - **n-api**: improve consistency of how we get context (Michael Dawson) [#36068](https://github.com/nodejs/node/pull/36068)
* [[`89da0c3353`](https://github.com/nodejs/node/commit/89da0c3353)] - **n-api**: factor out calling pattern (Gabriel Schulhof) [#36113](https://github.com/nodejs/node/pull/36113)
* [[`5c0ddbca01`](https://github.com/nodejs/node/commit/5c0ddbca01)] - **net**: fix invalid write after end error (Robert Nagy) [#36043](https://github.com/nodejs/node/pull/36043)
* [[`85c85d368a`](https://github.com/nodejs/node/commit/85c85d368a)] - **(SEMVER-MINOR)** **path**: add `path/posix` and `path/win32` alias modules (ExE Boss) [#34962](https://github.com/nodejs/node/pull/34962)
* [[`ed8af3a8b7`](https://github.com/nodejs/node/commit/ed8af3a8b7)] - **perf_hooks**: make nodeTiming a first-class object (Momtchil Momtchev) [#35977](https://github.com/nodejs/node/pull/35977)
* [[`eb9295b583`](https://github.com/nodejs/node/commit/eb9295b583)] - **promise**: emit error on domain unhandled rejections (Benjamin Gruenbaum) [#36082](https://github.com/nodejs/node/pull/36082)
* [[`59af919d6b`](https://github.com/nodejs/node/commit/59af919d6b)] - **querystring**: reduce memory usage by Int8Array (sapics) [#34179](https://github.com/nodejs/node/pull/34179)
* [[`d1baae3640`](https://github.com/nodejs/node/commit/d1baae3640)] - **(SEMVER-MINOR)** **readline**: add getPrompt to get the current prompt (Mattias Runge-Broberg) [#33675](https://github.com/nodejs/node/pull/33675)
* [[`6d1b1c7ad0`](https://github.com/nodejs/node/commit/6d1b1c7ad0)] - **src**: integrate URL::href() and use in inspector (Daijiro Wachi) [#35912](https://github.com/nodejs/node/pull/35912)
* [[`7086f2e653`](https://github.com/nodejs/node/commit/7086f2e653)] - **src**: refactor using-declarations node\_env\_var.cc (raisinten) [#36128](https://github.com/nodejs/node/pull/36128)
* [[`122797e87f`](https://github.com/nodejs/node/commit/122797e87f)] - **src**: remove duplicate logic for getting buffer (Yash Ladha) [#34553](https://github.com/nodejs/node/pull/34553)
* [[`5729478509`](https://github.com/nodejs/node/commit/5729478509)] - **(SEMVER-MINOR)** **src**: add loop idle time in diagnostic report (Gireesh Punathil) [#35940](https://github.com/nodejs/node/pull/35940)
* [[`a81dc9ae18`](https://github.com/nodejs/node/commit/a81dc9ae18)] - **src,crypto**: refactoring of crypto\_context, SecureContext (James M Snell) [#35665](https://github.com/nodejs/node/pull/35665)
* [[`5fa35f6934`](https://github.com/nodejs/node/commit/5fa35f6934)] - **test**: update comments in test-fs-read-offset-null (Rich Trott) [#36152](https://github.com/nodejs/node/pull/36152)
* [[`73bb54af77`](https://github.com/nodejs/node/commit/73bb54af77)] - **test**: update wpt url and resource (Daijiro Wachi) [#36032](https://github.com/nodejs/node/pull/36032)
* [[`77b47dfd08`](https://github.com/nodejs/node/commit/77b47dfd08)] - **test**: fix typo in inspector-helper.js (Luigi Pinca) [#36127](https://github.com/nodejs/node/pull/36127)
* [[`474664963c`](https://github.com/nodejs/node/commit/474664963c)] - **test**: deflake test-http-destroyed-socket-write2 (Luigi Pinca) [#36120](https://github.com/nodejs/node/pull/36120)
* [[`f9bbd35937`](https://github.com/nodejs/node/commit/f9bbd35937)] - **test**: make test-http2-client-jsstream-destroy.js reliable (Rich Trott) [#36129](https://github.com/nodejs/node/pull/36129)
* [[`c19df17acb`](https://github.com/nodejs/node/commit/c19df17acb)] - **test**: add test for fs.read when offset key is null (mayank agarwal) [#35918](https://github.com/nodejs/node/pull/35918)
* [[`9405cddbee`](https://github.com/nodejs/node/commit/9405cddbee)] - **test**: improve test-stream-duplex-readable-end (Luigi Pinca) [#36056](https://github.com/nodejs/node/pull/36056)
* [[`3be5e86c57`](https://github.com/nodejs/node/commit/3be5e86c57)] - **test**: add util.inspect test for null maxStringLength (Rich Trott) [#36086](https://github.com/nodejs/node/pull/36086)
* [[`6a4cc43028`](https://github.com/nodejs/node/commit/6a4cc43028)] - **test**: replace var with const (Aleksandr Krutko) [#36069](https://github.com/nodejs/node/pull/36069)
* [[`a367c0dfc2`](https://github.com/nodejs/node/commit/a367c0dfc2)] - **timers**: refactor to use more primordials (Antoine du Hamel) [#36132](https://github.com/nodejs/node/pull/36132)
* [[`a6ef92bc27`](https://github.com/nodejs/node/commit/a6ef92bc27)] - **tools**: bump unist-util-find@1.0.1 to unist-util-find@1.0.2 (Rich Trott) [#36106](https://github.com/nodejs/node/pull/36106)
* [[`2d2491284e`](https://github.com/nodejs/node/commit/2d2491284e)] - **tools**: only use 2 cores for macos action (Myles Borins) [#36169](https://github.com/nodejs/node/pull/36169)
* [[`d8fcf2c324`](https://github.com/nodejs/node/commit/d8fcf2c324)] - **tools**: remove bashisms from license builder script (Antoine du Hamel) [#36122](https://github.com/nodejs/node/pull/36122)
* [[`7e7ddb11c0`](https://github.com/nodejs/node/commit/7e7ddb11c0)] - **tools**: hide commit queue action link (Antoine du Hamel) [#36124](https://github.com/nodejs/node/pull/36124)
* [[`63494e434a`](https://github.com/nodejs/node/commit/63494e434a)] - **tools**: update doc tools to remark-parse@9.0.0 (Rich Trott) [#36049](https://github.com/nodejs/node/pull/36049)
* [[`bf0550ce4e`](https://github.com/nodejs/node/commit/bf0550ce4e)] - **tools**: enforce use of single quotes in editorconfig (Antoine du Hamel) [#36020](https://github.com/nodejs/node/pull/36020)
* [[`49649a499e`](https://github.com/nodejs/node/commit/49649a499e)] - **tools**: fix config serialization w/ long strings (Ole André Vadla Ravnås) [#35982](https://github.com/nodejs/node/pull/35982)
* [[`be220b213d`](https://github.com/nodejs/node/commit/be220b213d)] - **tools**: update ESLint to 7.13.0 (Luigi Pinca) [#36031](https://github.com/nodejs/node/pull/36031)
* [[`4140f491fd`](https://github.com/nodejs/node/commit/4140f491fd)] - **util**: fix to inspect getters that access this (raisinten) [#36052](https://github.com/nodejs/node/pull/36052)
* [[`baa87c1a7d`](https://github.com/nodejs/node/commit/baa87c1a7d)] - **(SEMVER-MINOR)** **util**: add `util/types` alias module (ExE Boss) [#34055](https://github.com/nodejs/node/pull/34055)
* [[`f7b2fce1c1`](https://github.com/nodejs/node/commit/f7b2fce1c1)] - **vm**: refactor to use more primordials (Antoine du Hamel) [#36023](https://github.com/nodejs/node/pull/36023)
* [[`4e3883ec2d`](https://github.com/nodejs/node/commit/4e3883ec2d)] - **win,build,tools**: support VS prerelease (Baruch Odem) [#36033](https://github.com/nodejs/node/pull/36033)

<a id="15.2.1"></a>
## 2020-11-16, Version 15.2.1 (Current), @targos

### Notable changes

This is a security release.

Vulnerabilities fixed:

* **CVE-2020-8277**: Denial of Service through DNS request (High). A Node.js application that allows an attacker to trigger a DNS request for a host of their choice could trigger a Denial of service by getting the application to resolve a DNS record with a larger number of responses.

### Commits

* [[`2a44836eeb`](https://github.com/nodejs/node/commit/2a44836eeb)] - **deps**: cherry-pick 0d252eb from upstream c-ares (Michael Dawson) [nodejs-private/node-private#231](https://github.com/nodejs-private/node-private/pull/231)
* [[`b1f5518a0a`](https://github.com/nodejs/node/commit/b1f5518a0a)] - **doc**: fix `events.getEventListeners` example (Dmitry Semigradsky) [#36085](https://github.com/nodejs/node/pull/36085)
* [[`b477447a55`](https://github.com/nodejs/node/commit/b477447a55)] - **doc**: fix `added:` info for `stream.\_construct()` (Luigi Pinca) [#36067](https://github.com/nodejs/node/pull/36067)
* [[`df211208c0`](https://github.com/nodejs/node/commit/df211208c0)] - **test**: add missing test coverage for setLocalAddress() (Rich Trott) [#36039](https://github.com/nodejs/node/pull/36039)
* [[`f5191f5bd2`](https://github.com/nodejs/node/commit/f5191f5bd2)] - **test**: remove flaky designation for fixed test (Rich Trott) [#35961](https://github.com/nodejs/node/pull/35961)
* [[`a2f652f7c5`](https://github.com/nodejs/node/commit/a2f652f7c5)] - **test**: move test-worker-eventlooputil to sequential (Rich Trott) [#35996](https://github.com/nodejs/node/pull/35996)
* [[`b0b43b27d6`](https://github.com/nodejs/node/commit/b0b43b27d6)] - **test**: fix unreliable test-fs-write-file.js (Rich Trott) [#36102](https://github.com/nodejs/node/pull/36102)

<a id="15.2.0"></a>
## 2020-11-10, Version 15.2.0 (Current), @danielleadams

### Notable changes

* **events**:
  * getEventListeners static (Benjamin Gruenbaum) [#35991](https://github.com/nodejs/node/pull/35991)
* **fs**:
  * support abortsignal in writeFile (Benjamin Gruenbaum) [#35993](https://github.com/nodejs/node/pull/35993)
  * add support for AbortSignal in readFile (Benjamin Gruenbaum) [#35911](https://github.com/nodejs/node/pull/35911)
* **stream**:
  * fix thrown object reference (Gil Pedersen) [#36065](https://github.com/nodejs/node/pull/36065)

### Commits

* [[`9d9a044c1b`](https://github.com/nodejs/node/commit/9d9a044c1b)] - **benchmark**: ignore build artifacts for napi addons (Richard Lau) [#35970](https://github.com/nodejs/node/pull/35970)
* [[`4c6de854be`](https://github.com/nodejs/node/commit/4c6de854be)] - **benchmark**: remove modules that require intl (Richard Lau) [#35968](https://github.com/nodejs/node/pull/35968)
* [[`292915a6a8`](https://github.com/nodejs/node/commit/292915a6a8)] - **bootstrap**: refactor to use more primordials (Antoine du Hamel) [#35999](https://github.com/nodejs/node/pull/35999)
* [[`10c9ea771d`](https://github.com/nodejs/node/commit/10c9ea771d)] - **build**: fix zlib inlining for IA-32 (raisinten) [#35679](https://github.com/nodejs/node/pull/35679)
* [[`6ac9c8f31b`](https://github.com/nodejs/node/commit/6ac9c8f31b)] - **build, tools**: look for local installation of NASM (Richard Lau) [#36014](https://github.com/nodejs/node/pull/36014)
* [[`9757b47c44`](https://github.com/nodejs/node/commit/9757b47c44)] - **console**: use more primordials (Antoine du Hamel) [#35734](https://github.com/nodejs/node/pull/35734)
* [[`0d7422651b`](https://github.com/nodejs/node/commit/0d7422651b)] - **crypto**: refactor to use more primordials (Antoine du Hamel) [#36012](https://github.com/nodejs/node/pull/36012)
* [[`dc4936ba50`](https://github.com/nodejs/node/commit/dc4936ba50)] - **crypto**: fix comment in ByteSource (Tobias Nießen) [#35972](https://github.com/nodejs/node/pull/35972)
* [[`7cb5c0911e`](https://github.com/nodejs/node/commit/7cb5c0911e)] - **deps**: cherry-pick 9a49b22 from V8 upstream (Daniel Bevenius) [#35939](https://github.com/nodejs/node/pull/35939)
* [[`4b03670877`](https://github.com/nodejs/node/commit/4b03670877)] - **dns**: fix trace\_events name for resolveCaa() (Rich Trott) [#35979](https://github.com/nodejs/node/pull/35979)
* [[`dcb27600da`](https://github.com/nodejs/node/commit/dcb27600da)] - **doc**: escape asterisk in cctest gtest-filter (raisinten) [#36034](https://github.com/nodejs/node/pull/36034)
* [[`923276ca53`](https://github.com/nodejs/node/commit/923276ca53)] - **doc**: move v8.getHeapCodeStatistics() (Rich Trott) [#36027](https://github.com/nodejs/node/pull/36027)
* [[`71fa9c6b24`](https://github.com/nodejs/node/commit/71fa9c6b24)] - **doc**: add note regarding file structure in src/README.md (Denys Otrishko) [#35000](https://github.com/nodejs/node/pull/35000)
* [[`99cb36238d`](https://github.com/nodejs/node/commit/99cb36238d)] - **doc**: advise users to import the full set of trusted release keys (Reşat SABIQ) [#32655](https://github.com/nodejs/node/pull/32655)
* [[`06cc400160`](https://github.com/nodejs/node/commit/06cc400160)] - **doc**: fix crypto doc linter errors (Antoine du Hamel) [#36035](https://github.com/nodejs/node/pull/36035)
* [[`01129a7b39`](https://github.com/nodejs/node/commit/01129a7b39)] - **doc**: revise v8.getHeapSnapshot() (Rich Trott) [#35849](https://github.com/nodejs/node/pull/35849)
* [[`77d33c9b2f`](https://github.com/nodejs/node/commit/77d33c9b2f)] - **doc**: update core-validate-commit link in guide (Daijiro Wachi) [#35938](https://github.com/nodejs/node/pull/35938)
* [[`6d56ba03e2`](https://github.com/nodejs/node/commit/6d56ba03e2)] - **doc**: update benchmark CI test indicator in README (Rich Trott) [#35945](https://github.com/nodejs/node/pull/35945)
* [[`8bd364a9b3`](https://github.com/nodejs/node/commit/8bd364a9b3)] - **doc**: add new wordings to the API description (Pooja D.P) [#35588](https://github.com/nodejs/node/pull/35588)
* [[`acd3617e1a`](https://github.com/nodejs/node/commit/acd3617e1a)] - **doc**: option --prof documentation help added (krank2me) [#34991](https://github.com/nodejs/node/pull/34991)
* [[`6968b0fd49`](https://github.com/nodejs/node/commit/6968b0fd49)] - **doc**: fix release-schedule link in backport guide (Daijiro Wachi) [#35920](https://github.com/nodejs/node/pull/35920)
* [[`efbfeff62b`](https://github.com/nodejs/node/commit/efbfeff62b)] - **doc**: fix incorrect heading level (Bryan Field) [#35965](https://github.com/nodejs/node/pull/35965)
* [[`9c4b360d08`](https://github.com/nodejs/node/commit/9c4b360d08)] - **doc,crypto**: added sign/verify method changes about dsaEncoding (Filip Skokan) [#35480](https://github.com/nodejs/node/pull/35480)
* [[`85cf30541d`](https://github.com/nodejs/node/commit/85cf30541d)] - **doc,fs**: document value of stats.isDirectory on symbolic links (coderaiser) [#27413](https://github.com/nodejs/node/pull/27413)
* [[`d6bd78ff82`](https://github.com/nodejs/node/commit/d6bd78ff82)] - **doc,net**: document socket.timeout (Brandon Kobel) [#34543](https://github.com/nodejs/node/pull/34543)
* [[`36c20d939a`](https://github.com/nodejs/node/commit/36c20d939a)] - **doc,stream**: write(chunk, encoding, cb) encoding can be null (dev-script) [#35372](https://github.com/nodejs/node/pull/35372)
* [[`9d26c4d496`](https://github.com/nodejs/node/commit/9d26c4d496)] - **domain**: refactor to use more primordials (Antoine du Hamel) [#35885](https://github.com/nodejs/node/pull/35885)
* [[`d83e253065`](https://github.com/nodejs/node/commit/d83e253065)] - **errors**: refactor to use more primordials (Antoine du Hamel) [#35944](https://github.com/nodejs/node/pull/35944)
* [[`567f8d8caf`](https://github.com/nodejs/node/commit/567f8d8caf)] - **(SEMVER-MINOR)** **events**: getEventListeners static (Benjamin Gruenbaum) [#35991](https://github.com/nodejs/node/pull/35991)
* [[`9e673723e3`](https://github.com/nodejs/node/commit/9e673723e3)] - **events**: fire handlers in correct oder (Benjamin Gruenbaum) [#35931](https://github.com/nodejs/node/pull/35931)
* [[`ff59fcdf7b`](https://github.com/nodejs/node/commit/ff59fcdf7b)] - **events**: define abort on prototype (Benjamin Gruenbaum) [#35931](https://github.com/nodejs/node/pull/35931)
* [[`ab0eb4f2c9`](https://github.com/nodejs/node/commit/ab0eb4f2c9)] - **events**: support event handlers on prototypes (Benjamin Gruenbaum) [#35931](https://github.com/nodejs/node/pull/35931)
* [[`33e2ee58a7`](https://github.com/nodejs/node/commit/33e2ee58a7)] - **events**: define event handler as enumerable (Benjamin Gruenbaum) [#35931](https://github.com/nodejs/node/pull/35931)
* [[`a7d0c76f86`](https://github.com/nodejs/node/commit/a7d0c76f86)] - **events**: support emit on nodeeventtarget (Benjamin Gruenbaum) [#35851](https://github.com/nodejs/node/pull/35851)
* [[`76332a0439`](https://github.com/nodejs/node/commit/76332a0439)] - **events**: port some wpt tests (Benjamin Gruenbaum) [#33621](https://github.com/nodejs/node/pull/33621)
* [[`ccf9f0e62e`](https://github.com/nodejs/node/commit/ccf9f0e62e)] - **(SEMVER-MINOR)** **fs**: support abortsignal in writeFile (Benjamin Gruenbaum) [#35993](https://github.com/nodejs/node/pull/35993)
* [[`7ef9c707e9`](https://github.com/nodejs/node/commit/7ef9c707e9)] - **fs**: replace finally with PromisePrototypeFinally (Baruch Odem (Rothkoff)) [#35995](https://github.com/nodejs/node/pull/35995)
* [[`ccbe267515`](https://github.com/nodejs/node/commit/ccbe267515)] - **fs**: remove unnecessary Function#bind() in fs/promises (Ben Noordhuis) [#35208](https://github.com/nodejs/node/pull/35208)
* [[`6011bfdec5`](https://github.com/nodejs/node/commit/6011bfdec5)] - **fs**: remove unused assignment (Rich Trott) [#35882](https://github.com/nodejs/node/pull/35882)
* [[`92bdfd141b`](https://github.com/nodejs/node/commit/92bdfd141b)] - **(SEMVER-MINOR)** **fs**: add support for AbortSignal in readFile (Benjamin Gruenbaum) [#35911](https://github.com/nodejs/node/pull/35911)
* [[`11f592450b`](https://github.com/nodejs/node/commit/11f592450b)] - **http2**: add has method to proxySocketHandler (masx200) [#35197](https://github.com/nodejs/node/pull/35197)
* [[`28ed7d062e`](https://github.com/nodejs/node/commit/28ed7d062e)] - **http2**: centralise socket event binding in Http2Session (Momtchil Momtchev) [#35772](https://github.com/nodejs/node/pull/35772)
* [[`429113ebfb`](https://github.com/nodejs/node/commit/429113ebfb)] - **http2**: move events to the JSStreamSocket (Momtchil Momtchev) [#35772](https://github.com/nodejs/node/pull/35772)
* [[`1dd744a420`](https://github.com/nodejs/node/commit/1dd744a420)] - **http2**: fix error stream write followed by destroy (David Halls) [#35951](https://github.com/nodejs/node/pull/35951)
* [[`af2a560c42`](https://github.com/nodejs/node/commit/af2a560c42)] - **lib**: add %TypedArray% abstract constructor to primordials (ExE Boss) [#36016](https://github.com/nodejs/node/pull/36016)
* [[`b700900d02`](https://github.com/nodejs/node/commit/b700900d02)] - **lib**: refactor to use more primordials (Antoine du Hamel) [#35875](https://github.com/nodejs/node/pull/35875)
* [[`7a375902ff`](https://github.com/nodejs/node/commit/7a375902ff)] - **module**: refactor to use more primordials (Antoine du Hamel) [#36024](https://github.com/nodejs/node/pull/36024)
* [[`8d76db86b5`](https://github.com/nodejs/node/commit/8d76db86b5)] - **module**: refactor to use iterable-weak-map (Benjamin Coe) [#35915](https://github.com/nodejs/node/pull/35915)
* [[`9b6512f7de`](https://github.com/nodejs/node/commit/9b6512f7de)] - **n-api**: unlink reference during its destructor (Gabriel Schulhof) [#35933](https://github.com/nodejs/node/pull/35933)
* [[`1b277d97f3`](https://github.com/nodejs/node/commit/1b277d97f3)] - **src**: remove ERR prefix in crypto status enums (Daniel Bevenius) [#35867](https://github.com/nodejs/node/pull/35867)
* [[`9774b4cc72`](https://github.com/nodejs/node/commit/9774b4cc72)] - **stream**: fix thrown object reference (Gil Pedersen) [#36065](https://github.com/nodejs/node/pull/36065)
* [[`359a6590b0`](https://github.com/nodejs/node/commit/359a6590b0)] - **stream**: writableNeedDrain (Robert Nagy) [#35348](https://github.com/nodejs/node/pull/35348)
* [[`b7aa5e2296`](https://github.com/nodejs/node/commit/b7aa5e2296)] - **stream**: remove isPromise utility function (Antoine du Hamel) [#35925](https://github.com/nodejs/node/pull/35925)
* [[`fdae9ad188`](https://github.com/nodejs/node/commit/fdae9ad188)] - **test**: fix races in test-performance-eventlooputil (Gerhard Stoebich) [#36028](https://github.com/nodejs/node/pull/36028)
* [[`0a4c96a7df`](https://github.com/nodejs/node/commit/0a4c96a7df)] - **test**: use global.EventTarget instead of internals (Antoine du Hamel) [#36002](https://github.com/nodejs/node/pull/36002)
* [[`f73b8d84db`](https://github.com/nodejs/node/commit/f73b8d84db)] - **test**: improve error message for policy failures (Bradley Meck) [#35633](https://github.com/nodejs/node/pull/35633)
* [[`cb6f0d3d89`](https://github.com/nodejs/node/commit/cb6f0d3d89)] - **test**: update old comment style test\_util.cc (raisinten) [#35884](https://github.com/nodejs/node/pull/35884)
* [[`23f0d0c45c`](https://github.com/nodejs/node/commit/23f0d0c45c)] - **test**: fix error in test/internet/test-dns.js (Rich Trott) [#35969](https://github.com/nodejs/node/pull/35969)
* [[`77e4f19701`](https://github.com/nodejs/node/commit/77e4f19701)] - **timers**: cleanup abort listener on awaitable timers (James M Snell) [#36006](https://github.com/nodejs/node/pull/36006)
* [[`a7350b3a8f`](https://github.com/nodejs/node/commit/a7350b3a8f)] - **tools**: don't print gold linker warning w/o flag (Myles Borins) [#35955](https://github.com/nodejs/node/pull/35955)
* [[`1f27214480`](https://github.com/nodejs/node/commit/1f27214480)] - **tools**: add new ESLint rule: prefer-primordials (Leko) [#35448](https://github.com/nodejs/node/pull/35448)
* [[`da3c2ab828`](https://github.com/nodejs/node/commit/da3c2ab828)] - **tools,doc**: enable ecmaVersion 2021 in acorn parser (Antoine du Hamel) [#35994](https://github.com/nodejs/node/pull/35994)
* [[`f8098c3e43`](https://github.com/nodejs/node/commit/f8098c3e43)] - **tools,lib**: recommend using safe primordials (Antoine du Hamel) [#36026](https://github.com/nodejs/node/pull/36026)
* [[`eea7e3b0d0`](https://github.com/nodejs/node/commit/eea7e3b0d0)] - **tools,lib**: tighten prefer-primordials rules for Error statics (Antoine du Hamel) [#36017](https://github.com/nodejs/node/pull/36017)
* [[`7a2edea7ed`](https://github.com/nodejs/node/commit/7a2edea7ed)] - **win, build**: fix build time on Windows (Bartosz Sosnowski) [#35932](https://github.com/nodejs/node/pull/35932)

<a id="15.1.0"></a>
## 2020-11-04, Version 15.1.0 (Current), @targos

### Notable Changes

#### Diagnostics channel (experimental module)

`diagnostics_channel` is a new experimental module that provides an API to create named channels to report arbitrary message data for diagnostics purposes.

With `diagnostics_channel`, Node.js core and module authors can publish contextual data about what they are doing at a given time. This could be the hostname and query string of a mysql query, for example. Just create a named channel with `dc.channel(name)` and call `channel.publish(data)` to send the data to any listeners to that channel.

```js
const dc = require('diagnostics_channel');
const channel = dc.channel('mysql.query');

MySQL.prototype.query = function query(queryString, values, callback) {
  // Broadcast query information whenever a query is made
  channel.publish({
    query: queryString,
    host: this.hostname,
  });

  this.doQuery(queryString, values, callback);
};
```

Channels are like one big global event emitter but are split into separate objects to ensure they get the best performance. If nothing is listening to the channel, the publishing overhead should be as close to zero as possible. Consuming channel data is as easy as using `channel.subscribe(listener)` to run a function whenever a message is published to that channel.

```js
const dc = require('diagnostics_channel');
const channel = dc.channel('mysql.query');

channel.subscribe(({ query, host }) => {
  console.log(`mysql query to ${host}: ${query}`);
});
```

The data captured can be used to provide context for what an app is doing at a given time. This can be used for things like augmenting tracing data, tracking network and filesystem activity, logging queries, and many other things. It's also a very useful data source for diagnostics tools to provide a clearer picture of exactly what the application is doing at a given point in the data they are presenting.

Contributed by Stephen Belanger [#34895](https://github.com/nodejs/node/pull/34895).

#### New child process `'spawn'` event

Instances of `ChildProcess` now emit a new `'spawn'` event once the child process has spawned successfully.

If emitted, the `'spawn'` event comes before all other events and before any data is received via `stdout` or `stderr`.

The `'spawn'` event will fire regardless of whether an error occurs **within** the spawned process.
For example, if `bash some-command` spawns successfully, the `'spawn'` event will fire, though `bash` may fail to spawn `some-command`.
This caveat also applies when using `{ shell: true }`.

Contributed by Matthew Francis Brunetti [#35369](https://github.com/nodejs/node/pull/35369).

#### Set the local address for DNS resolution

It is now possible to set the local IP address used by a `Resolver` instance to send its requests.
This allows programs to specify outbound interfaces when used on multi-homed
systems.

The resolver will use the v4 local address when making requests to IPv4 DNS servers, and the v6 local address when making requests to IPv6 DNS servers.

```js
const { Resolver } = require('dns');

const resolver = new Resolver();

resolver.setLocalAddress('10.1.2.3');
// Equivalent to: resolver.setLocalAddress('10.1.2.3', '::0');
```

Contributed by Josh Dague [#34824](https://github.com/nodejs/node/pull/34824).

#### Control V8 coverage at runtime

The `v8` module includes two new methods to control the V8 coverage started by the `NODE_V8_COVERAGE` environment variable.

With `v8.takeCoverage()`, it is possible to write a coverage report to disk on demand. This can be done multiple times during the lifetime of the process, and the execution counter will be reset on each call.
When the process is about to exit, one last coverage will still be written to disk, unless `v8.stopCoverage()` was invoked before.

The `v8.stopCoverage()` method allows to stop the coverage collection, so that V8 can release the execution counters and optimize code.

Contributed by Joyee Cheung [#33807](https://github.com/nodejs/node/pull/33807).

#### Analyze Worker's event loop utilization

`Worker` instances now have a `performance` property, with a single `eventLoopUtilization` method that can be used to gather information about the worker's event loop utilization between the `'online'` and `'exit'` events.

The method works the same way as `perf_hooks` `eventLoopUtilization()`.

Contributed by Trevor Norris [#35664](https://github.com/nodejs/node/pull/35664).

#### Take a V8 heap snapshot just before running out of memory (experimental)

With the new `--heapsnapshot-near-heap-limit=max_count` experimental command line flag, it is now possible to automatically generate a heap snapshot when the V8 heap usage is approaching the heap limit. `count` should be a non-negative integer (in which case Node.js will write no more than `max_count` snapshots to disk).

When generating snapshots, garbage collection may be triggered and bring the heap usage down, therefore multiple snapshots may be written to disk before the Node.js instance finally runs out of memory. These heap snapshots can be compared to determine what objects are being allocated during the time consecutive snapshots are taken.

Generating V8 snapshots takes time and memory (both memory managed by the V8 heap and native memory outside the V8 heap). The bigger the heap is, the more resources it needs. Node.js will adjust the V8 heap to accommondate the additional V8 heap memory overhead, and try its best to avoid using up all the memory avialable to the process.

```console
$ node --max-old-space-size=100 --heapsnapshot-near-heap-limit=3 index.js
Wrote snapshot to Heap.20200430.100036.49580.0.001.heapsnapshot
Wrote snapshot to Heap.20200430.100037.49580.0.002.heapsnapshot
Wrote snapshot to Heap.20200430.100038.49580.0.003.heapsnapshot

<--- Last few GCs --->

[49580:0x110000000]     4826 ms: Mark-sweep 130.6 (147.8) -> 130.5 (147.8) MB, 27.4 / 0.0 ms  (average mu = 0.126, current mu = 0.034) allocation failure scavenge might not succeed
[49580:0x110000000]     4845 ms: Mark-sweep 130.6 (147.8) -> 130.6 (147.8) MB, 18.8 / 0.0 ms  (average mu = 0.088, current mu = 0.031) allocation failure scavenge might not succeed


<--- JS stacktrace --->

FATAL ERROR: Ineffective mark-compacts near heap limit Allocation failed - JavaScript heap out of memory
....
```

Contributed by Joyee Cheung [#33010](https://github.com/nodejs/node/pull/33010).

### Commits

#### Semver-minor commits

* [[`8169902b40`](https://github.com/nodejs/node/commit/8169902b40)] - **(SEMVER-MINOR)** **child_process**: add ChildProcess 'spawn' event (Matthew Francis Brunetti) [#35369](https://github.com/nodejs/node/pull/35369)
* [[`548f91af2c`](https://github.com/nodejs/node/commit/548f91af2c)] - **(SEMVER-MINOR)** **dns**: add setLocalAddress to Resolver (Josh Dague) [#34824](https://github.com/nodejs/node/pull/34824)
* [[`f861733bac`](https://github.com/nodejs/node/commit/f861733bac)] - **(SEMVER-MINOR)** **http**: report request start and end with diagnostics\_channel (Stephen Belanger) [#34895](https://github.com/nodejs/node/pull/34895)
* [[`883ed4b7f1`](https://github.com/nodejs/node/commit/883ed4b7f1)] - **(SEMVER-MINOR)** **http2**: add updateSettings to both http2 servers (Vincent Boivin) [#35383](https://github.com/nodejs/node/pull/35383)
* [[`b38a43d5d9`](https://github.com/nodejs/node/commit/b38a43d5d9)] - **(SEMVER-MINOR)** **lib**: create diagnostics\_channel module (Stephen Belanger) [#34895](https://github.com/nodejs/node/pull/34895)
* [[`a7f37bc725`](https://github.com/nodejs/node/commit/a7f37bc725)] - **(SEMVER-MINOR)** **src**: add --heapsnapshot-near-heap-limit option (Joyee Cheung) [#33010](https://github.com/nodejs/node/pull/33010)
* [[`7bfa872013`](https://github.com/nodejs/node/commit/7bfa872013)] - **(SEMVER-MINOR)** **v8**: implement v8.stopCoverage() (Joyee Cheung) [#33807](https://github.com/nodejs/node/pull/33807)
* [[`15ffed5319`](https://github.com/nodejs/node/commit/15ffed5319)] - **(SEMVER-MINOR)** **v8**: implement v8.takeCoverage() (Joyee Cheung) [#33807](https://github.com/nodejs/node/pull/33807)
* [[`221e28311f`](https://github.com/nodejs/node/commit/221e28311f)] - **(SEMVER-MINOR)** **worker**: add eventLoopUtilization() (Trevor Norris) [#35664](https://github.com/nodejs/node/pull/35664)

#### Semver-patch commits

* [[`d95013f399`](https://github.com/nodejs/node/commit/d95013f399)] - **assert,repl**: enable ecmaVersion 2021 in acorn parser (Michaël Zasso) [#35827](https://github.com/nodejs/node/pull/35827)
* [[`b11c7378e3`](https://github.com/nodejs/node/commit/b11c7378e3)] - **build**: fix lint-js-fix target (Antoine du Hamel) [#35927](https://github.com/nodejs/node/pull/35927)
* [[`a5fa849631`](https://github.com/nodejs/node/commit/a5fa849631)] - **build**: add vcbuilt test-doc target (Antoine du Hamel) [#35708](https://github.com/nodejs/node/pull/35708)
* [[`34281cdaba`](https://github.com/nodejs/node/commit/34281cdaba)] - **build**: turn off Codecov comments (bcoe) [#35800](https://github.com/nodejs/node/pull/35800)
* [[`a9c09246bb`](https://github.com/nodejs/node/commit/a9c09246bb)] - **build**: add license-builder GitHub Action (Tierney Cyren) [#35712](https://github.com/nodejs/node/pull/35712)
* [[`4447ff1162`](https://github.com/nodejs/node/commit/4447ff1162)] - **build,tools**: gitHub Actions: use Node.js Fermium (Antoine du Hamel) [#35840](https://github.com/nodejs/node/pull/35840)
* [[`273e147017`](https://github.com/nodejs/node/commit/273e147017)] - **build,tools**: add lint-js-doc target (Antoine du Hamel) [#35708](https://github.com/nodejs/node/pull/35708)
* [[`0ebf44b466`](https://github.com/nodejs/node/commit/0ebf44b466)] - **crypto**: pass empty passphrases to OpenSSL properly (Tobias Nießen) [#35914](https://github.com/nodejs/node/pull/35914)
* [[`644c416389`](https://github.com/nodejs/node/commit/644c416389)] - **crypto**: rename check to createJob (Daniel Bevenius) [#35858](https://github.com/nodejs/node/pull/35858)
* [[`79a8fb62e6`](https://github.com/nodejs/node/commit/79a8fb62e6)] - **crypto**: fixup scrypt regressions (James M Snell) [#35821](https://github.com/nodejs/node/pull/35821)
* [[`abd7c9447c`](https://github.com/nodejs/node/commit/abd7c9447c)] - **crypto**: fix webcrypto ECDH JWK import (Filip Skokan) [#35855](https://github.com/nodejs/node/pull/35855)
* [[`d3f1cde908`](https://github.com/nodejs/node/commit/d3f1cde908)] - **deps**: upgrade npm to 7.0.8 (Myles Borins) [#35953](https://github.com/nodejs/node/pull/35953)
* [[`55adee0947`](https://github.com/nodejs/node/commit/55adee0947)] - **deps**: upgrade npm to 7.0.7 (Luigi Pinca) [#35908](https://github.com/nodejs/node/pull/35908)
* [[`5cb77f2e79`](https://github.com/nodejs/node/commit/5cb77f2e79)] - **deps**: upgrade to cjs-module-lexer@1.0.0 (Guy Bedford) [#35928](https://github.com/nodejs/node/pull/35928)
* [[`1303a1fca8`](https://github.com/nodejs/node/commit/1303a1fca8)] - **deps**: update to cjs-module-lexer@0.5.2 (Guy Bedford) [#35901](https://github.com/nodejs/node/pull/35901)
* [[`20accb08fa`](https://github.com/nodejs/node/commit/20accb08fa)] - **deps**: upgrade to cjs-module-lexer@0.5.0 (Guy Bedford) [#35871](https://github.com/nodejs/node/pull/35871)
* [[`52a77db759`](https://github.com/nodejs/node/commit/52a77db759)] - **deps**: update acorn to v8.0.4 (Michaël Zasso) [#35791](https://github.com/nodejs/node/pull/35791)
* [[`e0a1541260`](https://github.com/nodejs/node/commit/e0a1541260)] - **deps**: update to cjs-module-lexer@0.4.3 (Guy Bedford) [#35745](https://github.com/nodejs/node/pull/35745)
* [[`894419c1f4`](https://github.com/nodejs/node/commit/894419c1f4)] - **deps**: V8: backport 4263f8a5e8e0 (Brian 'bdougie' Douglas) [#35650](https://github.com/nodejs/node/pull/35650)
* [[`564aadedac`](https://github.com/nodejs/node/commit/564aadedac)] - **doc,src,test**: revise C++ code for linter update (Rich Trott) [#35719](https://github.com/nodejs/node/pull/35719)
* [[`7c8b5e5e0e`](https://github.com/nodejs/node/commit/7c8b5e5e0e)] - **errors**: do not call resolve on URLs with schemes (bcoe) [#35903](https://github.com/nodejs/node/pull/35903)
* [[`1cdfaa80f8`](https://github.com/nodejs/node/commit/1cdfaa80f8)] - **events**: add a few tests (Benjamin Gruenbaum) [#35806](https://github.com/nodejs/node/pull/35806)
* [[`f08e2c0213`](https://github.com/nodejs/node/commit/f08e2c0213)] - **events**: make abort\_controller event trusted (Benjamin Gruenbaum) [#35811](https://github.com/nodejs/node/pull/35811)
* [[`438d9debfd`](https://github.com/nodejs/node/commit/438d9debfd)] - **events**: make eventTarget.removeAllListeners() return this (Luigi Pinca) [#35805](https://github.com/nodejs/node/pull/35805)
* [[`b6b7a3b86a`](https://github.com/nodejs/node/commit/b6b7a3b86a)] - **http**: lazy create IncomingMessage.headers (Robert Nagy) [#35281](https://github.com/nodejs/node/pull/35281)
* [[`86ed87b6b7`](https://github.com/nodejs/node/commit/86ed87b6b7)] - **http2**: fix reinjection check (Momtchil Momtchev) [#35678](https://github.com/nodejs/node/pull/35678)
* [[`5833007eb0`](https://github.com/nodejs/node/commit/5833007eb0)] - **http2**: reinject data received before http2 is attached (Momtchil Momtchev) [#35678](https://github.com/nodejs/node/pull/35678)
* [[`cfe61b8714`](https://github.com/nodejs/node/commit/cfe61b8714)] - **http2**: remove unsupported %.\* specifier (Momtchil Momtchev) [#35694](https://github.com/nodejs/node/pull/35694)
* [[`d2f574b5be`](https://github.com/nodejs/node/commit/d2f574b5be)] - **lib**: let abort\_controller target be EventTarget (Daijiro Wachi) [#35869](https://github.com/nodejs/node/pull/35869)
* [[`b1e531a70b`](https://github.com/nodejs/node/commit/b1e531a70b)] - **lib**: use primordials when calling methods of Error (Antoine du Hamel) [#35837](https://github.com/nodejs/node/pull/35837)
* [[`0f5a8c55c2`](https://github.com/nodejs/node/commit/0f5a8c55c2)] - **module**: runtime deprecate subpath folder mappings (Guy Bedford) [#35747](https://github.com/nodejs/node/pull/35747)
* [[`d16e2fa69a`](https://github.com/nodejs/node/commit/d16e2fa69a)] - **n-api**: napi\_make\_callback emit async init with resource of async\_context (legendecas) [#32930](https://github.com/nodejs/node/pull/32930)
* [[`0c17dbd201`](https://github.com/nodejs/node/commit/0c17dbd201)] - **n-api**: revert change to finalization (Michael Dawson) [#35777](https://github.com/nodejs/node/pull/35777)
* [[`fb7196434e`](https://github.com/nodejs/node/commit/fb7196434e)] - **src**: remove redundant OpenSSLBuffer (James M Snell) [#35663](https://github.com/nodejs/node/pull/35663)
* [[`c9225789d3`](https://github.com/nodejs/node/commit/c9225789d3)] - **src**: remove ERR prefix in WebCryptoKeyExportStatus (Daniel Bevenius) [#35639](https://github.com/nodejs/node/pull/35639)
* [[`4128eefcb3`](https://github.com/nodejs/node/commit/4128eefcb3)] - **src**: remove ignore GCC -Wcast-function-type for v8 (Daniel Bevenius) [#35768](https://github.com/nodejs/node/pull/35768)
* [[`4b8b5fee6a`](https://github.com/nodejs/node/commit/4b8b5fee6a)] - **src**: use MaybeLocal.ToLocal instead of IsEmpty (Daniel Bevenius) [#35716](https://github.com/nodejs/node/pull/35716)
* [[`01d7c46776`](https://github.com/nodejs/node/commit/01d7c46776)] - ***Revert*** "**src**: ignore GCC -Wcast-function-type for v8.h" (Daniel Bevenius) [#35758](https://github.com/nodejs/node/pull/35758)
* [[`2868f52a5c`](https://github.com/nodejs/node/commit/2868f52a5c)] - **stream**: fix regression on duplex end (Momtchil Momtchev) [#35941](https://github.com/nodejs/node/pull/35941)
* [[`70c41a830d`](https://github.com/nodejs/node/commit/70c41a830d)] - **stream**: remove redundant context from comments (Yash Ladha) [#35728](https://github.com/nodejs/node/pull/35728)
* [[`88eb6191e4`](https://github.com/nodejs/node/commit/88eb6191e4)] - **stream**: fix duplicate logic in stream destroy (Yash Ladha) [#35727](https://github.com/nodejs/node/pull/35727)
* [[`a41e3ebc3a`](https://github.com/nodejs/node/commit/a41e3ebc3a)] - **timers**: correct explanation in comment (Turner Jabbour) [#35437](https://github.com/nodejs/node/pull/35437)
* [[`ee15142fef`](https://github.com/nodejs/node/commit/ee15142fef)] - **tls**: allow reading data into a static buffer (Andrey Pechkurov) [#35753](https://github.com/nodejs/node/pull/35753)
* [[`102d7dfe02`](https://github.com/nodejs/node/commit/102d7dfe02)] - **zlib**: test BrotliCompress throws invalid arg value (raisinten) [#35830](https://github.com/nodejs/node/pull/35830)

#### Documentation commits

* [[`7937fbe3bc`](https://github.com/nodejs/node/commit/7937fbe3bc)] - **doc**: update tables in README files for linting changes (Rich Trott) [#35905](https://github.com/nodejs/node/pull/35905)
* [[`c5b94220c5`](https://github.com/nodejs/node/commit/c5b94220c5)] - **doc**: temporarily disable list-item-bullet-indent (Nick Schonning) [#35647](https://github.com/nodejs/node/pull/35647)
* [[`59b36af8d5`](https://github.com/nodejs/node/commit/59b36af8d5)] - **doc**: disable no-undefined-references workarounds (Nick Schonning) [#35647](https://github.com/nodejs/node/pull/35647)
* [[`eb55462a75`](https://github.com/nodejs/node/commit/eb55462a75)] - **doc**: adjust table alignment for remark v13 (Nick Schonning) [#35647](https://github.com/nodejs/node/pull/35647)
* [[`0ac4a6ab16`](https://github.com/nodejs/node/commit/0ac4a6ab16)] - **doc**: update crypto.createSecretKey history (Ben Turner) [#35874](https://github.com/nodejs/node/pull/35874)
* [[`4899998855`](https://github.com/nodejs/node/commit/4899998855)] - **doc**: move bnoordhuis to emeritus (Ben Noordhuis) [#35865](https://github.com/nodejs/node/pull/35865)
* [[`337bfcf614`](https://github.com/nodejs/node/commit/337bfcf614)] - **doc**: add on statement in the APIs docs (Pooja D.P) [#35610](https://github.com/nodejs/node/pull/35610)
* [[`9703219fdb`](https://github.com/nodejs/node/commit/9703219fdb)] - **doc**: fix a typo in CHANGELOG\_V15 (Takuya Noguchi) [#35804](https://github.com/nodejs/node/pull/35804)
* [[`c14889bcc1`](https://github.com/nodejs/node/commit/c14889bcc1)] - **doc**: move ronkorving to emeritus (Rich Trott) [#35828](https://github.com/nodejs/node/pull/35828)
* [[`8c2b17926c`](https://github.com/nodejs/node/commit/8c2b17926c)] - **doc**: recommend test-doc instead of lint-md (Antoine du Hamel) [#35708](https://github.com/nodejs/node/pull/35708)
* [[`0580258449`](https://github.com/nodejs/node/commit/0580258449)] - **doc**: fix reference to googletest test fixture (Tobias Nießen) [#35813](https://github.com/nodejs/node/pull/35813)
* [[`d291e3abd9`](https://github.com/nodejs/node/commit/d291e3abd9)] - **doc**: stabilize packages features (Myles Borins) [#35742](https://github.com/nodejs/node/pull/35742)
* [[`5e8d821b4c`](https://github.com/nodejs/node/commit/5e8d821b4c)] - **doc**: add conditional example for setBreakpoint() (Chris Opperwall) [#35823](https://github.com/nodejs/node/pull/35823)
* [[`8074f69f82`](https://github.com/nodejs/node/commit/8074f69f82)] - **doc**: make small improvements to REPL doc (Rich Trott) [#35808](https://github.com/nodejs/node/pull/35808)
* [[`4e76a3c106`](https://github.com/nodejs/node/commit/4e76a3c106)] - **doc**: update MessagePort documentation for EventTarget inheritance (Anna Henningsen) [#35839](https://github.com/nodejs/node/pull/35839)
* [[`3db4354cc8`](https://github.com/nodejs/node/commit/3db4354cc8)] - **doc**: use case-sensitive in the example (Pooja D.P) [#35624](https://github.com/nodejs/node/pull/35624)
* [[`b07f4a3f7a`](https://github.com/nodejs/node/commit/b07f4a3f7a)] - **doc**: consolidate and clarify breakOnSigInt text (Rich Trott) [#35787](https://github.com/nodejs/node/pull/35787)
* [[`c2e6a4b081`](https://github.com/nodejs/node/commit/c2e6a4b081)] - **doc**: fix \_construct example params order (Alejandro Oviedo) [#35790](https://github.com/nodejs/node/pull/35790)
* [[`6513a589fe`](https://github.com/nodejs/node/commit/6513a589fe)] - **doc**: add a subsystems header in pull-requests.md (Pooja D.P) [#35718](https://github.com/nodejs/node/pull/35718)
* [[`c365867c60`](https://github.com/nodejs/node/commit/c365867c60)] - **doc**: fix typo in BUILDING.md (raisinten) [#35807](https://github.com/nodejs/node/pull/35807)
* [[`6211ffd2f7`](https://github.com/nodejs/node/commit/6211ffd2f7)] - **doc**: add require statement in the example (Pooja D.P) [#35554](https://github.com/nodejs/node/pull/35554)
* [[`7b3743d8dd`](https://github.com/nodejs/node/commit/7b3743d8dd)] - **doc**: modified memory set statement set size (Pooja D.P) [#35517](https://github.com/nodejs/node/pull/35517)
* [[`afbe23d800`](https://github.com/nodejs/node/commit/afbe23d800)] - **doc**: use kbd element in readline doc prose (Rich Trott) [#35737](https://github.com/nodejs/node/pull/35737)
* [[`c0a4fac040`](https://github.com/nodejs/node/commit/c0a4fac040)] - **doc**: fix a typo in CHANGELOG\_V12 (Shubham Parihar) [#35786](https://github.com/nodejs/node/pull/35786)
* [[`0e9acf83f7`](https://github.com/nodejs/node/commit/0e9acf83f7)] - **doc**: fix header level in fs.md (ax1) [#35771](https://github.com/nodejs/node/pull/35771)
* [[`f49afb5e10`](https://github.com/nodejs/node/commit/f49afb5e10)] - **doc**: remove stability warning in v8 module doc (Rich Trott) [#35774](https://github.com/nodejs/node/pull/35774)
* [[`368ae952b2`](https://github.com/nodejs/node/commit/368ae952b2)] - **doc**: mark optional parameters in timers.md (Vse Mozhe Buty) [#35764](https://github.com/nodejs/node/pull/35764)
* [[`f6aa7c82c5`](https://github.com/nodejs/node/commit/f6aa7c82c5)] - **doc**: add a example code to API doc property (Pooja D.P) [#35738](https://github.com/nodejs/node/pull/35738)
* [[`55b7a6cea3`](https://github.com/nodejs/node/commit/55b7a6cea3)] - **doc**: document changes for `\*/promises` alias modules (ExE Boss) [#34002](https://github.com/nodejs/node/pull/34002)
* [[`4b7708a316`](https://github.com/nodejs/node/commit/4b7708a316)] - **doc**: update console.error example (Lee, Bonggi) [#34964](https://github.com/nodejs/node/pull/34964)
* [[`292b529dfa`](https://github.com/nodejs/node/commit/292b529dfa)] - **doc**: add missing link in Node.js 14 Changelog (Antoine du Hamel) [#35782](https://github.com/nodejs/node/pull/35782)
* [[`890b03ecd6`](https://github.com/nodejs/node/commit/890b03ecd6)] - **doc**: improve text for breakOnSigint (Rich Trott) [#35692](https://github.com/nodejs/node/pull/35692)
* [[`1892532ee8`](https://github.com/nodejs/node/commit/1892532ee8)] - **doc**: this prints replaced with this is printed (Pooja D.P) [#35515](https://github.com/nodejs/node/pull/35515)
* [[`6590f8cb4a`](https://github.com/nodejs/node/commit/6590f8cb4a)] - **doc**: update package.json field definitions (Myles Borins) [#35741](https://github.com/nodejs/node/pull/35741)
* [[`f269c6cbe2`](https://github.com/nodejs/node/commit/f269c6cbe2)] - **doc**: add Installing Node.js header in BUILDING.md (Pooja D.P) [#35710](https://github.com/nodejs/node/pull/35710)
* [[`05a888a8c3`](https://github.com/nodejs/node/commit/05a888a8c3)] - **doc,esm**: document experimental warning removal (Antoine du Hamel) [#35750](https://github.com/nodejs/node/pull/35750)
* [[`092c6c4f8f`](https://github.com/nodejs/node/commit/092c6c4f8f)] - **doc,test**: update v8 method doc and comment (Rich Trott) [#35795](https://github.com/nodejs/node/pull/35795)

#### Other commits

* [[`76ebae4c05`](https://github.com/nodejs/node/commit/76ebae4c05)] - **benchmark**: make the benchmark tool work with Node 10 (Joyee Cheung) [#35817](https://github.com/nodejs/node/pull/35817)
* [[`9b549c1691`](https://github.com/nodejs/node/commit/9b549c1691)] - **benchmark**: add startup benchmark for loading public modules (Joyee Cheung) [#35816](https://github.com/nodejs/node/pull/35816)
* [[`5d61e3db4b`](https://github.com/nodejs/node/commit/5d61e3db4b)] - **test**: add missing ref comments to parallel.status (Rich Trott) [#35896](https://github.com/nodejs/node/pull/35896)
* [[`231af88001`](https://github.com/nodejs/node/commit/231af88001)] - **test**: correct test-worker-eventlooputil (Gerhard Stoebich) [#35891](https://github.com/nodejs/node/pull/35891)
* [[`da612dfc20`](https://github.com/nodejs/node/commit/da612dfc20)] - **test**: integrate abort\_controller tests from wpt (Daijiro Wachi) [#35869](https://github.com/nodejs/node/pull/35869)
* [[`66ad4be2c1`](https://github.com/nodejs/node/commit/66ad4be2c1)] - **test**: add test to fs/promises setImmediate (tyankatsu) [#35852](https://github.com/nodejs/node/pull/35852)
* [[`830b789299`](https://github.com/nodejs/node/commit/830b789299)] - **test**: mark test-worker-eventlooputil flaky (Myles Borins) [#35886](https://github.com/nodejs/node/pull/35886)
* [[`7691b673dc`](https://github.com/nodejs/node/commit/7691b673dc)] - **test**: mark test-http2-respond-file-error-pipe-offset flaky (Myles Borins) [#35883](https://github.com/nodejs/node/pull/35883)
* [[`de3dcd7356`](https://github.com/nodejs/node/commit/de3dcd7356)] - **test**: fix reference to WPT testharness.js (Tobias Nießen) [#35814](https://github.com/nodejs/node/pull/35814)
* [[`8958af4aa0`](https://github.com/nodejs/node/commit/8958af4aa0)] - **test**: add onerror test cases to policy (Daijiro Wachi) [#35797](https://github.com/nodejs/node/pull/35797)
* [[`dd3cbb455a`](https://github.com/nodejs/node/commit/dd3cbb455a)] - **test**: add upstream test cases to encoding (Daijiro Wachi) [#35794](https://github.com/nodejs/node/pull/35794)
* [[`76991c039f`](https://github.com/nodejs/node/commit/76991c039f)] - **test**: add upstream test cases to urlsearchparam (Daijiro Wachi) [#35792](https://github.com/nodejs/node/pull/35792)
* [[`110ef8aa50`](https://github.com/nodejs/node/commit/110ef8aa50)] - **test**: refactor coverage logic (bcoe) [#35767](https://github.com/nodejs/node/pull/35767)
* [[`0c5e8ed651`](https://github.com/nodejs/node/commit/0c5e8ed651)] - **test**: add additional deprecation warning tests for rmdir recursive (Ian Sutherland) [#35683](https://github.com/nodejs/node/pull/35683)
* [[`11eca36e83`](https://github.com/nodejs/node/commit/11eca36e83)] - **test**: add windows and C++ coverage (Benjamin Coe) [#35670](https://github.com/nodejs/node/pull/35670)
* [[`fd027cd61a`](https://github.com/nodejs/node/commit/fd027cd61a)] - **tools**: bump remark-lint-preset-node to 2.0.0 (Rich Trott) [#35905](https://github.com/nodejs/node/pull/35905)
* [[`c09fdba786`](https://github.com/nodejs/node/commit/c09fdba786)] - **tools**: refloat 7 Node.js patches to cpplint.py (Rich Trott) [#35866](https://github.com/nodejs/node/pull/35866)
* [[`3955ccd305`](https://github.com/nodejs/node/commit/3955ccd305)] - **tools**: bump cpplint to 1.5.1 (Rich Trott) [#35866](https://github.com/nodejs/node/pull/35866)
* [[`a07d1af4ea`](https://github.com/nodejs/node/commit/a07d1af4ea)] - **tools**: update ESLint to 7.12.1 (cjihrig) [#35799](https://github.com/nodejs/node/pull/35799)
* [[`d20b318c58`](https://github.com/nodejs/node/commit/d20b318c58)] - **tools**: update ESLint to 7.12.0 (cjihrig) [#35799](https://github.com/nodejs/node/pull/35799)
* [[`31753ec8aa`](https://github.com/nodejs/node/commit/31753ec8aa)] - **tools**: add msvc /P output to .gitignore (Jiawen Geng) [#35735](https://github.com/nodejs/node/pull/35735)
* [[`afb3e24cb0`](https://github.com/nodejs/node/commit/afb3e24cb0)] - **tools**: add update-npm script (Myles Borins) [#35822](https://github.com/nodejs/node/pull/35822)
* [[`66da122d46`](https://github.com/nodejs/node/commit/66da122d46)] - **tools**: refloat 7 Node.js patches to cpplint.py (Rich Trott) [#35719](https://github.com/nodejs/node/pull/35719)
* [[`042d4dd71c`](https://github.com/nodejs/node/commit/042d4dd71c)] - **tools**: bump cpplint to 1.5.0 (Rich Trott) [#35719](https://github.com/nodejs/node/pull/35719)

<a id="15.0.1"></a>
## 2020-10-21, Version 15.0.1 (Current), @BethGriggs

### Notable changes

* **crypto**: fix regression on randomFillSync (James M Snell) [#35723](https://github.com/nodejs/node/pull/35723)
  * This fixes issue https://github.com/nodejs/node/issues/35722.
* **deps**: upgrade npm to 7.0.3 (Ruy Adorno) [#35724](https://github.com/nodejs/node/pull/35724)
* **doc**: add release key for Danielle Adams (Danielle Adams) [#35545](https://github.com/nodejs/node/pull/35545)

### Commits

* [[`c509485c19`](https://github.com/nodejs/node/commit/c509485c19)] - **build**: use make functions instead of echo (Antoine du Hamel) [#35707](https://github.com/nodejs/node/pull/35707)
* [[`f5acc2d030`](https://github.com/nodejs/node/commit/f5acc2d030)] - **crypto**: fix regression on randomFillSync (James M Snell) [#35723](https://github.com/nodejs/node/pull/35723)
* [[`595c8df48d`](https://github.com/nodejs/node/commit/595c8df48d)] - **deps**: upgrade npm to 7.0.3 (Ruy Adorno) [#35724](https://github.com/nodejs/node/pull/35724)
* [[`69e7f20f2d`](https://github.com/nodejs/node/commit/69e7f20f2d)] - **deps**: V8: set correct V8 version patch number (Michaël Zasso) [#35732](https://github.com/nodejs/node/pull/35732)
* [[`b78294dc00`](https://github.com/nodejs/node/commit/b78294dc00)] - **doc**: use kbd element in readline doc (Rich Trott) [#35698](https://github.com/nodejs/node/pull/35698)
* [[`1efa87082b`](https://github.com/nodejs/node/commit/1efa87082b)] - **doc**: add release key for Danielle Adams (Danielle Adams) [#35545](https://github.com/nodejs/node/pull/35545)
* [[`6e91d644e3`](https://github.com/nodejs/node/commit/6e91d644e3)] - **doc**: use kbd element in os doc (Rich Trott) [#35656](https://github.com/nodejs/node/pull/35656)
* [[`5a48a7b6f8`](https://github.com/nodejs/node/commit/5a48a7b6f8)] - **doc**: add a statement in the documentation. (Pooja D.P) [#35585](https://github.com/nodejs/node/pull/35585)
* [[`6a7a61be7c`](https://github.com/nodejs/node/commit/6a7a61be7c)] - **src**: mark/pop OpenSSL errors in NewRootCertStore (Daniel Bevenius) [#35514](https://github.com/nodejs/node/pull/35514)
* [[`d54edece99`](https://github.com/nodejs/node/commit/d54edece99)] - **test**: refactor test-crypto-pbkdf2 (Tobias Nießen) [#35693](https://github.com/nodejs/node/pull/35693)

<a id="15.0.0"></a>
## 2020-10-20, Version 15.0.0 (Current), @BethGriggs

### Notable Changes

#### Deprecations and Removals

* [[`a11788736a`](https://github.com/nodejs/node/commit/a11788736a)] - **(SEMVER-MAJOR)** **build**: remove --build-v8-with-gn configure option (Yang Guo) [#27576](https://github.com/nodejs/node/pull/27576)
* [[`89428c7a2d`](https://github.com/nodejs/node/commit/89428c7a2d)] - **(SEMVER-MAJOR)** **build**: drop support for VS2017 (Michaël Zasso) [#33694](https://github.com/nodejs/node/pull/33694)
* [[`c25cf34ac1`](https://github.com/nodejs/node/commit/c25cf34ac1)] - **(SEMVER-MAJOR)** **doc**: move DEP0018 to End-of-Life (Rich Trott) [#35316](https://github.com/nodejs/node/pull/35316)
* [[`2002d90abd`](https://github.com/nodejs/node/commit/2002d90abd)] - **(SEMVER-MAJOR)** **fs**: deprecation warning on recursive rmdir (Ian Sutherland) [#35562](https://github.com/nodejs/node/pull/35562)
* [[`eee522ac29`](https://github.com/nodejs/node/commit/eee522ac29)] - **(SEMVER-MAJOR)** **lib**: add EventTarget-related browser globals (Anna Henningsen) [#35496](https://github.com/nodejs/node/pull/35496)
* [[`41796ebd30`](https://github.com/nodejs/node/commit/41796ebd30)] - **(SEMVER-MAJOR)** **net**: remove long deprecated server.connections property (James M Snell) [#33647](https://github.com/nodejs/node/pull/33647)
* [[`a416692e93`](https://github.com/nodejs/node/commit/a416692e93)] - **(SEMVER-MAJOR)** **repl**: remove deprecated repl.memory function (Ruben Bridgewater) [#33286](https://github.com/nodejs/node/pull/33286)
* [[`f217b2dfb0`](https://github.com/nodejs/node/commit/f217b2dfb0)] - **(SEMVER-MAJOR)** **repl**: remove deprecated repl.turnOffEditorMode() function (Ruben Bridgewater) [#33286](https://github.com/nodejs/node/pull/33286)
* [[`a1bcad8dc0`](https://github.com/nodejs/node/commit/a1bcad8dc0)] - **(SEMVER-MAJOR)** **repl**: remove deprecated repl.parseREPLKeyword() function (Ruben Bridgewater) [#33286](https://github.com/nodejs/node/pull/33286)
* [[`4ace010b53`](https://github.com/nodejs/node/commit/4ace010b53)] - **(SEMVER-MAJOR)** **repl**: remove deprecated bufferedCommand property (Ruben Bridgewater) [#33286](https://github.com/nodejs/node/pull/33286)
* [[`37524307fe`](https://github.com/nodejs/node/commit/37524307fe)] - **(SEMVER-MAJOR)** **repl**: remove deprecated .rli (Ruben Bridgewater) [#33286](https://github.com/nodejs/node/pull/33286)
* [[`a85ce885bd`](https://github.com/nodejs/node/commit/a85ce885bd)] - **(SEMVER-MAJOR)** **src**: remove deprecated node debug command (James M Snell) [#33648](https://github.com/nodejs/node/pull/33648)
* [[`a8904e8eee`](https://github.com/nodejs/node/commit/a8904e8eee)] - **(SEMVER-MAJOR)** **timers**: introduce timers/promises (James M Snell) [#33950](https://github.com/nodejs/node/pull/33950)
* [[`1211b9a72f`](https://github.com/nodejs/node/commit/1211b9a72f)] - **(SEMVER-MAJOR)** **util**: change default value of `maxStringLength` to 10000 (unknown) [#32744](https://github.com/nodejs/node/pull/32744)
* [[`ca8f3ef2e5`](https://github.com/nodejs/node/commit/ca8f3ef2e5)] - **(SEMVER-MAJOR)** **wasi**: drop --experimental-wasm-bigint requirement (Colin Ihrig) [#35415](https://github.com/nodejs/node/pull/35415)

#### npm 7 - [#35631](https://github.com/nodejs/node/pull/35631)

Node.js 15 comes with a new major release of npm, npm 7. npm 7 comes with many new features - including npm workspaces and a new package-lock.json format. npm 7 also includes yarn.lock file support. One of the big changes in npm 7 is that peer dependencies are now installed by default.

#### Throw On Unhandled Rejections - [#33021](https://github.com/nodejs/node/pull/33021)

As of Node.js 15, the default mode for `unhandledRejection` is changed to `throw` (from `warn`). In `throw` mode, if an `unhandledRejection` hook is not set, the `unhandledRejection` is raised as an uncaught exception. Users that have an `unhandledRejection` hook should see no change in behavior, and it’s still possible to switch modes using the `--unhandled-rejections=mode` process flag.

#### QUIC - [#32379](https://github.com/nodejs/node/pull/32379)

Node.js 15 comes with experimental support QUIC, which can be enabled by compiling Node.js with the `--experimental-quic` configuration flag. The Node.js QUIC implementation is exposed by the core `net` module.

#### V8 8.6 - [#35415](https://github.com/nodejs/node/pull/35415)

The V8 JavaScript engine has been updated to V8 8.6 (V8 8.4 is the latest available in Node.js 14). Along with performance tweaks and improvements the V8 update also brings the following language features:
* `Promise.any()` (from V8 8.5)
* `AggregateError` (from V8 8.5)
* `String.prototype.replaceAll()` (from V8 8.5)
* Logical assignment operators `&&=`, `||=`, and `??=` (from V8 8.5)

#### Other Notable Changes

* [[`50228cf6ff`](https://github.com/nodejs/node/commit/50228cf6ff)] - **(SEMVER-MAJOR)** **assert**: add `assert/strict` alias module (ExE Boss) [#34001](https://github.com/nodejs/node/pull/34001)
* [[`039cd00a9a`](https://github.com/nodejs/node/commit/039cd00a9a)] - **(SEMVER-MAJOR)** **dns**: add dns/promises alias (shisama) [#32953](https://github.com/nodejs/node/pull/32953)
* [[`54b36e401d`](https://github.com/nodejs/node/commit/54b36e401d)] - **(SEMVER-MAJOR)** **fs**: reimplement read and write streams using stream.construct (Robert Nagy) [#29656](https://github.com/nodejs/node/pull/29656)
* [[`f5c0e282cc`](https://github.com/nodejs/node/commit/f5c0e282cc)] - **(SEMVER-MAJOR)** **http2**: allow Host in HTTP/2 requests (Alba Mendez) [#34664](https://github.com/nodejs/node/pull/34664)
* [[`eee522ac29`](https://github.com/nodejs/node/commit/eee522ac29)] - **(SEMVER-MAJOR)** **lib**: add EventTarget-related browser globals (Anna Henningsen) [#35496](https://github.com/nodejs/node/pull/35496)
* [[`a8b26d72c5`](https://github.com/nodejs/node/commit/a8b26d72c5)] - **(SEMVER-MAJOR)** **lib**: unflag AbortController (James M Snell) [#33527](https://github.com/nodejs/node/pull/33527)
* [[`74ca960aac`](https://github.com/nodejs/node/commit/74ca960aac)] - **(SEMVER-MAJOR)** **lib**: initial experimental AbortController implementation (James M Snell) [#33527](https://github.com/nodejs/node/pull/33527)
* [[`efefdd668d`](https://github.com/nodejs/node/commit/efefdd668d)] - **(SEMVER-MAJOR)** **net**: autoDestroy Socket (Robert Nagy) [#31806](https://github.com/nodejs/node/pull/31806)
* [[`0fb91acedf`](https://github.com/nodejs/node/commit/0fb91acedf)] - **(SEMVER-MAJOR)** **src**: disallow JS execution inside FreeEnvironment (Anna Henningsen) [#33874](https://github.com/nodejs/node/pull/33874)
* [[`21782277c2`](https://github.com/nodejs/node/commit/21782277c2)] - **(SEMVER-MAJOR)** **src**: use node:moduleName as builtin module filename (Michaël Zasso) [#35498](https://github.com/nodejs/node/pull/35498)
* [[`fb8cc72e73`](https://github.com/nodejs/node/commit/fb8cc72e73)] - **(SEMVER-MAJOR)** **stream**: construct (Robert Nagy) [#29656](https://github.com/nodejs/node/pull/29656)
* [[`705d888387`](https://github.com/nodejs/node/commit/705d888387)] - **(SEMVER-MAJOR)** **worker**: make MessageEvent class more Web-compatible (Anna Henningsen) [#35496](https://github.com/nodejs/node/pull/35496)

### Semver-Major Commits

* [[`50228cf6ff`](https://github.com/nodejs/node/commit/50228cf6ff)] - **(SEMVER-MAJOR)** **assert**: add `assert/strict` alias module (ExE Boss) [#34001](https://github.com/nodejs/node/pull/34001)
* [[`d701247165`](https://github.com/nodejs/node/commit/d701247165)] - **(SEMVER-MAJOR)** **build**: reset embedder string to "-node.0" (Michaël Zasso) [#35415](https://github.com/nodejs/node/pull/35415)
* [[`a11788736a`](https://github.com/nodejs/node/commit/a11788736a)] - **(SEMVER-MAJOR)** **build**: remove --build-v8-with-gn configure option (Yang Guo) [#27576](https://github.com/nodejs/node/pull/27576)
* [[`89428c7a2d`](https://github.com/nodejs/node/commit/89428c7a2d)] - **(SEMVER-MAJOR)** **build**: drop support for VS2017 (Michaël Zasso) [#33694](https://github.com/nodejs/node/pull/33694)
* [[`dae283d96f`](https://github.com/nodejs/node/commit/dae283d96f)] - **(SEMVER-MAJOR)** **crypto**: refactoring internals, add WebCrypto (James M Snell) [#35093](https://github.com/nodejs/node/pull/35093)
* [[`ba77dc8597`](https://github.com/nodejs/node/commit/ba77dc8597)] - **(SEMVER-MAJOR)** **crypto**: move node\_crypto files to src/crypto (James M Snell) [#35093](https://github.com/nodejs/node/pull/35093)
* [[`9378070da0`](https://github.com/nodejs/node/commit/9378070da0)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick d76abfed3512 (Michaël Zasso) [#35415](https://github.com/nodejs/node/pull/35415)
* [[`efee8341ad`](https://github.com/nodejs/node/commit/efee8341ad)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick 717543bbf0ef (Michaël Zasso) [#35415](https://github.com/nodejs/node/pull/35415)
* [[`b006fa8730`](https://github.com/nodejs/node/commit/b006fa8730)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick 6be2f6e26e8d (Michaël Zasso) [#35415](https://github.com/nodejs/node/pull/35415)
* [[`3c23af4cb7`](https://github.com/nodejs/node/commit/3c23af4cb7)] - **(SEMVER-MAJOR)** **deps**: fix V8 build issue with inline methods (Jiawen Geng) [#35415](https://github.com/nodejs/node/pull/35415)
* [[`b803b3f48b`](https://github.com/nodejs/node/commit/b803b3f48b)] - **(SEMVER-MAJOR)** **deps**: fix platform-embedded-file-writer-win for ARM64 (Michaël Zasso) [#35415](https://github.com/nodejs/node/pull/35415)
* [[`47cb9f14e8`](https://github.com/nodejs/node/commit/47cb9f14e8)] - **(SEMVER-MAJOR)** **deps**: update V8 postmortem metadata script (Colin Ihrig) [#35415](https://github.com/nodejs/node/pull/35415)
* [[`a1d639ba5d`](https://github.com/nodejs/node/commit/a1d639ba5d)] - **(SEMVER-MAJOR)** **deps**: update V8 to 8.6.395 (Michaël Zasso) [#35415](https://github.com/nodejs/node/pull/35415)
* [[`3ddcad55fb`](https://github.com/nodejs/node/commit/3ddcad55fb)] - **(SEMVER-MAJOR)** **deps**: upgrade npm to 7.0.0 (Myles Borins) [#35631](https://github.com/nodejs/node/pull/35631)
* [[`2e54524955`](https://github.com/nodejs/node/commit/2e54524955)] - **(SEMVER-MAJOR)** **deps**: update npm to 7.0.0-rc.3 (Myles Borins) [#35474](https://github.com/nodejs/node/pull/35474)
* [[`e983b1cece`](https://github.com/nodejs/node/commit/e983b1cece)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick 0d6debcc5f08 (Gus Caplan) [#33600](https://github.com/nodejs/node/pull/33600)
* [[`039cd00a9a`](https://github.com/nodejs/node/commit/039cd00a9a)] - **(SEMVER-MAJOR)** **dns**: add dns/promises alias (shisama) [#32953](https://github.com/nodejs/node/pull/32953)
* [[`c25cf34ac1`](https://github.com/nodejs/node/commit/c25cf34ac1)] - **(SEMVER-MAJOR)** **doc**: move DEP0018 to End-of-Life (Rich Trott) [#35316](https://github.com/nodejs/node/pull/35316)
* [[`8bf37ee496`](https://github.com/nodejs/node/commit/8bf37ee496)] - **(SEMVER-MAJOR)** **doc**: update support macos version for 15.x (Ash Cripps) [#35022](https://github.com/nodejs/node/pull/35022)
* [[`2002d90abd`](https://github.com/nodejs/node/commit/2002d90abd)] - **(SEMVER-MAJOR)** **fs**: deprecation warning on recursive rmdir (Ian Sutherland) [#35562](https://github.com/nodejs/node/pull/35562)
* [[`54b36e401d`](https://github.com/nodejs/node/commit/54b36e401d)] - **(SEMVER-MAJOR)** **fs**: reimplement read and write streams using stream.construct (Robert Nagy) [#29656](https://github.com/nodejs/node/pull/29656)
* [[`32b641e528`](https://github.com/nodejs/node/commit/32b641e528)] - **(SEMVER-MAJOR)** **http**: fixed socket.setEncoding fatal error (iskore) [#33405](https://github.com/nodejs/node/pull/33405)
* [[`8a6fab02ad`](https://github.com/nodejs/node/commit/8a6fab02ad)] - **(SEMVER-MAJOR)** **http**: emit 'error' on aborted server request (Robert Nagy) [#33172](https://github.com/nodejs/node/pull/33172)
* [[`d005f490a8`](https://github.com/nodejs/node/commit/d005f490a8)] - **(SEMVER-MAJOR)** **http**: cleanup end argument handling (Robert Nagy) [#31818](https://github.com/nodejs/node/pull/31818)
* [[`f5c0e282cc`](https://github.com/nodejs/node/commit/f5c0e282cc)] - **(SEMVER-MAJOR)** **http2**: allow Host in HTTP/2 requests (Alba Mendez) [#34664](https://github.com/nodejs/node/pull/34664)
* [[`1e4187fcf4`](https://github.com/nodejs/node/commit/1e4187fcf4)] - **(SEMVER-MAJOR)** **http2**: add `invalidheaders` test (Pranshu Srivastava) [#33161](https://github.com/nodejs/node/pull/33161)
* [[`d79c330186`](https://github.com/nodejs/node/commit/d79c330186)] - **(SEMVER-MAJOR)** **http2**: refactor state code validation for the http2Stream class (rickyes) [#33535](https://github.com/nodejs/node/pull/33535)
* [[`df31f71f1e`](https://github.com/nodejs/node/commit/df31f71f1e)] - **(SEMVER-MAJOR)** **http2**: header field valid checks (Pranshu Srivastava) [#33193](https://github.com/nodejs/node/pull/33193)
* [[`1428db8a1f`](https://github.com/nodejs/node/commit/1428db8a1f)] - **(SEMVER-MAJOR)** **lib**: refactor Socket.\_getpeername and Socket.\_getsockname (himself65) [#32969](https://github.com/nodejs/node/pull/32969)
* [[`eee522ac29`](https://github.com/nodejs/node/commit/eee522ac29)] - **(SEMVER-MAJOR)** **lib**: add EventTarget-related browser globals (Anna Henningsen) [#35496](https://github.com/nodejs/node/pull/35496)
* [[`c66e6471e7`](https://github.com/nodejs/node/commit/c66e6471e7)] - **(SEMVER-MAJOR)** **lib**: remove ERR\_INVALID\_OPT\_VALUE and ERR\_INVALID\_OPT\_VALUE\_ENCODING (Denys Otrishko) [#34682](https://github.com/nodejs/node/pull/34682)
* [[`b546a2b469`](https://github.com/nodejs/node/commit/b546a2b469)] - **(SEMVER-MAJOR)** **lib**: handle one of args case in ERR\_MISSING\_ARGS (Denys Otrishko) [#34022](https://github.com/nodejs/node/pull/34022)
* [[`a86a295fd7`](https://github.com/nodejs/node/commit/a86a295fd7)] - **(SEMVER-MAJOR)** **lib**: remove NodeError from the prototype of errors with code (Michaël Zasso) [#33857](https://github.com/nodejs/node/pull/33857)
* [[`a8b26d72c5`](https://github.com/nodejs/node/commit/a8b26d72c5)] - **(SEMVER-MAJOR)** **lib**: unflag AbortController (James M Snell) [#33527](https://github.com/nodejs/node/pull/33527)
* [[`74ca960aac`](https://github.com/nodejs/node/commit/74ca960aac)] - **(SEMVER-MAJOR)** **lib**: initial experimental AbortController implementation (James M Snell) [#33527](https://github.com/nodejs/node/pull/33527)
* [[`78ca61e2cf`](https://github.com/nodejs/node/commit/78ca61e2cf)] - **(SEMVER-MAJOR)** **net**: check args in net.connect() and socket.connect() calls (Denys Otrishko) [#34022](https://github.com/nodejs/node/pull/34022)
* [[`41796ebd30`](https://github.com/nodejs/node/commit/41796ebd30)] - **(SEMVER-MAJOR)** **net**: remove long deprecated server.connections property (James M Snell) [#33647](https://github.com/nodejs/node/pull/33647)
* [[`efefdd668d`](https://github.com/nodejs/node/commit/efefdd668d)] - **(SEMVER-MAJOR)** **net**: autoDestroy Socket (Robert Nagy) [#31806](https://github.com/nodejs/node/pull/31806)
* [[`6cfba9f7f6`](https://github.com/nodejs/node/commit/6cfba9f7f6)] - **(SEMVER-MAJOR)** **process**: update v8 fast api calls usage (Maya Lekova) [#35415](https://github.com/nodejs/node/pull/35415)
* [[`3b10f7f933`](https://github.com/nodejs/node/commit/3b10f7f933)] - **(SEMVER-MAJOR)** **process**: change default --unhandled-rejections=throw (Dan Fabulich) [#33021](https://github.com/nodejs/node/pull/33021)
* [[`d8eef83757`](https://github.com/nodejs/node/commit/d8eef83757)] - **(SEMVER-MAJOR)** **process**: use v8 fast api calls for hrtime (Gus Caplan) [#33600](https://github.com/nodejs/node/pull/33600)
* [[`49745cdef0`](https://github.com/nodejs/node/commit/49745cdef0)] - **(SEMVER-MAJOR)** **process**: delay throwing an error using `throwDeprecation` (Ruben Bridgewater) [#32312](https://github.com/nodejs/node/pull/32312)
* [[`a416692e93`](https://github.com/nodejs/node/commit/a416692e93)] - **(SEMVER-MAJOR)** **repl**: remove deprecated repl.memory function (Ruben Bridgewater) [#33286](https://github.com/nodejs/node/pull/33286)
* [[`f217b2dfb0`](https://github.com/nodejs/node/commit/f217b2dfb0)] - **(SEMVER-MAJOR)** **repl**: remove deprecated repl.turnOffEditorMode() function (Ruben Bridgewater) [#33286](https://github.com/nodejs/node/pull/33286)
* [[`a1bcad8dc0`](https://github.com/nodejs/node/commit/a1bcad8dc0)] - **(SEMVER-MAJOR)** **repl**: remove deprecated repl.parseREPLKeyword() function (Ruben Bridgewater) [#33286](https://github.com/nodejs/node/pull/33286)
* [[`4ace010b53`](https://github.com/nodejs/node/commit/4ace010b53)] - **(SEMVER-MAJOR)** **repl**: remove deprecated bufferedCommand property (Ruben Bridgewater) [#33286](https://github.com/nodejs/node/pull/33286)
* [[`37524307fe`](https://github.com/nodejs/node/commit/37524307fe)] - **(SEMVER-MAJOR)** **repl**: remove deprecated .rli (Ruben Bridgewater) [#33286](https://github.com/nodejs/node/pull/33286)
* [[`b65e5aeaa7`](https://github.com/nodejs/node/commit/b65e5aeaa7)] - **(SEMVER-MAJOR)** **src**: implement NodePlatform::PostJob (Clemens Backes) [#35415](https://github.com/nodejs/node/pull/35415)
* [[`b1e8e0e604`](https://github.com/nodejs/node/commit/b1e8e0e604)] - **(SEMVER-MAJOR)** **src**: update NODE\_MODULE\_VERSION to 88 (Michaël Zasso) [#35415](https://github.com/nodejs/node/pull/35415)
* [[`eeb6b473fd`](https://github.com/nodejs/node/commit/eeb6b473fd)] - **(SEMVER-MAJOR)** **src**: error reporting on CPUUsage (Yash Ladha) [#34762](https://github.com/nodejs/node/pull/34762)
* [[`21782277c2`](https://github.com/nodejs/node/commit/21782277c2)] - **(SEMVER-MAJOR)** **src**: use node:moduleName as builtin module filename (Michaël Zasso) [#35498](https://github.com/nodejs/node/pull/35498)
* [[`05771279af`](https://github.com/nodejs/node/commit/05771279af)] - **(SEMVER-MAJOR)** **src**: enable wasm trap handler on windows (Gus Caplan) [#35033](https://github.com/nodejs/node/pull/35033)
* [[`b7cf823410`](https://github.com/nodejs/node/commit/b7cf823410)] - **(SEMVER-MAJOR)** **src**: update NODE\_MODULE\_VERSION to 86 (Michaël Zasso) [#33579](https://github.com/nodejs/node/pull/33579)
* [[`0fb91acedf`](https://github.com/nodejs/node/commit/0fb91acedf)] - **(SEMVER-MAJOR)** **src**: disallow JS execution inside FreeEnvironment (Anna Henningsen) [#33874](https://github.com/nodejs/node/pull/33874)
* [[`53fb2b6b41`](https://github.com/nodejs/node/commit/53fb2b6b41)] - **(SEMVER-MAJOR)** **src**: remove \_third\_party\_main support (Anna Henningsen) [#33971](https://github.com/nodejs/node/pull/33971)
* [[`a85ce885bd`](https://github.com/nodejs/node/commit/a85ce885bd)] - **(SEMVER-MAJOR)** **src**: remove deprecated node debug command (James M Snell) [#33648](https://github.com/nodejs/node/pull/33648)
* [[`ac3714637e`](https://github.com/nodejs/node/commit/ac3714637e)] - **(SEMVER-MAJOR)** **src**: remove unused CancelPendingDelayedTasks (Anna Henningsen) [#32859](https://github.com/nodejs/node/pull/32859)
* [[`a65218f5e8`](https://github.com/nodejs/node/commit/a65218f5e8)] - **(SEMVER-MAJOR)** **stream**: try to wait for flush to complete before 'finish' (Robert Nagy) [#34314](https://github.com/nodejs/node/pull/34314)
* [[`4e3f6f355b`](https://github.com/nodejs/node/commit/4e3f6f355b)] - **(SEMVER-MAJOR)** **stream**: cleanup and fix Readable.wrap (Robert Nagy) [#34204](https://github.com/nodejs/node/pull/34204)
* [[`527e2147af`](https://github.com/nodejs/node/commit/527e2147af)] - **(SEMVER-MAJOR)** **stream**: add promises version to utility functions (rickyes) [#33991](https://github.com/nodejs/node/pull/33991)
* [[`c7e55c6b72`](https://github.com/nodejs/node/commit/c7e55c6b72)] - **(SEMVER-MAJOR)** **stream**: fix writable.end callback behavior (Robert Nagy) [#34101](https://github.com/nodejs/node/pull/34101)
* [[`fb8cc72e73`](https://github.com/nodejs/node/commit/fb8cc72e73)] - **(SEMVER-MAJOR)** **stream**: construct (Robert Nagy) [#29656](https://github.com/nodejs/node/pull/29656)
* [[`4bc7025309`](https://github.com/nodejs/node/commit/4bc7025309)] - **(SEMVER-MAJOR)** **stream**: write should throw on unknown encoding (Robert Nagy) [#33075](https://github.com/nodejs/node/pull/33075)
* [[`ea87809bb6`](https://github.com/nodejs/node/commit/ea87809bb6)] - **(SEMVER-MAJOR)** **stream**: fix \_final and 'prefinish' timing (Robert Nagy) [#32780](https://github.com/nodejs/node/pull/32780)
* [[`0bd5595509`](https://github.com/nodejs/node/commit/0bd5595509)] - **(SEMVER-MAJOR)** **stream**: simplify Transform stream implementation (Robert Nagy) [#32763](https://github.com/nodejs/node/pull/32763)
* [[`8f86986985`](https://github.com/nodejs/node/commit/8f86986985)] - **(SEMVER-MAJOR)** **stream**: use callback to properly propagate error (Robert Nagy) [#29179](https://github.com/nodejs/node/pull/29179)
* [[`94dd7b9f94`](https://github.com/nodejs/node/commit/94dd7b9f94)] - **(SEMVER-MAJOR)** **test**: update tests after increasing typed array size to 4GB (Kim-Anh Tran) [#35415](https://github.com/nodejs/node/pull/35415)
* [[`d9e98df01b`](https://github.com/nodejs/node/commit/d9e98df01b)] - **(SEMVER-MAJOR)** **test**: fix tests for npm 7.0.0 (Myles Borins) [#35631](https://github.com/nodejs/node/pull/35631)
* [[`c87641aa97`](https://github.com/nodejs/node/commit/c87641aa97)] - **(SEMVER-MAJOR)** **test**: fix test suite to work with npm 7 (Myles Borins) [#35474](https://github.com/nodejs/node/pull/35474)
* [[`eb9d7a437e`](https://github.com/nodejs/node/commit/eb9d7a437e)] - **(SEMVER-MAJOR)** **test**: update WPT harness and tests (Michaël Zasso) [#33770](https://github.com/nodejs/node/pull/33770)
* [[`a8904e8eee`](https://github.com/nodejs/node/commit/a8904e8eee)] - **(SEMVER-MAJOR)** **timers**: introduce timers/promises (James M Snell) [#33950](https://github.com/nodejs/node/pull/33950)
* [[`c55f661551`](https://github.com/nodejs/node/commit/c55f661551)] - **(SEMVER-MAJOR)** **tools**: disable x86 safe exception handlers in V8 (Michaël Zasso) [#35415](https://github.com/nodejs/node/pull/35415)
* [[`80e8aec4a5`](https://github.com/nodejs/node/commit/80e8aec4a5)] - **(SEMVER-MAJOR)** **tools**: update V8 gypfiles for 8.6 (Ujjwal Sharma) [#35415](https://github.com/nodejs/node/pull/35415)
* [[`faeb9607c6`](https://github.com/nodejs/node/commit/faeb9607c6)] - **(SEMVER-MAJOR)** **tools**: update V8 gypfiles for 8.5 (Ujjwal Sharma) [#35415](https://github.com/nodejs/node/pull/35415)
* [[`bb62f4ad9e`](https://github.com/nodejs/node/commit/bb62f4ad9e)] - **(SEMVER-MAJOR)** **url**: file URL path normalization (Daijiro Wachi) [#35477](https://github.com/nodejs/node/pull/35477)
* [[`69ef4c2375`](https://github.com/nodejs/node/commit/69ef4c2375)] - **(SEMVER-MAJOR)** **url**: verify domain is not empty after "ToASCII" (Michaël Zasso) [#33770](https://github.com/nodejs/node/pull/33770)
* [[`4831278a16`](https://github.com/nodejs/node/commit/4831278a16)] - **(SEMVER-MAJOR)** **url**: remove U+0000 case in the fragment state (Michaël Zasso) [#33770](https://github.com/nodejs/node/pull/33770)
* [[`0d08d5ae7c`](https://github.com/nodejs/node/commit/0d08d5ae7c)] - **(SEMVER-MAJOR)** **url**: remove gopher from special schemes (Michaël Zasso) [#33325](https://github.com/nodejs/node/pull/33325)
* [[`9be51ee9a1`](https://github.com/nodejs/node/commit/9be51ee9a1)] - **(SEMVER-MAJOR)** **url**: forbid lt and gt in url host code point (Yash Ladha) [#33328](https://github.com/nodejs/node/pull/33328)
* [[`1211b9a72f`](https://github.com/nodejs/node/commit/1211b9a72f)] - **(SEMVER-MAJOR)** **util**: change default value of `maxStringLength` to 10000 (unknown) [#32744](https://github.com/nodejs/node/pull/32744)
* [[`ca8f3ef2e5`](https://github.com/nodejs/node/commit/ca8f3ef2e5)] - **(SEMVER-MAJOR)** **wasi**: drop --experimental-wasm-bigint requirement (Colin Ihrig) [#35415](https://github.com/nodejs/node/pull/35415)
* [[`abd8cdfc4e`](https://github.com/nodejs/node/commit/abd8cdfc4e)] - **(SEMVER-MAJOR)** **win, child_process**: sanitize env variables (Bartosz Sosnowski) [#35210](https://github.com/nodejs/node/pull/35210)
* [[`705d888387`](https://github.com/nodejs/node/commit/705d888387)] - **(SEMVER-MAJOR)** **worker**: make MessageEvent class more Web-compatible (Anna Henningsen) [#35496](https://github.com/nodejs/node/pull/35496)
* [[`7603c7e50c`](https://github.com/nodejs/node/commit/7603c7e50c)] - **(SEMVER-MAJOR)** **worker**: set trackUnmanagedFds to true by default (Anna Henningsen) [#34394](https://github.com/nodejs/node/pull/34394)
* [[`5ef5116311`](https://github.com/nodejs/node/commit/5ef5116311)] - **(SEMVER-MAJOR)** **worker**: rename error code to be more accurate (Anna Henningsen) [#33872](https://github.com/nodejs/node/pull/33872)

### Semver-Minor Commits

* [[`1d5fa88eb8`](https://github.com/nodejs/node/commit/1d5fa88eb8)] - **(SEMVER-MINOR)** **cli**: add --node-memory-debug option (Anna Henningsen) [#35537](https://github.com/nodejs/node/pull/35537)
* [[`095be6a01f`](https://github.com/nodejs/node/commit/095be6a01f)] - **(SEMVER-MINOR)** **crypto**: add getCipherInfo method (James M Snell) [#35368](https://github.com/nodejs/node/pull/35368)
* [[`df1023bb22`](https://github.com/nodejs/node/commit/df1023bb22)] - **(SEMVER-MINOR)** **events**: allow use of AbortController with on (James M Snell) [#34912](https://github.com/nodejs/node/pull/34912)
* [[`883fc779b6`](https://github.com/nodejs/node/commit/883fc779b6)] - **(SEMVER-MINOR)** **events**: allow use of AbortController with once (James M Snell) [#34911](https://github.com/nodejs/node/pull/34911)
* [[`e876c0c308`](https://github.com/nodejs/node/commit/e876c0c308)] - **(SEMVER-MINOR)** **http2**: add support for sensitive headers (Anna Henningsen) [#34145](https://github.com/nodejs/node/pull/34145)
* [[`6f34498148`](https://github.com/nodejs/node/commit/6f34498148)] - **(SEMVER-MINOR)** **net**: add support for resolving DNS CAA records (Danny Sonnenschein) [#35466](https://github.com/nodejs/node/pull/35466)
* [[`37a8179673`](https://github.com/nodejs/node/commit/37a8179673)] - **(SEMVER-MINOR)** **net**: make blocklist family case insensitive (James M Snell) [#34864](https://github.com/nodejs/node/pull/34864)
* [[`1f9b20b637`](https://github.com/nodejs/node/commit/1f9b20b637)] - **(SEMVER-MINOR)** **net**: introduce net.BlockList (James M Snell) [#34625](https://github.com/nodejs/node/pull/34625)
* [[`278d38f4cf`](https://github.com/nodejs/node/commit/278d38f4cf)] - **(SEMVER-MINOR)** **src**: add maybe versions of EmitExit and EmitBeforeExit (Anna Henningsen) [#35486](https://github.com/nodejs/node/pull/35486)
* [[`2310f679a1`](https://github.com/nodejs/node/commit/2310f679a1)] - **(SEMVER-MINOR)** **src**: move node\_binding to modern THROW\_ERR\* (James M Snell) [#35469](https://github.com/nodejs/node/pull/35469)
* [[`744a284ccc`](https://github.com/nodejs/node/commit/744a284ccc)] - **(SEMVER-MINOR)** **stream**: support async for stream impl functions (James M Snell) [#34416](https://github.com/nodejs/node/pull/34416)
* [[`bfbdc84738`](https://github.com/nodejs/node/commit/bfbdc84738)] - **(SEMVER-MINOR)** **timers**: allow promisified timeouts/immediates to be canceled (James M Snell) [#33833](https://github.com/nodejs/node/pull/33833)
* [[`a8971f87d3`](https://github.com/nodejs/node/commit/a8971f87d3)] - **(SEMVER-MINOR)** **url**: support non-special URLs (Daijiro Wachi) [#34925](https://github.com/nodejs/node/pull/34925)

### Semver-Patch Commits

* [[`d10c59fc60`](https://github.com/nodejs/node/commit/d10c59fc60)] - **benchmark,test**: remove output from readable-async-iterator benchmark (Rich Trott) [#34411](https://github.com/nodejs/node/pull/34411)
* [[`8a12e9994f`](https://github.com/nodejs/node/commit/8a12e9994f)] - **bootstrap**: use file URL instead of relative url (Daijiro Wachi) [#35622](https://github.com/nodejs/node/pull/35622)
* [[`f8bde7ce06`](https://github.com/nodejs/node/commit/f8bde7ce06)] - **bootstrap**: build fast APIs in pre-execution (Joyee Cheung) [#32984](https://github.com/nodejs/node/pull/32984)
* [[`b18651bcd2`](https://github.com/nodejs/node/commit/b18651bcd2)] - **build**: do not pass mode option to test-v8 command (Michaël Zasso) [#35705](https://github.com/nodejs/node/pull/35705)
* [[`bb2945ed6b`](https://github.com/nodejs/node/commit/bb2945ed6b)] - **build**: add GitHub Action for code coverage (Benjamin Coe) [#35653](https://github.com/nodejs/node/pull/35653)
* [[`cfbbeea4a1`](https://github.com/nodejs/node/commit/cfbbeea4a1)] - **build**: use GITHUB\_ENV file to set env variables (Michaël Zasso) [#35638](https://github.com/nodejs/node/pull/35638)
* [[`8a93b371a3`](https://github.com/nodejs/node/commit/8a93b371a3)] - **build**: do not install jq in workflows (Michaël Zasso) [#35638](https://github.com/nodejs/node/pull/35638)
* [[`ccbd1d5efa`](https://github.com/nodejs/node/commit/ccbd1d5efa)] - **build**: add quic to github action (gengjiawen) [#34336](https://github.com/nodejs/node/pull/34336)
* [[`f4f191bbc2`](https://github.com/nodejs/node/commit/f4f191bbc2)] - **build**: define NODE\_EXPERIMENTAL\_QUIC in mkcodecache and node\_mksnapshot (Joyee Cheung) [#34454](https://github.com/nodejs/node/pull/34454)
* [[`5b2c263ba8`](https://github.com/nodejs/node/commit/5b2c263ba8)] - **deps**: fix typo in zlib.gyp that break arm-fpu-neon build (lucasg) [#35659](https://github.com/nodejs/node/pull/35659)
* [[`5b9593f727`](https://github.com/nodejs/node/commit/5b9593f727)] - **deps**: upgrade npm to 7.0.2 (Myles Borins) [#35667](https://github.com/nodejs/node/pull/35667)
* [[`dabc6ddddc`](https://github.com/nodejs/node/commit/dabc6ddddc)] - **deps**: upgrade npm to 7.0.0-rc.4 (Myles Borins) [#35576](https://github.com/nodejs/node/pull/35576)
* [[`757bac6711`](https://github.com/nodejs/node/commit/757bac6711)] - **deps**: update nghttp3 (James M Snell) [#34752](https://github.com/nodejs/node/pull/34752)
* [[`c788be2e6e`](https://github.com/nodejs/node/commit/c788be2e6e)] - **deps**: update ngtcp2 (James M Snell) [#34752](https://github.com/nodejs/node/pull/34752)
* [[`7816e5f7b9`](https://github.com/nodejs/node/commit/7816e5f7b9)] - **deps**: fix indenting of sources in ngtcp2.gyp (James M Snell) [#34033](https://github.com/nodejs/node/pull/34033)
* [[`f5343d1b40`](https://github.com/nodejs/node/commit/f5343d1b40)] - **deps**: re-enable OPENSSL\_NO\_QUIC guards (James M Snell) [#34033](https://github.com/nodejs/node/pull/34033)
* [[`9de95f494e`](https://github.com/nodejs/node/commit/9de95f494e)] - **deps**: temporary fixup for ngtcp2 to build on windows (James M Snell) [#34033](https://github.com/nodejs/node/pull/34033)
* [[`ec7ad1d0ec`](https://github.com/nodejs/node/commit/ec7ad1d0ec)] - **deps**: cherry-pick akamai/openssl/commit/bf4b08ecfbb7a26ca4b0b9ecaee3b31d18d7bda9 (Tatsuhiro Tsujikawa) [#34033](https://github.com/nodejs/node/pull/34033)
* [[`c3d85b7637`](https://github.com/nodejs/node/commit/c3d85b7637)] - **deps**: cherry-pick akamai/openssl/commit/a5a08cb8050bb69120e833456e355f482e392456 (Benjamin Kaduk) [#34033](https://github.com/nodejs/node/pull/34033)
* [[`bad1a150ea`](https://github.com/nodejs/node/commit/bad1a150ea)] - **deps**: cherry-pick akamai/openssl/commit/d5a13ca6e29f3ff85c731770ab0ee2f2487bf8b3 (Benjamin Kaduk) [#34033](https://github.com/nodejs/node/pull/34033)
* [[`74cbfd3f36`](https://github.com/nodejs/node/commit/74cbfd3f36)] - **deps**: cherry-pick akamai/openssl/commit/a6282c566d88db11300c82abc3c84a4e2e9ea568 (Benjamin Kaduk) [#34033](https://github.com/nodejs/node/pull/34033)
* [[`8a9763a8ea`](https://github.com/nodejs/node/commit/8a9763a8ea)] - **deps**: update nghttp3 (James M Snell) [#34033](https://github.com/nodejs/node/pull/34033)
* [[`6b27d07779`](https://github.com/nodejs/node/commit/6b27d07779)] - **deps**: update ngtcp2 (James M Snell) [#34033](https://github.com/nodejs/node/pull/34033)
* [[`a041723774`](https://github.com/nodejs/node/commit/a041723774)] - **deps**: fix indentation for sources in nghttp3.gyp (Daniel Bevenius) [#33942](https://github.com/nodejs/node/pull/33942)
* [[`a0cbd676e7`](https://github.com/nodejs/node/commit/a0cbd676e7)] - **deps**: add defines to nghttp3/ngtcp2 gyp configs (Daniel Bevenius) [#33942](https://github.com/nodejs/node/pull/33942)
* [[`bccb514936`](https://github.com/nodejs/node/commit/bccb514936)] - **deps**: maintaining ngtcp2 and nghttp3 (James M Snell) [#32379](https://github.com/nodejs/node/pull/32379)
* [[`834fa8f23f`](https://github.com/nodejs/node/commit/834fa8f23f)] - **deps**: add ngtcp2 and nghttp3 (James M Snell) [#32379](https://github.com/nodejs/node/pull/32379)
* [[`f96b981528`](https://github.com/nodejs/node/commit/f96b981528)] - **deps**: details for updating openssl quic support (James M Snell) [#32379](https://github.com/nodejs/node/pull/32379)
* [[`98c8498552`](https://github.com/nodejs/node/commit/98c8498552)] - **deps**: update archs files for OpenSSL-1.1.0 (James M Snell) [#32379](https://github.com/nodejs/node/pull/32379)
* [[`2c549e505e`](https://github.com/nodejs/node/commit/2c549e505e)] - **deps**: add support for BoringSSL QUIC APIs (Todd Short) [#32379](https://github.com/nodejs/node/pull/32379)
* [[`1103b15af6`](https://github.com/nodejs/node/commit/1103b15af6)] - **doc**: fix YAML lint error on master (Rich Trott) [#35709](https://github.com/nodejs/node/pull/35709)
* [[`7798e59e98`](https://github.com/nodejs/node/commit/7798e59e98)] - **doc**: upgrade stability status of report API (Gireesh Punathil) [#35654](https://github.com/nodejs/node/pull/35654)
* [[`ce03a182cf`](https://github.com/nodejs/node/commit/ce03a182cf)] - **doc**: clarify experimental API elements in vm.md (Rich Trott) [#35594](https://github.com/nodejs/node/pull/35594)
* [[`89defff3b9`](https://github.com/nodejs/node/commit/89defff3b9)] - **doc**: correct order of metadata for deprecation (Rich Trott) [#35668](https://github.com/nodejs/node/pull/35668)
* [[`ee85eb9f8a`](https://github.com/nodejs/node/commit/ee85eb9f8a)] - **doc**: importModuleDynamically gets Script, not Module (Simen Bekkhus) [#35593](https://github.com/nodejs/node/pull/35593)
* [[`9e5a27a9d3`](https://github.com/nodejs/node/commit/9e5a27a9d3)] - **doc**: fix EventEmitter examples (Sourav Shaw) [#33513](https://github.com/nodejs/node/pull/33513)
* [[`2c2c87e291`](https://github.com/nodejs/node/commit/2c2c87e291)] - **doc**: fix stability indicator in webcrypto doc (Rich Trott) [#35672](https://github.com/nodejs/node/pull/35672)
* [[`f59d4e05a2`](https://github.com/nodejs/node/commit/f59d4e05a2)] - **doc**: add example code for process.getgroups() (Pooja D.P) [#35625](https://github.com/nodejs/node/pull/35625)
* [[`8a3808dc37`](https://github.com/nodejs/node/commit/8a3808dc37)] - **doc**: use kbd element in tty doc (Rich Trott) [#35613](https://github.com/nodejs/node/pull/35613)
* [[`4079bfd462`](https://github.com/nodejs/node/commit/4079bfd462)] - **doc**: Remove reference to io.js (Hussaina Begum Nandyala) [#35618](https://github.com/nodejs/node/pull/35618)
* [[`e6d5af3c95`](https://github.com/nodejs/node/commit/e6d5af3c95)] - **doc**: fix typos in quic.md (Luigi Pinca) [#35444](https://github.com/nodejs/node/pull/35444)
* [[`524123fbf0`](https://github.com/nodejs/node/commit/524123fbf0)] - **doc**: update releaser in v12.18.4 changelog (Beth Griggs) [#35217](https://github.com/nodejs/node/pull/35217)
* [[`ccdd1bd82a`](https://github.com/nodejs/node/commit/ccdd1bd82a)] - **doc**: fix incorrectly marked Buffer in quic.md (Rich Trott) [#35075](https://github.com/nodejs/node/pull/35075)
* [[`cc754f2985`](https://github.com/nodejs/node/commit/cc754f2985)] - **doc**: make AbortSignal text consistent in events.md (Rich Trott) [#35005](https://github.com/nodejs/node/pull/35005)
* [[`f9c362ff6c`](https://github.com/nodejs/node/commit/f9c362ff6c)] - **doc**: revise AbortSignal text and example using events.once() (Rich Trott) [#35005](https://github.com/nodejs/node/pull/35005)
* [[`7aeff6b8c8`](https://github.com/nodejs/node/commit/7aeff6b8c8)] - **doc**: claim ABI version for Electron v12 (Shelley Vohr) [#34816](https://github.com/nodejs/node/pull/34816)
* [[`7a1220a1d7`](https://github.com/nodejs/node/commit/7a1220a1d7)] - **doc**: fix headings in quic.md (Anna Henningsen) [#34717](https://github.com/nodejs/node/pull/34717)
* [[`d5c7aec3cb`](https://github.com/nodejs/node/commit/d5c7aec3cb)] - **doc**: use \_can\_ to describe actions in quic.md (Rich Trott) [#34613](https://github.com/nodejs/node/pull/34613)
* [[`319c275b26`](https://github.com/nodejs/node/commit/319c275b26)] - **doc**: use \_can\_ to describe actions in quic.md (Rich Trott) [#34613](https://github.com/nodejs/node/pull/34613)
* [[`2c30920886`](https://github.com/nodejs/node/commit/2c30920886)] - **doc**: use sentence-case in quic.md headers (Rich Trott) [#34453](https://github.com/nodejs/node/pull/34453)
* [[`8ada27510d`](https://github.com/nodejs/node/commit/8ada27510d)] - **doc**: add missing backticks in timers.md (vsemozhetbyt) [#34030](https://github.com/nodejs/node/pull/34030)
* [[`862d005e60`](https://github.com/nodejs/node/commit/862d005e60)] - **doc**: make globals Extends usage consistent (Colin Ihrig) [#33777](https://github.com/nodejs/node/pull/33777)
* [[`85dbd17bde`](https://github.com/nodejs/node/commit/85dbd17bde)] - **doc**: make perf\_hooks Extends usage consistent (Colin Ihrig) [#33777](https://github.com/nodejs/node/pull/33777)
* [[`2e49010bc8`](https://github.com/nodejs/node/commit/2e49010bc8)] - **doc**: make events Extends usage consistent (Colin Ihrig) [#33777](https://github.com/nodejs/node/pull/33777)
* [[`680fb8fc62`](https://github.com/nodejs/node/commit/680fb8fc62)] - **doc**: fix deprecation "End-of-Life" capitalization (Colin Ihrig) [#33691](https://github.com/nodejs/node/pull/33691)
* [[`458677f5ef`](https://github.com/nodejs/node/commit/458677f5ef)] - **errors**: print original exception context (Benjamin Coe) [#33491](https://github.com/nodejs/node/pull/33491)
* [[`b1831fed3a`](https://github.com/nodejs/node/commit/b1831fed3a)] - **events**: simplify event target agnostic logic in on and once (Denys Otrishko) [#34997](https://github.com/nodejs/node/pull/34997)
* [[`7f25fe8b67`](https://github.com/nodejs/node/commit/7f25fe8b67)] - **fs**: remove unused assignment (Rich Trott) [#35642](https://github.com/nodejs/node/pull/35642)
* [[`2c4f30deea`](https://github.com/nodejs/node/commit/2c4f30deea)] - **fs**: fix when path is buffer on fs.symlinkSync (himself65) [#34540](https://github.com/nodejs/node/pull/34540)
* [[`db0e991d52`](https://github.com/nodejs/node/commit/db0e991d52)] - **fs**: remove custom Buffer pool for streams (Robert Nagy) [#33981](https://github.com/nodejs/node/pull/33981)
* [[`51a2df4439`](https://github.com/nodejs/node/commit/51a2df4439)] - **fs**: document why isPerformingIO is required (Robert Nagy) [#33982](https://github.com/nodejs/node/pull/33982)
* [[`999e7d7b44`](https://github.com/nodejs/node/commit/999e7d7b44)] - **gyp,build**: consistent shared library location (Rod Vagg) [#35635](https://github.com/nodejs/node/pull/35635)
* [[`30cc54275d`](https://github.com/nodejs/node/commit/30cc54275d)] - **http**: don't emit error after close (Robert Nagy) [#33654](https://github.com/nodejs/node/pull/33654)
* [[`ddff2b2b22`](https://github.com/nodejs/node/commit/ddff2b2b22)] - **lib**: honor setUncaughtExceptionCaptureCallback (Gireesh Punathil) [#35595](https://github.com/nodejs/node/pull/35595)
* [[`a8806535d9`](https://github.com/nodejs/node/commit/a8806535d9)] - **lib**: use Object static properties from primordials (Michaël Zasso) [#35380](https://github.com/nodejs/node/pull/35380)
* [[`11f1ad939f`](https://github.com/nodejs/node/commit/11f1ad939f)] - **module**: only try to enrich CJS syntax errors (Michaël Zasso) [#35691](https://github.com/nodejs/node/pull/35691)
* [[`aaf225a2a0`](https://github.com/nodejs/node/commit/aaf225a2a0)] - **module**: add setter for module.parent (Antoine du Hamel) [#35522](https://github.com/nodejs/node/pull/35522)
* [[`109a296e2a`](https://github.com/nodejs/node/commit/109a296e2a)] - **quic**: fix typo in code comment (Ikko Ashimine) [#35308](https://github.com/nodejs/node/pull/35308)
* [[`186230527b`](https://github.com/nodejs/node/commit/186230527b)] - **quic**: fix error message on invalid connection ID (Rich Trott) [#35026](https://github.com/nodejs/node/pull/35026)
* [[`e5116b304f`](https://github.com/nodejs/node/commit/e5116b304f)] - **quic**: remove unused function arguments (Rich Trott) [#35010](https://github.com/nodejs/node/pull/35010)
* [[`449f73e05f`](https://github.com/nodejs/node/commit/449f73e05f)] - **quic**: remove undefined variable (Rich Trott) [#35007](https://github.com/nodejs/node/pull/35007)
* [[`44e6a6af67`](https://github.com/nodejs/node/commit/44e6a6af67)] - **quic**: use qlog fin flag (James M Snell) [#34752](https://github.com/nodejs/node/pull/34752)
* [[`2a80737278`](https://github.com/nodejs/node/commit/2a80737278)] - **quic**: fixups after ngtcp2/nghttp3 update (James M Snell) [#34752](https://github.com/nodejs/node/pull/34752)
* [[`c855c3e8ca`](https://github.com/nodejs/node/commit/c855c3e8ca)] - **quic**: use net.BlockList for limiting access to a QuicSocket (James M Snell) [#34741](https://github.com/nodejs/node/pull/34741)
* [[`bfc35354c1`](https://github.com/nodejs/node/commit/bfc35354c1)] - **quic**: consolidate stats collecting in QuicSession (James M Snell) [#34741](https://github.com/nodejs/node/pull/34741)
* [[`94aa291348`](https://github.com/nodejs/node/commit/94aa291348)] - **quic**: clarify TODO statements (James M Snell) [#34741](https://github.com/nodejs/node/pull/34741)
* [[`19e712b9b2`](https://github.com/nodejs/node/commit/19e712b9b2)] - **quic**: resolve InitializeSecureContext TODO comment (James M Snell) [#34741](https://github.com/nodejs/node/pull/34741)
* [[`240592228b`](https://github.com/nodejs/node/commit/240592228b)] - **quic**: fixup session ticket app data todo comments (James M Snell) [#34741](https://github.com/nodejs/node/pull/34741)
* [[`c17eaa3f3f`](https://github.com/nodejs/node/commit/c17eaa3f3f)] - **quic**: add natRebinding argument to docs (James M Snell) [#34669](https://github.com/nodejs/node/pull/34669)
* [[`442968c92a`](https://github.com/nodejs/node/commit/442968c92a)] - **quic**: check setSocket natRebinding argument, extend test (James M Snell) [#34669](https://github.com/nodejs/node/pull/34669)
* [[`10d5047a4f`](https://github.com/nodejs/node/commit/10d5047a4f)] - **quic**: fixup set\_socket, fix skipped test (James M Snell) [#34669](https://github.com/nodejs/node/pull/34669)
* [[`344c5e4e50`](https://github.com/nodejs/node/commit/344c5e4e50)] - **quic**: limit push check to http/3 (James M Snell) [#34655](https://github.com/nodejs/node/pull/34655)
* [[`34165f03aa`](https://github.com/nodejs/node/commit/34165f03aa)] - **quic**: resolve some minor TODOs (James M Snell) [#34655](https://github.com/nodejs/node/pull/34655)
* [[`1e6e5c3ef3`](https://github.com/nodejs/node/commit/1e6e5c3ef3)] - **quic**: resolve minor TODO in QuicSocket (James M Snell) [#34655](https://github.com/nodejs/node/pull/34655)
* [[`ba5c64bf45`](https://github.com/nodejs/node/commit/ba5c64bf45)] - **quic**: use AbortController with correct name/message (Anna Henningsen) [#34763](https://github.com/nodejs/node/pull/34763)
* [[`a7477704c4`](https://github.com/nodejs/node/commit/a7477704c4)] - **quic**: prefer modernize-make-unique (gengjiawen) [#34692](https://github.com/nodejs/node/pull/34692)
* [[`5b6cd6fa1a`](https://github.com/nodejs/node/commit/5b6cd6fa1a)] - **quic**: use the SocketAddressLRU to track validation status (James M Snell) [#34618](https://github.com/nodejs/node/pull/34618)
* [[`f75e69a94b`](https://github.com/nodejs/node/commit/f75e69a94b)] - **quic**: use SocketAddressLRU to track known SocketAddress info (James M Snell) [#34618](https://github.com/nodejs/node/pull/34618)
* [[`6b0b33cd4c`](https://github.com/nodejs/node/commit/6b0b33cd4c)] - **quic**: cleanup some outstanding todo items (James M Snell) [#34618](https://github.com/nodejs/node/pull/34618)
* [[`6e65f26b73`](https://github.com/nodejs/node/commit/6e65f26b73)] - **quic**: use QuicCallbackScope consistently for QuicSession (James M Snell) [#34541](https://github.com/nodejs/node/pull/34541)
* [[`d96083bad5`](https://github.com/nodejs/node/commit/d96083bad5)] - **quic**: introduce QuicCallbackScope (James M Snell) [#34541](https://github.com/nodejs/node/pull/34541)
* [[`4b0275ab87`](https://github.com/nodejs/node/commit/4b0275ab87)] - **quic**: refactor clientHello (James M Snell) [#34541](https://github.com/nodejs/node/pull/34541)
* [[`a97b5f9c6a`](https://github.com/nodejs/node/commit/a97b5f9c6a)] - **quic**: use OpenSSL built-in cert and hostname validation (James M Snell) [#34533](https://github.com/nodejs/node/pull/34533)
* [[`7a5fbafe96`](https://github.com/nodejs/node/commit/7a5fbafe96)] - **quic**: fix build for macOS (gengjiawen) [#34336](https://github.com/nodejs/node/pull/34336)
* [[`1f94b89309`](https://github.com/nodejs/node/commit/1f94b89309)] - **quic**: refactor ocsp to use async function rather than event/callback (James M Snell) [#34498](https://github.com/nodejs/node/pull/34498)
* [[`06664298fa`](https://github.com/nodejs/node/commit/06664298fa)] - **quic**: remove no-longer relevant TODO statements (James M Snell) [#34498](https://github.com/nodejs/node/pull/34498)
* [[`2fb92f4cc6`](https://github.com/nodejs/node/commit/2fb92f4cc6)] - **quic**: remove extraneous unused debug property (James M Snell) [#34498](https://github.com/nodejs/node/pull/34498)
* [[`b06fe33de1`](https://github.com/nodejs/node/commit/b06fe33de1)] - **quic**: use async \_construct for QuicStream (James M Snell) [#34351](https://github.com/nodejs/node/pull/34351)
* [[`8bd61d4c38`](https://github.com/nodejs/node/commit/8bd61d4c38)] - **quic**: documentation updates (James M Snell) [#34351](https://github.com/nodejs/node/pull/34351)
* [[`086c916997`](https://github.com/nodejs/node/commit/086c916997)] - **quic**: extensive refactoring of QuicStream lifecycle (James M Snell) [#34351](https://github.com/nodejs/node/pull/34351)
* [[`cf28f8a7dd`](https://github.com/nodejs/node/commit/cf28f8a7dd)] - **quic**: gitignore qlog files (James M Snell) [#34351](https://github.com/nodejs/node/pull/34351)
* [[`83bf0d7e8c`](https://github.com/nodejs/node/commit/83bf0d7e8c)] - **quic**: remove unneeded quicstream.aborted and fixup docs (James M Snell) [#34351](https://github.com/nodejs/node/pull/34351)
* [[`a65296db2c`](https://github.com/nodejs/node/commit/a65296db2c)] - **quic**: remove stream pending code (James M Snell) [#34351](https://github.com/nodejs/node/pull/34351)
* [[`da20287e1a`](https://github.com/nodejs/node/commit/da20287e1a)] - **quic**: simplify QuicStream construction logic (James M Snell) [#34351](https://github.com/nodejs/node/pull/34351)
* [[`6e30fe7a7f`](https://github.com/nodejs/node/commit/6e30fe7a7f)] - **quic**: convert openStream to Promise (James M Snell) [#34351](https://github.com/nodejs/node/pull/34351)
* [[`89453cfc08`](https://github.com/nodejs/node/commit/89453cfc08)] - **quic**: fixup quic.md (James M Snell) [#34283](https://github.com/nodejs/node/pull/34283)
* [[`4523d4a813`](https://github.com/nodejs/node/commit/4523d4a813)] - **quic**: fixup closing/draining period timing (James M Snell) [#34283](https://github.com/nodejs/node/pull/34283)
* [[`ed4882241c`](https://github.com/nodejs/node/commit/ed4882241c)] - **quic**: properly pass readable/writable constructor options (James M Snell) [#34283](https://github.com/nodejs/node/pull/34283)
* [[`57c1129508`](https://github.com/nodejs/node/commit/57c1129508)] - **quic**: implement QuicSession close as promise (James M Snell) [#34283](https://github.com/nodejs/node/pull/34283)
* [[`8e5c5b16ab`](https://github.com/nodejs/node/commit/8e5c5b16ab)] - **quic**: cleanup QuicClientSession constructor (James M Snell) [#34283](https://github.com/nodejs/node/pull/34283)
* [[`fe4e7e4598`](https://github.com/nodejs/node/commit/fe4e7e4598)] - **quic**: use promisified dns lookup (James M Snell) [#34283](https://github.com/nodejs/node/pull/34283)
* [[`346aeaf874`](https://github.com/nodejs/node/commit/346aeaf874)] - **quic**: eliminate "ready"/"not ready" states for QuicSession (James M Snell) [#34283](https://github.com/nodejs/node/pull/34283)
* [[`6665dda9f6`](https://github.com/nodejs/node/commit/6665dda9f6)] - **quic**: implement QuicSocket Promise API, part 2 (James M Snell) [#34283](https://github.com/nodejs/node/pull/34283)
* [[`79c0e892dd`](https://github.com/nodejs/node/commit/79c0e892dd)] - **quic**: implement QuicSocket Promise API, part 1 (James M Snell) [#34283](https://github.com/nodejs/node/pull/34283)
* [[`53b12f0c7b`](https://github.com/nodejs/node/commit/53b12f0c7b)] - **quic**: implement QuicEndpoint Promise API (James M Snell) [#34283](https://github.com/nodejs/node/pull/34283)
* [[`16b32eae3e`](https://github.com/nodejs/node/commit/16b32eae3e)] - **quic**: handle unhandled rejections on QuicSession (James M Snell) [#34283](https://github.com/nodejs/node/pull/34283)
* [[`e5d963e24d`](https://github.com/nodejs/node/commit/e5d963e24d)] - **quic**: fixup kEndpointClose (James M Snell) [#34283](https://github.com/nodejs/node/pull/34283)
* [[`9f552df5b4`](https://github.com/nodejs/node/commit/9f552df5b4)] - **quic**: fix endpointClose error handling, document (James M Snell) [#34283](https://github.com/nodejs/node/pull/34283)
* [[`b80108c033`](https://github.com/nodejs/node/commit/b80108c033)] - **quic**: restrict addEndpoint to before QuicSocket bind (James M Snell) [#34283](https://github.com/nodejs/node/pull/34283)
* [[`81c01bbdba`](https://github.com/nodejs/node/commit/81c01bbdba)] - **quic**: use a getter for stream options (James M Snell) [#34283](https://github.com/nodejs/node/pull/34283)
* [[`b8945ba2ab`](https://github.com/nodejs/node/commit/b8945ba2ab)] - **quic**: clarifying code comments (James M Snell) [#34283](https://github.com/nodejs/node/pull/34283)
* [[`429ab1dce6`](https://github.com/nodejs/node/commit/429ab1dce6)] - **quic**: minor reduction in code duplication (James M Snell) [#34283](https://github.com/nodejs/node/pull/34283)
* [[`aafdc2fcad`](https://github.com/nodejs/node/commit/aafdc2fcad)] - **quic**: replace ipv6Only option with `'udp6-only'` type (James M Snell) [#34283](https://github.com/nodejs/node/pull/34283)
* [[`fbc38ee134`](https://github.com/nodejs/node/commit/fbc38ee134)] - **quic**: clear clang warning (gengjiawen) [#34335](https://github.com/nodejs/node/pull/34335)
* [[`c176d5fac2`](https://github.com/nodejs/node/commit/c176d5fac2)] - **quic**: set destroyed at timestamps for duration calculation (James M Snell) [#34262](https://github.com/nodejs/node/pull/34262)
* [[`48a349efd9`](https://github.com/nodejs/node/commit/48a349efd9)] - **quic**: use Number instead of BigInt for more stats (James M Snell) [#34262](https://github.com/nodejs/node/pull/34262)
* [[`5e769b2eaf`](https://github.com/nodejs/node/commit/5e769b2eaf)] - **quic**: use less specific error codes (James M Snell) [#34262](https://github.com/nodejs/node/pull/34262)
* [[`26493c02a2`](https://github.com/nodejs/node/commit/26493c02a2)] - **quic**: remove no longer valid CHECK (James M Snell) [#34247](https://github.com/nodejs/node/pull/34247)
* [[`458d243f20`](https://github.com/nodejs/node/commit/458d243f20)] - **quic**: proper custom inspect for QuicStream (James M Snell) [#34247](https://github.com/nodejs/node/pull/34247)
* [[`0860b11655`](https://github.com/nodejs/node/commit/0860b11655)] - **quic**: proper custom inspect for QuicSession (James M Snell) [#34247](https://github.com/nodejs/node/pull/34247)
* [[`b047930d76`](https://github.com/nodejs/node/commit/b047930d76)] - **quic**: proper custom inspect for QuicSocket (James M Snell) [#34247](https://github.com/nodejs/node/pull/34247)
* [[`511f8c1138`](https://github.com/nodejs/node/commit/511f8c1138)] - **quic**: proper custom inspect for QuicEndpoint (James M Snell) [#34247](https://github.com/nodejs/node/pull/34247)
* [[`fe11f6bf7c`](https://github.com/nodejs/node/commit/fe11f6bf7c)] - **quic**: cleanup QuicSocketFlags, used shared state struct (James M Snell) [#34247](https://github.com/nodejs/node/pull/34247)
* [[`d08e99de24`](https://github.com/nodejs/node/commit/d08e99de24)] - **quic**: use getter/setter for stateless reset toggle (James M Snell) [#34247](https://github.com/nodejs/node/pull/34247)
* [[`f2753c7695`](https://github.com/nodejs/node/commit/f2753c7695)] - **quic**: unref timers again (Anna Henningsen) [#34247](https://github.com/nodejs/node/pull/34247)
* [[`71236097d0`](https://github.com/nodejs/node/commit/71236097d0)] - **quic**: use Number() instead of bigint for QuicSocket stats (James M Snell) [#34247](https://github.com/nodejs/node/pull/34247)
* [[`94372b124a`](https://github.com/nodejs/node/commit/94372b124a)] - **quic**: refactor/improve/document QuicSocket listening event (James M Snell) [#34247](https://github.com/nodejs/node/pull/34247)
* [[`afc9390ae5`](https://github.com/nodejs/node/commit/afc9390ae5)] - **quic**: refactor/improve QuicSocket ready event handling (James M Snell) [#34247](https://github.com/nodejs/node/pull/34247)
* [[`e3813261b8`](https://github.com/nodejs/node/commit/e3813261b8)] - **quic**: add tests confirming error handling for QuicSocket close event (James M Snell) [#34247](https://github.com/nodejs/node/pull/34247)
* [[`cc89aac5f7`](https://github.com/nodejs/node/commit/cc89aac5f7)] - **quic**: refactor/improve error handling for busy event (James M Snell) [#34247](https://github.com/nodejs/node/pull/34247)
* [[`edc71ef008`](https://github.com/nodejs/node/commit/edc71ef008)] - **quic**: handle errors thrown / rejections in the session event (James M Snell) [#34247](https://github.com/nodejs/node/pull/34247)
* [[`bcde849be9`](https://github.com/nodejs/node/commit/bcde849be9)] - **quic**: remove unnecessary bool conversion (James M Snell) [#34247](https://github.com/nodejs/node/pull/34247)
* [[`c535131627`](https://github.com/nodejs/node/commit/c535131627)] - **quic**: additional minor cleanups in node\_quic\_session.h (James M Snell) [#34247](https://github.com/nodejs/node/pull/34247)
* [[`0f97d6066a`](https://github.com/nodejs/node/commit/0f97d6066a)] - **quic**: use TimerWrap for idle and retransmit timers (James M Snell) [#34186](https://github.com/nodejs/node/pull/34186)
* [[`1b1e985478`](https://github.com/nodejs/node/commit/1b1e985478)] - **quic**: add missing memory tracker fields (James M Snell) [#34160](https://github.com/nodejs/node/pull/34160)
* [[`5a87e9b0a5`](https://github.com/nodejs/node/commit/5a87e9b0a5)] - **quic**: cleanup timers if they haven't been already (James M Snell) [#34160](https://github.com/nodejs/node/pull/34160)
* [[`3837d9cf1f`](https://github.com/nodejs/node/commit/3837d9cf1f)] - **quic**: fixup lint issues (James M Snell) [#34160](https://github.com/nodejs/node/pull/34160)
* [[`7b062ca015`](https://github.com/nodejs/node/commit/7b062ca015)] - **quic**: refactor qlog handling (James M Snell) [#34160](https://github.com/nodejs/node/pull/34160)
* [[`e4d369e96e`](https://github.com/nodejs/node/commit/e4d369e96e)] - **quic**: remove onSessionDestroy callback (James M Snell) [#34160](https://github.com/nodejs/node/pull/34160)
* [[`3acdd6aac7`](https://github.com/nodejs/node/commit/3acdd6aac7)] - **quic**: refactor QuicSession shared state to use AliasedStruct (James M Snell) [#34160](https://github.com/nodejs/node/pull/34160)
* [[`f9c2245fb5`](https://github.com/nodejs/node/commit/f9c2245fb5)] - **quic**: refactor QuicSession close/destroy flow (James M Snell) [#34160](https://github.com/nodejs/node/pull/34160)
* [[`f7510ca439`](https://github.com/nodejs/node/commit/f7510ca439)] - **quic**: additional cleanups on the c++ side (James M Snell) [#34160](https://github.com/nodejs/node/pull/34160)
* [[`b5bf5bb20f`](https://github.com/nodejs/node/commit/b5bf5bb20f)] - **quic**: refactor native object flags for better readability (James M Snell) [#34160](https://github.com/nodejs/node/pull/34160)
* [[`b1750a4d53`](https://github.com/nodejs/node/commit/b1750a4d53)] - **quic**: continued refactoring for quic\_stream/quic\_session (James M Snell) [#34160](https://github.com/nodejs/node/pull/34160)
* [[`31d6d9d0f7`](https://github.com/nodejs/node/commit/31d6d9d0f7)] - **quic**: reduce duplication of code (James M Snell) [#34137](https://github.com/nodejs/node/pull/34137)
* [[`b5fe31ef19`](https://github.com/nodejs/node/commit/b5fe31ef19)] - **quic**: avoid using private JS fields for now (James M Snell) [#34137](https://github.com/nodejs/node/pull/34137)
* [[`2afc1abd05`](https://github.com/nodejs/node/commit/2afc1abd05)] - **quic**: fixup constant exports, export all protocol error codes (James M Snell) [#34137](https://github.com/nodejs/node/pull/34137)
* [[`b1fab88ff0`](https://github.com/nodejs/node/commit/b1fab88ff0)] - **quic**: remove unused callback function (James M Snell) [#34137](https://github.com/nodejs/node/pull/34137)
* [[`3bae2d5073`](https://github.com/nodejs/node/commit/3bae2d5073)] - **quic**: consolidate onSessionClose and onSessionSilentClose (James M Snell) [#34137](https://github.com/nodejs/node/pull/34137)
* [[`def8e76999`](https://github.com/nodejs/node/commit/def8e76999)] - **quic**: fixup set\_final\_size (James M Snell) [#34137](https://github.com/nodejs/node/pull/34137)
* [[`d6034186d6`](https://github.com/nodejs/node/commit/d6034186d6)] - **quic**: cleanups for QuicSocket (James M Snell) [#34137](https://github.com/nodejs/node/pull/34137)
* [[`73a51bb9dc`](https://github.com/nodejs/node/commit/73a51bb9dc)] - **quic**: cleanups in JS API (James M Snell) [#34137](https://github.com/nodejs/node/pull/34137)
* [[`204f20f2d1`](https://github.com/nodejs/node/commit/204f20f2d1)] - **quic**: minor cleanups in quic\_buffer (James M Snell) [#34087](https://github.com/nodejs/node/pull/34087)
* [[`68634d2592`](https://github.com/nodejs/node/commit/68634d2592)] - **quic**: remove redundant cast (gengjiawen) [#34086](https://github.com/nodejs/node/pull/34086)
* [[`213cac0b94`](https://github.com/nodejs/node/commit/213cac0b94)] - **quic**: temporarily skip quic-ipv6only test (James M Snell) [#34033](https://github.com/nodejs/node/pull/34033)
* [[`99f7c4bb5e`](https://github.com/nodejs/node/commit/99f7c4bb5e)] - **quic**: possibly resolve flaky assertion failure in ipv6only test (James M Snell) [#34033](https://github.com/nodejs/node/pull/34033)
* [[`2a5922e483`](https://github.com/nodejs/node/commit/2a5922e483)] - **quic**: temporarily disable packetloss tests (James M Snell) [#34033](https://github.com/nodejs/node/pull/34033)
* [[`86e67aaa69`](https://github.com/nodejs/node/commit/86e67aaa69)] - **quic**: updates to implement for h3-29 (James M Snell) [#34033](https://github.com/nodejs/node/pull/34033)
* [[`adf14e2617`](https://github.com/nodejs/node/commit/adf14e2617)] - **quic**: fix lint error in node\_quic\_crypto (Daniel Bevenius) [#34019](https://github.com/nodejs/node/pull/34019)
* [[`9f2e00fb99`](https://github.com/nodejs/node/commit/9f2e00fb99)] - **quic**: temporarily disable preferred address tests (James M Snell) [#33934](https://github.com/nodejs/node/pull/33934)
* [[`0e7c8bdc0c`](https://github.com/nodejs/node/commit/0e7c8bdc0c)] - **quic**: return 0 from SSL\_CTX\_sess\_set\_new\_cb callback (Anna Henningsen) [#33931](https://github.com/nodejs/node/pull/33931)
* [[`c7d859e756`](https://github.com/nodejs/node/commit/c7d859e756)] - **quic**: refactor and improve ipv6Only (James M Snell) [#33935](https://github.com/nodejs/node/pull/33935)
* [[`1b7434dfc0`](https://github.com/nodejs/node/commit/1b7434dfc0)] - **quic**: set up FunctionTemplates more cleanly (Anna Henningsen) [#33968](https://github.com/nodejs/node/pull/33968)
* [[`8ef86a920c`](https://github.com/nodejs/node/commit/8ef86a920c)] - **quic**: fix clang warning (gengjiawen) [#33963](https://github.com/nodejs/node/pull/33963)
* [[`013cd1ac6f`](https://github.com/nodejs/node/commit/013cd1ac6f)] - **quic**: use Check instead of FromJust in node\_quic.cc (Daniel Bevenius) [#33937](https://github.com/nodejs/node/pull/33937)
* [[`09330fc155`](https://github.com/nodejs/node/commit/09330fc155)] - **quic**: fix clang-tidy performance-faster-string-find issue (gengjiawen) [#33975](https://github.com/nodejs/node/pull/33975)
* [[`9743624c0b`](https://github.com/nodejs/node/commit/9743624c0b)] - **quic**: fix typo in comments (gengjiawen) [#33975](https://github.com/nodejs/node/pull/33975)
* [[`88ef15812c`](https://github.com/nodejs/node/commit/88ef15812c)] - **quic**: remove unused string include http3\_application (Daniel Bevenius) [#33926](https://github.com/nodejs/node/pull/33926)
* [[`1bd88a3ac6`](https://github.com/nodejs/node/commit/1bd88a3ac6)] - **quic**: fix up node\_quic\_stream includes (Daniel Bevenius) [#33921](https://github.com/nodejs/node/pull/33921)
* [[`d7d79f2163`](https://github.com/nodejs/node/commit/d7d79f2163)] - **quic**: avoid memory fragmentation issue (James M Snell) [#33912](https://github.com/nodejs/node/pull/33912)
* [[`16116f5f5f`](https://github.com/nodejs/node/commit/16116f5f5f)] - **quic**: remove noop code (Robert Nagy) [#33914](https://github.com/nodejs/node/pull/33914)
* [[`272b46e04d`](https://github.com/nodejs/node/commit/272b46e04d)] - **quic**: skip test-quic-preferred-address-ipv6.js when no ipv6 (James M Snell) [#33919](https://github.com/nodejs/node/pull/33919)
* [[`4b70f95d64`](https://github.com/nodejs/node/commit/4b70f95d64)] - **quic**: use Check instead of FromJust in QuicStream (Daniel Bevenius) [#33909](https://github.com/nodejs/node/pull/33909)
* [[`133a97f60d`](https://github.com/nodejs/node/commit/133a97f60d)] - **quic**: always copy stateless reset token (Anna Henningsen) [#33917](https://github.com/nodejs/node/pull/33917)
* [[`14d012ef96`](https://github.com/nodejs/node/commit/14d012ef96)] - **quic**: fix minor linting issue (James M Snell) [#33913](https://github.com/nodejs/node/pull/33913)
* [[`55360443ce`](https://github.com/nodejs/node/commit/55360443ce)] - **quic**: initial QUIC implementation (James M Snell) [#32379](https://github.com/nodejs/node/pull/32379)
* [[`a12a2d892f`](https://github.com/nodejs/node/commit/a12a2d892f)] - **repl**: update deprecation codes (Antoine du HAMEL) [#33430](https://github.com/nodejs/node/pull/33430)
* [[`2b3acc44f0`](https://github.com/nodejs/node/commit/2b3acc44f0)] - **src**: large pages support in illumos/solaris systems (David Carlier) [#34320](https://github.com/nodejs/node/pull/34320)
* [[`84a7880749`](https://github.com/nodejs/node/commit/84a7880749)] - **src**: minor cleanup and simplification of crypto::Hash (James M Snell) [#35651](https://github.com/nodejs/node/pull/35651)
* [[`bfc906906f`](https://github.com/nodejs/node/commit/bfc906906f)] - **src**: combine TLSWrap/SSLWrap (James M Snell) [#35552](https://github.com/nodejs/node/pull/35552)
* [[`9fd6122659`](https://github.com/nodejs/node/commit/9fd6122659)] - **src**: add embedding helpers to reduce boilerplate code (Anna Henningsen) [#35597](https://github.com/nodejs/node/pull/35597)
* [[`f7ed5f4ae3`](https://github.com/nodejs/node/commit/f7ed5f4ae3)] - **src**: remove toLocalChecked in crypto\_context (James M Snell) [#35509](https://github.com/nodejs/node/pull/35509)
* [[`17d5d94921`](https://github.com/nodejs/node/commit/17d5d94921)] - **src**: replace more toLocalCheckeds in crypto\_\* (James M Snell) [#35509](https://github.com/nodejs/node/pull/35509)
* [[`83eaaf9731`](https://github.com/nodejs/node/commit/83eaaf9731)] - **src**: remove unused AsyncWrapObject (James M Snell) [#35511](https://github.com/nodejs/node/pull/35511)
* [[`ee5f849fda`](https://github.com/nodejs/node/commit/ee5f849fda)] - **src**: fix compiler warning in env.cc (Anna Henningsen) [#35547](https://github.com/nodejs/node/pull/35547)
* [[`40364b181d`](https://github.com/nodejs/node/commit/40364b181d)] - **src**: add check against non-weak BaseObjects at process exit (Anna Henningsen) [#35490](https://github.com/nodejs/node/pull/35490)
* [[`bc0c094b74`](https://github.com/nodejs/node/commit/bc0c094b74)] - **src**: unset NODE\_VERSION\_IS\_RELEASE from master (Antoine du Hamel) [#35531](https://github.com/nodejs/node/pull/35531)
* [[`fdf0a84e82`](https://github.com/nodejs/node/commit/fdf0a84e82)] - **src**: move all base64.h inline methods into -inl.h header file (Anna Henningsen) [#35432](https://github.com/nodejs/node/pull/35432)
* [[`ff4cf817a3`](https://github.com/nodejs/node/commit/ff4cf817a3)] - **src**: create helper for reading Uint32BE (Juan José Arboleda) [#34944](https://github.com/nodejs/node/pull/34944)
* [[`c6e1edcc28`](https://github.com/nodejs/node/commit/c6e1edcc28)] - **src**: add Update(const sockaddr\*) variant (James M Snell) [#34752](https://github.com/nodejs/node/pull/34752)
* [[`1c14810edc`](https://github.com/nodejs/node/commit/1c14810edc)] - **src**: allow instances of net.BlockList to be created internally (James M Snell) [#34741](https://github.com/nodejs/node/pull/34741)
* [[`6d1f0aed52`](https://github.com/nodejs/node/commit/6d1f0aed52)] - **src**: add SocketAddressLRU Utility (James M Snell) [#34618](https://github.com/nodejs/node/pull/34618)
* [[`feb93c4e84`](https://github.com/nodejs/node/commit/feb93c4e84)] - **src**: guard against nullptr deref in TimerWrapHandle::Stop (Anna Henningsen) [#34460](https://github.com/nodejs/node/pull/34460)
* [[`7a447bcd54`](https://github.com/nodejs/node/commit/7a447bcd54)] - **src**: snapshot node (Joyee Cheung) [#32984](https://github.com/nodejs/node/pull/32984)
* [[`c943cb4809`](https://github.com/nodejs/node/commit/c943cb4809)] - **src**: reset zero fill toggle at pre-execution (Joyee Cheung) [#32984](https://github.com/nodejs/node/pull/32984)
* [[`0b8ae5f2cd`](https://github.com/nodejs/node/commit/0b8ae5f2cd)] - **src**: snapshot loaders (Joyee Cheung) [#32984](https://github.com/nodejs/node/pull/32984)
* [[`7ecb285842`](https://github.com/nodejs/node/commit/7ecb285842)] - **src**: make code cache test work with snapshots (Joyee Cheung) [#32984](https://github.com/nodejs/node/pull/32984)
* [[`1faf6f459f`](https://github.com/nodejs/node/commit/1faf6f459f)] - **src**: snapshot Environment upon instantiation (Joyee Cheung) [#32984](https://github.com/nodejs/node/pull/32984)
* [[`ef9964f4c1`](https://github.com/nodejs/node/commit/ef9964f4c1)] - **src**: add an ExternalReferenceRegistry class (Joyee Cheung) [#32984](https://github.com/nodejs/node/pull/32984)
* [[`404302fff5`](https://github.com/nodejs/node/commit/404302fff5)] - **src**: split the main context initialization from Environment ctor (Joyee Cheung) [#32984](https://github.com/nodejs/node/pull/32984)
* [[`874460a1d1`](https://github.com/nodejs/node/commit/874460a1d1)] - **src**: refactor TimerWrap lifetime management (Anna Henningsen) [#34252](https://github.com/nodejs/node/pull/34252)
* [[`e2f9dc6e5a`](https://github.com/nodejs/node/commit/e2f9dc6e5a)] - **src**: remove user\_data from TimerWrap (Anna Henningsen) [#34252](https://github.com/nodejs/node/pull/34252)
* [[`e19a251824`](https://github.com/nodejs/node/commit/e19a251824)] - **src**: replace InspectorTimer with TimerWrap utility (James M Snell) [#34186](https://github.com/nodejs/node/pull/34186)
* [[`d4f69002b4`](https://github.com/nodejs/node/commit/d4f69002b4)] - **src**: add TimerWrap utility (James M Snell) [#34186](https://github.com/nodejs/node/pull/34186)
* [[`52de4cb107`](https://github.com/nodejs/node/commit/52de4cb107)] - **src**: minor updates to FastHrtime (Anna Henningsen) [#33851](https://github.com/nodejs/node/pull/33851)
* [[`4678e44bb2`](https://github.com/nodejs/node/commit/4678e44bb2)] - **src**: perform bounds checking on error source line (Anna Henningsen) [#33645](https://github.com/nodejs/node/pull/33645)
* [[`7232c2a160`](https://github.com/nodejs/node/commit/7232c2a160)] - **src**: use getauxval in node\_main.cc (Daniel Bevenius) [#33693](https://github.com/nodejs/node/pull/33693)
* [[`6be80e1893`](https://github.com/nodejs/node/commit/6be80e1893)] - **stream**: fix legacy pipe error handling (Robert Nagy) [#35257](https://github.com/nodejs/node/pull/35257)
* [[`2b9003b165`](https://github.com/nodejs/node/commit/2b9003b165)] - **stream**: don't destroy on async iterator success (Robert Nagy) [#35122](https://github.com/nodejs/node/pull/35122)
* [[`9c62e0e384`](https://github.com/nodejs/node/commit/9c62e0e384)] - **stream**: move to internal/streams (Matteo Collina) [#35239](https://github.com/nodejs/node/pull/35239)
* [[`e0d3b758a0`](https://github.com/nodejs/node/commit/e0d3b758a0)] - **stream**: improve Writable.destroy performance (Robert Nagy) [#35067](https://github.com/nodejs/node/pull/35067)
* [[`02c4869bee`](https://github.com/nodejs/node/commit/02c4869bee)] - **stream**: fix Duplex.\_construct race (Robert Nagy) [#34456](https://github.com/nodejs/node/pull/34456)
* [[`5aeaff6499`](https://github.com/nodejs/node/commit/5aeaff6499)] - **stream**: refactor lazyLoadPromises (rickyes) [#34354](https://github.com/nodejs/node/pull/34354)
* [[`a55b77d2d3`](https://github.com/nodejs/node/commit/a55b77d2d3)] - **stream**: finished on closed OutgoingMessage (Robert Nagy) [#34313](https://github.com/nodejs/node/pull/34313)
* [[`e10e292c5e`](https://github.com/nodejs/node/commit/e10e292c5e)] - **stream**: remove unused \_transformState (Robert Nagy) [#33105](https://github.com/nodejs/node/pull/33105)
* [[`f5c11a1a0a`](https://github.com/nodejs/node/commit/f5c11a1a0a)] - **stream**: don't emit finish after close (Robert Nagy) [#32933](https://github.com/nodejs/node/pull/32933)
* [[`089d654dd8`](https://github.com/nodejs/node/commit/089d654dd8)] - **test**: fix addons/dlopen-ping-pong for npm 7.0.1 (Myles Borins) [#35667](https://github.com/nodejs/node/pull/35667)
* [[`9ce5a03148`](https://github.com/nodejs/node/commit/9ce5a03148)] - **test**: add test for listen callback runtime binding (H Adinarayana) [#35657](https://github.com/nodejs/node/pull/35657)
* [[`a3731309cc`](https://github.com/nodejs/node/commit/a3731309cc)] - **test**: refactor test-https-host-headers (himself65) [#32805](https://github.com/nodejs/node/pull/32805)
* [[`30fb4a015d`](https://github.com/nodejs/node/commit/30fb4a015d)] - **test**: add common.mustSucceed (Tobias Nießen) [#35086](https://github.com/nodejs/node/pull/35086)
* [[`c143266b55`](https://github.com/nodejs/node/commit/c143266b55)] - **test**: add a few uncovered url tests from wpt (Daijiro Wachi) [#35636](https://github.com/nodejs/node/pull/35636)
* [[`6751b6dc3d`](https://github.com/nodejs/node/commit/6751b6dc3d)] - **test**: check for AbortController existence (James M Snell) [#35616](https://github.com/nodejs/node/pull/35616)
* [[`9f2e19fa30`](https://github.com/nodejs/node/commit/9f2e19fa30)] - **test**: update url test for win (Daijiro Wachi) [#35622](https://github.com/nodejs/node/pull/35622)
* [[`c88d845db3`](https://github.com/nodejs/node/commit/c88d845db3)] - **test**: update wpt status for url (Daijiro Wachi) [#35335](https://github.com/nodejs/node/pull/35335)
* [[`589dbf1392`](https://github.com/nodejs/node/commit/589dbf1392)] - **test**: update wpt tests for url (Daijiro Wachi) [#35329](https://github.com/nodejs/node/pull/35329)
* [[`46bef7b771`](https://github.com/nodejs/node/commit/46bef7b771)] - **test**: add Actions annotation output (Mary Marchini) [#34590](https://github.com/nodejs/node/pull/34590)
* [[`a9c5b873ca`](https://github.com/nodejs/node/commit/a9c5b873ca)] - **test**: move buffer-as-path symlink test to its own test file (Rich Trott) [#34569](https://github.com/nodejs/node/pull/34569)
* [[`31ba9a20bd`](https://github.com/nodejs/node/commit/31ba9a20bd)] - **test**: run test-benchmark-napi on arm (Rich Trott) [#34502](https://github.com/nodejs/node/pull/34502)
* [[`2c4ebe0426`](https://github.com/nodejs/node/commit/2c4ebe0426)] - **test**: use `.then(common.mustCall())` for all async IIFEs (Anna Henningsen) [#34363](https://github.com/nodejs/node/pull/34363)
* [[`772fdb0cd3`](https://github.com/nodejs/node/commit/772fdb0cd3)] - **test**: fix flaky test-fs-stream-construct (Rich Trott) [#34203](https://github.com/nodejs/node/pull/34203)
* [[`9b8d317d99`](https://github.com/nodejs/node/commit/9b8d317d99)] - **test**: fix flaky test-http2-invalidheaderfield (Rich Trott) [#34173](https://github.com/nodejs/node/pull/34173)
* [[`2ccf15b2bf`](https://github.com/nodejs/node/commit/2ccf15b2bf)] - **test**: ensure finish is emitted before destroy (Robert Nagy) [#33137](https://github.com/nodejs/node/pull/33137)
* [[`27f3530da3`](https://github.com/nodejs/node/commit/27f3530da3)] - **test**: remove unnecessary eslint-disable comment (Rich Trott) [#34000](https://github.com/nodejs/node/pull/34000)
* [[`326a79ebb9`](https://github.com/nodejs/node/commit/326a79ebb9)] - **test**: fix typo in test-quic-client-empty-preferred-address.js (gengjiawen) [#33976](https://github.com/nodejs/node/pull/33976)
* [[`b0b268f5a2`](https://github.com/nodejs/node/commit/b0b268f5a2)] - **test**: fix flaky fs-construct test (Robert Nagy) [#33625](https://github.com/nodejs/node/pull/33625)
* [[`cbe955c227`](https://github.com/nodejs/node/commit/cbe955c227)] - **test**: add net regression test (Robert Nagy) [#32794](https://github.com/nodejs/node/pull/32794)
* [[`5d179cb2ec`](https://github.com/nodejs/node/commit/5d179cb2ec)] - **timers**: use AbortController with correct name/message (Anna Henningsen) [#34763](https://github.com/nodejs/node/pull/34763)
* [[`64d22c320c`](https://github.com/nodejs/node/commit/64d22c320c)] - **timers**: fix multipleResolves in promisified timeouts/immediates (Denys Otrishko) [#33949](https://github.com/nodejs/node/pull/33949)
* [[`fbe33aa52e`](https://github.com/nodejs/node/commit/fbe33aa52e)] - **tools**: bump remark-lint-preset-node to 1.17.1 (Rich Trott) [#35668](https://github.com/nodejs/node/pull/35668)
* [[`35a6946193`](https://github.com/nodejs/node/commit/35a6946193)] - **tools**: update gyp-next to v0.6.2 (Michaël Zasso) [#35690](https://github.com/nodejs/node/pull/35690)
* [[`be80faa0c8`](https://github.com/nodejs/node/commit/be80faa0c8)] - **tools**: update gyp-next to v0.6.0 (Ujjwal Sharma) [#35635](https://github.com/nodejs/node/pull/35635)
* [[`2d83e743d9`](https://github.com/nodejs/node/commit/2d83e743d9)] - **tools**: update ESLint to 7.11.0 (Colin Ihrig) [#35578](https://github.com/nodejs/node/pull/35578)
* [[`0eca660948`](https://github.com/nodejs/node/commit/0eca660948)] - **tools**: update ESLint to 7.7.0 (Colin Ihrig) [#34783](https://github.com/nodejs/node/pull/34783)
* [[`77b68f9a29`](https://github.com/nodejs/node/commit/77b68f9a29)] - **tools**: add linting rule for async IIFEs (Anna Henningsen) [#34363](https://github.com/nodejs/node/pull/34363)
* [[`f04538761f`](https://github.com/nodejs/node/commit/f04538761f)] - **tools**: enable Node.js command line flags in node\_mksnapshot (Joyee Cheung) [#32984](https://github.com/nodejs/node/pull/32984)
* [[`b0d4eb37c7`](https://github.com/nodejs/node/commit/b0d4eb37c7)] - **tools**: update ESLint to 7.4.0 (Colin Ihrig) [#34205](https://github.com/nodejs/node/pull/34205)
* [[`076e4ed2d1`](https://github.com/nodejs/node/commit/076e4ed2d1)] - **tools**: update ESLint from 7.2.0 to 7.3.1 (Rich Trott) [#34000](https://github.com/nodejs/node/pull/34000)
* [[`7afe3af200`](https://github.com/nodejs/node/commit/7afe3af200)] - **url**: fix file url reparse (Daijiro Wachi) [#35671](https://github.com/nodejs/node/pull/35671)
