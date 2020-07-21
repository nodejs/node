# Change Log

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

<a name="10.2.4"></a>
## [10.2.4](https://github.com/npm/npx/compare/v10.2.3...v10.2.4) (2020-07-20)



<a name="10.2.3"></a>
## [10.2.3](https://github.com/npm/npx/compare/v10.2.2...v10.2.3) (2020-03-24)



<a name="10.2.2"></a>
## [10.2.2](https://github.com/npm/npx/compare/v10.2.1...v10.2.2) (2020-01-28)


### Bug Fixes

* correct Kat's github url ([9a23db1](https://github.com/npm/npx/commit/9a23db1))
* install latest npm on travis for node 6 ([e0eb3cb](https://github.com/npm/npx/commit/e0eb3cb))
* Update changelog to fix old issue links ([3733137](https://github.com/npm/npx/commit/3733137))



<a name="10.2.0"></a>
# [10.2.0](https://github.com/npm/npx/compare/v10.1.1...v10.2.0) (2018-04-13)


### Bug Fixes

* **i18n:** fix korean; 쉘 -> 셸 ([#163](https://github.com/zkat/npx/issues/163)) ([11d9fe0](https://github.com/npm/npx/commit/11d9fe0))
* **spawn:** spawn child processes with node without relying on the shebang. ([#174](https://github.com/zkat/npx/issues/174)) ([cba97bb](https://github.com/npm/npx/commit/cba97bb))
* **windows:** Allow spaces in the node path when using --node-arg ([#173](https://github.com/zkat/npx/issues/173)) ([fe0d48a](https://github.com/npm/npx/commit/fe0d48a)), closes [#170](https://github.com/zkat/npx/issues/170)


### Features

* **i18n:** add translation ([#159](https://github.com/zkat/npx/issues/159)) ([5da008b](https://github.com/npm/npx/commit/5da008b))



<a name="10.1.1"></a>
## [10.1.1](https://github.com/npm/npx/compare/v10.1.0...v10.1.1) (2018-04-12)



<a name="10.1.0"></a>
# [10.1.0](https://github.com/npm/npx/compare/v10.0.1...v10.1.0) (2018-04-12)


### Features

* **spawn:** add --always-spawn to opt out of process takeover optimization feature ([#172](https://github.com/zkat/npx/issues/172)) ([c0d6abc](https://github.com/npm/npx/commit/c0d6abc))



<a name="10.0.1"></a>
## [10.0.1](https://github.com/npm/npx/compare/v10.0.0...v10.0.1) (2018-03-08)


### Bug Fixes

* **i18n:** Improve French localization ([#158](https://github.com/zkat/npx/issues/158)) ([c88823e](https://github.com/npm/npx/commit/c88823e))
* **windows:** on Windows, throw useful error when package contains no binaries([#142](https://github.com/zkat/npx/issues/142)) ([a69276e](https://github.com/npm/npx/commit/a69276e)), closes [#137](https://github.com/zkat/npx/issues/137)



<a name="10.0.0"></a>
# [10.0.0](https://github.com/npm/npx/compare/v9.7.1...v10.0.0) (2018-03-08)


### Bug Fixes

* **i18n:** Fix Korean locale ([#130](https://github.com/zkat/npx/issues/130)) ([752db48](https://github.com/npm/npx/commit/752db48))
* **index:** remove extraneous logging on Windows ([#136](https://github.com/zkat/npx/issues/136)) ([357e6ab](https://github.com/npm/npx/commit/357e6ab)), closes [#131](https://github.com/zkat/npx/issues/131)
* **license:** change npx license to ISC ([a617d7b](https://github.com/npm/npx/commit/a617d7b))
* **parse-args:** fix version thing for yargs ([30677ed](https://github.com/npm/npx/commit/30677ed))
* **prefix:** Handle node_modules without package.json ([#128](https://github.com/zkat/npx/issues/128)) ([f64ae43](https://github.com/npm/npx/commit/f64ae43)), closes [/github.com/babel/babel/issues/4066#issuecomment-336705199](https://github.com//github.com/babel/babel/issues/4066/issues/issuecomment-336705199)
* **standard:** get things in line with standard 11 ([6cf8e88](https://github.com/npm/npx/commit/6cf8e88))


### BREAKING CHANGES

* **license:** This moves the code over from CC0-1.0 to the ISC license.



<a name="9.7.1"></a>
## [9.7.1](https://github.com/npm/npx/compare/v9.7.0...v9.7.1) (2017-10-19)


### Bug Fixes

* **main:** err... oops? ([f24b4e3](https://github.com/npm/npx/commit/f24b4e3))



<a name="9.7.0"></a>
# [9.7.0](https://github.com/npm/npx/compare/v9.6.0...v9.7.0) (2017-10-19)


### Bug Fixes

* **exec:** fixed unix binary pathing issues (#120) ([f80a970](https://github.com/npm/npx/commit/f80a970)), closes [#120](https://github.com/zkat/npx/issues/120)


### Features

* **child:** add opts.installerStdio (#126) ([ade03f7](https://github.com/npm/npx/commit/ade03f7))



<a name="9.6.0"></a>
# [9.6.0](https://github.com/npm/npx/compare/v9.5.0...v9.6.0) (2017-08-17)


### Features

* **i18n:** add Arabic translation (#111) ([3c5b99a](https://github.com/npm/npx/commit/3c5b99a))
* **i18n:** add Dutch (#108) ([ed116fd](https://github.com/npm/npx/commit/ed116fd))



<a name="9.5.0"></a>
# [9.5.0](https://github.com/npm/npx/compare/v9.4.1...v9.5.0) (2017-07-28)


### Features

* **i18n:** add Polish translations (#99) ([8442f59](https://github.com/npm/npx/commit/8442f59))



<a name="9.4.1"></a>
## [9.4.1](https://github.com/npm/npx/compare/v9.4.0...v9.4.1) (2017-07-21)


### Bug Fixes

* **i18n:** fix filename for uk.json locale ([2c770e4](https://github.com/npm/npx/commit/2c770e4))



<a name="9.4.0"></a>
# [9.4.0](https://github.com/npm/npx/compare/v9.3.2...v9.4.0) (2017-07-21)


### Bug Fixes

* **i18n:** minor fixes to ru locale (#92) ([f4d5051](https://github.com/npm/npx/commit/f4d5051)), closes [#92](https://github.com/zkat/npx/issues/92)


### Features

* **i18n:** `no` locale fallback for Norwegian bokmål ⚠️ In case of weird setups ⚠️ (#91) ([74f0e4c](https://github.com/npm/npx/commit/74f0e4c))
* **i18n:** add Bahasa Indonesia locale (#95) ([80dceeb](https://github.com/npm/npx/commit/80dceeb))
* **i18n:** add serbian translation (#96) ([040de7a](https://github.com/npm/npx/commit/040de7a))
* **i18n:** add Ukrainian locale (#93) ([9a3ef33](https://github.com/npm/npx/commit/9a3ef33))
* **i18n:** Added Norwegian (bokmål and nynorsk) translations (#90) ([6c5c733](https://github.com/npm/npx/commit/6c5c733))



<a name="9.3.2"></a>
## [9.3.2](https://github.com/npm/npx/compare/v9.3.1...v9.3.2) (2017-07-17)


### Bug Fixes

* **exec:** detect a wider range of shebang lines for node scripts (#89) ([1841b6f](https://github.com/npm/npx/commit/1841b6f))
* **windows:** escape spawn args because windows is picky (#87) ([314e5eb](https://github.com/npm/npx/commit/314e5eb))
* **windows:** get magic shim detection working on Windows (#88) ([255aeeb](https://github.com/npm/npx/commit/255aeeb))



<a name="9.3.1"></a>
## [9.3.1](https://github.com/npm/npx/compare/v9.3.0...v9.3.1) (2017-07-17)


### Bug Fixes

* **deps:** update to npm[@5](https://github.com/5).3.0 ([2b14de2](https://github.com/npm/npx/commit/2b14de2))



<a name="9.3.0"></a>
# [9.3.0](https://github.com/npm/npx/compare/v9.2.3...v9.3.0) (2017-07-17)


### Features

* **i18n:** add Korean locale (#86) ([3655314](https://github.com/npm/npx/commit/3655314))



<a name="9.2.3"></a>
## [9.2.3](https://github.com/npm/npx/compare/v9.2.2...v9.2.3) (2017-07-17)


### Bug Fixes

* **paths:** support npm/npx paths with spaces in them ([8f3b829](https://github.com/npm/npx/commit/8f3b829))



<a name="9.2.2"></a>
## [9.2.2](https://github.com/npm/npx/compare/v9.2.1...v9.2.2) (2017-07-15)


### Bug Fixes

* **npm:** escape path to npm, too ([333d2ff](https://github.com/npm/npx/commit/333d2ff))



<a name="9.2.1"></a>
## [9.2.1](https://github.com/npm/npx/compare/v9.2.0...v9.2.1) (2017-07-14)


### Bug Fixes

* **windows:** fixed windows binary pathing issues ([761dfe9](https://github.com/npm/npx/commit/761dfe9))



<a name="9.2.0"></a>
# [9.2.0](https://github.com/npm/npx/compare/v9.1.0...v9.2.0) (2017-07-14)


### Bug Fixes

* **binpath:** fix calling binaries from subdirectories ([f185d0d](https://github.com/npm/npx/commit/f185d0d))
* **i18n:** Fix typos in french locale (#78) ([f277fc7](https://github.com/npm/npx/commit/f277fc7)), closes [#78](https://github.com/zkat/npx/issues/78)


### Features

* **i18n:** Add German translations (#79) ([c81e26d](https://github.com/npm/npx/commit/c81e26d))
* **i18n:** add zh_TW translation (#80) ([98288d8](https://github.com/npm/npx/commit/98288d8))



<a name="9.1.0"></a>
# [9.1.0](https://github.com/npm/npx/compare/v9.0.7...v9.1.0) (2017-07-12)


### Bug Fixes

* **call:** only npm run env if package.json exists ([370f395](https://github.com/npm/npx/commit/370f395))
* **i18n:** Fix grammar and spelling for de.json (#63) ([b14020f](https://github.com/npm/npx/commit/b14020f)), closes [#63](https://github.com/zkat/npx/issues/63)
* **i18n:** wording revisions for Brazilian Portuguese (#75) ([b5dc536](https://github.com/npm/npx/commit/b5dc536))
* **npm:** path directly to the npm-cli.js script ([d531206](https://github.com/npm/npx/commit/d531206))
* **rimraf:** fix rimraf.sync is not a function issue ([d2ecba3](https://github.com/npm/npx/commit/d2ecba3))
* **windows:** get npx working well on Windows again (#69) ([6cfb8de](https://github.com/npm/npx/commit/6cfb8de)), closes [#60](https://github.com/zkat/npx/issues/60) [#58](https://github.com/zkat/npx/issues/58) [#62](https://github.com/zkat/npx/issues/62)


### Features

* **i18n:** add Czech translation (#76) ([8a0b3f6](https://github.com/npm/npx/commit/8a0b3f6))
* **i18n:** Add Turkish translation (#73) ([26e5edf](https://github.com/npm/npx/commit/26e5edf))
* **i18n:** Added support for Italian language (#71) ([6883e75](https://github.com/npm/npx/commit/6883e75))
* **i18n:** Fix Romanian translation (#70) ([fd6bbcf](https://github.com/npm/npx/commit/fd6bbcf)), closes [#70](https://github.com/zkat/npx/issues/70)
* **node:** add --node-arg support to pass flags to node for script binaries (#77) ([65665bd](https://github.com/npm/npx/commit/65665bd))



<a name="9.0.7"></a>
## [9.0.7](https://github.com/npm/npx/compare/v9.0.6...v9.0.7) (2017-07-11)


### Bug Fixes

* **i18n:** Fix some Catalan translations (#59) ([11c8a19](https://github.com/npm/npx/commit/11c8a19)), closes [#59](https://github.com/zkat/npx/issues/59)



<a name="9.0.6"></a>
## [9.0.6](https://github.com/npm/npx/compare/v9.0.5...v9.0.6) (2017-07-11)


### Bug Fixes

* **auto-fallback:** fix syntax error in bash/zsh auto-fallback ([d8b19db](https://github.com/npm/npx/commit/d8b19db))



<a name="9.0.5"></a>
## [9.0.5](https://github.com/npm/npx/compare/v9.0.4...v9.0.5) (2017-07-11)


### Bug Fixes

* **npx:** something went wrong with the 9.0.4 build and bundledeps ([75fc436](https://github.com/npm/npx/commit/75fc436))



<a name="9.0.4"></a>
## [9.0.4](https://github.com/npm/npx/compare/v9.0.3...v9.0.4) (2017-07-11)


### Bug Fixes

* **auto-fallback:** prevent infinite loop if npx disappears ([6c24e58](https://github.com/npm/npx/commit/6c24e58))
* **bin:** add repository and more detailed author info ([906574e](https://github.com/npm/npx/commit/906574e))
* **bin:** pin the npx bin's dependencies ([ae62f7a](https://github.com/npm/npx/commit/ae62f7a))
* **build:** make sure changelog and license are copied to bin ([4fbb599](https://github.com/npm/npx/commit/4fbb599))
* **deps:** stop bundling deps in libnpx itself ([c3e56e9](https://github.com/npm/npx/commit/c3e56e9))
* **errors:** print command not found for packages without valid binaries ([9b24359](https://github.com/npm/npx/commit/9b24359))
* **help:** --no-install help text was contradicting itself ([9d96f5e](https://github.com/npm/npx/commit/9d96f5e))
* **install:** prevent concurrent npx runs from clobbering each other ([6b35c91](https://github.com/npm/npx/commit/6b35c91))
* **npx:** npx npx npx npx npx npx npx npx npx works again ([875d4cd](https://github.com/npm/npx/commit/875d4cd))
* **updater:** dependency injection for update-notifier target ([c3027a9](https://github.com/npm/npx/commit/c3027a9))
* **updater:** ignore some kinds of update-notifier errors ([7631bbe](https://github.com/npm/npx/commit/7631bbe))



<a name="9.0.3"></a>
## [9.0.3](https://github.com/npm/npx/compare/v9.0.2...v9.0.3) (2017-07-08)


### Bug Fixes

* **version:** hand version to yargs directly ([e0b5eeb](https://github.com/npm/npx/commit/e0b5eeb))



<a name="9.0.2"></a>
## [9.0.2](https://github.com/npm/npx/compare/v9.0.1...v9.0.2) (2017-07-08)


### Bug Fixes

* **manpage:** fix manpage for real because files syntax is weird ([9145e2a](https://github.com/npm/npx/commit/9145e2a))



<a name="9.0.1"></a>
## [9.0.1](https://github.com/npm/npx/compare/v9.0.0...v9.0.1) (2017-07-08)


### Bug Fixes

* **man:** make sure manpage is used in npx bin ([704b94f](https://github.com/npm/npx/commit/704b94f))



<a name="9.0.0"></a>
# [9.0.0](https://github.com/npm/npx/compare/v8.1.1...v9.0.0) (2017-07-08)


### Features

* **libnpx:** libify main npx codebase ([643f58e](https://github.com/npm/npx/commit/643f58e))
* **npx:** create a new binary for standalone publishing ([da5a3b7](https://github.com/npm/npx/commit/da5a3b7))


### BREAKING CHANGES

* **libnpx:** This version of npx can no longer be used as a
standalone binary. It will be available on the registry as `libnpx`,
and a separate project will take over the role of the main `npx` binary.



<a name="8.1.1"></a>
## [8.1.1](https://github.com/npm/npx/compare/v8.1.0...v8.1.1) (2017-07-06)


### Bug Fixes

* **deps:** bump all deps ([6ea24bf](https://github.com/npm/npx/commit/6ea24bf))
* **npm:** bump npm to 5.1.0 for a bunch of fixes ([18e4587](https://github.com/npm/npx/commit/18e4587))



<a name="8.1.0"></a>
# [8.1.0](https://github.com/npm/npx/compare/v8.0.1...v8.1.0) (2017-06-27)


### Bug Fixes

* **i18n:** minor tweaks to ja.json (#46) ([1ed63c2](https://github.com/npm/npx/commit/1ed63c2))


### Features

* **i18n:** Update pt_BR.json (#51) ([d292f22](https://github.com/npm/npx/commit/d292f22))



<a name="8.0.1"></a>
## [8.0.1](https://github.com/npm/npx/compare/v8.0.0...v8.0.1) (2017-06-27)


### Bug Fixes

* **npm:** bump npm version for more bugfixes ([30711a8](https://github.com/npm/npx/commit/30711a8))
* **npm:** Use --parseable option to work around output quirks ([8cb75a2](https://github.com/npm/npx/commit/8cb75a2))



<a name="8.0.0"></a>
# [8.0.0](https://github.com/npm/npx/compare/v7.0.0...v8.0.0) (2017-06-24)


### Features

* **exec:** auto-guess binaries when different from pkg name ([139c434](https://github.com/npm/npx/commit/139c434))


### BREAKING CHANGES

* **exec:** `npx ember-cli` and such things will now execute the
binary based on some guesswork, but only when using the shorthand format
for npx execution, with no `-p` option or `-c`. This might cause npx to
unintentionally execute the wrong binary if the package in question has
multiple non-matching binaries, but that should be rare.



<a name="7.0.0"></a>
# [7.0.0](https://github.com/npm/npx/compare/v6.2.0...v7.0.0) (2017-06-24)


### Bug Fixes

* **win32:** improve win32 situation a bit (#50) ([b7ad934](https://github.com/npm/npx/commit/b7ad934))


### Features

* **local:** improve the behavior when calling ./local paths (#48) ([2e418d1](https://github.com/npm/npx/commit/2e418d1))


### BREAKING CHANGES

* **local:** `npx ./something` will now execute `./something` as a
binary or script instead of trying to install it as npm would. Other behavior
related to local path deps has likewise been changed. See
[#49](https://github.com/zkat/npx/issues/49) for a detailed explanation
of all the various cases and how each of them is handled.



<a name="6.2.0"></a>
# [6.2.0](https://github.com/npm/npx/compare/v6.1.0...v6.2.0) (2017-06-23)


### Bug Fixes

* **child:** iron out a few crinkles and add tests ([b3b5ef6](https://github.com/npm/npx/commit/b3b5ef6))
* **execCmd:** only reuse the current process if no shell passed in ([e413cff](https://github.com/npm/npx/commit/e413cff))
* **execCmd:** use the module built-in directly ([6f741c2](https://github.com/npm/npx/commit/6f741c2))
* **help:** fuck it. just hard-code it ([d5d5085](https://github.com/npm/npx/commit/d5d5085))
* **main:** only exec if this is the main module ([9631e2a](https://github.com/npm/npx/commit/9631e2a))


### Features

* **i18n:** Update fr.json (#44) ([ea47c4f](https://github.com/npm/npx/commit/ea47c4f))
* **i18n:** update the Romanian translation. (#42) ([2ed36b6](https://github.com/npm/npx/commit/2ed36b6))



<a name="6.1.0"></a>
# [6.1.0](https://github.com/npm/npx/compare/v6.0.0...v6.1.0) (2017-06-21)


### Bug Fixes

* **deps:** remove unused gauge dep ([aa40a34](https://github.com/npm/npx/commit/aa40a34))


### Features

* **i18n:** update ru locale (#41) ([7c84dee](https://github.com/npm/npx/commit/7c84dee))
* **i18n:** update zh_CN (#40) ([da4ec67](https://github.com/npm/npx/commit/da4ec67))
* **perf:** run node-based commands in the current process ([6efcde4](https://github.com/npm/npx/commit/6efcde4))



<a name="6.0.0"></a>
# [6.0.0](https://github.com/npm/npx/compare/v5.4.0...v6.0.0) (2017-06-20)


### Bug Fixes

* **call:** stop parsing -c for commands + fix corner cases ([bd4e538](https://github.com/npm/npx/commit/bd4e538))
* **child:** exec does not have the information needed to correctly escape its args ([6714992](https://github.com/npm/npx/commit/6714992))
* **guessCmdName:** tests failed because of lazy npa ([53a0119](https://github.com/npm/npx/commit/53a0119))
* **i18n:** gender inclusiveness fix for french version (#37) ([04920ae](https://github.com/npm/npx/commit/04920ae)), closes [#37](https://github.com/zkat/npx/issues/37)
* **i18n:** typo 😇 (#38) ([ede4a53](https://github.com/npm/npx/commit/ede4a53))
* **install:** handle JSON parsing failures ([bec2887](https://github.com/npm/npx/commit/bec2887))
* **output:** stop printing out Command Failed messages ([873cffe](https://github.com/npm/npx/commit/873cffe))
* **parseArgs:** fix booboo in fast path ([d1e5487](https://github.com/npm/npx/commit/d1e5487))
* **perf:** fast-path `npx foo` arg parsing ([ba4fe71](https://github.com/npm/npx/commit/ba4fe71))
* **perf:** remove bluebird and defer some requires for SPEED ([00fc313](https://github.com/npm/npx/commit/00fc313))


### Features

* **i18n:** add Romanian translations. (#34) ([9e98bd0](https://github.com/npm/npx/commit/9e98bd0))
* **i18n:** added a few more localizable strings ([779d950](https://github.com/npm/npx/commit/779d950))
* **i18n:** updated ca.json ([af7a035](https://github.com/npm/npx/commit/af7a035))
* **i18n:** updated es.json ([414644f](https://github.com/npm/npx/commit/414644f))
* **i18n:** updated ja.json ([448b082](https://github.com/npm/npx/commit/448b082))
* **i18n:** Ze German Translation (#35) ([6f003f5](https://github.com/npm/npx/commit/6f003f5))
* **package:** report number of temp packages installed ([5b7fe8d](https://github.com/npm/npx/commit/5b7fe8d))
* **perf:** only launch update-notifier when npx installs stuff ([549d413](https://github.com/npm/npx/commit/549d413))
* **quiet:** added -q/--quiet to suppress output from npx itself ([16607d9](https://github.com/npm/npx/commit/16607d9))


### BREAKING CHANGES

* **call:** `npx -c "foo"` will no longer install `foo`. Use `-p` to specicify packages to install. npx will no longer assume any particular format or escape status for `-c` strings: they will be passed directly, unparsed, and unaltered, to child_process.spawn.



<a name="5.4.0"></a>
# [5.4.0](https://github.com/npm/npx/compare/v5.3.0...v5.4.0) (2017-06-17)


### Bug Fixes

* **i18n:** some corrections for es.json ([4d50b71](https://github.com/npm/npx/commit/4d50b71))
* **i18n:** update locale files with bugfixes ([77caf82](https://github.com/npm/npx/commit/77caf82))
* **i18n:** Y utility was ignoring falsy entries ([f22a4d0](https://github.com/npm/npx/commit/f22a4d0))
* **i18n:** してください -> します ([01671af](https://github.com/npm/npx/commit/01671af))


### Features

* **i18n:** add catalan translation ([579efa1](https://github.com/npm/npx/commit/579efa1))
* **i18n:** add pt-br translation (#33) ([6142551](https://github.com/npm/npx/commit/6142551))
* **i18n:** added largely machine-translated ja.json ([827705f](https://github.com/npm/npx/commit/827705f))
* **i18n:** adds russian translation (#32) ([b2619c1](https://github.com/npm/npx/commit/b2619c1))



<a name="5.3.0"></a>
# [5.3.0](https://github.com/npm/npx/compare/v5.2.0...v5.3.0) (2017-06-13)


### Features

* **i18n:** add Chinese translation (#31) ([24e1b31](https://github.com/npm/npx/commit/24e1b31))



<a name="5.2.0"></a>
# [5.2.0](https://github.com/npm/npx/compare/v5.1.3...v5.2.0) (2017-06-12)


### Bug Fixes

* **i18n:** removing extra spacing in fr.json ([002e2b8](https://github.com/npm/npx/commit/002e2b8))


### Features

* **i18n:** add french locale (#29) ([662395b](https://github.com/npm/npx/commit/662395b))



<a name="5.1.3"></a>
## [5.1.3](https://github.com/npm/npx/compare/v5.1.2...v5.1.3) (2017-06-12)


### Bug Fixes

* **fallback:** put the Y in the wrong place lol ([d6bf8aa](https://github.com/npm/npx/commit/d6bf8aa))



<a name="5.1.2"></a>
## [5.1.2](https://github.com/npm/npx/compare/v5.1.1...v5.1.2) (2017-06-10)



<a name="5.1.1"></a>
## [5.1.1](https://github.com/npm/npx/compare/v5.1.0...v5.1.1) (2017-06-10)


### Bug Fixes

* **i18n:** forgot to add locales to files ([4118d6a](https://github.com/npm/npx/commit/4118d6a))



<a name="5.1.0"></a>
# [5.1.0](https://github.com/npm/npx/compare/v5.0.3...v5.1.0) (2017-06-10)


### Bug Fixes

* **exit:** let process exit normally to finish writes ([c50a398](https://github.com/npm/npx/commit/c50a398))


### Features

* **i18n:** added es.json ([6cf58b9](https://github.com/npm/npx/commit/6cf58b9))
* **i18n:** set up i18n plus baseline en.json locale ([b67bb3a](https://github.com/npm/npx/commit/b67bb3a))



<a name="5.0.3"></a>
## [5.0.3](https://github.com/npm/npx/compare/v5.0.2...v5.0.3) (2017-06-09)


### Bug Fixes

* **fallback:** exec is no ([42c1d30](https://github.com/npm/npx/commit/42c1d30))



<a name="5.0.2"></a>
## [5.0.2](https://github.com/npm/npx/compare/v5.0.1...v5.0.2) (2017-06-09)


### Bug Fixes

* **fallback:** allow fallback to local anyway ([569cf2c](https://github.com/npm/npx/commit/569cf2c))



<a name="5.0.1"></a>
## [5.0.1](https://github.com/npm/npx/compare/v5.0.0...v5.0.1) (2017-06-09)



<a name="5.0.0"></a>
# [5.0.0](https://github.com/npm/npx/compare/v4.0.3...v5.0.0) (2017-06-09)


### Features

* **fallback:** by default, only fall back if you have an @ in the name ([bea08a0](https://github.com/npm/npx/commit/bea08a0))


### BREAKING CHANGES

* **fallback:** auto-fallback will no longer fall back unless there was
an @ sign in the command.



<a name="4.0.3"></a>
## [4.0.3](https://github.com/npm/npx/compare/v4.0.2...v4.0.3) (2017-06-04)


### Bug Fixes

* **npm:** use --userconfig when querying for npm cache config (#28) ([21bc3bf](https://github.com/npm/npx/commit/21bc3bf))



<a name="4.0.2"></a>
## [4.0.2](https://github.com/npm/npx/compare/v4.0.1...v4.0.2) (2017-06-04)


### Bug Fixes

* **install:** get windows workin (#27) ([9472175](https://github.com/npm/npx/commit/9472175))



<a name="4.0.1"></a>
## [4.0.1](https://github.com/npm/npx/compare/v4.0.0...v4.0.1) (2017-06-04)


### Bug Fixes

* **cmd:** make sure to use our own, enriched path ([9c89c2a](https://github.com/npm/npx/commit/9c89c2a))
* **error:** join args with a space on Command failed error ([c2f6f18](https://github.com/npm/npx/commit/c2f6f18))



<a name="4.0.0"></a>
# [4.0.0](https://github.com/npm/npx/compare/v3.0.0...v4.0.0) (2017-06-03)


### Features

* **call:** -c now loads same env as run-script ([76ae44c](https://github.com/npm/npx/commit/76ae44c))
* **npm:** allow configuration of npm binary ([e5d5634](https://github.com/npm/npx/commit/e5d5634))
* **npm:** embed npm binary ([a2cae9d](https://github.com/npm/npx/commit/a2cae9d))


### BREAKING CHANGES

* **call:** scripts invoked with -c will now have a bunch of
variables added to them that were not there before.
* **npm:** npx will no longer use the system npm -- it embeds its own



<a name="3.0.0"></a>
# [3.0.0](https://github.com/npm/npx/compare/v2.1.0...v3.0.0) (2017-06-03)


### Bug Fixes

* **args:** accept argv as arg and fix minor bugs ([46f10fe](https://github.com/npm/npx/commit/46f10fe))
* **deps:** explicitly add mkdirp and rimraf to devDeps ([832c75d](https://github.com/npm/npx/commit/832c75d))
* **docs:** misc tweaks to docs ([ed70a7b](https://github.com/npm/npx/commit/ed70a7b))
* **exec:** escape binaries and args to cp.exec (#18) ([55d6a11](https://github.com/npm/npx/commit/55d6a11))
* **fallback:** shells were sometimes ignored based on $SHELL ([07b7efc](https://github.com/npm/npx/commit/07b7efc))
* **get-prefix:** nudge isRootPath ([1ab31eb](https://github.com/npm/npx/commit/1ab31eb))
* **help:** correctly enable -h and --help ([adc2f45](https://github.com/npm/npx/commit/adc2f45))
* **startup:** delay loading some things to speed up startup ([6b32bf5](https://github.com/npm/npx/commit/6b32bf5))


### Features

* **cmd:** do some heuristic guesswork on default command names (#23) ([2404420](https://github.com/npm/npx/commit/2404420))
* **ignore:** add --ignore-existing option (#20) ([0866a83](https://github.com/npm/npx/commit/0866a83))
* **install:** added --no-install option to prevent install fallbacks ([a5fbdaf](https://github.com/npm/npx/commit/a5fbdaf))
* **package:** multiple --package options are now accepted ([f2fa6b3](https://github.com/npm/npx/commit/f2fa6b3))
* **save:** remove all save-related functionality (#19) ([ab77f6c](https://github.com/npm/npx/commit/ab77f6c))
* **shell:** run -c strings inside a system shell (#22) ([17db461](https://github.com/npm/npx/commit/17db461))


### BREAKING CHANGES

* **save:** npx can no longer be used to save packages locally or globally. Use an actual package manager for that, instead.



<a name="2.1.0"></a>
# [2.1.0](https://github.com/npm/npx/compare/v2.0.1...v2.1.0) (2017-06-01)


### Features

* **opts:** add --shell-auto-fallback (#7) ([ac9cb40](https://github.com/npm/npx/commit/ac9cb40))



<a name="2.0.1"></a>
## [2.0.1](https://github.com/npm/npx/compare/v2.0.0...v2.0.1) (2017-05-31)


### Bug Fixes

* **exec:** use command lookup joined with current PATH ([d9175e8](https://github.com/npm/npx/commit/d9175e8))



<a name="2.0.0"></a>
# [2.0.0](https://github.com/npm/npx/compare/v1.1.1...v2.0.0) (2017-05-31)


### Bug Fixes

* **npm:** manually look up npm path for Windows compat ([0fe8fbf](https://github.com/npm/npx/commit/0fe8fbf))


### Features

* **commands:** -p and [@version](https://github.com/version) now trigger installs ([9668c83](https://github.com/npm/npx/commit/9668c83))


### BREAKING CHANGES

* **commands:** If a command has an explicit --package option, or if the command has an @version part, any version of the command in $PATH will be ignored and a regular install will be executed.



<a name="1.1.1"></a>
## [1.1.1](https://github.com/npm/npx/compare/v1.1.0...v1.1.1) (2017-05-30)


### Bug Fixes

* **docs:** make sure man page gets installed ([2aadc16](https://github.com/npm/npx/commit/2aadc16))



<a name="1.1.0"></a>
# [1.1.0](https://github.com/npm/npx/compare/v1.0.2...v1.1.0) (2017-05-30)


### Bug Fixes

* **help:** update usage string for help ([0747cff](https://github.com/npm/npx/commit/0747cff))
* **main:** exit if no package was parsed ([cdb579d](https://github.com/npm/npx/commit/cdb579d))
* **opts:** allow -- to prevent further parsing ([db7a0e4](https://github.com/npm/npx/commit/db7a0e4))


### Features

* **updates:** added update-notifier ([8dc91d4](https://github.com/npm/npx/commit/8dc91d4))



<a name="1.0.2"></a>
## [1.0.2](https://github.com/npm/npx/compare/v1.0.1...v1.0.2) (2017-05-30)


### Bug Fixes

* **pkg:** bundle deps to guarantee global install precision ([3e21217](https://github.com/npm/npx/commit/3e21217))



<a name="1.0.1"></a>
## [1.0.1](https://github.com/npm/npx/compare/v1.0.0...v1.0.1) (2017-05-30)


### Bug Fixes

* **build:** add dummy test file to let things build ([6199eb6](https://github.com/npm/npx/commit/6199eb6))
* **docs:** fix arg documentation in readme/manpage ([d1cf44c](https://github.com/npm/npx/commit/d1cf44c))
* **opts:** add --version/-v ([2633a0e](https://github.com/npm/npx/commit/2633a0e))



<a name="1.0.0"></a>
# 1.0.0 (2017-05-30)


### Features

* **npx:** initial working implementation ([a83a67d](https://github.com/npm/npx/commit/a83a67d))
