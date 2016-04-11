# Node.js ChangeLog

## 2016-04-12, Version 4.4.3 'Argon' (LTS), @thealphanerd

### Notable Changes

* **deps**:
  - Fix `--gdbjit` for embedders. Backported from v8 upstream. (Ben Noordhuis) [#5577](https://github.com/nodejs/node/pull/5577)
* **etw**:
  - Correctly display descriptors for ETW events 9 and 23 on the windows platform. (João Reis) [#5742](https://github.com/nodejs/node/pull/5742)
* **querystring**:
  - Restore throw when attempting to stringify bad surrogate pair. (Brian White) [#5858](https://github.com/nodejs/node/pull/5858)

### Commits

* [[`f949c273cd`](https://github.com/nodejs/node/commit/f949c273cd)] - **assert**: Check typed array view type in deepEqual (Anna Henningsen) [#5910](https://github.com/nodejs/node/pull/5910)
* [[`132acea0d4`](https://github.com/nodejs/node/commit/132acea0d4)] - **build**: introduce ci targets for lint/benchmark (Johan Bergström) [#5921](https://github.com/nodejs/node/pull/5921)
* [[`9a8f922dee`](https://github.com/nodejs/node/commit/9a8f922dee)] - **build**: add missing `openssl_fips%` to common.gypi (Fedor Indutny) [#5919](https://github.com/nodejs/node/pull/5919)
* [[`d275cdf202`](https://github.com/nodejs/node/commit/d275cdf202)] - **child_process**: refactor self=this in socket_list (Benjamin Gruenbaum) [#5860](https://github.com/nodejs/node/pull/5860)
* [[`aadf356aa2`](https://github.com/nodejs/node/commit/aadf356aa2)] - **deps**: backport 8d00c2c from v8 upstream (Ben Noordhuis) [#5577](https://github.com/nodejs/node/pull/5577)
* [[`200f763c43`](https://github.com/nodejs/node/commit/200f763c43)] - **deps**: completely upgrade npm in LTS to 2.15.1 (Forrest L Norvell) [#5989](https://github.com/nodejs/node/pull/5989)
* [[`86e3903626`](https://github.com/nodejs/node/commit/86e3903626)] - **dns**: Use object without protoype for map (Benjamin Gruenbaum) [#5843](https://github.com/nodejs/node/pull/5843)
* [[`9a33f43f73`](https://github.com/nodejs/node/commit/9a33f43f73)] - **doc**: update openssl LICENSE using license-builder.sh (Steven R. Loomis) [#6065](https://github.com/nodejs/node/pull/6065)
* [[`9679e2dc70`](https://github.com/nodejs/node/commit/9679e2dc70)] - **doc**: clarify that __dirname is module local (James M Snell) [#6018](https://github.com/nodejs/node/pull/6018)
* [[`86d2af58d6`](https://github.com/nodejs/node/commit/86d2af58d6)] - **doc**: simple doc typo fix (Brendon Pierson) [#6041](https://github.com/nodejs/node/pull/6041)
* [[`f16802f3ca`](https://github.com/nodejs/node/commit/f16802f3ca)] - **doc**: note about Android support (Rich Trott) [#6040](https://github.com/nodejs/node/pull/6040)
* [[`8c2befe176`](https://github.com/nodejs/node/commit/8c2befe176)] - **doc**: note assert.throws() pitfall (Rich Trott) [#6029](https://github.com/nodejs/node/pull/6029)
* [[`0870ac65f2`](https://github.com/nodejs/node/commit/0870ac65f2)] - **doc**: use HTTPS for links where possible (Rich Trott) [#6019](https://github.com/nodejs/node/pull/6019)
* [[`56755de96e`](https://github.com/nodejs/node/commit/56755de96e)] - **doc**: clarify stdout/stderr arguments to callback (James M Snell) [#6015](https://github.com/nodejs/node/pull/6015)
* [[`bb603b89a2`](https://github.com/nodejs/node/commit/bb603b89a2)] - **doc**: add 'Command Line Options' to 'View on single page' (firedfox) [#6011](https://github.com/nodejs/node/pull/6011)
* [[`c91f3d897a`](https://github.com/nodejs/node/commit/c91f3d897a)] - **doc**: add copy about how to curl SHA256.txt (Myles Borins) [#6120](https://github.com/nodejs/node/pull/6120)
* [[`f9cf232284`](https://github.com/nodejs/node/commit/f9cf232284)] - **doc**: add example using algorithms not directly exposed (Brad Hill) [#6108](https://github.com/nodejs/node/pull/6108)
* [[`f60ce1078d`](https://github.com/nodejs/node/commit/f60ce1078d)] - **doc**: document unspecified behavior for buf.write* methods (James M Snell) [#5925](https://github.com/nodejs/node/pull/5925)
* [[`02401a6cbd`](https://github.com/nodejs/node/commit/02401a6cbd)] - **doc**: fix scrolling on iOS devices (Luigi Pinca) [#5878](https://github.com/nodejs/node/pull/5878)
* [[`aed22d0855`](https://github.com/nodejs/node/commit/aed22d0855)] - **doc**: path.format provide more examples (John Eversole) [#5838](https://github.com/nodejs/node/pull/5838)
* [[`6e2bfbe1fd`](https://github.com/nodejs/node/commit/6e2bfbe1fd)] - **doc**: fix doc for Buffer.readInt32LE() (ghaiklor) [#5890](https://github.com/nodejs/node/pull/5890)
* [[`940d204401`](https://github.com/nodejs/node/commit/940d204401)] - **doc**: consolidate timers docs in timers.markdown (Bryan English) [#5837](https://github.com/nodejs/node/pull/5837)
* [[`505faf6360`](https://github.com/nodejs/node/commit/505faf6360)] - **doc**: refine child_process detach behaviour (Robert Jefe Lindstaedt) [#5330](https://github.com/nodejs/node/pull/5330)
* [[`feedca7879`](https://github.com/nodejs/node/commit/feedca7879)] - **doc**: add topic - event loop, timers, `nextTick()` (Jeff Harris) [#4936](https://github.com/nodejs/node/pull/4936)
* [[`6d3822c12b`](https://github.com/nodejs/node/commit/6d3822c12b)] - **etw**: fix descriptors of events 9 and 23 (João Reis) [#5742](https://github.com/nodejs/node/pull/5742)
* [[`56dda6f336`](https://github.com/nodejs/node/commit/56dda6f336)] - **fs**: Remove unused branches (Benjamin Gruenbaum) [#5289](https://github.com/nodejs/node/pull/5289)
* [[`dfe9e157c1`](https://github.com/nodejs/node/commit/dfe9e157c1)] - **governance**: remove target size for CTC (Rich Trott) [#5879](https://github.com/nodejs/node/pull/5879)
* [[`c4103b154f`](https://github.com/nodejs/node/commit/c4103b154f)] - **lib**: refactor code with startsWith/endsWith (Jackson Tian) [#5753](https://github.com/nodejs/node/pull/5753)
* [[`16216a81de`](https://github.com/nodejs/node/commit/16216a81de)] - **meta**: add "joining a wg" section to WORKING_GROUPS.md (Matteo Collina) [#5488](https://github.com/nodejs/node/pull/5488)
* [[`65fc4e36ce`](https://github.com/nodejs/node/commit/65fc4e36ce)] - **querystring**: don't stringify bad surrogate pair (Brian White) [#5858](https://github.com/nodejs/node/pull/5858)
* [[`4f683ab912`](https://github.com/nodejs/node/commit/4f683ab912)] - **src,tools**: use template literals (Rich Trott) [#5778](https://github.com/nodejs/node/pull/5778)
* [[`ac40a4510d`](https://github.com/nodejs/node/commit/ac40a4510d)] - **test**: explicitly set global in test-repl (Rich Trott) [#6026](https://github.com/nodejs/node/pull/6026)
* [[`a7b3a7533a`](https://github.com/nodejs/node/commit/a7b3a7533a)] - **test**: be explicit about polluting of `global` (Rich Trott) [#6017](https://github.com/nodejs/node/pull/6017)
* [[`73e3b7b9a8`](https://github.com/nodejs/node/commit/73e3b7b9a8)] - **test**: make use of globals explicit (Rich Trott) [#6014](https://github.com/nodejs/node/pull/6014)
* [[`e7877e61b6`](https://github.com/nodejs/node/commit/e7877e61b6)] - **test**: fix flaky test-net-socket-timeout-unref (Rich Trott) [#6003](https://github.com/nodejs/node/pull/6003)
* [[`a39051f5b3`](https://github.com/nodejs/node/commit/a39051f5b3)] - **test**: make arch available in status files (Santiago Gimeno) [#5997](https://github.com/nodejs/node/pull/5997)
* [[`ccf90b651a`](https://github.com/nodejs/node/commit/ccf90b651a)] - **test**: fix test-dns.js flakiness (Rich Trott) [#5996](https://github.com/nodejs/node/pull/5996)
* [[`1994ac0912`](https://github.com/nodejs/node/commit/1994ac0912)] - **test**: add test for piping large input from stdin (Anna Henningsen) [#5949](https://github.com/nodejs/node/pull/5949)
* [[`cc1aab9f6a`](https://github.com/nodejs/node/commit/cc1aab9f6a)] - **test**: mitigate flaky test-https-agent (Rich Trott) [#5939](https://github.com/nodejs/node/pull/5939)
* [[`10fe79b809`](https://github.com/nodejs/node/commit/10fe79b809)] - **test**: fix offending max-len linter error (Sakthipriyan Vairamani) [#5980](https://github.com/nodejs/node/pull/5980)
* [[`63d82960fd`](https://github.com/nodejs/node/commit/63d82960fd)] - **test**: stdin is not always a net.Socket (Jeremiah Senkpiel) [#5935](https://github.com/nodejs/node/pull/5935)
* [[`fe0233b923`](https://github.com/nodejs/node/commit/fe0233b923)] - **test**: add known_issues test for GH-2148 (Rich Trott) [#5920](https://github.com/nodejs/node/pull/5920)
* [[`d59be4d248`](https://github.com/nodejs/node/commit/d59be4d248)] - **test**: ensure _handle property existence (Rich Trott) [#5916](https://github.com/nodejs/node/pull/5916)
* [[`9702153107`](https://github.com/nodejs/node/commit/9702153107)] - **test**: fix flaky test-repl (Brian White) [#5914](https://github.com/nodejs/node/pull/5914)
* [[`a0a2e69097`](https://github.com/nodejs/node/commit/a0a2e69097)] - **test**: move dns test to test/internet (Ben Noordhuis) [#5905](https://github.com/nodejs/node/pull/5905)
* [[`8462d8f465`](https://github.com/nodejs/node/commit/8462d8f465)] - **test**: fix flaky test-net-socket-timeout (Brian White) [#5902](https://github.com/nodejs/node/pull/5902)
* [[`e0b283af73`](https://github.com/nodejs/node/commit/e0b283af73)] - **test**: fix flaky test-http-set-timeout (Rich Trott) [#5856](https://github.com/nodejs/node/pull/5856)
* [[`5853fec36f`](https://github.com/nodejs/node/commit/5853fec36f)] - **test**: fix test-debugger-client.js (Rich Trott) [#5851](https://github.com/nodejs/node/pull/5851)
* [[`ea83c382f9`](https://github.com/nodejs/node/commit/ea83c382f9)] - **test**: ensure win32.isAbsolute() is consistent (Brian White) [#6043](https://github.com/nodejs/node/pull/6043)
* [[`c33a23fd1e`](https://github.com/nodejs/node/commit/c33a23fd1e)] - **tools**: fix json doc generation (firedfox) [#5943](https://github.com/nodejs/node/pull/5943)
* [[`6f0bd64122`](https://github.com/nodejs/node/commit/6f0bd64122)] - **tools,doc**: fix incomplete json produced by doctool (firedfox) [#5966](https://github.com/nodejs/node/pull/5966)
* [[`f7eb48302c`](https://github.com/nodejs/node/commit/f7eb48302c)] - **win,build**: build and test add-ons on test-ci (Bogdan Lobor) [#5886](https://github.com/nodejs/node/pull/5886)

## 2016-03-31, Version 4.4.2 'Argon' (LTS), @thealphanerd

### Notable Changes

* **https**:
  - Under certain conditions ssl sockets may have been causing a memory leak when keepalive is enabled. This is no longer the case. (Alexander Penev) [#5713](https://github.com/nodejs/node/pull/5713)
* **lib**:
  - The way that we were internally passing arguments was causing a potential leak. By copying the arguments into an array we can avoid this. (Nathan Woltman) [#4361](https://github.com/nodejs/node/pull/4361)
* **npm**: 
  - Upgrade to v2.15.1. Fixes a security flaw in the use of authentication tokens in HTTP requests that would allow an attacker to set up a server that could collect tokens from users of the command-line interface. Authentication tokens have previously been sent with every request made by the CLI for logged-in users, regardless of the destination of the request. This update fixes this by only including those tokens for requests made against the registry or registries used for the current install. (Forrest L Norvell)
* **repl**:
  - Previously if you were using the repl in strict mode the column number would be wrong in a stack trace. This is no longer an issue. (Prince J Wesley) [#5416](https://github.com/nodejs/node/pull/5416)

### Commits

* [[`96e163a79f`](https://github.com/nodejs/node/commit/96e163a79f)] - **buffer**: changing let in for loops back to var (Gareth Ellis) [#5819](https://github.com/nodejs/node/pull/5819)
* [[`0c6f6742f2`](https://github.com/nodejs/node/commit/0c6f6742f2)] - **console**: check that stderr is writable (Rich Trott) [#5635](https://github.com/nodejs/node/pull/5635)
* [[`55c3f804c4`](https://github.com/nodejs/node/commit/55c3f804c4)] - **deps**: upgrade npm in LTS to 2.15.1 (Forrest L Norvell)
* [[`1d0e4a987d`](https://github.com/nodejs/node/commit/1d0e4a987d)] - **deps**: remove unused openssl files (Ben Noordhuis) [#5619](https://github.com/nodejs/node/pull/5619)
* [[`d55599f4d8`](https://github.com/nodejs/node/commit/d55599f4d8)] - **dns**: use template literals (Benjamin Gruenbaum) [#5809](https://github.com/nodejs/node/pull/5809)
* [[`42bbdc9dd1`](https://github.com/nodejs/node/commit/42bbdc9dd1)] - **doc** Add @mhdawson back to the CTC (James M Snell) [#5633](https://github.com/nodejs/node/pull/5633)
* [[`8d86d232e7`](https://github.com/nodejs/node/commit/8d86d232e7)] - **doc**: typo: interal->internal. (Corey Kosak) [#5849](https://github.com/nodejs/node/pull/5849)
* [[`60ddab841e`](https://github.com/nodejs/node/commit/60ddab841e)] - **doc**: add instructions to only sign a release (Jeremiah Senkpiel) [#5876](https://github.com/nodejs/node/pull/5876)
* [[`040263e0f3`](https://github.com/nodejs/node/commit/040263e0f3)] - **doc**: grammar, clarity and links in timers doc (Bryan English) [#5792](https://github.com/nodejs/node/pull/5792)
* [[`8c24bd25a6`](https://github.com/nodejs/node/commit/8c24bd25a6)] - **doc**: fix order of end tags of list after heading (firedfox) [#5874](https://github.com/nodejs/node/pull/5874)
* [[`7c837028da`](https://github.com/nodejs/node/commit/7c837028da)] - **doc**: use consistent event name parameter (Benjamin Gruenbaum) [#5850](https://github.com/nodejs/node/pull/5850)
* [[`20faf9097d`](https://github.com/nodejs/node/commit/20faf9097d)] - **doc**: explain error message on missing main file (Wolfgang Steiner) [#5812](https://github.com/nodejs/node/pull/5812)
* [[`79d26ae196`](https://github.com/nodejs/node/commit/79d26ae196)] - **doc**: explain path.format expected properties (John Eversole) [#5801](https://github.com/nodejs/node/pull/5801)
* [[`e43e8e3a31`](https://github.com/nodejs/node/commit/e43e8e3a31)] - **doc**: add a cli options doc page (Jeremiah Senkpiel) [#5787](https://github.com/nodejs/node/pull/5787)
* [[`c0a24e4a1d`](https://github.com/nodejs/node/commit/c0a24e4a1d)] - **doc**: fix multiline return comments in querystring (Claudio Rodriguez) [#5705](https://github.com/nodejs/node/pull/5705)
* [[`bf1fe4693c`](https://github.com/nodejs/node/commit/bf1fe4693c)] - **doc**: Add windows example for Path.format (Mithun Patel) [#5700](https://github.com/nodejs/node/pull/5700)
* [[`3b8fc4fddc`](https://github.com/nodejs/node/commit/3b8fc4fddc)] - **doc**: update crypto docs to use good defaults (Bill Automata) [#5505](https://github.com/nodejs/node/pull/5505)
* [[`a6ec8a6cb7`](https://github.com/nodejs/node/commit/a6ec8a6cb7)] - **doc**: fix crypto update() signatures (Brian White) [#5500](https://github.com/nodejs/node/pull/5500)
* [[`eb0ed46665`](https://github.com/nodejs/node/commit/eb0ed46665)] - **doc**: reformat & improve node.1 manual page (Jeremiah Senkpiel) [#5497](https://github.com/nodejs/node/pull/5497)
* [[`b70ca4a4b4`](https://github.com/nodejs/node/commit/b70ca4a4b4)] - **doc**: updated fs #5862 removed irrelevant data in fs.markdown (topal) [#5877](https://github.com/nodejs/node/pull/5877)
* [[`81876612f7`](https://github.com/nodejs/node/commit/81876612f7)] - **https**: fix ssl socket leak when keepalive is used (Alexander Penev) [#5713](https://github.com/nodejs/node/pull/5713)
* [[`6daebdbd9b`](https://github.com/nodejs/node/commit/6daebdbd9b)] - **lib**: simplify code with String.prototype.repeat() (Jackson Tian) [#5359](https://github.com/nodejs/node/pull/5359)
* [[`108fc90dd7`](https://github.com/nodejs/node/commit/108fc90dd7)] - **lib**: reduce usage of `self = this` (Jackson Tian) [#5231](https://github.com/nodejs/node/pull/5231)
* [[`3c8e59c396`](https://github.com/nodejs/node/commit/3c8e59c396)] - **lib**: copy arguments object instead of leaking it (Nathan Woltman) [#4361](https://github.com/nodejs/node/pull/4361)
* [[`8648420586`](https://github.com/nodejs/node/commit/8648420586)] - **net**: make `isIPv4` and `isIPv6` more efficient (Vladimir Kurchatkin) [#5478](https://github.com/nodejs/node/pull/5478)
* [[`07b7172d76`](https://github.com/nodejs/node/commit/07b7172d76)] - **net**: remove unused `var self = this` from old code (Benjamin Gruenbaum) [#5224](https://github.com/nodejs/node/pull/5224)
* [[`acbce4b72b`](https://github.com/nodejs/node/commit/acbce4b72b)] - **repl**: fix stack trace column number in strict mode (Prince J Wesley) [#5416](https://github.com/nodejs/node/pull/5416)
* [[`0a1eb168e0`](https://github.com/nodejs/node/commit/0a1eb168e0)] - **test**: fix `test-cluster-worker-kill` (Santiago Gimeno) [#5814](https://github.com/nodejs/node/pull/5814)
* [[`86b876fe7b`](https://github.com/nodejs/node/commit/86b876fe7b)] - **test**: smaller chunk size for smaller person.jpg (Jérémy Lal) [#5813](https://github.com/nodejs/node/pull/5813)
* [[`1135ee97e7`](https://github.com/nodejs/node/commit/1135ee97e7)] - **test**: strip non-free icc profile from person.jpg (Jérémy Lal) [#5813](https://github.com/nodejs/node/pull/5813)
* [[`0836d7e2fb`](https://github.com/nodejs/node/commit/0836d7e2fb)] - **test**: fix flaky test-cluster-shared-leak (Claudio Rodriguez) [#5802](https://github.com/nodejs/node/pull/5802)
* [[`e57355c2f4`](https://github.com/nodejs/node/commit/e57355c2f4)] - **test**: make test-net-connect-options-ipv6.js better (Michael Dawson) [#5791](https://github.com/nodejs/node/pull/5791)
* [[`1b266fc15c`](https://github.com/nodejs/node/commit/1b266fc15c)] - **test**: remove the use of curl in the test suite (Santiago Gimeno) [#5750](https://github.com/nodejs/node/pull/5750)
* [[`7e45d4f076`](https://github.com/nodejs/node/commit/7e45d4f076)] - **test**: minimize test-http-get-pipeline-problem (Rich Trott) [#5728](https://github.com/nodejs/node/pull/5728)
* [[`78effc3484`](https://github.com/nodejs/node/commit/78effc3484)] - **test**: add batch of known issue tests (cjihrig) [#5653](https://github.com/nodejs/node/pull/5653)
* [[`d506eea4b7`](https://github.com/nodejs/node/commit/d506eea4b7)] - **test**: improve test-npm-install (Santiago Gimeno) [#5613](https://github.com/nodejs/node/pull/5613)
* [[`7520100e8b`](https://github.com/nodejs/node/commit/7520100e8b)] - **test**: add test-npm-install to parallel tests suite (Myles Borins) [#5166](https://github.com/nodejs/node/pull/5166)
* [[`b258dddb8c`](https://github.com/nodejs/node/commit/b258dddb8c)] - **test**: repl tab completion test (Santiago Gimeno) [#5534](https://github.com/nodejs/node/pull/5534)
* [[`f209effe8b`](https://github.com/nodejs/node/commit/f209effe8b)] - **test**: remove timer from test-http-1.0 (Santiago Gimeno) [#5129](https://github.com/nodejs/node/pull/5129)
* [[`3a901b0e3e`](https://github.com/nodejs/node/commit/3a901b0e3e)] - **tools**: remove unused imports (Sakthipriyan Vairamani) [#5765](https://github.com/nodejs/node/pull/5765)

## 2016-03-22, Version 4.4.1 'Argon' (LTS), @thealphanerd

This LTS release comes with 113 commits, 56 of which are doc related,
18 of which are build / tooling related, 16 of which are test related
and 7 which are benchmark related.

### Notable Changes
* **build**:
  - Updated Logos for the OSX + Windows installers
    - (Rod Vagg) [#5401](https://github.com/nodejs/node/pull/5401)
    - (Robert Jefe Lindstaedt) [#5531](https://github.com/nodejs/node/pull/5531)
  - New option to select your VS Version in the Windows installer
    - (julien.waechter) [#4645](https://github.com/nodejs/node/pull/4645)
  - Support Visual C++ Build Tools 2015
    - (João Reis) [#5627](https://github.com/nodejs/node/pull/5627)
* **tools**:
  - Gyp now works on OSX without XCode
    - (Shigeki Ohtsu) [nodejs/node#1325](https://github.com/nodejs/node/pull/1325)

### Commits

* [[`df283f8a03`](https://github.com/nodejs/node/commit/df283f8a03)] - **benchmark**: fix linting issues (Rich Trott) [#5773](https://github.com/nodejs/node/pull/5773)
* [[`c901741c60`](https://github.com/nodejs/node/commit/c901741c60)] - **benchmark**: use strict mode (Rich Trott) [#5773](https://github.com/nodejs/node/pull/5773)
* [[`4be2065dbc`](https://github.com/nodejs/node/commit/4be2065dbc)] - **benchmark**: refactor to eliminate redeclared vars (Rich Trott) [#5773](https://github.com/nodejs/node/pull/5773)
* [[`ddac368533`](https://github.com/nodejs/node/commit/ddac368533)] - **benchmark**: fix lint errors (Rich Trott) [#5773](https://github.com/nodejs/node/pull/5773)
* [[`03b20a73b9`](https://github.com/nodejs/node/commit/03b20a73b9)] - **benchmark**: add benchmark for buf.compare() (Rich Trott) [#5441](https://github.com/nodejs/node/pull/5441)
* [[`b816044845`](https://github.com/nodejs/node/commit/b816044845)] - **buffer**: remove duplicated code in fromObject (HUANG Wei) [#4948](https://github.com/nodejs/node/pull/4948)
* [[`067ce9b905`](https://github.com/nodejs/node/commit/067ce9b905)] - **build**: don't install github templates (Johan Bergström) [#5612](https://github.com/nodejs/node/pull/5612)
* [[`a1772dc515`](https://github.com/nodejs/node/commit/a1772dc515)] - **build**: update Node.js logo on OSX installer (Rod Vagg) [#5401](https://github.com/nodejs/node/pull/5401)
* [[`9058fc0383`](https://github.com/nodejs/node/commit/9058fc0383)] - **build**: correctly detect clang version (Stefan Budeanu) [#5553](https://github.com/nodejs/node/pull/5553)
* [[`1165ecc6f7`](https://github.com/nodejs/node/commit/1165ecc6f7)] - **build**: update Node.js logo on Win installer (Robert Jefe Lindstaedt) [#5531](https://github.com/nodejs/node/pull/5531)
* [[`4990ddad72`](https://github.com/nodejs/node/commit/4990ddad72)] - **build**: remove --quiet from eslint invocation (firedfox) [#5519](https://github.com/nodejs/node/pull/5519)
* [[`46a5d519dd`](https://github.com/nodejs/node/commit/46a5d519dd)] - **build**: skip msi build if WiX is not found (Tsarevich Dmitry) [#5220](https://github.com/nodejs/node/pull/5220)
* [[`dac4e64491`](https://github.com/nodejs/node/commit/dac4e64491)] - **build**: add option to select VS version (julien.waechter) [#4645](https://github.com/nodejs/node/pull/4645)
* [[`7a10fd3a56`](https://github.com/nodejs/node/commit/7a10fd3a56)] - **collaborator_guide**: clarify commit message rules (Wyatt Preul) [#5661](https://github.com/nodejs/node/pull/5661)
* [[`97e95d04c2`](https://github.com/nodejs/node/commit/97e95d04c2)] - **crypto**: PBKDF2 works with `int` not `ssize_t` (Fedor Indutny) [#5397](https://github.com/nodejs/node/pull/5397)
* [[`57b02e6a3e`](https://github.com/nodejs/node/commit/57b02e6a3e)] - **debugger**: remove unneeded callback check (Rich Trott) [#5319](https://github.com/nodejs/node/pull/5319)
* [[`19ae308867`](https://github.com/nodejs/node/commit/19ae308867)] - **deps**: update openssl config (Shigeki Ohtsu) [#5630](https://github.com/nodejs/node/pull/5630)
* [[`d7b81b5bc7`](https://github.com/nodejs/node/commit/d7b81b5bc7)] - **deps**: cherry-pick 2e4da65 from v8's 4.8 upstream (Michael Dawson) [#5293](https://github.com/nodejs/node/pull/5293)
* [[`1e05f371d6`](https://github.com/nodejs/node/commit/1e05f371d6)] - **doc**: fix typo in synchronous randomBytes example (Andrea Giammarchi) [#5781](https://github.com/nodejs/node/pull/5781)
* [[`5f54bd2088`](https://github.com/nodejs/node/commit/5f54bd2088)] - **doc**: topic blocking vs non-blocking (Jarrett Widman) [#5326](https://github.com/nodejs/node/pull/5326)
* [[`0943001563`](https://github.com/nodejs/node/commit/0943001563)] - **doc**: fix invalid path doc comments (Rich Trott) [#5797](https://github.com/nodejs/node/pull/5797)
* [[`bb423bb1e6`](https://github.com/nodejs/node/commit/bb423bb1e6)] - **doc**: update release tweet template (Jeremiah Senkpiel) [#5628](https://github.com/nodejs/node/pull/5628)
* [[`1e877f10aa`](https://github.com/nodejs/node/commit/1e877f10aa)] - **doc**: fix typo in child_process docs (Benjamin Gruenbaum) [#5681](https://github.com/nodejs/node/pull/5681)
* [[`d53dcc599b`](https://github.com/nodejs/node/commit/d53dcc599b)] - **doc**: update fansworld-claudio username on README (Claudio Rodriguez) [#5680](https://github.com/nodejs/node/pull/5680)
* [[`4332f8011e`](https://github.com/nodejs/node/commit/4332f8011e)] - **doc**: fix return value of write methods (Felix Böhm) [#5736](https://github.com/nodejs/node/pull/5736)
* [[`e572542de5`](https://github.com/nodejs/node/commit/e572542de5)] - **doc**: Add note about use of JSON.stringify() (Mithun Patel) [#5723](https://github.com/nodejs/node/pull/5723)
* [[`daf3ef66ef`](https://github.com/nodejs/node/commit/daf3ef66ef)] - **doc**: explain path.format() algorithm (Rich Trott) [#5688](https://github.com/nodejs/node/pull/5688)
* [[`f6d4982aa0`](https://github.com/nodejs/node/commit/f6d4982aa0)] - **doc**: clarify type of first argument in zlib (Kirill Fomichev) [#5685](https://github.com/nodejs/node/pull/5685)
* [[`07e71b2d44`](https://github.com/nodejs/node/commit/07e71b2d44)] - **doc**: fix typo in api/addons (Daijiro Wachi) [#5678](https://github.com/nodejs/node/pull/5678)
* [[`c6dc56175b`](https://github.com/nodejs/node/commit/c6dc56175b)] - **doc**: remove non-standard use of hyphens (Stefano Vozza)
* [[`4c92316972`](https://github.com/nodejs/node/commit/4c92316972)] - **doc**: add fansworld-claudio to collaborators (Claudio Rodriguez) [#5668](https://github.com/nodejs/node/pull/5668)
* [[`0a6e883f85`](https://github.com/nodejs/node/commit/0a6e883f85)] - **doc**: add thekemkid to collaborators (Glen Keane) [#5667](https://github.com/nodejs/node/pull/5667)
* [[`39c7d8a972`](https://github.com/nodejs/node/commit/39c7d8a972)] - **doc**: add AndreasMadsen to collaborators (Andreas Madsen) [#5666](https://github.com/nodejs/node/pull/5666)
* [[`eec3008970`](https://github.com/nodejs/node/commit/eec3008970)] - **doc**: add whitlockjc to collaborators (Jeremy Whitlock) [#5665](https://github.com/nodejs/node/pull/5665)
* [[`e5f254d83c`](https://github.com/nodejs/node/commit/e5f254d83c)] - **doc**: add benjamingr to collaborator list (Benjamin Gruenbaum) [#5664](https://github.com/nodejs/node/pull/5664)
* [[`3f718643c9`](https://github.com/nodejs/node/commit/3f718643c9)] - **doc**: add phillipj to collaborators (Phillip Johnsen) [#5663](https://github.com/nodejs/node/pull/5663)
* [[`2d5527fe69`](https://github.com/nodejs/node/commit/2d5527fe69)] - **doc**: add mattloring to collaborators (Matt Loring) [#5662](https://github.com/nodejs/node/pull/5662)
* [[`51763462bc`](https://github.com/nodejs/node/commit/51763462bc)] - **doc**: include typo in 'unhandledRejection' example (Robert C Jensen) [#5654](https://github.com/nodejs/node/pull/5654)
* [[`cae5da2f0a`](https://github.com/nodejs/node/commit/cae5da2f0a)] - **doc**: fix markdown links (Steve Mao) [#5641](https://github.com/nodejs/node/pull/5641)
* [[`b1b17efcb7`](https://github.com/nodejs/node/commit/b1b17efcb7)] - **doc**: move build instructions to a new document (Johan Bergström) [#5634](https://github.com/nodejs/node/pull/5634)
* [[`13a8bde1fa`](https://github.com/nodejs/node/commit/13a8bde1fa)] - **doc**: fix dns.resolveCname description typo (axvm) [#5622](https://github.com/nodejs/node/pull/5622)
* [[`1faea43c40`](https://github.com/nodejs/node/commit/1faea43c40)] - **doc**: fix typo in fs.symlink (Michaël Zasso) [#5560](https://github.com/nodejs/node/pull/5560)
* [[`98a1bb6989`](https://github.com/nodejs/node/commit/98a1bb6989)] - **doc**: document directories in test directory (Michael Barrett) [#5557](https://github.com/nodejs/node/pull/5557)
* [[`04d3f8a741`](https://github.com/nodejs/node/commit/04d3f8a741)] - **doc**: update link green to match homepage (silverwind) [#5548](https://github.com/nodejs/node/pull/5548)
* [[`1afab6ac9c`](https://github.com/nodejs/node/commit/1afab6ac9c)] - **doc**: add clarification on birthtime in fs stat (Kári Tristan Helgason) [#5479](https://github.com/nodejs/node/pull/5479)
* [[`d871ae2349`](https://github.com/nodejs/node/commit/d871ae2349)] - **doc**: fix typo in child_process documentation (Evan Lucas) [#5474](https://github.com/nodejs/node/pull/5474)
* [[`97a18bdbad`](https://github.com/nodejs/node/commit/97a18bdbad)] - **doc**: update NAN urls in ROADMAP.md and doc/releases.md (ronkorving) [#5472](https://github.com/nodejs/node/pull/5472)
* [[`d4a1fc7acd`](https://github.com/nodejs/node/commit/d4a1fc7acd)] - **doc**: add Testing WG (Rich Trott) [#5461](https://github.com/nodejs/node/pull/5461)
* [[`1642078580`](https://github.com/nodejs/node/commit/1642078580)] - **doc**: fix crypto function indentation level (Brian White) [#5460](https://github.com/nodejs/node/pull/5460)
* [[`2b0c7ad985`](https://github.com/nodejs/node/commit/2b0c7ad985)] - **doc**: fix links in tls, cluster docs (Alexander Makarenko) [#5364](https://github.com/nodejs/node/pull/5364)
* [[`901dbabea6`](https://github.com/nodejs/node/commit/901dbabea6)] - **doc**: fix relative links in net docs (Evan Lucas) [#5358](https://github.com/nodejs/node/pull/5358)
* [[`38d429172d`](https://github.com/nodejs/node/commit/38d429172d)] - **doc**: fix typo in pbkdf2Sync code sample (Marc Cuva) [#5306](https://github.com/nodejs/node/pull/5306)
* [[`d4cfc6f97c`](https://github.com/nodejs/node/commit/d4cfc6f97c)] - **doc**: add missing property in cluster example (Rafael Cepeda) [#5305](https://github.com/nodejs/node/pull/5305)
* [[`b66d6b1458`](https://github.com/nodejs/node/commit/b66d6b1458)] - **doc**: improve httpVersionMajor / httpVersionMajor (Jackson Tian) [#5296](https://github.com/nodejs/node/pull/5296)
* [[`70c872c9c4`](https://github.com/nodejs/node/commit/70c872c9c4)] - **doc**: improve unhandledException doc copy (James M Snell) [#5287](https://github.com/nodejs/node/pull/5287)
* [[`ba5e0b6110`](https://github.com/nodejs/node/commit/ba5e0b6110)] - **doc**: fix buf.readInt16LE output (Chinedu Francis Nwafili) [#5282](https://github.com/nodejs/node/pull/5282)
* [[`1624d5b049`](https://github.com/nodejs/node/commit/1624d5b049)] - **doc**: document base64url encoding support (Tristan Slominski) [#5243](https://github.com/nodejs/node/pull/5243)
* [[`b1d580c9d2`](https://github.com/nodejs/node/commit/b1d580c9d2)] - **doc**: update removeListener behaviour (Vaibhav) [#5201](https://github.com/nodejs/node/pull/5201)
* [[`ca17f91ba8`](https://github.com/nodejs/node/commit/ca17f91ba8)] - **doc**: add note for binary safe string reading (Anton Andesen) [#5155](https://github.com/nodejs/node/pull/5155)
* [[`0830bb4950`](https://github.com/nodejs/node/commit/0830bb4950)] - **doc**: clarify when writable.write callback is called (Kevin Locke) [#4810](https://github.com/nodejs/node/pull/4810)
* [[`17a74305c8`](https://github.com/nodejs/node/commit/17a74305c8)] - **doc**: add info to docs on how to submit docs patch (Sequoia McDowell) [#4591](https://github.com/nodejs/node/pull/4591)
* [[`470a9ca909`](https://github.com/nodejs/node/commit/470a9ca909)] - **doc**: add onboarding resources (Jeremiah Senkpiel) [#3726](https://github.com/nodejs/node/pull/3726)
* [[`3168e6b486`](https://github.com/nodejs/node/commit/3168e6b486)] - **doc**: update V8 URL (Craig Akimoto) [#5530](https://github.com/nodejs/node/pull/5530)
* [[`04d16eb7e8`](https://github.com/nodejs/node/commit/04d16eb7e8)] - **doc**: document fs.datasync(Sync) (Ron Korving) [#5402](https://github.com/nodejs/node/pull/5402)
* [[`29646200f8`](https://github.com/nodejs/node/commit/29646200f8)] - **doc**: add Evan Lucas to the CTC (Rod Vagg)
* [[`a2a32b7810`](https://github.com/nodejs/node/commit/a2a32b7810)] - **doc**: add Rich Trott to the CTC (Rod Vagg) [#5276](https://github.com/nodejs/node/pull/5276)
* [[`4e469d5e47`](https://github.com/nodejs/node/commit/4e469d5e47)] - **doc**: add Ali Ijaz Sheikh to the CTC (Rod Vagg) [#5277](https://github.com/nodejs/node/pull/5277)
* [[`d09b44f59b`](https://github.com/nodejs/node/commit/d09b44f59b)] - **doc**: add Сковорода Никита Андреевич to the CTC (Rod Vagg) [#5278](https://github.com/nodejs/node/pull/5278)
* [[`ebbc64bc97`](https://github.com/nodejs/node/commit/ebbc64bc97)] - **doc**: add "building node with ninja" guide (Jeremiah Senkpiel) [#4767](https://github.com/nodejs/node/pull/4767)
* [[`67245fa0e3`](https://github.com/nodejs/node/commit/67245fa0e3)] - **doc**: clarify code of conduct reporting (Julie Pagano) [#5107](https://github.com/nodejs/node/pull/5107)
* [[`cd78ff9706`](https://github.com/nodejs/node/commit/cd78ff9706)] - **doc**: fix links in Addons docs (Alexander Makarenko) [#5072](https://github.com/nodejs/node/pull/5072)
* [[`20539954ff`](https://github.com/nodejs/node/commit/20539954ff)] - **docs**: fix man pages link if tok type is code (Mithun Patel) [#5721](https://github.com/nodejs/node/pull/5721)
* [[`38d7b0b6ea`](https://github.com/nodejs/node/commit/38d7b0b6ea)] - **docs**: update link to iojs+release ci job (Myles Borins) [#5632](https://github.com/nodejs/node/pull/5632)
* [[`f982632f90`](https://github.com/nodejs/node/commit/f982632f90)] - **http**: remove old, confusing comment (Brian White) [#5233](https://github.com/nodejs/node/pull/5233)
* [[`ca5d7a8bb6`](https://github.com/nodejs/node/commit/ca5d7a8bb6)] - **http**: remove unnecessary check (Brian White) [#5233](https://github.com/nodejs/node/pull/5233)
* [[`2ce83bd8f9`](https://github.com/nodejs/node/commit/2ce83bd8f9)] - **http,util**: fix typos in comments (Alexander Makarenko) [#5279](https://github.com/nodejs/node/pull/5279)
* [[`b690916e5a`](https://github.com/nodejs/node/commit/b690916e5a)] - **lib**: freelist: use .pop() for allocation (Anton Khlynovskiy) [#2174](https://github.com/nodejs/node/pull/2174)
* [[`e7f45f0a17`](https://github.com/nodejs/node/commit/e7f45f0a17)] - **repl**: handle quotes within regexp literal (Prince J Wesley) [#5117](https://github.com/nodejs/node/pull/5117)
* [[`7c3b844f78`](https://github.com/nodejs/node/commit/7c3b844f78)] - **src**: return UV_EAI_NODATA on empty lookup (cjihrig) [#4715](https://github.com/nodejs/node/pull/4715)
* [[`242a65e930`](https://github.com/nodejs/node/commit/242a65e930)] - **stream**: prevent object map change in TransformState (Evan Lucas) [#5032](https://github.com/nodejs/node/pull/5032)
* [[`fb5ba6b928`](https://github.com/nodejs/node/commit/fb5ba6b928)] - **stream**: prevent object map change in ReadableState (Evan Lucas) [#4761](https://github.com/nodejs/node/pull/4761)
* [[`04db9efd78`](https://github.com/nodejs/node/commit/04db9efd78)] - **stream**: fix no data on partial decode (Brian White) [#5226](https://github.com/nodejs/node/pull/5226)
* [[`cc0e36ff98`](https://github.com/nodejs/node/commit/cc0e36ff98)] - **string_decoder**: fix performance regression (Brian White) [#5134](https://github.com/nodejs/node/pull/5134)
* [[`666d3690d8`](https://github.com/nodejs/node/commit/666d3690d8)] - **test**: eval a strict function (Kári Tristan Helgason) [#5250](https://github.com/nodejs/node/pull/5250)
* [[`9952bcf203`](https://github.com/nodejs/node/commit/9952bcf203)] - **test**: bug repro for vm function redefinition (cjihrig) [#5528](https://github.com/nodejs/node/pull/5528)
* [[`063f22f1f0`](https://github.com/nodejs/node/commit/063f22f1f0)] - **test**: check memoryUsage properties The properties on memoryUsage were not checked before, this commit checks them. (Wyatt Preul) [#5546](https://github.com/nodejs/node/pull/5546)
* [[`7a0fcfc127`](https://github.com/nodejs/node/commit/7a0fcfc127)] - **test**: remove broken debugger scenarios (Rich Trott) [#5532](https://github.com/nodejs/node/pull/5532)
* [[`ba9ad2662c`](https://github.com/nodejs/node/commit/ba9ad2662c)] - **test**: apply Linux workaround to Linux only (Rich Trott) [#5471](https://github.com/nodejs/node/pull/5471)
* [[`4aa2c03d31`](https://github.com/nodejs/node/commit/4aa2c03d31)] - **test**: increase timeout for test-tls-fast-writing (Rich Trott) [#5466](https://github.com/nodejs/node/pull/5466)
* [[`b4ef644ce4`](https://github.com/nodejs/node/commit/b4ef644ce4)] - **test**: retry on known SmartOS bug (Rich Trott) [#5454](https://github.com/nodejs/node/pull/5454)
* [[`d681bf24b5`](https://github.com/nodejs/node/commit/d681bf24b5)] - **test**: fix flaky child-process-fork-regr-gh-2847 (Santiago Gimeno) [#5422](https://github.com/nodejs/node/pull/5422)
* [[`b4fbe04514`](https://github.com/nodejs/node/commit/b4fbe04514)] - **test**: fix test-timers.reliability on OS X (Rich Trott) [#5379](https://github.com/nodejs/node/pull/5379)
* [[`99269ffdbf`](https://github.com/nodejs/node/commit/99269ffdbf)] - **test**: increase timeouts on some unref timers tests (Jeremiah Senkpiel) [#5352](https://github.com/nodejs/node/pull/5352)
* [[`85f927a774`](https://github.com/nodejs/node/commit/85f927a774)] - **test**: prevent flakey test on pi2 (Trevor Norris) [#5537](https://github.com/nodejs/node/pull/5537)
* [[`c86902d800`](https://github.com/nodejs/node/commit/c86902d800)] - **test**: mitigate flaky test-http-agent (Rich Trott) [#5346](https://github.com/nodejs/node/pull/5346)
* [[`f242e62817`](https://github.com/nodejs/node/commit/f242e62817)] - **test**: remove flaky designation from fixed tests (Rich Trott) [#5459](https://github.com/nodejs/node/pull/5459)
* [[`a39aacf035`](https://github.com/nodejs/node/commit/a39aacf035)] - **test**: refactor test-dgram-udp4 (Santiago Gimeno) [#5339](https://github.com/nodejs/node/pull/5339)
* [[`6386f62221`](https://github.com/nodejs/node/commit/6386f62221)] - **test**: remove unneeded bind() and related comments (Aayush Naik) [#5023](https://github.com/nodejs/node/pull/5023)
* [[`068b0cbd12`](https://github.com/nodejs/node/commit/068b0cbd12)] - **test**: move cluster tests to parallel (Rich Trott) [#4774](https://github.com/nodejs/node/pull/4774)
* [[`a673c9ae2d`](https://github.com/nodejs/node/commit/a673c9ae2d)] - **tls**: fix assert in context._external accessor (Ben Noordhuis) [#5521](https://github.com/nodejs/node/pull/5521)
* [[`8ffef48fee`](https://github.com/nodejs/node/commit/8ffef48fee)] - **tools**: fix gyp to work on MacOSX without XCode (Shigeki Ohtsu) [nodejs/node#1325](https://github.com/nodejs/node/pull/1325)
* [[`4b6a8f4321`](https://github.com/nodejs/node/commit/4b6a8f4321)] - **tools**: update gyp to b3cef02 (Imran Iqbal) [#3487](https://github.com/nodejs/node/pull/3487)
* [[`7501ddc878`](https://github.com/nodejs/node/commit/7501ddc878)] - **tools**: support testing known issues (cjihrig) [#5528](https://github.com/nodejs/node/pull/5528)
* [[`10ec1d2a6b`](https://github.com/nodejs/node/commit/10ec1d2a6b)] - **tools**: enable linting for benchmarks (Rich Trott) [#5773](https://github.com/nodejs/node/pull/5773)
* [[`deec8bc5f5`](https://github.com/nodejs/node/commit/deec8bc5f5)] - **tools**: reduce verbosity of cpplint (Sakthipriyan Vairamani) [#5578](https://github.com/nodejs/node/pull/5578)
* [[`64d5752711`](https://github.com/nodejs/node/commit/64d5752711)] - **tools**: enable no-self-assign ESLint rule (Rich Trott) [#5552](https://github.com/nodejs/node/pull/5552)
* [[`131ed494e2`](https://github.com/nodejs/node/commit/131ed494e2)] - **tools**: enable no-extra-parens in ESLint (Rich Trott) [#5512](https://github.com/nodejs/node/pull/5512)
* [[`d4b9f02fdc`](https://github.com/nodejs/node/commit/d4b9f02fdc)] - **tools**: apply custom buffer lint rule to /lib only (Rich Trott) [#5371](https://github.com/nodejs/node/pull/5371)
* [[`6867bed4c4`](https://github.com/nodejs/node/commit/6867bed4c4)] - **tools**: enable additional lint rules (Rich Trott) [#5357](https://github.com/nodejs/node/pull/5357)
* [[`5e6b7605ee`](https://github.com/nodejs/node/commit/5e6b7605ee)] - **tools**: add Node.js-specific ESLint rules (Rich Trott) [#5320](https://github.com/nodejs/node/pull/5320)
* [[`6dc49ae203`](https://github.com/nodejs/node/commit/6dc49ae203)] - **tools,benchmark**: increase lint compliance (Rich Trott) [#5773](https://github.com/nodejs/node/pull/5773)
* [[`dff7091fce`](https://github.com/nodejs/node/commit/dff7091fce)] - **url**: group slashed protocols by protocol name (nettofarah) [#5380](https://github.com/nodejs/node/pull/5380)
* [[`0e97a3ea51`](https://github.com/nodejs/node/commit/0e97a3ea51)] - **win,build**: support Visual C++ Build Tools 2015 (João Reis) [#5627](https://github.com/nodejs/node/pull/5627)

## 2016-03-08, Version 4.4.0 'Argon' (LTS), @thealphanerd

In December we announced that we would be doing a minor release in order to
get a number of voted on SEMVER-MINOR changes into LTS. Our ability to release this
was delayed due to the unforeseen security release v4.3. We are quickly bumping to
v4.4 in order to bring you the features that we had committed to releasing.

This release also includes over 70 fixes to our docs and over 50 fixes to tests.

### Notable changes

The SEMVER-MINOR changes include:
  * **deps**: 
    - An update to v8 that introduces a new flag --perf_basic_prof_only_functions (Ali Ijaz Sheikh) [#3609](https://github.com/nodejs/node/pull/3609)
  * **http**:
    - A new feature in http(s) agent that catches errors on *keep alived* connections (José F. Romaniello) [#4482](https://github.com/nodejs/node/pull/4482)
  * **src**:
    - Better support for Big-Endian systems (Bryon Leung) [#3410](https://github.com/nodejs/node/pull/3410)
  * **tls**:
    - A new feature that allows you to pass common SSL options to `tls.createSecurePair` (Коренберг Марк) [#2441](https://github.com/nodejs/node/pull/2441)
  * **tools**:
    - a new flag `--prof-process` which will execute the tick processor on the provided isolate files (Matt Loring) [#4021](https://github.com/nodejs/node/pull/4021)

Notable semver patch changes include:

  * **buld**:
    - Support python path that includes spaces. This should be of particular interest to our Windows users who may have python living in `c:/Program Files` (Felix Becker) [#4841](https://github.com/nodejs/node/pull/4841)
  * **https**:
    - A potential fix for [#3692](https://github.com/nodejs/node/issues/3692) HTTP/HTTPS client requests throwing EPROTO (Fedor Indutny) [#4982](https://github.com/nodejs/node/pull/4982)
  * **installer**:
    - More readable profiling information from isolate tick logs (Matt Loring) [#3032](https://github.com/nodejs/node/pull/3032)
  * **npm**:
    - upgrade to npm 2.14.20 (Kat Marchán) [#5510](https://github.com/nodejs/node/pull/5510)
  * **process**:
    - Add support for symbols in event emitters. Symbols didn't exist when it was written ¯\_(ツ)_/¯ (cjihrig) [#4798](https://github.com/nodejs/node/pull/4798)
  * **querystring**: 
    - querystring.parse() is now 13-22% faster! (Brian White) [#4675](https://github.com/nodejs/node/pull/4675)
  * **streams**:
    - performance improvements for moving small buffers that shows a 5% throughput gain. IoT projects have been seen to be as much as 10% faster with this change! (Matteo Collina) [#4354](https://github.com/nodejs/node/pull/4354)
  * **tools**:
    - eslint has been updated to version 2.1.0 (Rich Trott) [#5214](https://github.com/nodejs/node/pull/5214)

### Commits

* [[`360e04fd5a`](https://github.com/nodejs/node/commit/360e04fd5a)] - internal/child_process: call postSend on error (Fedor Indutny) [#4752](https://github.com/nodejs/node/pull/4752)
* [[`a29f501aa2`](https://github.com/nodejs/node/commit/a29f501aa2)] - **benchmark**: add a constant declaration for `net` (Minwoo Jung) [#3950](https://github.com/nodejs/node/pull/3950)
* [[`85e06a2e34`](https://github.com/nodejs/node/commit/85e06a2e34)] - **(SEMVER-MINOR)** **buffer**: allow encoding param to collapse (Trevor Norris) [#4803](https://github.com/nodejs/node/pull/4803)
* [[`fe893a8ebc`](https://github.com/nodejs/node/commit/fe893a8ebc)] - **(SEMVER-MINOR)** **buffer**: properly retrieve binary length of needle (Trevor Norris) [#4803](https://github.com/nodejs/node/pull/4803)
* [[`fae7c9db3f`](https://github.com/nodejs/node/commit/fae7c9db3f)] - **buffer**: refactor redeclared variables (Rich Trott) [#4886](https://github.com/nodejs/node/pull/4886)
* [[`4a6e2b26f7`](https://github.com/nodejs/node/commit/4a6e2b26f7)] - **build**: treat aarch64 as arm64 (Johan Bergström) [#5191](https://github.com/nodejs/node/pull/5191)
* [[`bc2536dfc6`](https://github.com/nodejs/node/commit/bc2536dfc6)] - **build**: add a help message and removed a TODO. (Ojas Shirekar) [#5080](https://github.com/nodejs/node/pull/5080)
* [[`f6416be5d2`](https://github.com/nodejs/node/commit/f6416be5d2)] - **build**: remove redundant TODO in configure (Ojas Shirekar) [#5080](https://github.com/nodejs/node/pull/5080)
* [[`6deb7a6eb8`](https://github.com/nodejs/node/commit/6deb7a6eb8)] - **build**: remove Makefile.build (Ojas Shirekar) [#5080](https://github.com/nodejs/node/pull/5080)
* [[`66d1115555`](https://github.com/nodejs/node/commit/66d1115555)] - **build**: fix build when python path contains spaces (Felix Becker) [#4841](https://github.com/nodejs/node/pull/4841)
* [[`29951cf36a`](https://github.com/nodejs/node/commit/29951cf36a)] - **child_process**: fix data loss with readable event (Brian White) [#5036](https://github.com/nodejs/node/pull/5036)
* [[`81d4127279`](https://github.com/nodejs/node/commit/81d4127279)] - **cluster**: dont rely on `this` in `fork` (Igor Klopov) [#5216](https://github.com/nodejs/node/pull/5216)
* [[`de4c07b29e`](https://github.com/nodejs/node/commit/de4c07b29e)] - **console**: apply null as `this` for util.format (Jackson Tian) [#5222](https://github.com/nodejs/node/pull/5222)
* [[`4e0755cab3`](https://github.com/nodejs/node/commit/4e0755cab3)] - **crypto**: have fixed NodeBIOs return EOF (Adam Langley) [#5105](https://github.com/nodejs/node/pull/5105)
* [[`a7955d5071`](https://github.com/nodejs/node/commit/a7955d5071)] - **crypto**: fix memory leak in LoadPKCS12 (Fedor Indutny) [#5109](https://github.com/nodejs/node/pull/5109)
* [[`5d9c1cf001`](https://github.com/nodejs/node/commit/5d9c1cf001)] - **crypto**: add `pfx` certs as CA certs too (Fedor Indutny) [#5109](https://github.com/nodejs/node/pull/5109)
* [[`ab5cb0539b`](https://github.com/nodejs/node/commit/ab5cb0539b)] - **crypto**: use SSL_CTX_clear_extra_chain_certs. (Adam Langley) [#4919](https://github.com/nodejs/node/pull/4919)
* [[`198928eb9f`](https://github.com/nodejs/node/commit/198928eb9f)] - **crypto**: fix build when OCSP-stapling not provided (Adam Langley) [#4914](https://github.com/nodejs/node/pull/4914)
* [[`b8e1089df0`](https://github.com/nodejs/node/commit/b8e1089df0)] - **crypto**: use a const SSL_CIPHER (Adam Langley) [#4913](https://github.com/nodejs/node/pull/4913)
* [[`139d6d9284`](https://github.com/nodejs/node/commit/139d6d9284)] - **debugger**: assert test before accessing this.binding (Prince J Wesley) [#5145](https://github.com/nodejs/node/pull/5145)
* [[`9c8f2ab546`](https://github.com/nodejs/node/commit/9c8f2ab546)] - **deps**: upgrade to npm 2.14.20 (Kat Marchán) [#5510](https://github.com/nodejs/node/pull/5510)
* [[`e591a0927f`](https://github.com/nodejs/node/commit/e591a0927f)] - **deps**: upgrade to npm 2.14.19 (Kat Marchán) [#5335](https://github.com/nodejs/node/pull/5335)
* [[`a5ce67a0aa`](https://github.com/nodejs/node/commit/a5ce67a0aa)] - **deps**: upgrade to npm 2.14.18 (Kat Marchán) [#5245](https://github.com/nodejs/node/pull/5245)
* [[`469db021f7`](https://github.com/nodejs/node/commit/469db021f7)] - **(SEMVER-MINOR)** **deps**: backport 9da3ab6 from V8 upstream (Ali Ijaz Sheikh) [#3609](https://github.com/nodejs/node/pull/3609)
* [[`3ca04a5de9`](https://github.com/nodejs/node/commit/3ca04a5de9)] - **deps**: backport 8d00c2c from v8 upstream (Gibson Fahnestock) [#5024](https://github.com/nodejs/node/pull/5024)
* [[`60e0bd4be9`](https://github.com/nodejs/node/commit/60e0bd4be9)] - **deps**: upgrade to npm 2.14.17 (Kat Marchán) [#5110](https://github.com/nodejs/node/pull/5110)
* [[`976b9a9ab3`](https://github.com/nodejs/node/commit/976b9a9ab3)] - **deps**: upgrade to npm 2.14.16 (Kat Marchán) [#4960](https://github.com/nodejs/node/pull/4960)
* [[`38b370abea`](https://github.com/nodejs/node/commit/38b370abea)] - **deps**: upgrade to npm 2.14.15 (Kat Marchán) [#4872](https://github.com/nodejs/node/pull/4872)
* [[`82f549ef81`](https://github.com/nodejs/node/commit/82f549ef81)] - **dgram**: scope redeclared variables (Rich Trott) [#4940](https://github.com/nodejs/node/pull/4940)
* [[`063e14b568`](https://github.com/nodejs/node/commit/063e14b568)] - **dns**: throw a TypeError in lookupService with invalid port (Evan Lucas) [#4839](https://github.com/nodejs/node/pull/4839)
* [[`a2613aefae`](https://github.com/nodejs/node/commit/a2613aefae)] - **doc**: remove out-of-date matter from internal docs (Rich Trott) [#5421](https://github.com/nodejs/node/pull/5421)
* [[`394743f4b3`](https://github.com/nodejs/node/commit/394743f4b3)] - **doc**: explicit about VS 2015 support in readme (Phillip Johnsen) [#5406](https://github.com/nodejs/node/pull/5406)
* [[`da6b26fbfb`](https://github.com/nodejs/node/commit/da6b26fbfb)] - **doc**: copyedit util doc (Rich Trott) [#5399](https://github.com/nodejs/node/pull/5399)
* [[`7070ad0cc0`](https://github.com/nodejs/node/commit/7070ad0cc0)] - **doc**: mention prototype check in deepStrictEqual() (cjihrig) [#5367](https://github.com/nodejs/node/pull/5367)
* [[`d4789fc5fd`](https://github.com/nodejs/node/commit/d4789fc5fd)] - **doc**: s/http/https in Myles Borins' GitHub link (Rod Vagg) [#5356](https://github.com/nodejs/node/pull/5356)
* [[`b86540d1eb`](https://github.com/nodejs/node/commit/b86540d1eb)] - **doc**: clarify error handling in net.createServer (Dirceu Pereira Tiegs) [#5353](https://github.com/nodejs/node/pull/5353)
* [[`3106297037`](https://github.com/nodejs/node/commit/3106297037)] - **doc**: `require` behavior on case-insensitive systems (Hugo Wood)
* [[`e0b45e4315`](https://github.com/nodejs/node/commit/e0b45e4315)] - **doc**: update repo docs to use 'CTC' (Alexis Campailla) [#5304](https://github.com/nodejs/node/pull/5304)
* [[`e355f13989`](https://github.com/nodejs/node/commit/e355f13989)] - **doc**: improvements to crypto.markdown copy (Alexander Makarenko) [#5230](https://github.com/nodejs/node/pull/5230)
* [[`a9035b5e1d`](https://github.com/nodejs/node/commit/a9035b5e1d)] - **doc**: link to man pages (dcposch@dcpos.ch) [#5073](https://github.com/nodejs/node/pull/5073)
* [[`2043e6a63c`](https://github.com/nodejs/node/commit/2043e6a63c)] - **doc**: clarify child_process.execFile{,Sync} file arg (Kevin Locke) [#5310](https://github.com/nodejs/node/pull/5310)
* [[`8c732ad1e1`](https://github.com/nodejs/node/commit/8c732ad1e1)] - **doc**: fix buf.length slice example (Chinedu Francis Nwafili) [#5259](https://github.com/nodejs/node/pull/5259)
* [[`6c27c78b8b`](https://github.com/nodejs/node/commit/6c27c78b8b)] - **doc**: fix buffer\[index\] example (Chinedu Francis Nwafili) [#5253](https://github.com/nodejs/node/pull/5253)
* [[`7765f99683`](https://github.com/nodejs/node/commit/7765f99683)] - **doc**: fix template string (Rafael Cepeda) [#5240](https://github.com/nodejs/node/pull/5240)
* [[`d15ef20162`](https://github.com/nodejs/node/commit/d15ef20162)] - **doc**: improvements to console.markdown copy (Alexander Makarenko) [#5225](https://github.com/nodejs/node/pull/5225)
* [[`593206a752`](https://github.com/nodejs/node/commit/593206a752)] - **doc**: fix net.createConnection() example (Brian White) [#5219](https://github.com/nodejs/node/pull/5219)
* [[`464636b5c5`](https://github.com/nodejs/node/commit/464636b5c5)] - **doc**: improve scrolling, various CSS tweaks (Roman Reiss) [#5198](https://github.com/nodejs/node/pull/5198)
* [[`f615cd5b0b`](https://github.com/nodejs/node/commit/f615cd5b0b)] - **doc**: console is asynchronous unless it's a file (Ben Noordhuis) [#5133](https://github.com/nodejs/node/pull/5133)
* [[`fbed0d11f1`](https://github.com/nodejs/node/commit/fbed0d11f1)] - **doc**: merging behavior of writeHead vs setHeader (Alejandro Oviedo) [#5081](https://github.com/nodejs/node/pull/5081)
* [[`b0bb42bd7d`](https://github.com/nodejs/node/commit/b0bb42bd7d)] - **doc**: fix reference to API `hash.final` (Minwoo Jung) [#5050](https://github.com/nodejs/node/pull/5050)
* [[`dee5045221`](https://github.com/nodejs/node/commit/dee5045221)] - **doc**: uppercase 'RSA-SHA256' in crypto.markdown (Rainer Oviir) [#5044](https://github.com/nodejs/node/pull/5044)
* [[`498052a017`](https://github.com/nodejs/node/commit/498052a017)] - **doc**: consistent styling for functions in TLS docs (Alexander Makarenko) [#5000](https://github.com/nodejs/node/pull/5000)
* [[`031277e6f8`](https://github.com/nodejs/node/commit/031277e6f8)] - **doc**: apply consistent styling for functions (Rich Trott) [#4974](https://github.com/nodejs/node/pull/4974)
* [[`808fe0ea48`](https://github.com/nodejs/node/commit/808fe0ea48)] - **doc**: fix `notDeepEqual` API (Minwoo Jung) [#4971](https://github.com/nodejs/node/pull/4971)
* [[`5b9025689f`](https://github.com/nodejs/node/commit/5b9025689f)] - **doc**: show links consistently in deprecations (Sakthipriyan Vairamani) [#4907](https://github.com/nodejs/node/pull/4907)
* [[`3a1865db5e`](https://github.com/nodejs/node/commit/3a1865db5e)] - **doc**: don't use "interface" as a variable name (ChALkeR) [#4900](https://github.com/nodejs/node/pull/4900)
* [[`90715c3d68`](https://github.com/nodejs/node/commit/90715c3d68)] - **doc**: keep the names in sorted order (Sakthipriyan Vairamani) [#4876](https://github.com/nodejs/node/pull/4876)
* [[`d8b3b25c9c`](https://github.com/nodejs/node/commit/d8b3b25c9c)] - **doc**: fix JSON generation for aliased methods (Timothy Gu) [#4871](https://github.com/nodejs/node/pull/4871)
* [[`7b763c8d25`](https://github.com/nodejs/node/commit/7b763c8d25)] - **doc**: fix code type of markdowns (Jackson Tian) [#4858](https://github.com/nodejs/node/pull/4858)
* [[`37d4e7afc2`](https://github.com/nodejs/node/commit/37d4e7afc2)] - **doc**: check for errors in 'listen' event (Benjamin Gruenbaum) [#4834](https://github.com/nodejs/node/pull/4834)
* [[`3f876b104c`](https://github.com/nodejs/node/commit/3f876b104c)] - **doc**: Examples work when data exceeds buffer size (Glen Arrowsmith) [#4811](https://github.com/nodejs/node/pull/4811)
* [[`e3e20422a7`](https://github.com/nodejs/node/commit/e3e20422a7)] - **doc**: harmonize $ node command line notation (Robert Jefe Lindstaedt) [#4806](https://github.com/nodejs/node/pull/4806)
* [[`73e0195cef`](https://github.com/nodejs/node/commit/73e0195cef)] - **doc**: fix type references for link gen, link css (Claudio Rodriguez) [#4741](https://github.com/nodejs/node/pull/4741)
* [[`0bdac429e1`](https://github.com/nodejs/node/commit/0bdac429e1)] - **doc**: multiple improvements in Stream docs (Alexander Makarenko) [#5009](https://github.com/nodejs/node/pull/5009)
* [[`693c16fb6b`](https://github.com/nodejs/node/commit/693c16fb6b)] - **doc**: fix anchor links from stream to http and events (piepmatz) [#5007](https://github.com/nodejs/node/pull/5007)
* [[`5fb533522c`](https://github.com/nodejs/node/commit/5fb533522c)] - **doc**: replace function expressions with arrows (Benjamin Gruenbaum) [#4832](https://github.com/nodejs/node/pull/4832)
* [[`e3572fb809`](https://github.com/nodejs/node/commit/e3572fb809)] - **doc**: fix links order in Buffer doc (Alexander Makarenko) [#5076](https://github.com/nodejs/node/pull/5076)
* [[`5c936ab765`](https://github.com/nodejs/node/commit/5c936ab765)] - **doc**: clarify optional arguments of Buffer methods (Michaël Zasso) [#5008](https://github.com/nodejs/node/pull/5008)
* [[`6df350c2b3`](https://github.com/nodejs/node/commit/6df350c2b3)] - **doc**: improve styling consistency in Buffer docs (Alexander Makarenko) [#5001](https://github.com/nodejs/node/pull/5001)
* [[`047f4a157f`](https://github.com/nodejs/node/commit/047f4a157f)] - **doc**: make buffer methods styles consistent (Timothy Gu) [#4873](https://github.com/nodejs/node/pull/4873)
* [[`4cfc017b90`](https://github.com/nodejs/node/commit/4cfc017b90)] - **doc**: fix nonsensical grammar in Buffer::write (Jimb Esser) [#4863](https://github.com/nodejs/node/pull/4863)
* [[`9087f6daca`](https://github.com/nodejs/node/commit/9087f6daca)] - **doc**: fix named anchors in addons.markdown and http.markdown (Michael Theriot) [#4708](https://github.com/nodejs/node/pull/4708)
* [[`4c8713ce58`](https://github.com/nodejs/node/commit/4c8713ce58)] - **doc**: add buf.indexOf encoding param with example (Karl Skomski) [#3373](https://github.com/nodejs/node/pull/3373)
* [[`1819d74491`](https://github.com/nodejs/node/commit/1819d74491)] - **doc**: fenced all code blocks, typo fixes (Robert Jefe Lindstaedt) [#4733](https://github.com/nodejs/node/pull/4733)
* [[`961735e645`](https://github.com/nodejs/node/commit/961735e645)] - **doc**: make references clickable (Roman Klauke) [#4654](https://github.com/nodejs/node/pull/4654)
* [[`7e80442483`](https://github.com/nodejs/node/commit/7e80442483)] - **doc**: improve child_process.execFile() code example (Ryan Sobol) [#4504](https://github.com/nodejs/node/pull/4504)
* [[`de9ad5b39d`](https://github.com/nodejs/node/commit/de9ad5b39d)] - **doc**: remove "above" and "below" references (Richard Sun) [#4499](https://github.com/nodejs/node/pull/4499)
* [[`c549ca3b69`](https://github.com/nodejs/node/commit/c549ca3b69)] - **doc**: fix heading level error in Buffer doc (Shigeki Ohtsu) [#4537](https://github.com/nodejs/node/pull/4537)
* [[`a613bae14c`](https://github.com/nodejs/node/commit/a613bae14c)] - **doc**: improvements to crypto.markdown copy (James M Snell) [#4435](https://github.com/nodejs/node/pull/4435)
* [[`18f580d0c1`](https://github.com/nodejs/node/commit/18f580d0c1)] - **doc**: improve child_process.markdown copy (James M Snell) [#4383](https://github.com/nodejs/node/pull/4383)
* [[`a929837311`](https://github.com/nodejs/node/commit/a929837311)] - **doc**: improvements to buffer.markdown copy (James M Snell) [#4370](https://github.com/nodejs/node/pull/4370)
* [[`a22f688407`](https://github.com/nodejs/node/commit/a22f688407)] - **doc**: improve addons.markdown copy (James M Snell) [#4320](https://github.com/nodejs/node/pull/4320)
* [[`94c2de47b1`](https://github.com/nodejs/node/commit/94c2de47b1)] - **doc**: update process.send() signature (cjihrig) [#5284](https://github.com/nodejs/node/pull/5284)
* [[`4e1926cb08`](https://github.com/nodejs/node/commit/4e1926cb08)] - **doc**: replace node-forward link in CONTRIBUTING.md (Ben Noordhuis) [#5227](https://github.com/nodejs/node/pull/5227)
* [[`e1713e81e5`](https://github.com/nodejs/node/commit/e1713e81e5)] - **doc**: fix minor inconsistencies in repl doc (Rich Trott) [#5193](https://github.com/nodejs/node/pull/5193)
* [[`b2e72c0d92`](https://github.com/nodejs/node/commit/b2e72c0d92)] - **doc**: clarify exceptions during uncaughtException (Noah Rose) [#5180](https://github.com/nodejs/node/pull/5180)
* [[`c3c549836a`](https://github.com/nodejs/node/commit/c3c549836a)] - **doc**: update DCO to v1.1 (Mikeal Rogers) [#5170](https://github.com/nodejs/node/pull/5170)
* [[`9dd35ad594`](https://github.com/nodejs/node/commit/9dd35ad594)] - **doc**: fix dgram doc indentation (Rich Trott) [#5118](https://github.com/nodejs/node/pull/5118)
* [[`eed830702c`](https://github.com/nodejs/node/commit/eed830702c)] - **doc**: fix typo in dgram doc (Rich Trott) [#5114](https://github.com/nodejs/node/pull/5114)
* [[`abfb2f5864`](https://github.com/nodejs/node/commit/abfb2f5864)] - **doc**: fix link in cluster documentation (Timothy Gu) [#5068](https://github.com/nodejs/node/pull/5068)
* [[`8b040b5bb2`](https://github.com/nodejs/node/commit/8b040b5bb2)] - **doc**: fix minor typo in process doc (Prayag Verma) [#5018](https://github.com/nodejs/node/pull/5018)
* [[`47eebe1d80`](https://github.com/nodejs/node/commit/47eebe1d80)] - **doc**: fix typo in Readme.md (Prayag Verma) [#5017](https://github.com/nodejs/node/pull/5017)
* [[`2b97ff89a6`](https://github.com/nodejs/node/commit/2b97ff89a6)] - **doc**: minor improvement in OS docs (Alexander Makarenko) [#5006](https://github.com/nodejs/node/pull/5006)
* [[`9a5d58b89e`](https://github.com/nodejs/node/commit/9a5d58b89e)] - **doc**: improve styling consistency in VM docs (Alexander Makarenko) [#5005](https://github.com/nodejs/node/pull/5005)
* [[`960e1bab98`](https://github.com/nodejs/node/commit/960e1bab98)] - **doc**: minor improvement to HTTPS doc (Alexander Makarenko) [#5002](https://github.com/nodejs/node/pull/5002)
* [[`6048b011e8`](https://github.com/nodejs/node/commit/6048b011e8)] - **doc**: spell writable consistently (Peter Lyons) [#4954](https://github.com/nodejs/node/pull/4954)
* [[`7b8f904167`](https://github.com/nodejs/node/commit/7b8f904167)] - **doc**: update eol handling in readline (Kári Tristan Helgason) [#4927](https://github.com/nodejs/node/pull/4927)
* [[`83efd0d4d1`](https://github.com/nodejs/node/commit/83efd0d4d1)] - **doc**: add more details to process.env (Evan Lucas) [#4924](https://github.com/nodejs/node/pull/4924)
* [[`b2d2c0b588`](https://github.com/nodejs/node/commit/b2d2c0b588)] - **doc**: undo move http.IncomingMessage.statusMessage (Jeff Harris) [#4822](https://github.com/nodejs/node/pull/4822)
* [[`b091c41b53`](https://github.com/nodejs/node/commit/b091c41b53)] - **doc**: proper markdown escaping -> \_\_, \*, \_ (Robert Jefe Lindstaedt) [#4805](https://github.com/nodejs/node/pull/4805)
* [[`0887208290`](https://github.com/nodejs/node/commit/0887208290)] - **doc**: remove unnecessary bind(this) (Dmitriy Lazarev) [#4797](https://github.com/nodejs/node/pull/4797)
* [[`f3e3c70bca`](https://github.com/nodejs/node/commit/f3e3c70bca)] - **doc**: Update small error in LICENSE for npm (Kat Marchán) [#4872](https://github.com/nodejs/node/pull/4872)
* [[`e703b180b3`](https://github.com/nodejs/node/commit/e703b180b3)] - **doc,tools,test**: lint doc-based addon tests (Rich Trott) [#5427](https://github.com/nodejs/node/pull/5427)
* [[`0f3b8ca192`](https://github.com/nodejs/node/commit/0f3b8ca192)] - **fs**: refactor redeclared variables (Rich Trott) [#4959](https://github.com/nodejs/node/pull/4959)
* [[`152c6b6b8d`](https://github.com/nodejs/node/commit/152c6b6b8d)] - **http**: remove reference to onParserExecute (Tom Atkinson) [#4773](https://github.com/nodejs/node/pull/4773)
* [[`6a0571cd72`](https://github.com/nodejs/node/commit/6a0571cd72)] - **http**: do not emit `upgrade` on advertisement (Fedor Indutny) [#4337](https://github.com/nodejs/node/pull/4337)
* [[`567ced9ef0`](https://github.com/nodejs/node/commit/567ced9ef0)] - **(SEMVER-MINOR)** **http**: handle errors on idle sockets (José F. Romaniello) [#4482](https://github.com/nodejs/node/pull/4482)
* [[`de5177ccb8`](https://github.com/nodejs/node/commit/de5177ccb8)] - **https**: evict cached sessions on error (Fedor Indutny) [#4982](https://github.com/nodejs/node/pull/4982)
* [[`77a6036264`](https://github.com/nodejs/node/commit/77a6036264)] - **installer**: install the tick processor (Matt Loring) [#3032](https://github.com/nodejs/node/pull/3032)
* [[`ea16d8d7c5`](https://github.com/nodejs/node/commit/ea16d8d7c5)] - **lib**: remove string_decoder.js var redeclarations (Rich Trott) [#4978](https://github.com/nodejs/node/pull/4978)
* [[`1389660ab3`](https://github.com/nodejs/node/commit/1389660ab3)] - **lib**: scope loop variables (Rich Trott) [#4965](https://github.com/nodejs/node/pull/4965)
* [[`59255d7218`](https://github.com/nodejs/node/commit/59255d7218)] - **lib**: use arrow functions instead of bind (Minwoo Jung) [#3622](https://github.com/nodejs/node/pull/3622)
* [[`fd26960aab`](https://github.com/nodejs/node/commit/fd26960aab)] - **lib,test**: remove extra semicolons (Michaël Zasso) [#2205](https://github.com/nodejs/node/pull/2205)
* [[`9646d26ffd`](https://github.com/nodejs/node/commit/9646d26ffd)] - **module**: refactor redeclared variable (Rich Trott) [#4962](https://github.com/nodejs/node/pull/4962)
* [[`09311128e8`](https://github.com/nodejs/node/commit/09311128e8)] - **net**: use `_server` for internal book-keeping (Fedor Indutny) [#5262](https://github.com/nodejs/node/pull/5262)
* [[`824c402174`](https://github.com/nodejs/node/commit/824c402174)] - **net**: refactor redeclared variables (Rich Trott) [#4963](https://github.com/nodejs/node/pull/4963)
* [[`96f306f3cf`](https://github.com/nodejs/node/commit/96f306f3cf)] - **net**: move isLegalPort to internal/net (Evan Lucas) [#4882](https://github.com/nodejs/node/pull/4882)
* [[`78d64889bd`](https://github.com/nodejs/node/commit/78d64889bd)] - **node**: set process._eventsCount to 0 on startup (Evan Lucas) [#5208](https://github.com/nodejs/node/pull/5208)
* [[`7a2e8f4356`](https://github.com/nodejs/node/commit/7a2e8f4356)] - **process**: support symbol events (cjihrig) [#4798](https://github.com/nodejs/node/pull/4798)
* [[`c9e2dce247`](https://github.com/nodejs/node/commit/c9e2dce247)] - **querystring**: improve parse() performance (Brian White) [#4675](https://github.com/nodejs/node/pull/4675)
* [[`18542c41fe`](https://github.com/nodejs/node/commit/18542c41fe)] - **repl**: remove variable redeclaration (Rich Trott) [#4977](https://github.com/nodejs/node/pull/4977)
* [[`10be8dc360`](https://github.com/nodejs/node/commit/10be8dc360)] - **src**: force line buffering for stderr (Rich Trott) [#3701](https://github.com/nodejs/node/pull/3701)
* [[`7958664e85`](https://github.com/nodejs/node/commit/7958664e85)] - **src**: clean up usage of __proto__ (Jackson Tian) [#5069](https://github.com/nodejs/node/pull/5069)
* [[`4e0a0d51b3`](https://github.com/nodejs/node/commit/4e0a0d51b3)] - **src**: remove no longer relevant comments (Chris911) [#4843](https://github.com/nodejs/node/pull/4843)
* [[`51c8bc8abc`](https://github.com/nodejs/node/commit/51c8bc8abc)] - **src**: remove __builtin_bswap16 call (Ben Noordhuis) [#4290](https://github.com/nodejs/node/pull/4290)
* [[`5e1976e37c`](https://github.com/nodejs/node/commit/5e1976e37c)] - **src**: remove unused BITS_PER_LONG macro (Ben Noordhuis) [#4290](https://github.com/nodejs/node/pull/4290)
* [[`c18ef54d88`](https://github.com/nodejs/node/commit/c18ef54d88)] - **(SEMVER-MINOR)** **src**: add BE support to StringBytes::Encode() (Bryon Leung) [#3410](https://github.com/nodejs/node/pull/3410)
* [[`be9e7610b5`](https://github.com/nodejs/node/commit/be9e7610b5)] - **src,test,tools**: modify for more stringent linting (Rich Trott) [#5214](https://github.com/nodejs/node/pull/5214)
* [[`538c4756a7`](https://github.com/nodejs/node/commit/538c4756a7)] - **stream**: refactor redeclared variables (Rich Trott) [#4816](https://github.com/nodejs/node/pull/4816)
* [[`4fa22e4126`](https://github.com/nodejs/node/commit/4fa22e4126)] - **streams**: 5% throughput gain when sending small chunks (Matteo Collina) [#4354](https://github.com/nodejs/node/pull/4354)
* [[`b6bd87495f`](https://github.com/nodejs/node/commit/b6bd87495f)] - **test**: remove flaky mark for test-debug-no-context (Rich Trott) [#5317](https://github.com/nodejs/node/pull/5317)
* [[`7705360e35`](https://github.com/nodejs/node/commit/7705360e35)] - **test**: add test for https server close event (Braydon Fuller) [#5106](https://github.com/nodejs/node/pull/5106)
* [[`9d6623e1d1`](https://github.com/nodejs/node/commit/9d6623e1d1)] - **test**: use String.prototype.repeat() for clarity (Rich Trott) [#5311](https://github.com/nodejs/node/pull/5311)
* [[`18e3987e2e`](https://github.com/nodejs/node/commit/18e3987e2e)] - **test**: mitigate flaky test-debug-no-context (Rich Trott) [#5269](https://github.com/nodejs/node/pull/5269)
* [[`058db07ce8`](https://github.com/nodejs/node/commit/058db07ce8)] - **test**: refactor test-dgram-send-callback-recursive (Santiago Gimeno) [#5079](https://github.com/nodejs/node/pull/5079)
* [[`1647113d7a`](https://github.com/nodejs/node/commit/1647113d7a)] - **test**: refactor test-http-destroyed-socket-write2 (Santiago Gimeno) [#4970](https://github.com/nodejs/node/pull/4970)
* [[`07dc2b50e2`](https://github.com/nodejs/node/commit/07dc2b50e2)] - **test**: shorten path for bogus socket (Rich Trott) [#4478](https://github.com/nodejs/node/pull/4478)
* [[`47e7c8c359`](https://github.com/nodejs/node/commit/47e7c8c359)] - **test**: mark test-http-regr-gh-2928 flaky (Rich Trott) [#5280](https://github.com/nodejs/node/pull/5280)
* [[`9dbd66f7ef`](https://github.com/nodejs/node/commit/9dbd66f7ef)] - **test**: mark test-http-agent flaky (Rich Trott) [#5209](https://github.com/nodejs/node/pull/5209)
* [[`98049876b5`](https://github.com/nodejs/node/commit/98049876b5)] - **test**: minimal repl eval option test (Rich Trott) [#5192](https://github.com/nodejs/node/pull/5192)
* [[`ae3185b8ac`](https://github.com/nodejs/node/commit/ae3185b8ac)] - **test**: disable fs watch tests for AIX (Michael Dawson) [#5187](https://github.com/nodejs/node/pull/5187)
* [[`b639c3345b`](https://github.com/nodejs/node/commit/b639c3345b)] - **test**: fix child-process-fork-regr-gh-2847 again (Santiago Gimeno) [#5179](https://github.com/nodejs/node/pull/5179)
* [[`8be3afc474`](https://github.com/nodejs/node/commit/8be3afc474)] - **test**: fix flaky test-http-regr-gh-2928 (Rich Trott) [#5154](https://github.com/nodejs/node/pull/5154)
* [[`46dc12bdcc`](https://github.com/nodejs/node/commit/46dc12bdcc)] - **test**: enable to work pkcs12 test in FIPS mode (Shigeki Ohtsu) [#5150](https://github.com/nodejs/node/pull/5150)
* [[`e19b8ea692`](https://github.com/nodejs/node/commit/e19b8ea692)] - **test**: remove unneeded common.indirectInstanceOf() (Rich Trott) [#5149](https://github.com/nodejs/node/pull/5149)
* [[`6072d2e15e`](https://github.com/nodejs/node/commit/6072d2e15e)] - **test**: disable gh-5100 test when in FIPS mode (Fedor Indutny) [#5144](https://github.com/nodejs/node/pull/5144)
* [[`a8417a2787`](https://github.com/nodejs/node/commit/a8417a2787)] - **test**: fix flaky test-dgram-pingpong (Rich Trott) [#5125](https://github.com/nodejs/node/pull/5125)
* [[`9db67a6a44`](https://github.com/nodejs/node/commit/9db67a6a44)] - **test**: fix child-process-fork-regr-gh-2847 (Santiago Gimeno) [#5121](https://github.com/nodejs/node/pull/5121)
* [[`69150caedc`](https://github.com/nodejs/node/commit/69150caedc)] - **test**: don't run test-tick-processor.js on Aix (Michael Dawson) [#5093](https://github.com/nodejs/node/pull/5093)
* [[`4a492b96b1`](https://github.com/nodejs/node/commit/4a492b96b1)] - **test**: mark flaky tests on Raspberry Pi (Rich Trott) [#5082](https://github.com/nodejs/node/pull/5082)
* [[`4301f2cdc2`](https://github.com/nodejs/node/commit/4301f2cdc2)] - **test**: fix inconsistent styling in test-url (Brian White) [#5014](https://github.com/nodejs/node/pull/5014)
* [[`865baaed60`](https://github.com/nodejs/node/commit/865baaed60)] - **test**: fix redeclared vars in sequential tests (Rich Trott) [#4999](https://github.com/nodejs/node/pull/4999)
* [[`663e852c1b`](https://github.com/nodejs/node/commit/663e852c1b)] - **test**: pummel test fixes (Rich Trott) [#4998](https://github.com/nodejs/node/pull/4998)
* [[`72d38a4a38`](https://github.com/nodejs/node/commit/72d38a4a38)] - **test**: fix redeclared vars in test-vm-* (Rich Trott) [#4997](https://github.com/nodejs/node/pull/4997)
* [[`97ddfa2b6e`](https://github.com/nodejs/node/commit/97ddfa2b6e)] - **test**: fix redeclared vars in test-url (Rich Trott) [#4993](https://github.com/nodejs/node/pull/4993)
* [[`43d4db4314`](https://github.com/nodejs/node/commit/43d4db4314)] - **test**: fix redeclared test-util-* vars (Rich Trott) [#4994](https://github.com/nodejs/node/pull/4994)
* [[`88fae38d0c`](https://github.com/nodejs/node/commit/88fae38d0c)] - **test**: fix variable redeclarations (Rich Trott) [#4992](https://github.com/nodejs/node/pull/4992)
* [[`58595f146a`](https://github.com/nodejs/node/commit/58595f146a)] - **test**: fix redeclared test-path vars (Rich Trott) [#4991](https://github.com/nodejs/node/pull/4991)
* [[`2b711d51fa`](https://github.com/nodejs/node/commit/2b711d51fa)] - **test**: fix var redeclarations in test-os (Rich Trott) [#4990](https://github.com/nodejs/node/pull/4990)
* [[`bd9e2c31d6`](https://github.com/nodejs/node/commit/bd9e2c31d6)] - **test**: fix test-net-* variable redeclarations (Rich Trott) [#4989](https://github.com/nodejs/node/pull/4989)
* [[`d67ab81882`](https://github.com/nodejs/node/commit/d67ab81882)] - **test**: fix redeclared test-intl var (Rich Trott) [#4988](https://github.com/nodejs/node/pull/4988)
* [[`d6dbb2fae7`](https://github.com/nodejs/node/commit/d6dbb2fae7)] - **test**: fix redeclared test-http-* vars (Rich Trott) [#4987](https://github.com/nodejs/node/pull/4987)
* [[`ecaa89a8cb`](https://github.com/nodejs/node/commit/ecaa89a8cb)] - **test**: fix redeclared test-event-emitter-* vars (Rich Trott) [#4985](https://github.com/nodejs/node/pull/4985)
* [[`299c729371`](https://github.com/nodejs/node/commit/299c729371)] - **test**: remove redeclared var in test-domain (Rich Trott) [#4984](https://github.com/nodejs/node/pull/4984)
* [[`35a4a203bf`](https://github.com/nodejs/node/commit/35a4a203bf)] - **test**: remove var redeclarations in test-crypto-* (Rich Trott) [#4981](https://github.com/nodejs/node/pull/4981)
* [[`1d56b74af0`](https://github.com/nodejs/node/commit/1d56b74af0)] - **test**: remove test-cluster-* var redeclarations (Rich Trott) [#4980](https://github.com/nodejs/node/pull/4980)
* [[`0ce12cc1ec`](https://github.com/nodejs/node/commit/0ce12cc1ec)] - **test**: fix test-http-extra-response flakiness (Santiago Gimeno) [#4979](https://github.com/nodejs/node/pull/4979)
* [[`c6b4bf138c`](https://github.com/nodejs/node/commit/c6b4bf138c)] - **test**: scope redeclared vars in test-child-process* (Rich Trott) [#4944](https://github.com/nodejs/node/pull/4944)
* [[`7654c171c7`](https://github.com/nodejs/node/commit/7654c171c7)] - **test**: refactor switch (Rich Trott) [#4870](https://github.com/nodejs/node/pull/4870)
* [[`226dfef690`](https://github.com/nodejs/node/commit/226dfef690)] - **test**: add common.platformTimeout() to dgram test (Rich Trott) [#4938](https://github.com/nodejs/node/pull/4938)
* [[`fb14bac662`](https://github.com/nodejs/node/commit/fb14bac662)] - **test**: fix flaky cluster test on Windows 10 (Rich Trott) [#4934](https://github.com/nodejs/node/pull/4934)
* [[`f5d29d7ac4`](https://github.com/nodejs/node/commit/f5d29d7ac4)] - **test**: Add assertion for TLS peer certificate fingerprint (Alan Cohen) [#4923](https://github.com/nodejs/node/pull/4923)
* [[`618427cea6`](https://github.com/nodejs/node/commit/618427cea6)] - **test**: fix test-tls-zero-clear-in flakiness (Santiago Gimeno) [#4888](https://github.com/nodejs/node/pull/4888)
* [[`8700c39c70`](https://github.com/nodejs/node/commit/8700c39c70)] - **test**: fix irregular whitespace issue (Roman Reiss) [#4864](https://github.com/nodejs/node/pull/4864)
* [[`2b026c9d5a`](https://github.com/nodejs/node/commit/2b026c9d5a)] - **test**: fs.link() test runs on same device (Drew Folta) [#4861](https://github.com/nodejs/node/pull/4861)
* [[`80a637ac4d`](https://github.com/nodejs/node/commit/80a637ac4d)] - **test**: scope redeclared variable (Rich Trott) [#4854](https://github.com/nodejs/node/pull/4854)
* [[`8c4903d4ef`](https://github.com/nodejs/node/commit/8c4903d4ef)] - **test**: update arrow function style (cjihrig) [#4813](https://github.com/nodejs/node/pull/4813)
* [[`0a44e6a447`](https://github.com/nodejs/node/commit/0a44e6a447)] - **test**: mark test-tick-processor flaky (Rich Trott) [#4809](https://github.com/nodejs/node/pull/4809)
* [[`363460616c`](https://github.com/nodejs/node/commit/363460616c)] - **test**: refactor test-net-settimeout (Rich Trott) [#4799](https://github.com/nodejs/node/pull/4799)
* [[`6841d82c22`](https://github.com/nodejs/node/commit/6841d82c22)] - **test**: remove race condition in http flood test (Rich Trott) [#4793](https://github.com/nodejs/node/pull/4793)
* [[`b5bae32847`](https://github.com/nodejs/node/commit/b5bae32847)] - **test**: remove test-http-exit-delay (Rich Trott) [#4786](https://github.com/nodejs/node/pull/4786)
* [[`60514f9521`](https://github.com/nodejs/node/commit/60514f9521)] - **test**: refactor test-fs-watch (Rich Trott) [#4776](https://github.com/nodejs/node/pull/4776)
* [[`2a3a431119`](https://github.com/nodejs/node/commit/2a3a431119)] - **test**: fix `net-socket-timeout-unref` flakiness (Santiago Gimeno) [#4772](https://github.com/nodejs/node/pull/4772)
* [[`9e6f3632a1`](https://github.com/nodejs/node/commit/9e6f3632a1)] - **test**: remove Object.observe from tests (Vladimir Kurchatkin) [#4769](https://github.com/nodejs/node/pull/4769)
* [[`f78daa67b8`](https://github.com/nodejs/node/commit/f78daa67b8)] - **test**: make npm tests work on prerelease node versions (Kat Marchán) [#4960](https://github.com/nodejs/node/pull/4960)
* [[`1c03191b6a`](https://github.com/nodejs/node/commit/1c03191b6a)] - **test**: make npm tests work on prerelease node versions (Kat Marchán) [#4872](https://github.com/nodejs/node/pull/4872)
* [[`d9c22cc896`](https://github.com/nodejs/node/commit/d9c22cc896)] - **test,buffer**: refactor redeclarations (Rich Trott) [#4893](https://github.com/nodejs/node/pull/4893)
* [[`5c4960468a`](https://github.com/nodejs/node/commit/5c4960468a)] - **tls**: nullify `.ssl` on handle close (Fedor Indutny) [#5168](https://github.com/nodejs/node/pull/5168)
* [[`c0f5f01c9c`](https://github.com/nodejs/node/commit/c0f5f01c9c)] - **tls**: scope loop vars with let (Rich Trott) [#4853](https://github.com/nodejs/node/pull/4853)
* [[`c86627e0d1`](https://github.com/nodejs/node/commit/c86627e0d1)] - **(SEMVER-MINOR)** **tls**: add `options` argument to createSecurePair (Коренберг Марк) [#2441](https://github.com/nodejs/node/pull/2441)
* [[`c908ff36f4`](https://github.com/nodejs/node/commit/c908ff36f4)] - **tls_wrap**: reach error reporting for UV_EPROTO (Fedor Indutny) [#4885](https://github.com/nodejs/node/pull/4885)
* [[`cebe3b95e3`](https://github.com/nodejs/node/commit/cebe3b95e3)] - **tools**: run tick processor without forking (Matt Loring) [#4224](https://github.com/nodejs/node/pull/4224)
* [[`70d8827714`](https://github.com/nodejs/node/commit/70d8827714)] - **(SEMVER-MINOR)** **tools**: add --prof-process flag to node binary (Matt Loring) [#4021](https://github.com/nodejs/node/pull/4021)
* [[`a43b9291c7`](https://github.com/nodejs/node/commit/a43b9291c7)] - **tools**: replace obsolete ESLint rules (Rich Trott) [#5214](https://github.com/nodejs/node/pull/5214)
* [[`a89c6f58f1`](https://github.com/nodejs/node/commit/a89c6f58f1)] - **tools**: update ESLint to version 2.1.0 (Rich Trott) [#5214](https://github.com/nodejs/node/pull/5214)
* [[`789f62196a`](https://github.com/nodejs/node/commit/789f62196a)] - **tools**: remove obsolete lint rules (Rich Trott) [#5214](https://github.com/nodejs/node/pull/5214)
* [[`154772cfa8`](https://github.com/nodejs/node/commit/154772cfa8)] - **tools**: parse types into links in doc html gen (Claudio Rodriguez) [#4741](https://github.com/nodejs/node/pull/4741)
* [[`9237b6e38a`](https://github.com/nodejs/node/commit/9237b6e38a)] - **tools**: fix warning in doc parsing (Shigeki Ohtsu) [#4537](https://github.com/nodejs/node/pull/4537)
* [[`c653cc0c03`](https://github.com/nodejs/node/commit/c653cc0c03)] - **tools**: add recommended ES6 lint rules (Rich Trott) [#5210](https://github.com/nodejs/node/pull/5210)
* [[`993d9b7df0`](https://github.com/nodejs/node/commit/993d9b7df0)] - **tools**: add recommended linting rules (Rich Trott) [#5188](https://github.com/nodejs/node/pull/5188)
* [[`8423125223`](https://github.com/nodejs/node/commit/8423125223)] - **tools**: remove excessive comments from .eslintrc (Rich Trott) [#5151](https://github.com/nodejs/node/pull/5151)
* [[`4c687c98e4`](https://github.com/nodejs/node/commit/4c687c98e4)] - **tools**: enable no-proto rule for linter (Jackson Tian) [#5140](https://github.com/nodejs/node/pull/5140)
* [[`28e4e6f312`](https://github.com/nodejs/node/commit/28e4e6f312)] - **tools**: disallow mixed spaces and tabs for indents (Rich Trott) [#5135](https://github.com/nodejs/node/pull/5135)
* [[`50c6fe8604`](https://github.com/nodejs/node/commit/50c6fe8604)] - **tools**: alphabetize eslint stylistic issues section (Rich Trott)
* [[`ee594f1ed7`](https://github.com/nodejs/node/commit/ee594f1ed7)] - **tools**: lint for empty character classes in regex (Rich Trott) [#5115](https://github.com/nodejs/node/pull/5115)
* [[`bf0e239e99`](https://github.com/nodejs/node/commit/bf0e239e99)] - **tools**: lint for spacing around unary operators (Rich Trott) [#5063](https://github.com/nodejs/node/pull/5063)
* [[`6345acb792`](https://github.com/nodejs/node/commit/6345acb792)] - **tools**: enable no-redeclare rule for linter (Rich Trott) [#5047](https://github.com/nodejs/node/pull/5047)
* [[`1dae175b62`](https://github.com/nodejs/node/commit/1dae175b62)] - **tools**: fix redeclared vars in doc/json.js (Rich Trott) [#5047](https://github.com/nodejs/node/pull/5047)
* [[`d1d220a1cf`](https://github.com/nodejs/node/commit/d1d220a1cf)] - **tools**: apply linting to doc tools (Rich Trott) [#4973](https://github.com/nodejs/node/pull/4973)
* [[`eddde1f60c`](https://github.com/nodejs/node/commit/eddde1f60c)] - **tools**: fix detecting constructor for JSON doc (Timothy Gu) [#4966](https://github.com/nodejs/node/pull/4966)
* [[`bcb327c8dd`](https://github.com/nodejs/node/commit/bcb327c8dd)] - **tools**: add property types in JSON documentation (Timothy Gu) [#4884](https://github.com/nodejs/node/pull/4884)
* [[`9a06a4c116`](https://github.com/nodejs/node/commit/9a06a4c116)] - **tools**: enable assorted ESLint error rules (Roman Reiss) [#4864](https://github.com/nodejs/node/pull/4864)
* [[`38474cfd49`](https://github.com/nodejs/node/commit/38474cfd49)] - **tools**: add arrow function rules to eslint (cjihrig) [#4813](https://github.com/nodejs/node/pull/4813)
* [[`f898abaa4f`](https://github.com/nodejs/node/commit/f898abaa4f)] - **tools**: fix setting path containing an ampersand (Brian White) [#4804](https://github.com/nodejs/node/pull/4804)
* [[`d10bee8e79`](https://github.com/nodejs/node/commit/d10bee8e79)] - **tools**: enable no-extra-semi rule in eslint (Michaël Zasso) [#2205](https://github.com/nodejs/node/pull/2205)
* [[`01006392cf`](https://github.com/nodejs/node/commit/01006392cf)] - **tools,doc**: fix linting errors (Rich Trott) [#5161](https://github.com/nodejs/node/pull/5161)
* [[`57a5f8731a`](https://github.com/nodejs/node/commit/57a5f8731a)] - **url**: change scoping of variables with let (Kári Tristan Helgason) [#4867](https://github.com/nodejs/node/pull/4867)

## 2016-03-02, Version 4.3.2 'Argon' (LTS), @thealphanerd

This is a security release with only a single commit, an update to openssl due to a recent security advisory. You can read more about the security advisory on [the Node.js website](https://nodejs.org/en/blog/vulnerability/openssl-march-2016/)

### Notable changes

* **openssl**: Upgrade from 1.0.2f to 1.0.2g (Ben Noordhuis) [#5507](https://github.com/nodejs/node/pull/5507)
  - Fix a double-free defect in parsing malformed DSA keys that may potentially be used for DoS or memory corruption attacks. It is likely to be very difficult to use this defect for a practical attack and is therefore considered low severity for Node.js users. More info is available at [CVE-2016-0705](https://www.openssl.org/news/vulnerabilities.html#2016-0705).
  - Fix a defect that can cause memory corruption in certain very rare cases relating to the internal `BN_hex2bn()` and `BN_dec2bn()` functions. It is believed that Node.js is not invoking the code paths that use these functions so practical attacks via Node.js using this defect are _unlikely_ to be possible. More info is available at [CVE-2016-0797](https://www.openssl.org/news/vulnerabilities.html#2016-0797).
  - Fix a defect that makes the _[CacheBleed Attack](https://ssrg.nicta.com.au/projects/TS/cachebleed/)_ possible. This defect enables attackers to execute side-channel attacks leading to the potential recovery of entire RSA private keys. It only affects the Intel Sandy Bridge (and possibly older) microarchitecture when using hyper-threading. Newer microarchitectures, including Haswell, are unaffected. More info is available at [CVE-2016-0702](https://www.openssl.org/news/vulnerabilities.html#2016-0702).
  
## Commits

* [[`c133797d09`](https://github.com/nodejs/node/commit/c133797d09)] - **deps**: upgrade openssl to 1.0.2g (Ben Noordhuis) [#5507](https://github.com/nodejs/node/pull/5507)

## 2016-02-16, Version 4.3.1 'Argon' (LTS), @thealphanerd

### Notable changes

* **buffer**
  * make byteLength work with Buffer correctly (Jackson Tian)
    - [#4738](https://github.com/nodejs/node/pull/4738)
* **debugger** 
  * guard against call from non-node context (Ben Noordhuis)
    - [#4328](https://github.com/nodejs/node/pull/4328)
    - fixes segfaults in debugger
  * do not incept debug context (Myles Borins)
    - [#4819](https://github.com/nodejs/node/pull/4819)
    - fixes crash in debugger when using util methods
* **deps**
  * update to http-parser 2.5.2 (James Snell)
    - [#5238](https://github.com/nodejs/node/pull/5238)

### Commits

* [[`748d2b4de1`](https://github.com/nodejs/node/commit/748d2b4de1)] - **buffer**: make byteLength work with Buffer correctly (Jackson Tian) [#4738](https://github.com/nodejs/node/pull/4738)
* [[`fb615bdaf4`](https://github.com/nodejs/node/commit/fb615bdaf4)] - **buffer**: remove unnecessary TODO comments (Peter Geiss) [#4719](https://github.com/nodejs/node/pull/4719)
* [[`b8213ba7e1`](https://github.com/nodejs/node/commit/b8213ba7e1)] - **cluster**: ignore queryServer msgs on disconnection (Santiago Gimeno) [#4465](https://github.com/nodejs/node/pull/4465)
* [[`f8a676ed59`](https://github.com/nodejs/node/commit/f8a676ed59)] - **cluster**: fix race condition setting suicide prop (Santiago Gimeno) [#4349](https://github.com/nodejs/node/pull/4349)
* [[`9d4a226dad`](https://github.com/nodejs/node/commit/9d4a226dad)] - **crypto**: clear error stack in ECDH::Initialize (Fedor Indutny) [#4689](https://github.com/nodejs/node/pull/4689)
* [[`583f3347d8`](https://github.com/nodejs/node/commit/583f3347d8)] - **debugger**: remove variable redeclarations (Rich Trott) [#4633](https://github.com/nodejs/node/pull/4633)
* [[`667f7a7ab3`](https://github.com/nodejs/node/commit/667f7a7ab3)] - **debugger**: guard against call from non-node context (Ben Noordhuis) [#4328](https://github.com/nodejs/node/pull/4328)
* [[`188cff3c31`](https://github.com/nodejs/node/commit/188cff3c31)] - **deps**: update to http-parser 2.5.2 (James Snell) [#5238](https://github.com/nodejs/node/pull/5238)
* [[`6e829b44e3`](https://github.com/nodejs/node/commit/6e829b44e3)] - **dgram**: prevent disabled optimization of bind() (Brian White) [#4613](https://github.com/nodejs/node/pull/4613)
* [[`c3956d05b1`](https://github.com/nodejs/node/commit/c3956d05b1)] - **doc**: update list of personal traits in CoC (Kat Marchán) [#4801](https://github.com/nodejs/node/pull/4801)
* [[`39cb69ca21`](https://github.com/nodejs/node/commit/39cb69ca21)] - **doc**: style fixes for the TOC (Roman Reiss) [#4748](https://github.com/nodejs/node/pull/4748)
* [[`cb5986da81`](https://github.com/nodejs/node/commit/cb5986da81)] - **doc**: add `servername` parameter docs (Alexander Makarenko) [#4729](https://github.com/nodejs/node/pull/4729)
* [[`91066b5f34`](https://github.com/nodejs/node/commit/91066b5f34)] - **doc**: update branch-diff arguments in release doc (Rod Vagg) [#4691](https://github.com/nodejs/node/pull/4691)
* [[`9ca24de41d`](https://github.com/nodejs/node/commit/9ca24de41d)] - **doc**: add docs for more stream options (zoubin) [#4639](https://github.com/nodejs/node/pull/4639)
* [[`437d0e336d`](https://github.com/nodejs/node/commit/437d0e336d)] - **doc**: mention that http.Server inherits from net.Server (Ryan Sobol) [#4455](https://github.com/nodejs/node/pull/4455)
* [[`393e569160`](https://github.com/nodejs/node/commit/393e569160)] - **doc**: copyedit setTimeout() documentation (Rich Trott) [#4434](https://github.com/nodejs/node/pull/4434)
* [[`e2a682ecc3`](https://github.com/nodejs/node/commit/e2a682ecc3)] - **doc**: fix formatting in process.markdown (Rich Trott) [#4433](https://github.com/nodejs/node/pull/4433)
* [[`75b0ea85bd`](https://github.com/nodejs/node/commit/75b0ea85bd)] - **doc**: add path property to Write/ReadStream in fs.markdown (Claudio Rodriguez) [#4368](https://github.com/nodejs/node/pull/4368)
* [[`48c2783421`](https://github.com/nodejs/node/commit/48c2783421)] - **doc**: add docs working group (Bryan English) [#4244](https://github.com/nodejs/node/pull/4244)
* [[`c0432e9f56`](https://github.com/nodejs/node/commit/c0432e9f56)] - **doc**: restore ICU third-party software licenses (Richard Lau) [#4762](https://github.com/nodejs/node/pull/4762)
* [[`36a4159dab`](https://github.com/nodejs/node/commit/36a4159dab)] - **doc**: rebuild LICENSE using tools/license-builder.sh (Rod Vagg) [#4194](https://github.com/nodejs/node/pull/4194)
* [[`a2998a1bce`](https://github.com/nodejs/node/commit/a2998a1bce)] - **gitignore**: never ignore debug module (Michaël Zasso) [#2286](https://github.com/nodejs/node/pull/2286)
* [[`661b2557d9`](https://github.com/nodejs/node/commit/661b2557d9)] - **http**: remove variable redeclaration (Rich Trott) [#4612](https://github.com/nodejs/node/pull/4612)
* [[`1bb2967d48`](https://github.com/nodejs/node/commit/1bb2967d48)] - **http**: fix non-string header value concatenation (Brian White) [#4460](https://github.com/nodejs/node/pull/4460)
* [[`15ed64e34c`](https://github.com/nodejs/node/commit/15ed64e34c)] - **lib**: fix style issues after eslint update (Michaël Zasso) [nodejs/io.js#2286](https://github.com/nodejs/io.js/pull/2286)
* [[`2e92a1a6b4`](https://github.com/nodejs/node/commit/2e92a1a6b4)] - **module**: move unnecessary work for early return (Andres Suarez) [#3579](https://github.com/nodejs/node/pull/3579)
* [[`40c8e6d75d`](https://github.com/nodejs/node/commit/40c8e6d75d)] - **net**: remove hot path comment from connect (Evan Lucas) [#4648](https://github.com/nodejs/node/pull/4648)
* [[`8ed0c1c22c`](https://github.com/nodejs/node/commit/8ed0c1c22c)] - **net**: fix dns lookup for android (Josh Dague) [#4580](https://github.com/nodejs/node/pull/4580)
* [[`15fa555204`](https://github.com/nodejs/node/commit/15fa555204)] - **net, doc**: fix line wrapping lint in net.js (James M Snell) [#4588](https://github.com/nodejs/node/pull/4588)
* [[`1b070e48e0`](https://github.com/nodejs/node/commit/1b070e48e0)] - **node_contextify**: do not incept debug context (Myles Borins) [#4815](https://github.com/nodejs/node/issues/4815)
* [[`4fbcb47fe9`](https://github.com/nodejs/node/commit/4fbcb47fe9)] - **readline**: Remove XXX and output debuglog (Kohei TAKATA) [#4690](https://github.com/nodejs/node/pull/4690)
* [[`26f02405d0`](https://github.com/nodejs/node/commit/26f02405d0)] - **repl**: make sure historyPath is trimmed (Evan Lucas) [#4539](https://github.com/nodejs/node/pull/4539)
* [[`5990ba2a0a`](https://github.com/nodejs/node/commit/5990ba2a0a)] - **src**: remove redeclarations of variables (Rich Trott) [#4605](https://github.com/nodejs/node/pull/4605)
* [[`c41ed59dbc`](https://github.com/nodejs/node/commit/c41ed59dbc)] - **src**: don't check failure with ERR_peek_error() (Ben Noordhuis) [#4731](https://github.com/nodejs/node/pull/4731)
* [[`d71f9992f9`](https://github.com/nodejs/node/commit/d71f9992f9)] - **stream**: remove useless if test in transform (zoubin) [#4617](https://github.com/nodejs/node/pull/4617)
* [[`f205e9920e`](https://github.com/nodejs/node/commit/f205e9920e)] - **test**: fix tls-no-rsa-key flakiness (Santiago Gimeno) [#4043](https://github.com/nodejs/node/pull/4043)
* [[`447347cd62`](https://github.com/nodejs/node/commit/447347cd62)] - **test**: fix issues for space-in-parens ESLint rule (Roman Reiss) [#4753](https://github.com/nodejs/node/pull/4753)
* [[`be8274508c`](https://github.com/nodejs/node/commit/be8274508c)] - **test**: improve test-cluster-disconnect-suicide-race (Rich Trott) [#4739](https://github.com/nodejs/node/pull/4739)
* [[`0178001163`](https://github.com/nodejs/node/commit/0178001163)] - **test**: make test-cluster-disconnect-leak reliable (Rich Trott) [#4736](https://github.com/nodejs/node/pull/4736)
* [[`d615757da2`](https://github.com/nodejs/node/commit/d615757da2)] - **test**: fix flaky test-net-socket-local-address (cjihrig) [#4650](https://github.com/nodejs/node/pull/4650)
* [[`baa0a3dff5`](https://github.com/nodejs/node/commit/baa0a3dff5)] - **test**: fix race in test-net-server-pause-on-connect (Rich Trott) [#4637](https://github.com/nodejs/node/pull/4637)
* [[`909b5167cb`](https://github.com/nodejs/node/commit/909b5167cb)] - **test**: remove 1 second delay from test (Rich Trott) [#4616](https://github.com/nodejs/node/pull/4616)
* [[`8ea76608ed`](https://github.com/nodejs/node/commit/8ea76608ed)] - **test**: move resource intensive tests to sequential (Rich Trott) [#4615](https://github.com/nodejs/node/pull/4615)
* [[`7afcdd358e`](https://github.com/nodejs/node/commit/7afcdd358e)] - **test**: require common module only once (Rich Trott) [#4611](https://github.com/nodejs/node/pull/4611)
* [[`0e02eb0bbe`](https://github.com/nodejs/node/commit/0e02eb0bbe)] - **test**: only include http module once (Rich Trott) [#4606](https://github.com/nodejs/node/pull/4606)
* [[`34d9e48bb6`](https://github.com/nodejs/node/commit/34d9e48bb6)] - **test**: fix `http-upgrade-client` flakiness (Santiago Gimeno) [#4602](https://github.com/nodejs/node/pull/4602)
* [[`556703d531`](https://github.com/nodejs/node/commit/556703d531)] - **test**: fix flaky unrefed timers test (Rich Trott) [#4599](https://github.com/nodejs/node/pull/4599)
* [[`3d5bc69796`](https://github.com/nodejs/node/commit/3d5bc69796)] - **test**: fix `http-upgrade-agent` flakiness (Santiago Gimeno) [#4520](https://github.com/nodejs/node/pull/4520)
* [[`ec24d3767b`](https://github.com/nodejs/node/commit/ec24d3767b)] - **test**: fix flaky test-cluster-shared-leak (Rich Trott) [#4510](https://github.com/nodejs/node/pull/4510)
* [[`a256790327`](https://github.com/nodejs/node/commit/a256790327)] - **test**: fix flaky cluster-net-send (Brian White) [#4444](https://github.com/nodejs/node/pull/4444)
* [[`6809c2be1a`](https://github.com/nodejs/node/commit/6809c2be1a)] - **test**: fix flaky child-process-fork-regr-gh-2847 (Brian White) [#4442](https://github.com/nodejs/node/pull/4442)
* [[`e6448aa36b`](https://github.com/nodejs/node/commit/e6448aa36b)] - **test**: use addon.md block headings as test dir names (Rod Vagg) [#4412](https://github.com/nodejs/node/pull/4412)
* [[`305d340fca`](https://github.com/nodejs/node/commit/305d340fca)] - **test**: test each block in addon.md contains js & cc (Rod Vagg) [#4411](https://github.com/nodejs/node/pull/4411)
* [[`f213406575`](https://github.com/nodejs/node/commit/f213406575)] - **test**: fix tls-multi-key race condition (Santiago Gimeno) [#3966](https://github.com/nodejs/node/pull/3966)
* [[`607f545568`](https://github.com/nodejs/node/commit/607f545568)] - **test**: fix style issues after eslint update (Michaël Zasso) [nodejs/io.js#2286](https://github.com/nodejs/io.js/pull/2286)
* [[`aefb20a94f`](https://github.com/nodejs/node/commit/aefb20a94f)] - **tls**: copy client CAs and cert store on CertCb (Fedor Indutny) [#3537](https://github.com/nodejs/node/pull/3537)
* [[`7821b3e305`](https://github.com/nodejs/node/commit/7821b3e305)] - **tls_legacy**: do not read on OpenSSL's stack (Fedor Indutny) [#4624](https://github.com/nodejs/node/pull/4624)
* [[`b66db49f94`](https://github.com/nodejs/node/commit/b66db49f94)] - **tools**: add support for subkeys in release tools (Myles Borins) [#4807](https://github.com/nodejs/node/pull/4807)
* [[`837ebd1985`](https://github.com/nodejs/node/commit/837ebd1985)] - **tools**: enable space-in-parens ESLint rule (Roman Reiss) [#4753](https://github.com/nodejs/node/pull/4753)
* [[`066d5e7da2`](https://github.com/nodejs/node/commit/066d5e7da2)] - **tools**: fix style issue after eslint update (Michaël Zasso) [nodejs/io.js#2286](https://github.com/nodejs/io.js/pull/2286)
* [[`b20ea69f46`](https://github.com/nodejs/node/commit/b20ea69f46)] - **tools**: update eslint config (Michaël Zasso) [nodejs/io.js#2286](https://github.com/nodejs/io.js/pull/2286)
* [[`2e0352d50c`](https://github.com/nodejs/node/commit/2e0352d50c)] - **tools**: update eslint to v1.10.3 (Michaël Zasso) [nodejs/io.js#2286](https://github.com/nodejs/io.js/pull/2286)
* [[`c96800a432`](https://github.com/nodejs/node/commit/c96800a432)] - **tools**: fix license-builder.sh for ICU (Richard Lau) [#4762](https://github.com/nodejs/node/pull/4762)
* [[`720b03dca7`](https://github.com/nodejs/node/commit/720b03dca7)] - **tools**: add license-builder.sh to construct LICENSE (Rod Vagg) [#4194](https://github.com/nodejs/node/pull/4194)

## 2016-02-09, Version 4.3.0 'Argon' (LTS), @jasnell

This is an important security release. All Node.js users should consult the security release summary at nodejs.org for details on patched vulnerabilities.

Note that this release includes a non-backward compatible change to address a security issue. This change increases the version of the LTS v4.x line to v4.3.0. There will be *no further updates* to v4.2.x.

### Notable changes

* **http**: fix defects in HTTP header parsing for requests and responses that can allow request smuggling (CVE-2016-2086) or response splitting (CVE-2016-2216). HTTP header parsing now aligns more closely with the HTTP spec including restricting the acceptable characters.
* **http-parser**: upgrade from 2.5.0 to 2.5.1
* **openssl**: upgrade from 1.0.2e to 1.0.2f. To mitigate against the Logjam attack, TLS clients now reject Diffie-Hellman handshakes with parameters shorter than 1024-bits, up from the previous limit of 768-bits.
* **src**:
  - introduce new `--security-revert={cvenum}` command line flag for selective reversion of specific CVE fixes
  - allow the fix for CVE-2016-2216 to be selectively reverted using `--security-revert=CVE-2016-2216`

### Commits

* [[`d94f864abd`](https://github.com/nodejs/node/commit/d94f864abd0933c125afeb84b6f72ec709c63b43)] - **deps**: add -no_rand_screen to openssl s_client (Shigeki Ohtsu) [#1836](https://github.com/nodejs/node/pull/1836)
* [[`136295e202`](https://github.com/nodejs/node/commit/136295e202)] - **deps**: upgrade openssl sources to 1.0.2f (Myles Borins) [#4961](https://github.com/nodejs/node/pull/4961)
* [[`0eae95eae3`](https://github.com/nodejs/node/commit/0eae95eae3)] - **(SEMVER-MINOR)** **deps**: update http-parser to version 2.5.1 (James M Snell)
* [[`cf2b714b02`](https://github.com/nodejs/node/commit/cf2b714b02)] - **(SEMVER-MINOR)** **http**: strictly forbid invalid characters from headers (James M Snell)
* [[`49ae2e0334`](https://github.com/nodejs/node/commit/49ae2e0334)] - **src**: avoid compiler warning in node_revert.cc (James M Snell)
* [[`da3750f981`](https://github.com/nodejs/node/commit/da3750f981)] - **(SEMVER-MAJOR)** **src**: add --security-revert command line flag (James M Snell)

## 2016-01-21, Version 4.2.6 'Argon' (LTS), @TheAlphaNerd

### Notable changes

* Fix regression in debugger and profiler functionality

### Known issues

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`1408f7abb1`](https://github.com/nodejs/node/commit/1408f7abb1)] - **module,src**: do not wrap modules with -1 lineOffset (cjihrig) [#4298](https://github.com/nodejs/node/pull/4298)
* [[`1f8e1472cc`](https://github.com/nodejs/node/commit/1f8e1472cc)] - **test**: add test for debugging one line files (cjihrig) [#4298](https://github.com/nodejs/node/pull/4298)


## 2016-01-20, Version 4.2.5 'Argon' (LTS), @TheAlphaNerd

Maintenance update.

### Notable changes

* **assert**
  * accommodate ES6 classes that extend Error (Rich Trott) [#4166](https://github.com/nodejs/node/pull/4166)
* **build**
  * add "--partly-static" build options (Super Zheng) [#4152](https://github.com/nodejs/node/pull/4152)
* **deps**
  * backport 066747e from upstream V8 (Ali Ijaz Sheikh) [#4655](https://github.com/nodejs/node/pull/4655)
  * backport 200315c from V8 upstream (Vladimir Kurchatkin) [#4128](https://github.com/nodejs/node/pull/4128)
  * upgrade libuv to 1.8.0 (Saúl Ibarra Corretgé)
* **docs**
  * various updates landed in 70 different commits!
* **repl**
  * attach location info to syntax errors (cjihrig) [#4013](https://github.com/nodejs/node/pull/4013)
  * display error message when loading directory (Prince J Wesley) [#4170](https://github.com/nodejs/node/pull/4170)
* **tests**
  * various updates landed in over 50 commits
* **tools**
  * add tap output to cpplint (Johan Bergström) [#3448](https://github.com/nodejs/node/pull/3448)
* **util**
  * allow lookup of hidden values (cjihrig) [#3988](https://github.com/nodejs/node/pull/3988)

### Known issues

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits


* [[`87181cd74c`](https://github.com/nodejs/node/commit/87181cd74c)] - **assert**: accommodate ES6 classes that extend Error (Rich Trott) [#4166](https://github.com/nodejs/node/pull/4166)
* [[`901172a783`](https://github.com/nodejs/node/commit/901172a783)] - **assert**: typed array deepequal performance fix (Claudio Rodriguez) [#4330](https://github.com/nodejs/node/pull/4330)
* [[`55336810ee`](https://github.com/nodejs/node/commit/55336810ee)] - **async_wrap**: call callback in destructor (Trevor Norris) [#3461](https://github.com/nodejs/node/pull/3461)
* [[`a8b45e9e96`](https://github.com/nodejs/node/commit/a8b45e9e96)] - **async_wrap**: new instances get uid (Trevor Norris) [#3461](https://github.com/nodejs/node/pull/3461)
* [[`49f16d77c4`](https://github.com/nodejs/node/commit/49f16d77c4)] - **async_wrap**: allow some hooks to be optional (Trevor Norris) [#3461](https://github.com/nodejs/node/pull/3461)
* [[`44ee33f945`](https://github.com/nodejs/node/commit/44ee33f945)] - **buffer**: refactor create buffer (Jackson Tian) [#4340](https://github.com/nodejs/node/pull/4340)
* [[`138d004ac0`](https://github.com/nodejs/node/commit/138d004ac0)] - **buffer**: faster case for create Buffer from new Buffer(0) (Jackson Tian) [#4326](https://github.com/nodejs/node/pull/4326)
* [[`c6dc2a1609`](https://github.com/nodejs/node/commit/c6dc2a1609)] - **buffer**: Prevent Buffer constructor deopt (Bryce Baril) [#4158](https://github.com/nodejs/node/pull/4158)
* [[`a320045e68`](https://github.com/nodejs/node/commit/a320045e68)] - **buffer**: default to UTF8 in byteLength() (Tom Gallacher) [#4010](https://github.com/nodejs/node/pull/4010)
* [[`c5f71ac771`](https://github.com/nodejs/node/commit/c5f71ac771)] - **build**: add "--partly-static" build options (Super Zheng) [#4152](https://github.com/nodejs/node/pull/4152)
* [[`e6c25335ea`](https://github.com/nodejs/node/commit/e6c25335ea)] - **build**: omit -gline-tables-only for --enable-asan (Ben Noordhuis) [#3680](https://github.com/nodejs/node/pull/3680)
* [[`80b4ba286c`](https://github.com/nodejs/node/commit/80b4ba286c)] - **build**: Updates for AIX npm support - part 1 (Michael Dawson) [#3114](https://github.com/nodejs/node/pull/3114)
* [[`35e32985ca`](https://github.com/nodejs/node/commit/35e32985ca)] - **child_process**: guard against race condition (Rich Trott) [#4418](https://github.com/nodejs/node/pull/4418)
* [[`48564204f0`](https://github.com/nodejs/node/commit/48564204f0)] - **child_process**: flush consuming streams (Dave) [#4071](https://github.com/nodejs/node/pull/4071)
* [[`481d59a74c`](https://github.com/nodejs/node/commit/481d59a74c)] - **configure**: fix arm vfpv2 (Jörg Krause) [#4203](https://github.com/nodejs/node/pull/4203)
* [[`d19da6638d`](https://github.com/nodejs/node/commit/d19da6638d)] - **crypto**: load PFX chain the same way as regular one (Fedor Indutny) [#4165](https://github.com/nodejs/node/pull/4165)
* [[`b8e75de1f3`](https://github.com/nodejs/node/commit/b8e75de1f3)] - **crypto**: fix native module compilation with FIPS (Stefan Budeanu) [#4023](https://github.com/nodejs/node/pull/4023)
* [[`b7c3fb7f75`](https://github.com/nodejs/node/commit/b7c3fb7f75)] - **crypto**: disable crypto.createCipher in FIPS mode (Stefan Budeanu) [#3754](https://github.com/nodejs/node/pull/3754)
* [[`31b4091a1e`](https://github.com/nodejs/node/commit/31b4091a1e)] - **debugger**: also exit when the repl emits 'exit' (Felix Böhm) [#2369](https://github.com/nodejs/node/pull/2369)
* [[`9baa5618f5`](https://github.com/nodejs/node/commit/9baa5618f5)] - **deps**: backport 066747e from upstream V8 (Ali Ijaz Sheikh) [#4655](https://github.com/nodejs/node/pull/4655)
* [[`c3a9d8a62e`](https://github.com/nodejs/node/commit/c3a9d8a62e)] - **deps**: backport 200315c from V8 upstream (Vladimir Kurchatkin) [#4128](https://github.com/nodejs/node/pull/4128)
* [[`1ebb0c0fdf`](https://github.com/nodejs/node/commit/1ebb0c0fdf)] - **deps**: upgrade libuv to 1.8.0 (Saúl Ibarra Corretgé) [#4276](https://github.com/nodejs/node/pull/4276)
* [[`253fe3e7c8`](https://github.com/nodejs/node/commit/253fe3e7c8)] - **dns**: remove nonexistant exports.ADNAME (Roman Reiss) [#3051](https://github.com/nodejs/node/pull/3051)
* [[`8c2b65ad82`](https://github.com/nodejs/node/commit/8c2b65ad82)] - **doc**: clarify protocol default in http.request() (cjihrig) [#4714](https://github.com/nodejs/node/pull/4714)
* [[`33e72e135f`](https://github.com/nodejs/node/commit/33e72e135f)] - **doc**: update links to use https where possible (jpersson) [#4054](https://github.com/nodejs/node/pull/4054)
* [[`5f4aa79410`](https://github.com/nodejs/node/commit/5f4aa79410)] - **doc**: clarify explanation of first stream section (Vitor Cortez) [#4234](https://github.com/nodejs/node/pull/4234)
* [[`295ca5bfb2`](https://github.com/nodejs/node/commit/295ca5bfb2)] - **doc**: add branch-diff example to releases.md (Myles Borins) [#4636](https://github.com/nodejs/node/pull/4636)
* [[`18f5cd8710`](https://github.com/nodejs/node/commit/18f5cd8710)] - **doc**: update stylesheet to match frontpage (Roman Reiss) [#4621](https://github.com/nodejs/node/pull/4621)
* [[`2f40715f08`](https://github.com/nodejs/node/commit/2f40715f08)] - **doc**: adds usage of readline line-by-line parsing (Robert Jefe Lindstaedt) [#4609](https://github.com/nodejs/node/pull/4609)
* [[`5b45a464ee`](https://github.com/nodejs/node/commit/5b45a464ee)] - **doc**: document http's server.listen return value (Sequoia McDowell) [#4590](https://github.com/nodejs/node/pull/4590)
* [[`bd31740339`](https://github.com/nodejs/node/commit/bd31740339)] - **doc**: label http.IncomingMessage as a Class (Sequoia McDowell) [#4589](https://github.com/nodejs/node/pull/4589)
* [[`bcd2cbbb93`](https://github.com/nodejs/node/commit/bcd2cbbb93)] - **doc**: fix description about the latest-codename (Minwoo Jung) [#4583](https://github.com/nodejs/node/pull/4583)
* [[`0b12bcb35d`](https://github.com/nodejs/node/commit/0b12bcb35d)] - **doc**: add Evan Lucas to Release Team (Evan Lucas) [#4579](https://github.com/nodejs/node/pull/4579)
* [[`e20b1f6f10`](https://github.com/nodejs/node/commit/e20b1f6f10)] - **doc**: add Myles Borins to Release Team (Myles Borins) [#4578](https://github.com/nodejs/node/pull/4578)
* [[`54977e63eb`](https://github.com/nodejs/node/commit/54977e63eb)] - **doc**: add missing backtick for readline (Brian White) [#4549](https://github.com/nodejs/node/pull/4549)
* [[`5d6bed895c`](https://github.com/nodejs/node/commit/5d6bed895c)] - **doc**: bring releases.md up to date (cjihrig) [#4540](https://github.com/nodejs/node/pull/4540)
* [[`0cd2252e85`](https://github.com/nodejs/node/commit/0cd2252e85)] - **doc**: fix numbering in stream.markdown (Richard Sun) [#4538](https://github.com/nodejs/node/pull/4538)
* [[`8574d91f27`](https://github.com/nodejs/node/commit/8574d91f27)] - **doc**: stronger suggestion for userland assert (Wyatt Preul) [#4535](https://github.com/nodejs/node/pull/4535)
* [[`a7bcf8b84d`](https://github.com/nodejs/node/commit/a7bcf8b84d)] - **doc**: close backtick in process.title description (Dave) [#4534](https://github.com/nodejs/node/pull/4534)
* [[`0ceb3148b0`](https://github.com/nodejs/node/commit/0ceb3148b0)] - **doc**: improvements to events.markdown copy (James M Snell) [#4468](https://github.com/nodejs/node/pull/4468)
* [[`bf56d509b9`](https://github.com/nodejs/node/commit/bf56d509b9)] - **doc**: explain ClientRequest#setTimeout time unit (Ben Ripkens) [#4458](https://github.com/nodejs/node/pull/4458)
* [[`d927c51be3`](https://github.com/nodejs/node/commit/d927c51be3)] - **doc**: improvements to errors.markdown copy (James M Snell) [#4454](https://github.com/nodejs/node/pull/4454)
* [[`ceea6df581`](https://github.com/nodejs/node/commit/ceea6df581)] - **doc**: improvements to dns.markdown copy (James M Snell) [#4449](https://github.com/nodejs/node/pull/4449)
* [[`506f2f8ed1`](https://github.com/nodejs/node/commit/506f2f8ed1)] - **doc**: add anchors for _transform _flush _writev in stream.markdown (iamchenxin) [#4448](https://github.com/nodejs/node/pull/4448)
* [[`74bcad0b78`](https://github.com/nodejs/node/commit/74bcad0b78)] - **doc**: improvements to dgram.markdown copy (James M Snell) [#4437](https://github.com/nodejs/node/pull/4437)
* [[`e244d560c9`](https://github.com/nodejs/node/commit/e244d560c9)] - **doc**: improvements to debugger.markdown copy (James M Snell) [#4436](https://github.com/nodejs/node/pull/4436)
* [[`df7e1281a5`](https://github.com/nodejs/node/commit/df7e1281a5)] - **doc**: improvements to console.markdown copy (James M Snell) [#4428](https://github.com/nodejs/node/pull/4428)
* [[`abb17cc6c1`](https://github.com/nodejs/node/commit/abb17cc6c1)] - **doc**: fix spelling error in lib/url.js comment (Nik Nyby) [#4390](https://github.com/nodejs/node/pull/4390)
* [[`823269db2d`](https://github.com/nodejs/node/commit/823269db2d)] - **doc**: improve assert.markdown copy (James M Snell) [#4360](https://github.com/nodejs/node/pull/4360)
* [[`2b1804f6cb`](https://github.com/nodejs/node/commit/2b1804f6cb)] - **doc**: copyedit releases.md (Rich Trott) [#4384](https://github.com/nodejs/node/pull/4384)
* [[`2b142fd876`](https://github.com/nodejs/node/commit/2b142fd876)] - **doc**: catch the WORKING_GROUPS.md bootstrap docs up to date (James M Snell) [#4367](https://github.com/nodejs/node/pull/4367)
* [[`ed87873de3`](https://github.com/nodejs/node/commit/ed87873de3)] - **doc**: fix link in addons.markdown (Nicholas Young) [#4331](https://github.com/nodejs/node/pull/4331)
* [[`fe693b7a4f`](https://github.com/nodejs/node/commit/fe693b7a4f)] - **doc**: Typo in buffer.markdown referencing buf.write() (chrisjohn404) [#4324](https://github.com/nodejs/node/pull/4324)
* [[`764df2166e`](https://github.com/nodejs/node/commit/764df2166e)] - **doc**: document the cache parameter for fs.realpathSync (Jackson Tian) [#4285](https://github.com/nodejs/node/pull/4285)
* [[`61f91b2f29`](https://github.com/nodejs/node/commit/61f91b2f29)] - **doc**: fix, modernize examples in docs (James M Snell) [#4282](https://github.com/nodejs/node/pull/4282)
* [[`d87ad302ce`](https://github.com/nodejs/node/commit/d87ad302ce)] - **doc**: clarify error events in HTTP module documentation (Lenny Markus) [#4275](https://github.com/nodejs/node/pull/4275)
* [[`7983577e41`](https://github.com/nodejs/node/commit/7983577e41)] - **doc**: fix improper http.get sample code (Hideki Yamamura) [#4263](https://github.com/nodejs/node/pull/4263)
* [[`6c30d087e5`](https://github.com/nodejs/node/commit/6c30d087e5)] - **doc**: Fixing broken links to the v8 wiki (Tom Gallacher) [#4241](https://github.com/nodejs/node/pull/4241)
* [[`cf214e56e4`](https://github.com/nodejs/node/commit/cf214e56e4)] - **doc**: move description of 'equals' method to right place (janriemer) [#4227](https://github.com/nodejs/node/pull/4227)
* [[`fb8e8dbb92`](https://github.com/nodejs/node/commit/fb8e8dbb92)] - **doc**: copyedit console doc (Rich Trott) [#4225](https://github.com/nodejs/node/pull/4225)
* [[`4ccf04c229`](https://github.com/nodejs/node/commit/4ccf04c229)] - **doc**: add mcollina to collaborators (Matteo Collina) [#4220](https://github.com/nodejs/node/pull/4220)
* [[`59654c21d4`](https://github.com/nodejs/node/commit/59654c21d4)] - **doc**: add rmg to collaborators (Ryan Graham) [#4219](https://github.com/nodejs/node/pull/4219)
* [[`bfe1a6bd2b`](https://github.com/nodejs/node/commit/bfe1a6bd2b)] - **doc**: add calvinmetcalf to collaborators (Calvin Metcalf) [#4218](https://github.com/nodejs/node/pull/4218)
* [[`5140c404ae`](https://github.com/nodejs/node/commit/5140c404ae)] - **doc**: harmonize description of `ca` argument (Ben Noordhuis) [#4213](https://github.com/nodejs/node/pull/4213)
* [[`2e642051cf`](https://github.com/nodejs/node/commit/2e642051cf)] - **doc**: copyedit child_process doc (Rich Trott) [#4188](https://github.com/nodejs/node/pull/4188)
* [[`7920f8dbde`](https://github.com/nodejs/node/commit/7920f8dbde)] - **doc**: copyedit buffer doc (Rich Trott) [#4187](https://github.com/nodejs/node/pull/4187)
* [[`c35a409cbe`](https://github.com/nodejs/node/commit/c35a409cbe)] - **doc**: clarify assert.fail doc (Rich Trott) [#4186](https://github.com/nodejs/node/pull/4186)
* [[`6235fdf72e`](https://github.com/nodejs/node/commit/6235fdf72e)] - **doc**: copyedit addons doc (Rich Trott) [#4185](https://github.com/nodejs/node/pull/4185)
* [[`990e7ff93e`](https://github.com/nodejs/node/commit/990e7ff93e)] - **doc**: update AUTHORS list (Rod Vagg) [#4183](https://github.com/nodejs/node/pull/4183)
* [[`8d676ef55e`](https://github.com/nodejs/node/commit/8d676ef55e)] - **doc**: change references from node to Node.js (Roman Klauke) [#4177](https://github.com/nodejs/node/pull/4177)
* [[`1c34b139a2`](https://github.com/nodejs/node/commit/1c34b139a2)] - **doc**: add brief Node.js overview to README (wurde) [#4174](https://github.com/nodejs/node/pull/4174)
* [[`27b9b72ab0`](https://github.com/nodejs/node/commit/27b9b72ab0)] - **doc**: add iarna to collaborators (Rebecca Turner) [#4144](https://github.com/nodejs/node/pull/4144)
* [[`683d8dd564`](https://github.com/nodejs/node/commit/683d8dd564)] - **doc**: add JungMinu to collaborators (Minwoo Jung) [#4143](https://github.com/nodejs/node/pull/4143)
* [[`17b06dfa94`](https://github.com/nodejs/node/commit/17b06dfa94)] - **doc**: add zkat to collaborators (Kat Marchán) [#4142](https://github.com/nodejs/node/pull/4142)
* [[`39364c4c72`](https://github.com/nodejs/node/commit/39364c4c72)] - **doc**: improve child_process.markdown wording (yorkie) [#4138](https://github.com/nodejs/node/pull/4138)
* [[`abe452835f`](https://github.com/nodejs/node/commit/abe452835f)] - **doc**: url.format - true slash postfix behaviour (fansworld-claudio) [#4119](https://github.com/nodejs/node/pull/4119)
* [[`6dd375cfe2`](https://github.com/nodejs/node/commit/6dd375cfe2)] - **doc**: document backlog for server.listen() variants (Jan Schär) [#4025](https://github.com/nodejs/node/pull/4025)
* [[`b71a3b363a`](https://github.com/nodejs/node/commit/b71a3b363a)] - **doc**: fixup socket.remoteAddress (Arthur Gautier) [#4198](https://github.com/nodejs/node/pull/4198)
* [[`e2fe214857`](https://github.com/nodejs/node/commit/e2fe214857)] - **doc**: add links and backticks around names (jpersson) [#4054](https://github.com/nodejs/node/pull/4054)
* [[`bb158f8aed`](https://github.com/nodejs/node/commit/bb158f8aed)] - **doc**: s/node.js/Node.js in readme (Rod Vagg) [#3998](https://github.com/nodejs/node/pull/3998)
* [[`f55491ad47`](https://github.com/nodejs/node/commit/f55491ad47)] - **doc**: move fs.existsSync() deprecation message (Martin Forsberg) [#3942](https://github.com/nodejs/node/pull/3942)
* [[`8c5b847f5b`](https://github.com/nodejs/node/commit/8c5b847f5b)] - **doc**: Describe FIPSDIR environment variable (Stefan Budeanu) [#3752](https://github.com/nodejs/node/pull/3752)
* [[`70c95ea0e5`](https://github.com/nodejs/node/commit/70c95ea0e5)] - **doc**: add warning about Windows process groups (Roman Klauke) [#3681](https://github.com/nodejs/node/pull/3681)
* [[`46c59b7256`](https://github.com/nodejs/node/commit/46c59b7256)] - **doc**: add CTC meeting minutes 2015-10-28 (Rod Vagg) [#3661](https://github.com/nodejs/node/pull/3661)
* [[`7ffd299a1d`](https://github.com/nodejs/node/commit/7ffd299a1d)] - **doc**: add final full stop in CONTRIBUTING.md (Emily Aviva Kapor-Mater) [#3576](https://github.com/nodejs/node/pull/3576)
* [[`1f78bff7ce`](https://github.com/nodejs/node/commit/1f78bff7ce)] - **doc**: add TSC meeting minutes 2015-10-21 (Rod Vagg) [#3480](https://github.com/nodejs/node/pull/3480)
* [[`2e623ff024`](https://github.com/nodejs/node/commit/2e623ff024)] - **doc**: add TSC meeting minutes 2015-10-14 (Rod Vagg) [#3463](https://github.com/nodejs/node/pull/3463)
* [[`b9c69964bb`](https://github.com/nodejs/node/commit/b9c69964bb)] - **doc**: add TSC meeting minutes 2015-10-07 (Rod Vagg) [#3364](https://github.com/nodejs/node/pull/3364)
* [[`f31d23c724`](https://github.com/nodejs/node/commit/f31d23c724)] - **doc**: add TSC meeting minutes 2015-09-30 (Rod Vagg) [#3235](https://github.com/nodejs/node/pull/3235)
* [[`ae8e3af178`](https://github.com/nodejs/node/commit/ae8e3af178)] - **doc**: update irc channels: #node.js and #node-dev (Nelson Pecora) [#2743](https://github.com/nodejs/node/pull/2743)
* [[`830caeb1bd`](https://github.com/nodejs/node/commit/830caeb1bd)] - **doc, test**: symbols as event names (Bryan English) [#4151](https://github.com/nodejs/node/pull/4151)
* [[`82cbfcdcbe`](https://github.com/nodejs/node/commit/82cbfcdcbe)] - **docs**: update gpg key for Myles Borins (Myles Borins) [#4657](https://github.com/nodejs/node/pull/4657)
* [[`50b72aa5a3`](https://github.com/nodejs/node/commit/50b72aa5a3)] - **docs**: fix npm command in releases.md (Myles Borins) [#4656](https://github.com/nodejs/node/pull/4656)
* [[`5bf56882e1`](https://github.com/nodejs/node/commit/5bf56882e1)] - **fs,doc**: use `target` instead of `destination` (yorkie) [#3912](https://github.com/nodejs/node/pull/3912)
* [[`41fcda840c`](https://github.com/nodejs/node/commit/41fcda840c)] - **http**: use `self.keepAlive` instead of `self.options.keepAlive` (Damian Schenkelman) [#4407](https://github.com/nodejs/node/pull/4407)
* [[`3ff237333d`](https://github.com/nodejs/node/commit/3ff237333d)] - **http**: Remove an unnecessary assignment (Bo Borgerson) [#4323](https://github.com/nodejs/node/pull/4323)
* [[`39dc054572`](https://github.com/nodejs/node/commit/39dc054572)] - **http**: remove excess calls to removeSocket (Dave) [#4172](https://github.com/nodejs/node/pull/4172)
* [[`751fbd84dd`](https://github.com/nodejs/node/commit/751fbd84dd)] - **https**: use `servername` in agent key (Fedor Indutny) [#4389](https://github.com/nodejs/node/pull/4389)
* [[`7a1a0a0055`](https://github.com/nodejs/node/commit/7a1a0a0055)] - **lib**: remove unused modules (Rich Trott) [#4683](https://github.com/nodejs/node/pull/4683)
* [[`3d81ea99bb`](https://github.com/nodejs/node/commit/3d81ea99bb)] - **lib,test**: update let to const where applicable (Sakthipriyan Vairamani) [#3152](https://github.com/nodejs/node/pull/3152)
* [[`8a9869eeab`](https://github.com/nodejs/node/commit/8a9869eeab)] - **module**: fix column offsets in errors (Tristian Flanagan) [#2867](https://github.com/nodejs/node/pull/2867)
* [[`0ae90ecd3d`](https://github.com/nodejs/node/commit/0ae90ecd3d)] - **module,repl**: remove repl require() hack (Ben Noordhuis) [#4026](https://github.com/nodejs/node/pull/4026)
* [[`a7367fdc1e`](https://github.com/nodejs/node/commit/a7367fdc1e)] - **net**: small code cleanup (Jan Schär) [#3943](https://github.com/nodejs/node/pull/3943)
* [[`03e9495cc2`](https://github.com/nodejs/node/commit/03e9495cc2)] - **node**: remove unused variables in AppendExceptionLine (Yazhong Liu) [#4264](https://github.com/nodejs/node/pull/4264)
* [[`06113b8711`](https://github.com/nodejs/node/commit/06113b8711)] - **node**: s/doNTCallbackX/nextTickCallbackWithXArgs/ (Rod Vagg) [#4167](https://github.com/nodejs/node/pull/4167)
* [[`8ce6843fe4`](https://github.com/nodejs/node/commit/8ce6843fe4)] - **os**: fix crash in GetInterfaceAddresses (Martin Bark) [#4272](https://github.com/nodejs/node/pull/4272)
* [[`53dcbb6aa4`](https://github.com/nodejs/node/commit/53dcbb6aa4)] - **repl**: remove unused function (Rich Trott)
* [[`db0e906fc1`](https://github.com/nodejs/node/commit/db0e906fc1)] - **repl**: Fixed node repl history edge case. (Mudit Ameta) [#4108](https://github.com/nodejs/node/pull/4108)
* [[`9855fab05f`](https://github.com/nodejs/node/commit/9855fab05f)] - **repl**: use String#repeat instead of Array#join (Evan Lucas) [#3900](https://github.com/nodejs/node/pull/3900)
* [[`41882e4077`](https://github.com/nodejs/node/commit/41882e4077)] - **repl**: fix require('3rdparty') regression (Ben Noordhuis) [#4215](https://github.com/nodejs/node/pull/4215)
* [[`93afc39d4a`](https://github.com/nodejs/node/commit/93afc39d4a)] - **repl**: attach location info to syntax errors (cjihrig) [#4013](https://github.com/nodejs/node/pull/4013)
* [[`d4806675a6`](https://github.com/nodejs/node/commit/d4806675a6)] - **repl**: display error message when loading directory (Prince J Wesley) [#4170](https://github.com/nodejs/node/pull/4170)
* [[`3080bdc7d7`](https://github.com/nodejs/node/commit/3080bdc7d7)] - **src**: define Is* util functions with macros (cjihrig) [#4118](https://github.com/nodejs/node/pull/4118)
* [[`2b8a32a13b`](https://github.com/nodejs/node/commit/2b8a32a13b)] - **src**: refactor vcbuild configure args creation (Rod Vagg) [#3399](https://github.com/nodejs/node/pull/3399)
* [[`d47f6ba768`](https://github.com/nodejs/node/commit/d47f6ba768)] - **src**: fix deprecation message for ErrnoException (Martin von Gagern) [#4269](https://github.com/nodejs/node/pull/4269)
* [[`5ba08fbf76`](https://github.com/nodejs/node/commit/5ba08fbf76)] - **src**: fix line numbers on core errors (cjihrig) [#4254](https://github.com/nodejs/node/pull/4254)
* [[`70974e9362`](https://github.com/nodejs/node/commit/70974e9362)] - **src**: use GetCurrentProcessId() for process.pid (Ben Noordhuis) [#4163](https://github.com/nodejs/node/pull/4163)
* [[`c96eca164f`](https://github.com/nodejs/node/commit/c96eca164f)] - **src**: don't print garbage errors (cjihrig) [#4112](https://github.com/nodejs/node/pull/4112)
* [[`f61412c753`](https://github.com/nodejs/node/commit/f61412c753)] - **test**: mark test-debug-no-context is flaky (Rich Trott) [#4421](https://github.com/nodejs/node/pull/4421)
* [[`46d8c93ed2`](https://github.com/nodejs/node/commit/46d8c93ed2)] - **test**: don't use cwd for relative path (Johan Bergström) [#4477](https://github.com/nodejs/node/pull/4477)
* [[`b6124ea39c`](https://github.com/nodejs/node/commit/b6124ea39c)] - **test**: write to tmp dir rather than fixture dir (Rich Trott) [#4489](https://github.com/nodejs/node/pull/4489)
* [[`350fa664bb`](https://github.com/nodejs/node/commit/350fa664bb)] - **test**: don't assume a certain folder structure (Johan Bergström) [#3325](https://github.com/nodejs/node/pull/3325)
* [[`6b2ef0efac`](https://github.com/nodejs/node/commit/6b2ef0efac)] - **test**: make temp path customizable (Johan Bergström) [#3325](https://github.com/nodejs/node/pull/3325)
* [[`f1837703a9`](https://github.com/nodejs/node/commit/f1837703a9)] - **test**: remove unused vars from parallel tests (Rich Trott) [#4511](https://github.com/nodejs/node/pull/4511)
* [[`b4964b099a`](https://github.com/nodejs/node/commit/b4964b099a)] - **test**: remove unused variables form http tests (Rich Trott) [#4422](https://github.com/nodejs/node/pull/4422)
* [[`0d5a508dfb`](https://github.com/nodejs/node/commit/0d5a508dfb)] - **test**: extend timeout in Debug mode (Rich Trott) [#4431](https://github.com/nodejs/node/pull/4431)
* [[`6e4598d5da`](https://github.com/nodejs/node/commit/6e4598d5da)] - **test**: remove unused variables from TLS tests (Rich Trott) [#4424](https://github.com/nodejs/node/pull/4424)
* [[`7b1aa045a0`](https://github.com/nodejs/node/commit/7b1aa045a0)] - **test**: remove unused variables from HTTPS tests (Rich Trott) [#4426](https://github.com/nodejs/node/pull/4426)
* [[`da9e5c1b01`](https://github.com/nodejs/node/commit/da9e5c1b01)] - **test**: remove unused variables from net tests (Rich Trott) [#4430](https://github.com/nodejs/node/pull/4430)
* [[`13241bd24b`](https://github.com/nodejs/node/commit/13241bd24b)] - **test**: remove unused vars in ChildProcess tests (Rich Trott) [#4425](https://github.com/nodejs/node/pull/4425)
* [[`2f4538ddda`](https://github.com/nodejs/node/commit/2f4538ddda)] - **test**: remove unused vars (Rich Trott) [#4536](https://github.com/nodejs/node/pull/4536)
* [[`dffe83ccd6`](https://github.com/nodejs/node/commit/dffe83ccd6)] - **test**: remove unused modules (Rich Trott) [#4684](https://github.com/nodejs/node/pull/4684)
* [[`c4eeb88ba1`](https://github.com/nodejs/node/commit/c4eeb88ba1)] - **test**: fix flaky cluster-disconnect-race (Brian White) [#4457](https://github.com/nodejs/node/pull/4457)
* [[`7caf87bf6c`](https://github.com/nodejs/node/commit/7caf87bf6c)] - **test**: fix flaky test-http-agent-keepalive (Rich Trott) [#4524](https://github.com/nodejs/node/pull/4524)
* [[`25c41d084d`](https://github.com/nodejs/node/commit/25c41d084d)] - **test**: remove flaky designations for tests (Rich Trott) [#4519](https://github.com/nodejs/node/pull/4519)
* [[`b8f097ece2`](https://github.com/nodejs/node/commit/b8f097ece2)] - **test**: fix flaky streams test (Rich Trott) [#4516](https://github.com/nodejs/node/pull/4516)
* [[`c24fa1437c`](https://github.com/nodejs/node/commit/c24fa1437c)] - **test**: inherit JOBS from environment (Johan Bergström) [#4495](https://github.com/nodejs/node/pull/4495)
* [[`7dc90e9e7f`](https://github.com/nodejs/node/commit/7dc90e9e7f)] - **test**: remove time check (Rich Trott) [#4494](https://github.com/nodejs/node/pull/4494)
* [[`7ca3c6c388`](https://github.com/nodejs/node/commit/7ca3c6c388)] - **test**: refactor test-fs-empty-readStream (Rich Trott) [#4490](https://github.com/nodejs/node/pull/4490)
* [[`610727dea7`](https://github.com/nodejs/node/commit/610727dea7)] - **test**: clarify role of domains in test (Rich Trott) [#4474](https://github.com/nodejs/node/pull/4474)
* [[`1ae0e355b9`](https://github.com/nodejs/node/commit/1ae0e355b9)] - **test**: improve assert message (Rich Trott) [#4461](https://github.com/nodejs/node/pull/4461)
* [[`e70c88df56`](https://github.com/nodejs/node/commit/e70c88df56)] - **test**: remove unused assert module imports (Rich Trott) [#4438](https://github.com/nodejs/node/pull/4438)
* [[`c77fc71f9b`](https://github.com/nodejs/node/commit/c77fc71f9b)] - **test**: remove unused var from test-assert.js (Rich Trott) [#4405](https://github.com/nodejs/node/pull/4405)
* [[`f613b3033f`](https://github.com/nodejs/node/commit/f613b3033f)] - **test**: add test-domain-exit-dispose-again back (Julien Gilli) [#4256](https://github.com/nodejs/node/pull/4256)
* [[`f5bfacd858`](https://github.com/nodejs/node/commit/f5bfacd858)] - **test**: remove unused `util` imports (Rich Trott) [#4562](https://github.com/nodejs/node/pull/4562)
* [[`d795301025`](https://github.com/nodejs/node/commit/d795301025)] - **test**: remove unnecessary assignments (Rich Trott) [#4563](https://github.com/nodejs/node/pull/4563)
* [[`acc3d66934`](https://github.com/nodejs/node/commit/acc3d66934)] - **test**: move ArrayStream to common (cjihrig) [#4027](https://github.com/nodejs/node/pull/4027)
* [[`6c0021361c`](https://github.com/nodejs/node/commit/6c0021361c)] - **test**: refactor test-net-connect-options-ipv6 (Rich Trott) [#4395](https://github.com/nodejs/node/pull/4395)
* [[`29804e00ad`](https://github.com/nodejs/node/commit/29804e00ad)] - **test**: use platformTimeout() in more places (Brian White) [#4387](https://github.com/nodejs/node/pull/4387)
* [[`761af37d0e`](https://github.com/nodejs/node/commit/761af37d0e)] - **test**: fix race condition in test-http-client-onerror (Devin Nakamura) [#4346](https://github.com/nodejs/node/pull/4346)
* [[`980852165f`](https://github.com/nodejs/node/commit/980852165f)] - **test**: fix flaky test-net-error-twice (Brian White) [#4342](https://github.com/nodejs/node/pull/4342)
* [[`1bc44e79d3`](https://github.com/nodejs/node/commit/1bc44e79d3)] - **test**: try other ipv6 localhost alternatives (Brian White) [#4325](https://github.com/nodejs/node/pull/4325)
* [[`44dbe15640`](https://github.com/nodejs/node/commit/44dbe15640)] - **test**: fix debug-port-cluster flakiness (Ben Noordhuis) [#4310](https://github.com/nodejs/node/pull/4310)
* [[`73e781172b`](https://github.com/nodejs/node/commit/73e781172b)] - **test**: add test for tls.parseCertString (Evan Lucas) [#4283](https://github.com/nodejs/node/pull/4283)
* [[`15c295a21b`](https://github.com/nodejs/node/commit/15c295a21b)] - **test**: use regular timeout times for ARMv8 (Jeremiah Senkpiel) [#4248](https://github.com/nodejs/node/pull/4248)
* [[`fd250b8fab`](https://github.com/nodejs/node/commit/fd250b8fab)] - **test**: parallelize test-repl-persistent-history (Jeremiah Senkpiel) [#4247](https://github.com/nodejs/node/pull/4247)
* [[`9a0f156e5a`](https://github.com/nodejs/node/commit/9a0f156e5a)] - **test**: fix domain-top-level-error-handler-throw (Santiago Gimeno) [#4364](https://github.com/nodejs/node/pull/4364)
* [[`6bc1b1c259`](https://github.com/nodejs/node/commit/6bc1b1c259)] - **test**: don't assume openssl s_client supports -ssl3 (Ben Noordhuis) [#4204](https://github.com/nodejs/node/pull/4204)
* [[`d00b9fc66f`](https://github.com/nodejs/node/commit/d00b9fc66f)] - **test**: fix tls-inception flakiness (Santiago Gimeno) [#4195](https://github.com/nodejs/node/pull/4195)
* [[`c41b280a2b`](https://github.com/nodejs/node/commit/c41b280a2b)] - **test**: fix tls-inception (Santiago Gimeno) [#4195](https://github.com/nodejs/node/pull/4195)
* [[`6f4ab1d1ab`](https://github.com/nodejs/node/commit/6f4ab1d1ab)] - **test**: mark test-cluster-shared-leak flaky (Rich Trott) [#4162](https://github.com/nodejs/node/pull/4162)
* [[`90498e2a68`](https://github.com/nodejs/node/commit/90498e2a68)] - **test**: skip long path tests on non-Windows (Rafał Pocztarski) [#4116](https://github.com/nodejs/node/pull/4116)
* [[`c9100d78f3`](https://github.com/nodejs/node/commit/c9100d78f3)] - **test**: fix flaky test-net-socket-local-address (Rich Trott) [#4109](https://github.com/nodejs/node/pull/4109)
* [[`ac939d51d9`](https://github.com/nodejs/node/commit/ac939d51d9)] - **test**: improve cluster-disconnect-handles test (Brian White) [#4084](https://github.com/nodejs/node/pull/4084)
* [[`22ba1b4115`](https://github.com/nodejs/node/commit/22ba1b4115)] - **test**: eliminate multicast test FreeBSD flakiness (Rich Trott) [#4042](https://github.com/nodejs/node/pull/4042)
* [[`2ee7853bb7`](https://github.com/nodejs/node/commit/2ee7853bb7)] - **test**: fix http-many-ended-pipelines flakiness (Santiago Gimeno) [#4041](https://github.com/nodejs/node/pull/4041)
* [[`a77dcfec06`](https://github.com/nodejs/node/commit/a77dcfec06)] - **test**: use platform-based timeout for reliability (Rich Trott) [#4015](https://github.com/nodejs/node/pull/4015)
* [[`3f0ff879cf`](https://github.com/nodejs/node/commit/3f0ff879cf)] - **test**: fix time resolution constraint (Gireesh Punathil) [#3981](https://github.com/nodejs/node/pull/3981)
* [[`22b88e1c48`](https://github.com/nodejs/node/commit/22b88e1c48)] - **test**: add TAP diagnostic message for retried tests (Rich Trott) [#3960](https://github.com/nodejs/node/pull/3960)
* [[`22d2887b1c`](https://github.com/nodejs/node/commit/22d2887b1c)] - **test**: add OS X to module loading error test (Evan Lucas) [#3901](https://github.com/nodejs/node/pull/3901)
* [[`e2141cb75e`](https://github.com/nodejs/node/commit/e2141cb75e)] - **test**: skip instead of fail when mem constrained (Michael Cornacchia) [#3697](https://github.com/nodejs/node/pull/3697)
* [[`166523d0ed`](https://github.com/nodejs/node/commit/166523d0ed)] - **test**: fix race condition in unrefd interval test (Michael Cornacchia) [#3550](https://github.com/nodejs/node/pull/3550)
* [[`86b47e8dc0`](https://github.com/nodejs/node/commit/86b47e8dc0)] - **timers**: optimize callback call: bind -> arrow (Andrei Sedoi) [#4038](https://github.com/nodejs/node/pull/4038)
* [[`4d37472ea7`](https://github.com/nodejs/node/commit/4d37472ea7)] - **tls_wrap**: clear errors on return (Fedor Indutny) [#4709](https://github.com/nodejs/node/pull/4709)
* [[`5b695d0343`](https://github.com/nodejs/node/commit/5b695d0343)] - **tls_wrap**: inherit from the `AsyncWrap` first (Fedor Indutny) [#4268](https://github.com/nodejs/node/pull/4268)
* [[`0efc35e6d8`](https://github.com/nodejs/node/commit/0efc35e6d8)] - **tls_wrap**: slice buffer properly in `ClearOut` (Fedor Indutny) [#4184](https://github.com/nodejs/node/pull/4184)
* [[`628cb8657c`](https://github.com/nodejs/node/commit/628cb8657c)] - **tools**: add .editorconfig (ronkorving) [#2993](https://github.com/nodejs/node/pull/2993)
* [[`69fef19624`](https://github.com/nodejs/node/commit/69fef19624)] - **tools**: implement no-unused-vars for eslint (Rich Trott) [#4536](https://github.com/nodejs/node/pull/4536)
* [[`3ee16706f2`](https://github.com/nodejs/node/commit/3ee16706f2)] - **tools**: enforce `throw new Error()` with lint rule (Rich Trott) [#3714](https://github.com/nodejs/node/pull/3714)
* [[`32801de4ef`](https://github.com/nodejs/node/commit/32801de4ef)] - **tools**: Use `throw new Error()` consistently (Rich Trott) [#3714](https://github.com/nodejs/node/pull/3714)
* [[`f413fae0cd`](https://github.com/nodejs/node/commit/f413fae0cd)] - **tools**: add tap output to cpplint (Johan Bergström) [#3448](https://github.com/nodejs/node/pull/3448)
* [[`efa30dd2f0`](https://github.com/nodejs/node/commit/efa30dd2f0)] - **tools**: enable prefer-const eslint rule (Sakthipriyan Vairamani) [#3152](https://github.com/nodejs/node/pull/3152)
* [[`dd0c925896`](https://github.com/nodejs/node/commit/dd0c925896)] - **udp**: remove a needless instanceof Buffer check (ronkorving) [#4301](https://github.com/nodejs/node/pull/4301)
* [[`f4414102ed`](https://github.com/nodejs/node/commit/f4414102ed)] - **util**: faster arrayToHash (Jackson Tian)
* [[`b421119984`](https://github.com/nodejs/node/commit/b421119984)] - **util**: determine object types in C++ (cjihrig) [#4100](https://github.com/nodejs/node/pull/4100)
* [[`6a7c9d9293`](https://github.com/nodejs/node/commit/6a7c9d9293)] - **util**: move .decorateErrorStack to internal/util (Ben Noordhuis) [#4026](https://github.com/nodejs/node/pull/4026)
* [[`422a865d46`](https://github.com/nodejs/node/commit/422a865d46)] - **util**: add decorateErrorStack() (cjihrig) [#4013](https://github.com/nodejs/node/pull/4013)
* [[`2d5380ea25`](https://github.com/nodejs/node/commit/2d5380ea25)] - **util**: fix constructor/instanceof checks (Brian White) [#3385](https://github.com/nodejs/node/pull/3385)
* [[`1bf84b9d41`](https://github.com/nodejs/node/commit/1bf84b9d41)] - **util,src**: allow lookup of hidden values (cjihrig) [#3988](https://github.com/nodejs/node/pull/3988)

## 2015-12-23, Version 4.2.4 'Argon' (LTS), @jasnell

Maintenance update.

### Notable changes

* Roughly 78% of the commits are documentation and test improvements
* **domains**:
** Fix handling of uncaught exceptions (Julien Gilli) [#3884](https://github.com/nodejs/node/pull/3884)
* **deps**:
** Upgrade to npm 2.14.12 (Kat Marchán) [#4110](https://github.com/nodejs/node/pull/4110)
** Backport 819b40a from V8 upstream (Michaël Zasso) [#3938](https://github.com/nodejs/node/pull/3938)
** Updated node LICENSE file with new npm license (Kat Marchán) [#4110](https://github.com/nodejs/node/pull/4110)

### Known issues

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`907a13a07f`](https://github.com/nodejs/node/commit/907a13a07f)] - Add missing va_end before return (Ömer Fadıl Usta) [#3565](https://github.com/nodejs/node/pull/3565)
* [[`7ffc01756f`](https://github.com/nodejs/node/commit/7ffc01756f)] - **buffer**: fix writeInt{B,L}E for some neg values (Peter A. Bigot) [#3994](https://github.com/nodejs/node/pull/3994)
* [[`db0186e435`](https://github.com/nodejs/node/commit/db0186e435)] - **buffer**: let WriteFloatGeneric silently drop values (P.S.V.R)
* [[`5c6740865a`](https://github.com/nodejs/node/commit/5c6740865a)] - **build**: update signtool description, add url (Rod Vagg) [#4011](https://github.com/nodejs/node/pull/4011)
* [[`60dda70f89`](https://github.com/nodejs/node/commit/60dda70f89)] - **build**: fix --with-intl=system-icu for x-compile (Steven R. Loomis) [#3808](https://github.com/nodejs/node/pull/3808)
* [[`22208b067c`](https://github.com/nodejs/node/commit/22208b067c)] - **build**: fix configuring with prebuilt libraries (Markus Tzoe) [#3135](https://github.com/nodejs/node/pull/3135)
* [[`914caf9c69`](https://github.com/nodejs/node/commit/914caf9c69)] - **child_process**: add safety checks on stdio access (cjihrig) [#3799](https://github.com/nodejs/node/pull/3799)
* [[`236ad90a84`](https://github.com/nodejs/node/commit/236ad90a84)] - **child_process**: don't fork bomb ourselves from -e (Ben Noordhuis) [#3575](https://github.com/nodejs/node/pull/3575)
* [[`f28f69dac4`](https://github.com/nodejs/node/commit/f28f69dac4)] - **cluster**: remove handles when disconnecting worker (Ben Noordhuis) [#3677](https://github.com/nodejs/node/pull/3677)
* [[`f5c5e8bf91`](https://github.com/nodejs/node/commit/f5c5e8bf91)] - **cluster**: send suicide message on disconnect (cjihrig) [#3720](https://github.com/nodejs/node/pull/3720)
* [[`629d5d18d7`](https://github.com/nodejs/node/commit/629d5d18d7)] - **configure**: `v8_use_snapshot` should be `true` (Fedor Indutny) [#3962](https://github.com/nodejs/node/pull/3962)
* [[`3094464871`](https://github.com/nodejs/node/commit/3094464871)] - **configure**: use __ARM_ARCH to determine arm version (João Reis) [#4123](https://github.com/nodejs/node/pull/4123)
* [[`1e1173fc5c`](https://github.com/nodejs/node/commit/1e1173fc5c)] - **configure**: respect CC_host in host arch detection (João Reis) [#4117](https://github.com/nodejs/node/pull/4117)
* [[`2e9b886fbf`](https://github.com/nodejs/node/commit/2e9b886fbf)] - **crypto**: DSA parameter validation in FIPS mode (Stefan Budeanu) [#3756](https://github.com/nodejs/node/pull/3756)
* [[`00b77d9e84`](https://github.com/nodejs/node/commit/00b77d9e84)] - **crypto**: Improve error checking and reporting (Stefan Budeanu) [#3753](https://github.com/nodejs/node/pull/3753)
* [[`3dd90ddc73`](https://github.com/nodejs/node/commit/3dd90ddc73)] - **deps**: upgrade to npm 2.14.12 (Kat Marchán) [#4110](https://github.com/nodejs/node/pull/4110)
* [[`51ae8d10b3`](https://github.com/nodejs/node/commit/51ae8d10b3)] - **deps**: Updated node LICENSE file with new npm license (Kat Marchán) [#4110](https://github.com/nodejs/node/pull/4110)
* [[`9e1edead22`](https://github.com/nodejs/node/commit/9e1edead22)] - **deps**: backport 819b40a from V8 upstream (Michaël Zasso) [#3938](https://github.com/nodejs/node/pull/3938)
* [[`a2ce3843cc`](https://github.com/nodejs/node/commit/a2ce3843cc)] - **deps**: upgrade npm to 2.14.9 (Forrest L Norvell) [#3686](https://github.com/nodejs/node/pull/3686)
* [[`b140cb29f4`](https://github.com/nodejs/node/commit/b140cb29f4)] - **dns**: prevent undefined values in results (Junliang Yan) [#3696](https://github.com/nodejs/node/pull/3696)
* [[`8aafa2ecc0`](https://github.com/nodejs/node/commit/8aafa2ecc0)] - **doc**: standardize references to node.js in docs (Scott Buchanan) [#4136](https://github.com/nodejs/node/pull/4136)
* [[`72f43a263a`](https://github.com/nodejs/node/commit/72f43a263a)] - **doc**: fix internal link to child.send() (Luigi Pinca) [#4089](https://github.com/nodejs/node/pull/4089)
* [[`dcfdbac457`](https://github.com/nodejs/node/commit/dcfdbac457)] - **doc**: reword https.Agent example text (Jan Krems) [#4075](https://github.com/nodejs/node/pull/4075)
* [[`f93d268dec`](https://github.com/nodejs/node/commit/f93d268dec)] - **doc**: add HTTP working group (James M Snell) [#3919](https://github.com/nodejs/node/pull/3919)
* [[`beee0553ca`](https://github.com/nodejs/node/commit/beee0553ca)] - **doc**: update WORKING_GROUPS.md - add missing groups (Michael Dawson) [#3450](https://github.com/nodejs/node/pull/3450)
* [[`3327415fc4`](https://github.com/nodejs/node/commit/3327415fc4)] - **doc**: fix the exception description (yorkie) [#3658](https://github.com/nodejs/node/pull/3658)
* [[`da8d012c88`](https://github.com/nodejs/node/commit/da8d012c88)] - **doc**: clarify v4.2.3 notable items (Rod Vagg) [#4155](https://github.com/nodejs/node/pull/4155)
* [[`44a2d8ca24`](https://github.com/nodejs/node/commit/44a2d8ca24)] - **doc**: fix color of linked code blocks (jpersson) [#4068](https://github.com/nodejs/node/pull/4068)
* [[`bebde48ebc`](https://github.com/nodejs/node/commit/bebde48ebc)] - **doc**: fix typo in README (Rich Trott) [#4000](https://github.com/nodejs/node/pull/4000)
* [[`b48d5ec301`](https://github.com/nodejs/node/commit/b48d5ec301)] - **doc**: message.header duplication correction (Bryan English) [#3997](https://github.com/nodejs/node/pull/3997)
* [[`6ef3625456`](https://github.com/nodejs/node/commit/6ef3625456)] - **doc**: replace sane with reasonable (Lewis Cowper) [#3980](https://github.com/nodejs/node/pull/3980)
* [[`c5be3c63f0`](https://github.com/nodejs/node/commit/c5be3c63f0)] - **doc**: fix rare case of misaligned columns (Roman Reiss) [#3948](https://github.com/nodejs/node/pull/3948)
* [[`bd82fb06ff`](https://github.com/nodejs/node/commit/bd82fb06ff)] - **doc**: fix broken references (Alexander Gromnitsky) [#3944](https://github.com/nodejs/node/pull/3944)
* [[`8eb28c3d50`](https://github.com/nodejs/node/commit/8eb28c3d50)] - **doc**: add reference for buffer.inspect() (cjihrig) [#3921](https://github.com/nodejs/node/pull/3921)
* [[`4bc71e0078`](https://github.com/nodejs/node/commit/4bc71e0078)] - **doc**: clarify module loading behavior (cjihrig) [#3920](https://github.com/nodejs/node/pull/3920)
* [[`4c382e7aaa`](https://github.com/nodejs/node/commit/4c382e7aaa)] - **doc**: numeric flags to fs.open (Carl Lei) [#3641](https://github.com/nodejs/node/pull/3641)
* [[`5207099dc9`](https://github.com/nodejs/node/commit/5207099dc9)] - **doc**: clarify that fs streams expect blocking fd (Carl Lei) [#3641](https://github.com/nodejs/node/pull/3641)
* [[`753c5071ea`](https://github.com/nodejs/node/commit/753c5071ea)] - **doc**: Adding best practises for crypto.pbkdf2 (Tom Gallacher) [#3290](https://github.com/nodejs/node/pull/3290)
* [[`8f0291beba`](https://github.com/nodejs/node/commit/8f0291beba)] - **doc**: update WORKING_GROUPS.md to include Intl (Steven R. Loomis) [#3251](https://github.com/nodejs/node/pull/3251)
* [[`c31d472487`](https://github.com/nodejs/node/commit/c31d472487)] - **doc**: sort repl alphabetically (Tristian Flanagan) [#3859](https://github.com/nodejs/node/pull/3859)
* [[`6b172d9fe8`](https://github.com/nodejs/node/commit/6b172d9fe8)] - **doc**: consistent reference-style links (Bryan English) [#3845](https://github.com/nodejs/node/pull/3845)
* [[`ffd3335e29`](https://github.com/nodejs/node/commit/ffd3335e29)] - **doc**: address use of profanity in code of conduct (James M Snell) [#3827](https://github.com/nodejs/node/pull/3827)
* [[`a36a5b63cf`](https://github.com/nodejs/node/commit/a36a5b63cf)] - **doc**: reword message.headers to indicate they are not read-only (Tristian Flanagan) [#3814](https://github.com/nodejs/node/pull/3814)
* [[`6de77cd320`](https://github.com/nodejs/node/commit/6de77cd320)] - **doc**: clarify duplicate header handling (Bryan English) [#3810](https://github.com/nodejs/node/pull/3810)
* [[`b22973af81`](https://github.com/nodejs/node/commit/b22973af81)] - **doc**: replace head of readme with updated text (Rod Vagg) [#3482](https://github.com/nodejs/node/pull/3482)
* [[`eab0d56ea9`](https://github.com/nodejs/node/commit/eab0d56ea9)] - **doc**: repl: add defineComand and displayPrompt (Bryan English) [#3765](https://github.com/nodejs/node/pull/3765)
* [[`15fb02985f`](https://github.com/nodejs/node/commit/15fb02985f)] - **doc**: document release types in readme (Rod Vagg) [#3482](https://github.com/nodejs/node/pull/3482)
* [[`29f26b882f`](https://github.com/nodejs/node/commit/29f26b882f)] - **doc**: add link to \[customizing util.inspect colors\]. (Jesse McCarthy) [#3749](https://github.com/nodejs/node/pull/3749)
* [[`90fdb4f7b3`](https://github.com/nodejs/node/commit/90fdb4f7b3)] - **doc**: sort tls alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`39fa9fa85c`](https://github.com/nodejs/node/commit/39fa9fa85c)] - **doc**: sort stream alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`e98e8afb2b`](https://github.com/nodejs/node/commit/e98e8afb2b)] - **doc**: sort net alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`6de887483d`](https://github.com/nodejs/node/commit/6de887483d)] - **doc**: sort process alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`37033dcb71`](https://github.com/nodejs/node/commit/37033dcb71)] - **doc**: sort zlib alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`9878034567`](https://github.com/nodejs/node/commit/9878034567)] - **doc**: sort util alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`48fc765eb6`](https://github.com/nodejs/node/commit/48fc765eb6)] - **doc**: sort https alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`3546eb4f40`](https://github.com/nodejs/node/commit/3546eb4f40)] - **doc**: sort http alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`dedfb1156a`](https://github.com/nodejs/node/commit/dedfb1156a)] - **doc**: sort modules alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`71722fe1a1`](https://github.com/nodejs/node/commit/71722fe1a1)] - **doc**: sort readline alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`660062bf9e`](https://github.com/nodejs/node/commit/660062bf9e)] - **doc**: sort repl alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`34b8d28725`](https://github.com/nodejs/node/commit/34b8d28725)] - **doc**: sort string_decoder alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`3f3b9ed7d7`](https://github.com/nodejs/node/commit/3f3b9ed7d7)] - **doc**: sort timers alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`af876ddc64`](https://github.com/nodejs/node/commit/af876ddc64)] - **doc**: sort tty alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`3c2068704a`](https://github.com/nodejs/node/commit/3c2068704a)] - **doc**: sort url alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`363692fd0c`](https://github.com/nodejs/node/commit/363692fd0c)] - **doc**: sort vm alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`ca41b55166`](https://github.com/nodejs/node/commit/ca41b55166)] - **doc**: sort querystring alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`f37ff22b9f`](https://github.com/nodejs/node/commit/f37ff22b9f)] - **doc**: sort punycode alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`4d569607af`](https://github.com/nodejs/node/commit/4d569607af)] - **doc**: sort path alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`daa62447d1`](https://github.com/nodejs/node/commit/daa62447d1)] - **doc**: sort os alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`0906f9a8bb`](https://github.com/nodejs/node/commit/0906f9a8bb)] - **doc**: sort globals alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`6cd06c1319`](https://github.com/nodejs/node/commit/6cd06c1319)] - **doc**: sort fs alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`5b310f8d9e`](https://github.com/nodejs/node/commit/5b310f8d9e)] - **doc**: sort events alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`782cb7d15b`](https://github.com/nodejs/node/commit/782cb7d15b)] - **doc**: sort errors alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`c39eabbec4`](https://github.com/nodejs/node/commit/c39eabbec4)] - **doc**: sort dgram alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`261e0f3a21`](https://github.com/nodejs/node/commit/261e0f3a21)] - **doc**: sort crypto alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`0e6121d04d`](https://github.com/nodejs/node/commit/0e6121d04d)] - **doc**: sort dns alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`435ffb79f7`](https://github.com/nodejs/node/commit/435ffb79f7)] - **doc**: sort console alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`28935a10d6`](https://github.com/nodejs/node/commit/28935a10d6)] - **doc**: sort cluster alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`5e79dc4406`](https://github.com/nodejs/node/commit/5e79dc4406)] - **doc**: sort child_process alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`af0bf1a72c`](https://github.com/nodejs/node/commit/af0bf1a72c)] - **doc**: sort buffer alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`f43a0330aa`](https://github.com/nodejs/node/commit/f43a0330aa)] - **doc**: sort assert alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`1bbc3b3ff8`](https://github.com/nodejs/node/commit/1bbc3b3ff8)] - **doc**: add note on tls connection meta data methods (Tyler Henkel) [#3746](https://github.com/nodejs/node/pull/3746)
* [[`3c415bbb12`](https://github.com/nodejs/node/commit/3c415bbb12)] - **doc**: add note to util.isBuffer (Evan Lucas) [#3790](https://github.com/nodejs/node/pull/3790)
* [[`7b5e4574fd`](https://github.com/nodejs/node/commit/7b5e4574fd)] - **doc**: add romankl to collaborators (Roman Klauke) [#3725](https://github.com/nodejs/node/pull/3725)
* [[`4f7c638a7a`](https://github.com/nodejs/node/commit/4f7c638a7a)] - **doc**: add saghul as a collaborator (Saúl Ibarra Corretgé)
* [[`523251270a`](https://github.com/nodejs/node/commit/523251270a)] - **doc**: add thealphanerd to collaborators (Myles Borins) [#3723](https://github.com/nodejs/node/pull/3723)
* [[`488e74f27d`](https://github.com/nodejs/node/commit/488e74f27d)] - **doc**: update lts description in the collaborator guide (James M Snell) [#3668](https://github.com/nodejs/node/pull/3668)
* [[`fe3ae3cea4`](https://github.com/nodejs/node/commit/fe3ae3cea4)] - **doc**: add LTS info to COLLABORATOR_GUIDE.md (Myles Borins) [#3442](https://github.com/nodejs/node/pull/3442)
* [[`daa10a345e`](https://github.com/nodejs/node/commit/daa10a345e)] - **doc**: typo fix in readme.md (Sam P Gallagher-Bishop) [#3649](https://github.com/nodejs/node/pull/3649)
* [[`eca5720761`](https://github.com/nodejs/node/commit/eca5720761)] - **doc**: fix wrong date and known issue in changelog.md (James M Snell) [#3650](https://github.com/nodejs/node/pull/3650)
* [[`83494f8f3e`](https://github.com/nodejs/node/commit/83494f8f3e)] - **doc**: rename iojs-* groups to nodejs-* (Steven R. Loomis) [#3634](https://github.com/nodejs/node/pull/3634)
* [[`347fb65aee`](https://github.com/nodejs/node/commit/347fb65aee)] - **doc**: fix crypto spkac function descriptions (Jason Gerfen) [#3614](https://github.com/nodejs/node/pull/3614)
* [[`11d2050d63`](https://github.com/nodejs/node/commit/11d2050d63)] - **doc**: Updated streams simplified constructor API (Tom Gallacher) [#3602](https://github.com/nodejs/node/pull/3602)
* [[`6db4392bfb`](https://github.com/nodejs/node/commit/6db4392bfb)] - **doc**: made code spans more visible in the API docs (phijohns) [#3573](https://github.com/nodejs/node/pull/3573)
* [[`8a7dd73af1`](https://github.com/nodejs/node/commit/8a7dd73af1)] - **doc**: added what buf.copy returns (Manuel B) [#3555](https://github.com/nodejs/node/pull/3555)
* [[`cf4b65c2d6`](https://github.com/nodejs/node/commit/cf4b65c2d6)] - **doc**: fix function param order in assert doc (David Woods) [#3533](https://github.com/nodejs/node/pull/3533)
* [[`a2efe4c72b`](https://github.com/nodejs/node/commit/a2efe4c72b)] - **doc**: add note about timeout delay > TIMEOUT_MAX (Guilherme Souza) [#3512](https://github.com/nodejs/node/pull/3512)
* [[`d1b5833476`](https://github.com/nodejs/node/commit/d1b5833476)] - **doc**: add caveats of algs and key size in crypto (Shigeki Ohtsu) [#3479](https://github.com/nodejs/node/pull/3479)
* [[`12cdf6fcf3`](https://github.com/nodejs/node/commit/12cdf6fcf3)] - **doc**: add method links in events.markdown (Alejandro Oviedo) [#3187](https://github.com/nodejs/node/pull/3187)
* [[`f50f19e384`](https://github.com/nodejs/node/commit/f50f19e384)] - **doc**: stdout/stderr can block when directed to file (Ben Noordhuis) [#3170](https://github.com/nodejs/node/pull/3170)
* [[`b2cc1302e0`](https://github.com/nodejs/node/commit/b2cc1302e0)] - **docs**: improve discoverability of Code of Conduct (Ashley Williams) [#3774](https://github.com/nodejs/node/pull/3774)
* [[`fa1ab497f1`](https://github.com/nodejs/node/commit/fa1ab497f1)] - **docs**: fs - change links to buffer encoding to Buffer class anchor (fansworld-claudio) [#2796](https://github.com/nodejs/node/pull/2796)
* [[`34e64e5390`](https://github.com/nodejs/node/commit/34e64e5390)] - **domains**: fix handling of uncaught exceptions (Julien Gilli) [#3884](https://github.com/nodejs/node/pull/3884)
* [[`0311836e7a`](https://github.com/nodejs/node/commit/0311836e7a)] - **meta**: remove use of profanity in source (Myles Borins) [#4122](https://github.com/nodejs/node/pull/4122)
* [[`971762ada9`](https://github.com/nodejs/node/commit/971762ada9)] - **module**: cache regular expressions (Evan Lucas) [#3869](https://github.com/nodejs/node/pull/3869)
* [[`d80fa2c77c`](https://github.com/nodejs/node/commit/d80fa2c77c)] - **module**: remove unnecessary JSON.stringify (Andres Suarez) [#3578](https://github.com/nodejs/node/pull/3578)
* [[`aa85d62f09`](https://github.com/nodejs/node/commit/aa85d62f09)] - **net**: add local address/port for better errors (Jan Schär) [#3946](https://github.com/nodejs/node/pull/3946)
* [[`803a56de52`](https://github.com/nodejs/node/commit/803a56de52)] - **querystring**: Parse multiple separator characters (Yosuke Furukawa) [#3807](https://github.com/nodejs/node/pull/3807)
* [[`ff02b295fc`](https://github.com/nodejs/node/commit/ff02b295fc)] - **repl**: don't crash if cannot open history file (Evan Lucas) [#3630](https://github.com/nodejs/node/pull/3630)
* [[`329e88e545`](https://github.com/nodejs/node/commit/329e88e545)] - **repl**: To exit, press ^C again or type .exit. (Hemanth.HM) [#3368](https://github.com/nodejs/node/pull/3368)
* [[`9b05905361`](https://github.com/nodejs/node/commit/9b05905361)] - **src**: Revert "nix stdin _readableState.reading" (Roman Reiss) [#3490](https://github.com/nodejs/node/pull/3490)
* [[`957c1f2543`](https://github.com/nodejs/node/commit/957c1f2543)] - **stream_wrap**: error if stream has StringDecoder (Fedor Indutny) [#4031](https://github.com/nodejs/node/pull/4031)
* [[`43e3b69dae`](https://github.com/nodejs/node/commit/43e3b69dae)] - **test**: refactor test-http-exit-delay (Rich Trott) [#4055](https://github.com/nodejs/node/pull/4055)
* [[`541d0d21be`](https://github.com/nodejs/node/commit/541d0d21be)] - **test**: fix cluster-disconnect-handles flakiness (Santiago Gimeno) [#4009](https://github.com/nodejs/node/pull/4009)
* [[`5f66d66e84`](https://github.com/nodejs/node/commit/5f66d66e84)] - **test**: don't check the # of chunks in test-http-1.0 (Santiago Gimeno) [#3961](https://github.com/nodejs/node/pull/3961)
* [[`355edf585b`](https://github.com/nodejs/node/commit/355edf585b)] - **test**: fix cluster-worker-isdead (Santiago Gimeno) [#3954](https://github.com/nodejs/node/pull/3954)
* [[`4e46e04002`](https://github.com/nodejs/node/commit/4e46e04002)] - **test**: add test for repl.defineCommand() (Bryan English) [#3908](https://github.com/nodejs/node/pull/3908)
* [[`4ea1a69c53`](https://github.com/nodejs/node/commit/4ea1a69c53)] - **test**: mark test flaky on FreeBSD (Rich Trott) [#4016](https://github.com/nodejs/node/pull/4016)
* [[`05b64c11f5`](https://github.com/nodejs/node/commit/05b64c11f5)] - **test**: mark cluster-net-send test flaky on windows (Rich Trott) [#4006](https://github.com/nodejs/node/pull/4006)
* [[`695015579b`](https://github.com/nodejs/node/commit/695015579b)] - **test**: remove flaky designation from ls-no-sslv3 (Rich Trott) [#3620](https://github.com/nodejs/node/pull/3620)
* [[`abbd87b273`](https://github.com/nodejs/node/commit/abbd87b273)] - **test**: mark fork regression test flaky on windows (Rich Trott) [#4005](https://github.com/nodejs/node/pull/4005)
* [[`38ba152a7a`](https://github.com/nodejs/node/commit/38ba152a7a)] - **test**: skip test if in FreeBSD jail (Rich Trott) [#3995](https://github.com/nodejs/node/pull/3995)
* [[`cc24f0ea58`](https://github.com/nodejs/node/commit/cc24f0ea58)] - **test**: fix test-domain-exit-dispose-again (Julien Gilli) [#3990](https://github.com/nodejs/node/pull/3990)
* [[`b2f1014d26`](https://github.com/nodejs/node/commit/b2f1014d26)] - **test**: remove flaky status for cluster test (Rich Trott) [#3975](https://github.com/nodejs/node/pull/3975)
* [[`e66794fd30`](https://github.com/nodejs/node/commit/e66794fd30)] - **test**: address flaky test-http-client-timeout-event (Rich Trott) [#3968](https://github.com/nodejs/node/pull/3968)
* [[`5a2727421a`](https://github.com/nodejs/node/commit/5a2727421a)] - **test**: retry on smartos if ECONNREFUSED (Rich Trott) [#3941](https://github.com/nodejs/node/pull/3941)
* [[`dbc85a275c`](https://github.com/nodejs/node/commit/dbc85a275c)] - **test**: avoid test timeouts on rpi (Stefan Budeanu) [#3902](https://github.com/nodejs/node/pull/3902)
* [[`b9d7378d20`](https://github.com/nodejs/node/commit/b9d7378d20)] - **test**: fix flaky test-child-process-spawnsync-input (Rich Trott) [#3889](https://github.com/nodejs/node/pull/3889)
* [[`cca216a034`](https://github.com/nodejs/node/commit/cca216a034)] - **test**: move test-specific function out of common (Rich Trott) [#3871](https://github.com/nodejs/node/pull/3871)
* [[`fb8df8d6c2`](https://github.com/nodejs/node/commit/fb8df8d6c2)] - **test**: module loading error fix solaris #3798 (fansworld-claudio) [#3855](https://github.com/nodejs/node/pull/3855)
* [[`9ea6bc1e0f`](https://github.com/nodejs/node/commit/9ea6bc1e0f)] - **test**: skip test if FreeBSD jail will break it (Rich Trott) [#3839](https://github.com/nodejs/node/pull/3839)
* [[`150f126618`](https://github.com/nodejs/node/commit/150f126618)] - **test**: fix flaky SmartOS test (Rich Trott) [#3830](https://github.com/nodejs/node/pull/3830)
* [[`603a6f5405`](https://github.com/nodejs/node/commit/603a6f5405)] - **test**: run pipeline flood test in parallel (Rich Trott) [#3811](https://github.com/nodejs/node/pull/3811)
* [[`4a26f74ee3`](https://github.com/nodejs/node/commit/4a26f74ee3)] - **test**: skip/replace weak crypto tests in FIPS mode (Stefan Budeanu) [#3757](https://github.com/nodejs/node/pull/3757)
* [[`3f9562b6bd`](https://github.com/nodejs/node/commit/3f9562b6bd)] - **test**: stronger crypto in test fixtures (Stefan Budeanu) [#3759](https://github.com/nodejs/node/pull/3759)
* [[`1f83eebec5`](https://github.com/nodejs/node/commit/1f83eebec5)] - **test**: increase crypto strength for FIPS standard (Stefan Budeanu) [#3758](https://github.com/nodejs/node/pull/3758)
* [[`7c5fbf7850`](https://github.com/nodejs/node/commit/7c5fbf7850)] - **test**: add hasFipsCrypto to test/common.js (Stefan Budeanu) [#3756](https://github.com/nodejs/node/pull/3756)
* [[`f30214f135`](https://github.com/nodejs/node/commit/f30214f135)] - **test**: add test for invalid DSA key size (Stefan Budeanu) [#3756](https://github.com/nodejs/node/pull/3756)
* [[`9a6c9faafb`](https://github.com/nodejs/node/commit/9a6c9faafb)] - **test**: numeric flags to fs.open (Carl Lei) [#3641](https://github.com/nodejs/node/pull/3641)
* [[`93d1d3cfcd`](https://github.com/nodejs/node/commit/93d1d3cfcd)] - **test**: refactor test-http-pipeline-flood (Rich Trott) [#3636](https://github.com/nodejs/node/pull/3636)
* [[`6c23f67504`](https://github.com/nodejs/node/commit/6c23f67504)] - **test**: fix flaky test test-http-pipeline-flood (Devin Nakamura) [#3636](https://github.com/nodejs/node/pull/3636)
* [[`4e5cae4360`](https://github.com/nodejs/node/commit/4e5cae4360)] - **test**: use really invalid hostname (Sakthipriyan Vairamani) [#3711](https://github.com/nodejs/node/pull/3711)
* [[`da189f793b`](https://github.com/nodejs/node/commit/da189f793b)] - **test**: Fix test-cluster-worker-exit.js for AIX (Imran Iqbal) [#3666](https://github.com/nodejs/node/pull/3666)
* [[`7b4194a863`](https://github.com/nodejs/node/commit/7b4194a863)] - **test**: fix test-module-loading-error for musl (Hugues Malphettes) [#3657](https://github.com/nodejs/node/pull/3657)
* [[`3dc52e99df`](https://github.com/nodejs/node/commit/3dc52e99df)] - **test**: fix test-net-persistent-keepalive for AIX (Imran Iqbal) [#3646](https://github.com/nodejs/node/pull/3646)
* [[`0e8eb66a78`](https://github.com/nodejs/node/commit/0e8eb66a78)] - **test**: fix path to module for repl test on Windows (Michael Cornacchia) [#3608](https://github.com/nodejs/node/pull/3608)
* [[`3aecbc86d2`](https://github.com/nodejs/node/commit/3aecbc86d2)] - **test**: add test-zlib-flush-drain (Myles Borins) [#3534](https://github.com/nodejs/node/pull/3534)
* [[`542d05cbe1`](https://github.com/nodejs/node/commit/542d05cbe1)] - **test**: enhance fs-watch-recursive test (Sakthipriyan Vairamani) [#2599](https://github.com/nodejs/node/pull/2599)
* [[`0eb0119d64`](https://github.com/nodejs/node/commit/0eb0119d64)] - **tls**: Use SHA1 for sessionIdContext in FIPS mode (Stefan Budeanu) [#3755](https://github.com/nodejs/node/pull/3755)
* [[`c10c08604c`](https://github.com/nodejs/node/commit/c10c08604c)] - **tls**: remove util and calls to util.format (Myles Borins) [#3456](https://github.com/nodejs/node/pull/3456)
* [[`a558a570c0`](https://github.com/nodejs/node/commit/a558a570c0)] - **util**: use regexp instead of str.replace().join() (qinjia) [#3689](https://github.com/nodejs/node/pull/3689)
* [[`47bb94a0c3`](https://github.com/nodejs/node/commit/47bb94a0c3)] - **zlib**: only apply drain listener if given callback (Craig Cavalier) [#3534](https://github.com/nodejs/node/pull/3534)
* [[`4733a60158`](https://github.com/nodejs/node/commit/4733a60158)] - **zlib**: pass kind to recursive calls to flush (Myles Borins) [#3534](https://github.com/nodejs/node/pull/3534)

## 2015-12-04, Version 4.2.3 'Argon' (LTS), @rvagg

Security Update

### Notable changes

* **http**: Fix CVE-2015-8027, a bug whereby an HTTP socket may no longer have a parser associated with it but a pipelined request attempts to trigger a pause or resume on the non-existent parser, a potential denial-of-service vulnerability. (Fedor Indutny)
* **openssl**: Upgrade to 1.0.2e, containing fixes for:
  - CVE-2015-3193 "BN_mod_exp may produce incorrect results on x86_64", an attack may be possible against a Node.js TLS server using DHE key exchange. Details are available at <http://openssl.org/news/secadv/20151203.txt>.
  - CVE-2015-3194 "Certificate verify crash with missing PSS parameter", a potential denial-of-service vector for Node.js TLS servers using client certificate authentication; TLS clients are also impacted. Details are available at <http://openssl.org/news/secadv/20151203.txt>.
  (Shigeki Ohtsu) [#4134](https://github.com/nodejs/node/pull/4134)
* **v8**: Backport fix for CVE-2015-6764, a bug in `JSON.stringify()` that can result in out-of-bounds reads for arrays. (Ben Noordhuis)

### Known issues

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`49bbd563be`](https://github.com/nodejs/node/commit/49bbd563be)] - **deps**: upgrade openssl sources to 1.0.2e (Shigeki Ohtsu) [#4134](https://github.com/nodejs/node/pull/4134)
* [[`9a063fd492`](https://github.com/nodejs/node/commit/9a063fd492)] - **deps**: backport a7e50a5 from upstream v8 (Ben Noordhuis)
* [[`07233206e9`](https://github.com/nodejs/node/commit/07233206e9)] - **deps**: backport 6df9a1d from upstream v8 (Ben Noordhuis)
* [[`1c8e6de78e`](https://github.com/nodejs/node/commit/1c8e6de78e)] - **http**: fix pipeline regression (Fedor Indutny)

## 2015-11-03, Version 4.2.2 'Argon' (LTS), @jasnell

### Notable changes

This is an LTS maintenance release that addresses a number of issues:

* [[`1d0f2cbf87`](https://github.com/nodejs/node/commit/1d0f2cbf87)] - **buffer**: fix value check for writeUInt{B,L}E (Trevor Norris) [#3500](https://github.com/nodejs/node/pull/3500)
* [[`2a45b72b4a`](https://github.com/nodejs/node/commit/2a45b72b4a)] - **buffer**: don't CHECK on zero-sized realloc (Ben Noordhuis) [#3499](https://github.com/nodejs/node/pull/3499)
* [[`a6469e901a`](https://github.com/nodejs/node/commit/a6469e901a)] - **deps**: backport 010897c from V8 upstream (Ali Ijaz Sheikh) [#3520](https://github.com/nodejs/node/pull/3520)
* [[`cadee67c25`](https://github.com/nodejs/node/commit/cadee67c25)] - **deps**: backport 8d6a228 from the v8's upstream (Fedor Indutny) [#3549](https://github.com/nodejs/node/pull/3549)
* [[`46c8c94055`](https://github.com/nodejs/node/commit/46c8c94055)] - **fs**: reduced duplicate code in fs.write() (ronkorving) [#2947](https://github.com/nodejs/node/pull/2947)
* [[`0427cdf094`](https://github.com/nodejs/node/commit/0427cdf094)] - **http**: fix stalled pipeline bug (Fedor Indutny) [#3342](https://github.com/nodejs/node/pull/3342)
* [[`2109708186`](https://github.com/nodejs/node/commit/2109708186)] - **lib**: fix cluster handle leak (Rich Trott) [#3510](https://github.com/nodejs/node/pull/3510)
* [[`f49c7c6955`](https://github.com/nodejs/node/commit/f49c7c6955)] - **lib**: avoid REPL exit on completion error (Rich Trott) [#3358](https://github.com/nodejs/node/pull/3358)
* [[`8a2c4aeeaa`](https://github.com/nodejs/node/commit/8a2c4aeeaa)] - **repl**: handle comments properly (Sakthipriyan Vairamani) [#3515](https://github.com/nodejs/node/pull/3515)
* [[`a04408acce`](https://github.com/nodejs/node/commit/a04408acce)] - **repl**: limit persistent history correctly on load (Jeremiah Senkpiel) [#2356](https://github.com/nodejs/node/pull/2356)
* [[`3bafe1a59b`](https://github.com/nodejs/node/commit/3bafe1a59b)] - **src**: fix race condition in debug signal on exit (Ben Noordhuis) [#3528](https://github.com/nodejs/node/pull/3528)
* [[`fe01d0df7a`](https://github.com/nodejs/node/commit/fe01d0df7a)] - **src**: fix exception message encoding on Windows (Brian White) [#3288](https://github.com/nodejs/node/pull/3288)
* [[`4bac5d9ddf`](https://github.com/nodejs/node/commit/4bac5d9ddf)] - **stream**: avoid unnecessary concat of a single buffer. (Calvin Metcalf) [#3300](https://github.com/nodejs/node/pull/3300)
* [[`8d78d687d5`](https://github.com/nodejs/node/commit/8d78d687d5)] - **timers**: reuse timer in `setTimeout().unref()` (Fedor Indutny) [#3407](https://github.com/nodejs/node/pull/3407)
* [[`e69c869399`](https://github.com/nodejs/node/commit/e69c869399)] - **tls**: TLSSocket options default isServer false (Yuval Brik) [#2614](https://github.com/nodejs/node/pull/2614)

### Known issues

* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`1d0f2cbf87`](https://github.com/nodejs/node/commit/1d0f2cbf87)] - **buffer**: fix value check for writeUInt{B,L}E (Trevor Norris) [#3500](https://github.com/nodejs/node/pull/3500)
* [[`2a45b72b4a`](https://github.com/nodejs/node/commit/2a45b72b4a)] - **buffer**: don't CHECK on zero-sized realloc (Ben Noordhuis) [#3499](https://github.com/nodejs/node/pull/3499)
* [[`dc655e1dd2`](https://github.com/nodejs/node/commit/dc655e1dd2)] - **build**: rectify --link-module help text (P.S.V.R) [#3379](https://github.com/nodejs/node/pull/3379)
* [[`a6469e901a`](https://github.com/nodejs/node/commit/a6469e901a)] - **deps**: backport 010897c from V8 upstream (Ali Ijaz Sheikh) [#3520](https://github.com/nodejs/node/pull/3520)
* [[`cadee67c25`](https://github.com/nodejs/node/commit/cadee67c25)] - **deps**: backport 8d6a228 from the v8's upstream (Fedor Indutny) [#3549](https://github.com/nodejs/node/pull/3549)
* [[`1ebd35550b`](https://github.com/nodejs/node/commit/1ebd35550b)] - **doc**: fix typos in changelog (reggi) [#3291](https://github.com/nodejs/node/pull/3291)
* [[`fbd93d4c1c`](https://github.com/nodejs/node/commit/fbd93d4c1c)] - **doc**: more use-cases for promise events (Domenic Denicola) [#3438](https://github.com/nodejs/node/pull/3438)
* [[`6ceb9af407`](https://github.com/nodejs/node/commit/6ceb9af407)] - **doc**: remove old note, 'cluster' is marked stable (Balázs Galambosi) [#3314](https://github.com/nodejs/node/pull/3314)
* [[`a5f0d64ddc`](https://github.com/nodejs/node/commit/a5f0d64ddc)] - **doc**: createServer's key option can be an array (Sakthipriyan Vairamani) [#3123](https://github.com/nodejs/node/pull/3123)
* [[`317e0ec6b3`](https://github.com/nodejs/node/commit/317e0ec6b3)] - **doc**: binary encoding is not deprecated (Trevor Norris) [#3441](https://github.com/nodejs/node/pull/3441)
* [[`b422f6ee1a`](https://github.com/nodejs/node/commit/b422f6ee1a)] - **doc**: mention the behaviour if URL is invalid (Sakthipriyan Vairamani) [#2966](https://github.com/nodejs/node/pull/2966)
* [[`bc29aad22b`](https://github.com/nodejs/node/commit/bc29aad22b)] - **doc**: fix indent in tls resumption example (Roman Reiss) [#3372](https://github.com/nodejs/node/pull/3372)
* [[`313877bd8f`](https://github.com/nodejs/node/commit/313877bd8f)] - **doc**: fix typo in changelog (Timothy Gu) [#3353](https://github.com/nodejs/node/pull/3353)
* [[`4be432862a`](https://github.com/nodejs/node/commit/4be432862a)] - **doc**: show keylen in pbkdf2 as a byte length (calebboyd) [#3334](https://github.com/nodejs/node/pull/3334)
* [[`23a1140ddb`](https://github.com/nodejs/node/commit/23a1140ddb)] - **doc**: add information about Assert behavior and maintenance (Rich Trott) [#3330](https://github.com/nodejs/node/pull/3330)
* [[`e04cb1e1fc`](https://github.com/nodejs/node/commit/e04cb1e1fc)] - **doc**: clarify API buffer.concat (Martii) [#3255](https://github.com/nodejs/node/pull/3255)
* [[`eae714c370`](https://github.com/nodejs/node/commit/eae714c370)] - **doc**: clarify the use of `option.detached` (Kyle Smith) [#3250](https://github.com/nodejs/node/pull/3250)
* [[`b884899e67`](https://github.com/nodejs/node/commit/b884899e67)] - **doc**: label v4.2.1 as LTS in changelog heading (Phillip Johnsen) [#3360](https://github.com/nodejs/node/pull/3360)
* [[`9120a04981`](https://github.com/nodejs/node/commit/9120a04981)] - **docs**: add missing shell option to execSync (fansworld-claudio) [#3440](https://github.com/nodejs/node/pull/3440)
* [[`46c8c94055`](https://github.com/nodejs/node/commit/46c8c94055)] - **fs**: reduced duplicate code in fs.write() (ronkorving) [#2947](https://github.com/nodejs/node/pull/2947)
* [[`0427cdf094`](https://github.com/nodejs/node/commit/0427cdf094)] - **http**: fix stalled pipeline bug (Fedor Indutny) [#3342](https://github.com/nodejs/node/pull/3342)
* [[`2109708186`](https://github.com/nodejs/node/commit/2109708186)] - **lib**: fix cluster handle leak (Rich Trott) [#3510](https://github.com/nodejs/node/pull/3510)
* [[`f49c7c6955`](https://github.com/nodejs/node/commit/f49c7c6955)] - **lib**: avoid REPL exit on completion error (Rich Trott) [#3358](https://github.com/nodejs/node/pull/3358)
* [[`8a2c4aeeaa`](https://github.com/nodejs/node/commit/8a2c4aeeaa)] - **repl**: handle comments properly (Sakthipriyan Vairamani) [#3515](https://github.com/nodejs/node/pull/3515)
* [[`a04408acce`](https://github.com/nodejs/node/commit/a04408acce)] - **repl**: limit persistent history correctly on load (Jeremiah Senkpiel) [#2356](https://github.com/nodejs/node/pull/2356)
* [[`5d1f1c5fa8`](https://github.com/nodejs/node/commit/5d1f1c5fa8)] - **src**: wrap source before doing syntax check (Evan Lucas) [#3587](https://github.com/nodejs/node/pull/3587)
* [[`3bafe1a59b`](https://github.com/nodejs/node/commit/3bafe1a59b)] - **src**: fix race condition in debug signal on exit (Ben Noordhuis) [#3528](https://github.com/nodejs/node/pull/3528)
* [[`fe01d0df7a`](https://github.com/nodejs/node/commit/fe01d0df7a)] - **src**: fix exception message encoding on Windows (Brian White) [#3288](https://github.com/nodejs/node/pull/3288)
* [[`4bac5d9ddf`](https://github.com/nodejs/node/commit/4bac5d9ddf)] - **stream**: avoid unnecessary concat of a single buffer. (Calvin Metcalf) [#3300](https://github.com/nodejs/node/pull/3300)
* [[`117fb47a16`](https://github.com/nodejs/node/commit/117fb47a16)] - **stream**: fix signature of _write() in a comment (Fábio Santos) [#3248](https://github.com/nodejs/node/pull/3248)
* [[`c563a34427`](https://github.com/nodejs/node/commit/c563a34427)] - **test**: split independent tests into separate files (Rich Trott) [#3548](https://github.com/nodejs/node/pull/3548)
* [[`3f62952d42`](https://github.com/nodejs/node/commit/3f62952d42)] - **test**: add node::MakeCallback() test coverage (Ben Noordhuis) [#3478](https://github.com/nodejs/node/pull/3478)
* [[`6b75f10d8a`](https://github.com/nodejs/node/commit/6b75f10d8a)] - **test**: use port number from env in tls socket test (Stefan Budeanu) [#3557](https://github.com/nodejs/node/pull/3557)
* [[`39ff44e94f`](https://github.com/nodejs/node/commit/39ff44e94f)] - **test**: fix heap-profiler link error LNK1194 on win (Junliang Yan) [#3572](https://github.com/nodejs/node/pull/3572)
* [[`a2786dd408`](https://github.com/nodejs/node/commit/a2786dd408)] - **test**: fix missing unistd.h on windows (Junliang Yan) [#3532](https://github.com/nodejs/node/pull/3532)
* [[`5e6f7c9a23`](https://github.com/nodejs/node/commit/5e6f7c9a23)] - **test**: add regression test for --debug-brk -e 0 (Ben Noordhuis) [#3585](https://github.com/nodejs/node/pull/3585)
* [[`7cad182cb6`](https://github.com/nodejs/node/commit/7cad182cb6)] - **test**: port domains regression test from v0.10 (Jonas Dohse) [#3356](https://github.com/nodejs/node/pull/3356)
* [[`78d854c6ce`](https://github.com/nodejs/node/commit/78d854c6ce)] - **test**: remove util from common (Rich Trott) [#3324](https://github.com/nodejs/node/pull/3324)
* [[`c566c8b8c0`](https://github.com/nodejs/node/commit/c566c8b8c0)] - **test**: remove util properties from common (Rich Trott) [#3304](https://github.com/nodejs/node/pull/3304)
* [[`eb7c3fb2f4`](https://github.com/nodejs/node/commit/eb7c3fb2f4)] - **test**: split up buffer tests for reliability (Rich Trott) [#3323](https://github.com/nodejs/node/pull/3323)
* [[`b398a85e19`](https://github.com/nodejs/node/commit/b398a85e19)] - **test**: parallelize long-running test (Rich Trott) [#3287](https://github.com/nodejs/node/pull/3287)
* [[`b5f3b4956b`](https://github.com/nodejs/node/commit/b5f3b4956b)] - **test**: change call to deprecated util.isError() (Rich Trott) [#3084](https://github.com/nodejs/node/pull/3084)
* [[`32149cacb5`](https://github.com/nodejs/node/commit/32149cacb5)] - **test**: improve tests for util.inherits (Michaël Zasso) [#3507](https://github.com/nodejs/node/pull/3507)
* [[`5be686fab8`](https://github.com/nodejs/node/commit/5be686fab8)] - **test**: print helpful err msg on test-dns-ipv6.js (Junliang Yan) [#3501](https://github.com/nodejs/node/pull/3501)
* [[`0429131e32`](https://github.com/nodejs/node/commit/0429131e32)] - **test**: fix domain with abort-on-uncaught on PPC (Julien Gilli) [#3354](https://github.com/nodejs/node/pull/3354)
* [[`788106eee9`](https://github.com/nodejs/node/commit/788106eee9)] - **test**: cleanup, improve repl-persistent-history (Jeremiah Senkpiel) [#2356](https://github.com/nodejs/node/pull/2356)
* [[`ea58fa0bac`](https://github.com/nodejs/node/commit/ea58fa0bac)] - **test**: add Symbol test for assert.deepEqual() (Rich Trott) [#3327](https://github.com/nodejs/node/pull/3327)
* [[`d409ac473b`](https://github.com/nodejs/node/commit/d409ac473b)] - **test**: disable test-tick-processor - aix and be ppc (Michael Dawson) [#3491](https://github.com/nodejs/node/pull/3491)
* [[`c1623039dd`](https://github.com/nodejs/node/commit/c1623039dd)] - **test**: harden test-child-process-fork-regr-gh-2847 (Michael Dawson) [#3459](https://github.com/nodejs/node/pull/3459)
* [[`3bb4437abb`](https://github.com/nodejs/node/commit/3bb4437abb)] - **test**: fix test-net-keepalive for AIX (Imran Iqbal) [#3458](https://github.com/nodejs/node/pull/3458)
* [[`af55641a69`](https://github.com/nodejs/node/commit/af55641a69)] - **test**: wrap assert.fail when passed to callback (Myles Borins) [#3453](https://github.com/nodejs/node/pull/3453)
* [[`7c7ef01e65`](https://github.com/nodejs/node/commit/7c7ef01e65)] - **test**: skip test-dns-ipv6.js if ipv6 is unavailable (Junliang Yan) [#3444](https://github.com/nodejs/node/pull/3444)
* [[`a4d1510ba4`](https://github.com/nodejs/node/commit/a4d1510ba4)] - **test**: repl-persistent-history is no longer flaky (Jeremiah Senkpiel) [#3437](https://github.com/nodejs/node/pull/3437)
* [[`a5d968b8a2`](https://github.com/nodejs/node/commit/a5d968b8a2)] - **test**: fix flaky test-child-process-emfile (Rich Trott) [#3430](https://github.com/nodejs/node/pull/3430)
* [[`eac2acca76`](https://github.com/nodejs/node/commit/eac2acca76)] - **test**: remove flaky status from eval_messages test (Rich Trott) [#3420](https://github.com/nodejs/node/pull/3420)
* [[`155c778584`](https://github.com/nodejs/node/commit/155c778584)] - **test**: fix flaky test for symlinks (Rich Trott) [#3418](https://github.com/nodejs/node/pull/3418)
* [[`74eb632483`](https://github.com/nodejs/node/commit/74eb632483)] - **test**: apply correct assert.fail() arguments (Rich Trott) [#3378](https://github.com/nodejs/node/pull/3378)
* [[`0a4323dd82`](https://github.com/nodejs/node/commit/0a4323dd82)] - **test**: replace util with backtick strings (Myles Borins) [#3359](https://github.com/nodejs/node/pull/3359)
* [[`93847694ec`](https://github.com/nodejs/node/commit/93847694ec)] - **test**: add test-child-process-emfile fail message (Rich Trott) [#3335](https://github.com/nodejs/node/pull/3335)
* [[`8d78d687d5`](https://github.com/nodejs/node/commit/8d78d687d5)] - **timers**: reuse timer in `setTimeout().unref()` (Fedor Indutny) [#3407](https://github.com/nodejs/node/pull/3407)
* [[`e69c869399`](https://github.com/nodejs/node/commit/e69c869399)] - **tls**: TLSSocket options default isServer false (Yuval Brik) [#2614](https://github.com/nodejs/node/pull/2614)
* [[`0b32bbbf69`](https://github.com/nodejs/node/commit/0b32bbbf69)] - **v8**: pull fix for builtin code size on PPC (Michael Dawson) [#3474](https://github.com/nodejs/node/pull/3474)


## 2015-10-13, Version 4.2.1 'Argon' (LTS), @jasnell

### Notable changes

* Includes fixes for two regressions
  - Assertion error in WeakCallback  - see [#3329](https://github.com/nodejs/node/pull/3329)
  - Undefined timeout regression - see [#3331](https://github.com/nodejs/node/pull/3331)

### Known issues

* When a server queues a large amount of data to send to a client over a pipelined HTTP connection, the underlying socket may be destroyed. See [#3332](https://github.com/nodejs/node/issues/3332) and [#3342](https://github.com/nodejs/node/pull/3342).
* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`b3cbd13340`](https://github.com/nodejs/node/commit/b3cbd13340)] - **buffer**: fix assertion error in WeakCallback (Fedor Indutny) [#3329](https://github.com/nodejs/node/pull/3329)
* [[`102cb7288c`](https://github.com/nodejs/node/commit/102cb7288c)] - **doc**: label v4.2.0 as LTS in changelog heading (Rod Vagg) [#3343](https://github.com/nodejs/node/pull/3343)
* [[`c245a199a7`](https://github.com/nodejs/node/commit/c245a199a7)] - **lib**: fix undefined timeout regression (Ryan Graham) [#3331](https://github.com/nodejs/node/pull/3331)

## 2015-10-07, Version 4.2.0 'Argon' (LTS), @jasnell

### Notable changes

The first Node.js LTS release! See https://github.com/nodejs/LTS/ for details of the LTS process.

* **icu**: Updated to version 56 with significant performance improvements (Steven R. Loomis) [#3281](https://github.com/nodejs/node/pull/3281)
* **node**:
  - Added new `-c` (or `--check`) command line argument for checking script syntax without executing the code (Dave Eddy) [#2411](https://github.com/nodejs/node/pull/2411)
  - Added `process.versions.icu` to hold the current ICU library version (Evan Lucas) [#3102](https://github.com/nodejs/node/pull/3102)
  - Added `process.release.lts` to hold the current LTS codename when the binary is from an active LTS release line (Rod Vagg) [#3212](https://github.com/nodejs/node/pull/3212)
* **npm**: Upgraded to npm 2.14.7 from 2.14.4, see [release notes](https://github.com/npm/npm/releases/tag/v2.14.7) for full details (Kat Marchán) [#3299](https://github.com/nodejs/node/pull/3299)

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits


* [[`8383c4fe00`](https://github.com/nodejs/node/commit/8383c4fe00)] - **assert**: support arrow functions in .throws() (Ben Noordhuis) [#3276](https://github.com/nodejs/node/pull/3276)
* [[`3eaa593a32`](https://github.com/nodejs/node/commit/3eaa593a32)] - **async_wrap**: correctly pass parent to init callback (Trevor Norris) [#3216](https://github.com/nodejs/node/pull/3216)
* [[`54795620f6`](https://github.com/nodejs/node/commit/54795620f6)] - **buffer**: don't abort on prototype getters (Trevor Norris) [#3302](https://github.com/nodejs/node/pull/3302)
* [[`660f7591c8`](https://github.com/nodejs/node/commit/660f7591c8)] - **buffer**: FreeCallback should be tied to ArrayBuffer (Fedor Indutny) [#3198](https://github.com/nodejs/node/pull/3198)
* [[`651a5b51eb`](https://github.com/nodejs/node/commit/651a5b51eb)] - **buffer**: only check if instance is Uint8Array (Trevor Norris) [#3080](https://github.com/nodejs/node/pull/3080)
* [[`d5a1b1ad7c`](https://github.com/nodejs/node/commit/d5a1b1ad7c)] - **buffer**: clean up usage of __proto__ (Trevor Norris) [#3080](https://github.com/nodejs/node/pull/3080)
* [[`af24376e18`](https://github.com/nodejs/node/commit/af24376e18)] - **build**: Intl: deps: bump ICU to 56.1 (GA) (Steven R. Loomis) [#3281](https://github.com/nodejs/node/pull/3281)
* [[`9136359d57`](https://github.com/nodejs/node/commit/9136359d57)] - **build**: make icu download path customizable (Johan Bergström) [#3200](https://github.com/nodejs/node/pull/3200)
* [[`b3c5ad10a8`](https://github.com/nodejs/node/commit/b3c5ad10a8)] - **build**: add --with-arm-fpu option (Jérémy Lal) [#3228](https://github.com/nodejs/node/pull/3228)
* [[`f00f3268e4`](https://github.com/nodejs/node/commit/f00f3268e4)] - **build**: intl: avoid 'duplicate main()' on ICU 56 (Steven R. Loomis) [#3066](https://github.com/nodejs/node/pull/3066)
* [[`071c72a6a3`](https://github.com/nodejs/node/commit/071c72a6a3)] - **deps**: upgrade to npm 2.14.7 (Kat Marchán) [#3299](https://github.com/nodejs/node/pull/3299)
* [[`8b50e95f06`](https://github.com/nodejs/node/commit/8b50e95f06)] - **(SEMVER-MINOR)** **deps**: backport 1ee712a from V8 upstream (Julien Gilli) [#3036](https://github.com/nodejs/node/pull/3036)
* [[`747271372f`](https://github.com/nodejs/node/commit/747271372f)] - **doc**: update the assert module summary (David Boivin) [#2799](https://github.com/nodejs/node/pull/2799)
* [[`0d506556b0`](https://github.com/nodejs/node/commit/0d506556b0)] - **doc**: replace node-gyp link with nodejs/node-gyp (Roman Klauke) [#3320](https://github.com/nodejs/node/pull/3320)
* [[`40a159e4f4`](https://github.com/nodejs/node/commit/40a159e4f4)] - **doc**: Amend capitalization of word JavaScript (Dave Hodder) [#3285](https://github.com/nodejs/node/pull/3285)
* [[`6dd34761fd`](https://github.com/nodejs/node/commit/6dd34761fd)] - **doc**: add method links in dns.markdown (Alejandro Oviedo) [#3196](https://github.com/nodejs/node/pull/3196)
* [[`333e8336be`](https://github.com/nodejs/node/commit/333e8336be)] - **doc**: add method links in child_process.markdown (Alejandro Oviedo) [#3186](https://github.com/nodejs/node/pull/3186)
* [[`0cfc6d39ca`](https://github.com/nodejs/node/commit/0cfc6d39ca)] - **doc**: recommend Infinity on emitter.setMaxListeners (Jason Karns) [#2559](https://github.com/nodejs/node/pull/2559)
* [[`d4fc6d93ef`](https://github.com/nodejs/node/commit/d4fc6d93ef)] - **doc**: add help repo link to CONTRIBUTING.md (Doug Shamoo) [#3233](https://github.com/nodejs/node/pull/3233)
* [[`28aac7f19d`](https://github.com/nodejs/node/commit/28aac7f19d)] - **doc**: add TLS session resumption example (Roman Reiss) [#3147](https://github.com/nodejs/node/pull/3147)
* [[`365cf22cce`](https://github.com/nodejs/node/commit/365cf22cce)] - **doc**: update AUTHORS list (Rod Vagg) [#3211](https://github.com/nodejs/node/pull/3211)
* [[`d4399613b7`](https://github.com/nodejs/node/commit/d4399613b7)] - **doc**: standardize references to userland (Martial) [#3192](https://github.com/nodejs/node/pull/3192)
* [[`75de258376`](https://github.com/nodejs/node/commit/75de258376)] - **doc**: fix spelling in Buffer documentation (Rod Machen) [#3226](https://github.com/nodejs/node/pull/3226)
* [[`725c7276dd`](https://github.com/nodejs/node/commit/725c7276dd)] - **doc**: fix README.md link to joyent/node intl wiki (Steven R. Loomis) [#3067](https://github.com/nodejs/node/pull/3067)
* [[`4a35ba4966`](https://github.com/nodejs/node/commit/4a35ba4966)] - **(SEMVER-MINOR)** **fs**: include filename in watch errors (charlierudolph) [#2748](https://github.com/nodejs/node/pull/2748)
* [[`2ddbbfd164`](https://github.com/nodejs/node/commit/2ddbbfd164)] - **http**: cork/uncork before flushing pipelined res (Fedor Indutny) [#3172](https://github.com/nodejs/node/pull/3172)
* [[`f638402e2f`](https://github.com/nodejs/node/commit/f638402e2f)] - **http**: add comment about `outputSize` in res/server (Fedor Indutny) [#3128](https://github.com/nodejs/node/pull/3128)
* [[`1850879b0e`](https://github.com/nodejs/node/commit/1850879b0e)] - **js_stream**: prevent abort if isalive doesn't exist (Trevor Norris) [#3282](https://github.com/nodejs/node/pull/3282)
* [[`63644dd1cd`](https://github.com/nodejs/node/commit/63644dd1cd)] - **lib**: remove redundant code, add tests in timers.js (Rich Trott) [#3143](https://github.com/nodejs/node/pull/3143)
* [[`74f443583c`](https://github.com/nodejs/node/commit/74f443583c)] - **module**: use UNC paths when loading native addons (Justin Chase) [#2965](https://github.com/nodejs/node/pull/2965)
* [[`01cb3fc36b`](https://github.com/nodejs/node/commit/01cb3fc36b)] - **net**: don't throw on bytesWritten access (Trevor Norris) [#3305](https://github.com/nodejs/node/pull/3305)
* [[`9d65528b01`](https://github.com/nodejs/node/commit/9d65528b01)] - **(SEMVER-MINOR)** **node**: add -c|--check CLI arg to syntax check script (Dave Eddy) [#2411](https://github.com/nodejs/node/pull/2411)
* [[`42b936e78d`](https://github.com/nodejs/node/commit/42b936e78d)] - **(SEMVER-MINOR)** **src**: add process.release.lts property (Rod Vagg) [#3212](https://github.com/nodejs/node/pull/3212)
* [[`589287b2e3`](https://github.com/nodejs/node/commit/589287b2e3)] - **src**: convert BE-utf16-string to LE before search (Karl Skomski) [#3295](https://github.com/nodejs/node/pull/3295)
* [[`2314378f06`](https://github.com/nodejs/node/commit/2314378f06)] - **src**: fix u-a-free if uv returns err in ASYNC_CALL (Karl Skomski) [#3049](https://github.com/nodejs/node/pull/3049)
* [[`d99336a391`](https://github.com/nodejs/node/commit/d99336a391)] - **(SEMVER-MINOR)** **src**: replace naive search in Buffer::IndexOf (Karl Skomski) [#2539](https://github.com/nodejs/node/pull/2539)
* [[`546e8333ba`](https://github.com/nodejs/node/commit/546e8333ba)] - **(SEMVER-MINOR)** **src**: fix --abort-on-uncaught-exception (Jeremy Whitlock) [#3036](https://github.com/nodejs/node/pull/3036)
* [[`7271cb047c`](https://github.com/nodejs/node/commit/7271cb047c)] - **(SEMVER-MINOR)** **src**: add process.versions.icu (Evan Lucas) [#3102](https://github.com/nodejs/node/pull/3102)
* [[`7b9f78acb2`](https://github.com/nodejs/node/commit/7b9f78acb2)] - **stream**: avoid pause with unpipe in buffered write (Brian White) [#2325](https://github.com/nodejs/node/pull/2325)
* [[`f0f8afd879`](https://github.com/nodejs/node/commit/f0f8afd879)] - **test**: remove common.inspect() (Rich Trott) [#3257](https://github.com/nodejs/node/pull/3257)
* [[`5ca4f6f8bd`](https://github.com/nodejs/node/commit/5ca4f6f8bd)] - **test**: test `util` rather than `common` (Rich Trott) [#3256](https://github.com/nodejs/node/pull/3256)
* [[`7a5ae34345`](https://github.com/nodejs/node/commit/7a5ae34345)] - **test**: refresh temp directory when using pipe (Rich Trott) [#3231](https://github.com/nodejs/node/pull/3231)
* [[`7c85557ef0`](https://github.com/nodejs/node/commit/7c85557ef0)] - **test**: Fix test-fs-read-stream-fd-leak race cond (Junliang Yan) [#3218](https://github.com/nodejs/node/pull/3218)
* [[`26a7ec6960`](https://github.com/nodejs/node/commit/26a7ec6960)] - **test**: fix losing original env vars issue (Junliang Yan) [#3190](https://github.com/nodejs/node/pull/3190)
* [[`e922716192`](https://github.com/nodejs/node/commit/e922716192)] - **test**: remove deprecated error logging (Rich Trott) [#3079](https://github.com/nodejs/node/pull/3079)
* [[`8f29d95a8c`](https://github.com/nodejs/node/commit/8f29d95a8c)] - **test**: report timeout in TapReporter (Karl Skomski) [#2647](https://github.com/nodejs/node/pull/2647)
* [[`2d0fe4c657`](https://github.com/nodejs/node/commit/2d0fe4c657)] - **test**: linting for buffer-free-callback test (Rich Trott) [#3230](https://github.com/nodejs/node/pull/3230)
* [[`70c9e4337e`](https://github.com/nodejs/node/commit/70c9e4337e)] - **test**: make common.js mandatory via linting rule (Rich Trott) [#3157](https://github.com/nodejs/node/pull/3157)
* [[`b7179562aa`](https://github.com/nodejs/node/commit/b7179562aa)] - **test**: load common.js in all tests (Rich Trott) [#3157](https://github.com/nodejs/node/pull/3157)
* [[`bab555a1c1`](https://github.com/nodejs/node/commit/bab555a1c1)] - **test**: speed up stringbytes-external test (Evan Lucas) [#3005](https://github.com/nodejs/node/pull/3005)
* [[`ddf258376d`](https://github.com/nodejs/node/commit/ddf258376d)] - **test**: use normalize() for unicode paths (Roman Reiss) [#3007](https://github.com/nodejs/node/pull/3007)
* [[`46876d519c`](https://github.com/nodejs/node/commit/46876d519c)] - **test**: remove arguments.callee usage (Roman Reiss) [#3167](https://github.com/nodejs/node/pull/3167)
* [[`af10df6108`](https://github.com/nodejs/node/commit/af10df6108)] - **tls**: use parent handle's close callback (Fedor Indutny) [#2991](https://github.com/nodejs/node/pull/2991)
* [[`9c2748bad1`](https://github.com/nodejs/node/commit/9c2748bad1)] - **tools**: remove leftover license boilerplate (Nathan Rajlich) [#3225](https://github.com/nodejs/node/pull/3225)
* [[`5d9f83ff2a`](https://github.com/nodejs/node/commit/5d9f83ff2a)] - **tools**: apply linting to custom rules code (Rich Trott) [#3195](https://github.com/nodejs/node/pull/3195)
* [[`18a8b2ec73`](https://github.com/nodejs/node/commit/18a8b2ec73)] - **tools**: remove unused gflags module (Ben Noordhuis) [#3220](https://github.com/nodejs/node/pull/3220)
* [[`e0fffca836`](https://github.com/nodejs/node/commit/e0fffca836)] - **util**: fix for inspecting promises (Evan Lucas) [#3221](https://github.com/nodejs/node/pull/3221)
* [[`8dfdee3733`](https://github.com/nodejs/node/commit/8dfdee3733)] - **util**: correctly inspect Map/Set Iterators (Evan Lucas) [#3119](https://github.com/nodejs/node/pull/3119)
* [[`b5c51fdba0`](https://github.com/nodejs/node/commit/b5c51fdba0)] - **util**: fix check for Array constructor (Evan Lucas) [#3119](https://github.com/nodejs/node/pull/3119)

## 2015-10-05, Version 4.1.2 (Stable), @rvagg

### Notable changes

* **http**:
  - Fix out-of-order 'finish' event bug in pipelining that can abort execution, fixes DoS vulnerability [CVE-2015-7384](https://github.com/nodejs/node/issues/3138) (Fedor Indutny) [#3128](https://github.com/nodejs/node/pull/3128)
  - Account for pending response data instead of just the data on the current request to decide whether pause the socket or not (Fedor Indutny) [#3128](https://github.com/nodejs/node/pull/3128)
* **libuv**: Upgraded from v1.7.4 to v1.7.5, see [release notes](https://github.com/libuv/libuv/releases/tag/v1.7.5) for details (Saúl Ibarra Corretgé) [#3010](https://github.com/nodejs/node/pull/3010)
  - A better rwlock implementation for all Windows versions
  - Improved AIX support
* **v8**:
  - Upgraded from v4.5.103.33 to v4.5.103.35 (Ali Ijaz Sheikh) [#3117](https://github.com/nodejs/node/pull/3117)
  - Backported [f782159](https://codereview.chromium.org/1367123003) from v8's upstream to help speed up Promise introspection (Ben Noordhuis) [#3130](https://github.com/nodejs/node/pull/3130)
  - Backported [c281c15](https://codereview.chromium.org/1363683002) from v8's upstream to add JSTypedArray length in post-mortem metadata (Julien Gilli) [#3031](https://github.com/nodejs/node/pull/3031)

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`39b8730e8b`](https://github.com/nodejs/node/commit/39b8730e8b)] - **async_wrap**: ensure all objects have internal field (Trevor Norris) [#3139](https://github.com/nodejs/node/pull/3139)
* [[`99e66074d7`](https://github.com/nodejs/node/commit/99e66074d7)] - **async_wrap**: update providers and add test (Trevor Norris) [#3139](https://github.com/nodejs/node/pull/3139)
* [[`7a58157d4e`](https://github.com/nodejs/node/commit/7a58157d4e)] - **benchmark**: update comment in common.js (Minwoo Jung) [#2399](https://github.com/nodejs/node/pull/2399)
* [[`9e9bfa4dc0`](https://github.com/nodejs/node/commit/9e9bfa4dc0)] - **build**: iojs -> nodejs of release-urlbase (P.S.V.R) [#3015](https://github.com/nodejs/node/pull/3015)
* [[`8335ec7191`](https://github.com/nodejs/node/commit/8335ec7191)] - **build**: fix some typos inside the configure script (P.S.V.R) [#3016](https://github.com/nodejs/node/pull/3016)
* [[`d6ac547d5d`](https://github.com/nodejs/node/commit/d6ac547d5d)] - **build,win**: fix node.exe resource version (João Reis) [#3053](https://github.com/nodejs/node/pull/3053)
* [[`798dad24f4`](https://github.com/nodejs/node/commit/798dad24f4)] - **child_process**: `null` channel handle on close (Fedor Indutny) [#3041](https://github.com/nodejs/node/pull/3041)
* [[`e5615854ea`](https://github.com/nodejs/node/commit/e5615854ea)] - **contextify**: use CHECK instead of `if` (Oguz Bastemur) [#3125](https://github.com/nodejs/node/pull/3125)
* [[`f055a66a38`](https://github.com/nodejs/node/commit/f055a66a38)] - **crypto**: enable FIPS only when configured with it (Fedor Indutny) [#3153](https://github.com/nodejs/node/pull/3153)
* [[`4c8d96bc30`](https://github.com/nodejs/node/commit/4c8d96bc30)] - **crypto**: add more keylen sanity checks in pbkdf2 (Johann) [#3029](https://github.com/nodejs/node/pull/3029)
* [[`4c5940776c`](https://github.com/nodejs/node/commit/4c5940776c)] - **deps**: upgrade libuv to 1.7.5 (Saúl Ibarra Corretgé) [#3010](https://github.com/nodejs/node/pull/3010)
* [[`5a9e795577`](https://github.com/nodejs/node/commit/5a9e795577)] - **deps**: upgrade V8 to 4.5.103.35 (Ali Ijaz Sheikh) [#3117](https://github.com/nodejs/node/pull/3117)
* [[`925b29f959`](https://github.com/nodejs/node/commit/925b29f959)] - **deps**: backport f782159 from v8's upstream (Ben Noordhuis) [#3130](https://github.com/nodejs/node/pull/3130)
* [[`039f73fa83`](https://github.com/nodejs/node/commit/039f73fa83)] - **deps**: remove and gitignore .bin directory (Ben Noordhuis) [#3004](https://github.com/nodejs/node/pull/3004)
* [[`5fbb24812d`](https://github.com/nodejs/node/commit/5fbb24812d)] - **deps**: backport c281c15 from V8's upstream (Julien Gilli) [#3031](https://github.com/nodejs/node/pull/3031)
* [[`6ee5d0f69f`](https://github.com/nodejs/node/commit/6ee5d0f69f)] - **dns**: add missing exports.BADNAME (Roman Reiss) [#3051](https://github.com/nodejs/node/pull/3051)
* [[`f92aee7170`](https://github.com/nodejs/node/commit/f92aee7170)] - **doc**: fix outdated 'try/catch' statement in sync (Minwoo Jung) [#3087](https://github.com/nodejs/node/pull/3087)
* [[`c7161f39e8`](https://github.com/nodejs/node/commit/c7161f39e8)] - **doc**: add TSC meeting minutes 2015-09-16 (Rod Vagg) [#3023](https://github.com/nodejs/node/pull/3023)
* [[`928166c4a8`](https://github.com/nodejs/node/commit/928166c4a8)] - **doc**: copyedit fs.watch() information (Rich Trott) [#3097](https://github.com/nodejs/node/pull/3097)
* [[`75d5dcea76`](https://github.com/nodejs/node/commit/75d5dcea76)] - **doc**: jenkins-iojs.nodesource.com -> ci.nodejs.org (Michał Gołębiowski) [#2886](https://github.com/nodejs/node/pull/2886)
* [[`5c3f50b21d`](https://github.com/nodejs/node/commit/5c3f50b21d)] - **doc**: rearrange execSync and execFileSync (Laurent Fortin) [#2940](https://github.com/nodejs/node/pull/2940)
* [[`4fc33ac11a`](https://github.com/nodejs/node/commit/4fc33ac11a)] - **doc**: make execFileSync in line with execFile (Laurent Fortin) [#2940](https://github.com/nodejs/node/pull/2940)
* [[`a366e84b17`](https://github.com/nodejs/node/commit/a366e84b17)] - **doc**: fix typos in cluster & errors (reggi) [#3011](https://github.com/nodejs/node/pull/3011)
* [[`52031e1bf1`](https://github.com/nodejs/node/commit/52031e1bf1)] - **doc**: switch LICENSE from closure-linter to eslint (P.S.V.R) [#3018](https://github.com/nodejs/node/pull/3018)
* [[`b28f6a53bc`](https://github.com/nodejs/node/commit/b28f6a53bc)] - **docs**: Clarify assert.doesNotThrow behavior (Fabio Oliveira) [#2807](https://github.com/nodejs/node/pull/2807)
* [[`99943e189d`](https://github.com/nodejs/node/commit/99943e189d)] - **http**: fix out-of-order 'finish' bug in pipelining (Fedor Indutny) [#3128](https://github.com/nodejs/node/pull/3128)
* [[`fb7a491d1c`](https://github.com/nodejs/node/commit/fb7a491d1c)] - **http_server**: pause socket properly (Fedor Indutny) [#3128](https://github.com/nodejs/node/pull/3128)
* [[`a0b35bfcf3`](https://github.com/nodejs/node/commit/a0b35bfcf3)] - **i18n**: add caller to removal list for bidi in ICU55 (Michael Dawson) [#3115](https://github.com/nodejs/node/pull/3115)
* [[`ac2bce0b0c`](https://github.com/nodejs/node/commit/ac2bce0b0c)] - **path**: improve posixSplitPath performance (Evan Lucas) [#3034](https://github.com/nodejs/node/pull/3034)
* [[`37cdeafa2f`](https://github.com/nodejs/node/commit/37cdeafa2f)] - **smalloc**: remove module (Brendan Ashworth) [#3099](https://github.com/nodejs/node/pull/3099)
* [[`5ec5d0aa8b`](https://github.com/nodejs/node/commit/5ec5d0aa8b)] - **src**: internalize binding function property names (Ben Noordhuis) [#3060](https://github.com/nodejs/node/pull/3060)
* [[`c8175fc2af`](https://github.com/nodejs/node/commit/c8175fc2af)] - **src**: internalize per-isolate string properties (Ben Noordhuis) [#3060](https://github.com/nodejs/node/pull/3060)
* [[`9a593abc47`](https://github.com/nodejs/node/commit/9a593abc47)] - **src**: include signal.h in util.h (Cheng Zhao) [#3058](https://github.com/nodejs/node/pull/3058)
* [[`fde0c6f321`](https://github.com/nodejs/node/commit/fde0c6f321)] - **src**: fix function and variable names in comments (Sakthipriyan Vairamani) [#3039](https://github.com/nodejs/node/pull/3039)
* [[`1cc7b41ba4`](https://github.com/nodejs/node/commit/1cc7b41ba4)] - **stream_wrap**: support empty `TryWrite`s (Fedor Indutny) [#3128](https://github.com/nodejs/node/pull/3128)
* [[`9faf4c6fcf`](https://github.com/nodejs/node/commit/9faf4c6fcf)] - **test**: load common.js to test for global leaks (Rich Trott) [#3095](https://github.com/nodejs/node/pull/3095)
* [[`0858c86374`](https://github.com/nodejs/node/commit/0858c86374)] - **test**: fix invalid variable name (Sakthipriyan Vairamani) [#3150](https://github.com/nodejs/node/pull/3150)
* [[`1167171004`](https://github.com/nodejs/node/commit/1167171004)] - **test**: change calls to deprecated util.print() (Rich Trott) [#3083](https://github.com/nodejs/node/pull/3083)
* [[`5ada45bf28`](https://github.com/nodejs/node/commit/5ada45bf28)] - **test**: replace deprecated util.debug() calls (Rich Trott) [#3082](https://github.com/nodejs/node/pull/3082)
* [[`d8ab4e185d`](https://github.com/nodejs/node/commit/d8ab4e185d)] - **util**: optimize promise introspection (Ben Noordhuis) [#3130](https://github.com/nodejs/node/pull/3130)

## 2015-09-22, Version 4.1.1 (Stable), @rvagg

### Notable changes

* **buffer**: Fixed a bug introduced in v4.1.0 where allocating a new zero-length buffer can result in the _next_ allocation of a TypedArray in JavaScript not being zero-filled. In certain circumstances this could result in data leakage via reuse of memory space in TypedArrays, breaking the normally safe assumption that TypedArrays should be always zero-filled. (Trevor Norris) [#2931](https://github.com/nodejs/node/pull/2931).
* **http**: Guard against response-splitting of HTTP trailing headers added via [`response.addTrailers()`](https://nodejs.org/api/http.html#http_response_addtrailers_headers) by removing new-line (`[\r\n]`) characters from values. Note that standard header values are already stripped of new-line characters. The expected security impact is low because trailing headers are rarely used. (Ben Noordhuis) [#2945](https://github.com/nodejs/node/pull/2945).
* **npm**: Upgrade to npm 2.14.4 from 2.14.3, see [release notes](https://github.com/npm/npm/releases/tag/v2.14.4) for full details (Kat Marchán) [#2958](https://github.com/nodejs/node/pull/2958)
  - Upgrades `graceful-fs` on multiple dependencies to no longer rely on monkey-patching `fs`
  - Fix `npm link` for pre-release / RC builds of Node
* **v8**: Update post-mortem metadata to allow post-mortem debugging tools to find and inspect:
  - JavaScript objects that use dictionary properties (Julien Gilli) [#2959](https://github.com/nodejs/node/pull/2959)
  - ScopeInfo and thus closures (Julien Gilli) [#2974](https://github.com/nodejs/node/pull/2974)

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`d63e02e08d`](https://github.com/nodejs/node/commit/d63e02e08d)] - **buffer**: don't set zero fill for zero-length buffer (Trevor Norris) [#2931](https://github.com/nodejs/node/pull/2931)
* [[`5905b14bff`](https://github.com/nodejs/node/commit/5905b14bff)] - **build**: fix icutrim when building small-icu on BE (Stewart Addison) [#2602](https://github.com/nodejs/node/pull/2602)
* [[`f010cb5d96`](https://github.com/nodejs/node/commit/f010cb5d96)] - **configure**: detect mipsel host (Jérémy Lal) [#2971](https://github.com/nodejs/node/pull/2971)
* [[`b93ad5abbd`](https://github.com/nodejs/node/commit/b93ad5abbd)] - **deps**: backport 357e6b9 from V8's upstream (Julien Gilli) [#2974](https://github.com/nodejs/node/pull/2974)
* [[`8da3da4d41`](https://github.com/nodejs/node/commit/8da3da4d41)] - **deps**: backport ff7d70b from V8's upstream (Julien Gilli) [#2959](https://github.com/nodejs/node/pull/2959)
* [[`2600fb8ae6`](https://github.com/nodejs/node/commit/2600fb8ae6)] - **deps**: upgraded to node-gyp@3.0.3 in npm (Kat Marchán) [#2958](https://github.com/nodejs/node/pull/2958)
* [[`793aad2d7a`](https://github.com/nodejs/node/commit/793aad2d7a)] - **deps**: upgrade to npm 2.14.4 (Kat Marchán) [#2958](https://github.com/nodejs/node/pull/2958)
* [[`43e2b7f836`](https://github.com/nodejs/node/commit/43e2b7f836)] - **doc**: remove usage of events.EventEmitter (Sakthipriyan Vairamani) [#2921](https://github.com/nodejs/node/pull/2921)
* [[`9c59d2f16a`](https://github.com/nodejs/node/commit/9c59d2f16a)] - **doc**: remove extra using v8::HandleScope statement (Christopher J. Brody) [#2983](https://github.com/nodejs/node/pull/2983)
* [[`f7edbab367`](https://github.com/nodejs/node/commit/f7edbab367)] - **doc**: clarify description of assert.ifError() (Rich Trott) [#2941](https://github.com/nodejs/node/pull/2941)
* [[`b2ddf0f9a2`](https://github.com/nodejs/node/commit/b2ddf0f9a2)] - **doc**: refine process.kill() and exit explanations (Rich Trott) [#2918](https://github.com/nodejs/node/pull/2918)
* [[`f68fed2e6f`](https://github.com/nodejs/node/commit/f68fed2e6f)] - **http**: remove redundant code in _deferToConnect (Malcolm Ahoy) [#2769](https://github.com/nodejs/node/pull/2769)
* [[`f542e74c93`](https://github.com/nodejs/node/commit/f542e74c93)] - **http**: guard against response splitting in trailers (Ben Noordhuis) [#2945](https://github.com/nodejs/node/pull/2945)
* [[`bc9f629387`](https://github.com/nodejs/node/commit/bc9f629387)] - **http_parser**: do not dealloc during kOnExecute (Fedor Indutny) [#2956](https://github.com/nodejs/node/pull/2956)
* [[`1860e0cebd`](https://github.com/nodejs/node/commit/1860e0cebd)] - **lib,src**: remove usage of events.EventEmitter (Sakthipriyan Vairamani) [#2921](https://github.com/nodejs/node/pull/2921)
* [[`d4cd5ac407`](https://github.com/nodejs/node/commit/d4cd5ac407)] - **readline**: fix tab completion bug (Matt Harrison) [#2816](https://github.com/nodejs/node/pull/2816)
* [[`9760e04839`](https://github.com/nodejs/node/commit/9760e04839)] - **repl**: don't use tty control codes when $TERM is set to "dumb" (Salman Aljammaz) [#2712](https://github.com/nodejs/node/pull/2712)
* [[`cb971cc97d`](https://github.com/nodejs/node/commit/cb971cc97d)] - **repl**: backslash bug fix (Sakthipriyan Vairamani) [#2968](https://github.com/nodejs/node/pull/2968)
* [[`2034f68668`](https://github.com/nodejs/node/commit/2034f68668)] - **src**: honor --abort_on_uncaught_exception flag (Evan Lucas) [#2776](https://github.com/nodejs/node/pull/2776)
* [[`0b1ca4a9ef`](https://github.com/nodejs/node/commit/0b1ca4a9ef)] - **src**: Add ABORT macro (Evan Lucas) [#2776](https://github.com/nodejs/node/pull/2776)
* [[`4519dd00f9`](https://github.com/nodejs/node/commit/4519dd00f9)] - **test**: test sync version of mkdir & rmdir (Sakthipriyan Vairamani) [#2588](https://github.com/nodejs/node/pull/2588)
* [[`816f609c8b`](https://github.com/nodejs/node/commit/816f609c8b)] - **test**: use tmpDir instead of fixtures in readdir (Sakthipriyan Vairamani) [#2587](https://github.com/nodejs/node/pull/2587)
* [[`2084f52585`](https://github.com/nodejs/node/commit/2084f52585)] - **test**: test more http response splitting scenarios (Ben Noordhuis) [#2945](https://github.com/nodejs/node/pull/2945)
* [[`fa08d1d8a1`](https://github.com/nodejs/node/commit/fa08d1d8a1)] - **test**: add test-spawn-cmd-named-pipe (Alexis Campailla) [#2770](https://github.com/nodejs/node/pull/2770)
* [[`71b5d80682`](https://github.com/nodejs/node/commit/71b5d80682)] - **test**: make cluster tests more time tolerant (Michael Dawson) [#2891](https://github.com/nodejs/node/pull/2891)
* [[`3e09dcfc32`](https://github.com/nodejs/node/commit/3e09dcfc32)] - **test**: update cwd-enoent tests for AIX (Imran Iqbal) [#2909](https://github.com/nodejs/node/pull/2909)
* [[`6ea8ec1c59`](https://github.com/nodejs/node/commit/6ea8ec1c59)] - **tools**: single, cross-platform tick processor (Matt Loring) [#2868](https://github.com/nodejs/node/pull/2868)

## 2015-09-17, Version 4.1.0 (Stable), @Fishrock123

### Notable changes

* **buffer**:
  - Buffers are now created in JavaScript, rather than C++. This increases the speed of buffer creation (Trevor Norris) [#2866](https://github.com/nodejs/node/pull/2866).
  - `Buffer#slice()` now uses `Uint8Array#subarray()` internally, increasing `slice()` performance (Karl Skomski) [#2777](https://github.com/nodejs/node/pull/2777).
* **fs**:
  - `fs.utimes()` now properly converts numeric strings, `NaN`, and `Infinity` (Yazhong Liu) [#2387](https://github.com/nodejs/node/pull/2387).
  - `fs.WriteStream` now implements `_writev`, allowing for super-fast bulk writes (Ron Korving) [#2167](https://github.com/nodejs/node/pull/2167).
* **http**: Fixed an issue with certain `write()` sizes causing errors when using `http.request()` (Fedor Indutny) [#2824](https://github.com/nodejs/node/pull/2824).
* **npm**: Upgrade to version 2.14.3, see https://github.com/npm/npm/releases/tag/v2.14.3 for more details (Kat Marchán) [#2822](https://github.com/nodejs/node/pull/2822).
* **src**: V8 cpu profiling no longer erroneously shows idle time (Oleksandr Chekhovskyi) [#2324](https://github.com/nodejs/node/pull/2324).
* **timers**: `#ref()` and `#unref()` now return the timer they belong to (Sam Roberts) [#2905](https://github.com/nodejs/node/pull/2905).
* **v8**: Lateral upgrade to 4.5.103.33 from 4.5.103.30, contains minor fixes (Ali Ijaz Sheikh) [#2870](https://github.com/nodejs/node/pull/2870).
  - This fixes a previously known bug where some computed object shorthand properties did not work correctly ([#2507](https://github.com/nodejs/node/issues/2507)).

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`b1abe812cd`](https://github.com/nodejs/node/commit/b1abe812cd)] - Working on 4.0.1 (Rod Vagg)
* [[`f9f8378853`](https://github.com/nodejs/node/commit/f9f8378853)] - 2015-09-08, Version 4.0.0 (Stable) Release (Rod Vagg)
* [[`9683e5df51`](https://github.com/nodejs/node/commit/9683e5df51)] - **bindings**: close after reading module struct (Fedor Indutny) [#2792](https://github.com/nodejs/node/pull/2792)
* [[`4b4cfa2d44`](https://github.com/nodejs/node/commit/4b4cfa2d44)] - **buffer**: always allocate typed arrays outside heap (Trevor Norris) [#2893](https://github.com/nodejs/node/pull/2893)
* [[`7df018a29b`](https://github.com/nodejs/node/commit/7df018a29b)] - **buffer**: construct Uint8Array in JS (Trevor Norris) [#2866](https://github.com/nodejs/node/pull/2866)
* [[`43397b204e`](https://github.com/nodejs/node/commit/43397b204e)] - **(SEMVER-MINOR)** **build**: Updates to enable AIX support (Michael Dawson) [#2364](https://github.com/nodejs/node/pull/2364)
* [[`e35b1fd610`](https://github.com/nodejs/node/commit/e35b1fd610)] - **build**: clean up the generated tap file (Sakthipriyan Vairamani) [#2837](https://github.com/nodejs/node/pull/2837)
* [[`96670ebe37`](https://github.com/nodejs/node/commit/96670ebe37)] - **deps**: backport 6d32be2 from v8's upstream (Michaël Zasso) [#2916](https://github.com/nodejs/node/pull/2916)
* [[`94972d5b13`](https://github.com/nodejs/node/commit/94972d5b13)] - **deps**: backport 0d01728 from v8's upstream (Fedor Indutny) [#2912](https://github.com/nodejs/node/pull/2912)
* [[`7ebd881c29`](https://github.com/nodejs/node/commit/7ebd881c29)] - **deps**: upgrade V8 to 4.5.103.33 (Ali Ijaz Sheikh) [#2870](https://github.com/nodejs/node/pull/2870)
* [[`ed47ab6e44`](https://github.com/nodejs/node/commit/ed47ab6e44)] - **deps**: upgraded to node-gyp@3.0.3 in npm (Kat Marchán) [#2822](https://github.com/nodejs/node/pull/2822)
* [[`f4641ae875`](https://github.com/nodejs/node/commit/f4641ae875)] - **deps**: upgrade to npm 2.14.3 (Kat Marchán) [#2822](https://github.com/nodejs/node/pull/2822)
* [[`8119693a3d`](https://github.com/nodejs/node/commit/8119693a3d)] - **deps**: update libuv to version 1.7.4 (Saúl Ibarra Corretgé) [#2817](https://github.com/nodejs/node/pull/2817)
* [[`6098504685`](https://github.com/nodejs/node/commit/6098504685)] - **deps**: cherry-pick 6da51b4 from v8's upstream (Fedor Indutny) [#2801](https://github.com/nodejs/node/pull/2801)
* [[`bf42cc8dba`](https://github.com/nodejs/node/commit/bf42cc8dba)] - **doc**: process exit event is not guaranteed to fire (Rich Trott) [#2861](https://github.com/nodejs/node/pull/2861)
* [[`bb0f869f67`](https://github.com/nodejs/node/commit/bb0f869f67)] - **doc**: remove incorrect reference to TCP in net docs (Sam Roberts) [#2903](https://github.com/nodejs/node/pull/2903)
* [[`302d59dce8`](https://github.com/nodejs/node/commit/302d59dce8)] - **doc**: correct buffer.slice arg syntax (Sam Roberts) [#2903](https://github.com/nodejs/node/pull/2903)
* [[`74db9637b7`](https://github.com/nodejs/node/commit/74db9637b7)] - **doc**: describe spawn option.detached (Sam Roberts) [#2903](https://github.com/nodejs/node/pull/2903)
* [[`a7bd897273`](https://github.com/nodejs/node/commit/a7bd897273)] - **doc**: rename from iojs(1) to node(1) in benchmarks (Dmitry Vasilyev) [#2884](https://github.com/nodejs/node/pull/2884)
* [[`cd643d7c37`](https://github.com/nodejs/node/commit/cd643d7c37)] - **doc**: add missing backtick in buffer.markdown (Sven Slootweg) [#2881](https://github.com/nodejs/node/pull/2881)
* [[`e8a206e802`](https://github.com/nodejs/node/commit/e8a206e802)] - **doc**: fix broken link in repl.markdown (Danny Nemer) [#2827](https://github.com/nodejs/node/pull/2827)
* [[`7ee36d61f7`](https://github.com/nodejs/node/commit/7ee36d61f7)] - **doc**: fix typos in README (Ionică Bizău) [#2852](https://github.com/nodejs/node/pull/2852)
* [[`4d1ae26196`](https://github.com/nodejs/node/commit/4d1ae26196)] - **doc**: add tunniclm as a collaborator (Mike Tunnicliffe) [#2826](https://github.com/nodejs/node/pull/2826)
* [[`2d77d03643`](https://github.com/nodejs/node/commit/2d77d03643)] - **doc**: fix two doc errors in stream and process (Jeremiah Senkpiel) [#2549](https://github.com/nodejs/node/pull/2549)
* [[`55ac24f721`](https://github.com/nodejs/node/commit/55ac24f721)] - **doc**: fixed io.js references in process.markdown (Tristian Flanagan) [#2846](https://github.com/nodejs/node/pull/2846)
* [[`cd1297fb57`](https://github.com/nodejs/node/commit/cd1297fb57)] - **doc**: use "Calls" over "Executes" for consistency (Minwoo Jung) [#2800](https://github.com/nodejs/node/pull/2800)
* [[`d664b95581`](https://github.com/nodejs/node/commit/d664b95581)] - **doc**: use US English for consistency (Anne-Gaelle Colom) [#2784](https://github.com/nodejs/node/pull/2784)
* [[`82ba1839fb`](https://github.com/nodejs/node/commit/82ba1839fb)] - **doc**: use 3rd person singular for consistency (Anne-Gaelle Colom) [#2765](https://github.com/nodejs/node/pull/2765)
* [[`432cce6e95`](https://github.com/nodejs/node/commit/432cce6e95)] - **doc**: describe process API for IPC (Sam Roberts) [#1978](https://github.com/nodejs/node/pull/1978)
* [[`1d75012b9d`](https://github.com/nodejs/node/commit/1d75012b9d)] - **doc**: fix comma splice in Assertion Testing doc (Rich Trott) [#2728](https://github.com/nodejs/node/pull/2728)
* [[`6108ea9bb4`](https://github.com/nodejs/node/commit/6108ea9bb4)] - **fs**: consider NaN/Infinity in toUnixTimestamp (Yazhong Liu) [#2387](https://github.com/nodejs/node/pull/2387)
* [[`2b6aa9415f`](https://github.com/nodejs/node/commit/2b6aa9415f)] - **(SEMVER-MINOR)** **fs**: implemented WriteStream#writev (Ron Korving) [#2167](https://github.com/nodejs/node/pull/2167)
* [[`431bf74c55`](https://github.com/nodejs/node/commit/431bf74c55)] - **http**: default Agent.getName to 'localhost' (Malcolm Ahoy) [#2825](https://github.com/nodejs/node/pull/2825)
* [[`ea15d71c16`](https://github.com/nodejs/node/commit/ea15d71c16)] - **http_server**: fix resume after socket close (Fedor Indutny) [#2824](https://github.com/nodejs/node/pull/2824)
* [[`8e5843405b`](https://github.com/nodejs/node/commit/8e5843405b)] - **src**: null env_ field from constructor (Trevor Norris) [#2913](https://github.com/nodejs/node/pull/2913)
* [[`0a5f80a11f`](https://github.com/nodejs/node/commit/0a5f80a11f)] - **src**: use subarray() in Buffer#slice() for speedup (Karl Skomski) [#2777](https://github.com/nodejs/node/pull/2777)
* [[`57707e2490`](https://github.com/nodejs/node/commit/57707e2490)] - **src**: use ZCtxt as a source for v8::Isolates (Roman Klauke) [#2547](https://github.com/nodejs/node/pull/2547)
* [[`b0df2273ab`](https://github.com/nodejs/node/commit/b0df2273ab)] - **src**: fix v8::CpuProfiler idle sampling (Oleksandr Chekhovskyi) [#2324](https://github.com/nodejs/node/pull/2324)
* [[`eaa8e60b91`](https://github.com/nodejs/node/commit/eaa8e60b91)] - **streams**: refactor LazyTransform to internal/ (Brendan Ashworth) [#2566](https://github.com/nodejs/node/pull/2566)
* [[`648c003e14`](https://github.com/nodejs/node/commit/648c003e14)] - **test**: use tmp directory in chdir test (Sakthipriyan Vairamani) [#2589](https://github.com/nodejs/node/pull/2589)
* [[`079a2173d4`](https://github.com/nodejs/node/commit/079a2173d4)] - **test**: fix Buffer OOM error message (Trevor Norris) [#2915](https://github.com/nodejs/node/pull/2915)
* [[`52019a1b21`](https://github.com/nodejs/node/commit/52019a1b21)] - **test**: fix default value for additional param (Sakthipriyan Vairamani) [#2553](https://github.com/nodejs/node/pull/2553)
* [[`5df5d0423a`](https://github.com/nodejs/node/commit/5df5d0423a)] - **test**: remove disabled test (Rich Trott) [#2841](https://github.com/nodejs/node/pull/2841)
* [[`9e5f0995bd`](https://github.com/nodejs/node/commit/9e5f0995bd)] - **test**: split up internet dns tests (Rich Trott) [#2802](https://github.com/nodejs/node/pull/2802)
* [[`41f2dde51a`](https://github.com/nodejs/node/commit/41f2dde51a)] - **test**: increase dgram timeout for armv6 (Rich Trott) [#2808](https://github.com/nodejs/node/pull/2808)
* [[`6e2fe1c21a`](https://github.com/nodejs/node/commit/6e2fe1c21a)] - **test**: remove valid hostname check in test-dns.js (Rich Trott) [#2785](https://github.com/nodejs/node/pull/2785)
* [[`779e14f1a7`](https://github.com/nodejs/node/commit/779e14f1a7)] - **test**: expect error for test_lookup_ipv6_hint on FreeBSD (Rich Trott) [#2724](https://github.com/nodejs/node/pull/2724)
* [[`f931b9dd95`](https://github.com/nodejs/node/commit/f931b9dd95)] - **(SEMVER-MINOR)** **timer**: ref/unref return self (Sam Roberts) [#2905](https://github.com/nodejs/node/pull/2905)
* [[`59d03738cc`](https://github.com/nodejs/node/commit/59d03738cc)] - **tools**: enable arrow functions in .eslintrc (Sakthipriyan Vairamani) [#2840](https://github.com/nodejs/node/pull/2840)
* [[`69e7b875a2`](https://github.com/nodejs/node/commit/69e7b875a2)] - **tools**: open `test.tap` file in write-binary mode (Sakthipriyan Vairamani) [#2837](https://github.com/nodejs/node/pull/2837)
* [[`ff6d30d784`](https://github.com/nodejs/node/commit/ff6d30d784)] - **tools**: add missing tick processor polyfill (Matt Loring) [#2694](https://github.com/nodejs/node/pull/2694)
* [[`519caba021`](https://github.com/nodejs/node/commit/519caba021)] - **tools**: fix flakiness in test-tick-processor (Matt Loring) [#2694](https://github.com/nodejs/node/pull/2694)
* [[`ac004b8555`](https://github.com/nodejs/node/commit/ac004b8555)] - **tools**: remove hyphen in TAP result (Sakthipriyan Vairamani) [#2718](https://github.com/nodejs/node/pull/2718)
* [[`ba47511976`](https://github.com/nodejs/node/commit/ba47511976)] - **tsc**: adjust TSC membership for IBM+StrongLoop (James M Snell) [#2858](https://github.com/nodejs/node/pull/2858)
* [[`e035266805`](https://github.com/nodejs/node/commit/e035266805)] - **win,msi**: fix documentation shortcut url (Brian White) [#2781](https://github.com/nodejs/node/pull/2781)

## 2015-09-08, Version 4.0.0 (Stable), @rvagg

### Notable changes

This list of changes is relative to the last io.js v3.x branch release, v3.3.0. Please see the list of notable changes in the v3.x, v2.x and v1.x releases for a more complete list of changes from 0.12.x. Note, that some changes in the v3.x series as well as major breaking changes in this release constitute changes required for full convergence of the Node.js and io.js projects.

* **child_process**: `ChildProcess.prototype.send()` and `process.send()` operate asynchronously across all platforms so an optional callback parameter has been introduced that will be invoked once the message has been sent, i.e. `.send(message[, sendHandle][, callback])` (Ben Noordhuis) [#2620](https://github.com/nodejs/node/pull/2620).
* **node**: Rename "io.js" code to "Node.js" (cjihrig) [#2367](https://github.com/nodejs/node/pull/2367).
* **node-gyp**: This release bundles an updated version of node-gyp that works with all versions of Node.js and io.js including nightly and release candidate builds. From io.js v3 and Node.js v4 onward, it will only download a headers tarball when building addons rather than the entire source. (Rod Vagg) [#2700](https://github.com/nodejs/node/pull/2700)
* **npm**: Upgrade to version 2.14.2 from 2.13.3, includes a security update, see https://github.com/npm/npm/releases/tag/v2.14.2 for more details, (Kat Marchán) [#2696](https://github.com/nodejs/node/pull/2696).
* **timers**: Improved timer performance from porting the 0.12 implementation, plus minor fixes (Jeremiah Senkpiel) [#2540](https://github.com/nodejs/node/pull/2540), (Julien Gilli) [nodejs/node-v0.x-archive#8751](https://github.com/nodejs/node-v0.x-archive/pull/8751) [nodejs/node-v0.x-archive#8905](https://github.com/nodejs/node-v0.x-archive/pull/8905)
* **util**: The `util.is*()` functions have been deprecated, beginning with deprecation warnings in the documentation for this release, users are encouraged to seek more robust alternatives in the npm registry, (Sakthipriyan Vairamani) [#2447](https://github.com/nodejs/node/pull/2447).
* **v8**: Upgrade to version 4.5.103.30 from 4.4.63.30 (Ali Ijaz Sheikh) [#2632](https://github.com/nodejs/node/pull/2632).
 - Implement new `TypedArray` prototype methods: `copyWithin()`, `every()`, `fill()`, `filter()`, `find()`, `findIndex()`, `forEach()`, `indexOf()`, `join()`, `lastIndexOf()`, `map()`, `reduce()`, `reduceRight()`, `reverse()`, `slice()`, `some()`, `sort()`. See https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/TypedArray for further information.
 - Implement new `TypedArray.from()` and `TypedArray.of()` functions. See https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/TypedArray for further information.
 - Implement arrow functions, see https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Functions/Arrow_functions for further information.
 - Full ChangeLog available at https://github.com/v8/v8-git-mirror/blob/4.5.103/ChangeLog

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some uses of computed object shorthand properties are not handled correctly by the current version of V8. e.g. `[{ [prop]: val }]` evaluates to `[{}]`. [#2507](https://github.com/nodejs/node/issues/2507)
* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`4f50d3fb90`](https://github.com/nodejs/node/commit/4f50d3fb90)] - **(SEMVER-MAJOR)** This commit sets the value of process.release.name to "node". (cjihrig) [#2367](https://github.com/nodejs/node/pull/2367)
* [[`d3178d8b1b`](https://github.com/nodejs/node/commit/d3178d8b1b)] - **buffer**: SlowBuffer only accept valid numeric values (Michaël Zasso) [#2635](https://github.com/nodejs/node/pull/2635)
* [[`0cb0f4a6e4`](https://github.com/nodejs/node/commit/0cb0f4a6e4)] - **build**: fix v8_enable_handle_zapping override (Karl Skomski) [#2731](https://github.com/nodejs/node/pull/2731)
* [[`a7596d7efc`](https://github.com/nodejs/node/commit/a7596d7efc)] - **build**: remote commands on staging in single session (Rod Vagg) [#2717](https://github.com/nodejs/node/pull/2717)
* [[`be427e9efa`](https://github.com/nodejs/node/commit/be427e9efa)] - **build**: make .msi install to "nodejs", not "node" (Rod Vagg) [#2701](https://github.com/nodejs/node/pull/2701)
* [[`5652ce0dbc`](https://github.com/nodejs/node/commit/5652ce0dbc)] - **build**: fix .pkg creation tooling (Rod Vagg) [#2687](https://github.com/nodejs/node/pull/2687)
* [[`101db80111`](https://github.com/nodejs/node/commit/101db80111)] - **build**: add --enable-asan with builtin leakcheck (Karl Skomski) [#2376](https://github.com/nodejs/node/pull/2376)
* [[`2c3939c9c0`](https://github.com/nodejs/node/commit/2c3939c9c0)] - **child_process**: use stdio.fd even if it is 0 (Evan Lucas) [#2727](https://github.com/nodejs/node/pull/2727)
* [[`609db5a1dd`](https://github.com/nodejs/node/commit/609db5a1dd)] - **child_process**: check execFile and fork args (James M Snell) [#2667](https://github.com/nodejs/node/pull/2667)
* [[`d010568c23`](https://github.com/nodejs/node/commit/d010568c23)] - **(SEMVER-MAJOR)** **child_process**: add callback parameter to .send() (Ben Noordhuis) [#2620](https://github.com/nodejs/node/pull/2620)
* [[`c60857a81a`](https://github.com/nodejs/node/commit/c60857a81a)] - **cluster**: allow shared reused dgram sockets (Fedor Indutny) [#2548](https://github.com/nodejs/node/pull/2548)
* [[`b2ecbb6191`](https://github.com/nodejs/node/commit/b2ecbb6191)] - **contextify**: ignore getters during initialization (Fedor Indutny) [#2091](https://github.com/nodejs/node/pull/2091)
* [[`3711934095`](https://github.com/nodejs/node/commit/3711934095)] - **cpplint**: make it possible to run outside git repo (Ben Noordhuis) [#2710](https://github.com/nodejs/node/pull/2710)
* [[`03f900ab25`](https://github.com/nodejs/node/commit/03f900ab25)] - **crypto**: replace rwlocks with simple mutexes (Ben Noordhuis) [#2723](https://github.com/nodejs/node/pull/2723)
* [[`847459c29b`](https://github.com/nodejs/node/commit/847459c29b)] - **(SEMVER-MAJOR)** **crypto**: show exponent in decimal and hex (Chad Johnston) [#2320](https://github.com/nodejs/node/pull/2320)
* [[`e1c976184d`](https://github.com/nodejs/node/commit/e1c976184d)] - **deps**: improve ArrayBuffer performance in v8 (Fedor Indutny) [#2732](https://github.com/nodejs/node/pull/2732)
* [[`cc0ab17a23`](https://github.com/nodejs/node/commit/cc0ab17a23)] - **deps**: float node-gyp v3.0.0 (Rod Vagg) [#2700](https://github.com/nodejs/node/pull/2700)
* [[`b2c3c6d727`](https://github.com/nodejs/node/commit/b2c3c6d727)] - **deps**: create .npmrc during npm tests (Kat Marchán) [#2696](https://github.com/nodejs/node/pull/2696)
* [[`babdbfdbd5`](https://github.com/nodejs/node/commit/babdbfdbd5)] - **deps**: upgrade to npm 2.14.2 (Kat Marchán) [#2696](https://github.com/nodejs/node/pull/2696)
* [[`155783d876`](https://github.com/nodejs/node/commit/155783d876)] - **deps**: backport 75e43a6 from v8 upstream (again) (saper) [#2692](https://github.com/nodejs/node/pull/2692)
* [[`5424d6fcf0`](https://github.com/nodejs/node/commit/5424d6fcf0)] - **deps**: upgrade V8 to 4.5.103.30 (Ali Ijaz Sheikh) [#2632](https://github.com/nodejs/node/pull/2632)
* [[`c43172578e`](https://github.com/nodejs/node/commit/c43172578e)] - **(SEMVER-MAJOR)** **deps**: upgrade V8 to 4.5.103.24 (Ali Ijaz Sheikh) [#2509](https://github.com/nodejs/node/pull/2509)
* [[`714e96e8b9`](https://github.com/nodejs/node/commit/714e96e8b9)] - **deps**: backport 75e43a6 from v8 upstream (saper) [#2636](https://github.com/nodejs/node/pull/2636)
* [[`8637755cbf`](https://github.com/nodejs/node/commit/8637755cbf)] - **doc**: add TSC meeting minutes 2015-09-02 (Rod Vagg) [#2674](https://github.com/nodejs/node/pull/2674)
* [[`d3d5b93214`](https://github.com/nodejs/node/commit/d3d5b93214)] - **doc**: update environment vars in manpage and --help (Roman Reiss) [#2690](https://github.com/nodejs/node/pull/2690)
* [[`29f586ac0a`](https://github.com/nodejs/node/commit/29f586ac0a)] - **doc**: update url doc to account for escaping (Jeremiah Senkpiel) [#2605](https://github.com/nodejs/node/pull/2605)
* [[`ba50cfebef`](https://github.com/nodejs/node/commit/ba50cfebef)] - **doc**: reorder collaborators by their usernames (Johan Bergström) [#2322](https://github.com/nodejs/node/pull/2322)
* [[`8a9a3bf798`](https://github.com/nodejs/node/commit/8a9a3bf798)] - **doc**: update changelog for io.js v3.3.0 (Rod Vagg) [#2653](https://github.com/nodejs/node/pull/2653)
* [[`6cd0e2664b`](https://github.com/nodejs/node/commit/6cd0e2664b)] - **doc**: update io.js reference (Ben Noordhuis) [#2580](https://github.com/nodejs/node/pull/2580)
* [[`f9539c19e8`](https://github.com/nodejs/node/commit/f9539c19e8)] - **doc**: update changelog for io.js v3.2.0 (Rod Vagg) [#2512](https://github.com/nodejs/node/pull/2512)
* [[`cded6e7993`](https://github.com/nodejs/node/commit/cded6e7993)] - **doc**: fix CHANGELOG.md on master (Roman Reiss) [#2513](https://github.com/nodejs/node/pull/2513)
* [[`93e2830686`](https://github.com/nodejs/node/commit/93e2830686)] - **(SEMVER-MINOR)** **doc**: document deprecation of util.is* functions (Sakthipriyan Vairamani) [#2447](https://github.com/nodejs/node/pull/2447)
* [[`7038388558`](https://github.com/nodejs/node/commit/7038388558)] - **doc,test**: enable recursive file watching in Windows (Sakthipriyan Vairamani) [#2649](https://github.com/nodejs/node/pull/2649)
* [[`f3696f64a1`](https://github.com/nodejs/node/commit/f3696f64a1)] - **events,lib**: don't require EE#listenerCount() (Jeremiah Senkpiel) [#2661](https://github.com/nodejs/node/pull/2661)
* [[`45a2046f5d`](https://github.com/nodejs/node/commit/45a2046f5d)] - **(SEMVER-MAJOR)** **installer**: fix installers for node.js rename (Frederic Hemberger) [#2367](https://github.com/nodejs/node/pull/2367)
* [[`7a999a1376`](https://github.com/nodejs/node/commit/7a999a1376)] - **(SEMVER-MAJOR)** **lib**: add net.Socket#localFamily property (Ben Noordhuis) [#956](https://github.com/nodejs/node/pull/956)
* [[`de88255b0f`](https://github.com/nodejs/node/commit/de88255b0f)] - ***Revert*** "**lib,src**: add unix socket getsockname/getpeername" (Ben Noordhuis) [#2584](https://github.com/nodejs/node/pull/2584)
* [[`f337595441`](https://github.com/nodejs/node/commit/f337595441)] - **(SEMVER-MAJOR)** **lib,src**: add unix socket getsockname/getpeername (Ben Noordhuis) [#956](https://github.com/nodejs/node/pull/956)
* [[`3b602527d1`](https://github.com/nodejs/node/commit/3b602527d1)] - **(SEMVER-MAJOR)** **node**: additional cleanup for node rename (cjihrig) [#2367](https://github.com/nodejs/node/pull/2367)
* [[`a69ab27ab4`](https://github.com/nodejs/node/commit/a69ab27ab4)] - **(SEMVER-MAJOR)** **node**: rename from io.js to node (cjihrig) [#2367](https://github.com/nodejs/node/pull/2367)
* [[`9358eee9dd`](https://github.com/nodejs/node/commit/9358eee9dd)] - **node-gyp**: float 3.0.1, minor fix for download url (Rod Vagg) [#2737](https://github.com/nodejs/node/pull/2737)
* [[`d2d981252b`](https://github.com/nodejs/node/commit/d2d981252b)] - **src**: s/ia32/x86 for process.release.libUrl for win (Rod Vagg) [#2699](https://github.com/nodejs/node/pull/2699)
* [[`eba3d3dccd`](https://github.com/nodejs/node/commit/eba3d3dccd)] - **src**: use standard conform snprintf on windows (Karl Skomski) [#2404](https://github.com/nodejs/node/pull/2404)
* [[`cddbec231f`](https://github.com/nodejs/node/commit/cddbec231f)] - **src**: fix buffer overflow for long exception lines (Karl Skomski) [#2404](https://github.com/nodejs/node/pull/2404)
* [[`dd3f3417c7`](https://github.com/nodejs/node/commit/dd3f3417c7)] - **src**: re-enable fast math on arm (Michaël Zasso) [#2592](https://github.com/nodejs/node/pull/2592)
* [[`e137c1177c`](https://github.com/nodejs/node/commit/e137c1177c)] - **(SEMVER-MAJOR)** **src**: enable vector ics on arm again (Ali Ijaz Sheikh) [#2509](https://github.com/nodejs/node/pull/2509)
* [[`7ce749d722`](https://github.com/nodejs/node/commit/7ce749d722)] - **src**: replace usage of v8::Handle with v8::Local (Michaël Zasso) [#2202](https://github.com/nodejs/node/pull/2202)
* [[`b1a2d9509f`](https://github.com/nodejs/node/commit/b1a2d9509f)] - **src**: enable v8 deprecation warnings and fix them (Ben Noordhuis) [#2091](https://github.com/nodejs/node/pull/2091)
* [[`808de0da03`](https://github.com/nodejs/node/commit/808de0da03)] - **(SEMVER-MAJOR)** **src**: apply debug force load fixups from 41e63fb (Ali Ijaz Sheikh) [#2509](https://github.com/nodejs/node/pull/2509)
* [[`5201cb0ff1`](https://github.com/nodejs/node/commit/5201cb0ff1)] - **src**: fix memory leak in ExternString (Karl Skomski) [#2402](https://github.com/nodejs/node/pull/2402)
* [[`2308a27c0a`](https://github.com/nodejs/node/commit/2308a27c0a)] - **src**: only set v8 flags if argc > 1 (Evan Lucas) [#2646](https://github.com/nodejs/node/pull/2646)
* [[`384effed20`](https://github.com/nodejs/node/commit/384effed20)] - **test**: fix use of `common` before required (Rod Vagg) [#2685](https://github.com/nodejs/node/pull/2685)
* [[`f146f686b7`](https://github.com/nodejs/node/commit/f146f686b7)] - **(SEMVER-MAJOR)** **test**: fix test-repl-tab-complete.js for V8 4.5 (Ali Ijaz Sheikh) [#2509](https://github.com/nodejs/node/pull/2509)
* [[`fe4b309fd3`](https://github.com/nodejs/node/commit/fe4b309fd3)] - **test**: refactor to eliminate flaky test (Rich Trott) [#2609](https://github.com/nodejs/node/pull/2609)
* [[`619721e6b8`](https://github.com/nodejs/node/commit/619721e6b8)] - **test**: mark eval_messages as flaky (Alexis Campailla) [#2648](https://github.com/nodejs/node/pull/2648)
* [[`93ba585b66`](https://github.com/nodejs/node/commit/93ba585b66)] - **test**: mark test-vm-syntax-error-stderr as flaky (João Reis) [#2662](https://github.com/nodejs/node/pull/2662)
* [[`367140bca0`](https://github.com/nodejs/node/commit/367140bca0)] - **test**: mark test-repl-persistent-history as flaky (João Reis) [#2659](https://github.com/nodejs/node/pull/2659)
* [[`f6b093343d`](https://github.com/nodejs/node/commit/f6b093343d)] - **timers**: minor `_unrefActive` fixes and improvements (Jeremiah Senkpiel) [#2540](https://github.com/nodejs/node/pull/2540)
* [[`403d7ee7d1`](https://github.com/nodejs/node/commit/403d7ee7d1)] - **timers**: don't mutate unref list while iterating it (Julien Gilli) [#2540](https://github.com/nodejs/node/pull/2540)
* [[`7a8c3e08c3`](https://github.com/nodejs/node/commit/7a8c3e08c3)] - **timers**: Avoid linear scan in `_unrefActive`. (Julien Gilli) [#2540](https://github.com/nodejs/node/pull/2540)
* [[`b630ebaf43`](https://github.com/nodejs/node/commit/b630ebaf43)] - **win,msi**: Upgrade from old upgrade code (João Reis) [#2439](https://github.com/nodejs/node/pull/2439)

## 2015-09-02, Version 3.3.0, @rvagg

### Notable changes

* **build**: Add a `--link-module` option to `configure` that can be used to bundle additional JavaScript modules into a built binary (Bradley Meck) [#2497](https://github.com/nodejs/node/pull/2497)
* **docs**: Merge outstanding doc updates from joyent/node (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* **http_parser**: Significant performance improvement by having `http.Server` consume all initial data from its `net.Socket` and parsing directly without having to enter JavaScript. Any `'data'` listeners on the `net.Socket` will result in the data being "unconsumed" into JavaScript, thereby undoing any performance gains. (Fedor Indutny) [#2355](https://github.com/nodejs/node/pull/2355)
* **libuv**: Upgrade to 1.7.3 (from 1.6.1), see [ChangeLog](https://github.com/libuv/libuv/blob/v1.x/ChangeLog) for details (Saúl Ibarra Corretgé) [#2310](https://github.com/nodejs/node/pull/2310)
* **V8**: Upgrade to 4.4.63.30 (from 4.4.63.26) (Michaël Zasso) [#2482](https://github.com/nodejs/node/pull/2482)

### Known issues

See https://github.com/nodejs/io.js/labels/confirmed-bug for complete and current list of known issues.

* Some uses of computed object shorthand properties are not handled correctly by the current version of V8. e.g. `[{ [prop]: val }]` evaluates to `[{}]`. [#2507](https://github.com/nodejs/node/issues/2507)
* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/io.js/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/io.js/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/io.js/issues/760).
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/io.js/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/io.js/issues/1435).

### Commits

* [[`1a531b4e44`](https://github.com/nodejs/node/commit/1a531b4e44)] - **(SEMVER-MINOR)** Introduce --link-module to ./configure (Bradley Meck) [#2497](https://github.com/nodejs/node/pull/2497)
* [[`d2f314c190`](https://github.com/nodejs/node/commit/d2f314c190)] - **build**: fix borked chmod call for release uploads (Rod Vagg) [#2645](https://github.com/nodejs/node/pull/2645)
* [[`3172e9c541`](https://github.com/nodejs/node/commit/3172e9c541)] - **build**: set file permissions before uploading (Rod Vagg) [#2623](https://github.com/nodejs/node/pull/2623)
* [[`a860d7fae1`](https://github.com/nodejs/node/commit/a860d7fae1)] - **build**: change staging directory on new server (Rod Vagg) [#2623](https://github.com/nodejs/node/pull/2623)
* [[`50c0baa8d7`](https://github.com/nodejs/node/commit/50c0baa8d7)] - **build**: rename 'doc' directory to 'docs' for upload (Rod Vagg) [#2623](https://github.com/nodejs/node/pull/2623)
* [[`0a0577cf5f`](https://github.com/nodejs/node/commit/0a0577cf5f)] - **build**: fix bad cherry-pick for vcbuild.bat build-release (Rod Vagg) [#2625](https://github.com/nodejs/node/pull/2625)
* [[`34de90194b`](https://github.com/nodejs/node/commit/34de90194b)] - **build**: only define NODE_V8_OPTIONS if not empty (Evan Lucas) [#2532](https://github.com/nodejs/node/pull/2532)
* [[`944174b189`](https://github.com/nodejs/node/commit/944174b189)] - **build**: make ci test addons in test/addons (Ben Noordhuis) [#2428](https://github.com/nodejs/node/pull/2428)
* [[`e955f9a1b0`](https://github.com/nodejs/node/commit/e955f9a1b0)] - **crypto**: Use OPENSSL_cleanse to shred the data. (Сковорода Никита Андреевич) [#2575](https://github.com/nodejs/node/pull/2575)
* [[`395d736b9d`](https://github.com/nodejs/node/commit/395d736b9d)] - **debugger**: use strict equality comparison (Minwoo Jung) [#2558](https://github.com/nodejs/node/pull/2558)
* [[`1d0e5210a8`](https://github.com/nodejs/node/commit/1d0e5210a8)] - **deps**: upgrade libuv to 1.7.3 (Saúl Ibarra Corretgé) [#2310](https://github.com/nodejs/node/pull/2310)
* [[`34ef53364f`](https://github.com/nodejs/node/commit/34ef53364f)] - **deps**: update V8 to 4.4.63.30 (Michaël Zasso) [#2482](https://github.com/nodejs/node/pull/2482)
* [[`23579a5f4a`](https://github.com/nodejs/node/commit/23579a5f4a)] - **doc**: add TSC meeting minutes 2015-08-12 (Rod Vagg) [#2438](https://github.com/nodejs/node/pull/2438)
* [[`0cc59299a4`](https://github.com/nodejs/node/commit/0cc59299a4)] - **doc**: add TSC meeting minutes 2015-08-26 (Rod Vagg) [#2591](https://github.com/nodejs/node/pull/2591)
* [[`6efa96e33a`](https://github.com/nodejs/node/commit/6efa96e33a)] - **doc**: merge CHANGELOG.md with joyent/node ChangeLog (P.S.V.R) [#2536](https://github.com/nodejs/node/pull/2536)
* [[`f75d54607b`](https://github.com/nodejs/node/commit/f75d54607b)] - **doc**: clarify cluster behaviour with no workers (Jeremiah Senkpiel) [#2606](https://github.com/nodejs/node/pull/2606)
* [[`8936302121`](https://github.com/nodejs/node/commit/8936302121)] - **doc**: minor clarification in buffer.markdown (Сковорода Никита Андреевич) [#2574](https://github.com/nodejs/node/pull/2574)
* [[`0db0e53753`](https://github.com/nodejs/node/commit/0db0e53753)] - **doc**: add @jasnell and @sam-github to release team (Rod Vagg) [#2455](https://github.com/nodejs/node/pull/2455)
* [[`c16e100593`](https://github.com/nodejs/node/commit/c16e100593)] - **doc**: reorg release team to separate section (Rod Vagg) [#2455](https://github.com/nodejs/node/pull/2455)
* [[`e3e00143fd`](https://github.com/nodejs/node/commit/e3e00143fd)] - **doc**: fix bad merge on modules.markdown (James M Snell)
* [[`2f62455880`](https://github.com/nodejs/node/commit/2f62455880)] - **doc**: minor additional corrections and improvements (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`3bd08aac4b`](https://github.com/nodejs/node/commit/3bd08aac4b)] - **doc**: minor grammatical update in crypto.markdown (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`f707189370`](https://github.com/nodejs/node/commit/f707189370)] - **doc**: minor grammatical update (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`6c98cf0266`](https://github.com/nodejs/node/commit/6c98cf0266)] - **doc**: remove repeated statement in globals.markdown (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`48e6ccf8c2`](https://github.com/nodejs/node/commit/48e6ccf8c2)] - **doc**: remove 'dudes' from documentation (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`b5d68f8076`](https://github.com/nodejs/node/commit/b5d68f8076)] - **doc**: update tense in child_process.markdown (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`242e3fe3ba`](https://github.com/nodejs/node/commit/242e3fe3ba)] - **doc**: fixed worker.id type (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`ea9ee15c21`](https://github.com/nodejs/node/commit/ea9ee15c21)] - **doc**: port is optional for socket.bind() (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`0ff6657a50`](https://github.com/nodejs/node/commit/0ff6657a50)] - **doc**: fix minor types and grammar in fs docs (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`94d83c04f2`](https://github.com/nodejs/node/commit/94d83c04f2)] - **doc**: update parameter name in net.markdown (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`04111ce40f`](https://github.com/nodejs/node/commit/04111ce40f)] - **doc**: small typo in domain.markdown (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`c9fdd1bbbf`](https://github.com/nodejs/node/commit/c9fdd1bbbf)] - **doc**: fixed typo in net.markdown (missing comma) (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`27c07b3f8e`](https://github.com/nodejs/node/commit/27c07b3f8e)] - **doc**: update description of fs.exists in fs.markdown (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`52018e73d9`](https://github.com/nodejs/node/commit/52018e73d9)] - **doc**: clarification on the 'close' event (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`f6d3b87a25`](https://github.com/nodejs/node/commit/f6d3b87a25)] - **doc**: improve working in stream.markdown (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`b5da89431a`](https://github.com/nodejs/node/commit/b5da89431a)] - **doc**: update path.extname documentation (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`1d4ea609db`](https://github.com/nodejs/node/commit/1d4ea609db)] - **doc**: small clarifications to modules.markdown (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`c888985591`](https://github.com/nodejs/node/commit/c888985591)] - **doc**: code style cleanups in repl.markdown (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`105b493595`](https://github.com/nodejs/node/commit/105b493595)] - **doc**: correct grammar in cluster.markdown (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`51b86ccac7`](https://github.com/nodejs/node/commit/51b86ccac7)] - **doc**: Clarify the module.parent is set once (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`d2ffecba2d`](https://github.com/nodejs/node/commit/d2ffecba2d)] - **doc**: add internal modules notice (Jeremiah Senkpiel) [#2523](https://github.com/nodejs/node/pull/2523)
* [[`b36debd5cb`](https://github.com/nodejs/node/commit/b36debd5cb)] - **env**: introduce `KickNextTick` (Fedor Indutny) [#2355](https://github.com/nodejs/node/pull/2355)
* [[`1bc446863f`](https://github.com/nodejs/node/commit/1bc446863f)] - **http_parser**: consume StreamBase instance (Fedor Indutny) [#2355](https://github.com/nodejs/node/pull/2355)
* [[`ce04b735cc`](https://github.com/nodejs/node/commit/ce04b735cc)] - **src**: only memcmp if length > 0 in Buffer::Compare (Karl Skomski) [#2544](https://github.com/nodejs/node/pull/2544)
* [[`31823e37c7`](https://github.com/nodejs/node/commit/31823e37c7)] - **src**: DRY getsockname/getpeername code (Ben Noordhuis) [#956](https://github.com/nodejs/node/pull/956)
* [[`13fd96dda3`](https://github.com/nodejs/node/commit/13fd96dda3)] - **src**: missing Exception::Error in node_http_parser (Jeremiah Senkpiel) [#2550](https://github.com/nodejs/node/pull/2550)
* [[`42e075ae02`](https://github.com/nodejs/node/commit/42e075ae02)] - **test**: improve performance of stringbytes test (Trevor Norris) [#2544](https://github.com/nodejs/node/pull/2544)
* [[`fc726399fd`](https://github.com/nodejs/node/commit/fc726399fd)] - **test**: unmark test-process-argv-0.js as flaky (Rich Trott) [#2613](https://github.com/nodejs/node/pull/2613)
* [[`7727ba1394`](https://github.com/nodejs/node/commit/7727ba1394)] - **test**: lint and refactor to avoid autocrlf issue (Roman Reiss) [#2494](https://github.com/nodejs/node/pull/2494)
* [[`c56aa829f0`](https://github.com/nodejs/node/commit/c56aa829f0)] - **test**: use tmpDir instead of fixturesDir (Sakthipriyan Vairamani) [#2583](https://github.com/nodejs/node/pull/2583)
* [[`5e65181ea4`](https://github.com/nodejs/node/commit/5e65181ea4)] - **test**: handling failure cases properly (Sakthipriyan Vairamani) [#2206](https://github.com/nodejs/node/pull/2206)
* [[`c48b95e847`](https://github.com/nodejs/node/commit/c48b95e847)] - **test**: initial list of flaky tests (Alexis Campailla) [#2424](https://github.com/nodejs/node/pull/2424)
* [[`94e88498ba`](https://github.com/nodejs/node/commit/94e88498ba)] - **test**: pass args to test-ci via env variable (Alexis Campailla) [#2424](https://github.com/nodejs/node/pull/2424)
* [[`09987c7a1c`](https://github.com/nodejs/node/commit/09987c7a1c)] - **test**: support flaky tests in test-ci (Alexis Campailla) [#2424](https://github.com/nodejs/node/pull/2424)
* [[`08b83c8b45`](https://github.com/nodejs/node/commit/08b83c8b45)] - **test**: add test configuration templates (Alexis Campailla) [#2424](https://github.com/nodejs/node/pull/2424)
* [[`8f8ab6fa57`](https://github.com/nodejs/node/commit/8f8ab6fa57)] - **test**: runner should return 0 on flaky tests (Alexis Campailla) [#2424](https://github.com/nodejs/node/pull/2424)
* [[`0cfd3be9c6`](https://github.com/nodejs/node/commit/0cfd3be9c6)] - **test**: runner support for flaky tests (Alexis Campailla) [#2424](https://github.com/nodejs/node/pull/2424)
* [[`3492d2d4c6`](https://github.com/nodejs/node/commit/3492d2d4c6)] - **test**: make test-process-argv-0 robust (Rich Trott) [#2541](https://github.com/nodejs/node/pull/2541)
* [[`a96cc31710`](https://github.com/nodejs/node/commit/a96cc31710)] - **test**: speed up test-child-process-spawnsync.js (Rich Trott) [#2542](https://github.com/nodejs/node/pull/2542)
* [[`856baf4c67`](https://github.com/nodejs/node/commit/856baf4c67)] - **test**: make spawnSync() test robust (Rich Trott) [#2535](https://github.com/nodejs/node/pull/2535)
* [[`3aa6bbb648`](https://github.com/nodejs/node/commit/3aa6bbb648)] - **tools**: update release.sh to work with new website (Rod Vagg) [#2623](https://github.com/nodejs/node/pull/2623)
* [[`f2f0fe45ff`](https://github.com/nodejs/node/commit/f2f0fe45ff)] - **tools**: make add-on scraper print filenames (Ben Noordhuis) [#2428](https://github.com/nodejs/node/pull/2428)
* [[`bb24c4a418`](https://github.com/nodejs/node/commit/bb24c4a418)] - **win,msi**: correct installation path registry keys (João Reis) [#2565](https://github.com/nodejs/node/pull/2565)
* [[`752977b888`](https://github.com/nodejs/node/commit/752977b888)] - **win,msi**: change InstallScope to perMachine (João Reis) [#2565](https://github.com/nodejs/node/pull/2565)

## 2015-08-25, Version 3.2.0, @rvagg

### Notable changes

* **events**: Added `EventEmitter#listenerCount(event)` as a replacement for `EventEmitter.listenerCount(emitter, event)`, which has now been marked as deprecated in the docs. (Sakthipriyan Vairamani) [#2349](https://github.com/nodejs/node/pull/2349)
* **module**: Fixed an error with preloaded modules when the current working directory doesn't exist. (Bradley Meck) [#2353](https://github.com/nodejs/node/pull/2353)
* **node**: Startup time is now about 5% faster when not passing V8 flags. (Evan Lucas) [#2483](https://github.com/nodejs/node/pull/2483)
* **repl**: Tab-completion now works better with arrays. (James M Snell) [#2409](https://github.com/nodejs/node/pull/2409)
* **string_bytes**: Fixed an unaligned write in the handling of UCS2 encoding. (Fedor Indutny) [#2480](https://github.com/nodejs/node/pull/2480)
* **tls**: Added a new `--tls-cipher-list` flag that can be used to override the built-in default cipher list. (James M Snell) [#2412](https://github.com/nodejs/node/pull/2412) _Note: it is suggested you use the built-in cipher list as it has been carefully selected to reflect current security best practices and risk mitigation._

### Known issues

See https://github.com/nodejs/io.js/labels/confirmed-bug for complete and current list of known issues.

* Some uses of computed object shorthand properties are not handled correctly by the current version of V8. e.g. `[{ [prop]: val }]` evaluates to `[{}]`. [#2507](https://github.com/nodejs/node/issues/2507)
* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/io.js/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/io.js/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/io.js/issues/760).
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/io.js/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/io.js/issues/1435).

### Commits

* [[`1cd794f129`](https://github.com/nodejs/node/commit/1cd794f129)] - **buffer**: reapply 07c0667 (Fedor Indutny) [#2487](https://github.com/nodejs/node/pull/2487)
* [[`156781dedd`](https://github.com/nodejs/node/commit/156781dedd)] - **build**: use required platform in android-configure (Evan Lucas) [#2501](https://github.com/nodejs/node/pull/2501)
* [[`77075ec906`](https://github.com/nodejs/node/commit/77075ec906)] - **crypto**: fix mem {de}allocation in ExportChallenge (Karl Skomski) [#2359](https://github.com/nodejs/node/pull/2359)
* [[`cb30414d9e`](https://github.com/nodejs/node/commit/cb30414d9e)] - **doc**: sync CHANGELOG.md from master (Roman Reiss) [#2524](https://github.com/nodejs/node/pull/2524)
* [[`9330f5ef45`](https://github.com/nodejs/node/commit/9330f5ef45)] - **doc**: make the deprecations consistent (Sakthipriyan Vairamani) [#2450](https://github.com/nodejs/node/pull/2450)
* [[`09437e0146`](https://github.com/nodejs/node/commit/09437e0146)] - **doc**: fix comments in tls_wrap.cc and _http_client.js (Minwoo Jung) [#2489](https://github.com/nodejs/node/pull/2489)
* [[`c9867fed29`](https://github.com/nodejs/node/commit/c9867fed29)] - **doc**: document response.finished in http.markdown (hackerjs) [#2414](https://github.com/nodejs/node/pull/2414)
* [[`7f23a83c42`](https://github.com/nodejs/node/commit/7f23a83c42)] - **doc**: update AUTHORS list (Rod Vagg) [#2505](https://github.com/nodejs/node/pull/2505)
* [[`cd0c362f67`](https://github.com/nodejs/node/commit/cd0c362f67)] - **doc**: update AUTHORS list (Rod Vagg) [#2318](https://github.com/nodejs/node/pull/2318)
* [[`2c7b9257ea`](https://github.com/nodejs/node/commit/2c7b9257ea)] - **doc**: add TSC meeting minutes 2015-07-29 (Rod Vagg) [#2437](https://github.com/nodejs/node/pull/2437)
* [[`aaefde793e`](https://github.com/nodejs/node/commit/aaefde793e)] - **doc**: add TSC meeting minutes 2015-08-19 (Rod Vagg) [#2460](https://github.com/nodejs/node/pull/2460)
* [[`51ef9106f5`](https://github.com/nodejs/node/commit/51ef9106f5)] - **doc**: add TSC meeting minutes 2015-06-03 (Rod Vagg) [#2453](https://github.com/nodejs/node/pull/2453)
* [[`7130b4cf1d`](https://github.com/nodejs/node/commit/7130b4cf1d)] - **doc**: fix links to original converged repo (Rod Vagg) [#2454](https://github.com/nodejs/node/pull/2454)
* [[`14f2aee1df`](https://github.com/nodejs/node/commit/14f2aee1df)] - **doc**: fix links to original gh issues for TSC meetings (Rod Vagg) [#2454](https://github.com/nodejs/node/pull/2454)
* [[`87a9ef0a40`](https://github.com/nodejs/node/commit/87a9ef0a40)] - **doc**: add audio recording links to TSC meeting minutes (Rod Vagg) [#2454](https://github.com/nodejs/node/pull/2454)
* [[`f5cf24afbc`](https://github.com/nodejs/node/commit/f5cf24afbc)] - **doc**: add TSC meeting minutes 2015-07-22 (Rod Vagg) [#2436](https://github.com/nodejs/node/pull/2436)
* [[`3f821b96eb`](https://github.com/nodejs/node/commit/3f821b96eb)] - **doc**: fix spelling mistake in node.js comment (Jacob Edelman) [#2391](https://github.com/nodejs/node/pull/2391)
* [[`3e6a6fcdd6`](https://github.com/nodejs/node/commit/3e6a6fcdd6)] - **(SEMVER-MINOR)** **events**: deprecate static listenerCount function (Sakthipriyan Vairamani) [#2349](https://github.com/nodejs/node/pull/2349)
* [[`023386c852`](https://github.com/nodejs/node/commit/023386c852)] - **fs**: replace bad_args macro with concrete error msg (Roman Klauke) [#2495](https://github.com/nodejs/node/pull/2495)
* [[`d1c27b2e29`](https://github.com/nodejs/node/commit/d1c27b2e29)] - **module**: fix module preloading when cwd is ENOENT (Bradley Meck) [#2353](https://github.com/nodejs/node/pull/2353)
* [[`5d7486941b`](https://github.com/nodejs/node/commit/5d7486941b)] - **repl**: filter integer keys from repl tab complete list (James M Snell) [#2409](https://github.com/nodejs/node/pull/2409)
* [[`7f02443a9a`](https://github.com/nodejs/node/commit/7f02443a9a)] - **repl**: dont throw ENOENT on NODE_REPL_HISTORY_FILE (Todd Kennedy) [#2451](https://github.com/nodejs/node/pull/2451)
* [[`56a2ae9cef`](https://github.com/nodejs/node/commit/56a2ae9cef)] - **src**: improve startup time (Evan Lucas) [#2483](https://github.com/nodejs/node/pull/2483)
* [[`14653c7429`](https://github.com/nodejs/node/commit/14653c7429)] - **stream**: rename poorly named function (Ben Noordhuis) [#2479](https://github.com/nodejs/node/pull/2479)
* [[`1c6e014bfa`](https://github.com/nodejs/node/commit/1c6e014bfa)] - **stream**: micro-optimize high water mark calculation (Ben Noordhuis) [#2479](https://github.com/nodejs/node/pull/2479)
* [[`f1f4b4c46d`](https://github.com/nodejs/node/commit/f1f4b4c46d)] - **stream**: fix off-by-factor-16 error in comment (Ben Noordhuis) [#2479](https://github.com/nodejs/node/pull/2479)
* [[`2d3f09bd76`](https://github.com/nodejs/node/commit/2d3f09bd76)] - **stream_base**: various improvements (Fedor Indutny) [#2351](https://github.com/nodejs/node/pull/2351)
* [[`c1ce423b35`](https://github.com/nodejs/node/commit/c1ce423b35)] - **string_bytes**: fix unaligned write in UCS2 (Fedor Indutny) [#2480](https://github.com/nodejs/node/pull/2480)
* [[`e4d0e86165`](https://github.com/nodejs/node/commit/e4d0e86165)] - **test**: refactor test-https-simple.js (Rich Trott) [#2433](https://github.com/nodejs/node/pull/2433)
* [[`0ea5c8d737`](https://github.com/nodejs/node/commit/0ea5c8d737)] - **test**: remove test-timers-first-fire (João Reis) [#2458](https://github.com/nodejs/node/pull/2458)
* [[`536c3d0537`](https://github.com/nodejs/node/commit/536c3d0537)] - **test**: use reserved IP in test-net-connect-timeout (Rich Trott) [#2257](https://github.com/nodejs/node/pull/2257)
* [[`5df06fd8df`](https://github.com/nodejs/node/commit/5df06fd8df)] - **test**: add spaces after keywords (Brendan Ashworth)
* [[`e714b5620e`](https://github.com/nodejs/node/commit/e714b5620e)] - **test**: remove unreachable code (Michaël Zasso) [#2289](https://github.com/nodejs/node/pull/2289)
* [[`3579f3a2a4`](https://github.com/nodejs/node/commit/3579f3a2a4)] - **test**: disallow unreachable code (Michaël Zasso) [#2289](https://github.com/nodejs/node/pull/2289)
* [[`3545e236fc`](https://github.com/nodejs/node/commit/3545e236fc)] - **test**: reduce timeouts in test-net-keepalive (Brendan Ashworth) [#2429](https://github.com/nodejs/node/pull/2429)
* [[`b60e690023`](https://github.com/nodejs/node/commit/b60e690023)] - **test**: improve test-net-server-pause-on-connect (Brendan Ashworth) [#2429](https://github.com/nodejs/node/pull/2429)
* [[`11d1b8fcaf`](https://github.com/nodejs/node/commit/11d1b8fcaf)] - **test**: improve test-net-pingpong (Brendan Ashworth) [#2429](https://github.com/nodejs/node/pull/2429)
* [[`5fef5c6562`](https://github.com/nodejs/node/commit/5fef5c6562)] - **(SEMVER-MINOR)** **tls**: add --tls-cipher-list command line switch (James M Snell) [#2412](https://github.com/nodejs/node/pull/2412)
* [[`d9b70f9cbf`](https://github.com/nodejs/node/commit/d9b70f9cbf)] - **tls**: handle empty cert in checkServerIndentity (Mike Atkins) [#2343](https://github.com/nodejs/node/pull/2343)
* [[`4f8e34c202`](https://github.com/nodejs/node/commit/4f8e34c202)] - **tools**: add license boilerplate to check-imports.sh (James M Snell) [#2386](https://github.com/nodejs/node/pull/2386)
* [[`b76b9197f9`](https://github.com/nodejs/node/commit/b76b9197f9)] - **tools**: enable space-after-keywords in eslint (Brendan Ashworth)
* [[`64a8f30a70`](https://github.com/nodejs/node/commit/64a8f30a70)] - **tools**: fix anchors in generated documents (Sakthipriyan Vairamani) [#2491](https://github.com/nodejs/node/pull/2491)
* [[`22e344ea10`](https://github.com/nodejs/node/commit/22e344ea10)] - **win**: fix custom actions for WiX older than 3.9 (João Reis) [#2365](https://github.com/nodejs/node/pull/2365)
* [[`b5bd3ebfc8`](https://github.com/nodejs/node/commit/b5bd3ebfc8)] - **win**: fix custom actions on Visual Studio != 2013 (Julien Gilli) [#2365](https://github.com/nodejs/node/pull/2365)

## 2015-08-18, Version 3.1.0, @Fishrock123

### Notable changes

* **buffer**: Fixed a couple large memory leaks (Ben Noordhuis) [#2352](https://github.com/nodejs/node/pull/2352).
* **crypto**:
  - Fixed a couple of minor memory leaks (Karl Skomski) [#2375](https://github.com/nodejs/node/pull/2375).
  - Signing now checks for OpenSSL errors (P.S.V.R) [#2342](https://github.com/nodejs/node/pull/2342). **Note that this may expose previously hidden errors in user code.**
* **intl**: Intl support using small-icu is now enabled by default in builds (Steven R. Loomis) [#2264](https://github.com/nodejs/node/pull/2264).
  - [`String#normalize()`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/normalize) can now be used for unicode normalization.
  - The [`Intl`](https://developer.mozilla.org/en/docs/Web/JavaScript/Reference/Global_Objects/Intl) object and various `String` and `Number` methods are present, but only support the English locale.
  - For support of all locales, node must be built with [full-icu](https://github.com/nodejs/node#build-with-full-icu-support-all-locales-supported-by-icu).
* **tls**: Fixed tls throughput being much lower after an incorrect merge (Fedor Indutny) [#2381](https://github.com/nodejs/node/pull/2381).
* **tools**: The v8 tick processor now comes bundled with node (Matt Loring) [#2090](https://github.com/nodejs/node/pull/2090).
  - This can be used by producing performance profiling output by running node with `--perf`, then running your appropriate platform's script on the output as found in [tools/v8-prof](https://github.com/nodejs/node/tree/master/tools/v8-prof).
* **util**: `util.inspect(obj)` now prints the constructor name of the object if there is one (Christopher Monsanto) [#1935](https://github.com/nodejs/io.js/pull/1935).

### Known issues

See https://github.com/nodejs/io.js/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/io.js/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/io.js/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/io.js/issues/760).
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/io.js/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/io.js/issues/1435).

### Commits

* [[`3645dc62ed`](https://github.com/nodejs/node/commit/3645dc62ed)] - **build**: work around VS2015 issue in ICU <56 (Steven R. Loomis) [#2283](https://github.com/nodejs/node/pull/2283)
* [[`1f12e03266`](https://github.com/nodejs/node/commit/1f12e03266)] - **(SEMVER-MINOR)** **build**: intl: converge from joyent/node (Steven R. Loomis) [#2264](https://github.com/nodejs/node/pull/2264)
* [[`071640abdd`](https://github.com/nodejs/node/commit/071640abdd)] - **build**: Intl: bump ICU4C from 54 to 55 (Steven R. Loomis) [#2293](https://github.com/nodejs/node/pull/2293)
* [[`07a88b0c8b`](https://github.com/nodejs/node/commit/07a88b0c8b)] - **build**: update manifest to include Windows 10 (Lucien Greathouse) [#2332](https://github.com/nodejs/io.js/pull/2332)
* [[`0bb099f444`](https://github.com/nodejs/node/commit/0bb099f444)] - **build**: expand ~ in install prefix early (Ben Noordhuis) [#2307](https://github.com/nodejs/io.js/pull/2307)
* [[`7fe6dd8f5d`](https://github.com/nodejs/node/commit/7fe6dd8f5d)] - **crypto**: check for OpenSSL errors when signing (P.S.V.R) [#2342](https://github.com/nodejs/node/pull/2342)
* [[`605f6ee904`](https://github.com/nodejs/node/commit/605f6ee904)] - **crypto**: fix memory leak in PBKDF2Request (Karl Skomski) [#2375](https://github.com/nodejs/node/pull/2375)
* [[`ba6eb8af12`](https://github.com/nodejs/node/commit/ba6eb8af12)] - **crypto**: fix memory leak in ECDH::SetPrivateKey (Karl Skomski) [#2375](https://github.com/nodejs/node/pull/2375)
* [[`6a16368611`](https://github.com/nodejs/node/commit/6a16368611)] - **crypto**: fix memory leak in PublicKeyCipher::Cipher (Karl Skomski) [#2375](https://github.com/nodejs/node/pull/2375)
* [[`a760a87803`](https://github.com/nodejs/node/commit/a760a87803)] - **crypto**: fix memory leak in SafeX509ExtPrint (Karl Skomski) [#2375](https://github.com/nodejs/node/pull/2375)
* [[`f45487cd6e`](https://github.com/nodejs/node/commit/f45487cd6e)] - **crypto**: fix memory leak in SetDHParam (Karl Skomski) [#2375](https://github.com/nodejs/node/pull/2375)
* [[`2ff183dd86`](https://github.com/nodejs/node/commit/2ff183dd86)] - **doc**: Update FIPS instructions in README.md (Michael Dawson) [#2278](https://github.com/nodejs/node/pull/2278)
* [[`6483bc2e8f`](https://github.com/nodejs/node/commit/6483bc2e8f)] - **doc**: clarify options for fs.watchFile() (Rich Trott) [#2425](https://github.com/nodejs/node/pull/2425)
* [[`e76822f454`](https://github.com/nodejs/node/commit/e76822f454)] - **doc**: multiple documentation updates cherry picked from v0.12 (James M Snell) [#2302](https://github.com/nodejs/io.js/pull/2302)
* [[`1738c9680b`](https://github.com/nodejs/node/commit/1738c9680b)] - **net**: ensure Socket reported address is current (Ryan Graham) [#2095](https://github.com/nodejs/io.js/pull/2095)
* [[`844d3f0e3e`](https://github.com/nodejs/node/commit/844d3f0e3e)] - **path**: use '===' instead of '==' for comparison (Sam Stites) [#2388](https://github.com/nodejs/node/pull/2388)
* [[`7118b8a882`](https://github.com/nodejs/node/commit/7118b8a882)] - **path**: remove dead code in favor of unit tests (Nathan Woltman) [#2282](https://github.com/nodejs/io.js/pull/2282)
* [[`34f2cfa806`](https://github.com/nodejs/node/commit/34f2cfa806)] - **src**: better error message on failed Buffer malloc (Karl Skomski) [#2422](https://github.com/nodejs/node/pull/2422)
* [[`b196c1da3c`](https://github.com/nodejs/node/commit/b196c1da3c)] - **src**: fix memory leak in DLOpen (Karl Skomski) [#2375](https://github.com/nodejs/node/pull/2375)
* [[`d1307b2995`](https://github.com/nodejs/node/commit/d1307b2995)] - **src**: don't use fopen() in require() fast path (Ben Noordhuis) [#2377](https://github.com/nodejs/node/pull/2377)
* [[`455ec570d1`](https://github.com/nodejs/node/commit/455ec570d1)] - **src**: rename Buffer::Use() to Buffer::New() (Ben Noordhuis) [#2352](https://github.com/nodejs/node/pull/2352)
* [[`fd63e1ce2b`](https://github.com/nodejs/node/commit/fd63e1ce2b)] - **src**: introduce internal Buffer::Copy() function (Ben Noordhuis) [#2352](https://github.com/nodejs/node/pull/2352)
* [[`5586ceca13`](https://github.com/nodejs/node/commit/5586ceca13)] - **src**: move internal functions out of node_buffer.h (Ben Noordhuis) [#2352](https://github.com/nodejs/node/pull/2352)
* [[`bff9bcddb6`](https://github.com/nodejs/node/commit/bff9bcddb6)] - **src**: plug memory leaks (Ben Noordhuis) [#2352](https://github.com/nodejs/node/pull/2352)
* [[`ccf12df4f3`](https://github.com/nodejs/node/commit/ccf12df4f3)] - **(SEMVER-MINOR)** **src**: add total_available_size to v8 statistics (Roman Klauke) [#2348](https://github.com/nodejs/io.js/pull/2348)
* [[`194eeb841b`](https://github.com/nodejs/node/commit/194eeb841b)] - **test**: drop Isolate::GetCurrent() from addon tests (Ben Noordhuis) [#2427](https://github.com/nodejs/node/pull/2427)
* [[`46cdb2f6e2`](https://github.com/nodejs/node/commit/46cdb2f6e2)] - **test**: lint addon tests (Ben Noordhuis) [#2427](https://github.com/nodejs/node/pull/2427)
* [[`850c794882`](https://github.com/nodejs/node/commit/850c794882)] - **test**: refactor test-fs-watchfile.js (Rich Trott) [#2393](https://github.com/nodejs/node/pull/2393)
* [[`a3160c0a33`](https://github.com/nodejs/node/commit/a3160c0a33)] - **test**: correct spelling of 'childProcess' (muddletoes) [#2389](https://github.com/nodejs/node/pull/2389)
* [[`e51f90d747`](https://github.com/nodejs/node/commit/e51f90d747)] - **test**: option to run a subset of tests (João Reis) [#2260](https://github.com/nodejs/io.js/pull/2260)
* [[`cc46d3bca3`](https://github.com/nodejs/node/commit/cc46d3bca3)] - **test**: clarify dropMembership() call (Rich Trott) [#2062](https://github.com/nodejs/io.js/pull/2062)
* [[`0ee4df9c7a`](https://github.com/nodejs/node/commit/0ee4df9c7a)] - **test**: make listen-fd-cluster/server more robust (Sam Roberts) [#1944](https://github.com/nodejs/io.js/pull/1944)
* [[`cf9ba81398`](https://github.com/nodejs/node/commit/cf9ba81398)] - **test**: address timing issues in simple http tests (Gireesh Punathil) [#2294](https://github.com/nodejs/io.js/pull/2294)
* [[`cbb75c4f86`](https://github.com/nodejs/node/commit/cbb75c4f86)] - **tls**: fix throughput issues after incorrect merge (Fedor Indutny) [#2381](https://github.com/nodejs/node/pull/2381)
* [[`94b765f409`](https://github.com/nodejs/node/commit/94b765f409)] - **tls**: fix check for reused session (Fedor Indutny) [#2312](https://github.com/nodejs/io.js/pull/2312)
* [[`e83a41ad65`](https://github.com/nodejs/node/commit/e83a41ad65)] - **tls**: introduce internal `onticketkeycallback` (Fedor Indutny) [#2312](https://github.com/nodejs/io.js/pull/2312)
* [[`fb0f5d733f`](https://github.com/nodejs/node/commit/fb0f5d733f)] - **(SEMVER-MINOR)** **tools**: run the tick processor without building v8 (Matt Loring) [#2090](https://github.com/nodejs/node/pull/2090)
* [[`7606bdb897`](https://github.com/nodejs/node/commit/7606bdb897)] - **(SEMVER-MINOR)** **util**: display constructor when inspecting objects (Christopher Monsanto) [#1935](https://github.com/nodejs/io.js/pull/1935)

## 2015-08-04, Version 3.0.0, @rvagg

### Notable changes

* **buffer**:
  - Due to changes in V8, it has been necessary to reimplement `Buffer` on top of V8's `Uint8Array`. Every effort has been made to minimize the performance impact, however `Buffer` instantiation is measurably slower. Access operations may be faster in some circumstances but the exact performance profile and difference over previous versions will depend on how `Buffer` is used within applications. (Trevor Norris) [#1825](https://github.com/nodejs/node/pull/1825).
  - `Buffer` can now take `ArrayBuffer`s as a constructor argument (Trevor Norris) [#2002](https://github.com/nodejs/node/pull/2002).
  - When a single buffer is passed to `Buffer.concat()`, a new, copied `Buffer` object will be returned; previous behavior was to return the original `Buffer` object (Sakthipriyan Vairamani) [#1937](https://github.com/nodejs/node/pull/1937).
* **build**: PPC support has been added to core to allow compiling on pLinux BE and LE (AIX support coming soon) (Michael Dawson) [#2124](https://github.com/nodejs/node/pull/2124).
* **dgram**: If an error occurs within `socket.send()` and a callback has been provided, the error is only passed as the first argument to the callback and not emitted on the `socket` object; previous behavior was to do both (Matteo Collina & Chris Dickinson) [#1796](https://github.com/nodejs/node/pull/1796)
* **freelist**: Deprecate the undocumented `freelist` core module (Sakthipriyan Vairamani) [#2176](https://github.com/nodejs/node/pull/2176).
* **http**:
  - Status codes now all use the official [IANA names](http://www.iana.org/assignments/http-status-codes) as per [RFC7231](https://tools.ietf.org/html/rfc7231), e.g. `http.STATUS_CODES[414]` now returns `'URI Too Long'` rather than `'Request-URI Too Large'` (jomo) [#1470](https://github.com/nodejs/node/pull/1470).
  - Calling .getName() on an HTTP agent no longer returns a trailing colon, HTTPS agents will no longer return an extra colon near the middle of the string (Brendan Ashworth) [#1617](https://github.com/nodejs/node/pull/1617).
* **node**:
  - `NODE_MODULE_VERSION` has been bumped to `45` to reflect the break in ABI (Rod Vagg) [#2096](https://github.com/nodejs/node/pull/2096).
  - Introduce a new `process.release` object that contains a `name` property set to `'io.js'` and `sourceUrl`, `headersUrl` and `libUrl` (Windows only) properties containing URLs for the relevant resources; this is intended to be used by node-gyp (Rod Vagg) [#2154](https://github.com/nodejs/node/pull/2154).
  - The version of node-gyp bundled with io.js now downloads and uses a tarball of header files from iojs.org rather than the full source for compiling native add-ons; it is hoped this is a temporary floating patch and the change will be upstreamed to node-gyp soon (Rod Vagg) [#2066](https://github.com/nodejs/node/pull/2066).
* **repl**: Persistent history is now enabled by default. The history file is located at ~/.node_repl_history, which can be overridden by the new environment variable `NODE_REPL_HISTORY`. This deprecates the previous `NODE_REPL_HISTORY_FILE` variable. Additionally, the format of the file has been changed to plain text to better handle file corruption. (Jeremiah Senkpiel) [#2224](https://github.com/nodejs/node/pull/2224).
* **smalloc**: The `smalloc` module has been removed as it is no longer possible to provide the API due to changes in V8 (Ben Noordhuis) [#2022](https://github.com/nodejs/node/pull/2022).
* **tls**: Add `server.getTicketKeys()` and `server.setTicketKeys()` methods for [TLS session key](https://www.ietf.org/rfc/rfc5077.txt) rotation (Fedor Indutny) [#2227](https://github.com/nodejs/node/pull/2227).
* **v8**: Upgraded to 4.4.63.26
  - ES6: Enabled [computed property names](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Object_initializer#Computed_property_names)
  - ES6: `Array` can now be subclassed in strict mode
  - ES6: Implement [rest parameters](https://developer.mozilla.org/en/docs/Web/JavaScript/Reference/Functions/rest_parameters) in staging, use the `--harmony-rest-parameters` command line flag
  - ES6: Implement the [spread operator](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Spread_operator) in staging, use the `--harmony-spreadcalls` command line flag
  - Removed `SetIndexedPropertiesToExternalArrayData` and related APIs, forcing a shift to `Buffer` to be reimplemented based on `Uint8Array`
  - Introduction of `Maybe` and `MaybeLocal` C++ API for objects which _may_ or _may not_ have a value.
  - Added support for PPC

See also https://github.com/nodejs/node/wiki/Breaking-Changes#300-from-2x for a summary of the breaking changes (SEMVER-MAJOR).

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760).
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`60a974d200`](https://github.com/nodejs/node/commit/60a974d200)] - **buffer**: fix missing null/undefined check (Trevor Norris) [#2195](https://github.com/nodejs/node/pull/2195)
* [[`e6ab2d92bc`](https://github.com/nodejs/node/commit/e6ab2d92bc)] - **buffer**: fix not return on error (Trevor Norris) [#2225](https://github.com/nodejs/node/pull/2225)
* [[`1057d1186b`](https://github.com/nodejs/node/commit/1057d1186b)] - **buffer**: rename internal/buffer_new.js to buffer.js (Ben Noordhuis) [#2022](https://github.com/nodejs/node/pull/2022)
* [[`4643b8b667`](https://github.com/nodejs/node/commit/4643b8b667)] - **(SEMVER-MINOR)** **buffer**: allow ArrayBuffer as Buffer argument (Trevor Norris) [#2002](https://github.com/nodejs/node/pull/2002)
* [[`e5ada116cd`](https://github.com/nodejs/node/commit/e5ada116cd)] - **buffer**: minor cleanup from rebase (Trevor Norris) [#2003](https://github.com/nodejs/node/pull/2003)
* [[`b625ab4242`](https://github.com/nodejs/node/commit/b625ab4242)] - **buffer**: fix usage of kMaxLength (Trevor Norris) [#2003](https://github.com/nodejs/node/pull/2003)
* [[`eea66e2a7b`](https://github.com/nodejs/node/commit/eea66e2a7b)] - **(SEMVER-MAJOR)** **buffer**: fix case of one buffer passed to concat (Sakthipriyan Vairamani) [#1937](https://github.com/nodejs/node/pull/1937)
* [[`8664084166`](https://github.com/nodejs/node/commit/8664084166)] - **buffer**: make additional changes to native API (Trevor Norris) [#1825](https://github.com/nodejs/node/pull/1825)
* [[`36f78f4c1c`](https://github.com/nodejs/node/commit/36f78f4c1c)] - **buffer**: switch API to return MaybeLocal<T> (Trevor Norris) [#1825](https://github.com/nodejs/node/pull/1825)
* [[`571ec13841`](https://github.com/nodejs/node/commit/571ec13841)] - **buffer**: switch to using Maybe<T> API (Trevor Norris) [#1825](https://github.com/nodejs/node/pull/1825)
* [[`d75f5c8d0e`](https://github.com/nodejs/node/commit/d75f5c8d0e)] - **buffer**: finish implementing FreeCallback (Trevor Norris) [#1825](https://github.com/nodejs/node/pull/1825)
* [[`63da0dfd3a`](https://github.com/nodejs/node/commit/63da0dfd3a)] - **buffer**: implement Uint8Array backed Buffer (Trevor Norris) [#1825](https://github.com/nodejs/node/pull/1825)
* [[`23be6ca189`](https://github.com/nodejs/node/commit/23be6ca189)] - **buffer**: allow ARGS_THIS to accept a name (Trevor Norris) [#1825](https://github.com/nodejs/node/pull/1825)
* [[`971de5e417`](https://github.com/nodejs/node/commit/971de5e417)] - **build**: prepare Windows installer for i18n support (Frederic Hemberger) [#2247](https://github.com/nodejs/node/pull/2247)
* [[`2ba8b23661`](https://github.com/nodejs/node/commit/2ba8b23661)] - **build**: add 'x86' option back in to configure (Rod Vagg) [#2233](https://github.com/nodejs/node/pull/2233)
* [[`b4226e797a`](https://github.com/nodejs/node/commit/b4226e797a)] - **build**: first set of updates to enable PPC support (Michael Dawson) [#2124](https://github.com/nodejs/node/pull/2124)
* [[`24dd016deb`](https://github.com/nodejs/node/commit/24dd016deb)] - **build**: produce symbol map files on windows (Ali Ijaz Sheikh) [#2243](https://github.com/nodejs/node/pull/2243)
* [[`423d8944ce`](https://github.com/nodejs/node/commit/423d8944ce)] - **cluster**: do not unconditionally set --debug-port (cjihrig) [#1949](https://github.com/nodejs/node/pull/1949)
* [[`fa98b97171`](https://github.com/nodejs/node/commit/fa98b97171)] - **cluster**: add handle ref/unref stubs in rr mode (Ben Noordhuis) [#2274](https://github.com/nodejs/node/pull/2274)
* [[`944f68046c`](https://github.com/nodejs/node/commit/944f68046c)] - **crypto**: remove kMaxLength on randomBytes() (Trevor Norris) [#1825](https://github.com/nodejs/node/pull/1825)
* [[`3d3c687012`](https://github.com/nodejs/node/commit/3d3c687012)] - **deps**: update V8 to 4.4.63.26 (Michaël Zasso) [#2220](https://github.com/nodejs/node/pull/2220)
* [[`3aad4fa89a`](https://github.com/nodejs/node/commit/3aad4fa89a)] - **deps**: upgrade v8 to 4.4.63.12 (Ben Noordhuis) [#2092](https://github.com/nodejs/node/pull/2092)
* [[`70d1f32f56`](https://github.com/nodejs/node/commit/70d1f32f56)] - **(SEMVER-MAJOR)** **deps**: update v8 to 4.4.63.9 (Ben Noordhuis) [#2022](https://github.com/nodejs/node/pull/2022)
* [[`deb7ee93a7`](https://github.com/nodejs/node/commit/deb7ee93a7)] - **deps**: backport 7b24219346 from v8 upstream (Rod Vagg) [#1805](https://github.com/nodejs/node/pull/1805)
* [[`d58e780504`](https://github.com/nodejs/node/commit/d58e780504)] - **(SEMVER-MAJOR)** **deps**: update v8 to 4.3.61.21 (Chris Dickinson) [iojs/io.js#1632](https://github.com/iojs/io.js/pull/1632)
* [[`2a63cf612b`](https://github.com/nodejs/node/commit/2a63cf612b)] - **deps**: make node-gyp work with io.js (cjihrig) [iojs/io.js#990](https://github.com/iojs/io.js/pull/990)
* [[`bf63266460`](https://github.com/nodejs/node/commit/bf63266460)] - **deps**: upgrade to npm 2.13.3 (Kat Marchán) [#2284](https://github.com/nodejs/node/pull/2284)
* [[`ef2c8cd4ec`](https://github.com/nodejs/node/commit/ef2c8cd4ec)] - **(SEMVER-MAJOR)** **dgram**: make send cb act as "error" event handler (Matteo Collina) [#1796](https://github.com/nodejs/node/pull/1796)
* [[`3da057fef6`](https://github.com/nodejs/node/commit/3da057fef6)] - **(SEMVER-MAJOR)** **dgram**: make send cb act as "error" event handler (Chris Dickinson) [#1796](https://github.com/nodejs/node/pull/1796)
* [[`df1994fe53`](https://github.com/nodejs/node/commit/df1994fe53)] - ***Revert*** "**dns**: remove AI_V4MAPPED hint flag on FreeBSD" (cjihrig) [iojs/io.js#1555](https://github.com/iojs/io.js/pull/1555)
* [[`1721968b22`](https://github.com/nodejs/node/commit/1721968b22)] - **doc**: document repl persistent history changes (Jeremiah Senkpiel) [#2224](https://github.com/nodejs/node/pull/2224)
* [[`d12df7f159`](https://github.com/nodejs/node/commit/d12df7f159)] - **doc**: update v8 flags in man page (Michaël Zasso) [iojs/io.js#1701](https://github.com/iojs/io.js/pull/1701)
* [[`d168d01b04`](https://github.com/nodejs/node/commit/d168d01b04)] - **doc**: properly inheriting from EventEmitter (Sakthipriyan Vairamani) [#2168](https://github.com/nodejs/node/pull/2168)
* [[`500f2538cc`](https://github.com/nodejs/node/commit/500f2538cc)] - **doc**: a listener, not "an" listener (Sam Roberts) [#1025](https://github.com/nodejs/node/pull/1025)
* [[`54627a919d`](https://github.com/nodejs/node/commit/54627a919d)] - **doc**: server close event does not have an argument (Sam Roberts) [#1025](https://github.com/nodejs/node/pull/1025)
* [[`ed85c95a9c`](https://github.com/nodejs/node/commit/ed85c95a9c)] - **doc,test**: documents behaviour of non-existent file (Sakthipriyan Vairamani) [#2169](https://github.com/nodejs/node/pull/2169)
* [[`2965442308`](https://github.com/nodejs/node/commit/2965442308)] - **(SEMVER-MAJOR)** **http**: fix agent.getName() and add tests (Brendan Ashworth) [#1617](https://github.com/nodejs/node/pull/1617)
* [[`2d9456e3e6`](https://github.com/nodejs/node/commit/2d9456e3e6)] - **(SEMVER-MAJOR)** **http**: use official IANA Status Codes (jomo) [#1470](https://github.com/nodejs/node/pull/1470)
* [[`11e4249227`](https://github.com/nodejs/node/commit/11e4249227)] - **(SEMVER-MAJOR)** **http_server**: `prefinish` vs `finish` (Fedor Indutny) [#1411](https://github.com/nodejs/node/pull/1411)
* [[`9bc2e26720`](https://github.com/nodejs/node/commit/9bc2e26720)] - **net**: do not set V4MAPPED on FreeBSD (Julien Gilli) [iojs/io.js#1555](https://github.com/iojs/io.js/pull/1555)
* [[`ba9ccf227e`](https://github.com/nodejs/node/commit/ba9ccf227e)] - **node**: remove redundant --use-old-buffer (Rod Vagg) [#2275](https://github.com/nodejs/node/pull/2275)
* [[`ef65321083`](https://github.com/nodejs/node/commit/ef65321083)] - **(SEMVER-MAJOR)** **node**: do not override `message`/`stack` of error (Fedor Indutny) [#2108](https://github.com/nodejs/node/pull/2108)
* [[`9f727f5e03`](https://github.com/nodejs/node/commit/9f727f5e03)] - **node-gyp**: detect RC build with x.y.z-rc.n format (Rod Vagg) [#2171](https://github.com/nodejs/node/pull/2171)
* [[`e52f963632`](https://github.com/nodejs/node/commit/e52f963632)] - **node-gyp**: download header tarball for compile (Rod Vagg) [#2066](https://github.com/nodejs/node/pull/2066)
* [[`902c9ca51d`](https://github.com/nodejs/node/commit/902c9ca51d)] - **node-gyp**: make aware of nightly, next-nightly & rc (Rod Vagg) [#2066](https://github.com/nodejs/node/pull/2066)
* [[`4cffaa3f55`](https://github.com/nodejs/node/commit/4cffaa3f55)] - **(SEMVER-MINOR)** **readline**: allow tabs in input (Rich Trott) [#1761](https://github.com/nodejs/node/pull/1761)
* [[`ed6c249104`](https://github.com/nodejs/node/commit/ed6c249104)] - **(SEMVER-MAJOR)** **repl**: persist history in plain text (Jeremiah Senkpiel) [#2224](https://github.com/nodejs/node/pull/2224)
* [[`f7d5e4c618`](https://github.com/nodejs/node/commit/f7d5e4c618)] - **(SEMVER-MINOR)** **repl**: default persistence to ~/.node_repl_history (Jeremiah Senkpiel) [#2224](https://github.com/nodejs/node/pull/2224)
* [[`ea05e760cd`](https://github.com/nodejs/node/commit/ea05e760cd)] - **repl**: don't clobber RegExp.$ properties (Sakthipriyan Vairamani) [#2137](https://github.com/nodejs/node/pull/2137)
* [[`d20093246b`](https://github.com/nodejs/node/commit/d20093246b)] - **src**: disable vector ICs on arm (Michaël Zasso) [#2220](https://github.com/nodejs/node/pull/2220)
* [[`04fd4fad46`](https://github.com/nodejs/node/commit/04fd4fad46)] - **(SEMVER-MINOR)** **src**: introduce process.release object (Rod Vagg) [#2154](https://github.com/nodejs/node/pull/2154)
* [[`9d34bd1147`](https://github.com/nodejs/node/commit/9d34bd1147)] - **src**: increment NODE_MODULE_VERSION to 45 (Rod Vagg) [#2096](https://github.com/nodejs/node/pull/2096)
* [[`ceee8d2807`](https://github.com/nodejs/node/commit/ceee8d2807)] - **test**: add tests for persistent repl history (Jeremiah Senkpiel) [#2224](https://github.com/nodejs/node/pull/2224)
* [[`8e1a8ffe24`](https://github.com/nodejs/node/commit/8e1a8ffe24)] - **test**: remove two obsolete pummel tests (Ben Noordhuis) [#2022](https://github.com/nodejs/node/pull/2022)
* [[`ae731ec0fa`](https://github.com/nodejs/node/commit/ae731ec0fa)] - **test**: don't use arguments.callee (Ben Noordhuis) [#2022](https://github.com/nodejs/node/pull/2022)
* [[`21d31c08e7`](https://github.com/nodejs/node/commit/21d31c08e7)] - **test**: remove obsolete harmony flags (Chris Dickinson)
* [[`64cf71195c`](https://github.com/nodejs/node/commit/64cf71195c)] - **test**: change the hostname to an invalid name (Sakthipriyan Vairamani) [#2287](https://github.com/nodejs/node/pull/2287)
* [[`80a1cf7425`](https://github.com/nodejs/node/commit/80a1cf7425)] - **test**: fix messages and use return to skip tests (Sakthipriyan Vairamani) [#2290](https://github.com/nodejs/node/pull/2290)
* [[`d5ab92bcc1`](https://github.com/nodejs/node/commit/d5ab92bcc1)] - **test**: use common.isWindows consistently (Sakthipriyan Vairamani) [#2269](https://github.com/nodejs/node/pull/2269)
* [[`bc733f7065`](https://github.com/nodejs/node/commit/bc733f7065)] - **test**: fix fs.readFile('/dev/stdin') tests (Ben Noordhuis) [#2265](https://github.com/nodejs/node/pull/2265)
* [[`3cbb5870e5`](https://github.com/nodejs/node/commit/3cbb5870e5)] - **tools**: expose skip output to test runner (Johan Bergström) [#2130](https://github.com/nodejs/node/pull/2130)
* [[`3b021efe11`](https://github.com/nodejs/node/commit/3b021efe11)] - **vm**: fix symbol access (Domenic Denicola) [#1773](https://github.com/nodejs/node/pull/1773)
* [[`7b81e4ba36`](https://github.com/nodejs/node/commit/7b81e4ba36)] - **vm**: remove unnecessary access checks (Domenic Denicola) [#1773](https://github.com/nodejs/node/pull/1773)
* [[`659dadd410`](https://github.com/nodejs/node/commit/659dadd410)] - **vm**: fix property descriptors of sandbox properties (Domenic Denicola) [#1773](https://github.com/nodejs/node/pull/1773)
* [[`9bac1dbae9`](https://github.com/nodejs/node/commit/9bac1dbae9)] - **win,node-gyp**: enable delay-load hook by default (Bert Belder) [iojs/io.js#1433](https://github.com/iojs/io.js/pull/1433)

## 2015-07-28, Version 2.5.0, @cjihrig

### Notable changes

* **https**: TLS sessions in Agent are reused (Fedor Indutny) [#2228](https://github.com/nodejs/node/pull/2228)
* **src**: base64 decoding is now 50% faster (Ben Noordhuis) [#2193](https://github.com/nodejs/node/pull/2193)
* **npm**: Upgraded to v2.13.2, release notes can be found in <https://github.com/npm/npm/releases/tag/v2.13.2> (Kat Marchán) [#2241](https://github.com/nodejs/node/pull/2241).

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Using multiple REPL instances in parallel may cause some REPL history corruption or loss. [#1634](https://github.com/nodejs/node/issues/1634)
* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760).
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`bf2cd225a8`](https://github.com/nodejs/node/commit/bf2cd225a8)] - **process**: resize stderr on SIGWINCH (Jeremiah Senkpiel) [#2231](https://github.com/nodejs/node/pull/2231)
* [[`99d9d7e716`](https://github.com/nodejs/node/commit/99d9d7e716)] - **benchmark**: add remaining path benchmarks & optimize (Nathan Woltman) [#2103](https://github.com/nodejs/node/pull/2103)
* [[`66fc8ca22b`](https://github.com/nodejs/node/commit/66fc8ca22b)] - **(SEMVER-MINOR)** **cluster**: emit 'message' event on cluster master (Sam Roberts) [#861](https://github.com/nodejs/node/pull/861)
* [[`eb35968de7`](https://github.com/nodejs/node/commit/eb35968de7)] - **crypto**: fix legacy SNICallback (Fedor Indutny) [#1720](https://github.com/nodejs/node/pull/1720)
* [[`fef190cea6`](https://github.com/nodejs/node/commit/fef190cea6)] - **deps**: make node-gyp work with io.js (cjihrig) [iojs/io.js#990](https://github.com/iojs/io.js/pull/990)
* [[`b73a7465c5`](https://github.com/nodejs/node/commit/b73a7465c5)] - **deps**: upgrade to npm 2.13.2 (Kat Marchán) [#2241](https://github.com/nodejs/node/pull/2241)
* [[`0a7bf81d2f`](https://github.com/nodejs/node/commit/0a7bf81d2f)] - **deps**: update V8 to 4.2.77.21 (Ali Ijaz Sheikh) [#2238](https://github.com/nodejs/node/issues/2238)
* [[`73cdcdd581`](https://github.com/nodejs/node/commit/73cdcdd581)] - **deps**: make node-gyp work with io.js (cjihrig) [iojs/io.js#990](https://github.com/iojs/io.js/pull/990)
* [[`04893a736d`](https://github.com/nodejs/node/commit/04893a736d)] - **deps**: upgrade to npm 2.13.1 (Kat Marchán) [#2210](https://github.com/nodejs/node/pull/2210)
* [[`a3c1b9720e`](https://github.com/nodejs/node/commit/a3c1b9720e)] - **doc**: add GPG fingerprint for cjihrig (cjihrig) [#2217](https://github.com/nodejs/node/pull/2217)
* [[`d9f857df3b`](https://github.com/nodejs/node/commit/d9f857df3b)] - **doc**: note about custom inspect functions (Sakthipriyan Vairamani) [#2142](https://github.com/nodejs/node/pull/2142)
* [[`4ef2b5fbfb`](https://github.com/nodejs/node/commit/4ef2b5fbfb)] - **doc**: Replace util.debug with console.error (Yosuke Furukawa) [#2214](https://github.com/nodejs/node/pull/2214)
* [[`b612f085ec`](https://github.com/nodejs/node/commit/b612f085ec)] - **doc**: add joaocgreis as a collaborator (João Reis) [#2208](https://github.com/nodejs/node/pull/2208)
* [[`6b85d5a4b3`](https://github.com/nodejs/node/commit/6b85d5a4b3)] - **doc**: add TSC meeting minutes 2015-07-15 (Rod Vagg) [#2191](https://github.com/nodejs/node/pull/2191)
* [[`c7d8b09162`](https://github.com/nodejs/node/commit/c7d8b09162)] - **doc**: recompile before testing core module changes (Phillip Johnsen) [#2051](https://github.com/nodejs/node/pull/2051)
* [[`9afee6785e`](https://github.com/nodejs/node/commit/9afee6785e)] - **http**: Check this.connection before using it (Sakthipriyan Vairamani) [#2172](https://github.com/nodejs/node/pull/2172)
* [[`2ca5a3db47`](https://github.com/nodejs/node/commit/2ca5a3db47)] - **https**: reuse TLS sessions in Agent (Fedor Indutny) [#2228](https://github.com/nodejs/node/pull/2228)
* [[`fef87fee1d`](https://github.com/nodejs/node/commit/fef87fee1d)] - **(SEMVER-MINOR)** **lib,test**: add freelist deprecation and test (Sakthipriyan Vairamani) [#2176](https://github.com/nodejs/node/pull/2176)
* [[`503b089dd8`](https://github.com/nodejs/node/commit/503b089dd8)] - **net**: don't throw on immediately destroyed socket (Evan Lucas) [#2251](https://github.com/nodejs/node/pull/2251)
* [[`93660c8b8e`](https://github.com/nodejs/node/commit/93660c8b8e)] - **node**: remove bad fn call and check (Trevor Norris) [#2157](https://github.com/nodejs/node/pull/2157)
* [[`afd7e37ee0`](https://github.com/nodejs/node/commit/afd7e37ee0)] - **repl**: better empty line handling (Sakthipriyan Vairamani) [#2163](https://github.com/nodejs/node/pull/2163)
* [[`81ea52aa01`](https://github.com/nodejs/node/commit/81ea52aa01)] - **repl**: improving line continuation handling (Sakthipriyan Vairamani) [#2163](https://github.com/nodejs/node/pull/2163)
* [[`30edb5aee9`](https://github.com/nodejs/node/commit/30edb5aee9)] - **repl**: preventing REPL crash with inherited properties (Sakthipriyan Vairamani) [#2163](https://github.com/nodejs/node/pull/2163)
* [[`77fa385e5d`](https://github.com/nodejs/node/commit/77fa385e5d)] - **repl**: fixing `undefined` in invalid REPL keyword error (Sakthipriyan Vairamani) [#2163](https://github.com/nodejs/node/pull/2163)
* [[`8fd3ce100e`](https://github.com/nodejs/node/commit/8fd3ce100e)] - **src**: make base64 decoding 50% faster (Ben Noordhuis) [#2193](https://github.com/nodejs/node/pull/2193)
* [[`c786d6341d`](https://github.com/nodejs/node/commit/c786d6341d)] - **test**: do not use public IPs for timeout testing (Rich Trott) [#2057](https://github.com/nodejs/node/pull/2057)
* [[`4e78cd71c0`](https://github.com/nodejs/node/commit/4e78cd71c0)] - **test**: skip IPv6 part before testing it (Sakthipriyan Vairamani) [#2226](https://github.com/nodejs/node/pull/2226)
* [[`ac70bc8240`](https://github.com/nodejs/node/commit/ac70bc8240)] - **test**: fix valgrind uninitialized memory warning (Ben Noordhuis) [#2193](https://github.com/nodejs/node/pull/2193)
* [[`ac7d3fa0d9`](https://github.com/nodejs/node/commit/ac7d3fa0d9)] - **test**: add -no_rand_screen to s_client opts on Win (Shigeki Ohtsu) [#2209](https://github.com/nodejs/node/pull/2209)
* [[`79c865a53f`](https://github.com/nodejs/node/commit/79c865a53f)] - **test**: changing process.exit to return while skipping tests (Sakthipriyan Vairamani) [#2109](https://github.com/nodejs/node/pull/2109)
* [[`69298d36cf`](https://github.com/nodejs/node/commit/69298d36cf)] - **test**: formatting skip messages for TAP parsing (Sakthipriyan Vairamani) [#2109](https://github.com/nodejs/node/pull/2109)
* [[`543dabb609`](https://github.com/nodejs/node/commit/543dabb609)] - **timers**: improve Timer.now() performance (Ben Noordhuis) [#2256](https://github.com/nodejs/node/pull/2256)
* [[`3663b124e6`](https://github.com/nodejs/node/commit/3663b124e6)] - **timers**: remove unused Timer.again() (Ben Noordhuis) [#2256](https://github.com/nodejs/node/pull/2256)
* [[`bcce5cf9bb`](https://github.com/nodejs/node/commit/bcce5cf9bb)] - **timers**: remove unused Timer.getRepeat() (Ben Noordhuis) [#2256](https://github.com/nodejs/node/pull/2256)
* [[`f2c83bd202`](https://github.com/nodejs/node/commit/f2c83bd202)] - **timers**: remove unused Timer.setRepeat() (Ben Noordhuis) [#2256](https://github.com/nodejs/node/pull/2256)
* [[`e11fc67225`](https://github.com/nodejs/node/commit/e11fc67225)] - **(SEMVER-MINOR)** **tls**: add `getTicketKeys()`/`setTicketKeys()` (Fedor Indutny) [#2227](https://github.com/nodejs/node/pull/2227)
* [[`68b06e94e3`](https://github.com/nodejs/node/commit/68b06e94e3)] - **tools**: use local or specified $NODE for test-npm (Jeremiah Senkpiel) [#1984](https://github.com/nodejs/node/pull/1984)
* [[`ab479659c7`](https://github.com/nodejs/node/commit/ab479659c7)] - **util**: delay creation of debug context (Ali Ijaz Sheikh) [#2248](https://github.com/nodejs/node/pull/2248)
* [[`6391f4d2fd`](https://github.com/nodejs/node/commit/6391f4d2fd)] - **util**: removing redundant checks in is* functions (Sakthipriyan Vairamani) [#2179](https://github.com/nodejs/node/pull/2179)
* [[`b148c0dff3`](https://github.com/nodejs/node/commit/b148c0dff3)] - **win,node-gyp**: enable delay-load hook by default (Bert Belder) [iojs/io.js#1433](https://github.com/iojs/io.js/pull/1433)
* [[`f90f1e75bb`](https://github.com/nodejs/node/commit/f90f1e75bb)] - **win,node-gyp**: enable delay-load hook by default (Bert Belder) [iojs/io.js#1433](https://github.com/iojs/io.js/pull/1433)

## 2015-07-17, Version 2.4.0, @Fishrock123

### Notable changes

* **src**: Added a new `--track-heap-objects` flag to track heap object allocations for heap snapshots (Bradley Meck) [#2135](https://github.com/nodejs/node/pull/2135).
* **readline**: Fixed a freeze that affected the repl if the keypress event handler threw (Alex Kocharin) [#2107](https://github.com/nodejs/node/pull/2107).
* **npm**: Upgraded to v2.13.0, release notes can be found in <https://github.com/npm/npm/releases/tag/v2.13.0> (Forrest L Norvell) [#2152](https://github.com/nodejs/node/pull/2152).

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760).
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`f95f9ef6ea`](https://github.com/nodejs/node/commit/f95f9ef6ea)] - **build**: always use prefix=/ for tar-headers (Rod Vagg) [#2082](https://github.com/nodejs/node/pull/2082)
* [[`12bc397207`](https://github.com/nodejs/node/commit/12bc397207)] - **build**: run-ci makefile rule (Alexis Campailla) [#2134](https://github.com/nodejs/node/pull/2134)
* [[`84012c99e0`](https://github.com/nodejs/node/commit/84012c99e0)] - **build**: fix vcbuild merge issues (Alexis Campailla) [#2131](https://github.com/nodejs/node/pull/2131)
* [[`47e2c5c828`](https://github.com/nodejs/node/commit/47e2c5c828)] - **build**: bail early if clean is invoked (Johan Bergström) [#2127](https://github.com/nodejs/node/pull/2127)
* [[`5acad6b163`](https://github.com/nodejs/node/commit/5acad6b163)] - **child_process**: fix arguments comments (Roman Reiss) [#2161](https://github.com/nodejs/node/pull/2161)
* [[`3c4121c418`](https://github.com/nodejs/node/commit/3c4121c418)] - **deps**: make node-gyp work with io.js (cjihrig) [iojs/io.js#990](https://github.com/iojs/io.js/pull/990)
* [[`938cc757bb`](https://github.com/nodejs/node/commit/938cc757bb)] - **deps**: upgrade to npm 2.13.0 (Forrest L Norvell) [#2152](https://github.com/nodejs/node/pull/2152)
* [[`6f306e0ed2`](https://github.com/nodejs/node/commit/6f306e0ed2)] - **doc**: add targos as a collaborator (Michaël Zasso) [#2200](https://github.com/nodejs/node/pull/2200)
* [[`c019d9a239`](https://github.com/nodejs/node/commit/c019d9a239)] - **doc**: add thefourtheye as a collaborator (Sakthipriyan Vairamani) [#2199](https://github.com/nodejs/node/pull/2199)
* [[`4e92dbc26b`](https://github.com/nodejs/node/commit/4e92dbc26b)] - **doc**: add TSC members from the combined project (Jeremiah Senkpiel) [#2085](https://github.com/nodejs/node/pull/2085)
* [[`6c3aabf455`](https://github.com/nodejs/node/commit/6c3aabf455)] - **doc**: add TSC meeting minutes 2015-07-08 (Rod Vagg) [#2184](https://github.com/nodejs/node/pull/2184)
* [[`30a0d47d51`](https://github.com/nodejs/node/commit/30a0d47d51)] - **doc**: add TSC meeting minutes 2015-07-01 (Rod Vagg) [#2132](https://github.com/nodejs/node/pull/2132)
* [[`23efb05cc3`](https://github.com/nodejs/node/commit/23efb05cc3)] - **doc**: document fs.watchFile behaviour on ENOENT (Brendan Ashworth) [#2093](https://github.com/nodejs/node/pull/2093)
* [[`65963ec26f`](https://github.com/nodejs/node/commit/65963ec26f)] - **doc,test**: empty strings in path module (Sakthipriyan Vairamani) [#2106](https://github.com/nodejs/node/pull/2106)
* [[`0ab81e6f58`](https://github.com/nodejs/node/commit/0ab81e6f58)] - **docs**: link to more up-to-date v8 docs (Jeremiah Senkpiel) [#2196](https://github.com/nodejs/node/pull/2196)
* [[`1afc0c9e86`](https://github.com/nodejs/node/commit/1afc0c9e86)] - **fs**: fix error on bad listener type (Brendan Ashworth) [#2093](https://github.com/nodejs/node/pull/2093)
* [[`2ba84606a6`](https://github.com/nodejs/node/commit/2ba84606a6)] - **path**: assert path.join() arguments equally (Phillip Johnsen) [#2159](https://github.com/nodejs/node/pull/2159)
* [[`bd01603201`](https://github.com/nodejs/node/commit/bd01603201)] - **readline**: fix freeze if `keypress` event throws (Alex Kocharin) [#2107](https://github.com/nodejs/node/pull/2107)
* [[`59f6b5da2a`](https://github.com/nodejs/node/commit/59f6b5da2a)] - **repl**: Prevent crash when tab-completed with Proxy (Sakthipriyan Vairamani) [#2120](https://github.com/nodejs/node/pull/2120)
* [[`cf14a2427c`](https://github.com/nodejs/node/commit/cf14a2427c)] - **(SEMVER-MINOR)** **src**: add --track-heap-objects (Bradley Meck) [#2135](https://github.com/nodejs/node/pull/2135)
* [[`2b4b600660`](https://github.com/nodejs/node/commit/2b4b600660)] - **test**: fix test-debug-port-from-cmdline (João Reis) [#2186](https://github.com/nodejs/node/pull/2186)
* [[`d4ceb16da2`](https://github.com/nodejs/node/commit/d4ceb16da2)] - **test**: properly clean up temp directory (Roman Reiss) [#2164](https://github.com/nodejs/node/pull/2164)
* [[`842eb5b853`](https://github.com/nodejs/node/commit/842eb5b853)] - **test**: add test for dgram.setTTL (Evan Lucas) [#2121](https://github.com/nodejs/node/pull/2121)
* [[`cff7300a57`](https://github.com/nodejs/node/commit/cff7300a57)] - **win,node-gyp**: enable delay-load hook by default (Bert Belder) [iojs/io.js#1433](https://github.com/iojs/io.js/pull/1433)

## 2015-07-09, Version 2.3.4, @Fishrock123

### Notable changes

* **openssl**: Upgrade to 1.0.2d, fixes CVE-2015-1793 (Alternate Chains Certificate Forgery) (Shigeki Ohtsu) [#2141](https://github.com/nodejs/node/pull/2141).
* **npm**: Upgraded to v2.12.1, release notes can be found in <https://github.com/npm/npm/releases/tag/v2.12.0> and <https://github.com/npm/npm/releases/tag/v2.12.1> (Kat Marchán) [#2112](https://github.com/nodejs/node/pull/2112).

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760).
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`0d15161c24`](https://github.com/nodejs/node/commit/0d15161c24)] - **benchmark**: Add some path benchmarks for #1778 (Nathan Woltman) [#1778](https://github.com/nodejs/node/pull/1778)
* [[`c70e68fa32`](https://github.com/nodejs/node/commit/c70e68fa32)] - **deps**: update deps/openssl/conf/arch/*/opensslconf.h (Shigeki Ohtsu) [#2141](https://github.com/nodejs/node/pull/2141)
* [[`ca93f7f2e6`](https://github.com/nodejs/node/commit/ca93f7f2e6)] - **deps**: upgrade openssl sources to 1.0.2d (Shigeki Ohtsu) [#2141](https://github.com/nodejs/node/pull/2141)
* [[`b18c841ec1`](https://github.com/nodejs/node/commit/b18c841ec1)] - **deps**: make node-gyp work with io.js (cjihrig) [iojs/io.js#990](https://github.com/iojs/io.js/pull/990)
* [[`863cdbdd08`](https://github.com/nodejs/node/commit/863cdbdd08)] - **deps**: upgrade to npm 2.12.1 (Kat Marchán) [#2112](https://github.com/nodejs/node/pull/2112)
* [[`84b3915764`](https://github.com/nodejs/node/commit/84b3915764)] - **doc**: document current release procedure (Rod Vagg) [#2099](https://github.com/nodejs/node/pull/2099)
* [[`46140334cd`](https://github.com/nodejs/node/commit/46140334cd)] - **doc**: update AUTHORS list (Rod Vagg) [#2100](https://github.com/nodejs/node/pull/2100)
* [[`bca53dce76`](https://github.com/nodejs/node/commit/bca53dce76)] - **path**: refactor for performance and consistency (Nathan Woltman) [#1778](https://github.com/nodejs/node/pull/1778)
* [[`6bef15afe7`](https://github.com/nodejs/node/commit/6bef15afe7)] - **src**: remove traceSyncIO property from process (Bradley Meck) [#2143](https://github.com/nodejs/node/pull/2143)
* [[`2ba1740ba1`](https://github.com/nodejs/node/commit/2ba1740ba1)] - **test**: add missing crypto checks (Johan Bergström) [#2129](https://github.com/nodejs/node/pull/2129)
* [[`180fd392ca`](https://github.com/nodejs/node/commit/180fd392ca)] - **test**: refactor test-repl-tab-complete (Sakthipriyan Vairamani) [#2122](https://github.com/nodejs/node/pull/2122)
* [[`fb05c8e27d`](https://github.com/nodejs/node/commit/fb05c8e27d)] - ***Revert*** "**test**: add test for missing `close`/`finish` event" (Fedor Indutny)
* [[`9436a860cb`](https://github.com/nodejs/node/commit/9436a860cb)] - **test**: add test for missing `close`/`finish` event (Mark Plomer) [iojs/io.js#1373](https://github.com/iojs/io.js/pull/1373)
* [[`ee3ce2ed88`](https://github.com/nodejs/node/commit/ee3ce2ed88)] - **tools**: install gdbinit from v8 to $PREFIX/share (Ali Ijaz Sheikh) [#2123](https://github.com/nodejs/node/pull/2123)
* [[`dd523c75da`](https://github.com/nodejs/node/commit/dd523c75da)] - **win,node-gyp**: enable delay-load hook by default (Bert Belder) [iojs/io.js#1433](https://github.com/iojs/io.js/pull/1433)

## 2015-07-09, Version 0.12.7 (Stable)

### Commits

* [[`0cf9f27703`](https://github.com/nodejs/node/commit/0cf9f27703)] - **deps**: upgrade openssl sources to 1.0.1p [#25654](https://github.com/joyent/node/pull/25654)
* [[`8917e430b8`](https://github.com/nodejs/node/commit/8917e430b8)] - **deps**: upgrade to npm 2.11.3 [#25545](https://github.com/joyent/node/pull/25545)
* [[`88a27a9621`](https://github.com/nodejs/node/commit/88a27a9621)] - **V8**: cherry-pick JitCodeEvent patch from upstream (Ben Noordhuis) [#25589](https://github.com/joyent/node/pull/25589)
* [[`18d413d299`](https://github.com/nodejs/node/commit/18d413d299)] - **win,msi**: create npm folder in AppData directory (Steven Rockarts) [#8838](https://github.com/joyent/node/pull/8838)

## 2015-07-09, Version 0.10.40 (Maintenance)

### Commits

* [[`0cf9f27703`](https://github.com/nodejs/node/commit/0cf9f27703)] - **openssl**: upgrade to 1.0.1p [#25654](https://github.com/joyent/node/pull/25654)
* [[`5a60e0d904`](https://github.com/nodejs/node/commit/5a60e0d904)] - **V8**: back-port JitCodeEvent patch from upstream (Ben Noordhuis) [#25588](https://github.com/joyent/node/pull/25588)
* [[`18d413d299`](https://github.com/nodejs/node/commit/18d413d299)] - **win,msi**: create npm folder in AppData directory (Steven Rockarts) [#8838](https://github.com/joyent/node/pull/8838)

## 2015-07-04, Version 2.3.3, @Fishrock123

### Notable changes

* **deps**: Fixed an out-of-band write in utf8 decoder. **This is an important security update** as it can be used to cause a denial of service attack.

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760).
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

## Commits

* [[`030f8045c7`](https://github.com/nodejs/node/commit/030f8045c7)] - **deps**: fix out-of-band write in utf8 decoder (Fedor Indutny)
* [[`0f09b8db28`](https://github.com/nodejs/node/commit/0f09b8db28)] - **doc**: don't recommend domains for error handling (Benjamin Gruenbaum) [#2056](https://github.com/nodejs/node/pull/2056)
* [[`9cd44bb2b6`](https://github.com/nodejs/node/commit/9cd44bb2b6)] - **util**: prepend '(node) ' to deprecation messages (Sakthipriyan Vairamani) [#1892](https://github.com/nodejs/node/pull/1892)

## 2015-07-03, Version 0.12.6 (Stable)

### Notable changes

* **deps**: Fixed an out-of-band write in utf8 decoder. **This is an important security update** as it can be used to cause a denial of service attack.

### Commits

* [[`78b0e30954`](https://github.com/nodejs/node/commit/78b0e30954)] - **deps**: fix out-of-band write in utf8 decoder (Fedor Indutny)

## 2015-07-01, Version 2.3.2, @rvagg

### Notable changes

* **build**:
  - Added support for compiling with Microsoft Visual C++ 2015
  - Started building and distributing headers-only tarballs along with binaries

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

## Commits

* [[`9180140231`](https://github.com/nodejs/node/commit/9180140231)] - **_stream_wrap**: prevent use after free in TLS (Fedor Indutny) [#1910](https://github.com/nodejs/node/pull/1910)
* [[`05a73c0f25`](https://github.com/nodejs/node/commit/05a73c0f25)] - **benchmark**: make concurrent requests configurable (Rich Trott) [#2068](https://github.com/nodejs/node/pull/2068)
* [[`f52d73352e`](https://github.com/nodejs/node/commit/f52d73352e)] - **benchmark**: fix typo in README (Rich Trott) [#2067](https://github.com/nodejs/node/pull/2067)
* [[`1cd9eeb556`](https://github.com/nodejs/node/commit/1cd9eeb556)] - **buffer**: prevent abort on bad proto (Trevor Norris) [#2012](https://github.com/nodejs/node/pull/2012)
* [[`8350f3a3a2`](https://github.com/nodejs/node/commit/8350f3a3a2)] - **buffer**: optimize Buffer#toString() (Ben Noordhuis) [#2027](https://github.com/nodejs/node/pull/2027)
* [[`628a3ab093`](https://github.com/nodejs/node/commit/628a3ab093)] - **build**: add tar-headers target for headers-only tar (Rod Vagg) [#1975](https://github.com/nodejs/node/pull/1975)
* [[`dcbb9e1da6`](https://github.com/nodejs/node/commit/dcbb9e1da6)] - **build**: update build targets for io.js (Rod Vagg) [#1938](https://github.com/nodejs/node/pull/1938)
* [[`c87c34c242`](https://github.com/nodejs/node/commit/c87c34c242)] - **build**: fix cherry-pick ooops, fix comment wording (Rod Vagg) [#2036](https://github.com/nodejs/node/pull/2036)
* [[`4208dc4fef`](https://github.com/nodejs/node/commit/4208dc4fef)] - **build**: add MSVS 2015 support (Rod Vagg) [#2036](https://github.com/nodejs/node/pull/2036)
* [[`834a365113`](https://github.com/nodejs/node/commit/834a365113)] - **build**: DTrace is enabled by default on darwin (Evan Lucas) [#2019](https://github.com/nodejs/node/pull/2019)
* [[`c0c0d73269`](https://github.com/nodejs/node/commit/c0c0d73269)] - **build,win**: set env before generating projects (Alexis Campailla) [joyent/node#20109](https://github.com/joyent/node/pull/20109)
* [[`9e890fe8b4`](https://github.com/nodejs/node/commit/9e890fe8b4)] - **crypto**: fix VerifyCallback in case of verify error (Shigeki Ohtsu) [#2064](https://github.com/nodejs/node/pull/2064)
* [[`1f371e3988`](https://github.com/nodejs/node/commit/1f371e3988)] - **deps**: copy all openssl header files to include dir (Shigeki Ohtsu) [#2016](https://github.com/nodejs/node/pull/2016)
* [[`c370bd3aea`](https://github.com/nodejs/node/commit/c370bd3aea)] - **doc**: make the abbreviation 1MM clear (Ivan Yan) [#2053](https://github.com/nodejs/node/pull/2053)
* [[`54d5437566`](https://github.com/nodejs/node/commit/54d5437566)] - **doc**: Added sample command to test iojs build (Jimmy Hsu) [#850](https://github.com/nodejs/node/pull/850)
* [[`f1f1b7e597`](https://github.com/nodejs/node/commit/f1f1b7e597)] - **doc**: add TSC meeting minutes 2015-06-17 (Rod Vagg) [#2048](https://github.com/nodejs/node/pull/2048)
* [[`dbd5dc932d`](https://github.com/nodejs/node/commit/dbd5dc932d)] - **doc**: clarify prerequisites in benchmark/README.md (Jeremiah Senkpiel) [#2034](https://github.com/nodejs/node/pull/2034)
* [[`50dbc8e143`](https://github.com/nodejs/node/commit/50dbc8e143)] - **doc**: add TSC meeting minutes 2015-05-27 (Rod Vagg) [#2037](https://github.com/nodejs/node/pull/2037)
* [[`941ad362a7`](https://github.com/nodejs/node/commit/941ad362a7)] - **doc**: archive io.js TC minutes (Rod Vagg)
* [[`644b2eaa89`](https://github.com/nodejs/node/commit/644b2eaa89)] - **doc**: rename tc-meetings to tsc-meetings (Rod Vagg)
* [[`1330ee3b27`](https://github.com/nodejs/node/commit/1330ee3b27)] - **doc**: add TC meeting 2015-05-13 minutes (Rod Vagg) [#1700](https://github.com/nodejs/node/pull/1700)
* [[`392e8fd64e`](https://github.com/nodejs/node/commit/392e8fd64e)] - **doc**: add @shigeki and @mscdex to TC (Rod Vagg) [#2008](https://github.com/nodejs/node/pull/2008)
* [[`af249fa8a1`](https://github.com/nodejs/node/commit/af249fa8a1)] - **net**: wrap connect in nextTick (Evan Lucas) [#2054](https://github.com/nodejs/node/pull/2054)
* [[`7f63449fde`](https://github.com/nodejs/node/commit/7f63449fde)] - **net**: fix debug for dnsopts (Evan Lucas) [#2059](https://github.com/nodejs/node/pull/2059)
* [[`eabed2f518`](https://github.com/nodejs/node/commit/eabed2f518)] - **repl**: remove obsolete TODO (Rich Trott) [#2081](https://github.com/nodejs/node/pull/2081)
* [[`a198c68b56`](https://github.com/nodejs/node/commit/a198c68b56)] - **repl**: make 'Unexpected token' errors recoverable (Julien Gilli) [#2052](https://github.com/nodejs/node/pull/2052)
* [[`d735b2c6ef`](https://github.com/nodejs/node/commit/d735b2c6ef)] - **repl**: fix tab completion for a non-global context (Sangmin Yoon) [#2052](https://github.com/nodejs/node/pull/2052)
* [[`8cee8f54fc`](https://github.com/nodejs/node/commit/8cee8f54fc)] - **src**: nix stdin _readableState.reading manipulation (Chris Dickinson) [#454](https://github.com/nodejs/node/pull/454)
* [[`856c11f8c8`](https://github.com/nodejs/node/commit/856c11f8c8)] - **test**: purge stale disabled tests (Rich Trott) [#2045](https://github.com/nodejs/node/pull/2045)
* [[`4d5089e181`](https://github.com/nodejs/node/commit/4d5089e181)] - **test**: do not swallow OpenSSL support error (Rich Trott) [#2042](https://github.com/nodejs/node/pull/2042)
* [[`06721fe005`](https://github.com/nodejs/node/commit/06721fe005)] - **test**: fix test-repl-tab-complete.js (cjihrig) [#2052](https://github.com/nodejs/node/pull/2052)
* [[`8e9089ac35`](https://github.com/nodejs/node/commit/8e9089ac35)] - **test**: check for error on Windows (Rich Trott) [#2035](https://github.com/nodejs/node/pull/2035)
* [[`776a65ebcd`](https://github.com/nodejs/node/commit/776a65ebcd)] - **test**: remove obsolete TODO comments (Rich Trott) [#2033](https://github.com/nodejs/node/pull/2033)
* [[`bdfeb798ad`](https://github.com/nodejs/node/commit/bdfeb798ad)] - **test**: remove obsolete TODO comments (Rich Trott) [#2032](https://github.com/nodejs/node/pull/2032)
* [[`58e914f9bc`](https://github.com/nodejs/node/commit/58e914f9bc)] - **tools**: fix gyp to work on MacOSX without XCode (Shigeki Ohtsu) [iojs/io.js#1325](https://github.com/iojs/io.js/pull/1325)
* [[`99cbbc0a13`](https://github.com/nodejs/node/commit/99cbbc0a13)] - **tools**: update gyp to 25ed9ac (Ben Noordhuis) [#2074](https://github.com/nodejs/node/pull/2074)
* [[`e3f9335c40`](https://github.com/nodejs/node/commit/e3f9335c40)] - **tools**: re-enable comma-spacing linter rule (Roman Reiss) [#2072](https://github.com/nodejs/node/pull/2072)
* [[`d91e10b3bd`](https://github.com/nodejs/node/commit/d91e10b3bd)] - **tools**: update eslint to 0.24.0 (Roman Reiss) [#2072](https://github.com/nodejs/node/pull/2072)
* [[`6c61ca5325`](https://github.com/nodejs/node/commit/6c61ca5325)] - **url**: fix typo in comment (Rich Trott) [#2071](https://github.com/nodejs/node/pull/2071)
* [[`1a51f0058c`](https://github.com/nodejs/node/commit/1a51f0058c)] - **v8**: cherry-pick JitCodeEvent patch from upstream (Ben Noordhuis) [#2075](https://github.com/nodejs/node/pull/2075)

## 2015-06-23, Version 2.3.1, @rvagg

### Notable changes

* **module**: The number of syscalls made during a `require()` have been significantly reduced again (see [#1801](https://github.com/nodejs/node/pull/1801) from v2.2.0 for previous work), which should lead to a performance improvement (Pierre Inglebert) [#1920](https://github.com/nodejs/node/pull/1920).
* **npm**:
  * Upgrade to [v2.11.2](https://github.com/npm/npm/releases/tag/v2.11.2) (Rebecca Turner) [#1956](https://github.com/nodejs/node/pull/1956).
  * Upgrade to [v2.11.3](https://github.com/npm/npm/releases/tag/v2.11.3) (Forrest L Norvell) [#2018](https://github.com/nodejs/node/pull/2018).
* **zlib**: A bug was discovered where the process would abort if the final part of a zlib decompression results in a buffer that would exceed the maximum length of `0x3fffffff` bytes (~1GiB). This was likely to only occur during buffered decompression (rather than streaming). This is now fixed and will instead result in a thrown `RangeError` (Michaël Zasso) [#1811](https://github.com/nodejs/node/pull/1811).

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

## Commits

* [[`e56758a5e0`](https://github.com/nodejs/node/commit/e56758a5e0)] - **async-wrap**: add provider id and object info cb (Trevor Norris) [#1896](https://github.com/nodejs/node/pull/1896)
* [[`d5637e67c9`](https://github.com/nodejs/node/commit/d5637e67c9)] - **buffer**: fix cyclic dependency with util (Brendan Ashworth) [#1988](https://github.com/nodejs/node/pull/1988)
* [[`c5353d7c62`](https://github.com/nodejs/node/commit/c5353d7c62)] - **build**: remove lint from test-ci on windows (Johan Bergström) [#2004](https://github.com/nodejs/node/pull/2004)
* [[`c207e8d223`](https://github.com/nodejs/node/commit/c207e8d223)] - **build**: fix pkg-config output parsing in configure (Ben Noordhuis) [#1986](https://github.com/nodejs/node/pull/1986)
* [[`8d8a26e8f7`](https://github.com/nodejs/node/commit/8d8a26e8f7)] - **build**: don't run lint from test-ci (Johan Bergström) [#1965](https://github.com/nodejs/node/pull/1965)
* [[`1ec53c044d`](https://github.com/nodejs/node/commit/1ec53c044d)] - **build**: simplify execution of built binary (Johan Bergström) [#1955](https://github.com/nodejs/node/pull/1955)
* [[`3beb880716`](https://github.com/nodejs/node/commit/3beb880716)] - **crypto**: add cert check to CNNIC Whitelist (Shigeki Ohtsu) [#1895](https://github.com/nodejs/node/pull/1895)
* [[`48c0fb8b1a`](https://github.com/nodejs/node/commit/48c0fb8b1a)] - **deps**: make node-gyp work with io.js (cjihrig) [iojs/io.js#990](https://github.com/iojs/io.js/pull/990)
* [[`6a359b1ce9`](https://github.com/nodejs/node/commit/6a359b1ce9)] - **deps**: upgrade to npm 2.11.3 (Forrest L Norvell) [#2018](https://github.com/nodejs/node/pull/2018)
* [[`6aab2f3b9a`](https://github.com/nodejs/node/commit/6aab2f3b9a)] - **deps**: make node-gyp work with io.js (cjihrig) [iojs/io.js#990](https://github.com/iojs/io.js/pull/990)
* [[`3e12561b55`](https://github.com/nodejs/node/commit/3e12561b55)] - **deps**: upgrade to npm 2.11.2 (Rebecca Turner) [#1956](https://github.com/nodejs/node/pull/1956)
* [[`8ac50819b6`](https://github.com/nodejs/node/commit/8ac50819b6)] - **doc**: add security section to README.md (Rod Vagg) [#1948](https://github.com/nodejs/node/pull/1948)
* [[`1f93b63b11`](https://github.com/nodejs/node/commit/1f93b63b11)] - **doc**: change the info to the same as in gitconfig (Christian Tellnes) [#2000](https://github.com/nodejs/node/pull/2000)
* [[`0cf94e6856`](https://github.com/nodejs/node/commit/0cf94e6856)] - **doc**: mention CI in Collaborator Guide (Rich Trott) [#1995](https://github.com/nodejs/node/pull/1995)
* [[`7a3006efe4`](https://github.com/nodejs/node/commit/7a3006efe4)] - **doc**: add TOC links to Collaborator Guide (Rich Trott) [#1994](https://github.com/nodejs/node/pull/1994)
* [[`30638b150f`](https://github.com/nodejs/node/commit/30638b150f)] - **doc**: add TSC meeting notes 2015-06-10 (Bert Belder) [#1943](https://github.com/nodejs/node/pull/1943)
* [[`c4ec04136b`](https://github.com/nodejs/node/commit/c4ec04136b)] - **doc**: reformat authors section (Johan Bergström) [#1966](https://github.com/nodejs/node/pull/1966)
* [[`96165f9be2`](https://github.com/nodejs/node/commit/96165f9be2)] - **doc**: minor clarification in the modules API doc. (Сковорода Никита Андреевич) [#1983](https://github.com/nodejs/node/pull/1983)
* [[`5c2707c1b2`](https://github.com/nodejs/node/commit/5c2707c1b2)] - **doc**: benchmark/README.md copyedit (Rich Trott) [#1970](https://github.com/nodejs/node/pull/1970)
* [[`74fdf732d0`](https://github.com/nodejs/node/commit/74fdf732d0)] - **doc**: copyedit COLLABORATOR_GUIDE.md (Rich Trott) [#1964](https://github.com/nodejs/node/pull/1964)
* [[`5fe6e83640`](https://github.com/nodejs/node/commit/5fe6e83640)] - **doc**: copyedit GOVERNANCE.md (Rich Trott) [#1963](https://github.com/nodejs/node/pull/1963)
* [[`428526544c`](https://github.com/nodejs/node/commit/428526544c)] - **doc**: add ChALkeR as collaborator (Сковорода Никита Андреевич) [#1927](https://github.com/nodejs/node/pull/1927)
* [[`5dfe0d5d61`](https://github.com/nodejs/node/commit/5dfe0d5d61)] - **doc**: remove irrelevant SEMVER-MINOR & MAJOR (Rod Vagg)
* [[`fb8811d95e`](https://github.com/nodejs/node/commit/fb8811d95e)] - **lib,test**: fix whitespace issues (Roman Reiss) [#1971](https://github.com/nodejs/node/pull/1971)
* [[`a4f4909f3d`](https://github.com/nodejs/node/commit/a4f4909f3d)] - **module**: fix stat with long paths on Windows (Michaël Zasso) [#2013](https://github.com/nodejs/node/pull/2013)
* [[`a71ee93afe`](https://github.com/nodejs/node/commit/a71ee93afe)] - **module**: reduce syscalls during require search (Pierre Inglebert) [#1920](https://github.com/nodejs/node/pull/1920)
* [[`671e64ac73`](https://github.com/nodejs/node/commit/671e64ac73)] - **module**: allow long paths for require on Windows (Michaël Zasso)
* [[`061342a500`](https://github.com/nodejs/node/commit/061342a500)] - **net**: Defer reading until listeners could be added (James Hartig) [#1496](https://github.com/nodejs/node/pull/1496)
* [[`5d2b846d11`](https://github.com/nodejs/node/commit/5d2b846d11)] - **test**: assert tmp and fixture dirs different (Rich Trott) [#2015](https://github.com/nodejs/node/pull/2015)
* [[`b0990ef45d`](https://github.com/nodejs/node/commit/b0990ef45d)] - **test**: confirm symlink (Rich Trott) [#2014](https://github.com/nodejs/node/pull/2014)
* [[`3ba4f71fc4`](https://github.com/nodejs/node/commit/3ba4f71fc4)] - **test**: check result as early as possible (Rich Trott) [#2007](https://github.com/nodejs/node/pull/2007)
* [[`0abcf44d6b`](https://github.com/nodejs/node/commit/0abcf44d6b)] - **test**: add Buffer slice UTF-8 test (Rich Trott) [#1989](https://github.com/nodejs/node/pull/1989)
* [[`88c1831ff4`](https://github.com/nodejs/node/commit/88c1831ff4)] - **test**: tmpdir creation failures should fail tests (Rich Trott) [#1976](https://github.com/nodejs/node/pull/1976)
* [[`52a822d944`](https://github.com/nodejs/node/commit/52a822d944)] - **test**: fix test-cluster-worker-disconnect (Santiago Gimeno) [#1919](https://github.com/nodejs/node/pull/1919)
* [[`7c79490bfb`](https://github.com/nodejs/node/commit/7c79490bfb)] - **test**: only refresh tmpDir for tests that need it (Rich Trott) [#1954](https://github.com/nodejs/node/pull/1954)
* [[`88d7904c0b`](https://github.com/nodejs/node/commit/88d7904c0b)] - **test**: remove test repetition (Rich Trott) [#1874](https://github.com/nodejs/node/pull/1874)
* [[`91dfb5e094`](https://github.com/nodejs/node/commit/91dfb5e094)] - **tools**: make test-npm work without global npm (Jeremiah Senkpiel) [#1926](https://github.com/nodejs/node/pull/1926)
* [[`3777f41562`](https://github.com/nodejs/node/commit/3777f41562)] - **tools**: enable whitespace related rules in eslint (Roman Reiss) [#1971](https://github.com/nodejs/node/pull/1971)
* [[`626432d843`](https://github.com/nodejs/node/commit/626432d843)] - **util**: dont repeat isBuffer (Brendan Ashworth) [#1988](https://github.com/nodejs/node/pull/1988)
* [[`1d79f572f1`](https://github.com/nodejs/node/commit/1d79f572f1)] - **util**: move deprecate() to internal module (Brendan Ashworth) [#1988](https://github.com/nodejs/node/pull/1988)
* [[`4b4b1760b5`](https://github.com/nodejs/node/commit/4b4b1760b5)] - **v8**: cherry-pick uclibc build patch from upstream (Ben Noordhuis) [#1974](https://github.com/nodejs/node/pull/1974)
* [[`5d0cee46bb`](https://github.com/nodejs/node/commit/5d0cee46bb)] - **vm**: remove unnecessary HandleScopes (Ben Noordhuis) [#2001](https://github.com/nodejs/node/pull/2001)
* [[`0ecf9457b5`](https://github.com/nodejs/node/commit/0ecf9457b5)] - **win,node-gyp**: enable delay-load hook by default (Bert Belder) [iojs/io.js#1433](https://github.com/iojs/io.js/pull/1433)
* [[`953b3e75e8`](https://github.com/nodejs/node/commit/953b3e75e8)] - **win,node-gyp**: enable delay-load hook by default (Bert Belder) [iojs/io.js#1433](https://github.com/iojs/io.js/pull/1433)
* [[`3806d875d3`](https://github.com/nodejs/node/commit/3806d875d3)] - **zlib**: prevent uncaught exception in zlibBuffer (Michaël Zasso) [#1811](https://github.com/nodejs/node/pull/1811)

## 2015-06-22, Version 0.12.5 (Stable)

### Commits

* [[`456c22f63f`](https://github.com/nodejs/node/commit/456c22f63f)] - **openssl**: upgrade to 1.0.1o (Addressing multiple CVEs) [#25523](https://github.com/joyent/node/pull/25523)
* [[`20d8db1a42`](https://github.com/nodejs/node/commit/20d8db1a42)] - **npm**: upgrade to 2.11.2 [#25517](https://github.com/joyent/node/pull/25517)
* [[`50f961596d`](https://github.com/nodejs/node/commit/50f961596d)] - **uv**: upgrade to 1.6.1 [#25475](https://github.com/joyent/node/pull/25475)
* [[`b81a643f9a`](https://github.com/nodejs/node/commit/b81a643f9a)] - **V8**: avoid deadlock when profiling is active (Dmitri Melikyan) [#25309](https://github.com/joyent/node/pull/25309)
* [[`9d19dfbfdb`](https://github.com/nodejs/node/commit/9d19dfbfdb)] - **install**: fix source path for openssl headers (Oguz Bastemur) [#14089](https://github.com/joyent/node/pull/14089)
* [[`4028669531`](https://github.com/nodejs/node/commit/4028669531)] - **install**: make sure opensslconf.h is overwritten (Oguz Bastemur) [#14089](https://github.com/joyent/node/pull/14089)
* [[`d38e865fce`](https://github.com/nodejs/node/commit/d38e865fce)] - **timers**: fix timeout when added in timer's callback (Julien Gilli) [#17203](https://github.com/joyent/node/pull/17203)
* [[`e7c84f82c7`](https://github.com/nodejs/node/commit/e7c84f82c7)] - **windows**: broadcast WM_SETTINGCHANGE after install (Mathias Küsel) [#25100](https://github.com/joyent/node/pull/25100)

## 2015-06-18, Version 0.10.39 (Maintenance)

### Commits

* [[`456c22f63f`](https://github.com/nodejs/node/commit/456c22f63f)] - **openssl**: upgrade to 1.0.1o (Addressing multiple CVEs) [#25523](https://github.com/joyent/node/pull/25523)
* [[`9d19dfbfdb`](https://github.com/nodejs/node/commit/9d19dfbfdb)] - **install**: fix source path for openssl headers (Oguz Bastemur) [#14089](https://github.com/joyent/node/pull/14089)
* [[`4028669531`](https://github.com/nodejs/node/commit/4028669531)] - **install**: make sure opensslconf.h is overwritten (Oguz Bastemur) [#14089](https://github.com/joyent/node/pull/14089)
* [[`d38e865fce`](https://github.com/nodejs/node/commit/d38e865fce)] - **timers**: fix timeout when added in timer's callback (Julien Gilli) [#17203](https://github.com/joyent/node/pull/17203)
* [[`e7c84f82c7`](https://github.com/nodejs/node/commit/e7c84f82c7)] - **windows**: broadcast WM_SETTINGCHANGE after install (Mathias Küsel) [#25100](https://github.com/joyent/node/pull/25100)

## 2015-06-13, Version 2.3.0, @rvagg

### Notable changes

* **libuv**: Upgraded to 1.6.0 and 1.6.1, see [full ChangeLog](https://github.com/libuv/libuv/blob/60e515d9e6f3d86c0eedad583805201f32ea3aed/ChangeLog#L1-L36) for details. (Saúl Ibarra Corretgé) [#1905](https://github.com/nodejs/node/pull/1905) [#1889](https://github.com/nodejs/node/pull/1889). Highlights include:
  - Fix TTY becoming blocked on OS X
  - Fix UDP send callbacks to not to be synchronous
  - Add `uv_os_homedir()` (exposed as `os.homedir()`, see below)
* **npm**: See full [release notes](https://github.com/npm/npm/releases/tag/v2.11.1) for details. (Kat Marchán) [#1899](https://github.com/nodejs/node/pull/1899). Highlight:
  - Use GIT_SSH_COMMAND (available as of Git 2.3)
* **openssl**:
  - Upgrade to 1.0.2b and 1.0.2c, introduces DHE man-in-the-middle protection (Logjam) and fixes malformed ECParameters causing infinite loop (CVE-2015-1788). See the [security advisory](https://www.openssl.org/news/secadv_20150611.txt) for full details. (Shigeki Ohtsu) [#1950](https://github.com/nodejs/node/pull/1950) [#1958](https://github.com/nodejs/node/pull/1958)
  - Support [FIPS](https://en.wikipedia.org/wiki/Federal_Information_Processing_Standards) mode of OpenSSL, see [README](https://github.com/nodejs/node#building-iojs-with-fips-compliant-openssl) for instructions. (Fedor Indutny) [#1890](https://github.com/nodejs/node/pull/1890)
* **os**: Add `os.homedir()` method. (Colin Ihrig) [#1791](https://github.com/nodejs/node/pull/1791)
* **smalloc**: Deprecate whole module. (Vladimir Kurchatkin) [#1822](https://github.com/nodejs/node/pull/1822)
* Add new collaborators:
  - Alex Kocharin ([@rlidwka](https://github.com/rlidwka))
  - Christopher Monsanto ([@monsanto](https://github.com/monsanto))
  - Ali Ijaz Sheikh ([@ofrobots](https://github.com/ofrobots))
  - Oleg Elifantiev ([@Olegas](https://github.com/Olegas))
  - Domenic Denicola ([@domenic](https://github.com/domenic))
  - Rich Trott ([@Trott](https://github.com/Trott))

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

## Commits

* [[`9c0a1b8cfc`](https://github.com/nodejs/node/commit/9c0a1b8cfc)] - **cluster**: wait on servers closing before disconnect (Oleg Elifantiev) [#1400](https://github.com/nodejs/node/pull/1400)
* [[`0f68377f69`](https://github.com/nodejs/node/commit/0f68377f69)] - **crypto**: support FIPS mode of OpenSSL (Fedor Indutny) [#1890](https://github.com/nodejs/node/pull/1890)
* [[`38d1afc24d`](https://github.com/nodejs/node/commit/38d1afc24d)] - **(SEMVER-MINOR)** **crypto**: add getCurves() to get supported ECs (Brian White) [#1914](https://github.com/nodejs/node/pull/1914)
* [[`a4dbf45b59`](https://github.com/nodejs/node/commit/a4dbf45b59)] - **crypto**: update root certificates (Ben Noordhuis) [#1833](https://github.com/nodejs/node/pull/1833)
* [[`81029c639a`](https://github.com/nodejs/node/commit/81029c639a)] - **debugger**: improve ESRCH error message (Jackson Tian) [#1863](https://github.com/nodejs/node/pull/1863)
* [[`2a7fd0ad32`](https://github.com/nodejs/node/commit/2a7fd0ad32)] - **deps**: update UPGRADING.md doc to openssl-1.0.2c (Shigeki Ohtsu) [#1958](https://github.com/nodejs/node/pull/1958)
* [[`6b3df929e0`](https://github.com/nodejs/node/commit/6b3df929e0)] - **deps**: replace all headers in openssl (Shigeki Ohtsu) [#1958](https://github.com/nodejs/node/pull/1958)
* [[`664a659696`](https://github.com/nodejs/node/commit/664a659696)] - **deps**: add -no_rand_screen to openssl s_client (Shigeki Ohtsu) [#1836](https://github.com/nodejs/node/pull/1836)
* [[`42a8de2ac6`](https://github.com/nodejs/node/commit/42a8de2ac6)] - **deps**: fix asm build error of openssl in x86_win32 (Shigeki Ohtsu) [iojs/io.js#1389](https://github.com/iojs/io.js/pull/1389)
* [[`c66c3d9fa3`](https://github.com/nodejs/node/commit/c66c3d9fa3)] - **deps**: fix openssl assembly error on ia32 win32 (Fedor Indutny) [iojs/io.js#1389](https://github.com/iojs/io.js/pull/1389)
* [[`86737cf0a0`](https://github.com/nodejs/node/commit/86737cf0a0)] - **deps**: upgrade openssl sources to 1.0.2c (Shigeki Ohtsu) [#1958](https://github.com/nodejs/node/pull/1958)
* [[`94804969b7`](https://github.com/nodejs/node/commit/94804969b7)] - **deps**: update asm files for openssl-1.0.2b (Shigeki Ohtsu) [#1950](https://github.com/nodejs/node/pull/1950)
* [[`38444915e0`](https://github.com/nodejs/node/commit/38444915e0)] - **deps**: replace all headers in openssl (Shigeki Ohtsu) [#1950](https://github.com/nodejs/node/pull/1950)
* [[`f62b613252`](https://github.com/nodejs/node/commit/f62b613252)] - **deps**: add -no_rand_screen to openssl s_client (Shigeki Ohtsu) [#1836](https://github.com/nodejs/node/pull/1836)
* [[`f624d0122c`](https://github.com/nodejs/node/commit/f624d0122c)] - **deps**: fix asm build error of openssl in x86_win32 (Shigeki Ohtsu) [iojs/io.js#1389](https://github.com/iojs/io.js/pull/1389)
* [[`dcd67cc8d7`](https://github.com/nodejs/node/commit/dcd67cc8d7)] - **deps**: fix openssl assembly error on ia32 win32 (Fedor Indutny) [iojs/io.js#1389](https://github.com/iojs/io.js/pull/1389)
* [[`c21b24decf`](https://github.com/nodejs/node/commit/c21b24decf)] - **deps**: upgrade openssl sources to 1.0.2b (Shigeki Ohtsu) [#1950](https://github.com/nodejs/node/pull/1950)
* [[`2dc819b09a`](https://github.com/nodejs/node/commit/2dc819b09a)] - **deps**: make node-gyp work with io.js (cjihrig) [iojs/io.js#990](https://github.com/iojs/io.js/pull/990)
* [[`f41b7f12b5`](https://github.com/nodejs/node/commit/f41b7f12b5)] - **deps**: upgrade to npm 2.11.1 (Kat Marchán) [#1899](https://github.com/nodejs/node/pull/1899)
* [[`a5bd466440`](https://github.com/nodejs/node/commit/a5bd466440)] - **deps**: update libuv to version 1.6.1 (Saúl Ibarra Corretgé) [#1905](https://github.com/nodejs/node/pull/1905)
* [[`aa33db3238`](https://github.com/nodejs/node/commit/aa33db3238)] - **deps**: update libuv to version 1.6.0 (Saúl Ibarra Corretgé) [#1889](https://github.com/nodejs/node/pull/1889)
* [[`0ee497f0b4`](https://github.com/nodejs/node/commit/0ee497f0b4)] - **deps**: add -no_rand_screen to openssl s_client (Shigeki Ohtsu) [#1836](https://github.com/nodejs/node/pull/1836)
* [[`b5cd2f0986`](https://github.com/nodejs/node/commit/b5cd2f0986)] - **dgram**: partially revert 18d457b (Saúl Ibarra Corretgé) [#1889](https://github.com/nodejs/node/pull/1889)
* [[`a3cc43d0a4`](https://github.com/nodejs/node/commit/a3cc43d0a4)] - **doc**: add Trott as collaborator (Rich Trott) [#1962](https://github.com/nodejs/node/pull/1962)
* [[`cf5020fc02`](https://github.com/nodejs/node/commit/cf5020fc02)] - **doc**: add domenic as collaborator (Domenic Denicola) [#1942](https://github.com/nodejs/node/pull/1942)
* [[`11ed5f31ab`](https://github.com/nodejs/node/commit/11ed5f31ab)] - **doc**: add Olegas as collaborator (Oleg Elifantiev) [#1930](https://github.com/nodejs/node/pull/1930)
* [[`f500e1833b`](https://github.com/nodejs/node/commit/f500e1833b)] - **doc**: add ofrobots as collaborator (Ali Ijaz Sheikh)
* [[`717724611a`](https://github.com/nodejs/node/commit/717724611a)] - **doc**: add monsanto as collaborator (Christopher Monsanto) [#1932](https://github.com/nodejs/node/pull/1932)
* [[`7192b6688c`](https://github.com/nodejs/node/commit/7192b6688c)] - **doc**: add rlidwka as collaborator (Alex Kocharin) [#1929](https://github.com/nodejs/node/pull/1929)
* [[`9f3a03f0d4`](https://github.com/nodejs/node/commit/9f3a03f0d4)] - **doc**: add references to crypto.getCurves() (Roman Reiss) [#1918](https://github.com/nodejs/node/pull/1918)
* [[`ff39ecb914`](https://github.com/nodejs/node/commit/ff39ecb914)] - **doc**: remove comma splice (Rich Trott) [#1900](https://github.com/nodejs/node/pull/1900)
* [[`deb8b87dc9`](https://github.com/nodejs/node/commit/deb8b87dc9)] - **doc**: add note about available ECC curves (Ryan Petschek) [#1913](https://github.com/nodejs/node/pull/1913)
* [[`89a5b9040e`](https://github.com/nodejs/node/commit/89a5b9040e)] - **doc**: fix http.IncomingMessage.socket documentation (Сковорода Никита Андреевич) [#1867](https://github.com/nodejs/node/pull/1867)
* [[`d29034b34b`](https://github.com/nodejs/node/commit/d29034b34b)] - **doc**: adjust changelog to clarify `client` revert (Rod Vagg) [#1859](https://github.com/nodejs/node/pull/1859)
* [[`a79dece8ad`](https://github.com/nodejs/node/commit/a79dece8ad)] - **docs**: add return value for sync fs functions (Tyler Anton) [#1770](https://github.com/nodejs/node/pull/1770)
* [[`1cb72c14c4`](https://github.com/nodejs/node/commit/1cb72c14c4)] - **docs**: delete unused/duplicate css files (Robert Kowalski) [#1770](https://github.com/nodejs/node/pull/1770)
* [[`53a4eb3198`](https://github.com/nodejs/node/commit/53a4eb3198)] - **fs**: make SyncWriteStream non-enumerable (Sakthipriyan Vairamani) [#1870](https://github.com/nodejs/node/pull/1870)
* [[`a011c3243f`](https://github.com/nodejs/node/commit/a011c3243f)] - **fs**: minor refactoring (Sakthipriyan Vairamani) [#1870](https://github.com/nodejs/node/pull/1870)
* [[`8841132f30`](https://github.com/nodejs/node/commit/8841132f30)] - **fs**: remove inStatWatchers and use Map for lookup (Sakthipriyan Vairamani) [#1870](https://github.com/nodejs/node/pull/1870)
* [[`67a11b9bcc`](https://github.com/nodejs/node/commit/67a11b9bcc)] - **fs**: removing unnecessary nullCheckCallNT (Sakthipriyan Vairamani) [#1870](https://github.com/nodejs/node/pull/1870)
* [[`09f2a67bd8`](https://github.com/nodejs/node/commit/09f2a67bd8)] - **fs**: improve error message descriptions (Sakthipriyan Vairamani) [#1870](https://github.com/nodejs/node/pull/1870)
* [[`2dcef83b5f`](https://github.com/nodejs/node/commit/2dcef83b5f)] - **fs**: use `kMaxLength` from binding (Vladimir Kurchatkin) [#1903](https://github.com/nodejs/node/pull/1903)
* [[`353e26e3c7`](https://github.com/nodejs/node/commit/353e26e3c7)] - **(SEMVER-MINOR)** **fs**: Add string encoding option for Stream method (Yosuke Furukawa) [#1845](https://github.com/nodejs/node/pull/1845)
* [[`8357c5084b`](https://github.com/nodejs/node/commit/8357c5084b)] - **fs**: set encoding on fs.createWriteStream (Yosuke Furukawa) [#1844](https://github.com/nodejs/node/pull/1844)
* [[`02c345020a`](https://github.com/nodejs/node/commit/02c345020a)] - **gitignore**: don't ignore the debug npm module (Kat Marchán) [#1908](https://github.com/nodejs/node/pull/1908)
* [[`b5b8ff117c`](https://github.com/nodejs/node/commit/b5b8ff117c)] - **lib**: don't use global Buffer (Roman Reiss) [#1794](https://github.com/nodejs/node/pull/1794)
* [[`a251657058`](https://github.com/nodejs/node/commit/a251657058)] - **node**: mark promises as handled as soon as possible (Vladimir Kurchatkin) [#1952](https://github.com/nodejs/node/pull/1952)
* [[`2eb170874a`](https://github.com/nodejs/node/commit/2eb170874a)] - **openssl**: fix keypress requirement in apps on win32 (Shigeki Ohtsu) [iojs/io.js#1389](https://github.com/iojs/io.js/pull/1389)
* [[`a130132c8f`](https://github.com/nodejs/node/commit/a130132c8f)] - **openssl**: fix keypress requirement in apps on win32 (Shigeki Ohtsu) [iojs/io.js#1389](https://github.com/iojs/io.js/pull/1389)
* [[`6e78e5feaa`](https://github.com/nodejs/node/commit/6e78e5feaa)] - **(SEMVER-MINOR)** **os**: add homedir() (cjihrig) [#1791](https://github.com/nodejs/node/pull/1791)
* [[`d9e250295b`](https://github.com/nodejs/node/commit/d9e250295b)] - ***Revert*** "**readline**: allow tabs in input" (Jeremiah Senkpiel) [#1961](https://github.com/nodejs/node/pull/1961)
* [[`4b3d493c4b`](https://github.com/nodejs/node/commit/4b3d493c4b)] - **readline**: allow tabs in input (Rich Trott) [#1761](https://github.com/nodejs/node/pull/1761)
* [[`6d95f4ff92`](https://github.com/nodejs/node/commit/6d95f4ff92)] - **(SEMVER-MINOR)** **smalloc**: deprecate whole module (Vladimir Kurchatkin) [#1822](https://github.com/nodejs/node/pull/1822)
* [[`8c71a9241d`](https://github.com/nodejs/node/commit/8c71a9241d)] - **src**: hide InitializeICUDirectory symbol (Ben Noordhuis) [#1815](https://github.com/nodejs/node/pull/1815)
* [[`5b6f575c1f`](https://github.com/nodejs/node/commit/5b6f575c1f)] - ***Revert*** "**src**: add getopt option parser" (Evan Lucas) [#1862](https://github.com/nodejs/node/pull/1862)
* [[`c0e7bf2d8c`](https://github.com/nodejs/node/commit/c0e7bf2d8c)] - **src**: add getopt option parser (Evan Lucas) [#1804](https://github.com/nodejs/node/pull/1804)
* [[`8ea6844d26`](https://github.com/nodejs/node/commit/8ea6844d26)] - **test**: add test for failed save in REPL (Rich Trott) [#1818](https://github.com/nodejs/node/pull/1818)
* [[`03ce84dfa1`](https://github.com/nodejs/node/commit/03ce84dfa1)] - **test**: fix cluster-worker-wait-server-close races (Sam Roberts) [#1953](https://github.com/nodejs/node/pull/1953)
* [[`a6b8ee19b8`](https://github.com/nodejs/node/commit/a6b8ee19b8)] - **test**: create temp dir in common.js (Rich Trott) [#1877](https://github.com/nodejs/node/pull/1877)
* [[`ff8202c6f4`](https://github.com/nodejs/node/commit/ff8202c6f4)] - **test**: fix undeclared variable access (Roman Reiss) [#1794](https://github.com/nodejs/node/pull/1794)
* [[`d9ddd7d345`](https://github.com/nodejs/node/commit/d9ddd7d345)] - **test**: remove TODO comment (Rich Trott) [#1820](https://github.com/nodejs/node/pull/1820)
* [[`6537fd4b55`](https://github.com/nodejs/node/commit/6537fd4b55)] - **test**: remove TODO (Rich Trott) [#1875](https://github.com/nodejs/node/pull/1875)
* [[`a804026c9b`](https://github.com/nodejs/node/commit/a804026c9b)] - **test**: fix broken FreeBSD test (Santiago Gimeno) [#1881](https://github.com/nodejs/node/pull/1881)
* [[`43a82f8a71`](https://github.com/nodejs/node/commit/43a82f8a71)] - **test**: fix test-sync-io-option (Evan Lucas) [#1840](https://github.com/nodejs/node/pull/1840)
* [[`4ed25f664d`](https://github.com/nodejs/node/commit/4ed25f664d)] - **test**: add -no_rand_screen for tls-server-verify (Shigeki Ohtsu) [#1836](https://github.com/nodejs/node/pull/1836)
* [[`4cf323d23d`](https://github.com/nodejs/node/commit/4cf323d23d)] - **test**: kill child in tls-server-verify for speed up (Shigeki Ohtsu) [#1836](https://github.com/nodejs/node/pull/1836)
* [[`e6ccdcc1fe`](https://github.com/nodejs/node/commit/e6ccdcc1fe)] - **test**: improve console output of tls-server-verify (João Reis) [#1836](https://github.com/nodejs/node/pull/1836)
* [[`975e5956f0`](https://github.com/nodejs/node/commit/975e5956f0)] - **test**: run tls-server-verify servers in parallel (João Reis) [#1836](https://github.com/nodejs/node/pull/1836)
* [[`b18604ba2c`](https://github.com/nodejs/node/commit/b18604ba2c)] - **test**: running tls-server-verify clients in parallel (João Reis) [#1836](https://github.com/nodejs/node/pull/1836)
* [[`f78c722df5`](https://github.com/nodejs/node/commit/f78c722df5)] - **test**: remove hardwired references to 'iojs' (Rod Vagg) [#1882](https://github.com/nodejs/node/pull/1882)
* [[`bd99e8de8e`](https://github.com/nodejs/node/commit/bd99e8de8e)] - **test**: more test coverage for maxConnections (Rich Trott) [#1855](https://github.com/nodejs/node/pull/1855)
* [[`b9267189a5`](https://github.com/nodejs/node/commit/b9267189a5)] - **test**: fix test-child-process-stdout-flush-exit (Santiago Gimeno) [#1868](https://github.com/nodejs/node/pull/1868)
* [[`d20f018dcf`](https://github.com/nodejs/node/commit/d20f018dcf)] - **test**: loosen condition to detect infinite loop (Yosuke Furukawa) [#1857](https://github.com/nodejs/node/pull/1857)
* [[`e0e96acc6f`](https://github.com/nodejs/node/commit/e0e96acc6f)] - **test**: remove smalloc add-on test (Ben Noordhuis) [#1835](https://github.com/nodejs/node/pull/1835)
* [[`8704c58fc4`](https://github.com/nodejs/node/commit/8704c58fc4)] - **test**: remove unneeded comment task (Rich Trott) [#1858](https://github.com/nodejs/node/pull/1858)
* [[`8732977536`](https://github.com/nodejs/node/commit/8732977536)] - **tls**: fix references to undefined `cb` (Fedor Indutny) [#1951](https://github.com/nodejs/node/pull/1951)
* [[`75930bb38c`](https://github.com/nodejs/node/commit/75930bb38c)] - **tls**: prevent use-after-free (Fedor Indutny) [#1702](https://github.com/nodejs/node/pull/1702)
* [[`5795e835a1`](https://github.com/nodejs/node/commit/5795e835a1)] - **tls**: emit errors on close whilst async action (Fedor Indutny) [#1702](https://github.com/nodejs/node/pull/1702)
* [[`59d9734e21`](https://github.com/nodejs/node/commit/59d9734e21)] - **tls_wrap**: invoke queued callbacks in DestroySSL (Fedor Indutny) [#1702](https://github.com/nodejs/node/pull/1702)
* [[`6e4d30286d`](https://github.com/nodejs/node/commit/6e4d30286d)] - **tools**: enable/add additional eslint rules (Roman Reiss) [#1794](https://github.com/nodejs/node/pull/1794)
* [[`098354a9f8`](https://github.com/nodejs/node/commit/098354a9f8)] - **tools**: update certdata.txt (Ben Noordhuis) [#1833](https://github.com/nodejs/node/pull/1833)
* [[`a2d921d6a0`](https://github.com/nodejs/node/commit/a2d921d6a0)] - **tools**: customize mk-ca-bundle.pl (Ben Noordhuis) [#1833](https://github.com/nodejs/node/pull/1833)
* [[`5be9efca40`](https://github.com/nodejs/node/commit/5be9efca40)] - **tools**: update mk-ca-bundle.pl to HEAD of upstream (Ben Noordhuis) [#1833](https://github.com/nodejs/node/pull/1833)
* [[`1baba0580d`](https://github.com/nodejs/node/commit/1baba0580d)] - **tools**: Fix copying contents of deps/npm (thefourtheye) [#1853](https://github.com/nodejs/node/pull/1853)
* [[`628845b816`](https://github.com/nodejs/node/commit/628845b816)] - **(SEMVER-MINOR)** **util**: introduce `printDeprecationMessage` function (Vladimir Kurchatkin) [#1822](https://github.com/nodejs/node/pull/1822)
* [[`91d0a8b19c`](https://github.com/nodejs/node/commit/91d0a8b19c)] - **win,node-gyp**: enable delay-load hook by default (Bert Belder) [iojs/io.js#1433](https://github.com/iojs/io.js/pull/1433)

## 2015-06-01, Version 2.2.1, @rvagg

### Notable changes

* **http**: Reverts the move of the `client` property of `IncomingMessage` to its prototype. Although undocumented, this property was used and assumed to be an "own property" in the wild, most notably by [request](https://github.com/request/request) which is used by npm. (Michaël Zasso) [#1852](https://github.com/nodejs/node/pull/1852).

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`c5a1009903`](https://github.com/nodejs/node/commit/c5a1009903)] - **build**: avoid passing empty strings to build flags (Johan Bergström) [#1789](https://github.com/nodejs/node/pull/1789)
* [[`5d83401086`](https://github.com/nodejs/node/commit/5d83401086)] - **doc**: put SEMVER-MINOR on pre-load module fix 2.2.0 (Rod Vagg)
* [[`4d6b768e5d`](https://github.com/nodejs/node/commit/4d6b768e5d)] - **http**: revert deprecation of client property (Michaël Zasso) [#1852](https://github.com/nodejs/node/pull/1852)

## 2015-05-31, Version 2.2.0, @rvagg

### Notable changes

* **node**: Speed-up `require()` by replacing usage of `fs.statSync()` and `fs.readFileSync()` with internal variants that are faster for this use-case and do not create as many objects for the garbage collector to clean up. The primary two benefits are: significant increase in application start-up time on typical applications and better start-up time for the debugger by eliminating almost all of the thousands of exception events. (Ben Noordhuis) [#1801](https://github.com/nodejs/node/pull/1801).
* **node**: Resolution of pre-load modules (`-r` or `--require`) now follows the standard `require()` rules rather than just resolving paths, so you can now pre-load modules in node_modules. (Ali Ijaz Sheikh) [#1812](https://github.com/nodejs/node/pull/1812).
* **npm**: Upgraded npm to v2.11.0. New hooks for `preversion`, `version`, and `postversion` lifecycle events, some SPDX-related license changes and license file inclusions. See the [release notes](https://github.com/npm/npm/releases/tag/v2.11.0) for full details.

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`a77c330c32`](https://github.com/nodejs/node/commit/a77c330c32)] - **(SEMVER-MINOR)** **child_process**: expose ChildProcess constructor (Evan Lucas) [#1760](https://github.com/nodejs/node/pull/1760)
* [[`3a1bc067d4`](https://github.com/nodejs/node/commit/3a1bc067d4)] - ***Revert*** "**core**: set PROVIDER type as Persistent class id" (Ben Noordhuis) [#1827](https://github.com/nodejs/node/pull/1827)
* [[`f9fd554500`](https://github.com/nodejs/node/commit/f9fd554500)] - **deps**: make node-gyp work with io.js (cjihrig) [iojs/io.js#990](https://github.com/iojs/io.js/pull/990)
* [[`c1afa53648`](https://github.com/nodejs/node/commit/c1afa53648)] - **deps**: upgrade npm to 2.11.0 (Forrest L Norvell) [iojs/io.js#1829](https://github.com/iojs/io.js/pull/1829)
* [[`ff794498e7`](https://github.com/nodejs/node/commit/ff794498e7)] - **doc**: `fs.*File()` also accept encoding strings (Rich Trott) [#1806](https://github.com/nodejs/node/pull/1806)
* [[`98649fd31a`](https://github.com/nodejs/node/commit/98649fd31a)] - **doc**: add documentation for AtExit hook (Steve Sharp) [#1014](https://github.com/nodejs/node/pull/1014)
* [[`eb1856dfd1`](https://github.com/nodejs/node/commit/eb1856dfd1)] - **doc**: clarify stability of fs.watch and relatives (Rich Trott) [#1775](https://github.com/nodejs/node/pull/1775)
* [[`a74c2c9458`](https://github.com/nodejs/node/commit/a74c2c9458)] - **doc**: state url decoding behavior (Josh Gummersall) [#1731](https://github.com/nodejs/node/pull/1731)
* [[`ba76a9d872`](https://github.com/nodejs/node/commit/ba76a9d872)] - **doc**: remove bad semver-major entry from CHANGELOG (Rod Vagg) [#1782](https://github.com/nodejs/node/pull/1782)
* [[`a6a3f8c78d`](https://github.com/nodejs/node/commit/a6a3f8c78d)] - **doc**: fix changelog s/2.0.3/2.1.0 (Rod Vagg)
* [[`2c686fd3ce`](https://github.com/nodejs/node/commit/2c686fd3ce)] - **http**: flush stored header (Vladimir Kurchatkin) [#1695](https://github.com/nodejs/node/pull/1695)
* [[`1eec5f091a`](https://github.com/nodejs/node/commit/1eec5f091a)] - **http**: simplify code and remove unused properties (Brian White) [#1572](https://github.com/nodejs/node/pull/1572)
* [[`1bbf8d0720`](https://github.com/nodejs/node/commit/1bbf8d0720)] - **lib**: speed up require(), phase 2 (Ben Noordhuis) [#1801](https://github.com/nodejs/node/pull/1801)
* [[`b14fd1a720`](https://github.com/nodejs/node/commit/b14fd1a720)] - **lib**: speed up require(), phase 1 (Ben Noordhuis) [#1801](https://github.com/nodejs/node/pull/1801)
* [[`5abd4ac079`](https://github.com/nodejs/node/commit/5abd4ac079)] - **lib**: simplify nextTick() usage (Brian White) [#1612](https://github.com/nodejs/node/pull/1612)
* [[`5759722cfa`](https://github.com/nodejs/node/commit/5759722cfa)] - **(SEMVER-MINOR)** **src**: fix module search path for preload modules (Ali Ijaz Sheikh) [#1812](https://github.com/nodejs/node/pull/1812)
* [[`a65762cab6`](https://github.com/nodejs/node/commit/a65762cab6)] - **src**: remove old code (Brendan Ashworth) [#1819](https://github.com/nodejs/node/pull/1819)
* [[`93a44d5228`](https://github.com/nodejs/node/commit/93a44d5228)] - **src**: fix deferred events not working with -e (Ben Noordhuis) [#1793](https://github.com/nodejs/node/pull/1793)
* [[`8059393934`](https://github.com/nodejs/node/commit/8059393934)] - **test**: check error type from net.Server.listen() (Rich Trott) [#1821](https://github.com/nodejs/node/pull/1821)
* [[`4e90c82cdb`](https://github.com/nodejs/node/commit/4e90c82cdb)] - **test**: add heap profiler add-on regression test (Ben Noordhuis) [#1828](https://github.com/nodejs/node/pull/1828)
* [[`6dfca71af0`](https://github.com/nodejs/node/commit/6dfca71af0)] - **test**: don't lint autogenerated test/addons/doc-*/ (Ben Noordhuis) [#1793](https://github.com/nodejs/node/pull/1793)
* [[`c2b8b30836`](https://github.com/nodejs/node/commit/c2b8b30836)] - **test**: remove stray copyright notices (Ben Noordhuis) [#1793](https://github.com/nodejs/node/pull/1793)
* [[`280fb01daf`](https://github.com/nodejs/node/commit/280fb01daf)] - **test**: fix deprecation warning in addons test (Ben Noordhuis) [#1793](https://github.com/nodejs/node/pull/1793)
* [[`8606793999`](https://github.com/nodejs/node/commit/8606793999)] - **tools**: pass constant to logger instead of string (Johan Bergström) [#1842](https://github.com/nodejs/node/pull/1842)
* [[`fbd2b59716`](https://github.com/nodejs/node/commit/fbd2b59716)] - **tools**: add objectLiteralShorthandProperties to .eslintrc (Evan Lucas) [#1760](https://github.com/nodejs/node/pull/1760)
* [[`53e98cc1b4`](https://github.com/nodejs/node/commit/53e98cc1b4)] - **win,node-gyp**: enable delay-load hook by default (Bert Belder) [#1763](https://github.com/nodejs/node/pull/1763)

## 2015-05-24, Version 2.1.0, @rvagg

### Notable changes

* **crypto**: Diffie-Hellman key exchange (DHE) parameters (`'dhparams'`) must now be 1024 bits or longer or an error will be thrown. A warning will also be printed to the console if you supply less than 2048 bits. See https://weakdh.org/ for further context on this security concern. (Shigeki Ohtsu) [#1739](https://github.com/nodejs/node/pull/1739).
* **node**: A new `--trace-sync-io` command line flag will print a warning and a stack trace whenever a synchronous API is used. This can be used to track down synchronous calls that may be slowing down an application. (Trevor Norris) [#1707](https://github.com/nodejs/node/pull/1707).
* **node**: To allow for chaining of methods, the `setTimeout()`, `setKeepAlive()`, `setNoDelay()`, `ref()` and `unref()` methods used in `'net'`, `'dgram'`, `'http'`, `'https'` and `'tls'` now return the current instance instead of `undefined` (Roman Reiss & Evan Lucas) [#1699](https://github.com/nodejs/node/pull/1699) [#1768](https://github.com/nodejs/node/pull/1768) [#1779](https://github.com/nodejs/node/pull/1779).
* **npm**: Upgraded to v2.10.1, release notes can be found in <https://github.com/npm/npm/releases/tag/v2.10.1> and <https://github.com/npm/npm/releases/tag/v2.10.0>.
* **util**: A significant speed-up (in the order of 35%) for the common-case of a single string argument to `util.format()`, used by `console.log()` (Сковорода Никита Андреевич) [#1749](https://github.com/nodejs/node/pull/1749).

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`9da168b71f`](https://github.com/nodejs/node/commit/9da168b71f)] - **buffer**: optimize Buffer.byteLength (Brendan Ashworth) [#1713](https://github.com/nodejs/node/pull/1713)
* [[`2b1c01c2cc`](https://github.com/nodejs/node/commit/2b1c01c2cc)] - **build**: refactor pkg-config for shared libraries (Johan Bergström) [#1603](https://github.com/nodejs/node/pull/1603)
* [[`3c44100558`](https://github.com/nodejs/node/commit/3c44100558)] - **core**: set PROVIDER type as Persistent class id (Trevor Norris) [#1730](https://github.com/nodejs/node/pull/1730)
* [[`c1de6d249e`](https://github.com/nodejs/node/commit/c1de6d249e)] - **(SEMVER-MINOR)** **core**: implement runtime flag to trace sync io (Trevor Norris) [#1707](https://github.com/nodejs/node/pull/1707)
* [[`9e7099fa4e`](https://github.com/nodejs/node/commit/9e7099fa4e)] - **deps**: make node-gyp work with io.js (cjihrig) [iojs/io.js#990](https://github.com/iojs/io.js/pull/990)
* [[`c54d057598`](https://github.com/nodejs/node/commit/c54d057598)] - **deps**: upgrade to npm 2.10.1 (Rebecca Turner) [#1763](https://github.com/nodejs/node/pull/1763)
* [[`367ffd167d`](https://github.com/nodejs/node/commit/367ffd167d)] - **doc**: update AUTHORS list (Rod Vagg) [#1776](https://github.com/nodejs/node/pull/1776)
* [[`2bb2f06b3e`](https://github.com/nodejs/node/commit/2bb2f06b3e)] - **doc**: fix typo in CONTRIBUTING.md (Rich Trott) [#1755](https://github.com/nodejs/node/pull/1755)
* [[`515afc6367`](https://github.com/nodejs/node/commit/515afc6367)] - **doc**: path is ignored in url.format (Maurice Butler) [#1753](https://github.com/nodejs/node/pull/1753)
* [[`f0a8bc3f84`](https://github.com/nodejs/node/commit/f0a8bc3f84)] - **doc**: fix spelling in CHANGELOG (Felipe Batista)
* [[`86dd244d9b`](https://github.com/nodejs/node/commit/86dd244d9b)] - **doc**: add notes to child_process.fork() and .exec() (Rich Trott) [#1718](https://github.com/nodejs/node/pull/1718)
* [[`066274794c`](https://github.com/nodejs/node/commit/066274794c)] - **doc**: update links from iojs/io.js to nodejs/io.js (Frederic Hemberger) [#1715](https://github.com/nodejs/node/pull/1715)
* [[`cb381fe3e0`](https://github.com/nodejs/node/commit/cb381fe3e0)] - **(SEMVER-MINOR)** **net**: return this from setNoDelay and setKeepAlive (Roman Reiss) [#1779](https://github.com/nodejs/node/pull/1779)
* [[`85d9983009`](https://github.com/nodejs/node/commit/85d9983009)] - **net**: persist net.Socket options before connect (Evan Lucas) [#1518](https://github.com/nodejs/node/pull/1518)
* [[`39dde3222e`](https://github.com/nodejs/node/commit/39dde3222e)] - **(SEMVER-MINOR)** **net,dgram**: return this from ref and unref methods (Roman Reiss) [#1768](https://github.com/nodejs/node/pull/1768)
* [[`5773438913`](https://github.com/nodejs/node/commit/5773438913)] - **test**: fix jslint error (Michaël Zasso) [#1743](https://github.com/nodejs/node/pull/1743)
* [[`867631986f`](https://github.com/nodejs/node/commit/867631986f)] - **test**: fix test-sync-io-option (Santiago Gimeno) [#1734](https://github.com/nodejs/node/pull/1734)
* [[`f29762f4dd`](https://github.com/nodejs/node/commit/f29762f4dd)] - **test**: enable linting for tests (Roman Reiss) [#1721](https://github.com/nodejs/node/pull/1721)
* [[`2a71f02988`](https://github.com/nodejs/node/commit/2a71f02988)] - **tls**: emit errors happening before handshake finish (Malte-Thorben Bruns) [#1769](https://github.com/nodejs/node/pull/1769)
* [[`80342f649d`](https://github.com/nodejs/node/commit/80342f649d)] - **tls**: use `.destroy(err)` instead of destroy+emit (Fedor Indutny) [#1711](https://github.com/nodejs/node/pull/1711)
* [[`9b35be5810`](https://github.com/nodejs/node/commit/9b35be5810)] - **tls**: make server not use DHE in less than 1024bits (Shigeki Ohtsu) [#1739](https://github.com/nodejs/node/pull/1739)
* [[`214d02040e`](https://github.com/nodejs/node/commit/214d02040e)] - **util**: speed up common case of formatting string (Сковорода Никита Андреевич) [#1749](https://github.com/nodejs/node/pull/1749)
* [[`d144e96fbf`](https://github.com/nodejs/node/commit/d144e96fbf)] - **win,node-gyp**: enable delay-load hook by default (Bert Belder) [#1763](https://github.com/nodejs/node/pull/1763)
* [[`0d6d3dda95`](https://github.com/nodejs/node/commit/0d6d3dda95)] - **win,node-gyp**: make delay-load hook C89 compliant (Sharat M R) [TooTallNate/node-gyp#616](https://github.com/TooTallNate/node-gyp/pull/616)

## 2015-05-22, Version 0.12.4 (Stable)

### Commits

* [[`202c18bbc3`](https://github.com/nodejs/node/commit/202c18bbc3)] - **npm**: upgrade to 2.10.1 [#25364](https://github.com/joyent/node/pull/25364)
* [[`6157697bd5`](https://github.com/nodejs/node/commit/6157697bd5)] - **V8**: revert v8 Array.prototype.values() removal (cjihrig) [#25328](https://github.com/joyent/node/pull/25328)
* [[`3122052890`](https://github.com/nodejs/node/commit/3122052890)] - **win**: bring back xp/2k3 support (Bert Belder) [#25367](https://github.com/joyent/node/pull/25367)

## 2015-05-15, Version 2.0.2, @Fishrock123

### Notable changes

* **win,node-gyp**: the delay-load hook for windows addons has now been correctly enabled by default, it had wrongly defaulted to off in the release version of 2.0.0 (Bert Belder) [#1433](https://github.com/nodejs/node/pull/1433)
* **os**: `tmpdir()`'s trailing slash stripping has been refined to fix an issue when the temp directory is at '/'. Also considers which slash is used by the operating system. (cjihrig) [#1673](https://github.com/nodejs/node/pull/1673)
* **tls**: default ciphers have been updated to use gcm and aes128 (Mike MacCana) [#1660](https://github.com/nodejs/node/pull/1660)
* **build**: v8 snapshots have been re-enabled by default as suggested by the v8 team, since prior security issues have been resolved. This should give some perf improvements to both startup and vm context creation. (Trevor Norris) [#1663](https://github.com/nodejs/node/pull/1663)
* **src**: fixed preload modules not working when other flags were used before `--require` (Yosuke Furukawa) [#1694](https://github.com/nodejs/node/pull/1694)
* **dgram**: fixed `send()`'s callback not being asynchronous (Yosuke Furukawa) [#1313](https://github.com/nodejs/node/pull/1313)
* **readline**: emitKeys now keeps buffering data until it has enough to parse. This fixes an issue with parsing split escapes. (Alex Kocharin) [#1601](https://github.com/nodejs/node/pull/1601)
* **cluster**: works now properly emit 'disconnect' to `cluser.worker` (Oleg Elifantiev) [#1386](https://github.com/nodejs/node/pull/1386)
* **events**: uncaught errors now provide some context (Evan Lucas) [#1654](https://github.com/nodejs/node/pull/1654)

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`8a0e5295b4`](https://github.com/nodejs/node/commit/8a0e5295b4)] - **build**: use backslashes for paths on windows (Johan Bergström) [#1698](https://github.com/nodejs/node/pull/1698)
* [[`20c9a52227`](https://github.com/nodejs/node/commit/20c9a52227)] - **build**: move --with-intl to intl optgroup (Johan Bergström) [#1680](https://github.com/nodejs/node/pull/1680)
* [[`36cdc7c8ac`](https://github.com/nodejs/node/commit/36cdc7c8ac)] - **build**: re-enable V8 snapshots (Trevor Norris) [#1663](https://github.com/nodejs/node/pull/1663)
* [[`5883a59b21`](https://github.com/nodejs/node/commit/5883a59b21)] - **cluster**: disconnect event not emitted correctly (Oleg Elifantiev) [#1386](https://github.com/nodejs/node/pull/1386)
* [[`0f850f7ae7`](https://github.com/nodejs/node/commit/0f850f7ae7)] - **deps**: provide TXT chunk info in c-ares (Fedor Indutny)
* [[`7e1c0e75ed`](https://github.com/nodejs/node/commit/7e1c0e75ed)] - **deps**: sync with upstream bagder/c-ares@bba4dc5 (Ben Noordhuis) [#1678](https://github.com/nodejs/node/pull/1678)
* [[`18d457bd34`](https://github.com/nodejs/node/commit/18d457bd34)] - **dgram**: call send callback asynchronously (Yosuke Furukawa) [#1313](https://github.com/nodejs/node/pull/1313)
* [[`8b9a1537ad`](https://github.com/nodejs/node/commit/8b9a1537ad)] - **events**: provide better error message for unhandled error (Evan Lucas) [#1654](https://github.com/nodejs/node/pull/1654)
* [[`19ffb5cf1c`](https://github.com/nodejs/node/commit/19ffb5cf1c)] - **lib**: fix eslint styles (Yosuke Furukawa) [#1539](https://github.com/nodejs/node/pull/1539)
* [[`76937051f8`](https://github.com/nodejs/node/commit/76937051f8)] - **os**: refine tmpdir() trailing slash stripping (cjihrig) [#1673](https://github.com/nodejs/node/pull/1673)
* [[`aed6bce906`](https://github.com/nodejs/node/commit/aed6bce906)] - **readline**: turn emitKeys into a streaming parser (Alex Kocharin) [#1601](https://github.com/nodejs/node/pull/1601)
* [[`0a461e5360`](https://github.com/nodejs/node/commit/0a461e5360)] - **src**: fix preload when used with prior flags (Yosuke Furukawa) [#1694](https://github.com/nodejs/node/pull/1694)
* [[`931a0d4634`](https://github.com/nodejs/node/commit/931a0d4634)] - **src**: add type check to v8.setFlagsFromString() (Roman Klauke) [#1652](https://github.com/nodejs/node/pull/1652)
* [[`08d08668c9`](https://github.com/nodejs/node/commit/08d08668c9)] - **src,deps**: replace LoadLibrary by LoadLibraryW (Cheng Zhao) [#226](https://github.com/nodejs/node/pull/226)
* [[`4e2f999a62`](https://github.com/nodejs/node/commit/4e2f999a62)] - **test**: fix infinite loop detection (Yosuke Furukawa) [#1681](https://github.com/nodejs/node/pull/1681)
* [[`5755fc099f`](https://github.com/nodejs/node/commit/5755fc099f)] - **tls**: update default ciphers to use gcm and aes128 (Mike MacCana) [#1660](https://github.com/nodejs/node/pull/1660)
* [[`966acb9916`](https://github.com/nodejs/node/commit/966acb9916)] - **tools**: remove closure_linter to eslint on windows (Yosuke Furukawa) [#1685](https://github.com/nodejs/node/pull/1685)
* [[`c58264e58b`](https://github.com/nodejs/node/commit/c58264e58b)] - **tools**: make eslint work on subdirectories (Roman Reiss) [#1686](https://github.com/nodejs/node/pull/1686)
* [[`0b21ab13b7`](https://github.com/nodejs/node/commit/0b21ab13b7)] - **tools**: refactor `make test-npm` into test-npm.sh (Jeremiah Senkpiel) [#1662](https://github.com/nodejs/node/pull/1662)
* [[`f07b3b600b`](https://github.com/nodejs/node/commit/f07b3b600b)] - **tools**: set eslint comma-spacing to 'warn' (Roman Reiss) [#1672](https://github.com/nodejs/node/pull/1672)
* [[`f9dd34d301`](https://github.com/nodejs/node/commit/f9dd34d301)] - **tools**: replace closure-linter with eslint (Yosuke Furukawa) [#1539](https://github.com/nodejs/node/pull/1539)
* [[`64d3210c98`](https://github.com/nodejs/node/commit/64d3210c98)] - **win,node-gyp**: enable delay-load hook by default (Bert Belder) [#1667](https://github.com/nodejs/node/issues/1667)

## 2015-05-13, Version 0.12.3 (Stable)

### Commits

* [[`32166a90cf`](https://github.com/nodejs/node/commit/32166a90cf)] - **V8**: update to 3.28.71.19 [#18206](https://github.com/joyent/node/pull/18206)
* [[`84f1ab6114`](https://github.com/nodejs/node/commit/84f1ab6114)] - **uv**: upgrade to 1.5.0 [#25141](https://github.com/joyent/node/pull/25141)
* [[`03cfbd65fb`](https://github.com/nodejs/node/commit/03cfbd65fb)] - **npm**: upgrade to 2.9.1 [#25289](https://github.com/joyent/node/pull/25289)
* [[`80cdae855f`](https://github.com/nodejs/node/commit/80cdae855f)] - **V8**: don't busy loop in v8 cpu profiler thread (Mike Tunnicliffe) [#25268](https://github.com/joyent/node/pull/25268)
* [[`2a5f4bd7ce`](https://github.com/nodejs/node/commit/2a5f4bd7ce)] - **V8**: fix issue with let bindings in for loops (adamk) [#23948](https://github.com/joyent/node/pull/23948)
* [[`f0ef597e09`](https://github.com/nodejs/node/commit/f0ef597e09)] - **debugger**: don't spawn child process in remote mode (Jackson Tian) [#14172](https://github.com/joyent/node/pull/14172)
* [[`0e392f3b68`](https://github.com/nodejs/node/commit/0e392f3b68)] - **net**: do not set V4MAPPED on FreeBSD (Julien Gilli) [#18204](https://github.com/joyent/node/pull/18204)
* [[`101e103e3b`](https://github.com/nodejs/node/commit/101e103e3b)] - **repl**: make 'Unexpected token' errors recoverable (Julien Gilli) [#8875](https://github.com/joyent/node/pull/8875)
* [[`d5b32246fb`](https://github.com/nodejs/node/commit/d5b32246fb)] - **src**: backport ignore ENOTCONN on shutdown race (Ben Noordhuis) [#14480](https://github.com/joyent/node/pull/14480)
* [[`f99eaefe75`](https://github.com/nodejs/node/commit/f99eaefe75)] - **src**: fix backport of SIGINT crash fix on FreeBSD (Julien Gilli) [#14819](https://github.com/joyent/node/pull/14819)

## 2015-05-07, Version 2.0.1, @rvagg

### Notable changes

* **async_wrap**: (Trevor Norris) [#1614](https://github.com/nodejs/node/pull/1614)
  - it is now possible to filter by providers
  - bit flags have been removed and replaced with method calls on the binding object
  - _note that this is an unstable API so feature additions and breaking changes won't change io.js semver_
* **libuv**: resolves numerous io.js issues:
  - [#862](https://github.com/nodejs/node/issues/862) prevent spawning child processes with invalid stdio file descriptors
  - [#1397](https://github.com/nodejs/node/issues/1397) fix EPERM error with fs.access(W_OK) on Windows
  - [#1621](https://github.com/nodejs/node/issues/1621) build errors associated with the bundled libuv
  - [#1512](https://github.com/nodejs/node/issues/1512) should properly fix Windows termination errors
* **addons**: the `NODE_DEPRECATED` macro was causing problems when compiling addons with older compilers, this should now be resolved (Ben Noordhuis) [#1626](https://github.com/nodejs/node/pull/1626)
* **V8**: upgrade V8 from 4.2.77.18 to 4.2.77.20 with minor fixes, including a bug preventing builds on FreeBSD

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).
* readline: split escapes are processed incorrectly, see [#1403](https://github.com/nodejs/node/issues/1403)

### Commits

* [[`7dde95a8bd`](https://github.com/nodejs/node/commit/7dde95a8bd)] - **async-wrap**: remove before/after calls in init (Trevor Norris) [#1614](https://github.com/nodejs/node/pull/1614)
* [[`bd42ba056a`](https://github.com/nodejs/node/commit/bd42ba056a)] - **async-wrap**: set flags using functions (Trevor Norris) [#1614](https://github.com/nodejs/node/pull/1614)
* [[`4b2c786449`](https://github.com/nodejs/node/commit/4b2c786449)] - **async-wrap**: pass PROVIDER as first arg to init (Trevor Norris) [#1614](https://github.com/nodejs/node/pull/1614)
* [[`84bf609fd2`](https://github.com/nodejs/node/commit/84bf609fd2)] - **async-wrap**: don't call init callback unnecessarily (Trevor Norris) [#1614](https://github.com/nodejs/node/pull/1614)
* [[`04cc03b029`](https://github.com/nodejs/node/commit/04cc03b029)] - **deps**: update libuv to 1.5.0 (Saúl Ibarra Corretgé) [#1646](https://github.com/nodejs/node/pull/1646)
* [[`b16d9c28e8`](https://github.com/nodejs/node/commit/b16d9c28e8)] - **deps**: upgrade v8 to 4.2.77.20 (Ben Noordhuis) [#1639](https://github.com/nodejs/node/pull/1639)
* [[`9ec3109272`](https://github.com/nodejs/node/commit/9ec3109272)] - **doc**: add TC meeting 2015-04-29 minutes (Rod Vagg) [#1585](https://github.com/nodejs/node/pull/1585)
* [[`2c7206254c`](https://github.com/nodejs/node/commit/2c7206254c)] - **doc**: fix typo in readme.md (AQNOUCH Mohammed) [#1643](https://github.com/nodejs/node/pull/1643)
* [[`71dc7152ee`](https://github.com/nodejs/node/commit/71dc7152ee)] - **doc**: fix PR link in CHANGELOG (Brian White) [#1624](https://github.com/nodejs/node/pull/1624)
* [[`b97b96d05a`](https://github.com/nodejs/node/commit/b97b96d05a)] - **install**: fix NameError (thefourtheye) [#1628](https://github.com/nodejs/node/pull/1628)
* [[`6ccbe75384`](https://github.com/nodejs/node/commit/6ccbe75384)] - **js_stream**: fix buffer index in DoWrite (Shigeki Ohtsu) [#1635](https://github.com/nodejs/node/pull/1635)
* [[`c43855c49c`](https://github.com/nodejs/node/commit/c43855c49c)] - **src**: export the ParseEncoding function on Windows (Ivan Kozik) [#1596](https://github.com/nodejs/node/pull/1596)
* [[`8315b22390`](https://github.com/nodejs/node/commit/8315b22390)] - **src**: fix pedantic cpplint whitespace warnings (Ben Noordhuis) [#1640](https://github.com/nodejs/node/pull/1640)
* [[`b712af79a7`](https://github.com/nodejs/node/commit/b712af79a7)] - **src**: fix NODE_DEPRECATED macro with old compilers (Ben Noordhuis) [#1626](https://github.com/nodejs/node/pull/1626)
* [[`2ed10f1349`](https://github.com/nodejs/node/commit/2ed10f1349)] - **src**: fix minor inefficiency in Buffer::New() call (Ben Noordhuis) [#1577](https://github.com/nodejs/node/pull/1577)
* [[`f696c9efab`](https://github.com/nodejs/node/commit/f696c9efab)] - **src**: fix deprecated use of Buffer::New() (Ben Noordhuis) [#1577](https://github.com/nodejs/node/pull/1577)
* [[`0c8f13df8f`](https://github.com/nodejs/node/commit/0c8f13df8f)] - **tools**: remove unused GuessWordSize function (thefourtheye) [#1638](https://github.com/nodejs/node/pull/1638)

## 2015-05-04, Version 2.0.0, @rvagg

### Breaking changes

Full details at https://github.com/nodejs/node/wiki/Breaking-Changes#200-from-1x

* V8 upgrade to 4.2, minor changes to C++ API
* `os.tmpdir()` is now cross-platform consistent and no longer returns a path with a trailing slash on any platform
* While not a *breaking change* the 'smalloc' module has been deprecated in anticipation of it becoming unsupportable with a future upgrade to V8 4.4. See [#1451](https://github.com/nodejs/node/issues/1451)  for further information.

_Note: a new version of the 'url' module was reverted prior to release as it was decided the potential for breakage across the npm ecosystem was too great and that more compatibility work needed to be done before releasing it. See [#1602](https://github.com/nodejs/node/pull/1602) for further information._

### Notable changes

* **crypto**: significantly reduced memory usage for TLS (Fedor Indutny & Сковорода Никита Андреевич) [#1529](https://github.com/nodejs/node/pull/1529)
* **net**: `socket.connect()` now accepts a `'lookup'` option for a custom DNS resolution mechanism, defaults to `dns.lookup()` (Evan Lucas) [#1505](https://github.com/nodejs/node/pull/1505)
* **npm**: Upgrade npm to 2.9.0. See the [v2.8.4](https://github.com/npm/npm/releases/tag/v2.8.4) and [v2.9.0](https://github.com/npm/npm/releases/tag/v2.9.0) release notes for details. Notable items:
  - Add support for default author field to make `npm init -y` work without user-input (@othiym23) [npm/npm/d8eee6cf9d](https://github.com/npm/npm/commit/d8eee6cf9d2ff7aca68dfaed2de76824a3e0d9af)
  - Include local modules in `npm outdated` and `npm update` (@ArnaudRinquin) [npm/npm#7426](https://github.com/npm/npm/issues/7426)
  - The prefix used before the version number on `npm version` is now configurable via `tag-version-prefix` (@kkragenbrink) [npm/npm#8014](https://github.com/npm/npm/issues/8014)
* **os**: `os.tmpdir()` is now cross-platform consistent and will no longer returns a path with a trailing slash on any platform (Christian Tellnes) [#747](https://github.com/nodejs/node/pull/747)
* **process**:
  - `process.nextTick()` performance has been improved by between 2-42% across the benchmark suite, notable because this is heavily used across core (Brian White) [#1571](https://github.com/nodejs/node/pull/1571)
  - New `process.geteuid()`, `process.seteuid(id)`, `process.getegid()` and `process.setegid(id)` methods allow you to get and set effective UID and GID of the process (Evan Lucas) [#1536](https://github.com/nodejs/node/pull/1536)
* **repl**:
  - REPL history can be persisted across sessions if the `NODE_REPL_HISTORY_FILE` environment variable is set to a user accessible file, `NODE_REPL_HISTORY_SIZE` can set the maximum history size and defaults to `1000` (Chris Dickinson) [#1513](https://github.com/nodejs/node/pull/1513)
  - The REPL can be placed in to one of three modes using the `NODE_REPL_MODE` environment variable: `sloppy`, `strict` or `magic` (default); the new `magic` mode will automatically run "strict mode only" statements in strict mode (Chris Dickinson) [#1513](https://github.com/nodejs/node/pull/1513)
* **smalloc**: the 'smalloc' module has been deprecated due to changes coming in V8 4.4 that will render it unusable
* **util**: add Promise, Map and Set inspection support (Christopher Monsanto) [#1471](https://github.com/nodejs/node/pull/1471)
* **V8**: upgrade to 4.2.77.18, see the [ChangeLog](https://chromium.googlesource.com/v8/v8/+/refs/heads/4.2.77/ChangeLog) for full details. Notable items:
  - Classes have moved out of staging; the `class` keyword is now usable in strict mode without flags
  - Object literal enhancements have moved out of staging; shorthand method and property syntax is now usable (`{ method() { }, property }`)
  - Rest parameters (`function(...args) {}`) are implemented in staging behind the `--harmony-rest-parameters` flag
  - Computed property names (`{['foo'+'bar']:'bam'}`) are implemented in staging behind the `--harmony-computed-property-names` flag
  - Unicode escapes (`'\u{xxxx}'`) are implemented in staging behind the `--harmony_unicode` flag and the `--harmony_unicode_regexps` flag for use in regular expressions
* **Windows**:
  - Random process termination on Windows fixed (Fedor Indutny)  [#1512](https://github.com/nodejs/node/issues/1512) / [#1563](https://github.com/nodejs/node/pull/1563)
  - The delay-load hook introduced to fix issues with process naming (iojs.exe / node.exe) has been made opt-out for native add-ons. Native add-ons should include `'win_delay_load_hook': 'false'` in their binding.gyp to disable this feature if they experience problems . (Bert Belder) [#1433](https://github.com/nodejs/node/pull/1433)
* **Governance**:
  - Rod Vagg (@rvagg) was added to the Technical Committee (TC)
  - Jeremiah Senkpiel (@Fishrock123) was added to the Technical Committee (TC)

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).
* readline: split escapes are processed incorrectly, see [#1403](https://github.com/nodejs/node/issues/1403)

### Commits

* [[`5404cbc745`](https://github.com/nodejs/node/commit/5404cbc745)] - **buffer**: fix copy() segfault with zero arguments (Trevor Norris) [#1520](https://github.com/nodejs/node/pull/1520)
* [[`3d3083b91f`](https://github.com/nodejs/node/commit/3d3083b91f)] - **buffer**: little improve for Buffer.concat method (Jackson Tian) [#1437](https://github.com/nodejs/node/pull/1437)
* [[`e67542ae17`](https://github.com/nodejs/node/commit/e67542ae17)] - **build**: disable -Og when building with clang (Ben Noordhuis) [#1609](https://github.com/nodejs/node/pull/1609)
* [[`78f4b038f8`](https://github.com/nodejs/node/commit/78f4b038f8)] - **build**: turn on debug-safe optimizations with -Og (Ben Noordhuis) [#1569](https://github.com/nodejs/node/pull/1569)
* [[`a5dcff827a`](https://github.com/nodejs/node/commit/a5dcff827a)] - **build**: Use option groups in configure output (Johan Bergström) [#1533](https://github.com/nodejs/node/pull/1533)
* [[`2a3c8c187e`](https://github.com/nodejs/node/commit/2a3c8c187e)] - **build**: remove -J from test-ci (Rod Vagg) [#1544](https://github.com/nodejs/node/pull/1544)
* [[`e6874dd0f9`](https://github.com/nodejs/node/commit/e6874dd0f9)] - **crypto**: track external memory for SSL structures (Fedor Indutny) [#1529](https://github.com/nodejs/node/pull/1529)
* [[`935c9d3fa7`](https://github.com/nodejs/node/commit/935c9d3fa7)] - **deps**: make node-gyp work with io.js (cjihrig) [#990](https://github.com/nodejs/node/pull/990)
* [[`56e4255382`](https://github.com/nodejs/node/commit/56e4255382)] - **deps**: upgrade npm to 2.9.0 (Forrest L Norvell) [#1573](https://github.com/nodejs/node/pull/1573)
* [[`509b59ea7c`](https://github.com/nodejs/node/commit/509b59ea7c)] - **deps**: enable v8 postmortem debugging again (Ben Noordhuis) [#1232](https://github.com/nodejs/node/pull/1232)
* [[`01652c7709`](https://github.com/nodejs/node/commit/01652c7709)] - **deps**: upgrade v8 to 4.2.77.18 (Chris Dickinson) [#1506](https://github.com/nodejs/node/pull/1506)
* [[`01e6632d70`](https://github.com/nodejs/node/commit/01e6632d70)] - **deps**: upgrade v8 to 4.2.77.15 (Ben Noordhuis) [#1399](https://github.com/nodejs/node/pull/1399)
* [[`db4ded5903`](https://github.com/nodejs/node/commit/db4ded5903)] - **deps**: enable v8 postmortem debugging again (Ben Noordhuis) [#1232](https://github.com/nodejs/node/pull/1232)
* [[`36cd5fb9d2`](https://github.com/nodejs/node/commit/36cd5fb9d2)] - **(SEMVER-MAJOR)** **deps**: upgrade v8 to 4.2.77.13 (Ben Noordhuis) [#1232](https://github.com/nodejs/node/pull/1232)
* [[`b3a7da1091`](https://github.com/nodejs/node/commit/b3a7da1091)] - **deps**: update http_parser to 2.5.0 (Fedor Indutny) [#1517](https://github.com/nodejs/node/pull/1517)
* [[`ac1fb39ce8`](https://github.com/nodejs/node/commit/ac1fb39ce8)] - **doc**: add rvagg to the TC (Rod Vagg) [#1613](https://github.com/nodejs/node/pull/1613)
* [[`dacc1fa35c`](https://github.com/nodejs/node/commit/dacc1fa35c)] - **doc**: update AUTHORS list (Rod Vagg) [#1586](https://github.com/nodejs/node/pull/1586)
* [[`2a3a1909ab`](https://github.com/nodejs/node/commit/2a3a1909ab)] - **doc**: add require() lines to child.stdio example (Nick Raienko) [#1504](https://github.com/nodejs/node/pull/1504)
* [[`02388dbf40`](https://github.com/nodejs/node/commit/02388dbf40)] - **doc**: fix some cross-references (Alexander Gromnitsky) [#1584](https://github.com/nodejs/node/pull/1584)
* [[`57c4cc26e2`](https://github.com/nodejs/node/commit/57c4cc26e2)] - **doc**: add TC meeting 2015-04-22 minutes (Rod Vagg) [#1556](https://github.com/nodejs/node/pull/1556)
* [[`b4ad5d7050`](https://github.com/nodejs/node/commit/b4ad5d7050)] - **doc**: improve http.request and https.request opts (Roman Reiss) [#1551](https://github.com/nodejs/node/pull/1551)
* [[`7dc8eec0a6`](https://github.com/nodejs/node/commit/7dc8eec0a6)] - **doc**: deprecate smalloc module (Ben Noordhuis) [#1566](https://github.com/nodejs/node/pull/1566)
* [[`1bcdf46ca7`](https://github.com/nodejs/node/commit/1bcdf46ca7)] - **doc**: add TC meeting 2015-04-15 minutes (Rod Vagg) [#1498](https://github.com/nodejs/node/pull/1498)
* [[`391cae3595`](https://github.com/nodejs/node/commit/391cae3595)] - **doc**: Add Known issues to v1.7.0/1.7.1 CHANGELOG (Yosuke Furukawa) [#1473](https://github.com/nodejs/node/pull/1473)
* [[`e55fdc47a7`](https://github.com/nodejs/node/commit/e55fdc47a7)] - **doc**: fix util.deprecate example (Nick Raienko) [#1535](https://github.com/nodejs/node/pull/1535)
* [[`5178f93bc0`](https://github.com/nodejs/node/commit/5178f93bc0)] - **doc**: Add Addon API (NAN) to working group list (Julian Duque) [#1523](https://github.com/nodejs/node/pull/1523)
* [[`f3cc50f811`](https://github.com/nodejs/node/commit/f3cc50f811)] - **doc**: add TC meeting 2015-04-08 minutes (Rod Vagg) [#1497](https://github.com/nodejs/node/pull/1497)
* [[`bb254b533b`](https://github.com/nodejs/node/commit/bb254b533b)] - **doc**: update branch to master (Roman Reiss) [#1511](https://github.com/nodejs/node/pull/1511)
* [[`22aafa5597`](https://github.com/nodejs/node/commit/22aafa5597)] - **doc**: add Fishrock123 to the TC (Jeremiah Senkpiel) [#1507](https://github.com/nodejs/node/pull/1507)
* [[`b16a328ede`](https://github.com/nodejs/node/commit/b16a328ede)] - **doc**: add spaces to child.kill example (Nick Raienko) [#1503](https://github.com/nodejs/node/pull/1503)
* [[`26327757f8`](https://github.com/nodejs/node/commit/26327757f8)] - **doc**: update AUTHORS list (Rod Vagg) [#1476](https://github.com/nodejs/node/pull/1476)
* [[`f9c681cf62`](https://github.com/nodejs/node/commit/f9c681cf62)] - **fs**: validate fd on fs.write (Julian Duque) [#1553](https://github.com/nodejs/node/pull/1553)
* [[`801b47acc5`](https://github.com/nodejs/node/commit/801b47acc5)] - **gitignore**: ignore xcode workspaces and projects (Roman Klauke) [#1562](https://github.com/nodejs/node/pull/1562)
* [[`d5ce47e433`](https://github.com/nodejs/node/commit/d5ce47e433)] - **(SEMVER-MINOR)** **lib**: deprecate the smalloc module (Ben Noordhuis) [#1564](https://github.com/nodejs/node/pull/1564)
* [[`7384ca83f9`](https://github.com/nodejs/node/commit/7384ca83f9)] - **module**: remove '' from Module.globalPaths (Chris Yip) [#1488](https://github.com/nodejs/node/pull/1488)
* [[`b4f5898395`](https://github.com/nodejs/node/commit/b4f5898395)] - **net**: ensure Write/ShutdownWrap references handle (Fedor Indutny) [#1590](https://github.com/nodejs/node/pull/1590)
* [[`4abe2fa1cf`](https://github.com/nodejs/node/commit/4abe2fa1cf)] - **(SEMVER-MINOR)** **net**: add lookup option to Socket.prototype.connect (Evan Lucas) [#1505](https://github.com/nodejs/node/pull/1505)
* [[`1bef717476`](https://github.com/nodejs/node/commit/1bef717476)] - **(SEMVER-MINOR)** **net**: cleanup connect logic (Evan Lucas) [#1505](https://github.com/nodejs/node/pull/1505)
* [[`c7782c0af8`](https://github.com/nodejs/node/commit/c7782c0af8)] - **node**: improve nextTick performance (Brian White) [#1571](https://github.com/nodejs/node/pull/1571)
* [[`b57cc51d8d`](https://github.com/nodejs/node/commit/b57cc51d8d)] - **(SEMVER-MAJOR)** **os**: remove trailing slash from os.tmpdir() (Christian Tellnes) [#747](https://github.com/nodejs/node/pull/747)
* [[`ca219b00d1`](https://github.com/nodejs/node/commit/ca219b00d1)] - **repl**: fix for a+ fd clearing the file on read (Chris Dickinson) [#1605](https://github.com/nodejs/node/pull/1605)
* [[`051d482b15`](https://github.com/nodejs/node/commit/051d482b15)] - **repl**: fix \_debugger by properly proxying repl (Chris Dickinson) [#1605](https://github.com/nodejs/node/pull/1605)
* [[`2e2fce0502`](https://github.com/nodejs/node/commit/2e2fce0502)] - **repl**: fix persistent history and env variable name (Roman Reiss) [#1593](https://github.com/nodejs/node/pull/1593)
* [[`ea5195ccaf`](https://github.com/nodejs/node/commit/ea5195ccaf)] - **repl**: do not save history for non-terminal repl (Fedor Indutny) [#1575](https://github.com/nodejs/node/pull/1575)
* [[`0450ce7db2`](https://github.com/nodejs/node/commit/0450ce7db2)] - **repl**: add mode detection, cli persistent history (Chris Dickinson) [#1513](https://github.com/nodejs/node/pull/1513)
* [[`af9fe3bbc7`](https://github.com/nodejs/node/commit/af9fe3bbc7)] - **(SEMVER-MAJOR)** **src**: bump NODE_MODULE_VERSION due to V8 API (Rod Vagg) [#1532](https://github.com/nodejs/node/pull/1532)
* [[`279f6116aa`](https://github.com/nodejs/node/commit/279f6116aa)] - **src**: fix -Wmissing-field-initializers warning (Ben Noordhuis) [#1606](https://github.com/nodejs/node/pull/1606)
* [[`73062521a4`](https://github.com/nodejs/node/commit/73062521a4)] - **src**: deprecate smalloc public functions (Ben Noordhuis) [#1565](https://github.com/nodejs/node/pull/1565)
* [[`ccb199af17`](https://github.com/nodejs/node/commit/ccb199af17)] - **src**: fix deprecation warnings (Ben Noordhuis) [#1565](https://github.com/nodejs/node/pull/1565)
* [[`609fa0de03`](https://github.com/nodejs/node/commit/609fa0de03)] - **src**: fix NODE_DEPRECATED macro (Ben Noordhuis) [#1565](https://github.com/nodejs/node/pull/1565)
* [[`3c92ca2b5c`](https://github.com/nodejs/node/commit/3c92ca2b5c)] - **(SEMVER-MINOR)** **src**: add ability to get/set effective uid/gid (Evan Lucas) [#1536](https://github.com/nodejs/node/pull/1536)
* [[`30b7349176`](https://github.com/nodejs/node/commit/30b7349176)] - **stream_base**: dispatch reqs in the stream impl (Fedor Indutny) [#1563](https://github.com/nodejs/node/pull/1563)
* [[`0fa6c4a6fc`](https://github.com/nodejs/node/commit/0fa6c4a6fc)] - **string_decoder**: don't cache Buffer.isEncoding (Brian White) [#1548](https://github.com/nodejs/node/pull/1548)
* [[`f9b226c1c1`](https://github.com/nodejs/node/commit/f9b226c1c1)] - **test**: extend timeouts for ARMv6 (Rod Vagg) [#1554](https://github.com/nodejs/node/pull/1554)
* [[`bfae8236b1`](https://github.com/nodejs/node/commit/bfae8236b1)] - **test**: fix test-net-dns-custom-lookup test assertion (Evan Lucas) [#1531](https://github.com/nodejs/node/pull/1531)
* [[`547213913b`](https://github.com/nodejs/node/commit/547213913b)] - **test**: adjust Makefile/test-ci, add to vcbuild.bat (Rod Vagg) [#1530](https://github.com/nodejs/node/pull/1530)
* [[`550c2638c0`](https://github.com/nodejs/node/commit/550c2638c0)] - **tls**: use `SSL_set_cert_cb` for async SNI/OCSP (Fedor Indutny) [#1464](https://github.com/nodejs/node/pull/1464)
* [[`1787416376`](https://github.com/nodejs/node/commit/1787416376)] - **tls**: destroy singleUse context immediately (Fedor Indutny) [#1529](https://github.com/nodejs/node/pull/1529)
* [[`2684c902c4`](https://github.com/nodejs/node/commit/2684c902c4)] - **tls**: zero SSL_CTX freelist for a singleUse socket (Fedor Indutny) [#1529](https://github.com/nodejs/node/pull/1529)
* [[`2d241b3b82`](https://github.com/nodejs/node/commit/2d241b3b82)] - **tls**: destroy SSL once it is out of use (Fedor Indutny) [#1529](https://github.com/nodejs/node/pull/1529)
* [[`f7620fb96d`](https://github.com/nodejs/node/commit/f7620fb96d)] - **tls_wrap**: Unlink TLSWrap and SecureContext objects (Сковорода Никита Андреевич) [#1580](https://github.com/nodejs/node/pull/1580)
* [[`a7d74633f2`](https://github.com/nodejs/node/commit/a7d74633f2)] - **tls_wrap**: use localhost if options.host is empty (Guilherme Souza) [#1493](https://github.com/nodejs/node/pull/1493)
* [[`702997c1f0`](https://github.com/nodejs/node/commit/702997c1f0)] - ***Revert*** "**url**: significantly improve the performance of the url module" (Rod Vagg) [#1602](https://github.com/nodejs/node/pull/1602)
* [[`0daed24883`](https://github.com/nodejs/node/commit/0daed24883)] - ***Revert*** "**url**: delete href cache on all setter code paths" (Rod Vagg) [#1602](https://github.com/nodejs/node/pull/1602)
* [[`0f39ef4ca1`](https://github.com/nodejs/node/commit/0f39ef4ca1)] - ***Revert*** "**url**: fix treatment of some values as non-empty" (Rod Vagg) [#1602](https://github.com/nodejs/node/pull/1602)
* [[`66877216bd`](https://github.com/nodejs/node/commit/66877216bd)] - **url**: fix treatment of some values as non-empty (Petka Antonov) [#1589](https://github.com/nodejs/node/pull/1589)
* [[`dbdd81a91b`](https://github.com/nodejs/node/commit/dbdd81a91b)] - **url**: delete href cache on all setter code paths (Petka Antonov) [#1589](https://github.com/nodejs/node/pull/1589)
* [[`3fd7fc429c`](https://github.com/nodejs/node/commit/3fd7fc429c)] - **url**: significantly improve the performance of the url module (Petka Antonov) [#1561](https://github.com/nodejs/node/pull/1561)
* [[`bf7ac08dd0`](https://github.com/nodejs/node/commit/bf7ac08dd0)] - **util**: add Map and Set inspection support (Christopher Monsanto) [#1471](https://github.com/nodejs/node/pull/1471)
* [[`30e83d2e84`](https://github.com/nodejs/node/commit/30e83d2e84)] - **win,node-gyp**: optionally allow node.exe/iojs.exe to be renamed (Bert Belder) [#1266](https://github.com/nodejs/node/pull/1266)
* [[`3bda6cbfa4`](https://github.com/nodejs/node/commit/3bda6cbfa4)] - **(SEMVER-MAJOR)** **win,node-gyp**: enable delay-load hook by default (Bert Belder) [#1433](https://github.com/nodejs/node/pull/1433)

## 2015-04-20, Version 1.8.1, @chrisdickinson

### Notable changes

* **NOTICE**: Skipped v1.8.0 due to problems with release tooling.
  See [#1436](https://github.com/nodejs/node/issues/1436) for details.
* **build**: Support for building io.js as a static library (Marat Abdullin) [#1341](https://github.com/nodejs/node/pull/1341)
* **deps**: Upgrade openssl to 1.0.2a (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
  * Users should see performance improvements when using the crypto API.
  See [here](https://github.com/nodejs/node/wiki/Crypto-Performance-Notes-for-OpenSSL-1.0.2a-on-iojs-v1.8.0)
  for details.
* **npm**: Upgrade npm to 2.8.3. See the [release notes](https://github.com/npm/npm/releases/tag/v2.8.3) for details. Includes improved git support. Summary:
  * [`387f889`](https://github.com/npm/npm/commit/387f889c0e8fb617d9cc9a42ed0a3ec49424ab5d)
  [#7961](https://github.com/npm/npm/issues/7961) Ensure that hosted git SSH
  URLs always have a valid protocol when stored in `resolved` fields in
  `npm-shrinkwrap.json`. ([@othiym23](https://github.com/othiym23))
  * [`394c2f5`](https://github.com/npm/npm/commit/394c2f5a1227232c0baf42fbba1402aafe0d6ffb)
  Switch the order in which hosted Git providers are checked to `git:`,
  `git+https:`, then `git+ssh:` (from `git:`, `git+ssh:`, then `git+https:`) in
  an effort to go from most to least likely to succeed, to make for less
  confusing error message. ([@othiym23](https://github.com/othiym23))
  * [`431c3bf`](https://github.com/npm/npm/commit/431c3bf6cdec50f9f0c735f478cb2f3f337d3313)
  [#7699](https://github.com/npm/npm/issues/7699) `npm-registry-client@6.3.2`:
  Don't send body with HTTP GET requests when logging in.
  ([@smikes](https://github.com/smikes))
  * [`15efe12`](https://github.com/npm/npm/commit/15efe124753257728a0ddc64074fa5a4b9c2eb30)
  [#7872](https://github.com/npm/npm/issues/7872) Use the new version of
  `hosted-git-info` to pass along credentials embedded in git URLs. Test it.
  Test it a lot. ([@othiym23](https://github.com/othiym23))
  * [`b027319`](https://github.com/npm/npm/commit/b0273190c71eba14395ddfdd1d9f7ba625297523)
  [#7920](https://github.com/npm/npm/issues/7920) Scoped packages with
  `peerDependencies` were installing the `peerDependencies` into the wrong
  directory. ([@ewie](https://github.com/ewie))
  * [`6b0f588`](https://github.com/npm/npm/commit/6b0f58877f37df9904490ffbaaad33862bd36dce)
  [#7867](https://github.com/npm/npm/issues/7867) Use git shorthand and git
  URLs as presented by user. Support new `hosted-git-info` shortcut syntax.
  Save shorthand in `package.json`. Try cloning via `git:`, `git+ssh:`, and
  `git+https:`, in that order, when supported by the underlying hosting
  provider. ([@othiym23](https://github.com/othiym23))
* **src**: Allow multiple arguments to be passed to process.nextTick (Trevor Norris) [#1077](https://github.com/nodejs/node/pull/1077)
* **module**: The interaction of `require('.')` with `NODE_PATH` has been restored and deprecated. This functionality
will be removed at a later point. (Roman Reiss) [#1363](https://github.com/nodejs/node/pull/1363)

### Known issues

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).
* readline: split escapes are processed incorrectly, see [#1403](https://github.com/nodejs/node/issues/1403)

### Commits

* [[`53ed89d927`](https://github.com/nodejs/node/commit/53ed89d927)] - ***Revert*** "**build**: use %PYTHON% instead of python" (Rod Vagg) [#1475](https://github.com/nodejs/node/pull/1475)
* [[`2b744b0ab7`](https://github.com/nodejs/node/commit/2b744b0ab7)] - **src**: revert NODE_MODULE_VERSION to 43 (Chris Dickinson) [#1460](https://github.com/nodejs/node/pull/1460)
* [[`431673ebd1`](https://github.com/nodejs/node/commit/431673ebd1)] - **buffer**: fast-case for empty string in byteLength (Jackson Tian) [#1441](https://github.com/nodejs/node/pull/1441)
* [[`1b22bad35f`](https://github.com/nodejs/node/commit/1b22bad35f)] - **build**: fix logic for shared library flags (Jeremiah Senkpiel) [#1454](https://github.com/nodejs/node/pull/1454)
* [[`91943a99d5`](https://github.com/nodejs/node/commit/91943a99d5)] - **build**: use %PYTHON% instead of python (Rod Vagg) [#1444](https://github.com/nodejs/node/pull/1444)
* [[`c7769d417b`](https://github.com/nodejs/node/commit/c7769d417b)] - **build**: Expose xz compression level (Johan Bergström) [#1428](https://github.com/nodejs/node/pull/1428)
* [[`a530b2baf1`](https://github.com/nodejs/node/commit/a530b2baf1)] - **build**: fix error message in configure (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`92dfb794f9`](https://github.com/nodejs/node/commit/92dfb794f9)] - **build**: enable ssl support on arm64 (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`7de0dcde83`](https://github.com/nodejs/node/commit/7de0dcde83)] - **deps**: make node-gyp work with io.js (cjihrig) [#990](https://github.com/nodejs/node/pull/990)
* [[`4870213f9e`](https://github.com/nodejs/node/commit/4870213f9e)] - **deps**: upgrade npm to 2.8.3 (Forrest L Norvell)
* [[`49bb7ded2c`](https://github.com/nodejs/node/commit/49bb7ded2c)] - **deps**: fix git case sensitivity issue in npm (Chris Dickinson) [#1456](https://github.com/nodejs/node/pull/1456)
* [[`4830b4bce8`](https://github.com/nodejs/node/commit/4830b4bce8)] - **deps**: add docs to upgrade openssl (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`11bec72c87`](https://github.com/nodejs/node/commit/11bec72c87)] - **deps**: update asm files for openssl-1.0.2a (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`53924d8ebe`](https://github.com/nodejs/node/commit/53924d8ebe)] - **deps**: update asm Makefile for openssl-1.0.2a (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`418e839456`](https://github.com/nodejs/node/commit/418e839456)] - **deps**: update openssl.gyp/gypi for openssl-1.0.2a (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`02f12ab666`](https://github.com/nodejs/node/commit/02f12ab666)] - **deps**: update opensslconf.h for 1.0.2a (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`eb7a23595f`](https://github.com/nodejs/node/commit/eb7a23595f)] - **deps**: add x32 and arm64 support for opensslconf.h (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`033a663127`](https://github.com/nodejs/node/commit/033a663127)] - **deps**: replace all headers in openssl (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`ae8831f240`](https://github.com/nodejs/node/commit/ae8831f240)] - **deps**: backport openssl patch of alt cert chains 1 (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`71316c46d9`](https://github.com/nodejs/node/commit/71316c46d9)] - **deps**: fix asm build error of openssl in x86_win32 (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`d293a4f096`](https://github.com/nodejs/node/commit/d293a4f096)] - **deps**: fix openssl assembly error on ia32 win32 (Fedor Indutny) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`e4872d7405`](https://github.com/nodejs/node/commit/e4872d7405)] - **deps**: upgrade openssl to 1.0.2a (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`a1c9ef3142`](https://github.com/nodejs/node/commit/a1c9ef3142)] - **deps, build**: add support older assembler (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`76f219c128`](https://github.com/nodejs/node/commit/76f219c128)] - **doc**: Document forced pushing with git (Johan Bergström) [#1420](https://github.com/nodejs/node/pull/1420)
* [[`12e51d56c1`](https://github.com/nodejs/node/commit/12e51d56c1)] - **doc**: add Addon API WG (Rod Vagg) [#1226](https://github.com/nodejs/node/pull/1226)
* [[`7956a13dad`](https://github.com/nodejs/node/commit/7956a13dad)] - **http**: logically respect maxSockets (fengmk2) [#1242](https://github.com/nodejs/node/pull/1242)
* [[`5b844e140b`](https://github.com/nodejs/node/commit/5b844e140b)] - **module**: fix style (Roman Reiss) [#1453](https://github.com/nodejs/node/pull/1453)
* [[`3ad82c335d`](https://github.com/nodejs/node/commit/3ad82c335d)] - **(SEMVER-MINOR)** **module**: handle NODE_PATH in require('.') (Roman Reiss) [#1363](https://github.com/nodejs/node/pull/1363)
* [[`cd60ff0328`](https://github.com/nodejs/node/commit/cd60ff0328)] - **net**: add fd into listen2 debug info (Jackson Tian) [#1442](https://github.com/nodejs/node/pull/1442)
* [[`10e31ba56c`](https://github.com/nodejs/node/commit/10e31ba56c)] - **(SEMVER-MINOR)** **node**: allow multiple arguments passed to nextTick (Trevor Norris) [#1077](https://github.com/nodejs/node/pull/1077)
* [[`116c54692a`](https://github.com/nodejs/node/commit/116c54692a)] - **openssl**: fix keypress requirement in apps on win32 (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`62f5f4cec9`](https://github.com/nodejs/node/commit/62f5f4cec9)] - **src**: remove duplicate byteLength from Buffer (Jackson Tian) [#1438](https://github.com/nodejs/node/pull/1438)
* [[`51d0808c90`](https://github.com/nodejs/node/commit/51d0808c90)] - **stream**: remove duplicated expression (Yazhong Liu) [#1444](https://github.com/nodejs/node/pull/1444)
* [[`deb9d23d7b`](https://github.com/nodejs/node/commit/deb9d23d7b)] - **test**: fix error message check for openssl-1.0.2a (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`ca8c9ec2c8`](https://github.com/nodejs/node/commit/ca8c9ec2c8)] - **win,node-gyp**: optionally allow node.exe/iojs.exe to be renamed (Bert Belder) [#1266](https://github.com/nodejs/node/pull/1266)

## 2015-04-14, Version 1.7.1, @rvagg

### Notable changes

* **build**: A syntax error in the Makefile for release builds caused 1.7.0 to be DOA and unreleased. (Rod Vagg) [#1421](https://github.com/nodejs/node/pull/1421).

### Known issues

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)
* readline: split escapes are processed incorrectly, see [#1403](https://github.com/nodejs/node/issues/1403)

### Commits

* [[`aee86a21f2`](https://github.com/nodejs/node/commit/aee86a21f2)] - **build**: fix RELEASE check (Rod Vagg) [#1421](https://github.com/nodejs/node/pull/1421)

## 2015-04-14, Version 1.7.0, @rvagg

### Notable changes

* **C++ API**: Fedor Indutny contributed a feature to V8 which has been backported to the V8 bundled in io.js. `SealHandleScope` allows a C++ add-on author to _seal_ a `HandleScope` to prevent further, unintended allocations within it. Currently only enabled for debug builds of io.js. This feature helped detect the leak in [#1075](https://github.com/nodejs/node/issues/1075) and is now activated on the root `HandleScope` in io.js. (Fedor Indutny) [#1395](https://github.com/nodejs/node/pull/1395).
* **ARM**: This release includes significant work to improve the state of ARM support for builds and tests. The io.js CI cluster's ARMv6, ARMv7 and ARMv8 build servers are now all (mostly) reporting passing builds and tests.
  * ARMv8 64-bit (AARCH64) is now properly supported, including a backported fix in libuv that was mistakenly detecting the existence of `epoll_wait()`. (Ben Noordhuis) [#1365](https://github.com/nodejs/node/pull/1365).
  * ARMv6: [#1376](https://github.com/nodejs/node/issues/1376) reported a problem with `Math.exp()` on ARMv6 (incl Raspberry Pi). The culprit is erroneous codegen for ARMv6 when using the "fast math" feature of V8. `--nofast_math` has been turned on for all ARMv6 variants by default to avoid this, fast math can be turned back on with `--fast_math`. (Ben Noordhuis) [#1398](https://github.com/nodejs/node/pull/1398).
  * Tests: timeouts have been tuned specifically for slower platforms, detected as ARMv6 and ARMv7. (Roman Reiss) [#1366](https://github.com/nodejs/node/pull/1366).
* **npm**: Upgrade npm to 2.7.6. See the [release notes](https://github.com/npm/npm/releases/tag/v2.7.6) for details. Summary:
  * [`b747593`](https://github.com/npm/npm/commit/b7475936f473f029e6a027ba1b16277523747d0b)[#7630](https://github.com/npm/npm/issues/7630) Don't automatically log all git failures as errors. `maybeGithub` needs to be able to fail without logging to support its fallback logic. ([@othiym23](https://github.com/othiym23))
  * [`78005eb`](https://github.com/npm/npm/commit/78005ebb6f4103c20f077669c3929b7ea46a4c0d)[#7743](https://github.com/npm/npm/issues/7743) Always quote arguments passed to `npm run-script`. This allows build systems and the like to safely escape glob patterns passed as arguments to `run-scripts` with `npm run-script <script> -- <arguments>`. This is a tricky change to test, and may be reverted or moved to `npm@3` if it turns out it breaks things for users. ([@mantoni](https://github.com/mantoni))
  * [`da015ee`](https://github.com/npm/npm/commit/da015eee45f6daf384598151d06a9b57ffce136e)[#7074](https://github.com/npm/npm/issues/7074) `read-package-json@1.3.3`: `read-package-json` no longer caches `package.json` files, which trades a very small performance loss for the elimination of a large class of really annoying race conditions. See [#7074](https://github.com/npm/npm/issues/7074) for the grisly details. ([@othiym23](https://github.com/othiym23))

### Known issues

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)
* readline: split escapes are processed incorrectly, see [#1403](https://github.com/nodejs/node/issues/1403)

### Commits

* [[`d2b62a4973`](https://github.com/nodejs/node/commit/d2b62a4973)] - **benchmark**: don't check wrk in non-http benchmark (Jackson Tian) [#1368](https://github.com/nodejs/node/pull/1368)
* [[`fd90b33b94`](https://github.com/nodejs/node/commit/fd90b33b94)] - **build**: validate options passed to configure (Johan Bergström) [#1335](https://github.com/nodejs/node/pull/1335)
* [[`04b02f5e34`](https://github.com/nodejs/node/commit/04b02f5e34)] - **build**: Remove deprecated flags (Johan Bergström) [#1407](https://github.com/nodejs/node/pull/1407)
* [[`39d395c966`](https://github.com/nodejs/node/commit/39d395c966)] - **build**: minor changes to fix rpm build (Dan Varga) [#1408](https://github.com/nodejs/node/pull/1408)
* [[`f9a2d31b32`](https://github.com/nodejs/node/commit/f9a2d31b32)] - **build**: Simplify fetching release version (Johan Bergström) [#1405](https://github.com/nodejs/node/pull/1405)
* [[`cd38a4af8f`](https://github.com/nodejs/node/commit/cd38a4af8f)] - **build**: support building io.js as a static library (Marat Abdullin) [#1341](https://github.com/nodejs/node/pull/1341)
* [[`d726a177ed`](https://github.com/nodejs/node/commit/d726a177ed)] - **build**: Remove building against a shared V8 (Johan Bergström) [#1331](https://github.com/nodejs/node/pull/1331)
* [[`a5244d3a39`](https://github.com/nodejs/node/commit/a5244d3a39)] - **(SEMVER-MINOR)** **deps**: backport 1f8555 from v8's upstream (Fedor Indutny) [#1395](https://github.com/nodejs/node/pull/1395)
* [[`09d4a286ea`](https://github.com/nodejs/node/commit/09d4a286ea)] - **deps**: make node-gyp work with io.js (cjihrig) [#990](https://github.com/nodejs/node/pull/990)
* [[`cc8376ae67`](https://github.com/nodejs/node/commit/cc8376ae67)] - **deps**: upgrade npm to 2.7.6 (Forrest L Norvell) [#1390](https://github.com/nodejs/node/pull/1390)
* [[`5b0e5755a0`](https://github.com/nodejs/node/commit/5b0e5755a0)] - **deps**: generate opensslconf.h for architectures (Shigeki Ohtsu) [#1377](https://github.com/nodejs/node/pull/1377)
* [[`7d14aa0222`](https://github.com/nodejs/node/commit/7d14aa0222)] - **deps**: add Makefile to generate opensslconf.h (Shigeki Ohtsu) [#1377](https://github.com/nodejs/node/pull/1377)
* [[`29a3301461`](https://github.com/nodejs/node/commit/29a3301461)] - **deps**: make opensslconf.h include each target arch (Shigeki Ohtsu) [#1377](https://github.com/nodejs/node/pull/1377)
* [[`93a1a07ef4`](https://github.com/nodejs/node/commit/93a1a07ef4)] - **doc**: remove keepAlive options from http.request (Jeremiah Senkpiel) [#1392](https://github.com/nodejs/node/pull/1392)
* [[`3ad6ea7c38`](https://github.com/nodejs/node/commit/3ad6ea7c38)] - **doc**: remove redundant parameter in `end` listener. (Alex Yursha) [#1387](https://github.com/nodejs/node/pull/1387)
* [[`2bc3532461`](https://github.com/nodejs/node/commit/2bc3532461)] - **doc**: document Console class (Jackson Tian) [#1388](https://github.com/nodejs/node/pull/1388)
* [[`69bc1382b7`](https://github.com/nodejs/node/commit/69bc1382b7)] - **doc**: properly indent http.Agent keepAlive options (Jeremiah Senkpiel) [#1384](https://github.com/nodejs/node/pull/1384)
* [[`b464d467a2`](https://github.com/nodejs/node/commit/b464d467a2)] - **doc**: update curl usage in COLLABORATOR_GUIDE (Roman Reiss) [#1382](https://github.com/nodejs/node/pull/1382)
* [[`61c0e7b70f`](https://github.com/nodejs/node/commit/61c0e7b70f)] - **doc**: update CONTRIBUTING links. (Andrew Crites) [#1380](https://github.com/nodejs/node/pull/1380)
* [[`8d467e521c`](https://github.com/nodejs/node/commit/8d467e521c)] - **doc**: add TC meeting 2015-03-18 minutes (Rod Vagg) [#1370](https://github.com/nodejs/node/pull/1370)
* [[`8ba9c4a7c2`](https://github.com/nodejs/node/commit/8ba9c4a7c2)] - **doc**: add TC meeting 2015-04-01 minutes (Rod Vagg) [#1371](https://github.com/nodejs/node/pull/1371)
* [[`48facf93ad`](https://github.com/nodejs/node/commit/48facf93ad)] - **doc**: update AUTHORS list (Rod Vagg) [#1372](https://github.com/nodejs/node/pull/1372)
* [[`1219e7466c`](https://github.com/nodejs/node/commit/1219e7466c)] - **lib**: reduce process.binding() calls (Brendan Ashworth) [#1367](https://github.com/nodejs/node/pull/1367)
* [[`264a8f3a1b`](https://github.com/nodejs/node/commit/264a8f3a1b)] - **linux**: fix epoll_pwait() fallback on arm64 (Ben Noordhuis) [#1365](https://github.com/nodejs/node/pull/1365)
* [[`f0bf6bb024`](https://github.com/nodejs/node/commit/f0bf6bb024)] - **readline**: fix calling constructor without new (Alex Kocharin) [#1385](https://github.com/nodejs/node/pull/1385)
* [[`ff74931107`](https://github.com/nodejs/node/commit/ff74931107)] - **smalloc**: do not track external memory (Fedor Indutny) [#1375](https://github.com/nodejs/node/pull/1375)
* [[`a07c69113a`](https://github.com/nodejs/node/commit/a07c69113a)] - **(SEMVER-MINOR)** **src**: use global SealHandleScope (Fedor Indutny) [#1395](https://github.com/nodejs/node/pull/1395)
* [[`a4d88475fa`](https://github.com/nodejs/node/commit/a4d88475fa)] - **src**: disable fast math only on armv6 (Ben Noordhuis) [#1398](https://github.com/nodejs/node/pull/1398)
* [[`e306c78f83`](https://github.com/nodejs/node/commit/e306c78f83)] - **src**: disable fast math on arm (Ben Noordhuis) [#1398](https://github.com/nodejs/node/pull/1398)
* [[`7049d7b474`](https://github.com/nodejs/node/commit/7049d7b474)] - **test**: increase timeouts on ARM (Roman Reiss) [#1366](https://github.com/nodejs/node/pull/1366)
* [[`3066f2c0c3`](https://github.com/nodejs/node/commit/3066f2c0c3)] - **test**: double test timeout on arm machines (Ben Noordhuis) [#1357](https://github.com/nodejs/node/pull/1357)
* [[`66db9241cb`](https://github.com/nodejs/node/commit/66db9241cb)] - **tools**: Remove unused files (Johan Bergström) [#1406](https://github.com/nodejs/node/pull/1406)
* [[`8bc8bd4bc2`](https://github.com/nodejs/node/commit/8bc8bd4bc2)] - **tools**: add to install deps/openssl/config/archs (Shigeki Ohtsu) [#1377](https://github.com/nodejs/node/pull/1377)
* [[`907aaf325a`](https://github.com/nodejs/node/commit/907aaf325a)] - **win,node-gyp**: optionally allow node.exe/iojs.exe to be renamed (Bert Belder) [#1266](https://github.com/nodejs/node/pull/1266)
* [[`372bf83818`](https://github.com/nodejs/node/commit/372bf83818)] - **zlib**: make constants keep readonly (Jackson Tian) [#1361](https://github.com/nodejs/node/pull/1361)

## 2015-04-06, Version 1.6.4, @Fishrock123

### Notable changes

* **npm**: upgrade npm to 2.7.5. See [npm CHANGELOG.md](https://github.com/npm/npm/blob/master/CHANGELOG.md#v275-2015-03-26) for details. Includes two important security fixes. Summary:
  * [`300834e`](https://github.com/npm/npm/commit/300834e91a4e2a95fb7fb59c309e7c3fc91d2312)
  `tar@2.0.0`: Normalize symbolic links that point to targets outside the
  extraction root. This prevents packages containing symbolic links from
  overwriting targets outside the expected paths for a package. Thanks to [Tim
  Cuthbertson](http://gfxmonk.net/) and the team at [Lift
  Security](https://liftsecurity.io/) for working with the npm team to identify
  this issue. ([@othiym23](https://github.com/othiym23))
  * [`0dc6875`](https://github.com/npm/npm/commit/0dc68757cffd5397c280bc71365d106523a5a052)
  `semver@4.3.2`: Package versions can be no more than 256 characters long.
  This prevents a situation in which parsing the version number can use
  exponentially more time and memory to parse, leading to a potential denial of
  service. Thanks to Adam Baldwin at Lift Security for bringing this to our
  attention.  ([@isaacs](https://github.com/isaacs))
  * [`eab6184`](https://github.com/npm/npm/commit/eab618425c51e3aa4416da28dcd8ca4ba63aec41)
  [#7766](https://github.com/npm/npm/issues/7766) One last tweak to ensure that
  GitHub shortcuts work with private repositories.
  ([@iarna](https://github.com/iarna))
  * [`a840a13`](https://github.com/npm/npm/commit/a840a13bbf0330157536381ea8e58d0bd93b4c05)
  [#7746](https://github.com/npm/npm/issues/7746) Only fix up git URL paths when
  there are paths to fix up. ([@othiym23](https://github.com/othiym23))
* **openssl**: preliminary work has been done for an upcoming upgrade to OpenSSL 1.0.2a [#1325](https://github.com/nodejs/node/pull/1325) (Shigeki Ohtsu). See [#589](https://github.com/nodejs/node/issues/589) for additional details.
* **timers**: a minor memory leak when timers are unreferenced was fixed, alongside some related timers issues [#1330](https://github.com/nodejs/node/pull/1330) (Fedor Indutny). This appears to have fixed the remaining leak reported in [#1075](https://github.com/nodejs/node/issues/1075).
* **android**: it is now possible to compile io.js for Android and related devices [#1307](https://github.com/nodejs/node/pull/1307) (Giovanny Andres Gongora Granada).

### Known issues

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* Not possible to build io.js as a static library [#686](https://github.com/nodejs/node/issues/686)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)

### Commits

* [[`3a69b7689b`](https://github.com/io.js/io.js/commit/3a69b7689b)] - **benchmark**: add rsa/aes-gcm performance test (Shigeki Ohtsu) [iojs/io.js#1325](https://github.com/nodejs/node/pull/1325)
* [[`1c709f3aa9`](https://github.com/io.js/io.js/commit/1c709f3aa9)] - **benchmark**: add/remove hash algorithm (Shigeki Ohtsu) [iojs/io.js#1325](https://github.com/nodejs/node/pull/1325)
* [[`a081c7c522`](https://github.com/io.js/io.js/commit/a081c7c522)] - **benchmark**: fix chunky client benchmark execution (Brian White) [iojs/io.js#1257](https://github.com/nodejs/node/pull/1257)
* [[`65d4d25f52`](https://github.com/io.js/io.js/commit/65d4d25f52)] - **build**: default to armv7+vfpv3 for android (Giovanny Andres Gongora Granada) [iojs/io.js#1307](https://github.com/nodejs/node/pull/1307)
* [[`6a134f7d70`](https://github.com/io.js/io.js/commit/6a134f7d70)] - **build**: avoid passing private flags from pmake (Johan Bergström) [iojs/io.js#1334](https://github.com/nodejs/node/pull/1334)
* [[`5094a0fde3`](https://github.com/io.js/io.js/commit/5094a0fde3)] - **build**: Pass BSDmakefile args to gmake (Johan Bergström) [iojs/io.js#1298](https://github.com/nodejs/node/pull/1298)
* [[`f782824d48`](https://github.com/io.js/io.js/commit/f782824d48)] - **deps**: refactor openssl.gyp (Shigeki Ohtsu) [iojs/io.js#1325](https://github.com/nodejs/node/pull/1325)
* [[`21f4fb6215`](https://github.com/io.js/io.js/commit/21f4fb6215)] - **deps**: update gyp to e1c8fcf7 (Shigeki Ohtsu) [iojs/io.js#1325](https://github.com/nodejs/node/pull/1325)
* [[`dac903f9b6`](https://github.com/io.js/io.js/commit/dac903f9b6)] - **deps**: make node-gyp work with io.js (cjihrig) [iojs/io.js#990](https://github.com/nodejs/node/pull/990)
* [[`5eb983e0b3`](https://github.com/io.js/io.js/commit/5eb983e0b3)] - **deps**: upgrade npm to 2.7.5 (Forrest L Norvell) [iojs/io.js#1337](https://github.com/nodejs/node/pull/1337)
* [[`008078862e`](https://github.com/io.js/io.js/commit/008078862e)] - **deps**: check in gtest, add util unit test (Ben Noordhuis) [iojs/io.js#1199](https://github.com/nodejs/node/pull/1199)
* [[`48d69cf1bb`](https://github.com/io.js/io.js/commit/48d69cf1bb)] - ***Revert*** "**doc**: fix typo in CHANGELOG.md" (Giovanny Andres Gongora Granada) [iojs/io.js#1349](https://github.com/nodejs/node/pull/1349)
* [[`679596c848`](https://github.com/io.js/io.js/commit/679596c848)] - **doc**: add Docker WG (Peter Petrov) [iojs/io.js#1134](https://github.com/nodejs/node/pull/1134)
* [[`d8578bad25`](https://github.com/io.js/io.js/commit/d8578bad25)] - **doc**: fix minor typos in COLLABORATOR_GUIDE.md (Kelsey) [iojs/io.js#1320](https://github.com/nodejs/node/pull/1320)
* [[`bde2b3e397`](https://github.com/io.js/io.js/commit/bde2b3e397)] - **doc**: fix typo in CHANGELOG.md (Giovanny Andres Gongora Granada) [iojs/io.js#1342](https://github.com/nodejs/node/pull/1342)
* [[`8c6c376a94`](https://github.com/io.js/io.js/commit/8c6c376a94)] - **doc**: add GPG fingerprint for Fishrock123 (Jeremiah Senkpiel) [iojs/io.js#1324](https://github.com/nodejs/node/pull/1324)
* [[`ccbea18960`](https://github.com/io.js/io.js/commit/ccbea18960)] - **doc**: better formatting for collaborator GPG keys (Jeremiah Senkpiel) [iojs/io.js#1324](https://github.com/nodejs/node/pull/1324)
* [[`87053e8aee`](https://github.com/io.js/io.js/commit/87053e8aee)] - **doc**: add back quote to boolean variable 'true' (Kohei TAKATA) [iojs/io.js#1338](https://github.com/nodejs/node/pull/1338)
* [[`634e9629a0`](https://github.com/io.js/io.js/commit/634e9629a0)] - **doc**: add TC meeting minutes 2015-03-04 (Rod Vagg) [iojs/io.js#1123](https://github.com/nodejs/node/pull/1123)
* [[`245ba1d658`](https://github.com/io.js/io.js/commit/245ba1d658)] - **doc**: fix util.isObject documentation (Jeremiah Senkpiel) [iojs/io.js#1295](https://github.com/nodejs/node/pull/1295)
* [[`ad937752ee`](https://github.com/io.js/io.js/commit/ad937752ee)] - **doc,src**: remove references to --max-stack-size (Aria Stewart) [iojs/io.js#1327](https://github.com/nodejs/node/pull/1327)
* [[`15f058f609`](https://github.com/io.js/io.js/commit/15f058f609)] - **gyp**: fix build with python 2.6 (Fedor Indutny) [iojs/io.js#1325](https://github.com/nodejs/node/pull/1325)
* [[`4dc6ae2181`](https://github.com/io.js/io.js/commit/4dc6ae2181)] - **lib**: remove unused variables (Brian White) [iojs/io.js#1290](https://github.com/nodejs/node/pull/1290)
* [[`b6e22c4bd5`](https://github.com/io.js/io.js/commit/b6e22c4bd5)] - **src**: setup cluster workers before preloading (Ali Ijaz Sheikh) [iojs/io.js#1314](https://github.com/nodejs/node/pull/1314)
* [[`4a801c211c`](https://github.com/io.js/io.js/commit/4a801c211c)] - **src**: drop homegrown thread pool, use libplatform (Ben Noordhuis) [iojs/io.js#1329](https://github.com/nodejs/node/pull/1329)
* [[`f1e5a13516`](https://github.com/io.js/io.js/commit/f1e5a13516)] - **src**: wrap MIN definition in infdef (Johan Bergström) [iojs/io.js#1322](https://github.com/nodejs/node/pull/1322)
* [[`6f72d87c27`](https://github.com/io.js/io.js/commit/6f72d87c27)] - **test**: add test for a unref'ed timer leak (Fedor Indutny) [iojs/io.js#1330](https://github.com/nodejs/node/pull/1330)
* [[`416499c872`](https://github.com/io.js/io.js/commit/416499c872)] - **timers**: remove redundant code (Fedor Indutny) [iojs/io.js#1330](https://github.com/nodejs/node/pull/1330)
* [[`d22b2a934a`](https://github.com/io.js/io.js/commit/d22b2a934a)] - **timers**: do not restart the interval after close (Fedor Indutny) [iojs/io.js#1330](https://github.com/nodejs/node/pull/1330)
* [[`cca5efb086`](https://github.com/io.js/io.js/commit/cca5efb086)] - **timers**: don't close interval timers when unrefd (Julien Gilli)
* [[`0e061975d7`](https://github.com/io.js/io.js/commit/0e061975d7)] - **timers**: fix unref() memory leak (Trevor Norris) [iojs/io.js#1330](https://github.com/nodejs/node/pull/1330)
* [[`ec7fbf2bb2`](https://github.com/io.js/io.js/commit/ec7fbf2bb2)] - **tools**: fix install source path for openssl headers (Oguz Bastemur) [iojs/io.js#1354](https://github.com/nodejs/node/pull/1354)
* [[`644ece1f67`](https://github.com/io.js/io.js/commit/644ece1f67)] - **tools**: remove gyp test directory (Shigeki Ohtsu) [iojs/io.js#1350](https://github.com/nodejs/node/pull/1350)
* [[`eb459c8151`](https://github.com/io.js/io.js/commit/eb459c8151)] - **tools**: fix gyp to work on MacOSX without XCode (Shigeki Ohtsu) [iojs/io.js#1325](https://github.com/nodejs/node/pull/1325)
* [[`1e94057c05`](https://github.com/io.js/io.js/commit/1e94057c05)] - **url**: fix resolving from non-file to file URLs. (Jeffrey Jagoda) [iojs/io.js#1277](https://github.com/nodejs/node/pull/1277)
* [[`382bd9d2e0`](https://github.com/io.js/io.js/commit/382bd9d2e0)] - **v8**: back-port openbsd/amd64 build fix (Ben Noordhuis) [iojs/io.js#1318](https://github.com/nodejs/node/pull/1318)
* [[`efadffe861`](https://github.com/io.js/io.js/commit/efadffe861)] - **win,node-gyp**: optionally allow node.exe/iojs.exe to be renamed (Bert Belder) [iojs/io.js#1266](https://github.com/nodejs/node/pull/1266)

## 2015-03-31, Version 1.6.3, @rvagg

### Notable changes

* **fs**: corruption can be caused by `fs.writeFileSync()` and append-mode `fs.writeFile()` and `fs.writeFileSync()` under certain circumstances, reported in [#1058](https://github.com/nodejs/node/issues/1058), fixed in [#1063](https://github.com/nodejs/node/pull/1063) (Olov Lassus).
* **iojs**: an "internal modules" API has been introduced to allow core code to share JavaScript modules internally only without having to expose them as a public API, this feature is for core-only [#848](https://github.com/nodejs/node/pull/848) (Vladimir Kurchatkin).
* **timers**: two minor problems with timers have been fixed:
  - `Timer#close()` is now properly idempotent [#1288](https://github.com/nodejs/node/issues/1288) (Petka Antonov).
  - `setTimeout()` will only run the callback once now after an `unref()` during the callback [#1231](https://github.com/nodejs/node/pull/1231) (Roman Reiss).
  - NOTE: there are still other unresolved concerns with the timers code, such as [#1152](https://github.com/nodejs/node/pull/1152).
* **Windows**: a "delay-load hook" has been added for compiled add-ons on Windows that should alleviate some of the problems that Windows users may be experiencing with add-ons in io.js [#1251](https://github.com/nodejs/node/pull/1251) (Bert Belder).
* **V8**: minor bug-fix upgrade for V8 to 4.1.0.27.
* **npm**: upgrade npm to 2.7.4. See [npm CHANGELOG.md](https://github.com/npm/npm/blob/master/CHANGELOG.md#v274-2015-03-20) for details. Summary:
  * [`1549106`](https://github.com/npm/npm/commit/1549106f518000633915686f5f1ccc6afcf77f8f) [#7641](https://github.com/npm/npm/issues/7641) Due to 448efd0, running `npm shrinkwrap --dev` caused production dependencies to no longer be included in `npm-shrinkwrap.json`. Whoopsie! ([@othiym23](https://github.com/othiym23))
  * [`fb0ac26`](https://github.com/npm/npm/commit/fb0ac26eecdd76f6eaa4a96a865b7c6f52ce5aa5) [#7579](https://github.com/npm/npm/issues/7579) Only block removing files and links when we're sure npm isn't responsible for them. This change is hard to summarize, because if things are working correctly you should never see it, but if you want more context, just [go read the commit message](https://github.com/npm/npm/commit/fb0ac26eecdd76f6eaa4a96a865b7c6f52ce5aa5), which lays it all out. ([@othiym23](https://github.com/othiym23))
  * [`051c473`](https://github.com/npm/npm/commit/051c4738486a826300f205b71590781ce7744f01) [#7552](https://github.com/npm/npm/issues/7552) `bundledDependencies` are now properly included in the installation context. This is another fantastically hard-to-summarize bug, and once again, I encourage you to [read the commit message](https://github.com/npm/npm/commit/051c4738486a826300f205b71590781ce7744f01) if you're curious about the details. The snappy takeaway is that this unbreaks many use cases for `ember-cli`. ([@othiym23](https://github.com/othiym23))
  * [`fe1bc38`](https://github.com/npm/npm/commit/fe1bc387a14475e373557de669e03d9d006d3173)[#7672](https://github.com/npm/npm/issues/7672) `npm-registry-client@3.1.2`: Fix client-side certificate handling by correcting property name. ([@atamon](https://github.com/atamon))
  * [`89ce829`](https://github.com/npm/npm/commit/89ce829a00b526d0518f5cd855c323bffe182af0)[#7630](https://github.com/npm/npm/issues/7630) `hosted-git-info@1.5.3`: Part 3 of ensuring that GitHub shorthand is handled consistently. ([@othiym23](https://github.com/othiym23))
  * [`63313eb`](https://github.com/npm/npm/commit/63313eb0c37891c355546fd1093010c8a0c3cd81)[#7630](https://github.com/npm/npm/issues/7630) `realize-package-specifier@2.2.0`: Part 2 of ensuring that GitHub shorthand is handled consistently. ([@othiym23](https://github.com/othiym23))
  * [`3ed41bf`](https://github.com/npm/npm/commit/3ed41bf64a1bb752bb3155c74dd6ffbbd28c89c9)[#7630](https://github.com/npm/npm/issues/7630) `npm-package-arg@3.1.1`: Part 1 of ensuring that GitHub shorthand is handled consistently. ([@othiym23](https://github.com/othiym23))

### Known issues

* Some problems exist with timers and `unref()` still to be resolved. See [#1152](https://github.com/nodejs/node/pull/1152).
* Possible small memory leak(s) may still exist but have yet to be properly identified, details at [#1075](https://github.com/nodejs/node/issues/1075).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* Not possible to build io.js as a static library [#686](https://github.com/nodejs/node/issues/686)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)

### Commits

* [[`7dd5e824be`](https://github.com/nodejs/node/commit/7dd5e824be)] - **assert**: simplify logic of testing buffer equality (Alex Yursha) [#1171](https://github.com/nodejs/node/pull/1171)
* [[`a2ea16838f`](https://github.com/nodejs/node/commit/a2ea16838f)] - **debugger**: don't spawn child process in remote mode (Jackson Tian) [#1282](https://github.com/nodejs/node/pull/1282)
* [[`2752da4b64`](https://github.com/nodejs/node/commit/2752da4b64)] - **deps**: make node-gyp work with io.js (cjihrig) [#990](https://github.com/nodejs/node/pull/990)
* [[`f166cdecf1`](https://github.com/nodejs/node/commit/f166cdecf1)] - **deps**: upgrade npm to 2.7.4 (Forrest L Norvell)
* [[`318d9d8fd7`](https://github.com/nodejs/node/commit/318d9d8fd7)] - **deps**: upgrade v8 to 4.1.0.27 (Ben Noordhuis) [#1289](https://github.com/nodejs/node/pull/1289)
* [[`269e46be37`](https://github.com/nodejs/node/commit/269e46be37)] - **deps**: make node-gyp work with io.js (cjihrig) [#990](https://github.com/nodejs/node/pull/990)
* [[`b542fb94a4`](https://github.com/nodejs/node/commit/b542fb94a4)] - **deps**: upgrade npm to 2.7.3 (Forrest L Norvell) [#1219](https://github.com/nodejs/node/pull/1219)
* [[`73de13511d`](https://github.com/nodejs/node/commit/73de13511d)] - **doc**: add WG links in WORKING_GROUPS.md & fix nits (Farrin Reid) [#1113](https://github.com/nodejs/node/pull/1113)
* [[`19641b17be`](https://github.com/nodejs/node/commit/19641b17be)] - **doc**: decouple sidebar scrolling (Roman Reiss) [#1274](https://github.com/nodejs/node/pull/1274)
* [[`dbccf8d3ed`](https://github.com/nodejs/node/commit/dbccf8d3ed)] - **doc**: fix spelling error in feature flags (Phillip Lamplugh) [#1286](https://github.com/nodejs/node/pull/1286)
* [[`5e609e9324`](https://github.com/nodejs/node/commit/5e609e9324)] - ***Revert*** "**doc**: clarify real name requirement" (Jeremiah Senkpiel) [#1276](https://github.com/nodejs/node/pull/1276)
* [[`45814216ee`](https://github.com/nodejs/node/commit/45814216ee)] - **doc**: fix format docs discrepancy (Brendan Ashworth) [#1255](https://github.com/nodejs/node/pull/1255)
* [[`4e9bf93e9c`](https://github.com/nodejs/node/commit/4e9bf93e9c)] - **doc**: clarify real name requirement (Roman Reiss) [#1250](https://github.com/nodejs/node/pull/1250)
* [[`e84dd5f651`](https://github.com/nodejs/node/commit/e84dd5f651)] - **doc**: document repl on-demand module loading (Roman Reiss) [#1249](https://github.com/nodejs/node/pull/1249)
* [[`c9207f7fc2`](https://github.com/nodejs/node/commit/c9207f7fc2)] - **fs**: fix corruption in writeFile and writeFileSync (Olov Lassus) [#1063](https://github.com/nodejs/node/pull/1063)
* [[`2db758c562`](https://github.com/nodejs/node/commit/2db758c562)] - **iojs**: introduce internal modules (Vladimir Kurchatkin) [#848](https://github.com/nodejs/node/pull/848)
* [[`36f017afaf`](https://github.com/nodejs/node/commit/36f017afaf)] - **js2c**: fix module id generation on windows (Ben Noordhuis) [#1281](https://github.com/nodejs/node/pull/1281)
* [[`1832743e18`](https://github.com/nodejs/node/commit/1832743e18)] - **lib**: add missing `new` for errors lib/*.js (Mayhem) [#1246](https://github.com/nodejs/node/pull/1246)
* [[`ea37ac04f4`](https://github.com/nodejs/node/commit/ea37ac04f4)] - **src**: ignore ENOTCONN on shutdown race with child (Ben Noordhuis) [#1214](https://github.com/nodejs/node/pull/1214)
* [[`f06b16f2e9`](https://github.com/nodejs/node/commit/f06b16f2e9)] - **src**: fix minor memleak in preload-modules (Ali Ijaz Sheikh) [#1265](https://github.com/nodejs/node/pull/1265)
* [[`2903410aa8`](https://github.com/nodejs/node/commit/2903410aa8)] - **src**: don't lazy-load timer globals (Ben Noordhuis) [#1280](https://github.com/nodejs/node/pull/1280)
* [[`2e5b87a147`](https://github.com/nodejs/node/commit/2e5b87a147)] - **src**: remove unnecessary environment lookups (Ben Noordhuis) [#1238](https://github.com/nodejs/node/pull/1238)
* [[`7e88a9322c`](https://github.com/nodejs/node/commit/7e88a9322c)] - **src**: make accessors immune to context confusion (Ben Noordhuis) [#1238](https://github.com/nodejs/node/pull/1238)
* [[`c8fa8ccdbc`](https://github.com/nodejs/node/commit/c8fa8ccdbc)] - **streams**: use strict on _stream_wrap (Brendan Ashworth) [#1279](https://github.com/nodejs/node/pull/1279)
* [[`8a945814dd`](https://github.com/nodejs/node/commit/8a945814dd)] - **string_decoder**: optimize write() (Brian White) [#1209](https://github.com/nodejs/node/pull/1209)
* [[`8d1c87ea0a`](https://github.com/nodejs/node/commit/8d1c87ea0a)] - **test**: fix race in parallel/test-vm-debug-context (Ben Noordhuis) [#1294](https://github.com/nodejs/node/pull/1294)
* [[`955c1508da`](https://github.com/nodejs/node/commit/955c1508da)] - **test**: reduce sequential/test-fs-watch flakiness (Roman Reiss) [#1275](https://github.com/nodejs/node/pull/1275)
* [[`77c2da10fd`](https://github.com/nodejs/node/commit/77c2da10fd)] - **timers**: make Timer.close idempotent (Petka Antonov) [#1288](https://github.com/nodejs/node/pull/1288)
* [[`776b73b243`](https://github.com/nodejs/node/commit/776b73b243)] - **timers**: cleanup interval handling (Jeremiah Senkpiel) [#1272](https://github.com/nodejs/node/pull/1272)
* [[`caf0b36de3`](https://github.com/nodejs/node/commit/caf0b36de3)] - **timers**: assure setTimeout callback only runs once (Roman Reiss) [#1231](https://github.com/nodejs/node/pull/1231)
* [[`2ccc8f3970`](https://github.com/nodejs/node/commit/2ccc8f3970)] - **tls_wrap**: fix this incredibly stupid leak (Fedor Indutny) [#1244](https://github.com/nodejs/node/pull/1244)
* [[`e74b5d278c`](https://github.com/nodejs/node/commit/e74b5d278c)] - **tls_wrap**: fix BIO leak on SSL error (Fedor Indutny) [#1244](https://github.com/nodejs/node/pull/1244)
* [[`ba93c583bc`](https://github.com/nodejs/node/commit/ba93c583bc)] - **win,node-gyp**: optionally allow node.exe/iojs.exe to be renamed (Bert Belder) [#1266](https://github.com/nodejs/node/pull/1266)
* [[`08acf1352c`](https://github.com/nodejs/node/commit/08acf1352c)] - **win,node-gyp**: make delay-load hook optional (Bert Belder) [#1266](https://github.com/nodejs/node/pull/1266)
* [[`3d46fefe0c`](https://github.com/nodejs/node/commit/3d46fefe0c)] - **win,node-gyp**: allow node.exe/iojs.exe to be renamed (Bert Belder) [#1251](https://github.com/nodejs/node/pull/1251)

## 2015-03-31, Version 0.12.2 (Stable)

### Commits

* [[`7a37910f25`](https://github.com/nodejs/node/commit/7a37910f25)] - **uv**: Upgrade to 1.4.2 [#9179](https://github.com/joyent/node/pull/9179)
* [[`2704c62933`](https://github.com/nodejs/node/commit/2704c62933)] - **npm**: Upgrade to 2.7.4 [#14180](https://github.com/joyent/node/pull/14180)
* [[`a103712a62`](https://github.com/nodejs/node/commit/a103712a62)] - **V8**: do not add extra newline in log file (Julien Gilli)
* [[`2fc5eeb3da`](https://github.com/nodejs/node/commit/2fc5eeb3da)] - **V8**: Fix --max_old_space_size=4096 integer overflow (Andrei Sedoi) [#9200](https://github.com/joyent/node/pull/9200)
* [[`605329d7f7`](https://github.com/nodejs/node/commit/605329d7f7)] - **asyncwrap**: fix constructor condition for early ret (Trevor Norris) [#9146](https://github.com/joyent/node/pull/9146)
* [[`a33f23cbbc`](https://github.com/nodejs/node/commit/a33f23cbbc)] - **buffer**: align chunks on 8-byte boundary (Fedor Indutny) [#9375](https://github.com/joyent/node/pull/9375)
* [[`a35ba2f67d`](https://github.com/nodejs/node/commit/a35ba2f67d)] - **buffer**: fix pool offset adjustment (Trevor Norris)
* [[`c0766eb1a4`](https://github.com/nodejs/node/commit/c0766eb1a4)] - **build**: fix use of strict aliasing (Trevor Norris) [#9179](https://github.com/joyent/node/pull/9179)
* [[`6c3647c38d`](https://github.com/nodejs/node/commit/6c3647c38d)] - **console**: allow Object.prototype fields as labels (Colin Ihrig) [#9116](https://github.com/joyent/node/pull/9116)
* [[`4823afcbe2`](https://github.com/nodejs/node/commit/4823afcbe2)] - **fs**: make F_OK/R_OK/W_OK/X_OK not writable (Jackson Tian) [#9060](https://github.com/joyent/node/pull/9060)
* [[`b3aa876f08`](https://github.com/nodejs/node/commit/b3aa876f08)] - **fs**: properly handle fd passed to truncate() (Bruno Jouhier) [#9161](https://github.com/joyent/node/pull/9161)
* [[`d6484f3f7b`](https://github.com/nodejs/node/commit/d6484f3f7b)] - **http**: fix assert on data/end after socket error (Fedor Indutny) [#14087](https://github.com/joyent/node/pull/14087)
* [[`04b63e022a`](https://github.com/nodejs/node/commit/04b63e022a)] - **lib**: fix max size check in Buffer constructor (Ben Noordhuis) [#657](https://github.com/iojs/io.js/pull/657)
* [[`2411bea0df`](https://github.com/nodejs/node/commit/2411bea0df)] - **lib**: fix stdio/ipc sync i/o regression (Ben Noordhuis) [#9179](https://github.com/joyent/node/pull/9179)
* [[`b8604fa480`](https://github.com/nodejs/node/commit/b8604fa480)] - **module**: replace NativeModule.require (Herbert Vojčík) [#9201](https://github.com/joyent/node/pull/9201)
* [[`1a2a4dac23`](https://github.com/nodejs/node/commit/1a2a4dac23)] - **net**: allow port 0 in connect() (cjihrig) [#9268](https://github.com/joyent/node/pull/9268)
* [[`bada87bd66`](https://github.com/nodejs/node/commit/bada87bd66)] - **net**: unref timer in parent sockets (Fedor Indutny) [#891](https://github.com/iojs/io.js/pull/891)
* [[`c66f8c21f0`](https://github.com/nodejs/node/commit/c66f8c21f0)] - **path**: refactor for performance and consistency (Nathan Woltman) [#9289](https://github.com/joyent/node/pull/9289)
* [[`9deade4322`](https://github.com/nodejs/node/commit/9deade4322)] - **smalloc**: extend user API (Trevor Norris) [#905](https://github.com/iojs/io.js/pull/905)
* [[`61fe1fe21b`](https://github.com/nodejs/node/commit/61fe1fe21b)] - **src**: fix for SIGINT crash on FreeBSD (Fedor Indutny) [#14184](https://github.com/joyent/node/pull/14184)
* [[`b233131901`](https://github.com/nodejs/node/commit/b233131901)] - **src**: fix builtin modules failing with --use-strict (Julien Gilli) [#9237](https://github.com/joyent/node/pull/9237)
* [[`7e9d2f8de8`](https://github.com/nodejs/node/commit/7e9d2f8de8)] - **watchdog**: fix timeout for early polling return (Saúl Ibarra Corretgé) [#9410](https://github.com/joyent/node/pull/9410)

## 2015-03-23, Version 1.6.2, @rvagg

### Notable changes

* **Windows**: The ongoing work in improving the state of Windows support has resulted in full test suite passes once again. As noted in the release notes for v1.4.2, CI system and configuration problems prevented it from properly reporting problems with the Windows tests, the problems with the CI and the codebase appear to have been fully resolved.
* **FreeBSD**: A [kernel bug](https://lists.freebsd.org/pipermail/freebsd-current/2015-March/055043.html) impacting io.js/Node.js was [discovered](https://github.com/joyent/node/issues/9326) and a patch has been introduced to prevent it causing problems for io.js (Fedor Indutny) [#1218](https://github.com/nodejs/node/pull/1218).
* **module**: you can now `require('.')` instead of having to `require('./')`, this is considered a bugfix (Michaël Zasso) [#1185](https://github.com/nodejs/node/pull/1185).
* **v8**: updated to 4.1.0.25 including patches for `--max_old_space_size` values above `4096` and Solaris support, both of which are already included in io.js.

### Known issues

* Possible small memory leak(s) may still exist but have yet to be properly identified, details at [#1075](https://github.com/nodejs/node/issues/1075).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* Not possible to build io.js as a static library [#686](https://github.com/nodejs/node/issues/686)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)

### Commits

* [[`fe4434b77a`](https://github.com/nodejs/node/commit/fe4434b77a)] - **deps**: upgrade v8 to 4.1.0.25 (Johan Bergström) [#1224](https://github.com/nodejs/node/pull/1224)
* [[`d8f383ba3f`](https://github.com/nodejs/node/commit/d8f383ba3f)] - **doc**: update AUTHORS list (Rod Vagg) [#1234](https://github.com/nodejs/node/pull/1234)
* [[`bc9c1a5a7b`](https://github.com/nodejs/node/commit/bc9c1a5a7b)] - **doc**: fix typo in CHANGELOG (Mathieu Darse) [#1230](https://github.com/nodejs/node/pull/1230)
* [[`99c79f8d41`](https://github.com/nodejs/node/commit/99c79f8d41)] - **doc**: call js function in null context (Ben Noordhuis) [#1125](https://github.com/nodejs/node/pull/1125)
* [[`55abf34be5`](https://github.com/nodejs/node/commit/55abf34be5)] - **doc**: don't use `using namespace v8` (Ben Noordhuis) [#1125](https://github.com/nodejs/node/pull/1125)
* [[`c4e1b82120`](https://github.com/nodejs/node/commit/c4e1b82120)] - **doc**: replace v8::Handle<T> with v8::Local<T> (Ben Noordhuis) [#1125](https://github.com/nodejs/node/pull/1125)
* [[`2f1b78347c`](https://github.com/nodejs/node/commit/2f1b78347c)] - **doc**: remove unnecessary v8::HandleScopes (Ben Noordhuis) [#1125](https://github.com/nodejs/node/pull/1125)
* [[`409d413363`](https://github.com/nodejs/node/commit/409d413363)] - **doc**: remove uses of v8::Isolate::GetCurrent() (Ben Noordhuis) [#1125](https://github.com/nodejs/node/pull/1125)
* [[`33fea6ed5f`](https://github.com/nodejs/node/commit/33fea6ed5f)] - **lib**: don't penalize setInterval() common case (Ben Noordhuis) [#1221](https://github.com/nodejs/node/pull/1221)
* [[`31da9758a0`](https://github.com/nodejs/node/commit/31da9758a0)] - **lib**: don't penalize setTimeout() common case (Ben Noordhuis) [#1221](https://github.com/nodejs/node/pull/1221)
* [[`6fc5e95354`](https://github.com/nodejs/node/commit/6fc5e95354)] - **module**: allow require('.') (Michaël Zasso) [#1185](https://github.com/nodejs/node/pull/1185)
* [[`9ae1a61214`](https://github.com/nodejs/node/commit/9ae1a61214)] - **node**: ensure that streams2 won't `.end()` stdin (Fedor Indutny) [#1233](https://github.com/nodejs/node/pull/1233)
* [[`b64983d77c`](https://github.com/nodejs/node/commit/b64983d77c)] - **src**: reset signal handler to SIG_DFL on FreeBSD (Fedor Indutny) [#1218](https://github.com/nodejs/node/pull/1218)
* [[`9705a34e96`](https://github.com/nodejs/node/commit/9705a34e96)] - **test**: move sequential/test-signal-unregister (Ben Noordhuis) [#1227](https://github.com/nodejs/node/pull/1227)
* [[`10a9c00563`](https://github.com/nodejs/node/commit/10a9c00563)] - **test**: fix timing issue in signal test (Ben Noordhuis) [#1227](https://github.com/nodejs/node/pull/1227)
* [[`999fbe9d96`](https://github.com/nodejs/node/commit/999fbe9d96)] - **test**: fix crypto-binary-default bad crypto check (Brendan Ashworth) [#1141](https://github.com/nodejs/node/pull/1141)
* [[`2b3b2d392f`](https://github.com/nodejs/node/commit/2b3b2d392f)] - **test**: add setTimeout/setInterval multi-arg tests (Ben Noordhuis) [#1221](https://github.com/nodejs/node/pull/1221)
* [[`849319a260`](https://github.com/nodejs/node/commit/849319a260)] - **util**: Check input to util.inherits (Connor Peet) [#1240](https://github.com/nodejs/node/pull/1240)
* [[`cf081a4712`](https://github.com/nodejs/node/commit/cf081a4712)] - **vm**: fix crash on fatal error in debug context (Ben Noordhuis) [#1229](https://github.com/nodejs/node/pull/1229)

## 2015-03-23, Version 0.12.1 (Stable)

### Commits

* [[`3b511a8ccd`](https://github.com/nodejs/node/commit/3b511a8ccd)] - **openssl**: upgrade to 1.0.1m (Addressing multiple CVES)

## 2015-03-23, Version 0.10.38 (Maintenance)

### Commits

* [[`3b511a8ccd`](https://github.com/nodejs/node/commit/3b511a8ccd)] - **openssl**: upgrade to 1.0.1m (Addressing multiple CVES)

## 2015-03-20, Version 1.6.1, @rvagg

### Notable changes

* **path**: New type-checking on `path.resolve()` [#1153](https://github.com/nodejs/node/pull/1153) uncovered some edge-cases being relied upon in the wild, most notably `path.dirname(undefined)`. Type-checking has been loosened for `path.dirname()`, `path.basename()`, and `path.extname()` (Colin Ihrig) [#1216](https://github.com/nodejs/node/pull/1216).
* **querystring**: Internal optimizations in `querystring.parse()` and `querystring.stringify()` [#847](https://github.com/nodejs/node/pull/847) prevented `Number` literals from being properly converted via `querystring.escape()` [#1208](https://github.com/nodejs/node/issues/1208), exposing a blind-spot in the test suite. The bug and the tests have now been fixed (Jeremiah Senkpiel) [#1213](https://github.com/nodejs/node/pull/1213).

### Known issues

* Possible remaining TLS-related memory leak(s), details at [#1075](https://github.com/nodejs/node/issues/1075).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* Not possible to build io.js as a static library [#686](https://github.com/nodejs/node/issues/686)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)

### Commits

* [[`3b9eab9779`](https://github.com/nodejs/node/commit/3b9eab9779)] - **build**: make check aliases test (Johan Bergström) [#1211](https://github.com/nodejs/node/pull/1211)
* [[`4c731042d4`](https://github.com/nodejs/node/commit/4c731042d4)] - **configure**: use cc and c++ as defaults on os x (Ben Noordhuis) [#1210](https://github.com/nodejs/node/pull/1210)
* [[`8de78e470d`](https://github.com/nodejs/node/commit/8de78e470d)] - **path**: reduce type checking on some methods (cjihrig) [#1216](https://github.com/nodejs/node/pull/1216)
* [[`c9aec2b716`](https://github.com/nodejs/node/commit/c9aec2b716)] - **querystring**: fix broken stringifyPrimitive (Jeremiah Senkpiel) [#1213](https://github.com/nodejs/node/pull/1213)
* [[`a89f5c2156`](https://github.com/nodejs/node/commit/a89f5c2156)] - **querystring**: parse numbers correctly (Jeremiah Senkpiel) [#1213](https://github.com/nodejs/node/pull/1213)
* [[`2034137385`](https://github.com/nodejs/node/commit/2034137385)] - **smalloc**: don't mix malloc() and new char\[\] (Ben Noordhuis) [#1205](https://github.com/nodejs/node/pull/1205)

## 2015-03-19, Version 1.6.0, @chrisdickinson

### Notable changes

* **node**: a new `-r` or `--require` command-line option can be used to pre-load modules at start-up (Ali Ijaz Sheikh) [#881](https://github.com/nodejs/node/pull/881).
* **querystring**: `parse()` and `stringify()` are now faster (Brian White) [#847](https://github.com/nodejs/node/pull/847).
* **http**: the `http.ClientRequest#flush()` method has been deprecated and replaced with `http.ClientRequest#flushHeaders()` to match the same change now in Node.js v0.12 as per [joyent/node#9048](https://github.com/joyent/node/pull/9048) (Yosuke Furukawa) [#1156](https://github.com/nodejs/node/pull/1156).
* **net**: allow `server.listen()` to accept a `String` option for `port`, e.g. `{ port: "1234" }`, to match the same option being accepted in `net.connect()` as of [joyent/node#9268](https://github.com/joyent/node/pull/9268) (Ben Noordhuis) [#1116](https://github.com/nodejs/node/pull/1116).
* **tls**: further work on the reported memory leak although there appears to be a minor leak remaining for the use-case in question, track progress at [#1075](https://github.com/nodejs/node/issues/1075).
* **v8**: backport a fix for an integer overflow when `--max_old_space_size` values above `4096` are used (Ben Noordhuis) [#1166](https://github.com/nodejs/node/pull/1166).
* **platforms**: the io.js CI system now reports passes on **FreeBSD** and **SmartOS** (_Solaris_).
* **npm**: upgrade npm to 2.7.1. See [npm CHANGELOG.md](https://github.com/npm/npm/blob/master/CHANGELOG.md#v271-2015-03-05) for details. Summary:
  * [`6823807`](https://github.com/npm/npm/commit/6823807bba) [#7121](https://github.com/npm/npm/issues/7121) `npm install --save` for Git dependencies saves the URL passed in, instead of the temporary directory used to clone the remote repo. Fixes using Git dependencies when shrinkwwapping. In the process, rewrote the Git dependency caching code. Again. No more single-letter variable names, and a much clearer workflow. ([@othiym23](https://github.com/othiym23))
  * [`abdd040`](https://github.com/npm/npm/commit/abdd040da9) read-package-json@1.3.2: Provide more helpful error messages when JSON parse errors are encountered by using a more forgiving JSON parser than JSON.parse. ([@smikes](https://github.com/smikes))
  * [`c56cfcd`](https://github.com/npm/npm/commit/c56cfcd79c) [#7525](https://github.com/npm/npm/issues/7525) `npm dedupe` handles scoped packages. ([@KidkArolis](https://github.com/KidkArolis))
  * [`4ef1412`](https://github.com/npm/npm/commit/4ef1412d00) [#7075](https://github.com/npm/npm/issues/7075) If you try to tag a release as a valid semver range, `npm publish` and `npm tag` will error early instead of proceeding. ([@smikes](https://github.com/smikes))

### Known issues

* Possible remaining TLS-related memory leak(s), details at [#1075](https://github.com/nodejs/node/issues/1075).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* Not possible to build io.js as a static library [#686](https://github.com/nodejs/node/issues/686)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)

### Commits

* [[`a84ea66b35`](https://github.com/nodejs/node/commit/a84ea66b35)] - **deps**: upgrade to openssl-1.0.1m (Shigeki Ohtsu) [#1206](https://github.com/nodejs/node/pull/1206)
* [[`3bc445f6c2`](https://github.com/nodejs/node/commit/3bc445f6c2)] - **doc**: fix a broken collaborator github link (Aleksanteri Negru-Vode) [#1204](https://github.com/nodejs/node/pull/1204)
* [[`813a536126`](https://github.com/nodejs/node/commit/813a536126)] - **buffer**: removing duplicate code (Thorsten Lorenz) [#1144](https://github.com/nodejs/node/pull/1144)
* [[`1514b82355`](https://github.com/nodejs/node/commit/1514b82355)] - **(SEMVER-MINOR) src**: add -r/--require flags for preloading modules (Ali Ijaz Sheikh) [#881](https://github.com/nodejs/node/pull/881)
* [[`f600111d82`](https://github.com/nodejs/node/commit/f600111d82)] - **test**: cache lazy properties, fix style nits (Rod Vagg) [#1196](https://github.com/nodejs/node/pull/1196)
* [[`3038b8ee6a`](https://github.com/nodejs/node/commit/3038b8ee6a)] - **test**: double timeout in tls-wrap-timeout.js (Fedor Indutny) [#1201](https://github.com/nodejs/node/pull/1201)
* [[`dd37fb4c48`](https://github.com/nodejs/node/commit/dd37fb4c48)] - **build**: remove incorrect argument in vcbuild.bat (Jeremiah Senkpiel) [#1198](https://github.com/nodejs/node/pull/1198)
* [[`2b2e48a4b9`](https://github.com/nodejs/node/commit/2b2e48a4b9)] - **lib**: don't error in repl when cwd doesn't exist (Ben Noordhuis) [#1194](https://github.com/nodejs/node/pull/1194)
* [[`2c6f79c08c`](https://github.com/nodejs/node/commit/2c6f79c08c)] - **src**: don't error at startup when cwd doesn't exist (Ben Noordhuis) [#1194](https://github.com/nodejs/node/pull/1194)
* [[`c15e81afdd`](https://github.com/nodejs/node/commit/c15e81afdd)] - **test**: Introduce knowledge of FreeBSD jails (Johan Bergström) [#1167](https://github.com/nodejs/node/pull/1167)
* [[`fe0f015c51`](https://github.com/nodejs/node/commit/fe0f015c51)] - **src**: fix crypto bio integer wraparound on 32 bits (Ben Noordhuis) [#1192](https://github.com/nodejs/node/pull/1192)
* [[`2b63bcd247`](https://github.com/nodejs/node/commit/2b63bcd247)] - **doc**: add yosuke-furukawa as collaborator (Yosuke Furukawa) [#1183](https://github.com/nodejs/node/pull/1183)
* [[`69350baaef`](https://github.com/nodejs/node/commit/69350baaef)] - **doc**: update test section in CONTRIBUTING.md (Ben Noordhuis) [#1181](https://github.com/nodejs/node/pull/1181)
* [[`3c8ae2d934`](https://github.com/nodejs/node/commit/3c8ae2d934)] - **doc**: add petkaantonov as collaborator (Petka Antonov) [#1179](https://github.com/nodejs/node/pull/1179)
* [[`92c1ad97c0`](https://github.com/nodejs/node/commit/92c1ad97c0)] - **doc**: add silverwind as collaborator (Roman Reiss) [#1176](https://github.com/nodejs/node/pull/1176)
* [[`14c74d5326`](https://github.com/nodejs/node/commit/14c74d5326)] - **doc**: add jbergstroem as collaborator (Johan Bergström) [#1175](https://github.com/nodejs/node/pull/1175)
* [[`8b2363d2fd`](https://github.com/nodejs/node/commit/8b2363d2fd)] - **configure**: use gcc and g++ as CC and CXX defaults (Ben Noordhuis) [#1174](https://github.com/nodejs/node/pull/1174)
* [[`08ec897f82`](https://github.com/nodejs/node/commit/08ec897f82)] - **doc**: fix typo in buffer module documentation (Alex Yursha) [#1169](https://github.com/nodejs/node/pull/1169)
* [[`c638dad567`](https://github.com/nodejs/node/commit/c638dad567)] - **benchmark**: add output format option \[csv\] (Brendan Ashworth) [#777](https://github.com/nodejs/node/pull/777)
* [[`97d8d4928d`](https://github.com/nodejs/node/commit/97d8d4928d)] - **benchmark**: add plot_csv R graphing script (Brendan Ashworth) [#777](https://github.com/nodejs/node/pull/777)
* [[`22793da485`](https://github.com/nodejs/node/commit/22793da485)] - **v8**: fix --max_old_space_size=4096 integer overflow (Ben Noordhuis) [#1166](https://github.com/nodejs/node/pull/1166)
* [[`b2e00e38dc`](https://github.com/nodejs/node/commit/b2e00e38dc)] - **(SEMVER-MINOR) http**: add flushHeaders and deprecate flush (Yosuke Furukawa) [#1156](https://github.com/nodejs/node/pull/1156)
* [[`68d4bed2fd`](https://github.com/nodejs/node/commit/68d4bed2fd)] - **make**: remove node_dtrace from cpplint excludes (Julien Gilli) [joyent/node#8741](https://github.com/joyent/node/pull/8741)
* [[`30666f22ca`](https://github.com/nodejs/node/commit/30666f22ca)] - **net**: use cached peername to resolve remote fields (James Hartig) [joyent/node#9366](https://github.com/joyent/node/pull/9366)
* [[`e6e616fdcb`](https://github.com/nodejs/node/commit/e6e616fdcb)] - **doc**: fix '\\' typos on Windows (Steven Vercruysse) [joyent/node#9412](https://github.com/joyent/node/pull/9412)
* [[`89bf6c05e9`](https://github.com/nodejs/node/commit/89bf6c05e9)] - **build**: allow custom PackageMaker path (Julien Gilli) [joyent/node#9377](https://github.com/joyent/node/pull/9377)
* [[`f58e59649d`](https://github.com/nodejs/node/commit/f58e59649d)] - **lib**: remove broken NODE_MODULE_CONTEXTS feature (Ben Noordhuis) [#1162](https://github.com/nodejs/node/pull/1162)
* [[`2551c1d2ca`](https://github.com/nodejs/node/commit/2551c1d2ca)] - **src**: use Number::New() for heapTotal/heapUsed (Ben Noordhuis) [#1148](https://github.com/nodejs/node/pull/1148)
* [[`4f394998ba`](https://github.com/nodejs/node/commit/4f394998ba)] - **src**: don't create js string twice on error (Ben Noordhuis) [#1148](https://github.com/nodejs/node/pull/1148)
* [[`eb995d6822`](https://github.com/nodejs/node/commit/eb995d6822)] - **path**: add type checking for path inputs (cjihrig) [#1153](https://github.com/nodejs/node/pull/1153)
* [[`a28945b128`](https://github.com/nodejs/node/commit/a28945b128)] - **doc**: reflect new require('events') behaviour (Alex Yursha) [#975](https://github.com/nodejs/node/pull/975)
* [[`85a92a37ef`](https://github.com/nodejs/node/commit/85a92a37ef)] - **querystring**: optimize parse and stringify (Brian White) [#847](https://github.com/nodejs/node/pull/847)
* [[`65d0a8eca8`](https://github.com/nodejs/node/commit/65d0a8eca8)] - **deps**: make node-gyp work with io.js (cjihrig) [#990](https://github.com/nodejs/node/pull/990)
* [[`7d0baf1741`](https://github.com/nodejs/node/commit/7d0baf1741)] - **deps**: upgrade npm to 2.7.1 (Forrest L Norvell) [#1142](https://github.com/nodejs/node/pull/1142)
* [[`4eb8810a27`](https://github.com/nodejs/node/commit/4eb8810a27)] - **tls**: re-enable `.writev()` on TLSWrap (Fedor Indutny) [#1155](https://github.com/nodejs/node/pull/1155)
* [[`e90ed790c3`](https://github.com/nodejs/node/commit/e90ed790c3)] - **tls**: fix leak on `DoWrite()` errors (Fedor Indutny) [#1154](https://github.com/nodejs/node/pull/1154)
* [[`056ed4b0c9`](https://github.com/nodejs/node/commit/056ed4b0c9)] - **src**: revert -r/--require flags (Chris Dickinson) [#1150](https://github.com/nodejs/node/pull/1150)
* [[`7a5b023bac`](https://github.com/nodejs/node/commit/7a5b023bac)] - **doc**: fix vm module examples (FangDun Cai) [#1147](https://github.com/nodejs/node/pull/1147)
* [[`7bde3f1a8f`](https://github.com/nodejs/node/commit/7bde3f1a8f)] - **(SEMVER-MINOR) src**: add -r/--require flags for preloading modules (Ali Ijaz Sheikh) [#881](https://github.com/nodejs/node/pull/881)
* [[`53e200acc2`](https://github.com/nodejs/node/commit/53e200acc2)] - **test**: fix test-http-content-length (Jeremiah Senkpiel) [#1145](https://github.com/nodejs/node/pull/1145)
* [[`d8c4a932c9`](https://github.com/nodejs/node/commit/d8c4a932c9)] - **crypto**: add deprecated ValiCert CA for cross cert (Shigeki Ohtsu) [#1135](https://github.com/nodejs/node/pull/1135)
* [[`82f067e60b`](https://github.com/nodejs/node/commit/82f067e60b)] - **test**: fix ext commands to be double quoted (Shigeki Ohtsu) [#1122](https://github.com/nodejs/node/pull/1122)
* [[`5ecdc0314d`](https://github.com/nodejs/node/commit/5ecdc0314d)] - **test**: add test for reading a large file through a pipe (Santiago Gimeno) [#1074](https://github.com/nodejs/node/pull/1074)
* [[`a6af709489`](https://github.com/nodejs/node/commit/a6af709489)] - **fs**: use stat.st_size only to read regular files (Santiago Gimeno) [#1074](https://github.com/nodejs/node/pull/1074)
* [[`0782c24993`](https://github.com/nodejs/node/commit/0782c24993)] - **test**: fix readfile-zero-byte-liar test (Santiago Gimeno) [#1074](https://github.com/nodejs/node/pull/1074)
* [[`e2c9040995`](https://github.com/nodejs/node/commit/e2c9040995)] - **src**: do not leak handles on debug and exit (Fedor Indutny) [#1133](https://github.com/nodejs/node/pull/1133)
* [[`8c4f0df464`](https://github.com/nodejs/node/commit/8c4f0df464)] - **v8**: fix build on solaris platforms (Johan Bergström) [#1079](https://github.com/nodejs/node/pull/1079)
* [[`41c9daa143`](https://github.com/nodejs/node/commit/41c9daa143)] - **build**: fix incorrect set in vcbuild.bat (Bert Belder)
* [[`07c066724c`](https://github.com/nodejs/node/commit/07c066724c)] - **buffer**: align chunks on 8-byte boundary (Fedor Indutny) [#1126](https://github.com/nodejs/node/pull/1126)
* [[`d33a647b4b`](https://github.com/nodejs/node/commit/d33a647b4b)] - **doc**: make tools/update-authors.sh cross-platform (Ben Noordhuis) [#1121](https://github.com/nodejs/node/pull/1121)
* [[`8453fbc879`](https://github.com/nodejs/node/commit/8453fbc879)] - **https**: don't overwrite servername option (skenqbx) [#1110](https://github.com/nodejs/node/pull/1110)
* [[`60dac07b06`](https://github.com/nodejs/node/commit/60dac07b06)] - **doc**: add Malte-Thorben Bruns to .mailmap (Ben Noordhuis) [#1118](https://github.com/nodejs/node/pull/1118)
* [[`480b48244f`](https://github.com/nodejs/node/commit/480b48244f)] - **(SEMVER-MINOR) lib**: allow server.listen({ port: "1234" }) (Ben Noordhuis) [#1116](https://github.com/nodejs/node/pull/1116)
* [[`80e14d736e`](https://github.com/nodejs/node/commit/80e14d736e)] - **doc**: move checkServerIdentity option to tls.connect() (skenqbx) [#1107](https://github.com/nodejs/node/pull/1107)
* [[`684a5878b6`](https://github.com/nodejs/node/commit/684a5878b6)] - **doc**: fix missing periods in url.markdown (Ryuichi Okumura) [#1115](https://github.com/nodejs/node/pull/1115)
* [[`8431fc53f1`](https://github.com/nodejs/node/commit/8431fc53f1)] - **tls_wrap**: proxy handle methods in prototype (Fedor Indutny) [#1108](https://github.com/nodejs/node/pull/1108)
* [[`8070b1ff99`](https://github.com/nodejs/node/commit/8070b1ff99)] - **buffer**: Don't assign .parent if none exists (Trevor Norris) [#1109](https://github.com/nodejs/node/pull/1109)

## 2015-03-11, Version 0.10.37 (Maintenance)

### Commits

* [[`dcff5d565c`](https://github.com/nodejs/node/commit/dcff5d565c)] - uv: update to 0.10.36 (CVE-2015-0278) [#9274](https://github.com/joyent/node/pull/9274)
* [[`f2a45caf2e`](https://github.com/nodejs/node/commit/f2a45caf2e)] - domains: fix stack clearing after error handled (Jonas Dohse) [#9364](https://github.com/joyent/node/pull/9364)
* [[`d01a900078`](https://github.com/nodejs/node/commit/d01a900078)] - buffer: reword Buffer.concat error message (Chris Dickinson) [#8723](https://github.com/joyent/node/pull/8723)
* [[`c8239c08d7`](https://github.com/nodejs/node/commit/c8239c08d7)] - console: allow Object.prototype fields as labels (Julien Gilli) [#9215](https://github.com/joyent/node/pull/9215)
* [[`431eb172f9`](https://github.com/nodejs/node/commit/431eb172f9)] - V8: log version in profiler log file (Ben Noordhuis) [#9043](https://github.com/joyent/node/pull/9043)
* [[`8bcd0a4c4a`](https://github.com/nodejs/node/commit/8bcd0a4c4a)] - http: fix performance regression for GET requests (Florin-Cristian Gavrila) [#9026](https://github.com/joyent/node/pull/9026)

## 2015-03-09, Version 1.5.1, @rvagg

### Notable changes

* **tls**: The reported TLS memory leak has been at least partially resolved via various commits in this release. Current testing indicated that there _may_ still be some leak problems. Track complete progress at [#1075](https://github.com/nodejs/node/issues/1075).
* **http**: Fixed an error reported at [joyent/node#9348](https://github.com/joyent/node/issues/9348) and [npm/npm#7349](https://github.com/npm/npm/issues/7349). Pending data was not being fully read upon an `'error'` event leading to an assertion failure on `socket.destroy()`. (Fedor Indutny) [#1103](https://github.com/nodejs/node/pull/1103)

### Known issues

* Possible remaining TLS-related memory leak(s), details at [#1075](https://github.com/nodejs/node/issues/1075).
* Windows still reports some minor test failures and we are continuing to address all of these as a priority. See [#1005](https://github.com/nodejs/node/issues/1005).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* Not possible to build io.js as a static library [#686](https://github.com/nodejs/node/issues/686)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)

### Commits

* [[`030a92347d`](https://github.com/nodejs/node/commit/030a92347d)] - **benchmark**: chunky http client benchmark variation (Rudi Cilibrasi) [#228](https://github.com/nodejs/node/pull/228)
* [[`3b57819b58`](https://github.com/nodejs/node/commit/3b57819b58)] - **crypto**: fix leak in SafeX509ExtPrint (Fedor Indutny) [#1087](https://github.com/nodejs/node/pull/1087)
* [[`f8c893dd39`](https://github.com/nodejs/node/commit/f8c893dd39)] - **doc**: fix confusion markdown in util.markdown (Yazhong Liu) [#1097](https://github.com/nodejs/node/pull/1097)
* [[`e763220f66`](https://github.com/nodejs/node/commit/e763220f66)] - **doc**: update clang version prerequisite (Brendan Ashworth) [#1094](https://github.com/nodejs/node/pull/1094)
* [[`0f7c8ebeea`](https://github.com/nodejs/node/commit/0f7c8ebeea)] - **doc**: replace article "an" with "a" in net docs (Evan Lucas) [#1093](https://github.com/nodejs/node/pull/1093)
* [[`cf565b5516`](https://github.com/nodejs/node/commit/cf565b5516)] - **fs**: fix .write() not coercing non-string values (Jeremiah Senkpiel) [#1102](https://github.com/nodejs/node/pull/1102)
* [[`1a3ca8223e`](https://github.com/nodejs/node/commit/1a3ca8223e)] - **http_client**: ensure empty socket on error (Fedor Indutny) [#1103](https://github.com/nodejs/node/pull/1103)
* [[`8670613d2d`](https://github.com/nodejs/node/commit/8670613d2d)] - **node_crypto_bio**: adjust external memory size (Fedor Indutny) [#1085](https://github.com/nodejs/node/pull/1085)
* [[`528d8786ff`](https://github.com/nodejs/node/commit/528d8786ff)] - **src**: fix memory leak in fs.writeSync error path (Ben Noordhuis) [#1092](https://github.com/nodejs/node/pull/1092)
* [[`648fc63cd1`](https://github.com/nodejs/node/commit/648fc63cd1)] - **src**: fix mismatched delete[] in src/node_file.cc (Ben Noordhuis) [#1092](https://github.com/nodejs/node/pull/1092)
* [[`9f7c9811e2`](https://github.com/nodejs/node/commit/9f7c9811e2)] - **src**: add missing Context::Scope (Ben Noordhuis) [#1084](https://github.com/nodejs/node/pull/1084)
* [[`fe36076c78`](https://github.com/nodejs/node/commit/fe36076c78)] - **stream_base**: WriteWrap::New/::Dispose (Fedor Indutny) [#1090](https://github.com/nodejs/node/pull/1090)
* [[`7f4c95e160`](https://github.com/nodejs/node/commit/7f4c95e160)] - **tls**: do not leak WriteWrap objects (Fedor Indutny) [#1090](https://github.com/nodejs/node/pull/1090)
* [[`4bd3620382`](https://github.com/nodejs/node/commit/4bd3620382)] - **url**: remove redundant assignment in url.parse (Alex Kocharin) [#1095](https://github.com/nodejs/node/pull/1095)

## 2015-03-06, Version 1.5.0, @rvagg

### Notable changes

* **buffer**: New `Buffer#indexOf()` method, modelled off [`Array#indexOf()`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/indexOf). Accepts a String, Buffer or a Number. Strings are interpreted as UTF8. (Trevor Norris) [#561](https://github.com/nodejs/node/pull/561)
* **fs**: `options` object properties in `'fs'` methods no longer perform a `hasOwnProperty()` check, thereby allowing options objects to have prototype properties that apply. (Jonathan Ong) [#635](https://github.com/nodejs/node/pull/635)
* **tls**: A likely TLS memory leak was reported by PayPal. Some of the recent changes in **stream_wrap** appear to be to blame. The initial fix is in [#1078](https://github.com/nodejs/node/pull/1078), you can track the progress toward closing the leak at [#1075](https://github.com/nodejs/node/issues/1075) (Fedor Indutny).
* **npm**: Upgrade npm to 2.7.0. See [npm CHANGELOG.md](https://github.com/npm/npm/blob/master/CHANGELOG.md#v270-2015-02-26) for details including why this is a semver-minor when it could have been semver-major. Summary:
  * [`145af65`](https://github.com/npm/npm/commit/145af6587f45de135cc876be2027ed818ed4ca6a)
    [#4887](https://github.com/npm/npm/issues/4887) Replace calls to the
    `node-gyp` script bundled with npm by passing the
    `--node-gyp=/path/to/node-gyp` option to npm. Swap in `pangyp` or a version
    of `node-gyp` modified to work better with io.js without having to touch
    npm's code!  ([@ackalker](https://github.com/ackalker))
  * [`2f6a1df`](https://github.com/npm/npm/commit/2f6a1df3e1e3e0a3bc4abb69e40f59a64204e7aa)
    [#1999](https://github.com/npm/npm/issues/1999) Only run `stop` and `start`
    scripts (plus their pre- and post- scripts) when there's no `restart` script
    defined. This makes it easier to support graceful restarts of services
    managed by npm.  ([@watilde](https://github.com/watilde) /
    [@scien](https://github.com/scien))
  * [`448efd0`](https://github.com/npm/npm/commit/448efd0eaa6f97af0889bf47efc543a1ea2f8d7e)
    [#2853](https://github.com/npm/npm/issues/2853) Add support for `--dev` and
    `--prod` to `npm ls`, so that you can list only the trees of production or
    development dependencies, as desired.
    ([@watilde](https://github.com/watilde))
  * [`a0a8777`](https://github.com/npm/npm/commit/a0a87777af8bee180e4e9321699f050c29ed5ac4)
    [#7463](https://github.com/npm/npm/issues/7463) Split the list printed by
    `npm run-script` into lifecycle scripts and scripts directly invoked via `npm
    run-script`. ([@watilde](https://github.com/watilde))
  * [`a5edc17`](https://github.com/npm/npm/commit/a5edc17d5ef1435b468a445156a4a109df80f92b)
    [#6749](https://github.com/npm/npm/issues/6749) `init-package-json@1.3.1`:
    Support for passing scopes to `npm init` so packages are initialized as part
    of that scope / organization / team. ([@watilde](https://github.com/watilde))
* **TC**: Colin Ihrig (@cjihrig) resigned from the TC due to his desire to do more code and fewer meetings.

### Known issues

* Possible TLS-related memory leak, details at [#1075](https://github.com/nodejs/node/issues/1075).
* Windows still reports some minor test failures and we are continuing to address all of these as a priority. See [#1005](https://github.com/nodejs/node/issues/1005).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* Not possible to build io.js as a static library [#686](https://github.com/nodejs/node/issues/686)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)

### Commits

* [[`b27931b0fe`](https://github.com/nodejs/node/commit/b27931b0fe)] - **benchmark**: fix `wrk` check (Brian White) [#1076](https://github.com/nodejs/node/pull/1076)
* [[`2b79052494`](https://github.com/nodejs/node/commit/2b79052494)] - **benchmark**: check for wrk ahead of running benchmarks (Johan Bergström) [#982](https://github.com/nodejs/node/pull/982)
* [[`31421afe89`](https://github.com/nodejs/node/commit/31421afe89)] - **buffer**: reword Buffer.concat error message (Chris Dickinson) [joyent/node#8723](https://github.com/joyent/node/pull/8723)
* [[`78581c8d90`](https://github.com/nodejs/node/commit/78581c8d90)] - **(SEMVER-MINOR) buffer**: add indexOf() method (Trevor Norris) [#561](https://github.com/nodejs/node/pull/561)
* [[`37bb1df7c4`](https://github.com/nodejs/node/commit/37bb1df7c4)] - **build**: remove mdb from io.js (Johan Bergström) [#1023](https://github.com/nodejs/node/pull/1023)
* [[`726671cb0e`](https://github.com/nodejs/node/commit/726671cb0e)] - **build**: add basic mips/mipsel support (Ben Noordhuis) [#1045](https://github.com/nodejs/node/pull/1045)
* [[`a45d4f8fd6`](https://github.com/nodejs/node/commit/a45d4f8fd6)] - **build**: remove tools/wrk from the tree (Johan Bergström) [#982](https://github.com/nodejs/node/pull/982)
* [[`dee07e2983`](https://github.com/nodejs/node/commit/dee07e2983)] - **deps**: make node-gyp work with io.js (cjihrig) [#990](https://github.com/nodejs/node/pull/990)
* [[`fe14802fb7`](https://github.com/nodejs/node/commit/fe14802fb7)] - **deps**: upgrade npm to 2.7.0 (Forrest L Norvell) [#1080](https://github.com/nodejs/node/pull/1080)
* [[`31142415de`](https://github.com/nodejs/node/commit/31142415de)] - **doc**: add TC meeting 2015-02-18 minutes (Rod Vagg) [#1051](https://github.com/nodejs/node/pull/1051)
* [[`6190a2236b`](https://github.com/nodejs/node/commit/6190a2236b)] - **doc**: remove cjihrig from TC (cjihrig) [#1056](https://github.com/nodejs/node/pull/1056)
* [[`9741291fe9`](https://github.com/nodejs/node/commit/9741291fe9)] - **doc**: fix child_process heading depth (Sam Roberts) [#1038](https://github.com/nodejs/node/pull/1038)
* [[`c8110692a5`](https://github.com/nodejs/node/commit/c8110692a5)] - **doc**: add explanations for querystring (Robert Kowalski) [joyent/node#9259](https://github.com/joyent/node/pull/9259)
* [[`8fb711e06c`](https://github.com/nodejs/node/commit/8fb711e06c)] - **doc**: fix default value of opts.decodeURIComponent (h7lin) [joyent/node#9259](https://github.com/joyent/node/pull/9259)
* [[`6433ad1eef`](https://github.com/nodejs/node/commit/6433ad1eef)] - **doc**: add missing newline in CHANGELOG (Rod Vagg)
* [[`555a7c48cf`](https://github.com/nodejs/node/commit/555a7c48cf)] - **events**: optimize listener array cloning (Brian White) [#1050](https://github.com/nodejs/node/pull/1050)
* [[`4d0329ebeb`](https://github.com/nodejs/node/commit/4d0329ebeb)] - **(SEMVER-MINOR) fs**: remove unnecessary usage of .hasOwnProperty() (Jonathan Ong) [#635](https://github.com/nodejs/node/pull/635)
* [[`4874182065`](https://github.com/nodejs/node/commit/4874182065)] - **http**: send Content-Length when possible (Christian Tellnes) [#1062](https://github.com/nodejs/node/pull/1062)
* [[`08133f45c7`](https://github.com/nodejs/node/commit/08133f45c7)] - **http**: optimize outgoing requests (Brendan Ashworth) [#605](https://github.com/nodejs/node/pull/605)
* [[`dccb69a21a`](https://github.com/nodejs/node/commit/dccb69a21a)] - **js_stream**: fix leak of instances (Fedor Indutny) [#1078](https://github.com/nodejs/node/pull/1078)
* [[`4ddd6406ce`](https://github.com/nodejs/node/commit/4ddd6406ce)] - **lib**: avoid .toLowerCase() call in Buffer#write() (Ben Noordhuis) [#1048](https://github.com/nodejs/node/pull/1048)
* [[`bbf54a554a`](https://github.com/nodejs/node/commit/bbf54a554a)] - **lib**: hand-optimize Buffer constructor (Ben Noordhuis) [#1048](https://github.com/nodejs/node/pull/1048)
* [[`9d2b89d06c`](https://github.com/nodejs/node/commit/9d2b89d06c)] - **net**: allow port 0 in connect() (cjihrig) [joyent/node#9268](https://github.com/joyent/node/pull/9268)
* [[`e0835c9cda`](https://github.com/nodejs/node/commit/e0835c9cda)] - **node**: improve performance of nextTick (Trevor Norris) [#985](https://github.com/nodejs/node/pull/985)
* [[`8f5f12bb48`](https://github.com/nodejs/node/commit/8f5f12bb48)] - **smalloc**: export constants from C++ (Vladimir Kurchatkin) [#920](https://github.com/nodejs/node/pull/920)
* [[`0697f8b44d`](https://github.com/nodejs/node/commit/0697f8b44d)] - **smalloc**: validate arguments in js (Vladimir Kurchatkin) [#920](https://github.com/nodejs/node/pull/920)
* [[`1640dedb3b`](https://github.com/nodejs/node/commit/1640dedb3b)] - **src**: fix ucs-2 buffer encoding regression (Ben Noordhuis) [#1042](https://github.com/nodejs/node/pull/1042)
* [[`2eda2d6096`](https://github.com/nodejs/node/commit/2eda2d6096)] - **src**: fix external string length calculation (Ben Noordhuis) [#1042](https://github.com/nodejs/node/pull/1042)
* [[`4aea16f214`](https://github.com/nodejs/node/commit/4aea16f214)] - **src**: rename confusingly named local variable (Ben Noordhuis) [#1042](https://github.com/nodejs/node/pull/1042)
* [[`c9ee654290`](https://github.com/nodejs/node/commit/c9ee654290)] - **src**: simplify node::Utf8Value() (Ben Noordhuis) [#1042](https://github.com/nodejs/node/pull/1042)
* [[`364cc7e08a`](https://github.com/nodejs/node/commit/364cc7e08a)] - **src**: remove NODE_INVALID_UTF8 environment variable (Ben Noordhuis) [#1042](https://github.com/nodejs/node/pull/1042)
* [[`826cde8661`](https://github.com/nodejs/node/commit/826cde8661)] - **src**: fix gc heuristic for external twobyte strings (Ben Noordhuis) [#1042](https://github.com/nodejs/node/pull/1042)
* [[`f5b7e18243`](https://github.com/nodejs/node/commit/f5b7e18243)] - **src**: remove unused code (Ben Noordhuis) [#1042](https://github.com/nodejs/node/pull/1042)
* [[`4ae64b2626`](https://github.com/nodejs/node/commit/4ae64b2626)] - **src**: extract node env init out of process init (Petka Antonov) [#980](https://github.com/nodejs/node/pull/980)
* [[`b150c9839e`](https://github.com/nodejs/node/commit/b150c9839e)] - **src**: fix -Wempty-body compiler warnings (Ben Noordhuis) [#974](https://github.com/nodejs/node/pull/974)
* [[`fb284e2e4d`](https://github.com/nodejs/node/commit/fb284e2e4d)] - **src**: fix compiler warning in smalloc.cc (Ben Noordhuis) [#1055](https://github.com/nodejs/node/pull/1055)
* [[`583a868bcd`](https://github.com/nodejs/node/commit/583a868bcd)] - **stream_wrap**: add HandleScope's in uv callbacks (Fedor Indutny) [#1078](https://github.com/nodejs/node/pull/1078)
* [[`e2fb733a95`](https://github.com/nodejs/node/commit/e2fb733a95)] - **test**: simplify parallel/test-stringbytes-external (Ben Noordhuis) [#1042](https://github.com/nodejs/node/pull/1042)
* [[`7b554b1a8f`](https://github.com/nodejs/node/commit/7b554b1a8f)] - **test**: don't spawn child processes in domain test (Ben Noordhuis) [#974](https://github.com/nodejs/node/pull/974)
* [[`b72fa03057`](https://github.com/nodejs/node/commit/b72fa03057)] - **test**: adds a test for undefined value in setHeader (Ken Perkins) [#970](https://github.com/nodejs/node/pull/970)
* [[`563771d8b1`](https://github.com/nodejs/node/commit/563771d8b1)] - **test**: split parts out of host-headers test into its own test (Johan Bergström) [#1049](https://github.com/nodejs/node/pull/1049)
* [[`671fbd5a9d`](https://github.com/nodejs/node/commit/671fbd5a9d)] - **test**: refactor all tests that depends on crypto (Johan Bergström) [#1049](https://github.com/nodejs/node/pull/1049)
* [[`c7ad320472`](https://github.com/nodejs/node/commit/c7ad320472)] - **test**: check for openssl cli and provide path if it exists (Johan Bergström) [#1049](https://github.com/nodejs/node/pull/1049)
* [[`71776f9057`](https://github.com/nodejs/node/commit/71776f9057)] - **test**: remove unused https imports (Johan Bergström) [#1049](https://github.com/nodejs/node/pull/1049)
* [[`3d5726c4ad`](https://github.com/nodejs/node/commit/3d5726c4ad)] - **test**: introduce a helper that checks if crypto is available (Johan Bergström) [#1049](https://github.com/nodejs/node/pull/1049)
* [[`d0e7c359a7`](https://github.com/nodejs/node/commit/d0e7c359a7)] - **test**: don't assume process.versions.openssl always is available (Johan Bergström) [#1049](https://github.com/nodejs/node/pull/1049)
* [[`e1bf6709dc`](https://github.com/nodejs/node/commit/e1bf6709dc)] - **test**: fix racey-ness in tls-inception (Fedor Indutny) [#1040](https://github.com/nodejs/node/pull/1040)
* [[`fd3ea29902`](https://github.com/nodejs/node/commit/fd3ea29902)] - **test**: fix test-fs-access when uid is 0 (Johan Bergström) [#1037](https://github.com/nodejs/node/pull/1037)
* [[`5abfa930b8`](https://github.com/nodejs/node/commit/5abfa930b8)] - **test**: make destroyed-socket-write2.js more robust (Michael Dawson) [joyent/node#9270](https://github.com/joyent/node/pull/9270)
* [[`1009130495`](https://github.com/nodejs/node/commit/1009130495)] - **tests**: fix race in test-http-curl-chunk-problem (Julien Gilli) [joyent/node#9301](https://github.com/joyent/node/pull/9301)
* [[`bd1bd7e38d`](https://github.com/nodejs/node/commit/bd1bd7e38d)] - **timer**: Improve performance of callbacks (Ruben Verborgh) [#406](https://github.com/nodejs/node/pull/406)
* [[`7b3b8acfa6`](https://github.com/nodejs/node/commit/7b3b8acfa6)] - **tls**: accept empty `net.Socket`s (Fedor Indutny) [#1046](https://github.com/nodejs/node/pull/1046)
* [[`c09c90c1a9`](https://github.com/nodejs/node/commit/c09c90c1a9)] - **tls_wrap**: do not hold persistent ref to parent (Fedor Indutny) [#1078](https://github.com/nodejs/node/pull/1078)
* [[`3446ff417b`](https://github.com/nodejs/node/commit/3446ff417b)] - **tty**: do not add `shutdown` method to handle (Fedor Indutny) [#1073](https://github.com/nodejs/node/pull/1073)
* [[`abb00cc915`](https://github.com/nodejs/node/commit/abb00cc915)] - **url**: throw for invalid values to url.format (Christian Tellnes) [#1036](https://github.com/nodejs/node/pull/1036)
* [[`abd3ecfbd1`](https://github.com/nodejs/node/commit/abd3ecfbd1)] - **win,test**: fix test-stdin-from-file (Bert Belder) [#1067](https://github.com/nodejs/node/pull/1067)

## 2015-03-02, Version 1.4.3, @rvagg

### Notable changes

* **stream**: Fixed problems for platforms without `writev()` support, particularly Windows. Changes introduced in 1.4.1, via [#926](https://github.com/nodejs/node/pull/926), broke some functionality for these platforms, this has now been addressed. [#1008](https://github.com/nodejs/node/pull/1008) (Fedor Indutny)
* **arm**: We have the very beginnings of ARMv8 / ARM64 / AARCH64 support. An upgrade to OpenSSL 1.0.2 is one requirement for full support. [#1028](https://github.com/nodejs/node/pull/1028) (Ben Noordhuis)
* Add new collaborator: Julian Duque ([@julianduque](https://github.com/julianduque))

### Known issues

* Windows still reports some minor test failures and we are continuing to address all of these ASAP. See [#1005](https://github.com/nodejs/node/issues/1005).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* Not possible to build io.js as a static library [#686](https://github.com/nodejs/node/issues/686)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)

### Commits

* [[`ca3c50b789`](https://github.com/nodejs/node/commit/ca3c50b789)] - **build**: add basic arm64 support (Ben Noordhuis) [#1028](https://github.com/nodejs/node/pull/1028)
* [[`08e89b1880`](https://github.com/nodejs/node/commit/08e89b1880)] - **doc**: update AUTHORS list (Rod Vagg) [#1018](https://github.com/nodejs/node/pull/1018)
* [[`ea02d90cd0`](https://github.com/nodejs/node/commit/ea02d90cd0)] - **doc**: add julianduque as collaborator (Julian Duque) [#1021](https://github.com/nodejs/node/pull/1021)
* [[`dfe7a17784`](https://github.com/nodejs/node/commit/dfe7a17784)] - **doc**: fix typos and sources in WORKING_GROUPS.md (&! (bitandbang)) [#1022](https://github.com/nodejs/node/pull/1022)
* [[`6d26990d32`](https://github.com/nodejs/node/commit/6d26990d32)] - **doc**: Clean up net.Socket (Ryan Scheel) [#951](https://github.com/nodejs/node/pull/951)
* [[`c380ac6e98`](https://github.com/nodejs/node/commit/c380ac6e98)] - **doc**: suggest alternatives to deprecated APs (Benjamin Gruenbaum) [#1007](https://github.com/nodejs/node/pull/1007)
* [[`3d6440cf2a`](https://github.com/nodejs/node/commit/3d6440cf2a)] - **src**: fix --without-ssl build (Ben Noordhuis) [#1027](https://github.com/nodejs/node/pull/1027)
* [[`2b47fd2eb6`](https://github.com/nodejs/node/commit/2b47fd2eb6)] - **stream_base**: `.writev()` has limited support (Fedor Indutny) [#1008](https://github.com/nodejs/node/pull/1008)

## 2015-02-28, Version 1.4.2, @rvagg

### Notable changes

* **tls**: A typo introduced in the TLSWrap changes in [#840](https://github.com/nodejs/node/pull/840) only encountered as a bug on Windows was not caught by the io.js CI system due to problems with the Windows build script and the Windows CI slave configuration, see Known Issues below. Fixed in [#994](https://github.com/nodejs/node/pull/994) & [#1004](https://github.com/nodejs/node/pull/1004). (Fedor Indutny)
* **npm**: Upgrade npm to 2.6.1. See [npm CHANGELOG.md](https://github.com/npm/npm/blob/master/CHANGELOG.md#v260-2015-02-12) for details. Summary:
  * [`8b98f0e`](https://github.com/npm/npm/commit/8b98f0e709d77a8616c944aebd48ab726f726f76)
    [#4471](https://github.com/npm/npm/issues/4471) `npm outdated` (and only `npm
    outdated`) now defaults to `--depth=0`. This also has the excellent but unexpected effect of making `npm update -g` work the way almost everyone wants it to. See the [docs for
    `--depth`](https://github.com/npm/npm/blob/82f484672adb1a3caf526a8a48832789495bb43d/doc/misc/npm-config.md#depth)
    for the mildly confusing details. ([@smikes](https://github.com/smikes))
  * [`aa79194`](https://github.com/npm/npm/commit/aa791942a9f3c8af6a650edec72a675deb7a7c6e)
    [#6565](https://github.com/npm/npm/issues/6565) Tweak `peerDependency`
    deprecation warning to include which peer dependency on which package is
    going to need to change. ([@othiym23](https://github.com/othiym23))
  * [`5fa067f`](https://github.com/npm/npm/commit/5fa067fd47682ac3cdb12a2b009d8ca59b05f992)
    [#7171](https://github.com/npm/npm/issues/7171) Tweak `engineStrict`
    deprecation warning to include which `package.json` is using it.
    ([@othiym23](https://github.com/othiym23))
* Add new collaborators:
  - Robert Kowalski ([@robertkowalski](https://github.com/robertkowalski))
  - Christian Vaagland Tellnes ([@tellnes](https://github.com/tellnes))
  - Brian White ([@mscdex](https://github.com/mscdex))

### Known issues

* Windows support has some outstanding failures that have not been properly picked up by the io.js CI system due to a combination of factors including human, program and Jenkins errors. See [#1005](https://github.com/nodejs/node/issues/1005) for details & discussion. Expect these problems to be addressed ASAP.
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* Not possible to build io.js as a static library [#686](https://github.com/nodejs/node/issues/686)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)

### Commits

* [[`25da0742ee`](https://github.com/nodejs/node/commit/25da0742ee)] - **build**: improve vcbuild.bat (Bert Belder) [#998](https://github.com/nodejs/node/pull/998)
* [[`b8310cbd3e`](https://github.com/nodejs/node/commit/b8310cbd3e)] - **build**: reduce tarball size by 8-10% (Johan Bergström) [#961](https://github.com/nodejs/node/pull/961)
* [[`58a612ea9d`](https://github.com/nodejs/node/commit/58a612ea9d)] - **deps**: make node-gyp work with io.js (cjihrig) [#990](https://github.com/nodejs/node/pull/990)
* [[`2a2fe5c4f2`](https://github.com/nodejs/node/commit/2a2fe5c4f2)] - **deps**: upgrade npm to 2.6.1 (Forrest L Norvell) [#990](https://github.com/nodejs/node/pull/990)
* [[`84ee2722a3`](https://github.com/nodejs/node/commit/84ee2722a3)] - **doc**: minor formatting fixes. (Tim Oxley) [#996](https://github.com/nodejs/node/pull/996)
* [[`cf0306cd71`](https://github.com/nodejs/node/commit/cf0306cd71)] - **doc**: update stability index (Chris Dickinson) [#943](https://github.com/nodejs/node/pull/943)
* [[`fb2439a699`](https://github.com/nodejs/node/commit/fb2439a699)] - **doc**: add robertkowalski as collaborator (Robert Kowalski) [#977](https://github.com/nodejs/node/pull/977)
* [[`f83d380647`](https://github.com/nodejs/node/commit/f83d380647)] - **doc**: update os.markdown (Benjamin Gruenbaum) [#976](https://github.com/nodejs/node/pull/976)
* [[`ae7a23351f`](https://github.com/nodejs/node/commit/ae7a23351f)] - **doc**: add roadmap, i18n, tracing, evangelism WGs (Mikeal Rogers) [#911](https://github.com/nodejs/node/pull/911)
* [[`14174a95a5`](https://github.com/nodejs/node/commit/14174a95a5)] - **doc**: document roadmap, workgroups (Mikeal Rogers)
* [[`865ee313cf`](https://github.com/nodejs/node/commit/865ee313cf)] - **doc**: add tellnes as collaborator (Christian Tellnes) [#973](https://github.com/nodejs/node/pull/973)
* [[`01296923db`](https://github.com/nodejs/node/commit/01296923db)] - **doc**: add mscdex as collaborator (Brian White) [#972](https://github.com/nodejs/node/pull/972)
* [[`675cffb33e`](https://github.com/nodejs/node/commit/675cffb33e)] - **http**: don't confuse automatic headers for others (Christian Tellnes) [#828](https://github.com/nodejs/node/pull/828)
* [[`7887e119ed`](https://github.com/nodejs/node/commit/7887e119ed)] - **install**: new performance counters provider guid (Russell Dempsey)
* [[`4d1fa2ca97`](https://github.com/nodejs/node/commit/4d1fa2ca97)] - **src**: add check for already defined macro NOMINMAX (Pavel Medvedev) [#986](https://github.com/nodejs/node/pull/986)
* [[`1ab7e80838`](https://github.com/nodejs/node/commit/1ab7e80838)] - **tls**: proxy `handle.reading` back to parent handle (Fedor Indutny) [#1004](https://github.com/nodejs/node/pull/1004)
* [[`755461219d`](https://github.com/nodejs/node/commit/755461219d)] - **tls**: fix typo `handle._reading` => `handle.reading` (Fedor Indutny) [#994](https://github.com/nodejs/node/pull/994)

## 2015-02-26, Version 1.4.1, @rvagg

_Note: version **1.4.0** was tagged and built but not released. A libuv bug was discovered in the process so the release was aborted. The tag was straight after [`a558cd0a61`](https://github.com/nodejs/node/commit/a558cd0a61) but has since been removed. We have jumped to 1.4.1 to avoid confusion._

### Notable changes

* **process** / **promises**: An `'unhandledRejection'` event is now emitted on `process` whenever a `Promise` is rejected and no error handler is attached to the `Promise` within a turn of the event loop. A `'rejectionHandled'` event is now emitted whenever a `Promise` was rejected and an error handler was attached to it later than after an event loop turn. See the [process](https://iojs.org/api/process.html) documentation for more detail. [#758](https://github.com/nodejs/node/pull/758) (Petka Antonov)
* **streams**: you can now use regular streams as an underlying socket for `tls.connect()` [#926](https://github.com/nodejs/node/pull/926) (Fedor Indutny)
* **http**: A new `'abort'` event emitted when a `http.ClientRequest` is aborted by the client. [#945](https://github.com/nodejs/node/pull/945) (Evan Lucas)
* **V8**: Upgrade V8 to 4.1.0.21. Includes an embargoed fix, details should be available at https://code.google.com/p/chromium/issues/detail?id=430201 when embargo is lifted. A breaking ABI change has been held back from this upgrade, possibly to be included when io.js merges V8 4.2. See [#952](https://github.com/nodejs/node/pull/952) for discussion.
* **npm**: Upgrade npm to 2.6.0. Includes features to support the new registry and to prepare for `npm@3`. See [npm CHANGELOG.md](https://github.com/npm/npm/blob/master/CHANGELOG.md#v260-2015-02-12) for details. Summary:
  * [`38c4825`](https://github.com/npm/npm/commit/38c48254d3d217b4babf5027cb39492be4052fc2) [#5068](https://github.com/npm/npm/issues/5068) Add new logout command, and make it do something useful on both bearer-based and basic-based authed clients. ([@othiym23](https://github.com/othiym23))
  * [`c8e08e6`](https://github.com/npm/npm/commit/c8e08e6d91f4016c80f572aac5a2080df0f78098) [#6565](https://github.com/npm/npm/issues/6565) Warn that `peerDependency` behavior is changing and add a note to the docs. ([@othiym23](https://github.com/othiym23))
  * [`7c81a5f`](https://github.com/npm/npm/commit/7c81a5f5f058941f635a92f22641ea68e79b60db) [#7171](https://github.com/npm/npm/issues/7171) Warn that `engineStrict` in `package.json` will be going away in the next major version of npm (coming soon!) ([@othiym23](https://github.com/othiym23))
* **libuv**: Upgrade to 1.4.2. See [libuv ChangeLog](https://github.com/libuv/libuv/blob/v1.x/ChangeLog) for details of fixes.

### Known issues

* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* Not possible to build io.js as a static library [#686](https://github.com/nodejs/node/issues/686)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)

### Commits

* [[`8a1e22af3a`](https://github.com/nodejs/node/commit/8a1e22af3a)] - **benchmark**: pass execArgv to the benchmarking process (Petka Antonov) [#928](https://github.com/nodejs/node/pull/928)
* [[`234e6916b8`](https://github.com/nodejs/node/commit/234e6916b8)] - **build**: Fix incorrect reference (Johan Bergström) [#924](https://github.com/nodejs/node/pull/924)
* [[`e00c938d24`](https://github.com/nodejs/node/commit/e00c938d24)] - **build**: make test-ci output TAP to stdout and log (Rod Vagg) [#938](https://github.com/nodejs/node/pull/938)
* [[`b2a0d8f65e`](https://github.com/nodejs/node/commit/b2a0d8f65e)] - **deps**: update libuv to 1.4.2 (Ben Noordhuis) [#966](https://github.com/nodejs/node/pull/966)
* [[`a558cd0a61`](https://github.com/nodejs/node/commit/a558cd0a61)] - **deps**: revert v8 abi change (Ben Noordhuis) [#952](https://github.com/nodejs/node/pull/952)
* [[`54532a9761`](https://github.com/nodejs/node/commit/54532a9761)] - **deps**: fix postmortem support in v8 (Fedor Indutny) [#706](https://github.com/nodejs/node/pull/706)
* [[`78f4837926`](https://github.com/nodejs/node/commit/78f4837926)] - **deps**: upgrade v8 to 4.1.0.21 (Ben Noordhuis) [#952](https://github.com/nodejs/node/pull/952)
* [[`739fda16a9`](https://github.com/nodejs/node/commit/739fda16a9)] - **deps**: update libuv to 1.4.1 (Ben Noordhuis) [#940](https://github.com/nodejs/node/pull/940)
* [[`da730c76e9`](https://github.com/nodejs/node/commit/da730c76e9)] - **deps**: enable node-gyp iojs.lib download checksum (Ben Noordhuis) [#918](https://github.com/nodejs/node/pull/918)
* [[`97b424365a`](https://github.com/nodejs/node/commit/97b424365a)] - **deps**: make node-gyp work again on windows (Bert Belder)
* [[`19e3d5e10a`](https://github.com/nodejs/node/commit/19e3d5e10a)] - **deps**: make node-gyp fetch tarballs from iojs.org (Ben Noordhuis) [#343](https://github.com/nodejs/node/pull/343)
* [[`1e2fa1537f`](https://github.com/nodejs/node/commit/1e2fa1537f)] - **deps**: upgrade npm to 2.6.0 (Forrest L Norvell) [#904](https://github.com/nodejs/node/pull/904)
* [[`2e2cf81476`](https://github.com/nodejs/node/commit/2e2cf81476)] - **doc**: fix process.stdout reference to console.log (Brendan Ashworth) [#964](https://github.com/nodejs/node/pull/964)
* [[`2e63bad7eb`](https://github.com/nodejs/node/commit/2e63bad7eb)] - **doc**: link & formatting of SHAs in commit list (Tim Oxley) [#967](https://github.com/nodejs/node/pull/967)
* [[`c5050d8e4d`](https://github.com/nodejs/node/commit/c5050d8e4d)] - **doc**: fix 'dhparam' description of tls.createServer (silverwind) [#968](https://github.com/nodejs/node/pull/968)
* [[`06ee782f24`](https://github.com/nodejs/node/commit/06ee782f24)] - **doc**: document 'unhandledRejection' and 'rejectionHandled' (Benjamin Gruenbaum) [#946](https://github.com/nodejs/node/pull/946)
* [[`b65dade102`](https://github.com/nodejs/node/commit/b65dade102)] - **doc**: update documentation.markdown for io.js. (Ryan Scheel) [#950](https://github.com/nodejs/node/pull/950)
* [[`87e4bfd582`](https://github.com/nodejs/node/commit/87e4bfd582)] - **doc**: link cluster worker.send() to child.send() (Sam Roberts) [#839](https://github.com/nodejs/node/pull/839)
* [[`cb22bc9b8a`](https://github.com/nodejs/node/commit/cb22bc9b8a)] - **doc**: fix footer sizing (Jeremiah Senkpiel) [#860](https://github.com/nodejs/node/pull/860)
* [[`3ab9b92e90`](https://github.com/nodejs/node/commit/3ab9b92e90)] - **doc**: fix stream `_writev` header size (René Kooi) [#916](https://github.com/nodejs/node/pull/916)
* [[`4fcbb8aaaf`](https://github.com/nodejs/node/commit/4fcbb8aaaf)] - **doc**: use HTTPS URL for the API documentation page (Shinnosuke Watanabe) [#913](https://github.com/nodejs/node/pull/913)
* [[`329f364ea2`](https://github.com/nodejs/node/commit/329f364ea2)] - **doc**: fix PR reference in CHANGELOG (Brian White) [#903](https://github.com/nodejs/node/pull/903)
* [[`0ac57317aa`](https://github.com/nodejs/node/commit/0ac57317aa)] - **doc**: fix typo, rephrase cipher change in CHANGELOG (Rod Vagg) [#902](https://github.com/nodejs/node/pull/902)
* [[`1f40b2a636`](https://github.com/nodejs/node/commit/1f40b2a636)] - **fs**: add type checking to makeCallback() (cjihrig) [#866](https://github.com/nodejs/node/pull/866)
* [[`c82e580a50`](https://github.com/nodejs/node/commit/c82e580a50)] - **fs**: properly handle fd passed to truncate() (Bruno Jouhier) [joyent/node#9161](https://github.com/joyent/node/pull/9161)
* [[`2ca22aacbd`](https://github.com/nodejs/node/commit/2ca22aacbd)] - **(SEMVER-MINOR) http**: emit abort event from ClientRequest (Evan Lucas) [#945](https://github.com/nodejs/node/pull/945)
* [[`d8eb974a98`](https://github.com/nodejs/node/commit/d8eb974a98)] - **net**: make Server.prototype.unref() persistent (cjihrig) [#897](https://github.com/nodejs/node/pull/897)
* [[`872702d9b7`](https://github.com/nodejs/node/commit/872702d9b7)] - **(SEMVER-MINOR) node**: implement unhandled rejection tracking (Petka Antonov) [#758](https://github.com/nodejs/node/pull/758)
* [[`b41dbc2737`](https://github.com/nodejs/node/commit/b41dbc2737)] - **readline**: use native `codePointAt` (Vladimir Kurchatkin) [#825](https://github.com/nodejs/node/pull/825)
* [[`26ebe9805e`](https://github.com/nodejs/node/commit/26ebe9805e)] - **smalloc**: extend user API (Trevor Norris) [#905](https://github.com/nodejs/node/pull/905)
* [[`e435a0114d`](https://github.com/nodejs/node/commit/e435a0114d)] - **src**: fix intermittent SIGSEGV in resolveTxt (Evan Lucas) [#960](https://github.com/nodejs/node/pull/960)
* [[`0af4c9ea74`](https://github.com/nodejs/node/commit/0af4c9ea74)] - **src**: fix domains + --abort-on-uncaught-exception (Chris Dickinson) [#922](https://github.com/nodejs/node/pull/922)
* [[`89e133a1d8`](https://github.com/nodejs/node/commit/89e133a1d8)] - **stream_base**: remove static JSMethod declarations (Fedor Indutny) [#957](https://github.com/nodejs/node/pull/957)
* [[`b9686233fc`](https://github.com/nodejs/node/commit/b9686233fc)] - **stream_base**: introduce StreamBase (Fedor Indutny) [#840](https://github.com/nodejs/node/pull/840)
* [[`1738c77835`](https://github.com/nodejs/node/commit/1738c77835)] - **(SEMVER-MINOR) streams**: introduce StreamWrap and JSStream (Fedor Indutny) [#926](https://github.com/nodejs/node/pull/926)
* [[`506c7fd40b`](https://github.com/nodejs/node/commit/506c7fd40b)] - **test**: fix infinite spawn cycle in stdio test (Ben Noordhuis) [#948](https://github.com/nodejs/node/pull/948)
* [[`a7bdce249c`](https://github.com/nodejs/node/commit/a7bdce249c)] - **test**: support writing test output to file (Johan Bergström) [#934](https://github.com/nodejs/node/pull/934)
* [[`0df54303c1`](https://github.com/nodejs/node/commit/0df54303c1)] - **test**: common.js -> common (Brendan Ashworth) [#917](https://github.com/nodejs/node/pull/917)
* [[`ed3b057e9f`](https://github.com/nodejs/node/commit/ed3b057e9f)] - **util**: handle symbols properly in format() (cjihrig) [#931](https://github.com/nodejs/node/pull/931)


## 2015-02-20, Version 1.3.0, @rvagg

### Notable changes

* **url**: `url.resolve('/path/to/file', '.')` now returns `/path/to/` with the trailing slash, `url.resolve('/', '.')` returns `/`. [#278](https://github.com/nodejs/node/issues/278) (Amir Saboury)
* **tls**: The default cipher suite used by `tls` and `https` has been changed to one that achieves Perfect Forward Secrecy with all modern browsers. Additionally, insecure RC4 ciphers have been excluded. If you absolutely require RC4, please specify your own cipher suites. [#826](https://github.com/nodejs/node/issues/826) (Roman Reiss)

### Known issues

* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* Not possible to build io.js as a static library [#686](https://github.com/nodejs/node/issues/686)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)

### Commits

* [[`35ed79932c`](https://github.com/nodejs/node/commit/35ed79932c)] - **benchmark**: add a few querystring benchmarks (Brian White) [#846](https://github.com/nodejs/node/pull/846)
* [[`c6fd2c5e95`](https://github.com/nodejs/node/commit/c6fd2c5e95)] - **buffer**: fix pool offset adjustment (Trevor Norris)
* [[`36a779560a`](https://github.com/nodejs/node/commit/36a779560a)] - **buffer**: expose internals on binding (Vladimir Kurchatkin) [#770](https://github.com/nodejs/node/pull/770)
* [[`e63b51793b`](https://github.com/nodejs/node/commit/e63b51793b)] - **crypto**: fix to check ext method for shared lib (Shigeki Ohtsu) [#800](https://github.com/nodejs/node/pull/800)
* [[`afdef70fcc`](https://github.com/nodejs/node/commit/afdef70fcc)] - **doc**: update AUTHORS list (Rod Vagg) [#900](https://github.com/nodejs/node/pull/900)
* [[`1bf91878e7`](https://github.com/nodejs/node/commit/1bf91878e7)] - **doc**: add TC meeting 2015-02-04 minutes (Rod Vagg) [#876](https://github.com/nodejs/node/pull/876)
* [[`9e05c8d2fc`](https://github.com/nodejs/node/commit/9e05c8d2fc)] - **doc**: remove outdated language on consensus (Emily Rose)
* [[`ed240f44f7`](https://github.com/nodejs/node/commit/ed240f44f7)] - **doc**: document 'ciphers' option of tls.connect (Roman Reiss) [#845](https://github.com/nodejs/node/pull/845)
* [[`0555b3c785`](https://github.com/nodejs/node/commit/0555b3c785)] - **doc**: fix typo miliseconds -> milliseconds (jigsaw) [#865](https://github.com/nodejs/node/pull/865)
* [[`fc6507dd4e`](https://github.com/nodejs/node/commit/fc6507dd4e)] - **doc**: add comma in README to increase clarity (Jimmy Hsu)
* [[`f0296933f8`](https://github.com/nodejs/node/commit/f0296933f8)] - **doc**: correct `it's` to `its` in process (Charmander) [#837](https://github.com/nodejs/node/pull/837)
* [[`e81731ad18`](https://github.com/nodejs/node/commit/e81731ad18)] - **doc**: add geek as collaborator (Wyatt Preul) [#835](https://github.com/nodejs/node/pull/835)
* [[`4ca7cca84a`](https://github.com/nodejs/node/commit/4ca7cca84a)] - **doc**: grammar fix in smalloc (Debjeet Biswas) [joyent/node#9164](https://github.com/joyent/node/pull/9164)
* [[`30dca66958`](https://github.com/nodejs/node/commit/30dca66958)] - **doc**: fix code syntax (Dan Dascalescu) [joyent/node#9198](https://github.com/joyent/node/pull/9198)
* [[`8c1df7a8a8`](https://github.com/nodejs/node/commit/8c1df7a8a8)] - **doc**: use correct signature for assert() (Andrei Sedoi) [joyent/node#9003](https://github.com/joyent/node/pull/9003)
* [[`ba40942ad2`](https://github.com/nodejs/node/commit/ba40942ad2)] - **doc**: fix sentence grammar timers.markdown (Omer Wazir) [#815](https://github.com/nodejs/node/pull/815)
* [[`789ff959be`](https://github.com/nodejs/node/commit/789ff959be)] - **doc**: increase mark class contrast ratio (Omer Wazir) [#824](https://github.com/nodejs/node/pull/824)
* [[`122a1758d1`](https://github.com/nodejs/node/commit/122a1758d1)] - **doc**: better font-smoothing for firefox (Jeremiah Senkpiel) [#820](https://github.com/nodejs/node/pull/820)
* [[`982b143ab3`](https://github.com/nodejs/node/commit/982b143ab3)] - **doc**: disable font ligatures (Roman Reiss) [#816](https://github.com/nodejs/node/pull/816)
* [[`cb5560bd62`](https://github.com/nodejs/node/commit/cb5560bd62)] - **doc**: Close code span correctly (Omer Wazir) [#814](https://github.com/nodejs/node/pull/814)
* [[`c3c2fbdf83`](https://github.com/nodejs/node/commit/c3c2fbdf83)] - **doc**: change effect to affect in errors.md (Ryan Seys) [#799](https://github.com/nodejs/node/pull/799)
* [[`b620129715`](https://github.com/nodejs/node/commit/b620129715)] - **doc**: add sam-github as collaborator (Sam Roberts) [#791](https://github.com/nodejs/node/pull/791)
* [[`e80f803298`](https://github.com/nodejs/node/commit/e80f803298)] - **doc**: remove Caine section from contributing guide (Michaël Zasso) [#804](https://github.com/nodejs/node/pull/804)
* [[`400d6e56f9`](https://github.com/nodejs/node/commit/400d6e56f9)] - **doc**: fix libuv link (Yosuke Furukawa) [#803](https://github.com/nodejs/node/pull/803)
* [[`15d156e3ec`](https://github.com/nodejs/node/commit/15d156e3ec)] - **doc**: fix wording in fs.appendFile (Rudolf Meijering) [#801](https://github.com/nodejs/node/pull/801)
* [[`dbf75924f1`](https://github.com/nodejs/node/commit/dbf75924f1)] - **doc**: update error links (Chris Dickinson) [#793](https://github.com/nodejs/node/pull/793)
* [[`7061669dba`](https://github.com/nodejs/node/commit/7061669dba)] - **events**: optimize adding and removing of listeners (Brian White) [#785](https://github.com/nodejs/node/pull/785)
* [[`630f636334`](https://github.com/nodejs/node/commit/630f636334)] - **events**: move slow path to separate function too (Brian White) [#785](https://github.com/nodejs/node/pull/785)
* [[`ecef87177a`](https://github.com/nodejs/node/commit/ecef87177a)] - **fs**: ensure nullCheck() callback is a function (cjihrig) [#887](https://github.com/nodejs/node/pull/887)
* [[`6a2b204bbc`](https://github.com/nodejs/node/commit/6a2b204bbc)] - **module**: replace NativeModule.require (Herbert Vojčík) [joyent/node#9201](https://github.com/joyent/node/pull/9201)
* [[`9b6b05556f`](https://github.com/nodejs/node/commit/9b6b05556f)] - **net**: unref timer in parent sockets (Fedor Indutny) [#891](https://github.com/nodejs/node/pull/891)
* [[`cca8de6709`](https://github.com/nodejs/node/commit/cca8de6709)] - **net**: remove use of arguments in Server constructor (cjihrig)
* [[`0cff0521c3`](https://github.com/nodejs/node/commit/0cff0521c3)] - **net**: throw on invalid socket timeouts (cjihrig) [joyent/node#8884](https://github.com/joyent/node/pull/8884)
* [[`b5f25a963c`](https://github.com/nodejs/node/commit/b5f25a963c)] - **src**: ensure that file descriptors 0-2 are valid (Ben Noordhuis) [#875](https://github.com/nodejs/node/pull/875)
* [[`a956791f69`](https://github.com/nodejs/node/commit/a956791f69)] - **src**: fix typo in error message (Ben Noordhuis) [#875](https://github.com/nodejs/node/pull/875)
* [[`fb28c91074`](https://github.com/nodejs/node/commit/fb28c91074)] - **src**: fix add-on builds, partially revert 8aed9d66 (Ben Noordhuis) [#868](https://github.com/nodejs/node/pull/868)
* [[`4bb3184d8d`](https://github.com/nodejs/node/commit/4bb3184d8d)] - **src**: reduce AsyncWrap memory footprint (Ben Noordhuis) [#667](https://github.com/nodejs/node/pull/667)
* [[`7e779b4593`](https://github.com/nodejs/node/commit/7e779b4593)] - **src**: remove obsoleted queue.h header (Ben Noordhuis) [#667](https://github.com/nodejs/node/pull/667)
* [[`38dc0cd8f4`](https://github.com/nodejs/node/commit/38dc0cd8f4)] - **src**: switch from QUEUE to intrusive list (Ben Noordhuis) [#667](https://github.com/nodejs/node/pull/667)
* [[`58eb00c693`](https://github.com/nodejs/node/commit/58eb00c693)] - **src**: add typesafe intrusive list (Ben Noordhuis) [#667](https://github.com/nodejs/node/pull/667)
* [[`8aed9d6610`](https://github.com/nodejs/node/commit/8aed9d6610)] - **src**: cleanup `Isolate::GetCurrent()` (Vladimir Kurchatkin) [#807](https://github.com/nodejs/node/pull/807)
* [[`7c22372303`](https://github.com/nodejs/node/commit/7c22372303)] - **src**: remove trailing whitespace (Vladimir Kurchatkin) [#798](https://github.com/nodejs/node/pull/798)
* [[`20f8e7f17a`](https://github.com/nodejs/node/commit/20f8e7f17a)] - **test**: remove flaky test functionality (Rod Vagg) [#812](https://github.com/nodejs/node/pull/812)
* [[`30e340ad9d`](https://github.com/nodejs/node/commit/30e340ad9d)] - **test**: fix parallel/test-tls-getcipher (Roman Reiss) [#853](https://github.com/nodejs/node/pull/853)
* [[`d53b636d94`](https://github.com/nodejs/node/commit/d53b636d94)] - **test**: verify fields in spawn{Sync} errors (cjihrig) [#838](https://github.com/nodejs/node/pull/838)
* [[`3b1b4de903`](https://github.com/nodejs/node/commit/3b1b4de903)] - **test**: Timeout#unref() does not return instance (Jan Schär) [joyent/node#9171](https://github.com/joyent/node/pull/9171)
* [[`becb4e980e`](https://github.com/nodejs/node/commit/becb4e980e)] - **test**: distribute crypto tests into separate files (Brendan Ashworth) [#827](https://github.com/nodejs/node/pull/827)
* [[`77f35861d0`](https://github.com/nodejs/node/commit/77f35861d0)] - **(SEMVER-MINOR) tls**: more secure defaults (Roman Reiss) [#826](https://github.com/nodejs/node/pull/826)
* [[`faa687b4be`](https://github.com/nodejs/node/commit/faa687b4be)] - **url**: reslove urls with . and .. (Amir Saboury) [#278](https://github.com/nodejs/node/pull/278)

## 2015-02-10, Version 1.2.0, @rvagg

### Notable changes

* **stream**:
  - Simpler stream construction, see [readable-stream/issues#102](https://github.com/nodejs/readable-stream/issues/102) for details. This extends the streams base objects to make their constructors accept default implementation methods, reducing the boilerplate required to implement custom streams. An updated version of readable-stream will eventually be released to match this change in core. (@sonewman)
* **dns**:
  - `lookup()` now supports an `'all'` boolean option, default to `false` but when turned on will cause the method to return an array of *all* resolved names for an address, see, [#744](https://github.com/nodejs/node/pull/744) (@silverwind)
* **assert**:
  - Remove `prototype` property comparison in `deepEqual()`, considered a bugfix, see [#636](https://github.com/nodejs/node/pull/636) (@vkurchatkin)
  - Introduce a `deepStrictEqual()` method to mirror `deepEqual()` but performs strict equality checks on primitives, see [#639](https://github.com/nodejs/node/pull/639) (@vkurchatkin)
* **tracing**:
  - Add [LTTng](http://lttng.org/) (Linux Trace Toolkit Next Generation) when compiled with the  `--with-lttng` option. Trace points match those available for DTrace and ETW. [#702](https://github.com/nodejs/node/pull/702) (@thekemkid)
* **docs**:
  - Lots of doc updates, see individual commits
  - New **Errors** page discussing JavaScript errors, V8 specifics, and io.js specific error details. (@chrisdickinson)
* **npm** upgrade to 2.5.1, short changelog:
  - [npm/0e8d473](https://github.com/npm/npm/commit/0e8d4736a1cbdda41ae8eba8a02c7ff7ce80c2ff) [#7281](https://github.com/npm/npm/issues/7281) `npm-registry-mock@1.0.0`: Clean up API, set `connection: close`, which makes tests pass on io.js 1.1.x.
  ([@robertkowalski](https://github.com/robertkowalski))
  - [npm/f9313a0](https://github.com/npm/npm/commit/f9313a066c9889a0ee898d8a35676e40b8101e7f)
  [#7226](https://github.com/npm/npm/issues/7226) Ensure that all request
  settings are copied onto the agent.
  ([@othiym23](https://github.com/othiym23))
   - [npm/fec4c96](https://github.com/npm/npm/commit/fec4c967ee235030bf31393e8605e9e2811f4a39)
  Allow `--no-proxy` to override `HTTP_PROXY` setting in environment.
  ([@othiym23](https://github.com/othiym23))
  - [npm/9d61e96](https://github.com/npm/npm/commit/9d61e96fb1f48687a85c211e4e0cd44c7f95a38e)
  `npm outdated --long` now includes a column showing the type of dependency.
  ([@watilde](https://github.com/watilde))
* **libuv** upgrade to 1.4.0, see [libuv ChangeLog](https://github.com/libuv/libuv/blob/v1.x/ChangeLog)
* Add new collaborators:
  - Aleksey Smolenchuk (@lxe)
  - Shigeki Ohtsu (@shigeki)

### Known issues

* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* Not possible to build io.js as a static library [#686](https://github.com/nodejs/node/issues/686)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774) that should appear in the next patch release.

### Commits

* [[`7e2235a`](https://github.com/nodejs/node/commit/7e2235aebb067b84974e001f9fa4d83f45b7c9dd)] - doc: add error documentation (Chris Dickinson)
* [[`d832be4`](https://github.com/nodejs/node/commit/d832be4aa3c68c29017339a65593f62cb73179bc)] - doc: update AUTHORS list (Rod Vagg)
* [[`aea9b89`](https://github.com/nodejs/node/commit/aea9b89b5c2e3fb9fdbd96c7483eb1f60d09a39e)] - doc: add shigeki as collaborator (Shigeki Ohtsu)
* [[`e653080`](https://github.com/nodejs/node/commit/e65308053c871352be948b9001737df01aad1965)] - fs: improve `readFile` performance (Vladimir Kurchatkin)
* [[`9681fca`](https://github.com/nodejs/node/commit/9681fcacf0fd477f999a52f6ff4151d4125d49d0)] - deps: update libuv to 1.4.0 (Saúl Ibarra Corretgé)
* [[`5e825d1`](https://github.com/nodejs/node/commit/5e825d1073b57a87fc9a77751ed3e21c86970082)] - tracing: add lttng support for tracing on linux (Glen Keane)
* [[`b677b84`](https://github.com/nodejs/node/commit/b677b844fc1de328a0f2b0151bdfc045cb5d0c81)] - events: optimize various functions (Brian White)
* [[`c86e383`](https://github.com/nodejs/node/commit/c86e383c41f35b17ba79cc1c6dbfff674214177d)] - test: fix test failure with shared openssl (Shigeki Ohtsu)
* [[`1151016`](https://github.com/nodejs/node/commit/1151016d0a13dcb5973f602d0717c2da6abca551)] - doc: fix typo in crypto (Haoliang Gao)
* [[`7c56868`](https://github.com/nodejs/node/commit/7c568684b834a6a3c4d15bb29d2f95cf76773cb8)] - doc: change the order of crypto.publicDecrypt (Haoliang Gao)
* [[`3f473ef`](https://github.com/nodejs/node/commit/3f473ef141fdc7059928ebc4542b00e2f126ab07)] - assert: introduce `deepStrictEqual` (Vladimir Kurchatkin)
* [[`828d19a`](https://github.com/nodejs/node/commit/828d19a1f696840acf43b70125b85b0d61ff5056)] - doc: fix dns.lookup options example (Roman Reiss)
* [[`90d2b35`](https://github.com/nodejs/node/commit/90d2b352810bc352620e61e0dacc8573faf11dfb)] - doc: update antiquated process.versions output (Ben Noordhuis)
* [[`789bbb9`](https://github.com/nodejs/node/commit/789bbb91d3eb30fa2a51e9b064592d6a461a6fe5)] - doc: update node.js references in api docs (Ben Noordhuis)
* [[`c22e5ac`](https://github.com/nodejs/node/commit/c22e5ace846f93b4531a39b0e055f89a46598f63)] - https: simpler argument check (Michaël Zasso)
* [[`b9d3928`](https://github.com/nodejs/node/commit/b9d3928f80992a812795a974cbae02288fc5049c)] - util: simplify `isPrimitive` (Vladimir Kurchatkin)
* [[`2c3121c`](https://github.com/nodejs/node/commit/2c3121c606967f8595d671601493e623a7157385)] - benchmark: bump eventemitter number of iterations (Ben Noordhuis)
* [[`633a990`](https://github.com/nodejs/node/commit/633a9908487efadda6a86026a36d5325a28805c6)] - dns: allow dns.lookup() to return all addresses (Roman Reiss)
* [[`1cd1d7a`](https://github.com/nodejs/node/commit/1cd1d7a182c2d16c28c778ddcd72bbeac6bc5c75)] - buffer: don't compare same buffers (Vladimir Kurchatkin)
* [[`847b9d2`](https://github.com/nodejs/node/commit/847b9d212a404e5906ea9f366c458332c0318c53)] - benchmark: add more EventEmitter benchmarks (Brian White)
* [[`96597bc`](https://github.com/nodejs/node/commit/96597bc5927c57737c3bea943dd163d69ac76a96)] - doc: add lxe as collaborator (Aleksey Smolenchuk)
* [[`7a301e2`](https://github.com/nodejs/node/commit/7a301e29de1e4ab5f39165beb6d0b41435c221dd)] - deps: make node-gyp work again on windows (Bert Belder)
* [[`b188a34`](https://github.com/nodejs/node/commit/b188a3459d9d8a6d0c5fd391f1aefba281407083)] - deps: make node-gyp fetch tarballs from iojs.org (Ben Noordhuis)
* [[`af1bf49`](https://github.com/nodejs/node/commit/af1bf49852b7a8bcc9b9b6dd718edea0b18e3cb6)] - deps: upgrade npm to 2.5.1 (Forrest L Norvell)
* [[`9dc9ec3`](https://github.com/nodejs/node/commit/9dc9ec3ce6ba6f3dd4020e00f5863e207fa08a75)] - lib: make debug client connect to 127.0.0.1 (Ben Noordhuis)
* [[`e7573f9`](https://github.com/nodejs/node/commit/e7573f9111f6b85c599ec225714d76e08ec8a4dc)] - assert: don't compare object `prototype` property (Vladimir Kurchatkin)
* [[`8d11799`](https://github.com/nodejs/node/commit/8d1179952aefaa0086ff5540671cfd6ff612594b)] - asyncwrap: fix nullptr parent check (Trevor Norris)
* [[`62512bb`](https://github.com/nodejs/node/commit/62512bb29cd000dd5ce848258c10f3211f153bd5)] - test: accept EPROTONOSUPPORT ipv6 error (Ben Noordhuis)
* [[`05f4dff`](https://github.com/nodejs/node/commit/05f4dff97519ada5d3149a16ca9e5a04df948a61)] - asyncwrap: fix constructor condition for early ret (Trevor Norris)
* [[`10277d2`](https://github.com/nodejs/node/commit/10277d2e57ee7fe9e0e3f63f10b9ea521e86e7f0)] - docs: include mention of new crypto methods (Calvin Metcalf)
* [[`9a8f186`](https://github.com/nodejs/node/commit/9a8f18613da4956c963377e2ad55cdd3dabc32aa)] - child_process: add debug and error details (Zach Bruggeman)
* [[`6f7a978`](https://github.com/nodejs/node/commit/6f7a9784eaef82a1aa6cf53bbbd7224c446876a0)] - crypto: clear error on return in TLS methods (Fedor Indutny)
* [[`50daee7`](https://github.com/nodejs/node/commit/50daee7243a3f987e1a28d93c43f913471d6885a)] - stream: simpler stream construction (Sam Newman)
* [[`e0730ee`](https://github.com/nodejs/node/commit/e0730eeaa5231841a7eba080c8170e41278c3c52)] - benchmark: allow compare via fine-grained filters (Brian White)
* [[`96ffcb9`](https://github.com/nodejs/node/commit/96ffcb9a210a2fa1248ae5931290193573512a96)] - src: reduce cpu profiler overhead (Ben Noordhuis)
* [[`3e675e4`](https://github.com/nodejs/node/commit/3e675e44b59f1be8e5581de000f3cb17ef747c14)] - benchmark: don't use template strings (Evan Lucas)
* [[`8ac8b76`](https://github.com/nodejs/node/commit/8ac8b760ac74e5a6938a49e563406716804672cb)] - doc: simplified pure consensus seeking (Mikeal Rogers)
* [[`0a54b6a`](https://github.com/nodejs/node/commit/0a54b6a134a6815e30d1f78f8c8612d4a00399ad)] - doc: update streams wg charter (Chris Dickinson)
* [[`b8ead4a`](https://github.com/nodejs/node/commit/b8ead4a23f8b0717204878235d61cfce3f3fdd30)] - Adjusting for feedback in the PR. (Mikeal Rogers)
* [[`3af7f30`](https://github.com/nodejs/node/commit/3af7f30a7cceb1e418e5cd26c65a8ec5cc589d09)] - Initial documentation for working groups. (Mikeal Rogers)
* [[`513724e`](https://github.com/nodejs/node/commit/513724efcc42ed150391915050fe60402f8dd48d)] - doc: add GPG fingerprint for chrisdickinson (Chris Dickinson)
* [[`4168198`](https://github.com/nodejs/node/commit/41681983921d323da79b6d45e4ae0f8edb541e18)] - doc: add TC meeting 2015-01-28 minutes (Rod Vagg)

## 2015-02-06, Version 0.12.0 (Stable)

### Commits

* [[`087a7519ce`](https://github.com/nodejs/node/commit/087a7519ce)] - **npm**: Upgrade to 2.5.1
* [[`4312f8d760`](https://github.com/nodejs/node/commit/4312f8d760)] - **mdb_v8**: update for v0.12 (Dave Pacheco)

## 2015-02-03, Version 1.1.0, @chrisdickinson

### Notable changes

* debug: fix v8 post-mortem debugging.
* crypto: publicEncrypt now supports password-protected private keys.
* crypto: ~30% speedup on hashing functions.
* crypto: added privateEncrypt/publicDecrypt functions.
* errors
  - better formatting via util.inspect
  - more descriptive errors from fs. This necessitated a `NODE_MODULE_VERSION` bump.
  - more descriptive errors from http.setHeader
* dep updates:
  - npm: upgrade to 2.4.1
  - http-parser: rollback to 2.3.0
  - libuv: update to 1.3.0
  - v8: update to 4.1.0.14
* http.request: inherited properties on options are now respected
* add iterable interface to buffers (`for (let byte of buffer.values()) { }`)
* fs: fix fd leak on `fs.createReadStream`. See 497fd72 for details.
* installer: on Windows, emit WM_SETTINGCHANGE after install to make other running
  processes aware of the PATH changes.
* Added new collaborators:
  - Vladimir Kurchatkin (@vkurchatkin)
  - Micleușanu Nicu (@micnic)

### Known issues

* Surrogate pair in REPL can freeze terminal (https://github.com/nodejs/node/issues/690)
* Not possible to build io.js as a static library (https://github.com/nodejs/node/issues/686)

### Commits

* [[`df48faf`](https://github.com/nodejs/node/commit/df48fafa92c4ff0caee54c2f7fa214073cbd787e)] - tools: add release tool and docs, remove old tools (Rod Vagg)
* [[`14684d3`](https://github.com/nodejs/node/commit/14684d3d67ad7c04bec7b63377343dab3e389470)] - v8abbr: ASCIISTRINGTAG =&gt; ONEBYTESTRINGTAG (Fedor Indutny)
* [[`6a5d731`](https://github.com/nodejs/node/commit/6a5d731f602b547074f4367a7eb3964395080c94)] - gyp: enable postmortem support, fix dtrace paths (Fedor Indutny)
* [[`8b88ff8`](https://github.com/nodejs/node/commit/8b88ff85f106eed03bf677b9ab3b842f4edbdc6b)] - deps: fix postmortem support in v8 (Fedor Indutny)
* [[`d0b0bb4`](https://github.com/nodejs/node/commit/d0b0bb4ae00f596042bebe1ae61ae685bfbebf7d)] - dtrace: fix removal of unused probes (Glen Keane)
* [[`3e67d7e`](https://github.com/nodejs/node/commit/3e67d7e46b80c90faa360d1d0e44dacc444e8e4f)] - http: replace util._extend() with [].slice() (Jonathan Ong)
* [[`89dd8e0`](https://github.com/nodejs/node/commit/89dd8e062f462106a6f7d3e92e9d18906445f851)] - benchmark: clean up common.js (Brendan Ashworth)
* [[`6561274`](https://github.com/nodejs/node/commit/6561274d2377d9fd9c55fa3ce2eb2e53c14d898e)] - crypto: support passwords in publicEncrypt (Calvin Metcalf)
* [[`e9eb2ec`](https://github.com/nodejs/node/commit/e9eb2ec1c491e82dda27fe07d0eaf14ff569351b)] - process: fix regression in unlistening signals (Sam Roberts)
* [[`233e333`](https://github.com/nodejs/node/commit/233e333b183edeea6b740911061c7dc526078260)] - events: remove indeterminancy from event ordering (Sam Roberts)
* [[`d75fecf`](https://github.com/nodejs/node/commit/d75fecf6fd7a1ef9d3d84a70ab832e7c062f5880)] - src: remove unused dtrace probes (Glen Keane)
* [[`8c0742f`](https://github.com/nodejs/node/commit/8c0742f43759d35da99f2475f81a026c2818c66a)] - net: check close callback is function (Yosuke Furukawa)
* [[`207e48c`](https://github.com/nodejs/node/commit/207e48c93459da5e47f2efd408cfad6328bb0e25)] - dgram: check close callback is function (Yosuke Furukawa)
* [[`6ac8bdc`](https://github.com/nodejs/node/commit/6ac8bdc0aba5f60f4b4f2da5abd36d664062aa40)] - lib: reduce util.is*() usage (cjihrig)
* [[`bce7a26`](https://github.com/nodejs/node/commit/bce7a2608eb198eee6ecd7991062efd6daeeb440)] - deps: make node-gyp work again on windows (Bert Belder)
* [[`1bdd74d`](https://github.com/nodejs/node/commit/1bdd74d20a3c979d51929a1039824d90abca2cdb)] - deps: make node-gyp fetch tarballs from iojs.org (Ben Noordhuis)
* [[`faf34ff`](https://github.com/nodejs/node/commit/faf34ffbd321f4657bd99fb82931e1c9a4dda6af)] - deps: upgrade npm to 2.4.1 (Forrest L Norvell)
* [[`40e29dc`](https://github.com/nodejs/node/commit/40e29dcbbf33d919f5cc0cbab5fa65a282adb04b)] - assert: use util.inspect() to create error messages (cjihrig)
* [[`bc2c85c`](https://github.com/nodejs/node/commit/bc2c85ceef7ac034830e4a4357d0aef69cd6e386)] - fs: improve error messages (Bert Belder)
* [[`0767c2f`](https://github.com/nodejs/node/commit/0767c2feb1cb6921acd18be3392d331e093b2b4c)] - lib: fix max size check in Buffer constructor (Ben Noordhuis)
* [[`65b1e4f`](https://github.com/nodejs/node/commit/65b1e4f56f1f49dccd19b65dee2856df05b06c89)] - dgram: implicit binds should be exclusive (Sam Roberts)
* [[`083c421`](https://github.com/nodejs/node/commit/083c421b5ca08576897b5da396085a462010780e)] - benchmark: remove extra spacing in http options (Brendan Ashworth)
* [[`e17e6fb`](https://github.com/nodejs/node/commit/e17e6fb2faa04193eddf8062fbd49f1612b4dbff)] - util: use on-stack buffer for Utf8Value (Fedor Indutny)
* [[`3d4e96f`](https://github.com/nodejs/node/commit/3d4e96f3ceea1d30b4affb66133016a3c2811005)] - crypto: use on-stack storage in HashUpdate (Fedor Indutny)
* [[`aca2011`](https://github.com/nodejs/node/commit/aca20112519decef44474a2ee9936049e2a38b67)] - string_bytes: introduce InlineDecoder (Fedor Indutny)
* [[`c6367e7`](https://github.com/nodejs/node/commit/c6367e7f2a68b2418a98dfe9e829f17f62ba403a)] - node: speed up ParseEncoding (Fedor Indutny)
* [[`7604e6d`](https://github.com/nodejs/node/commit/7604e6decc441a1110567e98f20f7ee122179d54)] - docs: add note about default padding in crypto (Calvin Metcalf)
* [[`cf3e908`](https://github.com/nodejs/node/commit/cf3e908b70dfb345711cbca6c8e5373d085b05ea)] - http: more descriptive setHeader errors (Qasim Zaidi)
* [[`cbc1262`](https://github.com/nodejs/node/commit/cbc1262bd952a6c52937abe47a0af625965fba65)] - deps: upgrade v8 to 4.1.0.14 (Ben Noordhuis)
* [[`00f822f`](https://github.com/nodejs/node/commit/00f822f276c08465db3f6c70f154e9f28cc372d6)] - doc: add micnic as collaborator (Micleusanu Nicu)
* [[`514b1d9`](https://github.com/nodejs/node/commit/514b1d964b2e67d0594c6a44a22fbc29fe71454b)] - doc: add more info to benchmark/README.md (Fishrock123)
* [[`097fde7`](https://github.com/nodejs/node/commit/097fde7129a3acc660beb372cecd9daf1164a7f2)] - deps: update libuv to 1.3.0 (Saúl Ibarra Corretgé)
* [[`6ad236c`](https://github.com/nodejs/node/commit/6ad236c9b6a344a88ec2f1f173d5f920984b77b7)] - build: configure formatting, add final message (Roman Reiss)
* [[`dd47a8c`](https://github.com/nodejs/node/commit/dd47a8c78547db14ea0c7fc2f3375e8c9cb1a129)] - src: set default signal dispositions at start-up (Ben Noordhuis)
* [[`63ae1d2`](https://github.com/nodejs/node/commit/63ae1d203aba94b9a35400acdf00ff968fb6eb05)] - src: rework early debug signal handling (Ben Noordhuis)
* [[`5756f92`](https://github.com/nodejs/node/commit/5756f92f464fd0f2d04dd05bc30b350010885f74)] - src: do platform-specific initialization earlier (Ben Noordhuis)
* [[`24bd4e0`](https://github.com/nodejs/node/commit/24bd4e055562d8eb8a0d8db907c1715cc37e90b4)] - test: add http upgrade header regression test (Ben Noordhuis)
* [[`6605096`](https://github.com/nodejs/node/commit/660509694cfd4de59df0548eabbe18c97d75c63a)] - deps: roll back http_parser to 2.3.0 (Ben Noordhuis)
* [[`90ddb46`](https://github.com/nodejs/node/commit/90ddb46d522c37d2bc2eb68a6e0c9d52f9fbba42)] - crypto: remove use of this._readableState (Calvin Metcalf)
* [[`45d8d9f`](https://github.com/nodejs/node/commit/45d8d9f8262983d7d6434f4500b4e88b63052cd5)] - buffer: implement `iterable` interface (Vladimir Kurchatkin)
* [[`3cbb5cd`](https://github.com/nodejs/node/commit/3cbb5cdfdb621baec5dc3a2ac505be37f1718086)] - console: allow Object.prototype fields as labels (cjihrig)
* [[`87e62bd`](https://github.com/nodejs/node/commit/87e62bd4c87e8674e3d1c432506e9b4991784ee2)] - crypto: implement privateEncrypt/publicDecrypt (Fedor Indutny)
* [[`b50fea4`](https://github.com/nodejs/node/commit/b50fea4d490278b291321e6b96c49cf20bee1552)] - watchdog: fix timeout for early polling return (Saúl Ibarra Corretgé)
* [[`b5166cb`](https://github.com/nodejs/node/commit/b5166cb7d269cd1bf90d1768f82767b05b9ac1f8)] - benchmark: add bench-(url &amp; events) make targets (Yosuke Furukawa)
* [[`5843ae8`](https://github.com/nodejs/node/commit/5843ae8dfba5db83f2c04ed2db847049cbd2ab0d)] - Revert "doc: clarify fs.symlink and fs.symlinkSync parameters" (Bert Belder)
* [[`668bde8`](https://github.com/nodejs/node/commit/668bde8ac0d16382cbc98c904d8b5f55fd9fd9f0)] - win,msi: broadcast WM_SETTINGCHANGE after install (Mathias Küsel)
* [[`69ce064`](https://github.com/nodejs/node/commit/69ce0641dc6a84c90ffdd0906790cd945f1c3629)] - build: remove artifacts on distclean (Johan Bergström)
* [[`1953886`](https://github.com/nodejs/node/commit/1953886126a2ab3e7291a73767ee4302a391a208)] - test: fs.createReadStream().destroy() fd leak (Rod Vagg)
* [[`497fd72`](https://github.com/nodejs/node/commit/497fd72e21d2d1216e8457928d1a8082349fd0e5)] - fs: fix fd leak in ReadStream.destroy() (Alex Kocharin)
* [[`8b09ae7`](https://github.com/nodejs/node/commit/8b09ae76f1d854a0db579fc0737df4809ce6087d)] - doc: add links for http_parser/libuv upgrades (Michael Hart)
* [[`683e096`](https://github.com/nodejs/node/commit/683e09603e3418ed13333bac05876cb7d52453f5)] - src: remove excessive license boilerplate (Aleksey Smolenchuk)
* [[`5c7ab96`](https://github.com/nodejs/node/commit/5c7ab96b90d1ab35e03e32a249d50e7651dee6ef)] - doc: fix net.Server.listen bind behavior (Andres Suarez)
* [[`84b05d4`](https://github.com/nodejs/node/commit/84b05d48d943e5b5e88485be129755277bedd1cb)] - doc: update writable streams default encoding (Johnny Ray Austin)
* [[`1855267`](https://github.com/nodejs/node/commit/18552677d7e4468b093f28e721d1c02ce001b558)] - doc: fix minor grammar mistake in streams docs (ttrfwork)
* [[`4f68369`](https://github.com/nodejs/node/commit/4f68369643cbbbcc6b12028091bb8064e89ce02d)] - build: disable v8 snapshots (Ben Noordhuis)
* [[`c0a9d1b`](https://github.com/nodejs/node/commit/c0a9d1bc74e1aa5ed1f5a934509c1984142e0eab)] - versions: add http-parser patchlevel (Johan Bergström)
* [[`7854811`](https://github.com/nodejs/node/commit/785481149d59fddead9007d469e2578204f24cfb)] - child_process: clone spawn options argument (cjihrig)
* [[`88aaff9`](https://github.com/nodejs/node/commit/88aaff9aa6dd2aa2baadaf9b8d5f08e89fb77402)] - deps: update http_parser to 2.4.2 (Fedor Indutny)
* [[`804ab7e`](https://github.com/nodejs/node/commit/804ab7ebaaf5d87499e3cbce03184f064264dd2a)] - doc: add seishun as a collaborator (Nikolai Vavilov)
* [[`301a968`](https://github.com/nodejs/node/commit/301a968a40152c1ad3562482b4044458a13ebc4f)] - child_process: remove redundant condition (Vladimir Kurchatkin)
* [[`06cfff9`](https://github.com/nodejs/node/commit/06cfff935012ed2826cac56284cea982630cbc27)] - http: don't bother making a copy of the options (Jonathan Ong)
* [[`55c222c`](https://github.com/nodejs/node/commit/55c222ceba8e2b22fb5639082906faace520ec4e)] - doc: add vkurchatkin as collaborator (Vladimir Kurchatkin)
* [[`50ac4b7`](https://github.com/nodejs/node/commit/50ac4b7e2a823f92f0e102b804ec73f00eacb216)] - Working on 1.0.5 (Rod Vagg)
* [[`d1fc9c6`](https://github.com/nodejs/node/commit/d1fc9c6caec68883401fe601d99f3a69fee52556)] - 2015-01-24 io.js v1.0.4 Release (Rod Vagg)

## 2015-01-26, Version 0.10.36 (Stable)

### Commits

* [[`deef605085`](https://github.com/nodejs/node/commit/deef605085)] - **openssl**: update to 1.0.1l
* [[`45f1330425`](https://github.com/nodejs/node/commit/45f1330425)] - **v8**: Fix debugger and strict mode regression (Julien Gilli)
* [[`6ebd85e105`](https://github.com/nodejs/node/commit/6ebd85e105)] - **v8**: don't busy loop in cpu profiler thread (Ben Noordhuis) [#8789](https://github.com/joyent/node/pull/8789)

## 2015-01-24, Version 1.0.4, @rvagg

### Notable changes

* npm upgrade to 2.3.0 fixes Windows "uid is undefined" errors
* crypto.pseudoRandomBytes() is now an alias for crypto.randomBytes()
  and will block if there is insufficient entropy to produce secure
  values. See https://github.com/nodejs/node/commit/e5e5980 for details.
* Patch for V8 to properly detect ARMv6; binaries now work again on
  ARMv6 (Raspberry Pi etc.)
* Minor V8 upgrade from 4.1.0.7 to 4.1.0.12
* 'punycode' core module bumped from stability level 2-Unstable,
  to 3-Stable
* Added new collaborators:
  - Thorsten Lorenz (@thlorenz)
  - Stephen Belanger (@qard)
  - Jeremiah Senkpiel (@fishrock123)
  - Evan Lucas (@evanlucas)
  - Brendan Ashworth (@brendanashworth)

### Commits

* [[`bb766d2`](https://github.com/nodejs/node/commit/bb766d2c47e8a416ce9f1e4f693f124afe857c1a)] - doc: update "net" section in node to io.js changes (Andres Suarez)
* [[`73ddaa6`](https://github.com/nodejs/node/commit/73ddaa629c145af1632ac67d5d7d3a2abeabdf24)] - tools: remove old updateAuthors.awk script (Rod Vagg)
* [[`6230bf9`](https://github.com/nodejs/node/commit/6230bf9b79a6c451d678693004d52249fe9c1702)] - doc: update AUTHORS list (Rod Vagg)
* [[`33186fa`](https://github.com/nodejs/node/commit/33186fa7d89aef988e5cf24801de891d325afd7d)] - doc: adds brendanashworth as collaborator (Brendan Ashworth)
* [[`8f9502a`](https://github.com/nodejs/node/commit/8f9502a20a8851cfbf5f6181a52813baec23fe0f)] - doc: add evanlucas to collaborators (Evan Lucas)
* [[`35a4f11`](https://github.com/nodejs/node/commit/35a4f1107eeab39f9cd0e5b9abe6a314e1f6ddd7)] - doc: alphabetize all.markdown (Brendan Ashworth)
* [[`a0831c5`](https://github.com/nodejs/node/commit/a0831c580d50b54fd4add58071341b3b7ec83499)] - doc: add Fishrock123 to collaborators (Fishrock123)
* [[`5412487`](https://github.com/nodejs/node/commit/54124874dcc7eee1e8909cf2056c7f69722be4aa)] - doc: add qard to collaborators (Stephen Belanger)
* [[`8b55048`](https://github.com/nodejs/node/commit/8b55048d670d22d4e6d93710fe039d576a2b71bc)] - deps: make node-gyp work again on windows (Bert Belder)
* [[`82227f3`](https://github.com/nodejs/node/commit/82227f35110dcefa5a02e068a78dc3eb4aa0d3bc)] - deps: make node-gyp fetch tarballs from iojs.org (Ben Noordhuis)
* [[`f5b35db`](https://github.com/nodejs/node/commit/f5b35dbda45c466eda888a4451591c66e8671faf)] - deps: upgrade npm to 2.3.0 (Forrest L Norvell)
* [[`f3fed51`](https://github.com/nodejs/node/commit/f3fed5193caaac151acd555a7523068ee269801c)] - doc: adding thlorenz to list of collaborators (Thorsten Lorenz)
* [[`8de89ec`](https://github.com/nodejs/node/commit/8de89ec465d8f1e31521e0b888c19b0a3309cd88)] - lib: move default address logic to `net._listen2` (Vladimir Kurchatkin)
* [[`3143d73`](https://github.com/nodejs/node/commit/3143d732f6efd82da76e9c53ad192ac14071bf70)] - test: delete parallel/test-process-active-wraps (Ben Noordhuis)
* [[`4f95b5d`](https://github.com/nodejs/node/commit/4f95b5d8253ef64e3673b9fa178c41dc8109b72b)] - test: fix parallel/test-http-destroyed-socket-write2 (Ben Noordhuis)
* [[`5ba307a`](https://github.com/nodejs/node/commit/5ba307a97879342ff81aa813ffd7da46b6411b1c)] - test: fix parallel/test-dgram-error-message-address (Ben Noordhuis)
* [[`f4c536b`](https://github.com/nodejs/node/commit/f4c536b749735a0240da08386d6784767f95cb5d)] - debugger: don't override module binding (Vladimir Kurchatkin)
* [[`40ffed8`](https://github.com/nodejs/node/commit/40ffed8f3f4392d6e110769ca06d86d6295fc645)] - stream: use nop as write() callback if omitted (cjihrig)
* [[`df0d790`](https://github.com/nodejs/node/commit/df0d790107edf635dc233f3338b3c2e68db58cc7)] - doc: dns.lookupService has wrong header level (Icer Liang)
* [[`8b1db9c`](https://github.com/nodejs/node/commit/8b1db9c0a7dc39261218a0fac2dd6cf4fbb6a7b4)] - doc: note in docs about missing interfaces (Todd Kennedy)
* [[`2928ac6`](https://github.com/nodejs/node/commit/2928ac68e524bb5cacd522507bac0a147d01cd75)] - doc: bump punycode api stability to 'stable' (Ben Noordhuis)
* [[`328e67b`](https://github.com/nodejs/node/commit/328e67b58bc6dbcbed8ec452e6903ea6f121dc59)] - doc: add TC meeting 2015-01-21 minutes (Rod Vagg)
* [[`e5e5980`](https://github.com/nodejs/node/commit/e5e598060eb43faf2142184d523a04f0ca2d95c3)] - lib,src: make pseudoRandomBytes alias randomBytes (Calvin Metcalf)
* [[`c6cd460`](https://github.com/nodejs/node/commit/c6cd46041c70794d89634da380555fb613c2e0ab)] - configure: remove unused arm_neon variable (Ben Noordhuis)
* [[`7d9d756`](https://github.com/nodejs/node/commit/7d9d7560cfbd24172ede690e74cedbb4b26e32c9)] - configure: disable vfpv3 on armv6 (Ben Noordhuis)
* [[`297cadb`](https://github.com/nodejs/node/commit/297cadbab6a37fa4f14811452e4621770a321371)] - deps: fix v8 armv6 run-time detection (Ben Noordhuis)
* [[`d481bb6`](https://github.com/nodejs/node/commit/d481bb68c4f2cf01ec7d26dcc91862b265b7effa)] - doc: more explicit crypto.pseudoRandomBytes docs (Calvin Metcalf)
* [[`7d46247`](https://github.com/nodejs/node/commit/7d462479f6aad374fab90dd10bb07a8097f750aa)] - src: s/node/io.js/ in `iojs --help` message (Ben Noordhuis)
* [[`069c0df`](https://github.com/nodejs/node/commit/069c0dfb1cbfeb7c9c66a30f1fb5f065a9e22ee6)] - deps: upgrade v8 to 4.1.0.12 (Ben Noordhuis)
* [[`ada2a43`](https://github.com/nodejs/node/commit/ada2a4308c5a70728d01ea7447c0a7a153a9b703)] - doc: add TC meeting 2015-01-13 minutes (Rod Vagg)
* [[`60402b9`](https://github.com/nodejs/node/commit/60402b924b4b38196a658a023fad945421710457)] - docs: remove incorrect entry from changelog (Bert Belder)
* [[`8b98096`](https://github.com/nodejs/node/commit/8b98096c921f8a210b05aed64e0b2f1440667a7c)] - fs: make fs.access() flags read only (Jackson Tian)
* [[`804e7aa`](https://github.com/nodejs/node/commit/804e7aa9ab0b34fa88709ef0980b960abca5e059)] - lib: use const to define constants (cjihrig)
* [[`803883b`](https://github.com/nodejs/node/commit/803883bb1a701da12c285fd735233eed7627eada)] - v8: fix template literal NULL pointer deref (Ben Noordhuis)
* [[`5435cf2`](https://github.com/nodejs/node/commit/5435cf2f1619721745c7a8ac06b4f833d0b80d25)] - v8: optimize `getHeapStatistics` (Vladimir Kurchatkin)
* [[`5d01463`](https://github.com/nodejs/node/commit/5d014637b618af7eac6ab0fce8d67884598c7b35)] - benchmark: print score to five decimal places (Yosuke Furukawa)
* [[`752585d`](https://github.com/nodejs/node/commit/752585db6355ead7e6484f321e053b8d543c0a67)] - src: silence clang warnings (Trevor Norris)
* [[`22e1aea`](https://github.com/nodejs/node/commit/22e1aea8a025b6439493dec4d44afe4c9f454c86)] - src: set node_is_initialized in node::Init (Cheng Zhao)
* [[`668420d`](https://github.com/nodejs/node/commit/668420d1f7685f49843bbf81ee3b4733a1989852)] - src: clean up unused macros in node_file.cc (Ben Noordhuis)
* [[`52f624e`](https://github.com/nodejs/node/commit/52f624e72a419d3fd7f7f8ccc2d22ebdb0ba4fff)] - src: rename ASSERT macros in node_crypto.cc (Ben Noordhuis)
* [[`e95cfe1`](https://github.com/nodejs/node/commit/e95cfe14e343c5abed96a8d3cb9397c0c84abecc)] - src: add ASSERT_EQ style macros (Ben Noordhuis)
* [[`ee9cd00`](https://github.com/nodejs/node/commit/ee9cd004d8a211871439fc77c0696b79c5d0e52d)] - lib: fix TypeError with EventEmitter#on() abuse (Ben Noordhuis)
* [[`77d6807`](https://github.com/nodejs/node/commit/77d68070dafe56b5593ad92759a57c64de6b4cf1)] - test: fix event-emitter-get-max-listeners style (Ben Noordhuis)
* [[`767ee73`](https://github.com/nodejs/node/commit/767ee7348624803e6f90cf111df8b917fac442fc)] - test: strip copyright boilerplate (Ben Noordhuis)
* [[`86eda17`](https://github.com/nodejs/node/commit/86eda173b16b6ece9712e066661a0ac5db6795e8)] - fs: define constants with const (cjihrig)


## 2015-01-20, Version 1.0.3, @rvagg

### Notable changes

* V8 upgrade from 3.31 to 4.1, this is not a major upgrade, the version number "4.1" signifies tracking towards Chrome 41. The 3.31 branch is now not tracking towards a stable release.
* Re-enable Windows XP / 2003 support
* npm upgrade to 2.2.0
* Improved FreeBSD support

### Known issues

* ARMv6 builds still not working, there is a hold-up in V8 on this, issue #283
* Template strings can cause segfaults in V8 4.1, https://codereview.chromium.org/857433004, also issue #333

### Commits

* [[`9419e1f`](https://github.com/nodejs/node/commit/9419e1fb698e1a9319fec5c4777686d62fad4a51)] - src: fix inconsistency between a check and error (toastynerd)
* [[`03ee4d8`](https://github.com/nodejs/node/commit/03ee4d854744e83f99bc5857b98f75139c448564)] - fs: add error code on null byte paths (cjihrig)
* [[`e2558f0`](https://github.com/nodejs/node/commit/e2558f02dfb671fc74f5768d4401a826efb5c117)] - net: fix error details in connect() (cjihrig)
* [[`4af5746`](https://github.com/nodejs/node/commit/4af5746993a6b91c88973b6debcee19c6cd35185)] - win,build: remove duplicate definition (Bert Belder)
* [[`e8d0850`](https://github.com/nodejs/node/commit/e8d08503c7821e8c92e9fa236ed7328e9bdfe62a)] - win: bring back xp/2k3 support (Bert Belder)
* [[`4dd22b9`](https://github.com/nodejs/node/commit/4dd22b946ebfec81a7c4a61aa9c6ed528e317802)] - cluster: avoid race enabling debugger in worker (Timothy J Fontaine)
* [[`6b91c78`](https://github.com/nodejs/node/commit/6b91c78e201948937a4524027a6778aa7f82fb0a)] - test: reland changes from [`11c1bae`](https://github.com/nodejs/node/commit/11c1bae734dae3a017f2c4f3f71b5e679a9ddfa6) (Ben Noordhuis)
* [[`992a1e7`](https://github.com/nodejs/node/commit/992a1e7f5f87606276af8504c2d57cc5a966830a)] - test: debug-signal-cluster should not be racey (Timothy J Fontaine)
* [[`cdf0df1`](https://github.com/nodejs/node/commit/cdf0df13d85391b3b8ac36fa5b70da7f21072619)] - test: temporarily back out changes from [`11c1bae`](https://github.com/nodejs/node/commit/11c1bae734dae3a017f2c4f3f71b5e679a9ddfa6) (Ben Noordhuis)
* [[`1ea607c`](https://github.com/nodejs/node/commit/1ea607cb299b0bb59d7d557e01b21b3c615d689e)] - test: move sequential/test-debug-port-from-cmdline (Ben Noordhuis)
* [[`2f33e00`](https://github.com/nodejs/node/commit/2f33e00d716d692e84b02768430664fd92298c98)] - test: fix test-debug-port-from-cmdline.js (Julien Gilli)
* [[`b7365c1`](https://github.com/nodejs/node/commit/b7365c15597253e906590045aa6f3f07f6e76b52)] - repl: make REPL support multiline template literals (Xiaowei Li)
* [[`2253d30`](https://github.com/nodejs/node/commit/2253d30d9cbba42abc1faa183e4480cac69c4222)] - build: remove unused variable (Johan Bergström)
* [[`ab04a43`](https://github.com/nodejs/node/commit/ab04a434761cf66d107481d58798f36d3cb49d46)] - doc: add optional sudo to make install in README (Glen Keane)
* [[`1b1cd1c`](https://github.com/nodejs/node/commit/1b1cd1c3f8e21b34a8e1355e545057a661acaa15)] - build: shorten configurate script print on stdout (Roman Reiss)
* [[`d566ded`](https://github.com/nodejs/node/commit/d566ded26b996c27afeb7fc208709bb6096bfa13)] - deps: fix V8 debugger bugs (Jay Jaeho Lee)
* [[`6f36630`](https://github.com/nodejs/node/commit/6f36630f55efcbe5954a52ac22bbb0a378020e98)] - doc: fix util.isBuffer examples (Thomas Jensen)
* [[`3abfb56`](https://github.com/nodejs/node/commit/3abfb56f9b012da0d1e1deaec1529ea7384a0a71)] - benchmark: fix tcp bench after internal api change (Yosuke Furukawa)
* [[`50177fb`](https://github.com/nodejs/node/commit/50177fb13cae68067845cca7622798eb7a34f8e9)] - benchmark: stop v8 benchmark clobbering RegExp (Ben Noordhuis)
* [[`1952219`](https://github.com/nodejs/node/commit/19522197ef28275344ad2f1e0799ce8106276ec1)] - deps: make node-gyp work again on windows (Bert Belder)
* [[`a28de9b`](https://github.com/nodejs/node/commit/a28de9bd3684f54379ccf101f62656771002205d)] - deps: make node-gyp fetch tarballs from iojs.org (Ben Noordhuis)
* [[`9dc8f59`](https://github.com/nodejs/node/commit/9dc8f59fea5a294df039f70e523be2d45aef1324)] - deps: upgrade npm to 2.2.0 (Forrest L Norvell)
* [[`e8ad773`](https://github.com/nodejs/node/commit/e8ad773b56a94fad2cd8a454453a7214a8ce92d1)] - src: remove --noharmony_classes again (Ben Noordhuis)
* [[`334020e`](https://github.com/nodejs/node/commit/334020e016a72952a9a3b3f7e9179145c7e167ad)] - deps: fix v8 build on FreeBSD (Fedor Indutny)
* [[`5e7ebc7`](https://github.com/nodejs/node/commit/5e7ebc7af6d08d4e31cf66f4ae22d29c688ef814)] - deps: upgrade v8 to 4.1.0.7 (Ben Noordhuis)
* [[`ea7750b`](https://github.com/nodejs/node/commit/ea7750bddd8051f39fa538905e05f9bf1d1afa5f)] - benchmark: add filter option for benchmark (Yosuke Furukawa)
* [[`4764eef`](https://github.com/nodejs/node/commit/4764eef9b2efdf17cafeb4ec40898c6669a84e3b)] - doc: fixed punctuation (Brenard Cubacub)
* [[`de224d6`](https://github.com/nodejs/node/commit/de224d6e6c9381e71ffee965dbda928802cc438e)] - configure: remove --ninja switch (Ben Noordhuis)
* [[`48774ec`](https://github.com/nodejs/node/commit/48774ec027a28cca17656659d316bb7ed8d6f33c)] - configure: print warning for old compilers (Ben Noordhuis)
* [[`daf9562`](https://github.com/nodejs/node/commit/daf9562d918b7926186471cd0db60cec2f72547a)] - doc: change to iojs from node in the usage message (Jongyeol Choi)
* [[`3fde649`](https://github.com/nodejs/node/commit/3fde64937a3a0c8ed941ee97b07e1828b392a672)] - build: add tools/gflags to PYTHONPATH (Shigeki Ohtsu)
* [[`8b22df1`](https://github.com/nodejs/node/commit/8b22df15ae0e3499b2e057ffd8a6f65cbf978da3)] - doc: add python-gflags LICENSE block (Shigeki Ohtsu)
* [[`6242229`](https://github.com/nodejs/node/commit/62422297f52523d2214136cd5514e2453197e3e8)] - tools: add python-gflags module (Shigeki Ohtsu)


## 2015-01-16, Version 1.0.2, @rvagg

### Notable changes

* Windows installer fixes
* Bundled node-gyp fixes for Windows
* [http_parser v2.4.1 upgrade](https://github.com/joyent/http-parser/compare/v2.3...v2.4.1)
* [libuv v1.2.1 upgrade](https://github.com/libuv/libuv/compare/v1.2.0...v1.2.1)

### Commits

* [[`265cb76`](https://github.com/nodejs/node/commit/265cb76517d81408afb72506c778f0c0b889f4dc)] - build: add new installer config for OS X (Rod Vagg)
* [[`8cf6079`](https://github.com/nodejs/node/commit/8cf6079a6a7f5d1afb06606b7c51acf9b1a046a0)] - doc: update AUTHORS list (Rod Vagg)
* [[`c80a944`](https://github.com/nodejs/node/commit/c80a9449b309f9c52a5910b7ac6ba0c84ee1b6f6)] - doc: Add http keepalive behavior to CHANGELOG.md (Isaac Z. Schlueter)
* [[`9b81c3e`](https://github.com/nodejs/node/commit/9b81c3e77ffd733645956129a38fdc2fddd08b50)] - doc: fix author attribution (Tom Hughes)
* [[`fd30eb2`](https://github.com/nodejs/node/commit/fd30eb21526bdaa5aabb15523b0a766e0cbbe535)] - src: fix jslint errors (Yosuke Furukawa)
* [[`946eabd`](https://github.com/nodejs/node/commit/946eabd18f623b438e17164b14c98066f7054168)] - tools: update closure linter to 2.3.17 (Yosuke Furukawa)
* [[`9e62ae4`](https://github.com/nodejs/node/commit/9e62ae4304a0bee3aec8c5fb743eb17d78b1cd35)] - _debug_agent: use `readableObjectMode` option (Vladimir Kurchatkin)
* [[`eec4c81`](https://github.com/nodejs/node/commit/eec4c8168be1f0a01db3576ae99f7756eea01151)] - doc: fix formatting in LICENSE for RTF generation (Rod Vagg)
* [[`e789103`](https://github.com/nodejs/node/commit/e7891034c269dccf8d6264acc4b7421e19a905f6)] - doc: fix 404s for syntax highlighting js (Phil Hughes)
* [[`ca039b4`](https://github.com/nodejs/node/commit/ca039b4616915b95130ba5ee5a2cf9f5c768645e)] - src: define AI_V4MAPPED for OpenBSD (Aaron Bieber)
* [[`753fcaa`](https://github.com/nodejs/node/commit/753fcaa27066b34a99ee1c02b43a32744fc92a3c)] - doc: extend example of http.request by end event (Michal Tehnik)
* [[`8440cac`](https://github.com/nodejs/node/commit/8440cacb100ae83c2b2c02e82a87c73a66380c21)] - src: fix documentation url in help message (Shigeki Ohtsu)
* [[`24def66`](https://github.com/nodejs/node/commit/24def662936ae8c15770ede0344cd7a7402a63ef)] - win,msi: warn that older io.js needs manual uninstall (Bert Belder)
* [[`59d9361`](https://github.com/nodejs/node/commit/59d93613d8e1e8507b5c8462c52dd3cbda98e99b)] - win,msi: change UpgradeCode (Bert Belder)
* [[`5de334c`](https://github.com/nodejs/node/commit/5de334c23096492014a097ff487f07ad8eaee6d2)] - deps: make node-gyp work again on windows (Bert Belder)
* [[`07bd05b`](https://github.com/nodejs/node/commit/07bd05ba332e078c1ba76635921f5448a3e884cf)] - deps: update libuv to 1.2.1 (Saúl Ibarra Corretgé)
* [[`e177377`](https://github.com/nodejs/node/commit/e177377a4bc0cdbaecb8b17a58e57c73b4ca0090)] - doc: mention io.js alongside Node in Punycode docs (Mathias Bynens)
* [[`598efcb`](https://github.com/nodejs/node/commit/598efcbe7f4d795622f038e0ba28c7b119927a14)] - deps: update http_parser to 2.4.1 (Fedor Indutny)
* [[`3dd7ebb`](https://github.com/nodejs/node/commit/3dd7ebb0ba181960fb6d7131e11243a6ec85458d)] - doc: update cluster entry in CHANGELOG (Ben Noordhuis)
* [[`0c5de1f`](https://github.com/nodejs/node/commit/0c5de1ff813de9661d33cb9aefc4a9540b58b147)] - doc: fix double smalloc example (Mathias Buus)


## 2015-01-14, Version 1.0.1, @rvagg

Rebuild due to stale build slave git reflogs for 1.0.0 release

* doc: improve write style consistency (Rui Marinho)
* win,msi: correct doc website link (Bert Belder)

--------------------------------------

Below is a summary of the user-facing changes to be found in the io.js v1.0.0 release as compared to the
current _stable_ Node.js release, v0.10.35. At the time of the v1.0.0 release, the latest _unstable_
Node.js release is v0.11.14 with much progress made towards a v0.11.15 release. The io.js codebase inherits
the majority of the changes found in the v0.11 branch of the [joyent/node](https://github.com/joyent/node)
repository and therefore can be seen as an extension to v0.11.

## Summary of changes from Node.js v0.10.35 to io.js v1.0.0

### General

- The V8 JavaScript engine bundled with io.js was upgraded dramatically, from version 3.14.5.9 in Node.js v0.10.35 and 3.26.33 in Node.js v0.11.14 to 3.31.74.1 for io.js v1.0.0. This brings along many fixes and performance improvements, as well as additional support for new ES6 language features! For more information on this, check out [the io.js ES6 page](https://iojs.org/es6.html).
- Other bundled technologies were upgraded:
  - c-ares: 1.9.0-DEV to 1.10.0-DEV
  - http_parser: 1.0 to 2.3
  - libuv: 0.10.30 to 1.2.0
  - npm: 1.4.28 to 2.1.18
  - openssl: 1.0.1j to 1.0.1k
  - punycode: 1.2.0 to 1.3.2.
- Performance and stability improvements on all platforms.

### buffer

https://iojs.org/api/buffer.html

- Added `buf.writeUIntLE`, `buf.writeUIntBE`, `buf.writeIntLE`, `buf.writeIntBE`, `buf.readUIntLE`, `buf.readUIntBE`, `buf.readIntLE` and `buf.readIntBE` methods that read and write value up to 6 bytes.
- Added `Buffer.compare()` which does a `memcmp()` on two Buffer instances. Instances themselves also have a `compare()`.
- Added `buffer.equals()` that checks equality of Buffers by their contents.
- Added `new Buffer(otherBuffer)` constructor.
- Tweaked `SlowBuffer`'s semantics.
- Updated the output of `buffer.toJSON()` to not be the same as an array. Instead it is an object specifically tagged as a buffer, which can be recovered by passing it to (a new overload of) the `Buffer` constructor.

### child_process

https://iojs.org/api/child_process.html

- Added a `shell` option to `child_process.exec`.
- Added synchronous counterparts for the child process functions: `child_process.spawnSync`, `child_process.execSync`, and `child_process.execFileSync`.
- Added the path to any `ENOENT` errors, for easier debugging.

### console

https://iojs.org/api/console.html

- Added an `options` parameter to `console.dir`.

### cluster

https://iojs.org/api/cluster.html

- Updated `cluster` to use round-robin load balancing by default on non-Windows platforms. The scheduling policy is configurable however.
- `--debug` has been made cluster-aware.
- Many bug fixes.

### crypto

https://iojs.org/api/crypto.html

- Added support for custom generator values to `DiffieHellman` (defaulting to 2 for backwards compatibility).
- Added support for custom pbkdf2 digest methods.
- Added support for elliptic curve-based Diffie-Hellman.
- Added support for loading and setting the engine for some/all OpenSSL functions.
- Added support for passing in a passphrase for decrypting the signing key to `Sign.sign()`.
- Added support for private key passphrase in every method that accepts it.
- Added support for RSA public/private encryption/decryption functionality.
- Added support for setting and getting of authentication tags and setting additional authentication data when using ciphers such as AES-GCM.

### dgram

https://iojs.org/api/dgram.html

- Added support for receiving empty UDP packets.

### dns

https://iojs.org/api/dns.html

- Added `dns.resolveSoa`, `dns.getServers`, and `dns.setServers` methods.
- Added `hostname` on error messages when available.
- Improved error handling consistency.

### events

https://iojs.org/api/events.html

- Added chaining support to `EventEmitter.setMaxListeners`.
- Updated `require('events')` to return the `EventEmitter` constructor, allowing the module to be used like `var EventEmitter = require('events')` instead of `var EventEmitter = require('events').EventEmitter`.

### fs

https://iojs.org/api/fs.html

- Added `fs.access`, and deprecated `fs.exists`. Please read the documentation carefully.
- Added more informative errors and method call site details when the `NODE_DEBUG` environment is set to ease debugging.
- Added option to `fs.watch` for recursive sub-directory support (OS X only).
- Fixed missing callbacks errors just being printed instead of thrown.

### http

https://iojs.org/api/http.html

- Added support for `response.write` and `response.end` to receive a callback to know when the operation completes.
- Added support for 308 status code (see RFC 7238).
- Added `http.METHODS` array, listing the HTTP methods supported by the parser.
- Added `request.flush` method.
- Added `response.getHeader('header')` method that may be used before headers are flushed.
- Added `response.statusMessage` property.
- Added Client Keep-Alive behavior.  Set `keepAlive:true` in request options to reuse connections indefinitely.
- Added `rawHeaders` and `rawTrailers` members on incoming message.
- Removed default chunked encoding on `DELETE` and `OPTIONS`.

### net

https://iojs.org/api/net.html

- Changed `net.Server.listen` such that, when the bind address is omitted, IPv6 is tried first, and IPv4 is used as a fallback.

### os

https://iojs.org/api/os.html

- Added MAC addresses, netmasks and scope IDs for IPv6 addresses to `os.networkInterfaces` method output.
- Updated `os.tmpdir` on Windows to use the `%SystemRoot%` or `%WINDIR%` environment variables instead of the hard-coded value of `c:\windows` when determining the temporary directory location.

### path

https://iojs.org/api/path.html

- Added `path.isAbsolute` and `path.parse` methods.
- Added `path.win32` and `path.posix` objects that contain platform-specific versions of the various `path` functions.
- Improved `path.join` performance.

### process

https://iojs.org/api/process.html

- Added `beforeExit` event.
- Added `process.mainModule` and `process.exitCode`.

### querystring

https://iojs.org/api/querystring.html

- Added the ability to pass custom versions of `encodeURIComponent` and `decodeURIComponent` when stringifying or parsing a querystring.
- Fixed several issues with the formatting of query strings in edge cases.

### smalloc

https://iojs.org/api/smalloc.html

`smalloc` is a new core module for doing (external) raw memory allocation/deallocation/copying in JavaScript.

### streams

https://iojs.org/api/stream.html

The changes to streams are not as drastic as the transition from streams1 to streams2: they are a
refinement of existing ideas, and should make the API slightly less surprising for humans and faster
for computers. As a whole the changes are referred to as "streams3", but the changes should largely go
unnoticed by the majority of stream consumers and implementers.

#### Readable streams

The distinction between "flowing" and "non-flowing" modes has been refined. Entering "flowing" mode is
no longer an irreversible operation&mdash;it is possible to return to "non-flowing" mode from "flowing" mode.
Additionally, the two modes now flow through the same machinery instead of replacing methods. Any time
data is returned as a result of a `.read` call that data will *also* be emitted on the `"data"` event.

As before, adding a listener for the `"readable"` or `"data"` event will start flowing the stream; as
will piping to another stream.

#### Writable streams

The ability to "bulk write" to underlying resources has been added to `Writable` streams. For stream
implementers, one can signal that a stream is bulk-writable by specifying a [_writev](https://iojs.org/api/stream.html#stream_writable_writev_chunks_callback) method.
Bulk writes will occur in two situations:

1. When a bulk-writable stream is clearing its backlog of buffered write requests,
2. or if an end user has made use of the new `.cork()` and `.uncork()` API methods.

`.cork` and `.uncork` allow the end user to control the buffering behavior of writable streams separate
from exerting backpressure. `.cork` indicates that the stream should accept new writes (up to `highWaterMark`),
while `.uncork` resets that behavior and attempts to bulk-write all buffered writes to the underlying resource.

The only core stream API that **currently** implements `_writev` is `net.Socket`.

In addition to the bulk-write changes, the performance of repeated small writes to non-bulk-writable streams
(such as `fs.WriteStream`) has been drastically improved. Users piping high volume log streams to disk should
see an improvement.

For a detailed overview of how streams3 interact, [see this diagram](https://cloud.githubusercontent.com/assets/37303/5728694/f9a3e300-9b20-11e4-9e14-a6938b3327f0.png).

### timers

https://iojs.org/api/timers.html

- Removed `process.maxTickDepth`, allowing `process.nextTick` to be used recursively without limit.
- Updated `setImmediate` to process the full queue each turn of the event loop, instead of one per queue.

### tls

https://iojs.org/api/tls.html

- Added `detailed` boolean flag to `getPeerCertificate` to return detailed certificate information (with raw DER bytes).
- Added `renegotiate(options, callback)` method for session renegotiation.
- Added `setMaxSendFragment` method for varying TLS fragment size.
- Added a `dhparam` option for DH ciphers.
- Added a `ticketKeys` option for TLS ticket AES encryption keys setup.
- Added async OCSP-stapling callback.
- Added async session storage events.
- Added async SNI callback.
- Added multi-key server support (for example, ECDSA+RSA server).
- Added optional callback to `checkServerIdentity` for manual certificate validation in userland.
- Added support for ECDSA/ECDHE cipher.
- Implemented TLS streams in C++, boosting their performance.
- Moved `createCredentials` to `tls` and renamed it to `createSecureContext`.
- Removed SSLv2 and SSLv3 support.

### url

https://iojs.org/api/url.html

- Improved escaping of certain characters.
- Improved parsing speed.

### util

https://iojs.org/api/util.html

- Added `util.debuglog`.
- Added a plethora of new type-testing methods. See [the docs](https://iojs.org/api/util.html).
- Updated `util.format` to receive several changes:
  - `-0` is now displayed as such, instead of as `0`.
  - Anything that is `instanceof Error` is now formatted as an error.
  - Circular references in JavaScript objects are now handled for the `%j` specifier.
  - Custom `inspect` functions are now allowed to return an object.
  - Custom `inspect` functions now receive any arguments passed to `util.inspect`.

## v8

https://iojs.org/api/v8.html

`v8` is a new core module for interfacing directly with the V8 engine.

### vm

https://iojs.org/api/vm.html

The `vm` module has been rewritten to work better, based on the excellent [Contextify](https://github.com/brianmcd/contextify) native module. All of the functionality of Contextify is now in core, with improvements!

- Added `vm.isContext(object)` method to determine whether `object` has been contextified.
- Added `vm.runInDebugContext(code)` method to compile and execute `code` inside the V8 debug context.
- Updated `vm.createContext(sandbox)` to "contextify" the sandbox, making it suitable for use as a global for `vm` scripts, and then return it. It no longer creates a separate context object.
- Updated most `vm` and `vm.Script` methods to accept an `options` object, allowing you to configure a timeout for the script, the error display behavior, and sometimes the filename (for stack traces).
- Updated the supplied sandbox object to be used directly as the global, remove error-prone copying of properties back and forth between the supplied sandbox object and the global that appears inside the scripts run by the `vm` module.

For more information, see the `vm` documentation linked above.

### zlib

https://iojs.org/api/zlib.html

- Added support for `zlib.flush` to specify a particular flush method (defaulting to `Z_FULL_FLUSH`).
- Added support for `zlib.params` to dynamically update the compression level and strategy when deflating.
- Added synchronous versions of the zlib methods.

### C++ API Changes

https://iojs.org/api/addons.html

In general it is recommended that you use [NAN](https://github.com/rvagg/nan) as a compatibility layer for your addons. This will also help with future changes in the V8 and Node/io.js C++ API. Most of the following changes are already handled by NAN-specific wrappers.

#### V8 highlights

- Exposed method signature has changed from `Handle<Value> Method(const Arguments& args)` to `void Method(const v8::FunctionCallbackInfo<Value>& args)` with the newly introduced `FunctionCallbackInfo` also taking the return value via `args.GetReturnValue().Set(value)` instead of `scope.Close(value)`, `Arguments` has been removed.
- Exposed setter signature has changed from `void Setter(Local<String> property, Local<Value> value, const v8::AccessorInfo& args)` `void Setter(Local<String> property, Local<Value> value, const v8::PropertyCallbackInfo<void>& args)`.
- Exposed getter signature has changed from `void Getter(Local<String> property, Local<Value> value, const v8::AccessorInfo& args)` `void Getter(Local<String> property, Local<Value> value, const v8::PropertyCallbackInfo<Value>& args)`.
- Exposed property setter signature has changed from `Handle<Value> Setter(Local<String> property, Local<Value> value, const v8::AccessorInfo& args)` `void Setter(Local<String> property, Local<Value> value, const v8::PropertyCallbackInfo<Value>& args)`.
- Exposed property getter signature has changed from `Handle<Value> Getter(Local<String> property, Local<Value> value, const v8::AccessorInfo& args)` `void Getter(Local<String> property, Local<Value> value, const v8::PropertyCallbackInfo<Value>& args)`.
- Similar changes have been made to property enumerators, property deleters, property query, index getter, index setter, index enumerator, index deleter, index query.
- V8 objects instantiated in C++ now require an `Isolate*` argument as the first argument. In most cases it is OK to simply pass `v8::Isolate::GetCurrent()`, e.g. `Date::New(Isolate::GetCurrent(), time)`, or `String::NewFromUtf8(Isolate::GetCurrent(), "foobar")`.
- `HandleScope scope` now requires an `Isolate*` argument, i.e. `HandleScope scope(isolate)`, in most cases `v8::Isolate::GetCurrent()` is OK.
- Similar changes have been made to `Locker` and `Unlocker`.
- V8 objects that need to "escape" a scope should be enclosed in a `EscapableHandleScope` rather than a `HandleScope` and should be returned with `scope.Escape(value)`.
- Exceptions are now thrown from isolates with `isolate->ThrowException(ExceptionObject)`.
- `Context::GetCurrent()` must now be done on an isolate, e.g. `Isolate::GetCurrent()->GetCurrentContext()`.
- `String::NewSymbol()` has been removed, use plain strings instead.
- `String::New()` has been removed, use `String::NewFromUtf8()` instead.
- `Persistent` objects no longer inherit from `Handle` and cannot be instantiated with another object. Instead, the `Persistent` should simply be declared, e.g. `Persistent<Type> handle` and then have a `Local` assigned to it with `handle.Reset(isolate, value)`. To get a `Local` from a `Persistent` you must instantiate it as the argument, i.e. `Local::New(Isolate*, Persistent)`.

#### Node / io.js

- Updated `node::Buffer::New()` to return a `Handle` directly so you no longer need to fetch the `handle_` property.
- Updated `node::MakeCallback()` to require an `Isolate*` as the first argument. Generally `Isolate::GetCurrent()` will be OK for this.


--------------------------------------

**The changelog below was inherited from joyent/node prior to the io.js fork.**


## 2014.09.24, Version 0.11.14 (Unstable)

* uv: Upgrade to v1.0.0-rc1
* http_parser: Upgrade to v2.3.0
* npm: Upgrade to v2.0.0
* openssl: Upgrade to v1.0.1i
* v8: Upgrade to 3.26.33
* Add fast path for simple URL parsing (Gabriel Wicke)
* Added support for options parameter in console.dir() (Xavi Magrinyà)
* Cluster: fix shared handles on Windows (Alexis Campailla)
* buffer: Fix incorrect Buffer.compare behavior (Feross Aboukhadijeh)
* buffer: construct new buffer from buffer toJSON() output (cjihrig)
* buffer: improve Buffer constructor (Kang-Hao Kenny)
* build: linking CoreFoundation framework for OSX (Thorsten Lorenz)
* child_process: accept uid/gid everywhere (Fedor Indutny)
* child_process: add path to spawn ENOENT Error (Ryan Cole)
* child_process: copy spawnSync() cwd option to proper buffer (cjihrig)
* child_process: do not access stderr when stdio set to 'ignore' (cjihrig)
* child_process: don't throw on EAGAIN (Charles)
* child_process: don't throw on EMFILE/ENFILE (Ben Noordhuis)
* child_process: use full path for cmd.exe on Win32 (Ed Morley)
* cluster: allow multiple calls to setupMaster() (Ryan Graham)
* cluster: centralize removal from workers list. (Julien Gilli)
* cluster: enable error/message events using .worker (cjihrig)
* cluster: include settings object in 'setup' event (Ryan Graham)
* cluster: restore v0.10.x setupMaster() behaviour (Ryan Graham)
* cluster: support options in Worker constructor (cjihrig)
* cluster: test events emit on cluster.worker (Sam Roberts)
* console: console.dir() accepts options object (Xavi Magrinyà)
* crypto: add `honorCipherOrder` argument (Fedor Indutny)
* crypto: allow padding in RSA methods (Fedor Indutny)
* crypto: clarify RandomBytes() error msg (Mickael van der Beek)
* crypto: never store pointer to conn in SSL_CTX (Fedor Indutny)
* crypto: unsigned value can't be negative (Brian White)
* dgram: remove new keyword from errnoException (Jackson Tian)
* dns: always set variable family in lookup() (cjihrig)
* dns: include host name in error message if available (Maciej Małecki)
* dns: introduce lookupService function (Saúl Ibarra Corretgé)
* dns: send lookup c-ares errors to callback (Chris Dickinson)
* dns: throw if hostname is not string or falsey (cjihrig)
* events: Output the event that is leaking (Arnout Kazemier)
* fs: close file if fstat() fails in readFile() (cjihrig)
* fs: fs.readFile should not throw uncaughtException (Jackson Tian)
* http: add 308 status_code, see RFC7238 (Yazhong Liu)
* http: don't default OPTIONS to chunked encoding (Nick Muerdter)
* http: fix bailout for writeHead (Alex Kocharin)
* http: remove unused code block (Fedor Indutny)
* http: write() after end() emits an error. (Julien Gilli)
* lib, src: add vm.runInDebugContext() (Ben Noordhuis)
* lib: noisy deprecation of child_process customFds (Ryan Graham)
* module: don't require fs several times (Robert Kowalski)
* net,dgram: workers can listen on exclusive ports (cjihrig)
* net,stream: add isPaused, don't read() when paused (Chris Dickinson)
* net: Ensure consistent binding to IPV6 if address is absent (Raymond Feng)
* net: add remoteFamily for socket (Jackson Tian)
* net: don't emit listening if handle is closed (Eli Skeggs)
* net: don't prefer IPv4 addresses during resolution (cjihrig)
* net: don't throw on net.Server.close() (cjihrig)
* net: reset `errorEmitted` on reconnect (Ed Umansky)
* node: set names for prototype methods (Trevor Norris)
* node: support v8 microtask queue (Vladimir Kurchatkin)
* path: fix slice OOB in trim (Lucio M. Tato)
* path: isAbsolute() should always return boolean (Herman Lee)
* process: throw TypeError if kill pid not a number (Sam Roberts)
* querystring: custom encode and decode (fengmk2)
* querystring: do not add sep for empty array (cjihrig)
* querystring: remove prepended ? from query field (Ezequiel Rabinovich)
* readline: fix close event of readline.Interface() (Yazhong Liu)
* readline: fixes scoping bug (Dan Kaplun)
* readline: implements keypress buffering (Dan Kaplun)
* repl: fix multi-line input (Fedor Indutny)
* repl: fix overwrite for this._prompt (Yazhong Liu)
* repl: proper `setPrompt()` and `multiline` support (Fedor Indutny)
* stream: don't try to finish if buffer is not empty (Vladimir Kurchatkin)
* stream: only end reading on null, not undefined (Jonathan Reem)
* streams: set default hwm properly for Duplex (Andrew Oppenlander)
* string_bytes: ucs2 support big endian (Andrew Low)
* tls, crypto: add DHE support (Shigeki Ohtsu)
* tls: `checkServerIdentity` option (Trevor Livingston)
* tls: add DHE-RSA-AES128-SHA256 to the def ciphers (Shigeki Ohtsu)
* tls: better error reporting at cert validation (Fedor Indutny)
* tls: support multiple keys/certs (Fedor Indutny)
* tls: throw an error, not string (Jackson Tian)
* udp: make it possible to receive empty udp packets (Andrius Bentkus)
* url: treat \ the same as / (isaacs)

## 2014.05.01, Version 0.11.13 (Unstable)

https://github.com/nodejs/node/commit/99c9930ad626e2796af23def7cac19b65c608d18

* v8: upgrade to 3.24.35.22
* buffer: add compare and equals methods (Sean McArthur)
* buffer: improve {read,write}{U}Int* methods (Nick Apperson)
* buffer: return uint if MSB is 1 in readUInt32 (goussardg)
* buffer: truncate buffer after string decode (Fedor Indutny)
* child_process: fix assertion error in spawnSync (Shigeki Ohtsu)
* crypto: fix memory leak in CipherBase::Final (Fedor Indutny)
* crypto: improve error messages (Ingmar Runge)
* crypto: move `createCredentials` to tls (Fedor Indutny)
* crypto: work around OpenSSL oddness (Fedor Indutny)
* dgram: introduce `reuseAddr` option (Fedor Indutny)
* domain: don't crash on "throw null" (Alex Kocharin)
* events: check if _events is an own property (Vladimir Kurchatkin)
* fs: improve performance of all stat functions (James Pickard)
* fs: return blksize on stats object (Trevor Norris)
* http: add request.flush() method (Ben Noordhuis)
* http: better client "protocol not supported" error (Nathan Rajlich)
* http: use defaultAgent.protocol in protocol check (Nathan Rajlich)
* main: Handle SIGINT properly. (Geir Hauge)
* net: bind to `::` TCP address by default (Fedor Indutny)
* readline: consider newlines for cursor position (Yazhong Liu)
* stream: split `objectMode` for Duplex (Vladimir Kurchatkin)
* tls: `getPeerCertificate(detailed)` (Fedor Indutny)
* tls: do not call SNICallback unless present (Fedor Indutny)
* tls: force readable/writable to `true` (Fedor Indutny)
* tls: support OCSP on client and server (Fedor Indutny)
* util: made util.isArray a direct alias for Array.isArray (Evan Carroll)

## 2014.03.11, Version 0.11.12 (Unstable)

https://github.com/nodejs/node/commit/7d6b8db40f32e817ff145b7cfe6b3aec3179fba7

* uv: Upgrade to v0.11.22 (Timothy J Fontaine)
* buffer: allow toString to accept Infinity for end (Brian White)
* child_process: add spawnSync/execSync (Bert Belder, Timothy J Fontaine)
* cluster: handle bind errors on Windows (Alexis Campailla)
* contextify: handle infinite recursion errors (Fedor Indutny)
* crypto: allow custom generator for DiffieHellman (Brian White)
* crypto: allow setting add'l authenticated data (Brian White)
* crypto: fix CipherFinal return value check (Brian White)
* crypto: make NewSessionDoneCb public (Fedor Indutny)
* dgram: pass the bytes sent to the send callback (Timothy J Fontaine)
* dns: validate arguments in resolver (Kenan Sulayman)
* dns: verify argument is valid function in resolve (Kenan Sulayman)
* http: avoid duplicate keys in writeHead (David Björklund)
* net: add localPort to connect options (Timothy J Fontaine)
* node: do not print SyntaxError hints to stderr (Fedor Indutny)
* node: invoke `beforeExit` again if loop was active (Fedor Indutny)
* node: make AsyncListenerInst field more explicit (Trevor Norris)
* os: networkInterfaces include scopeid for ipv6 (Xidorn Quan)
* process: allow changing `exitCode` in `on('exit')` (Fedor Indutny)
* readline: fix `line` event, if input emit 'end' (Yazhong Liu)
* src: add tracing.v8.on('gc') statistics hooks (Ben Noordhuis)
* src: add v8.getHeapStatistics() function (Ben Noordhuis)
* src: emit 'beforeExit' event on process object (Ben Noordhuis)
* src: move AsyncListener from process to tracing (Trevor Norris)
* tls: fix crash in SNICallback (Fedor Indutny)
* tls: introduce asynchronous `newSession` (Fedor Indutny)
* util: show meaningful values for boxed primitives (Nathan Rajlich)
* vm: don't copy Proxy object from parent context (Ben Noordhuis)
* windows: make stdout/sterr pipes blocking (Alexis Campailla)
* zlib: add sync versions for convenience methods (Nikolai Vavilov)

## 2014.01.29, Version 0.11.11 (Unstable)

https://github.com/nodejs/node/commit/b46e77421581ea358e221a8a843d057c747f7e90

* v8: Upgrade to 3.22.24.19
* http_parser: Upgrade to 2.2.1
* openssl: Upgrade to 1.0.1f
* uv: Upgrade to 0.11.18
* async-listener: revamp of subsystem (Trevor Norris)
* node: do not ever close stdio (Fedor Indutny)
* http: use writev on chunked encoding (Trevor Norris)
* async_wrap/timers: remove Add/RemoveAsyncListener (Trevor Norris)
* child_process: better error reporting for exec (Fedor Indutny)
* crypto: add newline to cert and key if not present (Fedor Indutny)
* crypto: clear error in GetPeerCertificate (Fedor Indutny)
* crypto: honor default ciphers in client mode (Jacob Hoffman-Andrews)
* crypto: introduce .setEngine(engine, [flags]) (Fedor Indutny)
* crypto: support custom pbkdf2 digest methods (Ben Noordhuis)
* domain: fix off-by-one in Domain.exit() (Ryan Graham)
* http: concatenate duplicate headers by default (Alex Kocharin)
* http: do not emit EOF non-readable socket (Fedor Indutny)
* node: fix argument parsing with -p arg (Alexis Campailla)
* path: improve POSIX path.join() performance (Jo Liss)
* tls: emit `clientError` on early socket close (Fedor Indutny)
* tls: introduce `.setMaxSendFragment(size)` (Fedor Indutny)
* tls: make cert/pfx optional in tls.createServer() (Ben Noordhuis)
* tls: process accumulated input (Fedor Indutny)
* tls: show human-readable error messages (Ben Noordhuis)
* util: handle escaped forward slashes correctly (Tom Gallacher)

## 2013.12.31, Version 0.11.10 (Unstable)

https://github.com/nodejs/node/commit/66931791f06207d1cdfea5ec1529edf3c94026d3

* http_parser: update to 2.2
* uv: Upgrade to v0.11.17
* v8: Upgrade to 3.22.24.10
* buffer: optimize writeInt* methods (Paul Loyd)
* child_process: better error handling (Alexis Campailla)
* cluster: do not synchronously emit 'setup' event (Sam Roberts)
* cluster: restore backwards compatibility and various fixes (Sam Roberts)
* crypto: remove unnecessary OpenSSL_add_all_digests (Yorkie)
* crypto: support GCM authenticated encryption mode. (Ingmar Runge)
* dns: add resolveSoa and 'SOA' rrtype (Tuğrul Topuz)
* events: move EE c'tor guts to EventEmitter.init (Bert Belder)
* http: DELETE shouldn't default to chunked encoding (Lalit Kapoor)
* http: parse the status message in a http response. (Cam Swords)
* node: fix removing AsyncListener in callback (Vladimir Kurchatkin)
* node: follow specification, zero-fill ArrayBuffers (Trevor Norris)
* openssl: use ASM optimized routines (Fedor Indutny)
* process: allow nextTick infinite recursion (Trevor Norris)
* querystring: remove `name` from `stringify()` (Yorkie)
* timers: setImmediate v8 optimization fix (pflannery)
* tls: add serialNumber to getPeerCertificate() (Ben Noordhuis)
* tls: reintroduce socket.encrypted (Fedor Indutny)
* tls: fix handling of asterisk in SNI context (Fedor Indutny)
* util: Format negative zero as '-0' (David Chan)
* vm: fix race condition in timeout (Alexis Campailla)
* windows: fix dns lookup of localhost with ipv6 (Alexis Campailla)

## 2013.11.20, Version 0.11.9 (Unstable)

https://github.com/nodejs/node/commit/dcfd032bdd69dfb38c120e18438d6316ae522edc

* uv: upgrade to v0.11.15 (Timothy J Fontaine)
* v8: upgrade to 3.22.24.5 (Timothy J Fontaine)
* buffer: remove warning when no encoding is passed (Trevor Norris)
* build: make v8 use random seed for hash tables (Ben Noordhuis)
* crypto: build with shared openssl without NPN (Ben Noordhuis)
* crypto: update root certificates (Ben Noordhuis)
* debugger: pass on v8 debug switches (Ben Noordhuis)
* domain: use AsyncListener API (Trevor Norris)
* fs: add recursive subdirectory support to fs.watch (Nick Simmons)
* fs: make fs.watch() non-recursive by default (Ben Noordhuis)
* http: cleanup freeSockets when socket destroyed (fengmk2)
* http: force socket encoding to be null (isaacs)
* http: make DELETE requests set `req.method` (Nathan Rajlich)
* node: add AsyncListener support (Trevor Norris)
* src: remove global HandleScope that hid memory leaks (Ben Noordhuis)
* tls: add ECDH ciphers support (Erik Dubbelboer)
* tls: do not default to 'localhost' servername (Fedor Indutny)
* tls: more accurate wrapping of connecting socket (Fedor Indutny)

## 2013.10.30, Version 0.11.8 (Unstable)

https://github.com/nodejs/node/commit/f8d86e24f3463c36f7f3f4c3b3ec779e5b6201e1

* uv: Upgrade to v0.11.14
* v8: upgrade 3.21.18.3
* assert: indicate if exception message is generated (Glen Mailer)
* buffer: add buf.toArrayBuffer() API (Trevor Norris)
* cluster: fix premature 'disconnect' event (Ben Noordhuis)
* crypto: add SPKAC support (Jason Gerfen)
* debugger: count space for line numbers correctly (Alex Kocharin)
* debugger: make busy loops SIGUSR1-interruptible (Ben Noordhuis)
* debugger: repeat last command (Alex Kocharin)
* debugger: show current line, fix for [#6150](https://github.com/joyent/node/issues/6150) (Alex Kocharin)
* dgram: send() can accept strings (Trevor Norris)
* dns: rename domain to hostname (Ben Noordhuis)
* dns: set hostname property on error object (Ben Noordhuis)
* dtrace, mdb_v8: support more string, frame types (Dave Pacheco)
* http: add statusMessage (Patrik Stutz)
* http: expose supported methods (Ben Noordhuis)
* http: provide backpressure for pipeline flood (isaacs)
* process: Add exitCode property (isaacs)
* tls: socket.renegotiate(options, callback) (Fedor Indutny)
* util: format as Error if instanceof Error (Rod Vagg)

## 2013.08.21, Version 0.11.7 (Unstable)

https://github.com/nodejs/node/commit/be52549bfa5311208b5fcdb3ba09210460fa9ceb

* uv: upgrade to v0.11.13
* v8: upgrade to 3.20.17
* buffer: adhere to INSPECT_MAX_BYTES (Timothy J Fontaine)
* buffer: fix regression for large buffer creation (Trevor Norris)
* buffer: don't throw if slice length too long (Trevor Norris)
* buffer: Buffer(buf) constructor copies into the proper buffer (Ben Noordhuis)
* cli: remove --max-stack-size (Ben Noordhuis)
* cli: unknown command line options are errors (Ben Noordhuis)
* child_process: exec accept buffer as an encoding (Seth Fitzsimmons)
* crypto: make randomBytes/pbkdf2 callbacks domain aware (Ben Noordhuis)
* domain: deprecate domain.dispose(). (Forrest L Norvell)
* fs: Expose birthtime on stat objects (isaacs)
* http: Only send connection:keep-alive if necessary (isaacs)
* repl: Catch syntax errors better (isaacs, Nathan Rajlich)
* stream: change default highWaterMark for objectMode to 16 (Mathias Buus)
* stream: make setEncoding/pause/resume chainable (Julian Gruber, isaacs)
* util: pass opts to custom inspect functions (Timothy J Fontaine)
* vm: rewritten to behave like Contextify (Domenic Denicola)

## 2013.08.21, Version 0.11.6 (Unstable)

https://github.com/nodejs/node/commit/04018d4b3938fd30ba14822e79195e4af2be36f6

* uv: Upgrade to v0.11.8
* v8: upgrade v8 to 3.20.14.1
* build: disable SSLv2 by default (Ben Noordhuis)
* build: don't auto-destroy existing configuration (Ben Noordhuis)
* crypto: add TLS 1.1 and 1.2 to secureProtocol list (Matthias Bartelmeß)
* crypto: fix memory leak in randomBytes() error path (Ben Noordhuis)
* dgram: don't call into js when send cb is omitted (Ben Noordhuis)
* dgram: fix regression in string argument handling (Ben Noordhuis)
* domains: performance improvements (Trevor Norris)
* events: EventEmitter = require('events') (Jake Verbaten)
* http: Add write()/end() callbacks (isaacs)
* http: Consistent 'finish' event semantics (isaacs)
* http: Prefer 'binary' over 'ascii' (isaacs)
* http: Support legacy agent.addRequest API (isaacs)
* http: Write hex/base64 chunks properly (isaacs)
* http: add agent.maxFreeSockets option (isaacs)
* http: provide access to raw headers/trailers (isaacs)
* http: removed headers stay removed (James Halliday)
* http,timers: improve callback performance (Ben Noordhuis)
* net: family option in net.connect (Vsevolod Strukchinsky)
* readline: pause stdin before turning off terminal raw mode (Daniel Chatfield)
* smalloc: allow different external array types (Trevor Norris)
* smalloc: expose ExternalArraySize (Trevor Norris)
* stream: Short-circuit buffer pushes when flowing (isaacs)
* tls: handle errors on socket before releasing it (Fedor Indutny)
* util: fix isPrimitive check (Trevor Norris)
* util: isObject should always return boolean (Trevor Norris)

## 2013.08.06, Version 0.11.5 (Unstable)

https://github.com/nodejs/node/commit/6f92da2dd106b0c63fde563284f83e08e2a521b5

* v8: upgrade to 3.20.11
* uv: upgrade to v0.11.7
* buffer: return offset for end of last write (Trevor Norris)
* build: embed the mdb_v8.so into the binary (Timothy J Fontaine)
* build: fix --without-ssl build (Ben Noordhuis)
* child_process: add 'shell' option to .exec() (Ben Noordhuis)
* dgram: report send errors to cb, don't pass bytes (Ben Noordhuis)
* fs: write strings directly to disk (Trevor Norris)
* https: fix default port (Koichi Kobayashi)
* openssl: use asm for sha, md5, rmd (Fedor Indutny)
* os: add mac address to networkInterfaces() output (Brian White)
* smalloc: introduce smalloc module (Trevor Norris)
* stream: Simplify flowing, passive data listening (streams3) (isaacs)
* tls: asynchronous SNICallback (Fedor Indutny)
* tls: share tls tickets key between cluster workers (Fedor Indutny)
* util: don't throw on circular %j input to format() (Ben Noordhuis)

## 2013.07.12, Version 0.11.4 (Unstable)

https://github.com/nodejs/node/commit/b5b84197ed037918fd1a26e5cb87cce7c812ca55

* npm: Upgrade to 1.3.4
* v8: Upgrade to v3.20.2
* c-ares: Upgrade to piscisaureus/cares@805d153
* timers: setImmediate process full queue each turn (Ben Noordhuis)
* http: Add agent.get/request methods (isaacs)
* http: Proper KeepAlive behavior (isaacs)
* configure: fix the --without-ssl option (Nathan Rajlich)
* buffer: propagate originating parent (Trevor Norris)
* tls_wrap: return Error not throw for missing cert (Timothy J Fontaine)
* src: enable native v8 typed arrays (Ben Noordhuis)
* stream: objectMode transform should allow falsey values (Jeff Barczewski)
* slab_allocator: remove SlabAllocator (Trevor Norris)
* crypto: fix memory leak in LoadPKCS12 (Fedor Indutny)
* tls: export TLSSocket (Fedor Indutny)
* zlib: allow changing of level and strategy (Brian White)
* zlib: allow custom flush type for flush() (Brian White)

## 2013.06.26, Version 0.11.3 (Unstable)

https://github.com/nodejs/node/commit/38c0c47bbe280ddc42054418091571e532d82a1e

* uv: Upgrade to v0.11.5
* c-ares: upgrade to 1.10.0
* v8: upgrade to v3.19.13
* punycode: update to v1.2.3 (Mathias Bynens)
* debugger: break on uncaught exception (Miroslav Bajtos)
* child_process: emit 'disconnect' asynchronously (Ben Noordhuis)
* dtrace: enable uv's probes if enabled (Timothy J Fontaine)
* dtrace: unify dtrace and systemtap interfaces (Timothy J Fontaine)
* buffer: New API for backing data store (Trevor Norris)
* buffer: return `this` in fill() for chainability (Brian White)
* build: fix include order for building on windows (Timothy J Fontaine)
* build: add android support (Linus Mårtensson)
* readline: strip ctrl chars for prompt width calc (Krzysztof Chrapka)
* tls: introduce TLSSocket based on tls_wrap binding (Fedor Indutny)
* tls: add localAddress and localPort properties (Ben Noordhuis)
* crypto: free excessive memory in NodeBIO (Fedor Indutny)
* process: remove maxTickDepth (Trevor Norris)
* timers: use uv_now instead of Date.now (Timothy J Fontaine)
* util: Add debuglog, deprecate console lookalikes (isaacs)
* module: use path.sep instead of a custom solution (Robert Kowalski)
* http: don't escape request path, reject bad chars (Ben Noordhuis)
* net: emit dns 'lookup' event before connect (Ben Noordhuis)
* dns: add getServers and setServers (Timothy J Fontaine)

## 2013.05.13, Version 0.11.2 (Unstable)

https://github.com/nodejs/node/commit/5d3dc0e4c3369dfb00b7b13e08936c2e652fa696

* uv: Upgrade to 0.11.2
* V8: Upgrade to 3.19.0
* npm: Upgrade to 1.2.21
* build: Makefile should respect configure --prefix (Timothy J Fontaine)
* cluster: use round-robin load balancing (Ben Noordhuis)
* debugger, cluster: each worker has new debug port (Miroslav Bajtoš)
* debugger: `restart` with custom debug port (Miroslav Bajtoš)
* debugger: breakpoints in scripts not loaded yet (Miroslav Bajtoš)
* event: EventEmitter#setMaxListeners() returns this (Sam Roberts)
* events: add EventEmitter.defaultMaxListeners (Ben Noordhuis)
* install: Support $(PREFIX) install target directory prefix (Olof Johansson)
* os: Include netmask in os.networkInterfaces() (Ben Kelly)
* path: add path.isAbsolute(path) (Ryan Doenges)
* stream: Guarantee ordering of 'finish' event (isaacs)
* streams: introduce .cork/.uncork/._writev (Fedor Indutny)
* vm: add support for timeout argument (Andrew Paprocki)

## 2013.04.19, Version 0.11.1 (Unstable)

https://github.com/nodejs/node/commit/4babd2b46ebf9fbea2c9946af5cfae25a33b2b22

* V8: upgrade to 3.18.0
* uv: Upgrade to v0.11.1
* http: split into multiple separate modules (Timothy J Fontaine)
* http: escape unsafe characters in request path (Ben Noordhuis)
* url: Escape all unwise characters (isaacs)
* build: depend on v8 postmortem-metadata if enabled (Paddy Byers)
* etw: update prototypes to match dtrace provider (Timothy J Fontaine)
* buffer: change output of Buffer.prototype.toJSON() (David Braun)
* dtrace: actually use the _handle.fd value (Timothy J Fontaine)
* dtrace: pass more arguments to probes (Dave Pacheco)
* build: allow building with dtrace on osx (Dave Pacheco)
* zlib: allow passing options to convenience methods (Kyle Robinson Young)

## 2013.03.28, Version 0.11.0 (Unstable)

https://github.com/nodejs/node/commit/bce38b3d74e64fcb7d04a2dd551151da6168cdc5

* V8: update to 3.17.13
* os: use %SystemRoot% or %windir% in os.tmpdir() (Suwon Chae)
* util: fix util.inspect() line width calculation (Marcin Kostrzewa)
* buffer: remove _charsWritten (Trevor Norris)
* fs: uv_[fl]stat now reports subsecond resolution (Timothy J Fontaine)
* fs: Throw if error raised and missing callback (bnoordhuis)
* tls: expose SSL_CTX_set_timeout via tls.createServer (Manav Rathi)
* tls: remove harmful unnecessary bounds checking (Marcel Laverdet)
* buffer: write ascii strings using WriteOneByte (Trevor Norris)
* dtrace: fix generation of v8 constants on freebsd (Fedor Indutny)
* dtrace: x64 ustack helper (Fedor Indutny)
* readline: handle wide characters properly (Nao Iizuka)
* repl: Use a domain to catch async errors safely (isaacs)
* repl: emit 'reset' event when context is reset (Sami Samhuri)
* util: custom `inspect()` method may return an Object (Nathan Rajlich)
* console: `console.dir()` bypasses inspect() methods (Nathan Rajlich)

## 2014.12.22, Version 0.10.35 (Stable)

* tls: re-add 1024-bit SSL certs removed by f9456a2 (Chris Dickinson)
* timers: don't close interval timers when unrefd (Julien Gilli)
* timers: don't mutate unref list while iterating it (Julien Gilli)

## 2014.12.17, Version 0.10.34 (Stable)

https://github.com/nodejs/node/commit/52795f8fcc2de77cf997e671ea58614e5e425dfe

* uv: update to v0.10.30
* zlib: upgrade to v1.2.8
* child_process: check execFile args is an array (Sam Roberts)
* child_process: check fork args is an array (Sam Roberts)
* crypto: update root certificates (Ben Noordhuis)
* domains: fix issues with abort on uncaught (Julien Gilli)
* timers: Avoid linear scan in _unrefActive. (Julien Gilli)
* timers: fix unref() memory leak (Trevor Norris)
* v8: add api for aborting on uncaught exception (Julien Gilli)
* debugger: fix when using "use strict" (Julien Gilli)

## 2014.10.20, Version 0.10.33 (Stable)

https://github.com/nodejs/node/commit/8d045a30e95602b443eb259a5021d33feb4df079

* openssl: Update to 1.0.1j (Addressing multiple CVEs)
* uv: Update to v0.10.29
* child_process: properly support optional args (cjihrig)
* crypto: Disable autonegotiation for SSLv2/3 by default (Fedor Indutny,
	Timothy J Fontaine, Alexis Campailla)
	This is a behavior change, by default we will not allow the negotiation to
	SSLv2 or SSLv3. If you want this behavior, run Node.js with either
	`--enable-ssl2` or `--enable-ssl3` respectively.
	This does not change the behavior for users specifically requesting
	`SSLv2_method` or `SSLv3_method`. While this behavior is not advised, it is
	assumed you know what you're doing since you're specifically asking to use
	these methods.

## 2014.09.16, Version 0.10.32 (Stable)

https://github.com/nodejs/node/commit/0fe0d121551593c23a565db8397f85f17bb0f00e

* npm: Update to 1.4.28
* v8: fix a crash introduced by previous release (Fedor Indutny)
* configure: add --openssl-no-asm flag (Fedor Indutny)
* crypto: use domains for any callback-taking method (Chris Dickinson)
* http: do not send `0\r\n\r\n` in TE HEAD responses (Fedor Indutny)
* querystring: fix unescape override (Tristan Berger)
* url: Add support for RFC 3490 separators (Mathias Bynens)

## 2014.08.19, Version 0.10.31 (Stable)

https://github.com/nodejs/node/commit/7fabdc23d843cb705d2d0739e7bbdaaf50aa3292

* v8: backport CVE-2013-6668
* openssl: Update to v1.0.1i
* npm: Update to v1.4.23
* cluster: disconnect should not be synchronous (Sam Roberts)
* fs: fix fs.readFileSync fd leak when get RangeError (Jackson Tian)
* stream: fix Readable.wrap objectMode falsy values (James Halliday)
* timers: fix timers with non-integer delay hanging. (Julien Gilli)

## 2014.07.31, Version 0.10.30 (Stable)

https://github.com/nodejs/node/commit/bc0ff830aff1e016163d855e86ded5c98b0899e8

* uv: Upgrade to v0.10.28
* npm: Upgrade to v1.4.21
* v8: Interrupts must not mask stack overflow.
* Revert "stream: start old-mode read in a next tick" (Fedor Indutny)
* buffer: fix sign overflow in `readUIn32BE` (Fedor Indutny)
* buffer: improve {read,write}{U}Int* methods (Nick Apperson)
* child_process: handle writeUtf8String error (Fedor Indutny)
* deps: backport 4ed5fde4f from v8 upstream (Fedor Indutny)
* deps: cherry-pick eca441b2 from OpenSSL (Fedor Indutny)
* lib: remove and restructure calls to isNaN() (cjihrig)
* module: eliminate double `getenv()` (Maciej Małecki)
* stream2: flush extant data on read of ended stream (Chris Dickinson)
* streams: remove unused require('assert') (Rod Vagg)
* timers: backport f8193ab (Julien Gilli)
* util.h: interface compatibility (Oguz Bastemur)
* zlib: do not crash on write after close (Fedor Indutny)

## 2014.06.05, Version 0.10.29 (Stable)

https://github.com/nodejs/node/commit/ce82d6b8474bde7ac7df6d425fb88fb1bcba35bc

* openssl: to 1.0.1h (CVE-2014-0224)
* npm: upgrade to 1.4.14
* utf8: Prevent Node from sending invalid UTF-8 (Felix Geisendörfer)
  - *NOTE* this introduces a breaking change, previously you could construct
    invalid UTF-8 and invoke an error in a client that was expecting valid
    UTF-8, now unmatched surrogate pairs are replaced with the unknown UTF-8
    character. To restore the old functionality simply have NODE_INVALID_UTF8
    environment variable set.

* child_process: do not set args before throwing (Greg Sabia Tucker)
* child_process: spawn() does not throw TypeError (Greg Sabia Tucker)
* constants: export O_NONBLOCK (Fedor Indutny)
* crypto: improve memory usage (Alexis Campailla)
* fs: close file if fstat() fails in readFile() (cjihrig)
* lib: name EventEmitter prototype methods (Ben Noordhuis)
* tls: fix performance issue (Alexis Campailla)

## 2014.05.01, Version 0.10.28 (Stable)

https://github.com/nodejs/node/commit/b148cbe09d4657766fdb61575ba985734c2ff0a8

* npm: upgrade to v1.4.9

## 2014.05.01, Version 0.10.27 (Stable)

https://github.com/nodejs/node/commit/cb7911f78ae96ef7a540df992cc1359ba9636e86

* npm: upgrade to v1.4.8
* openssl: upgrade to 1.0.1g
* uv: update to v0.10.27
* dns: fix certain txt entries (Fedor Indutny)
* assert: Ensure reflexivity of deepEqual (Mike Pennisi)
* child_process: fix deadlock when sending handles (Fedor Indutny)
* child_process: fix sending handle twice (Fedor Indutny)
* crypto: do not lowercase cipher/hash names (Fedor Indutny)
* dtrace: workaround linker bug on FreeBSD (Fedor Indutny)
* http: do not emit EOF non-readable socket (Fedor Indutny)
* http: invoke createConnection when no agent (Nathan Rajlich)
* stream: remove useless check (Brian White)
* timer: don't reschedule timer bucket in a domain (Greg Brail)
* url: treat \ the same as / (isaacs)
* util: format as Error if instanceof Error (Rod Vagg)

## 2014.02.18, Version 0.10.26 (Stable)

https://github.com/nodejs/node/commit/cc56c62ed879ad4f93b1fdab3235c43e60f48b7e

* uv: Upgrade to v0.10.25 (Timothy J Fontaine)
* npm: upgrade to 1.4.3 (isaacs)
* v8: support compiling with VS2013 (Fedor Indutny)
* cares: backport TXT parsing fix (Fedor Indutny)
* crypto: throw on SignFinal failure (Fedor Indutny)
* crypto: update root certificates (Ben Noordhuis)
* debugger: Fix breakpoint not showing after restart (Farid Neshat)
* fs: make unwatchFile() insensitive to path (iamdoron)
* net: do not re-emit stream errors (Fedor Indutny)
* net: make Socket destroy() re-entrance safe (Jun Ma)
* net: reset `endEmitted` on reconnect (Fedor Indutny)
* node: do not close stdio implicitly (Fedor Indutny)
* zlib: avoid assertion in close (Fedor Indutny)

## 2014.01.23, Version 0.10.25 (Stable)

https://github.com/nodejs/node/commit/b0e5f195dfce3e2b99f5091373d49f6616682596

* uv: Upgrade to v0.10.23
* npm: Upgrade to v1.3.24
* v8: Fix enumeration for objects with lots of properties
* child_process: fix spawn() optional arguments (Sam Roberts)
* cluster: report more errors to workers (Fedor Indutny)
* domains: exit() only affects active domains (Ryan Graham)
* src: OnFatalError handler must abort() (Timothy J Fontaine)
* stream: writes may return false but forget to emit drain (Yang Tianyang)

## 2013.12.18, Version 0.10.24 (Stable)

https://github.com/nodejs/node/commit/b7fd6bc899ccb629d790c47aee06aba87e535c41

* uv: Upgrade to v0.10.21
* npm: upgrade to 1.3.21
* v8: backport fix for CVE-2013-{6639|6640}
* build: unix install node and dep library headers (Timothy J Fontaine)
* cluster, v8: fix --logfile=%p.log (Ben Noordhuis)
* module: only cache package main (Wyatt Preul)

## 2013.12.12, Version 0.10.23 (Stable)

https://github.com/nodejs/node/commit/0462bc23564e7e950a70ae4577a840b04db6c7c6

* uv: Upgrade to v0.10.20 (Timothy J Fontaine)
* npm: Upgrade to 1.3.17 (isaacs)
* gyp: update to 78b26f7 (Timothy J Fontaine)
* build: include postmortem symbols on linux (Timothy J Fontaine)
* crypto: Make Decipher._flush() emit errors. (Kai Groner)
* dgram: fix abort when getting `fd` of closed dgram (Fedor Indutny)
* events: do not accept NaN in setMaxListeners (Fedor Indutny)
* events: avoid calling `once` functions twice (Tim Wood)
* events: fix TypeError in removeAllListeners (Jeremy Martin)
* fs: report correct path when EEXIST (Fedor Indutny)
* process: enforce allowed signals for kill (Sam Roberts)
* tls: emit 'end' on .receivedShutdown (Fedor Indutny)
* tls: fix potential data corruption (Fedor Indutny)
* tls: handle `ssl.start()` errors appropriately (Fedor Indutny)
* tls: reset NPN callbacks after SNI (Fedor Indutny)

## 2013.11.12, Version 0.10.22 (Stable)

https://github.com/nodejs/node/commit/cbff8f091c22fb1df6b238c7a1b9145db950fa65

* npm: Upgrade to 1.3.14
* uv: Upgrade to v0.10.19
* child_process: don't assert on stale file descriptor events (Fedor Indutny)
* darwin: Fix "Not Responding" in Mavericks activity monitor (Fedor Indutny)
* debugger: Fix bug in sb() with unnamed script (Maxim Bogushevich)
* repl: do not insert duplicates into completions (Maciej Małecki)
* src: Fix memory leak on closed handles (Timothy J Fontaine)
* tls: prevent stalls by using read(0) (Fedor Indutny)
* v8: use correct timezone information on Solaris (Maciej Małecki)

## 2013.10.18, Version 0.10.21 (Stable)

https://github.com/nodejs/node/commit/e2da042844a830fafb8031f6c477eb4f96195210

* uv: Upgrade to v0.10.18
* crypto: clear errors from verify failure (Timothy J Fontaine)
* dtrace: interpret two byte strings (Dave Pacheco)
* fs: fix fs.truncate() file content zeroing bug (Ben Noordhuis)
* http: provide backpressure for pipeline flood (isaacs)
* tls: fix premature connection termination (Ben Noordhuis)

## 2013.09.30, Version 0.10.20 (Stable)

https://github.com/nodejs/node/commit/d7234c8d50a1af73f60d2d3c0cc7eed17429a481

* tls: fix sporadic hang and partial reads (Fedor Indutny)
  - fixes "npm ERR! cb() never called!"

## 2013.09.24, Version 0.10.19 (Stable)

https://github.com/nodejs/node/commit/6b5e6a5a3ec8d994c9aab3b800b9edbf1b287904

* uv: Upgrade to v0.10.17
* npm: upgrade to 1.3.11
* readline: handle input starting with control chars (Eric Schrock)
* configure: add mips-float-abi (soft, hard) option (Andrei Sedoi)
* stream: objectMode transforms allow falsey values (isaacs)
* tls: prevent duplicate values returned from read (Nathan Rajlich)
* tls: NPN protocols are now local to connections (Fedor Indutny)

## 2013.09.04, Version 0.10.18 (Stable)

https://github.com/nodejs/node/commit/67a1f0c52e0708e2596f3f2134b8386d6112561e

* uv: Upgrade to v0.10.15
* stream: Don't crash on unset _events property (isaacs)
* stream: Pass 'buffer' encoding with decoded writable chunks (isaacs)

## 2013.08.21, Version 0.10.17 (Stable)

https://github.com/nodejs/node/commit/469a4a5091a677df62be319675056b869c31b35c

* uv: Upgrade v0.10.14
* http_parser: Do not accept PUN/GEM methods as PUT/GET (Chris Dickinson)
* tls: fix assertion when ssl is destroyed at read (Fedor Indutny)
* stream: Throw on 'error' if listeners removed (isaacs)
* dgram: fix assertion on bad send() arguments (Ben Noordhuis)
* readline: pause stdin before turning off terminal raw mode (Daniel Chatfield)

## 2013.08.16, Version 0.10.16 (Stable)

https://github.com/nodejs/node/commit/50b4c905a4425430ae54db4906f88982309e128d

* v8: back-port fix for CVE-2013-2882
* npm: Upgrade to 1.3.8
* crypto: fix assert() on malformed hex input (Ben Noordhuis)
* crypto: fix memory leak in randomBytes() error path (Ben Noordhuis)
* events: fix memory leak, don't leak event names (Ben Noordhuis)
* http: Handle hex/base64 encodings properly (isaacs)
* http: improve chunked res.write(buf) performance (Ben Noordhuis)
* stream: Fix double pipe error emit (Eran Hammer)

## 2013.07.25, Version 0.10.15 (Stable)

https://github.com/nodejs/node/commit/2426d65af860bda7be9f0832a99601cc43c6cf63

* src: fix process.getuid() return value (Ben Noordhuis)

## 2013.07.25, Version 0.10.14 (Stable)

https://github.com/nodejs/node/commit/fdf57f811f9683a4ec49a74dc7226517e32e6c9d

* uv: Upgrade to v0.10.13
* npm: Upgrade to v1.3.5
* os: Don't report negative times in cpu info (Ben Noordhuis)
* fs: Handle large UID and GID (Ben Noordhuis)
* url: Fix edge-case when protocol is non-lowercase (Shuan Wang)
* doc: Streams API Doc Rewrite (isaacs)
* node: call MakeDomainCallback in all domain cases (Trevor Norris)
* crypto: fix memory leak in LoadPKCS12 (Fedor Indutny)

## 2013.07.09, Version 0.10.13 (Stable)

https://github.com/nodejs/node/commit/e32660a984427d46af6a144983cf7b8045b7299c

* uv: Upgrade to v0.10.12
* npm: Upgrade to 1.3.2
* windows: get proper errno (Ben Noordhuis)
* tls: only wait for finish if we haven't seen it (Timothy J Fontaine)
* http: Dump response when request is aborted (isaacs)
* http: use an unref'd timer to fix delay in exit (Peter Rust)
* zlib: level can be negative (Brian White)
* zlib: allow zero values for level and strategy (Brian White)
* buffer: add comment explaining buffer alignment (Ben Noordhuis)
* string_bytes: properly detect 64bit (Timothy J Fontaine)
* src: fix memory leak in UsingDomains() (Ben Noordhuis)

## 2013.06.18, Version 0.10.12 (Stable)

https://github.com/nodejs/node/commit/a088cf4f930d3928c97d239adf950ab43e7794aa

* npm: Upgrade to 1.2.32
* readline: make `ctrl + L` clear the screen (Yuan Chuan)
* v8: add setVariableValue debugger command (Ben Noordhuis)
* net: Do not destroy socket mid-write (isaacs)
* v8: fix build for mips32r2 architecture (Andrei Sedoi)
* configure: fix cross-compilation host_arch_cc() (Andrei Sedoi)

## 2013.06.13, Version 0.10.11 (Stable)

https://github.com/nodejs/node/commit/d9d5bc465450ae5d60da32e9ffcf71c2767f1fad

* uv: upgrade to 0.10.11
* npm: Upgrade to 1.2.30
* openssl: add missing configuration pieces for MIPS (Andrei Sedoi)
* Revert "http: remove bodyHead from 'upgrade' events" (isaacs)
* v8: fix pointer arithmetic undefined behavior (Trevor Norris)
* crypto: fix utf8/utf-8 encoding check (Ben Noordhuis)
* net: Fix busy loop on POLLERR|POLLHUP on older linux kernels (Ben Noordhuis, isaacs)

## 2013.06.04, Version 0.10.10 (Stable)

https://github.com/nodejs/node/commit/25e51c396aa23018603baae2b1d9390f5d9db496

* uv: Upgrade to 0.10.10
* npm: Upgrade to 1.2.25
* url: Properly parse certain oddly formed urls (isaacs)
* stream: unshift('') is a noop (isaacs)

## 2013.05.30, Version 0.10.9 (Stable)

https://github.com/nodejs/node/commit/878ffdbe6a8eac918ef3a7f13925681c3778060b

* npm: Upgrade to 1.2.24
* uv: Upgrade to v0.10.9
* repl: fix JSON.parse error check (Brian White)
* tls: proper .destroySoon (Fedor Indutny)
* tls: invoke write cb only after opposite read end (Fedor Indutny)
* tls: ignore .shutdown() syscall error (Fedor Indutny)

## 2013.05.24, Version 0.10.8 (Stable)

https://github.com/nodejs/node/commit/30d9e9fdd9d4c33d3d95a129d021cd8b5b91eddb

* v8: update to 3.14.5.9
* uv: upgrade to 0.10.8
* npm: Upgrade to 1.2.23
* http: remove bodyHead from 'upgrade' events (Nathan Zadoks)
* http: Return true on empty writes, not false (isaacs)
* http: save roundtrips, convert buffers to strings (Ben Noordhuis)
* configure: respect the --dest-os flag consistently (Nathan Rajlich)
* buffer: throw when writing beyond buffer (Trevor Norris)
* crypto: Clear error after DiffieHellman key errors (isaacs)
* string_bytes: strip padding from base64 strings (Trevor Norris)

## 2013.05.17, Version 0.10.7 (Stable)

https://github.com/nodejs/node/commit/d2fdae197ac542f686ee06835d1153dd43b862e5

* uv: upgrade to v0.10.7
* npm: Upgrade to 1.2.21
* crypto: Don't ignore verify encoding argument (isaacs)
* buffer, crypto: fix default encoding regression (Ben Noordhuis)
* timers: fix setInterval() assert (Ben Noordhuis)

## 2013.05.14, Version 0.10.6 (Stable)

https://github.com/nodejs/node/commit/5deb1672f2b5794f8be19498a425ea4dc0b0711f

* module: Deprecate require.extensions (isaacs)
* stream: make Readable.wrap support objectMode, empty streams (Daniel Moore)
* child_process: fix handle delivery (Ben Noordhuis)
* crypto: Fix performance regression (isaacs)
* src: DRY string encoding/decoding (isaacs)

## 2013.04.23, Version 0.10.5 (Stable)

https://github.com/nodejs/node/commit/deeaf8fab978e3cadb364e46fb32dafdebe5f095

* uv: Upgrade to 0.10.5 (isaacs)
* build: added support for Visual Studio 2012 (Miroslav Bajtoš)
* http: Don't try to destroy nonexistent sockets (isaacs)
* crypto: LazyTransform on properties, not methods (isaacs)
* assert: put info in err.message, not err.name (Ryan Doenges)
* dgram: fix no address bind() (Ben Noordhuis)
* handle_wrap: fix NULL pointer dereference (Ben Noordhuis)
* os: fix unlikely buffer overflow in os.type() (Ben Noordhuis)
* stream: Fix unshift() race conditions (isaacs)

## 2013.04.11, Version 0.10.4 (Stable)

https://github.com/nodejs/node/commit/9712aa9f76073c30850b20a188b1ed12ffb74d17

* uv: Upgrade to 0.10.4
* npm: Upgrade to 1.2.18
* v8: Avoid excessive memory growth in JSON.parse (Fedor Indutny)
* child_process, cluster: fix O(n*m) scan of cmd string (Ben Noordhuis)
* net: fix socket.bytesWritten Buffers support (Fedor Indutny)
* buffer: fix offset checks (Łukasz Walukiewicz)
* stream: call write cb before finish event (isaacs)
* http: Support write(data, 'hex') (isaacs)
* crypto: dh secret should be left-padded (Fedor Indutny)
* process: expose NODE_MODULE_VERSION in process.versions (Rod Vagg)
* crypto: fix constructor call in crypto streams (Andreas Madsen)
* net: account for encoding in .byteLength (Fedor Indutny)
* net: fix buffer iteration in bytesWritten (Fedor Indutny)
* crypto: zero is not an error if writing 0 bytes (Fedor Indutny)
* tls: Re-enable check of CN-ID in cert verification (Tobias Müllerleile)

## 2013.04.03, Version 0.10.3 (Stable)

https://github.com/nodejs/node/commit/d4982f6f5e4a9a703127489a553b8d782997ea43

* npm: Upgrade to 1.2.17
* child_process: acknowledge sent handles (Fedor Indutny)
* etw: update prototypes to match dtrace provider (Timothy J Fontaine)
* dtrace: pass more arguments to probes (Dave Pacheco)
* build: allow building with dtrace on osx (Dave Pacheco)
* http: Remove legacy ECONNRESET workaround code (isaacs)
* http: Ensure socket cleanup on client response end (isaacs)
* tls: Destroy socket when encrypted side closes (isaacs)
* repl: isSyntaxError() catches "strict mode" errors (Nathan Rajlich)
* crypto: Pass options to ctor calls (isaacs)
* src: tie process.versions.uv to uv_version_string() (Ben Noordhuis)

## 2013.03.28, Version 0.10.2 (Stable)

https://github.com/nodejs/node/commit/1e0de9c426e07a260bbec2d2196c2d2db8eb8886

* npm: Upgrade to 1.2.15
* uv: Upgrade to 0.10.3
* tls: handle SSL_ERROR_ZERO_RETURN (Fedor Indutny)
* tls: handle errors before calling C++ methods (Fedor Indutny)
* tls: remove harmful unnecessary bounds checking (Marcel Laverdet)
* crypto: make getCiphers() return non-SSL ciphers (Ben Noordhuis)
* crypto: check randomBytes() size argument (Ben Noordhuis)
* timers: do not calculate Timeout._when property (Alexey Kupershtokh)
* timers: fix off-by-one ms error (Alexey Kupershtokh)
* timers: handle signed int32 overflow in enroll() (Fedor Indutny)
* stream: Fix stall in Transform under very specific conditions (Gil Pedersen)
* stream: Handle late 'readable' event listeners (isaacs)
* stream: Fix early end in Writables on zero-length writes (isaacs)
* domain: fix domain callback from MakeCallback (Trevor Norris)
* child_process: don't emit same handle twice (Ben Noordhuis)
* child_process: fix sending utf-8 to child process (Ben Noordhuis)

## 2013.03.21, Version 0.10.1 (Stable)

https://github.com/nodejs/node/commit/c274d1643589bf104122674a8c3fd147527a667d

* npm: upgrade to 1.2.15
* crypto: Improve performance of non-stream APIs (Fedor Indutny)
* tls: always reset this.ssl.error after handling (Fedor Indutny)
* tls: Prevent mid-stream hangs (Fedor Indutny, isaacs)
* net: improve arbitrary tcp socket support (Ben Noordhuis)
* net: handle 'finish' event only after 'connect' (Fedor Indutny)
* http: Don't hot-path end() for large buffers (isaacs)
* fs: Missing cb errors are deprecated, not a throw (isaacs)
* fs: make write/appendFileSync correctly set file mode (Raymond Feng)
* stream: Return self from readable.wrap (isaacs)
* stream: Never call decoder.end() multiple times (Gil Pedersen)
* windows: enable watching signals with process.on('SIGXYZ') (Bert Belder)
* node: revert removal of MakeCallback (Trevor Norris)
* node: Unwrap without aborting in handle fd getter (isaacs)

## 2013.03.11, Version 0.10.0 (Stable)

https://github.com/nodejs/node/commit/163ca274230fce536afe76c64676c332693ad7c1

* npm: Upgrade to 1.2.14
* core: Append filename properly in dlopen on windows (isaacs)
* zlib: Manage flush flags appropriately (isaacs)
* domains: Handle errors thrown in nested error handlers (isaacs)
* buffer: Strip high bits when converting to ascii (Ben Noordhuis)
* win/msi: Enable modify and repair (Bert Belder)
* win/msi: Add feature selection for various node parts (Bert Belder)
* win/msi: use consistent registry key paths (Bert Belder)
* child_process: support sending dgram socket (Andreas Madsen)
* fs: Raise EISDIR on Windows when calling fs.read/write on a dir (isaacs)
* unix: fix strict aliasing warnings, macro-ify functions (Ben Noordhuis)
* unix: honor UV_THREADPOOL_SIZE environment var (Ben Noordhuis)
* win/tty: fix typo in color attributes enumeration (Bert Belder)
* win/tty: don't touch insert mode or quick edit mode (Bert Belder)

## 2013.03.06, Version 0.9.12 (Unstable)

https://github.com/nodejs/node/commit/0debf5a82934da805592b6496756cdf27c993abc

* stream: Allow strings in Readable.push/unshift (isaacs)
* stream: Remove bufferSize option (isaacs)
* stream: Increase highWaterMark on large reads (isaacs)
* stream: _write: takes an encoding argument (isaacs)
* stream: _transform: remove output() method, provide encoding (isaacs)
* stream: Don't require read(0) to emit 'readable' event (isaacs)
* node: Add --throw-deprecation (isaacs)
* http: fix multiple timeout events (Eugene Girshov)
* http: More useful setTimeout API on server (isaacs)
* net: use close callback, not process.nextTick (Ben Noordhuis)
* net: Provide better error when writing after FIN (isaacs)
* dns: Support NAPTR queries (Pavel Lang)
* dns: fix ReferenceError in resolve() error path (Xidorn Quan)
* child_process: handle ENOENT correctly on Windows (Scott Blomquist)
* cluster: Rename destroy() to kill(signal=SIGTERM) (isaacs)
* build: define nightly tag external to build system (Timothy J Fontaine)
* build: make msi build work when spaces are present in the path (Bert Belder)
* build: fix msi build issue with WiX 3.7/3.8 (Raymond Feng)
* repl: make compatible with domains (Dave Olszewski)
* events: Code cleanup and performance improvements (Trevor Norris)

## 2013.03.01, Version 0.9.11 (Unstable)

https://github.com/nodejs/node/commit/83392403b7a9b7782b37c17688938c75010f81ba

* V8: downgrade to 3.14.5
* openssl: update to 1.0.1e
* darwin: Make process.title work properly (Ben Noordhuis)
* fs: Support mode/flag options to read/append/writeFile (isaacs)
* stream: _read() no longer takes a callback (isaacs)
* stream: Add stream.unshift(chunk) (isaacs)
* stream: remove lowWaterMark feature (isaacs)
* net: omit superfluous 'connect' event (Ben Noordhuis)
* build, windows: disable SEH (Ben Noordhuis)
* core: remove errno global (Ben Noordhuis)
* core: Remove the nextTick for running the main file (isaacs)
* core: Mark exit() calls with status codes (isaacs)
* core: Fix debug signal handler race condition lock (isaacs)
* crypto: clear error stack (Ben Noordhuis)
* test: optionally set common.PORT via env variable (Timothy J Fontaine)
* path: Throw TypeError on non-string args to path.resolve/join (isaacs, Arianit Uka)
* crypto: fix uninitialized memory access in openssl (Ben Noordhuis)

## 2013.02.19, Version 0.9.10 (Unstable)

* V8: Upgrade to 3.15.11.15
* npm: Upgrade to 1.2.12
* fs: Change default WriteStream config, increase perf (isaacs)
* process: streamlining tick callback logic (Trevor Norris)
* stream_wrap, udp_wrap: add read-only fd property (Ben Noordhuis)
* buffer: accept negative indices in Buffer#slice() (Ben Noordhuis)
* tls: Cycle data when underlying socket drains (isaacs)
* stream: read(0) should not always trigger _read(n,cb) (isaacs)
* stream: Empty strings/buffers do not signal EOF any longer (isaacs)
* crypto: improve cipher/decipher error messages (Ben Noordhuis)
* net: Respect the 'readable' flag on sockets (isaacs)
* net: don't suppress ECONNRESET (Ben Noordhuis)
* typed arrays: copy Buffer in typed array constructor (Ben Noordhuis)
* typed arrays: make DataView throw on non-ArrayBuffer (Ben Noordhuis)
* windows: MSI installer enhancements (Scott Blomquist, Jim Schubert)

## 2013.02.07, Version 0.9.9 (Unstable)

https://github.com/nodejs/node/commit/4b9f0d190cd6b22853caeb0e07145a98ce1d1d7f

* tls: port CryptoStream to streams2 (Fedor Indutny)
* typed arrays: only share ArrayBuffer backing store (Ben Noordhuis)
* stream: make Writable#end() accept a callback function (Nathan Rajlich)
* buffer: optimize 'hex' handling (Ben Noordhuis)
* dns, cares: don't filter NOTIMP, REFUSED, SERVFAIL (Ben Noordhuis)
* readline: treat bare \r as a line ending (isaacs)
* readline: make \r\n emit one 'line' event (Ben Noordhuis)
* cluster: support datagram sockets (Bert Belder)
* stream: Correct Transform class backpressure (isaacs)
* addon: Pass module object to NODE_MODULE init function (isaacs, Rod Vagg)
* buffer: slow buffer copy compatibility fix (Trevor Norris)
* Add bytesWritten to tls.CryptoStream (Andy Burke)

## 2013.01.24, Version 0.9.8 (Unstable)

https://github.com/nodejs/node/commit/5f2f8400f665dc32c3e10e7d31d53d756ded9156

* npm: Upgrade to v1.2.3
* V8: Upgrade to 3.15.11.10
* streams: Support objects other than Buffers (Jake Verbaten)
* buffer: remove float write range checks (Trevor Norris)
* http: close connection on 304/204 responses with chunked encoding (Ben Noordhuis)
* build: fix build with dtrace support on FreeBSD (Fedor Indutny)
* console: Support formatting options in trace() (isaacs)
* domain: empty stack on all exceptions (Dave Olszewski)
* unix, windows: make uv_*_bind() error codes consistent (Andrius Bentkus)
* linux: add futimes() fallback (Ben Noordhuis)

## 2013.01.18, Version 0.9.7 (Unstable)

https://github.com/nodejs/node/commit/9e7bebeb8305edd55735a95955a98fdbe47572e5

* V8: Upgrade to 3.15.11.7
* npm: Upgrade to 1.2.2
* punycode: Upgrade to 1.2.0 (Mathias Bynens)
* repl: make built-in modules available by default (Felix Böhm)
* windows: add support for '_Total' perf counters (Scott Blomquist)
* cluster: make --prof work for workers (Ben Noordhuis)
* child_process: do not keep list of sent sockets (Fedor Indutny)
* tls: Follow RFC6125 more strictly (Fedor Indutny)
* buffer: floating point read/write improvements (Trevor Norris)
* TypedArrays: Improve dataview perf without endian param (Dean McNamee)
* module: assert require() called with a non-empty string (Felix Böhm, James Campos)
* stdio: Set readable/writable flags properly (isaacs)
* stream: Properly handle large reads from push-streams (isaacs)

## 2013.01.11, Version 0.9.6 (Unstable)

https://github.com/nodejs/node/commit/9313fdc71ca8335d5e3a391c103230ee6219b3e2

* V8: update to 3.15.11.5
* node: remove ev-emul.h (Ben Noordhuis)
* path: make basename and extname ignore trailing slashes (Bert Belder)
* typed arrays: fix sunos signed/unsigned char issue (Ben Noordhuis)
* child_process: Fix {stdio:'inherit'} regression (Ben Noordhuis)
* child_process: Fix pipe() from child stdio streams  (Maciej Małecki)
* child_process: make fork() execPath configurable (Bradley Meck)
* stream: Add readable.push(chunk) method (isaacs)
* dtrace: x64 ustack helper (Fedor Indutny)
* repl: fix floating point number parsing (Nirk Niggler)
* repl: allow overriding builtins (Ben Noordhuis)
* net: add localAddress and localPort to Socket (James Hight)
* fs: make pool size coincide with ReadStream bufferSize (Shigeki Ohtsu)
* typed arrays: implement load and store swizzling (Dean McNamee)
* windows: fix perfctr crash on XP and 2003 (Scott Blomquist)
* dgram: fix double implicit bind error (Ben Noordhuis)

## 2012.12.30, Version 0.9.5 (Unstable)

https://github.com/nodejs/node/commit/01994e8119c24f2284bac0779b32acb49c95bee7

* assert: improve support for new execution contexts (lukebayes)
* domain: use camelCase instead of snake_case (isaacs)
* domain: Do not use uncaughtException handler (isaacs)
* fs: make 'end' work with ReadStream without 'start' (Ben Noordhuis)
* https: optimize createConnection() (Ryunosuke SATO)
* buffer: speed up base64 encoding by 20% (Ben Noordhuis)
* doc: Colorize API stability index headers in docs (Luke Arduini)
* net: socket.readyState corrections (bentaber)
* http: Performance enhancements for http under streams2 (isaacs)
* stream: fix to emit end event on http.ClientResponse (Shigeki Ohtsu)
* stream: fix event handler leak in readstream pipe and unpipe (Andreas Madsen)
* build: Support ./configure --tag switch (Maciej Małecki)
* repl: don't touch `require.cache` (Nathan Rajlich)
* node: Emit 'exit' event when exiting for an uncaught exception (isaacs)

## 2012.12.21, Version 0.9.4 (Unstable)

https://github.com/nodejs/node/commit/d86d83c75f6343b5368bb7bd328b4466a035e1d4

* streams: Update all streaming interfaces to use new classes (isaacs)
* node: remove idle gc (Ben Noordhuis)
* http: protect against response splitting attacks (Bert Belder)
* fs: Raise error when null bytes detected in paths (isaacs)
* fs: fix 'object is not a function' callback errors (Ben Noordhuis)
* fs: add autoClose=true option to fs.createReadStream (Farid Neshat)
* process: add getgroups(), setgroups(), initgroups() (Ben Noordhuis)
* openssl: optimized asm code on x86 and x64 (Bert Belder)
* crypto: fix leak in GetPeerCertificate (Fedor Indutny)
* add systemtap support (Jan Wynholds)
* windows: add ETW and PerfCounters support (Scott Blomquist)
* windows: fix normalization of UNC paths (Bert Belder)
* crypto: fix ssl error handling (Sergey Kholodilov)
* node: remove eio-emul.h (Ben Noordhuis)
* os: add os.endianness() function (Nathan Rajlich)
* readline: don't emit "line" events with a trailing '\n' char (Nathan Rajlich)
* build: add configure option to generate xcode build files (Timothy J Fontaine)
* build: allow linking against system libuv, cares, http_parser (Stephen Gallagher)
* typed arrays: add slice() support to ArrayBuffer (Anthony Pesch)
* debugger: exit and kill child on SIGTERM or SIGHUP (Fedor Indutny)
* url: url.format escapes delimiters in path and query (J. Lee Coltrane)

## 2012.10.24, Version 0.9.3 (Unstable)

https://github.com/nodejs/node/commit/1ed4c6776e4f52956918b70565502e0f8869829d

* V8: Upgrade to 3.13.7.4
* crypto: Default to buffers instead of binary strings (isaacs, Fedor Indutny)
* crypto: add getHashes() and getCiphers() (Ben Noordhuis)
* unix: add custom thread pool, remove libeio (Ben Noordhuis)
* util: make `inspect()` accept an "options" argument (Nathan Rajlich)
* https: fix renegotation attack protection (Ben Noordhuis)
* cluster: make 'listening' handler see actual port (Aaditya Bhatia)
* windows: use USERPROFILE to get the user's home dir (Bert Belder)
* path: add platform specific path delimiter (Paul Serby)
* http: add response.headersSent property (Pavel Lang)
* child_process: make .fork()'d child auto-exit (Ben Noordhuis)
* events: add 'removeListener' event (Ben Noordhuis)
* string_decoder: Add 'end' method, do base64 properly (isaacs)
* buffer: include encoding value in exception when invalid (Ricky Ng-Adam)
* http: make http.ServerResponse no longer emit 'end' (isaacs)
* streams: fix pipe is destructed by 'end' from destination (koichik)

## 2012.09.17, Version 0.9.2 (Unstable)

https://github.com/nodejs/node/commit/6e2055889091a424fbb5c500bc3ab9c05d1c28b4

* http_parser: upgrade to ad3b631
* openssl: upgrade 1.0.1c
* darwin: use FSEvents to watch directory changes (Fedor Indutny)
* unix: support missing API on NetBSD (Shigeki Ohtsu)
* unix: fix EMFILE busy loop (Ben Noordhuis)
* windows: un-break writable tty handles (Bert Belder)
* windows: map WSAESHUTDOWN to UV_EPIPE (Bert Belder)
* windows: make spawn with custom environment work again (Bert Belder)
* windows: map ERROR_DIRECTORY to UV_ENOENT (Bert Belder)
* tls, https: validate server certificate by default (Ben Noordhuis)
* tls, https: throw exception on missing key/cert (Ben Noordhuis)
* tls: async session storage (Fedor Indutny)
* installer: don't install header files (Ben Noordhuis)
* buffer: implement Buffer.prototype.toJSON() (Nathan Rajlich)
* buffer: added support for writing NaN and Infinity (koichik)
* http: make http.ServerResponse emit 'end' (Ben Noordhuis)
* build: ./configure --ninja (Ben Noordhuis, Timothy J Fontaine)
* installer: fix --without-npm (Ben Noordhuis)
* cli: make -p equivalent to -pe (Ben Noordhuis)
* url: Go much faster by using Url class (isaacs)

## 2012.08.28, Version 0.9.1 (Unstable)

https://github.com/nodejs/node/commit/e6ce259d2caf338fec991c2dd447de763ce99ab7

* buffer: Add Buffer.isEncoding(enc) to test for valid encoding values (isaacs)
* Raise UV_ECANCELED on premature close. (Ben Noordhuis)
* Remove c-ares from libuv, move to a top-level node dependency (Bert Belder)
* ref/unref for all HandleWraps, timers, servers, and sockets (Timothy J Fontaine)
* addon: remove node-waf, superseded by node-gyp (Ben Noordhuis)
* child_process: emit error on exec failure (Ben Noordhuis)
* cluster: do not use internal server API (Andreas Madsen)
* constants: add O_DIRECT (Ian Babrou)
* crypto: add sync interface to crypto.pbkdf2() (Ben Noordhuis)
* darwin: emulate fdatasync() (Fedor Indutny)
* dgram: make .bind() always asynchronous (Ben Noordhuis)
* events: Make emitter.listeners() side-effect free (isaacs, Joe Andaverde)
* fs: Throw early on invalid encoding args (isaacs)
* fs: fix naming of truncate/ftruncate functions (isaacs)
* http: bubble up parser errors to ClientRequest (Brian White)
* linux: improve cpuinfo parser on ARM and MIPS (Ben Noordhuis)
* net: add support for IPv6 addresses ending in :: (Josh Erickson)
* net: support Server.listen(Pipe) (Andreas Madsen)
* node: don't scan add-on for "init" symbol (Ben Noordhuis)
* remove process.uvCounters() (Ben Noordhuis)
* repl: console writes to repl rather than process stdio (Nathan Rajlich)
* timers: implement setImmediate (Timothy J Fontaine)
* tls: fix segfault in pummel/test-tls-ci-reneg-attack (Ben Noordhuis)
* tools: Move gyp addon tools to node-gyp (Nathan Rajlich)
* unix: preliminary signal handler support (Ben Noordhuis)
* unix: remove dependency on ev_child (Ben Noordhuis)
* unix: work around darwin bug, don't poll() on pipe (Fedor Indutny)
* util: Formally deprecate util.pump() (Ben Noordhuis)
* windows: make active and closing handle state independent (Bert Belder)
* windows: report spawn errors to the exit callback (Bert Belder)
* windows: signal handling support with uv_signal_t (Bert Belder)

## 2012.07.20, Version 0.9.0 (Unstable)

https://github.com/nodejs/node/commit/f9b237f478c372fd55e4590d7399dcd8f25f3603

* punycode: update to v1.1.1 (Mathias Bynens)
* c-ares: upgrade to 1.9.0 (Saúl Ibarra Corretgé)
* dns: ignore rogue DNS servers reported by windows (Saúl Ibarra Corretgé)
* unix: speed up uv_async_send() (Ben Noordhuis)
* darwin: get cpu model correctly on mac (Xidorn Quan)
* nextTick: Handle tick callbacks before any other I/O (isaacs)
* Enable color customization of `util.inspect` (Pavel Lang)
* tls: Speed and memory improvements (Fedor Indutny)
* readline: Use one history item for reentered line (Vladimir Beloborodov)
* Fix [#3521](https://github.com/joyent/node/issues/3521) Make process.env more like a regular Object (isaacs)

## 2013.06.13, Version 0.8.25 (maintenance)

https://github.com/nodejs/node/commit/0b9bdb2bc7e1c872f0ea4713517fda22a4b0b202

* npm: Upgrade to 1.2.30
* child_process: fix handle delivery (Ben Noordhuis)

## 2013.06.04, Version 0.8.24 (maintenance)

https://github.com/nodejs/node/commit/c1a1ab067721ea17ef7b05ec5c68b01321017f05

* npm: Upgrade to v1.2.24
* url: Properly parse certain oddly formed urls (isaacs)
* http: Don't try to destroy nonexistent sockets (isaacs)
* handle_wrap: fix NULL pointer dereference (Ben Noordhuis)

## 2013.04.09, Version 0.8.23 (maintenance)

https://github.com/nodejs/node/commit/c67f8d0500fe15637a623eb759d2ad7eb9fb3b0b

* npm: Upgrade to v1.2.18
* http: Avoid EE warning on ECONNREFUSED handling (isaacs)
* tls: Re-enable check of CN-ID in cert verification (Tobias Müllerleile)
* child_process: fix sending utf-8 to child process (Ben Noordhuis)
* crypto: check key type in GetPeerCertificate() (Ben Noordhuis)
* win/openssl: mark assembled object files as seh safe (Bert Belder)
* windows/msi: fix msi build issue with WiX 3.7/3.8 (Raymond Feng)

## 2013.03.07, Version 0.8.22 (Stable)

https://github.com/nodejs/node/commit/67a4cb4fe8c2346e30ffb83f7178e205cc2dab33

* npm: Update to 1.2.14
* cluster: propagate bind errors (Ben Noordhuis)
* crypto: don't assert when calling Cipher#final() twice (Ben Noordhuis)
* build, windows: disable SEH (Ben Noordhuis)

## 2013.02.25, Version 0.8.21 (Stable)

https://github.com/nodejs/node/commit/530d8c05d4c546146f18e5ba811d7eb3b7b7c0c5

* http: Do not free the wrong parser on socket close (isaacs)
* http: Handle hangup writes more gently (isaacs)
* zlib: fix assert on bad input (Ben Noordhuis)
* test: add TAP output to the test runner (Timothy J Fontaine)
* unix: Handle EINPROGRESS from domain sockets (Ben Noordhuis)

## 2013.02.15, Version 0.8.20 (Stable)

https://github.com/nodejs/node/commit/e10c75579b536581ddd7ae4e2c3bf8a9d550d343

* npm: Upgrade to v1.2.11
* http: Do not let Agent hand out destroyed sockets (isaacs)
* http: Raise hangup error on destroyed socket write (isaacs)
* http: protect against response splitting attacks (Bert Belder)

## 2013.02.06, Version 0.8.19 (Stable)

https://github.com/nodejs/node/commit/53978bdf420622ff0121c63c0338c9e7c2e60869

* npm: Upgrade to v1.2.10
* zlib: pass object size hint to V8 (Ben Noordhuis)
* zlib: reduce memory consumption, release early (Ben Noordhuis)
* buffer: slow buffer copy compatibility fix (Trevor Norris)
* zlib: don't assert on malformed dictionary (Ben Noordhuis)
* zlib: don't assert on missing dictionary (Ben Noordhuis)
* windows: better ipv6 support (Bert Belder)
* windows: add error mappings related to unsupported protocols (Bert Belder)
* windows: map ERROR_DIRECTORY to UV_ENOENT (Bert Belder)

## 2013.01.18, Version 0.8.18 (Stable)

https://github.com/nodejs/node/commit/2c4eef0d972838c51999d32c0d251857a713dc18

* npm: Upgrade to v1.2.2
* dns: make error message match errno (Dan Milon)
* tls: follow RFC6125 more strictly (Fedor Indutny)
* buffer: reject negative SlowBuffer offsets (Ben Noordhuis)
* install: add simplejson fallback (Chris Dent)
* http: fix "Cannot call method 'emit' of null" (Ben Noordhuis)

## 2013.01.09, Version 0.8.17 (Stable)

https://github.com/nodejs/node/commit/c50c33e9397d7a0a8717e8ce7530572907c054ad

* npm: Upgrade to v1.2.0
  - peerDependencies (Domenic Denicola)
  - node-gyp v0.8.2 (Nathan Rajlich)
  - Faster installs from github user/project shorthands (Nathan Zadoks)

* typed arrays: fix 32 bit size/index overflow (Ben Noordhuis)
* http: Improve performance of single-packet responses (Ben Noordhuis)
* install: fix openbsd man page location (Ben Noordhuis)
* http: bubble up parser errors to ClientRequest (Brian White)

## 2012.12.13, Version 0.8.16 (Stable)

https://github.com/nodejs/node/commit/1c9c6277d5cfcaaac8569c0c8f7daa64292048a9

* npm: Upgrade to 1.1.69
* fs: fix WriteStream/ReadStream fd leaks (Ben Noordhuis)
* crypto: fix leak in GetPeerCertificate (Fedor Indutny)
* buffer: Don't double-negate numeric buffer arg (Trevor Norris)
* net: More accurate IP address validation and IPv6 dotted notation. (Joshua Erickson)

## 2012.11.26, Version 0.8.15 (Stable)

https://github.com/nodejs/node/commit/fdf91afb494a7a2fff2913d817f589c191a2c88f

* npm: Upgrade to 1.1.66 (isaacs)
* linux: use /proc/cpuinfo for CPU frequency (Ben Noordhuis)
* windows: map WSAESHUTDOWN to UV_EPIPE (Ben Noordhuis)
* windows: map ERROR_GEN_FAILURE to UV_EIO (Bert Belder)
* unix: do not set environ unless one is provided (Charlie McConnell)
* domains: don't crash if domain is set to null (Bert Belder)
* windows: fix the x64 debug build (Bert Belder)
* net, tls: fix connect() resource leak (Ben Noordhuis)

## 2012.10.25, Version 0.8.14 (Stable)

https://github.com/nodejs/node/commit/b00527fcf05c3d9f https://github.com/nodejs/node/commit/b5d5d790f9472906a59fe218

* events: Don't clobber pre-existing _events obj in EE ctor (isaacs)

## 2012.10.25, Version 0.8.13 (Stable)

https://github.com/nodejs/node/commit/ff4c974873f9a7cc6a5b042eb9b6389bb8dde6d6

* V8: Upgrade to 3.11.10.25
* npm: Upgrade to 1.1.65
* url: parse hostnames that start with - or _ (Ben Noordhuis)
* repl: Fix Windows 8 terminal issue (Bert Belder)
* typed arrays: use signed char for signed int8s (Aaron Jacobs)
* crypto: fix bugs in DiffieHellman (Ben Noordhuis)
* configure: turn on VFPv3 on ARMv7 (Ben Noordhuis)
* Re-enable OpenSSL UI for entering passphrases via tty (Ben Noordhuis)
* repl: ensure each REPL instance gets its own "context" (Nathan Rajlich)

## 2012.10.12, Version 0.8.12 (Stable)

https://github.com/nodejs/node/commit/38c72d4e29574dec5205bcf23c2a85efe65331a4

* npm: Upgrade to 1.1.63
* crypto: Reduce stability index to 2-Unstable (isaacs)
* windows: fix handle leak in uv_fs_utime (Bert Belder)
* windows: fix application crashed popup in debug version (Bert Belder)
* buffer: report proper retained size in profiler (Ben Noordhuis)
* buffer: fix byteLength with UTF-16LE (koichik)
* repl: make "end of input" JSON.parse() errors throw in the REPL (Nathan Rajlich)
* repl: make invalid RegExp modifiers throw in the REPL (Nathan Rajlich)
* http: handle multiple Proxy-Authenticate values (Willi Eggeling)

## 2012.09.27, Version 0.8.11 (Stable)

https://github.com/nodejs/node/commit/e1f39468fa580c1e4cb15fac621f87944ee625dc

* fs: Fix stat() size reporting for large files (Ben Noordhuis)

## 2012.09.25, Version 0.8.10 (Stable)

https://github.com/nodejs/node/commit/0bc273da4fcaa79b209ed755ad249a3e7be626a6

* npm: Upgrade to 1.1.62
* repl: make invalid RegExps throw in the REPL (Nathan Rajlich)
* v8: loosen artificial mmap constraint (Bryan Cantrill)
* process: fix setuid() and setgid() error reporting (Ben Noordhuis)
* domain: Properly exit() on domain disposal (isaacs)
* fs: fix watchFile() missing deletion events (Ben Noordhuis)
* fs: fix assert in fs.watch() (Ben Noordhuis)
* fs: don't segfault on deeply recursive stat() (Ben Noordhuis)
* http: Remove timeout handler when data arrives (Frédéric Germain)
* http: make the client "res" object gets the same domain as "req" (Nathan Rajlich)
* windows: don't blow up when an invalid FD is used (Bert Belder)
* unix: map EDQUOT to UV_ENOSPC (Charlie McConnell)
* linux: improve /proc/cpuinfo parser (Ben Noordhuis)
* win/tty: reset background brightness when color is set to default (Bert Belder)
* unix: put child process stdio fds in blocking mode (Ben Noordhuis)
* unix: fix EMFILE busy loop (Ben Noordhuis)
* sunos: don't set TCP_KEEPALIVE (Ben Noordhuis)
* tls: Use slab allocator for memory management (Fedor Indutny)
* openssl: Use optimized assembly code for x86 and x64 (Bert Belder)

## 2012.09.11, Version 0.8.9 (Stable)

https://github.com/nodejs/node/commit/b88c3902b241cf934e75443b934f2033ad3915b1

* v8: upgrade to 3.11.10.22
* GYP: upgrade to r1477
* npm: Upgrade to 1.1.61
* npm: Don't create world-writable files (isaacs)
* windows: fix single-accept mode for shared server sockets (Bert Belder)
* windows: fix uninitialized memory access in uv_update_time() (Bert Belder)
* windows: don't throw when a signal handler is attached (Bert Belder)
* unix: fix memory leak in udp (Ben Noordhuis)
* unix: map errno ESPIPE (Ben Noordhuis)
* unix, windows: fix memory corruption in fs-poll.c (Ben Noordhuis)
* sunos: fix os.cpus() on x86_64 (Ben Noordhuis)
* child process: fix processes with IPC channel don't emit 'close' (Bert Belder)
* build: add a "--dest-os" option to force a gyp "flavor" (Nathan Rajlich)
* build: set `process.platform` to "sunos" on SunOS (Nathan Rajlich)
* build: fix `make -j` fails after `make clean` (Bearice Ren)
* build: fix openssl configuration for "arm" builds (Nathan Rajlich)
* tls: support unix domain socket/named pipe in tls.connect (Shigeki Ohtsu)
* https: make https.get() accept a URL (koichik)
* http: respect HTTP/1.0 TE header (Ben Noordhuis)
* crypto, tls: Domainify setSNICallback, pbkdf2, randomBytes (Ben Noordhuis)
* stream.pipe: Don't call destroy() unless it's a function (isaacs)

## 2012.08.22, Version 0.8.8 (Stable)

https://github.com/nodejs/node/commit/a299c97bbc701f4d460e91214d7bfe7a9589d361

* V8: upgrade to 3.11.10.19
* npm: upgrade to 1.1.59
* windows: fix uninitialized memory access in uv_update_time() (Bert Belder)
* unix, windows: fix memory corruption in fs-poll.c (Ben Noordhuis)
* unix: fix integer overflow in uv_hrtime (Tim Holy)
* sunos: fix uv_cpu_info() on x86_64 (Ben Noordhuis)
* tls: update default cipher list (Ben Noordhuis)
* unix: Fix llvm and older gcc duplicate symbol warnings (Bert Belder)
* fs: fix use after free in stat watcher (Ben Noordhuis)
* build: Fix using manually compiled gcc on OS X (Nathan Rajlich)
* windows: make junctions work again (Bert Belder)

## 2012.08.15, Version 0.8.7 (Stable)

https://github.com/nodejs/node/commit/f640c5d35cba96634cd8176a525a1d876e361a61

* npm: Upgrade to 1.1.49
* website: download page (Golo Roden)
* crypto: fix uninitialized memory access in openssl (Ben Noordhuis)
* buffer, crypto: fix buffer decoding (Ben Noordhuis)
* build: compile with -fno-tree-vrp when gcc >= 4.0 (Ben Noordhuis)
* tls: handle multiple CN fields when verifying cert (Ben Noordhuis)
* doc: remove unused util from child_process (Kyle Robinson Young)
* build: rework -fvisibility=hidden detection (Ben Noordhuis)
* windows: don't duplicate invalid stdio handles (Bert Belder)
* windows: fix typos in process-stdio.c (Bert Belder)

## 2012.08.07, Version 0.8.6 (Stable)

https://github.com/nodejs/node/commit/0544a586ca6b6b900a42e164033dbf350765700a

* npm: Upgrade to v1.1.48
* Add 'make binary' to build binary tarballs for all Unixes (Nathan Rajlich)
* zlib: Emit 'close' on destroy(). (Dominic Tarr)
* child_process: Fix stdout=null when stdio=['pipe'] (Tyler Neylon)
* installer: prevent ETXTBSY errors (Ben Noordhuis)
* installer: honor --without-npm, default install path (Ben Noordhuis)
* net: make pause work with connecting sockets (Bert Belder)
* installer: fix cross-compile installs (Ben Noordhuis)
* net: fix .listen({fd:0}) (Ben Noordhuis)
* windows: map WSANO_DATA to UV_ENOENT (Bert Belder)

## 2012.08.02, Version 0.8.5 (Stable)

https://github.com/nodejs/node/commit/9b86a4453f0c76f2707a75c0b2343aba33ec63bc

* node: tag Encode and friends NODE_EXTERN (Ben Noordhuis)
* fs: fix ReadStream / WriteStream missing callback (Gil Pedersen)
* fs: fix readFileSync("/proc/cpuinfo") regression (Ben Noordhuis)
* installer: don't assume bash is installed (Ben Noordhuis)
* Report errors properly from --eval and stdin (isaacs)
* assert: fix throws() throws an error without message property (koichik)
* cluster: fix libuv assert in net.listen() (Ben Noordhuis)
* build: always link sunos builds with libumem (Trent Mick)
* build: improve armv7 / hard-float detection (Adam Malcontenti-Wilson)
* https: Use host header as effective servername (isaacs)
* sunos: work around OS bug to prevent fs.watch() from spinning (Bryan Cantrill)
* linux: fix 'two watchers, one path' segfault (Ben Noordhuis)
* windows: fix memory leaks in many fs functions (Bert Belder)
* windows: don't allow directories to be opened for writing/appending (Bert Belder)
* windows: make fork() work even when not all stdio handles are valid (Bert Belder)
* windows: make unlink() not remove mount points, and improve performance (Bert Belder)
* build: Sign pkg installer for OS X (isaacs)

## 2012.07.25, Version 0.8.4 (Stable)

https://github.com/nodejs/node/commit/f98562fcd7d1cab573ca4dc1612157d6999befd4

* V8: Upgrade to 3.11.10.17
* npm: Upgrade to 1.1.45
* net: fix Socket({ fd: 42 }) api (Ben Noordhuis)
* readline: Remove event listeners on close (isaacs)
* windows: correctly prep long path for fs.exists(Sync) (Bert Belder)
* debugger: wake up the event loop when a debugger command is dispatched (Peter Rybin)
* tls: verify server's identity (Fedor Indutny)
* net: ignore socket.setTimeout(Infinity or NaN) (Fedor Indutny)

## 2012.07.19, Version 0.8.3 (Stable)

https://github.com/nodejs/node/commit/60bf2d6cb33e4ce55604f73889ab840a9de8bdab

* V8: upgrade to 3.11.10.15
* npm: Upgrade to 1.1.43
* net: fix net.Server.listen({fd:x}) error reporting (Ben Noordhuis)
* net: fix bogus errno reporting (Ben Noordhuis)
* build: Move npm shebang logic into an npm script (isaacs)
* build: fix add-on loading on freebsd (Ben Noordhuis)
* build: disable unsafe optimizations (Ben Noordhuis)
* build: fix spurious mksnapshot crashes for good (Ben Noordhuis)
* build: speed up genv8constants (Dave Pacheco)
* fs: make unwatchFile() remove a specific listener (Ben Noordhuis)
* domain: Remove first arg from intercepted fn (Toshihiro Nakamura)
* domain: Fix memory leak on error (isaacs)
* events: Fix memory leak from removeAllListeners (Nathan Rajlich)
* zlib: Fix memory leak in Unzip class. (isaacs)
* crypto: Fix memory leak in DecipherUpdate() (Ben Noordhuis)

## 2012.07.09, Version 0.8.2 (Stable)

https://github.com/nodejs/node/commit/cc6084b9ac5cf1d4fe5e7165b71e8fc05d11be1f

* npm: Upgrade to 1.1.36
* readline: don't use Function#call() (Nathan Rajlich)
* Code cleanup to pass 'use strict' (Jonas Westerlund)
* module: add filename to require() json errors (TJ Holowaychuk)
* readline: fix for unicode prompts (Tim Macfarlane)
* timers: fix handling of large timeouts (Ben Noordhuis)
* repl: fix passing an empty line inserting "undefined" into the buffer (Nathan Rajlich)
* repl: fix crashes when buffering command (Maciej Małecki)
* build: rename strict_aliasing to node_no_strict_aliasing (Ben Noordhuis)
* build: disable -fstrict-aliasing for any gcc < 4.6.0 (Ben Noordhuis)
* build: detect cc version with -dumpversion (Ben Noordhuis)
* build: handle output of localized gcc or clang (Ben Noordhuis)
* unix: fix memory corruption in freebsd.c (Ben Noordhuis)
* unix: fix 'zero handles, one request' busy loop (Ben Noordhuis)
* unix: fix busy loop on unexpected tcp message (Ben Noordhuis)
* unix: fix EINPROGRESS busy loop (Ben Noordhuis)

## 2012.06.29, Version 0.8.1 (stable)

https://github.com/nodejs/node/commit/2134aa3d5c622fc3c3b02ccb713fcde0e0df479a

* V8: upgrade to v3.11.10.12
* npm: upgrade to v1.1.33
  - Support for parallel use of the cache folder
  - Retry on registry timeouts or network failures (Trent Mick)
  - Reduce 'engines' failures to a warning
  - Use new zsh completion if available (Jeremy Cantrell)

* Fix [#3577](https://github.com/joyent/node/issues/3577) Un-break require('sys')
* util: speed up formatting of large arrays/objects (Ben Noordhuis)
* windows: make fs.realpath(Sync) work with UNC paths (Bert Belder)
* build: fix --shared-v8 option (Ben Noordhuis)
* doc: `detached` is a boolean (Andreas Madsen)
* build: use proper python interpreter (Ben Noordhuis)
* build: expand ~ in `./configure --prefix=~/a/b/c` (Ben Noordhuis)
* build: handle CC env var with spaces (Gabriel de Perthuis)
* build: fix V8 build when compiling with gcc 4.5 (Ben Noordhuis)
* build: fix --shared-v8 option (Ben Noordhuis)
* windows msi: Fix icon issue which caused huge file size (Bert Belder)
* unix: assume that dlopen() may clobber dlerror() (Ben Noordhuis)
* sunos: fix memory corruption bugs (Ben Noordhuis)
* windows: better (f)utimes and (f)stat (Bert Belder)

## 2012.06.25, Version 0.8.0 (stable)

https://github.com/nodejs/node/commit/8b8a7a7f9b41e74e1e810d0330738ad06fc302ec

* V8: upgrade to v3.11.10.10
* npm: Upgrade to 1.1.32
* Deprecate iowatcher (Ben Noordhuis)
* windows: update icon (Bert Belder)
* http: Hush 'MUST NOT have a body' warnings to debug() (isaacs)
* Move blog.nodejs.org content into repository (isaacs)
* Fix [#3503](https://github.com/joyent/node/issues/3503): stdin: resume() on pipe(dest) (isaacs)
* crypto: fix error reporting in SetKey() (Fedor Indutny)
* Add --no-deprecation and --trace-deprecation command-line flags (isaacs)
* fs: fix fs.watchFile() (Ben Noordhuis)
* fs: Fix fs.readfile() on pipes (isaacs)
* Rename GYP variable node_use_system_openssl to be consistent (Ryan Dahl)

## 2012.06.19, Version 0.7.12 (unstable)

https://github.com/nodejs/node/commit/a72120190a8ffdbcd3d6ad2a2e6ceecd2087111e

* npm: Upgrade to 1.1.30
	- Improved 'npm init'
	- Fix the 'cb never called' error from 'oudated' and 'update'
	- Add --save-bundle|-B config
	- Fix isaacs/npm[#2465](https://github.com/joyent/node/issues/2465): Make npm script and windows shims cygwin-aware
	- Fix isaacs/npm[#2452](https://github.com/joyent/node/issues/2452) Use --save(-dev|-optional) in npm rm
	- `logstream` option to replace removed `logfd` (Rod Vagg)
	- Read default descriptions from README.md files

* Shims to support deprecated ev_* and eio_* methods (Ben Noordhuis)
* [#3118](https://github.com/joyent/node/issues/3118) net.Socket: Delay pause/resume until after connect (isaacs)
* [#3465](https://github.com/joyent/node/issues/3465) Add ./configure --no-ifaddrs flag (isaacs)
* child_process: add .stdin stream to forks (Fedor Indutny)
* build: fix `make install DESTDIR=/path` (Ben Noordhuis)
* tls: fix off-by-one error in renegotiation check (Ben Noordhuis)
* crypto: Fix diffie-hellman key generation UTF-8 errors (Fedor Indutny)
* node: change the constructor name of process from EventEmitter to process (Andreas Madsen)
* net: Prevent property access throws during close (Reid Burke)
* querystring: improved speed and code cleanup (Felix Böhm)
* sunos: fix assertion errors breaking fs.watch() (Fedor Indutny)
* unix: stat: detect sub-second changes (Ben Noordhuis)
* add stat() based file watcher (Ben Noordhuis)

## 2012.06.15, Version 0.7.11 (unstable)

https://github.com/nodejs/node/commit/5cfe0b86d5be266ef51bbba369c39e412ee51944

* V8: Upgrade to v3.11.10
* npm: Upgrade to 1.1.26
* doc: Improve cross-linking in API docs markdown (Ben Kelly)
* Fix [#3425](https://github.com/joyent/node/issues/3425): removeAllListeners should delete array (Reid Burke)
* cluster: don't silently drop messages when the write queue gets big (Bert Belder)
* Add Buffer.concat method (isaacs)
* windows: make symlinks tolerant to forward slashes (Bert Belder)
* build: Add node.d and node.1 to installer (isaacs)
* cluster: rename worker.unqiueID to worker.id (Andreas Madsen)
* Windows: Enable ETW events on Windows for existing DTrace probes. (Igor Zinkovsky)
* test: bundle node-weak in test/gc so that it doesn't need to be downloaded (Nathan Rajlich)
* Make many tests pass on Windows (Bert Belder)
* Fix [#3388](https://github.com/joyent/node/issues/3388) Support listening on file descriptors (isaacs)
* Fix [#3407](https://github.com/joyent/node/issues/3407) Add os.tmpDir() (isaacs)
* Unbreak the snapshotted build on Windows (Bert Belder)
* Clean up child_process.kill throws (Bert Belder)
* crypto: make cipher/decipher accept buffer args (Ben Noordhuis)

## 2012.06.11, Version 0.7.10 (unstable)

https://github.com/nodejs/node/commit/12a32a48a30182621b3f8e9b9695d1946b53c131

* Roll V8 back to 3.9.24.31
* build: x64 target should always pass -m64 (Robert Mustacchi)
* add NODE_EXTERN to node::Start (Joel Brandt)
* repl: Warn about running npm commands (isaacs)
* slab_allocator: fix crash in dtor if V8 is dead (Ben Noordhuis)
* slab_allocator: fix leak of Persistent handles (Shigeki Ohtsu)
* windows/msi: add node.js prompt to startmenu (Jeroen Janssen)
* windows/msi: fix adding node to PATH (Jeroen Janssen)
* windows/msi: add start menu links when installing (Jeroen Janssen)
* windows: don't install x64 version into the 'program files (x86)' folder (Matt Gollob)
* domain: Fix [#3379](https://github.com/joyent/node/issues/3379) domain.intercept no longer passes error arg to cb (Marc Harter)
* fs: make callbacks run in global context (Ben Noordhuis)
* fs: enable fs.realpath on windows (isaacs)
* child_process: expose UV_PROCESS_DETACHED as options.detached (Charlie McConnell)
* child_process: new stdio API for .spawn() method (Fedor Indutny)
* child_process: spawn().ref() and spawn().unref() (Fedor Indutny)
* Upgrade npm to 1.1.25
	- Enable npm link on windows
	- Properly remove sh-shim on Windows
	- Abstract out registry client and logger

## 2012.05.28, Version 0.7.9 (unstable)

https://github.com/nodejs/node/commit/782277f11a753ded831439ed826448c06fc0f356

* Upgrade V8 to 3.11.1
* Upgrade npm to 1.1.23
* uv: rework reference counting scheme (Ben Noordhuis)
* uv: add interface for joining external event loops (Bert Belder)
* repl, readline: Handle Ctrl+Z and SIGCONT better (Nathan Rajlich)
* fs: 64bit offsets for fs calls (Igor Zinkovsky)
* fs: add sync open flags 'rs' and 'rs+' (Kevin Bowman)
* windows: enable creating directory junctions with fs.symlink (Igor Zinkovsky, Bert Belder)
* windows: fix fs.lstat to properly detect symlinks. (Igor Zinkovsky)
* Fix [#3270](https://github.com/joyent/node/issues/3270) Escape url.parse delims (isaacs)
* http: make http.get() accept a URL (Adam Malcontenti-Wilson)
* Cleanup vm module memory leakage (Marcel Laverdet)
* Optimize writing strings with Socket.write (Bert Belder)
* add support for CESU-8 and UTF-16LE encodings (koichik)
* path: add path.sep to get the path separator. (Yi, EungJun)
* net, http: add backlog parameter to .listen() (Erik Dubbelboer)
* debugger: support mirroring Date objects (Fedor Indutny)
* addon: add AtExit() function (Ben Noordhuis)
* net: signal localAddress bind failure in connect (Brian Schroeder)
* util: handle non-string return value in .inspect() (Alex Kocharin)

## 2012.04.18, Version 0.7.8 (unstable)

https://github.com/nodejs/node/commit/c2b47097c0b483552efc1947c6766fa1128600b6

* Upgrade V8 to 3.9.24.9
* Upgrade OpenSSL to 1.0.0f
* Upgrade npm to 1.1.18
* Show licenses in Binary installers
* Domains (isaacs)
* readline: rename "end" to "close" (Nathan Rajlich)
* tcp: make getsockname() return address family as string (Shigeki Ohtsu)
* http, https: fix .setTimeout() (ssuda)
* os: add cross platform EOL character (Mustansir Golawala)
* typed arrays: unexport SizeOfArrayElementForType() (Aaron Jacobs)
* net: honor 'enable' flag in .setNoDelay() (Ben Noordhuis)
* child_process: emit error when .kill fails (Andreas Madsen)
* gyp: fix 'argument list too long' build error (Ben Noordhuis)
* fs.WriteStream: Handle modifications to fs.open (isaacs)
* repl, readline: Handle newlines better (Nathan Rajlich, Nathan Friedly)
* build: target OSX 10.5 when building on darwin (Nathan Rajlich)
* Fix [#3052](https://github.com/joyent/node/issues/3052) Handle errors properly in zlib (isaacs)
* build: add support for DTrace and postmortem (Dave Pacheco)
* core: add reusable Slab allocator (Ben Noordhuis)

## 2012.03.30, Version 0.7.7 (unstable)

https://github.com/nodejs/node/commit/5cda2542fdb086f9fe5de889bea435a65e377dea

* Upgrade V8 to 3.9.24.7
* Upgrade npm to 1.1.15
* Handle Emoji characters properly (Erik Corry, Bert Belder)
* readline: migrate ansi/vt100 logic from tty to readline (Nathan Rajlich)
* readline: Fix multiline handling (Alex Kocharin)
* add a -i/--interactive flag to force the REPL (Nathan Rajlich)
* debugger: add breakOnException command (Fedor Indutny)
* cluster: kill workers when master dies (Andreas Madsen)
* cluster: add graceful disconnect support (Andreas Madsen)
* child_process: Separate 'close' event from 'exit' (Charlie McConnell)
* typed arrays: add Uint8ClampedArray (Mikael Bourges-Sevenier)
* buffer: Fix byte alignment issues (Ben Noordhuis, Erik Lundin)
* tls: fix CryptoStream.setKeepAlive() (Shigeki Ohtsu)
* Expose http parse error codes (Felix Geisendörfer)
* events: don't delete the listeners array (Ben Noordhuis, Nathan Rajlich)
* process: add process.config to view node's ./configure settings (Nathan Rajlich)
* process: process.execArgv to see node's arguments (Micheil Smith)
* process: fix process.title setter (Ben Noordhuis)
* timers: handle negative or non-numeric timeout values (Ben Noordhuis)

## 2012.03.13, Version 0.7.6 (unstable)

https://github.com/nodejs/node/commit/f06abda6f58e517349d1b63a2cbf5a8d04a03505

* Upgrade v8 to 3.9.17
* Upgrade npm to 1.1.8
  - Add support for os/cpu fields in package.json (Adam Blackburn)
  - Automatically node-gyp packages containing a binding.gyp
  - Fix failures unpacking in UNC shares
  - Never create un-listable directories
  - Handle cases where an optionalDependency fails to build

* events: newListener emit correct fn when using 'once' (Roly Fentanes)
* url: Ignore empty port component (Łukasz Walukiewicz)
* module: replace 'children' array (isaacs)
* tls: parse multiple values of a key in ssl certificate (Sambasiva Suda)
* cluster: support passing of named pipes (Ben Noordhuis)
* Windows: include syscall in fs errors (Bert Belder)
* http: [#2888](https://github.com/joyent/node/issues/2888) Emit end event only once (Igor Zinkovsky)
* readline: add multiline support (Rlidwka)
* process: add `process.hrtime()` (Nathan Rajlich)
* net, http, https: add localAddress option (Dmitry Nizovtsev)
* addon improvements (Nathan Rajlich)
* build improvements (Ben Noordhuis, Sadique Ali, T.C. Hollingsworth, Nathan Rajlich)
* add support for "SEARCH" request methods (Nathan Rajlich)
* expose the zlib and http_parser version in process.versions (Nathan Rajlich)

## 2012.02.23, Version 0.7.5 (unstable)

https://github.com/nodejs/node/commit/d384b8b0d2ab7f05465f0a3e15fe20b4e25b5f86

* startup speed improvements (Maciej Małecki)
* crypto: add function getDiffieHellman() (Tomasz Buchert)
* buffer: support decoding of URL-safe base64 (Ben Noordhuis)
* Make QueryString.parse() even faster (Brian White)
* url: decode url entities in auth section (Ben Noordhuis)
* http: support PURGE request method (Ben Noordhuis)
* http: Generate Date headers on responses (Mark Nottingham)
* Fix [#2762](https://github.com/joyent/node/issues/2762): Add callback to close function. (Mikeal Rogers)
* dgram: fix out-of-bound memory read (Ben Noordhuis)
* repl: add automatic loading of built-in libs (Brandon Benvie)
* repl: remove double calls where possible (Fedor Indutny)
* Readline improvements. Related: [#2737](https://github.com/joyent/node/issues/2737) [#2756](https://github.com/joyent/node/issues/2756) (Colton Baker)
* build: disable -fomit-frame-pointer on solaris (Dave Pacheco)
* build: arch detection improvements (Nathan Rajlich)
* build: Make a fat binary for the OS X `make pkg`. (Nathan Rajlich)
* jslint src/ and lib/ on 'make test' (isaacs)

## 2012.02.14, Version 0.7.4 (unstable)

https://github.com/nodejs/node/commit/de21de920cf93ec40736ada3792a7f85f3eadeda

* Upgrade V8 to 3.9.5
* Upgrade npm to 1.1.1
* build: Detect host_arch better (Karl Skomski)
* debugger: export `debug_port` to `process` (Fedor Indutny)
* api docs: CSS bug fixes (isaacs)
* build: use -fPIC for native addons on UNIX (Nathan Rajlich)
* Re-add top-level v8::Locker (Marcel Laverdet)
* Move images out of the dist tarballs (isaacs)
* libuv: Remove uv_export and uv_import (Ben Noordhuis)
* build: Support x64 build on Windows (Igor Zinkovsky)

## 2012.02.07, Version 0.7.3 (unstable)

https://github.com/nodejs/node/commit/99059aad8d654acda4abcfaa68df182b50f2ec90

* Upgrade V8 to 3.9.2
* Revert support for isolates. (Ben Noordhuis)
* cluster: Cleanup docs, event handling, and process.disconnect (Andreas Madsen)
* gyp_addon: link with node.lib on Windows (Nathan Rajlich)
* http: fix case where http-parser is freed twice (koichik)
* Windows: disable RTTI and exceptions (Bert Belder)

## 2012.02.01, Version 0.7.2 (unstable)

https://github.com/nodejs/node/commit/ec79acb3a6166e30f0bf271fbbfda1fb575b3321

* Update V8 to 3.8.9
* Support for sharing streams across Isolates (Igor Zinkovsky)
* [#2636](https://github.com/joyent/node/issues/2636) - Fix case where http_parsers are freed too early (koichik)
* url: Support for IPv6 addresses in URLs (Łukasz Walukiewicz)
* child_process: Add disconnect() method to child processes (Andreas Madsen)
* fs: add O_EXCL support, exclusive open file (Ben Noordhuis)
* fs: more specific error messages (Tj Holowaychuk)
* tty: emit 'unknown' key event if key sequence not found (Dan VerWeire, Nathan Rajlich)
* build: compile release build too if BUILDTYPE=Debug (Ben Noordhuis)
* module: fix --debug-brk on symlinked scripts (Fedor Indutny)
* zlib: fix `Failed to set dictionary` issue (Fedor Indutny)
* waf: predict target arch for OS X (Fedor Indutny)

## 2012.01.23, Version 0.7.1 (unstable)

https://github.com/nodejs/node/commit/a74354735ab5d5b0fa35a1e4ff7e653757d2069b

* Update V8 to 3.8.8
* Install node-waf by default (Fedor Indutny)
* crypto: Add ability to turn off PKCS padding (Ingmar Runge)
* v8: implement VirtualMemory class on SunOS (Ben Noordhuis)
* Add cluster.setupMaster (Andreas Madsen)
* move `path.exists*` to `fs.exists*` (Maciej Małecki)
* typed arrays: set class name (Ben Noordhuis)
* libuv bug fixes (Igor Zinkovsky, Ben Noordhuis, Dan VerWeire)

## 2012.01.16, Version 0.7.0 (unstable)

https://github.com/nodejs/node/commit/9cc55dca6f67a6096c858b841c677b0593404321

* Upgrade V8 to 3.8.6
* Use GYP build system on unix (Ben Noordhuis)
* Experimental isolates support (Ben Noordhuis)
* Improvements to Cluster API (Andreas Madsen)
* Use isolates for internal debugger (Fedor Indutny)
* Bug fixes

## 2012.07.10 Version 0.6.20 (maintenance)

https://github.com/nodejs/node/commit/952e513379169ec1b40909d1db056e9bf4294899

* npm: Upgrade to 1.1.37 (isaacs)
* benchmark: Backport improvements made in master (isaacs)
* build: always link with -lz (Trent Mick)
* core: use proper #include directives (Ben Noordhuis)
* cluster: don't silently drop messages when the write queue gets big (Bert Belder)
* windows: don't print error when GetConsoleTitleW returns an empty string (Bert Belder)

## 2012.06.06 Version 0.6.19 (stable)

https://github.com/nodejs/node/commit/debf552ed2d4a53957446e82ff3c52a8182d5ff4

* npm: upgrade to 1.1.24
* fs: no end emit after createReadStream.pause() (Andreas Madsen)
* vm: cleanup module memory leakage (Marcel Laverdet)
* unix: fix loop starvation under high network load (Ben Noordhuis)
* unix: remove abort() in ev_unref() (Ben Noordhuis)
* windows/tty: never report error after forcibly aborting line-buffered read (Bert Belder)
* windows: skip GetFileAttributes call when opening a file (Bert Belder)

## 2012.05.15 Version 0.6.18 (stable)

https://github.com/nodejs/node/commit/4bc1d395de6abed2cf1e4d0b7b3a1480a21c368f

* windows: skip GetFileAttributes call when opening a file (Bert Belder)
* crypto: add PKCS12/PFX support (Sambasiva Suda)
* [#3240](https://github.com/joyent/node/issues/3240): child_process: delete NODE_CHANNEL_FD from env in spawn (Ben Noordhuis)
* windows: add test for path.normalize with UNC paths (Bert Belder)
* windows: make path.normalize convert all slashes to backslashes (Bert Belder)
* fs: Automatically close FSWatcher on error (Bert Belder)
* [#3258](https://github.com/joyent/node/issues/3258): fs.ReadStream.pause() emits duplicate data event (koichik)
* pipe_wrap: don't assert() on pipe accept errors (Ben Noordhuis)
* Better exception output for module load and process.nextTick (Felix Geisendörfer)
* zlib: fix error reporting (Ben Noordhuis)
* http: Don't destroy on timeout (isaacs)
* [#3231](https://github.com/joyent/node/issues/3231): http: Don't try to emit error on a null'ed req object (isaacs)
* [#3236](https://github.com/joyent/node/issues/3236): http: Refactor ClientRequest.onSocket (isaacs)

## 2012.05.04 Version 0.6.17 (stable)

https://github.com/nodejs/node/commit/4ced23deaf36493f4303a18f6fdce768c58becc0

* Upgrade npm to 1.1.21
* uv: Add support for EROFS errors (Ben Noordhuis, Maciej Małecki)
* uv: Add support for EIO and ENOSPC errors (Fedor Indutny)
* windows: Add support for EXDEV errors (Bert Belder)
* http: Fix client memory leaks (isaacs, Vincent Voyer)
* fs: fix file descriptor leak in sync functions (Ben Noordhuis)
* fs: fix ReadStream / WriteStream double close bug (Ben Noordhuis)

## 2012.04.30 Version 0.6.16 (stable)

https://github.com/nodejs/node/commit/a1d193963ddc80a27da5da01b59751e14e33d1d6

* Upgrade V8 to 3.6.6.25
* Upgrade npm to 1.1.19
* Windows: add mappings for UV_ENOENT (Bert Belder)
* linux: add IN_MOVE_SELF to inotify event mask (Ben Noordhuis)
* unix: call pipe handle connection cb on accept() error (Ben Noordhuis)
* unix: handle EWOULDBLOCK (Ben Noordhuis)
* map EWOULDBLOCK to UV_EAGAIN (Ben Noordhuis)
* Map ENOMEM to UV_ENOMEM (isaacs)
* Child process: support the `gid` and `uid` options (Bert Belder)
* test: cluster: add worker death event test (Ben Noordhuis)
* typo in node_http_parser (isaacs)
* http_parser: Eat CRLF between requests, even on connection:close. (Ben Noordhuis)
* don't check return value of unsetenv (Ben Noordhuis)

## 2012.04.09 Version 0.6.15 (stable)

https://github.com/nodejs/node/commit/f160a45b254e591eb33716311c92be533c6d86c4

* Update npm to 1.1.16
* Show licenses in binary installers.
* unix: add uv_fs_read64, uv_fs_write64 and uv_fs_ftruncate64 (Ben Noordhuis)
* add 64bit offset fs functions (Igor Zinkovsky)
* windows: don't report ENOTSOCK when attempting to bind an udp handle twice (Bert Belder)
* windows: backport pipe-connect-to-file fixes from master (Bert Belder)
* windows: never call fs event callbacks after closing the watcher (Bert Belder)
* fs.readFile: don't make the callback before the fd is closed (Bert Belder)
* windows: use 64bit offsets for uv_fs apis (Igor Zinkovsky)
* Fix [#2061](https://github.com/joyent/node/issues/2061): segmentation fault on OS X due to stat size mismatch (Ben Noordhuis)

## 2012.03.22 Version 0.6.14 (stable)

https://github.com/nodejs/node/commit/e513ffef7549a56a5af728e1f0c2c0c8f290518a

* net: don't crash when queued write fails (Igor Zinkovsky)
* sunos: fix EMFILE on process.memoryUsage() (Bryan Cantrill)
* crypto: fix compile-time error with openssl 0.9.7e (Ben Noordhuis)
* unix: ignore ECONNABORTED errors from accept() (Ben Noordhuis)
* Add UV_ENOSPC and mappings to it (Bert Belder)
* http-parser: Fix response body is not read (koichik)
* Upgrade npm to 1.1.12
  - upgrade node-gyp to 0.3.7
  - work around AV-locked directories on Windows
  - Fix isaacs/npm[#2293](https://github.com/joyent/node/issues/2293) Don't try to 'uninstall' /
  - Exclude symbolic links from packages.
  - Fix isaacs/npm[#2275](https://github.com/joyent/node/issues/2275) Spurious 'unresolvable cycle' error.
  - Exclude/include dot files as if they were normal files

## 2012.03.15 Version 0.6.13 (stable)

https://github.com/nodejs/node/commit/9f7f86b534f8556290eb8cad915984ff4ca54996

* Windows: Many libuv test fixes (Bert Belder)
* Windows: avoid uv_guess_handle crash in when fd < 0 (Bert Belder)
* Map EBUSY and ENOTEMPTY errors (Bert Belder)
* Windows: include syscall in fs errors (Bert Belder)
* Fix fs.watch ENOSYS on Linux kernel version mismatch (Ben Noordhuis)
* Update npm to 1.1.9
  - upgrade node-gyp to 0.3.5 (Nathan Rajlich)
  - Fix isaacs/npm[#2249](https://github.com/joyent/node/issues/2249) Add cache-max and cache-min configs
  - Properly redirect across https/http registry requests
  - log config usage if undefined key in set function (Kris Windham)
  - Add support for os/cpu fields in package.json (Adam Blackburn)
  - Automatically node-gyp packages containing a binding.gyp
  - Fix failures unpacking in UNC shares
  - Never create un-listable directories
  - Handle cases where an optionalDependency fails to build

## 2012.03.02 Version 0.6.12 (stable)

https://github.com/nodejs/node/commit/48a2d34cfe6b7e1c9d15202a4ef5e3c82d1fba35

* Upgrade V8 to 3.6.6.24
* dtrace ustack helper improvements (Dave Pacheco)
* API Documentation refactor (isaacs)
* [#2827](https://github.com/joyent/node/issues/2827) net: fix race write() before and after connect() (koichik)
* [#2554](https://github.com/joyent/node/issues/2554) [#2567](https://github.com/joyent/node/issues/2567) throw if fs args for 'start' or 'end' are strings (AJ ONeal)
* punycode: Update to v1.0.0 (Mathias Bynens)
* Make a fat binary for the OS X pkg (isaacs)
* Fix hang on accessing process.stdin (isaacs)
* repl: make tab completion work on non-objects (Nathan Rajlich)
* Fix fs.watch on OS X (Ben Noordhuis)
* Fix [#2515](https://github.com/joyent/node/issues/2515) nested setTimeouts cause premature process exit (Ben Noordhuis)
* windows: fix time conversion in stat (Igor Zinkovsky)
* windows: fs: handle EOF in read (Brandon Philips)
* windows: avoid IOCP short-circuit on non-ifs lsps (Igor Zinkovsky)
* Upgrade npm to 1.1.4 (isaacs)
  - windows fixes
  - Bundle nested bundleDependencies properly
  - install: support --save with url install targets
  - shrinkwrap: behave properly with url-installed modules
  - support installing uncompressed tars or single file modules from urls etc.
  - don't run make clean on rebuild
  - support HTTPS-over-HTTP proxy tunneling

## 2012.02.17 Version 0.6.11 (stable)

https://github.com/nodejs/node/commit/1eb1fe32250fc88cb5b0a97cddf3e02be02e3f4a

* http: allow multiple WebSocket RFC6455 headers (Einar Otto Stangvik)
* http: allow multiple WWW-Authenticate headers (Ben Noordhuis)
* windows: support unicode argv and environment variables (Bert Belder)
* tls: mitigate session renegotiation attacks (Ben Noordhuis)
* tcp, pipe: don't assert on uv_accept() errors (Ben Noordhuis)
* tls: Allow establishing secure connection on the existing socket (koichik)
* dgram: handle close of dgram socket before DNS lookup completes (Seth Fitzsimmons)
* windows: Support half-duplex pipes (Igor Zinkovsky)
* build: disable omit-frame-pointer on solaris systems (Dave Pacheco)
* debugger: fix --debug-brk (Ben Noordhuis)
* net: fix large file downloads failing (koichik)
* fs: fix ReadStream failure to read from existing fd (Christopher Jeffrey)
* net: destroy socket on DNS error (Stefan Rusu)
* dtrace: add missing translator (Dave Pacheco)
* unix: don't flush tty on switch to raw mode (Ben Noordhuis)
* windows: reset brightness when reverting to default text color (Bert Belder)
* npm: update to 1.1.1
  - Update which, fstream, mkdirp, request, and rimraf
  - Fix [#2123](https://github.com/joyent/node/issues/2123) Set path properly for lifecycle scripts on windows
  - Mark the root as seen, so we don't recurse into it. Fixes [#1838](https://github.com/joyent/node/issues/1838). (Martin Cooper)

## 2012.02.02, Version 0.6.10 (stable)

https://github.com/nodejs/node/commit/051908e023f87894fa68f5b64d0b99a19a7db01e

* Update V8 to 3.6.6.20
* Add npm msysgit bash shim to msi installer (isaacs)
* buffers: fix intermittent out of bounds error (Ben Noordhuis)
* buffers: honor length argument in base64 decoder (Ben Noordhuis)
* windows: Fix path.exists regression (Bert Belder)
* Make QueryString.parse run faster (Philip Tellis)
* http: avoid freeing http-parser objects too early (koichik)
* timers: add v0.4 compatibility hack (Ben Noordhuis)
* Proper EPERM error code support (Igor Zinkovsky, Brandon Philips)
* dgram: Implement udp multicast methods on windows (Bert Belder)

## 2012.01.27, Version 0.6.9 (stable)

https://github.com/nodejs/node/commit/f19e20d33f57c4d2853aaea7d2724d44f3b0012f

* dgram: Bring back missing functionality for Unix (Dan VerWeire, Roman Shtylman, Ben Noordhuis)
  - Note: Windows UDP support not yet complete.

* http: Fix parser memory leak (koichik)
* zlib: Fix [#2365](https://github.com/joyent/node/issues/2365) crashes on invalid input (Nicolas LaCasse)
* module: fix --debug-brk on symlinked scripts (Fedor Indutny)
* Documentation Restyling (Matthew Fitzsimmons)
* Update npm to 1.1.0-3 (isaacs)
* Windows: fix regression in stat() calls to C:\ (Bert Belder)

## 2012.01.19, Version 0.6.8 (stable)

https://github.com/nodejs/node/commit/d18cebaf8a7ac701dabd71a3aa4eb0571db6a645

* Update V8 to 3.6.6.19
* Numeric key hash collision fix for V8 (Erik Corry, Fedor Indutny)
* Add missing TTY key translations for F1-F5 on Windows (Brandon Benvie)
* path.extname bugfix with . and .. paths (Bert Belder)
* cluster: don't always kill the master on uncaughtException (Ben Noordhuis)
* Update npm to 1.1.0-2 (isaacs)
* typed arrays: set class name (Ben Noordhuis)
* zlib binding cleanup (isaacs, Bert Belder)
* dgram: use slab memory allocator (Michael Bernstein)
* fix segfault [#2473](https://github.com/joyent/node/issues/2473)
* [#2521](https://github.com/joyent/node/issues/2521) 60% improvement in fs.stat on Windows (Igor Zinkovsky)

## 2012.01.06, Version 0.6.7 (stable)

https://github.com/nodejs/node/commit/d5a189acef14a851287ee555f7a39431fe276e1c

* V8 hash collision fix (Breaks MIPS) (Bert Belder, Erik Corry)
* Upgrade V8 to 3.6.6.15
* Upgrade npm to 1.1.0-beta-10 (isaacs)
* many doc updates (Ben Noordhuis, Jeremy Martin, koichik, Dave Irvine,
  Seong-Rak Choi, Shannen, Adam Malcontenti-Wilson, koichik)

* Fix segfault in node_http_parser.cc
* dgram, timers: fix memory leaks (Ben Noordhuis, Yoshihiro Kikuchi)
* repl: fix repl.start not passing the `ignoreUndefined` arg (Damon Oehlman)
* [#1980](https://github.com/joyent/node/issues/1980): Socket.pause null reference when called on a closed Stream (koichik)
* [#2263](https://github.com/joyent/node/issues/2263): XMLHttpRequest piped in a writable file stream hang (koichik)
* [#2069](https://github.com/joyent/node/issues/2069): http resource leak (koichik)
* buffer.readInt global pollution fix (Phil Sung)
* timers: fix performance regression (Ben Noordhuis)
* [#2308](https://github.com/joyent/node/issues/2308), [#2246](https://github.com/joyent/node/issues/2246): node swallows openssl error on request (koichik)
* [#2114](https://github.com/joyent/node/issues/2114): timers: remove _idleTimeout from item in .unenroll() (James Hartig)
* [#2379](https://github.com/joyent/node/issues/2379): debugger: Request backtrace w/o refs (Fedor Indutny)
* simple DTrace ustack helper (Dave Pacheco)
* crypto: rewrite HexDecode without snprintf (Roman Shtylman)
* crypto: don't ignore DH init errors (Ben Noordhuis)

## 2011.12.14, Version 0.6.6 (stable)

https://github.com/nodejs/node/commit/9a059ea69e1f6ebd8899246682d8ca257610b8ab

* npm update to 1.1.0-beta-4 (Isaac Z. Schlueter)
* cli: fix output of --help (Ben Noordhuis)
* new website
* pause/resume semantics for stdin (Isaac Z. Schlueter)
* Travis CI integration (Maciej Małecki)
* child_process: Fix bug regarding closed stdin (Ben Noordhuis)
* Enable upgrades in MSI. (Igor Zinkovsky)
* net: Fixes memory leak (Ben Noordhuis)
* fs: handle fractional or NaN ReadStream buffer size (Ben Noordhuis)
* crypto: fix memory leaks in PBKDF2 error path (Ben Noordhuis)

## 2011.12.04, Version 0.6.5 (stable)

https://github.com/nodejs/node/commit/6cc94db653a2739ab28e33b2d6a63c51bd986a9f

* npm workaround Windows antivirus software (isaacs)
* Upgrade V8 to 3.6.6.11

## 2011.12.02, Version 0.6.4 (stable)

https://github.com/nodejs/node/commit/9170077f13e5e5475b23d1d3c2e7f69bfe139727

* doc improvements (Kyle Young, Tim Oxley, Roman Shtylman, Mathias Bynens)
* upgrade bundled npm (Isaac Schlueter)
* polish Windows installer (Igor Zinkovsky, Isaac Schlueter)
* punycode: upgrade to v0.2.1 (Mathias Bynens)
* build: add –without-npm flag to configure script
* sys: deprecate module some more, print stack trace if NODE_DEBUG=sys
* cli: add -p switch, prints result of –eval
* [#1997](https://github.com/joyent/node/issues/1997): fix Blowfish ECB encryption and decryption (Ingmar Runge)
* [#2223](https://github.com/joyent/node/issues/2223): fix socket ‘close’ event being emitted twice
* [#2224](https://github.com/joyent/node/issues/2224): fix RSS memory usage > 4 GB reporting (Russ Bradberry)
* [#2225](https://github.com/joyent/node/issues/2225): fix util.inspect() object stringification bug (Nathan Rajlich)

## 2011.11.25, Version 0.6.3 (stable)

https://github.com/nodejs/node/commit/b159c6d62e5756d3f8847419d29c6959ea288b56

* [#2083](https://github.com/joyent/node/issues/2083) Land NPM in Node. It is included in packages/installers and installed
  on `make install`.

* [#2076](https://github.com/joyent/node/issues/2076) Add logos to windows installer.
* [#1711](https://github.com/joyent/node/issues/1711) Correctly handle http requests without headers. (Ben Noordhuis,
  Felix Geisendörfer)

* TLS: expose more openssl SSL context options and constants. (Ben Noordhuis)
* [#2177](https://github.com/joyent/node/issues/2177) Windows: don't kill UDP socket when a packet fails to reach its
  destination. (Bert Belder)

* Windows: support paths longer than 260 characters. (Igor Zinkovsky)
* Windows: correctly resolve drive-relative paths. (Bert Belder)
* [#2166](https://github.com/joyent/node/issues/2166) Don't leave file descriptor open after lchmod. (Isaac Schlueter)
* [#2084](https://github.com/joyent/node/issues/2084) Add OS X .pkg build script to make file.
* [#2160](https://github.com/joyent/node/issues/2160) Documentation improvements. (Ben Noordhuis)

## 2011.11.18, Version 0.6.2 (stable)

https://github.com/nodejs/node/commit/a4402f0b2e410b19375a1d5c5fb7fe7f66f3c7f8

* doc improvements (Artur Adib, Trevor Burnham, Ryan Emery, Trent Mick)
* timers: remember extra setTimeout() arguments when timeout==0
* punycode: use Mathias Bynens's punycode library, it's more compliant
* repl: improved tab completion (Ryan Emery)
* buffer: fix range checks in .writeInt() functions (Lukasz Walukiewicz)
* tls: make cipher list configurable
* addons: make Buffer and ObjectWrap visible to Windows add-ons (Bert Belder)
* crypto: add PKCS[#1](https://github.com/joyent/node/issues/1) a.k.a RSA public key verification support
* windows: fix stdout writes when redirected to nul
* sunos: fix build on Solaris and Illumos
* Upgrade V8 to 3.6.6.8

## 2011.11.11, Version 0.6.1 (stable)

https://github.com/nodejs/node/commit/170f2addb2dd0c625bc4a6d461e89a31ad68b79b

* doc improvements (Eric Lovett, Ben Noordhuis, Scott Anderson, Yoji SHIDARA)
* crypto: make thread-safe (Ben Noordhuis)
* fix process.kill error object
* debugger: correctly handle source with multi-byte characters (Shigeki Ohtsu)
* make stdout and stderr non-destroyable (Igor Zinkovsky)
* fs: don't close uninitialized fs.watch handle (Ben Noordhuis)
* [#2026](https://github.com/joyent/node/issues/2026) fix man page install on BSDs (Ben Noordhuis)
* [#2040](https://github.com/joyent/node/issues/2040) fix unrecognized errno assert in uv_err_name
* [#2043](https://github.com/joyent/node/issues/2043) fs: mkdir() should call callback if mode is omitted
* [#2045](https://github.com/joyent/node/issues/2045) fs: fix fs.realpath on windows to return on error (Benjamin Pasero)
* [#2047](https://github.com/joyent/node/issues/2047) minor cluster improvements
* [#2052](https://github.com/joyent/node/issues/2052) readline get window columns correctly
* Upgrade V8 to 3.6.6.7

## 2011.11.04, Version 0.6.0 (stable)

https://github.com/nodejs/node/commit/865b077819a9271a29f982faaef99dc635b57fbc

* print undefined on undefined values in REPL (Nathan Rajlich)
* doc improvements (koichik, seebees, bnoordhuis,
  Maciej Małecki, Jacob Kragh)

* support native addon loading in windows (Bert Belder)
* rename getNetworkInterfaces() to networkInterfaces() (bnoordhuis)
* add pending accepts knob for windows (igorzi)
* http.request(url.parse(x)) (seebees)
* [#1929](https://github.com/joyent/node/issues/1929) zlib Respond to 'resume' events properly (isaacs)
* stream.pipe: Remove resume and pause events
* test fixes for windows (igorzi)
* build system improvements (bnoordhuis)
* [#1936](https://github.com/joyent/node/issues/1936) tls: does not emit 'end' from EncryptedStream (koichik)
* [#758](https://github.com/joyent/node/issues/758) tls: add address(), remoteAddress/remotePort
* [#1399](https://github.com/joyent/node/issues/1399) http: emit Error object after .abort() (bnoordhuis)
* [#1999](https://github.com/joyent/node/issues/1999) fs: make mkdir() default to 0777 permissions (bnoordhuis)
* [#2001](https://github.com/joyent/node/issues/2001) fix pipe error codes
* [#2002](https://github.com/joyent/node/issues/2002) Socket.write should reset timeout timer
* stdout and stderr are blocking when associated with file too.
* remote debugger support on windows (Bert Belder)
* convenience methods for zlib (Matt Robenolt)
* process.kill support on windows (igorzi)
* process.uptime() support on windows (igorzi)
* Return IPv4 addresses before IPv6 addresses from getaddrinfo
* util.inspect improvements (Nathan Rajlich)
* cluster module api changes
* Downgrade V8 to 3.6.6.6

## 2011.10.21, Version 0.5.10 (unstable)

https://github.com/nodejs/node/commit/220e61c1f65bf4db09699fcf6399c0809c0bc446

* Remove cmake build system, support for Cygwin, legacy code base,
	process.ENV, process.ARGV, process.memoryUsage().vsize, os.openOSHandle

* Documentation improvements (Igor Zinkovsky, Bert Belder, Ilya Dmitrichenko,
koichik, Maciej Małecki, Guglielmo Ferri, isaacs)

* Performance improvements (Daniel Ennis, Bert Belder, Ben Noordhuis)
* Long process.title support (Ben Noordhuis)
* net: register net.Server callback only once (Simen Brekken)
* net: fix connect queue bugs (Ben Noordhuis)
* debugger: fix backtrace err handling (Fedor Indutny)
* Use getaddrinfo instead of c-ares for dns.lookup
* Emit 'end' from crypto streams on close
* [#1902](https://github.com/joyent/node/issues/1902) buffer: use NO_NULL_TERMINATION flag (koichik)
* [#1907](https://github.com/joyent/node/issues/1907) http: Added support for HTTP PATCH verb (Thomas Parslow)
* [#1644](https://github.com/joyent/node/issues/1644) add GetCPUInfo on windows (Karl Skomski)
* [#1484](https://github.com/joyent/node/issues/1484), [#1834](https://github.com/joyent/node/issues/1834), [#1482](https://github.com/joyent/node/issues/1482), [#771](https://github.com/joyent/node/issues/771) Don't use a separate context for the repl.
  (isaacs)

* [#1882](https://github.com/joyent/node/issues/1882) zlib Update 'availOutBefore' value, and test (isaacs)
* [#1888](https://github.com/joyent/node/issues/1888) child_process.fork: don't modify args (koichik)
* [#1516](https://github.com/joyent/node/issues/1516) tls: requestCert unusable with Firefox and Chrome (koichik)
* [#1467](https://github.com/joyent/node/issues/1467) tls: The TLS API is inconsistent with the TCP API (koichik)
* [#1894](https://github.com/joyent/node/issues/1894) net: fix error handling in listen() (koichik)
* [#1860](https://github.com/joyent/node/issues/1860) console.error now goes through uv_tty_t
* Upgrade V8 to 3.7.0
* Upgrade GYP to r1081

## 2011.10.10, Version 0.5.9 (unstable)

https://github.com/nodejs/node/commit/3bd9b08fb125b606f97a4079b147accfdeebb07d

* fs.watch interface backed by kqueue, inotify, and ReadDirectoryChangesW
  (Igor Zinkovsky, Ben Noordhuis)

* add dns.resolveTxt (Christian Tellnes)
* Remove legacy http library (Ben Noordhuis)
* child_process.fork returns and works on Windows. Allows passing handles.
  (Igor Zinkovsky, Bert Belder)

* [#1774](https://github.com/joyent/node/issues/1774) Lint and clean up for --harmony_block_scoping (Tyler Larson, Colton
  Baker)

* [#1813](https://github.com/joyent/node/issues/1813) Fix ctrl+c on Windows (Bert Belder)
* [#1844](https://github.com/joyent/node/issues/1844) unbreak --use-legacy (Ben Noordhuis)
* process.stderr now goes through libuv. Both process.stdout and
  process.stderr are blocking when referencing a TTY.

* net_uv performance improvements (Ben Noordhuis, Bert Belder)

## 2011.09.30, Version 0.5.8 (unstable)

https://github.com/nodejs/node/commit/7cc17a0cea1d25188c103745a7d0c24375e3a609

* zlib bindings (isaacs)
* Windows supports TTY ANSI escape codes (Bert Belder)
* Debugger improvements (Fedor Indutny)
* crypto: look up SSL errors with ERR_print_errors() (Ben Noordhuis)
* dns callbacks go through MakeCallback now
* Raise an error when a malformed package.json file is found. (Ben Leslie)
* buffers: handle bad length argument in constructor (Ben Noordhuis)
* [#1726](https://github.com/joyent/node/issues/1726), unref process.stdout
* Doc improvements (Ben Noordhuis, Fedor Indutny, koichik)
* Upgrade libuv to fe18438

## 2011.09.16, Version 0.5.7 (unstable)

https://github.com/nodejs/node/commit/558241166c4f3c516e5a448e676db0b57119212f

* Upgrade V8 to 3.6.4
* Improve Windows compatibility
* Documentation improvements
* Debugger and REPL improvements (Fedor Indutny)
* Add legacy API support: net.Stream(fd), process.stdout.writable,
  process.stdout.fd

* Fix mkdir EEXIST handling (isaacs)
* Use net_uv instead of net_legacy for stdio
* Do not load readline from util.inspect
* [#1673](https://github.com/joyent/node/issues/1673) Fix bug related to V8 context with accessors (Fedor Indutny)
* [#1634](https://github.com/joyent/node/issues/1634) util: Fix inspection for Error (koichik)
* [#1645](https://github.com/joyent/node/issues/1645) fs: Add positioned file writing feature to fs.WriteStream (Thomas
  Shinnick)

* [#1637](https://github.com/joyent/node/issues/1637) fs: Unguarded fs.watchFile cache statWatchers checking fixed (Thomas
  Shinnick)

* [#1695](https://github.com/joyent/node/issues/1695) Forward customFds to ChildProcess.spawn
* [#1707](https://github.com/joyent/node/issues/1707) Fix hasOwnProperty security problem in querystring (isaacs)
* [#1719](https://github.com/joyent/node/issues/1719) Drain OpenSSL error queue

## 2011.09.08, Version 0.5.6 (unstable)

https://github.com/nodejs/node/commit/b49bec55806574a47403771bce1ee379c2b09ca2

* [#345](https://github.com/joyent/node/issues/345), [#1635](https://github.com/joyent/node/issues/1635), [#1648](https://github.com/joyent/node/issues/1648) Documentation improvements (Thomas Shinnick,
  Abimanyu Raja, AJ ONeal, Koichi Kobayashi, Michael Jackson, Logan Smyth,
  Ben Noordhuis)

* [#650](https://github.com/joyent/node/issues/650) Improve path parsing on windows (Bert Belder)
* [#752](https://github.com/joyent/node/issues/752) Remove headers sent check in OutgoingMessage.getHeader()
  (Peter Lyons)

* [#1236](https://github.com/joyent/node/issues/1236), [#1438](https://github.com/joyent/node/issues/1438), [#1506](https://github.com/joyent/node/issues/1506), [#1513](https://github.com/joyent/node/issues/1513), [#1621](https://github.com/joyent/node/issues/1621), [#1640](https://github.com/joyent/node/issues/1640), [#1647](https://github.com/joyent/node/issues/1647) Libuv-related bugs fixed
  (Jorge Chamorro Bieling, Peter Bright, Luis Lavena, Igor Zinkovsky)

* [#1296](https://github.com/joyent/node/issues/1296), [#1612](https://github.com/joyent/node/issues/1612) crypto: Fix BIO's usage. (Koichi Kobayashi)
* [#1345](https://github.com/joyent/node/issues/1345) Correctly set socket.remoteAddress with libuv backend (Bert Belder)
* [#1429](https://github.com/joyent/node/issues/1429) Don't clobber quick edit mode on windows (Peter Bright)
* [#1503](https://github.com/joyent/node/issues/1503) Make libuv backend default on unix, override with `node --use-legacy`
* [#1565](https://github.com/joyent/node/issues/1565) Fix fs.stat for paths ending with \ on windows (Igor Zinkovsky)
* [#1568](https://github.com/joyent/node/issues/1568) Fix x509 certificate subject parsing (Koichi Kobayashi)
* [#1586](https://github.com/joyent/node/issues/1586) Make socket write encoding case-insensitive (Koichi Kobayashi)
* [#1591](https://github.com/joyent/node/issues/1591), [#1656](https://github.com/joyent/node/issues/1656), [#1657](https://github.com/joyent/node/issues/1657) Implement fs in libuv, remove libeio and pthread-win32
  dependency on windows (Igor Zinkovsky, Ben Noordhuis, Ryan Dahl,
  Isaac Schlueter)

* [#1592](https://github.com/joyent/node/issues/1592) Don't load-time link against CreateSymbolicLink on windows
  (Peter Bright)

* [#1601](https://github.com/joyent/node/issues/1601) Improve API consistency when dealing with the socket underlying a HTTP
  client request (Mikeal Rogers)

* [#1610](https://github.com/joyent/node/issues/1610) Remove DigiNotar CA from trusted list (Isaac Schlueter)
* [#1617](https://github.com/joyent/node/issues/1617) Added some win32 os functions (Karl Skomski)
* [#1624](https://github.com/joyent/node/issues/1624) avoid buffer overrun with 'binary' encoding (Koichi Kobayashi)
* [#1633](https://github.com/joyent/node/issues/1633) make Buffer.write() always set _charsWritten (Koichi Kobayashi)
* [#1644](https://github.com/joyent/node/issues/1644) Windows: set executables to be console programs (Peter Bright)
* [#1651](https://github.com/joyent/node/issues/1651) improve inspection for sparse array (Koichi Kobayashi)
* [#1672](https://github.com/joyent/node/issues/1672) set .code='ECONNRESET' on socket hang up errors (Ben Noordhuis)
* Add test case for foaf+ssl client certificate (Niclas Hoyer)
* Added RPATH environment variable to override run-time library paths
  (Ashok Mudukutore)

* Added TLS client-side session resumption support (Sean Cunningham)
* Added additional properties to getPeerCertificate (Nathan Rixham,
  Niclas Hoyer)

* Don't eval repl command twice when an error is thrown (Nathan Rajlich)
* Improve util.isDate() (Nathan Rajlich)
* Improvements in libuv backend and bindings, upgrade libuv to
  bd6066cb349a9b3a1b0d87b146ddaee06db31d10

* Show warning when using lib/sys.js (Maciej Malecki)
* Support plus sign in url protocol (Maciej Malecki)
* Upgrade V8 to 3.6.2

## 2011.08.26, Version 0.5.5 (unstable)

https://github.com/nodejs/node/commit/d2d53d4bb262f517a227cc178a1648094ba54c20

* typed arrays, implementation from Plesk
* fix IP multicast on SunOS
* fix DNS lookup order: IPv4 first, IPv6 second (--use-uv only)
* remove support for UNIX datagram sockets (--use-uv only)
* UDP support for Windows (Bert Belder)
* [#1572](https://github.com/joyent/node/issues/1572) improve tab completion for objects in the REPL (Nathan Rajlich)
* [#1563](https://github.com/joyent/node/issues/1563) fix buffer overflow in child_process module (reported by Dean McNamee)
* [#1546](https://github.com/joyent/node/issues/1546) fix performance regression in http module (reported by Brian Geffon)
* [#1491](https://github.com/joyent/node/issues/1491) add PBKDF2 crypto support (Glen Low)
* [#1447](https://github.com/joyent/node/issues/1447) remove deprecated http.cat() function (Mikeal Rogers)
* [#1140](https://github.com/joyent/node/issues/1140) fix incorrect dispatch of vm.runInContext's filename argument
  (Antranig Basman)

* [#1140](https://github.com/joyent/node/issues/1140) document vm.runInContext() and vm.createContext() (Antranig Basman)
* [#1428](https://github.com/joyent/node/issues/1428) fix os.freemem() on 64 bits freebsd (Artem Zaytsev)
* [#1164](https://github.com/joyent/node/issues/1164) make all DNS lookups async, fixes uncatchable exceptions
  (Koichi Kobayashi)

* fix incorrect ssl shutdown check (Tom Hughes)
* various cmake fixes (Tom Hughes)
* improved documentation (Koichi Kobayashi, Logan Smyth, Fedor Indutny,
  Mikeal Rogers, Maciej Małecki, Antranig Basman, Mickaël Delahaye)

* upgrade libuv to commit 835782a
* upgrade V8 to 3.5.8

## 2011.08.12, Version 0.5.4 (unstable)

https://github.com/nodejs/node/commit/cfba1f59224ff8602c3fe9145181cad4c6df89a9

* libuv/Windows compatibility improvements
* Build on Microsoft Visual Studio via GYP. Use generate-projects.bat in the
  to build sln files. (Peter Bright, Igor Zinkovsky)

* Make Mikeal's HTTP agent client the default. Use old HTTP client with
  --use-http1

* Fixes https host header default port handling. (Mikeal Rogers)
* [#1440](https://github.com/joyent/node/issues/1440) strip byte order marker when loading *.js and *.json files
  (Ben Noordhuis)

* [#1434](https://github.com/joyent/node/issues/1434) Improve util.format() compatibility with browser. (Koichi Kobayashi)
* Provide unchecked uint entry points for integer Buffer.read/writeInt
  methods. (Robert Mustacchi)

* CMake improvements (Tom Huges)
* Upgrade V8 to 3.5.4.

## 2011.08.01, Version 0.5.3 (unstable)

https://github.com/nodejs/node/commit/4585330afef44ddfb6a4054bd9b0f190b352628b

* Fix crypto encryption/decryption with Base64. (SAWADA Tadashi)
* [#243](https://github.com/joyent/node/issues/243) Add an optional length argument to Buffer.write() (koichik)
* [#657](https://github.com/joyent/node/issues/657) convert nonbuffer data to string in fs.writeFile/Sync
  (Daniel Pihlström)

* Add process.features, remove process.useUV (Ben Noordhuis)
* [#324](https://github.com/joyent/node/issues/324) Fix crypto hmac to accept binary keys + add test cases from rfc 2202
  and 4231 (Stefan Bühler)

* Add Socket::bytesRead, Socket::bytesWritten (Alexander Uvarov)
* [#572](https://github.com/joyent/node/issues/572) Don't print result of --eval in CLI (Ben Noordhuis)
* [#1223](https://github.com/joyent/node/issues/1223) Fix http.ClientRequest crashes if end() was called twice (koichik)
* [#1383](https://github.com/joyent/node/issues/1383) Emit 'close' after all connections have closed (Felix Geisendörfer)
* Add sprintf-like util.format() function (Ben Noordhuis)
* Add support for TLS SNI (Fedor Indutny)
* New http agent implementation. Off by default the command line flag
  --use-http2 will enable it. "make test-http2" will run the tests
	for the new implementation. (Mikeal Rogers)

* Revert AMD compatibility. (isaacs)
* Windows: improvements, child_process support.
* Remove pkg-config file.
* Fix startup time regressions.
* doc improvements

## 2011.07.22, Version 0.5.2 (unstable)

https://github.com/nodejs/node/commit/08ffce1a00dde1199174b390a64a90b60768ddf5

* libuv improvements; named pipe support
* [#1242](https://github.com/joyent/node/issues/1242) check for SSL_COMP_get_compression_methods() (Ben Noordhuis)
* [#1348](https://github.com/joyent/node/issues/1348) remove require.paths (isaacs)
* [#1349](https://github.com/joyent/node/issues/1349) Delimit NODE_PATH with ; on Windows (isaacs)
* [#1335](https://github.com/joyent/node/issues/1335) Remove EventEmitter from C++
* [#1357](https://github.com/joyent/node/issues/1357) Load json files with require() (isaacs)
* [#1374](https://github.com/joyent/node/issues/1374) fix setting ServerResponse.statusCode in writeHead (Trent Mick)
* Fixed: GC was being run too often.
* Upgrade V8 to 3.4.14
* doc improvements

## 2011.07.14, Version 0.5.1 (unstable)

https://github.com/nodejs/node/commit/f8bfa54d0fa509f9242637bef2869a1b1e842ec8

* [#1233](https://github.com/joyent/node/issues/1233) Fix os.totalmem on FreeBSD amd64 (Artem Zaytsev)
* [#1149](https://github.com/joyent/node/issues/1149) IDNA and Punycode support in url.parse
  (Jeremy Selier, Ben Noordhuis, isaacs)

* Export $CC and $CXX to uv and V8's build systems
* Include pthread-win32 static libraries in build (Igor Zinkovsky)
* [#1199](https://github.com/joyent/node/issues/1199), [#1094](https://github.com/joyent/node/issues/1094) Fix fs can't handle large file on 64bit platform (koichik)
* [#1281](https://github.com/joyent/node/issues/1281) Make require a public member of module (isaacs)
* [#1303](https://github.com/joyent/node/issues/1303) Stream.pipe returns the destination (Elijah Insua)
* [#1229](https://github.com/joyent/node/issues/1229) Addons should not -DEV_MULTIPLICITY=0 (Brian White)
* libuv backend improvements
* Upgrade V8 to 3.4.10

## 2011.07.05, Version 0.5.0 (unstable)

https://github.com/nodejs/node/commit/ae7ed8482ea7e53c59acbdf3cf0e0a0ae9d792cd

* New non-default libuv backend to support IOCP on Windows.
  Use --use-uv to enable.

* deprecate http.cat
* docs improved.
* add child_process.fork
* add fs.utimes() and fs.futimes() support (Ben Noordhuis)
* add process.uptime() (Tom Huges)
* add path.relative (Tony Huang)
* add os.getNetworkInterfaces()
* add remoteAddress and remotePort for client TCP connections
  (Brian White)

* add secureOptions flag, setting ciphers,
  SSL_OP_CRYPTOPRO_TLSEXT_BUG to TLS (Theo Schlossnagle)

* add process.arch (Nathan Rajlich)
* add reading/writing of floats and doubles from/to buffers (Brian White)
* Allow script to be read from stdin
* [#477](https://github.com/joyent/node/issues/477) add Buffer::fill method to do memset (Konstantin Käfer)
* [#573](https://github.com/joyent/node/issues/573) Diffie-Hellman support to crypto module (Håvard Stranden)
* [#695](https://github.com/joyent/node/issues/695) add 'hex' encoding to buffer (isaacs)
* [#851](https://github.com/joyent/node/issues/851) Update how REPLServer uses contexts (Ben Weaver)
* [#853](https://github.com/joyent/node/issues/853) add fs.lchow, fs.lchmod, fs.fchmod, fs.fchown (isaacs)
* [#889](https://github.com/joyent/node/issues/889) Allow to remove all EventEmitter listeners at once
  (Felix Geisendörfer)

* [#926](https://github.com/joyent/node/issues/926) OpenSSL NPN support (Fedor Indutny)
* [#955](https://github.com/joyent/node/issues/955) Change ^C handling in REPL (isaacs)
* [#979](https://github.com/joyent/node/issues/979) add support for Unix Domain Sockets to HTTP (Mark Cavage)
* [#1173](https://github.com/joyent/node/issues/1173) [#1170](https://github.com/joyent/node/issues/1170) add AMD, asynchronous module definition (isaacs)
* DTrace probes: support X-Forwarded-For (Dave Pacheco)

## 2011.09.15, Version 0.4.12 (stable)

https://github.com/nodejs/node/commit/771ba34ca7b839add2ef96879e1ffc684813cf7c

* Improve docs
* [#1563](https://github.com/joyent/node/issues/1563) overflow in ChildProcess custom_fd.
* [#1569](https://github.com/joyent/node/issues/1569), parse error on multi-line HTTP headers. (Ben Noordhuis)
* [#1586](https://github.com/joyent/node/issues/1586) net: Socket write encoding case sensitivity (koichik)
* [#1610](https://github.com/joyent/node/issues/1610) Remove DigiNotar CA from trusted list (isaacs)
* [#1624](https://github.com/joyent/node/issues/1624) buffer: Avoid overrun with 'binary' encoding. (koichik)
* [#1633](https://github.com/joyent/node/issues/1633) buffer: write() should always set _charsWritten. (koichik)
* [#1707](https://github.com/joyent/node/issues/1707) hasOwnProperty usage security hole in querystring (isaacs)
* [#1719](https://github.com/joyent/node/issues/1719) Drain OpenSSL error queue
* Fix error reporting in net.Server.listen

## 2011.08.17, Version 0.4.11 (stable)

https://github.com/nodejs/node/commit/a745d19ce7d1c0e3778371af4f0346be70cf2c8e

* [#738](https://github.com/joyent/node/issues/738) Fix crypto encryption/decryption with Base64. (SAWADA Tadashi)
* [#1202](https://github.com/joyent/node/issues/1202) net.createConnection defer DNS lookup error events to next tick
  (Ben Noordhuis)

* [#1374](https://github.com/joyent/node/issues/1374) fix setting ServerResponse.statusCode in writeHead (Trent Mick)
* [#1417](https://github.com/joyent/node/issues/1417) Fix http.ClientRequest crashes if end() was called twice
* [#1497](https://github.com/joyent/node/issues/1497) querystring: Replace 'in' test with 'hasOwnProperty' (isaacs)
* [#1546](https://github.com/joyent/node/issues/1546) http perf improvement
* fix memleak in libeio (Tom Hughes)
* cmake improvements (Tom Hughes)
* node_net.cc: fix incorrect sizeof() (Tom Hughes)
* Windows/cygwin: no more GetConsoleTitleW errors on XP (Bert Belder)
* Doc improvements (koichik, Logan Smyth, Ben Noordhuis, Arnout Kazemier)

## 2011.07.19, Version 0.4.10 (stable)

https://github.com/nodejs/node/commit/1b8dd65d6e3b82b6863ef38835cc436c5d30c1d5

* [#394](https://github.com/joyent/node/issues/394) Fix Buffer drops last null character in UTF-8
* [#829](https://github.com/joyent/node/issues/829) Backport r8577 from V8 (Ben Noordhuis)
* [#877](https://github.com/joyent/node/issues/877) Don't wait for HTTP Agent socket pool to establish connections.
* [#915](https://github.com/joyent/node/issues/915) Find kqueue on FreeBSD correctly (Brett Kiefer)
* [#1085](https://github.com/joyent/node/issues/1085) HTTP: Fix race in abort/dispatch code (Stefan Rusu)
* [#1274](https://github.com/joyent/node/issues/1274) debugger improvement (Yoshihiro Kikuchi)
* [#1291](https://github.com/joyent/node/issues/1291) Properly respond to HEAD during end(body) hot path (Reid Burke)
* [#1304](https://github.com/joyent/node/issues/1304) TLS: Fix race in abort/connection code (Stefan Rusu)
* [#1360](https://github.com/joyent/node/issues/1360) Allow _ in url hostnames.
* Revert 37d529f8 - unbreaks debugger command parsing.
* Bring back global execScript
* Doc improvements

## 2011.06.29, Version 0.4.9 (stable)

https://github.com/nodejs/node/commit/de44eafd7854d06cd85006f509b7051e8540589b

* Improve documentation
* [#1095](https://github.com/joyent/node/issues/1095) error handling bug in stream.pipe() (Felix Geisendörfer)
* [#1097](https://github.com/joyent/node/issues/1097) Fix a few leaks in node_crypto.cc (Ben Noordhuis)
* [#562](https://github.com/joyent/node/issues/562) [#1078](https://github.com/joyent/node/issues/1078) Parse file:// urls properly (Ryan Petrello)
* [#880](https://github.com/joyent/node/issues/880) Option to disable SSLv2 (Jérémy Lal)
* [#1087](https://github.com/joyent/node/issues/1087) Disabling SSL compression disabled with early OpenSSLs.
* [#1144](https://github.com/joyent/node/issues/1144) debugger: don't allow users to input non-valid commands
  (Siddharth Mahendraker)

* Perf improvement for util.inherits
* [#1166](https://github.com/joyent/node/issues/1166) Support for signature verification with RSA/DSA public keys
  (Mark Cavage)

* [#1177](https://github.com/joyent/node/issues/1177) Remove node_modules lookup optimization to better support
  nested project structures (Mathias Buus)

* [#1203](https://github.com/joyent/node/issues/1203) Add missing scope.Close to fs.sendfileSync
* [#1187](https://github.com/joyent/node/issues/1187) Support multiple 'link' headers
* [#1196](https://github.com/joyent/node/issues/1196) Fix -e/--eval can't load module from node_modules (Koichi Kobayashi)
* Upgrade V8 to 3.1.8.25, upgrade http-parser.

## 2011.05.20, Version 0.4.8 (stable)

https://github.com/nodejs/node/commit/7dd22c26e4365698dc3efddf138c4d399cb912c8

* [#974](https://github.com/joyent/node/issues/974) Properly report traceless errors (isaacs)
* [#983](https://github.com/joyent/node/issues/983) Better JSON.parse error detection in REPL (isaacs)
* [#836](https://github.com/joyent/node/issues/836) Agent socket errors bubble up to req only if req exists
* [#1041](https://github.com/joyent/node/issues/1041) Fix event listener leak check timing (koichik)
*	[#1038](https://github.com/joyent/node/issues/1038) Fix dns.resolve() with 'PTR' throws Error: Unknown type "PTR"
  (koichik)

* [#1073](https://github.com/joyent/node/issues/1073) Share SSL context between server connections (Fedor Indutny)
* Disable compression with OpenSSL. Improves memory perf.
* Implement os.totalmem() and os.freemem() for SunOS (Alexandre Marangone)
* Fix a special characters in URL regression (isaacs)
* Fix idle timeouts in HTTPS (Felix Geisendörfer)
* SlowBuffer.write() with 'ucs2' throws ReferenceError. (koichik)
* http.ServerRequest 'close' sometimes gets an error argument
  (Felix Geisendörfer)

* Doc improvements
* cleartextstream.destroy() should close(2) the socket. Previously was being
	mapped to a shutdown(2) syscall.

* No longer compile out asserts and debug statements in normal build.
* Debugger improvements.
* Upgrade V8 to 3.1.8.16.

## 2011.04.22, Version 0.4.7 (stable)

https://github.com/nodejs/node/commit/c85455a954411b38232e79752d4abb61bb75031b

* Don't emit error on ECONNRESET from read() [#670](https://github.com/joyent/node/issues/670)
* Fix: Multiple pipes to the same stream were broken [#929](https://github.com/joyent/node/issues/929)
  (Felix Geisendörfer)

* URL parsing/formatting corrections [#954](https://github.com/joyent/node/issues/954) (isaacs)
* make it possible to do repl.start('', stream) (Wade Simmons)
* Add os.loadavg for SunOS (Robert Mustacchi)
* Fix timeouts with floating point numbers [#897](https://github.com/joyent/node/issues/897) (Jorge Chamorro Bieling)
* Improve docs.

## 2011.04.13, Version 0.4.6 (stable)

https://github.com/nodejs/node/commit/58002d56bc79410c5ff397fc0e1ffec0665db38a

* Don't error on ENOTCONN from shutdown() [#670](https://github.com/joyent/node/issues/670)
* Auto completion of built-in debugger suggests prefix match rather than
	partial match. (koichik)

* circular reference in vm modules. [#822](https://github.com/joyent/node/issues/822) (Jakub Lekstan)
* http response.readable should be false after 'end' [#867](https://github.com/joyent/node/issues/867) (Abe Fettig)
* Implement os.cpus() and os.uptime() on Solaris (Scott McWhirter)
* fs.ReadStream: Allow omission of end option for range reads [#801](https://github.com/joyent/node/issues/801)
	(Felix Geisendörfer)

* Buffer.write() with UCS-2 should not be write partial char
	[#916](https://github.com/joyent/node/issues/916) (koichik)

* Pass secureProtocol through on tls.Server creation (Theo Schlossnagle)
* TLS use RC4-SHA by default
* Don't strangely drop out of event loop on HTTPS client uploads [#892](https://github.com/joyent/node/issues/892)
* Doc improvements
* Upgrade v8 to 3.1.8.10

## 2011.04.01, Version 0.4.5 (stable)

https://github.com/nodejs/node/commit/787a343b588de26784fef97f953420b53a6e1d73

* Fix listener leak in stream.pipe() (Mikeal Rogers)
* Retain buffers in fs.read/write() GH-814 (Jorge Chamorro Bieling)
* TLS performance improvements
* SlowBuffer.prototype.slice bug GH-843
* process.stderr.write should return true
* Immediate pause/resume race condition GH-535 (isaacs)
* Set default host header properly GH-721 (isaacs)
* Upgrade V8 to 3.1.8.8

## 2011.03.26, Version 0.4.4 (stable)

https://github.com/nodejs/node/commit/25122b986a90ba0982697b7abcb0158c302a1019

* CryptoStream.end shouldn't throw if not writable GH-820
* Drop out if connection destroyed before connect() GH-819
* expose https.Agent
* Correctly setsid in tty.open GH-815
* Bug fix for failed buffer construction
* Added support for removing .once listeners (GH-806)
* Upgrade V8 to 3.1.8.5

## 2011.03.18, Version 0.4.3 (stable)

https://github.com/nodejs/node/commit/c095ce1a1b41ca015758a713283bf1f0bd41e4c4

* Don't decrease server connection counter again if destroy() is called more
	than once GH-431 (Andreas Reich, Anders Conbere)

* Documentation improvements (koichik)
* Fix bug with setMaxListeners GH-682
* Start up memory footprint improvement. (Tom Hughes)
* Solaris improvements.
* Buffer::Length(Buffer*) should not invoke itself recursively GH-759 (Ben
  Noordhuis)

* TLS: Advertise support for client certs GH-774 (Theo Schlossnagle)
* HTTP Agent bugs: GH-787, GH-784, GH-803.
* Don't call GetMemoryUsage every 5 seconds.
* Upgrade V8 to 3.1.8.3

## 2011.03.02, Version 0.4.2 (stable)

https://github.com/nodejs/node/commit/39280e1b5731f3fcd8cc42ad41b86cdfdcb6d58b

* Improve docs.
* Fix process.on edge case with signal event (Alexis Sellier)
* Pragma HTTP header comma separation
* In addition to 'aborted' emit 'close' from incoming requests
  (Felix Geisendörfer)

* Fix memleak in vm.runInNewContext
* Do not cache modules that throw exceptions (Felix Geisendörfer)
* Build system changes for libnode (Aria Stewart)
* Read up the prototype of the 'env' object. (Nathan Rajlich)
* Add 'close' and 'aborted' events to Agent responses
* http: fix missing 'drain' events (Russell Haering)
* Fix process.stdout.end() throws ENOTSOCK error. (Koichi Kobayashi)
* REPL bug fixes (isaacs)
* node_modules folders should be highest priority (isaacs)
* URL parse more safely (isaacs)
* Expose errno with a string for dns/cares (Felix Geisendörfer)
* Fix tty.setWindowSize
* spawn: setuid after chdir (isaacs)
* SIGUSR1 should break the VM without delay
* Upgrade V8 to 3.1.8.

## 2011.02.19, Version 0.4.1 (stable)

https://github.com/nodejs/node/commit/e8aef84191bc2c1ba2bcaa54f30aabde7f03769b

* Fixed field merging with progressive fields on writeHead()
  (TJ Holowaychuk)

* Make the repl respect node_modules folders (isaacs)
* Fix for DNS fail in HTTP request (Richard Rodger)
* Default to port 80 for http.request and http.get.
* Improve V8 support for Cygwin (Bert Belder)
* Fix fs.open param parsing. (Felix Geisendörfer)
* Fixed null signal.
* Fix various HTTP and HTTPS bugs
* cmake improvements (Tom Hughes)
* Fix: TLS sockets should not be writable after 'end'
* Fix os.cpus() on cygwin (Brian White)
* MinGW: OpenSSL support (Bert Belder)
* Upgrade V8 to 3.1.5, libev to 4.4.

## 2011.02.10, Version 0.4.0 (stable)

https://github.com/nodejs/node/commit/eb155ea6f6a6aa341aa8c731dca8da545c6a4008

* require() improvements (isaacs)
  - understand package.json (isaacs)
  - look for 'node_modules' dir

* cmake fixes (Daniel Gröber)
* http: fix buffer writes to outgoing messages (Russell Haering)
* Expose UCS-2 Encoding (Konstantin Käfer)
* Support strings for octal modes (isaacs)
* Support array-ish args to Buffer ctor (isaacs)
* cygwin and mingw improvements (Bert Belder)
* TLS improvements
* Fewer syscalls during require (Bert Belder, isaacs)
* More DTrace probes (Bryan Cantrill,  Robert Mustacchi)
* 'pipe' event on pipe() (Mikeal Rogers)
* CRL support in TLS (Theo Schlossnagle)
* HTTP header manipulation methods (Tim Caswell, Charlie Robbins)
* Upgrade V8 to 3.1.2

## 2011.02.04, Version 0.3.8 (unstable)

https://github.com/nodejs/node/commit/9493b7563bff31525b4080df5aeef09747782d5e

* Add req.abort() for client side requests.
* Add exception.code for easy testing:
  Example: if (err.code == 'EADDRINUSE');

* Add process.stderr.
* require.main is the main module. (Isaac Schlueter)
* dgram: setMulticastTTL, setMulticastLoopback and addMembership.
  (Joe Walnes)

* Fix throttling in TLS connections
* Add socket.bufferSize
* MinGW improvements (Bert Belder)
* Upgrade V8 to 3.1.1

## 2011.01.27, Version 0.3.7 (unstable)

https://github.com/nodejs/node/commit/d8579c6afdbe868de6dffa8db78bbe4ba2d03e0e

* Expose agent in http and https client. (Mikeal Rogers)
* Fix bug in http request's end method. (Ali Farhadi)
* MinGW: better net support (Bert Belder)
* fs.open should set FD_CLOEXEC
* DTrace probes (Bryan Cantrill)
* REPL fixes and improvements (isaacs, Bert Belder)
* Fix many bugs with legacy http.Client interface
* Deprecate process.assert. Use require('assert').ok
* Add callback parameter to socket.setTimeout(). (Ali Farhadi)
* Fixing bug in http request default encoding (Ali Farhadi)
* require: A module ID with a trailing slash must be a dir.
  (isaacs)

* Add ext_key_usage to getPeerCertificate (Greg Hughes)
* Error when child_process.exec hits maxBuffer.
* Fix option parsing in tls.connect()
* Upgrade to V8 3.0.10

## 2011.01.21, Version 0.3.6 (unstable)

https://github.com/nodejs/node/commit/bb3e71466e5240626d9d21cf791fe43e87d90011

* REPL and other improvements on MinGW (Bert Belder)
* listen/bind errors should close net.Server
* New HTTP and HTTPS client APIs
* Upgrade V8 to 3.0.9

## 2011.01.16, Version 0.3.5 (unstable)

https://github.com/nodejs/node/commit/b622bc6305e3c675e0edfcdbaa387d849ad0bba0

* Built-in debugger improvements.
* Add setsid, setuid, setgid options to child_process.spawn
  (Isaac Schlueter)

* tty module improvements.
* Upgrade libev to 4.3, libeio to latest, c-ares to 1.7.4
* Allow third party hooks before main module load.
  (See 496be457b6a2bc5b01ec13644b9c9783976159b2)

* Don't stat() on cached modules. (Felix Geisendörfer)

## 2011.01.08, Version 0.3.4 (unstable)

https://github.com/nodejs/node/commit/73f53e12e4a5b9ef7dbb4792bd5f8ad403094441

* Primordial mingw build (Bert Belder)
* HTTPS server
* Built in debugger 'node debug script.js'
* realpath files during module load (Mihai Călin Bazon)
* Rename net.Stream to net.Socket (existing name will continue to be
  supported)

* Fix process.platform

## 2011.01.02, Version 0.3.3 (unstable)

https://github.com/nodejs/node/commit/57544ba1c54c7d0da890317deeb73076350c5647

* TLS improvements.
* url.parse(url, true) defaults query field to {} (Jeremy Martin)
* Upgrade V8 to 3.0.4
* Handle ECONNABORT properly (Theo Schlossnagle)
* Fix memory leaks (Tom Hughes)
* Add os.cpus(), os.freemem(), os.totalmem(), os.loadavg() and other
  functions for OSX, Linux, and Cygwin. (Brian White)

* Fix REPL syntax error bug (GH-543), improve how REPL commands are
  evaluated.

* Use process.stdin instead of process.openStdin().
* Disable TLS tests when node doesn't have OpenSSL.

## 2010.12.16, Version 0.3.2 (unstable)

https://github.com/nodejs/node/commit/4bb914bde9f3c2d6de00853353b6b8fc9c66143a

* Rip out the old (broken) TLS implementation introduce new tested
  implementation and API. See docs. HTTPS not supported in this release.

* Introduce 'os' and 'tty' modules.
* Callback parameters for socket.write() and socket.connect().
* Support CNAME lookups in DNS module. (Ben Noordhuis)
* cmake support (Tom Hughes)
* 'make lint'
* oprofile support (./configure --oprofile)
* Lots of bug fixes, including:
  - Memory leak in ChildProcess:Spawn(). (Tom Hughes)
  - buffer.slice(0, 0)
  - Global variable leaks
  - clearTimeouts calling multiple times (Michael W)
  - utils.inspect's detection of circular structures (Tim Cooijmans)
  - Apple's threaded write()s bug (Jorge Chamorro Bieling)
  - Make sure raw mode is disabled when exiting a terminal-based REPL.
    (Brian White)

* Deprecate process.compile, process.ENV
* Upgrade V8 to 3.0.3, upgrade http-parser.

## 2010.11.16, Version 0.3.1 (unstable)

https://github.com/nodejs/node/commit/ce9a54aa1fbf709dd30316af8a2f14d83150e947

* TLS improvements (Paul Querna)
  - Centralize error handling in SecureStream
  - Add SecurePair for handling of a ssl/tls stream.

* New documentation organization (Micheil Smith)
* allowHalfOpen TCP connections disabled by default.
* Add C++ API for constructing fast buffer from string
* Move idle timers into its own module
* Gracefully handle EMFILE and server.maxConnections
* make "node --eval" eval in the global scope.
  (Jorge Chamorro Bieling)

* Let exit listeners know the exit code (isaacs)
* Handle cyclic links smarter in fs.realpath (isaacs)
* Remove node-repl (just use 'node' without args)
* Rewrite libeio After callback to use req->result instead of req->errorno
  for error checking (Micheil Smith)

* Remove warning about deprecating 'sys' - too aggressive
* Make writes to process.env update the real environment. (Ben Noordhuis)
* Set FD_CLOEXEC flag on stdio FDs before spawning. (Guillaume Tuton)
* Move ev_loop out of javascript
* Switch \n with \r\n for all strings printed out.
* Added support for cross compilation (Rasmus Andersson)
* Add --profile flag to configure script, enables gprof profiling.
  (Ben Noordhuis)

* writeFileSync could exhibit pathological behavior when a buffer
  could not be written to the file in a single write() call.

* new path.join behavior (isaacs)
  - Express desired path.join behavior in tests.
  - Update fs.realpath to reflect new path.join behavior
  - Update url.resolve() to use new path.join behavior.

* API: Move process.binding('evals') to require('vm')
* Fix V8 build on Cygwin (Bert Belder)
* Add ref to buffer during fs.write and fs.read
* Fix segfault on test-crypto
* Upgrade http-parser to latest and V8 to 2.5.3

## 2010.10.23, Version 0.3.0 (unstable)

https://github.com/nodejs/node/commit/1582cfebd6719b2d2373547994b3dca5c8c569c0

* Bugfix: Do not spin on accept() with EMFILE
* Improvements to readline.js (Trent Mick, Johan Euphrosine, Brian White)
* Safe constructors (missing 'new' doesn't segfault)
* Fix process.nextTick so thrown errors don't confuse it.
  (Benjamin Thomas)

* Allow Strings for ports on net.Server.listen (Bradley Meck)
* fs bugfixes (Tj Holowaychuk, Tobie Langel, Marco Rogers, isaacs)
* http bug fixes (Fedor Indutny, Mikeal Rogers)
* Faster buffers; breaks C++ API (Tim-Smart, Stéphan Kochen)
* crypto, tls improvements (Paul Querna)
* Add lfs flags to node addon script
* Simpler querystring parsing; breaks API (Peter Griess)
* HTTP trailers (Mark Nottingham)
* http 100-continue support (Mark Nottingham)
* Module system simplifications (Herbert Vojčík, isaacs, Tim-Smart)
  - remove require.async
  - remove registerExtension, add .extensions
  - expose require.resolve
  - expose require.cache
  - require looks in  node_modules folders

* Add --eval command line option (TJ Holowaychuk)
* Commas last in sys.inspect
* Constants moved from process object to require('constants')
* Fix parsing of linux memory (Vitali Lovich)
* inspect shows function names (Jorge Chamorro Bieling)
* uncaughtException corner cases (Felix Geisendörfer)
* TCP clients now buffer writes before connection
* Rename sys module to 'util' (Micheil Smith)
* Properly set stdio handlers to blocking on SIGTERM and SIGINT
  (Tom Hughes)

* Add destroy methods to HTTP messages
* base64 improvements (isaacs, Jorge Chamorro Bieling)
* API for defining REPL commands (Sami Samhuri)
* child_process.exec timeout fix (Aaron Heckmann)
* Upgrade V8 to 2.5.1, Libev to 4.00, libeio, http-parser

## 2010.08.20, Version 0.2.0

https://github.com/nodejs/node/commit/9283e134e558900ba89d9a33c18a9bdedab07cb9

* process.title support for FreeBSD, Macintosh, Linux
* Fix OpenSSL 100% CPU usage on error (Illarionov Oleg)
* Implement net.Server.maxConnections.
* Fix process.platform, add process.version.
* Add --without-snapshot configure option.
* Readline REPL improvements (Trent Mick)
* Bug fixes.
* Upgrade V8 to 2.3.8

## 2010.08.13, Version 0.1.104

https://github.com/nodejs/node/commit/b14dd49222687c12f3e8eac597cff4f2674f84e8

* Various bug fixes (console, querystring, require)
* Set cwd for child processes (Bert Belder)
* Tab completion for readline (Trent Mick)
* process.title getter/setter for OSX, Linux, Cygwin.
	(Rasmus Andersson, Bert Belder)

* Upgrade V8 to 2.3.6

## 2010.08.04, Version 0.1.103

https://github.com/nodejs/node/commit/0b925d075d359d03426f0b32bb58a5e05825b4ea

* Implement keep-alive for http.Client (Mikeal Rogers)
* base64 fixes. (Ben Noordhuis)
* Fix --debug-brk (Danny Coates)
* Don't let path.normalize get above the root. (Isaac Schlueter)
* Allow signals to be used with process.on in addition to
  process.addListener. (Brian White)

* Globalize the Buffer object
* Use kqueue on recent macintosh builds
* Fix addrlen for unix_dgram sockets (Benjamin Kramer)
* Fix stats.isDirectory() and friends (Benjamin Kramer)
* Upgrade http-parser, V8 to 2.3.5

## 2010.07.25, Version 0.1.102

https://github.com/nodejs/node/commit/2a4568c85f33869c75ff43ccd30f0ec188b43eab

* base64 encoding for Buffers.
* Buffer support for Cipher, Decipher, Hmac, Sign and Verify
  (Andrew Naylor)

* Support for reading byte ranges from files using fs.createReadStream.
  (Chandra Sekar)

* Fix Buffer.toString() on 0-length slices. (Peter Griess)
* Cache modules based on filename rather than ID (Isaac Schlueter)
* querystring improvements (Jan Kassens, Micheil Smith)
* Support DEL in the REPL. (Jérémy Lal)
* Upgrade http-parser, upgrade V8 to 2.3.2

## 2010.07.16, Version 0.1.101

https://github.com/nodejs/node/commit/0174ceb6b24caa0bdfc523934c56af9600fa9b58

* Added env to child_process.exec (Сергей Крыжановский)
* Allow modules to optionally be loaded in separate contexts
  with env var NODE_MODULE_CONTEXTS=1.

* setTTL and setBroadcast for dgram (Matt Ranney)
* Use execPath for default NODE_PATH, not installPrefix
  (Isaac Schlueter)

* Support of console.dir + console.assert (Jerome Etienne)
* on() as alias to addListener()
* Use javascript port of Ronn to build docs (Jérémy Lal)
* Upgrade V8 to 2.3.0

## 2010.07.03, Version 0.1.100

https://github.com/nodejs/node/commit/a6b8586e947f9c3ced180fe68c233d0c252add8b

* process.execPath (Marshall Culpepper)
* sys.pump (Mikeal Rogers)
* Remove ini and mjsunit libraries.
* Introduce console.log() and friends.
* Switch order of arguments for Buffer.write (Blake Mizerany)
* On overlapping buffers use memmove (Matt Ranney)
* Resolve .local domains with getaddrinfo()
* Upgrade http-parser, V8 to 2.2.21

## 2010.06.21, Version 0.1.99

https://github.com/nodejs/node/commit/a620b7298f68f68a855306437a3b60b650d61d78

* Datagram sockets (Paul Querna)
* fs.writeFile could not handle utf8 (Felix Geisendörfer)
  and now accepts Buffers (Aaron Heckmann)

* Fix crypto memory leaks.
* A replacement for decodeURIComponent that doesn't throw.
  (Isaac Schlueter)

* Only concatenate some incoming HTTP headers. (Peter Griess)
* Upgrade V8 to 2.2.18

## 2010.06.11, Version 0.1.98

https://github.com/nodejs/node/commit/10d8adb08933d1d4cea60192c2a31c56d896733d

* Port to Windows/Cygwin (Raffaele Sena)
* File descriptor passing on unix sockets. (Peter Griess)
* Simple, builtin readline library. REPL is now entered by
  executing "node" without arguments.

* Add a parameter to spawn() that sets the child's stdio file
  descriptors. (Orlando Vazquez)

* Upgrade V8 to 2.2.16, http-parser fixes, upgrade c-ares to 1.7.3.

## 2010.05.29, Version 0.1.97

https://github.com/nodejs/node/commit/0c1aa36835fa6a3557843dcbc6ed6714d353a783

* HTTP throttling: outgoing messages emit 'drain' and write() returns false
  when send buffer is full.

* API: readFileSync without encoding argument now returns a Buffer
* Improve Buffer C++ API; addons now compile with debugging symbols.
* Improvements to  path.extname() and REPL; add fs.chown().
* fs.ReadStream now emits buffers, fs.readFileSync returns buffers.
* Bugfix: parsing HTTP responses to HEAD requests.
* Port to OpenBSD.
* Upgrade V8 to 2.2.12, libeio, http-parser.

## 2010.05.21, Version 0.1.96

https://github.com/nodejs/node/commit/9514a4d5476225e8c8310ce5acae2857033bcaaa

* Thrown errors in http and socket call back get bubbled up.
* Add fs.fsync (Andrew Johnston)
* Bugfix: signal unregistering (Jonas Pfenniger)
* Added better error messages for async and sync fs calls with paths
  (TJ Holowaychuk)

* Support arrays and strings in buffer constructor.
  (Felix Geisendörfer)

* Fix errno reporting in DNS exceptions.
* Support buffers in fs.WriteStream.write.
* Bugfix: Safely decode a utf8 streams that are broken on a multbyte
  character (http and net). (Felix Geisendörfer)

* Make Buffer's C++ constructor public.
* Deprecate sys.p()
* FIX path.dirname('/tmp') => '/'. (Jonathan Rentzsch)

## 2010.05.13, Version 0.1.95

https://github.com/nodejs/node/commit/0914d33842976c2c870df06573b68f9192a1fb7a

* Change GC idle notify so that it runs alongside setInterval
* Install node_buffer.h on make install
* fs.readFile returns Buffer by default (Tim Caswell)
* Fix error reporting in child_process callbacks
* Better logic for testing if an argument is a port
* Improve error reporting (single line "node.js:176:9" errors)
* Bugfix: Some http responses being truncated (appeared in 0.1.94)
* Fix long standing net idle timeout bugs. Enable 2 minute timeout
  by default in HTTP servers.

* Add fs.fstat (Ben Noordhuis)
* Upgrade to V8 2.2.9

## 2010.05.06, Version 0.1.94

https://github.com/nodejs/node/commit/f711d5343b29d1e72e87107315708e40951a7826

* Look in /usr/local/lib/node for modules, so that there's a way
  to install modules globally (Issac Schlueter)

* SSL improvements (Rhys Jones, Paulo Matias)
* Added c-ares headers for linux-arm (Jonathan Knezek)
* Add symbols to release build
* HTTP upgrade improvements, docs (Micheil Smith)
* HTTP server emits 'clientError' instead of printing message
* Bugfix: Don't emit 'error' twice from http.Client
* Bugfix: Ignore SIGPIPE
* Bugfix: destroy() instead of end() http connection at end of
  pipeline

* Bugfix: http.Client may be prematurely released back to the
  free pool.  (Thomas Lee)

* Upgrade V8 to 2.2.8

## 2010.04.29, Version 0.1.93

https://github.com/nodejs/node/commit/557ba6bd97bad3afe0f9bd3ac07efac0a39978c1

  * Fixed no 'end' event on long chunked HTTP messages
    https://github.com/joyent/node/issues/77

  * Remove legacy modules http_old and tcp_old
  * Support DNS MX queries (Jérémy Lal)

  * Fix large socket write (tlb@tlb.org)
  * Fix child process exit codes (Felix Geisendörfer)

  * Allow callers to disable PHP/Rails style parameter munging in
    querystring.stringify (Thomas Lee)

  * Upgrade V8 to 2.2.6

## 2010.04.23, Version 0.1.92

https://github.com/nodejs/node/commit/caa828a242f39b6158084ef4376355161c14fe34

  * OpenSSL support. Still undocumented (see tests). (Rhys Jones)
  * API: Unhandled 'error' events throw.

  * Script class with eval-function-family in binding('evals') plus tests.
    (Herbert Vojcik)

  * stream.setKeepAlive (Julian Lamb)
  * Bugfix: Force no body on http 204 and 304

  * Upgrade Waf to 1.5.16, V8 to 2.2.4.2

## 2010.04.15, Version 0.1.91

https://github.com/nodejs/node/commit/311d7dee19034ff1c6bc9098c36973b8d687eaba

  * Add incoming.httpVersion
  * Object.prototype problem with C-Ares binding

  * REPL can be run from multiple different streams. (Matt Ranney)
  * After V8 heap is compact, don't use a timer every 2 seconds.

  * Improve nextTick implementation.
  * Add primative support for Upgrading HTTP connections.
    (See commit log for docs 760bba5)

  * Add timeout and maxBuffer options to child_process.exec
  * Fix bugs.

  * Upgrade V8 to 2.2.3.1

## 2010.04.09, Version 0.1.90

https://github.com/nodejs/node/commit/07e64d45ffa1856e824c4fa6afd0442ba61d6fd8

  * Merge writing of networking system (net2)
   - New Buffer object for binary data.
   - Support UNIX sockets, Pipes
   - Uniform stream API
   - Currently no SSL
   - Legacy modules can be accessed at 'http_old' and 'tcp_old'

  * Replace udns with c-ares. (Krishna Rajendran)
  * New documentation system using Markdown and Ronn
    (Tim Caswell, Micheil Smith)

  * Better idle-time GC
  * Countless small bug fixes.

  * Upgrade V8 to 2.2.X, WAF 1.5.15

## 2010.03.19, Version 0.1.33

https://github.com/nodejs/node/commit/618296ef571e873976f608d91a3d6b9e65fe8284

  * Include lib/ directory in node executable. Compile on demand.
  * evalcx clean ups (Isaac Z. Schlueter, Tim-Smart)

  * Various fixes, clean ups
  * V8 upgraded to 2.1.5

## 2010.03.12, Version 0.1.32

https://github.com/nodejs/node/commit/61c801413544a50000faa7f58376e9b33ba6254f

  * Optimize event emitter for single listener
  * Add process.evalcx, require.registerExtension (Tim Smart)

  * Replace --cflags with --vars
  * Fix bugs in fs.create*Stream (Felix Geisendörfer)

  * Deprecate process.mixin, process.unloop
  * Remove the 'Error: (no message)' exceptions, print stack
    trace instead

  * INI parser bug fixes (Isaac Schlueter)
  * FreeBSD fixes (Vanilla Hsu)

  * Upgrade to V8 2.1.3, WAF 1.5.14a, libev

## 2010.03.05, Version 0.1.31

https://github.com/nodejs/node/commit/39b63dfe1737d46a8c8818c92773ef181fd174b3

  * API: - Move process.watchFile into fs module
         - Move process.inherits to sys

  * Improve Solaris port
  * tcp.Connection.prototype.write now returns boolean to indicate if
    argument was flushed to the kernel buffer.

  * Added fs.link, fs.symlink, fs.readlink, fs.realpath
    (Rasmus Andersson)

  * Add setgid,getgid (James Duncan)
  * Improve sys.inspect (Benjamin Thomas)

  * Allow passing env to child process (Isaac Schlueter)
  * fs.createWriteStream, fs.createReadStream (Felix Geisendörfer)

  * Add INI parser (Rob Ellis)
  * Bugfix: fs.readFile handling encoding (Jacek Becela)

  * Upgrade V8 to 2.1.2

## 2010.02.22, Version 0.1.30

https://github.com/nodejs/node/commit/bb0d1e65e1671aaeb21fac186b066701da0bc33b

  * Major API Changes
    - Promises removed. See
      http://groups.google.com/group/nodejs/msg/426f3071f3eec16b
      http://groups.google.com/group/nodejs/msg/df199d233ff17efa
      The API for fs was
         fs.readdir("/usr").addCallback(function (files) {
           puts("/usr files: " + files);
         });
      It is now
         fs.readdir("/usr", function (err, files) {
           if (err) throw err;
           puts("/usr files: " + files);
         });
    - Synchronous fs operations exposed, use with care.
    - tcp.Connection.prototype.readPause() and readResume()
      renamed to pause() and resume()
    - http.ServerResponse.prototype.sendHeader() renamed to
      writeHeader(). Now accepts reasonPhrase.

  * Compact garbage on idle.
  * Configurable debug ports, and --debug-brk (Zoran Tomicic)

  * Better command line option parsing (Jeremy Ashkenas)
  * Add fs.chmod (Micheil Smith), fs.lstat (Isaac Z. Schlueter)

  * Fixes to process.mixin (Rasmus Andersson, Benjamin Thomas)
  * Upgrade V8 to 2.1.1

## 2010.02.17, Version 0.1.29

https://github.com/nodejs/node/commit/87d5e5b316a4276bcf881f176971c1a237dcdc7a

  * Major API Changes
    - Remove 'file' module
    - require('posix') -----------------> require('fs')
    - fs.cat ---------------------------> fs.readFile
    - file.write -----------------------> fs.writeFile
    - TCP 'receive' event --------------> 'data'
    - TCP 'eof' event ------------------> 'end'
    - TCP send() -----------------------> write()
    - HTTP sendBody() ------------------> write()
    - HTTP finish() --------------------> close()
    - HTTP 'body' event ----------------> 'data'
    - HTTP 'complete' event ------------> 'end'
    - http.Client.prototype.close() (formerly finish()) no longer
      takes an argument. Add the 'response' listener manually.
    - Allow strings for the flag argument to fs.open
      ("r", "r+", "w", "w+", "a", "a+")

  * Added multiple arg support for sys.puts(), print(), etc.
    (tj@vision-media.ca)

  * sys.inspect(Date) now shows the date value (Mark Hansen)
  * Calculate page size with getpagesize for armel (Jérémy Lal)

  * Bugfix: stderr flushing.
  * Bugfix: Promise late chain (Yuichiro MASUI)

  * Bugfix: wait() on fired promises
    (Felix Geisendörfer, Jonas Pfenniger)

  * Bugfix: Use InstanceTemplate() instead of PrototypeTemplate() for
    accessor methods. Was causing a crash with Eclipse debugger.
    (Zoran Tomicic)

  * Bugfix: Throw from connection.connect if resolving.
    (Reported by James Golick)

## 2010.02.09, Version 0.1.28

https://github.com/nodejs/node/commit/49de41ef463292988ddacfb01a20543b963d9669

  * Use Google's jsmin.py which can be used for evil.
  * Add posix.truncate()

  * Throw errors from server.listen()
  * stdio bugfix (test by Mikeal Rogers)

  * Module system refactor (Felix Geisendörfer, Blaine Cook)
  * Add process.setuid(), getuid() (Michael Carter)

  * sys.inspect refactor (Tim Caswell)
  * Multipart library rewrite (isaacs)

## 2010.02.03, Version 0.1.27

https://github.com/nodejs/node/commit/0cfa789cc530848725a8cb5595224e78ae7b9dd0

  * Implemented __dirname (Felix Geisendörfer)
  * Downcase process.ARGV, process.ENV, GLOBAL
    (now process.argv, process.env, global)

  * Bug Fix: Late promise promise callbacks firing
    (Felix Geisendörfer, Jonas Pfenniger)

  * Make assert.AssertionError instance of Error
  * Removed inline require call for querystring
    (self@cloudhead.net)

  * Add support for MX, TXT, and SRV records in DNS module.
    (Blaine Cook)

  * Bugfix: HTTP client automatically reconnecting
  * Adding OS X .dmg build scripts. (Standa Opichal)

  * Bugfix: ObjectWrap memory leak
  * Bugfix: Multipart handle Content-Type headers with charset
    (Felix Geisendörfer)

  * Upgrade http-parser to fix header overflow attack.
  * Upgrade V8 to 2.1.0

  * Various other bug fixes, performance improvements.

## 2010.01.20, Version 0.1.26

https://github.com/nodejs/node/commit/da00413196e432247346d9e587f8c78ce5ceb087

  * Bugfix, HTTP eof causing crash (Ben Williamson)
  * Better error message on SyntaxError

  * API: Move Promise and EventEmitter into 'events' module
  * API: Add process.nextTick()

  * Allow optional params to setTimeout, setInterval
    (Micheil Smith)

  * API: change some Promise behavior (Felix Geisendörfer)
    - Removed Promise.cancel()
    - Support late callback binding
    - Make unhandled Promise errors throw an exception

  * Upgrade V8 to 2.0.6.1
  * Solaris port (Erich Ocean)

## 2010.01.09, Version 0.1.25

https://github.com/nodejs/node/commit/39ca93549af91575ca9d4cbafd1e170fbcef3dfa

  * sys.inspect() improvements (Tim Caswell)
  * path module improvements (isaacs, Benjamin Thomas)

  * API: request.uri -> request.url
    It is no longer an object, but a string. The 'url' module
    was added to parse that string. That is, node no longer
    parses the request URL automatically.
       require('url').parse(request.url)
    is roughly equivalent to the old request.uri object.
    (isaacs)

  * Bugfix: Several libeio related race conditions.
  * Better errors for multipart library (Felix Geisendörfer)

  * Bugfix: Update node-waf version to 1.5.10
  * getmem for freebsd (Vanilla Hsu)

## 2009.12.31, Version 0.1.24

https://github.com/nodejs/node/commit/642c2773a7eb2034f597af1cd404b9e086b59632

  * Bugfix: don't chunk responses to HTTP/1.0 clients, even if
    they send Connection: Keep-Alive (e.g. wget)

  * Bugfix: libeio race condition
  * Bugfix: Don't segfault on unknown http method

  * Simplify exception reporting
  * Upgrade V8 to 2.0.5.4

## 2009.12.22, Version 0.1.23

https://github.com/nodejs/node/commit/f91e347eeeeac1a8bd6a7b462df0321b60f3affc

  * Bugfix: require("../blah") issues (isaacs)
  * Bugfix: posix.cat (Jonas Pfenniger)

  * Do not pause request for multipart parsing (Felix Geisendörfer)

## 2009.12.19, Version 0.1.22

https://github.com/nodejs/node/commit/a2d809fe902f6c4102dba8f2e3e9551aad137c0f

  * Bugfix: child modules get wrong id with "index.js" (isaacs)
  * Bugfix: require("../foo") cycles (isaacs)

  * Bugfix: require() should throw error if module does.
  * New URI parser stolen from Narwhal (isaacs)

  * Bugfix: correctly check kqueue and epoll. (Rasmus Andersson)
  * Upgrade WAF to 1.5.10

  * Bugfix: posix.statSync() was crashing
  * Statically define string symbols for performance improvement

  * Bugfix: ARGV[0] weirdness
  * Added superCtor to ctor.super_ instead superCtor.prototype.
    (Johan Dahlberg)

  * http-parser supports webdav methods
  * API: http.Client.prototype.request() (Christopher Lenz)

## 2009.12.06, Version 0.1.21

https://github.com/nodejs/node/commit/c6affb64f96a403a14d20035e7fbd6d0ce089db5

  * Feature: Add HTTP client TLS support (Rhys Jones)
  * Bugfix: use --jobs=1 with WAF

  * Bugfix: Don't use chunked encoding for 1.0 requests
  * Bugfix: Duplicated header weren't handled correctly

  * Improve sys.inspect (Xavier Shay)
  * Upgrade v8 to 2.0.3

  * Use CommonJS assert API (Felix Geisendörfer, Karl Guertin)

## 2009.11.28, Version 0.1.20

https://github.com/nodejs/node/commit/aa42c6790da8ed2cd2b72051c07f6251fe1724d8

  * Add gnutls version to configure script
  * Add V8 heap info to process.memoryUsage()

  * process.watchFile callback has 2 arguments with the stat object
    (choonkeat@gmail.com)

## 2009.11.28, Version 0.1.19

https://github.com/nodejs/node/commit/633d6be328708055897b72327b88ac88e158935f

  * Feature: Initial TLS support for TCP servers and clients.
    (Rhys Jones)

  * Add options to process.watchFile()
  * Add process.umask() (Friedemann Altrock)

  * Bugfix: only detach timers when active.
  * Bugfix: lib/file.js write(), shouldn't always emit errors or success
    (onne@onnlucky.com)

  * Bugfix: Memory leak in fs.write
    (Reported by onne@onnlucky.com)

  * Bugfix: Fix regular expressions detecting outgoing message headers.
    (Reported by Elliott Cable)

  * Improvements to Multipart parser (Felix Geisendörfer)
  * New HTTP parser

  * Upgrade v8 to 2.0.2

## 2009.11.17, Version 0.1.18

https://github.com/nodejs/node/commit/027829d2853a14490e6de9fc5f7094652d045ab8

  * Feature: process.watchFile() process.unwatchFile()
  * Feature: "uncaughtException" event on process
    (Felix Geisendörfer)

  * Feature: 'drain' event to tcp.Connection
  * Bugfix: Promise.timeout() blocked the event loop
    (Felix Geisendörfer)

  * Bugfix: sendBody() and chunked utf8 strings
    (Felix Geisendörfer)

  * Supply the strerror as a second arg to the tcp.Connection close
    event (Johan Sørensen)

  * Add EventEmitter.removeListener (frodenius@gmail.com)
  * Format JSON for inspecting objects (Felix Geisendörfer)

  * Upgrade libev to latest CVS

## 2009.11.07, Version 0.1.17

https://github.com/nodejs/node/commit/d1f69ef35dac810530df8249d523add168e09f03

  * Feature: process.chdir() (Brandon Beacher)
  * Revert http parser upgrade. (b893859c34f05db5c45f416949ebc0eee665cca6)
    Broke keep-alive.

  * API: rename process.inherits to sys.inherits

## 2009.11.03, Version 0.1.16

https://github.com/nodejs/node/commit/726865af7bbafe58435986f4a193ff11c84e4bfe

  * API: Use CommonJS-style module requiring
    - require("/sys.js") becomes require("sys")
    - require("circle.js") becomes require("./circle")
    - process.path.join() becomes require("path").join()
    - __module becomes module

  * API: Many namespacing changes
    - Move node.* into process.*
    - Move node.dns into module "dns"
    - Move node.fs into module "posix"
    - process is no longer the global object. GLOBAL is.
  For more information on the API changes see:
    http://thread.gmane.org/gmane.comp.lang.javascript.nodejs/6
    http://thread.gmane.org/gmane.comp.lang.javascript.nodejs/14

  * Feature: process.platform, process.memoryUsage()
  * Feature: promise.cancel() (Felix Geisendörfer)

  * Upgrade V8 to 1.3.18

## 2009.10.28, Version 0.1.15

https://github.com/nodejs/node/commit/eca2de73ed786b935507fd1c6faccd8df9938fd3

  * Many build system fixes (esp. for OSX users)
  * Feature: promise.timeout() (Felix Geisendörfer)

  * Feature: Added external interface for signal handlers, process.pid, and
    process.kill() (Brandon Beacher)

  * API: Rename node.libraryPaths to require.paths
  * Bugfix: 'data' event for stdio should emit a string

  * Large file support
  * Upgrade http_parser

  * Upgrade v8 to 1.3.16

## 2009.10.09, Version 0.1.14

https://github.com/nodejs/node/commit/b12c809bb84d1265b6a4d970a5b54ee8a4890513

  * Feature: Improved addon builds with node-waf
  * Feature: node.SignalHandler (Brandon Beacher)

  * Feature: Enable V8 debugging (but still need to make a debugger)
  * API: Rename library /utils.js to /sys.js

  * Clean up Node's build system
  * Don't use parseUri for HTTP server

  * Remove node.pc
  * Don't use /bin/sh to create child process except with exec()

  * API: Add __module to reference current module
  * API: Remove include() add node.mixin()

  * Normalize http headers; "Content-Length" becomes "content-length"
  * Upgrade V8 to 1.3.15

## 2009.09.30, Version 0.1.13

https://github.com/nodejs/node/commit/58493bb05b3da3dc8051fabc0bdea9e575c1a107

  * Feature: Multipart stream parser (Felix Geisendörfer)
  * API: Move node.puts(), node.exec() and others to /utils.js

  * API: Move http, tcp libraries to /http.js and /tcp.js
  * API: Rename node.exit() to process.exit()

  * Bugfix: require() and include() should work in callbacks.
  * Pass the Host header in http.cat calls

  * Add warning when coroutine stack size grows too large.
  * Enhance repl library (Ray Morgan)

  * Bugfix: build script for
      GCC 4.4 (removed -Werror in V8),
      on Linux 2.4,
      and with Python 2.4.4.

  * Add read() and write() to /file.js to read and write
    whole files at once.

## 2009.09.24, Version 0.1.12

https://github.com/nodejs/node/commit/2f56ccb45e87510de712f56705598b3b4e3548ec

  * Feature: System modules, node.libraryPaths
  * API: Remove "raw" encoding, rename "raws" to "binary".

  * API: Added connection.setNoDElay() to disable Nagle algo.
  * Decrease default TCP server backlog to 128

  * Bugfix: memory leak involving node.fs.* methods.
  * Upgrade v8 to 1.3.13

## 2009.09.18, Version 0.1.11

https://github.com/nodejs/node/commit/5ddc4f5d0c002bac0ae3d62fc0dc58f0d2d83ec4

  * API: default to utf8 encoding for node.fs.cat()
  * API: add node.exec()

  * API: node.fs.read() takes a normal encoding parameter.
  * API: Change arguments of emit(), emitSuccess(), emitError()

  * Bugfix: node.fs.write() was stack allocating buffer.
  * Bugfix: ReportException shouldn't forget the top frame.

  * Improve buffering for HTTP outgoing messages
  * Fix and reenable x64 macintosh build.

  * Upgrade v8 to 1.3.11

## 2009.09.11, Version 0.1.10

https://github.com/nodejs/node/commit/12bb0d46ce761e3d00a27170e63b40408c15b558

  * Feature: raw string encoding "raws"
  * Feature: access to environ through "ENV"

  * Feature: add isDirectory, isFile, isSocket, ... methods
    to stats object.

  * Bugfix: Internally use full paths when loading modules
    this fixes a shebang loading problem.

  * Bugfix: Add '--' command line argument for separating v8
    args from program args.

  * Add man page.
  * Add node-repl

  * Upgrade v8 to 1.3.10

## 2009.09.05, Version 0.1.9

https://github.com/nodejs/node/commit/d029764bb32058389ecb31ed54a5d24d2915ad4c

  * Bugfix: Compile on Snow Leopard.
  * Bugfix: Malformed URIs raising exceptions.
## 2009.09.04, Version 0.1.8

https://github.com/nodejs/node/commit/e6d712a937b61567e81b15085edba863be16ba96

  * Feature: External modules
  * Feature: setTimeout() for node.tcp.Connection

  * Feature: add node.cwd(), node.fs.readdir(), node.fs.mkdir()
  * Bugfix: promise.wait() releasing out of order.

  * Bugfix: Asyncly do getaddrinfo() on Apple.
  * Disable useless evcom error messages.

  * Better stack traces.
  * Built natively on x64.

  * Upgrade v8 to 1.3.9
## 2009.08.27, Version 0.1.7

https://github.com/nodejs/node/commit/f7acef9acf8ba8433d697ad5ed99d2e857387e4b

  * Feature: global 'process' object. Emits "exit".
  * Feature: promise.wait()

  * Feature: node.stdio
  * Feature: EventEmitters emit "newListener" when listeners are
    added

  * API:  Use flat object instead of array-of-arrays for HTTP
    headers.

  * API: Remove buffered file object (node.File)
  * API: require(), include() are synchronous. (Uses
    continuations.)

  * API: Deprecate onLoad and onExit.
  * API: Rename node.Process to node.ChildProcess

  * Refactor node.Process to take advantage of evcom_reader/writer.
  * Upgrade v8 to 1.3.7
## 2009.08.22, Version 0.1.6

https://github.com/nodejs/node/commit/9c97b1db3099d61cd292aa59ec2227a619f3a7ab

  * Bugfix: Ignore SIGPIPE.
## 2009.08.21, Version 0.1.5

https://github.com/nodejs/node/commit/b0fd3e281cb5f7cd8d3a26bd2b89e1b59998e5ed

  * Bugfix: Buggy connections could crash node.js. Now check
    connection before sending data every time (Kevin van Zonneveld)

  * Bugfix: stdin fd (0) being ignored by node.File. (Abe Fettig)
  * API: Remove connection.fullClose()

  * API: Return the EventEmitter from addListener for chaining.
  * API: tcp.Connection "disconnect" event renamed to "close"

  * Upgrade evcom
    Upgrade v8 to 1.3.6
## 2009.08.13, Version 0.1.4

https://github.com/nodejs/node/commit/0f888ed6de153f68c17005211d7e0f960a5e34f3

  * Major refactor to evcom.
  * Enable test-tcp-many-clients.

  * Add -m32 gcc flag to udns.
  * Add connection.readPause() and connection.readResume()
    Add IncomingMessage.prototype.pause() and resume().

  * Fix http benchmark. Wasn't correctly dispatching.
  * Bugfix: response.setBodyEncoding("ascii") not working.

  * Bugfix: Negative ints in HTTP's on_body and node.fs.read()
  * Upgrade v8 to 1.3.4
    Upgrade libev to 3.8
    Upgrade http_parser to v0.2
## 2009.08.06, Version 0.1.3

https://github.com/nodejs/node/commit/695f0296e35b30cf8322fd1bd934810403cca9f3

  * Upgrade v8 to 1.3.2
  * Bugfix: node.http.ServerRequest.setBodyEncoding('ascii') not
    working

  * Bugfix: node.encodeUtf8 was broken. (Connor Dunn)
  * Add ranlib to udns Makefile.

  * Upgrade evcom - fix accepting too many connections issue.
  * Initial support for shebang

  * Add simple command line switches
  * Add node.version API

## 2009.08.01, Version 0.1.2

https://github.com/nodejs/node/commit/025a34244d1cea94d6d40ad7bf92671cb909a96c

  * Add DNS API
  * node.tcp.Server's backlog option is now an argument to listen()

  * Upgrade V8 to 1.3.1
  * Bugfix: Default to chunked for client requests without
    Content-Length.

  * Bugfix: Line numbers in stack traces.
  * Bugfix: negative integers in raw encoding stream

  * Bugfix: node.fs.File was not passing args to promise callbacks.

## 2009.07.27, Version 0.1.1

https://github.com/nodejs/node/commit/77d407df2826b20e9177c26c0d2bb4481e497937

  * Simplify and clean up ObjectWrap.
  * Upgrade liboi (which is now called evcom)
    Upgrade libev to 3.7
    Upgrade V8 to 1.2.14

  * Array.prototype.encodeUtf8 renamed to node.encodeUtf8(array)
  * Move EventEmitter.prototype.emit() completely into C++.

  * Bugfix: Fix memory leak in event emitters.
    http://groups.google.com/group/nodejs/browse_thread/thread/a8d1dfc2fd57a6d1

  * Bugfix: Had problems reading scripts with non-ascii characters.
  * Bugfix: Fix Detach() in node::Server

  * Bugfix: Sockets not properly reattached if reconnected during
    disconnect event.

  * Bugfix: Server-side clients not attached between creation and
    on_connect.

  * Add 'close' event to node.tcp.Server
  * Simplify and clean up http.js. (Takes more advantage of event
    infrastructure.)

  * Add benchmark scripts. Run with "make benchmark".

## 2009.06.30, Version 0.1.0

https://github.com/nodejs/node/commit/0fe44d52fe75f151bceb59534394658aae6ac328

  * Update documentation, use asciidoc.
  * EventEmitter and Promise interfaces. (Breaks previous API.)

  * Remove node.Process constructor in favor of node.createProcess
  * Add -m32 flags for compiling on x64 platforms.
    (Thanks to András Bártházi)

  * Upgrade v8 to 1.2.10 and libev to 3.6
  * Bugfix: Timer::RepeatSetter wasn't working.

  * Bugfix: Spawning many processes in a loop
    (reported by Felix Geisendörfer)

## 2009.06.24, Version 0.0.6

https://github.com/nodejs/node/commit/fbe0be19ebfb422d8fa20ea5204c1713e9214d5f

  * Load modules via HTTP URLs (Urban Hafner)
  * Bugfix: Add HTTPConnection->size() and HTTPServer->size()

  * New node.Process API
  * Clean up build tools, use v8's test runner.

  * Use ev_unref() instead of starting/stopping the eio thread
    pool watcher.

## 2009.06.18, Version 0.0.5

https://github.com/nodejs/node/commit/3a2b41de74b6c343b8464a68eff04c4bfd9aebea

  * Support for IPv6
  * Remove namespace node.constants

  * Upgrade v8 to 1.2.8.1
  * Accept ports as strings in the TCP client and server.

  * Bugfix: HTTP Client race
  * Bugfix: freeaddrinfo() wasn't getting called after
    getaddrinfo() for TCP servers

  * Add "opening" to TCP client readyState
  * Add remoteAddress to TCP client

  * Add global print() function.

## 2009.06.13, Version 0.0.4

https://github.com/nodejs/node/commit/916b9ca715b229b0703f0ed6c2fc065410fb189c

 * Add interrupt() method to server-side HTTP requests.
 * Bugfix: onBodyComplete was not getting called on server-side
   HTTP

## 2009.06.11, Version 0.0.3

https://github.com/nodejs/node/commit/6e0dfe50006ae4f5dac987f055e0c9338662f40a

 * Many bug fixes including the problem with http.Client on
   macintosh

 * Upgrades v8 to 1.2.7
 * Adds onExit hook

 * Guard against buffer overflow in http parser
 * require() and include() now need the ".js" extension

 * http.Client uses identity transfer encoding by default.
