# Node.js 17 ChangeLog

<!--lint disable maximum-line-length no-literal-urls prohibited-strings-->

<table>
<tr>
<th>Current</th>
</tr>
<tr>
<td>
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
