# Changelog

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

### [8.1.5](https://github.com/npm/registry-fetch/compare/v8.1.4...v8.1.5) (2020-10-12)


### Bug Fixes

* respect publishConfig.registry when specified ([32e36ef](https://github.com/npm/registry-fetch/commit/32e36efe86302ed319973cd5b1e6ccc3f62e557e)), closes [#35](https://github.com/npm/registry-fetch/issues/35)

### [8.1.4](https://github.com/npm/registry-fetch/compare/v8.1.3...v8.1.4) (2020-08-17)


### Bug Fixes

* redact passwords from http logs ([3c294eb](https://github.com/npm/registry-fetch/commit/3c294ebbd7821725db4ff1bc5fe368c49613efcc))

### [8.1.3](https://github.com/npm/registry-fetch/compare/v8.1.2...v8.1.3) (2020-07-21)

### [8.1.2](https://github.com/npm/registry-fetch/compare/v8.1.1...v8.1.2) (2020-07-11)

### [8.1.1](https://github.com/npm/registry-fetch/compare/v8.1.0...v8.1.1) (2020-06-30)

## [8.1.0](https://github.com/npm/registry-fetch/compare/v8.0.3...v8.1.0) (2020-05-20)


### Features

* add npm-command HTTP header ([1bb4eb2](https://github.com/npm/registry-fetch/commit/1bb4eb2c66ee8a0dc62558bdcff1b548e2bb9820))

### [8.0.3](https://github.com/npm/registry-fetch/compare/v8.0.2...v8.0.3) (2020-05-13)


### Bug Fixes

* update minipass and make-fetch-happen to latest ([3b6c5d0](https://github.com/npm/registry-fetch/commit/3b6c5d0d8ccd4c4a97862a65acef956f19aec127)), closes [#23](https://github.com/npm/registry-fetch/issues/23)

### [8.0.2](https://github.com/npm/registry-fetch/compare/v8.0.1...v8.0.2) (2020-05-04)


### Bug Fixes

* update make-fetch-happen to 8.0.6 ([226df2c](https://github.com/npm/registry-fetch/commit/226df2c32e3f9ed8ceefcfdbd11efb178181b442))

## [8.0.0](https://github.com/npm/registry-fetch/compare/v7.0.1...v8.0.0) (2020-02-24)


### ⚠ BREAKING CHANGES

* Removes the 'opts.refer' option and the HTTP Referer
header (unless explicitly added to the 'headers' option, of course).

PR-URL: https://github.com/npm/npm-registry-fetch/pull/25
Credit: @isaacs

### Bug Fixes

* remove referer header and opts.refer ([eb8f7af](https://github.com/npm/registry-fetch/commit/eb8f7af3c102834856604c1be664b00ca0fe8ef2)), closes [#25](https://github.com/npm/registry-fetch/issues/25)

### [7.0.1](https://github.com/npm/registry-fetch/compare/v7.0.0...v7.0.1) (2020-02-24)

## [7.0.0](https://github.com/npm/registry-fetch/compare/v6.0.2...v7.0.0) (2020-02-18)


### ⚠ BREAKING CHANGES

* figgy pudding is now nowhere to be found.
* this removes figgy-pudding, and drops several option
aliases.

Defaults and behavior are all the same, and this module is now using the
canonical camelCase option names that npm v7 will provide to all its
deps.

Related to: https://github.com/npm/rfcs/pull/102

PR-URL: https://github.com/npm/npm-registry-fetch/pull/22
Credit: @isaacs

### Bug Fixes

* Remove figgy-pudding, use canonical option names ([ede3c08](https://github.com/npm/registry-fetch/commit/ede3c087007fd1808e02b1af70562220d03b18a9)), closes [#22](https://github.com/npm/registry-fetch/issues/22)


* update cacache, ssri, make-fetch-happen ([57fcc88](https://github.com/npm/registry-fetch/commit/57fcc889bee03edcc0a2025d96a171039108c231))

### [6.0.2](https://github.com/npm/registry-fetch/compare/v6.0.1...v6.0.2) (2020-02-14)


### Bug Fixes

* always bypass cache when ?write=true ([83f89f3](https://github.com/npm/registry-fetch/commit/83f89f35abd2ed0507c869e37f90ed746375772c))

### [6.0.1](https://github.com/npm/registry-fetch/compare/v6.0.0...v6.0.1) (2020-02-14)


### Bug Fixes

* use 30s default for timeout as per README ([50e8afc](https://github.com/npm/registry-fetch/commit/50e8afc6ff850542feb588f9f9c64ebae59e72a0)), closes [#20](https://github.com/npm/registry-fetch/issues/20)

## [6.0.0](https://github.com/npm/registry-fetch/compare/v5.0.1...v6.0.0) (2019-12-17)


### ⚠ BREAKING CHANGES

* This drops support for node < 10.

There are some lint failures due to standard pushing for using WhatWG URL
objects instead of url.parse/url.resolve.  However, the code in this lib
does some fancy things with the query/search portions of the parsed url
object, so it'll take a bit of care to make it work properly.

### Bug Fixes

* detect CI so our tests don't fail in CI ([5813da6](https://github.com/npm/registry-fetch/commit/5813da634cef73b12e40373972d7937e6934fce0))
* Use WhatWG URLs instead of url.parse ([8ccfa8a](https://github.com/npm/registry-fetch/commit/8ccfa8a72c38cfedb0f525b7f453644fd4444f99))


* normalize settings, drop old nodes, update deps ([510b125](https://github.com/npm/registry-fetch/commit/510b1255cc7ed4bb397a34e0007757dae33e2275))

<a name="5.0.1"></a>
## [5.0.1](https://github.com/npm/registry-fetch/compare/v5.0.0...v5.0.1) (2019-11-11)



<a name="5.0.0"></a>
# [5.0.0](https://github.com/npm/registry-fetch/compare/v4.0.2...v5.0.0) (2019-10-04)


### Bug Fixes

* prefer const in getAuth function ([90ac7b1](https://github.com/npm/registry-fetch/commit/90ac7b1))
* use minizlib instead of core zlib ([e64702e](https://github.com/npm/registry-fetch/commit/e64702e))


### Features

* refactor to use Minipass streams ([bb37f20](https://github.com/npm/registry-fetch/commit/bb37f20))


### BREAKING CHANGES

* this replaces all core streams (except for some
PassThrough streams in a few tests) with Minipass streams, and updates
all deps to the latest and greatest Minipass versions of things.



<a name="4.0.2"></a>
## [4.0.2](https://github.com/npm/registry-fetch/compare/v4.0.0...v4.0.2) (2019-10-04)


### Bug Fixes

* Add null check on body on 401 errors ([e3a0186](https://github.com/npm/registry-fetch/commit/e3a0186)), closes [#9](https://github.com/npm/registry-fetch/issues/9)
* **deps:** Add explicit dependency on safe-buffer ([8eae5f0](https://github.com/npm/registry-fetch/commit/8eae5f0)), closes [npm/libnpmaccess#2](https://github.com/npm/libnpmaccess/issues/2) [#3](https://github.com/npm/registry-fetch/issues/3)



<a name="4.0.0"></a>
# [4.0.0](https://github.com/npm/registry-fetch/compare/v3.9.1...v4.0.0) (2019-07-15)


* cacache@12.0.0, infer uid from cache folder ([0c4f060](https://github.com/npm/registry-fetch/commit/0c4f060))


### BREAKING CHANGES

* uid and gid are inferred from cache folder, rather than
being passed in as options.



<a name="3.9.1"></a>
## [3.9.1](https://github.com/npm/registry-fetch/compare/v3.9.0...v3.9.1) (2019-07-02)



<a name="3.9.0"></a>
# [3.9.0](https://github.com/npm/registry-fetch/compare/v3.8.0...v3.9.0) (2019-01-24)


### Features

* **auth:** support username:password encoded legacy _auth ([a91f90c](https://github.com/npm/registry-fetch/commit/a91f90c))



<a name="3.8.0"></a>
# [3.8.0](https://github.com/npm/registry-fetch/compare/v3.7.0...v3.8.0) (2018-08-23)


### Features

* **mapJson:** add support for passing in json stream mapper ([0600986](https://github.com/npm/registry-fetch/commit/0600986))



<a name="3.7.0"></a>
# [3.7.0](https://github.com/npm/registry-fetch/compare/v3.6.0...v3.7.0) (2018-08-23)


### Features

* **json.stream:** add utility function for streamed JSON parsing ([051d969](https://github.com/npm/registry-fetch/commit/051d969))



<a name="3.6.0"></a>
# [3.6.0](https://github.com/npm/registry-fetch/compare/v3.5.0...v3.6.0) (2018-08-22)


### Bug Fixes

* **docs:** document opts.forceAuth ([40bcd65](https://github.com/npm/registry-fetch/commit/40bcd65))


### Features

* **opts.ignoreBody:** add a boolean to throw away response bodies ([6923702](https://github.com/npm/registry-fetch/commit/6923702))



<a name="3.5.0"></a>
# [3.5.0](https://github.com/npm/registry-fetch/compare/v3.4.0...v3.5.0) (2018-08-22)


### Features

* **pkgid:** heuristic pkgid calculation for errors ([2e789a5](https://github.com/npm/registry-fetch/commit/2e789a5))



<a name="3.4.0"></a>
# [3.4.0](https://github.com/npm/registry-fetch/compare/v3.3.0...v3.4.0) (2018-08-22)


### Bug Fixes

* **deps:** use new figgy-pudding with aliases fix ([0308f54](https://github.com/npm/registry-fetch/commit/0308f54))


### Features

* **auth:** add forceAuth option to force a specific auth mechanism ([4524d17](https://github.com/npm/registry-fetch/commit/4524d17))



<a name="3.3.0"></a>
# [3.3.0](https://github.com/npm/registry-fetch/compare/v3.2.1...v3.3.0) (2018-08-21)


### Bug Fixes

* **query:** stop including undefined keys ([4718b1b](https://github.com/npm/registry-fetch/commit/4718b1b))


### Features

* **otp:** use heuristic detection for malformed EOTP responses ([f035194](https://github.com/npm/registry-fetch/commit/f035194))



<a name="3.2.1"></a>
## [3.2.1](https://github.com/npm/registry-fetch/compare/v3.2.0...v3.2.1) (2018-08-16)


### Bug Fixes

* **opts:** pass through non-null opts.retry ([beba040](https://github.com/npm/registry-fetch/commit/beba040))



<a name="3.2.0"></a>
# [3.2.0](https://github.com/npm/registry-fetch/compare/v3.1.1...v3.2.0) (2018-07-27)


### Features

* **gzip:** add opts.gzip convenience opt ([340abe0](https://github.com/npm/registry-fetch/commit/340abe0))



<a name="3.1.1"></a>
## [3.1.1](https://github.com/npm/registry-fetch/compare/v3.1.0...v3.1.1) (2018-04-09)



<a name="3.1.0"></a>
# [3.1.0](https://github.com/npm/registry-fetch/compare/v3.0.0...v3.1.0) (2018-04-09)


### Features

* **config:** support no-proxy and https-proxy options ([9aa906b](https://github.com/npm/registry-fetch/commit/9aa906b))



<a name="3.0.0"></a>
# [3.0.0](https://github.com/npm/registry-fetch/compare/v2.1.0...v3.0.0) (2018-04-09)


### Bug Fixes

* **api:** pacote integration-related fixes ([a29de4f](https://github.com/npm/registry-fetch/commit/a29de4f))
* **config:** stop caring about opts.config ([5856a6f](https://github.com/npm/registry-fetch/commit/5856a6f))


### BREAKING CHANGES

* **config:** opts.config is no longer supported. Pass the options down in opts itself.



<a name="2.1.0"></a>
# [2.1.0](https://github.com/npm/registry-fetch/compare/v2.0.0...v2.1.0) (2018-04-08)


### Features

* **token:** accept opts.token for opts._authToken ([108c9f0](https://github.com/npm/registry-fetch/commit/108c9f0))



<a name="2.0.0"></a>
# [2.0.0](https://github.com/npm/registry-fetch/compare/v1.1.1...v2.0.0) (2018-04-08)


### meta

* drop support for node@4 ([758536e](https://github.com/npm/registry-fetch/commit/758536e))


### BREAKING CHANGES

* node@4 is no longer supported



<a name="1.1.1"></a>
## [1.1.1](https://github.com/npm/registry-fetch/compare/v1.1.0...v1.1.1) (2018-04-06)



<a name="1.1.0"></a>
# [1.1.0](https://github.com/npm/registry-fetch/compare/v1.0.1...v1.1.0) (2018-03-16)


### Features

* **specs:** can use opts.spec to trigger pickManifest ([85c4ac9](https://github.com/npm/registry-fetch/commit/85c4ac9))



<a name="1.0.1"></a>
## [1.0.1](https://github.com/npm/registry-fetch/compare/v1.0.0...v1.0.1) (2018-03-16)


### Bug Fixes

* **query:** oops console.log ([870e4f5](https://github.com/npm/registry-fetch/commit/870e4f5))



<a name="1.0.0"></a>
# 1.0.0 (2018-03-16)


### Bug Fixes

* **auth:** get auth working with all the little details ([84b94ba](https://github.com/npm/registry-fetch/commit/84b94ba))
* **deps:** add bluebird as an actual dep ([1286e31](https://github.com/npm/registry-fetch/commit/1286e31))
* **errors:** Unknown auth errors use default code ([#1](https://github.com/npm/registry-fetch/issues/1)) ([3d91b93](https://github.com/npm/registry-fetch/commit/3d91b93))
* **standard:** remove args from invocation ([9620a0a](https://github.com/npm/registry-fetch/commit/9620a0a))


### Features

* **api:** baseline kinda-working API impl ([bf91f9f](https://github.com/npm/registry-fetch/commit/bf91f9f))
* **body:** automatic handling of different opts.body values ([f3b97db](https://github.com/npm/registry-fetch/commit/f3b97db))
* **config:** nicer input config input handling ([b9ce21d](https://github.com/npm/registry-fetch/commit/b9ce21d))
* **opts:** use figgy-pudding for opts handling ([0abd527](https://github.com/npm/registry-fetch/commit/0abd527))
* **query:** add query utility support ([65ea8b1](https://github.com/npm/registry-fetch/commit/65ea8b1))
