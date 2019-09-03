# Change Log

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

<a name="3.1.3"></a>
## [3.1.3](https://github.com/npm/lifecycle/compare/v3.1.2...v3.1.3) (2019-08-12)


### Bug Fixes

* fail properly if uid-number raises an error ([e0e1b62](https://github.com/npm/lifecycle/commit/e0e1b62))



<a name="3.1.2"></a>
## [3.1.2](https://github.com/npm/lifecycle/compare/v3.1.1...v3.1.2) (2019-07-22)


### Bug Fixes

* do not exclude /path/ from process.env copying ([53e6318](https://github.com/npm/lifecycle/commit/53e6318))



<a name="3.1.0"></a>
# [3.1.0](https://github.com/npm/lifecycle/compare/v3.0.0...v3.1.0) (2019-07-17)


### Bug Fixes

* remove procInterrupt listener on SIGINT in procError ([ea18ed2](https://github.com/npm/lifecycle/commit/ea18ed2)), closes [#36](https://github.com/npm/lifecycle/issues/36) [#11](https://github.com/npm/lifecycle/issues/11) [#18](https://github.com/npm/lifecycle/issues/18)
* set only one PATH env variable for child proc ([3aaf954](https://github.com/npm/lifecycle/commit/3aaf954)), closes [#22](https://github.com/npm/lifecycle/issues/22) [#25](https://github.com/npm/lifecycle/issues/25)


### Features

* **process.env.path:** Use platform specific path casing if present ([5523951](https://github.com/npm/lifecycle/commit/5523951)), closes [#29](https://github.com/npm/lifecycle/issues/29) [#30](https://github.com/npm/lifecycle/issues/30)



<a name="3.0.0"></a>
# [3.0.0](https://github.com/npm/lifecycle/compare/v2.1.1...v3.0.0) (2019-07-10)


* node-gyp@5.0.2 ([3c5aae6](https://github.com/npm/lifecycle/commit/3c5aae6))


### BREAKING CHANGES

* requires modifying the version of node-gyp in npm cli.

Required for https://github.com/npm/cli/pull/208
Fix: https://github.com/npm/npm-lifecycle/issues/37
Close: https://github.com/npm/npm-lifecycle/issues/38
PR-URL: https://github.com/npm/npm-lifecycle/pull/38
Credit: @irega
Reviewed-by: @isaacs



<a name="2.1.1"></a>
## [2.1.1](https://github.com/npm/lifecycle/compare/v2.1.0...v2.1.1) (2019-05-08)


### Bug Fixes

* **test:** update postinstall script for fixture ([220cd70](https://github.com/npm/lifecycle/commit/220cd70))



<a name="2.1.0"></a>
# [2.1.0](https://github.com/npm/lifecycle/compare/v2.0.3...v2.1.0) (2018-08-13)


### Bug Fixes

* **windows:** revert writing all possible cases of PATH variables ([#22](https://github.com/npm/lifecycle/issues/22)) ([8fcaa21](https://github.com/npm/lifecycle/commit/8fcaa21)), closes [#20](https://github.com/npm/lifecycle/issues/20)


### Features

* **ci:** add node@10 to CI ([e206aa0](https://github.com/npm/lifecycle/commit/e206aa0))



<a name="2.0.3"></a>
## [2.0.3](https://github.com/npm/lifecycle/compare/v2.0.2...v2.0.3) (2018-05-16)



<a name="2.0.2"></a>
## [2.0.2](https://github.com/npm/lifecycle/compare/v2.0.1...v2.0.2) (2018-05-16)


### Bug Fixes

* **hooks:** run .hooks scripts even if package.json script is not present ([#13](https://github.com/npm/lifecycle/issues/13)) ([67adc2d](https://github.com/npm/lifecycle/commit/67adc2d))
* **windows:** Write to all possible cases of PATH variables ([#17](https://github.com/npm/lifecycle/issues/17)) ([e4ecc54](https://github.com/npm/lifecycle/commit/e4ecc54))



<a name="2.0.1"></a>
## [2.0.1](https://github.com/npm/lifecycle/compare/v2.0.0...v2.0.1) (2018-03-08)


### Bug Fixes

* **log:** Fix formatting of invalid wd warning ([#12](https://github.com/npm/lifecycle/issues/12)) ([ced38f3](https://github.com/npm/lifecycle/commit/ced38f3))



<a name="2.0.0"></a>
# [2.0.0](https://github.com/npm/lifecycle/compare/v1.0.3...v2.0.0) (2017-11-17)


### Features

* **node-gyp:** use own node-gyp ([ae94ed2](https://github.com/npm/lifecycle/commit/ae94ed2))
* **nodeOptions:** add "nodeOptions" option to set NODE_OPTIONS for child ([#7](https://github.com/npm/lifecycle/issues/7)) ([2eb7a38](https://github.com/npm/lifecycle/commit/2eb7a38))
* **stdio:** add child process io options and default logging of piped stdout/err ([#3](https://github.com/npm/lifecycle/issues/3)) ([7b8281a](https://github.com/npm/lifecycle/commit/7b8281a))


### BREAKING CHANGES

* **node-gyp:** Previously you had to bring your own node-gyp AND you had
to provide access the way npm does, by having a `bin` dir with a
`node-gyp-bin` in it.

Fixes: #4



<a name="1.0.3"></a>
## [1.0.3](https://github.com/npm/lifecycle/compare/v1.0.2...v1.0.3) (2017-09-01)


### Bug Fixes

* **runCmd:** add missing option to runCmd recursive queue call ([1a69ce8](https://github.com/npm/lifecycle/commit/1a69ce8))



<a name="1.0.2"></a>
## [1.0.2](https://github.com/npm/lifecycle/compare/v1.0.1...v1.0.2) (2017-08-17)



<a name="1.0.1"></a>
## [1.0.1](https://github.com/npm/lifecycle/compare/v1.0.0...v1.0.1) (2017-08-16)


### Bug Fixes

* **license:** fix up license documentation ([a784ca0](https://github.com/npm/lifecycle/commit/a784ca0))



<a name="1.0.0"></a>
# 1.0.0 (2017-08-16)


### Bug Fixes

* **misc:** use strict to fix node[@4](https://github.com/4) ([#2](https://github.com/npm/lifecycle/issues/2)) ([961ceb9](https://github.com/npm/lifecycle/commit/961ceb9))


### Features

* **api:** Extract from npm proper ([#1](https://github.com/npm/lifecycle/issues/1)) ([27d9930](https://github.com/npm/lifecycle/commit/27d9930))


### BREAKING CHANGES

* **api:** this is the initial implementation
