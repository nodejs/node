# Node.js 24 ChangeLog

<!--lint disable maximum-line-length no-literal-urls prohibited-strings-->

<table>
<tr>
<th>Current</th>
</tr>
<tr>
<td>
<a href="#24.2.0">24.2.0</a><br/>
<a href="#24.1.0">24.1.0</a><br/>
<a href="#24.0.2">24.0.2</a><br/>
<a href="#24.0.1">24.0.1</a><br/>
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

<a id="24.2.0"></a>

## 2025-06-09, Version 24.2.0 (Current), @aduh95

### Notable Changes

#### Remove support for HTTP/2 priority signaling

The support for priority signaling has been removed in nghttp2, following its
deprecation in the [RFC 9113](https://datatracker.ietf.org/doc/html/rfc9113#section-5.3.1).
As a consequence of this, priority signaling is deprecated on all release lines of Node.js,
and removed from Node.js 24 so we can include latest updates from nghttp2.

Contributed by Matteo Collina and Antoine du Hamel in
[#58293](https://github.com/nodejs/node/pull/58293).

#### `import.meta.main` is now available

Boolean value available in ECMAScript modules, which can be used to detect
whether the current module was the entry point of the current process.

```mjs
export function foo() {
  return 'Hello, world';
}

function main() {
  const message = foo();
  console.log(message);
}

if (import.meta.main) main();
// `foo` can be imported from another module without possible side-effects from `main`
```

Contributed by Joe and Antoine du Hamel in
[#57804](https://github.com/nodejs/node/pull/57804).

#### Other Notable Changes

* \[[`e13930bbe0`](https://github.com/nodejs/node/commit/e13930bbe0)] - **doc**: add Filip Skokan to TSC (Rafael Gonzaga) [#58499](https://github.com/nodejs/node/pull/58499)
* \[[`984894b38c`](https://github.com/nodejs/node/commit/984894b38c)] - **doc**: deprecate `util.isNativeError` in favor of `Error.isError` (Miguel Marcondes Filho) [#58262](https://github.com/nodejs/node/pull/58262)
* \[[`d261274b0f`](https://github.com/nodejs/node/commit/d261274b0f)] - **doc**: deprecate passing an empty string to `options.shell` (Antoine du Hamel) [#58564](https://github.com/nodejs/node/pull/58564)
* \[[`510872a522`](https://github.com/nodejs/node/commit/510872a522)] - **(SEMVER-MINOR)** **doc**: graduate `Symbol.dispose`/`asyncDispose` from experimental (James M Snell) [#58467](https://github.com/nodejs/node/pull/58467)
* \[[`6f4c9dd423`](https://github.com/nodejs/node/commit/6f4c9dd423)] - **(SEMVER-MINOR)** **fs**: add `autoClose` option to `FileHandle` readableWebStream (James M Snell) [#58548](https://github.com/nodejs/node/pull/58548)
* \[[`32efb63242`](https://github.com/nodejs/node/commit/32efb63242)] - **http**: deprecate instantiating classes without new (Yagiz Nizipli) [#58518](https://github.com/nodejs/node/pull/58518)
* \[[`0234a8ef53`](https://github.com/nodejs/node/commit/0234a8ef53)] - **(SEMVER-MINOR)** **http2**: add diagnostics channel `http2.server.stream.finish` (Darshan Sen) [#58560](https://github.com/nodejs/node/pull/58560)
* \[[`0f1e94f731`](https://github.com/nodejs/node/commit/0f1e94f731)] - **(SEMVER-MINOR)** **lib**: graduate error codes that have been around for years (James M Snell) [#58541](https://github.com/nodejs/node/pull/58541)
* \[[`13abca3c26`](https://github.com/nodejs/node/commit/13abca3c26)] - **(SEMVER-MINOR)** **perf\_hooks**: make event loop delay histogram disposable (James M Snell) [#58384](https://github.com/nodejs/node/pull/58384)
* \[[`8ea1fc5f3b`](https://github.com/nodejs/node/commit/8ea1fc5f3b)] - **(SEMVER-MINOR)** **src**: support namespace options in configuration file (Pietro Marchini) [#58073](https://github.com/nodejs/node/pull/58073)
* \[[`d6ea36ad6c`](https://github.com/nodejs/node/commit/d6ea36ad6c)] - **src,permission**: implicit allow-fs-read to app entrypoint (Rafael Gonzaga) [#58579](https://github.com/nodejs/node/pull/58579)
* \[[`5936cef60a`](https://github.com/nodejs/node/commit/5936cef60a)] - **(SEMVER-MINOR)** **test**: add disposable histogram test (James M Snell) [#58384](https://github.com/nodejs/node/pull/58384)
* \[[`7a91f4aaa1`](https://github.com/nodejs/node/commit/7a91f4aaa1)] - **(SEMVER-MINOR)** **test**: add test for async disposable worker thread (James M Snell) [#58385](https://github.com/nodejs/node/pull/58385)
* \[[`532c173cf2`](https://github.com/nodejs/node/commit/532c173cf2)] - **(SEMVER-MINOR)** **util**: add `none` style to styleText (James M Snell) [#58437](https://github.com/nodejs/node/pull/58437)
* \[[`aeb9ab4c4c`](https://github.com/nodejs/node/commit/aeb9ab4c4c)] - **(SEMVER-MINOR)** **worker**: make Worker async disposable (James M Snell) [#58385](https://github.com/nodejs/node/pull/58385)

### Commits

* \[[`6c92329b1b`](https://github.com/nodejs/node/commit/6c92329b1b)] - _**Revert**_ "**benchmark**: fix broken fs.cpSync benchmark" (Yuesong Jake Li) [#58476](https://github.com/nodejs/node/pull/58476)
* \[[`8bc045264e`](https://github.com/nodejs/node/commit/8bc045264e)] - **benchmark**: fix broken fs.cpSync benchmark (Dario Piotrowicz) [#58472](https://github.com/nodejs/node/pull/58472)
* \[[`46aa079cce`](https://github.com/nodejs/node/commit/46aa079cce)] - **benchmark**: add callback-based `fs.glob` to glob benchmark (Livia Medeiros) [#58417](https://github.com/nodejs/node/pull/58417)
* \[[`a57b05e105`](https://github.com/nodejs/node/commit/a57b05e105)] - **benchmark**: add more options to cp-sync (Sonny) [#58278](https://github.com/nodejs/node/pull/58278)
* \[[`8b5ada4b31`](https://github.com/nodejs/node/commit/8b5ada4b31)] - **buffer**: use Utf8LengthV2() instead of Utf8Length() (Tobias Nießen) [#58156](https://github.com/nodejs/node/pull/58156)
* \[[`22e97362f3`](https://github.com/nodejs/node/commit/22e97362f3)] - **build**: search for libnode.so in multiple places (Jan Staněk) [#58213](https://github.com/nodejs/node/pull/58213)
* \[[`0b4056c573`](https://github.com/nodejs/node/commit/0b4056c573)] - **build**: add support for OpenHarmony operating system (hqzing) [#58350](https://github.com/nodejs/node/pull/58350)
* \[[`db7f413dd3`](https://github.com/nodejs/node/commit/db7f413dd3)] - **build**: fix pointer compression builds (Joyee Cheung) [#58171](https://github.com/nodejs/node/pull/58171)
* \[[`7ff37183e5`](https://github.com/nodejs/node/commit/7ff37183e5)] - **build**: fix defaults for shared llhttp (Antoine du Hamel) [#58269](https://github.com/nodejs/node/pull/58269)
* \[[`b8c33190fe`](https://github.com/nodejs/node/commit/b8c33190fe)] - **build,win**: fix dll build (Stefan Stojanovic) [#58357](https://github.com/nodejs/node/pull/58357)
* \[[`ef9ecbe8a6`](https://github.com/nodejs/node/commit/ef9ecbe8a6)] - **child\_process**: give names to `ChildProcess` functions (Livia Medeiros) [#58370](https://github.com/nodejs/node/pull/58370)
* \[[`cec9d9d016`](https://github.com/nodejs/node/commit/cec9d9d016)] - **crypto**: forward auth tag to OpenSSL immediately (Tobias Nießen) [#58547](https://github.com/nodejs/node/pull/58547)
* \[[`9fccb0609f`](https://github.com/nodejs/node/commit/9fccb0609f)] - **crypto**: expose crypto.constants.OPENSSL\_IS\_BORINGSSL (Shelley Vohr) [#58387](https://github.com/nodejs/node/pull/58387)
* \[[`e7c69b9345`](https://github.com/nodejs/node/commit/e7c69b9345)] - **deps**: update nghttp2 to 1.65.0 (Node.js GitHub Bot) [#57269](https://github.com/nodejs/node/pull/57269)
* \[[`d0b89598a3`](https://github.com/nodejs/node/commit/d0b89598a3)] - **deps**: use proper C standard when building libuv (Yaksh Bariya) [#58587](https://github.com/nodejs/node/pull/58587)
* \[[`8a1fe7bc6a`](https://github.com/nodejs/node/commit/8a1fe7bc6a)] - **deps**: update simdjson to 3.12.3 (Node.js GitHub Bot) [#57682](https://github.com/nodejs/node/pull/57682)
* \[[`36b639b1eb`](https://github.com/nodejs/node/commit/36b639b1eb)] - **deps**: update googletest to e9092b1 (Node.js GitHub Bot) [#58565](https://github.com/nodejs/node/pull/58565)
* \[[`f8a2a1ef52`](https://github.com/nodejs/node/commit/f8a2a1ef52)] - **deps**: update corepack to 0.33.0 (Node.js GitHub Bot) [#58566](https://github.com/nodejs/node/pull/58566)
* \[[`efb28f7895`](https://github.com/nodejs/node/commit/efb28f7895)] - **deps**: V8: cherry-pick 249de887a8d3 (Michaël Zasso) [#58561](https://github.com/nodejs/node/pull/58561)
* \[[`88e621ea97`](https://github.com/nodejs/node/commit/88e621ea97)] - **deps**: update sqlite to 3.50.0 (Node.js GitHub Bot) [#58272](https://github.com/nodejs/node/pull/58272)
* \[[`8d2ba386f1`](https://github.com/nodejs/node/commit/8d2ba386f1)] - **deps**: update OpenSSL gen container to Ubuntu 22.04 (Michaël Zasso) [#58432](https://github.com/nodejs/node/pull/58432)
* \[[`7e62a77a7f`](https://github.com/nodejs/node/commit/7e62a77a7f)] - **deps**: update undici to 7.10.0 (Node.js GitHub Bot) [#58445](https://github.com/nodejs/node/pull/58445)
* \[[`87eebd7bad`](https://github.com/nodejs/node/commit/87eebd7bad)] - **deps**: keep required OpenSSL doc files (Michaël Zasso) [#58431](https://github.com/nodejs/node/pull/58431)
* \[[`10910660f6`](https://github.com/nodejs/node/commit/10910660f6)] - **deps**: update undici to 7.9.0 (Node.js GitHub Bot) [#58268](https://github.com/nodejs/node/pull/58268)
* \[[`5e27078ca2`](https://github.com/nodejs/node/commit/5e27078ca2)] - **deps**: update ada to 3.2.4 (Node.js GitHub Bot) [#58152](https://github.com/nodejs/node/pull/58152)
* \[[`3b1e4bdbbb`](https://github.com/nodejs/node/commit/3b1e4bdbbb)] - **deps**: update libuv to 1.51.0 (Node.js GitHub Bot) [#58124](https://github.com/nodejs/node/pull/58124)
* \[[`6bddf587ae`](https://github.com/nodejs/node/commit/6bddf587ae)] - **dns**: fix dns query cache implementation (Ethan Arrowood) [#58404](https://github.com/nodejs/node/pull/58404)
* \[[`984894b38c`](https://github.com/nodejs/node/commit/984894b38c)] - **doc**: deprecate utilisNativeError in favor of ErrorisError (Miguel Marcondes Filho) [#58262](https://github.com/nodejs/node/pull/58262)
* \[[`377ef3ce3a`](https://github.com/nodejs/node/commit/377ef3ce3a)] - **doc**: add support link for panva (Filip Skokan) [#58591](https://github.com/nodejs/node/pull/58591)
* \[[`33a69ff9e4`](https://github.com/nodejs/node/commit/33a69ff9e4)] - **doc**: update metadata for \_transformState deprecation (James M Snell) [#58530](https://github.com/nodejs/node/pull/58530)
* \[[`d261274b0f`](https://github.com/nodejs/node/commit/d261274b0f)] - **doc**: deprecate passing an empty string to `options.shell` (Antoine du Hamel) [#58564](https://github.com/nodejs/node/pull/58564)
* \[[`447ca11010`](https://github.com/nodejs/node/commit/447ca11010)] - **doc**: correct formatting of example definitions for `--test-shard` (Jacob Smith) [#58571](https://github.com/nodejs/node/pull/58571)
* \[[`2f555e0e19`](https://github.com/nodejs/node/commit/2f555e0e19)] - **doc**: clarify DEP0194 scope (Antoine du Hamel) [#58504](https://github.com/nodejs/node/pull/58504)
* \[[`af0446edcb`](https://github.com/nodejs/node/commit/af0446edcb)] - **doc**: deprecate HTTP/2 priority signaling (Matteo Collina) [#58313](https://github.com/nodejs/node/pull/58313)
* \[[`80cc17f1ec`](https://github.com/nodejs/node/commit/80cc17f1ec)] - **doc**: explain child\_process code and signal null values everywhere (Darshan Sen) [#58479](https://github.com/nodejs/node/pull/58479)
* \[[`e13930bbe0`](https://github.com/nodejs/node/commit/e13930bbe0)] - **doc**: add Filip Skokan to TSC (Rafael Gonzaga) [#58499](https://github.com/nodejs/node/pull/58499)
* \[[`5f3f045ecc`](https://github.com/nodejs/node/commit/5f3f045ecc)] - **doc**: update `git node release` example (Antoine du Hamel) [#58475](https://github.com/nodejs/node/pull/58475)
* \[[`4bbd026cde`](https://github.com/nodejs/node/commit/4bbd026cde)] - **doc**: add missing options.info for ZstdOptions (Jimmy Leung) [#58360](https://github.com/nodejs/node/pull/58360)
* \[[`a6d0d2a0d7`](https://github.com/nodejs/node/commit/a6d0d2a0d7)] - **doc**: add missing options.info for BrotliOptions (Jimmy Leung) [#58359](https://github.com/nodejs/node/pull/58359)
* \[[`510872a522`](https://github.com/nodejs/node/commit/510872a522)] - **(SEMVER-MINOR)** **doc**: graduate Symbol.dispose/asyncDispose from experimental (James M Snell) [#58467](https://github.com/nodejs/node/pull/58467)
* \[[`08685256cd`](https://github.com/nodejs/node/commit/08685256cd)] - **doc**: clarify x509.checkIssued only checks metadata (Filip Skokan) [#58457](https://github.com/nodejs/node/pull/58457)
* \[[`095794fc24`](https://github.com/nodejs/node/commit/095794fc24)] - **doc**: add links to parent class for `node:zlib` classes (Antoine du Hamel) [#58433](https://github.com/nodejs/node/pull/58433)
* \[[`7acac70bce`](https://github.com/nodejs/node/commit/7acac70bce)] - **doc**: remove remaining uses of `@@wellknown` syntax (René) [#58413](https://github.com/nodejs/node/pull/58413)
* \[[`62056d40c7`](https://github.com/nodejs/node/commit/62056d40c7)] - **doc**: clarify behavior of --watch-path and --watch flags (Juan Ignacio Benito) [#58136](https://github.com/nodejs/node/pull/58136)
* \[[`e6e6ae887d`](https://github.com/nodejs/node/commit/e6e6ae887d)] - **doc**: fix the order of `process.md` sections (Allon Murienik) [#58403](https://github.com/nodejs/node/pull/58403)
* \[[`d2f6c82c0f`](https://github.com/nodejs/node/commit/d2f6c82c0f)] - **doc,lib**: update source map links to ECMA426 (Chengzhong Wu) [#58597](https://github.com/nodejs/node/pull/58597)
* \[[`a994d3d51a`](https://github.com/nodejs/node/commit/a994d3d51a)] - **doc,src,test**: fix typos (Noritaka Kobayashi) [#58477](https://github.com/nodejs/node/pull/58477)
* \[[`252acc1e89`](https://github.com/nodejs/node/commit/252acc1e89)] - **errors**: show url of unsupported attributes in the error message (Aditi) [#58303](https://github.com/nodejs/node/pull/58303)
* \[[`767e88cbc3`](https://github.com/nodejs/node/commit/767e88cbc3)] - **esm**: unwrap WebAssembly.Global on Wasm Namespaces (Guy Bedford) [#57525](https://github.com/nodejs/node/pull/57525)
* \[[`adef9af3ae`](https://github.com/nodejs/node/commit/adef9af3ae)] - **(SEMVER-MINOR)** **esm**: implement import.meta.main (Joe) [#57804](https://github.com/nodejs/node/pull/57804)
* \[[`308f4cac4b`](https://github.com/nodejs/node/commit/308f4cac4b)] - **esm**: add support for dynamic source phase hook (Guy Bedford) [#58147](https://github.com/nodejs/node/pull/58147)
* \[[`fcef56cb05`](https://github.com/nodejs/node/commit/fcef56cb05)] - **fs**: improve cpSync no-filter copyDir performance (Dario Piotrowicz) [#58461](https://github.com/nodejs/node/pull/58461)
* \[[`996fdb05ab`](https://github.com/nodejs/node/commit/996fdb05ab)] - **fs**: fix cp handle existing symlinks (Yuesong Jake Li) [#58476](https://github.com/nodejs/node/pull/58476)
* \[[`d2931e50e3`](https://github.com/nodejs/node/commit/d2931e50e3)] - **fs**: fix cpSync handle existing symlinks (Yuesong Jake Li) [#58476](https://github.com/nodejs/node/pull/58476)
* \[[`6f4c9dd423`](https://github.com/nodejs/node/commit/6f4c9dd423)] - **(SEMVER-MINOR)** **fs**: add autoClose option to FileHandle readableWebStream (James M Snell) [#58548](https://github.com/nodejs/node/pull/58548)
* \[[`8870bb8677`](https://github.com/nodejs/node/commit/8870bb8677)] - **fs**: improve `cpSync` dest overriding performance (Dario Piotrowicz) [#58160](https://github.com/nodejs/node/pull/58160)
* \[[`f2e2301559`](https://github.com/nodejs/node/commit/f2e2301559)] - **fs**: unexpose internal constants (Chengzhong Wu) [#58327](https://github.com/nodejs/node/pull/58327)
* \[[`32efb63242`](https://github.com/nodejs/node/commit/32efb63242)] - **http**: deprecate instantiating classes without new (Yagiz Nizipli) [#58518](https://github.com/nodejs/node/pull/58518)
* \[[`0b987e5741`](https://github.com/nodejs/node/commit/0b987e5741)] - **(SEMVER-MAJOR)** **http2**: remove support for priority signaling (Matteo Collina) [#58293](https://github.com/nodejs/node/pull/58293)
* \[[`44ca874b2c`](https://github.com/nodejs/node/commit/44ca874b2c)] - **http2**: add lenient flag for RFC-9113 (Carlos Fuentes) [#58116](https://github.com/nodejs/node/pull/58116)
* \[[`0234a8ef53`](https://github.com/nodejs/node/commit/0234a8ef53)] - **(SEMVER-MINOR)** **http2**: add diagnostics channel 'http2.server.stream.finish' (Darshan Sen) [#58560](https://github.com/nodejs/node/pull/58560)
* \[[`2b868e8aa7`](https://github.com/nodejs/node/commit/2b868e8aa7)] - **http2**: add diagnostics channel 'http2.server.stream.error' (Darshan Sen) [#58512](https://github.com/nodejs/node/pull/58512)
* \[[`b4df8d38cd`](https://github.com/nodejs/node/commit/b4df8d38cd)] - **http2**: add diagnostics channel 'http2.server.stream.start' (Darshan Sen) [#58449](https://github.com/nodejs/node/pull/58449)
* \[[`d86ff608bb`](https://github.com/nodejs/node/commit/d86ff608bb)] - **http2**: remove no longer userful options.selectPadding (Jimmy Leung) [#58373](https://github.com/nodejs/node/pull/58373)
* \[[`13dbbdc8a8`](https://github.com/nodejs/node/commit/13dbbdc8a8)] - **http2**: add diagnostics channel 'http2.server.stream.created' (Darshan Sen) [#58390](https://github.com/nodejs/node/pull/58390)
* \[[`08855464ec`](https://github.com/nodejs/node/commit/08855464ec)] - **http2**: add diagnostics channel 'http2.client.stream.close' (Darshan Sen) [#58329](https://github.com/nodejs/node/pull/58329)
* \[[`566fc567b8`](https://github.com/nodejs/node/commit/566fc567b8)] - **http2**: add diagnostics channel 'http2.client.stream.finish' (Darshan Sen) [#58317](https://github.com/nodejs/node/pull/58317)
* \[[`f30b9117d4`](https://github.com/nodejs/node/commit/f30b9117d4)] - **http2**: add diagnostics channel 'http2.client.stream.error' (Darshan Sen) [#58306](https://github.com/nodejs/node/pull/58306)
* \[[`79b852a692`](https://github.com/nodejs/node/commit/79b852a692)] - **inspector**: add mimeType and charset support to Network.Response (Shima Ryuhei) [#58192](https://github.com/nodejs/node/pull/58192)
* \[[`402ac8b1d8`](https://github.com/nodejs/node/commit/402ac8b1d8)] - **inspector**: add protocol method Network.dataReceived (Chengzhong Wu) [#58001](https://github.com/nodejs/node/pull/58001)
* \[[`29f34a7f86`](https://github.com/nodejs/node/commit/29f34a7f86)] - **lib**: disable REPL completion on proxies and getters (Dario Piotrowicz) [#57909](https://github.com/nodejs/node/pull/57909)
* \[[`0f1e94f731`](https://github.com/nodejs/node/commit/0f1e94f731)] - **(SEMVER-MINOR)** **lib**: graduate error codes that have been around for years (James M Snell) [#58541](https://github.com/nodejs/node/pull/58541)
* \[[`cc1aacabb0`](https://github.com/nodejs/node/commit/cc1aacabb0)] - **lib**: make ERM functions into wrappers returning undefined (Livia Medeiros) [#58400](https://github.com/nodejs/node/pull/58400)
* \[[`8df4dee38c`](https://github.com/nodejs/node/commit/8df4dee38c)] - **lib**: remove no-mixed-operators eslint rule (Ruben Bridgewater) [#58375](https://github.com/nodejs/node/pull/58375)
* \[[`104d173f58`](https://github.com/nodejs/node/commit/104d173f58)] - **meta**: bump github/codeql-action from 3.28.16 to 3.28.18 (dependabot\[bot]) [#58552](https://github.com/nodejs/node/pull/58552)
* \[[`b454e8386c`](https://github.com/nodejs/node/commit/b454e8386c)] - **meta**: bump codecov/codecov-action from 5.4.2 to 5.4.3 (dependabot\[bot]) [#58551](https://github.com/nodejs/node/pull/58551)
* \[[`f31e014b81`](https://github.com/nodejs/node/commit/f31e014b81)] - **meta**: bump step-security/harden-runner from 2.11.0 to 2.12.0 (dependabot\[bot]) [#58109](https://github.com/nodejs/node/pull/58109)
* \[[`4da920cc13`](https://github.com/nodejs/node/commit/4da920cc13)] - **meta**: bump ossf/scorecard-action from 2.4.1 to 2.4.2 (dependabot\[bot]) [#58550](https://github.com/nodejs/node/pull/58550)
* \[[`eb9bb95fe2`](https://github.com/nodejs/node/commit/eb9bb95fe2)] - **meta**: bump rtCamp/action-slack-notify from 2.3.2 to 2.3.3 (dependabot\[bot]) [#58108](https://github.com/nodejs/node/pull/58108)
* \[[`27ada1f18c`](https://github.com/nodejs/node/commit/27ada1f18c)] - **meta**: move one or more collaborators to emeritus (Node.js GitHub Bot) [#58456](https://github.com/nodejs/node/pull/58456)
* \[[`4606a6792b`](https://github.com/nodejs/node/commit/4606a6792b)] - **meta**: bump github/codeql-action from 3.28.11 to 3.28.16 (dependabot\[bot]) [#58112](https://github.com/nodejs/node/pull/58112)
* \[[`7dfe448b7f`](https://github.com/nodejs/node/commit/7dfe448b7f)] - **meta**: bump codecov/codecov-action from 5.4.0 to 5.4.2 (dependabot\[bot]) [#58110](https://github.com/nodejs/node/pull/58110)
* \[[`18bb5f7e7e`](https://github.com/nodejs/node/commit/18bb5f7e7e)] - **meta**: bump actions/download-artifact from 4.2.1 to 4.3.0 (dependabot\[bot]) [#58106](https://github.com/nodejs/node/pull/58106)
* \[[`72f2a22889`](https://github.com/nodejs/node/commit/72f2a22889)] - **module**: clarify cjs global-like error on ModuleJobSync (Carlos Espa) [#56491](https://github.com/nodejs/node/pull/56491)
* \[[`b0e0b1afae`](https://github.com/nodejs/node/commit/b0e0b1afae)] - **net**: always publish to 'net.client.socket' diagnostics channel (Darshan Sen) [#58349](https://github.com/nodejs/node/pull/58349)
* \[[`f373d6a540`](https://github.com/nodejs/node/commit/f373d6a540)] - **node-api**: use WriteOneByteV2 in napi\_get\_value\_string\_latin1 (Chengzhong Wu) [#58325](https://github.com/nodejs/node/pull/58325)
* \[[`429c38db1b`](https://github.com/nodejs/node/commit/429c38db1b)] - **node-api**: use WriteV2 in napi\_get\_value\_string\_utf16 (Tobias Nießen) [#58165](https://github.com/nodejs/node/pull/58165)
* \[[`b882148999`](https://github.com/nodejs/node/commit/b882148999)] - **path**: improve path.resolve() performance when used as process.cwd() (Ruben Bridgewater) [#58362](https://github.com/nodejs/node/pull/58362)
* \[[`13abca3c26`](https://github.com/nodejs/node/commit/13abca3c26)] - **(SEMVER-MINOR)** **perf\_hooks**: make event loop delay histogram disposable (James M Snell) [#58384](https://github.com/nodejs/node/pull/58384)
* \[[`1cd417d823`](https://github.com/nodejs/node/commit/1cd417d823)] - **permission**: remove useless conditional (Juan José) [#58514](https://github.com/nodejs/node/pull/58514)
* \[[`462c4b0c24`](https://github.com/nodejs/node/commit/462c4b0c24)] - **readline**: add stricter validation for functions called after closed (Dario Piotrowicz) [#58283](https://github.com/nodejs/node/pull/58283)
* \[[`e3e36f902c`](https://github.com/nodejs/node/commit/e3e36f902c)] - **repl**: extract and standardize history from both repl and interface (Giovanni Bucci) [#58225](https://github.com/nodejs/node/pull/58225)
* \[[`cbb2a0172f`](https://github.com/nodejs/node/commit/cbb2a0172f)] - **report**: use uv\_getrusage\_thread in report (theanarkh) [#58405](https://github.com/nodejs/node/pull/58405)
* \[[`3a6bd9c4c4`](https://github.com/nodejs/node/commit/3a6bd9c4c4)] - **sqlite**: handle thrown errors in result callback (Colin Ihrig) [#58426](https://github.com/nodejs/node/pull/58426)
* \[[`0d761bbccd`](https://github.com/nodejs/node/commit/0d761bbccd)] - **src**: env\_vars caching and local variable scope optimization (Mert Can Altin) [#57624](https://github.com/nodejs/node/pull/57624)
* \[[`8ea1fc5f3b`](https://github.com/nodejs/node/commit/8ea1fc5f3b)] - **(SEMVER-MINOR)** **src**: support namespace options in configuration file (Pietro Marchini) [#58073](https://github.com/nodejs/node/pull/58073)
* \[[`f72ce2ef75`](https://github.com/nodejs/node/commit/f72ce2ef75)] - **src**: remove fast API for InternalModuleStat (Joyee Cheung) [#58489](https://github.com/nodejs/node/pull/58489)
* \[[`8a1eaea151`](https://github.com/nodejs/node/commit/8a1eaea151)] - **src**: update std::vector\<v8::Local\<T>> to use v8::LocalVector\<T> (Aditi) [#58500](https://github.com/nodejs/node/pull/58500)
* \[[`d99d657842`](https://github.com/nodejs/node/commit/d99d657842)] - **src**: fix FIPS init error handling (Tobias Nießen) [#58379](https://github.com/nodejs/node/pull/58379)
* \[[`11e4cd698b`](https://github.com/nodejs/node/commit/11e4cd698b)] - **src**: fix possible dereference of null pointer (Eusgor) [#58459](https://github.com/nodejs/node/pull/58459)
* \[[`ca0f5a0188`](https://github.com/nodejs/node/commit/ca0f5a0188)] - **src**: add env->cppgc\_allocation\_handle() convenience method (James M Snell) [#58483](https://github.com/nodejs/node/pull/58483)
* \[[`440d4f42bd`](https://github.com/nodejs/node/commit/440d4f42bd)] - **src**: fix -Wreturn-stack-address error (Shelley Vohr) [#58439](https://github.com/nodejs/node/pull/58439)
* \[[`08615b1020`](https://github.com/nodejs/node/commit/08615b1020)] - **src**: prepare for v8 sandboxing (James M Snell) [#58376](https://github.com/nodejs/node/pull/58376)
* \[[`63f643e844`](https://github.com/nodejs/node/commit/63f643e844)] - **src**: reorganize ContextifyFunction methods (Chengzhong Wu) [#58434](https://github.com/nodejs/node/pull/58434)
* \[[`3b6895a506`](https://github.com/nodejs/node/commit/3b6895a506)] - **src**: improve CompileFunctionAndCacheResult error handling (Chengzhong Wu) [#58434](https://github.com/nodejs/node/pull/58434)
* \[[`7f1c95aee8`](https://github.com/nodejs/node/commit/7f1c95aee8)] - **src**: make a number of minor improvements to buffer (James M Snell) [#58377](https://github.com/nodejs/node/pull/58377)
* \[[`ce081bcb9a`](https://github.com/nodejs/node/commit/ce081bcb9a)] - **src**: fix build when using shared simdutf (Antoine du Hamel) [#58407](https://github.com/nodejs/node/pull/58407)
* \[[`a35cc216e5`](https://github.com/nodejs/node/commit/a35cc216e5)] - **src**: track cppgc wrappers with a list in Realm (Joyee Cheung) [#56534](https://github.com/nodejs/node/pull/56534)
* \[[`947c1c2cd5`](https://github.com/nodejs/node/commit/947c1c2cd5)] - **src,lib**: obtain sourceURL in magic comments from V8 (Chengzhong Wu) [#58389](https://github.com/nodejs/node/pull/58389)
* \[[`d6ea36ad6c`](https://github.com/nodejs/node/commit/d6ea36ad6c)] - **src,permission**: implicit allow-fs-read to app entrypoint (Rafael Gonzaga) [#58579](https://github.com/nodejs/node/pull/58579)
* \[[`e8a07f2198`](https://github.com/nodejs/node/commit/e8a07f2198)] - **stream**: making DecompressionStream spec compilent for trailing junk (0hm☘️) [#58316](https://github.com/nodejs/node/pull/58316)
* \[[`3caa2f71c1`](https://github.com/nodejs/node/commit/3caa2f71c1)] - **stream**: test explicit resource management implicitly (LiviaMedeiros) [#58296](https://github.com/nodejs/node/pull/58296)
* \[[`9ccdf4fdb4`](https://github.com/nodejs/node/commit/9ccdf4fdb4)] - **test**: improve flakiness detection on stack corruption tests (Darshan Sen) [#58601](https://github.com/nodejs/node/pull/58601)
* \[[`d3fea003df`](https://github.com/nodejs/node/commit/d3fea003df)] - **test**: mark timeouts & flaky test as flaky on IBM i (Abdirahim Musse) [#58583](https://github.com/nodejs/node/pull/58583)
* \[[`8347ef6b53`](https://github.com/nodejs/node/commit/8347ef6b53)] - **test**: dispose of filehandles in filehandle.read tests (Livia Medeiros) [#58543](https://github.com/nodejs/node/pull/58543)
* \[[`34e86f91aa`](https://github.com/nodejs/node/commit/34e86f91aa)] - **test**: rewrite test-child-process-spawn-args (Michaël Zasso) [#58546](https://github.com/nodejs/node/pull/58546)
* \[[`d7a2458a58`](https://github.com/nodejs/node/commit/d7a2458a58)] - **test**: make sqlite-database-sync tests work with system sqlite (Jelle Licht) [#58507](https://github.com/nodejs/node/pull/58507)
* \[[`4d9d6830e0`](https://github.com/nodejs/node/commit/4d9d6830e0)] - **test**: force slow JSON.stringify path for overflow (Shelley Vohr) [#58181](https://github.com/nodejs/node/pull/58181)
* \[[`bef67e45e3`](https://github.com/nodejs/node/commit/bef67e45e3)] - **test**: account for truthy signal in flaky async\_hooks tests (Darshan Sen) [#58478](https://github.com/nodejs/node/pull/58478)
* \[[`007c82f206`](https://github.com/nodejs/node/commit/007c82f206)] - **test**: mark `test-http2-debug` as flaky on LinuxONE (Richard Lau) [#58494](https://github.com/nodejs/node/pull/58494)
* \[[`21f6400098`](https://github.com/nodejs/node/commit/21f6400098)] - **test**: update WPT for WebCryptoAPI to 591c95ce61 (Node.js GitHub Bot) [#58176](https://github.com/nodejs/node/pull/58176)
* \[[`1deb5f06a5`](https://github.com/nodejs/node/commit/1deb5f06a5)] - **test**: remove --no-warnings flag (Tobias Nießen) [#58424](https://github.com/nodejs/node/pull/58424)
* \[[`beba631a10`](https://github.com/nodejs/node/commit/beba631a10)] - **test**: add tests ensuring worker threads cannot access internals (Joe) [#58332](https://github.com/nodejs/node/pull/58332)
* \[[`5936cef60a`](https://github.com/nodejs/node/commit/5936cef60a)] - **(SEMVER-MINOR)** **test**: add disposable histogram test (James M Snell) [#58384](https://github.com/nodejs/node/pull/58384)
* \[[`7a91f4aaa1`](https://github.com/nodejs/node/commit/7a91f4aaa1)] - **(SEMVER-MINOR)** **test**: add test for async disposable worker thread (James M Snell) [#58385](https://github.com/nodejs/node/pull/58385)
* \[[`5fc4706280`](https://github.com/nodejs/node/commit/5fc4706280)] - **test**: leverage process.features.openssl\_is\_boringssl in test (Shelley Vohr) [#58421](https://github.com/nodejs/node/pull/58421)
* \[[`4629b18397`](https://github.com/nodejs/node/commit/4629b18397)] - **test**: fix test-buffer-tostring-range on allocation failure (Joyee Cheung) [#58416](https://github.com/nodejs/node/pull/58416)
* \[[`4c445a8c85`](https://github.com/nodejs/node/commit/4c445a8c85)] - **test**: skip in test-buffer-tostring-rangeerror on allocation failure (Joyee Cheung) [#58415](https://github.com/nodejs/node/pull/58415)
* \[[`53cb29898b`](https://github.com/nodejs/node/commit/53cb29898b)] - **test**: fix missing edge case in test-blob-slice-with-large-size (Joyee Cheung) [#58414](https://github.com/nodejs/node/pull/58414)
* \[[`89fdfdedc1`](https://github.com/nodejs/node/commit/89fdfdedc1)] - **test**: make crypto tests work with BoringSSL (Shelley Vohr) [#58117](https://github.com/nodejs/node/pull/58117)
* \[[`3b5d0e62b1`](https://github.com/nodejs/node/commit/3b5d0e62b1)] - **test**: test reordering of setAAD and setAuthTag (Tobias Nießen) [#58396](https://github.com/nodejs/node/pull/58396)
* \[[`029440bec5`](https://github.com/nodejs/node/commit/029440bec5)] - **test**: switch from deprecated `optparse` to `argparse` (Aviv Keller) [#58224](https://github.com/nodejs/node/pull/58224)
* \[[`d05263edcc`](https://github.com/nodejs/node/commit/d05263edcc)] - **test**: do not skip OCB decryption in FIPS mode (Tobias Nießen) [#58382](https://github.com/nodejs/node/pull/58382)
* \[[`23474cb257`](https://github.com/nodejs/node/commit/23474cb257)] - **test**: show more information in test-http2-debug upon failure (Joyee Cheung) [#58391](https://github.com/nodejs/node/pull/58391)
* \[[`d0302e7b3d`](https://github.com/nodejs/node/commit/d0302e7b3d)] - **test**: remove loop over single element (Tobias Nießen) [#58368](https://github.com/nodejs/node/pull/58368)
* \[[`33f615897d`](https://github.com/nodejs/node/commit/33f615897d)] - **test**: add chacha20-poly1305 to auth tag order test (Tobias Nießen) [#58367](https://github.com/nodejs/node/pull/58367)
* \[[`8f09a1f502`](https://github.com/nodejs/node/commit/8f09a1f502)] - **test**: skip wasm-allocation tests for pointer compression builds (Joyee Cheung) [#58171](https://github.com/nodejs/node/pull/58171)
* \[[`4ae6a1a5ed`](https://github.com/nodejs/node/commit/4ae6a1a5ed)] - **test**: remove references to create(De|C)ipher (Tobias Nießen) [#58363](https://github.com/nodejs/node/pull/58363)
* \[[`4d647271b2`](https://github.com/nodejs/node/commit/4d647271b2)] - **test\_runner**: emit event when file changes in watch mode (Jacopo Martinelli) [#57903](https://github.com/nodejs/node/pull/57903)
* \[[`1eda87c365`](https://github.com/nodejs/node/commit/1eda87c365)] - **test\_runner**: add level parameter to reporter.diagnostic (Jacopo Martinelli) [#57923](https://github.com/nodejs/node/pull/57923)
* \[[`13377512be`](https://github.com/nodejs/node/commit/13377512be)] - **tools**: bump the eslint group in `/tools/eslint` with 6 updates (dependabot\[bot]) [#58549](https://github.com/nodejs/node/pull/58549)
* \[[`fcc881de0d`](https://github.com/nodejs/node/commit/fcc881de0d)] - **tools**: support `DisposableStack` and `AsyncDisposableStack` in linter (LiviaMedeiros) [#58454](https://github.com/nodejs/node/pull/58454)
* \[[`208d6a5754`](https://github.com/nodejs/node/commit/208d6a5754)] - **tools**: support explicit resource management in eslint (LiviaMedeiros) [#58296](https://github.com/nodejs/node/pull/58296)
* \[[`32070f304a`](https://github.com/nodejs/node/commit/32070f304a)] - **tools**: add missing highway defines for IBM i (Abdirahim Musse) [#58335](https://github.com/nodejs/node/pull/58335)
* \[[`ddab63a323`](https://github.com/nodejs/node/commit/ddab63a323)] - **tty**: improve color terminal color detection (Ruben Bridgewater) [#58146](https://github.com/nodejs/node/pull/58146)
* \[[`c094bea8d9`](https://github.com/nodejs/node/commit/c094bea8d9)] - **tty**: use terminal VT mode on Windows (Anna Henningsen) [#58358](https://github.com/nodejs/node/pull/58358)
* \[[`dc21054a1e`](https://github.com/nodejs/node/commit/dc21054a1e)] - **typings**: add inspector internalBinding typing (Shima Ryuhei) [#58492](https://github.com/nodejs/node/pull/58492)
* \[[`3499285904`](https://github.com/nodejs/node/commit/3499285904)] - **typings**: remove no longer valid `FixedSizeBlobCopyJob` type (Dario Piotrowicz) [#58305](https://github.com/nodejs/node/pull/58305)
* \[[`1ed2deb2c8`](https://github.com/nodejs/node/commit/1ed2deb2c8)] - **typings**: remove no longer valid `revokeDataObject` type (Dario Piotrowicz) [#58305](https://github.com/nodejs/node/pull/58305)
* \[[`532c173cf2`](https://github.com/nodejs/node/commit/532c173cf2)] - **(SEMVER-MINOR)** **util**: add 'none' style to styleText (James M Snell) [#58437](https://github.com/nodejs/node/pull/58437)
* \[[`2d5a1ef528`](https://github.com/nodejs/node/commit/2d5a1ef528)] - **vm**: import call should return a promise in the current context (Chengzhong Wu) [#58309](https://github.com/nodejs/node/pull/58309)
* \[[`588c2449f2`](https://github.com/nodejs/node/commit/588c2449f2)] - **win,tools**: use Azure Trusted Signing (Stefan Stojanovic) [#58502](https://github.com/nodejs/node/pull/58502)
* \[[`aeb9ab4c4c`](https://github.com/nodejs/node/commit/aeb9ab4c4c)] - **(SEMVER-MINOR)** **worker**: make Worker async disposable (James M Snell) [#58385](https://github.com/nodejs/node/pull/58385)
* \[[`23416cce0a`](https://github.com/nodejs/node/commit/23416cce0a)] - **worker**: give names to `MessagePort` functions (Livia Medeiros) [#58307](https://github.com/nodejs/node/pull/58307)
* \[[`44df21b7fb`](https://github.com/nodejs/node/commit/44df21b7fb)] - **zlib**: remove mentions of unexposed Z\_TREES constant (Jimmy Leung) [#58371](https://github.com/nodejs/node/pull/58371)

<a id="24.1.0"></a>

## 2025-05-21, Version 24.1.0 (Current), @aduh95

### Notable Changes

* \[[`9d35b4ce95`](https://github.com/nodejs/node/commit/9d35b4ce95)] - **doc**: add JonasBa to collaborators (Jonas Badalic) [#58355](https://github.com/nodejs/node/pull/58355)
* \[[`b7d1bfa7b4`](https://github.com/nodejs/node/commit/b7d1bfa7b4)] - **doc**: add puskin to collaborators (Giovanni Bucci) [#58308](https://github.com/nodejs/node/pull/58308)
* \[[`fcead7c28e`](https://github.com/nodejs/node/commit/fcead7c28e)] - **(SEMVER-MINOR)** **fs**: add to `Dir` support for explicit resource management (Antoine du Hamel) [#58206](https://github.com/nodejs/node/pull/58206)
* \[[`f7041b9369`](https://github.com/nodejs/node/commit/f7041b9369)] - _**Revert**_ "**test\_runner**: change ts default glob" (Théo LUDWIG) [#58202](https://github.com/nodejs/node/pull/58202)

### Commits

* \[[`b33e8d2a71`](https://github.com/nodejs/node/commit/b33e8d2a71)] - **async\_hooks**: ensure AsyncLocalStore instances work isolated (Gerhard Stöbich) [#58149](https://github.com/nodejs/node/pull/58149)
* \[[`a1b078b18c`](https://github.com/nodejs/node/commit/a1b078b18c)] - **buffer**: give names to `Buffer.prototype.*Write()` functions (Livia Medeiros) [#58258](https://github.com/nodejs/node/pull/58258)
* \[[`4c967b73c3`](https://github.com/nodejs/node/commit/4c967b73c3)] - **buffer**: use constexpr where possible (Yagiz Nizipli) [#58141](https://github.com/nodejs/node/pull/58141)
* \[[`327095a928`](https://github.com/nodejs/node/commit/327095a928)] - **build**: fix uvwasi pkgname (Antoine du Hamel) [#58270](https://github.com/nodejs/node/pull/58270)
* \[[`2e54653d3d`](https://github.com/nodejs/node/commit/2e54653d3d)] - **build**: use FILE\_OFFSET\_BITS=64 esp. on 32-bit arch (RafaelGSS) [#58090](https://github.com/nodejs/node/pull/58090)
* \[[`7e4453fe93`](https://github.com/nodejs/node/commit/7e4453fe93)] - **build**: escape > metachar in vcbuild (Gerhard Stöbich) [#58157](https://github.com/nodejs/node/pull/58157)
* \[[`7dabf079b1`](https://github.com/nodejs/node/commit/7dabf079b1)] - **child\_process**: give names to promisified `exec()` and `execFile()` (LiviaMedeiros) [#57916](https://github.com/nodejs/node/pull/57916)
* \[[`a896eff1f2`](https://github.com/nodejs/node/commit/a896eff1f2)] - **crypto**: handle missing OPENSSL\_TLS\_SECURITY\_LEVEL (Shelley Vohr) [#58103](https://github.com/nodejs/node/pull/58103)
* \[[`6403aa458f`](https://github.com/nodejs/node/commit/6403aa458f)] - **crypto**: merge CipherBase.initiv into constructor (Tobias Nießen) [#58166](https://github.com/nodejs/node/pull/58166)
* \[[`30897d915c`](https://github.com/nodejs/node/commit/30897d915c)] - **deps**: V8: backport 1d3362c55396 (Shu-yu Guo) [#58230](https://github.com/nodejs/node/pull/58230)
* \[[`63f5d69d2b`](https://github.com/nodejs/node/commit/63f5d69d2b)] - **deps**: V8: cherry-pick 4f38995c8295 (Shu-yu Guo) [#58230](https://github.com/nodejs/node/pull/58230)
* \[[`5a5f6bb1d4`](https://github.com/nodejs/node/commit/5a5f6bb1d4)] - **deps**: V8: cherry-pick 044b9b6f589d (Rezvan Mahdavi Hezaveh) [#58230](https://github.com/nodejs/node/pull/58230)
* \[[`db57f0a4c0`](https://github.com/nodejs/node/commit/db57f0a4c0)] - **deps**: patch V8 to 13.6.233.10 (Michaël Zasso) [#58230](https://github.com/nodejs/node/pull/58230)
* \[[`f54a7a44ab`](https://github.com/nodejs/node/commit/f54a7a44ab)] - _**Revert**_ "**deps**: patch V8 to support compilation with MSVC" (Michaël Zasso) [#58187](https://github.com/nodejs/node/pull/58187)
* \[[`e3193eeca4`](https://github.com/nodejs/node/commit/e3193eeca4)] - _**Revert**_ "**deps**: always define V8\_EXPORT\_PRIVATE as no-op" (Michaël Zasso) [#58187](https://github.com/nodejs/node/pull/58187)
* \[[`e75ecf8ad1`](https://github.com/nodejs/node/commit/e75ecf8ad1)] - _**Revert**_ "**deps**: disable V8 concurrent sparkplug compilation" (Michaël Zasso) [#58187](https://github.com/nodejs/node/pull/58187)
* \[[`a0ca15558d`](https://github.com/nodejs/node/commit/a0ca15558d)] - **deps**: update llhttp to 9.3.0 (Fedor Indutny) [#58144](https://github.com/nodejs/node/pull/58144)
* \[[`90d4c11992`](https://github.com/nodejs/node/commit/90d4c11992)] - **deps**: update amaro to 0.5.3 (Node.js GitHub Bot) [#58174](https://github.com/nodejs/node/pull/58174)
* \[[`9d35b4ce95`](https://github.com/nodejs/node/commit/9d35b4ce95)] - **doc**: add JonasBa to collaborators (Jonas Badalic) [#58355](https://github.com/nodejs/node/pull/58355)
* \[[`2676ca0cf5`](https://github.com/nodejs/node/commit/2676ca0cf5)] - **doc**: add latest security release steward (Rafael Gonzaga) [#58339](https://github.com/nodejs/node/pull/58339)
* \[[`c35cc1bdd9`](https://github.com/nodejs/node/commit/c35cc1bdd9)] - **doc**: document default test-reporter change (Nico Jansen) [#58302](https://github.com/nodejs/node/pull/58302)
* \[[`2bb433d4a5`](https://github.com/nodejs/node/commit/2bb433d4a5)] - **doc**: fix CryptoKey.algorithm type and other interfaces in webcrypto.md (Filip Skokan) [#58294](https://github.com/nodejs/node/pull/58294)
* \[[`f04f09d783`](https://github.com/nodejs/node/commit/f04f09d783)] - **doc**: mark the callback argument of crypto.generatePrime as mandatory (Allon Murienik) [#58299](https://github.com/nodejs/node/pull/58299)
* \[[`3b9b010844`](https://github.com/nodejs/node/commit/3b9b010844)] - **doc**: remove comma delimiter mention on permissions doc (Rafael Gonzaga) [#58297](https://github.com/nodejs/node/pull/58297)
* \[[`f0cf1a028d`](https://github.com/nodejs/node/commit/f0cf1a028d)] - **doc**: make Stability labels not sticky in Stability index (Livia Medeiros) [#58291](https://github.com/nodejs/node/pull/58291)
* \[[`a1b937bdee`](https://github.com/nodejs/node/commit/a1b937bdee)] - **doc**: update commit-queue documentation (Dario Piotrowicz) [#58275](https://github.com/nodejs/node/pull/58275)
* \[[`b7d1bfa7b4`](https://github.com/nodejs/node/commit/b7d1bfa7b4)] - **doc**: add puskin to collaborators (Giovanni Bucci) [#58308](https://github.com/nodejs/node/pull/58308)
* \[[`fc30cdd8d2`](https://github.com/nodejs/node/commit/fc30cdd8d2)] - **doc**: update stability status for diagnostics\_channel to experimental (Idan Goshen) [#58261](https://github.com/nodejs/node/pull/58261)
* \[[`290a5ab8ca`](https://github.com/nodejs/node/commit/290a5ab8ca)] - **doc**: clarify napi\_get\_value\_string\_\* for bufsize 0 (Tobias Nießen) [#58158](https://github.com/nodejs/node/pull/58158)
* \[[`c26863a683`](https://github.com/nodejs/node/commit/c26863a683)] - **doc**: fix typo of file `http.md`, `outgoingMessage.setTimeout` section (yusheng chen) [#58188](https://github.com/nodejs/node/pull/58188)
* \[[`62dbd36dcb`](https://github.com/nodejs/node/commit/62dbd36dcb)] - **doc**: update return types for eventNames method in EventEmitter (Yukihiro Hasegawa) [#58083](https://github.com/nodejs/node/pull/58083)
* \[[`130c135f38`](https://github.com/nodejs/node/commit/130c135f38)] - **fs**: add support for `URL` for `fs.glob`'s `cwd` option (Antoine du Hamel) [#58182](https://github.com/nodejs/node/pull/58182)
* \[[`fcead7c28e`](https://github.com/nodejs/node/commit/fcead7c28e)] - **(SEMVER-MINOR)** **fs**: add to `Dir` support for explicit resource management (Antoine du Hamel) [#58206](https://github.com/nodejs/node/pull/58206)
* \[[`655326ba9f`](https://github.com/nodejs/node/commit/655326ba9f)] - **fs**: glob is stable, so should not emit experimental warnings (Théo LUDWIG) [#58236](https://github.com/nodejs/node/pull/58236)
* \[[`6ebcce7625`](https://github.com/nodejs/node/commit/6ebcce7625)] - **fs**: ensure `dir.read()` does not throw synchronously (Antoine du Hamel) [#58228](https://github.com/nodejs/node/pull/58228)
* \[[`7715722323`](https://github.com/nodejs/node/commit/7715722323)] - **http**: remove unused functions and add todos (Yagiz Nizipli) [#58143](https://github.com/nodejs/node/pull/58143)
* \[[`74a807e31f`](https://github.com/nodejs/node/commit/74a807e31f)] - **http,https**: give names to anonymous or misnamed functions (Livia Medeiros) [#58180](https://github.com/nodejs/node/pull/58180)
* \[[`24a9aefb08`](https://github.com/nodejs/node/commit/24a9aefb08)] - **http2**: add diagnostics channel 'http2.client.stream.start' (Darshan Sen) [#58292](https://github.com/nodejs/node/pull/58292)
* \[[`2cb86a3cd6`](https://github.com/nodejs/node/commit/2cb86a3cd6)] - **http2**: add diagnostics channel 'http2.client.stream.created' (Darshan Sen) [#58246](https://github.com/nodejs/node/pull/58246)
* \[[`8f1aee90d9`](https://github.com/nodejs/node/commit/8f1aee90d9)] - **http2**: give name to promisified `connect()` (LiviaMedeiros) [#57916](https://github.com/nodejs/node/pull/57916)
* \[[`b66f1b0be6`](https://github.com/nodejs/node/commit/b66f1b0be6)] - **inspector**: support for worker inspection in chrome devtools (Shima Ryuhei) [#56759](https://github.com/nodejs/node/pull/56759)
* \[[`868e72e367`](https://github.com/nodejs/node/commit/868e72e367)] - **lib**: fix sourcemaps with ts module mocking (Marco Ippolito) [#58193](https://github.com/nodejs/node/pull/58193)
* \[[`570cb6f6b6`](https://github.com/nodejs/node/commit/570cb6f6b6)] - **meta**: ignore mailmap changes in linux ci (Jonas Badalic) [#58356](https://github.com/nodejs/node/pull/58356)
* \[[`b94f63b865`](https://github.com/nodejs/node/commit/b94f63b865)] - **module**: handle instantiated async module jobs in require(esm) (Joyee Cheung) [#58067](https://github.com/nodejs/node/pull/58067)
* \[[`714b706f2e`](https://github.com/nodejs/node/commit/714b706f2e)] - **repl**: add proper vertical cursor movements (Giovanni Bucci) [#58003](https://github.com/nodejs/node/pull/58003)
* \[[`629a954477`](https://github.com/nodejs/node/commit/629a954477)] - **repl**: add possibility to edit multiline commands while adding them (Giovanni Bucci) [#58003](https://github.com/nodejs/node/pull/58003)
* \[[`17746129f3`](https://github.com/nodejs/node/commit/17746129f3)] - **sqlite**: set `name` and `length` on `sqlite.backup()` (Livia Medeiros) [#58251](https://github.com/nodejs/node/pull/58251)
* \[[`908782b1c0`](https://github.com/nodejs/node/commit/908782b1c0)] - **sqlite**: add build option to build without sqlite (Michael Dawson) [#58122](https://github.com/nodejs/node/pull/58122)
* \[[`a92a4074e4`](https://github.com/nodejs/node/commit/a92a4074e4)] - **src**: remove unused `internalVerifyIntegrity` internal binding (Dario Piotrowicz) [#58285](https://github.com/nodejs/node/pull/58285)
* \[[`e0355b71ba`](https://github.com/nodejs/node/commit/e0355b71ba)] - **src**: add a variant of ToV8Value() for primitive arrays (Aditi) [#57576](https://github.com/nodejs/node/pull/57576)
* \[[`cb24fc71c4`](https://github.com/nodejs/node/commit/cb24fc71c4)] - **src**: remove unused `checkMessagePort` internal binding (Dario Piotrowicz) [#58267](https://github.com/nodejs/node/pull/58267)
* \[[`4db5d0bc49`](https://github.com/nodejs/node/commit/4db5d0bc49)] - **src**: remove unused `shouldRetryAsESM` internal binding (Dario Piotrowicz) [#58265](https://github.com/nodejs/node/pull/58265)
* \[[`3b8d4e32ca`](https://github.com/nodejs/node/commit/3b8d4e32ca)] - **src**: add a couple fast apis in node\_os (James M Snell) [#58210](https://github.com/nodejs/node/pull/58210)
* \[[`a135c0aea3`](https://github.com/nodejs/node/commit/a135c0aea3)] - **src**: remove unneeded explicit V8 flags (Michaël Zasso) [#58230](https://github.com/nodejs/node/pull/58230)
* \[[`abeb5c4cdc`](https://github.com/nodejs/node/commit/abeb5c4cdc)] - **src**: fix module buffer allocation (X-BW) [#57738](https://github.com/nodejs/node/pull/57738)
* \[[`9ca4b46eb3`](https://github.com/nodejs/node/commit/9ca4b46eb3)] - **src**: use String::WriteV2() in TwoByteValue (Tobias Nießen) [#58164](https://github.com/nodejs/node/pull/58164)
* \[[`bb28e2bfd7`](https://github.com/nodejs/node/commit/bb28e2bfd7)] - **src**: remove overzealous tcsetattr error check (Ben Noordhuis) [#58200](https://github.com/nodejs/node/pull/58200)
* \[[`329e008e73`](https://github.com/nodejs/node/commit/329e008e73)] - **src**: refactor WriteUCS2 and remove flags argument (Tobias Nießen) [#58163](https://github.com/nodejs/node/pull/58163)
* \[[`c815f29d61`](https://github.com/nodejs/node/commit/c815f29d61)] - **src**: remove NonCopyableMaybe (Tobias Nießen) [#58168](https://github.com/nodejs/node/pull/58168)
* \[[`685d137dec`](https://github.com/nodejs/node/commit/685d137dec)] - **test**: reduce iteration count in test-child-process-stdout-flush-exit (Antoine du Hamel) [#58273](https://github.com/nodejs/node/pull/58273)
* \[[`40dc092e25`](https://github.com/nodejs/node/commit/40dc092e25)] - **test**: remove unnecessary `console.log` from test-repl-null-thrown (Dario Piotrowicz) [#58281](https://github.com/nodejs/node/pull/58281)
* \[[`a3af644dda`](https://github.com/nodejs/node/commit/a3af644dda)] - **test**: allow `tmpDir.path` to be modified (Aviv Keller) [#58173](https://github.com/nodejs/node/pull/58173)
* \[[`97f80374a6`](https://github.com/nodejs/node/commit/97f80374a6)] - **test**: add `Float16Array` to `common.getArrayBufferViews()` (Livia Medeiros) [#58233](https://github.com/nodejs/node/pull/58233)
* \[[`65683735ab`](https://github.com/nodejs/node/commit/65683735ab)] - **test**: fix executable flags (Livia Medeiros) [#58250](https://github.com/nodejs/node/pull/58250)
* \[[`ebb82aa1c3`](https://github.com/nodejs/node/commit/ebb82aa1c3)] - **test**: deflake test-http2-client-socket-destroy (Luigi Pinca) [#58212](https://github.com/nodejs/node/pull/58212)
* \[[`eb4f130b17`](https://github.com/nodejs/node/commit/eb4f130b17)] - **test**: remove Float16Array flag (Livia Medeiros) [#58184](https://github.com/nodejs/node/pull/58184)
* \[[`09a85fdeb1`](https://github.com/nodejs/node/commit/09a85fdeb1)] - **test**: skip test-buffer-tostring-rangeerror when low on memory (Ruben Bridgewater) [#58142](https://github.com/nodejs/node/pull/58142)
* \[[`65446632b1`](https://github.com/nodejs/node/commit/65446632b1)] - **test**: reduce flakiness in test-heapdump-http2 (Joyee Cheung) [#58148](https://github.com/nodejs/node/pull/58148)
* \[[`f7041b9369`](https://github.com/nodejs/node/commit/f7041b9369)] - _**Revert**_ "**test\_runner**: change ts default glob" (Théo LUDWIG) [#58202](https://github.com/nodejs/node/pull/58202)
* \[[`287454298d`](https://github.com/nodejs/node/commit/287454298d)] - **test\_runner**: unify --require and --import behavior when isolation none (Pietro Marchini) [#57924](https://github.com/nodejs/node/pull/57924)
* \[[`6301b003f7`](https://github.com/nodejs/node/commit/6301b003f7)] - **tools**: ignore `deps/` and `benchmark/` for CodeQL (Rafael Gonzaga) [#58254](https://github.com/nodejs/node/pull/58254)
* \[[`2d5de7e309`](https://github.com/nodejs/node/commit/2d5de7e309)] - **tools**: add read permission to workflows that read contents (Antoine du Hamel) [#58255](https://github.com/nodejs/node/pull/58255)
* \[[`b8d4715527`](https://github.com/nodejs/node/commit/b8d4715527)] - **tools**: support environment variables via comments (Pietro Marchini) [#58186](https://github.com/nodejs/node/pull/58186)
* \[[`d8e88f2c17`](https://github.com/nodejs/node/commit/d8e88f2c17)] - **typings**: add missing typings for `TypedArray` (Jason Zhang) [#58248](https://github.com/nodejs/node/pull/58248)
* \[[`4c6f73c5d5`](https://github.com/nodejs/node/commit/4c6f73c5d5)] - **url**: improve performance of the format function (Giovanni Bucci) [#57099](https://github.com/nodejs/node/pull/57099)
* \[[`94c720c4ee`](https://github.com/nodejs/node/commit/94c720c4ee)] - **util**: add internal `assignFunctionName()` function (LiviaMedeiros) [#57916](https://github.com/nodejs/node/pull/57916)
* \[[`3ed159afd1`](https://github.com/nodejs/node/commit/3ed159afd1)] - **watch**: fix watch args not being properly filtered (Dario Piotrowicz) [#58279](https://github.com/nodejs/node/pull/58279)

<a id="24.0.2"></a>

## 2025-05-14, Version 24.0.2 (Current), @RafaelGSS

This is a security release.

### Notable Changes

* (CVE-2025-23166) fix error handling on async crypto operation

### Commits

* \[[`7d0c17b2ad`](https://github.com/nodejs/node/commit/7d0c17b2ad)] - **(CVE-2025-23166)** **src**: fix error handling on async crypto operations (RafaelGSS) [nodejs-private/node-private#688](https://github.com/nodejs-private/node-private/pull/688)

<a id="24.0.1"></a>

## 2025-05-08, Version 24.0.1 (Current), @aduh95

### Notable Changes

* \[[`2e1d9581e0`](https://github.com/nodejs/node/commit/2e1d9581e0)] - _**Revert**_ "**buffer**: move SlowBuffer to EOL" (Filip Skokan) [#58211](https://github.com/nodejs/node/pull/58211)

### Commits

* \[[`d38e811c5b`](https://github.com/nodejs/node/commit/d38e811c5b)] - **benchmark**: fix typo in method name for error-stack (Miguel Marcondes Filho) [#58128](https://github.com/nodejs/node/pull/58128)
* \[[`2e1d9581e0`](https://github.com/nodejs/node/commit/2e1d9581e0)] - _**Revert**_ "**buffer**: move SlowBuffer to EOL" (Filip Skokan) [#58211](https://github.com/nodejs/node/pull/58211)
* \[[`a883b0c979`](https://github.com/nodejs/node/commit/a883b0c979)] - **build**: use //third\_party/simdutf by default in GN (Shelley Vohr) [#58115](https://github.com/nodejs/node/pull/58115)
* \[[`3d84b5c7a4`](https://github.com/nodejs/node/commit/3d84b5c7a4)] - **doc**: add HBSPS as triager (Wiyeong Seo) [#57980](https://github.com/nodejs/node/pull/57980)
* \[[`1e57cb686e`](https://github.com/nodejs/node/commit/1e57cb686e)] - **doc**: add history entries to `--input-type` section (Antoine du Hamel) [#58175](https://github.com/nodejs/node/pull/58175)
* \[[`0b54f06b6f`](https://github.com/nodejs/node/commit/0b54f06b6f)] - **doc**: add ambassaor message (Brian Muenzenmeyer) [#57600](https://github.com/nodejs/node/pull/57600)
* \[[`46bee52d57`](https://github.com/nodejs/node/commit/46bee52d57)] - **doc**: fix misaligned options in vm.compileFunction() (Jimmy Leung) [#58145](https://github.com/nodejs/node/pull/58145)
* \[[`e732a8bfdd`](https://github.com/nodejs/node/commit/e732a8bfdd)] - **doc**: fix typo in benchmark script path (Miguel Marcondes Filho) [#58129](https://github.com/nodejs/node/pull/58129)
* \[[`d49ff34adb`](https://github.com/nodejs/node/commit/d49ff34adb)] - **doc**: add missing options.signal to readlinePromises.createInterface() (Jimmy Leung) [#55456](https://github.com/nodejs/node/pull/55456)
* \[[`bc9f5a2e79`](https://github.com/nodejs/node/commit/bc9f5a2e79)] - **doc**: fix typo of file `zlib.md` (yusheng chen) [#58093](https://github.com/nodejs/node/pull/58093)
* \[[`c8e8558958`](https://github.com/nodejs/node/commit/c8e8558958)] - **doc**: clarify future Corepack removal in v25+ (Trivikram Kamat) [#57825](https://github.com/nodejs/node/pull/57825)
* \[[`b13d5d14bd`](https://github.com/nodejs/node/commit/b13d5d14bd)] - **meta**: bump actions/setup-node from 4.3.0 to 4.4.0 (dependabot\[bot]) [#58111](https://github.com/nodejs/node/pull/58111)
* \[[`0ebb17a300`](https://github.com/nodejs/node/commit/0ebb17a300)] - **meta**: bump actions/setup-python from 5.5.0 to 5.6.0 (dependabot\[bot]) [#58107](https://github.com/nodejs/node/pull/58107)
* \[[`5946785bf4`](https://github.com/nodejs/node/commit/5946785bf4)] - **tools**: exclude deps/v8/tools from CodeQL scans (Rich Trott) [#58132](https://github.com/nodejs/node/pull/58132)
* \[[`0708497c7f`](https://github.com/nodejs/node/commit/0708497c7f)] - **tools**: bump the eslint group in /tools/eslint with 6 updates (dependabot\[bot]) [#58105](https://github.com/nodejs/node/pull/58105)

<a id="24.0.0"></a>

## 2025-05-06, Version 24.0.0 (Current), @RafaelGSS and @juanarbol

We’re excited to announce the release of Node.js 24! This release brings
several significant updates, including the upgrade of the V8 JavaScript
engine to version 13.6 and npm to version 11. Starting with
Node.js 24, support for MSVC has been removed, and ClangCL is now required
to compile Node.js on Windows. The `AsyncLocalStorage` API now uses
`AsyncContextFrame` by default, and `URLPattern` is available globally.
These changes, along with many other improvements, continue to push the
platform forward.

As a reminder, Node.js 24 will enter long-term support (LTS) in October,
but until then, it will be the "Current" release for the next six months.
We encourage you to explore the new features and benefits offered by this
latest release and evaluate their potential impact on your applications.

## Notable Changes

### V8 13.6

The V8 engine is updated to version 13.6, which includes several new
JavaScript features:

* [`Float16Array`](https://chromestatus.com/feature/5164400693215232)
* [Explicit resource management](https://tc39.es/proposal-explicit-resource-management/)
* [`RegExp.escape`](https://github.com/tc39/proposal-regex-escaping)
* [WebAssembly Memory64](https://github.com/WebAssembly/memory64)
* [`Error.isError`](https://github.com/tc39/proposal-is-error)

The V8 update was a contribution by Michaël Zasso in [#58070](https://github.com/nodejs/node/pull/58070).

### npm 11

Node.js 24 comes with npm 11, which includes several improvements and new
features. This update brings enhanced performance, improved security features,
and better compatibility with modern JavaScript packages.

The npm update was a contribution by the npm team in [#56274](https://github.com/nodejs/node/pull/56274).

### `AsyncLocalStorage` defaults to `AsyncContextFrame`

`AsyncLocalStorage` now uses `AsyncContextFrame` by default, which provides a
more efficient implementation of asynchronous context tracking.
This change improves performance and makes the API more robust for advanced
use cases.

This change was a contribution by Stephen Belanger in [#55552](https://github.com/nodejs/node/pull/55552).

### `URLPattern` as a global

The [`URLPattern`](https://developer.mozilla.org/en-US/docs/Web/API/URLPattern)
API is now exposed on the global object, making it easier to use without
explicit imports. This API provides a powerful pattern matching system for URLs,
similar to how regular expressions work for strings.

This feature was a contribution by Jonas Badalič in [#56950](https://github.com/nodejs/node/pull/56950).

### Permission Model Improvements

The experimental Permission Model introduced in Node.js 20 has been improved,
and the flag has been changed from `--experimental-permission` to simply
`--permission`, indicating its increasing stability and readiness for broader
adoption.

This change was a contribution by Rafael Gonzaga in [#56240](https://github.com/nodejs/node/pull/56240).

### Test Runner Enhancements

The test runner module now automatically waits for subtests to finish,
eliminating the need to manually await test promises. This makes writing tests
more intuitive and reduces common errors related to unhandled promises.

The test runner improvements were contributions by Colin Ihrig in [#56664](https://github.com/nodejs/node/pull/56664).

### Undici 7

Node.js 24 includes Undici 7, which brings numerous improvements to the
HTTP client capabilities, including better performance and support for newer
HTTP features.

### Deprecations and Removals

Several APIs have been deprecated or removed in this release:

* Runtime deprecation of `url.parse()` - use the WHATWG URL API instead ([#55017](https://github.com/nodejs/node/pull/55017))
* Removal of deprecated `tls.createSecurePair` ([#57361](https://github.com/nodejs/node/pull/57361))
* Runtime deprecation of `SlowBuffer` ([#55175](https://github.com/nodejs/node/pull/55175))
* Runtime deprecation of instantiating REPL without new ([#54869](https://github.com/nodejs/node/pull/54869))
* Deprecation of using Zlib classes without `new` ([#55718](https://github.com/nodejs/node/pull/55718))
* Deprecation of passing `args` to `spawn` and `execFile` in child\_process ([#57199](https://github.com/nodejs/node/pull/57199))

### Semver-Major Commits

* \[[`c6b934380a`](https://github.com/nodejs/node/commit/c6b934380a)] - **(SEMVER-MAJOR)** **src**: enable `Float16Array` on global object (Michaël Zasso) [#58154](https://github.com/nodejs/node/pull/58154)
* \[[`69efb81a73`](https://github.com/nodejs/node/commit/69efb81a73)] - **(SEMVER-MAJOR)** **src**: enable explicit resource management (Michaël Zasso) [#58154](https://github.com/nodejs/node/pull/58154)
* \[[`b00ff4270e`](https://github.com/nodejs/node/commit/b00ff4270e)] - **(SEMVER-MAJOR)** **src,test**: unregister the isolate after disposal and before freeing (Joyee Cheung) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`b81697d860`](https://github.com/nodejs/node/commit/b81697d860)] - **(SEMVER-MAJOR)** **src**: use non-deprecated WriteUtf8V2() method (Yagiz Nizipli) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`1f06169b87`](https://github.com/nodejs/node/commit/1f06169b87)] - **(SEMVER-MAJOR)** **src**: use non-deprecated Utf8LengthV2() method (Yagiz Nizipli) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`eae9a296f0`](https://github.com/nodejs/node/commit/eae9a296f0)] - **(SEMVER-MAJOR)** **src**: use V8-owned CppHeap (Joyee Cheung) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`087c254a11`](https://github.com/nodejs/node/commit/087c254a11)] - **(SEMVER-MAJOR)** **test**: fix test-fs-write for V8 13.6 (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`9e49bedd8e`](https://github.com/nodejs/node/commit/9e49bedd8e)] - **(SEMVER-MAJOR)** **build**: update list of installed cppgc headers (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`93cca8a43e`](https://github.com/nodejs/node/commit/93cca8a43e)] - **(SEMVER-MAJOR)** **tools**: update V8 gypfiles for 13.6 (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`347daa07be`](https://github.com/nodejs/node/commit/347daa07be)] - **(SEMVER-MAJOR)** **tools**: update V8 gypfiles for 13.5 (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`2a35d5a86c`](https://github.com/nodejs/node/commit/2a35d5a86c)] - **(SEMVER-MAJOR)** **build**: fix V8 TLS config for shared lib builds (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`b0fb5a09cf`](https://github.com/nodejs/node/commit/b0fb5a09cf)] - **(SEMVER-MAJOR)** **build**: pass `-fPIC` to linker as well for shared builds (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`dd4c5d6c73`](https://github.com/nodejs/node/commit/dd4c5d6c73)] - **(SEMVER-MAJOR)** **src,test**: add V8 API to test the hash seed (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`1d5d7b6eed`](https://github.com/nodejs/node/commit/1d5d7b6eed)] - **(SEMVER-MAJOR)** **src**: use `v8::ExternalMemoryAccounter` (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`3779e43cce`](https://github.com/nodejs/node/commit/3779e43cce)] - **(SEMVER-MAJOR)** **tools**: update license-builder and LICENSE for V8 deps (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`82c2255206`](https://github.com/nodejs/node/commit/82c2255206)] - **(SEMVER-MAJOR)** **deps**: remove deps/simdutf (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`8a258eb7b1`](https://github.com/nodejs/node/commit/8a258eb7b1)] - **(SEMVER-MAJOR)** **test**: handle explicit resource management globals (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`9e0d9b6024`](https://github.com/nodejs/node/commit/9e0d9b6024)] - **(SEMVER-MAJOR)** **test**: adapt assert tests to stack trace changes (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`f7406aa56d`](https://github.com/nodejs/node/commit/f7406aa56d)] - **(SEMVER-MAJOR)** **test**: update test-linux-perf-logger (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`c7493fac5e`](https://github.com/nodejs/node/commit/c7493fac5e)] - **(SEMVER-MAJOR)** _**Revert**_ "**test**: disable fast API call count checks" (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`50a8527867`](https://github.com/nodejs/node/commit/50a8527867)] - **(SEMVER-MAJOR)** **src**: replace uses of FastApiTypedArray (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`9c1ebb713c`](https://github.com/nodejs/node/commit/9c1ebb713c)] - **(SEMVER-MAJOR)** **build**: add `/bigobj` to compile V8 on Windows (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`fb3d5ea45d`](https://github.com/nodejs/node/commit/fb3d5ea45d)] - **(SEMVER-MAJOR)** **tools**: update V8 gypfiles for 13.4 (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`756abacf73`](https://github.com/nodejs/node/commit/756abacf73)] - **(SEMVER-MAJOR)** **build,src,tools**: adapt build config for V8 13.3 (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`f8953e54b0`](https://github.com/nodejs/node/commit/f8953e54b0)] - **(SEMVER-MAJOR)** **tools**: update V8 gypfiles for 13.2 (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`c8a0e205e1`](https://github.com/nodejs/node/commit/c8a0e205e1)] - **(SEMVER-MAJOR)** **tools**: update V8 gypfiles for 13.1 (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`1689ee84ce`](https://github.com/nodejs/node/commit/1689ee84ce)] - **(SEMVER-MAJOR)** **build**: enable shared RO heap with ptr compression (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`15f2fb9467`](https://github.com/nodejs/node/commit/15f2fb9467)] - **(SEMVER-MAJOR)** **build**: remove support for s390 32-bit (Richard Lau) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`4ab254c9f2`](https://github.com/nodejs/node/commit/4ab254c9f2)] - **(SEMVER-MAJOR)** **deps**: V8: backport 954187bb1b87 (Joyee Cheung) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`732923b927`](https://github.com/nodejs/node/commit/732923b927)] - **(SEMVER-MAJOR)** **deps**: patch V8 to support compilation with MSVC (StefanStojanovic) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`972834d7c0`](https://github.com/nodejs/node/commit/972834d7c0)] - **(SEMVER-MAJOR)** **deps**: always define V8\_EXPORT\_PRIVATE as no-op (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`7098bff3a9`](https://github.com/nodejs/node/commit/7098bff3a9)] - **(SEMVER-MAJOR)** **deps**: disable V8 concurrent sparkplug compilation (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`dc82c40d4a`](https://github.com/nodejs/node/commit/dc82c40d4a)] - **(SEMVER-MAJOR)** **deps**: use std::map in MSVC STL for EphemeronRememberedSet (Joyee Cheung) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`42f5130ee2`](https://github.com/nodejs/node/commit/42f5130ee2)] - **(SEMVER-MAJOR)** **deps**: patch V8 for illumos (Dan McDonald) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`23b17dbd9e`](https://github.com/nodejs/node/commit/23b17dbd9e)] - **(SEMVER-MAJOR)** **deps**: remove problematic comment from v8-internal (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`c5d71fcdab`](https://github.com/nodejs/node/commit/c5d71fcdab)] - **(SEMVER-MAJOR)** **deps**: define V8\_PRESERVE\_MOST as no-op on Windows (Stefan Stojanovic) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`fbc2005b15`](https://github.com/nodejs/node/commit/fbc2005b15)] - **(SEMVER-MAJOR)** **deps**: fix FP16 bitcasts.h (Stefan Stojanovic) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`57f9430503`](https://github.com/nodejs/node/commit/57f9430503)] - **(SEMVER-MAJOR)** **deps**: patch V8 to avoid duplicated zlib symbol (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`f26cab1b85`](https://github.com/nodejs/node/commit/f26cab1b85)] - **(SEMVER-MAJOR)** **src**: update NODE\_MODULE\_VERSION to 137 (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`f8923a4f17`](https://github.com/nodejs/node/commit/f8923a4f17)] - **(SEMVER-MAJOR)** **build**: reset embedder string to "-node.0" (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`c7964bc02b`](https://github.com/nodejs/node/commit/c7964bc02b)] - **(SEMVER-MAJOR)** **deps**: update V8 to 13.6.233.8 (Michaël Zasso) [#58070](https://github.com/nodejs/node/pull/58070)
* \[[`6682861d6f`](https://github.com/nodejs/node/commit/6682861d6f)] - **(SEMVER-MAJOR)** **build**: downgrade armv7 support to experimental (Michaël Zasso) [#58071](https://github.com/nodejs/node/pull/58071)
* \[[`0579e0ec93`](https://github.com/nodejs/node/commit/0579e0ec93)] - **(SEMVER-MAJOR)** **buffer**: move SlowBuffer to EOL (James M Snell) [#58008](https://github.com/nodejs/node/pull/58008)
* \[[`a55f5d5e63`](https://github.com/nodejs/node/commit/a55f5d5e63)] - **(SEMVER-MAJOR)** **readline**: add stricter validation for functions called after closed (Dario Piotrowicz) [#57680](https://github.com/nodejs/node/pull/57680)
* \[[`d16b0bae55`](https://github.com/nodejs/node/commit/d16b0bae55)] - **(SEMVER-MAJOR)** **http2**: session tracking and graceful server close (Kushagra Pandey) [#57586](https://github.com/nodejs/node/pull/57586)
* \[[`e2b94dc3f9`](https://github.com/nodejs/node/commit/e2b94dc3f9)] - **(SEMVER-MAJOR)** **readline**: fix unicode line separators being ignored (Dario Piotrowicz) [#57591](https://github.com/nodejs/node/pull/57591)
* \[[`4a47ce5ff9`](https://github.com/nodejs/node/commit/4a47ce5ff9)] - **(SEMVER-MAJOR)** _**Revert**_ "**assert,util**: revert recursive breaking change" (Ruben Bridgewater) [#57622](https://github.com/nodejs/node/pull/57622)
* \[[`7d4db69049`](https://github.com/nodejs/node/commit/7d4db69049)] - **(SEMVER-MAJOR)** **http**: remove outgoingmessage \_headers and \_headersList (Yagiz Nizipli) [#57551](https://github.com/nodejs/node/pull/57551)
* \[[`fabf9384e0`](https://github.com/nodejs/node/commit/fabf9384e0)] - **(SEMVER-MAJOR)** **fs**: remove ability to call truncate with fd (Yagiz Nizipli) [#57567](https://github.com/nodejs/node/pull/57567)
* \[[`a587bd2ee2`](https://github.com/nodejs/node/commit/a587bd2ee2)] - **(SEMVER-MAJOR)** **net**: make \_setSimultaneousAccepts() end-of-life deprecated (Yagiz Nizipli) [#57550](https://github.com/nodejs/node/pull/57550)
* \[[`c6bca3fd34`](https://github.com/nodejs/node/commit/c6bca3fd34)] - **(SEMVER-MAJOR)** **child\_process**: deprecate passing `args` to `spawn` and `execFile` (Daniel Venable) [#57199](https://github.com/nodejs/node/pull/57199)
* \[[`e42c01b56d`](https://github.com/nodejs/node/commit/e42c01b56d)] - **(SEMVER-MAJOR)** **buffer**: make `buflen` in integer range (zhenweijin) [#51821](https://github.com/nodejs/node/pull/51821)
* \[[`cc08ad56b8`](https://github.com/nodejs/node/commit/cc08ad56b8)] - **(SEMVER-MAJOR)** **tls**: remove deprecated tls.createSecurePair (Jonas) [#57361](https://github.com/nodejs/node/pull/57361)
* \[[`6f2a6b262b`](https://github.com/nodejs/node/commit/6f2a6b262b)] - **(SEMVER-MAJOR)** **tls**: make server.prototype.setOptions end-of-life (Yagiz Nizipli) [#57339](https://github.com/nodejs/node/pull/57339)
* \[[`0c371d919e`](https://github.com/nodejs/node/commit/0c371d919e)] - **(SEMVER-MAJOR)** **lib**: remove obsolete Cipher export (James M Snell) [#57266](https://github.com/nodejs/node/pull/57266)
* \[[`2cbf3c38db`](https://github.com/nodejs/node/commit/2cbf3c38db)] - **(SEMVER-MAJOR)** **timers**: check for immediate instance in clearImmediate (Gürgün Dayıoğlu) [#57069](https://github.com/nodejs/node/pull/57069)
* \[[`4f512faf4a`](https://github.com/nodejs/node/commit/4f512faf4a)] - **(SEMVER-MAJOR)** **lib**: unexpose six process bindings (Michaël Zasso) [#57149](https://github.com/nodejs/node/pull/57149)
* \[[`8b40221777`](https://github.com/nodejs/node/commit/8b40221777)] - **(SEMVER-MAJOR)** **build**: bump supported macOS version to 13.5 (Michaël Zasso) [#57115](https://github.com/nodejs/node/pull/57115)
* \[[`5d7091f1bc`](https://github.com/nodejs/node/commit/5d7091f1bc)] - **(SEMVER-MAJOR)** **timers**: set several methods EOL (Yagiz Nizipli) [#56966](https://github.com/nodejs/node/pull/56966)
* \[[`d1f8ccb10d`](https://github.com/nodejs/node/commit/d1f8ccb10d)] - **(SEMVER-MAJOR)** **url**: expose urlpattern as global (Jonas) [#56950](https://github.com/nodejs/node/pull/56950)
* \[[`ed52ab913b`](https://github.com/nodejs/node/commit/ed52ab913b)] - **(SEMVER-MAJOR)** **build**: increase minimum Xcode version to 16.1 (Michaël Zasso) [#56824](https://github.com/nodejs/node/pull/56824)
* \[[`1a2eb15bc6`](https://github.com/nodejs/node/commit/1a2eb15bc6)] - **(SEMVER-MAJOR)** **test\_runner**: remove promises returned by t.test() (Colin Ihrig) [#56664](https://github.com/nodejs/node/pull/56664)
* \[[`96718268fe`](https://github.com/nodejs/node/commit/96718268fe)] - **(SEMVER-MAJOR)** **test\_runner**: remove promises returned by test() (Colin Ihrig) [#56664](https://github.com/nodejs/node/pull/56664)
* \[[`aa3523ec22`](https://github.com/nodejs/node/commit/aa3523ec22)] - **(SEMVER-MAJOR)** **test\_runner**: automatically wait for subtests to finish (Colin Ihrig) [#56664](https://github.com/nodejs/node/pull/56664)
* \[[`6857dbc018`](https://github.com/nodejs/node/commit/6857dbc018)] - **(SEMVER-MAJOR)** **test**: disable fast API call count checks (Michaël Zasso) [#55014](https://github.com/nodejs/node/pull/55014)
* \[[`89f661dd66`](https://github.com/nodejs/node/commit/89f661dd66)] - **(SEMVER-MAJOR)** **build**: link V8 with atomic library (Michaël Zasso) [#55014](https://github.com/nodejs/node/pull/55014)
* \[[`7e8752006a`](https://github.com/nodejs/node/commit/7e8752006a)] - **(SEMVER-MAJOR)** **src**: update GetForegroundTaskRunner override (Etienne Pierre-doray) [#55014](https://github.com/nodejs/node/pull/55014)
* \[[`44b0e423dc`](https://github.com/nodejs/node/commit/44b0e423dc)] - **(SEMVER-MAJOR)** **build**: remove support for ppc 32-bit (Michaël Zasso) [#55014](https://github.com/nodejs/node/pull/55014)
* \[[`6f965260dd`](https://github.com/nodejs/node/commit/6f965260dd)] - **(SEMVER-MAJOR)** **tools**: update V8 gypfiles for 13.0 (Michaël Zasso) [#55014](https://github.com/nodejs/node/pull/55014)
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
* \[[`7917b67313`](https://github.com/nodejs/node/commit/7917b67313)] - **(SEMVER-MAJOR)** **src**: update NODE\_MODULE\_VERSION to 134 (Michaël Zasso) [#55014](https://github.com/nodejs/node/pull/55014)
* \[[`1f654e655c`](https://github.com/nodejs/node/commit/1f654e655c)] - **(SEMVER-MAJOR)** **build**: reset embedder string to "-node.0" (Michaël Zasso) [#55014](https://github.com/nodejs/node/pull/55014)
* \[[`5edec0e39a`](https://github.com/nodejs/node/commit/5edec0e39a)] - **(SEMVER-MAJOR)** **deps**: update V8 to 13.0.245.25 (Michaël Zasso) [#55014](https://github.com/nodejs/node/pull/55014)
* \[[`25b22e4754`](https://github.com/nodejs/node/commit/25b22e4754)] - **(SEMVER-MAJOR)** **deps**: upgrade npm to 11.0.0 (npm team) [#56274](https://github.com/nodejs/node/pull/56274)
* \[[`529b56ef9d`](https://github.com/nodejs/node/commit/529b56ef9d)] - **(SEMVER-MAJOR)** **fs**: deprecate passing invalid types in `fs.existsSync` (Carlos Espa) [#55753](https://github.com/nodejs/node/pull/55753)
* \[[`bf3bc4ec2f`](https://github.com/nodejs/node/commit/bf3bc4ec2f)] - **(SEMVER-MAJOR)** **src**: drop --experimental-permission in favour of --permission (Rafael Gonzaga) [#56240](https://github.com/nodejs/node/pull/56240)
* \[[`fd8de670da`](https://github.com/nodejs/node/commit/fd8de670da)] - **(SEMVER-MAJOR)** **stream**: catch and forward error from dest.write (jakecastelli) [#55270](https://github.com/nodejs/node/pull/55270)
* \[[`47b80c293d`](https://github.com/nodejs/node/commit/47b80c293d)] - **(SEMVER-MAJOR)** **deps**: update undici to 7.0.0 (Node.js GitHub Bot) [#56070](https://github.com/nodejs/node/pull/56070)
* \[[`58982d712b`](https://github.com/nodejs/node/commit/58982d712b)] - **(SEMVER-MAJOR)** **src**: add async context frame to AsyncResource (Gerhard Stöbich) [#56082](https://github.com/nodejs/node/pull/56082)
* \[[`4ee87b8bc3`](https://github.com/nodejs/node/commit/4ee87b8bc3)] - **(SEMVER-MAJOR)** **zlib**: deprecate classes usage without `new` (Yagiz Nizipli) [#55718](https://github.com/nodejs/node/pull/55718)
* \[[`b02cd411c2`](https://github.com/nodejs/node/commit/b02cd411c2)] - **(SEMVER-MAJOR)** **fs**: runtime deprecate `fs.F_OK`, `fs.R_OK`, `fs.W_OK`, `fs.X_OK` (Livia Medeiros) [#49686](https://github.com/nodejs/node/pull/49686)
* \[[`d9540b51eb`](https://github.com/nodejs/node/commit/d9540b51eb)] - **(SEMVER-MAJOR)** **fs**: remove `dirent.path` (Antoine du Hamel) [#55548](https://github.com/nodejs/node/pull/55548)
* \[[`0368f2f662`](https://github.com/nodejs/node/commit/0368f2f662)] - **(SEMVER-MAJOR)** **repl**: runtime deprecate instantiating without new (Aviv Keller) [#54869](https://github.com/nodejs/node/pull/54869)
* \[[`03dcd7077a`](https://github.com/nodejs/node/commit/03dcd7077a)] - **(SEMVER-MAJOR)** **src**: nuke deprecated and un-used enum members in `OptionEnvvarSettings` (Juan José) [#53079](https://github.com/nodejs/node/pull/53079)
* \[[`51ae57673d`](https://github.com/nodejs/node/commit/51ae57673d)] - **(SEMVER-MAJOR)** **lib**: make ALS default to AsyncContextFrame (Stephen Belanger) [#55552](https://github.com/nodejs/node/pull/55552)
* \[[`11fbdd8c9d`](https://github.com/nodejs/node/commit/11fbdd8c9d)] - **(SEMVER-MAJOR)** **url**: runtime deprecate url.parse (Yagiz Nizipli) [#55017](https://github.com/nodejs/node/pull/55017)
* \[[`019efe1453`](https://github.com/nodejs/node/commit/019efe1453)] - **(SEMVER-MAJOR)** **lib**: runtime deprecate SlowBuffer (Rafael Gonzaga) [#55175](https://github.com/nodejs/node/pull/55175)

### Semver-Minor Commits

* \[[`bf9f25719a`](https://github.com/nodejs/node/commit/bf9f25719a)] - **(SEMVER-MINOR)** **esm**: graduate import.meta properties (James M Snell) [#58011](https://github.com/nodejs/node/pull/58011)
* \[[`947c6a4405`](https://github.com/nodejs/node/commit/947c6a4405)] - **(SEMVER-MINOR)** **src**: add ExecutionAsyncId getter for any Context (Attila Szegedi) [#57820](https://github.com/nodejs/node/pull/57820)
* \[[`ea04184328`](https://github.com/nodejs/node/commit/ea04184328)] - **(SEMVER-MINOR)** **worker**: add worker.getHeapStatistics() (Matteo Collina) [#57888](https://github.com/nodejs/node/pull/57888)
* \[[`ec79f7686d`](https://github.com/nodejs/node/commit/ec79f7686d)] - **(SEMVER-MINOR)** **util**: add `types.isFloat16Array()` (Livia Medeiros) [#57879](https://github.com/nodejs/node/pull/57879)
* \[[`13dee58d0e`](https://github.com/nodejs/node/commit/13dee58d0e)] - **(SEMVER-MINOR)** **test\_runner**: add global setup and teardown functionality (Pietro Marchini) [#57438](https://github.com/nodejs/node/pull/57438)
* \[[`932c2d9c70`](https://github.com/nodejs/node/commit/932c2d9c70)] - **(SEMVER-MINOR)** **stream**: preserve AsyncLocalStorage context in finished() (Gürgün Dayıoğlu) [#57865](https://github.com/nodejs/node/pull/57865)
* \[[`18d6249580`](https://github.com/nodejs/node/commit/18d6249580)] - **(SEMVER-MINOR)** **repl**: add support for multiline history (Giovanni Bucci) [#57400](https://github.com/nodejs/node/pull/57400)
* \[[`c3e44342d9`](https://github.com/nodejs/node/commit/c3e44342d9)] - **(SEMVER-MINOR)** **lib**: add defaultValue and name options to AsyncLocalStorage (James M Snell) [#57766](https://github.com/nodejs/node/pull/57766)
* \[[`f99f815641`](https://github.com/nodejs/node/commit/f99f815641)] - **(SEMVER-MINOR)** **doc**: graduate multiple experimental apis (James M Snell) [#57765](https://github.com/nodejs/node/pull/57765)
* \[[`21f3c96199`](https://github.com/nodejs/node/commit/21f3c96199)] - **(SEMVER-MINOR)** **esm**: support top-level Wasm without package type (Guy Bedford) [#57610](https://github.com/nodejs/node/pull/57610)
* \[[`ada34bd0ea`](https://github.com/nodejs/node/commit/ada34bd0ea)] - **(SEMVER-MINOR)** **http**: support http proxy for fetch under NODE\_USE\_ENV\_PROXY (Joyee Cheung) [#57165](https://github.com/nodejs/node/pull/57165)
* \[[`05cf1410b1`](https://github.com/nodejs/node/commit/05cf1410b1)] - **(SEMVER-MINOR)** **assert**: mark partialDeepStrictEqual() as stable (Ruben Bridgewater) [#57370](https://github.com/nodejs/node/pull/57370)
* \[[`57e49ee777`](https://github.com/nodejs/node/commit/57e49ee777)] - **(SEMVER-MINOR)** **esm**: support source phase imports for WebAssembly (Guy Bedford) [#56919](https://github.com/nodejs/node/pull/56919)
* \[[`55413004c8`](https://github.com/nodejs/node/commit/55413004c8)] - **(SEMVER-MINOR)** **stream**: handle generator destruction from Duplex.from() (Matthieu Sieben) [#55096](https://github.com/nodejs/node/pull/55096)

### Semver-Patch Commits

* \[[`7df9558efc`](https://github.com/nodejs/node/commit/7df9558efc)] - **assert**: support `Float16Array` in loose deep equality checks (Livia Medeiros) [#57881](https://github.com/nodejs/node/pull/57881)
* \[[`d9e78c00c1`](https://github.com/nodejs/node/commit/d9e78c00c1)] - **assert,util**: fix constructor lookup in deep equal comparison (Ruben Bridgewater) [#57876](https://github.com/nodejs/node/pull/57876)
* \[[`f4572f0826`](https://github.com/nodejs/node/commit/f4572f0826)] - **assert,util**: improve deep object comparison performance (Ruben Bridgewater) [#57648](https://github.com/nodejs/node/pull/57648)
* \[[`2e9fb6e1e0`](https://github.com/nodejs/node/commit/2e9fb6e1e0)] - **assert,util**: improve unequal number comparison performance (Ruben Bridgewater) [#57619](https://github.com/nodejs/node/pull/57619)
* \[[`5f9cc5ecbb`](https://github.com/nodejs/node/commit/5f9cc5ecbb)] - **assert,util**: improve array comparison (Ruben Bridgewater) [#57619](https://github.com/nodejs/node/pull/57619)
* \[[`b5b192314c`](https://github.com/nodejs/node/commit/b5b192314c)] - **async\_hooks**: enable AsyncLocalStorage once constructed (Chengzhong Wu) [#58029](https://github.com/nodejs/node/pull/58029)
* \[[`442b4162fb`](https://github.com/nodejs/node/commit/442b4162fb)] - **benchmark**: add sqlite prepare select get (Vinícius Lourenço) [#58040](https://github.com/nodejs/node/pull/58040)
* \[[`2d894eacae`](https://github.com/nodejs/node/commit/2d894eacae)] - **benchmark**: add sqlite prepare select all (Vinícius Lourenço) [#58040](https://github.com/nodejs/node/pull/58040)
* \[[`4d47f3afef`](https://github.com/nodejs/node/commit/4d47f3afef)] - **benchmark**: add sqlite is transaction (Vinícius Lourenço) [#58040](https://github.com/nodejs/node/pull/58040)
* \[[`85f2bbc02b`](https://github.com/nodejs/node/commit/85f2bbc02b)] - **benchmark**: add sqlite prepare insert (Vinícius Lourenço) [#58040](https://github.com/nodejs/node/pull/58040)
* \[[`e61b38e47d`](https://github.com/nodejs/node/commit/e61b38e47d)] - **benchmark**: disambiguate `filename` and `dirname` read perf (Antoine du Hamel) [#58056](https://github.com/nodejs/node/pull/58056)
* \[[`ca86c93390`](https://github.com/nodejs/node/commit/ca86c93390)] - **buffer**: avoid creating unnecessary environment (Yagiz Nizipli) [#58053](https://github.com/nodejs/node/pull/58053)
* \[[`dc22890dd8`](https://github.com/nodejs/node/commit/dc22890dd8)] - **buffer**: improve byteLength performance (Yagiz Nizipli) [#58048](https://github.com/nodejs/node/pull/58048)
* \[[`619bf86fe9`](https://github.com/nodejs/node/commit/619bf86fe9)] - **buffer**: define global v8::CFunction objects as const (Mert Can Altin) [#57676](https://github.com/nodejs/node/pull/57676)
* \[[`d24414ceec`](https://github.com/nodejs/node/commit/d24414ceec)] - **build**: use `$(BUILDTYPE)` when cleaning coverage files (Aviv Keller) [#57995](https://github.com/nodejs/node/pull/57995)
* \[[`004913992c`](https://github.com/nodejs/node/commit/004913992c)] - **build**: define python when generating `out/Makefile` (Aviv Keller) [#57970](https://github.com/nodejs/node/pull/57970)
* \[[`77d11f9c7c`](https://github.com/nodejs/node/commit/77d11f9c7c)] - **build**: fix zstd libname (Antoine du Hamel) [#57999](https://github.com/nodejs/node/pull/57999)
* \[[`74473af8ee`](https://github.com/nodejs/node/commit/74473af8ee)] - **build**: use clang-cl in coverage-windows workflow (Michaël Zasso) [#57919](https://github.com/nodejs/node/pull/57919)
* \[[`46fc497e7b`](https://github.com/nodejs/node/commit/46fc497e7b)] - **build**: fix missing files warning (Luigi Pinca) [#57870](https://github.com/nodejs/node/pull/57870)
* \[[`403264c02e`](https://github.com/nodejs/node/commit/403264c02e)] - **build**: remove redundant `-mXX` flags for V8 (Michaël Zasso) [#57907](https://github.com/nodejs/node/pull/57907)
* \[[`e55b02b368`](https://github.com/nodejs/node/commit/e55b02b368)] - **build**: drop support for python 3.8 (Aviv Keller) [#55239](https://github.com/nodejs/node/pull/55239)
* \[[`234c71077b`](https://github.com/nodejs/node/commit/234c71077b)] - **crypto**: fix cross-realm `SharedArrayBuffer` validation (Antoine du Hamel) [#57974](https://github.com/nodejs/node/pull/57974)
* \[[`14367588d8`](https://github.com/nodejs/node/commit/14367588d8)] - **crypto**: fix cross-realm check of `ArrayBuffer` (Felipe Forbeck) [#57828](https://github.com/nodejs/node/pull/57828)
* \[[`0f55a96e9c`](https://github.com/nodejs/node/commit/0f55a96e9c)] - **crypto**: forbid passing `Float16Array` to `getRandomValues()` (Livia Medeiros) [#57880](https://github.com/nodejs/node/pull/57880)
* \[[`dce6f43a4f`](https://github.com/nodejs/node/commit/dce6f43a4f)] - **crypto**: revert dangerous uses of std::string\_view (Tobias Nießen) [#57816](https://github.com/nodejs/node/pull/57816)
* \[[`fd3fb0c347`](https://github.com/nodejs/node/commit/fd3fb0c347)] - **crypto**: fix misleading positional argument (Tobias Nießen) [#57843](https://github.com/nodejs/node/pull/57843)
* \[[`92aae40dce`](https://github.com/nodejs/node/commit/92aae40dce)] - **crypto**: make auth tag size assumption explicit (Tobias Nießen) [#57803](https://github.com/nodejs/node/pull/57803)
* \[[`4793bb2fdc`](https://github.com/nodejs/node/commit/4793bb2fdc)] - **crypto**: remove CipherBase::Init (Tobias Nießen) [#57787](https://github.com/nodejs/node/pull/57787)
* \[[`e567952388`](https://github.com/nodejs/node/commit/e567952388)] - **crypto**: remove BoringSSL dh-primes addition (Shelley Vohr) [#57023](https://github.com/nodejs/node/pull/57023)
* \[[`270ab65ee4`](https://github.com/nodejs/node/commit/270ab65ee4)] - **deps**: update ada to 3.2.3 (Node.js GitHub Bot) [#58045](https://github.com/nodejs/node/pull/58045)
* \[[`f725127c19`](https://github.com/nodejs/node/commit/f725127c19)] - **deps**: update zstd to 1.5.7 (Node.js GitHub Bot) [#57940](https://github.com/nodejs/node/pull/57940)
* \[[`fd6adb7de6`](https://github.com/nodejs/node/commit/fd6adb7de6)] - **deps**: update simdutf to 6.5.0 (Node.js GitHub Bot) [#57939](https://github.com/nodejs/node/pull/57939)
* \[[`cdedec7e29`](https://github.com/nodejs/node/commit/cdedec7e29)] - **deps**: update undici to 7.8.0 (Node.js GitHub Bot) [#57770](https://github.com/nodejs/node/pull/57770)
* \[[`878dc9337e`](https://github.com/nodejs/node/commit/878dc9337e)] - **deps**: update zlib to 1.3.0.1-motley-780819f (Node.js GitHub Bot) [#57768](https://github.com/nodejs/node/pull/57768)
* \[[`3e885e1441`](https://github.com/nodejs/node/commit/3e885e1441)] - **deps**: update timezone to 2025b (Node.js GitHub Bot) [#57857](https://github.com/nodejs/node/pull/57857)
* \[[`e92e100c9d`](https://github.com/nodejs/node/commit/e92e100c9d)] - **deps**: update amaro to 0.5.2 (Node.js GitHub Bot) [#57871](https://github.com/nodejs/node/pull/57871)
* \[[`afc49db038`](https://github.com/nodejs/node/commit/afc49db038)] - **deps**: update simdutf to 6.4.2 (Node.js GitHub Bot) [#57855](https://github.com/nodejs/node/pull/57855)
* \[[`70bd8bc174`](https://github.com/nodejs/node/commit/70bd8bc174)] - **deps**: delete OpenSSL demos, doc and test folders (Michaël Zasso) [#57835](https://github.com/nodejs/node/pull/57835)
* \[[`40dcd4a3d1`](https://github.com/nodejs/node/commit/40dcd4a3d1)] - **deps**: upgrade npm to 11.3.0 (npm team) [#57801](https://github.com/nodejs/node/pull/57801)
* \[[`678d82b9be`](https://github.com/nodejs/node/commit/678d82b9be)] - **deps**: update c-ares to v1.34.5 (Node.js GitHub Bot) [#57792](https://github.com/nodejs/node/pull/57792)
* \[[`f079c4aa37`](https://github.com/nodejs/node/commit/f079c4aa37)] - **deps**: update simdutf to 6.4.0 (Node.js GitHub Bot) [#56764](https://github.com/nodejs/node/pull/56764)
* \[[`ec29f563a9`](https://github.com/nodejs/node/commit/ec29f563a9)] - **deps**: update ada to 3.2.2 (Yagiz Nizipli) [#57693](https://github.com/nodejs/node/pull/57693)
* \[[`95296d0d84`](https://github.com/nodejs/node/commit/95296d0d84)] - **deps**: update amaro to 0.5.1 (Marco Ippolito) [#57704](https://github.com/nodejs/node/pull/57704)
* \[[`c377394657`](https://github.com/nodejs/node/commit/c377394657)] - **deps**: update undici to 7.6.0 (nodejs-github-bot) [#57685](https://github.com/nodejs/node/pull/57685)
* \[[`a56175561a`](https://github.com/nodejs/node/commit/a56175561a)] - **deps**: update amaro to 0.5.0 (nodejs-github-bot) [#57687](https://github.com/nodejs/node/pull/57687)
* \[[`a86912a462`](https://github.com/nodejs/node/commit/a86912a462)] - **deps**: update icu to 77.1 (Node.js GitHub Bot) [#57455](https://github.com/nodejs/node/pull/57455)
* \[[`0b2cf1b642`](https://github.com/nodejs/node/commit/0b2cf1b642)] - **deps**: update undici to 7.5.0 (Node.js GitHub Bot) [#57427](https://github.com/nodejs/node/pull/57427)
* \[[`c3927aa558`](https://github.com/nodejs/node/commit/c3927aa558)] - **deps**: upgrade npm to 11.2.0 (npm team) [#57334](https://github.com/nodejs/node/pull/57334)
* \[[`9c7bc95f56`](https://github.com/nodejs/node/commit/9c7bc95f56)] - **deps**: update undici to 7.4.0 (Node.js GitHub Bot) [#57236](https://github.com/nodejs/node/pull/57236)
* \[[`9dee7b94bf`](https://github.com/nodejs/node/commit/9dee7b94bf)] - **deps**: update undici to 7.3.0 (Node.js GitHub Bot) [#56624](https://github.com/nodejs/node/pull/56624)
* \[[`cadc4ed067`](https://github.com/nodejs/node/commit/cadc4ed067)] - **deps**: upgrade npm to 11.1.0 (npm team) [#56818](https://github.com/nodejs/node/pull/56818)
* \[[`5770972dc6`](https://github.com/nodejs/node/commit/5770972dc6)] - **deps**: update undici to 7.2.1 (Node.js GitHub Bot) [#56569](https://github.com/nodejs/node/pull/56569)
* \[[`67b647edc7`](https://github.com/nodejs/node/commit/67b647edc7)] - **deps**: update undici to 7.2.0 (Node.js GitHub Bot) [#56335](https://github.com/nodejs/node/pull/56335)
* \[[`6c03beba46`](https://github.com/nodejs/node/commit/6c03beba46)] - **deps**: update undici to 7.1.0 (Node.js GitHub Bot) [#56179](https://github.com/nodejs/node/pull/56179)
* \[[`8b4bacdf1a`](https://github.com/nodejs/node/commit/8b4bacdf1a)] - **dns**: restore dns query cache ttl (Ethan Arrowood) [#57640](https://github.com/nodejs/node/pull/57640)
* \[[`f6a085da3f`](https://github.com/nodejs/node/commit/f6a085da3f)] - **doc**: mark Node.js 18 as End-of-Life (Richard Lau) [#58084](https://github.com/nodejs/node/pull/58084)
* \[[`ca67c002d6`](https://github.com/nodejs/node/commit/ca67c002d6)] - **doc**: add dario-piotrowicz to collaborators (Dario Piotrowicz) [#58102](https://github.com/nodejs/node/pull/58102)
* \[[`cdb3d01194`](https://github.com/nodejs/node/commit/cdb3d01194)] - **doc**: fix formatting of `import.meta.filename` section (Antoine du Hamel) [#58079](https://github.com/nodejs/node/pull/58079)
* \[[`0557d60f41`](https://github.com/nodejs/node/commit/0557d60f41)] - **doc**: fix env variable name in `util.styleText` (Antoine du Hamel) [#58072](https://github.com/nodejs/node/pull/58072)
* \[[`d5783af1fe`](https://github.com/nodejs/node/commit/d5783af1fe)] - **doc**: add returns for https.get (Eng Zer Jun) [#58025](https://github.com/nodejs/node/pull/58025)
* \[[`a2260a4a18`](https://github.com/nodejs/node/commit/a2260a4a18)] - **doc**: fix typo in `buffer.md` (chocolateboy) [#58052](https://github.com/nodejs/node/pull/58052)
* \[[`352df168da`](https://github.com/nodejs/node/commit/352df168da)] - **doc**: reserve module version 136 for Electron 37 (Calvin) [#57979](https://github.com/nodejs/node/pull/57979)
* \[[`ebbbdd15a1`](https://github.com/nodejs/node/commit/ebbbdd15a1)] - **doc**: correct deprecation type of `assert.CallTracker` (René) [#57997](https://github.com/nodejs/node/pull/57997)
* \[[`36b0a296db`](https://github.com/nodejs/node/commit/36b0a296db)] - **doc**: fix `AsyncLocalStorage` example response changes after node v18 (Naor Tedgi (Abu Emma)) [#57969](https://github.com/nodejs/node/pull/57969)
* \[[`8b4adfb439`](https://github.com/nodejs/node/commit/8b4adfb439)] - **doc**: fix linter errors (Antoine du Hamel) [#57987](https://github.com/nodejs/node/pull/57987)
* \[[`626b26d888`](https://github.com/nodejs/node/commit/626b26d888)] - **doc**: mark devtools integration section as active development (Chengzhong Wu) [#57886](https://github.com/nodejs/node/pull/57886)
* \[[`56a808d20b`](https://github.com/nodejs/node/commit/56a808d20b)] - **doc**: fix typo in `module.md` (Alex Schwartz) [#57889](https://github.com/nodejs/node/pull/57889)
* \[[`df90bd9656`](https://github.com/nodejs/node/commit/df90bd9656)] - **doc**: increase z-index of header element (Dario Piotrowicz) [#57851](https://github.com/nodejs/node/pull/57851)
* \[[`74c415d46a`](https://github.com/nodejs/node/commit/74c415d46a)] - **doc**: add missing TS formats for `load` hooks (Antoine du Hamel) [#57837](https://github.com/nodejs/node/pull/57837)
* \[[`ce1b5aabd4`](https://github.com/nodejs/node/commit/ce1b5aabd4)] - **doc**: clarify the multi REPL example (Dario Piotrowicz) [#57759](https://github.com/nodejs/node/pull/57759)
* \[[`deb434e61f`](https://github.com/nodejs/node/commit/deb434e61f)] - **doc**: fix deprecation type for `DEP0148` (Livia Medeiros) [#57785](https://github.com/nodejs/node/pull/57785)
* \[[`a5ef2e8858`](https://github.com/nodejs/node/commit/a5ef2e8858)] - **doc**: list DOMException as a potential error raised by Node.js (Chengzhong Wu) [#57783](https://github.com/nodejs/node/pull/57783)
* \[[`f66a2717ee`](https://github.com/nodejs/node/commit/f66a2717ee)] - **doc**: add missing v0.x changelog entries (Antoine du Hamel) [#57779](https://github.com/nodejs/node/pull/57779)
* \[[`05098668ba`](https://github.com/nodejs/node/commit/05098668ba)] - **doc**: fix typo in writing-docs (Sebastian Beltran) [#57776](https://github.com/nodejs/node/pull/57776)
* \[[`379718e26e`](https://github.com/nodejs/node/commit/379718e26e)] - **doc**: clarify examples section in REPL doc (Dario Piotrowicz) [#57762](https://github.com/nodejs/node/pull/57762)
* \[[`952a212377`](https://github.com/nodejs/node/commit/952a212377)] - **doc**: explicitly state that corepack will be removed in v25+ (Trivikram Kamat) [#57747](https://github.com/nodejs/node/pull/57747)
* \[[`81066717d0`](https://github.com/nodejs/node/commit/81066717d0)] - **doc**: update position type to integer | null in fs (Yukihiro Hasegawa) [#57745](https://github.com/nodejs/node/pull/57745)
* \[[`a00fec62f9`](https://github.com/nodejs/node/commit/a00fec62f9)] - **doc**: allow the $schema property in node.config.json (Remco Haszing) [#57560](https://github.com/nodejs/node/pull/57560)
* \[[`cc848986ad`](https://github.com/nodejs/node/commit/cc848986ad)] - **doc**: update CI instructions (Antoine du Hamel) [#57743](https://github.com/nodejs/node/pull/57743)
* \[[`576a6df5bb`](https://github.com/nodejs/node/commit/576a6df5bb)] - **doc**: update example of using `await` in REPL (Dario Piotrowicz) [#57653](https://github.com/nodejs/node/pull/57653)
* \[[`0a15b00d34`](https://github.com/nodejs/node/commit/0a15b00d34)] - **doc**: add back mention of visa fees to onboarding doc (Darshan Sen) [#57730](https://github.com/nodejs/node/pull/57730)
* \[[`766d9a8eac`](https://github.com/nodejs/node/commit/766d9a8eac)] - **doc**: remove link to `QUIC.md` (Antoine du Hamel) [#57729](https://github.com/nodejs/node/pull/57729)
* \[[`a8da209796`](https://github.com/nodejs/node/commit/a8da209796)] - **doc**: process.execve is only unavailable for Windows (Yaksh Bariya) [#57726](https://github.com/nodejs/node/pull/57726)
* \[[`d066d1fcec`](https://github.com/nodejs/node/commit/d066d1fcec)] - **doc**: mark type stripping as release candidate (Marco Ippolito) [#57705](https://github.com/nodejs/node/pull/57705)
* \[[`35096b7353`](https://github.com/nodejs/node/commit/35096b7353)] - **doc**: clarify `unhandledRejection` events behaviors in process doc (Dario Piotrowicz) [#57654](https://github.com/nodejs/node/pull/57654)
* \[[`27b113dced`](https://github.com/nodejs/node/commit/27b113dced)] - **doc**: improved fetch docs (Alessandro Miliucci) [#57296](https://github.com/nodejs/node/pull/57296)
* \[[`310ccb5b7d`](https://github.com/nodejs/node/commit/310ccb5b7d)] - **doc**: document REPL custom eval arguments (Dario Piotrowicz) [#57690](https://github.com/nodejs/node/pull/57690)
* \[[`44dfbeca23`](https://github.com/nodejs/node/commit/44dfbeca23)] - **doc**: classify Chrome DevTools Protocol as tier 2 (Chengzhong Wu) [#57634](https://github.com/nodejs/node/pull/57634)
* \[[`1e920a06c7`](https://github.com/nodejs/node/commit/1e920a06c7)] - **doc**: mark multiple vm module APIS stable (James M Snell) [#57513](https://github.com/nodejs/node/pull/57513)
* \[[`db770a0b3b`](https://github.com/nodejs/node/commit/db770a0b3b)] - **doc**: correct status of require(esm) warning in v20 changelog (Joyee Cheung) [#57529](https://github.com/nodejs/node/pull/57529)
* \[[`24c460dc0c`](https://github.com/nodejs/node/commit/24c460dc0c)] - **doc**: reserve NMV 135 for Electron 36 (David Sanders) [#57151](https://github.com/nodejs/node/pull/57151)
* \[[`5119049ca6`](https://github.com/nodejs/node/commit/5119049ca6)] - **doc**: fix faulty YAML metadata (Antoine du Hamel) [#56508](https://github.com/nodejs/node/pull/56508)
* \[[`7bedcfd4a2`](https://github.com/nodejs/node/commit/7bedcfd4a2)] - **doc**: fix typo (Alex Yang) [#56125](https://github.com/nodejs/node/pull/56125)
* \[[`069ec1b983`](https://github.com/nodejs/node/commit/069ec1b983)] - **doc**: consolidate history table of CustomEvent (Edigleysson Silva (Edy)) [#55758](https://github.com/nodejs/node/pull/55758)
* \[[`304f164f52`](https://github.com/nodejs/node/commit/304f164f52)] - **doc,build,win**: update docs with clang (Stefan Stojanovic) [#57991](https://github.com/nodejs/node/pull/57991)
* \[[`c4ca0d7ab1`](https://github.com/nodejs/node/commit/c4ca0d7ab1)] - **esm**: avoid `import.meta` setup costs for unused properties (Antoine du Hamel) [#57286](https://github.com/nodejs/node/pull/57286)
* \[[`073d40be42`](https://github.com/nodejs/node/commit/073d40be42)] - **fs**: added test for missing call to uv\_fs\_req\_cleanup (Justin Nietzel) [#57811](https://github.com/nodejs/node/pull/57811)
* \[[`52e4967f45`](https://github.com/nodejs/node/commit/52e4967f45)] - **fs**: add missing call to uv\_fs\_req\_cleanup (Justin Nietzel) [#57811](https://github.com/nodejs/node/pull/57811)
* \[[`3edea66431`](https://github.com/nodejs/node/commit/3edea66431)] - **fs**: improve globSync performance (Rich Trott) [#57725](https://github.com/nodejs/node/pull/57725)
* \[[`b8865dfda5`](https://github.com/nodejs/node/commit/b8865dfda5)] - **fs**: only show deprecation warning when error code matches (Antoine du Hamel) [#56549](https://github.com/nodejs/node/pull/56549)
* \[[`c91ce2120c`](https://github.com/nodejs/node/commit/c91ce2120c)] - **fs**: fix `getDirent().parentPath` when type is `UV_DIRENT_UNKNOWN` (Livia Medeiros) [#55553](https://github.com/nodejs/node/pull/55553)
* \[[`5e9cac2714`](https://github.com/nodejs/node/commit/5e9cac2714)] - **http2**: add raw header array support to h2Session.request() (Tim Perry) [#57917](https://github.com/nodejs/node/pull/57917)
* \[[`924ebcd7f7`](https://github.com/nodejs/node/commit/924ebcd7f7)] - **http2**: use args.This() instead of args.Holder() (Joyee Cheung) [#58004](https://github.com/nodejs/node/pull/58004)
* \[[`a3655645d9`](https://github.com/nodejs/node/commit/a3655645d9)] - **http2**: fix graceful session close (Kushagra Pandey) [#57808](https://github.com/nodejs/node/pull/57808)
* \[[`406b06b046`](https://github.com/nodejs/node/commit/406b06b046)] - **http2**: fix check for `frame->hd.type` (hanguanqiang) [#57644](https://github.com/nodejs/node/pull/57644)
* \[[`8f3aeea613`](https://github.com/nodejs/node/commit/8f3aeea613)] - **http2**: skip writeHead if stream is closed (Shima Ryuhei) [#57686](https://github.com/nodejs/node/pull/57686)
* \[[`398674a25a`](https://github.com/nodejs/node/commit/398674a25a)] - **lib**: avoid StackOverflow on `serializeError` (Chengzhong Wu) [#58075](https://github.com/nodejs/node/pull/58075)
* \[[`4ef6376cff`](https://github.com/nodejs/node/commit/4ef6376cff)] - **lib**: resolve the issue of not adhering to the specified buffer size (0hm☘️🏳️‍⚧️) [#55896](https://github.com/nodejs/node/pull/55896)
* \[[`5edcb28583`](https://github.com/nodejs/node/commit/5edcb28583)] - **lib**: fix AbortSignal.any() with timeout signals (Gürgün Dayıoğlu) [#57867](https://github.com/nodejs/node/pull/57867)
* \[[`68c5954d59`](https://github.com/nodejs/node/commit/68c5954d59)] - **lib**: use Map primordial for ActiveAsyncContextFrame (Gürgün Dayıoğlu) [#57670](https://github.com/nodejs/node/pull/57670)
* \[[`62640750fd`](https://github.com/nodejs/node/commit/62640750fd)] - **meta**: allow penetration testing on live system with prior authorization (Matteo Collina) [#57966](https://github.com/nodejs/node/pull/57966)
* \[[`33803a5fbb`](https://github.com/nodejs/node/commit/33803a5fbb)] - **meta**: fix subsystem in commit title (Luigi Pinca) [#57945](https://github.com/nodejs/node/pull/57945)
* \[[`7e195ec8f8`](https://github.com/nodejs/node/commit/7e195ec8f8)] - **meta**: bump Mozilla-Actions/sccache-action from 0.0.8 to 0.0.9 (dependabot\[bot]) [#57720](https://github.com/nodejs/node/pull/57720)
* \[[`6ab9db9552`](https://github.com/nodejs/node/commit/6ab9db9552)] - **meta**: bump actions/download-artifact from 4.1.9 to 4.2.1 (dependabot\[bot]) [#57719](https://github.com/nodejs/node/pull/57719)
* \[[`f0c84a6aab`](https://github.com/nodejs/node/commit/f0c84a6aab)] - **meta**: bump actions/setup-python from 5.4.0 to 5.5.0 (dependabot\[bot]) [#57718](https://github.com/nodejs/node/pull/57718)
* \[[`eb1a515c99`](https://github.com/nodejs/node/commit/eb1a515c99)] - **meta**: bump peter-evans/create-pull-request from 7.0.7 to 7.0.8 (dependabot\[bot]) [#57717](https://github.com/nodejs/node/pull/57717)
* \[[`89c156d715`](https://github.com/nodejs/node/commit/89c156d715)] - **meta**: bump github/codeql-action from 3.28.10 to 3.28.13 (dependabot\[bot]) [#57716](https://github.com/nodejs/node/pull/57716)
* \[[`8e27c827fa`](https://github.com/nodejs/node/commit/8e27c827fa)] - **meta**: bump actions/cache from 4.2.2 to 4.2.3 (dependabot\[bot]) [#57715](https://github.com/nodejs/node/pull/57715)
* \[[`dd5e580acd`](https://github.com/nodejs/node/commit/dd5e580acd)] - **meta**: bump actions/setup-node from 4.2.0 to 4.3.0 (dependabot\[bot]) [#57714](https://github.com/nodejs/node/pull/57714)
* \[[`4876e1658f`](https://github.com/nodejs/node/commit/4876e1658f)] - **meta**: bump actions/upload-artifact from 4.6.1 to 4.6.2 (dependabot\[bot]) [#57713](https://github.com/nodejs/node/pull/57713)
* \[[`004914722f`](https://github.com/nodejs/node/commit/004914722f)] - **module**: fix incorrect formatting in require(esm) cycle error message (haykam821) [#57453](https://github.com/nodejs/node/pull/57453)
* \[[`a5406899db`](https://github.com/nodejs/node/commit/a5406899db)] - **module**: improve `getPackageType` performance (Dario Piotrowicz) [#57599](https://github.com/nodejs/node/pull/57599)
* \[[`6adbbe2887`](https://github.com/nodejs/node/commit/6adbbe2887)] - **module**: remove unnecessary `readPackage` function (Dario Piotrowicz) [#57596](https://github.com/nodejs/node/pull/57596)
* \[[`1e490aa570`](https://github.com/nodejs/node/commit/1e490aa570)] - **module**: improve typescript error message format (Marco Ippolito) [#57687](https://github.com/nodejs/node/pull/57687)
* \[[`ecd081df82`](https://github.com/nodejs/node/commit/ecd081df82)] - **node-api**: add nested object wrap and napi\_ref test (Chengzhong Wu) [#57981](https://github.com/nodejs/node/pull/57981)
* \[[`b4f6aa8a87`](https://github.com/nodejs/node/commit/b4f6aa8a87)] - **node-api**: convert NewEnv to node\_napi\_env\_\_::New (Vladimir Morozov) [#57834](https://github.com/nodejs/node/pull/57834)
* \[[`8cd98220af`](https://github.com/nodejs/node/commit/8cd98220af)] - **os**: fix netmask format check condition in getCIDR function (Wiyeong Seo) [#57324](https://github.com/nodejs/node/pull/57324)
* \[[`8b83ab39e3`](https://github.com/nodejs/node/commit/8b83ab39e3)] - **process**: disable building execve on IBM i (Abdirahim Musse) [#57883](https://github.com/nodejs/node/pull/57883)
* \[[`9230f22029`](https://github.com/nodejs/node/commit/9230f22029)] - **process**: remove support for undocumented symbol (Antoine du Hamel) [#56552](https://github.com/nodejs/node/pull/56552)
* \[[`5835de65ee`](https://github.com/nodejs/node/commit/5835de65ee)] - **quic**: fix debug log (jakecastelli) [#57689](https://github.com/nodejs/node/pull/57689)
* \[[`14b357940c`](https://github.com/nodejs/node/commit/14b357940c)] - _**Revert**_ "**readline**: add stricter validation for functions called after closed" (Dario Piotrowicz) [#58024](https://github.com/nodejs/node/pull/58024)
* \[[`ab99ee6f4c`](https://github.com/nodejs/node/commit/ab99ee6f4c)] - **repl**: fix multiline history editing string order (Giovanni Bucci) [#57874](https://github.com/nodejs/node/pull/57874)
* \[[`160da87484`](https://github.com/nodejs/node/commit/160da87484)] - **repl**: deprecate `repl.builtinModules` (Dario Piotrowicz) [#57508](https://github.com/nodejs/node/pull/57508)
* \[[`10eb2b079e`](https://github.com/nodejs/node/commit/10eb2b079e)] - **sqlite**: add location method (Edy Silva) [#57860](https://github.com/nodejs/node/pull/57860)
* \[[`da05addc5e`](https://github.com/nodejs/node/commit/da05addc5e)] - **sqlite**: add getter to detect transactions (Colin Ihrig) [#57925](https://github.com/nodejs/node/pull/57925)
* \[[`0df87e07a0`](https://github.com/nodejs/node/commit/0df87e07a0)] - **sqlite**: add timeout options to DatabaseSync (Edy Silva) [#57752](https://github.com/nodejs/node/pull/57752)
* \[[`2b2a0bf96b`](https://github.com/nodejs/node/commit/2b2a0bf96b)] - **sqlite**: add setReturnArrays method to StatementSync (Gürgün Dayıoğlu) [#57542](https://github.com/nodejs/node/pull/57542)
* \[[`064e0ebc90`](https://github.com/nodejs/node/commit/064e0ebc90)] - **sqlite**: enable common flags (Edy Silva) [#57621](https://github.com/nodejs/node/pull/57621)
* \[[`26fa594454`](https://github.com/nodejs/node/commit/26fa594454)] - **sqlite**: refactor prepared statement iterator (Colin Ihrig) [#57569](https://github.com/nodejs/node/pull/57569)
* \[[`0bf2c2827c`](https://github.com/nodejs/node/commit/0bf2c2827c)] - **sqlite,doc,test**: add aggregate function (Edy Silva) [#56600](https://github.com/nodejs/node/pull/56600)
* \[[`da281d7651`](https://github.com/nodejs/node/commit/da281d7651)] - **sqlite,src**: refactor sqlite value conversion (Edy Silva) [#57571](https://github.com/nodejs/node/pull/57571)
* \[[`413e93ce7d`](https://github.com/nodejs/node/commit/413e93ce7d)] - **src**: only block on user blocking worker tasks (Joyee Cheung) [#58047](https://github.com/nodejs/node/pull/58047)
* \[[`a5d01667e1`](https://github.com/nodejs/node/commit/a5d01667e1)] - **src**: use priority queue to run worker tasks (Joyee Cheung) [#58047](https://github.com/nodejs/node/pull/58047)
* \[[`d2f5ceb757`](https://github.com/nodejs/node/commit/d2f5ceb757)] - **src**: add more debug logs and comments in NodePlatform (Joyee Cheung) [#58047](https://github.com/nodejs/node/pull/58047)
* \[[`130eaa20a4`](https://github.com/nodejs/node/commit/130eaa20a4)] - **src**: improve parsing of boolean options (Edy Silva) [#58039](https://github.com/nodejs/node/pull/58039)
* \[[`f7ab6300de`](https://github.com/nodejs/node/commit/f7ab6300de)] - **src**: remove unused detachArrayBuffer method (Yagiz Nizipli) [#58055](https://github.com/nodejs/node/pull/58055)
* \[[`d712aa4cc0`](https://github.com/nodejs/node/commit/d712aa4cc0)] - **src**: fix internalModuleStat v8 fast path (Yagiz Nizipli) [#58054](https://github.com/nodejs/node/pull/58054)
* \[[`902cbe66a2`](https://github.com/nodejs/node/commit/902cbe66a2)] - **src**: fix EnvironmentOptions.async\_context\_frame default value (Chengzhong Wu) [#58030](https://github.com/nodejs/node/pull/58030)
* \[[`cfb39b9adb`](https://github.com/nodejs/node/commit/cfb39b9adb)] - **src**: annotate BaseObjects in the heap snapshots correctly (Joyee Cheung) [#57417](https://github.com/nodejs/node/pull/57417)
* \[[`4e02f239e4`](https://github.com/nodejs/node/commit/4e02f239e4)] - **src**: use macros to reduce code duplication is cares\_wrap (James M Snell) [#57937](https://github.com/nodejs/node/pull/57937)
* \[[`f36d30043a`](https://github.com/nodejs/node/commit/f36d30043a)] - **src**: improve error handling in cares\_wrap (James M Snell) [#57937](https://github.com/nodejs/node/pull/57937)
* \[[`88f047b828`](https://github.com/nodejs/node/commit/88f047b828)] - **src**: use ranges library (C++20) to simplify code (Daniel Lemire) [#57975](https://github.com/nodejs/node/pull/57975)
* \[[`09206e9731`](https://github.com/nodejs/node/commit/09206e9731)] - **src**: fix -Wunreachable-code-return in node\_sea (Shelley Vohr) [#57664](https://github.com/nodejs/node/pull/57664)
* \[[`87fd838a73`](https://github.com/nodejs/node/commit/87fd838a73)] - **src**: add dcheck\_eq for Object::New constructor calls (Jonas) [#57943](https://github.com/nodejs/node/pull/57943)
* \[[`2877207e19`](https://github.com/nodejs/node/commit/2877207e19)] - **src**: move windows specific fns to `_WIN32` (Yagiz Nizipli) [#57951](https://github.com/nodejs/node/pull/57951)
* \[[`b4055150bd`](https://github.com/nodejs/node/commit/b4055150bd)] - **src**: avoid calling SetPrototypeV2() (Yagiz Nizipli) [#57949](https://github.com/nodejs/node/pull/57949)
* \[[`46062f14e7`](https://github.com/nodejs/node/commit/46062f14e7)] - **src**: change DCHECK to CHECK (Wuli Zuo) [#57948](https://github.com/nodejs/node/pull/57948)
* \[[`a1106cc878`](https://github.com/nodejs/node/commit/a1106cc878)] - **src**: improve thread safety of TaskQueue (Shelley Vohr) [#57910](https://github.com/nodejs/node/pull/57910)
* \[[`99ed5034ea`](https://github.com/nodejs/node/commit/99ed5034ea)] - **src**: fixup errorhandling more in various places (James M Snell) [#57852](https://github.com/nodejs/node/pull/57852)
* \[[`227f2cb9a8`](https://github.com/nodejs/node/commit/227f2cb9a8)] - **src**: fix typo in comments (Edy Silva) [#57868](https://github.com/nodejs/node/pull/57868)
* \[[`a7d614a930`](https://github.com/nodejs/node/commit/a7d614a930)] - **src**: update std::vector\<v8::Local\<T>> to use v8::LocalVector\<T> (Aditi) [#57646](https://github.com/nodejs/node/pull/57646)
* \[[`4e7ae97dce`](https://github.com/nodejs/node/commit/4e7ae97dce)] - **src**: update std::vector\<v8::Local\<T>> to use v8::LocalVector\<T> (Aditi) [#57642](https://github.com/nodejs/node/pull/57642)
* \[[`aab4adb34e`](https://github.com/nodejs/node/commit/aab4adb34e)] - **src**: update std::vector\<v8::Local\<T>> to use v8::LocalVector\<T> (Aditi) [#57578](https://github.com/nodejs/node/pull/57578)
* \[[`fded233676`](https://github.com/nodejs/node/commit/fded233676)] - **src**: migrate from deprecated SnapshotCreator constructor (Joyee Cheung) [#55337](https://github.com/nodejs/node/pull/55337)
* \[[`8c5f9b4708`](https://github.com/nodejs/node/commit/8c5f9b4708)] - **src**: improve error message for invalid child stdio type in spawn\_sync (Dario Piotrowicz) [#57589](https://github.com/nodejs/node/pull/57589)
* \[[`14d751a736`](https://github.com/nodejs/node/commit/14d751a736)] - **src**: implement util.types fast API calls (Ruben Bridgewater) [#57819](https://github.com/nodejs/node/pull/57819)
* \[[`5e14fd13aa`](https://github.com/nodejs/node/commit/5e14fd13aa)] - **src**: enter and lock isolate properly in json parser (Joyee Cheung) [#57823](https://github.com/nodejs/node/pull/57823)
* \[[`34350019f8`](https://github.com/nodejs/node/commit/34350019f8)] - **src**: add BaseObjectPtr nullptr operations (Chengzhong Wu) [#56585](https://github.com/nodejs/node/pull/56585)
* \[[`d50b8a8815`](https://github.com/nodejs/node/commit/d50b8a8815)] - **src**: remove `void*` -> `char*` -> `void*` casts (Tobias Nießen) [#57791](https://github.com/nodejs/node/pull/57791)
* \[[`2b0f65ed5f`](https://github.com/nodejs/node/commit/2b0f65ed5f)] - **src**: improve error handling in `node_env_var.cc` (Antoine du Hamel) [#57767](https://github.com/nodejs/node/pull/57767)
* \[[`fc5295521a`](https://github.com/nodejs/node/commit/fc5295521a)] - **src**: improve error handling in node\_http2 (James M Snell) [#57764](https://github.com/nodejs/node/pull/57764)
* \[[`c707633f45`](https://github.com/nodejs/node/commit/c707633f45)] - **src**: improve error handing in node\_messaging (James M Snell) [#57760](https://github.com/nodejs/node/pull/57760)
* \[[`4093de6ff5`](https://github.com/nodejs/node/commit/4093de6ff5)] - **src**: improve error handling in crypto\_x509 (James M Snell) [#57757](https://github.com/nodejs/node/pull/57757)
* \[[`d309712820`](https://github.com/nodejs/node/commit/d309712820)] - **src**: improve error handling in callback.cc (James M Snell) [#57758](https://github.com/nodejs/node/pull/57758)
* \[[`6d39c47ee8`](https://github.com/nodejs/node/commit/6d39c47ee8)] - **src**: improve StringBytes error handling (James M Snell) [#57706](https://github.com/nodejs/node/pull/57706)
* \[[`3ff37a844f`](https://github.com/nodejs/node/commit/3ff37a844f)] - **src**: initialize privateSymbols for per\_context (Jason Zhang) [#57479](https://github.com/nodejs/node/pull/57479)
* \[[`56380df40c`](https://github.com/nodejs/node/commit/56380df40c)] - **src**: improve error handling in process.env handling (James M Snell) [#57707](https://github.com/nodejs/node/pull/57707)
* \[[`db8b29d282`](https://github.com/nodejs/node/commit/db8b29d282)] - **src**: remove unused variable in crypto\_x509.cc (Michaël Zasso) [#57754](https://github.com/nodejs/node/pull/57754)
* \[[`ed72044cca`](https://github.com/nodejs/node/commit/ed72044cca)] - **src**: update std::vector\<v8::Local\<T>> to use v8::LocalVector\<T> (Aditi) [#57733](https://github.com/nodejs/node/pull/57733)
* \[[`50f57073d8`](https://github.com/nodejs/node/commit/50f57073d8)] - **src**: fix kill signal 0 on Windows (Stefan Stojanovic) [#57695](https://github.com/nodejs/node/pull/57695)
* \[[`e144b69044`](https://github.com/nodejs/node/commit/e144b69044)] - **src**: fixup fs SyncCall to propagate errors correctly (James M Snell) [#57711](https://github.com/nodejs/node/pull/57711)
* \[[`f58c12078b`](https://github.com/nodejs/node/commit/f58c12078b)] - **src**: fix inefficient usage of v8\_inspector::StringView (Simon Zünd) [#52372](https://github.com/nodejs/node/pull/52372)
* \[[`a3ad331ce5`](https://github.com/nodejs/node/commit/a3ad331ce5)] - **src**: disable abseil deadlock detection (Chengzhong Wu) [#57582](https://github.com/nodejs/node/pull/57582)
* \[[`e4ff2b6fad`](https://github.com/nodejs/node/commit/e4ff2b6fad)] - **src**: remove deleted tls file (Shelley Vohr) [#57481](https://github.com/nodejs/node/pull/57481)
* \[[`d5db63a1a8`](https://github.com/nodejs/node/commit/d5db63a1a8)] - _**Revert**_ "**src**: do not expose simdjson.h in node\_config\_file.h" (James M Snell) [#57197](https://github.com/nodejs/node/pull/57197)
* \[[`076a99f11d`](https://github.com/nodejs/node/commit/076a99f11d)] - **src**: do not expose simdjson.h in node\_config\_file.h (Cheng) [#57173](https://github.com/nodejs/node/pull/57173)
* \[[`ad845588d0`](https://github.com/nodejs/node/commit/ad845588d0)] - _**Revert**_ "**src**: modernize cleanup queue to use c++20" (Richard Lau) [#56846](https://github.com/nodejs/node/pull/56846)
* \[[`581b44421a`](https://github.com/nodejs/node/commit/581b44421a)] - **src**: modernize cleanup queue to use c++20 (Yagiz Nizipli) [#56063](https://github.com/nodejs/node/pull/56063)
* \[[`a154352215`](https://github.com/nodejs/node/commit/a154352215)] - **src,permission**: make ERR\_ACCESS\_DENIED more descriptive (Rafael Gonzaga) [#57585](https://github.com/nodejs/node/pull/57585)
* \[[`6156f8a6d5`](https://github.com/nodejs/node/commit/6156f8a6d5)] - _**Revert**_ "**stream**: handle generator destruction from Duplex.from()" (jakecastelli) [#56278](https://github.com/nodejs/node/pull/56278)
* \[[`a0077c9b8b`](https://github.com/nodejs/node/commit/a0077c9b8b)] - **test**: remove deadlock workaround (Joyee Cheung) [#58047](https://github.com/nodejs/node/pull/58047)
* \[[`1f2b26172a`](https://github.com/nodejs/node/commit/1f2b26172a)] - **test**: prevent extraneous HOSTNAME substitution in test-runner-output (René) [#58076](https://github.com/nodejs/node/pull/58076)
* \[[`9ba16469c3`](https://github.com/nodejs/node/commit/9ba16469c3)] - **test**: update WPT for WebCryptoAPI to b48efd681e (Node.js GitHub Bot) [#58044](https://github.com/nodejs/node/pull/58044)
* \[[`3d708e0132`](https://github.com/nodejs/node/commit/3d708e0132)] - **test**: add missing newlines to repl .exit writes (Dario Piotrowicz) [#58041](https://github.com/nodejs/node/pull/58041)
* \[[`3457aee009`](https://github.com/nodejs/node/commit/3457aee009)] - **test**: use validateByRetainingPath in heapdump tests (Joyee Cheung) [#57417](https://github.com/nodejs/node/pull/57417)
* \[[`3d34c5f5e3`](https://github.com/nodejs/node/commit/3d34c5f5e3)] - **test**: add fast api tests for getLibuvNow() (Yagiz Nizipli) [#58022](https://github.com/nodejs/node/pull/58022)
* \[[`b8b019245b`](https://github.com/nodejs/node/commit/b8b019245b)] - **test**: add ALS test using http agent keep alive (Gerhard Stöbich) [#58017](https://github.com/nodejs/node/pull/58017)
* \[[`cbd2abeb8d`](https://github.com/nodejs/node/commit/cbd2abeb8d)] - **test**: deflake test-http2-options-max-headers-block-length (Luigi Pinca) [#57959](https://github.com/nodejs/node/pull/57959)
* \[[`21d052a578`](https://github.com/nodejs/node/commit/21d052a578)] - **test**: rename to getCallSites (Wuli Zuo) [#57948](https://github.com/nodejs/node/pull/57948)
* \[[`f2fd19e641`](https://github.com/nodejs/node/commit/f2fd19e641)] - **test**: force GC in test-file-write-stream4 (Luigi Pinca) [#57930](https://github.com/nodejs/node/pull/57930)
* \[[`7039173398`](https://github.com/nodejs/node/commit/7039173398)] - **test**: enable skipped colorize test (Shima Ryuhei) [#57887](https://github.com/nodejs/node/pull/57887)
* \[[`baa6968f95`](https://github.com/nodejs/node/commit/baa6968f95)] - **test**: update WPT for WebCryptoAPI to 164426ace2 (Node.js GitHub Bot) [#57854](https://github.com/nodejs/node/pull/57854)
* \[[`660d238798`](https://github.com/nodejs/node/commit/660d238798)] - **test**: deflake test-buffer-large-size (jakecastelli) [#57789](https://github.com/nodejs/node/pull/57789)
* \[[`ce2274d52f`](https://github.com/nodejs/node/commit/ce2274d52f)] - **test**: add test for frame count being 0.5 (Jake Yuesong Li) [#57732](https://github.com/nodejs/node/pull/57732)
* \[[`9d2a09db00`](https://github.com/nodejs/node/commit/9d2a09db00)] - **test**: fix the decimal fractions explaination (Jake Yuesong Li) [#57732](https://github.com/nodejs/node/pull/57732)
* \[[`12f4124af8`](https://github.com/nodejs/node/commit/12f4124af8)] - _**Revert**_ "**test**: add tests for REPL custom evals" (Tobias Nießen) [#57793](https://github.com/nodejs/node/pull/57793)
* \[[`3cdf8ec7c7`](https://github.com/nodejs/node/commit/3cdf8ec7c7)] - **test**: add tests for REPL custom evals (Dario Piotrowicz) [#57691](https://github.com/nodejs/node/pull/57691)
* \[[`9af8b92fb4`](https://github.com/nodejs/node/commit/9af8b92fb4)] - **test**: update expected error message for macOS (Antoine du Hamel) [#57742](https://github.com/nodejs/node/pull/57742)
* \[[`eaec2b5169`](https://github.com/nodejs/node/commit/eaec2b5169)] - **test**: fix dangling promise in test\_runner no isolation test setup (Jacob Smith) [#57595](https://github.com/nodejs/node/pull/57595)
* \[[`51ded6eaeb`](https://github.com/nodejs/node/commit/51ded6eaeb)] - **test**: improve test description (jakecastelli) [#56943](https://github.com/nodejs/node/pull/56943)
* \[[`75b9c1cdd8`](https://github.com/nodejs/node/commit/75b9c1cdd8)] - **test**: remove test-macos-app-sandbox flaky designation (Luigi Pinca) [#56471](https://github.com/nodejs/node/pull/56471)
* \[[`72537f5631`](https://github.com/nodejs/node/commit/72537f5631)] - **test**: remove flaky test-pipe-file-to-http designation (Luigi Pinca) [#56472](https://github.com/nodejs/node/pull/56472)
* \[[`984a472137`](https://github.com/nodejs/node/commit/984a472137)] - **test**: remove test-runner-watch-mode-complex flaky designation (Luigi Pinca) [#56470](https://github.com/nodejs/node/pull/56470)
* \[[`23275cc7bc`](https://github.com/nodejs/node/commit/23275cc7bc)] - **test**: add test case for `util.inspect` (Jordan Harband) [#55778](https://github.com/nodejs/node/pull/55778)
* \[[`99e4685636`](https://github.com/nodejs/node/commit/99e4685636)] - **test\_runner**: support mocking json modules (Jacob Smith) [#58007](https://github.com/nodejs/node/pull/58007)
* \[[`8207828aad`](https://github.com/nodejs/node/commit/8207828aad)] - **test\_runner**: recalculate run duration on watch restart (Pietro Marchini) [#57786](https://github.com/nodejs/node/pull/57786)
* \[[`7416a7f35a`](https://github.com/nodejs/node/commit/7416a7f35a)] - **test\_runner**: match minimum file column to 'all files' (Shima Ryuhei) [#57848](https://github.com/nodejs/node/pull/57848)
* \[[`87ac6cfed7`](https://github.com/nodejs/node/commit/87ac6cfed7)] - **test\_runner**: improve --test-timeout to be per test (jakecastelli) [#57672](https://github.com/nodejs/node/pull/57672)
* \[[`ae08210e37`](https://github.com/nodejs/node/commit/ae08210e37)] - **tools**: ignore V8 tests in CodeQL scans (Rich Trott) [#58081](https://github.com/nodejs/node/pull/58081)
* \[[`25c17ab365`](https://github.com/nodejs/node/commit/25c17ab365)] - **tools**: enable CodeQL config file (Rich Trott) [#58036](https://github.com/nodejs/node/pull/58036)
* \[[`c3d2a1c723`](https://github.com/nodejs/node/commit/c3d2a1c723)] - **tools**: ignore test directory in CodeQL scans (Rich Trott) [#57978](https://github.com/nodejs/node/pull/57978)
* \[[`d31e630462`](https://github.com/nodejs/node/commit/d31e630462)] - **tools**: add semver-major release support to release-lint (Antoine du Hamel) [#57892](https://github.com/nodejs/node/pull/57892)
* \[[`3a99975a88`](https://github.com/nodejs/node/commit/3a99975a88)] - **tools**: add codeql nightly (Rafael Gonzaga) [#57788](https://github.com/nodejs/node/pull/57788)
* \[[`77dee41a5d`](https://github.com/nodejs/node/commit/77dee41a5d)] - **tools**: edit create-release-proposal workflow to handle pr body length (Elves Vieira) [#57841](https://github.com/nodejs/node/pull/57841)
* \[[`6592803bd0`](https://github.com/nodejs/node/commit/6592803bd0)] - **tools**: add zstd updater to workflow (KASEYA\yahor.siarheyenka) [#57831](https://github.com/nodejs/node/pull/57831)
* \[[`c08349393b`](https://github.com/nodejs/node/commit/c08349393b)] - **tools**: remove unused `osx-pkg-postinstall.sh` (Antoine du Hamel) [#57667](https://github.com/nodejs/node/pull/57667)
* \[[`82bb228796`](https://github.com/nodejs/node/commit/82bb228796)] - **tools**: do not use temp files when merging PRs (Antoine du Hamel) [#57790](https://github.com/nodejs/node/pull/57790)
* \[[`f2cdc98e75`](https://github.com/nodejs/node/commit/f2cdc98e75)] - **tools**: update gyp-next to 0.20.0 (Node.js GitHub Bot) [#57683](https://github.com/nodejs/node/pull/57683)
* \[[`02d36cd61d`](https://github.com/nodejs/node/commit/02d36cd61d)] - **tools**: update doc to new version (Node.js GitHub Bot) [#57769](https://github.com/nodejs/node/pull/57769)
* \[[`74ac98c78e`](https://github.com/nodejs/node/commit/74ac98c78e)] - **tools**: bump the eslint group in /tools/eslint with 4 updates (dependabot\[bot]) [#57721](https://github.com/nodejs/node/pull/57721)
* \[[`dcba975031`](https://github.com/nodejs/node/commit/dcba975031)] - **tools**: enable linter in `test/fixtures/source-map/output` (Antoine du Hamel) [#57700](https://github.com/nodejs/node/pull/57700)
* \[[`b9043c9e9b`](https://github.com/nodejs/node/commit/b9043c9e9b)] - **tools**: enable linter in `test/fixtures/errors` (Antoine du Hamel) [#57701](https://github.com/nodejs/node/pull/57701)
* \[[`bbbf49812e`](https://github.com/nodejs/node/commit/bbbf49812e)] - **tools**: enable linter in `test/fixtures/test-runner/output` (Antoine du Hamel) [#57698](https://github.com/nodejs/node/pull/57698)
* \[[`9f1ad3c6da`](https://github.com/nodejs/node/commit/9f1ad3c6da)] - **tools**: enable linter in `test/fixtures/eval` (Antoine du Hamel) [#57699](https://github.com/nodejs/node/pull/57699)
* \[[`98df74464f`](https://github.com/nodejs/node/commit/98df74464f)] - **tools**: enable linter on some fixtures file (Antoine du Hamel) [#57674](https://github.com/nodejs/node/pull/57674)
* \[[`cf02cdb799`](https://github.com/nodejs/node/commit/cf02cdb799)] - **tools**: update ESLint to 9.23 (Antoine du Hamel) [#57673](https://github.com/nodejs/node/pull/57673)
* \[[`8790348303`](https://github.com/nodejs/node/commit/8790348303)] - **tools**: update doc to new version (Node.js GitHub Bot) [#57085](https://github.com/nodejs/node/pull/57085)
* \[[`b1ee186a62`](https://github.com/nodejs/node/commit/b1ee186a62)] - **tools**: update doc to new version (Node.js GitHub Bot) [#51192](https://github.com/nodejs/node/pull/51192)
* \[[`be34b5e7fc`](https://github.com/nodejs/node/commit/be34b5e7fc)] - **tools**: disable doc building when ICU is not available (Antoine du Hamel) [#51192](https://github.com/nodejs/node/pull/51192)
* \[[`6a486347fb`](https://github.com/nodejs/node/commit/6a486347fb)] - **url**: improve canParse() performance for non-onebyte strings (Yagiz Nizipli) [#58023](https://github.com/nodejs/node/pull/58023)
* \[[`7e3503fff1`](https://github.com/nodejs/node/commit/7e3503fff1)] - **util**: fix parseEnv handling of invalid lines (Augustin Mauroy) [#57798](https://github.com/nodejs/node/pull/57798)
* \[[`594269fcca`](https://github.com/nodejs/node/commit/594269fcca)] - **util**: fix formatting of objects with built-in Symbol.toPrimitive (Shima Ryuhei) [#57832](https://github.com/nodejs/node/pull/57832)
* \[[`8ca56a8db8`](https://github.com/nodejs/node/commit/8ca56a8db8)] - **util**: preserve `length` of deprecated functions (Livia Medeiros) [#57806](https://github.com/nodejs/node/pull/57806)
* \[[`6add4c56aa`](https://github.com/nodejs/node/commit/6add4c56aa)] - **util**: fix parseEnv incorrectly splitting multiple ‘=‘ in value (HEESEUNG) [#57421](https://github.com/nodejs/node/pull/57421)
* \[[`e577618227`](https://github.com/nodejs/node/commit/e577618227)] - **util**: inspect: enumerable Symbols no longer have square brackets (Jordan Harband) [#55778](https://github.com/nodejs/node/pull/55778)
* \[[`cb7eb15161`](https://github.com/nodejs/node/commit/cb7eb15161)] - **watch**: clarify completion/failure watch mode messages (Dario Piotrowicz) [#57926](https://github.com/nodejs/node/pull/57926)
* \[[`65562127bd`](https://github.com/nodejs/node/commit/65562127bd)] - **watch**: check parent and child path properly (Jason Zhang) [#57425](https://github.com/nodejs/node/pull/57425)
* \[[`b39fb9aa7f`](https://github.com/nodejs/node/commit/b39fb9aa7f)] - **win**: fix SIGQUIT on ClangCL (Stefan Stojanovic) [#57659](https://github.com/nodejs/node/pull/57659)
* \[[`76c5ea669d`](https://github.com/nodejs/node/commit/76c5ea669d)] - **worker**: add ESM version examples to worker docs (fisker Cheung) [#57645](https://github.com/nodejs/node/pull/57645)
* \[[`17965eb33d`](https://github.com/nodejs/node/commit/17965eb33d)] - **zlib**: fix pointer alignment (jhofstee) [#57727](https://github.com/nodejs/node/pull/57727)
