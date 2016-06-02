# Node.js v6 ChangeLog

<table>
<tr>
<th title="Previously called 'Stable'">Current</th>
</tr>
<tr>
<td>
<a href="#6.2.1">6.2.1</a><br/>
<a href="#6.2.0">6.2.0</a><br/>
<a href="#6.1.0">6.1.0</a><br/>
<a href="#6.0.0">6.0.0</a><br/>
</td>
</tr>
</table>

* Other Versions
  * [5.x](CHANGELOG_V5.md)
  * [4.x](CHANGELOG_V4.md)
  * [0.12.x](CHANGELOG_V012.md)
  * [0.10.x](CHANGELOG_V010.md)
  * [io.js](CHANGELOG_IOJS.md)
  * [Archive](CHANGELOG_ARCHIVE.md)

**Note:** The v6 release line will be covered by the
[Node.js Long Term Support plan](https://github.com/nodejs/LTS) starting in
October 2016.

<a id="6.2.1"></a>
## 2016-06-02, Version 6.2.1 (Current), @rvagg

### Notable changes

* **buffer**: Ignore negative lengths in calls to `Buffer()` and `Buffer.allocUnsafe()`. This fixes a possible security concern (reported by Feross Aboukhadijeh) where user input is passed unchecked to the Buffer constructor or `allocUnsafe()` as it can expose parts of the memory slab used by other Buffers in the application. Note that negative lengths are not supported by the Buffer API and user input to the constructor should always be sanitised and type-checked. (Anna Henningsen) [#7051](https://github.com/nodejs/node/pull/7051)
* **npm**: Upgrade npm to 3.9.3 (Kat Marchán) [#7030](https://github.com/nodejs/node/pull/7030)
  - [`npm/npm@42d71be`](https://github.com/npm/npm/commit/42d71be2cec674dd9e860ad414f53184f667620d) [npm/npm#12685](https://github.com/npm/npm/pull/12685) When using `npm ls <pkg>` without a semver specifier, `npm ls` would skip any packages in your tree that matched by name, but had a prerelease version in their `package.json`. ([@zkat](https://github.com/zkat))
  - [`npm/npm@f04e05`](https://github.com/npm/npm/commit/df04e05af1f257a1903372e1baf334c0969fbdbd) [npm/npm#10013](https://github.com/npm/npm/issues/10013) `read-package-tree@5.1.4`: Fixes an issue where `npm install` would fail if your `node_modules` was symlinked. ([@iarna](https://github.com/iarna))
  - [`b894413`](https://github.com/npm/npm/commit/b8944139a935680c4a267468bb2d3c3082b5609f) [#12372](https://github.com/npm/npm/issues/12372) Changing a nested dependency in an `npm-shrinkwrap.json` and then running `npm install` would not get up the updated package. This corrects that. ([@misterbyrne](https://github.com/misterbyrne))
  - This release includes `npm@3.9.0`, which is the result of our Windows testing push -- the test suite (should) pass on Windows now. We're working on getting AppVeyor to a place where we can just rely on it like Travis.
* **tty**: Default to blocking mode for stdio on OS X. A bug fix in libuv 1.9.0, introduced in Node.js v6.0.0, exposed problems with Node's use of non-blocking stdio, particularly on OS X which has a small output buffer. This change should fix CLI applications that have been having problems with output since Node.js v6.0.0 on OS X. The core team is continuing to address stdio concerns that exist across supported platforms and progress can be tracked at <https://github.com/nodejs/node/issues/6980>. (Jeremiah Senkpiel) [#6895](https://github.com/nodejs/node/pull/6895)
* **V8**: Upgrade to V8 5.0.71.52. This includes a fix that addresses problems experienced by users of node-inspector since Node.js v6.0.0, see <https://github.com/node-inspector/node-inspector/issues/864> for details. (Michaël Zasso) [#6928](https://github.com/nodejs/node/pull/6928)

### Commits

* [[`99c05a1af0`](https://github.com/nodejs/node/commit/99c05a1af0)] - **async_wrap**: pass uid to JS as double (Trevor Norris) [#7096](https://github.com/nodejs/node/pull/7096)
* [[`371be9cd80`](https://github.com/nodejs/node/commit/371be9cd80)] - **buffer**: ignore negative allocation lengths (Anna Henningsen) [#7051](https://github.com/nodejs/node/pull/7051)
* [[`1bcc226edf`](https://github.com/nodejs/node/commit/1bcc226edf)] - **buffer**: fix dataview-set benchmark (Ingvar Stepanyan) [#6922](https://github.com/nodejs/node/pull/6922)
* [[`98270c6d15`](https://github.com/nodejs/node/commit/98270c6d15)] - **buffer**: fix single digit hex string handling (Justin Sprigg) [#6775](https://github.com/nodejs/node/pull/6775)
* [[`1fece2f8c0`](https://github.com/nodejs/node/commit/1fece2f8c0)] - **build**: re-add --ninja option to configure (Ehsan Akhgari) [#6780](https://github.com/nodejs/node/pull/6780)
* [[`e7b03be191`](https://github.com/nodejs/node/commit/e7b03be191)] - **build**: update build-addons when node-gyp changes (Lance Ball) [#6787](https://github.com/nodejs/node/pull/6787)
* [[`55c0b3e0e5`](https://github.com/nodejs/node/commit/55c0b3e0e5)] - **build**: unbreak configure with python 2.6 (Ben Noordhuis) [#6874](https://github.com/nodejs/node/pull/6874)
* [[`0503681348`](https://github.com/nodejs/node/commit/0503681348)] - **child_process**: measure buffer length in bytes (Rich Trott) [#6764](https://github.com/nodejs/node/pull/6764)
* [[`27d0eb054c`](https://github.com/nodejs/node/commit/27d0eb054c)] - **child_process**: emit IPC messages on next tick (cjihrig) [#6909](https://github.com/nodejs/node/pull/6909)
* [[`b28468e0a7`](https://github.com/nodejs/node/commit/b28468e0a7)] - **child_process**: allow buffer encoding in spawnSync (cjihrig) [#6939](https://github.com/nodejs/node/pull/6939)
* [[`6a62bb0070`](https://github.com/nodejs/node/commit/6a62bb0070)] - **cluster**: expose result of send() (cjihrig) [#6998](https://github.com/nodejs/node/pull/6998)
* [[`2132d349b5`](https://github.com/nodejs/node/commit/2132d349b5)] - **cluster**: rewrite debug ports consistently (cjihrig) [#7050](https://github.com/nodejs/node/pull/7050)
* [[`0bd8f4c4d8`](https://github.com/nodejs/node/commit/0bd8f4c4d8)] - **cluster**: reset handle index on close (Santiago Gimeno) [#6981](https://github.com/nodejs/node/pull/6981)
* [[`93e150f1d6`](https://github.com/nodejs/node/commit/93e150f1d6)] - **cluster**: guard against undefined message handlers (cjihrig) [#6902](https://github.com/nodejs/node/pull/6902)
* [[`28b73428e1`](https://github.com/nodejs/node/commit/28b73428e1)] - **cluster**: close ownerless handles on disconnect() (cjihrig) [#6909](https://github.com/nodejs/node/pull/6909)
* [[`2184e772d2`](https://github.com/nodejs/node/commit/2184e772d2)] - **debugger**: propagate --debug-port= to debuggee (Ben Noordhuis) [#3470](https://github.com/nodejs/node/pull/3470)
* [[`ded02b909f`](https://github.com/nodejs/node/commit/ded02b909f)] - **deps**: upgrade npm to 3.9.3 (Kat Marchán) [#7030](https://github.com/nodejs/node/pull/7030)
* [[`bfd7b24c63`](https://github.com/nodejs/node/commit/bfd7b24c63)] - **deps**: upgrade to V8 5.0.71.52 (Michaël Zasso) [#6928](https://github.com/nodejs/node/pull/6928)
* [[`8e6f8b2fb5`](https://github.com/nodejs/node/commit/8e6f8b2fb5)] - **dgram**: copy the list in send (Matteo Collina) [#6804](https://github.com/nodejs/node/pull/6804)
* [[`588c76cd5c`](https://github.com/nodejs/node/commit/588c76cd5c)] - **dgram,test**: add addMembership/dropMembership tests (Rich Trott) [#6753](https://github.com/nodejs/node/pull/6753)
* [[`e93198e86d`](https://github.com/nodejs/node/commit/e93198e86d)] - **doc**: edit pull request template (Rich Trott) [#7058](https://github.com/nodejs/node/pull/7058)
* [[`1c1256718e`](https://github.com/nodejs/node/commit/1c1256718e)] - **doc**: addresses nits in string_decoder, url, util (Jeremiah Senkpiel) [#7026](https://github.com/nodejs/node/pull/7026)
* [[`14b3ba35fc`](https://github.com/nodejs/node/commit/14b3ba35fc)] - **doc**: improve debugger doc prose (Rich Trott) [#7007](https://github.com/nodejs/node/pull/7007)
* [[`3c2c4c8d5c`](https://github.com/nodejs/node/commit/3c2c4c8d5c)] - **doc**: update labels and CI info in onboarding doc (Rich Trott) [#7006](https://github.com/nodejs/node/pull/7006)
* [[`b5e93c97f8`](https://github.com/nodejs/node/commit/b5e93c97f8)] - **doc**: fix typos in WORKING_GROUPS.md (Joao Andrade) [#7032](https://github.com/nodejs/node/pull/7032)
* [[`f15448681a`](https://github.com/nodejs/node/commit/f15448681a)] - **doc**: buffers are not sent over IPC with a socket (Tim Kuijsten) [#6951](https://github.com/nodejs/node/pull/6951)
* [[`3518ab93b1`](https://github.com/nodejs/node/commit/3518ab93b1)] - **doc**: minor improvements to util.md (Sakthipriyan Vairamani) [#6932](https://github.com/nodejs/node/pull/6932)
* [[`216a3cdcce`](https://github.com/nodejs/node/commit/216a3cdcce)] - **doc**: add `added:` information for vm (Anna Henningsen) [#7011](https://github.com/nodejs/node/pull/7011)
* [[`b30d07845d`](https://github.com/nodejs/node/commit/b30d07845d)] - **doc**: add `added:` information for console (Adrian Estrada) [#6995](https://github.com/nodejs/node/pull/6995)
* [[`72d4692e94`](https://github.com/nodejs/node/commit/72d4692e94)] - **doc**: add info on what's used for fswatch on AIX (Michael Dawson) [#6837](https://github.com/nodejs/node/pull/6837)
* [[`7c38327dee`](https://github.com/nodejs/node/commit/7c38327dee)] - **doc**: update process.hrtime docs to include optional parameter (doug.wade) [#6585](https://github.com/nodejs/node/pull/6585)
* [[`0f17a28a00`](https://github.com/nodejs/node/commit/0f17a28a00)] - **doc**: improve server.listen() documentation prose (Rich Trott) [#7000](https://github.com/nodejs/node/pull/7000)
* [[`3ae9f1469d`](https://github.com/nodejs/node/commit/3ae9f1469d)] - **doc**: improve `server.address()` doc text (Rich Trott) [#7001](https://github.com/nodejs/node/pull/7001)
* [[`ae1bf83b6c`](https://github.com/nodejs/node/commit/ae1bf83b6c)] - **doc**: clarified use of sexual language in the CoC (Bryan Hughes) [#6973](https://github.com/nodejs/node/pull/6973)
* [[`3909209e7a`](https://github.com/nodejs/node/commit/3909209e7a)] - **doc**: general improvements to tty.md (James M Snell) [#6931](https://github.com/nodejs/node/pull/6931)
* [[`bc2efe22f6`](https://github.com/nodejs/node/commit/bc2efe22f6)] - **doc**: add `added: ` data for cli.md (Rich Trott) [#6960](https://github.com/nodejs/node/pull/6960)
* [[`856638d0b7`](https://github.com/nodejs/node/commit/856638d0b7)] - **doc**: add `added:` information for child_process (Anna Henningsen) [#6927](https://github.com/nodejs/node/pull/6927)
* [[`a5e3eddfbf`](https://github.com/nodejs/node/commit/a5e3eddfbf)] - **doc**: general improvements to url.md copy (James M Snell) [#6904](https://github.com/nodejs/node/pull/6904)
* [[`b7ca0a2653`](https://github.com/nodejs/node/commit/b7ca0a2653)] - **doc**: add yorkie to collaborators (Yazhong Liu) [#7004](https://github.com/nodejs/node/pull/7004)
* [[`a9b90a6952`](https://github.com/nodejs/node/commit/a9b90a6952)] - **doc**: general improvements to tls.md copy (James M Snell) [#6933](https://github.com/nodejs/node/pull/6933)
* [[`5990a7fe91`](https://github.com/nodejs/node/commit/5990a7fe91)] - **doc**: fix broken references (Anna Henningsen) [#6941](https://github.com/nodejs/node/pull/6941)
* [[`98e497bdad`](https://github.com/nodejs/node/commit/98e497bdad)] - **doc**: fix broken references in changelogs (Anna Henningsen) [#6942](https://github.com/nodejs/node/pull/6942)
* [[`f3ae42168a`](https://github.com/nodejs/node/commit/f3ae42168a)] - **doc**: general improvements to string_decoder.md copy (James M Snell) [#6940](https://github.com/nodejs/node/pull/6940)
* [[`8f623a3c75`](https://github.com/nodejs/node/commit/8f623a3c75)] - **doc**: add firedfox to collaborators (Daniel Wang) [#6961](https://github.com/nodejs/node/pull/6961)
* [[`145a6b946a`](https://github.com/nodejs/node/commit/145a6b946a)] - **doc**: add bmeck to collaborators (Bradley Meck) [#6962](https://github.com/nodejs/node/pull/6962)
* [[`95f8d59e0d`](https://github.com/nodejs/node/commit/95f8d59e0d)] - **doc**: remove "\" within backticks (Rod Machen) [#6952](https://github.com/nodejs/node/pull/6952)
* [[`ee1865dd2f`](https://github.com/nodejs/node/commit/ee1865dd2f)] - **doc**: clarify buffer class (Steve Mao) [#6914](https://github.com/nodejs/node/pull/6914)
* [[`db3d2a7b96`](https://github.com/nodejs/node/commit/db3d2a7b96)] - **doc**: fix typos in timers topic to aid readability (Kevin Donahue) [#6916](https://github.com/nodejs/node/pull/6916)
* [[`0f0003fc54`](https://github.com/nodejs/node/commit/0f0003fc54)] - **doc**: add jhamhader to collaborators (Yuval Brik) [#6946](https://github.com/nodejs/node/pull/6946)
* [[`f6558ec537`](https://github.com/nodejs/node/commit/f6558ec537)] - **doc**: add @othiym23 to list of collaborators (Forrest L Norvell) [#6945](https://github.com/nodejs/node/pull/6945)
* [[`9fa1b19eda`](https://github.com/nodejs/node/commit/9fa1b19eda)] - **doc**: reference list of language-specific globals (Anna Henningsen) [#6900](https://github.com/nodejs/node/pull/6900)
* [[`15f6224418`](https://github.com/nodejs/node/commit/15f6224418)] - **doc**: make doc-only -> fallback to user binary (Robert Jefe Lindstaedt) [#6906](https://github.com/nodejs/node/pull/6906)
* [[`a320a019f1`](https://github.com/nodejs/node/commit/a320a019f1)] - **doc**: general improvements to util.md (James M Snell) [#6897](https://github.com/nodejs/node/pull/6897)
* [[`527a8a4844`](https://github.com/nodejs/node/commit/527a8a4844)] - **doc**: add `added:` information for zlib (Anna Henningsen) [#6840](https://github.com/nodejs/node/pull/6840)
* [[`cb8de85100`](https://github.com/nodejs/node/commit/cb8de85100)] - **doc**: make the api doc print-friendly (Marian) [#6748](https://github.com/nodejs/node/pull/6748)
* [[`f1a8c3164a`](https://github.com/nodejs/node/commit/f1a8c3164a)] - **doc**: add bengl to collaborators (Bryan English) [#6921](https://github.com/nodejs/node/pull/6921)
* [[`565d4ca12e`](https://github.com/nodejs/node/commit/565d4ca12e)] - **doc**: Update DCO to v1.1 (William Kapke) [#6353](https://github.com/nodejs/node/pull/6353)
* [[`eff73c78c5`](https://github.com/nodejs/node/commit/eff73c78c5)] - **doc**: general improvements to v8.md copy (James M Snell) [#6829](https://github.com/nodejs/node/pull/6829)
* [[`e5a7cec828`](https://github.com/nodejs/node/commit/e5a7cec828)] - **doc**: fix typo in Error.captureStackTrace (Mohsen) [#6811](https://github.com/nodejs/node/pull/6811)
* [[`5afb91bef7`](https://github.com/nodejs/node/commit/5afb91bef7)] - **doc**: make param names consistent & fix doc link (Sakthipriyan Vairamani) [#6832](https://github.com/nodejs/node/pull/6832)
* [[`e1fb4805cf`](https://github.com/nodejs/node/commit/e1fb4805cf)] - **doc**: add `added:` info for `process.cpuUsage` (Anna Henningsen) [#6863](https://github.com/nodejs/node/pull/6863)
* [[`8a0329f110`](https://github.com/nodejs/node/commit/8a0329f110)] - **doc**: fix mkdtemp example by removing hyphen (Sakthipriyan Vairamani) [#6834](https://github.com/nodejs/node/pull/6834)
* [[`45ca7cfcdd`](https://github.com/nodejs/node/commit/45ca7cfcdd)] - **doc**: reduce GitHub template verbosity (Rich Trott) [#6801](https://github.com/nodejs/node/pull/6801)
* [[`12a3d0120b`](https://github.com/nodejs/node/commit/12a3d0120b)] - **doc**: improve vm.md copy (James M Snell) [#6827](https://github.com/nodejs/node/pull/6827)
* [[`0ae512a3cf`](https://github.com/nodejs/node/commit/0ae512a3cf)] - **doc**: Add resolveNaptr and naptr rrtype docs (Doug Wade) [#6586](https://github.com/nodejs/node/pull/6586)
* [[`8309dbaf9c`](https://github.com/nodejs/node/commit/8309dbaf9c)] - **doc**: fix name to match git log (Robert Jefe Lindstaedt) [#6880](https://github.com/nodejs/node/pull/6880)
* [[`b52d838f0d`](https://github.com/nodejs/node/commit/b52d838f0d)] - **doc**: add note for fs.watch virtualized env (Robert Jefe Lindstaedt) [#6809](https://github.com/nodejs/node/pull/6809)
* [[`08f1361cb6`](https://github.com/nodejs/node/commit/08f1361cb6)] - **doc**: add `added:` information for punycode (Daniel Wang) [#6805](https://github.com/nodejs/node/pull/6805)
* [[`253db33527`](https://github.com/nodejs/node/commit/253db33527)] - **doc**: add `added:` info for dgram.*Membership() (Rich Trott) [#6753](https://github.com/nodejs/node/pull/6753)
* [[`5cad04b063`](https://github.com/nodejs/node/commit/5cad04b063)] - **doc**: clarify fs.mkdtemp prefix argument (James M Snell) [#6800](https://github.com/nodejs/node/pull/6800)
* [[`5a1e823fa5`](https://github.com/nodejs/node/commit/5a1e823fa5)] - **doc**: add `added:` information for fs (Anna Henningsen) [#6717](https://github.com/nodejs/node/pull/6717)
* [[`bf4724a7bb`](https://github.com/nodejs/node/commit/bf4724a7bb)] - **doc**: remove link to Sign in crypto.md (Kirill Fomichev) [#6812](https://github.com/nodejs/node/pull/6812)
* [[`ba3089970d`](https://github.com/nodejs/node/commit/ba3089970d)] - **doc**: add `added:` in for `tty` (Rich Trott) [#6783](https://github.com/nodejs/node/pull/6783)
* [[`758fadfa0d`](https://github.com/nodejs/node/commit/758fadfa0d)] - **doc**: update openssl.org hash links (silverwind) [#6817](https://github.com/nodejs/node/pull/6817)
* [[`b2c7d466d4`](https://github.com/nodejs/node/commit/b2c7d466d4)] - **doc,test**: add `How to write a Node.js test` guide (Santiago Gimeno) [#6984](https://github.com/nodejs/node/pull/6984)
* [[`c4329aa226`](https://github.com/nodejs/node/commit/c4329aa226)] - **fs**: move mkdtemp* functions near static functions (Sakthipriyan Vairamani) [#6828](https://github.com/nodejs/node/pull/6828)
* [[`c068880757`](https://github.com/nodejs/node/commit/c068880757)] - **fs**: mkdtemp shouldn't crash if no callback passed (Sakthipriyan Vairamani) [#6828](https://github.com/nodejs/node/pull/6828)
* [[`2ab36093e6`](https://github.com/nodejs/node/commit/2ab36093e6)] - **http**: use `localAddress` instead of `path` (Dirceu Pereira Tiegs) [#5190](https://github.com/nodejs/node/pull/5190)
* [[`6f0d8b3a1b`](https://github.com/nodejs/node/commit/6f0d8b3a1b)] - **installer**: don't install node_internals.h (Ben Noordhuis) [#6913](https://github.com/nodejs/node/pull/6913)
* [[`178f3080f8`](https://github.com/nodejs/node/commit/178f3080f8)] - **module**: don't cache uninitialized builtins (Anna Henningsen) [#6907](https://github.com/nodejs/node/pull/6907)
* [[`1908b7f00a`](https://github.com/nodejs/node/commit/1908b7f00a)] - **path**: fix basename() regressions (Brian White) [#6590](https://github.com/nodejs/node/pull/6590)
* [[`10671406ac`](https://github.com/nodejs/node/commit/10671406ac)] - **process**: internal/process/stdio.js cleanup / modernization (James M Snell) [#6766](https://github.com/nodejs/node/pull/6766)
* [[`64445674f0`](https://github.com/nodejs/node/commit/64445674f0)] - **src**: add include guards to internal headers (Ben Noordhuis) [#6948](https://github.com/nodejs/node/pull/6948)
* [[`4333fda46d`](https://github.com/nodejs/node/commit/4333fda46d)] - **src**: no abort from getter if object isn't wrapped (Trevor Norris) [#6184](https://github.com/nodejs/node/pull/6184)
* [[`4da3e1e461`](https://github.com/nodejs/node/commit/4da3e1e461)] - **src**: always clear wrap before persistent Reset() (Trevor Norris) [#6184](https://github.com/nodejs/node/pull/6184)
* [[`7e5775704e`](https://github.com/nodejs/node/commit/7e5775704e)] - **src**: inherit first from AsyncWrap (Trevor Norris) [#6184](https://github.com/nodejs/node/pull/6184)
* [[`0841496992`](https://github.com/nodejs/node/commit/0841496992)] - **src**: fix without-intl build (Anna Henningsen) [#6820](https://github.com/nodejs/node/pull/6820)
* [[`0d08fc415f`](https://github.com/nodejs/node/commit/0d08fc415f)] - **stream_base**: always use Base template class (Trevor Norris) [#6184](https://github.com/nodejs/node/pull/6184)
* [[`756ec80d50`](https://github.com/nodejs/node/commit/756ec80d50)] - **string_bytes**: Make base64 encode/decode reusable (Eugene Ostroukhov) [#6910](https://github.com/nodejs/node/pull/6910)
* [[`79ad172589`](https://github.com/nodejs/node/commit/79ad172589)] - **string_decoder**: rewrite implementation (Brian White) [#6777](https://github.com/nodejs/node/pull/6777)
* [[`8b720c4582`](https://github.com/nodejs/node/commit/8b720c4582)] - **test**: remove non-incremental common.PORT changes (Rich Trott) [#7055](https://github.com/nodejs/node/pull/7055)
* [[`6439fbfac0`](https://github.com/nodejs/node/commit/6439fbfac0)] - **test**: test TTY problems by fakeing a TTY using openpty (Jeremiah Senkpiel) [#6895](https://github.com/nodejs/node/pull/6895)
* [[`81a9f96a29`](https://github.com/nodejs/node/commit/81a9f96a29)] - **test**: make test-child-process-fork-net more robust (Rich Trott) [#7033](https://github.com/nodejs/node/pull/7033)
* [[`6cf0f622ef`](https://github.com/nodejs/node/commit/6cf0f622ef)] - **test**: fix spurious EADDRINUSE in test-https-strict (Rich Trott) [#7024](https://github.com/nodejs/node/pull/7024)
* [[`dea120f247`](https://github.com/nodejs/node/commit/dea120f247)] - **test**: update weak module for gc tests (Rich Trott) [#7014](https://github.com/nodejs/node/pull/7014)
* [[`3bfbe8a62a`](https://github.com/nodejs/node/commit/3bfbe8a62a)] - **test**: remove `common.PORT` from gc tests (Rich Trott) [#7013](https://github.com/nodejs/node/pull/7013)
* [[`b23cd48ca0`](https://github.com/nodejs/node/commit/b23cd48ca0)] - **test**: fix test-debug-port-numbers on OS X (Santiago Gimeno) [#7046](https://github.com/nodejs/node/pull/7046)
* [[`0a258e5369`](https://github.com/nodejs/node/commit/0a258e5369)] - **test**: remove modifcation to common.PORT (Rich Trott) [#6990](https://github.com/nodejs/node/pull/6990)
* [[`8c289df175`](https://github.com/nodejs/node/commit/8c289df175)] - **test**: use strictEqual consistently in agent test (Ben Noordhuis) [#6654](https://github.com/nodejs/node/pull/6654)
* [[`e4ac808c4d`](https://github.com/nodejs/node/commit/e4ac808c4d)] - **test**: work around debugger not killing inferior (Ben Noordhuis) [#7037](https://github.com/nodejs/node/pull/7037)
* [[`b5949f8bbc`](https://github.com/nodejs/node/commit/b5949f8bbc)] - **test**: verify cluster worker exit (cjihrig) [#6993](https://github.com/nodejs/node/pull/6993)
* [[`6f3f5af396`](https://github.com/nodejs/node/commit/6f3f5af396)] - **test**: add regression test for Proxy as vm context (Michaël Zasso) [#6967](https://github.com/nodejs/node/pull/6967)
* [[`38a3323cc9`](https://github.com/nodejs/node/commit/38a3323cc9)] - **test**: improve debug-break-on-uncaught reliability (Rich Trott) [#6793](https://github.com/nodejs/node/pull/6793)
* [[`83e6d53817`](https://github.com/nodejs/node/commit/83e6d53817)] - **test**: test cluster worker disconnection on error (Santiago Gimeno) [#6909](https://github.com/nodejs/node/pull/6909)
* [[`4cc6a18448`](https://github.com/nodejs/node/commit/4cc6a18448)] - **test**: verify IPC messages are emitted on next tick (Santiago Gimeno) [#6909](https://github.com/nodejs/node/pull/6909)
* [[`69e119dbfb`](https://github.com/nodejs/node/commit/69e119dbfb)] - **test**: refactor spawnSync() cwd test (cjihrig) [#6939](https://github.com/nodejs/node/pull/6939)
* [[`32cc43a1bd`](https://github.com/nodejs/node/commit/32cc43a1bd)] - **test**: fix component printing on windows (Ben Noordhuis) [#6915](https://github.com/nodejs/node/pull/6915)
* [[`c81b6f8d0d`](https://github.com/nodejs/node/commit/c81b6f8d0d)] - **test**: refactor to eliminate __defineGetter__ (Rich Trott) [#6774](https://github.com/nodejs/node/pull/6774)
* [[`1965e445ec`](https://github.com/nodejs/node/commit/1965e445ec)] - **test**: refactor test-tls-reuse-host-from-socket (Rich Trott) [#6756](https://github.com/nodejs/node/pull/6756)
* [[`2cf3a53ce1`](https://github.com/nodejs/node/commit/2cf3a53ce1)] - **test**: fix test-debug-port-cluster flakiness (Rich Trott) [#6769](https://github.com/nodejs/node/pull/6769)
* [[`5374afdef8`](https://github.com/nodejs/node/commit/5374afdef8)] - **test**: add logging for test-debug-port-cluster (Rich Trott) [#6769](https://github.com/nodejs/node/pull/6769)
* [[`bae7adb6fa`](https://github.com/nodejs/node/commit/bae7adb6fa)] - **test**: fix flaky test-stdout-close-catch (Santiago Gimeno) [#6808](https://github.com/nodejs/node/pull/6808)
* [[`528ca04e8d`](https://github.com/nodejs/node/commit/528ca04e8d)] - **test**: add more path.basename() tests (Brian White) [#6590](https://github.com/nodejs/node/pull/6590)
* [[`1469b98fa1`](https://github.com/nodejs/node/commit/1469b98fa1)] - **test**: remove duplicate path tests (Brian White) [#6590](https://github.com/nodejs/node/pull/6590)
* [[`81e765f521`](https://github.com/nodejs/node/commit/81e765f521)] - **test**: robust handling of env for npm-test-install (Myles Borins) [#6797](https://github.com/nodejs/node/pull/6797)
* [[`2895860138`](https://github.com/nodejs/node/commit/2895860138)] - **test**: cluster-setup-master online workers check (Devon Rifkin) [#6535](https://github.com/nodejs/node/pull/6535)
* [[`7c932c2d49`](https://github.com/nodejs/node/commit/7c932c2d49)] - **test**: added tests for https-agent-getname (suryagh) [#6762](https://github.com/nodejs/node/pull/6762)
* [[`827b3eb503`](https://github.com/nodejs/node/commit/827b3eb503)] - **test**: add --repeat option to tools/test.py (Michael Dawson) [#6700](https://github.com/nodejs/node/pull/6700)
* [[`ea287fc1a6`](https://github.com/nodejs/node/commit/ea287fc1a6)] - **test,win**: skip addons/load-long-path on WOW64 (Alexis Campailla) [#6675](https://github.com/nodejs/node/pull/6675)
* [[`21e31352d7`](https://github.com/nodejs/node/commit/21e31352d7)] - **tls**: catch `certCbDone` exceptions (Fedor Indutny) [#6887](https://github.com/nodejs/node/pull/6887)
* [[`257e54b9c0`](https://github.com/nodejs/node/commit/257e54b9c0)] - **tls,https**: respect address family when connecting (Ben Noordhuis) [#6654](https://github.com/nodejs/node/pull/6654)
* [[`5779ed2a4a`](https://github.com/nodejs/node/commit/5779ed2a4a)] - **tls_wrap**: do not abort on new TLSWrap() (Trevor Norris) [#6184](https://github.com/nodejs/node/pull/6184)
* [[`108523e06e`](https://github.com/nodejs/node/commit/108523e06e)] - **tools**: make sure doctool anchors respect includes (Anna Henningsen) [#6943](https://github.com/nodejs/node/pull/6943)
* [[`bf3afce668`](https://github.com/nodejs/node/commit/bf3afce668)] - **tools**: restore change of signatures to opts hashes (Jesse McCarthy) [#6690](https://github.com/nodejs/node/pull/6690)
* [[`ceee56b28b`](https://github.com/nodejs/node/commit/ceee56b28b)] - **tools**: disallow deprecated define getter/setter (Rich Trott) [#6774](https://github.com/nodejs/node/pull/6774)
* [[`614907e516`](https://github.com/nodejs/node/commit/614907e516)] - **tools**: print stderr on bad test.py `vmArch` check (Jeremiah Senkpiel) [#6786](https://github.com/nodejs/node/pull/6786)
* [[`4d3a7594a5`](https://github.com/nodejs/node/commit/4d3a7594a5)] - **tty**: use blocking mode on OS X (Jeremiah Senkpiel) [#6895](https://github.com/nodejs/node/pull/6895)
* [[`36ed4a2d7a`](https://github.com/nodejs/node/commit/36ed4a2d7a)] - **udp**: use libuv API to get file descriptor (Saúl Ibarra Corretgé) [#6908](https://github.com/nodejs/node/pull/6908)
* [[`f3e3eebec8`](https://github.com/nodejs/node/commit/f3e3eebec8)] - **unix,stream**: fix getting the correct fd for a handle (Saúl Ibarra Corretgé) [#6753](https://github.com/nodejs/node/pull/6753)
* [[`d270706881`](https://github.com/nodejs/node/commit/d270706881)] - **util**: pretty-print SIMD types (Ben Noordhuis) [#6917](https://github.com/nodejs/node/pull/6917)
* [[`55b736a63b`](https://github.com/nodejs/node/commit/55b736a63b)] - **vm**: don't abort process when stack space runs out (Anna Henningsen) [#6907](https://github.com/nodejs/node/pull/6907)
* [[`cb2ef35b76`](https://github.com/nodejs/node/commit/cb2ef35b76)] - **win,build**: add creation of zip and 7z package (Bartosz Sosnowski) [#5995](https://github.com/nodejs/node/pull/5995)
* [[`1e26b82ce4`](https://github.com/nodejs/node/commit/1e26b82ce4)] - **zlib**: release callback and buffer after processing (Matt Lavin) [#6955](https://github.com/nodejs/node/pull/6955)
* [[`64415564de`](https://github.com/nodejs/node/commit/64415564de)] - **zlib**: remove `_closed` in source (Anna Henningsen) [#6574](https://github.com/nodejs/node/pull/6574)

<a id="6.2.0"></a>
## 2016-05-17, Version 6.2.0 (Current), @evanlucas

### Notable changes

- **buffer**: fix lastIndexOf and indexOf in various edge cases (Anna Henningsen) [#6511](https://github.com/nodejs/node/pull/6511)
- **child_process**: use /system/bin/sh on android (Ben Noordhuis) [#6745](https://github.com/nodejs/node/pull/6745)
- **deps**:
  - upgrade npm to 3.8.9 (Rebecca Turner) [#6664](https://github.com/nodejs/node/pull/6664)
  - upgrade to V8 5.0.71.47 (Ali Ijaz Sheikh) [#6572](https://github.com/nodejs/node/pull/6572)
  - upgrade libuv to 1.9.1 (Saúl Ibarra Corretgé) [#6796](https://github.com/nodejs/node/pull/6796)
  - Intl: ICU 57 bump (Steven R. Loomis) [#6088](https://github.com/nodejs/node/pull/6088)
- **repl**:
  - copying tabs shouldn't trigger completion (Eugene Obrezkov) [#5958](https://github.com/nodejs/node/pull/5958)
  - exports `Recoverable` (Blake Embrey) [#3488](https://github.com/nodejs/node/pull/3488)
- **src**: add O_NOATIME constant (Rich Trott) [#6492](https://github.com/nodejs/node/pull/6492)
- **src,module**: add --preserve-symlinks command line flag (James M Snell) [#6537](https://github.com/nodejs/node/pull/6537)
- **util**: adhere to `noDeprecation` set at runtime (Anna Henningsen) [#6683](https://github.com/nodejs/node/pull/6683)

As of this release the 6.X line now includes 64-bit binaries for Linux on Power Systems 
running in big endian mode in addition to the existing 64-bit binaries for running in little endian mode.

### Commits

* [[`955c90d725`](https://github.com/nodejs/node/commit/955c90d725)] - **benchmark,test,lib**: remove extra spaces (Rich Trott) [#6645](https://github.com/nodejs/node/pull/6645)
* [[`9cd14ced09`](https://github.com/nodejs/node/commit/9cd14ced09)] - **buffer**: fix UCS2 indexOf for odd buffer length (Anna Henningsen) [#6511](https://github.com/nodejs/node/pull/6511)
* [[`a550ddbf3c`](https://github.com/nodejs/node/commit/a550ddbf3c)] - **buffer**: fix needle length misestimation for UCS2 (Anna Henningsen) [#6511](https://github.com/nodejs/node/pull/6511)
* [[`6fc20c5a97`](https://github.com/nodejs/node/commit/6fc20c5a97)] - **buffer**: fix lastIndexOf crash for overlong needle (Anna Henningsen) [#6511](https://github.com/nodejs/node/pull/6511)
* [[`44015754a3`](https://github.com/nodejs/node/commit/44015754a3)] - **buffer**: fix lastIndexOf index underflow issue (Anna Henningsen) [#6511](https://github.com/nodejs/node/pull/6511)
* [[`6032dc25cc`](https://github.com/nodejs/node/commit/6032dc25cc)] - **build**: add Make `doc-only` target (Jesse McCarthy) [#3888](https://github.com/nodejs/node/pull/3888)
* [[`3af9382a5d`](https://github.com/nodejs/node/commit/3af9382a5d)] - **build**: don't compile with -B, redux (Ben Noordhuis) [#6650](https://github.com/nodejs/node/pull/6650)
* [[`5149d66702`](https://github.com/nodejs/node/commit/5149d66702)] - **build**: fix DESTCPU detection for binary target (Richard Lau) [#6310](https://github.com/nodejs/node/pull/6310)
* [[`6eed6a3ac0`](https://github.com/nodejs/node/commit/6eed6a3ac0)] - **build,test**: fix build-addons dependency chain (Ben Noordhuis) [#6652](https://github.com/nodejs/node/pull/6652)
* [[`e0240ab592`](https://github.com/nodejs/node/commit/e0240ab592)] - **child_process**: use /system/bin/sh on android (Ben Noordhuis) [#6745](https://github.com/nodejs/node/pull/6745)
* [[`e8c9f01cdd`](https://github.com/nodejs/node/commit/e8c9f01cdd)] - **crypto**: disable ssl compression at build time (Ben Noordhuis) [#6582](https://github.com/nodejs/node/pull/6582)
* [[`62690aa0be`](https://github.com/nodejs/node/commit/62690aa0be)] - **deps**: update comment about PURIFY define (Ben Noordhuis) [#6582](https://github.com/nodejs/node/pull/6582)
* [[`bddf413412`](https://github.com/nodejs/node/commit/bddf413412)] - **deps**: upgrade npm to 3.8.9 (Rebecca Turner) [#6664](https://github.com/nodejs/node/pull/6664)
* [[`a6ca5e559a`](https://github.com/nodejs/node/commit/a6ca5e559a)] - **deps**: upgrade to V8 5.0.71.47 (Ali Ijaz Sheikh) [#6572](https://github.com/nodejs/node/pull/6572)
* [[`16159c23ed`](https://github.com/nodejs/node/commit/16159c23ed)] - **deps**: limit regress/regress-crbug-514081 v8 test (Michael Dawson) [#6678](https://github.com/nodejs/node/pull/6678)
* [[`2d84ac735a`](https://github.com/nodejs/node/commit/2d84ac735a)] - **deps**: upgrade libuv to 1.9.1 (Saúl Ibarra Corretgé) [#6796](https://github.com/nodejs/node/pull/6796)
* [[`7a6d2ad181`](https://github.com/nodejs/node/commit/7a6d2ad181)] - **deps**: Intl: Check in "small-icu" 57.1 (Steven R. Loomis) [#6088](https://github.com/nodejs/node/pull/6088)
* [[`ee1e5a267d`](https://github.com/nodejs/node/commit/ee1e5a267d)] - **deps**: Intl: ICU 57 bump (Steven R. Loomis) [#6088](https://github.com/nodejs/node/pull/6088)
* [[`a4ed7dfb3d`](https://github.com/nodejs/node/commit/a4ed7dfb3d)] - **doc**: Add CTC meeting minutes for 2016-05-04 (Michael Dawson) [#6579](https://github.com/nodejs/node/pull/6579)
* [[`5c7da210df`](https://github.com/nodejs/node/commit/5c7da210df)] - **doc**: refactor the changelog by version (James M Snell) [#6503](https://github.com/nodejs/node/pull/6503)
* [[`4f2a55f92f`](https://github.com/nodejs/node/commit/4f2a55f92f)] - **doc**: fix issues related to page scrolling (Roman Reiss)
* [[`b4fb95eade`](https://github.com/nodejs/node/commit/b4fb95eade)] - **doc**: add `added:` information for assert (Rich Trott) [#6688](https://github.com/nodejs/node/pull/6688)
* [[`64fcba2a2e`](https://github.com/nodejs/node/commit/64fcba2a2e)] - **doc**: appendFileSync accepts fd as well (Faiz Halde) [#6707](https://github.com/nodejs/node/pull/6707)
* [[`520369d8e0`](https://github.com/nodejs/node/commit/520369d8e0)] - **doc**: fix exec example in child_process (Evan Lucas) [#6660](https://github.com/nodejs/node/pull/6660)
* [[`51d1960955`](https://github.com/nodejs/node/commit/51d1960955)] - **doc**: undocument fs.open's 'rs' mode (Saúl Ibarra Corretgé) [#6732](https://github.com/nodejs/node/pull/6732)
* [[`f1c773d18b`](https://github.com/nodejs/node/commit/f1c773d18b)] - **doc**: add `added:` information for v8 (Rich Trott) [#6684](https://github.com/nodejs/node/pull/6684)
* [[`29b28a233c`](https://github.com/nodejs/node/commit/29b28a233c)] - **doc**: server.listen truncates socket path on unix (Jean Regisser) [#6659](https://github.com/nodejs/node/pull/6659)
* [[`c1d5f2e96e`](https://github.com/nodejs/node/commit/c1d5f2e96e)] - **doc**: update releases.md with new changelog structure (James M Snell) [#6503](https://github.com/nodejs/node/pull/6503)
* [[`d962fbafb2`](https://github.com/nodejs/node/commit/d962fbafb2)] - **doc**: "a" -> "an" in api/documentation.md (Anchika Agarwal) [#6689](https://github.com/nodejs/node/pull/6689)
* [[`26e22e200a`](https://github.com/nodejs/node/commit/26e22e200a)] - **doc**: move the readme newcomers section (Jeremiah Senkpiel) [#6681](https://github.com/nodejs/node/pull/6681)
* [[`8f526494b5`](https://github.com/nodejs/node/commit/8f526494b5)] - **doc**: fix deprecation warnings in addon examples (Ben Noordhuis) [#6652](https://github.com/nodejs/node/pull/6652)
* [[`d34343f0de`](https://github.com/nodejs/node/commit/d34343f0de)] - **doc**: mention existence/purpose of module wrapper (Matt Harrison) [#6433](https://github.com/nodejs/node/pull/6433)
* [[`5c154a87e0`](https://github.com/nodejs/node/commit/5c154a87e0)] - **doc**: add steps for running addons + npm tests (Myles Borins) [#6231](https://github.com/nodejs/node/pull/6231)
* [[`6ea43d12f4`](https://github.com/nodejs/node/commit/6ea43d12f4)] - **doc**: improve onboarding-extras.md formatting (Jeremiah Senkpiel) [#6548](https://github.com/nodejs/node/pull/6548)
* [[`38f5603e97`](https://github.com/nodejs/node/commit/38f5603e97)] - **doc**: fix linewrap in node.1 (Jeremiah Senkpiel) [#6532](https://github.com/nodejs/node/pull/6532)
* [[`5b47accfa6`](https://github.com/nodejs/node/commit/5b47accfa6)] - **doc**: v8 options can use either `_` or `-` (Jeremiah Senkpiel) [#6532](https://github.com/nodejs/node/pull/6532)
* [[`fa94a91bbd`](https://github.com/nodejs/node/commit/fa94a91bbd)] - **doc**: v8 functions as methods on v8 (Bryan English) [#6615](https://github.com/nodejs/node/pull/6615)
* [[`d49b49e8b1`](https://github.com/nodejs/node/commit/d49b49e8b1)] - **doc**: discourage use of util.inherits (Robert Jefe Lindstaedt) [#6514](https://github.com/nodejs/node/pull/6514)
* [[`925fc36dd9`](https://github.com/nodejs/node/commit/925fc36dd9)] - **doc**: add `added:` information for os (Bryan English) [#6609](https://github.com/nodejs/node/pull/6609)
* [[`58cd11c844`](https://github.com/nodejs/node/commit/58cd11c844)] - **doc**: add `added:` information for process (Bryan English) [#6589](https://github.com/nodejs/node/pull/6589)
* [[`d5f7f24aec`](https://github.com/nodejs/node/commit/d5f7f24aec)] - **doc**: add `added:` information for url (Bryan English) [#6593](https://github.com/nodejs/node/pull/6593)
* [[`715b48c1d3`](https://github.com/nodejs/node/commit/715b48c1d3)] - **doc**: add `added:` information for querystring (Bryan English) [#6593](https://github.com/nodejs/node/pull/6593)
* [[`04697a5023`](https://github.com/nodejs/node/commit/04697a5023)] - **doc**: correct anchors for buffer.md (Jackson Tian) [#6542](https://github.com/nodejs/node/pull/6542)
* [[`2d677411a4`](https://github.com/nodejs/node/commit/2d677411a4)] - **doc**: add `added:` information for buffer (Anna Henningsen) [#6495](https://github.com/nodejs/node/pull/6495)
* [[`49af20c2e6`](https://github.com/nodejs/node/commit/49af20c2e6)] - **doc**: update fs callback example error stack (DavidCai) [#6617](https://github.com/nodejs/node/pull/6617)
* [[`9196d87704`](https://github.com/nodejs/node/commit/9196d87704)] - **doc**: add `added:` info for `string_decoder` (Rich Trott) [#6741](https://github.com/nodejs/node/pull/6741)
* [[`688f563ca8`](https://github.com/nodejs/node/commit/688f563ca8)] - **doc**: update vm.runInDebugContext() example (Ben Noordhuis) [#6757](https://github.com/nodejs/node/pull/6757)
* [[`2273971a69`](https://github.com/nodejs/node/commit/2273971a69)] - **doc**: readline.emitKeypressEvents and raw mode (Arve Seljebu) [#6628](https://github.com/nodejs/node/pull/6628)
* [[`1c7b6e276f`](https://github.com/nodejs/node/commit/1c7b6e276f)] - **doc**: improve zlib docs (James M Snell) [#6746](https://github.com/nodejs/node/pull/6746)
* [[`897934a719`](https://github.com/nodejs/node/commit/897934a719)] - **doc**: copyedit maxBuffer note for child_process (Rich Trott) [#6760](https://github.com/nodejs/node/pull/6760)
* [[`c1bf3fc0b1`](https://github.com/nodejs/node/commit/c1bf3fc0b1)] - **doc**: fix links in socket.connecting (Kirill Fomichev) [#6657](https://github.com/nodejs/node/pull/6657)
* [[`ad895f490b`](https://github.com/nodejs/node/commit/ad895f490b)] - **doc**: trim GitHub template comments (Rich Trott) [#6755](https://github.com/nodejs/node/pull/6755)
* [[`af096f1172`](https://github.com/nodejs/node/commit/af096f1172)] - **doc**: add `added` info for `dgram.setBroadcast()` (Rich Trott) [#6750](https://github.com/nodejs/node/pull/6750)
* [[`e8c0dba4bd`](https://github.com/nodejs/node/commit/e8c0dba4bd)] - **doc,events**: fix a link typo (yorkie) [#6640](https://github.com/nodejs/node/pull/6640)
* [[`f31a5ec34a`](https://github.com/nodejs/node/commit/f31a5ec34a)] - **handle_wrap**: IsRefed() -> HasRef() (Jeremiah Senkpiel) [#6546](https://github.com/nodejs/node/pull/6546)
* [[`cc2af793d2`](https://github.com/nodejs/node/commit/cc2af793d2)] - ***Revert*** "**handle_wrap**: IsRefed -> Unrefed, no isAlive check" (Jeremiah Senkpiel) [#6546](https://github.com/nodejs/node/pull/6546)
* [[`2000072903`](https://github.com/nodejs/node/commit/2000072903)] - **handle_wrap**: IsRefed -> Unrefed, no isAlive check (Jeremiah Senkpiel) [#6204](https://github.com/nodejs/node/pull/6204)
* [[`d3132048cb`](https://github.com/nodejs/node/commit/d3132048cb)] - **(SEMVER-MINOR)** **handle_wrap**: expose an `isRefed()` check to JS (Jeremiah Senkpiel) [#5834](https://github.com/nodejs/node/pull/5834)
* [[`59666502c5`](https://github.com/nodejs/node/commit/59666502c5)] - **intl**: Don't crash if v8BreakIterator not available (Steven R. Loomis) [#4253](https://github.com/nodejs/node/pull/4253)
* [[`74582aa590`](https://github.com/nodejs/node/commit/74582aa590)] - **lib**: replace legacy uses of __defineGetter__ (James M Snell) [#6768](https://github.com/nodejs/node/pull/6768)
* [[`559c2583e0`](https://github.com/nodejs/node/commit/559c2583e0)] - **lib,test**: update in preparation for linter update (Rich Trott) [#6498](https://github.com/nodejs/node/pull/6498)
* [[`226b9668db`](https://github.com/nodejs/node/commit/226b9668db)] - **(SEMVER-MINOR)** **repl**: copying tabs shouldn't trigger completion (Eugene Obrezkov) [#5958](https://github.com/nodejs/node/pull/5958)
* [[`ce2d5be4a1`](https://github.com/nodejs/node/commit/ce2d5be4a1)] - **(SEMVER-MINOR)** **repl**: exports `Recoverable` (Blake Embrey) [#3488](https://github.com/nodejs/node/pull/3488)
* [[`635357958d`](https://github.com/nodejs/node/commit/635357958d)] - **repl**: create history file with mode 0600 (Carl Lei) [#3394](https://github.com/nodejs/node/pull/3394)
* [[`e3920d12ef`](https://github.com/nodejs/node/commit/e3920d12ef)] - **src**: fix check-imports.py linter errors (Sakthipriyan Vairamani) [#6105](https://github.com/nodejs/node/pull/6105)
* [[`bbf3b3ebbb`](https://github.com/nodejs/node/commit/bbf3b3ebbb)] - **src**: simplify handlewrap state tracking logic (Ben Noordhuis) [#6395](https://github.com/nodejs/node/pull/6395)
* [[`965274d384`](https://github.com/nodejs/node/commit/965274d384)] - **src**: use libuv's refcounting directly (Ben Noordhuis) [#6395](https://github.com/nodejs/node/pull/6395)
* [[`316871f268`](https://github.com/nodejs/node/commit/316871f268)] - **src**: fix -Wunused-variable compiler warning (Ben Noordhuis) [#6129](https://github.com/nodejs/node/pull/6129)
* [[`1def098b9f`](https://github.com/nodejs/node/commit/1def098b9f)] - **src**: fix sporadic deadlock in SIGUSR1 handler (Ben Noordhuis) [#5904](https://github.com/nodejs/node/pull/5904)
* [[`477e61db9f`](https://github.com/nodejs/node/commit/477e61db9f)] - **src**: don't use locale-sensitive strcasecmp() (Ben Noordhuis) [#6582](https://github.com/nodejs/node/pull/6582)
* [[`1e99643fc9`](https://github.com/nodejs/node/commit/1e99643fc9)] - **src**: remove unused #include statement (Ben Noordhuis) [#6582](https://github.com/nodejs/node/pull/6582)
* [[`62593bd44c`](https://github.com/nodejs/node/commit/62593bd44c)] - **src**: remove pre-openssl 1.0 legacy code (Ben Noordhuis) [#6582](https://github.com/nodejs/node/pull/6582)
* [[`27c17ce9d1`](https://github.com/nodejs/node/commit/27c17ce9d1)] - **src**: fix FindFirstCharacter argument alignment (Anna Henningsen) [#6511](https://github.com/nodejs/node/pull/6511)
* [[`37736f4dad`](https://github.com/nodejs/node/commit/37736f4dad)] - **(SEMVER-MINOR)** **src**: add O_NOATIME constant (Rich Trott) [#6492](https://github.com/nodejs/node/pull/6492)
* [[`bd4454fa0f`](https://github.com/nodejs/node/commit/bd4454fa0f)] - **src,lib**: minor --debug-brk cleanup (Ali Ijaz Sheikh) [#6599](https://github.com/nodejs/node/pull/6599)
* [[`95b7560d8e`](https://github.com/nodejs/node/commit/95b7560d8e)] - **(SEMVER-MINOR)** **src,module**: add --preserve-symlinks command line flag (James M Snell) [#6537](https://github.com/nodejs/node/pull/6537)
* [[`8a7e68ff83`](https://github.com/nodejs/node/commit/8a7e68ff83)] - **test**: check that 2nd handle.close() call is a nop (Ben Noordhuis) [#6395](https://github.com/nodejs/node/pull/6395)
* [[`ccbc78cfc6`](https://github.com/nodejs/node/commit/ccbc78cfc6)] - **test**: remove common.getServiceName() (Rich Trott) [#6709](https://github.com/nodejs/node/pull/6709)
* [[`8c634d78f8`](https://github.com/nodejs/node/commit/8c634d78f8)] - **test**: favor strictEqual() in addon test (Rich Trott) [#6704](https://github.com/nodejs/node/pull/6704)
* [[`1389a4fc5e`](https://github.com/nodejs/node/commit/1389a4fc5e)] - **test**: fix flaky test-preload (Rich Trott) [#6728](https://github.com/nodejs/node/pull/6728)
* [[`adb2d610e6`](https://github.com/nodejs/node/commit/adb2d610e6)] - **test**: include component in tap output (Ben Noordhuis) [#6653](https://github.com/nodejs/node/pull/6653)
* [[`abb063a6a3`](https://github.com/nodejs/node/commit/abb063a6a3)] - **test**: fix test-debugger-repl-break-in-module (Rich Trott) [#6686](https://github.com/nodejs/node/pull/6686)
* [[`5701599767`](https://github.com/nodejs/node/commit/5701599767)] - **test**: fix test-debugger-repl-term (Rich Trott) [#6682](https://github.com/nodejs/node/pull/6682)
* [[`71c91747ef`](https://github.com/nodejs/node/commit/71c91747ef)] - **test**: add tests for stream3 buffering using cork (Alex J Burke) [#6493](https://github.com/nodejs/node/pull/6493)
* [[`b1f58edd54`](https://github.com/nodejs/node/commit/b1f58edd54)] - **test**: abstract skip functionality to common (Jeremiah Senkpiel) [#6697](https://github.com/nodejs/node/pull/6697)
* [[`7d3f5751b3`](https://github.com/nodejs/node/commit/7d3f5751b3)] - **test**: make sure O_NOATIME is present only in Linux (Sakthipriyan Vairamani) [#6614](https://github.com/nodejs/node/pull/6614)
* [[`75adc6a026`](https://github.com/nodejs/node/commit/75adc6a026)] - **test**: move test-debugger-debug-brk to sequential (Rich Trott) [#6731](https://github.com/nodejs/node/pull/6731)
* [[`0bc1784529`](https://github.com/nodejs/node/commit/0bc1784529)] - **test**: refactor doctool tests (Rich Trott) [#6719](https://github.com/nodejs/node/pull/6719)
* [[`89b25fac4c`](https://github.com/nodejs/node/commit/89b25fac4c)] - **test**: fix test-process-exec-argv flakiness (Santiago Gimeno) [#6575](https://github.com/nodejs/node/pull/6575)
* [[`3f9d72408f`](https://github.com/nodejs/node/commit/3f9d72408f)] - **test**: pass python path to node-gyp (hefangshi) [#6646](https://github.com/nodejs/node/pull/6646)
* [[`0c62bd13fb`](https://github.com/nodejs/node/commit/0c62bd13fb)] - **test**: ensure test-npm-install uses correct node (Myles Borins) [#6658](https://github.com/nodejs/node/pull/6658)
* [[`1b71231dd2`](https://github.com/nodejs/node/commit/1b71231dd2)] - **test**: fix test-vm-cached-data to work with old v8 (Myles Borins) [#6317](https://github.com/nodejs/node/pull/6317)
* [[`0eb25cb4b5`](https://github.com/nodejs/node/commit/0eb25cb4b5)] - **test**: test preloaded modules using stdin or repl (Bradley Meck) [#2253](https://github.com/nodejs/node/pull/2253)
* [[`577e132f00`](https://github.com/nodejs/node/commit/577e132f00)] - **test**: fix test-debugger-pid (Santiago Gimeno) [#6584](https://github.com/nodejs/node/pull/6584)
* [[`d74062454b`](https://github.com/nodejs/node/commit/d74062454b)] - **test**: make stdout buffer test more robust (Rich Trott) [#6633](https://github.com/nodejs/node/pull/6633)
* [[`f264749c5c`](https://github.com/nodejs/node/commit/f264749c5c)] - **test**: build addons with V8_DEPRECATION_WARNINGS=1 (Ben Noordhuis) [#6652](https://github.com/nodejs/node/pull/6652)
* [[`01f010f669`](https://github.com/nodejs/node/commit/01f010f669)] - **test**: allow out-of-order replies in dgram tests (Anna Henningsen) [#6607](https://github.com/nodejs/node/pull/6607)
* [[`be241c3262`](https://github.com/nodejs/node/commit/be241c3262)] - **test**: run known_issues tests in CI (Rich Trott) [#6559](https://github.com/nodejs/node/pull/6559)
* [[`8141c2fce2`](https://github.com/nodejs/node/commit/8141c2fce2)] - **test**: add tests for console.\[info|error|warn\] (Bryan English) [#6538](https://github.com/nodejs/node/pull/6538)
* [[`83dab801e2`](https://github.com/nodejs/node/commit/83dab801e2)] - **test**: fix unreliable known_issues test (Rich Trott) [#6555](https://github.com/nodejs/node/pull/6555)
* [[`8c434e6d84`](https://github.com/nodejs/node/commit/8c434e6d84)] - **test**: unmark test-http-regr-gh-2928 as flaky (Rich Trott) [#6540](https://github.com/nodejs/node/pull/6540)
* [[`916e694b2b`](https://github.com/nodejs/node/commit/916e694b2b)] - **test**: avoid test-cluster-master-* flakiness (Stefan Budeanu) [#6531](https://github.com/nodejs/node/pull/6531)
* [[`be5386e0cf`](https://github.com/nodejs/node/commit/be5386e0cf)] - **test,dgram**: add tests for setBroadcast() (Rich Trott) [#6750](https://github.com/nodejs/node/pull/6750)
* [[`1370fdcad5`](https://github.com/nodejs/node/commit/1370fdcad5)] - **test,tools**: test yaml parsing of doctool (Anna Henningsen) [#6495](https://github.com/nodejs/node/pull/6495)
* [[`347abf341d`](https://github.com/nodejs/node/commit/347abf341d)] - **tools**: enforce linting for unix-style line endings (Rich Trott) [#6685](https://github.com/nodejs/node/pull/6685)
* [[`a63c556c11`](https://github.com/nodejs/node/commit/a63c556c11)] - **tools**: remove the minifying logic (Sakthipriyan Vairamani) [#6636](https://github.com/nodejs/node/pull/6636)
* [[`efcbafa3f7`](https://github.com/nodejs/node/commit/efcbafa3f7)] - **tools**: fix regression in doctool (Myles Borins) [#6680](https://github.com/nodejs/node/pull/6680)
* [[`edb29b8096`](https://github.com/nodejs/node/commit/edb29b8096)] - **tools**: lint for object literal spacing (Rich Trott) [#6592](https://github.com/nodejs/node/pull/6592)
* [[`6806ebb608`](https://github.com/nodejs/node/commit/6806ebb608)] - **tools**: lint for use of space in template strings (Rich Trott) [#6591](https://github.com/nodejs/node/pull/6591)
* [[`341eaf202e`](https://github.com/nodejs/node/commit/341eaf202e)] - **tools**: update marked dependency (Daniel Wang) [#6396](https://github.com/nodejs/node/pull/6396)
* [[`94f82553aa`](https://github.com/nodejs/node/commit/94f82553aa)] - **tools**: allow multiple added: version entries (Anna Henningsen) [#6495](https://github.com/nodejs/node/pull/6495)
* [[`1de25f208c`](https://github.com/nodejs/node/commit/1de25f208c)] - **tools**: do not build addons during compilation (Myles Borins) [#6723](https://github.com/nodejs/node/pull/6723)
* [[`26805c97d0`](https://github.com/nodejs/node/commit/26805c97d0)] - **tools**: fix tools/doc/addon-verify.js regression (Anna Henningsen) [#6652](https://github.com/nodejs/node/pull/6652)
* [[`cea17775e0`](https://github.com/nodejs/node/commit/cea17775e0)] - **tools**: parse documentation metadata (Tristian Flanagan) [#6495](https://github.com/nodejs/node/pull/6495)
* [[`a568ad4205`](https://github.com/nodejs/node/commit/a568ad4205)] - **tools**: add mock-y js-yaml dependency to doctool (Anna Henningsen) [#6495](https://github.com/nodejs/node/pull/6495)
* [[`dce6413576`](https://github.com/nodejs/node/commit/dce6413576)] - **tools**: Check in tools for shrinking ICU size, change default to small-icu (Steven R. Loomis) [#6088](https://github.com/nodejs/node/pull/6088)
* [[`d629f265fe`](https://github.com/nodejs/node/commit/d629f265fe)] - **tools**: update ESLint to 2.9.0 (Rich Trott) [#6498](https://github.com/nodejs/node/pull/6498)
* [[`6aa92d5a98`](https://github.com/nodejs/node/commit/6aa92d5a98)] - **tools**: disallow multiple spaces except indentation (Rich Trott) [#6645](https://github.com/nodejs/node/pull/6645)
* [[`ceeae4b180`](https://github.com/nodejs/node/commit/ceeae4b180)] - **tools,test**: make argument linting more stringent (Rich Trott) [#6720](https://github.com/nodejs/node/pull/6720)
* [[`6d1527bb37`](https://github.com/nodejs/node/commit/6d1527bb37)] - **util**: fix invalid date output with util.inspect (Rumkin) [#6504](https://github.com/nodejs/node/pull/6504)
* [[`1d6c17efd7`](https://github.com/nodejs/node/commit/1d6c17efd7)] - **util**: adhere to `noDeprecation` set at runtime (Anna Henningsen) [#6683](https://github.com/nodejs/node/pull/6683)

<a id="6.1.0"></a>
## 2016-05-05, Version 6.1.0 (Current), @Fishrock123

### Notable Changes

* **assert**: `deep{Strict}Equal()` now works correctly with circular references. (Rich Trott) [#6432](https://github.com/nodejs/node/pull/6432)
* **debugger**: Arrays are now formatted correctly in the debugger repl. (cjihrig) [#6448](https://github.com/nodejs/node/pull/6448)
* **deps**: Upgrade OpenSSL sources to 1.0.2h (Shigeki Ohtsu) [#6550](https://github.com/nodejs/node/pull/6550)
  - Please see our [blog post](https://nodejs.org/en/blog/vulnerability/openssl-may-2016/) for more info on the security contents of this release.
* **net**: Introduced a `Socket#connecting` property. (Fedor Indutny) [#6404](https://github.com/nodejs/node/pull/6404)
  - Previously this information was only available as the undocumented, internal `_connecting` property.
* **process**: Introduced `process.cpuUsage()`. (Patrick Mueller) [#6157](https://github.com/nodejs/node/pull/6157)
* **stream**: `Writable#setDefaultEncoding()` now returns `this`. (Alexander Makarenko) [#5040](https://github.com/nodejs/node/pull/5040)
* **util**: Two new additions to `util.inspect()`:
  - Added a `maxArrayLength` option to truncate the formatting of Arrays. (James M Snell) [#6334](https://github.com/nodejs/node/pull/6334)
    - This is set to `100` by default.
  - Added a `showProxy` option for formatting proxy intercepting handlers. (James M Snell) [#6465](https://github.com/nodejs/node/pull/6465)
    - Inspecting proxies is non-trivial and as such this is off by default.

### Commits

* [[`76c9ab5fcf`](https://github.com/nodejs/node/commit/76c9ab5fcf)] - **assert**: allow circular references (Rich Trott) [#6432](https://github.com/nodejs/node/pull/6432)
* [[`7b9ae70757`](https://github.com/nodejs/node/commit/7b9ae70757)] - **benchmark**: Fix crash in net benchmarks (Matt Loring) [#6407](https://www.github.com/nodejs/node/pull/6407)
* [[`0d1985358a`](https://github.com/nodejs/node/commit/0d1985358a)] - **build**: use shorthand lint target from test (Johan Bergström) [#6406](https://github.com/nodejs/node/pull/6406)
* [[`7153f96f0e`](https://github.com/nodejs/node/commit/7153f96f0e)] - **build**: unbreak -prof, disable PIE on OS X (Ben Noordhuis) [#6453](https://github.com/nodejs/node/pull/6453)
* [[`8956432e18`](https://github.com/nodejs/node/commit/8956432e18)] - **build**: exclude tap files from tarballs (Brian White) [#6348](https://github.com/nodejs/node/pull/6348)
* [[`11e7cc5310`](https://github.com/nodejs/node/commit/11e7cc5310)] - **build**: don't compile with -B (Ben Noordhuis) [#6393](https://github.com/nodejs/node/pull/6393)
* [[`1330496bbf`](https://github.com/nodejs/node/commit/1330496bbf)] - **cluster**: remove use of bind() in destroy() (yorkie) [#6502](https://github.com/nodejs/node/pull/6502)
* [[`fdde36909c`](https://github.com/nodejs/node/commit/fdde36909c)] - **crypto**: fix error in deprecation message (Rich Trott) [#6344](https://github.com/nodejs/node/pull/6344)
* [[`2d503b1d4b`](https://github.com/nodejs/node/commit/2d503b1d4b)] - **debugger**: display array contents in repl (cjihrig) [#6448](https://github.com/nodejs/node/pull/6448)
* [[`54f8600613`](https://github.com/nodejs/node/commit/54f8600613)] - **deps**: update openssl asm and asm_obsolete files (Shigeki Ohtsu) [#6550](https://github.com/nodejs/node/pull/6550)
* [[`a5a2944877`](https://github.com/nodejs/node/commit/a5a2944877)] - **deps**: add -no_rand_screen to openssl s_client (Shigeki Ohtsu) [nodejs/io.js#1836](https://github.com/nodejs/io.js/pull/1836)
* [[`3fe68129c8`](https://github.com/nodejs/node/commit/3fe68129c8)] - **deps**: fix asm build error of openssl in x86_win32 (Shigeki Ohtsu) [iojs/io.js#1389](https://github.com/iojs/io.js/pull/1389)
* [[`d159462fed`](https://github.com/nodejs/node/commit/d159462fed)] - **deps**: fix openssl assembly error on ia32 win32 (Fedor Indutny) [iojs/io.js#1389](https://github.com/iojs/io.js/pull/1389)
* [[`3af28d3693`](https://github.com/nodejs/node/commit/3af28d3693)] - **deps**: copy all openssl header files to include dir (Shigeki Ohtsu) [#6550](https://github.com/nodejs/node/pull/6550)
* [[`e6ab3ece65`](https://github.com/nodejs/node/commit/e6ab3ece65)] - **deps**: upgrade openssl sources to 1.0.2h (Shigeki Ohtsu) [#6550](https://github.com/nodejs/node/pull/6550)
* [[`65b6574d59`](https://github.com/nodejs/node/commit/65b6574d59)] - **deps**: backport IsValid changes from 4e8736d in V8 (Michaël Zasso) [#6544](https://github.com/nodejs/node/pull/6544)
* [[`33f24c821b`](https://github.com/nodejs/node/commit/33f24c821b)] - **doc**: adds 'close' events to fs.ReadStream and fs.WriteStream (Jenna Vuong) [#6499](https://github.com/nodejs/node/pull/6499)
* [[`4f728df1bf`](https://github.com/nodejs/node/commit/4f728df1bf)] - **doc**: linkify remaining references to fs.Stats object (Kevin Donahue) [#6485](https://github.com/nodejs/node/pull/6485)
* [[`9a29b50c52`](https://github.com/nodejs/node/commit/9a29b50c52)] - **doc**: fix the lint of an example in cluster.md (yorkie) [#6516](https://github.com/nodejs/node/pull/6516)
* [[`d674493fa5`](https://github.com/nodejs/node/commit/d674493fa5)] - **doc**: add missing underscore for markdown italics (Kevin Donahue) [#6529](https://github.com/nodejs/node/pull/6529)
* [[`7c30f15e1b`](https://github.com/nodejs/node/commit/7c30f15e1b)] - **doc**: ensure consistent grammar in node.1 file (justshiv) [#6426](https://github.com/nodejs/node/pull/6426)
* [[`e5ce53a217`](https://github.com/nodejs/node/commit/e5ce53a217)] - **doc**: fix sentence fragment in fs doc (Rich Trott) [#6488](https://github.com/nodejs/node/pull/6488)
* [[`3e028a143c`](https://github.com/nodejs/node/commit/3e028a143c)] - **doc**: remove obsolete comment in isError() example (cjihrig) [#6486](https://github.com/nodejs/node/pull/6486)
* [[`969f96a019`](https://github.com/nodejs/node/commit/969f96a019)] - **doc**: fix a typo in `__dirname` section (William Luo) [#6473](https://github.com/nodejs/node/pull/6473)
* [[`ab7055b003`](https://github.com/nodejs/node/commit/ab7055b003)] - **doc**: fix fs.realpath man pg links (phette23) [#6451](https://github.com/nodejs/node/pull/6451)
* [[`13e660888f`](https://github.com/nodejs/node/commit/13e660888f)] - **doc**: extra clarification of historySize option (vsemozhetbyt) [#6397](https://github.com/nodejs/node/pull/6397)
* [[`3d5b732660`](https://github.com/nodejs/node/commit/3d5b732660)] - **doc**: clarifies http.serverResponse implementation (Allen Hernandez) [#6072](https://github.com/nodejs/node/pull/6072)
* [[`7034ebe2bc`](https://github.com/nodejs/node/commit/7034ebe2bc)] - **doc**: use `Node.js` in synopsis document (Rich Trott) [#6476](https://github.com/nodejs/node/pull/6476)
* [[`4ae39f9863`](https://github.com/nodejs/node/commit/4ae39f9863)] - **doc**: remove all scrollbar styling (Claudio Rodriguez) [#6479](https://github.com/nodejs/node/pull/6479)
* [[`e6c8da45b1`](https://github.com/nodejs/node/commit/e6c8da45b1)] - **(SEMVER-MINOR)** **doc**: make `writable.setDefaultEncoding()` return `this` (Alexander Makarenko) [#5040](https://github.com/nodejs/node/pull/5040)
* [[`4068d64f4f`](https://github.com/nodejs/node/commit/4068d64f4f)] - **doc**: fix EventEmitter#eventNames() example (Сковорода Никита Андреевич) [#6417](https://github.com/nodejs/node/pull/6417)
* [[`bfcde97251`](https://github.com/nodejs/node/commit/bfcde97251)] - **doc**: fix incorrect syntax in examples (Evan Lucas) [#6463](https://github.com/nodejs/node/pull/6463)
* [[`8eb87ee239`](https://github.com/nodejs/node/commit/8eb87ee239)] - **doc**: Remove extra space in REPL example (Juan) [#6447](https://github.com/nodejs/node/pull/6447)
* [[`fd37d54eb5`](https://github.com/nodejs/node/commit/fd37d54eb5)] - **doc**: added note warning about change to console.endTime() (Ben Page) [#6454](https://github.com/nodejs/node/pull/6454)
* [[`b3f75ec801`](https://github.com/nodejs/node/commit/b3f75ec801)] - **doc**: expand documentation for process.exit() (James M Snell) [#6410](https://github.com/nodejs/node/pull/6410)
* [[`fc0fbf1c63`](https://github.com/nodejs/node/commit/fc0fbf1c63)] - **doc**: subdivide TOC, add auxiliary links (Jeremiah Senkpiel) [#6167](https://github.com/nodejs/node/pull/6167)
* [[`150dd36503`](https://github.com/nodejs/node/commit/150dd36503)] - **doc**: no Node.js(1) (Jeremiah Senkpiel) [#6167](https://github.com/nodejs/node/pull/6167)
* [[`ab84d69048`](https://github.com/nodejs/node/commit/ab84d69048)] - **doc**: better example & synopsis (Jeremiah Senkpiel) [#6167](https://github.com/nodejs/node/pull/6167)
* [[`f6d72791a1`](https://github.com/nodejs/node/commit/f6d72791a1)] - **doc**: update build instructions for OS X (Rich Trott) [#6309](https://github.com/nodejs/node/pull/6309)
* [[`36207c6daf`](https://github.com/nodejs/node/commit/36207c6daf)] - **doc**: correctly document the behavior of ee.once(). (Lance Ball) [#6371](https://github.com/nodejs/node/pull/6371)
* [[`19fb1345ba`](https://github.com/nodejs/node/commit/19fb1345ba)] - **doc**: use Buffer.from() instead of new Buffer() (Jackson Tian) [#6367](https://github.com/nodejs/node/pull/6367)
* [[`fb6753c75c`](https://github.com/nodejs/node/commit/fb6753c75c)] - **doc**: fix v6 changelog (James M Snell) [#6435](https://github.com/nodejs/node/pull/6435)
* [[`2c92a1fe03`](https://github.com/nodejs/node/commit/2c92a1fe03)] - **events**: pass the original listener added by once (DavidCai) [#6394](https://github.com/nodejs/node/pull/6394)
* [[`9ea6b282e8`](https://github.com/nodejs/node/commit/9ea6b282e8)] - **meta**: split CHANGELOG into two files (Myles Borins) [#6337](https://github.com/nodejs/node/pull/6337)
* [[`cbbe95e1e1`](https://github.com/nodejs/node/commit/cbbe95e1e1)] - **(SEMVER-MINOR)** **net**: introduce `Socket#connecting` property (Fedor Indutny) [#6404](https://github.com/nodejs/node/pull/6404)
* [[`534f03c2f0`](https://github.com/nodejs/node/commit/534f03c2f0)] - **openssl**: fix keypress requirement in apps on win32 (Shigeki Ohtsu) [iojs/io.js#1389](https://github.com/iojs/io.js/pull/1389)
* [[`1b9fdba04e`](https://github.com/nodejs/node/commit/1b9fdba04e)] - **(SEMVER-MINOR)** **process**: add process.cpuUsage() - implementation, doc, tests (Patrick Mueller) [#6157](https://github.com/nodejs/node/pull/6157)
* [[`fa9d82d120`](https://github.com/nodejs/node/commit/fa9d82d120)] - **src**: unify implementations of Utf8Value etc. (Anna Henningsen) [#6357](https://github.com/nodejs/node/pull/6357)
* [[`65030c77b7`](https://github.com/nodejs/node/commit/65030c77b7)] - **test**: fix alpn tests for openssl1.0.2h (Shigeki Ohtsu) [#6550](https://github.com/nodejs/node/pull/6550)
* [[`7641f9a6de`](https://github.com/nodejs/node/commit/7641f9a6de)] - **test**: refactor large event emitter tests (cjihrig) [#6446](https://github.com/nodejs/node/pull/6446)
* [[`5fe5fa2897`](https://github.com/nodejs/node/commit/5fe5fa2897)] - **test**: make addon testing part of `make test` (Ben Noordhuis) [#6232](https://github.com/nodejs/node/pull/6232)
* [[`457d12a0a1`](https://github.com/nodejs/node/commit/457d12a0a1)] - **test**: add failing url parse tests as known_issue (James M Snell) [#5885](https://github.com/nodejs/node/pull/5885)
* [[`089362f8b8`](https://github.com/nodejs/node/commit/089362f8b8)] - **test,tools**: limit lint tolerance of gc global (Rich Trott) [#6324](https://github.com/nodejs/node/pull/6324)
* [[`6d1606ee94`](https://github.com/nodejs/node/commit/6d1606ee94)] - **test,tools**: adjust function argument alignment (Rich Trott) [#6390](https://github.com/nodejs/node/pull/6390)
* [[`08e0884ae0`](https://github.com/nodejs/node/commit/08e0884ae0)] - **tools**: add -F flag for fixing lint issues (Rich Trott) [#6483](https://github.com/nodejs/node/pull/6483)
* [[`9f23cb24f2`](https://github.com/nodejs/node/commit/9f23cb24f2)] - **tools**: fix exit code when linting from CI (Brian White) [#6412](https://github.com/nodejs/node/pull/6412)
* [[`e62c42b8f4`](https://github.com/nodejs/node/commit/e62c42b8f4)] - **tools**: remove default parameters from lint rule (Rich Trott) [#6411](https://github.com/nodejs/node/pull/6411)
* [[`66903f6695`](https://github.com/nodejs/node/commit/66903f6695)] - **tools**: add tests for the doctool (Ian Kronquist) [#6031](https://github.com/nodejs/node/pull/6031)
* [[`3f608b16a7`](https://github.com/nodejs/node/commit/3f608b16a7)] - **tools**: lint for function argument alignment (Rich Trott) [#6390](https://github.com/nodejs/node/pull/6390)
* [[`91ab769940`](https://github.com/nodejs/node/commit/91ab769940)] - **(SEMVER-MINOR)** **util**: truncate inspect array and typed array (James M Snell) [#6334](https://github.com/nodejs/node/pull/6334)
* [[`0bca959617`](https://github.com/nodejs/node/commit/0bca959617)] - **(SEMVER-MINOR)** **util**: fix inspecting of proxy objects (James M Snell) [#6465](https://github.com/nodejs/node/pull/6465)

<a id="6.0.0"></a>
## 2016-04-26, Version 6.0.0 (Current), @jasnell

### Notable changes

The following significant changes have been made since the previous Node.js
v5.0.0 release.

* Buffer
  * New Buffer constructors have been added
    [#4682](https://github.com/nodejs/node/pull/4682) and
    [#5833](https://github.com/nodejs/node/pull/5833).
  * Existing `Buffer()` and `SlowBuffer()` constructors have been deprecated
    in docs [#4682](https://github.com/nodejs/node/pull/4682) and
    [#5833](https://github.com/nodejs/node/pull/5833).
  * Previously deprecated Buffer APIs are removed
    [#5048](https://github.com/nodejs/node/pull/5048),
    [#4594](https://github.com/nodejs/node/pull/4594).
  * Improved error handling [#4514](https://github.com/nodejs/node/pull/4514).
  * The `Buffer.prototype.lastIndexOf()` method has been added
    [#4846](https://github.com/nodejs/node/pull/4846).
* Cluster
  * Worker emitted as first argument in 'message' event
    [#5361](https://github.com/nodejs/node/pull/5361).
  * The `worker.exitedAfterDisconnect` property replaces `worker.suicide`
    [#3743](https://github.com/nodejs/node/pull/3743).
* Console
  * Calling `console.timeEnd()` with an unknown label now emits a process
    warning rather than throwing
    [#5901](https://github.com/nodejs/node/pull/5901).
* Crypto
  * Improved error handling [#3100](https://github.com/nodejs/node/pull/3100),
    [#5611](https://github.com/nodejs/node/pull/5611).
  * Simplified Certificate class bindings
    [#5382](https://github.com/nodejs/node/pull/5382).
  * Improved control over FIPS mode
    [#5181](https://github.com/nodejs/node/pull/5181).
  * pbkdf2 digest overloading is deprecated
    [#4047](https://github.com/nodejs/node/pull/4047).
* Dependencies
  * Reintroduce shared c-ares build support
    [#5775](https://github.com/nodejs/node/pull/5775).
  * V8 updated to 5.0.71.35 [#6372](https://github.com/nodejs/node/pull/6372).
* DNS
  * Add `dns.resolvePtr()` API to query plain DNS PTR records
    [#4921](https://github.com/nodejs/node/pull/4921).
* Domains
  * Clear stack when no error handler
  [#4659](https://github.com/nodejs/node/pull/4659).
* Events
  * The `EventEmitter.prototype._events` object no longer inherits from
    Object.prototype [#6092](https://github.com/nodejs/node/pull/6092).
  * The `EventEmitter.prototype.prependListener()` and
    `EventEmitter.prototype.prependOnceListener()` methods have been added
    [#6032](https://github.com/nodejs/node/pull/6032).
* File System
  * The `fs.realpath()` and `fs.realpathSync()` methods have been updated
    to use a more efficient libuv-based implementation. This change includes
    the removal of the `cache` argument and the method can throw new errors
    [#3594](https://github.com/nodejs/node/pull/3594).
  * FS apis can now accept and return paths as Buffers
    [#5616](https://github.com/nodejs/node/pull/5616).
  * Error handling and type checking improvements
    [#5616](https://github.com/nodejs/node/pull/5616),
    [#5590](https://github.com/nodejs/node/pull/5590),
    [#4518](https://github.com/nodejs/node/pull/4518),
    [#3917](https://github.com/nodejs/node/pull/3917).
  * fs.read's string interface is deprecated
    [#4525](https://github.com/nodejs/node/pull/4525).
* HTTP
  * 'clientError' can now be used to return custom errors from an
    HTTP server [#4557](https://github.com/nodejs/node/pull/4557).
* Modules
  * Current directory is now prioritized for local lookups
    [#5689](https://github.com/nodejs/node/pull/5689).
  * Symbolic links are preserved when requiring modules
    [#5950](https://github.com/nodejs/node/pull/5950).
* Net
  * DNS hints no longer implicitly set
    [#6021](https://github.com/nodejs/node/pull/6021).
  * Improved error handling and type checking
    [#5981](https://github.com/nodejs/node/pull/5981),
    [#5733](https://github.com/nodejs/node/pull/5733),
    [#2904](https://github.com/nodejs/node/pull/2904).
* npm
  * Running npm requires the node binary to be in the path
    [#6098](https://github.com/nodejs/node/pull/6098).
* OS X
  * MACOSX_DEPLOYMENT_TARGET has been bumped up to 10.7
    [#6402](https://github.com/nodejs/node/pull/6402).
* Path
  * Improved type checking [#5348](https://github.com/nodejs/node/pull/5348).
* Process
  * Introduce process warnings API
    [#4782](https://github.com/nodejs/node/pull/4782).
  * Throw exception when non-function passed to nextTick
    [#3860](https://github.com/nodejs/node/pull/3860).
* Querystring
  * The object returned by `querystring.parse()` no longer inherits from
    Object.prototype [#6055](https://github.com/nodejs/node/pull/6055).
* Readline
  * Key info is emitted unconditionally
    [#6024](https://github.com/nodejs/node/pull/6024).
  * History can now be explicitly disabled
    [#6352](https://github.com/nodejs/node/pull/6352).
* REPL
  * Assignment to `_` will emit a warning
    [#5535](https://github.com/nodejs/node/pull/5535).
  * Expressions will no longer be completed when eval fails
    [#6328](https://github.com/nodejs/node/pull/6328).
* Timers
  * Fail early when callback is not a function
    [#4362](https://github.com/nodejs/node/pull/4362).
* Streams
  * `null` is now an invalid chunk to write in object mode
    [#6170](https://github.com/nodejs/node/pull/6170).
* TLS
  * Rename 'clientError' to 'tlsClientError'
    [#4557](https://github.com/nodejs/node/pull/4557).
  * SHA1 used for sessionIdContext
    [#3866](https://github.com/nodejs/node/pull/3866).
* TTY
  * Previously deprecated setRawMode wrapper is removed
    [#2528](https://github.com/nodejs/node/pull/2528).
* URL
  * Username and password will be dropped by `url.resolve()` if the host
    changes [#1480](https://github.com/nodejs/node/pull/1480).
* Util
  * Changes to Error object formatting
    [#4582](https://github.com/nodejs/node/pull/4582).
  * The `util._extend()` method has been deprecated
    [#4903](https://github.com/nodejs/node/pull/4903)
  * The `util.log()` method has been deprecated
    [#6161](https://github.com/nodejs/node/pull/6161).
* Windows
  * Windows XP and Vista are no longer supported
    [#5167](https://github.com/nodejs/node/pull/5167).
* Zlib
  * Multiple improvements have been made to Zlib processing
    [#5883](https://github.com/nodejs/node/pull/5883) and
    [#5707](https://github.com/nodejs/node/pull/5707).


### Commits

Semver-major Changes since v5.0.0

* [[`829d613daa`](https://github.com/nodejs/node/commit/829d613daa)] - **(SEMVER-MAJOR)** **buffer**: docs-only deprecate SlowBuffer (James M Snell) [#5833](https://github.com/nodejs/node/pull/5833)
* [[`d0c6e6f7c4`](https://github.com/nodejs/node/commit/d0c6e6f7c4)] - **(SEMVER-MAJOR)** **buffer**: add Buffer.allocUnsafeSlow(size) (James M Snell) [#5833](https://github.com/nodejs/node/pull/5833)
* [[`85ab4a5f12`](https://github.com/nodejs/node/commit/85ab4a5f12)] - **(SEMVER-MAJOR)** **buffer**: add .from(), .alloc() and .allocUnsafe() (James M Snell) [#4682](https://github.com/nodejs/node/pull/4682)
* [[`2c55cc2d2c`](https://github.com/nodejs/node/commit/2c55cc2d2c)] - **(SEMVER-MAJOR)** **buffer**: remove deprecated Buffer.write branch (dcposch@dcpos.ch) [#5048](https://github.com/nodejs/node/pull/5048)
* [[`101bca988c`](https://github.com/nodejs/node/commit/101bca988c)] - **(SEMVER-MAJOR)** **buffer**: remove deprecated buffer.get/.set methods (Feross Aboukhadijeh) [#4594](https://github.com/nodejs/node/pull/4594)
* [[`3b27dd5ce1`](https://github.com/nodejs/node/commit/3b27dd5ce1)] - **(SEMVER-MAJOR)** **buffer**: throw if both length and enc are passed (Mathias Buus) [#4514](https://github.com/nodejs/node/pull/4514)
* [[`ce864ef502`](https://github.com/nodejs/node/commit/ce864ef502)] - **(SEMVER-MAJOR)** **build**: Bump MACOSX_DEPLOYMENT_TARGET to 10.7 (Сковорода Никита Андреевич) [#6402](https://github.com/nodejs/node/pull/6402)
* [[`90a5fc20be`](https://github.com/nodejs/node/commit/90a5fc20be)] - **(SEMVER-MAJOR)** **build**: remove lint/dotfiles from release tarball (Johan Bergström) [#5695](https://github.com/nodejs/node/pull/5695)
* [[`66f048af6f`](https://github.com/nodejs/node/commit/66f048af6f)] - **(SEMVER-MAJOR)** **cluster**: migrate from worker.suicide (Evan Lucas) [#3743](https://github.com/nodejs/node/pull/3743)
* [[`66f4586dd0`](https://github.com/nodejs/node/commit/66f4586dd0)] - **(SEMVER-MAJOR)** **cluster**: emit worker as first 'message' event arg (Ben Noordhuis) [#5361](https://github.com/nodejs/node/pull/5361)
* [[`30507951d9`](https://github.com/nodejs/node/commit/30507951d9)] - **(SEMVER-MAJOR)** **console**: timeEnd() with no label emits warning (Eugene Obrezkov) [#5901](https://github.com/nodejs/node/pull/5901)
* [[`a5cce79ec3`](https://github.com/nodejs/node/commit/a5cce79ec3)] - **(SEMVER-MAJOR)** **console**: delete timers that have ended (Vladimir Varankin) [#3562](https://github.com/nodejs/node/pull/3562)
* [[`47cca06412`](https://github.com/nodejs/node/commit/47cca06412)] - **(SEMVER-MAJOR)** **crypto**: better error message for createHash (Calvin Metcalf) [#6042](https://github.com/nodejs/node/pull/6042)
* [[`41feaa89e0`](https://github.com/nodejs/node/commit/41feaa89e0)] - **(SEMVER-MAJOR)** **crypto**: improve error messages (Sakthipriyan Vairamani) [#3100](https://github.com/nodejs/node/pull/3100)
* [[`f429fe1b88`](https://github.com/nodejs/node/commit/f429fe1b88)] - **(SEMVER-MAJOR)** **crypto**: fail early when loading crypto without openssl (James M Snell) [#5611](https://github.com/nodejs/node/pull/5611)
* [[`a37401e061`](https://github.com/nodejs/node/commit/a37401e061)] - **(SEMVER-MAJOR)** **crypto**: simplify Certificate class bindings (Alexander Makarenko) [#5382](https://github.com/nodejs/node/pull/5382)
* [[`7c48cb5601`](https://github.com/nodejs/node/commit/7c48cb5601)] - **(SEMVER-MAJOR)** **crypto**: Improve control of FIPS mode (Stefan Budeanu) [#5181](https://github.com/nodejs/node/pull/5181)
* [[`a1163582c5`](https://github.com/nodejs/node/commit/a1163582c5)] - **(SEMVER-MAJOR)** **crypto**: pbkdf2 deprecate digest overload. (Tom Gallacher) [#4047](https://github.com/nodejs/node/pull/4047)
* [[`b010c87164`](https://github.com/nodejs/node/commit/b010c87164)] - **(SEMVER-MAJOR)** **crypto, string_bytes**: treat `buffer` str as `utf8` (Fedor Indutny) [#5522](https://github.com/nodejs/node/pull/5522)
* [[`a42453b085`](https://github.com/nodejs/node/commit/a42453b085)] - **(SEMVER-MAJOR)** **deps**: upgrade V8 to 5.0.71.32 (Ali Ijaz Sheikh) [#6111](https://github.com/nodejs/node/pull/6111)
* [[`2253be95d0`](https://github.com/nodejs/node/commit/2253be95d0)] - **(SEMVER-MAJOR)** **deps**: reintroduce supporting shared c-ares builds (Johan Bergström) [#5775](https://github.com/nodejs/node/pull/5775)
* [[`4bc1cccb22`](https://github.com/nodejs/node/commit/4bc1cccb22)] - **(SEMVER-MAJOR)** **dgram**: pass null as error on successful send() (cjihrig) [#5929](https://github.com/nodejs/node/pull/5929)
* [[`dbdbdd4998`](https://github.com/nodejs/node/commit/dbdbdd4998)] - **(SEMVER-MAJOR)** **dns**: add resolvePtr to query plain DNS PTR records (Daniel Turing) [#4921](https://github.com/nodejs/node/pull/4921)
* [[`c4ab861a49`](https://github.com/nodejs/node/commit/c4ab861a49)] - **(SEMVER-MAJOR)** **dns**: add failure test for dns.resolveXXX (Daniel Turing) [#4921](https://github.com/nodejs/node/pull/4921)
* [[`f3be421c1c`](https://github.com/nodejs/node/commit/f3be421c1c)] - **(SEMVER-MAJOR)** **dns**: coerce port to number in lookupService (Evan Lucas) [#4883](https://github.com/nodejs/node/pull/4883)
* [[`fa74805bac`](https://github.com/nodejs/node/commit/fa74805bac)] - **(SEMVER-MAJOR)** **doc**: doc-only deprecation for util.log() (Jackson Tian) [#6161](https://github.com/nodejs/node/pull/6161)
* [[`34b1a64068`](https://github.com/nodejs/node/commit/34b1a64068)] - **(SEMVER-MAJOR)** **doc**: minor copy improvement in buffer.markdown (James M Snell) [#5833](https://github.com/nodejs/node/pull/5833)
* [[`4d4f3535a9`](https://github.com/nodejs/node/commit/4d4f3535a9)] - **(SEMVER-MAJOR)** **doc**: general improvements to fs.markdown (James M Snell) [#5616](https://github.com/nodejs/node/pull/5616)
* [[`d8290286b3`](https://github.com/nodejs/node/commit/d8290286b3)] - **(SEMVER-MAJOR)** **doc**: document deprecation of util._extend (Benjamin Gruenbaum) [#4903](https://github.com/nodejs/node/pull/4903)
* [[`3abf9d5df5`](https://github.com/nodejs/node/commit/3abf9d5df5)] - **(SEMVER-MAJOR)** **doc, tls**: deprecate createSecurePair (Yuval Brik) [#6063](https://github.com/nodejs/node/pull/6063)
* [[`90204cc468`](https://github.com/nodejs/node/commit/90204cc468)] - **(SEMVER-MAJOR)** **domains**: clear stack when no error handler (Julien Gilli) [#4659](https://github.com/nodejs/node/pull/4659)
* [[`c3ad97d3cc`](https://github.com/nodejs/node/commit/c3ad97d3cc)] - **(SEMVER-MAJOR)** **events**: don't inherit from Object.prototype (Brian White) [#6092](https://github.com/nodejs/node/pull/6092)
* [[`a2492f989f`](https://github.com/nodejs/node/commit/a2492f989f)] - **(SEMVER-MAJOR)** **fs**: optimize realpath using uv_fs_realpath() (Yuval Brik) [#3594](https://github.com/nodejs/node/pull/3594)
* [[`53a95a5b12`](https://github.com/nodejs/node/commit/53a95a5b12)] - **(SEMVER-MAJOR)** **fs**: make fs.watch error message more useful (James M Snell) [#5616](https://github.com/nodejs/node/pull/5616)
* [[`060e5f0c00`](https://github.com/nodejs/node/commit/060e5f0c00)] - **(SEMVER-MAJOR)** **fs**: Buffer and encoding enhancements to fs API (James M Snell) [#5616](https://github.com/nodejs/node/pull/5616)
* [[`8bb60e3c8d`](https://github.com/nodejs/node/commit/8bb60e3c8d)] - **(SEMVER-MAJOR)** **fs**: improve error message for invalid flag (James M Snell) [#5590](https://github.com/nodejs/node/pull/5590)
* [[`1124de2d76`](https://github.com/nodejs/node/commit/1124de2d76)] - **(SEMVER-MAJOR)** **fs**: deprecate fs.read's string interface (Sakthipriyan Vairamani) [#4525](https://github.com/nodejs/node/pull/4525)
* [[`2b15e68bbe`](https://github.com/nodejs/node/commit/2b15e68bbe)] - **(SEMVER-MAJOR)** **fs**: fs.read into zero buffer should not throw exception (Feross Aboukhadijeh) [#4518](https://github.com/nodejs/node/pull/4518)
* [[`8b97249893`](https://github.com/nodejs/node/commit/8b97249893)] - **(SEMVER-MAJOR)** **fs**: fix the error report on fs.link(Sync) (yorkie) [#3917](https://github.com/nodejs/node/pull/3917)
* [[`5f76b24e5e`](https://github.com/nodejs/node/commit/5f76b24e5e)] - **(SEMVER-MAJOR)** **http**: overridable `clientError` (Fedor Indutny) [#4557](https://github.com/nodejs/node/pull/4557)
* [[`d01eb6882f`](https://github.com/nodejs/node/commit/d01eb6882f)] - **(SEMVER-MAJOR)** **lib**: add 'pid' prefix in `internal/util` (Minwoo Jung) [#3878](https://github.com/nodejs/node/pull/3878)
* [[`20285ad177`](https://github.com/nodejs/node/commit/20285ad177)] - **(SEMVER-MAJOR)** **lib**: Consistent error messages in all modules (micnic) [#3374](https://github.com/nodejs/node/pull/3374)
* [[`94b9948d63`](https://github.com/nodejs/node/commit/94b9948d63)] - **(SEMVER-MAJOR)** **lib,src**: ensure '(node:pid)' prefix for stdout logging (Minwoo Jung) [#3833](https://github.com/nodejs/node/pull/3833)
* [[`b70dc67828`](https://github.com/nodejs/node/commit/b70dc67828)] - **(SEMVER-MAJOR)** **lib,test**: remove publicly exposed freelist (cjihrig) [#3738](https://github.com/nodejs/node/pull/3738)
* [[`c43b182b11`](https://github.com/nodejs/node/commit/c43b182b11)] - **(SEMVER-MAJOR)** **module**: preserve symlinks when requiring (Alex Lamar) [#5950](https://github.com/nodejs/node/pull/5950)
* [[`d38503ab01`](https://github.com/nodejs/node/commit/d38503ab01)] - **(SEMVER-MAJOR)** **module**: prioritize current dir for local lookups (Phillip Johnsen) [#5689](https://github.com/nodejs/node/pull/5689)
* [[`71470a8e45`](https://github.com/nodejs/node/commit/71470a8e45)] - **(SEMVER-MAJOR)** **module**: pass v8::Object to linked module initialization function (Phillip Kovalev) [#4771](https://github.com/nodejs/node/pull/4771)
* [[`18490d3d5a`](https://github.com/nodejs/node/commit/18490d3d5a)] - **(SEMVER-MAJOR)** **module**: always decorate thrown errors (Brian White) [#4287](https://github.com/nodejs/node/pull/4287)
* [[`c32c889c45`](https://github.com/nodejs/node/commit/c32c889c45)] - **(SEMVER-MAJOR)** **net**: Validate port in createServer().listen() (Dirceu Pereira Tiegs) [#5732](https://github.com/nodejs/node/pull/5732)
* [[`b85a50b6da`](https://github.com/nodejs/node/commit/b85a50b6da)] - **(SEMVER-MAJOR)** **net**: remove implicit setting of DNS hints (cjihrig) [#6021](https://github.com/nodejs/node/pull/6021)
* [[`ec49fc8229`](https://github.com/nodejs/node/commit/ec49fc8229)] - **(SEMVER-MAJOR)** **net**: improve socket.write() error message (Phillip Johnsen) [#5981](https://github.com/nodejs/node/pull/5981)
* [[`d0edabecbf`](https://github.com/nodejs/node/commit/d0edabecbf)] - **(SEMVER-MAJOR)** **net**: strict checking for internal/net isLegalPort (James M Snell) [#5733](https://github.com/nodejs/node/pull/5733)
* [[`a78b3344f8`](https://github.com/nodejs/node/commit/a78b3344f8)] - **(SEMVER-MAJOR)** **net**: type check createServer options object (Sam Roberts) [#2904](https://github.com/nodejs/node/pull/2904)
* [[`25751bedfe`](https://github.com/nodejs/node/commit/25751bedfe)] - **(SEMVER-MAJOR)** **node**: deprecate process.EventEmitter (Evan Lucas) [#5049](https://github.com/nodejs/node/pull/5049)
* [[`08085c49b6`](https://github.com/nodejs/node/commit/08085c49b6)] - **(SEMVER-MAJOR)** **path**: assert inputs are strings (Brian White) [#5348](https://github.com/nodejs/node/pull/5348)
* [[`d1000b4137`](https://github.com/nodejs/node/commit/d1000b4137)] - **(SEMVER-MAJOR)** **path**: make format() consistent and more functional (Nathan Woltman) [#2408](https://github.com/nodejs/node/pull/2408)
* [[`c6656db352`](https://github.com/nodejs/node/commit/c6656db352)] - **(SEMVER-MAJOR)** **process**: add 'warning' event and process.emitWarning() (James M Snell) [#4782](https://github.com/nodejs/node/pull/4782)
* [[`72e3dd9f43`](https://github.com/nodejs/node/commit/72e3dd9f43)] - **(SEMVER-MAJOR)** **process**: throw on non-function to nextTick() (yorkie) [#3860](https://github.com/nodejs/node/pull/3860)
* [[`33c286154b`](https://github.com/nodejs/node/commit/33c286154b)] - **(SEMVER-MAJOR)** **querystring**: don't inherit from Object.prototype (Brian White) [#6055](https://github.com/nodejs/node/pull/6055)
* [[`5c8b5971eb`](https://github.com/nodejs/node/commit/5c8b5971eb)] - **(SEMVER-MAJOR)** **querystring**: using toString for objects on querystring.escape (Igor Kalashnikov) [#5341](https://github.com/nodejs/node/pull/5341)
* [[`9c6ef5b3e1`](https://github.com/nodejs/node/commit/9c6ef5b3e1)] - **(SEMVER-MAJOR)** **readline**: allow history to be disabled (surya panikkal) [#6352](https://github.com/nodejs/node/pull/6352)
* [[`0a62f929da`](https://github.com/nodejs/node/commit/0a62f929da)] - **(SEMVER-MAJOR)** **readline**: emit key info unconditionally (cjihrig) [#6024](https://github.com/nodejs/node/pull/6024)
* [[`3de9bc9429`](https://github.com/nodejs/node/commit/3de9bc9429)] - **(SEMVER-MAJOR)** **readline**: document emitKeypressEvents() (cjihrig) [#6024](https://github.com/nodejs/node/pull/6024)
* [[`ca2e8b292f`](https://github.com/nodejs/node/commit/ca2e8b292f)] - **(SEMVER-MAJOR)** **readline**: deprecate undocumented exports (cjihrig) [#3862](https://github.com/nodejs/node/pull/3862)
* [[`bd2cee1b10`](https://github.com/nodejs/node/commit/bd2cee1b10)] - **(SEMVER-MAJOR)** **repl**: don’t complete expressions when eval fails (Anna Henningsen) [#6328](https://github.com/nodejs/node/pull/6328)
* [[`ad8257fa5b`](https://github.com/nodejs/node/commit/ad8257fa5b)] - **(SEMVER-MAJOR)** **repl**: Assignment of _ allowed with warning (Lance Ball) [#5535](https://github.com/nodejs/node/pull/5535)
* [[`757fbac64b`](https://github.com/nodejs/node/commit/757fbac64b)] - **(SEMVER-MAJOR)** **src**: remove deprecated internal functions (Ben Noordhuis) [#6053](https://github.com/nodejs/node/pull/6053)
* [[`57003520f8`](https://github.com/nodejs/node/commit/57003520f8)] - **(SEMVER-MAJOR)** **src**: attach error to stack on displayErrors (cjihrig) [#4874](https://github.com/nodejs/node/pull/4874)
* [[`4e46931406`](https://github.com/nodejs/node/commit/4e46931406)] - **(SEMVER-MAJOR)** **src**: deprecate undocumented variables (Jackson Tian) [#1838](https://github.com/nodejs/node/pull/1838)
* [[`c0abecdaf3`](https://github.com/nodejs/node/commit/c0abecdaf3)] - **(SEMVER-MAJOR)** **stream**: make null an invalid chunk to write in object mode (Calvin Metcalf) [#6170](https://github.com/nodejs/node/pull/6170)
* [[`652782d137`](https://github.com/nodejs/node/commit/652782d137)] - **(SEMVER-MAJOR)** **test**: update test-repl-require for local paths (Myles Borins) [#5689](https://github.com/nodejs/node/pull/5689)
* [[`a5aa7c1713`](https://github.com/nodejs/node/commit/a5aa7c1713)] - **(SEMVER-MAJOR)** **test**: expand test case for unknown file open flags (James M Snell) [#5590](https://github.com/nodejs/node/pull/5590)
* [[`2c33819370`](https://github.com/nodejs/node/commit/2c33819370)] - **(SEMVER-MAJOR)** **test**: fix tests that check error messages (cjihrig) [#3727](https://github.com/nodejs/node/pull/3727)
* [[`ac153bd2a6`](https://github.com/nodejs/node/commit/ac153bd2a6)] - **(SEMVER-MAJOR)** **timers**: fail early when callback is not a function (Anna Henningsen) [#4362](https://github.com/nodejs/node/pull/4362)
* [[`1ab6b21360`](https://github.com/nodejs/node/commit/1ab6b21360)] - **(SEMVER-MAJOR)** **tls**: rename `clientError` to `tlsClientError` (Fedor Indutny) [#4557](https://github.com/nodejs/node/pull/4557)
* [[`df268f97bc`](https://github.com/nodejs/node/commit/df268f97bc)] - **(SEMVER-MAJOR)** **tls**: use SHA1 for sessionIdContext (Stefan Budeanu) [#3866](https://github.com/nodejs/node/pull/3866)
* [[`102a77655a`](https://github.com/nodejs/node/commit/102a77655a)] - **(SEMVER-MAJOR)** **tools**: do not rewrite npm shebang in install.py (Evan Lucas) [#6098](https://github.com/nodejs/node/pull/6098)
* [[`a2c0aa84e0`](https://github.com/nodejs/node/commit/a2c0aa84e0)] - **(SEMVER-MAJOR)** **tty**: Remove deprecated setRawMode wrapper (Wyatt Preul) [#2528](https://github.com/nodejs/node/pull/2528)
* [[`1b50b80e06`](https://github.com/nodejs/node/commit/1b50b80e06)] - **(SEMVER-MAJOR)** **url**: drop auth in `url.resolve()` if host changes (Alex Kocharin) [#1480](https://github.com/nodejs/node/pull/1480)
* [[`e2f47f5698`](https://github.com/nodejs/node/commit/e2f47f5698)] - **(SEMVER-MAJOR)** **util**: Change how Error objects are formatted (Mudit Ameta) [#4582](https://github.com/nodejs/node/pull/4582)
* [[`93d6b5fb68`](https://github.com/nodejs/node/commit/93d6b5fb68)] - **(SEMVER-MAJOR)** **util**: use consistent Dates in inspect() (Xotic750) [#4318](https://github.com/nodejs/node/pull/4318)
* [[`24012a879d`](https://github.com/nodejs/node/commit/24012a879d)] - **(SEMVER-MAJOR)** **util**: make inspect more reliable (Evan Lucas) [#4098](https://github.com/nodejs/node/pull/4098)
* [[`007cfea308`](https://github.com/nodejs/node/commit/007cfea308)] - **(SEMVER-MAJOR)** **util**: remove pump (Wyatt Preul) [#2531](https://github.com/nodejs/node/pull/2531)
* [[`4cf19ad1bb`](https://github.com/nodejs/node/commit/4cf19ad1bb)] - **(SEMVER-MAJOR)** **util**: Remove exec, has been deprecated for years (Wyatt Preul) [#2530](https://github.com/nodejs/node/pull/2530)
* [[`34a35919e1`](https://github.com/nodejs/node/commit/34a35919e1)] - **(SEMVER-MAJOR)** **util**: improve typed array formatting (Ben Noordhuis) [#3793](https://github.com/nodejs/node/pull/3793)
* [[`1cf26c036d`](https://github.com/nodejs/node/commit/1cf26c036d)] - **(SEMVER-MAJOR)** **win**: prevent node from running on Windows Vista or earlier (Alexis Campailla) [#5167](https://github.com/nodejs/node/pull/5167)
* [[`55db19074d`](https://github.com/nodejs/node/commit/55db19074d)] - **(SEMVER-MAJOR)** **win,msi**: prevent from installing on Windows Vista or earlier (Alexis Campailla) [#5167](https://github.com/nodejs/node/pull/5167)
* [[`54a5287e3e`](https://github.com/nodejs/node/commit/54a5287e3e)] - **(SEMVER-MAJOR)** **zlib**: fix gzip member head/buffer boundary issue (Anna Henningsen) [#5883](https://github.com/nodejs/node/pull/5883)
* [[`8b43d3f52d`](https://github.com/nodejs/node/commit/8b43d3f52d)] - **(SEMVER-MAJOR)** **zlib**: do not emit event on *Sync() methods (Rich Trott) [#5707](https://github.com/nodejs/node/pull/5707)


Semver-minor and patch commits since v5.11.0

* [[`7d4f652ced`](https://github.com/nodejs/node/commit/7d4f652ced)] - **benchmarks**: fix lint error (Myles Borins) [#6314](https://github.com/nodejs/node/pull/6314)
* [[`451f4fc88e`](https://github.com/nodejs/node/commit/451f4fc88e)] - **benchmarks**: add microbenchmarks for new ES6 features (James M Snell) [#6222](https://github.com/nodejs/node/pull/6222)
* [[`962c1e1cac`](https://github.com/nodejs/node/commit/962c1e1cac)] - **benchmarks**: add common.v8ForceOptimization (James M Snell) [#6222](https://github.com/nodejs/node/pull/6222)
* [[`d866438ce1`](https://github.com/nodejs/node/commit/d866438ce1)] - **buffer**: safeguard against accidental kNoZeroFill (Сковорода Никита Андреевич) [nodejs/node-private#30](https://github.com/nodejs/node-private/pull/30)
* [[`3350b4712b`](https://github.com/nodejs/node/commit/3350b4712b)] - **(SEMVER-MINOR)** **buffer**: add Buffer.prototype.lastIndexOf() (dcposch@dcpos.ch) [#4846](https://github.com/nodejs/node/pull/4846)
* [[`afd821a91d`](https://github.com/nodejs/node/commit/afd821a91d)] - **buffer**: faster case for create buffer from empty string (Jackson Tian) [#4414](https://github.com/nodejs/node/pull/4414)
* [[`73fc440870`](https://github.com/nodejs/node/commit/73fc440870)] - **buffer**: fix buffer alignment restriction (Ali Ijaz Sheikh) [#5752](https://github.com/nodejs/node/pull/5752)
* [[`206a81fb14`](https://github.com/nodejs/node/commit/206a81fb14)] - **buffer**: avoid undefined behaviour (Ali Ijaz Sheikh) [#5494](https://github.com/nodejs/node/pull/5494)
* [[`ebbbc5a790`](https://github.com/nodejs/node/commit/ebbbc5a790)] - **buffer**: replace deprecated SetWeak usage (Ali Ijaz Sheikh) [#5204](https://github.com/nodejs/node/pull/5204)
* [[`34aac23d0b`](https://github.com/nodejs/node/commit/34aac23d0b)] - **buffer**: cleanup CallbackInfo (Ali Ijaz Sheikh) [#5204](https://github.com/nodejs/node/pull/5204)
* [[`d7259280ee`](https://github.com/nodejs/node/commit/d7259280ee)] - **build**: update android-configure script for npm (Robert Chiras) [#6349](https://github.com/nodejs/node/pull/6349)
* [[`8cf94006c9`](https://github.com/nodejs/node/commit/8cf94006c9)] - **build**: do not ignore errors during build-addons (Ali Ijaz Sheikh) [#5494](https://github.com/nodejs/node/pull/5494)
* [[`704b68aeb9`](https://github.com/nodejs/node/commit/704b68aeb9)] - **cares**: Support malloc(0) scenarios for AIX (Gireesh Punathil) [#6305](https://github.com/nodejs/node/pull/6305)
* [[`77e2efec8c`](https://github.com/nodejs/node/commit/77e2efec8c)] - **console**: refactor to use rest params and template strings (James M Snell) [#6233](https://github.com/nodejs/node/pull/6233)
* [[`a53b2ac4b1`](https://github.com/nodejs/node/commit/a53b2ac4b1)] - **contextify**: tie lifetimes of context & sandbox (Ali Ijaz Sheikh) [#5786](https://github.com/nodejs/node/pull/5786)
* [[`d6237aa7c6`](https://github.com/nodejs/node/commit/d6237aa7c6)] - **(SEMVER-MINOR)** **crypto**: Read OpenSSL config before init (Stefan Budeanu) [#6374](https://github.com/nodejs/node/pull/6374)
* [[`c1729b0c38`](https://github.com/nodejs/node/commit/c1729b0c38)] - **deps**: upgrade to V8 5.0.71.35 (Ali Ijaz Sheikh) [#6372](https://github.com/nodejs/node/pull/6372)
* [[`7337693c33`](https://github.com/nodejs/node/commit/7337693c33)] - **deps**: upgrade to V8 5.0.71.34 (Ali Ijaz Sheikh) [#6320](https://github.com/nodejs/node/pull/6320)
* [[`809ed52147`](https://github.com/nodejs/node/commit/809ed52147)] - **deps**: upgrade to V8 5.0.71.33 (Ali Ijaz Sheikh) [#6290](https://github.com/nodejs/node/pull/6290)
* [[`19867cedfe`](https://github.com/nodejs/node/commit/19867cedfe)] - **deps**: exclude tests on ppc for v8 5.0 (Michael Dawson) [#6267](https://github.com/nodejs/node/pull/6267)
* [[`450418286a`](https://github.com/nodejs/node/commit/450418286a)] - **deps**: cherry-pick 1ef7487b from v8 upstream (Michael Dawson) [#6218](https://github.com/nodejs/node/pull/6218)
* [[`c3cec1eefc`](https://github.com/nodejs/node/commit/c3cec1eefc)] - **deps**: upgrade libuv to 1.9.0 (Saúl Ibarra Corretgé) [#5994](https://github.com/nodejs/node/pull/5994)
* [[`3a8e8230ee`](https://github.com/nodejs/node/commit/3a8e8230ee)] - **deps**: upgrade to V8 4.9.385.35 (Ben Noordhuis) [#6077](https://github.com/nodejs/node/pull/6077)
* [[`f8e8075a62`](https://github.com/nodejs/node/commit/f8e8075a62)] - **deps**: upgrade to V8 4.9.385.27 (Ali Ijaz Sheikh) [#5494](https://github.com/nodejs/node/pull/5494)
* [[`079973b96a`](https://github.com/nodejs/node/commit/079973b96a)] - **deps**: add V8 DEP trace_event (Ali Ijaz Sheikh) [#4722](https://github.com/nodejs/node/pull/4722)
* [[`89f234300a`](https://github.com/nodejs/node/commit/89f234300a)] - **deps**: edit v8 gitignore to allow trace_event copy (Ali Ijaz Sheikh) [#4722](https://github.com/nodejs/node/pull/4722)
* [[`069e02ab47`](https://github.com/nodejs/node/commit/069e02ab47)] - **deps**: upgrade to V8 4.9.385.18 (Ali Ijaz Sheikh) [#4722](https://github.com/nodejs/node/pull/4722)
* [[`674e5131ed`](https://github.com/nodejs/node/commit/674e5131ed)] - **deps**: backport bc2e393 from v8 upstream (evan.lucas) [#4106](https://github.com/nodejs/node/pull/4106)
* [[`113d1f3821`](https://github.com/nodejs/node/commit/113d1f3821)] - **deps**: cherry-pick 68e89fb from v8's upstream (Fedor Indutny) [#4106](https://github.com/nodejs/node/pull/4106)
* [[`ef4170ea03`](https://github.com/nodejs/node/commit/ef4170ea03)] - **deps**: upgrade to V8 4.8.271.17 (Ali Ijaz Sheikh) [#4785](https://github.com/nodejs/node/pull/4785)
* [[`384b20362c`](https://github.com/nodejs/node/commit/384b20362c)] - **deps**: upgrade to V8 4.7.80.32 (Ali Ijaz Sheikh) [#4699](https://github.com/nodejs/node/pull/4699)
* [[`36ac3d642e`](https://github.com/nodejs/node/commit/36ac3d642e)] - **deps**: backport 8d6a228 from the v8's upstream (Fedor Indutny) [#4259](https://github.com/nodejs/node/pull/4259)
* [[`e2dec98837`](https://github.com/nodejs/node/commit/e2dec98837)] - **deps**: upgrade to V8 4.7.80.25 (Ali Ijaz Sheikh) [#4160](https://github.com/nodejs/node/pull/4160)
* [[`1e324d883e`](https://github.com/nodejs/node/commit/1e324d883e)] - **deps**: backport bc2e393 from v8 upstream (evan.lucas) [#4106](https://github.com/nodejs/node/pull/4106)
* [[`d9d050d396`](https://github.com/nodejs/node/commit/d9d050d396)] - **deps**: cherry-pick 68e89fb from v8's upstream (Fedor Indutny) [#4106](https://github.com/nodejs/node/pull/4106)
* [[`edfc8cde04`](https://github.com/nodejs/node/commit/edfc8cde04)] - **deps**: backport 1ee712a from V8 upstream (Julien Gilli) [#4106](https://github.com/nodejs/node/pull/4106)
* [[`8a43a3d761`](https://github.com/nodejs/node/commit/8a43a3d761)] - **deps**: upgrade V8 to 4.7.80.24 (Ali Ijaz Sheikh) [#4106](https://github.com/nodejs/node/pull/4106)
* [[`61be0004a6`](https://github.com/nodejs/node/commit/61be0004a6)] - **doc**: fix position of `fs.readSync()` (Jeremiah Senkpiel) [#6399](https://github.com/nodejs/node/pull/6399)
* [[`27ce7fe953`](https://github.com/nodejs/node/commit/27ce7fe953)] - **doc**: change references to Stable to Current (Myles Borins) [#6318](https://github.com/nodejs/node/pull/6318)
* [[`547a6fecb9`](https://github.com/nodejs/node/commit/547a6fecb9)] - **doc**: update authors (James M Snell) [#6373](https://github.com/nodejs/node/pull/6373)
* [[`af8dd44d43`](https://github.com/nodejs/node/commit/af8dd44d43)] - **doc**: add JacksonTian to collaborators (Jackson Tian) [#6388](https://github.com/nodejs/node/pull/6388)
* [[`09f7b8a818`](https://github.com/nodejs/node/commit/09f7b8a818)] - **doc**: add Minqi Pan to collaborators (Minqi Pan) [#6387](https://github.com/nodejs/node/pull/6387)
* [[`dfc7ffd24d`](https://github.com/nodejs/node/commit/dfc7ffd24d)] - **doc**: add eljefedelrodeodeljefe to collaborators (Robert Jefe Lindstaedt) [#6389](https://github.com/nodejs/node/pull/6389)
* [[`e45dbd3089`](https://github.com/nodejs/node/commit/e45dbd3089)] - **doc**: add ronkorving to collaborators (ronkorving) [#6385](https://github.com/nodejs/node/pull/6385)
* [[`96f6af7b26`](https://github.com/nodejs/node/commit/96f6af7b26)] - **doc**: add estliberitas to collaborators (Alexander Makarenko) [#6386](https://github.com/nodejs/node/pull/6386)
* [[`8def5fa4e7`](https://github.com/nodejs/node/commit/8def5fa4e7)] - **doc**: fix broken references (Alexander Gromnitsky) [#6350](https://github.com/nodejs/node/pull/6350)
* [[`11d71da558`](https://github.com/nodejs/node/commit/11d71da558)] - **doc**: add note for platform specific flags `fs.open()` (Robert Jefe Lindstaedt) [#6136](https://github.com/nodejs/node/pull/6136)
* [[`e6d4f3befb`](https://github.com/nodejs/node/commit/e6d4f3befb)] - **doc**: improvements to child_process, process docs (Alexander Makarenko)
* [[`7c03595b2a`](https://github.com/nodejs/node/commit/7c03595b2a)] - **doc**: fix a typo in the CONTRIBUTING.md (vsemozhetbyt) [#6343](https://github.com/nodejs/node/pull/6343)
* [[`557c309447`](https://github.com/nodejs/node/commit/557c309447)] - **doc**: add vm example, be able to require modules (Robert Jefe Lindstaedt) [#5323](https://github.com/nodejs/node/pull/5323)
* [[`61a214e957`](https://github.com/nodejs/node/commit/61a214e957)] - **doc**: note that process.config can and will be changed (James M Snell) [#6266](https://github.com/nodejs/node/pull/6266)
* [[`09ec1cc1c7`](https://github.com/nodejs/node/commit/09ec1cc1c7)] - **doc**: path.resolve ignores zero-length strings (Igor Klopov) [#5928](https://github.com/nodejs/node/pull/5928)
* [[`b743d82ffc`](https://github.com/nodejs/node/commit/b743d82ffc)] - **doc**: improve github templates by using comments (Johan Bergström) [#5710](https://github.com/nodejs/node/pull/5710)
* [[`d939152230`](https://github.com/nodejs/node/commit/d939152230)] - **doc**: fix typo in fs writeSync param list (firedfox) [#5984](https://github.com/nodejs/node/pull/5984)
* [[`4039ef1a58`](https://github.com/nodejs/node/commit/4039ef1a58)] - **doc**: remove redundant parameter comments from fs (firedfox) [#5952](https://github.com/nodejs/node/pull/5952)
* [[`9b6440a86c`](https://github.com/nodejs/node/commit/9b6440a86c)] - **doc**: add simple http clientError example (James M Snell) [#5248](https://github.com/nodejs/node/pull/5248)
* [[`de91e9a8a7`](https://github.com/nodejs/node/commit/de91e9a8a7)] - **doc**: fix v4.3.0 changelog commit entries (James M Snell) [#5164](https://github.com/nodejs/node/pull/5164)
* [[`e31bda81dd`](https://github.com/nodejs/node/commit/e31bda81dd)] - **doc**: add CTC meeting minutes 2016-01-20 (Rod Vagg) [#4904](https://github.com/nodejs/node/pull/4904)
* [[`ce11a37c4a`](https://github.com/nodejs/node/commit/ce11a37c4a)] - **doc**: add CTC meeting minutes 2016-01-27 (Rod Vagg) [#5057](https://github.com/nodejs/node/pull/5057)
* [[`3c70dc83ed`](https://github.com/nodejs/node/commit/3c70dc83ed)] - **doc**: add CTC meeting minutes 2016-01-06 (Rod Vagg) [#4667](https://github.com/nodejs/node/pull/4667)
* [[`55326f5488`](https://github.com/nodejs/node/commit/55326f5488)] - **doc**: add CTC meeting minutes 2015-12-16 (Rod Vagg) [#4666](https://github.com/nodejs/node/pull/4666)
* [[`1a346840bf`](https://github.com/nodejs/node/commit/1a346840bf)] - **doc**: add CTC meeting minutes 2015-12-02 (Rod Vagg) [#4665](https://github.com/nodejs/node/pull/4665)
* [[`3ac5f8dcba`](https://github.com/nodejs/node/commit/3ac5f8dcba)] - **doc**: add CTC meeting minutes 2015-11-11 (Rod Vagg) [#4664](https://github.com/nodejs/node/pull/4664)
* [[`db39625a44`](https://github.com/nodejs/node/commit/db39625a44)] - **doc**: add CTC meeting minutes 2015-11-04 (Rod Vagg) [#4663](https://github.com/nodejs/node/pull/4663)
* [[`aa53bf2faa`](https://github.com/nodejs/node/commit/aa53bf2faa)] - **(SEMVER-MINOR)** **events**: add prependListener() and prependOnceListener() (James M Snell) [#6032](https://github.com/nodejs/node/pull/6032)
* [[`c9628c55fa`](https://github.com/nodejs/node/commit/c9628c55fa)] - **events**: make eventNames() use Reflect.ownKeys() (Luigi Pinca) [#5822](https://github.com/nodejs/node/pull/5822)
* [[`b6e3fa745b`](https://github.com/nodejs/node/commit/b6e3fa745b)] - **(SEMVER-MINOR)** **events**: add eventNames() method (James M Snell) [#5617](https://github.com/nodejs/node/pull/5617)
* [[`1d79787e2e`](https://github.com/nodejs/node/commit/1d79787e2e)] - **fs**: add a temporary fix for re-evaluation support (Сковорода Никита Андреевич) [#5102](https://github.com/nodejs/node/pull/5102)
* [[`3441a4178b`](https://github.com/nodejs/node/commit/3441a4178b)] - **fs**: revert "change statSync to accessSync..." (Rich Trott) [#4679](https://github.com/nodejs/node/pull/4679)
* [[`809bf5e38c`](https://github.com/nodejs/node/commit/809bf5e38c)] - **fs**: change statSync to accessSync in realpathSync (Ben Noordhuis) [#4575](https://github.com/nodejs/node/pull/4575)
* [[`4bd8b20f20`](https://github.com/nodejs/node/commit/4bd8b20f20)] - **github**: put description of PR to the end of tmpl (Fedor Indutny) [#5378](https://github.com/nodejs/node/pull/5378)
* [[`81e35b5a29`](https://github.com/nodejs/node/commit/81e35b5a29)] - **github**: add issue and pull request templates (Fedor Indutny) [#5291](https://github.com/nodejs/node/pull/5291)
* [[`7cbd9fe39a`](https://github.com/nodejs/node/commit/7cbd9fe39a)] - **gyp**: inherit parent for `*.host` (Johan Bergström) [#6173](https://github.com/nodejs/node/pull/6173)
* [[`c29b3c1808`](https://github.com/nodejs/node/commit/c29b3c1808)] - **lib**: improve module loading performance (Brian White) [#5172](https://github.com/nodejs/node/pull/5172)
* [[`953173b0d7`](https://github.com/nodejs/node/commit/953173b0d7)] - **lib**: wrap tick_processor scripts in IIFE (Ali Ijaz Sheikh) [#4722](https://github.com/nodejs/node/pull/4722)
* [[`f3d9de772c`](https://github.com/nodejs/node/commit/f3d9de772c)] - **module**: fix resolution of filename with trailing slash (Michaël Zasso) [#6215](https://github.com/nodejs/node/pull/6215)
* [[`5c14efbbe9`](https://github.com/nodejs/node/commit/5c14efbbe9)] - **module**: return correct exports from linked binding (Phillip Kovalev) [#5337](https://github.com/nodejs/node/pull/5337)
* [[`19f700859b`](https://github.com/nodejs/node/commit/19f700859b)] - **module**: revert "optimize js and json file i/o" (Rich Trott) [#4679](https://github.com/nodejs/node/pull/4679)
* [[`7c60328002`](https://github.com/nodejs/node/commit/7c60328002)] - **module**: optimize js and json file i/o (Ben Noordhuis) [#4575](https://github.com/nodejs/node/pull/4575)
* [[`aa395b78fe`](https://github.com/nodejs/node/commit/aa395b78fe)] - **net**: set ADDRCONFIG DNS hint in connections (cjihrig) [#6281](https://github.com/nodejs/node/pull/6281)
* [[`e052096ee0`](https://github.com/nodejs/node/commit/e052096ee0)] - **(SEMVER-MINOR)** **os**: add userInfo() method (cjihrig) [#6104](https://github.com/nodejs/node/pull/6104)
* [[`d18daaa4e7`](https://github.com/nodejs/node/commit/d18daaa4e7)] - **querystring**: fix comments (Brian White) [#6365](https://github.com/nodejs/node/pull/6365)
* [[`633268c6df`](https://github.com/nodejs/node/commit/633268c6df)] - **src**: fix check-imports.py linter errors (Sakthipriyan Vairamani) [#6105](https://github.com/nodejs/node/pull/6105)
* [[`b0b7f6a12b`](https://github.com/nodejs/node/commit/b0b7f6a12b)] - **src**: squelch -Wunused-variable in non-icu builds (Ben Noordhuis) [#6351](https://github.com/nodejs/node/pull/6351)
* [[`b6a41d6120`](https://github.com/nodejs/node/commit/b6a41d6120)] - **src**: fix out-of-bounds write in TwoByteValue (Anna Henningsen) [#6330](https://github.com/nodejs/node/pull/6330)
* [[`86ff8447f3`](https://github.com/nodejs/node/commit/86ff8447f3)] - **src**: add intl and icu configs to process.binding('config') (James M Snell) [#6266](https://github.com/nodejs/node/pull/6266)
* [[`5b7a011e1b`](https://github.com/nodejs/node/commit/5b7a011e1b)] - **src**: add process.binding('config') (James M Snell) [#6266](https://github.com/nodejs/node/pull/6266)
* [[`f286e1e1ad`](https://github.com/nodejs/node/commit/f286e1e1ad)] - **src**: fix -Wunused-result warning in e38bade (Sakthipriyan Vairamani) [#6276](https://github.com/nodejs/node/pull/6276)
* [[`2a4998c5c0`](https://github.com/nodejs/node/commit/2a4998c5c0)] - **src**: don't set non-primitive values on templates (Ben Noordhuis) [#6228](https://github.com/nodejs/node/pull/6228)
* [[`00c876dec5`](https://github.com/nodejs/node/commit/00c876dec5)] - **src**: fix ARRAY_SIZE() logic error (Ben Noordhuis) [#5969](https://github.com/nodejs/node/pull/5969)
* [[`ad0ce57432`](https://github.com/nodejs/node/commit/ad0ce57432)] - **src**: fix NewInstance deprecation warnings (Michaël Zasso) [#4722](https://github.com/nodejs/node/pull/4722)
* [[`67b5a8a936`](https://github.com/nodejs/node/commit/67b5a8a936)] - **src**: replace usage of deprecated ForceSet (Michaël Zasso) [#5159](https://github.com/nodejs/node/pull/5159)
* [[`536cfecd71`](https://github.com/nodejs/node/commit/536cfecd71)] - **src**: replace deprecated SetWeak in object_wrap (Ali Ijaz Sheikh) [#5494](https://github.com/nodejs/node/pull/5494)
* [[`492fbfbff8`](https://github.com/nodejs/node/commit/492fbfbff8)] - **src**: update uses of deprecated NewExternal (Ali Ijaz Sheikh) [#5462](https://github.com/nodejs/node/pull/5462)
* [[`a45e1f9f98`](https://github.com/nodejs/node/commit/a45e1f9f98)] - **src**: fix deprecated SetWeak usage in base-object (Ali Ijaz Sheikh) [#5204](https://github.com/nodejs/node/pull/5204)
* [[`c1649a7e4e`](https://github.com/nodejs/node/commit/c1649a7e4e)] - **src**: replace usage of deprecated SetAccessor (Ali Ijaz Sheikh) [#5204](https://github.com/nodejs/node/pull/5204)
* [[`cddd5347f6`](https://github.com/nodejs/node/commit/cddd5347f6)] - **src**: replace deprecated ProcessDebugMessages (Michaël Zasso) [#5159](https://github.com/nodejs/node/pull/5159)
* [[`d515a3f4b4`](https://github.com/nodejs/node/commit/d515a3f4b4)] - **src**: replace usage of deprecated GetDebugContext (Michaël Zasso) [#5159](https://github.com/nodejs/node/pull/5159)
* [[`4417f99432`](https://github.com/nodejs/node/commit/4417f99432)] - **src**: replace usage of deprecated SetMessageHandler (Michaël Zasso) [#5159](https://github.com/nodejs/node/pull/5159)
* [[`8894f2745f`](https://github.com/nodejs/node/commit/8894f2745f)] - **src**: replace deprecated CancelTerminateExecution (Michaël Zasso) [#5159](https://github.com/nodejs/node/pull/5159)
* [[`f50ef1a46f`](https://github.com/nodejs/node/commit/f50ef1a46f)] - **src**: replace deprecated TerminateExecution (Michaël Zasso) [#5159](https://github.com/nodejs/node/pull/5159)
* [[`ac32340997`](https://github.com/nodejs/node/commit/ac32340997)] - **src**: replace usage of deprecated HasOwnProperty (Michaël Zasso) [#5159](https://github.com/nodejs/node/pull/5159)
* [[`a729208808`](https://github.com/nodejs/node/commit/a729208808)] - **src**: replace usage of deprecated Has (Michaël Zasso) [#5159](https://github.com/nodejs/node/pull/5159)
* [[`3d7fd9aa22`](https://github.com/nodejs/node/commit/3d7fd9aa22)] - **src**: replace usage of deprecated GetEndColumn (Michaël Zasso) [#5159](https://github.com/nodejs/node/pull/5159)
* [[`d45045f318`](https://github.com/nodejs/node/commit/d45045f318)] - **src**: replace usage of deprecated Delete (Michaël Zasso) [#5159](https://github.com/nodejs/node/pull/5159)
* [[`16b0a8c1ac`](https://github.com/nodejs/node/commit/16b0a8c1ac)] - **src**: replace usage of deprecated CompileUnbound (Michaël Zasso) [#5159](https://github.com/nodejs/node/pull/5159)
* [[`023c317173`](https://github.com/nodejs/node/commit/023c317173)] - **src**: replace usage of deprecated Compile (Michaël Zasso) [#5159](https://github.com/nodejs/node/pull/5159)
* [[`79d7475604`](https://github.com/nodejs/node/commit/79d7475604)] - **src**: fix TryCatch deprecation warnings (Michaël Zasso) [#4722](https://github.com/nodejs/node/pull/4722)
* [[`81f1732a05`](https://github.com/nodejs/node/commit/81f1732a05)] - **src**: replace deprecated String::NewFromOneByte (Michaël Zasso) [#4722](https://github.com/nodejs/node/pull/4722)
* [[`924cc6c633`](https://github.com/nodejs/node/commit/924cc6c633)] - **src**: upgrade to new v8::Private api (Ben Noordhuis) [#5045](https://github.com/nodejs/node/pull/5045)
* [[`79dc1d7635`](https://github.com/nodejs/node/commit/79dc1d7635)] - **src**: remove forwards for v8::GC*logueCallback (Ali Ijaz Sheikh) [#4381](https://github.com/nodejs/node/pull/4381)
* [[`89abe86808`](https://github.com/nodejs/node/commit/89abe86808)] - ***Revert*** "**stream**: emit 'pause' on nextTick" (Evan Lucas) [#5947](https://github.com/nodejs/node/pull/5947)
* [[`ace1009456`](https://github.com/nodejs/node/commit/ace1009456)] - **stream**: emit 'pause' on nextTick (Alexis Campailla) [#5776](https://github.com/nodejs/node/pull/5776)
* [[`eee9dc7e9d`](https://github.com/nodejs/node/commit/eee9dc7e9d)] - ***Revert*** "**stream**: add bytesRead property for readable" (cjihrig) [#4746](https://github.com/nodejs/node/pull/4746)
* [[`bfb2cd0bfd`](https://github.com/nodejs/node/commit/bfb2cd0bfd)] - **stream**: add bytesRead property for readable (Jackson Tian) [#4372](https://github.com/nodejs/node/pull/4372)
* [[`0ed85e35a9`](https://github.com/nodejs/node/commit/0ed85e35a9)] - **test**: increase the platform timeout for AIX (Michael Dawson) [#6342](https://github.com/nodejs/node/pull/6342)
* [[`591ebe7d82`](https://github.com/nodejs/node/commit/591ebe7d82)] - **test**: add tests for console.assert (Evan Lucas) [#6302](https://github.com/nodejs/node/pull/6302)
* [[`a9bab5fddb`](https://github.com/nodejs/node/commit/a9bab5fddb)] - **test**: v8-flags is sensitive to script caching (Ali Ijaz Sheikh) [#6316](https://github.com/nodejs/node/pull/6316)
* [[`c83204e2d9`](https://github.com/nodejs/node/commit/c83204e2d9)] - **test**: don't assume IPv6 in test-regress-GH-5727 (cjihrig) [#6319](https://github.com/nodejs/node/pull/6319)
* [[`6e57d2d7c4`](https://github.com/nodejs/node/commit/6e57d2d7c4)] - **test**: spawn new processes in vm-cached-data (Fedor Indutny) [#6280](https://github.com/nodejs/node/pull/6280)
* [[`3ebabc95aa`](https://github.com/nodejs/node/commit/3ebabc95aa)] - **test**: update error message for JSON.parse (Michaël Zasso) [#5945](https://github.com/nodejs/node/pull/5945)
* [[`b73e1b3c5a`](https://github.com/nodejs/node/commit/b73e1b3c5a)] - **test**: fix another flaky stringbytes test (Ali Ijaz Sheikh) [#6073](https://github.com/nodejs/node/pull/6073)
* [[`f4ebd5989a`](https://github.com/nodejs/node/commit/f4ebd5989a)] - **test**: fix flakiness of stringbytes-external (Ali Ijaz Sheikh) [#6039](https://github.com/nodejs/node/pull/6039)
* [[`be97db92af`](https://github.com/nodejs/node/commit/be97db92af)] - **test**: add buffer alignment regression tests (Anna Henningsen) [#5752](https://github.com/nodejs/node/pull/5752)
* [[`01f82f0685`](https://github.com/nodejs/node/commit/01f82f0685)] - **test**: fix proxy tab-completion test (Ali Ijaz Sheikh) [#4722](https://github.com/nodejs/node/pull/4722)
* [[`9968941797`](https://github.com/nodejs/node/commit/9968941797)] - **test**: fix tests after V8 upgrade (Michaël Zasso) [#4722](https://github.com/nodejs/node/pull/4722)
* [[`db9e122182`](https://github.com/nodejs/node/commit/db9e122182)] - **test**: update ArrayBuffer alloc failure message (Ali Ijaz Sheikh) [#4785](https://github.com/nodejs/node/pull/4785)
* [[`dc09bbe3ee`](https://github.com/nodejs/node/commit/dc09bbe3ee)] - **test**: fix test-repl-tab-complete after V8 upgrade (Ali Ijaz Sheikh) [#4106](https://github.com/nodejs/node/pull/4106)
* [[`3a0eef7b99`](https://github.com/nodejs/node/commit/3a0eef7b99)] - **test,benchmark**: use deepStrictEqual() (Rich Trott) [#6213](https://github.com/nodejs/node/pull/6213)
* [[`e928edad65`](https://github.com/nodejs/node/commit/e928edad65)] - **tools**: rewrite check-install.sh in python (Sakthipriyan Vairamani) [#6105](https://github.com/nodejs/node/pull/6105)
* [[`96978768c9`](https://github.com/nodejs/node/commit/96978768c9)] - **tools**: enforce deepStrictEqual over deepEqual (Rich Trott) [#6213](https://github.com/nodejs/node/pull/6213)
* [[`9e30129fa7`](https://github.com/nodejs/node/commit/9e30129fa7)] - **(SEMVER-MINOR)** **tools**: add buffer-constructor eslint rule (James M Snell) [#5740](https://github.com/nodejs/node/pull/5740)
* [[`b6475b9a9d`](https://github.com/nodejs/node/commit/b6475b9a9d)] - ***Revert*** "**tty**: don't read from console stream upon creation" (Evan Lucas) [#5947](https://github.com/nodejs/node/pull/5947)
* [[`4611389294`](https://github.com/nodejs/node/commit/4611389294)] - **tty**: don't read from console stream upon creation (Alexis Campailla) [#5776](https://github.com/nodejs/node/pull/5776)
* [[`59737b29c7`](https://github.com/nodejs/node/commit/59737b29c7)] - **url**: use "empty" object for empty query strings (Brian White) [#6289](https://github.com/nodejs/node/pull/6289)
* [[`0b43c08f44`](https://github.com/nodejs/node/commit/0b43c08f44)] - **util**: pass on additional error() args (Brian White) [#4279](https://github.com/nodejs/node/pull/4279)
* [[`967a15541b`](https://github.com/nodejs/node/commit/967a15541b)] - **v8**: warn in Template::Set() on improper use (Ben Noordhuis) [#6277](https://github.com/nodejs/node/pull/6277)
