# Node.js 20 ChangeLog

<!--lint disable maximum-line-length no-literal-urls prohibited-strings-->

<table>
<tr>
<th>Current</th>
</tr>
<tr>
<td>
<a href="#20.5.0">20.5.0</a><br/>
<a href="#20.4.0">20.4.0</a><br/>
<a href="#20.3.1">20.3.1</a><br/>
<a href="#20.3.0">20.3.0</a><br/>
<a href="#20.2.0">20.2.0</a><br/>
<a href="#20.1.0">20.1.0</a><br/>
<a href="#20.0.0">20.0.0</a><br/>
</td>
</tr>
</table>

* Other Versions
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

<a id="20.5.0"></a>

## 2023-07-18, Version 20.5.0 (Current), @juanarbol

### Notable Changes

* \[[`45be29d89f`](https://github.com/nodejs/node/commit/45be29d89f)] - **doc**: add atlowChemi to collaborators (atlowChemi) [#48757](https://github.com/nodejs/node/pull/48757)
* \[[`a316808136`](https://github.com/nodejs/node/commit/a316808136)] - **(SEMVER-MINOR)** **events**: allow safely adding listener to abortSignal (Chemi Atlow) [#48596](https://github.com/nodejs/node/pull/48596)
* \[[`986b46a567`](https://github.com/nodejs/node/commit/986b46a567)] - **fs**: add a fast-path for readFileSync utf-8 (Yagiz Nizipli) [#48658](https://github.com/nodejs/node/pull/48658)
* \[[`0ef73ff6f0`](https://github.com/nodejs/node/commit/0ef73ff6f0)] - **(SEMVER-MINOR)** **test\_runner**: add shards support (Raz Luvaton) [#48639](https://github.com/nodejs/node/pull/48639)

### Commits

* \[[`eb0aba59b8`](https://github.com/nodejs/node/commit/eb0aba59b8)] - **bootstrap**: use correct descriptor for Symbol.{dispose,asyncDispose} (Jordan Harband) [#48703](https://github.com/nodejs/node/pull/48703)
* \[[`e2d0195dcf`](https://github.com/nodejs/node/commit/e2d0195dcf)] - **bootstrap**: hide experimental web globals with flag kNoBrowserGlobals (Chengzhong Wu) [#48545](https://github.com/nodejs/node/pull/48545)
* \[[`67a1018389`](https://github.com/nodejs/node/commit/67a1018389)] - **build**: do not pass target toolchain flags to host toolchain (Ivan Trubach) [#48597](https://github.com/nodejs/node/pull/48597)
* \[[`7d843bb942`](https://github.com/nodejs/node/commit/7d843bb942)] - **child\_process**: use addAbortListener (atlowChemi) [#48550](https://github.com/nodejs/node/pull/48550)
* \[[`4e08160f8c`](https://github.com/nodejs/node/commit/4e08160f8c)] - **child\_process**: support `Symbol.dispose` (Moshe Atlow) [#48551](https://github.com/nodejs/node/pull/48551)
* \[[`ef7728bf36`](https://github.com/nodejs/node/commit/ef7728bf36)] - **deps**: update nghttp2 to 1.55.1 (Node.js GitHub Bot) [#48790](https://github.com/nodejs/node/pull/48790)
* \[[`1454f02499`](https://github.com/nodejs/node/commit/1454f02499)] - **deps**: update nghttp2 to 1.55.0 (Node.js GitHub Bot) [#48746](https://github.com/nodejs/node/pull/48746)
* \[[`fa94debf46`](https://github.com/nodejs/node/commit/fa94debf46)] - **deps**: update minimatch to 9.0.3 (Node.js GitHub Bot) [#48704](https://github.com/nodejs/node/pull/48704)
* \[[`c73cfcc144`](https://github.com/nodejs/node/commit/c73cfcc144)] - **deps**: update acorn to 8.10.0 (Node.js GitHub Bot) [#48713](https://github.com/nodejs/node/pull/48713)
* \[[`b7a076a052`](https://github.com/nodejs/node/commit/b7a076a052)] - **deps**: V8: cherry-pick cb00db4dba6c (Keyhan Vakil) [#48671](https://github.com/nodejs/node/pull/48671)
* \[[`150e15536b`](https://github.com/nodejs/node/commit/150e15536b)] - **deps**: upgrade npm to 9.8.0 (npm team) [#48665](https://github.com/nodejs/node/pull/48665)
* \[[`c47b2cbd35`](https://github.com/nodejs/node/commit/c47b2cbd35)] - **dgram**: socket add `asyncDispose` (atlowChemi) [#48717](https://github.com/nodejs/node/pull/48717)
* \[[`002ce31cca`](https://github.com/nodejs/node/commit/002ce31cca)] - **dgram**: use addAbortListener (atlowChemi) [#48550](https://github.com/nodejs/node/pull/48550)
* \[[`45be29d89f`](https://github.com/nodejs/node/commit/45be29d89f)] - **doc**: add atlowChemi to collaborators (atlowChemi) [#48757](https://github.com/nodejs/node/pull/48757)
* \[[`69b55d2261`](https://github.com/nodejs/node/commit/69b55d2261)] - **doc**: fix ambiguity in http.md and https.md (an5er) [#48692](https://github.com/nodejs/node/pull/48692)
* \[[`caccb051c7`](https://github.com/nodejs/node/commit/caccb051c7)] - **doc**: clarify transform.\_transform() callback argument logic (Rafael Sofi-zada) [#48680](https://github.com/nodejs/node/pull/48680)
* \[[`999ae0c8c3`](https://github.com/nodejs/node/commit/999ae0c8c3)] - **doc**: fix copy node executable in Windows (Yoav Vainrich) [#48624](https://github.com/nodejs/node/pull/48624)
* \[[`7daefaeb44`](https://github.com/nodejs/node/commit/7daefaeb44)] - **doc**: drop \<b> of v20 changelog (Rafael Gonzaga) [#48649](https://github.com/nodejs/node/pull/48649)
* \[[`dd7ea3e1df`](https://github.com/nodejs/node/commit/dd7ea3e1df)] - **doc**: mention git node release prepare (Rafael Gonzaga) [#48644](https://github.com/nodejs/node/pull/48644)
* \[[`cc7809df21`](https://github.com/nodejs/node/commit/cc7809df21)] - **esm**: fix emit deprecation on legacy main resolve (Antoine du Hamel) [#48664](https://github.com/nodejs/node/pull/48664)
* \[[`67b13d1dba`](https://github.com/nodejs/node/commit/67b13d1dba)] - **events**: fix bug listenerCount don't compare wrapped listener (yuzheng14) [#48592](https://github.com/nodejs/node/pull/48592)
* \[[`a316808136`](https://github.com/nodejs/node/commit/a316808136)] - **(SEMVER-MINOR)** **events**: allow safely adding listener to abortSignal (Chemi Atlow) [#48596](https://github.com/nodejs/node/pull/48596)
* \[[`986b46a567`](https://github.com/nodejs/node/commit/986b46a567)] - **fs**: add a fast-path for readFileSync utf-8 (Yagiz Nizipli) [#48658](https://github.com/nodejs/node/pull/48658)
* \[[`e4333ac41f`](https://github.com/nodejs/node/commit/e4333ac41f)] - **http2**: use addAbortListener (atlowChemi) [#48550](https://github.com/nodejs/node/pull/48550)
* \[[`4a0b66e4f9`](https://github.com/nodejs/node/commit/4a0b66e4f9)] - **http2**: send RST code 8 on AbortController signal (Devraj Mehta) [#48573](https://github.com/nodejs/node/pull/48573)
* \[[`1295c76fce`](https://github.com/nodejs/node/commit/1295c76fce)] - **lib**: use addAbortListener (atlowChemi) [#48550](https://github.com/nodejs/node/pull/48550)
* \[[`dff6c25a36`](https://github.com/nodejs/node/commit/dff6c25a36)] - **meta**: bump actions/checkout from 3.5.2 to 3.5.3 (dependabot\[bot]) [#48625](https://github.com/nodejs/node/pull/48625)
* \[[`b5cb69ceaa`](https://github.com/nodejs/node/commit/b5cb69ceaa)] - **meta**: bump step-security/harden-runner from 2.4.0 to 2.4.1 (dependabot\[bot]) [#48626](https://github.com/nodejs/node/pull/48626)
* \[[`332e480b46`](https://github.com/nodejs/node/commit/332e480b46)] - **meta**: bump ossf/scorecard-action from 2.1.3 to 2.2.0 (dependabot\[bot]) [#48628](https://github.com/nodejs/node/pull/48628)
* \[[`25c5a0aaee`](https://github.com/nodejs/node/commit/25c5a0aaee)] - **meta**: bump github/codeql-action from 2.3.6 to 2.20.1 (dependabot\[bot]) [#48627](https://github.com/nodejs/node/pull/48627)
* \[[`6406f50ab1`](https://github.com/nodejs/node/commit/6406f50ab1)] - **module**: add SourceMap.lineLengths (Isaac Z. Schlueter) [#48461](https://github.com/nodejs/node/pull/48461)
* \[[`cfa69bd48c`](https://github.com/nodejs/node/commit/cfa69bd48c)] - **net**: server add `asyncDispose` (atlowChemi) [#48717](https://github.com/nodejs/node/pull/48717)
* \[[`ac11264cc5`](https://github.com/nodejs/node/commit/ac11264cc5)] - **net**: use addAbortListener (atlowChemi) [#48550](https://github.com/nodejs/node/pull/48550)
* \[[`82d6b13bf6`](https://github.com/nodejs/node/commit/82d6b13bf6)] - **permission**: add debug log when inserting fs nodes (Rafael Gonzaga) [#48677](https://github.com/nodejs/node/pull/48677)
* \[[`f4333b1cdd`](https://github.com/nodejs/node/commit/f4333b1cdd)] - **permission**: v8.writeHeapSnapshot and process.report (Rafael Gonzaga) [#48564](https://github.com/nodejs/node/pull/48564)
* \[[`f691dca6c9`](https://github.com/nodejs/node/commit/f691dca6c9)] - **readline**: use addAbortListener (atlowChemi) [#48550](https://github.com/nodejs/node/pull/48550)
* \[[`227e6bd898`](https://github.com/nodejs/node/commit/227e6bd898)] - **src**: pass syscall on `fs.readFileSync` fail operation (Yagiz Nizipli) [#48815](https://github.com/nodejs/node/pull/48815)
* \[[`a9a4b73653`](https://github.com/nodejs/node/commit/a9a4b73653)] - **src**: make BaseObject iteration order deterministic (Joyee Cheung) [#48702](https://github.com/nodejs/node/pull/48702)
* \[[`d99ea4845a`](https://github.com/nodejs/node/commit/d99ea4845a)] - **src**: remove kEagerCompile for CompileFunction (Keyhan Vakil) [#48671](https://github.com/nodejs/node/pull/48671)
* \[[`df363d0010`](https://github.com/nodejs/node/commit/df363d0010)] - **src**: deduplicate X509 getter implementations (Tobias Nießen) [#48563](https://github.com/nodejs/node/pull/48563)
* \[[`9cf2e1f55b`](https://github.com/nodejs/node/commit/9cf2e1f55b)] - **src,lib**: reducing C++ calls of esm legacy main resolve (Vinicius Lourenço) [#48325](https://github.com/nodejs/node/pull/48325)
* \[[`daeb21dde9`](https://github.com/nodejs/node/commit/daeb21dde9)] - **stream**: fix deadlock when pipeing to full sink (Robert Nagy) [#48691](https://github.com/nodejs/node/pull/48691)
* \[[`5a382d02d6`](https://github.com/nodejs/node/commit/5a382d02d6)] - **stream**: use addAbortListener (atlowChemi) [#48550](https://github.com/nodejs/node/pull/48550)
* \[[`6e82077dd4`](https://github.com/nodejs/node/commit/6e82077dd4)] - **test**: deflake test-net-throttle (Luigi Pinca) [#48599](https://github.com/nodejs/node/pull/48599)
* \[[`d378b2c822`](https://github.com/nodejs/node/commit/d378b2c822)] - **test**: move test-net-throttle to parallel (Luigi Pinca) [#48599](https://github.com/nodejs/node/pull/48599)
* \[[`dfa0aee5bf`](https://github.com/nodejs/node/commit/dfa0aee5bf)] - _**Revert**_ "**test**: remove test-crypto-keygen flaky designation" (Luigi Pinca) [#48652](https://github.com/nodejs/node/pull/48652)
* \[[`0ef73ff6f0`](https://github.com/nodejs/node/commit/0ef73ff6f0)] - **(SEMVER-MINOR)** **test\_runner**: add shards support (Raz Luvaton) [#48639](https://github.com/nodejs/node/pull/48639)
* \[[`e2442bb7ef`](https://github.com/nodejs/node/commit/e2442bb7ef)] - **timers**: support Symbol.dispose (Moshe Atlow) [#48633](https://github.com/nodejs/node/pull/48633)
* \[[`4398ade426`](https://github.com/nodejs/node/commit/4398ade426)] - **tools**: run fetch\_deps.py with Python 3 (Richard Lau) [#48729](https://github.com/nodejs/node/pull/48729)
* \[[`38ce95d054`](https://github.com/nodejs/node/commit/38ce95d054)] - **tools**: update doc to unist-util-select\@5.0.0 unist-util-visit\@5.0.0 (Node.js GitHub Bot) [#48714](https://github.com/nodejs/node/pull/48714)
* \[[`b25e78a998`](https://github.com/nodejs/node/commit/b25e78a998)] - **tools**: update lint-md-dependencies to rollup\@3.26.2 (Node.js GitHub Bot) [#48705](https://github.com/nodejs/node/pull/48705)
* \[[`a1f4ff7c59`](https://github.com/nodejs/node/commit/a1f4ff7c59)] - **tools**: update eslint to 8.44.0 (Node.js GitHub Bot) [#48632](https://github.com/nodejs/node/pull/48632)
* \[[`42dc6eb698`](https://github.com/nodejs/node/commit/42dc6eb698)] - **tools**: update lint-md-dependencies to rollup\@3.26.0 (Node.js GitHub Bot) [#48631](https://github.com/nodejs/node/pull/48631)
* \[[`07bfcc45ab`](https://github.com/nodejs/node/commit/07bfcc45ab)] - **url**: fix `canParse` false value when v8 optimizes (Yagiz Nizipli) [#48817](https://github.com/nodejs/node/pull/48817)

<a id="20.4.0"></a>

## 2023-07-05, Version 20.4.0 (Current), @RafaelGSS

### Notable Changes

#### Mock Timers

The new feature allows developers to write more reliable and predictable tests for time-dependent functionality.
It includes `MockTimers` with the ability to mock `setTimeout`, `setInterval` from `globals`, `node:timers`, and `node:timers/promises`.

The feature provides a simple API to advance time, enable specific timers, and release all timers.

```mjs
import assert from 'node:assert';
import { test } from 'node:test';

test('mocks setTimeout to be executed synchronously without having to actually wait for it', (context) => {
  const fn = context.mock.fn();
  // Optionally choose what to mock
  context.mock.timers.enable(['setTimeout']);
  const nineSecs = 9000;
  setTimeout(fn, nineSecs);

  const threeSeconds = 3000;
  context.mock.timers.tick(threeSeconds);
  context.mock.timers.tick(threeSeconds);
  context.mock.timers.tick(threeSeconds);

  assert.strictEqual(fn.mock.callCount(), 1);
});
```

This feature was contributed by Erick Wendel in [#47775](https://github.com/nodejs/node/pull/47775).

#### Support to the explicit resource management proposal

Node is adding support to the [explicit resource management](https://github.com/tc39/proposal-explicit-resource-management)
proposal to its resources allowing users of TypeScript/babel to use `using`/`await using` with
V8 support for everyone else on the way.

This feature was contributed by Moshe Atlow and Benjamin Gruenbaum in [#48518](https://github.com/nodejs/node/pull/48518).

#### Other notable changes

* \[[`fe333d2584`](https://github.com/nodejs/node/commit/fe333d2584)] - **crypto**: update root certificates to NSS 3.90 (Node.js GitHub Bot) [#48416](https://github.com/nodejs/node/pull/48416)
* \[[`60c2ea4e79`](https://github.com/nodejs/node/commit/60c2ea4e79)] - **doc**: add vmoroz to collaborators (Vladimir Morozov) [#48527](https://github.com/nodejs/node/pull/48527)
* \[[`5cacdf9e6b`](https://github.com/nodejs/node/commit/5cacdf9e6b)] - **doc**: add kvakil to collaborators (Keyhan Vakil) [#48449](https://github.com/nodejs/node/pull/48449)
* \[[`504d1d7bdc`](https://github.com/nodejs/node/commit/504d1d7bdc)] - **(SEMVER-MINOR)** **tls**: add ALPNCallback server option for dynamic ALPN negotiation (Tim Perry) [#45190](https://github.com/nodejs/node/pull/45190)

### Commits

* \[[`8a611a387f`](https://github.com/nodejs/node/commit/8a611a387f)] - **benchmark**: add bar.R (Rafael Gonzaga) [#47729](https://github.com/nodejs/node/pull/47729)
* \[[`12fa716cf9`](https://github.com/nodejs/node/commit/12fa716cf9)] - **benchmark**: refactor crypto oneshot (Filip Skokan) [#48267](https://github.com/nodejs/node/pull/48267)
* \[[`d6ecbde592`](https://github.com/nodejs/node/commit/d6ecbde592)] - **benchmark**: add crypto.create\*Key (Filip Skokan) [#48284](https://github.com/nodejs/node/pull/48284)
* \[[`e60b6dedd8`](https://github.com/nodejs/node/commit/e60b6dedd8)] - **bootstrap**: unify snapshot builder and embedder entry points (Joyee Cheung) [#48242](https://github.com/nodejs/node/pull/48242)
* \[[`40662957b1`](https://github.com/nodejs/node/commit/40662957b1)] - **bootstrap**: simplify initialization of source map handlers (Joyee Cheung) [#48304](https://github.com/nodejs/node/pull/48304)
* \[[`6551538079`](https://github.com/nodejs/node/commit/6551538079)] - **build**: fix `configure --link-module` (Richard Lau) [#48522](https://github.com/nodejs/node/pull/48522)
* \[[`f7f32089e7`](https://github.com/nodejs/node/commit/f7f32089e7)] - **build**: sync libuv header change (Jiawen Geng) [#48429](https://github.com/nodejs/node/pull/48429)
* \[[`f60205c915`](https://github.com/nodejs/node/commit/f60205c915)] - **build**: update action to close stale PRs (Michael Dawson) [#48196](https://github.com/nodejs/node/pull/48196)
* \[[`4f4d0b802e`](https://github.com/nodejs/node/commit/4f4d0b802e)] - **child\_process**: improve spawn performance on Linux (Keyhan Vakil) [#48523](https://github.com/nodejs/node/pull/48523)
* \[[`fe333d2584`](https://github.com/nodejs/node/commit/fe333d2584)] - **crypto**: update root certificates to NSS 3.90 (Node.js GitHub Bot) [#48416](https://github.com/nodejs/node/pull/48416)
* \[[`89aaf16237`](https://github.com/nodejs/node/commit/89aaf16237)] - **crypto**: remove OPENSSL\_FIPS guard for OpenSSL 3 (Richard Lau) [#48392](https://github.com/nodejs/node/pull/48392)
* \[[`6199e1946c`](https://github.com/nodejs/node/commit/6199e1946c)] - **deps**: upgrade to libuv 1.46.0 (Santiago Gimeno) [#48618](https://github.com/nodejs/node/pull/48618)
* \[[`1b2b930fda`](https://github.com/nodejs/node/commit/1b2b930fda)] - **deps**: add loong64 config into openssl gypi (Shi Pujin) [#48043](https://github.com/nodejs/node/pull/48043)
* \[[`ba8d048929`](https://github.com/nodejs/node/commit/ba8d048929)] - **deps**: update acorn to 8.9.0 (Node.js GitHub Bot) [#48484](https://github.com/nodejs/node/pull/48484)
* \[[`d96f921d06`](https://github.com/nodejs/node/commit/d96f921d06)] - **deps**: update zlib to 1.2.13.1-motley-f81f385 (Node.js GitHub Bot) [#48541](https://github.com/nodejs/node/pull/48541)
* \[[`ed1d047e8f`](https://github.com/nodejs/node/commit/ed1d047e8f)] - **deps**: update googletest to ec4fed9 (Node.js GitHub Bot) [#48538](https://github.com/nodejs/node/pull/48538)
* \[[`f43d718c67`](https://github.com/nodejs/node/commit/f43d718c67)] - **deps**: update minimatch to 9.0.2 (Node.js GitHub Bot) [#48542](https://github.com/nodejs/node/pull/48542)
* \[[`2f66147cbf`](https://github.com/nodejs/node/commit/2f66147cbf)] - **deps**: update corepack to 0.19.0 (Node.js GitHub Bot) [#48540](https://github.com/nodejs/node/pull/48540)
* \[[`d91b0fde73`](https://github.com/nodejs/node/commit/d91b0fde73)] - **deps**: V8: cherry-pick 1a782f6543ae (Keyhan Vakil) [#48523](https://github.com/nodejs/node/pull/48523)
* \[[`112335e342`](https://github.com/nodejs/node/commit/112335e342)] - **deps**: update corepack to 0.18.1 (Node.js GitHub Bot) [#48483](https://github.com/nodejs/node/pull/48483)
* \[[`2b141c413f`](https://github.com/nodejs/node/commit/2b141c413f)] - **deps**: update icu to 73.2 (Node.js GitHub Bot) [#48502](https://github.com/nodejs/node/pull/48502)
* \[[`188b34d4a1`](https://github.com/nodejs/node/commit/188b34d4a1)] - **deps**: upgrade npm to 9.7.2 (npm team) [#48514](https://github.com/nodejs/node/pull/48514)
* \[[`bf0444b5d9`](https://github.com/nodejs/node/commit/bf0444b5d9)] - **deps**: update zlib to 1.2.13.1-motley-3ca9f16 (Node.js GitHub Bot) [#48413](https://github.com/nodejs/node/pull/48413)
* \[[`b339d80a56`](https://github.com/nodejs/node/commit/b339d80a56)] - **deps**: upgrade npm to 9.7.1 (npm team) [#48378](https://github.com/nodejs/node/pull/48378)
* \[[`4132931b87`](https://github.com/nodejs/node/commit/4132931b87)] - **deps**: update simdutf to 3.2.14 (Node.js GitHub Bot) [#48344](https://github.com/nodejs/node/pull/48344)
* \[[`8cd56c1e85`](https://github.com/nodejs/node/commit/8cd56c1e85)] - **deps**: update ada to 2.5.1 (Node.js GitHub Bot) [#48319](https://github.com/nodejs/node/pull/48319)
* \[[`78cffcd645`](https://github.com/nodejs/node/commit/78cffcd645)] - **deps**: update zlib to 982b036 (Node.js GitHub Bot) [#48327](https://github.com/nodejs/node/pull/48327)
* \[[`6d00c2e33b`](https://github.com/nodejs/node/commit/6d00c2e33b)] - **doc**: fix options order (Luigi Pinca) [#48617](https://github.com/nodejs/node/pull/48617)
* \[[`7ad2d3a5d1`](https://github.com/nodejs/node/commit/7ad2d3a5d1)] - **doc**: update security release stewards (Rafael Gonzaga) [#48569](https://github.com/nodejs/node/pull/48569)
* \[[`cc3a056fdd`](https://github.com/nodejs/node/commit/cc3a056fdd)] - **doc**: update return type for describe (Shrujal Shah) [#48572](https://github.com/nodejs/node/pull/48572)
* \[[`99ae0b98af`](https://github.com/nodejs/node/commit/99ae0b98af)] - **doc**: run license-builder (github-actions\[bot]) [#48552](https://github.com/nodejs/node/pull/48552)
* \[[`9750d8205c`](https://github.com/nodejs/node/commit/9750d8205c)] - **doc**: add description of autoAllocateChunkSize in ReadableStream (Debadree Chatterjee) [#48004](https://github.com/nodejs/node/pull/48004)
* \[[`417927bb41`](https://github.com/nodejs/node/commit/417927bb41)] - **doc**: fix `filename` type in `watch` result (Dmitry Semigradsky) [#48032](https://github.com/nodejs/node/pull/48032)
* \[[`ca2ae86bd7`](https://github.com/nodejs/node/commit/ca2ae86bd7)] - **doc**: unnest `mime` and `MIMEParams` from MIMEType constructor (Dmitry Semigradsky) [#47950](https://github.com/nodejs/node/pull/47950)
* \[[`bda1228135`](https://github.com/nodejs/node/commit/bda1228135)] - **doc**: update security-release-process.md (Rafael Gonzaga) [#48504](https://github.com/nodejs/node/pull/48504)
* \[[`60c2ea4e79`](https://github.com/nodejs/node/commit/60c2ea4e79)] - **doc**: add vmoroz to collaborators (Vladimir Morozov) [#48527](https://github.com/nodejs/node/pull/48527)
* \[[`37bc0eac4a`](https://github.com/nodejs/node/commit/37bc0eac4a)] - **doc**: improve inspector.close() description (mary marchini) [#48494](https://github.com/nodejs/node/pull/48494)
* \[[`2a403cdad5`](https://github.com/nodejs/node/commit/2a403cdad5)] - **doc**: link to Runtime Keys in export conditions (Jacob Hummer) [#48408](https://github.com/nodejs/node/pull/48408)
* \[[`e2d579e644`](https://github.com/nodejs/node/commit/e2d579e644)] - **doc**: update fs flags documentation (sinkhaha) [#48463](https://github.com/nodejs/node/pull/48463)
* \[[`38bf290115`](https://github.com/nodejs/node/commit/38bf290115)] - **doc**: revise `error.md` introduction (Antoine du Hamel) [#48423](https://github.com/nodejs/node/pull/48423)
* \[[`641a2e9c6d`](https://github.com/nodejs/node/commit/641a2e9c6d)] - **doc**: add preveen-stack to triagers (Preveen P) [#48387](https://github.com/nodejs/node/pull/48387)
* \[[`4ab5e8d2e3`](https://github.com/nodejs/node/commit/4ab5e8d2e3)] - **doc**: refine when file is undefined in test events (Moshe Atlow) [#48451](https://github.com/nodejs/node/pull/48451)
* \[[`5cacdf9e6b`](https://github.com/nodejs/node/commit/5cacdf9e6b)] - **doc**: add kvakil to collaborators (Keyhan Vakil) [#48449](https://github.com/nodejs/node/pull/48449)
* \[[`b9c643e3ef`](https://github.com/nodejs/node/commit/b9c643e3ef)] - **doc**: add additional info on TSFN dispatch (Michael Dawson) [#48367](https://github.com/nodejs/node/pull/48367)
* \[[`17a0e1d1bf`](https://github.com/nodejs/node/commit/17a0e1d1bf)] - **doc**: add link for news from security wg (Michael Dawson) [#48396](https://github.com/nodejs/node/pull/48396)
* \[[`3a62994a4f`](https://github.com/nodejs/node/commit/3a62994a4f)] - **doc**: fix typo in events.md (Darshan Sen) [#48436](https://github.com/nodejs/node/pull/48436)
* \[[`e10a4cdf68`](https://github.com/nodejs/node/commit/e10a4cdf68)] - **doc**: run license-builder (github-actions\[bot]) [#48336](https://github.com/nodejs/node/pull/48336)
* \[[`19fde638fd`](https://github.com/nodejs/node/commit/19fde638fd)] - **fs**: call the callback with an error if writeSync fails (killa) [#47949](https://github.com/nodejs/node/pull/47949)
* \[[`4cad9fd8bd`](https://github.com/nodejs/node/commit/4cad9fd8bd)] - **fs**: remove unneeded return statement (Luigi Pinca) [#48526](https://github.com/nodejs/node/pull/48526)
* \[[`d367b73f43`](https://github.com/nodejs/node/commit/d367b73f43)] - **fs**: use kResistStopPropagation (Chemi Atlow) [#48521](https://github.com/nodejs/node/pull/48521)
* \[[`e50c3169af`](https://github.com/nodejs/node/commit/e50c3169af)] - **fs, stream**: initial `Symbol.dispose` and `Symbol.asyncDispose` support (Moshe Atlow) [#48518](https://github.com/nodejs/node/pull/48518)
* \[[`7d8a0b6eb7`](https://github.com/nodejs/node/commit/7d8a0b6eb7)] - **http**: null the joinDuplicateHeaders property on cleanup (Luigi Pinca) [#48608](https://github.com/nodejs/node/pull/48608)
* \[[`94ebb02f59`](https://github.com/nodejs/node/commit/94ebb02f59)] - **http**: server add async dispose (atlowChemi) [#48548](https://github.com/nodejs/node/pull/48548)
* \[[`c6a69e31a3`](https://github.com/nodejs/node/commit/c6a69e31a3)] - **http**: remove useless ternary in test (geekreal) [#48481](https://github.com/nodejs/node/pull/48481)
* \[[`2f0f40328f`](https://github.com/nodejs/node/commit/2f0f40328f)] - **http**: fix for handling on boot timers headers and request (Franciszek Koltuniuk) [#48291](https://github.com/nodejs/node/pull/48291)
* \[[`5378ad8ab1`](https://github.com/nodejs/node/commit/5378ad8ab1)] - **http2**: server add `asyncDispose` (atlowChemi) [#48548](https://github.com/nodejs/node/pull/48548)
* \[[`97a58c5970`](https://github.com/nodejs/node/commit/97a58c5970)] - **https**: server add `asyncDispose` (atlowChemi) [#48548](https://github.com/nodejs/node/pull/48548)
* \[[`40ae6eb6aa`](https://github.com/nodejs/node/commit/40ae6eb6aa)] - **https**: fix connection checking interval not clearing on server close (Nitzan Uziely) [#48383](https://github.com/nodejs/node/pull/48383)
* \[[`15530fea4c`](https://github.com/nodejs/node/commit/15530fea4c)] - **lib**: merge cjs and esm package json reader caches (Yagiz Nizipli) [#48477](https://github.com/nodejs/node/pull/48477)
* \[[`32bda81c31`](https://github.com/nodejs/node/commit/32bda81c31)] - **lib**: reduce url getters on `makeRequireFunction` (Yagiz Nizipli) [#48492](https://github.com/nodejs/node/pull/48492)
* \[[`0da03f01ba`](https://github.com/nodejs/node/commit/0da03f01ba)] - **lib**: remove duplicated requires in check\_syntax (Yagiz Nizipli) [#48508](https://github.com/nodejs/node/pull/48508)
* \[[`97b00c347d`](https://github.com/nodejs/node/commit/97b00c347d)] - **lib**: add option to force handling stopped events (Chemi Atlow) [#48301](https://github.com/nodejs/node/pull/48301)
* \[[`fe16749649`](https://github.com/nodejs/node/commit/fe16749649)] - **lib**: fix output message when repl is used with pm (Rafael Gonzaga) [#48438](https://github.com/nodejs/node/pull/48438)
* \[[`8c2c02d28a`](https://github.com/nodejs/node/commit/8c2c02d28a)] - **lib**: create weakRef only if any signals provided (Chemi Atlow) [#48448](https://github.com/nodejs/node/pull/48448)
* \[[`b6ae411ea9`](https://github.com/nodejs/node/commit/b6ae411ea9)] - **lib**: remove obsolete deletion of bufferBinding.zeroFill (Chengzhong Wu) [#47881](https://github.com/nodejs/node/pull/47881)
* \[[`562b3d4856`](https://github.com/nodejs/node/commit/562b3d4856)] - **lib**: move web global bootstrapping to the expected file (Chengzhong Wu) [#47881](https://github.com/nodejs/node/pull/47881)
* \[[`f9c0d5acac`](https://github.com/nodejs/node/commit/f9c0d5acac)] - **lib**: fix blob.stream() causing hanging promises (Debadree Chatterjee) [#48232](https://github.com/nodejs/node/pull/48232)
* \[[`0162a0f5bf`](https://github.com/nodejs/node/commit/0162a0f5bf)] - **lib**: add support for inherited custom inspection methods (Antoine du Hamel) [#48306](https://github.com/nodejs/node/pull/48306)
* \[[`159ab6627a`](https://github.com/nodejs/node/commit/159ab6627a)] - **lib**: reduce URL invocations on http2 origins (Yagiz Nizipli) [#48338](https://github.com/nodejs/node/pull/48338)
* \[[`f0709fdc59`](https://github.com/nodejs/node/commit/f0709fdc59)] - **module**: add SourceMap.findOrigin (Isaac Z. Schlueter) [#47790](https://github.com/nodejs/node/pull/47790)
* \[[`4ec2d925b1`](https://github.com/nodejs/node/commit/4ec2d925b1)] - **module**: reduce url invocations in esm/load.js (Yagiz Nizipli) [#48337](https://github.com/nodejs/node/pull/48337)
* \[[`2c363971cc`](https://github.com/nodejs/node/commit/2c363971cc)] - **net**: improve network family autoselection handle handling (Paolo Insogna) [#48464](https://github.com/nodejs/node/pull/48464)
* \[[`dbf9e9ffc8`](https://github.com/nodejs/node/commit/dbf9e9ffc8)] - **node-api**: provide napi\_define\_properties fast path (Gabriel Schulhof) [#48440](https://github.com/nodejs/node/pull/48440)
* \[[`87ad657777`](https://github.com/nodejs/node/commit/87ad657777)] - **node-api**: implement external strings (Gabriel Schulhof) [#48339](https://github.com/nodejs/node/pull/48339)
* \[[`4efa6807ea`](https://github.com/nodejs/node/commit/4efa6807ea)] - **permission**: handle end nodes with children cases (Rafael Gonzaga) [#48531](https://github.com/nodejs/node/pull/48531)
* \[[`84fe811108`](https://github.com/nodejs/node/commit/84fe811108)] - **repl**: display dynamic import variant in static import error messages (Hemanth HM) [#48129](https://github.com/nodejs/node/pull/48129)
* \[[`bdcc037470`](https://github.com/nodejs/node/commit/bdcc037470)] - **report**: disable js stack when no context is entered (Chengzhong Wu) [#48495](https://github.com/nodejs/node/pull/48495)
* \[[`97bd9ccd04`](https://github.com/nodejs/node/commit/97bd9ccd04)] - **src**: fix uninitialized field access in AsyncHooks (Jan Olaf Krems) [#48566](https://github.com/nodejs/node/pull/48566)
* \[[`404958fc96`](https://github.com/nodejs/node/commit/404958fc96)] - **src**: fix Coverity issue regarding unnecessary copy (Yagiz Nizipli) [#48565](https://github.com/nodejs/node/pull/48565)
* \[[`c4b8edea24`](https://github.com/nodejs/node/commit/c4b8edea24)] - **src**: refactor `SplitString` in util (Yagiz Nizipli) [#48491](https://github.com/nodejs/node/pull/48491)
* \[[`5bc13a4772`](https://github.com/nodejs/node/commit/5bc13a4772)] - **src**: revert IS\_RELEASE (Rafael Gonzaga) [#48505](https://github.com/nodejs/node/pull/48505)
* \[[`4971e46051`](https://github.com/nodejs/node/commit/4971e46051)] - **src**: add V8 fast api to `guessHandleType` (Yagiz Nizipli) [#48349](https://github.com/nodejs/node/pull/48349)
* \[[`954e46e792`](https://github.com/nodejs/node/commit/954e46e792)] - **src**: return uint32 for `guessHandleType` (Yagiz Nizipli) [#48349](https://github.com/nodejs/node/pull/48349)
* \[[`05009675da`](https://github.com/nodejs/node/commit/05009675da)] - **src**: make realm binding data store weak (Chengzhong Wu) [#47688](https://github.com/nodejs/node/pull/47688)
* \[[`120ac74352`](https://github.com/nodejs/node/commit/120ac74352)] - **src**: remove aliased buffer weak callback (Chengzhong Wu) [#47688](https://github.com/nodejs/node/pull/47688)
* \[[`6591826e99`](https://github.com/nodejs/node/commit/6591826e99)] - **src**: handle wasm out of bound in osx will raise SIGBUS correctly (Congcong Cai) [#46561](https://github.com/nodejs/node/pull/46561)
* \[[`1b84ddeec2`](https://github.com/nodejs/node/commit/1b84ddeec2)] - **src**: implement constants binding directly (Joyee Cheung) [#48186](https://github.com/nodejs/node/pull/48186)
* \[[`06d49c1f10`](https://github.com/nodejs/node/commit/06d49c1f10)] - **src**: implement natives binding without special casing (Joyee Cheung) [#48186](https://github.com/nodejs/node/pull/48186)
* \[[`325441abf5`](https://github.com/nodejs/node/commit/325441abf5)] - **src**: add missing to\_ascii method in dns queries (Daniel Lemire) [#48354](https://github.com/nodejs/node/pull/48354)
* \[[`84d0eb74b8`](https://github.com/nodejs/node/commit/84d0eb74b8)] - **stream**: fix premature pipeline end (Robert Nagy) [#48435](https://github.com/nodejs/node/pull/48435)
* \[[`3df7368735`](https://github.com/nodejs/node/commit/3df7368735)] - **test**: add missing assertions to test-runner-cli (Moshe Atlow) [#48593](https://github.com/nodejs/node/pull/48593)
* \[[`07eb310b0d`](https://github.com/nodejs/node/commit/07eb310b0d)] - **test**: remove test-crypto-keygen flaky designation (Luigi Pinca) [#48575](https://github.com/nodejs/node/pull/48575)
* \[[`75aa0a7682`](https://github.com/nodejs/node/commit/75aa0a7682)] - **test**: remove test-timers-immediate-queue flaky designation (Luigi Pinca) [#48575](https://github.com/nodejs/node/pull/48575)
* \[[`a9756f3126`](https://github.com/nodejs/node/commit/a9756f3126)] - **test**: add Symbol.dispose support to mock timers (Benjamin Gruenbaum) [#48549](https://github.com/nodejs/node/pull/48549)
* \[[`0f912a7248`](https://github.com/nodejs/node/commit/0f912a7248)] - **test**: mark test-child-process-stdio-reuse-readable-stdio flaky (Luigi Pinca) [#48537](https://github.com/nodejs/node/pull/48537)
* \[[`30f4bc4985`](https://github.com/nodejs/node/commit/30f4bc4985)] - **test**: make IsolateData per-isolate in cctest (Joyee Cheung) [#48450](https://github.com/nodejs/node/pull/48450)
* \[[`407ce3fdcb`](https://github.com/nodejs/node/commit/407ce3fdcb)] - **test**: define NAPI\_VERSION before including node\_api.h (Chengzhong Wu) [#48376](https://github.com/nodejs/node/pull/48376)
* \[[`24a8fa95f0`](https://github.com/nodejs/node/commit/24a8fa95f0)] - **test**: remove unnecessary noop function args to `mustNotCall()` (Antoine du Hamel) [#48513](https://github.com/nodejs/node/pull/48513)
* \[[`09af579775`](https://github.com/nodejs/node/commit/09af579775)] - **test**: skip test-runner-watch-mode on IBMi (Moshe Atlow) [#48473](https://github.com/nodejs/node/pull/48473)
* \[[`77cb1ee0b2`](https://github.com/nodejs/node/commit/77cb1ee0b2)] - **test**: add missing \<algorithm> include for std::find (Sam James) [#48380](https://github.com/nodejs/node/pull/48380)
* \[[`7c790ca03c`](https://github.com/nodejs/node/commit/7c790ca03c)] - **test**: fix flaky test-watch-mode (Moshe Atlow) [#48147](https://github.com/nodejs/node/pull/48147)
* \[[`1398829746`](https://github.com/nodejs/node/commit/1398829746)] - **test**: fix `test-net-autoselectfamily` for kernel without IPv6 support (Livia Medeiros) [#48265](https://github.com/nodejs/node/pull/48265)
* \[[`764119ba4b`](https://github.com/nodejs/node/commit/764119ba4b)] - **test**: update url web-platform tests (Yagiz Nizipli) [#48319](https://github.com/nodejs/node/pull/48319)
* \[[`f1ead59629`](https://github.com/nodejs/node/commit/f1ead59629)] - **test**: ignore the copied entry\_point.c (Luigi Pinca) [#48297](https://github.com/nodejs/node/pull/48297)
* \[[`fc5d1bddcb`](https://github.com/nodejs/node/commit/fc5d1bddcb)] - **test**: refactor test-gc-http-client-timeout (Luigi Pinca) [#48292](https://github.com/nodejs/node/pull/48292)
* \[[`46a3d068a0`](https://github.com/nodejs/node/commit/46a3d068a0)] - **test**: update encoding web-platform tests (Yagiz Nizipli) [#48320](https://github.com/nodejs/node/pull/48320)
* \[[`141e5aad83`](https://github.com/nodejs/node/commit/141e5aad83)] - **test**: update FileAPI web-platform tests (Yagiz Nizipli) [#48322](https://github.com/nodejs/node/pull/48322)
* \[[`83cfc67099`](https://github.com/nodejs/node/commit/83cfc67099)] - **test**: update user-timing web-platform tests (Yagiz Nizipli) [#48321](https://github.com/nodejs/node/pull/48321)
* \[[`2c56835a33`](https://github.com/nodejs/node/commit/2c56835a33)] - **test\_runner**: fixed `test` shorthands return type (Shocker) [#48555](https://github.com/nodejs/node/pull/48555)
* \[[`7d01c8894a`](https://github.com/nodejs/node/commit/7d01c8894a)] - **(SEMVER-MINOR)** **test\_runner**: add initial draft for fakeTimers (Erick Wendel) [#47775](https://github.com/nodejs/node/pull/47775)
* \[[`de4f14c249`](https://github.com/nodejs/node/commit/de4f14c249)] - **test\_runner**: add enqueue and dequeue events (Moshe Atlow) [#48428](https://github.com/nodejs/node/pull/48428)
* \[[`5ebe3a4ea7`](https://github.com/nodejs/node/commit/5ebe3a4ea7)] - **test\_runner**: make `--test-name-pattern` recursive (Moshe Atlow) [#48382](https://github.com/nodejs/node/pull/48382)
* \[[`93bf447308`](https://github.com/nodejs/node/commit/93bf447308)] - **test\_runner**: refactor coverage report output for readability (Damien Seguin) [#47791](https://github.com/nodejs/node/pull/47791)
* \[[`504d1d7bdc`](https://github.com/nodejs/node/commit/504d1d7bdc)] - **(SEMVER-MINOR)** **tls**: add ALPNCallback server option for dynamic ALPN negotiation (Tim Perry) [#45190](https://github.com/nodejs/node/pull/45190)
* \[[`203c3cf4ca`](https://github.com/nodejs/node/commit/203c3cf4ca)] - **tools**: update lint-md-dependencies (Node.js GitHub Bot) [#48544](https://github.com/nodejs/node/pull/48544)
* \[[`333907b19d`](https://github.com/nodejs/node/commit/333907b19d)] - **tools**: speedup compilation of js2c output (Keyhan Vakil) [#48160](https://github.com/nodejs/node/pull/48160)
* \[[`10bd5f4d97`](https://github.com/nodejs/node/commit/10bd5f4d97)] - **tools**: update lint-md-dependencies (Node.js GitHub Bot) [#48486](https://github.com/nodejs/node/pull/48486)
* \[[`52de27b9fe`](https://github.com/nodejs/node/commit/52de27b9fe)] - **tools**: pin ruff version number (Rich Trott) [#48505](https://github.com/nodejs/node/pull/48505)
* \[[`4345526644`](https://github.com/nodejs/node/commit/4345526644)] - **tools**: replace sed with perl (Luigi Pinca) [#48499](https://github.com/nodejs/node/pull/48499)
* \[[`6c590835f3`](https://github.com/nodejs/node/commit/6c590835f3)] - **tools**: automate update openssl v16 (Marco Ippolito) [#48377](https://github.com/nodejs/node/pull/48377)
* \[[`90b5335338`](https://github.com/nodejs/node/commit/90b5335338)] - **tools**: update eslint to 8.43.0 (Node.js GitHub Bot) [#48487](https://github.com/nodejs/node/pull/48487)
* \[[`cd83530a11`](https://github.com/nodejs/node/commit/cd83530a11)] - **tools**: update doc to to-vfile\@8.0.0 (Node.js GitHub Bot) [#48485](https://github.com/nodejs/node/pull/48485)
* \[[`e500b439bd`](https://github.com/nodejs/node/commit/e500b439bd)] - **tools**: prepare tools/doc for to-vfile 8.0.0 (Rich Trott) [#48485](https://github.com/nodejs/node/pull/48485)
* \[[`d623616813`](https://github.com/nodejs/node/commit/d623616813)] - **tools**: update lint-md-dependencies (Node.js GitHub Bot) [#48417](https://github.com/nodejs/node/pull/48417)
* \[[`a2e107dde4`](https://github.com/nodejs/node/commit/a2e107dde4)] - **tools**: update create-or-update-pull-request-action (Richard Lau) [#48398](https://github.com/nodejs/node/pull/48398)
* \[[`8009e2c3be`](https://github.com/nodejs/node/commit/8009e2c3be)] - **tools**: update eslint-plugin-jsdoc (Richard Lau) [#48393](https://github.com/nodejs/node/pull/48393)
* \[[`10385c8565`](https://github.com/nodejs/node/commit/10385c8565)] - **tools**: add version update to external dependencies (Andrea Fassina) [#48081](https://github.com/nodejs/node/pull/48081)
* \[[`b1cef81b18`](https://github.com/nodejs/node/commit/b1cef81b18)] - **tools**: update eslint to 8.42.0 (Node.js GitHub Bot) [#48328](https://github.com/nodejs/node/pull/48328)
* \[[`0923dc0b8e`](https://github.com/nodejs/node/commit/0923dc0b8e)] - **tools**: disable jsdoc/no-defaults rule (Luigi Pinca) [#48328](https://github.com/nodejs/node/pull/48328)
* \[[`b03146da85`](https://github.com/nodejs/node/commit/b03146da85)] - **typings**: remove unused primordials (Yagiz Nizipli) [#48509](https://github.com/nodejs/node/pull/48509)
* \[[`e9c9d187b9`](https://github.com/nodejs/node/commit/e9c9d187b9)] - **typings**: fix JSDoc in ESM loader modules (Antoine du Hamel) [#48424](https://github.com/nodejs/node/pull/48424)
* \[[`fafe651d23`](https://github.com/nodejs/node/commit/fafe651d23)] - **url**: conform to origin getter spec changes (Yagiz Nizipli) [#48319](https://github.com/nodejs/node/pull/48319)

<a id="20.3.1"></a>

## 2023-06-20, Version 20.3.1 (Current), @RafaelGSS

This is a security release.

### Notable Changes

The following CVEs are fixed in this release:

* [CVE-2023-30581](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2023-30581): `mainModule.__proto__` Bypass Experimental Policy Mechanism (High)
* [CVE-2023-30584](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2023-30584): Path Traversal Bypass in Experimental Permission Model (High)
* [CVE-2023-30587](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2023-30587): Bypass of Experimental Permission Model via Node.js Inspector (High)
* [CVE-2023-30582](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2023-30582): Inadequate Permission Model Allows Unauthorized File Watching (Medium)
* [CVE-2023-30583](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2023-30583): Bypass of Experimental Permission Model via fs.openAsBlob() (Medium)
* [CVE-2023-30585](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2023-30585): Privilege escalation via Malicious Registry Key manipulation during Node.js installer repair process (Medium)
* [CVE-2023-30586](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2023-30586): Bypass of Experimental Permission Model via Arbitrary OpenSSL Engines (Medium)
* [CVE-2023-30588](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2023-30588): Process interuption due to invalid Public Key information in x509 certificates (Medium)
* [CVE-2023-30589](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2023-30589): HTTP Request Smuggling via Empty headers separated by CR (Medium)
* [CVE-2023-30590](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2023-30590): DiffieHellman does not generate keys after setting a private key (Medium)
* OpenSSL Security Releases
  * [OpenSSL security advisory 28th March](https://www.openssl.org/news/secadv/20230328.txt).
  * [OpenSSL security advisory 20th April](https://www.openssl.org/news/secadv/20230420.txt).
  * [OpenSSL security advisory 30th May](https://www.openssl.org/news/secadv/20230530.txt)

More detailed information on each of the vulnerabilities can be found in [June 2023 Security Releases](https://nodejs.org/en/blog/vulnerability/june-2023-security-releases/) blog post.

### Commits

* \[[`dac08dafc9`](https://github.com/nodejs/node/commit/dac08dafc9)] - **crypto**: handle cert with invalid SPKI gracefully (Tobias Nießen) [nodejs-private/node-private#393](https://github.com/nodejs-private/node-private/pull/393)
* \[[`d274c3babc`](https://github.com/nodejs/node/commit/d274c3babc)] - **crypto,https,tls**: disable engines if perms enabled (Tobias Nießen) [nodejs-private/node-private#409](https://github.com/nodejs-private/node-private/pull/409)
* \[[`5621c1de38`](https://github.com/nodejs/node/commit/5621c1de38)] - **deps**: update archs files for openssl-3.0.9-quic1 (Node.js GitHub Bot) [#48402](https://github.com/nodejs/node/pull/48402)
* \[[`771caa9f1c`](https://github.com/nodejs/node/commit/771caa9f1c)] - **deps**: upgrade openssl sources to quictls/openssl-3.0.9-quic1 (Node.js GitHub Bot) [#48402](https://github.com/nodejs/node/pull/48402)
* \[[`0459bf9c99`](https://github.com/nodejs/node/commit/0459bf9c99)] - **doc,test**: clarify behavior of DH generateKeys (Tobias Nießen) [nodejs-private/node-private#426](https://github.com/nodejs-private/node-private/pull/426)
* \[[`27e20501aa`](https://github.com/nodejs/node/commit/27e20501aa)] - **http**: disable request smuggling via empty headers (Paolo Insogna) [nodejs-private/node-private#427](https://github.com/nodejs-private/node-private/pull/427)
* \[[`9c17e335f1`](https://github.com/nodejs/node/commit/9c17e335f1)] - **msi**: do not create AppData\Roaming\npm (Tobias Nießen) [nodejs-private/node-private#408](https://github.com/nodejs-private/node-private/pull/408)
* \[[`b51124c637`](https://github.com/nodejs/node/commit/b51124c637)] - **permission**: handle fs path traversal (RafaelGSS) [nodejs-private/node-private#403](https://github.com/nodejs-private/node-private/pull/403)
* \[[`ebc5927adc`](https://github.com/nodejs/node/commit/ebc5927adc)] - **permission**: handle fs.openAsBlob (RafaelGSS) [nodejs-private/node-private#405](https://github.com/nodejs-private/node-private/pull/405)
* \[[`c39a43bff5`](https://github.com/nodejs/node/commit/c39a43bff5)] - **permission**: handle fs.watchFile (RafaelGSS) [nodejs-private/node-private#404](https://github.com/nodejs-private/node-private/pull/404)
* \[[`d0a8264ec9`](https://github.com/nodejs/node/commit/d0a8264ec9)] - **policy**: handle mainModule.\_\_proto\_\_ bypass (RafaelGSS) [nodejs-private/node-private#416](https://github.com/nodejs-private/node-private/pull/416)
* \[[`3df13d5a79`](https://github.com/nodejs/node/commit/3df13d5a79)] - **src,permission**: restrict inspector when pm enabled (RafaelGSS) [nodejs-private/node-private#410](https://github.com/nodejs-private/node-private/pull/410)

<a id="20.3.0"></a>

## 2023-06-08, Version 20.3.0 (Current), @targos

### Notable Changes

* \[[`bfcb3d1d9a`](https://github.com/nodejs/node/commit/bfcb3d1d9a)] - **deps**: upgrade to libuv 1.45.0, including significant performance improvements to file system operations on Linux (Santiago Gimeno) [#48078](https://github.com/nodejs/node/pull/48078)
* \[[`5094d1b292`](https://github.com/nodejs/node/commit/5094d1b292)] - **doc**: add Ruy Adorno to list of TSC members (Michael Dawson) [#48172](https://github.com/nodejs/node/pull/48172)
* \[[`2f5dbca690`](https://github.com/nodejs/node/commit/2f5dbca690)] - **doc**: mark Node.js 14 as End-of-Life (Richard Lau) [#48023](https://github.com/nodejs/node/pull/48023)
* \[[`b1828b325e`](https://github.com/nodejs/node/commit/b1828b325e)] - **(SEMVER-MINOR)** **lib**: implement `AbortSignal.any()` (Chemi Atlow) [#47821](https://github.com/nodejs/node/pull/47821)
* \[[`f380953103`](https://github.com/nodejs/node/commit/f380953103)] - **module**: change default resolver to not throw on unknown scheme (Gil Tayar) [#47824](https://github.com/nodejs/node/pull/47824)
* \[[`a94f87ed99`](https://github.com/nodejs/node/commit/a94f87ed99)] - **(SEMVER-MINOR)** **node-api**: define version 9 (Chengzhong Wu) [#48151](https://github.com/nodejs/node/pull/48151)
* \[[`9e2b13dfa7`](https://github.com/nodejs/node/commit/9e2b13dfa7)] - **stream**: deprecate `asIndexedPairs` (Chemi Atlow) [#48102](https://github.com/nodejs/node/pull/48102)

### Commits

* \[[`35c96156d1`](https://github.com/nodejs/node/commit/35c96156d1)] - **benchmark**: use `cluster.isPrimary` instead of `cluster.isMaster` (Deokjin Kim) [#48002](https://github.com/nodejs/node/pull/48002)
* \[[`3e6e3abf32`](https://github.com/nodejs/node/commit/3e6e3abf32)] - **bootstrap**: throw ERR\_NOT\_SUPPORTED\_IN\_SNAPSHOT in unsupported operation (Joyee Cheung) [#47887](https://github.com/nodejs/node/pull/47887)
* \[[`c480559347`](https://github.com/nodejs/node/commit/c480559347)] - **bootstrap**: put is\_building\_snapshot state in IsolateData (Joyee Cheung) [#47887](https://github.com/nodejs/node/pull/47887)
* \[[`50c0a15535`](https://github.com/nodejs/node/commit/50c0a15535)] - **build**: set v8\_enable\_webassembly=false when lite mode is enabled (Cheng Shao) [#48248](https://github.com/nodejs/node/pull/48248)
* \[[`4562805cf6`](https://github.com/nodejs/node/commit/4562805cf6)] - **build**: speed up compilation of mksnapshot output (Keyhan Vakil) [#48162](https://github.com/nodejs/node/pull/48162)
* \[[`8b89f13933`](https://github.com/nodejs/node/commit/8b89f13933)] - **build**: add action to close stale PRs (Michael Dawson) [#48051](https://github.com/nodejs/node/pull/48051)
* \[[`5d92202220`](https://github.com/nodejs/node/commit/5d92202220)] - **build**: replace js2c.py with js2c.cc (Joyee Cheung) [#46997](https://github.com/nodejs/node/pull/46997)
* \[[`6cf2adc36e`](https://github.com/nodejs/node/commit/6cf2adc36e)] - **cluster**: use ObjectPrototypeHasOwnProperty (Daeyeon Jeong) [#48141](https://github.com/nodejs/node/pull/48141)
* \[[`f564b03c38`](https://github.com/nodejs/node/commit/f564b03c38)] - **crypto**: use openssl's own memory BIOs in crypto\_context.cc (GauriSpears) [#47160](https://github.com/nodejs/node/pull/47160)
* \[[`ac8dd61fc3`](https://github.com/nodejs/node/commit/ac8dd61fc3)] - **crypto**: remove default encoding from cipher (Tobias Nießen) [#47998](https://github.com/nodejs/node/pull/47998)
* \[[`15c2de4407`](https://github.com/nodejs/node/commit/15c2de4407)] - **crypto**: fix setEngine() when OPENSSL\_NO\_ENGINE set (Tobias Nießen) [#47977](https://github.com/nodejs/node/pull/47977)
* \[[`9e2dd5b5e2`](https://github.com/nodejs/node/commit/9e2dd5b5e2)] - **deps**: update zlib to 337322d (Node.js GitHub Bot) [#48218](https://github.com/nodejs/node/pull/48218)
* \[[`bfcb3d1d9a`](https://github.com/nodejs/node/commit/bfcb3d1d9a)] - **deps**: upgrade to libuv 1.45.0 (Santiago Gimeno) [#48078](https://github.com/nodejs/node/pull/48078)
* \[[`13930f092f`](https://github.com/nodejs/node/commit/13930f092f)] - **deps**: update ada to 2.5.0 (Node.js GitHub Bot) [#48223](https://github.com/nodejs/node/pull/48223)
* \[[`3047caebec`](https://github.com/nodejs/node/commit/3047caebec)] - **deps**: set `CARES_RANDOM_FILE` for c-ares (Richard Lau) [#48156](https://github.com/nodejs/node/pull/48156)
* \[[`0db79a0872`](https://github.com/nodejs/node/commit/0db79a0872)] - **deps**: update histogram 0.11.8 (Marco Ippolito) [#47742](https://github.com/nodejs/node/pull/47742)
* \[[`99af6716f5`](https://github.com/nodejs/node/commit/99af6716f5)] - **deps**: update histogram to 0.11.7 (Marco Ippolito) [#47742](https://github.com/nodejs/node/pull/47742)
* \[[`d4922bc985`](https://github.com/nodejs/node/commit/d4922bc985)] - **deps**: update c-ares to 1.19.1 (Node.js GitHub Bot) [#48115](https://github.com/nodejs/node/pull/48115)
* \[[`f6ccdb289f`](https://github.com/nodejs/node/commit/f6ccdb289f)] - **deps**: update simdutf to 3.2.12 (Node.js GitHub Bot) [#48118](https://github.com/nodejs/node/pull/48118)
* \[[`3ed0afc778`](https://github.com/nodejs/node/commit/3ed0afc778)] - **deps**: update minimatch to 9.0.1 (Node.js GitHub Bot) [#48094](https://github.com/nodejs/node/pull/48094)
* \[[`df7540fb73`](https://github.com/nodejs/node/commit/df7540fb73)] - **deps**: update ada to 2.4.2 (Node.js GitHub Bot) [#48092](https://github.com/nodejs/node/pull/48092)
* \[[`07df5c48e8`](https://github.com/nodejs/node/commit/07df5c48e8)] - **deps**: update corepack to 0.18.0 (Node.js GitHub Bot) [#48091](https://github.com/nodejs/node/pull/48091)
* \[[`d95a5bb559`](https://github.com/nodejs/node/commit/d95a5bb559)] - **deps**: update uvwasi to 0.0.18 (Node.js GitHub Bot) [#47866](https://github.com/nodejs/node/pull/47866)
* \[[`443477e041`](https://github.com/nodejs/node/commit/443477e041)] - **deps**: update uvwasi to 0.0.17 (Node.js GitHub Bot) [#47866](https://github.com/nodejs/node/pull/47866)
* \[[`03f67d6d6d`](https://github.com/nodejs/node/commit/03f67d6d6d)] - **deps**: upgrade npm to 9.6.7 (npm team) [#48062](https://github.com/nodejs/node/pull/48062)
* \[[`d3e3a911fd`](https://github.com/nodejs/node/commit/d3e3a911fd)] - **deps**: update nghttp2 to 1.53.0 (Node.js GitHub Bot) [#47997](https://github.com/nodejs/node/pull/47997)
* \[[`f7c4daaf67`](https://github.com/nodejs/node/commit/f7c4daaf67)] - **deps**: update ada to 2.4.1 (Node.js GitHub Bot) [#48036](https://github.com/nodejs/node/pull/48036)
* \[[`c6a752560d`](https://github.com/nodejs/node/commit/c6a752560d)] - **deps**: add loongarch64 into openssl Makefile and gen openssl-loongarch64 (Shi Pujin) [#46401](https://github.com/nodejs/node/pull/46401)
* \[[`d194241716`](https://github.com/nodejs/node/commit/d194241716)] - **deps**: update undici to 5.22.1 (Node.js GitHub Bot) [#47994](https://github.com/nodejs/node/pull/47994)
* \[[`02e919f4a2`](https://github.com/nodejs/node/commit/02e919f4a2)] - **deps,test**: update postject to 1.0.0-alpha.6 (Node.js GitHub Bot) [#48072](https://github.com/nodejs/node/pull/48072)
* \[[`2c19f596ad`](https://github.com/nodejs/node/commit/2c19f596ad)] - **doc**: clarify array args to Buffer.from() (Bryan English) [#48274](https://github.com/nodejs/node/pull/48274)
* \[[`d681e5f456`](https://github.com/nodejs/node/commit/d681e5f456)] - **doc**: document watch option for node:test run() (Moshe Atlow) [#48256](https://github.com/nodejs/node/pull/48256)
* \[[`96e54ddbca`](https://github.com/nodejs/node/commit/96e54ddbca)] - **doc**: reserve 117 for Electron 26 (Calvin) [#48245](https://github.com/nodejs/node/pull/48245)
* \[[`9aff8c7818`](https://github.com/nodejs/node/commit/9aff8c7818)] - **doc**: update documentation for FIPS support (Richard Lau) [#48194](https://github.com/nodejs/node/pull/48194)
* \[[`8c5338648f`](https://github.com/nodejs/node/commit/8c5338648f)] - **doc**: improve the documentation of the stdio option (Kumar Arnav) [#48110](https://github.com/nodejs/node/pull/48110)
* \[[`11918d705f`](https://github.com/nodejs/node/commit/11918d705f)] - **doc**: update Buffer.allocUnsafe description (sinkhaha) [#48183](https://github.com/nodejs/node/pull/48183)
* \[[`2b51ee5e22`](https://github.com/nodejs/node/commit/2b51ee5e22)] - **doc**: update codeowners with website team (Claudio Wunder) [#48197](https://github.com/nodejs/node/pull/48197)
* \[[`360df25d04`](https://github.com/nodejs/node/commit/360df25d04)] - **doc**: fix broken link to new folder doc/contributing/maintaining (Andrea Fassina) [#48205](https://github.com/nodejs/node/pull/48205)
* \[[`13e95e21a4`](https://github.com/nodejs/node/commit/13e95e21a4)] - **doc**: add atlowChemi to triagers (Chemi Atlow) [#48104](https://github.com/nodejs/node/pull/48104)
* \[[`5f83ce530f`](https://github.com/nodejs/node/commit/5f83ce530f)] - **doc**: fix typo in readline completer function section (Vadym) [#48188](https://github.com/nodejs/node/pull/48188)
* \[[`3c82165d27`](https://github.com/nodejs/node/commit/3c82165d27)] - **doc**: remove broken link for keygen (Rich Trott) [#48176](https://github.com/nodejs/node/pull/48176)
* \[[`0ca90a1e6d`](https://github.com/nodejs/node/commit/0ca90a1e6d)] - **doc**: add `auto` intrinsic height to prevent jitter/flicker (Daniel Holbert) [#48195](https://github.com/nodejs/node/pull/48195)
* \[[`f117855092`](https://github.com/nodejs/node/commit/f117855092)] - **doc**: add version info on the SEA docs (Antoine du Hamel) [#48173](https://github.com/nodejs/node/pull/48173)
* \[[`5094d1b292`](https://github.com/nodejs/node/commit/5094d1b292)] - **doc**: add Ruy to list of TSC members (Michael Dawson) [#48172](https://github.com/nodejs/node/pull/48172)
* \[[`39d8140227`](https://github.com/nodejs/node/commit/39d8140227)] - **doc**: update socket.remote\* properties documentation (Saba Kharanauli) [#48139](https://github.com/nodejs/node/pull/48139)
* \[[`5497c13efe`](https://github.com/nodejs/node/commit/5497c13efe)] - **doc**: update outdated section on TLSv1.3-PSK (Tobias Nießen) [#48123](https://github.com/nodejs/node/pull/48123)
* \[[`281dfaf727`](https://github.com/nodejs/node/commit/281dfaf727)] - **doc**: improve HMAC key recommendations (Tobias Nießen) [#48121](https://github.com/nodejs/node/pull/48121)
* \[[`bd311b6c70`](https://github.com/nodejs/node/commit/bd311b6c70)] - **doc**: clarify mkdir() recursive behavior (Stephen Odogwu) [#48109](https://github.com/nodejs/node/pull/48109)
* \[[`5b061c8922`](https://github.com/nodejs/node/commit/5b061c8922)] - **doc**: fix typo in crypto legacy streams API section (Tobias Nießen) [#48122](https://github.com/nodejs/node/pull/48122)
* \[[`10ccb2bd81`](https://github.com/nodejs/node/commit/10ccb2bd81)] - **doc**: update SEA source link (Rich Trott) [#48080](https://github.com/nodejs/node/pull/48080)
* \[[`415bf7f532`](https://github.com/nodejs/node/commit/415bf7f532)] - **doc**: clarify tty.isRaw (Roberto Vidal) [#48055](https://github.com/nodejs/node/pull/48055)
* \[[`0ac4b33c76`](https://github.com/nodejs/node/commit/0ac4b33c76)] - **doc**: correct line break for Windows terminals (Alex Schwartz) [#48083](https://github.com/nodejs/node/pull/48083)
* \[[`f30ba5c320`](https://github.com/nodejs/node/commit/f30ba5c320)] - **doc**: fix Windows code snippet tags (Antoine du Hamel) [#48100](https://github.com/nodejs/node/pull/48100)
* \[[`12fef9b68c`](https://github.com/nodejs/node/commit/12fef9b68c)] - **doc**: harmonize fenced code snippet flags (Antoine du Hamel) [#48082](https://github.com/nodejs/node/pull/48082)
* \[[`13f163eace`](https://github.com/nodejs/node/commit/13f163eace)] - **doc**: use secure key length for HMAC generateKey (Tobias Nießen) [#48052](https://github.com/nodejs/node/pull/48052)
* \[[`1e3e7c9f33`](https://github.com/nodejs/node/commit/1e3e7c9f33)] - **doc**: update broken EVP\_BytesToKey link (Rich Trott) [#48064](https://github.com/nodejs/node/pull/48064)
* \[[`5917ba1838`](https://github.com/nodejs/node/commit/5917ba1838)] - **doc**: update broken spkac link (Rich Trott) [#48063](https://github.com/nodejs/node/pull/48063)
* \[[`0e4a3b7db1`](https://github.com/nodejs/node/commit/0e4a3b7db1)] - **doc**: document node-api version process (Chengzhong Wu) [#47972](https://github.com/nodejs/node/pull/47972)
* \[[`85bbaa94ea`](https://github.com/nodejs/node/commit/85bbaa94ea)] - **doc**: update process.versions properties (Saba Kharanauli) [#48019](https://github.com/nodejs/node/pull/48019)
* \[[`7660eb591a`](https://github.com/nodejs/node/commit/7660eb591a)] - **doc**: fix typo in binding functions (Deokjin Kim) [#48003](https://github.com/nodejs/node/pull/48003)
* \[[`2f5dbca690`](https://github.com/nodejs/node/commit/2f5dbca690)] - **doc**: mark Node.js 14 as End-of-Life (Richard Lau) [#48023](https://github.com/nodejs/node/pull/48023)
* \[[`3b94a739f2`](https://github.com/nodejs/node/commit/3b94a739f2)] - **doc**: clarify CRYPTO\_CUSTOM\_ENGINE\_NOT\_SUPPORTED (Tobias Nießen) [#47976](https://github.com/nodejs/node/pull/47976)
* \[[`9e381cfa89`](https://github.com/nodejs/node/commit/9e381cfa89)] - **doc**: add heading for permission model limitations (Tobias Nießen) [#47989](https://github.com/nodejs/node/pull/47989)
* \[[`802db923e0`](https://github.com/nodejs/node/commit/802db923e0)] - **doc,vm**: clarify usage of cachedData in vm.compileFunction() (Darshan Sen) [#48193](https://github.com/nodejs/node/pull/48193)
* \[[`11a3434810`](https://github.com/nodejs/node/commit/11a3434810)] - **esm**: remove support for arrays in `import` internal method (Antoine du Hamel) [#48296](https://github.com/nodejs/node/pull/48296)
* \[[`3b00f3afef`](https://github.com/nodejs/node/commit/3b00f3afef)] - **esm**: handle `globalPreload` hook returning a nullish value (Antoine du Hamel) [#48249](https://github.com/nodejs/node/pull/48249)
* \[[`3c7846d7e1`](https://github.com/nodejs/node/commit/3c7846d7e1)] - **esm**: handle more error types thrown from the loader thread (Antoine du Hamel) [#48247](https://github.com/nodejs/node/pull/48247)
* \[[`60ce2bcabc`](https://github.com/nodejs/node/commit/60ce2bcabc)] - **http**: send implicit headers on HEAD with no body (Matteo Collina) [#48108](https://github.com/nodejs/node/pull/48108)
* \[[`72de4e7170`](https://github.com/nodejs/node/commit/72de4e7170)] - **lib**: do not disable linter for entire files (Antoine du Hamel) [#48299](https://github.com/nodejs/node/pull/48299)
* \[[`10cc60fc91`](https://github.com/nodejs/node/commit/10cc60fc91)] - **lib**: use existing `isWindows` variable (sinkhaha) [#48134](https://github.com/nodejs/node/pull/48134)
* \[[`a90010aae9`](https://github.com/nodejs/node/commit/a90010aae9)] - **lib**: support FORCE\_COLOR for non TTY streams (Moshe Atlow) [#48034](https://github.com/nodejs/node/pull/48034)
* \[[`b1828b325e`](https://github.com/nodejs/node/commit/b1828b325e)] - **(SEMVER-MINOR)** **lib**: implement AbortSignal.any() (Chemi Atlow) [#47821](https://github.com/nodejs/node/pull/47821)
* \[[`8f1b86961f`](https://github.com/nodejs/node/commit/8f1b86961f)] - **meta**: bump github/codeql-action from 2.3.3 to 2.3.6 (dependabot\[bot]) [#48287](https://github.com/nodejs/node/pull/48287)
* \[[`1b87ccdf70`](https://github.com/nodejs/node/commit/1b87ccdf70)] - **meta**: bump actions/setup-python from 4.6.0 to 4.6.1 (dependabot\[bot]) [#48286](https://github.com/nodejs/node/pull/48286)
* \[[`10715aea26`](https://github.com/nodejs/node/commit/10715aea26)] - **meta**: bump codecov/codecov-action from 3.1.3 to 3.1.4 (dependabot\[bot]) [#48285](https://github.com/nodejs/node/pull/48285)
* \[[`79f73778ab`](https://github.com/nodejs/node/commit/79f73778ab)] - **meta**: remove dont-land-on-v14 auto labeling (Shrujal Shah) [#48031](https://github.com/nodejs/node/pull/48031)
* \[[`9c5711f3ea`](https://github.com/nodejs/node/commit/9c5711f3ea)] - **meta**: move one or more collaborators to emeritus (Node.js GitHub Bot) [#48010](https://github.com/nodejs/node/pull/48010)
* \[[`6d6bf3ee52`](https://github.com/nodejs/node/commit/6d6bf3ee52)] - **module**: reduce the number of URL initializations (Yagiz Nizipli) [#48272](https://github.com/nodejs/node/pull/48272)
* \[[`f380953103`](https://github.com/nodejs/node/commit/f380953103)] - **module**: change default resolver to not throw on unknown scheme (Gil Tayar) [#47824](https://github.com/nodejs/node/pull/47824)
* \[[`950185b0c0`](https://github.com/nodejs/node/commit/950185b0c0)] - **net**: fix address iteration with autoSelectFamily (Fedor Indutny) [#48258](https://github.com/nodejs/node/pull/48258)
* \[[`5ddca72e62`](https://github.com/nodejs/node/commit/5ddca72e62)] - **net**: fix family autoselection SSL connection handling (Paolo Insogna) [#48189](https://github.com/nodejs/node/pull/48189)
* \[[`750e53ca3c`](https://github.com/nodejs/node/commit/750e53ca3c)] - **net**: fix family autoselection timeout handling (Paolo Insogna) [#47860](https://github.com/nodejs/node/pull/47860)
* \[[`a94f87ed99`](https://github.com/nodejs/node/commit/a94f87ed99)] - **(SEMVER-MINOR)** **node-api**: define version 9 (Chengzhong Wu) [#48151](https://github.com/nodejs/node/pull/48151)
* \[[`e834979818`](https://github.com/nodejs/node/commit/e834979818)] - **node-api**: add status napi\_cannot\_run\_js (Gabriel Schulhof) [#47986](https://github.com/nodejs/node/pull/47986)
* \[[`eafe0c3ec6`](https://github.com/nodejs/node/commit/eafe0c3ec6)] - **node-api**: napi\_ref on all types is experimental (Vladimir Morozov) [#47975](https://github.com/nodejs/node/pull/47975)
* \[[`9a034746f5`](https://github.com/nodejs/node/commit/9a034746f5)] - **src**: add Realm document in the src README.md (Chengzhong Wu) [#47932](https://github.com/nodejs/node/pull/47932)
* \[[`b8f4070f71`](https://github.com/nodejs/node/commit/b8f4070f71)] - **src**: check node\_extra\_ca\_certs after openssl cfg (Raghu Saxena) [#48159](https://github.com/nodejs/node/pull/48159)
* \[[`0347a18056`](https://github.com/nodejs/node/commit/0347a18056)] - **src**: include missing header in node\_sea.h (Joyee Cheung) [#48152](https://github.com/nodejs/node/pull/48152)
* \[[`45c3782c20`](https://github.com/nodejs/node/commit/45c3782c20)] - **src**: remove INT\_MAX asserts in SecretKeyGenTraits (Tobias Nießen) [#48053](https://github.com/nodejs/node/pull/48053)
* \[[`b25e7045ad`](https://github.com/nodejs/node/commit/b25e7045ad)] - **src**: avoid prototype access in binding templates (Joyee Cheung) [#47913](https://github.com/nodejs/node/pull/47913)
* \[[`33aa373eec`](https://github.com/nodejs/node/commit/33aa373eec)] - **src**: use Blob{Des|S}erializer for SEA blobs (Joyee Cheung) [#47962](https://github.com/nodejs/node/pull/47962)
* \[[`9e2b13dfa7`](https://github.com/nodejs/node/commit/9e2b13dfa7)] - **stream**: deprecate asIndexedPairs (Chemi Atlow) [#48102](https://github.com/nodejs/node/pull/48102)
* \[[`96c323dee2`](https://github.com/nodejs/node/commit/96c323dee2)] - **test**: mark test-child-process-pipe-dataflow as flaky (Moshe Atlow) [#48334](https://github.com/nodejs/node/pull/48334)
* \[[`9875885357`](https://github.com/nodejs/node/commit/9875885357)] - **test**: adapt tests for OpenSSL 3.1 (OttoHollmann) [#47859](https://github.com/nodejs/node/pull/47859)
* \[[`3440d7c6bf`](https://github.com/nodejs/node/commit/3440d7c6bf)] - **test**: unflake test-vm-timeout-escape-nexttick (Santiago Gimeno) [#48078](https://github.com/nodejs/node/pull/48078)
* \[[`215b2bc72c`](https://github.com/nodejs/node/commit/215b2bc72c)] - **test**: fix zlib version regex (Luigi Pinca) [#48227](https://github.com/nodejs/node/pull/48227)
* \[[`e12ee59d26`](https://github.com/nodejs/node/commit/e12ee59d26)] - **test**: use lower security level in s\_client (Luigi Pinca) [#48192](https://github.com/nodejs/node/pull/48192)
* \[[`1dabc7390c`](https://github.com/nodejs/node/commit/1dabc7390c)] - _**Revert**_ "**test**: unskip negative-settimeout.any.js WPT" (Filip Skokan) [#48182](https://github.com/nodejs/node/pull/48182)
* \[[`c1c4796a86`](https://github.com/nodejs/node/commit/c1c4796a86)] - **test**: mark test\_cannot\_run\_js as flaky (Keyhan Vakil) [#48181](https://github.com/nodejs/node/pull/48181)
* \[[`8c49d74002`](https://github.com/nodejs/node/commit/8c49d74002)] - **test**: fix flaky test-runner-watch-mode (Moshe Atlow) [#48144](https://github.com/nodejs/node/pull/48144)
* \[[`6388766862`](https://github.com/nodejs/node/commit/6388766862)] - **test**: skip test-http-pipeline-flood on IBM i (Abdirahim Musse) [#48048](https://github.com/nodejs/node/pull/48048)
* \[[`8d2a3b1952`](https://github.com/nodejs/node/commit/8d2a3b1952)] - **test**: ignore helper files in WPTs (Filip Skokan) [#48079](https://github.com/nodejs/node/pull/48079)
* \[[`7a96d825fd`](https://github.com/nodejs/node/commit/7a96d825fd)] - **test**: move `test-cluster-primary-error` flaky test (Yagiz Nizipli) [#48039](https://github.com/nodejs/node/pull/48039)
* \[[`a80dd3a8b3`](https://github.com/nodejs/node/commit/a80dd3a8b3)] - **test**: fix suite signal (Benjamin Gruenbaum) [#47800](https://github.com/nodejs/node/pull/47800)
* \[[`a41cfd183f`](https://github.com/nodejs/node/commit/a41cfd183f)] - **test**: fix parsing test flags (Daeyeon Jeong) [#48012](https://github.com/nodejs/node/pull/48012)
* \[[`4d4e506f2b`](https://github.com/nodejs/node/commit/4d4e506f2b)] - **test,doc,sea**: run SEA tests on ppc64 (Darshan Sen) [#48111](https://github.com/nodejs/node/pull/48111)
* \[[`44411fc40c`](https://github.com/nodejs/node/commit/44411fc40c)] - **test\_runner**: apply `runOnly` on suites (Moshe Atlow) [#48279](https://github.com/nodejs/node/pull/48279)
* \[[`3f259b7a30`](https://github.com/nodejs/node/commit/3f259b7a30)] - **test\_runner**: emit `test:watch:drained` event (Moshe Atlow) [#48259](https://github.com/nodejs/node/pull/48259)
* \[[`c9f8e8c562`](https://github.com/nodejs/node/commit/c9f8e8c562)] - **test\_runner**: stop watch mode when abortSignal aborted (Moshe Atlow) [#48259](https://github.com/nodejs/node/pull/48259)
* \[[`f3268d64cb`](https://github.com/nodejs/node/commit/f3268d64cb)] - **test\_runner**: fix global after hook (Moshe Atlow) [#48231](https://github.com/nodejs/node/pull/48231)
* \[[`15336c3139`](https://github.com/nodejs/node/commit/15336c3139)] - **test\_runner**: remove redundant check from coverage (Colin Ihrig) [#48070](https://github.com/nodejs/node/pull/48070)
* \[[`750d3e8606`](https://github.com/nodejs/node/commit/750d3e8606)] - **test\_runner**: pass FORCE\_COLOR to child process (Moshe Atlow) [#48057](https://github.com/nodejs/node/pull/48057)
* \[[`3278542243`](https://github.com/nodejs/node/commit/3278542243)] - **test\_runner**: dont split lines on `test:stdout` (Moshe Atlow) [#48057](https://github.com/nodejs/node/pull/48057)
* \[[`027c531766`](https://github.com/nodejs/node/commit/027c531766)] - **test\_runner**: fix test deserialize edge cases (Moshe Atlow) [#48106](https://github.com/nodejs/node/pull/48106)
* \[[`2b797a6d39`](https://github.com/nodejs/node/commit/2b797a6d39)] - **test\_runner**: delegate stderr and stdout formatting to reporter (Shiba) [#48045](https://github.com/nodejs/node/pull/48045)
* \[[`23d310bee8`](https://github.com/nodejs/node/commit/23d310bee8)] - **test\_runner**: display dot report as wide as the terminal width (Raz Luvaton) [#48038](https://github.com/nodejs/node/pull/48038)
* \[[`fd2620dcf1`](https://github.com/nodejs/node/commit/fd2620dcf1)] - **tls**: reapply servername on happy eyeballs connect (Fedor Indutny) [#48255](https://github.com/nodejs/node/pull/48255)
* \[[`62f847d0b3`](https://github.com/nodejs/node/commit/62f847d0b3)] - **tools**: update rollup lint-md-dependencies (Node.js GitHub Bot) [#48329](https://github.com/nodejs/node/pull/48329)
* \[[`3e97826a66`](https://github.com/nodejs/node/commit/3e97826a66)] - _**Revert**_ "**tools**: open issue when update workflow fails" (Marco Ippolito) [#48312](https://github.com/nodejs/node/pull/48312)
* \[[`5f08bfe35f`](https://github.com/nodejs/node/commit/5f08bfe35f)] - **tools**: don't gitignore base64 config.h (Ben Noordhuis) [#48174](https://github.com/nodejs/node/pull/48174)
* \[[`ded0e2d755`](https://github.com/nodejs/node/commit/ded0e2d755)] - **tools**: update LICENSE and license-builder.sh (Santiago Gimeno) [#48078](https://github.com/nodejs/node/pull/48078)
* \[[`07aa264366`](https://github.com/nodejs/node/commit/07aa264366)] - **tools**: automate histogram update (Marco Ippolito) [#48171](https://github.com/nodejs/node/pull/48171)
* \[[`1416b75eaa`](https://github.com/nodejs/node/commit/1416b75eaa)] - **tools**: use shasum instead of sha256sum (Luigi Pinca) [#48229](https://github.com/nodejs/node/pull/48229)
* \[[`b81e9d9b7b`](https://github.com/nodejs/node/commit/b81e9d9b7b)] - **tools**: harmonize `dep_updaters` scripts (Antoine du Hamel) [#48201](https://github.com/nodejs/node/pull/48201)
* \[[`a60bc41e53`](https://github.com/nodejs/node/commit/a60bc41e53)] - **tools**: deps update authenticate github api request (Andrea Fassina) [#48200](https://github.com/nodejs/node/pull/48200)
* \[[`7478ed014e`](https://github.com/nodejs/node/commit/7478ed014e)] - **tools**: order dependency jobs alphabetically (Luca) [#48184](https://github.com/nodejs/node/pull/48184)
* \[[`568a705799`](https://github.com/nodejs/node/commit/568a705799)] - **tools**: refactor v8\_pch config (Michaël Zasso) [#47364](https://github.com/nodejs/node/pull/47364)
* \[[`801573ba46`](https://github.com/nodejs/node/commit/801573ba46)] - **tools**: log and verify sha256sum (Andrea Fassina) [#48088](https://github.com/nodejs/node/pull/48088)
* \[[`db62325e18`](https://github.com/nodejs/node/commit/db62325e18)] - **tools**: open issue when update workflow fails (Marco Ippolito) [#48018](https://github.com/nodejs/node/pull/48018)
* \[[`ad8a68856d`](https://github.com/nodejs/node/commit/ad8a68856d)] - **tools**: alphabetize CODEOWNERS (Rich Trott) [#48124](https://github.com/nodejs/node/pull/48124)
* \[[`4cf5a9edaf`](https://github.com/nodejs/node/commit/4cf5a9edaf)] - **tools**: use latest upstream commit for zlib updates (Andrea Fassina) [#48054](https://github.com/nodejs/node/pull/48054)
* \[[`8d93af381b`](https://github.com/nodejs/node/commit/8d93af381b)] - **tools**: add security-wg as dep updaters owner (Marco Ippolito) [#48113](https://github.com/nodejs/node/pull/48113)
* \[[`5325be1d99`](https://github.com/nodejs/node/commit/5325be1d99)] - **tools**: port js2c.py to C++ (Joyee Cheung) [#46997](https://github.com/nodejs/node/pull/46997)
* \[[`6c60d90277`](https://github.com/nodejs/node/commit/6c60d90277)] - **tools**: fix race condition when npm installing (Tobias Nießen) [#48101](https://github.com/nodejs/node/pull/48101)
* \[[`0ab840a58f`](https://github.com/nodejs/node/commit/0ab840a58f)] - **tools**: refloat 7 Node.js patches to cpplint.py (Rich Trott) [#48098](https://github.com/nodejs/node/pull/48098)
* \[[`a298193378`](https://github.com/nodejs/node/commit/a298193378)] - **tools**: update cpplint to 1.6.1 (Yagiz Nizipli) [#48098](https://github.com/nodejs/node/pull/48098)
* \[[`f6725751b7`](https://github.com/nodejs/node/commit/f6725751b7)] - **tools**: update eslint to 8.41.0 (Node.js GitHub Bot) [#48097](https://github.com/nodejs/node/pull/48097)
* \[[`6539361f4e`](https://github.com/nodejs/node/commit/6539361f4e)] - **tools**: update lint-md-dependencies (Node.js GitHub Bot) [#48096](https://github.com/nodejs/node/pull/48096)
* \[[`5d94dbb951`](https://github.com/nodejs/node/commit/5d94dbb951)] - **tools**: update doc to remark-parse\@10.0.2 (Node.js GitHub Bot) [#48095](https://github.com/nodejs/node/pull/48095)
* \[[`2226088048`](https://github.com/nodejs/node/commit/2226088048)] - **tools**: add debug logs (Marco Ippolito) [#48060](https://github.com/nodejs/node/pull/48060)
* \[[`0c8c383583`](https://github.com/nodejs/node/commit/0c8c383583)] - **tools**: fix zconf.h path (Luigi Pinca) [#48089](https://github.com/nodejs/node/pull/48089)
* \[[`6adaf4c648`](https://github.com/nodejs/node/commit/6adaf4c648)] - **tools**: update remark-preset-lint-node to 4.0.0 (Node.js GitHub Bot) [#47995](https://github.com/nodejs/node/pull/47995)
* \[[`92b3334231`](https://github.com/nodejs/node/commit/92b3334231)] - **url**: clean vertical alignment of docs (Robin Ury) [#48037](https://github.com/nodejs/node/pull/48037)
* \[[`ebb6536775`](https://github.com/nodejs/node/commit/ebb6536775)] - **url**: call `ada::can_parse` directly (Yagiz Nizipli) [#47919](https://github.com/nodejs/node/pull/47919)
* \[[`ed4514294a`](https://github.com/nodejs/node/commit/ed4514294a)] - **vm**: properly handle defining symbol props (Nicolas DUBIEN) [#47572](https://github.com/nodejs/node/pull/47572)

<a id="20.2.0"></a>

## 2023-05-16, Version 20.2.0 (Current), @targos

### Notable Changes

* \[[`c092df9094`](https://github.com/nodejs/node/commit/c092df9094)] - **doc**: add ovflowd to collaborators (Claudio Wunder) [#47844](https://github.com/nodejs/node/pull/47844)
* \[[`4197a9a5a0`](https://github.com/nodejs/node/commit/4197a9a5a0)] - **(SEMVER-MINOR)** **http**: prevent writing to the body when not allowed by HTTP spec (Gerrard Lindsay) [#47732](https://github.com/nodejs/node/pull/47732)
* \[[`c4596b9ce7`](https://github.com/nodejs/node/commit/c4596b9ce7)] - **(SEMVER-MINOR)** **sea**: add option to disable the experimental SEA warning (Darshan Sen) [#47588](https://github.com/nodejs/node/pull/47588)
* \[[`17befe008c`](https://github.com/nodejs/node/commit/17befe008c)] - **(SEMVER-MINOR)** **test\_runner**: add `skip`, `todo`, and `only` shorthands to `test` (Chemi Atlow) [#47909](https://github.com/nodejs/node/pull/47909)
* \[[`a0634d7f89`](https://github.com/nodejs/node/commit/a0634d7f89)] - **(SEMVER-MINOR)** **url**: add value argument to `URLSearchParams` `has` and `delete` methods (Sankalp Shubham) [#47885](https://github.com/nodejs/node/pull/47885)

### Commits

* \[[`456fca0d9c`](https://github.com/nodejs/node/commit/456fca0d9c)] - **bootstrap**: initialize per-isolate properties of bindings separately (Joyee Cheung) [#47768](https://github.com/nodejs/node/pull/47768)
* \[[`d6d12bf978`](https://github.com/nodejs/node/commit/d6d12bf978)] - **bootstrap**: log isolate data info in mksnapshot debug logs (Joyee Cheung) [#47768](https://github.com/nodejs/node/pull/47768)
* \[[`e457d89a1b`](https://github.com/nodejs/node/commit/e457d89a1b)] - **buffer**: combine checking range of sourceStart in `buf.copy` (Deokjin Kim) [#47758](https://github.com/nodejs/node/pull/47758)
* \[[`00668fcfb4`](https://github.com/nodejs/node/commit/00668fcfb4)] - **child\_process**: use signal.reason in child process abort (Debadree Chatterjee) [#47817](https://github.com/nodejs/node/pull/47817)
* \[[`d7993474ea`](https://github.com/nodejs/node/commit/d7993474ea)] - **crypto**: remove default encoding from scrypt (Tobias Nießen) [#47943](https://github.com/nodejs/node/pull/47943)
* \[[`09fb74a7cc`](https://github.com/nodejs/node/commit/09fb74a7cc)] - **crypto**: fix webcrypto private/secret import with empty usages (Filip Skokan) [#47877](https://github.com/nodejs/node/pull/47877)
* \[[`e9c6ee74f3`](https://github.com/nodejs/node/commit/e9c6ee74f3)] - **crypto**: remove default encoding from pbkdf2 (Tobias Nießen) [#47869](https://github.com/nodejs/node/pull/47869)
* \[[`b7f13a8679`](https://github.com/nodejs/node/commit/b7f13a8679)] - **deps**: update simdutf to 3.2.9 (Node.js GitHub Bot) [#47983](https://github.com/nodejs/node/pull/47983)
* \[[`b16f6da153`](https://github.com/nodejs/node/commit/b16f6da153)] - **deps**: V8: cherry-pick 5f025d1ca2ca (Michaël Zasso) [#47610](https://github.com/nodejs/node/pull/47610)
* \[[`99f8fcab45`](https://github.com/nodejs/node/commit/99f8fcab45)] - **deps**: V8: cherry-pick a8a11a87cb72 (Michaël Zasso) [#47610](https://github.com/nodejs/node/pull/47610)
* \[[`c2b14b4c78`](https://github.com/nodejs/node/commit/c2b14b4c78)] - **deps**: update ada to 2.4.0 (Node.js GitHub Bot) [#47922](https://github.com/nodejs/node/pull/47922)
* \[[`cad42e7a56`](https://github.com/nodejs/node/commit/cad42e7a56)] - **deps**: V8: cherry-pick 1b471b796022 (Lu Yahan) [#47399](https://github.com/nodejs/node/pull/47399)
* \[[`7b2f17ca59`](https://github.com/nodejs/node/commit/7b2f17ca59)] - **deps**: upgrade npm to 9.6.6 (npm team) [#47862](https://github.com/nodejs/node/pull/47862)
* \[[`d23b1af562`](https://github.com/nodejs/node/commit/d23b1af562)] - **deps**: update ada to 2.3.1 (Node.js GitHub Bot) [#47893](https://github.com/nodejs/node/pull/47893)
* \[[`72340c98fb`](https://github.com/nodejs/node/commit/72340c98fb)] - **dgram**: convert macro to template (Tobias Nießen) [#47891](https://github.com/nodejs/node/pull/47891)
* \[[`9be922892f`](https://github.com/nodejs/node/commit/9be922892f)] - **dns**: call `ada::idna::to_ascii` directly from c++ (Yagiz Nizipli) [#47920](https://github.com/nodejs/node/pull/47920)
* \[[`4a1e97156a`](https://github.com/nodejs/node/commit/4a1e97156a)] - **doc**: add missing deprecated blocks to cluster (Tobias Nießen) [#47981](https://github.com/nodejs/node/pull/47981)
* \[[`13118a19ee`](https://github.com/nodejs/node/commit/13118a19ee)] - **doc**: update description of global (Tobias Nießen) [#47969](https://github.com/nodejs/node/pull/47969)
* \[[`372796440b`](https://github.com/nodejs/node/commit/372796440b)] - **doc**: update measure memory rejection information (Yash Ladha) [#41639](https://github.com/nodejs/node/pull/41639)
* \[[`7ecc6740e4`](https://github.com/nodejs/node/commit/7ecc6740e4)] - **doc**: fix broken link to TC39 import attributes proposal (Rich Trott) [#47954](https://github.com/nodejs/node/pull/47954)
* \[[`b9771c95c7`](https://github.com/nodejs/node/commit/b9771c95c7)] - **doc**: fix broken link (Rich Trott) [#47953](https://github.com/nodejs/node/pull/47953)
* \[[`6f5ba92e61`](https://github.com/nodejs/node/commit/6f5ba92e61)] - **doc**: remove broken link (Rich Trott) [#47942](https://github.com/nodejs/node/pull/47942)
* \[[`c9ffc555f1`](https://github.com/nodejs/node/commit/c9ffc555f1)] - **doc**: document make lint-md-clean (Matteo Collina) [#47926](https://github.com/nodejs/node/pull/47926)
* \[[`7ed99e8ba5`](https://github.com/nodejs/node/commit/7ed99e8ba5)] - **doc**: mark global object as legacy (Mert Can Altın) [#47819](https://github.com/nodejs/node/pull/47819)
* \[[`bf39f2d252`](https://github.com/nodejs/node/commit/bf39f2d252)] - **doc**: ntfs junction points must link to directories (Ben Noordhuis) [#47907](https://github.com/nodejs/node/pull/47907)
* \[[`4dfc3890d8`](https://github.com/nodejs/node/commit/4dfc3890d8)] - **doc**: improve `permission.has` description (Daeyeon Jeong) [#47875](https://github.com/nodejs/node/pull/47875)
* \[[`93f1aa2856`](https://github.com/nodejs/node/commit/93f1aa2856)] - **doc**: fix params names (Dmitry Semigradsky) [#47853](https://github.com/nodejs/node/pull/47853)
* \[[`9a362aa2fb`](https://github.com/nodejs/node/commit/9a362aa2fb)] - **doc**: update supported version of FreeBSD to 12.4 (Michaël Zasso) [#47838](https://github.com/nodejs/node/pull/47838)
* \[[`89c70dc6e6`](https://github.com/nodejs/node/commit/89c70dc6e6)] - **doc**: add stability experimental to pm (Rafael Gonzaga) [#47890](https://github.com/nodejs/node/pull/47890)
* \[[`f96fb2eee7`](https://github.com/nodejs/node/commit/f96fb2eee7)] - **doc**: swap Matteo with Rafael in the stewards (Rafael Gonzaga) [#47841](https://github.com/nodejs/node/pull/47841)
* \[[`1666a146e3`](https://github.com/nodejs/node/commit/1666a146e3)] - **doc**: add valgrind suppression details (Kevin Eady) [#47760](https://github.com/nodejs/node/pull/47760)
* \[[`e53e8231ff`](https://github.com/nodejs/node/commit/e53e8231ff)] - **doc**: replace EOL versions in README (Tobias Nießen) [#47833](https://github.com/nodejs/node/pull/47833)
* \[[`c092df9094`](https://github.com/nodejs/node/commit/c092df9094)] - **doc**: add ovflowd to collaborators (Claudio Wunder) [#47844](https://github.com/nodejs/node/pull/47844)
* \[[`f7106765b3`](https://github.com/nodejs/node/commit/f7106765b3)] - **doc**: update BUILDING.md previous versions links (Tobias Nießen) [#47835](https://github.com/nodejs/node/pull/47835)
* \[[`811b43c215`](https://github.com/nodejs/node/commit/811b43c215)] - **doc,test**: update the v8.startupSnapshot doc and test the example (Joyee Cheung) [#47468](https://github.com/nodejs/node/pull/47468)
* \[[`1ec640ac70`](https://github.com/nodejs/node/commit/1ec640ac70)] - **esm**: do not use `'beforeExit'` on the main thread (Antoine du Hamel) [#47964](https://github.com/nodejs/node/pull/47964)
* \[[`106dc612d6`](https://github.com/nodejs/node/commit/106dc612d6)] - **fs**: make readdir recursive algorithm iterative (Ethan Arrowood) [#47650](https://github.com/nodejs/node/pull/47650)
* \[[`a0da2348a8`](https://github.com/nodejs/node/commit/a0da2348a8)] - **fs**: move fs\_use\_promises\_symbol to per-isolate symbols (Joyee Cheung) [#47768](https://github.com/nodejs/node/pull/47768)
* \[[`4197a9a5a0`](https://github.com/nodejs/node/commit/4197a9a5a0)] - **(SEMVER-MINOR)** **http**: prevent writing to the body when not allowed by HTTP spec (Gerrard Lindsay) [#47732](https://github.com/nodejs/node/pull/47732)
* \[[`a4d6543598`](https://github.com/nodejs/node/commit/a4d6543598)] - **http2**: improve nghttp2 error callback (Tobias Nießen) [#47840](https://github.com/nodejs/node/pull/47840)
* \[[`a4fed6c580`](https://github.com/nodejs/node/commit/a4fed6c580)] - **lib**: update comment (sinkhaha) [#47884](https://github.com/nodejs/node/pull/47884)
* \[[`fd8bec7b2b`](https://github.com/nodejs/node/commit/fd8bec7b2b)] - **meta**: bump step-security/harden-runner from 2.3.1 to 2.4.0 (Rich Trott) [#47980](https://github.com/nodejs/node/pull/47980)
* \[[`f5b4b6d5dc`](https://github.com/nodejs/node/commit/f5b4b6d5dc)] - **meta**: bump github/codeql-action from 2.3.2 to 2.3.3 (Rich Trott) [#47979](https://github.com/nodejs/node/pull/47979)
* \[[`c05c0a2359`](https://github.com/nodejs/node/commit/c05c0a2359)] - **meta**: bump actions/setup-python from 4.5.0 to 4.6.0 (Rich Trott) [#47968](https://github.com/nodejs/node/pull/47968)
* \[[`2a3d6d97cb`](https://github.com/nodejs/node/commit/2a3d6d97cb)] - **meta**: add security-wg ping to permission.js (Rafael Gonzaga) [#47941](https://github.com/nodejs/node/pull/47941)
* \[[`6c158e8dd1`](https://github.com/nodejs/node/commit/6c158e8dd1)] - **meta**: bump step-security/harden-runner from 2.2.1 to 2.3.1 (dependabot\[bot]) [#47808](https://github.com/nodejs/node/pull/47808)
* \[[`f7a8094d37`](https://github.com/nodejs/node/commit/f7a8094d37)] - **meta**: bump actions/setup-python from 4.5.0 to 4.6.0 (dependabot\[bot]) [#47806](https://github.com/nodejs/node/pull/47806)
* \[[`0f58e48792`](https://github.com/nodejs/node/commit/0f58e48792)] - **meta**: bump actions/checkout from 3.3.0 to 3.5.2 (dependabot\[bot]) [#47805](https://github.com/nodejs/node/pull/47805)
* \[[`652b06dd82`](https://github.com/nodejs/node/commit/652b06dd82)] - **meta**: remove extra space in scorecard workflow (Mestery) [#47805](https://github.com/nodejs/node/pull/47805)
* \[[`9f06eaccaf`](https://github.com/nodejs/node/commit/9f06eaccaf)] - **meta**: bump github/codeql-action from 2.2.9 to 2.3.2 (dependabot\[bot]) [#47809](https://github.com/nodejs/node/pull/47809)
* \[[`977fd7cf35`](https://github.com/nodejs/node/commit/977fd7cf35)] - **meta**: bump codecov/codecov-action from 3.1.1 to 3.1.3 (dependabot\[bot]) [#47807](https://github.com/nodejs/node/pull/47807)
* \[[`c19385c154`](https://github.com/nodejs/node/commit/c19385c154)] - **module**: refactor to use `normalizeRequirableId` in the CJS module loader (Darshan Sen) [#47896](https://github.com/nodejs/node/pull/47896)
* \[[`739113f2fc`](https://github.com/nodejs/node/commit/739113f2fc)] - **module**: block requiring `test/reporters` without scheme (Moshe Atlow) [#47831](https://github.com/nodejs/node/pull/47831)
* \[[`f489c6710c`](https://github.com/nodejs/node/commit/f489c6710c)] - **(NODE-API-SEMVER-MAJOR)** **node-api**: get Node API version used by addon (Vladimir Morozov) [#45715](https://github.com/nodejs/node/pull/45715)
* \[[`7222f9d74b`](https://github.com/nodejs/node/commit/7222f9d74b)] - **path**: indicate index of wrong resolve() parameter (sosoba) [#47660](https://github.com/nodejs/node/pull/47660)
* \[[`7dd32f1536`](https://github.com/nodejs/node/commit/7dd32f1536)] - **permission**: remove unused function declaration (Deokjin Kim) [#47957](https://github.com/nodejs/node/pull/47957)
* \[[`af86625a05`](https://github.com/nodejs/node/commit/af86625a05)] - **permission**: resolve reference to absolute path only for fs permission (Daeyeon Jeong) [#47930](https://github.com/nodejs/node/pull/47930)
* \[[`1625ae11fe`](https://github.com/nodejs/node/commit/1625ae11fe)] - **quic**: address recent coverity warning (Michael Dawson) [#47753](https://github.com/nodejs/node/pull/47753)
* \[[`c4596b9ce7`](https://github.com/nodejs/node/commit/c4596b9ce7)] - **(SEMVER-MINOR)** **sea**: add option to disable the experimental SEA warning (Darshan Sen) [#47588](https://github.com/nodejs/node/pull/47588)
* \[[`1a7fc186bc`](https://github.com/nodejs/node/commit/1a7fc186bc)] - **sea**: allow requiring core modules with the "node:" prefix (Darshan Sen) [#47779](https://github.com/nodejs/node/pull/47779)
* \[[`786a1c5398`](https://github.com/nodejs/node/commit/786a1c5398)] - **src**: deduplicate X509Certificate::Fingerprint\* (Tobias Nießen) [#47978](https://github.com/nodejs/node/pull/47978)
* \[[`060c1d502b`](https://github.com/nodejs/node/commit/060c1d502b)] - **src**: stop copying code cache, part 2 (Keyhan Vakil) [#47958](https://github.com/nodejs/node/pull/47958)
* \[[`1aec718619`](https://github.com/nodejs/node/commit/1aec718619)] - **(SEMVER-MINOR)** **src**: add cjs\_module\_lexer\_version base64\_version (Jithil P Ponnan) [#45629](https://github.com/nodejs/node/pull/45629)
* \[[`0c06bfd8dc`](https://github.com/nodejs/node/commit/0c06bfd8dc)] - **src**: move BlobSerializerDeserializer to a separate header file (Darshan Sen) [#47933](https://github.com/nodejs/node/pull/47933)
* \[[`bd553e7521`](https://github.com/nodejs/node/commit/bd553e7521)] - **src**: rename SKIP\_CHECK\_SIZE to SKIP\_CHECK\_STRLEN (Tobias Nießen) [#47845](https://github.com/nodejs/node/pull/47845)
* \[[`190596c189`](https://github.com/nodejs/node/commit/190596c189)] - **src**: register external references for source code (Keyhan Vakil) [#47055](https://github.com/nodejs/node/pull/47055)
* \[[`4293cc47f4`](https://github.com/nodejs/node/commit/4293cc47f4)] - **src**: support V8 experimental shared values in messaging (Shu-yu Guo) [#47706](https://github.com/nodejs/node/pull/47706)
* \[[`9bc5d78f0c`](https://github.com/nodejs/node/commit/9bc5d78f0c)] - **src**: register ext reference for Fingerprint512 (Tobias Nießen) [#47892](https://github.com/nodejs/node/pull/47892)
* \[[`a11507e23b`](https://github.com/nodejs/node/commit/a11507e23b)] - **src**: stop copying code cache (Keyhan Vakil) [#47144](https://github.com/nodejs/node/pull/47144)
* \[[`515c9b8de6`](https://github.com/nodejs/node/commit/515c9b8de6)] - **src**: clarify the parameter name in `Permission::Apply` (Daeyeon Jeong) [#47874](https://github.com/nodejs/node/pull/47874)
* \[[`c4217613f5`](https://github.com/nodejs/node/commit/c4217613f5)] - **src**: fix creating an ArrayBuffer from a Blob created with `openAsBlob` (Daeyeon Jeong) [#47691](https://github.com/nodejs/node/pull/47691)
* \[[`4bc17fd67b`](https://github.com/nodejs/node/commit/4bc17fd67b)] - **src**: avoid strcmp() with Utf8Value (Tobias Nießen) [#47827](https://github.com/nodejs/node/pull/47827)
* \[[`d358317f70`](https://github.com/nodejs/node/commit/d358317f70)] - **src**: get binding data store directly from the realm (Joyee Cheung) [#47437](https://github.com/nodejs/node/pull/47437)
* \[[`b04d51a0b5`](https://github.com/nodejs/node/commit/b04d51a0b5)] - **src**: prefer data accessor of string and vector (Mohammed Keyvanzadeh) [#47750](https://github.com/nodejs/node/pull/47750)
* \[[`2952cc576c`](https://github.com/nodejs/node/commit/2952cc576c)] - **src**: add per-isolate SetFastMethod and Set\[Fast]MethodNoSideEffect (Joyee Cheung) [#47768](https://github.com/nodejs/node/pull/47768)
* \[[`010d2ecf94`](https://github.com/nodejs/node/commit/010d2ecf94)] - **test**: mark test-esm-loader-http-imports as flaky (Tobias Nießen) [#47987](https://github.com/nodejs/node/pull/47987)
* \[[`bb33c74c07`](https://github.com/nodejs/node/commit/bb33c74c07)] - **test**: add getRandomValues return length (Jithil P Ponnan) [#46357](https://github.com/nodejs/node/pull/46357)
* \[[`6e019586f7`](https://github.com/nodejs/node/commit/6e019586f7)] - **test**: unskip negative-settimeout.any.js WPT (Filip Skokan) [#47946](https://github.com/nodejs/node/pull/47946)
* \[[`8f547afe5f`](https://github.com/nodejs/node/commit/8f547afe5f)] - **test**: use appropriate usages for a negative import test (Filip Skokan) [#47878](https://github.com/nodejs/node/pull/47878)
* \[[`7e34f77518`](https://github.com/nodejs/node/commit/7e34f77518)] - **test**: fix webcrypto wrap unwrap tests (Filip Skokan) [#47876](https://github.com/nodejs/node/pull/47876)
* \[[`30f4f35244`](https://github.com/nodejs/node/commit/30f4f35244)] - **test**: fix output tests when path includes node version (Moshe Atlow) [#47843](https://github.com/nodejs/node/pull/47843)
* \[[`54607bfd68`](https://github.com/nodejs/node/commit/54607bfd68)] - **test**: reduce WPT concurrency (Filip Skokan) [#47834](https://github.com/nodejs/node/pull/47834)
* \[[`17945a2495`](https://github.com/nodejs/node/commit/17945a2495)] - **test**: migrate a pseudo\_tty test to use assertSnapshot (Moshe Atlow) [#47803](https://github.com/nodejs/node/pull/47803)
* \[[`c9233679e8`](https://github.com/nodejs/node/commit/c9233679e8)] - **test**: fix WPT state when process exits but workers are still running (Filip Skokan) [#47826](https://github.com/nodejs/node/pull/47826)
* \[[`34bfb69b5b`](https://github.com/nodejs/node/commit/34bfb69b5b)] - **test**: migrate message tests to use assertSnapshot (Moshe Atlow) [#47498](https://github.com/nodejs/node/pull/47498)
* \[[`d25c785c2a`](https://github.com/nodejs/node/commit/d25c785c2a)] - **test**: allow SIGBUS in signal-handler abort test (Michaël Zasso) [#47851](https://github.com/nodejs/node/pull/47851)
* \[[`aa2c7e00d7`](https://github.com/nodejs/node/commit/aa2c7e00d7)] - **test,crypto**: update WebCryptoAPI WPT (Filip Skokan) [#47921](https://github.com/nodejs/node/pull/47921)
* \[[`da27542058`](https://github.com/nodejs/node/commit/da27542058)] - **test\_runner**: use v8.serialize instead of TAP (Moshe Atlow) [#47867](https://github.com/nodejs/node/pull/47867)
* \[[`17befe008c`](https://github.com/nodejs/node/commit/17befe008c)] - **(SEMVER-MINOR)** **test\_runner**: add shorthands to `test` (Chemi Atlow) [#47909](https://github.com/nodejs/node/pull/47909)
* \[[`42db1d50a0`](https://github.com/nodejs/node/commit/42db1d50a0)] - **test\_runner**: fix ordering of test hooks (Phil Nash) [#47931](https://github.com/nodejs/node/pull/47931)
* \[[`d81c54e3a8`](https://github.com/nodejs/node/commit/d81c54e3a8)] - **test\_runner**: omit inaccessible files from coverage (Colin Ihrig) [#47850](https://github.com/nodejs/node/pull/47850)
* \[[`a4e261e910`](https://github.com/nodejs/node/commit/a4e261e910)] - **tools**: debug log for nghttp3 (Marco Ippolito) [#47992](https://github.com/nodejs/node/pull/47992)
* \[[`f6ff318d4c`](https://github.com/nodejs/node/commit/f6ff318d4c)] - **tools**: automate icu-small update (Marco Ippolito) [#47727](https://github.com/nodejs/node/pull/47727)
* \[[`706c305381`](https://github.com/nodejs/node/commit/706c305381)] - **tools**: update lint-md-dependencies to rollup\@3.21.5 (Node.js GitHub Bot) [#47903](https://github.com/nodejs/node/pull/47903)
* \[[`e22c686ca9`](https://github.com/nodejs/node/commit/e22c686ca9)] - **tools**: update eslint to 8.40.0 (Node.js GitHub Bot) [#47906](https://github.com/nodejs/node/pull/47906)
* \[[`36f7cfac93`](https://github.com/nodejs/node/commit/36f7cfac93)] - **tools**: update eslint to 8.39.0 (Node.js GitHub Bot) [#47789](https://github.com/nodejs/node/pull/47789)
* \[[`7323902a40`](https://github.com/nodejs/node/commit/7323902a40)] - **tools**: fix jsdoc lint (Moshe Atlow) [#47789](https://github.com/nodejs/node/pull/47789)
* \[[`a0634d7f89`](https://github.com/nodejs/node/commit/a0634d7f89)] - **(SEMVER-MINOR)** **url**: add value argument to has and delete methods (Sankalp Shubham) [#47885](https://github.com/nodejs/node/pull/47885)
* \[[`1b06c1e003`](https://github.com/nodejs/node/commit/1b06c1e003)] - **url**: improve `isURL` detection (Yagiz Nizipli) [#47886](https://github.com/nodejs/node/pull/47886)
* \[[`2bd869d20c`](https://github.com/nodejs/node/commit/2bd869d20c)] - **vm**: fix crash when setting \_\_proto\_\_ on context's globalThis (Feng Yu) [#47939](https://github.com/nodejs/node/pull/47939)
* \[[`e6685f9e82`](https://github.com/nodejs/node/commit/e6685f9e82)] - **vm,lib**: refactor microtaskQueue assignment logic (Khaidi Chu) [#47765](https://github.com/nodejs/node/pull/47765)
* \[[`47fea13dac`](https://github.com/nodejs/node/commit/47fea13dac)] - **worker**: support more cases when (de)serializing errors (Moshe Atlow) [#47925](https://github.com/nodejs/node/pull/47925)
* \[[`6f3876c035`](https://github.com/nodejs/node/commit/6f3876c035)] - **worker**: use snapshot in workers spawned by workers (Joyee Cheung) [#47731](https://github.com/nodejs/node/pull/47731)

<a id="20.1.0"></a>

## 2023-05-03, Version 20.1.0 (Current), @targos

### Notable Changes

* \[[`5e99598639`](https://github.com/nodejs/node/commit/5e99598639)] - **assert**: deprecate `CallTracker` (Moshe Atlow) [#47740](https://github.com/nodejs/node/pull/47740)
* \[[`2d97c89c6f`](https://github.com/nodejs/node/commit/2d97c89c6f)] - **crypto**: update root certificates to NSS 3.89 (Node.js GitHub Bot) [#47659](https://github.com/nodejs/node/pull/47659)
* \[[`ce8820e292`](https://github.com/nodejs/node/commit/ce8820e292)] - **(SEMVER-MINOR)** **dns**: expose `getDefaultResultOrder` (btea) [#46973](https://github.com/nodejs/node/pull/46973)
* \[[`9d30f469aa`](https://github.com/nodejs/node/commit/9d30f469aa)] - **doc**: add KhafraDev to collaborators (Matthew Aitken) [#47510](https://github.com/nodejs/node/pull/47510)
* \[[`439ea47a77`](https://github.com/nodejs/node/commit/439ea47a77)] - **(SEMVER-MINOR)** **fs**: add `recursive` option to `readdir` and `opendir` (Ethan Arrowood) [#41439](https://github.com/nodejs/node/pull/41439)
* \[[`a54e898dc8`](https://github.com/nodejs/node/commit/a54e898dc8)] - **(SEMVER-MINOR)** **fs**: add support for `mode` flag to specify the copy behavior of the `cp` methods (Tetsuharu Ohzeki) [#47084](https://github.com/nodejs/node/pull/47084)
* \[[`4fa773964b`](https://github.com/nodejs/node/commit/4fa773964b)] - **(SEMVER-MINOR)** **http**: add `highWaterMark` option `http.createServer` (HinataKah0) [#47405](https://github.com/nodejs/node/pull/47405)
* \[[`2b411f4b42`](https://github.com/nodejs/node/commit/2b411f4b42)] - **(SEMVER-MINOR)** **stream**: preserve object mode in `compose` (Raz Luvaton) [#47413](https://github.com/nodejs/node/pull/47413)
* \[[`5327483f31`](https://github.com/nodejs/node/commit/5327483f31)] - **(SEMVER-MINOR)** **test\_runner**: add `testNamePatterns` to `run` API (Chemi Atlow) [#47628](https://github.com/nodejs/node/pull/47628)
* \[[`bdd02a467d`](https://github.com/nodejs/node/commit/bdd02a467d)] - **(SEMVER-MINOR)** **test\_runner**: execute `before` hook on test (Chemi Atlow) [#47586](https://github.com/nodejs/node/pull/47586)
* \[[`0e70c187bc`](https://github.com/nodejs/node/commit/0e70c187bc)] - **(SEMVER-MINOR)** **test\_runner**: support combining coverage reports (Colin Ihrig) [#47686](https://github.com/nodejs/node/pull/47686)
* \[[`75c1d1b66e`](https://github.com/nodejs/node/commit/75c1d1b66e)] - **(SEMVER-MINOR)** **wasi**: make `returnOnExit` true by default (Michael Dawson) [#47390](https://github.com/nodejs/node/pull/47390)

### Commits

* \[[`33d1bd3e02`](https://github.com/nodejs/node/commit/33d1bd3e02)] - **assert**: deprecate callTracker (Moshe Atlow) [#47740](https://github.com/nodejs/node/pull/47740)
* \[[`6d87355e83`](https://github.com/nodejs/node/commit/6d87355e83)] - **benchmark**: add eventtarget creation bench (Rafael Gonzaga) [#47774](https://github.com/nodejs/node/pull/47774)
* \[[`40324a1dea`](https://github.com/nodejs/node/commit/40324a1dea)] - **benchmark**: differentiate whatwg and legacy url (Yagiz Nizipli) [#47377](https://github.com/nodejs/node/pull/47377)
* \[[`936d7cb069`](https://github.com/nodejs/node/commit/936d7cb069)] - **benchmark**: add a benchmark for `defaultResolve` (Antoine du Hamel) [#47543](https://github.com/nodejs/node/pull/47543)
* \[[`202042ee93`](https://github.com/nodejs/node/commit/202042ee93)] - **bootstrap**: support namespaced builtins in snapshot scripts (Joyee Cheung) [#47467](https://github.com/nodejs/node/pull/47467)
* \[[`30af5cee55`](https://github.com/nodejs/node/commit/30af5cee55)] - **build**: use pathlib for paths (Mohammed Keyvanzadeh) [#47581](https://github.com/nodejs/node/pull/47581)
* \[[`089c9c51e9`](https://github.com/nodejs/node/commit/089c9c51e9)] - **build**: refactor configure.py (Mohammed Keyvanzadeh) [#47667](https://github.com/nodejs/node/pull/47667)
* \[[`5b851c8074`](https://github.com/nodejs/node/commit/5b851c8074)] - **build**: add devcontainer configuration (Tierney Cyren) [#40825](https://github.com/nodejs/node/pull/40825)
* \[[`35e8b3b467`](https://github.com/nodejs/node/commit/35e8b3b467)] - **build**: bump ossf/scorecard-action from 2.1.2 to 2.1.3 (dependabot\[bot]) [#47367](https://github.com/nodejs/node/pull/47367)
* \[[`78c08243df`](https://github.com/nodejs/node/commit/78c08243df)] - **build**: replace Python linter flake8 with ruff (Christian Clauss) [#47519](https://github.com/nodejs/node/pull/47519)
* \[[`2d97c89c6f`](https://github.com/nodejs/node/commit/2d97c89c6f)] - **crypto**: update root certificates to NSS 3.89 (Node.js GitHub Bot) [#47659](https://github.com/nodejs/node/pull/47659)
* \[[`420feb41cf`](https://github.com/nodejs/node/commit/420feb41cf)] - **crypto**: remove INT\_MAX restriction in randomBytes (Tobias Nießen) [#47559](https://github.com/nodejs/node/pull/47559)
* \[[`6046779dd9`](https://github.com/nodejs/node/commit/6046779dd9)] - **deps**: disable V8 concurrent sparkplug compilation (Michaël Zasso) [#47450](https://github.com/nodejs/node/pull/47450)
* \[[`00d461e93f`](https://github.com/nodejs/node/commit/00d461e93f)] - **deps**: V8: cherry-pick c5ab3e4f0c5a (Richard Lau) [#47736](https://github.com/nodejs/node/pull/47736)
* \[[`d08dd8069f`](https://github.com/nodejs/node/commit/d08dd8069f)] - **deps**: update ada to 2.3.0 (Node.js GitHub Bot) [#47737](https://github.com/nodejs/node/pull/47737)
* \[[`996245976b`](https://github.com/nodejs/node/commit/996245976b)] - **deps**: update undici to 5.22.0 (Node.js GitHub Bot) [#47679](https://github.com/nodejs/node/pull/47679)
* \[[`f3ee3126df`](https://github.com/nodejs/node/commit/f3ee3126df)] - **deps**: update ada to 2.2.0 (Node.js GitHub Bot) [#47678](https://github.com/nodejs/node/pull/47678)
* \[[`1391d3b9ff`](https://github.com/nodejs/node/commit/1391d3b9ff)] - **deps**: add minimatch as a dependency (Moshe Atlow) [#47499](https://github.com/nodejs/node/pull/47499)
* \[[`315454350d`](https://github.com/nodejs/node/commit/315454350d)] - **deps**: update ada to 2.1.0 (Node.js GitHub Bot) [#47598](https://github.com/nodejs/node/pull/47598)
* \[[`7f7735cad9`](https://github.com/nodejs/node/commit/7f7735cad9)] - **deps**: update ICU to 73.1 release (Steven R. Loomis) [#47456](https://github.com/nodejs/node/pull/47456)
* \[[`13105c12b7`](https://github.com/nodejs/node/commit/13105c12b7)] - **deps**: patch V8 to 11.3.244.8 (Michaël Zasso) [#47536](https://github.com/nodejs/node/pull/47536)
* \[[`ede69d272a`](https://github.com/nodejs/node/commit/ede69d272a)] - **deps**: update undici to 5.21.2 (Node.js GitHub Bot) [#47508](https://github.com/nodejs/node/pull/47508)
* \[[`64b5a5f872`](https://github.com/nodejs/node/commit/64b5a5f872)] - **deps**: update simdutf to 3.2.8 (Node.js GitHub Bot) [#47507](https://github.com/nodejs/node/pull/47507)
* \[[`2664536796`](https://github.com/nodejs/node/commit/2664536796)] - **deps**: V8: cherry-pick 8e10685ff918 (Jiawen Geng) [#47440](https://github.com/nodejs/node/pull/47440)
* \[[`ba9ec91f0e`](https://github.com/nodejs/node/commit/ba9ec91f0e)] - **deps**: update undici to 5.21.1 (Node.js GitHub Bot) [#47488](https://github.com/nodejs/node/pull/47488)
* \[[`ce8820e292`](https://github.com/nodejs/node/commit/ce8820e292)] - **(SEMVER-MINOR)** **dns**: expose getDefaultResultOrder (btea) [#46973](https://github.com/nodejs/node/pull/46973)
* \[[`4c26e28c33`](https://github.com/nodejs/node/commit/4c26e28c33)] - **doc**: create maintaining folder for deps (Marco Ippolito) [#47589](https://github.com/nodejs/node/pull/47589)
* \[[`aa0ef3eabd`](https://github.com/nodejs/node/commit/aa0ef3eabd)] - **doc**: fix --allow-\* CLI flag references (Tobias Nießen) [#47804](https://github.com/nodejs/node/pull/47804)
* \[[`98603b6fd3`](https://github.com/nodejs/node/commit/98603b6fd3)] - **doc**: clarify fs permissions only affect fs module (Tobias Nießen) [#47782](https://github.com/nodejs/node/pull/47782)
* \[[`3befe5dac9`](https://github.com/nodejs/node/commit/3befe5dac9)] - **doc**: add copy node executable guide on windows (XLor) [#47781](https://github.com/nodejs/node/pull/47781)
* \[[`98450d9892`](https://github.com/nodejs/node/commit/98450d9892)] - **doc**: remove MoLow from Triagers (Moshe Atlow) [#47792](https://github.com/nodejs/node/pull/47792)
* \[[`d75036410d`](https://github.com/nodejs/node/commit/d75036410d)] - **doc**: fix typo in webstreams.md (Christian Takle) [#47766](https://github.com/nodejs/node/pull/47766)
* \[[`ceba37a74f`](https://github.com/nodejs/node/commit/ceba37a74f)] - **doc**: move BethGriggs to regular member (Rich Trott) [#47776](https://github.com/nodejs/node/pull/47776)
* \[[`b954ea9781`](https://github.com/nodejs/node/commit/b954ea9781)] - **doc**: mark signing the binary is macOS and Windows only in SEA (Xuguang Mei) [#47722](https://github.com/nodejs/node/pull/47722)
* \[[`26bccbcd10`](https://github.com/nodejs/node/commit/26bccbcd10)] - **doc**: move addaleax to TSC emeriti (Anna Henningsen) [#47752](https://github.com/nodejs/node/pull/47752)
* \[[`20b0de242f`](https://github.com/nodejs/node/commit/20b0de242f)] - **doc**: add link to news for Node.js core (Michael Dawson) [#47704](https://github.com/nodejs/node/pull/47704)
* \[[`5709133dc7`](https://github.com/nodejs/node/commit/5709133dc7)] - **doc**: fix a typo in `permissions.md` (Daeyeon Jeong) [#47730](https://github.com/nodejs/node/pull/47730)
* \[[`c5c40a89f2`](https://github.com/nodejs/node/commit/c5c40a89f2)] - **doc**: async\_hooks asynchronous content example add mjs code (btea) [#47401](https://github.com/nodejs/node/pull/47401)
* \[[`a1403a8df2`](https://github.com/nodejs/node/commit/a1403a8df2)] - **doc**: clarify concurrency model of test runner (Tobias Nießen) [#47642](https://github.com/nodejs/node/pull/47642)
* \[[`c0c23fbe42`](https://github.com/nodejs/node/commit/c0c23fbe42)] - **doc**: fix a typo in `fs.openAsBlob` (Daeyeon Jeong) [#47693](https://github.com/nodejs/node/pull/47693)
* \[[`4cef98812d`](https://github.com/nodejs/node/commit/4cef98812d)] - **doc**: fix typos (Mohammed Keyvanzadeh) [#47685](https://github.com/nodejs/node/pull/47685)
* \[[`f30ef242ef`](https://github.com/nodejs/node/commit/f30ef242ef)] - **doc**: fix capitalization of ASan (Mohammed Keyvanzadeh) [#47676](https://github.com/nodejs/node/pull/47676)
* \[[`78a3503406`](https://github.com/nodejs/node/commit/78a3503406)] - **doc**: fix typos in SECURITY.md (Mohammed Keyvanzadeh) [#47677](https://github.com/nodejs/node/pull/47677)
* \[[`9101630e05`](https://github.com/nodejs/node/commit/9101630e05)] - **doc**: update error code of buffer (Deokjin Kim) [#47617](https://github.com/nodejs/node/pull/47617)
* \[[`183f0c3e79`](https://github.com/nodejs/node/commit/183f0c3e79)] - **doc**: change offset of example in `Buffer.copyBytesFrom` (Deokjin Kim) [#47606](https://github.com/nodejs/node/pull/47606)
* \[[`d11ff4bc53`](https://github.com/nodejs/node/commit/d11ff4bc53)] - **doc**: improve fs permissions description (Tobias Nießen) [#47596](https://github.com/nodejs/node/pull/47596)
* \[[`b58920c3a9`](https://github.com/nodejs/node/commit/b58920c3a9)] - **doc**: remove markdown link from heading (Tobias Nießen) [#47585](https://github.com/nodejs/node/pull/47585)
* \[[`c36634e880`](https://github.com/nodejs/node/commit/c36634e880)] - **doc**: fix history ordering of `WASI` constructor (Antoine du Hamel) [#47611](https://github.com/nodejs/node/pull/47611)
* \[[`d3fadd889d`](https://github.com/nodejs/node/commit/d3fadd889d)] - **doc**: fix release-post script location (Rafael Gonzaga) [#47517](https://github.com/nodejs/node/pull/47517)
* \[[`2a0bbe7883`](https://github.com/nodejs/node/commit/2a0bbe7883)] - **doc**: fix typo in webcrypto metadata (Tobias Nießen) [#47595](https://github.com/nodejs/node/pull/47595)
* \[[`b0b16ee9f6`](https://github.com/nodejs/node/commit/b0b16ee9f6)] - **doc**: add link for news from uvwasi team (Michael Dawson) [#47531](https://github.com/nodejs/node/pull/47531)
* \[[`7ca416af15`](https://github.com/nodejs/node/commit/7ca416af15)] - **doc**: add missing setEncoding call in ESM example (Anna Henningsen) [#47558](https://github.com/nodejs/node/pull/47558)
* \[[`f9abd59b41`](https://github.com/nodejs/node/commit/f9abd59b41)] - **doc**: update darwin-x64 toolchain used for Node.js 20 releases (Michaël Zasso) [#47546](https://github.com/nodejs/node/pull/47546)
* \[[`0dc508070f`](https://github.com/nodejs/node/commit/0dc508070f)] - **doc**: fix split infinitive in Hooks caveat (Jacob Smith) [#47550](https://github.com/nodejs/node/pull/47550)
* \[[`4046280475`](https://github.com/nodejs/node/commit/4046280475)] - **doc**: fix typo in util.types.isNativeError() (Julian Dax) [#47532](https://github.com/nodejs/node/pull/47532)
* \[[`9d30f469aa`](https://github.com/nodejs/node/commit/9d30f469aa)] - **doc**: add KhafraDev to collaborators (Matthew Aitken) [#47510](https://github.com/nodejs/node/pull/47510)
* \[[`537c17ec48`](https://github.com/nodejs/node/commit/537c17ec48)] - **doc**: create maintaining-brotli.md (Marco Ippolito) [#47380](https://github.com/nodejs/node/pull/47380)
* \[[`09ff9eafd9`](https://github.com/nodejs/node/commit/09ff9eafd9)] - **doc,fs**: update description of fs.stat() method (Mert Can Altın) [#47654](https://github.com/nodejs/node/pull/47654)
* \[[`185d6090cd`](https://github.com/nodejs/node/commit/185d6090cd)] - **doc,test**: fix concurrency option of test() (Tobias Nießen) [#47734](https://github.com/nodejs/node/pull/47734)
* \[[`a793cf401d`](https://github.com/nodejs/node/commit/a793cf401d)] - **esm**: rename `URLCanParse` to be consistent (Antoine du Hamel) [#47668](https://github.com/nodejs/node/pull/47668)
* \[[`fbb6b72f87`](https://github.com/nodejs/node/commit/fbb6b72f87)] - **esm**: remove support for deprecated hooks (Antoine du Hamel) [#47580](https://github.com/nodejs/node/pull/47580)
* \[[`c150976c4f`](https://github.com/nodejs/node/commit/c150976c4f)] - **esm**: initialize `import.meta` on eval (Antoine du Hamel) [#47551](https://github.com/nodejs/node/pull/47551)
* \[[`55f70f6395`](https://github.com/nodejs/node/commit/55f70f6395)] - **esm**: propagate `process.exit` from the loader thread to the main thread (Antoine du Hamel) [#47548](https://github.com/nodejs/node/pull/47548)
* \[[`269482f61f`](https://github.com/nodejs/node/commit/269482f61f)] - **esm**: avoid accessing lazy getters for urls (Yagiz Nizipli) [#47542](https://github.com/nodejs/node/pull/47542)
* \[[`889add68e5`](https://github.com/nodejs/node/commit/889add68e5)] - **esm**: avoid try/catch when validating urls (Yagiz Nizipli) [#47541](https://github.com/nodejs/node/pull/47541)
* \[[`439ea47a77`](https://github.com/nodejs/node/commit/439ea47a77)] - **(SEMVER-MINOR)** **fs**: add recursive option to readdir and opendir (Ethan Arrowood) [#41439](https://github.com/nodejs/node/pull/41439)
* \[[`a54e898dc8`](https://github.com/nodejs/node/commit/a54e898dc8)] - **(SEMVER-MINOR)** **fs**: add support for mode flag to specify the copy behavior (Tetsuharu Ohzeki) [#47084](https://github.com/nodejs/node/pull/47084)
* \[[`96f93cc500`](https://github.com/nodejs/node/commit/96f93cc500)] - **(SEMVER-MINOR)** **http**: remove internal error in assignSocket (Matteo Collina) [#47723](https://github.com/nodejs/node/pull/47723)
* \[[`4fa773964b`](https://github.com/nodejs/node/commit/4fa773964b)] - **(SEMVER-MINOR)** **http**: add highWaterMark opt in http.createServer (HinataKah0) [#47405](https://github.com/nodejs/node/pull/47405)
* \[[`94a5abb1e0`](https://github.com/nodejs/node/commit/94a5abb1e0)] - **inspector**: add tips for Session (theanarkh) [#47195](https://github.com/nodejs/node/pull/47195)
* \[[`21ff33127a`](https://github.com/nodejs/node/commit/21ff33127a)] - **lib**: improve esm resolve performance (Yagiz Nizipli) [#46652](https://github.com/nodejs/node/pull/46652)
* \[[`b8bdaf86c4`](https://github.com/nodejs/node/commit/b8bdaf86c4)] - **lib**: disallow file-backed blob cloning (James M Snell) [#47574](https://github.com/nodejs/node/pull/47574)
* \[[`e8bc03b372`](https://github.com/nodejs/node/commit/e8bc03b372)] - **lib**: use webidl DOMString converter in EventTarget (Matthew Aitken) [#47514](https://github.com/nodejs/node/pull/47514)
* \[[`91e4a7cdee`](https://github.com/nodejs/node/commit/91e4a7cdee)] - **loader**: use default loader as cascaded loader in the in loader worker (Joyee Cheung) [#47620](https://github.com/nodejs/node/pull/47620)
* \[[`d5089fe00a`](https://github.com/nodejs/node/commit/d5089fe00a)] - **meta**: fix dependabot commit message (Mestery) [#47810](https://github.com/nodejs/node/pull/47810)
* \[[`92794400ce`](https://github.com/nodejs/node/commit/92794400ce)] - **meta**: ping nodejs/startup for startup test changes (Joyee Cheung) [#47771](https://github.com/nodejs/node/pull/47771)
* \[[`8d43689077`](https://github.com/nodejs/node/commit/8d43689077)] - **meta**: add mailmap entry for KhafraDev (Rich Trott) [#47512](https://github.com/nodejs/node/pull/47512)
* \[[`4d02901935`](https://github.com/nodejs/node/commit/4d02901935)] - **node-api**: test passing NULL to napi\_define\_class (Gabriel Schulhof) [#47567](https://github.com/nodejs/node/pull/47567)
* \[[`568256dca0`](https://github.com/nodejs/node/commit/568256dca0)] - **node-api**: test passing NULL to number APIs (Gabriel Schulhof) [#47549](https://github.com/nodejs/node/pull/47549)
* \[[`12f0fa386d`](https://github.com/nodejs/node/commit/12f0fa386d)] - **node-api**: remove unused mark\_arraybuffer\_as\_untransferable (Chengzhong Wu) [#47557](https://github.com/nodejs/node/pull/47557)
* \[[`e8ea83416a`](https://github.com/nodejs/node/commit/e8ea83416a)] - **quic**: add more QUIC implementation (James M Snell) [#47494](https://github.com/nodejs/node/pull/47494)
* \[[`af227b159d`](https://github.com/nodejs/node/commit/af227b159d)] - **readline**: fix issue with newline-less last line (Ian Harris) [#47317](https://github.com/nodejs/node/pull/47317)
* \[[`e948bec969`](https://github.com/nodejs/node/commit/e948bec969)] - **src**: avoid copying string in fs\_permission (Yagiz Nizipli) [#47746](https://github.com/nodejs/node/pull/47746)
* \[[`dc43ce7706`](https://github.com/nodejs/node/commit/dc43ce7706)] - **src**: replace idna functions with ada::idna (Yagiz Nizipli) [#47735](https://github.com/nodejs/node/pull/47735)
* \[[`1f9e7ce7e8`](https://github.com/nodejs/node/commit/1f9e7ce7e8)] - **src**: fix typo in comment in quic/sessionticket.cc (Tobias Nießen) [#47754](https://github.com/nodejs/node/pull/47754)
* \[[`2acb57b777`](https://github.com/nodejs/node/commit/2acb57b777)] - **src**: mark fatal error functions as noreturn (Chengzhong Wu) [#47695](https://github.com/nodejs/node/pull/47695)
* \[[`4431df7481`](https://github.com/nodejs/node/commit/4431df7481)] - **src**: split BlobSerializer/BlobDeserializer (Joyee Cheung) [#47458](https://github.com/nodejs/node/pull/47458)
* \[[`bf9a52cb3d`](https://github.com/nodejs/node/commit/bf9a52cb3d)] - **src**: prevent changing FunctionTemplateInfo after publish (Shelley Vohr) [#46979](https://github.com/nodejs/node/pull/46979)
* \[[`872e6706ca`](https://github.com/nodejs/node/commit/872e6706ca)] - **src**: add v8 fast api for url canParse (Matthew Aitken) [#47552](https://github.com/nodejs/node/pull/47552)
* \[[`cfafe431f2`](https://github.com/nodejs/node/commit/cfafe431f2)] - **src**: make AliasedBuffers in the binding data weak (Joyee Cheung) [#47354](https://github.com/nodejs/node/pull/47354)
* \[[`cf48db0034`](https://github.com/nodejs/node/commit/cf48db0034)] - **src**: use v8::Boolean(b) over b ? True() : False() (Tobias Nießen) [#47554](https://github.com/nodejs/node/pull/47554)
* \[[`ba255eda37`](https://github.com/nodejs/node/commit/ba255eda37)] - **src**: fix typo in process.env accessor error message (Moritz Raho) [#47014](https://github.com/nodejs/node/pull/47014)
* \[[`daf0c78232`](https://github.com/nodejs/node/commit/daf0c78232)] - **src**: replace static const string\_view by static constexpr (Daniel Lemire) [#47524](https://github.com/nodejs/node/pull/47524)
* \[[`57e7ed7f47`](https://github.com/nodejs/node/commit/57e7ed7f47)] - **src**: fix CSPRNG when length exceeds INT\_MAX (Tobias Nießen) [#47515](https://github.com/nodejs/node/pull/47515)
* \[[`cda36bfd8f`](https://github.com/nodejs/node/commit/cda36bfd8f)] - **src**: use correct variable in node\_builtins.cc (Michaël Zasso) [#47343](https://github.com/nodejs/node/pull/47343)
* \[[`adc1601ccd`](https://github.com/nodejs/node/commit/adc1601ccd)] - **src**: slim down stream\_base-inl.h (lilsweetcaligula) [#46972](https://github.com/nodejs/node/pull/46972)
* \[[`f88132f1b8`](https://github.com/nodejs/node/commit/f88132f1b8)] - **stream**: prevent pipeline hang with generator functions (Debadree Chatterjee) [#47712](https://github.com/nodejs/node/pull/47712)
* \[[`2b411f4b42`](https://github.com/nodejs/node/commit/2b411f4b42)] - **(SEMVER-MINOR)** **stream**: preserve object mode in compose (Raz Luvaton) [#47413](https://github.com/nodejs/node/pull/47413)
* \[[`159cf02920`](https://github.com/nodejs/node/commit/159cf02920)] - **test**: refactor to use `getEventListeners` in timers (Deokjin Kim) [#47759](https://github.com/nodejs/node/pull/47759)
* \[[`97a3d39b8f`](https://github.com/nodejs/node/commit/97a3d39b8f)] - **test**: add and use tmpdir.hasEnoughSpace() (Tobias Nießen) [#47767](https://github.com/nodejs/node/pull/47767)
* \[[`5bb7b26bb5`](https://github.com/nodejs/node/commit/5bb7b26bb5)] - **test**: remove spaces from test runner test names (Tobias Nießen) [#47733](https://github.com/nodejs/node/pull/47733)
* \[[`84fa9fd725`](https://github.com/nodejs/node/commit/84fa9fd725)] - **test**: refactor WPTRunner and enable parallel WPT execution (Filip Skokan) [#47635](https://github.com/nodejs/node/pull/47635)
* \[[`9d3768eb01`](https://github.com/nodejs/node/commit/9d3768eb01)] - _**Revert**_ "**test**: run WPT files in parallel again" (Filip Skokan) [#47627](https://github.com/nodejs/node/pull/47627)
* \[[`826f4041d1`](https://github.com/nodejs/node/commit/826f4041d1)] - **test**: mark test-cluster-primary-error flaky on asan (Yagiz Nizipli) [#47422](https://github.com/nodejs/node/pull/47422)
* \[[`e5251e31eb`](https://github.com/nodejs/node/commit/e5251e31eb)] - **test\_runner**: fix --require with --experimental-loader (Moshe Atlow) [#47751](https://github.com/nodejs/node/pull/47751)
* \[[`6ee5e42c73`](https://github.com/nodejs/node/commit/6ee5e42c73)] - **(SEMVER-MINOR)** **test\_runner**: support combining coverage reports (Colin Ihrig) [#47686](https://github.com/nodejs/node/pull/47686)
* \[[`f8581e7629`](https://github.com/nodejs/node/commit/f8581e7629)] - **test\_runner**: remove no-op validation (Colin Ihrig) [#47687](https://github.com/nodejs/node/pull/47687)
* \[[`40b38797c5`](https://github.com/nodejs/node/commit/40b38797c5)] - **test\_runner**: fix test runner concurrency (Moshe Atlow) [#47675](https://github.com/nodejs/node/pull/47675)
* \[[`2d7cac0c5b`](https://github.com/nodejs/node/commit/2d7cac0c5b)] - **test\_runner**: fix test counting (Moshe Atlow) [#47675](https://github.com/nodejs/node/pull/47675)
* \[[`5a9b71a52e`](https://github.com/nodejs/node/commit/5a9b71a52e)] - **test\_runner**: fix nested hooks (Moshe Atlow) [#47648](https://github.com/nodejs/node/pull/47648)
* \[[`5327483f31`](https://github.com/nodejs/node/commit/5327483f31)] - **(SEMVER-MINOR)** **test\_runner**: add testNamePatterns to run api (Chemi Atlow) [#47628](https://github.com/nodejs/node/pull/47628)
* \[[`b6fb7914ca`](https://github.com/nodejs/node/commit/b6fb7914ca)] - **test\_runner**: support coverage of unnamed functions (Colin Ihrig) [#47652](https://github.com/nodejs/node/pull/47652)
* \[[`1f120a396f`](https://github.com/nodejs/node/commit/1f120a396f)] - **test\_runner**: move coverage collection to root.postRun() (Colin Ihrig) [#47651](https://github.com/nodejs/node/pull/47651)
* \[[`bdd02a467d`](https://github.com/nodejs/node/commit/bdd02a467d)] - **(SEMVER-MINOR)** **test\_runner**: execute before hook on test (Chemi Atlow) [#47586](https://github.com/nodejs/node/pull/47586)
* \[[`ec24abaa03`](https://github.com/nodejs/node/commit/ec24abaa03)] - **test\_runner**: avoid reporting parents of failing tests in summary (Moshe Atlow) [#47579](https://github.com/nodejs/node/pull/47579)
* \[[`4203057740`](https://github.com/nodejs/node/commit/4203057740)] - **test\_runner**: fix spec skip detection (Moshe Atlow) [#47537](https://github.com/nodejs/node/pull/47537)
* \[[`57c69987ba`](https://github.com/nodejs/node/commit/57c69987ba)] - **tls**: accept SecureContext object in server.addContext() (HinataKah0) [#47570](https://github.com/nodejs/node/pull/47570)
* \[[`c620eb80a0`](https://github.com/nodejs/node/commit/c620eb80a0)] - **tools**: update doc to highlight.js\@11.8.0 (Node.js GitHub Bot) [#47786](https://github.com/nodejs/node/pull/47786)
* \[[`326c3f1593`](https://github.com/nodejs/node/commit/326c3f1593)] - **tools**: add the missing LoongArch64 definition in the v8.gyp file (Sun Haiyong) [#47641](https://github.com/nodejs/node/pull/47641)
* \[[`8d1588acdc`](https://github.com/nodejs/node/commit/8d1588acdc)] - **tools**: update lint-md-dependencies to rollup\@3.21.1 (Node.js GitHub Bot) [#47787](https://github.com/nodejs/node/pull/47787)
* \[[`226e5b83ee`](https://github.com/nodejs/node/commit/226e5b83ee)] - **tools**: move update-npm to dep updaters (Marco Ippolito) [#47619](https://github.com/nodejs/node/pull/47619)
* \[[`9d0bef6c0a`](https://github.com/nodejs/node/commit/9d0bef6c0a)] - **tools**: fix update-v8-patch cache (Marco Ippolito) [#47725](https://github.com/nodejs/node/pull/47725)
* \[[`63e8c95a66`](https://github.com/nodejs/node/commit/63e8c95a66)] - **tools**: automate v8 patch update (Marco Ippolito) [#47594](https://github.com/nodejs/node/pull/47594)
* \[[`d2994e52d3`](https://github.com/nodejs/node/commit/d2994e52d3)] - **tools**: fix skip message in update-cjs-module-lexer (Tobias Nießen) [#47701](https://github.com/nodejs/node/pull/47701)
* \[[`ccf9c37b43`](https://github.com/nodejs/node/commit/ccf9c37b43)] - **tools**: update lint-md-dependencies to @rollup/plugin-commonjs\@24.1.0 (Node.js GitHub Bot) [#47577](https://github.com/nodejs/node/pull/47577)
* \[[`0887fa0464`](https://github.com/nodejs/node/commit/0887fa0464)] - **tools**: keep PR titles/description up-to-date (Tobias Nießen) [#47621](https://github.com/nodejs/node/pull/47621)
* \[[`b8927ddf16`](https://github.com/nodejs/node/commit/b8927ddf16)] - **tools**: fix updating root certificates (Richard Lau) [#47607](https://github.com/nodejs/node/pull/47607)
* \[[`87cae0cb59`](https://github.com/nodejs/node/commit/87cae0cb59)] - **tools**: update PR label config (Mohammed Keyvanzadeh) [#47593](https://github.com/nodejs/node/pull/47593)
* \[[`c17f2688b8`](https://github.com/nodejs/node/commit/c17f2688b8)] - _**Revert**_ "**tools**: ensure failed daily wpt run still generates a report" (Filip Skokan) [#47627](https://github.com/nodejs/node/pull/47627)
* \[[`fbe7d73234`](https://github.com/nodejs/node/commit/fbe7d73234)] - **tools**: add execution permission to uvwasi script (Mert Can Altın) [#47600](https://github.com/nodejs/node/pull/47600)
* \[[`e3f4ff439e`](https://github.com/nodejs/node/commit/e3f4ff439e)] - **tools**: add update script for googletest (Tobias Nießen) [#47482](https://github.com/nodejs/node/pull/47482)
* \[[`7c552e650a`](https://github.com/nodejs/node/commit/7c552e650a)] - **tools**: add option to run workflow with specific tool id (Michaël Zasso) [#47591](https://github.com/nodejs/node/pull/47591)
* \[[`1509312170`](https://github.com/nodejs/node/commit/1509312170)] - **tools**: automate zlib update (Marco Ippolito) [#47417](https://github.com/nodejs/node/pull/47417)
* \[[`6af7f1ee03`](https://github.com/nodejs/node/commit/6af7f1ee03)] - **tools**: add url and whatwg-url labels automatically (Yagiz Nizipli) [#47545](https://github.com/nodejs/node/pull/47545)
* \[[`ff73c05d54`](https://github.com/nodejs/node/commit/ff73c05d54)] - **tools**: add performance label to benchmark changes (Yagiz Nizipli) [#47545](https://github.com/nodejs/node/pull/47545)
* \[[`9e3e0b0a84`](https://github.com/nodejs/node/commit/9e3e0b0a84)] - **tools**: automate uvwasi dependency update (Ranieri Innocenti Spada) [#47509](https://github.com/nodejs/node/pull/47509)
* \[[`233b628f22`](https://github.com/nodejs/node/commit/233b628f22)] - **tools**: add missing pinned dependencies (Mateo Nunez) [#47346](https://github.com/nodejs/node/pull/47346)
* \[[`e4d95859f5`](https://github.com/nodejs/node/commit/e4d95859f5)] - **tools**: automate ngtcp2 and nghttp3 update (Marco Ippolito) [#47402](https://github.com/nodejs/node/pull/47402)
* \[[`2e8338126b`](https://github.com/nodejs/node/commit/2e8338126b)] - **tools**: move update-undici.sh to dep\_updaters and create maintain md (Marco Ippolito) [#47380](https://github.com/nodejs/node/pull/47380)
* \[[`8712eafc87`](https://github.com/nodejs/node/commit/8712eafc87)] - **typings**: fix syntax error in tsconfig (Mohammed Keyvanzadeh) [#47584](https://github.com/nodejs/node/pull/47584)
* \[[`e4b6b79f18`](https://github.com/nodejs/node/commit/e4b6b79f18)] - **url**: reduce revokeObjectURL cpp calls (Yagiz Nizipli) [#47728](https://github.com/nodejs/node/pull/47728)
* \[[`9aae76727f`](https://github.com/nodejs/node/commit/9aae76727f)] - **url**: handle URL.canParse without base parameter (Yagiz Nizipli) [#47547](https://github.com/nodejs/node/pull/47547)
* \[[`180d365439`](https://github.com/nodejs/node/commit/180d365439)] - **url**: validate URL constructor arg length (Matthew Aitken) [#47513](https://github.com/nodejs/node/pull/47513)
* \[[`4839fc4369`](https://github.com/nodejs/node/commit/4839fc4369)] - **url**: validate argument length in canParse (Matthew Aitken) [#47513](https://github.com/nodejs/node/pull/47513)
* \[[`606523d37e`](https://github.com/nodejs/node/commit/606523d37e)] - **v8**: fix ERR\_NOT\_BUILDING\_SNAPSHOT is not a constructor (Chengzhong Wu) [#47721](https://github.com/nodejs/node/pull/47721)
* \[[`75c1d1b66e`](https://github.com/nodejs/node/commit/75c1d1b66e)] - **(SEMVER-MINOR)** **wasi**: make returnOnExit true by default (Michael Dawson) [#47390](https://github.com/nodejs/node/pull/47390)

<a id="20.0.0"></a>

## 2023-04-18, Version 20.0.0 (Current), @RafaelGSS

We're excited to announce the release of Node.js 20! Highlights include the new Node.js Permission Model,
a synchronous `import.meta.resolve`, a stable test\_runner, updates of the V8 JavaScript engine to 11.3, Ada to 2.0,
and more!

As a reminder, Node.js 20 will enter long-term support (LTS) in October, but until then, it will be the "Current" release for the next six months.
We encourage you to explore the new features and benefits offered by this latest release and evaluate their potential impact on your applications.

### Notable Changes

#### Permission Model

Node.js now has an experimental feature called the Permission Model.
It allows developers to restrict access to specific resources during program execution, such as file system operations,
child process spawning, and worker thread creation.
The API exists behind a flag `--experimental-permission` which when enabled will restrict access to all available permissions.
By using this feature, developers can prevent their applications from accessing or modifying sensitive data or running potentially harmful code.
More information about the Permission Model can be found in the [Node.js documentation](https://nodejs.org/api/permissions.html#process-based-permissions).

The Permission Model was a contribution by Rafael Gonzaga in [#44004](https://github.com/nodejs/node/pull/44004).

#### Custom ESM loader hooks run on dedicated thread

ESM hooks supplied via loaders (`--experimental-loader=foo.mjs`) now run in a dedicated thread, isolated from the main thread.
This provides a separate scope for loaders and ensures no cross-contamination between loaders and application code.

**Synchronous `import.meta.resolve()`**

In alignment with browser behavior, this function now returns synchronously.
Despite this, user loader `resolve` hooks can still be defined as async functions (or as sync functions, if the author prefers).
Even when there are async `resolve` hooks loaded, `import.meta.resolve` will still return synchronously for application code.

Contributed by Anna Henningsen, Antoine du Hamel, Geoffrey Booth, Guy Bedford, Jacob Smith, and Michaël Zasso in [#44710](https://github.com/nodejs/node/pull/44710)

#### V8 11.3

The V8 engine is updated to version 11.3, which is part of Chromium 113.
This version includes three new features to the JavaScript API:

* [String.prototype.isWellFormed and toWellFormed](https://chromestatus.com/feature/5200195346759680)
* [Methods that change Array and TypedArray by copy](https://chromestatus.com/feature/5068609911521280)
* [Resizable ArrayBuffer and growable SharedArrayBuffer](https://chromestatus.com/feature/4668361878274048)
* [RegExp v flag with set notation + properties of strings](https://chromestatus.com/feature/5144156542861312)
* [WebAssembly Tail Call](https://chromestatus.com/feature/5423405012615168)

The V8 update was a contribution by Michaël Zasso in [#47251](https://github.com/nodejs/node/pull/47251).

#### Stable Test Runner

The recent update to Node.js, version 20, includes an important change to the test\_runner module. The module has been marked as stable after a recent update.
Previously, the test\_runner module was experimental, but this change marks it as a stable module that is ready for production use.

Contributed by Colin Ihrig in [#46983](https://github.com/nodejs/node/pull/46983)

#### Ada 2.0

Node.js v20 comes with the latest version of the URL parser, Ada. This update brings significant performance improvements
to URL parsing, including enhancements to the `url.domainToASCII` and `url.domainToUnicode` functions in `node:url`.

Ada 2.0 has been integrated into the Node.js codebase, ensuring that all parts of the application can benefit from the
improved performance. Additionally, Ada 2.0 features a significant performance boost over its predecessor, Ada 1.0.4,
while also eliminating the need for the ICU requirement for URL hostname parsing.

Contributed by Yagiz Nizipli and Daniel Lemire in [#47339](https://github.com/nodejs/node/pull/47339)

#### Preparing single executable apps now requires injecting a Blob

Building a single executable app now requires injecting a blob prepared by
Node.js from a JSON config instead of injecting the raw JS file.
This opens up the possibility of embedding multiple co-existing resources into the SEA (Single Executable Apps).

Contributed by Joyee Cheung in [#47125](https://github.com/nodejs/node/pull/47125)

#### Web Crypto API

Web Crypto API functions' arguments are now coerced and validated as per their WebIDL definitions like in other Web Crypto API implementations.
This further improves interoperability with other implementations of Web Crypto API.

This change was made by Filip Skokan in [#46067](https://github.com/nodejs/node/pull/46067).

#### Official support for ARM64 Windows

Node.js now includes binaries for ARM64 Windows, allowing for native execution on the platform.
The MSI, zip/7z packages, and executable are available from the Node.js download site along with all other platforms.
The CI system was updated and all changes are now fully tested on ARM64 Windows, to prevent regressions and ensure compatibility.

ARM64 Windows was upgraded to tier 2 support by Stefan Stojanovic in [#47233](https://github.com/nodejs/node/pull/47233).

#### WASI version must now be specified

When `new WASI()` is called, the version option is now required and has no default value.
Any code that relied on the default for the version will need to be updated to request a specific version.

This change was made by Michael Dawson in [#47391](https://github.com/nodejs/node/pull/47391).

#### Deprecations and Removals

* \[[`3bed5f11e0`](https://github.com/nodejs/node/commit/3bed5f11e0)] - **(SEMVER-MAJOR)** **url**: runtime-deprecate url.parse() with invalid ports (Rich Trott) [#45526](https://github.com/nodejs/node/pull/45526)

`url.parse()` accepts URLs with ports that are not numbers. This behavior might result in host name spoofing with unexpected input.
These URLs will throw an error in future versions of Node.js, as the WHATWG URL API does already.
Starting with Node.js 20, these URLS cause `url.parse()` to emit a warning.

### Semver-Major Commits

* \[[`9fafb0a090`](https://github.com/nodejs/node/commit/9fafb0a090)] - **(SEMVER-MAJOR)** **async\_hooks**: deprecate the AsyncResource.bind asyncResource property (James M Snell) [#46432](https://github.com/nodejs/node/pull/46432)
* \[[`1948d37595`](https://github.com/nodejs/node/commit/1948d37595)] - **(SEMVER-MAJOR)** **buffer**: check INSPECT\_MAX\_BYTES with validateNumber (Umuoy) [#46599](https://github.com/nodejs/node/pull/46599)
* \[[`7bc0e6a4e7`](https://github.com/nodejs/node/commit/7bc0e6a4e7)] - **(SEMVER-MAJOR)** **buffer**: graduate File from experimental and expose as global (Khafra) [#47153](https://github.com/nodejs/node/pull/47153)
* \[[`671ffd7825`](https://github.com/nodejs/node/commit/671ffd7825)] - **(SEMVER-MAJOR)** **buffer**: use min/max of `validateNumber` (Deokjin Kim) [#45796](https://github.com/nodejs/node/pull/45796)
* \[[`ab1614d280`](https://github.com/nodejs/node/commit/ab1614d280)] - **(SEMVER-MAJOR)** **build**: reset embedder string to "-node.0" (Michaël Zasso) [#47251](https://github.com/nodejs/node/pull/47251)
* \[[`c1bcdbcf79`](https://github.com/nodejs/node/commit/c1bcdbcf79)] - **(SEMVER-MAJOR)** **build**: warn for gcc versions earlier than 10.1 (Richard Lau) [#46806](https://github.com/nodejs/node/pull/46806)
* \[[`649f68fc1e`](https://github.com/nodejs/node/commit/649f68fc1e)] - **(SEMVER-MAJOR)** **build**: reset embedder string to "-node.0" (Yagiz Nizipli) [#45579](https://github.com/nodejs/node/pull/45579)
* \[[`9374700d7a`](https://github.com/nodejs/node/commit/9374700d7a)] - **(SEMVER-MAJOR)** **crypto**: remove DEFAULT\_ENCODING (Tobias Nießen) [#47182](https://github.com/nodejs/node/pull/47182)
* \[[`1640aeb680`](https://github.com/nodejs/node/commit/1640aeb680)] - **(SEMVER-MAJOR)** **crypto**: remove obsolete SSL\_OP\_\* constants (Tobias Nießen) [#47073](https://github.com/nodejs/node/pull/47073)
* \[[`c2e4b1fa9a`](https://github.com/nodejs/node/commit/c2e4b1fa9a)] - **(SEMVER-MAJOR)** **crypto**: remove ALPN\_ENABLED (Tobias Nießen) [#47028](https://github.com/nodejs/node/pull/47028)
* \[[`3ef38c4bd7`](https://github.com/nodejs/node/commit/3ef38c4bd7)] - **(SEMVER-MAJOR)** **crypto**: use WebIDL converters in WebCryptoAPI (Filip Skokan) [#46067](https://github.com/nodejs/node/pull/46067)
* \[[`08af023b1f`](https://github.com/nodejs/node/commit/08af023b1f)] - **(SEMVER-MAJOR)** **crypto**: runtime deprecate replaced rsa-pss keygen parameters (Filip Skokan) [#45653](https://github.com/nodejs/node/pull/45653)
* \[[`7eb0ac3cb6`](https://github.com/nodejs/node/commit/7eb0ac3cb6)] - **(SEMVER-MAJOR)** **deps**: patch V8 to support compilation on win-arm64 (Michaël Zasso) [#47251](https://github.com/nodejs/node/pull/47251)
* \[[`a7c129f286`](https://github.com/nodejs/node/commit/a7c129f286)] - **(SEMVER-MAJOR)** **deps**: silence irrelevant V8 warning (Michaël Zasso) [#47251](https://github.com/nodejs/node/pull/47251)
* \[[`6f5655a18e`](https://github.com/nodejs/node/commit/6f5655a18e)] - **(SEMVER-MAJOR)** **deps**: always define V8\_EXPORT\_PRIVATE as no-op (Michaël Zasso) [#47251](https://github.com/nodejs/node/pull/47251)
* \[[`f226350fcb`](https://github.com/nodejs/node/commit/f226350fcb)] - **(SEMVER-MAJOR)** **deps**: update V8 to 11.3.244.4 (Michaël Zasso) [#47251](https://github.com/nodejs/node/pull/47251)
* \[[`d6dae7420e`](https://github.com/nodejs/node/commit/d6dae7420e)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick f1c888e7093e (Michaël Zasso) [#45579](https://github.com/nodejs/node/pull/45579)
* \[[`56c436533e`](https://github.com/nodejs/node/commit/56c436533e)] - **(SEMVER-MAJOR)** **deps**: fix V8 build on Windows with MSVC (Michaël Zasso) [#45579](https://github.com/nodejs/node/pull/45579)
* \[[`51ab98c71b`](https://github.com/nodejs/node/commit/51ab98c71b)] - **(SEMVER-MAJOR)** **deps**: silence irrelevant V8 warning (Michaël Zasso) [#45579](https://github.com/nodejs/node/pull/45579)
* \[[`9f84d3eea8`](https://github.com/nodejs/node/commit/9f84d3eea8)] - **(SEMVER-MAJOR)** **deps**: V8: fix v8-cppgc.h for MSVC (Jiawen Geng) [#45579](https://github.com/nodejs/node/pull/45579)
* \[[`f2318cd4b5`](https://github.com/nodejs/node/commit/f2318cd4b5)] - **(SEMVER-MAJOR)** **deps**: fix V8 build issue with inline methods (Jiawen Geng) [#45579](https://github.com/nodejs/node/pull/45579)
* \[[`16e03e7968`](https://github.com/nodejs/node/commit/16e03e7968)] - **(SEMVER-MAJOR)** **deps**: update V8 to 10.9.194.4 (Yagiz Nizipli) [#45579](https://github.com/nodejs/node/pull/45579)
* \[[`6473f5e7f7`](https://github.com/nodejs/node/commit/6473f5e7f7)] - **(SEMVER-MAJOR)** **doc**: update toolchains used for Node.js 20 releases (Richard Lau) [#47352](https://github.com/nodejs/node/pull/47352)
* \[[`cc18fd9608`](https://github.com/nodejs/node/commit/cc18fd9608)] - **(SEMVER-MAJOR)** **events**: refactor to use `validateNumber` (Deokjin Kim) [#45770](https://github.com/nodejs/node/pull/45770)
* \[[`ff92b40ffc`](https://github.com/nodejs/node/commit/ff92b40ffc)] - **(SEMVER-MAJOR)** **http**: close the connection after sending a body without declared length (Tim Perry) [#46333](https://github.com/nodejs/node/pull/46333)
* \[[`2a29df6464`](https://github.com/nodejs/node/commit/2a29df6464)] - **(SEMVER-MAJOR)** **http**: keep HTTP/1.1 conns alive even if the Connection header is removed (Tim Perry) [#46331](https://github.com/nodejs/node/pull/46331)
* \[[`391dc74a10`](https://github.com/nodejs/node/commit/391dc74a10)] - **(SEMVER-MAJOR)** **http**: throw error if options of http.Server is array (Deokjin Kim) [#46283](https://github.com/nodejs/node/pull/46283)
* \[[`ed3604cd64`](https://github.com/nodejs/node/commit/ed3604cd64)] - **(SEMVER-MAJOR)** **http**: server check Host header, to meet RFC 7230 5.4 requirement (wwwzbwcom) [#45597](https://github.com/nodejs/node/pull/45597)
* \[[`88d71dc301`](https://github.com/nodejs/node/commit/88d71dc301)] - **(SEMVER-MAJOR)** **lib**: refactor to use min/max of `validateNumber` (Deokjin Kim) [#45772](https://github.com/nodejs/node/pull/45772)
* \[[`e4d641f02a`](https://github.com/nodejs/node/commit/e4d641f02a)] - **(SEMVER-MAJOR)** **lib**: refactor to use validators in http2 (Debadree Chatterjee) [#46174](https://github.com/nodejs/node/pull/46174)
* \[[`0f3e531096`](https://github.com/nodejs/node/commit/0f3e531096)] - **(SEMVER-MAJOR)** **lib**: performance improvement on readline async iterator (Thiago Oliveira Santos) [#41276](https://github.com/nodejs/node/pull/41276)
* \[[`5b5898ac86`](https://github.com/nodejs/node/commit/5b5898ac86)] - **(SEMVER-MAJOR)** **lib,src**: update exit codes as per todos (Debadree Chatterjee) [#45841](https://github.com/nodejs/node/pull/45841)
* \[[`55321bafd1`](https://github.com/nodejs/node/commit/55321bafd1)] - **(SEMVER-MAJOR)** **net**: enable autoSelectFamily by default (Paolo Insogna) [#46790](https://github.com/nodejs/node/pull/46790)
* \[[`2d0d99733b`](https://github.com/nodejs/node/commit/2d0d99733b)] - **(SEMVER-MAJOR)** **process**: remove `process.exit()`, `process.exitCode` coercion to integer (Daeyeon Jeong) [#43716](https://github.com/nodejs/node/pull/43716)
* \[[`dc06df31b6`](https://github.com/nodejs/node/commit/dc06df31b6)] - **(SEMVER-MAJOR)** **readline**: refactor to use `validateNumber` (Deokjin Kim) [#45801](https://github.com/nodejs/node/pull/45801)
* \[[`295b2f3ff4`](https://github.com/nodejs/node/commit/295b2f3ff4)] - **(SEMVER-MAJOR)** **src**: update NODE\_MODULE\_VERSION to 115 (Michaël Zasso) [#47251](https://github.com/nodejs/node/pull/47251)
* \[[`3803b028dd`](https://github.com/nodejs/node/commit/3803b028dd)] - **(SEMVER-MAJOR)** **src**: share common code paths for SEA and embedder script (Anna Henningsen) [#46825](https://github.com/nodejs/node/pull/46825)
* \[[`e8bddac3e9`](https://github.com/nodejs/node/commit/e8bddac3e9)] - **(SEMVER-MAJOR)** **src**: apply ABI-breaking API simplifications (Anna Henningsen) [#46705](https://github.com/nodejs/node/pull/46705)
* \[[`f84de0ad4c`](https://github.com/nodejs/node/commit/f84de0ad4c)] - **(SEMVER-MAJOR)** **src**: use uint32\_t for process initialization flags enum (Anna Henningsen) [#46427](https://github.com/nodejs/node/pull/46427)
* \[[`a6242772ec`](https://github.com/nodejs/node/commit/a6242772ec)] - **(SEMVER-MAJOR)** **src**: fix ArrayBuffer::Detach deprecation (Michaël Zasso) [#45579](https://github.com/nodejs/node/pull/45579)
* \[[`dd5c39a808`](https://github.com/nodejs/node/commit/dd5c39a808)] - **(SEMVER-MAJOR)** **src**: update NODE\_MODULE\_VERSION to 112 (Yagiz Nizipli) [#45579](https://github.com/nodejs/node/pull/45579)
* \[[`63eca7fec0`](https://github.com/nodejs/node/commit/63eca7fec0)] - **(SEMVER-MAJOR)** **stream**: validate readable defaultEncoding (Marco Ippolito) [#46430](https://github.com/nodejs/node/pull/46430)
* \[[`9e7093f416`](https://github.com/nodejs/node/commit/9e7093f416)] - **(SEMVER-MAJOR)** **stream**: validate writable defaultEncoding (Marco Ippolito) [#46322](https://github.com/nodejs/node/pull/46322)
* \[[`fb91ee4f26`](https://github.com/nodejs/node/commit/fb91ee4f26)] - **(SEMVER-MAJOR)** **test**: make trace-gc-flag tests less strict (Yagiz Nizipli) [#45579](https://github.com/nodejs/node/pull/45579)
* \[[`eca618071e`](https://github.com/nodejs/node/commit/eca618071e)] - **(SEMVER-MAJOR)** **test**: adapt test-v8-stats for V8 update (Michaël Zasso) [#45579](https://github.com/nodejs/node/pull/45579)
* \[[`c03354d3e0`](https://github.com/nodejs/node/commit/c03354d3e0)] - **(SEMVER-MAJOR)** **test**: test case for multiple res.writeHead and res.getHeader (Marco Ippolito) [#45508](https://github.com/nodejs/node/pull/45508)
* \[[`c733cc0c7f`](https://github.com/nodejs/node/commit/c733cc0c7f)] - **(SEMVER-MAJOR)** **test\_runner**: mark module as stable (Colin Ihrig) [#46983](https://github.com/nodejs/node/pull/46983)
* \[[`7ce223273d`](https://github.com/nodejs/node/commit/7ce223273d)] - **(SEMVER-MAJOR)** **tools**: update V8 gypfiles for 11.1 (Michaël Zasso) [#47251](https://github.com/nodejs/node/pull/47251)
* \[[`ca4bd3023e`](https://github.com/nodejs/node/commit/ca4bd3023e)] - **(SEMVER-MAJOR)** **tools**: update V8 gypfiles for 11.0 (Michaël Zasso) [#47251](https://github.com/nodejs/node/pull/47251)
* \[[`58b06a269a`](https://github.com/nodejs/node/commit/58b06a269a)] - **(SEMVER-MAJOR)** **tools**: update V8 gypfiles (Michaël Zasso) [#45579](https://github.com/nodejs/node/pull/45579)
* \[[`027841c964`](https://github.com/nodejs/node/commit/027841c964)] - **(SEMVER-MAJOR)** **url**: use private properties for brand check (Yagiz Nizipli) [#46904](https://github.com/nodejs/node/pull/46904)
* \[[`3bed5f11e0`](https://github.com/nodejs/node/commit/3bed5f11e0)] - **(SEMVER-MAJOR)** **url**: runtime-deprecate url.parse() with invalid ports (Rich Trott) [#45526](https://github.com/nodejs/node/pull/45526)
* \[[`7c76fddf25`](https://github.com/nodejs/node/commit/7c76fddf25)] - **(SEMVER-MAJOR)** **util,doc**: mark parseArgs() as stable (Colin Ihrig) [#46718](https://github.com/nodejs/node/pull/46718)
* \[[`4b52727976`](https://github.com/nodejs/node/commit/4b52727976)] - **(SEMVER-MAJOR)** **wasi**: make version non-optional (Michael Dawson) [#47391](https://github.com/nodejs/node/pull/47391)

### Semver-Minor Commits

* \[[`d4b440bfac`](https://github.com/nodejs/node/commit/d4b440bfac)] - **(SEMVER-MINOR)** **fs**: implement byob mode for readableWebStream() (Debadree Chatterjee) [#46933](https://github.com/nodejs/node/pull/46933)
* \[[`00c222593e`](https://github.com/nodejs/node/commit/00c222593e)] - **(SEMVER-MINOR)** **src,process**: add permission model (Rafael Gonzaga) [#44004](https://github.com/nodejs/node/pull/44004)
* \[[`978b57d750`](https://github.com/nodejs/node/commit/978b57d750)] - **(SEMVER-MINOR)** **wasi**: no longer require flag to enable wasi (Michael Dawson) [#47286](https://github.com/nodejs/node/pull/47286)

### Semver-Patch Commits

* \[[`e50c6b9a22`](https://github.com/nodejs/node/commit/e50c6b9a22)] - **bootstrap**: do not expand process.argv\[1] for snapshot entry points (Joyee Cheung) [#47466](https://github.com/nodejs/node/pull/47466)
* \[[`c81e1143e4`](https://github.com/nodejs/node/commit/c81e1143e4)] - **bootstrap**: store internal loaders in C++ via a binding (Joyee Cheung) [#47215](https://github.com/nodejs/node/pull/47215)
* \[[`8e673bdb84`](https://github.com/nodejs/node/commit/8e673bdb84)] - **build**: add node-core-utils to setup (Jiawen Geng) [#47442](https://github.com/nodejs/node/pull/47442)
* \[[`5b561d72a6`](https://github.com/nodejs/node/commit/5b561d72a6)] - **build**: sync cares source change (Jiawen Geng) [#47359](https://github.com/nodejs/node/pull/47359)
* \[[`8e6ee53e4e`](https://github.com/nodejs/node/commit/8e6ee53e4e)] - **build**: remove non-exist build file (Jiawen Geng) [#47361](https://github.com/nodejs/node/pull/47361)
* \[[`9a4d21d1d9`](https://github.com/nodejs/node/commit/9a4d21d1d9)] - **build, deps, tools**: avoid excessive LTO (Konstantin Demin) [#47313](https://github.com/nodejs/node/pull/47313)
* \[[`48c01485cd`](https://github.com/nodejs/node/commit/48c01485cd)] - **crypto**: replace THROW with CHECK for scrypt keylen (Tobias Nießen) [#47407](https://github.com/nodejs/node/pull/47407)
* \[[`4c1a27716b`](https://github.com/nodejs/node/commit/4c1a27716b)] - **crypto**: re-add padding for AES-KW wrapped JWKs (Filip Skokan) [#46563](https://github.com/nodejs/node/pull/46563)
* \[[`b66eb15d12`](https://github.com/nodejs/node/commit/b66eb15d12)] - **deps**: update simdutf to 3.2.7 (Node.js GitHub Bot) [#47473](https://github.com/nodejs/node/pull/47473)
* \[[`3fc11477ba`](https://github.com/nodejs/node/commit/3fc11477ba)] - **deps**: update corepack to 0.17.2 (Node.js GitHub Bot) [#47474](https://github.com/nodejs/node/pull/47474)
* \[[`c1776531ab`](https://github.com/nodejs/node/commit/c1776531ab)] - **deps**: upgrade npm to 9.6.4 (npm team) [#47432](https://github.com/nodejs/node/pull/47432)
* \[[`e7ca09f310`](https://github.com/nodejs/node/commit/e7ca09f310)] - **deps**: update zlib to upstream 5edb52d4 (Luigi Pinca) [#47151](https://github.com/nodejs/node/pull/47151)
* \[[`88387ccd12`](https://github.com/nodejs/node/commit/88387ccd12)] - **deps**: update ada to 2.0.0 (Node.js GitHub Bot) [#47339](https://github.com/nodejs/node/pull/47339)
* \[[`9f468cc37e`](https://github.com/nodejs/node/commit/9f468cc37e)] - **deps**: cherry-pick Windows ARM64 fix for openssl (Richard Lau) [#46570](https://github.com/nodejs/node/pull/46570)
* \[[`eeab210b1b`](https://github.com/nodejs/node/commit/eeab210b1b)] - **deps**: update archs files for quictls/openssl-3.0.8+quic (RafaelGSS) [#46570](https://github.com/nodejs/node/pull/46570)
* \[[`d93d7716c7`](https://github.com/nodejs/node/commit/d93d7716c7)] - **deps**: upgrade openssl sources to quictls/openssl-3.0.8+quic (RafaelGSS) [#46571](https://github.com/nodejs/node/pull/46571)
* \[[`0f69ec4dd7`](https://github.com/nodejs/node/commit/0f69ec4dd7)] - **deps**: patch V8 to 10.9.194.9 (Michaël Zasso) [#45995](https://github.com/nodejs/node/pull/45995)
* \[[`5890d09644`](https://github.com/nodejs/node/commit/5890d09644)] - **deps**: patch V8 to 10.9.194.6 (Michaël Zasso) [#45748](https://github.com/nodejs/node/pull/45748)
* \[[`c02a7e7e93`](https://github.com/nodejs/node/commit/c02a7e7e93)] - **diagnostics\_channel**: fix ref counting bug when reaching zero subscribers (Stephen Belanger) [#47520](https://github.com/nodejs/node/pull/47520)
* \[[`c7ad5bb37d`](https://github.com/nodejs/node/commit/c7ad5bb37d)] - **doc**: info on handling unintended breaking changes (Michael Dawson) [#47426](https://github.com/nodejs/node/pull/47426)
* \[[`7d2d40ed0d`](https://github.com/nodejs/node/commit/7d2d40ed0d)] - **doc**: add performance initiative (Yagiz Nizipli) [#47424](https://github.com/nodejs/node/pull/47424)
* \[[`d56c0f7318`](https://github.com/nodejs/node/commit/d56c0f7318)] - **doc**: do not create a backup file (Luigi Pinca) [#47151](https://github.com/nodejs/node/pull/47151)
* \[[`412d27b65b`](https://github.com/nodejs/node/commit/412d27b65b)] - **doc**: add MoLow to the TSC (Colin Ihrig) [#47436](https://github.com/nodejs/node/pull/47436)
* \[[`f131cca0c0`](https://github.com/nodejs/node/commit/f131cca0c0)] - **doc**: reserve 116 for Electron 25 (Keeley Hammond) [#47375](https://github.com/nodejs/node/pull/47375)
* \[[`1022c6f424`](https://github.com/nodejs/node/commit/1022c6f424)] - **doc**: add experimental stages (Geoffrey Booth) [#46100](https://github.com/nodejs/node/pull/46100)
* \[[`42d3d74717`](https://github.com/nodejs/node/commit/42d3d74717)] - **doc**: clarify release notes for Node.js 16.19.0 (Richard Lau) [#45846](https://github.com/nodejs/node/pull/45846)
* \[[`533c6512da`](https://github.com/nodejs/node/commit/533c6512da)] - **doc**: clarify release notes for Node.js 14.21.2 (Richard Lau) [#45846](https://github.com/nodejs/node/pull/45846)
* \[[`97165fc1a6`](https://github.com/nodejs/node/commit/97165fc1a6)] - **doc**: fix doc metadata for Node.js 16.19.0 (Richard Lau) [#45863](https://github.com/nodejs/node/pull/45863)
* \[[`a266b8b702`](https://github.com/nodejs/node/commit/a266b8b702)] - **doc**: add registry number for Electron 23 & 24 (Keeley Hammond) [#45661](https://github.com/nodejs/node/pull/45661)
* \[[`2613a9ced9`](https://github.com/nodejs/node/commit/2613a9ced9)] - **esm**: move hook execution to separate thread (Jacob Smith) [#44710](https://github.com/nodejs/node/pull/44710)
* \[[`841f6b3abf`](https://github.com/nodejs/node/commit/841f6b3abf)] - **esm**: increase test coverage of edge cases (Antoine du Hamel) [#47033](https://github.com/nodejs/node/pull/47033)
* \[[`0d575fe61a`](https://github.com/nodejs/node/commit/0d575fe61a)] - **gyp**: put filenames in variables (Cheng Zhao) [#46965](https://github.com/nodejs/node/pull/46965)
* \[[`41b186722c`](https://github.com/nodejs/node/commit/41b186722c)] - **lib**: distinguish webidl interfaces with the extended property "Exposed" (Chengzhong Wu) [#46809](https://github.com/nodejs/node/pull/46809)
* \[[`9b7db62276`](https://github.com/nodejs/node/commit/9b7db62276)] - **lib**: makeRequireFunction patch when experimental policy (RafaelGSS) [nodejs-private/node-private#358](https://github.com/nodejs-private/node-private/pull/358)
* \[[`d43b532789`](https://github.com/nodejs/node/commit/d43b532789)] - **lib**: refactor to use `validateBuffer` (Deokjin Kim) [#46489](https://github.com/nodejs/node/pull/46489)
* \[[`9a76a2521b`](https://github.com/nodejs/node/commit/9a76a2521b)] - **meta**: ping security-wg team on permission model changes (Rafael Gonzaga) [#47483](https://github.com/nodejs/node/pull/47483)
* \[[`a4dadde1ba`](https://github.com/nodejs/node/commit/a4dadde1ba)] - **meta**: ping startup and realm team on src/node\_realm\* changes (Joyee Cheung) [#47448](https://github.com/nodejs/node/pull/47448)
* \[[`631c3ef3de`](https://github.com/nodejs/node/commit/631c3ef3de)] - **module**: do less CJS module loader initialization at run time (Joyee Cheung) [#47194](https://github.com/nodejs/node/pull/47194)
* \[[`8bcf0a42f7`](https://github.com/nodejs/node/commit/8bcf0a42f7)] - **permission**: fix chmod,chown improve fs coverage (Rafael Gonzaga) [#47529](https://github.com/nodejs/node/pull/47529)
* \[[`54d17ff4b5`](https://github.com/nodejs/node/commit/54d17ff4b5)] - **permission**: support fs.mkdtemp (Rafael Gonzaga) [#47470](https://github.com/nodejs/node/pull/47470)
* \[[`b441b5dc65`](https://github.com/nodejs/node/commit/b441b5dc65)] - **permission**: drop process.permission.deny (Rafael Gonzaga) [#47335](https://github.com/nodejs/node/pull/47335)
* \[[`aa30e16716`](https://github.com/nodejs/node/commit/aa30e16716)] - **permission**: fix some vulnerabilities in fs (Tobias Nießen) [#47091](https://github.com/nodejs/node/pull/47091)
* \[[`1726da9300`](https://github.com/nodejs/node/commit/1726da9300)] - **permission**: add path separator to loader check (Rafael Gonzaga) [#47030](https://github.com/nodejs/node/pull/47030)
* \[[`b164038c86`](https://github.com/nodejs/node/commit/b164038c86)] - **permission**: fix spawnSync permission check (RafaelGSS) [#46975](https://github.com/nodejs/node/pull/46975)
* \[[`af91400886`](https://github.com/nodejs/node/commit/af91400886)] - **policy**: makeRequireFunction on mainModule.require (RafaelGSS) [nodejs-private/node-private#358](https://github.com/nodejs-private/node-private/pull/358)
* \[[`f8b4e26aee`](https://github.com/nodejs/node/commit/f8b4e26aee)] - **quic**: add more QUIC impl (James M Snell) [#47348](https://github.com/nodejs/node/pull/47348)
* \[[`d65ae9f678`](https://github.com/nodejs/node/commit/d65ae9f678)] - **quic**: add additional quic implementation utilities (James M Snell) [#47289](https://github.com/nodejs/node/pull/47289)
* \[[`9b104be502`](https://github.com/nodejs/node/commit/9b104be502)] - **quic**: do not dereference shared\_ptr after move (Tobias Nießen) [#47294](https://github.com/nodejs/node/pull/47294)
* \[[`09a4bb152f`](https://github.com/nodejs/node/commit/09a4bb152f)] - **quic**: add multiple internal utilities (James M Snell) [#47263](https://github.com/nodejs/node/pull/47263)
* \[[`2bde0059ca`](https://github.com/nodejs/node/commit/2bde0059ca)] - **sea**: use JSON configuration and blob content for SEA (Joyee Cheung) [#47125](https://github.com/nodejs/node/pull/47125)
* \[[`78c7475493`](https://github.com/nodejs/node/commit/78c7475493)] - **src**: allow simdutf::convert\_\* functions to return zero (Daniel Lemire) [#47471](https://github.com/nodejs/node/pull/47471)
* \[[`5250947a53`](https://github.com/nodejs/node/commit/5250947a53)] - **src**: track ShadowRealm native objects correctly in the heap snapshot (Joyee Cheung) [#47389](https://github.com/nodejs/node/pull/47389)
* \[[`8059764621`](https://github.com/nodejs/node/commit/8059764621)] - **src**: use the internal field to determine if an object is a BaseObject (Joyee Cheung) [#47217](https://github.com/nodejs/node/pull/47217)
* \[[`698508afa8`](https://github.com/nodejs/node/commit/698508afa8)] - **src**: bootstrap prepare stack trace callback in shadow realm (Chengzhong Wu) [#47107](https://github.com/nodejs/node/pull/47107)
* \[[`e6b4d30a2f`](https://github.com/nodejs/node/commit/e6b4d30a2f)] - **src**: bootstrap Web \[Exposed=\*] APIs in the shadow realm (Chengzhong Wu) [#46809](https://github.com/nodejs/node/pull/46809)
* \[[`3646a66044`](https://github.com/nodejs/node/commit/3646a66044)] - **src**: fix AliasedBuffer memory attribution in heap snapshots (Joyee Cheung) [#46817](https://github.com/nodejs/node/pull/46817)
* \[[`8b2126f63f`](https://github.com/nodejs/node/commit/8b2126f63f)] - **src**: move AliasedBuffer implementation to -inl.h (Joyee Cheung) [#46817](https://github.com/nodejs/node/pull/46817)
* \[[`3abbc3829a`](https://github.com/nodejs/node/commit/3abbc3829a)] - **src**: fix useless call in permission.cc (Tobias Nießen) [#46833](https://github.com/nodejs/node/pull/46833)
* \[[`7b1e153530`](https://github.com/nodejs/node/commit/7b1e153530)] - **src**: simplify exit code accesses (Daeyeon Jeong) [#45125](https://github.com/nodejs/node/pull/45125)
* \[[`7359b92a41`](https://github.com/nodejs/node/commit/7359b92a41)] - **test**: remove unnecessary status check on test-release-npm (RafaelGSS) [#47516](https://github.com/nodejs/node/pull/47516)
* \[[`a5a5d2fb7e`](https://github.com/nodejs/node/commit/a5a5d2fb7e)] - **test**: mark test/parallel/test-file-write-stream4 as flaky (Yagiz Nizipli) [#47423](https://github.com/nodejs/node/pull/47423)
* \[[`81ad73a205`](https://github.com/nodejs/node/commit/81ad73a205)] - **test**: remove unused callback variables (angellovc) [#47167](https://github.com/nodejs/node/pull/47167)
* \[[`757a586ead`](https://github.com/nodejs/node/commit/757a586ead)] - **test**: migrate test runner message tests to snapshot (Moshe Atlow) [#47392](https://github.com/nodejs/node/pull/47392)
* \[[`86f890539f`](https://github.com/nodejs/node/commit/86f890539f)] - **test**: remove stale entry from known\_issues.status (Richard Lau) [#47454](https://github.com/nodejs/node/pull/47454)
* \[[`1f3773d0c1`](https://github.com/nodejs/node/commit/1f3773d0c1)] - **test**: move more inspector sequential tests to parallel (Joyee Cheung) [#47412](https://github.com/nodejs/node/pull/47412)
* \[[`617b8d44c6`](https://github.com/nodejs/node/commit/617b8d44c6)] - **test**: use random port in test-inspector-enabled (Joyee Cheung) [#47412](https://github.com/nodejs/node/pull/47412)
* \[[`ade0170c4f`](https://github.com/nodejs/node/commit/ade0170c4f)] - **test**: use random port in test-inspector-debug-brk-flag (Joyee Cheung) [#47412](https://github.com/nodejs/node/pull/47412)
* \[[`1a78632cd3`](https://github.com/nodejs/node/commit/1a78632cd3)] - **test**: use random port in NodeInstance.startViaSignal() (Joyee Cheung) [#47412](https://github.com/nodejs/node/pull/47412)
* \[[`23f66b137e`](https://github.com/nodejs/node/commit/23f66b137e)] - **test**: move test-shadow-realm-gc.js to known\_issues (Joyee Cheung) [#47355](https://github.com/nodejs/node/pull/47355)
* \[[`9dfd0394c5`](https://github.com/nodejs/node/commit/9dfd0394c5)] - **test**: remove useless WPT init scripts (Khafra) [#47221](https://github.com/nodejs/node/pull/47221)
* \[[`1cfe058778`](https://github.com/nodejs/node/commit/1cfe058778)] - **test**: fix test-permission-deny-fs-wildcard (win32) (Tobias Nießen) [#47095](https://github.com/nodejs/node/pull/47095)
* \[[`b8ef1b476e`](https://github.com/nodejs/node/commit/b8ef1b476e)] - **test**: add coverage for custom loader hooks with permission model (Antoine du Hamel) [#46977](https://github.com/nodejs/node/pull/46977)
* \[[`4a7c3e9c50`](https://github.com/nodejs/node/commit/4a7c3e9c50)] - **test**: fix file path in permission symlink test (Livia Medeiros) [#46859](https://github.com/nodejs/node/pull/46859)
* \[[`10005de6a8`](https://github.com/nodejs/node/commit/10005de6a8)] - **tools**: make `js2c.py` usable for other build systems (Cheng Zhao) [#46930](https://github.com/nodejs/node/pull/46930)
* \[[`1e2f9aca72`](https://github.com/nodejs/node/commit/1e2f9aca72)] - **tools**: move update-acorn.sh to dep\_updaters and create maintaining md (Marco Ippolito) [#47382](https://github.com/nodejs/node/pull/47382)
* \[[`174662a463`](https://github.com/nodejs/node/commit/174662a463)] - **tools**: update eslint to 8.38.0 (Node.js GitHub Bot) [#47475](https://github.com/nodejs/node/pull/47475)
* \[[`a58ca61f35`](https://github.com/nodejs/node/commit/a58ca61f35)] - **tools**: update eslint to 8.38.0 (Node.js GitHub Bot) [#47475](https://github.com/nodejs/node/pull/47475)
* \[[`37d12730ab`](https://github.com/nodejs/node/commit/37d12730ab)] - **tools**: automate cjs-module-lexer dependency update (Marco Ippolito) [#47446](https://github.com/nodejs/node/pull/47446)
* \[[`4fbfa3c9f2`](https://github.com/nodejs/node/commit/4fbfa3c9f2)] - **tools**: fix notify-on-push Slack messages (Antoine du Hamel) [#47453](https://github.com/nodejs/node/pull/47453)
* \[[`b1f2ff1242`](https://github.com/nodejs/node/commit/b1f2ff1242)] - **tools**: update lint-md-dependencies to @rollup/plugin-node-resolve\@15.0.2 (Node.js GitHub Bot) [#47431](https://github.com/nodejs/node/pull/47431)
* \[[`26b2584b84`](https://github.com/nodejs/node/commit/26b2584b84)] - **tools**: add root certificate update script (Richard Lau) [#47425](https://github.com/nodejs/node/pull/47425)
* \[[`553b052648`](https://github.com/nodejs/node/commit/553b052648)] - **tools**: remove targets for individual test suites in `Makefile` (Antoine du Hamel) [#46892](https://github.com/nodejs/node/pull/46892)
* \[[`747ff43e5b`](https://github.com/nodejs/node/commit/747ff43e5b)] - **url**: more sophisticated brand check for URLSearchParams (Timothy Gu) [#47414](https://github.com/nodejs/node/pull/47414)
* \[[`e727eb066f`](https://github.com/nodejs/node/commit/e727eb066f)] - **url**: do not use object as hashmap (Timothy Gu) [#47415](https://github.com/nodejs/node/pull/47415)
* \[[`81c7875eb7`](https://github.com/nodejs/node/commit/81c7875eb7)] - **url**: drop ICU requirement for parsing hostnames (Yagiz Nizipli) [#47339](https://github.com/nodejs/node/pull/47339)
* \[[`a4895df94a`](https://github.com/nodejs/node/commit/a4895df94a)] - **url**: use ada::url\_aggregator for parsing urls (Yagiz Nizipli) [#47339](https://github.com/nodejs/node/pull/47339)
