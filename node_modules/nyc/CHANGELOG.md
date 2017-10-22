# Change Log

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

<a name="11.2.1"></a>
## [11.2.1](https://github.com/istanbuljs/nyc/compare/v11.2.0...v11.2.1) (2017-09-06)


### Bug Fixes

* apply exclude logic before remapping coverage ([#667](https://github.com/istanbuljs/nyc/issues/667)) ([a10d478](https://github.com/istanbuljs/nyc/commit/a10d478))
* create temp directory when --no-clean flag is set [#663](https://github.com/istanbuljs/nyc/issues/663) ([#664](https://github.com/istanbuljs/nyc/issues/664)) ([3bd1527](https://github.com/istanbuljs/nyc/commit/3bd1527))



<a name="11.2.0"></a>
# [11.2.0](https://github.com/istanbuljs/nyc/compare/v11.1.0...v11.2.0) (2017-09-05)


### Bug Fixes

* remove excluded files from coverage before writing ([#649](https://github.com/istanbuljs/nyc/issues/649)) ([658dba4](https://github.com/istanbuljs/nyc/commit/658dba4))


### Features

* add possibility to filter coverage-maps ([#637](https://github.com/istanbuljs/nyc/issues/637)) ([dd40dc5](https://github.com/istanbuljs/nyc/commit/dd40dc5))
* allow cwd to be configured see [#620](https://github.com/istanbuljs/nyc/issues/620) ([0dcceda](https://github.com/istanbuljs/nyc/commit/0dcceda))



<a name="11.1.0"></a>
# [11.1.0](https://github.com/istanbuljs/nyc/compare/v11.0.3...v11.1.0) (2017-07-16)


### Features

* add support for per file coverage checking ([#591](https://github.com/istanbuljs/nyc/issues/591)) ([bbadc1f](https://github.com/istanbuljs/nyc/commit/bbadc1f))
* upgrade to version of spawn-wrap that supports .EXE ([#626](https://github.com/istanbuljs/nyc/issues/626)) ([8768afe](https://github.com/istanbuljs/nyc/commit/8768afe))



<a name="11.0.3"></a>
## [11.0.3](https://github.com/istanbuljs/nyc/compare/v11.0.2...v11.0.3) (2017-06-25)


### Bug Fixes

* update help link to list of reporters ([#601](https://github.com/istanbuljs/nyc/issues/601)) ([b1eb4d6](https://github.com/istanbuljs/nyc/commit/b1eb4d6))
* upgrade to spawn-wrap version that works with babel-register ([#617](https://github.com/istanbuljs/nyc/issues/617)) ([923b062](https://github.com/istanbuljs/nyc/commit/923b062))



<a name="11.0.2"></a>
## [11.0.2](https://github.com/istanbuljs/nyc/compare/v11.0.1...v11.0.2) (2017-06-02)



<a name="11.0.1"></a>
## [11.0.1](https://github.com/istanbuljs/nyc/compare/v11.0.0...v11.0.1) (2017-06-01)



<a name="11.0.0"></a>
# [11.0.0](https://github.com/istanbuljs/nyc/compare/v10.3.2...v11.0.0) (2017-05-31)


### Bug Fixes

* add support for ES6 modules ([f18f780](https://github.com/istanbuljs/nyc/commit/f18f780))


### Features

* allow .nycrc.json ([#580](https://github.com/istanbuljs/nyc/issues/580)) ([a1a457f](https://github.com/istanbuljs/nyc/commit/a1a457f))
* upgrade to version of yargs with support for presets ([33829b8](https://github.com/istanbuljs/nyc/commit/33829b8))


### BREAKING CHANGES

* new version of find-up requires dropping 0.10/0.12 support (which we had already been planning).



<a name="10.3.2"></a>
## [10.3.2](https://github.com/istanbuljs/nyc/compare/v10.3.1...v10.3.2) (2017-05-05)


### Bug Fixes

* we should not create a cache folder if cache is false ([#567](https://github.com/istanbuljs/nyc/issues/567)) ([213206f](https://github.com/istanbuljs/nyc/commit/213206f))



<a name="10.3.1"></a>
## [10.3.1](https://github.com/istanbuljs/nyc/compare/v10.3.0...v10.3.1) (2017-05-04)


### Bug Fixes

* introduced a bug that resulted in source-maps not being loaded approriately on second test run ([#566](https://github.com/istanbuljs/nyc/issues/566)) ([1bf74fd](https://github.com/istanbuljs/nyc/commit/1bf74fd))



<a name="10.3.0"></a>
# [10.3.0](https://github.com/istanbuljs/nyc/compare/v10.2.0...v10.3.0) (2017-04-29)


### Bug Fixes

* source-maps were not being cached in the parent process when --all was being used ([#556](https://github.com/istanbuljs/nyc/issues/556)) ([ff73b18](https://github.com/istanbuljs/nyc/commit/ff73b18))


### Features

* add support for --no-clean, to disable deleting raw coverage output ([#558](https://github.com/istanbuljs/nyc/issues/558)) ([1887d1c](https://github.com/istanbuljs/nyc/commit/1887d1c))



<a name="10.2.0"></a>
# [10.2.0](https://github.com/istanbuljs/nyc/compare/v10.1.0...v10.2.0) (2017-03-28)


### Bug Fixes

* fix bug related to merging coverage reports see [#482](https://github.com/istanbuljs/nyc/issues/482) ([81229a0](https://github.com/istanbuljs/nyc/commit/81229a0))
* revert defaulting to empty file-coverage report, this caused too many issues ([25aec77](https://github.com/istanbuljs/nyc/commit/25aec77))


### Features

* allow babel cache to be enabled ([#517](https://github.com/istanbuljs/nyc/issues/517)) ([98ebdff](https://github.com/istanbuljs/nyc/commit/98ebdff))
* exclude the coverage/ folder by default 🚀 ([#502](https://github.com/istanbuljs/nyc/issues/502)) ([50adde4](https://github.com/istanbuljs/nyc/commit/50adde4))
* upgrade to version of yargs with extend support ([#541](https://github.com/istanbuljs/nyc/issues/541)) ([95cc09a](https://github.com/istanbuljs/nyc/commit/95cc09a))



<a name="10.1.2"></a>
## [10.1.2](https://github.com/istanbuljs/nyc/compare/v10.1.1...v10.1.2) (2017-01-18)


### Bug Fixes

* revert defaulting to empty file-coverage report, this caused too many issues ([25aec77](https://github.com/istanbuljs/nyc/commit/25aec77))



<a name="10.1.1"></a>
## [10.1.1](https://github.com/istanbuljs/nyc/compare/v10.1.0...v10.1.1) (2017-01-18)


### Bug Fixes

* fix bug related to merging coverage reports see [#482](https://github.com/istanbuljs/nyc/issues/482) ([81229a0](https://github.com/istanbuljs/nyc/commit/81229a0))



<a name="10.1.0"></a>
# [10.1.0](https://github.com/istanbuljs/nyc/compare/v10.0.2...v10.1.0) (2017-01-17)


### Bug Fixes

* address edge-cases related to --all when instrumentation is disabled ([#482](https://github.com/istanbuljs/nyc/issues/482)) ([8c58d68](https://github.com/istanbuljs/nyc/commit/8c58d68))
* pass configuration options to --check-coverage ([#483](https://github.com/istanbuljs/nyc/issues/483)) ([1022b16](https://github.com/istanbuljs/nyc/commit/1022b16))


### Features

* allow eager instantiation of instrumenter ([#490](https://github.com/istanbuljs/nyc/issues/490)) ([8b58c05](https://github.com/istanbuljs/nyc/commit/8b58c05))
* reporting watermarks can now be set in nyc config stanza ([#469](https://github.com/istanbuljs/nyc/issues/469)) ([0a1d72a](https://github.com/istanbuljs/nyc/commit/0a1d72a))
* upgrade to istanbul-lib-instrument with support for 'const foo = function () {}' name preservation. upgrade to istanbul-lib-hook with fix for ts-node. ([#494](https://github.com/istanbuljs/nyc/issues/494)) ([d8d2de0](https://github.com/istanbuljs/nyc/commit/d8d2de0))



<a name="10.0.2"></a>
## [10.0.2](https://github.com/istanbuljs/nyc/compare/v10.0.1...v10.0.2) (2017-01-02)


### Bug Fixes

* upgrade to newer istanbul-lib-instrument, with fixes for inferred function names ([#479](https://github.com/istanbuljs/nyc/issues/479)) ([e01ec8c](https://github.com/istanbuljs/nyc/commit/e01ec8c))



<a name="10.0.1"></a>
## [10.0.1](https://github.com/istanbuljs/nyc/compare/v10.0.0...v10.0.1) (2016-12-27)


### Bug Fixes

* upgrade spawn-wrap and istanbul-lib-instrument ([#477](https://github.com/istanbuljs/nyc/issues/477)) ([e0ef1d5](https://github.com/istanbuljs/nyc/commit/e0ef1d5))



<a name="10.0.0"></a>
# [10.0.0](https://github.com/istanbuljs/nyc/compare/v9.0.1...v10.0.0) (2016-11-22)


### Bug Fixes

* debug-log should be production dependency ([a3c7f83](https://github.com/istanbuljs/nyc/commit/a3c7f83))


### Features

* cache now turned on by default ([#454](https://github.com/istanbuljs/nyc/issues/454)) ([0dd970c](https://github.com/istanbuljs/nyc/commit/0dd970c))


### BREAKING CHANGES

* nyc's cache is now enabled by default



<a name="9.0.1"></a>
## [9.0.1](https://github.com/istanbuljs/nyc/compare/v9.0.0...v9.0.1) (2016-11-14)


### Bug Fixes

* addresses several test-exclude edge-cases. addresses perf issue with yargs ([#442](https://github.com/istanbuljs/nyc/issues/444))

<a name="9.0.0"></a>
# [9.0.0](https://github.com/istanbuljs/nyc/compare/v8.4.0...v9.0.0) (2016-11-13)


### Features

* adds support for source-map production ([#439](https://github.com/istanbuljs/nyc/issues/439)) ([31488f5](https://github.com/istanbuljs/nyc/commit/31488f5))
* allow an alternate cache folder to be provided ([#443](https://github.com/istanbuljs/nyc/issues/443)) ([b6713a3](https://github.com/istanbuljs/nyc/commit/b6713a3))
* node_modules is again excluded by default when custom exclude is provided ([#442](https://github.com/istanbuljs/nyc/issues/442)) ([2828538](https://github.com/istanbuljs/nyc/commit/2828538))


### BREAKING CHANGES

* **/node_modules/** is again excluded by default.



<a name="8.4.0"></a>
# [8.4.0](https://github.com/istanbuljs/nyc/compare/v8.3.1...v8.4.0) (2016-10-30)


### Bug Fixes

* hot-fix bad release of nyc ([c487eb3](https://github.com/istanbuljs/nyc/commit/c487eb3))
* reverts _maybeInstrumentSource logic, so that exclude is still applied ([#429](https://github.com/istanbuljs/nyc/issues/429)) ([b90d26f](https://github.com/istanbuljs/nyc/commit/b90d26f))
* update core istanbul libraries ([#426](https://github.com/istanbuljs/nyc/issues/426)) ([4945dac](https://github.com/istanbuljs/nyc/commit/4945dac))


### Features

* coverage information is now returned for process tree ([#416](https://github.com/istanbuljs/nyc/issues/416)) ([92dedda](https://github.com/istanbuljs/nyc/commit/92dedda))
* read coverage header when using "noop" instrumenter ([#420](https://github.com/istanbuljs/nyc/issues/420)) ([63a8758](https://github.com/istanbuljs/nyc/commit/63a8758))



<a name="8.3.1"></a>
## [8.3.1](https://github.com/istanbuljs/nyc/compare/v8.3.0...v8.3.1) (2016-10-06)


### Bug Fixes

* swap to version of test-exclude that does not warn ([#410](https://github.com/istanbuljs/nyc/issues/410)) ([78aac45](https://github.com/istanbuljs/nyc/commit/78aac45))
* update istanbul-lib-source-maps to 1.0.2 ([#411](https://github.com/istanbuljs/nyc/issues/411)) ([9c89945](https://github.com/istanbuljs/nyc/commit/9c89945))



<a name="8.3.0"></a>
# [8.3.0](https://github.com/istanbuljs/nyc/compare/v8.2.0...v8.3.0) (2016-09-15)


### Bug Fixes

* add a feature which allows us to bust the cache when breaking changes are introduced ([#394](https://github.com/istanbuljs/nyc/issues/394)) ([b7a413a](https://github.com/istanbuljs/nyc/commit/b7a413a))
* add shim for check-coverage on node 0.10 ([#386](https://github.com/istanbuljs/nyc/issues/386)) ([9ebaea8](https://github.com/istanbuljs/nyc/commit/9ebaea8))
* upgrade to newer versions of source-map, signal-exit, and instrument ([#389](https://github.com/istanbuljs/nyc/issues/389)) ([a9bdf0f](https://github.com/istanbuljs/nyc/commit/a9bdf0f)), closes [#379](https://github.com/istanbuljs/nyc/issues/379)


### Features

* add support for .nycrc ([#391](https://github.com/istanbuljs/nyc/issues/391)) ([1c2349b](https://github.com/istanbuljs/nyc/commit/1c2349b))
* refactored config to fix precedence of config vs. args ([#388](https://github.com/istanbuljs/nyc/issues/388)) ([99dbbb3](https://github.com/istanbuljs/nyc/commit/99dbbb3)), closes [#379](https://github.com/istanbuljs/nyc/issues/379)



<a name="8.2.0"></a>
# [8.2.0](https://github.com/istanbuljs/nyc/compare/v8.1.0...v8.2.0) (2016-09-02)


### Bug Fixes

* upgrade standard, and a few other dependencies. fix standard nits ([#375](https://github.com/istanbuljs/nyc/issues/375)) ([64c68b7](https://github.com/istanbuljs/nyc/commit/64c68b7))


### Features

* gather process tree information ([#364](https://github.com/istanbuljs/nyc/issues/364)) ([fabe5f3](https://github.com/istanbuljs/nyc/commit/fabe5f3))



<a name="8.1.0"></a>
# [8.1.0](https://github.com/bcoe/nyc/compare/v8.0.0...v8.1.0) (2016-08-14)


### Bug Fixes

* serialization using ',' was breaking globs ([#353](https://github.com/bcoe/nyc/issues/353)) ([22929db](https://github.com/bcoe/nyc/commit/22929db))


### Features

* implicitly assume `node` when the command starts with an option ([#350](https://github.com/bcoe/nyc/issues/350)) ([2bb52cd](https://github.com/bcoe/nyc/commit/2bb52cd))



<a name="8.0.0"></a>
# [8.0.0](https://github.com/bcoe/nyc/compare/v7.1.0...v8.0.0) (2016-08-12)


### Bug Fixes

* make `nyc instrument` work in subdirectories ([#343](https://github.com/bcoe/nyc/issues/343)) ([a82cf49](https://github.com/bcoe/nyc/commit/a82cf49))
* upgrade to versions of coverage/instrument that solve out-of-bound errors ([#349](https://github.com/bcoe/nyc/issues/349)) ([bee0328](https://github.com/bcoe/nyc/commit/bee0328))


### Features

* upgrade to new test-exclude; with suppport for node_modules, and empty exclude ([#348](https://github.com/bcoe/nyc/issues/348)) ([d616ffc](https://github.com/bcoe/nyc/commit/d616ffc))


### BREAKING CHANGES

* node_modules is no longer automatically excluded, and an empty array of exclude rules can now be provided.



<a name="7.1.0"></a>
# [7.1.0](https://github.com/bcoe/nyc/compare/v7.0.0...v7.1.0) (2016-07-24)


### Bug Fixes

* make --all flag work with files with extensions other than .js ([#326](https://github.com/bcoe/nyc/issues/326)) ([d0a8674](https://github.com/bcoe/nyc/commit/d0a8674))
* work around for Windows path issue nodejs/node[#6624](https://github.com/bcoe/nyc/issues/6624) ([6b1fed0](https://github.com/bcoe/nyc/commit/6b1fed0))


### Features

* nyc no longer tries to run arguments passed to the instrumented bin ([#322](https://github.com/bcoe/nyc/issues/322)) ([e0a8c0b](https://github.com/bcoe/nyc/commit/e0a8c0b))
* use istanbul-lib-hook to wrap require and support vm hooks ([#308](https://github.com/bcoe/nyc/issues/308)) ([2b64cf8](https://github.com/bcoe/nyc/commit/2b64cf8))



<a name="7.0.0"></a>
# [7.0.0](https://github.com/bcoe/nyc/compare/v6.6.1...v7.0.0) (2016-07-09)


### Bug Fixes

* avoid pid collisions. ([#301](https://github.com/bcoe/nyc/issues/301)) ([f67bff7](https://github.com/bcoe/nyc/commit/f67bff7))
* disable the babel/nyc cache ([#303](https://github.com/bcoe/nyc/issues/303)) ([104b3da](https://github.com/bcoe/nyc/commit/104b3da))


### Features

* adds instrument command line option ([#298](https://github.com/bcoe/nyc/issues/298)) ([e45b51b](https://github.com/bcoe/nyc/commit/e45b51b))
* nyc is being refactored to become the official Istanbul 1.0 bin ([#286](https://github.com/bcoe/nyc/issues/286)) ([61a05ea](https://github.com/bcoe/nyc/commit/61a05ea))


### BREAKING CHANGES

* significant chunks of nyc's API have been reworked, to use the Istanbul 1.0 API: source-map support, instrumentation, the check-coverage command, etc.



<a name="6.6.1"></a>
## [6.6.1](https://github.com/bcoe/nyc/compare/v6.6.0...v6.6.1) (2016-06-14)


### Bug Fixes

* the package tree of bundled dependencies was incorrect ([7bdccf5](https://github.com/bcoe/nyc/commit/7bdccf5))



<a name="6.6.0"></a>
# [6.6.0](https://github.com/bcoe/nyc/compare/v6.5.1...v6.6.0) (2016-06-14)


### Features

* add "instrument" option as alternative to specifying noop instrumenter ([#278](https://github.com/bcoe/nyc/issues/278)) ([ea028b9](https://github.com/bcoe/nyc/commit/ea028b9))



<a name="6.5.1"></a>
## [6.5.1](https://github.com/bcoe/nyc/compare/v6.5.0...v6.5.1) (2016-06-14)


### Bug Fixes

* upgrade foreground-child dependency ([#276](https://github.com/bcoe/nyc/issues/276)) ([1b9e4a8](https://github.com/bcoe/nyc/commit/1b9e4a8))



<a name="6.5.0"></a>
# [6.5.0](https://github.com/bcoe/nyc/compare/v6.4.4...v6.5.0) (2016-06-13)


### Bug Fixes

* cleanup dependencies ([#254](https://github.com/bcoe/nyc/issues/254)) ([a20d03d](https://github.com/bcoe/nyc/commit/a20d03d))
* discard more bad source map positions ([#255](https://github.com/bcoe/nyc/issues/255)) ([0838a0e](https://github.com/bcoe/nyc/commit/0838a0e))
* Update AppVeyor config with ~faster npm install ([#252](https://github.com/bcoe/nyc/issues/252)) ([df591f4](https://github.com/bcoe/nyc/commit/df591f4))


### Features

* make `__coverage__` the default approach we advocate for ES2015 coverage ([#268](https://github.com/bcoe/nyc/issues/268)) ([9fd2295](https://github.com/bcoe/nyc/commit/9fd2295))



<a name="6.4.4"></a>
## [6.4.4](https://github.com/bcoe/nyc/compare/v6.4.3...v6.4.4) (2016-05-07)


### Bug Fixes

* upgraded dependencies, add missing lodash bundled dependency ([#250](https://github.com/bcoe/nyc/issues/250))([32042fc](https://github.com/bcoe/nyc/commit/32042fc))



<a name="6.4.3"></a>
## [6.4.3](https://github.com/bcoe/nyc/compare/v6.4.2...v6.4.3) (2016-05-06)


### Bug Fixes

* must bundle dependencies on npm<3.x or they will flatten (we need a better long-term solution)([9826f11](https://github.com/bcoe/nyc/commit/9826f11))



<a name="6.4.2"></a>
## [6.4.2](https://github.com/bcoe/nyc/compare/v6.4.1...v6.4.2) (2016-05-02)


### Bug Fixes

* **update:** update strip-bom to version 3.0.0 ([#240](https://github.com/bcoe/nyc/issues/240))([24f55e7](https://github.com/bcoe/nyc/commit/24f55e7))
* upgrade spawn-wrap to version that works with new shelljs ([#242](https://github.com/bcoe/nyc/issues/242))([b16053c](https://github.com/bcoe/nyc/commit/b16053c))



<a name="6.4.1"></a>
## [6.4.1](https://github.com/bcoe/nyc/compare/v6.4.0...v6.4.1) (2016-04-27)


### Bug Fixes

* strip any duplicate extensions from --extension ([#237](https://github.com/bcoe/nyc/issues/237)) ([0946f82](https://github.com/bcoe/nyc/commit/0946f82))



<a name="6.4.0"></a>
# [6.4.0](https://github.com/bcoe/nyc/compare/v6.3.0...v6.4.0) (2016-04-11)


### Bug Fixes

* adds CLI integration testing, where there was no integration testing before. ([3403ca1](https://github.com/bcoe/nyc/commit/3403ca1))

### Features

* **cli:** --include and --exclude are now accepted as CLI options, thanks [@rpominov](https://github.com/rpominov) \o/ see [#207](https://github.com/bcoe/nyc/issues/207) ([f8a02b4](https://github.com/bcoe/nyc/commit/f8a02b4))



<a name="6.3.0"></a>
# [6.3.0](https://github.com/bcoe/nyc/compare/v6.2.1...v6.3.0) (2016-04-08)


### Features

* better docs for excluding, thanks @kentdodds \o/  ([22b06fe](https://github.com/bcoe/nyc/commit/22b06fe))
* updating dependencies (spawn wrap with npm patches \o/) ([ac841b8](https://github.com/bcoe/nyc/commit/ac841b8))



<a name="6.2.1"></a>
## [6.2.1](https://github.com/bcoe/nyc/compare/v6.2.0...v6.2.1) (2016-04-05)


### Bug Fixes

* **bundling:** .gitignore was interfering with bundle ([0e4adae](https://github.com/bcoe/nyc/commit/0e4adae))



<a name="6.2.0"></a>
# [6.2.0](https://github.com/bcoe/nyc/compare/v6.1.1...v6.2.0) (2016-04-05)


### Bug Fixes

* **bundle dependencies:** start bundling dependencies, which should address some issues we have seen with  ([6116077](https://github.com/bcoe/nyc/commit/6116077))
* **exit code:** use test program’s exit code even with `--check-coverage` ([00bbeb2](https://github.com/bcoe/nyc/commit/00bbeb2))

### Features

* **conventional changelog:** introducing conventional-changelog for changelog generation ([f594c5e](https://github.com/bcoe/nyc/commit/f594c5e))
* **exclude patterns:** introduces new exclude-patterns based on @kentcdodds' coding conventions. ([51b1777](https://github.com/bcoe/nyc/commit/51b1777))
* **update dependencies:** new foreground-child and spawn-wrap have landed \o/ ([1a0ad0b](https://github.com/bcoe/nyc/commit/1a0ad0b))



### v6.1.1 (2016/03/13 14:23 +7:00)

- [#194](https://github.com/bcoe/nyc/pull/194) hot-fix for --all with multiple extensions (@bcoe)

### v6.1.0 (2016/03/12 15:00 +7:00)

- [#191](https://github.com/bcoe/nyc/pull/191) upgrade to non-singleton verison of yargs (@bcoe)
- [#185](https://github.com/bcoe/nyc/pull/185) default to long-form option names so that they can be overridden in package.json (@rapzo)
- [#180](https://github.com/bcoe/nyc/pull/180) fix bug with findUp (@bcoe)
- [#178](https://github.com/bcoe/nyc/pull/178) --all should handle extensions other than .js. (@lloydcotten)
- [#177](https://github.com/bcoe/nyc/pull/177) add .editorconfig (@JaKXz)

### v6.0.0 (2016/02/20 +7:00)

- [#167](https://github.com/bcoe/nyc/pull/167) all of nyc's settings can now
  be configured in package.json (@bcoe)
- [#164](https://github.com/bcoe/nyc/pull/164) coverage tracking now uses absolute paths, awesome \o/ (@novemberborn)
- [#163](https://github.com/bcoe/nyc/pull/163) support for extensions other than .js (@lloydcotten)

### v5.6.0 (2016/02/03 +7:00)

- [#159](https://github.com/bcoe/nyc/pull/159) skip should continue working with source-maps (@novemberborn)
- [#160](https://github.com/bcoe/nyc/pull/160) don't instrument files outside of the current working directory (@novemberborn)

### v5.5.0 (2016/01/24 +07:00)

- [#152](https://github.com/bcoe/nyc/pull/152) upgrade to newest version of foreground-child (@isaacs)
- [#150](https://github.com/bcoe/nyc/pull/150) guard against illegal positions in source-map (@novemberborn)

### v5.4.0 (2016/01/20 +07:00)

- [#147](https://github.com/bcoe/nyc/pull/147) fix for foreground-child on Windows (@isaacs)
- [#145](https://github.com/bcoe/nyc/pull/145) allow coverage output directory to be specified (@bcoe)
- [#143](https://github.com/bcoe/nyc/pull/143) run files included via --all through transpilers (@bcoe)
- [#139](https://github.com/bcoe/nyc/pull/139) documentation updates (@tcurdt)
- [#138](https://github.com/bcoe/nyc/pull/138) Split CLI from spawn wrapper (@isaacs)

### v5.3.0 (2016/01/05 14:07 -08:00)

- [#132](https://github.com/bcoe/nyc/pull/132/files) Move config to top level nyc argument. (@jamestalmage)

### v5.2.0 (2016/01/02 17:13 -08:00)

- [#126](https://github.com/bcoe/nyc/pull/126) Add --check-coverage shorthand, which fails tests if coverage slips below threshold (@bcoe)
- [#123](https://github.com/bcoe/nyc/pull/123) Upgrade spawn-wrap, foreground-child (@isaacs)
- [#122](https://github.com/bcoe/nyc/pull/122) Use module for finding cache directory (@jamestalmage)

### v5.1.1 (2015/12/30 14:52 -08:00)

- [#121](https://github.com/bcoe/nyc/pull/121) Fix for --all functionality. (@jamestalmage)

### v5.1.0 (2015/12/27 20:36 -08:00)

- [#108](https://github.com/bcoe/nyc/pull/108) Adds cache functionality. this is a big one, thanks! (@jamestalmage)
- [#118](https://github.com/bcoe/nyc/pull/118) Stop bundling spawn-wrap dependency (@bcoe)
- [#114](https://github.com/bcoe/nyc/pull/114) Update to latest versions of tap, glob, rimraf (@isaacs)
- [#107](https://github.com/bcoe/nyc/pull/107) Get test-suite running on Windows (@bcoe)

### v5.0.1 (2015/12/14 09:09 -07:00)

- [#94](https://github.com/bcoe/nyc/pull/93) Windows failed if argument had no replace() method. (@bcoe)

### v5.0.0 (2015/12/09 11:03 -07:00)

- [#87](https://github.com/bcoe/nyc/pull/87) make spawn() work on Windows (@bcoe)
- [#84](https://github.com/bcoe/nyc/pull/84) glob based include/exclude of files (@Lalem001)
- [#78](https://github.com/bcoe/nyc/pull/78) improvements to sourcemap tests (@novemberborn)
- [#73](https://github.com/bcoe/nyc/pull/73) improvements to require tests (@novemberborn)
- [#65](https://github.com/bcoe/nyc/pull/65) significant improvements to require hooks (@novemberborn)
- [#64](https://github.com/bcoe/nyc/pull/64) upgrade Istanbul (@novemberborn)

### v4.0.0 (2015/11/29 10:13 -07:00)

- [#58](https://github.com/bcoe/nyc/pull/58) adds support for Babel (@bcoe)

### v3.2.2 (2015/09/11 22:02 -07:00)

- [#47](https://github.com/bcoe/nyc/pull/47) make the default exclude rules work on Windows (@bcoe)
- [#45](https://github.com/bcoe/nyc/pull/45) pull in patched versions of spawn-wrap and foreground-child, which support Windows (@bcoe)
- [#44](https://github.com/bcoe/nyc/pull/44) Adds --all option which adds 0% coverage reports for all files in project, regardless of whether code touches them (@ronkorving)

### v3.1.0 (2015/08/02 19:04 +00:00)

- [#38](https://github.com/bcoe/nyc/pull/38) fixes for windows spawning (@rmg)

### v3.0.1 (2015/07/25 20:51 +00:00)
- [#33](https://github.com/bcoe/nyc/pull/33) spawn istanbul in a way that is less likely to break npm@3.x (@bcoe)

### v3.0.0 (2015/06/28 19:49 +00:00)

- [#31](https://github.com/bcoe/nyc/pull/31) Combine instrumentation and reporting steps, based
  on @Raynos' suggestion (@bcoe)

### v2.4.0 (2015/06/24 15:57 +00:00)
- [#30](https://github.com/bcoe/nyc/pull/30) Added check-coverage functionality, thanks
  @Raynos! (@bcoe)

### v2.3.0 (2015/06/04 06:43 +00:00)
- [#27](https://github.com/bcoe/nyc/pull/27) upgraded tap, and switched tests to using tap --coverage (@bcoe)
- [#25](https://github.com/bcoe/nyc/pull/25) support added for multiple reporters, thanks @jasisk! (@jasisk)

### v2.2.0 (2015/05/25 21:05 +00:00)
- [b2e4707](https://github.com/bcoe/nyc/commit/b2e4707ca16750fe274f61039baf1cabdd6b0149) change location of nyc_output to .nyc_output. Added note about coveralls comments. (@sindresorhus)

### v2.1.3 (2015/05/25 06:30 +00:00)
- [376e328](https://github.com/bcoe/nyc/commit/376e32871d2d65ca31e7d8ba691293ac3ba6117e) handle corrupt JSON files in nyc_output (@bcoe)

### v2.1.1 (2015/05/25 02:52 +00:00)
- [b39dec5](https://github.com/bcoe/nyc/commit/b39dec5a7fb9004be72d024d5d1df2984dd21a52) new signal-exit handles process.exit() in process.on('exit') (@isaacs)

### v2.1.0 (2015/05/23 20:55 +00:00)
- [ad13b30](https://github.com/bcoe/nyc/commit/ad13b30cf263ccc3607e1707ebdf582345ce90fe) added CHANGELOG.md \o/ (@bcoe)
- [53fef48](https://github.com/bcoe/nyc/commit/53fef4820e7b502d00561fb5d16f5bfb4b641192) put tests around @shackpank's work on .istanbul.yml (@bcoe)
- [da81c54](https://github.com/bcoe/nyc/commit/da81c5427c2dee38496def9741fdde5524fa0942) upgrade spawn-wrap and foreground-child (@isaacs)
- [4f69327](https://github.com/bcoe/nyc/commit/4f69327b5e6247770bf299fab86abb67a042b26a) pin tap until new version of nyc can be pulled in (@bcoe)

### v2.0.6 (2015/05/23 06:52 +00:00)
- [cd70a41](https://github.com/bcoe/nyc/commit/cd70a414adc12b79770eaca9e8ca0e5f954924f3) upgrade signal-exit (@bcoe)

### v2.0.5 (2015/05/20 05:44 +00:00)
- [#11](https://github.com/bcoe/nyc/pull/11) Merge pull request #11 from bcoe/exlude-docs (@bcoe)

### v2.0.4 (2015/05/19 04:58 +00:00)
- [4d920ef](https://github.com/bcoe/nyc/commit/4d920ef6e0843729a911ca1cf6deaf6645e21f60) ensure that writing code coverage always happens last (@bcoe)

### v2.0.3 (2015/05/18 01:52 +00:00)
- [94d2693](https://github.com/bcoe/nyc/commit/94d2693739cf7145333d941c88e0d3af9592c1d6) spawn-wrap@0.1.1 (@isaacs)

### v2.0.1 (2015/05/18 01:46 +00:00)
- [62c2cb0](https://github.com/bcoe/nyc/commit/62c2cb0941fbda8aa5ef6ba4877c02a046b68c6c) upgrade signal-exit dependency (@bcoe)

### v2.0.0 (2015/05/16 21:38 +00:00)
- [d27794e](https://github.com/bcoe/nyc/commit/d27794e3c527ccf743501f328b9749f1bcf9cefe) got rid of nyc-report bin (@bcoe)
- [64c9824](https://github.com/bcoe/nyc/commit/64c98241db36331b611cf990343da40d5f45685a) added better documentation and CLI. (@bcoe)

### v1.4.1 (2015/05/16 19:23 +00:00)
- [ae05346](https://github.com/bcoe/nyc/commit/ae0534617a59c86905f1da290d067945bf7d1bb9) pulled in new version of signal-exit (@bcoe)

### v1.4.0 (2015/05/16 09:11 +00:00)
- [8ca6e16](https://github.com/bcoe/nyc/commit/8ca6e16f6ecb7fa488944cd00d84ae5d355345d2) pulled in signal-exit module (@bcoe)

### v1.3.0 (2015/05/15 15:56 +00:00)
- [0f701da](https://github.com/bcoe/nyc/commit/0f701da5aa3ad8a02872c4c6c8c37d0deb2c5877) pulled in new spawn-wrap, various bug fixes (@isaacs)

### v1.2.0 (2015/05/13 20:21 +00:00)
- [2611ba4](https://github.com/bcoe/nyc/commit/2611ba44f12a25c12c0f95a9bdcfbf905dbb070f) handle signals when writing coverage report (@bcoe)

### v1.1.3 (2015/05/11 18:31 +00:00)
- [8b362d6](https://github.com/bcoe/nyc/commit/8b362d600845722943c1da8213f0406d6b3a3874) istanbul has a text lcov report now \o/ (@bcoe)

### v1.1.2 (2015/05/11 06:52 +00:00)
- [48b21cf](https://github.com/bcoe/nyc/commit/48b21cf3b35f6d14d35ac9afdd423ead09a2368e) added coverage and build badges (@bcoe)

### v1.1.0 (2015/05/10 01:32 +00:00)
- [6c3f8a6](https://github.com/bcoe/nyc/commit/6c3f8a6147c376e87a22c4a72a1ab28ab4177349) pulled in @isaacs spawn-wrap module (@isaacs)
- [d8956f1](https://github.com/bcoe/nyc/commit/d8956f170f12a8a27cc3f7611f78230393bf105b) we now pass cwd around using the process.env.NYC_CWD variable (@bcoe)
