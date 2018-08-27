# Change Log

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

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
