# Node.js 17 ChangeLog

<!--lint disable maximum-line-length no-literal-urls prohibited-strings-->

<table>
<tr>
<th>Current</th>
</tr>
<tr>
<td>
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
