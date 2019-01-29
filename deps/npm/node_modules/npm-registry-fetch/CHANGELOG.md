# Change Log

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

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
