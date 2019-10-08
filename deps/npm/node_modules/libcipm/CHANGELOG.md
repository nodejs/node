# Change Log

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

<a name="4.0.4"></a>
## [4.0.4](https://github.com/npm/libcipm/compare/v4.0.3...v4.0.4) (2019-09-24)


### Bug Fixes

* pack git directories properly ([576ab36](https://github.com/npm/libcipm/commit/576ab36)), closes [#4](https://github.com/npm/libcipm/issues/4)



<a name="4.0.3"></a>
## [4.0.3](https://github.com/npm/libcipm/compare/v4.0.2...v4.0.3) (2019-08-12)


### Bug Fixes

* do not pass opts.log to lifecycle ([46b2101](https://github.com/npm/libcipm/commit/46b2101))



<a name="4.0.2"></a>
## [4.0.2](https://github.com/npm/libcipm/compare/v4.0.1...v4.0.2) (2019-08-12)



<a name="4.0.1"></a>
## [4.0.1](https://github.com/npm/libcipm/compare/v4.0.0...v4.0.1) (2019-08-12)


### Bug Fixes

* respect and retain all configs passed in ([20b7372](https://github.com/npm/libcipm/commit/20b7372))



<a name="4.0.0"></a>
# [4.0.0](https://github.com/npm/libcipm/compare/v3.0.3...v4.0.0) (2019-07-10)


* npm-lifecycle@3.0.0 ([84b8d7e](https://github.com/npm/libcipm/commit/84b8d7e))


### Bug Fixes

* **lifecycle:** remove warning from bluebird ([#59](https://github.com/npm/libcipm/issues/59)) ([7af39e6](https://github.com/npm/libcipm/commit/7af39e6)), closes [#58](https://github.com/npm/libcipm/issues/58)


### BREAKING CHANGES

* requires updating node-gyp in npm/cli



<a name="3.0.3"></a>
## [3.0.3](https://github.com/npm/libcipm/compare/v3.0.2...v3.0.3) (2019-01-22)


### Bug Fixes

* **scripts:** pass in opts.dir directly ([018df27](https://github.com/npm/libcipm/commit/018df27))



<a name="3.0.2"></a>
## [3.0.2](https://github.com/npm/libcipm/compare/v3.0.1...v3.0.2) (2018-08-31)


### Bug Fixes

* **worker:** missed a spot ([4371558](https://github.com/npm/libcipm/commit/4371558))



<a name="3.0.1"></a>
## [3.0.1](https://github.com/npm/libcipm/compare/v3.0.0...v3.0.1) (2018-08-31)


### Bug Fixes

* **workers:** disable workers for now ([64db490](https://github.com/npm/libcipm/commit/64db490))



<a name="3.0.0"></a>
# [3.0.0](https://github.com/npm/libcipm/compare/v2.0.2...v3.0.0) (2018-08-31)


### Features

* **config:** switch to modern, figgy-pudding configuration ([#57](https://github.com/npm/libcipm/issues/57)) ([161f6b2](https://github.com/npm/libcipm/commit/161f6b2))


### BREAKING CHANGES

* **config:** this updates cipm to use pacote@9, which consumes npm-style config objects, not pacoteOpts()-style objects.



<a name="2.0.2"></a>
## [2.0.2](https://github.com/npm/libcipm/compare/v2.0.1...v2.0.2) (2018-08-10)


### Bug Fixes

* **child:** only override dirPacker if opts.dirPacker is defined ([#55](https://github.com/npm/libcipm/issues/55)) ([13ab2f0](https://github.com/npm/libcipm/commit/13ab2f0))



<a name="2.0.1"></a>
## [2.0.1](https://github.com/npm/libcipm/compare/v2.0.0...v2.0.1) (2018-07-27)


### Bug Fixes

* **deps:** move mkdirp to prod deps ([6878f39](https://github.com/npm/libcipm/commit/6878f39))



<a name="2.0.0"></a>
# [2.0.0](https://github.com/npm/libcipm/compare/v1.6.3...v2.0.0) (2018-05-24)


### meta

* update node version support ([694b4d3](https://github.com/npm/libcipm/commit/694b4d3))


### BREAKING CHANGES

* node@4 is no longer supported



<a name="1.6.3"></a>
## [1.6.3](https://github.com/npm/libcipm/compare/v1.6.2...v1.6.3) (2018-05-24)



<a name="1.6.2"></a>
## [1.6.2](https://github.com/npm/libcipm/compare/v1.6.1...v1.6.2) (2018-04-08)


### Bug Fixes

* **lifecycle:** detect binding.gyp for default install lifecycle ([#46](https://github.com/npm/libcipm/issues/46)) ([9149631](https://github.com/npm/libcipm/commit/9149631)), closes [#45](https://github.com/npm/libcipm/issues/45)



<a name="1.6.1"></a>
## [1.6.1](https://github.com/npm/libcipm/compare/v1.6.0...v1.6.1) (2018-03-13)


### Bug Fixes

* **bin:** Set non-zero exit code on error ([#41](https://github.com/npm/libcipm/issues/41)) ([54d0106](https://github.com/npm/libcipm/commit/54d0106))
* **lifecycle:** defer to lifecycle’s internal logic as to whether or not to execute a run-script ([#42](https://github.com/npm/libcipm/issues/42)) ([7f27a52](https://github.com/npm/libcipm/commit/7f27a52)), closes [npm/npm#19258](https://github.com/npm/npm/issues/19258)
* **prefix:** don't reference prefix before computing it ([#40](https://github.com/npm/libcipm/issues/40)) ([08ed1cc](https://github.com/npm/libcipm/commit/08ed1cc))
* **prefix:** Resolve to promise when passing --prefix to npm ci ([#43](https://github.com/npm/libcipm/issues/43)) ([401d466](https://github.com/npm/libcipm/commit/401d466))



<a name="1.6.0"></a>
# [1.6.0](https://github.com/npm/libcipm/compare/v1.5.1...v1.6.0) (2018-03-01)


### Bug Fixes

* **bin:** cli.js was being excluded ([d62668e](https://github.com/npm/libcipm/commit/d62668e))


### Features

* **libcipm:** working standalone cipm release! ([a3383fd](https://github.com/npm/libcipm/commit/a3383fd))



<a name="1.5.1"></a>
## [1.5.1](https://github.com/npm/libcipm/compare/v1.5.0...v1.5.1) (2018-03-01)


### Bug Fixes

* **_from:** do not add _from to directory deps ([7405360](https://github.com/npm/libcipm/commit/7405360))



<a name="1.5.0"></a>
# [1.5.0](https://github.com/npm/libcipm/compare/v1.4.1...v1.5.0) (2018-03-01)


### Bug Fixes

* **errors:** handle aggregate errors better ([6239499](https://github.com/npm/libcipm/commit/6239499))


### Features

* **logger:** rudimentary progress bar update ([c5d9dc7](https://github.com/npm/libcipm/commit/c5d9dc7))



<a name="1.4.1"></a>
## [1.4.1](https://github.com/npm/libcipm/compare/v1.4.0...v1.4.1) (2018-02-27)


### Bug Fixes

* **buildTree:** linking in parallel causes hoist-clobbering ([5ffbc0e](https://github.com/npm/libcipm/commit/5ffbc0e)), closes [#39](https://github.com/npm/libcipm/issues/39)
* **buildTree:** use checkDepEnv here too ([41a4634](https://github.com/npm/libcipm/commit/41a4634))
* **perf:** split up updateJson and buildTree ([df5aba0](https://github.com/npm/libcipm/commit/df5aba0))
* **perf:** stop using the readPackageJson version to update packages ([8da3d5a](https://github.com/npm/libcipm/commit/8da3d5a))



<a name="1.4.0"></a>
# [1.4.0](https://github.com/npm/libcipm/compare/v1.3.3...v1.4.0) (2018-02-21)


### Features

* **extract:** add support for --only and --also ([ad143ae](https://github.com/npm/libcipm/commit/ad143ae))



<a name="1.3.3"></a>
## [1.3.3](https://github.com/npm/libcipm/compare/v1.3.2...v1.3.3) (2018-02-21)


### Bug Fixes

* **extract:** stop extracting deps before parent :\ ([c6847dc](https://github.com/npm/libcipm/commit/c6847dc))



<a name="1.3.2"></a>
## [1.3.2](https://github.com/npm/libcipm/compare/v1.3.1...v1.3.2) (2018-02-15)



<a name="1.3.1"></a>
## [1.3.1](https://github.com/npm/libcipm/compare/v1.3.0...v1.3.1) (2018-02-15)



<a name="1.3.0"></a>
# [1.3.0](https://github.com/npm/libcipm/compare/v1.2.0...v1.3.0) (2018-02-13)


### Features

* **extract:** link directory deps and install missing bundle deps ([8334e9e](https://github.com/npm/libcipm/commit/8334e9e))



<a name="1.2.0"></a>
# [1.2.0](https://github.com/npm/libcipm/compare/v1.1.2...v1.2.0) (2018-02-07)


### Features

* **metadata:** add _resolved, _integrity, and _from on install ([36642dc](https://github.com/npm/libcipm/commit/36642dc))



<a name="1.1.2"></a>
## [1.1.2](https://github.com/npm/libcipm/compare/v1.1.1...v1.1.2) (2018-01-19)



<a name="1.1.1"></a>
## [1.1.1](https://github.com/npm/libcipm/compare/v1.1.0...v1.1.1) (2018-01-19)



<a name="1.1.0"></a>
# [1.1.0](https://github.com/npm/libcipm/compare/v1.0.1...v1.1.0) (2018-01-07)


### Features

* **log:** add some helpful log output ([f443f03](https://github.com/npm/libcipm/commit/f443f03))



<a name="1.0.1"></a>
## [1.0.1](https://github.com/npm/libcipm/compare/v1.0.0...v1.0.1) (2018-01-07)


### Bug Fixes

* **deps:** added protoduck to pkgjson ([ecbe719](https://github.com/npm/libcipm/commit/ecbe719))



<a name="1.0.0"></a>
# [1.0.0](https://github.com/npm/libcipm/compare/v0.9.1...v1.0.0) (2018-01-07)


### Features

* **cli:** splitting off CLI into a separate tool ([cff65c1](https://github.com/npm/libcipm/commit/cff65c1))


### BREAKING CHANGES

* **cli:** libcipm is its own library now,



<a name="0.9.1"></a>
## [0.9.1](https://github.com/npm/libcipm/compare/v0.9.0...v0.9.1) (2018-01-07)


### Bug Fixes

* **prefix:** oops @ prefix ([cc5adac](https://github.com/npm/libcipm/commit/cc5adac))



<a name="0.9.0"></a>
# [0.9.0](https://github.com/npm/libcipm/compare/v0.8.0...v0.9.0) (2018-01-07)


### Bug Fixes

* **package:** add pacote to bundleDependencies ([#36](https://github.com/npm/libcipm/issues/36)) ([a69742e](https://github.com/npm/libcipm/commit/a69742e))


### Features

* **config:** allow injection of npm configs ([#35](https://github.com/npm/libcipm/issues/35)) ([1f5694b](https://github.com/npm/libcipm/commit/1f5694b))



<a name="0.8.0"></a>
# [0.8.0](https://github.com/npm/libcipm/compare/v0.7.2...v0.8.0) (2017-11-28)


### Features

* **gyp:** new npm-lifecycle[@2](https://github.com/2) with included node-gyp ([a4ed938](https://github.com/npm/libcipm/commit/a4ed938))



<a name="0.7.2"></a>
## [0.7.2](https://github.com/npm/libcipm/compare/v0.7.1...v0.7.2) (2017-10-13)


### Bug Fixes

* **extract:** idk why this was breaking. Seriously. ([433a2be](https://github.com/npm/libcipm/commit/433a2be))
* **tree:** pass through a custom Promise to logiTree ([2d29efb](https://github.com/npm/libcipm/commit/2d29efb))


### Performance Improvements

* zoomzoom. Even more concurrency! ([db9c2e0](https://github.com/npm/libcipm/commit/db9c2e0))



<a name="0.7.1"></a>
## [0.7.1](https://github.com/npm/libcipm/compare/v0.7.0...v0.7.1) (2017-10-13)


### Bug Fixes

* **scripts:** separate extract and build and fix ordering ([eb072a5](https://github.com/npm/libcipm/commit/eb072a5))



<a name="0.7.0"></a>
# [0.7.0](https://github.com/npm/libcipm/compare/v0.6.0...v0.7.0) (2017-10-12)


### Bug Fixes

* **lockfile:** npm-shrinkwrap takes precedence over package-lock (#28) ([3b98fb3](https://github.com/npm/libcipm/commit/3b98fb3))


### Features

* **optional:** ignore failed optional deps (#27) ([a654629](https://github.com/npm/libcipm/commit/a654629))



<a name="0.6.0"></a>
# [0.6.0](https://github.com/npm/libcipm/compare/v0.5.1...v0.6.0) (2017-10-09)


### Features

* **scripts:** run prepare and prepublish scripts in the root (#26) ([e0e35a3](https://github.com/npm/libcipm/commit/e0e35a3))



<a name="0.5.1"></a>
## [0.5.1](https://github.com/npm/libcipm/compare/v0.5.0...v0.5.1) (2017-10-09)



<a name="0.5.0"></a>
# [0.5.0](https://github.com/npm/libcipm/compare/v0.4.0...v0.5.0) (2017-10-09)


### Bug Fixes

* **output:** npm does not punctuate this ([e7ba976](https://github.com/npm/libcipm/commit/e7ba976))
* **shutdown:** make sure workers close ([7ab57d0](https://github.com/npm/libcipm/commit/7ab57d0))


### Features

* **bin:** link bins and run scripts (#25) ([fab74bf](https://github.com/npm/libcipm/commit/fab74bf))
* **lifecycle:** run scripts in dep order (#23) ([68ecfac](https://github.com/npm/libcipm/commit/68ecfac))



<a name="0.4.0"></a>
# [0.4.0](https://github.com/npm/libcipm/compare/v0.3.2...v0.4.0) (2017-10-04)


### Features

* **opts:** support full range of relevant CLI opts (#19) ([6f2bd51](https://github.com/npm/libcipm/commit/6f2bd51))



<a name="0.3.2"></a>
## [0.3.2](https://github.com/npm/libcipm/compare/v0.3.1...v0.3.2) (2017-09-06)


### Bug Fixes

* **bin:** make cli executable by default (#13) ([14a9a5f](https://github.com/npm/libcipm/commit/14a9a5f))
* **config:** use npm.cmd on win32 and fix tests (#12) ([d912d16](https://github.com/npm/libcipm/commit/d912d16)), closes [#12](https://github.com/npm/libcipm/issues/12)
* **json:** strip BOM when reading JSON files (#8) ([2529149](https://github.com/npm/libcipm/commit/2529149))



<a name="0.3.1"></a>
## [0.3.1](https://github.com/npm/libcipm/compare/v0.3.0...v0.3.1) (2017-09-05)



<a name="0.3.0"></a>
# [0.3.0](https://github.com/npm/libcipm/compare/v0.2.0...v0.3.0) (2017-09-05)


### Features

* **lockfile:** verify that lockfile matches package.json (#5) ([f631203](https://github.com/npm/libcipm/commit/f631203))
* **scripts:** support --ignore-scripts option (#9) ([213ca02](https://github.com/npm/libcipm/commit/213ca02))



<a name="0.2.0"></a>
# [0.2.0](https://github.com/npm/libcipm/compare/v0.1.1...v0.2.0) (2017-09-01)


### Bug Fixes

* **main:** default --prefix ([ff06a31](https://github.com/npm/libcipm/commit/ff06a31))


### Features

* **lifecycle:** actually run lifecycle scripts correctly ([7f8933e](https://github.com/npm/libcipm/commit/7f8933e))



<a name="0.1.1"></a>
## [0.1.1](https://github.com/npm/libcipm/compare/v0.1.0...v0.1.1) (2017-08-30)


### Bug Fixes

* **files:** oops. forgot to include new files in tarball ([1ee85c9](https://github.com/npm/libcipm/commit/1ee85c9))



<a name="0.1.0"></a>
# 0.1.0 (2017-08-30)


### Bug Fixes

* **config:** pipe stdout ([08e6af8](https://github.com/npm/libcipm/commit/08e6af8))
* **extract:** make sure to extract properly ([9643583](https://github.com/npm/libcipm/commit/9643583))
* **license:** switch to MIT ([0d10d0d](https://github.com/npm/libcipm/commit/0d10d0d))


### Features

* **impl:** rough prototype ([2970e43](https://github.com/npm/libcipm/commit/2970e43))
* **lifecycle:** Run lifecycle events, implement prefix option, add unit tests (#1) ([d6629be](https://github.com/npm/libcipm/commit/d6629be)), closes [#1](https://github.com/npm/libcipm/issues/1)
* **opts:** add usage string and --help ([efcc48d](https://github.com/npm/libcipm/commit/efcc48d))
