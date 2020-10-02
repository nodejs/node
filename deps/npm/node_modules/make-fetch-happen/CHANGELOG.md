# Changelog

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

### [8.0.3](https://github.com/npm/make-fetch-happen/compare/v8.0.2...v8.0.3) (2020-03-03)


### Bug Fixes

* remoteFetch takes instance of fetch.Headers ([6e0de7b](https://github.com/npm/make-fetch-happen/commit/6e0de7b10b8597eaff69fea06a266914766cf5ab)), closes [#22](https://github.com/npm/make-fetch-happen/issues/22)

### [8.0.1](https://github.com/npm/make-fetch-happen/compare/v8.0.0...v8.0.1) (2020-02-18)

## [8.0.0](https://github.com/npm/make-fetch-happen/compare/v7.1.1...v8.0.0) (2020-02-18)


### ⚠ BREAKING CHANGES

* this module now only supports taking a plain JavaScript
options object, not a figgy pudding config object.

* update cacache and ssri ([09e4f97](https://github.com/npm/make-fetch-happen/commit/09e4f9794a6f134d3f1d8e65eb9bd940e38e5bfc))

### [7.1.1](https://github.com/npm/make-fetch-happen/compare/v7.1.0...v7.1.1) (2020-01-28)

## [7.1.0](https://github.com/npm/make-fetch-happen/compare/v7.0.0...v7.1.0) (2019-12-17)


### Features

* use globalAgent when in lambda ([bd9409d](https://github.com/npm/make-fetch-happen/commit/bd9409da246a979b665ebd23967ec01dd928ce47)), closes [#4](https://github.com/npm/make-fetch-happen/issues/4)

## [7.0.0](https://github.com/npm/make-fetch-happen/compare/v6.1.0...v7.0.0) (2019-12-17)


### ⚠ BREAKING CHANGES

* drops support for node v8, since it's EOL as of 2020-01-01

### Features

* **github:** added github actions with coveralls integration ([1913c1b](https://github.com/npm/make-fetch-happen/commit/1913c1b51aaac6044b4dab65b3d19ec943a35f39))
* updated fetch module; linting mostly; based on testing ([063f28e](https://github.com/npm/make-fetch-happen/commit/063f28ea1ac23f7e9d9d79e15949ca82b634ce97))
* **utils:** fixed configure-options based on testing ([9dd4f6f](https://github.com/npm/make-fetch-happen/commit/9dd4f6f108442dc247de44e1ddc0341edcb84c9b))
* fixed test dep requires; added mockRequire function to mock tests properly ([95de7a1](https://github.com/npm/make-fetch-happen/commit/95de7a171110907e30f41f489e4be983cd8184a5))
* refactored functions into utilities ([74620dd](https://github.com/npm/make-fetch-happen/commit/74620dd7c2262ac46d9b4f6ac2dc9ff45a4f19ee))
* updated dev deps; update tap; updated standard ([dce6eec](https://github.com/npm/make-fetch-happen/commit/dce6eece130fb20164a62eeabc6090811d8f14a4))
* updated fetch tests; linting, logic, added tests ([d50aeaf](https://github.com/npm/make-fetch-happen/commit/d50aeafebeb5d8f7118d7f6660208f40ac487804))


### Bug Fixes

* format cache key with new URL object shape ([21cb6cc](https://github.com/npm/make-fetch-happen/commit/21cb6cc968aabff8b5c5c02e3666fb093fd6578c))
* polish out an unnecessary URL object creation ([67a01d4](https://github.com/npm/make-fetch-happen/commit/67a01d46b2cacbadc22f49604ee524526cee3912)), closes [#14](https://github.com/npm/make-fetch-happen/issues/14)
* support user without password in proxy auth ([e24bbf9](https://github.com/npm/make-fetch-happen/commit/e24bbf935bc8a2c49070cdb2518e5ee290143191))
* updated 'files' property in package file ([945e40c](https://github.com/npm/make-fetch-happen/commit/945e40c7fbb59333e0c632c490683e4babc68dc1))
* Use WhatWG URL objects over deprecated legacy url API ([28aca97](https://github.com/npm/make-fetch-happen/commit/28aca97dfb63ca003ebf62d1b961771cfbb2481d))


* drop node 8 ([9fa7944](https://github.com/npm/make-fetch-happen/commit/9fa7944cbc603f3a194dfb440f519a7d5265653e))

## [6.1.0](https://github.com/npm/make-fetch-happen/compare/v6.0.1...v6.1.0) (2019-11-14)


### Bug Fixes

* **streams:** change condition/logic of fitInMemory used when defining memoize ([c173723](https://github.com/npm/make-fetch-happen/commit/c173723))

### [6.0.1](https://github.com/npm/make-fetch-happen/compare/v6.0.0...v6.0.1) (2019-10-23)

<a name="6.0.0"></a>
# [6.0.0](https://github.com/npm/make-fetch-happen/compare/v5.0.0...v6.0.0) (2019-10-01)

### Bug Fixes

* preserve rfc7234 5.5.4 warnings ([001b91e](https://github.com/npm/make-fetch-happen/commit/001b91e))
* properly detect thrown HTTP "error" objects ([d7cbeb4](https://github.com/npm/make-fetch-happen/commit/d7cbeb4))
* safely create synthetic response body for 304 ([bc70f88](https://github.com/npm/make-fetch-happen/commit/bc70f88))

### Features

* **promises:** refactor bluebird with native promises ([7482d54](https://github.com/npm/make-fetch-happen/commit/7482d54))

### BREAKING CHANGES

* **streams:** refactor node streams with minipass ([1d7f5a3](https://github.com/npm/make-fetch-happen/commit/1d7f5a3))

<a name="5.0.0"></a>
# [5.0.0](https://github.com/npm/make-fetch-happen/compare/v4.0.2...v5.0.0) (2019-07-15)


### Features

* cacache@12, no need for uid/gid opts ([fdb956f](https://github.com/npm/make-fetch-happen/commit/fdb956f))


### BREAKING CHANGES

* cache uid and gid are inferred from the cache folder itself,
not passed in as options.



<a name="4.0.2"></a>
## [4.0.2](https://github.com/npm/make-fetch-happen/compare/v4.0.1...v4.0.2) (2019-07-02)



<a name="4.0.1"></a>
## [4.0.1](https://github.com/npm/make-fetch-happen/compare/v4.0.0...v4.0.1) (2018-04-12)


### Bug Fixes

* **integrity:** use new sri.match() for verification ([4f371a0](https://github.com/npm/make-fetch-happen/commit/4f371a0))



<a name="4.0.0"></a>
# [4.0.0](https://github.com/npm/make-fetch-happen/compare/v3.0.0...v4.0.0) (2018-04-09)


### meta

* drop node@4, add node@9 ([7b0191a](https://github.com/npm/make-fetch-happen/commit/7b0191a))


### BREAKING CHANGES

* node@4 is no longer supported



<a name="3.0.0"></a>
# [3.0.0](https://github.com/npm/make-fetch-happen/compare/v2.6.0...v3.0.0) (2018-03-12)


### Bug Fixes

* **license:** switch to ISC ([#49](https://github.com/npm/make-fetch-happen/issues/49)) ([bf90c6d](https://github.com/npm/make-fetch-happen/commit/bf90c6d))
* **standard:** standard@11 update ([ff0aa70](https://github.com/npm/make-fetch-happen/commit/ff0aa70))


### BREAKING CHANGES

* **license:** license changed from CC0 to ISC.



<a name="2.6.0"></a>
# [2.6.0](https://github.com/npm/make-fetch-happen/compare/v2.5.0...v2.6.0) (2017-11-14)


### Bug Fixes

* **integrity:** disable node-fetch compress when checking integrity (#42) ([a7cc74c](https://github.com/npm/make-fetch-happen/commit/a7cc74c))


### Features

* **onretry:** Add `options.onRetry` (#48) ([f90ccff](https://github.com/npm/make-fetch-happen/commit/f90ccff))



<a name="2.5.0"></a>
# [2.5.0](https://github.com/npm/make-fetch-happen/compare/v2.4.13...v2.5.0) (2017-08-24)


### Bug Fixes

* **agent:** support timeout durations greater than 30 seconds ([04875ae](https://github.com/npm/make-fetch-happen/commit/04875ae)), closes [#35](https://github.com/npm/make-fetch-happen/issues/35)


### Features

* **cache:** export cache deletion functionality (#40) ([3da4250](https://github.com/npm/make-fetch-happen/commit/3da4250))



<a name="2.4.13"></a>
## [2.4.13](https://github.com/npm/make-fetch-happen/compare/v2.4.12...v2.4.13) (2017-06-29)


### Bug Fixes

* **deps:** bump other deps for bugfixes ([eab8297](https://github.com/npm/make-fetch-happen/commit/eab8297))
* **proxy:** bump proxy deps with bugfixes (#32) ([632f860](https://github.com/npm/make-fetch-happen/commit/632f860)), closes [#32](https://github.com/npm/make-fetch-happen/issues/32)



<a name="2.4.12"></a>
## [2.4.12](https://github.com/npm/make-fetch-happen/compare/v2.4.11...v2.4.12) (2017-06-06)


### Bug Fixes

* **cache:** encode x-local-cache-etc headers to be header-safe ([dc9fb1b](https://github.com/npm/make-fetch-happen/commit/dc9fb1b))



<a name="2.4.11"></a>
## [2.4.11](https://github.com/npm/make-fetch-happen/compare/v2.4.10...v2.4.11) (2017-06-05)


### Bug Fixes

* **deps:** bump deps with ssri fix ([bef1994](https://github.com/npm/make-fetch-happen/commit/bef1994))



<a name="2.4.10"></a>
## [2.4.10](https://github.com/npm/make-fetch-happen/compare/v2.4.9...v2.4.10) (2017-05-31)


### Bug Fixes

* **deps:** bump dep versions with bugfixes ([0af4003](https://github.com/npm/make-fetch-happen/commit/0af4003))
* **proxy:** use auth parameter for proxy authentication (#30) ([c687306](https://github.com/npm/make-fetch-happen/commit/c687306))



<a name="2.4.9"></a>
## [2.4.9](https://github.com/npm/make-fetch-happen/compare/v2.4.8...v2.4.9) (2017-05-25)


### Bug Fixes

* **cache:** use the passed-in promise for resolving cache stuff ([4c46257](https://github.com/npm/make-fetch-happen/commit/4c46257))



<a name="2.4.8"></a>
## [2.4.8](https://github.com/npm/make-fetch-happen/compare/v2.4.7...v2.4.8) (2017-05-25)


### Bug Fixes

* **cache:** pass uid/gid/Promise through to cache ([a847c92](https://github.com/npm/make-fetch-happen/commit/a847c92))



<a name="2.4.7"></a>
## [2.4.7](https://github.com/npm/make-fetch-happen/compare/v2.4.6...v2.4.7) (2017-05-24)


### Bug Fixes

* **deps:** pull in various fixes from deps ([fc2a587](https://github.com/npm/make-fetch-happen/commit/fc2a587))



<a name="2.4.6"></a>
## [2.4.6](https://github.com/npm/make-fetch-happen/compare/v2.4.5...v2.4.6) (2017-05-24)


### Bug Fixes

* **proxy:** choose agent for http(s)-proxy by protocol of destUrl ([ea4832a](https://github.com/npm/make-fetch-happen/commit/ea4832a))
* **proxy:** make socks proxy working ([1de810a](https://github.com/npm/make-fetch-happen/commit/1de810a))
* **proxy:** revert previous proxy solution ([563b0d8](https://github.com/npm/make-fetch-happen/commit/563b0d8))



<a name="2.4.5"></a>
## [2.4.5](https://github.com/npm/make-fetch-happen/compare/v2.4.4...v2.4.5) (2017-05-24)


### Bug Fixes

* **proxy:** use the destination url when determining agent ([1a714e7](https://github.com/npm/make-fetch-happen/commit/1a714e7))



<a name="2.4.4"></a>
## [2.4.4](https://github.com/npm/make-fetch-happen/compare/v2.4.3...v2.4.4) (2017-05-23)


### Bug Fixes

* **redirect:** handle redirects explicitly  (#27) ([4c4af54](https://github.com/npm/make-fetch-happen/commit/4c4af54))



<a name="2.4.3"></a>
## [2.4.3](https://github.com/npm/make-fetch-happen/compare/v2.4.2...v2.4.3) (2017-05-06)


### Bug Fixes

* **redirect:** redirects now delete authorization if hosts fail to match ([c071805](https://github.com/npm/make-fetch-happen/commit/c071805))



<a name="2.4.2"></a>
## [2.4.2](https://github.com/npm/make-fetch-happen/compare/v2.4.1...v2.4.2) (2017-05-04)


### Bug Fixes

* **cache:** reduce race condition window by checking for content ([24544b1](https://github.com/npm/make-fetch-happen/commit/24544b1))
* **match:** Rewrite the conditional stream logic (#25) ([66bba4b](https://github.com/npm/make-fetch-happen/commit/66bba4b))



<a name="2.4.1"></a>
## [2.4.1](https://github.com/npm/make-fetch-happen/compare/v2.4.0...v2.4.1) (2017-04-28)


### Bug Fixes

* **memoization:** missed spots + allow passthrough of memo objs ([ac0cd12](https://github.com/npm/make-fetch-happen/commit/ac0cd12))



<a name="2.4.0"></a>
# [2.4.0](https://github.com/npm/make-fetch-happen/compare/v2.3.0...v2.4.0) (2017-04-28)


### Bug Fixes

* **memoize:** cacache had a broken memoizer ([8a9ed4c](https://github.com/npm/make-fetch-happen/commit/8a9ed4c))


### Features

* **memoization:** only slurp stuff into memory if opts.memoize is not false ([0744adc](https://github.com/npm/make-fetch-happen/commit/0744adc))



<a name="2.3.0"></a>
# [2.3.0](https://github.com/npm/make-fetch-happen/compare/v2.2.6...v2.3.0) (2017-04-27)


### Features

* **agent:** added opts.strictSSL and opts.localAddress ([c35015a](https://github.com/npm/make-fetch-happen/commit/c35015a))
* **proxy:** Added opts.noProxy and NO_PROXY support ([f45c915](https://github.com/npm/make-fetch-happen/commit/f45c915))



<a name="2.2.6"></a>
## [2.2.6](https://github.com/npm/make-fetch-happen/compare/v2.2.5...v2.2.6) (2017-04-26)


### Bug Fixes

* **agent:** check uppercase & lowercase proxy env (#24) ([acf2326](https://github.com/npm/make-fetch-happen/commit/acf2326)), closes [#22](https://github.com/npm/make-fetch-happen/issues/22)
* **deps:** switch to node-fetch-npm and stop bundling ([3db603b](https://github.com/npm/make-fetch-happen/commit/3db603b))



<a name="2.2.5"></a>
## [2.2.5](https://github.com/npm/make-fetch-happen/compare/v2.2.4...v2.2.5) (2017-04-23)


### Bug Fixes

* **deps:** bump cacache and use its size feature ([926c1d3](https://github.com/npm/make-fetch-happen/commit/926c1d3))



<a name="2.2.4"></a>
## [2.2.4](https://github.com/npm/make-fetch-happen/compare/v2.2.3...v2.2.4) (2017-04-18)


### Bug Fixes

* **integrity:** hash verification issues fixed ([07f9402](https://github.com/npm/make-fetch-happen/commit/07f9402))



<a name="2.2.3"></a>
## [2.2.3](https://github.com/npm/make-fetch-happen/compare/v2.2.2...v2.2.3) (2017-04-18)


### Bug Fixes

* **staleness:** responses older than 8h were never stale :< ([b54dd75](https://github.com/npm/make-fetch-happen/commit/b54dd75))
* **warning:** remove spurious warning, make format more spec-compliant ([2e4f6bb](https://github.com/npm/make-fetch-happen/commit/2e4f6bb))



<a name="2.2.2"></a>
## [2.2.2](https://github.com/npm/make-fetch-happen/compare/v2.2.1...v2.2.2) (2017-04-12)


### Bug Fixes

* **retry:** stop retrying 404s ([6fafd53](https://github.com/npm/make-fetch-happen/commit/6fafd53))



<a name="2.2.1"></a>
## [2.2.1](https://github.com/npm/make-fetch-happen/compare/v2.2.0...v2.2.1) (2017-04-10)


### Bug Fixes

* **deps:** move test-only deps to devDeps ([2daaf80](https://github.com/npm/make-fetch-happen/commit/2daaf80))



<a name="2.2.0"></a>
# [2.2.0](https://github.com/npm/make-fetch-happen/compare/v2.1.0...v2.2.0) (2017-04-09)


### Bug Fixes

* **cache:** treat caches as private ([57b7dc2](https://github.com/npm/make-fetch-happen/commit/57b7dc2))


### Features

* **retry:** accept shorthand retry settings ([dfed69d](https://github.com/npm/make-fetch-happen/commit/dfed69d))



<a name="2.1.0"></a>
# [2.1.0](https://github.com/npm/make-fetch-happen/compare/v2.0.4...v2.1.0) (2017-04-09)


### Features

* **cache:** cache now obeys Age and a variety of other things (#13) ([7b9652d](https://github.com/npm/make-fetch-happen/commit/7b9652d))



<a name="2.0.4"></a>
## [2.0.4](https://github.com/npm/make-fetch-happen/compare/v2.0.3...v2.0.4) (2017-04-09)


### Bug Fixes

* **agent:** accept Request as fetch input, not just strings ([b71669a](https://github.com/npm/make-fetch-happen/commit/b71669a))



<a name="2.0.3"></a>
## [2.0.3](https://github.com/npm/make-fetch-happen/compare/v2.0.2...v2.0.3) (2017-04-09)


### Bug Fixes

* **deps:** seriously ([c29e7e7](https://github.com/npm/make-fetch-happen/commit/c29e7e7))



<a name="2.0.2"></a>
## [2.0.2](https://github.com/npm/make-fetch-happen/compare/v2.0.1...v2.0.2) (2017-04-09)


### Bug Fixes

* **deps:** use bundleDeps instead ([c36ebf0](https://github.com/npm/make-fetch-happen/commit/c36ebf0))



<a name="2.0.1"></a>
## [2.0.1](https://github.com/npm/make-fetch-happen/compare/v2.0.0...v2.0.1) (2017-04-09)


### Bug Fixes

* **deps:** make sure node-fetch tarball included in release ([3bf49d1](https://github.com/npm/make-fetch-happen/commit/3bf49d1))



<a name="2.0.0"></a>
# [2.0.0](https://github.com/npm/make-fetch-happen/compare/v1.7.0...v2.0.0) (2017-04-09)


### Bug Fixes

* **deps:** manually pull in newer node-fetch to avoid babel prod dep ([66e5e87](https://github.com/npm/make-fetch-happen/commit/66e5e87))
* **retry:** be more specific about when we retry ([a47b782](https://github.com/npm/make-fetch-happen/commit/a47b782))


### Features

* **agent:** add ca/cert/key support to auto-agent (#15) ([57585a7](https://github.com/npm/make-fetch-happen/commit/57585a7))


### BREAKING CHANGES

* **agent:** pac proxies are no longer supported.
* **retry:** Retry logic has changes.

* 404s, 420s, and 429s all retry now.
* ENOTFOUND no longer retries.
* Only ECONNRESET, ECONNREFUSED, EADDRINUSE, ETIMEDOUT, and `request-timeout` errors are retried.



<a name="1.7.0"></a>
# [1.7.0](https://github.com/npm/make-fetch-happen/compare/v1.6.0...v1.7.0) (2017-04-08)


### Features

* **cache:** add useful headers to inform users about cached data ([9bd7b00](https://github.com/npm/make-fetch-happen/commit/9bd7b00))



<a name="1.6.0"></a>
# [1.6.0](https://github.com/npm/make-fetch-happen/compare/v1.5.1...v1.6.0) (2017-04-06)


### Features

* **agent:** better, keepalive-supporting, default http agents ([16277f6](https://github.com/npm/make-fetch-happen/commit/16277f6))



<a name="1.5.1"></a>
## [1.5.1](https://github.com/npm/make-fetch-happen/compare/v1.5.0...v1.5.1) (2017-04-05)


### Bug Fixes

* **cache:** bump cacache for its fixed error messages ([2f2b916](https://github.com/npm/make-fetch-happen/commit/2f2b916))
* **cache:** fix handling of errors in cache reads ([5729222](https://github.com/npm/make-fetch-happen/commit/5729222))



<a name="1.5.0"></a>
# [1.5.0](https://github.com/npm/make-fetch-happen/compare/v1.4.0...v1.5.0) (2017-04-04)


### Features

* **retry:** retry requests on 408 timeouts, too ([8d8b5bd](https://github.com/npm/make-fetch-happen/commit/8d8b5bd))



<a name="1.4.0"></a>
# [1.4.0](https://github.com/npm/make-fetch-happen/compare/v1.3.1...v1.4.0) (2017-04-04)


### Bug Fixes

* **cache:** stop relying on BB.catch ([2b04494](https://github.com/npm/make-fetch-happen/commit/2b04494))


### Features

* **retry:** report retry attempt number as extra header ([fd50927](https://github.com/npm/make-fetch-happen/commit/fd50927))



<a name="1.3.1"></a>
## [1.3.1](https://github.com/npm/make-fetch-happen/compare/v1.3.0...v1.3.1) (2017-04-04)


### Bug Fixes

* **cache:** pretend cache entry is missing on ENOENT ([9c2bb26](https://github.com/npm/make-fetch-happen/commit/9c2bb26))



<a name="1.3.0"></a>
# [1.3.0](https://github.com/npm/make-fetch-happen/compare/v1.2.1...v1.3.0) (2017-04-04)


### Bug Fixes

* **cache:** if metadata is missing for some odd reason, ignore the entry ([a021a6b](https://github.com/npm/make-fetch-happen/commit/a021a6b))


### Features

* **cache:** add special headers when request was loaded straight from cache ([8a7dbd1](https://github.com/npm/make-fetch-happen/commit/8a7dbd1))
* **cache:** allow configuring algorithms to be calculated on insertion ([bf4a0f2](https://github.com/npm/make-fetch-happen/commit/bf4a0f2))



<a name="1.2.1"></a>
## [1.2.1](https://github.com/npm/make-fetch-happen/compare/v1.2.0...v1.2.1) (2017-04-03)


### Bug Fixes

* **integrity:** update cacache and ssri and change EBADCHECKSUM -> EINTEGRITY ([b6cf6f6](https://github.com/npm/make-fetch-happen/commit/b6cf6f6))



<a name="1.2.0"></a>
# [1.2.0](https://github.com/npm/make-fetch-happen/compare/v1.1.0...v1.2.0) (2017-04-03)


### Features

* **integrity:** full Subresource Integrity support (#10) ([a590159](https://github.com/npm/make-fetch-happen/commit/a590159))



<a name="1.1.0"></a>
# [1.1.0](https://github.com/npm/make-fetch-happen/compare/v1.0.1...v1.1.0) (2017-04-01)


### Features

* **opts:** fetch.defaults() for default options ([522a65e](https://github.com/npm/make-fetch-happen/commit/522a65e))



<a name="1.0.1"></a>
## [1.0.1](https://github.com/npm/make-fetch-happen/compare/v1.0.0...v1.0.1) (2017-04-01)



<a name="1.0.0"></a>
# 1.0.0 (2017-04-01)


### Bug Fixes

* **cache:** default on cache-control header ([b872a2c](https://github.com/npm/make-fetch-happen/commit/b872a2c))
* standard stuff and cache matching ([753f2c2](https://github.com/npm/make-fetch-happen/commit/753f2c2))
* **agent:** nudge around things with opts.agent ([ed62b57](https://github.com/npm/make-fetch-happen/commit/ed62b57))
* **agent:** {agent: false} has special behavior ([b8cc923](https://github.com/npm/make-fetch-happen/commit/b8cc923))
* **cache:** invalidation on non-GET ([fe78fac](https://github.com/npm/make-fetch-happen/commit/fe78fac))
* **cache:** make force-cache and only-if-cached work as expected ([f50e9df](https://github.com/npm/make-fetch-happen/commit/f50e9df))
* **cache:** more spec compliance ([d5a56db](https://github.com/npm/make-fetch-happen/commit/d5a56db))
* **cache:** only cache 200 gets ([0abb25a](https://github.com/npm/make-fetch-happen/commit/0abb25a))
* **cache:** only load cache code if cache opt is a string ([250fcd5](https://github.com/npm/make-fetch-happen/commit/250fcd5))
* **cache:** oops ([e3fa15a](https://github.com/npm/make-fetch-happen/commit/e3fa15a))
* **cache:** refactored warning removal into main file ([5b0a9f9](https://github.com/npm/make-fetch-happen/commit/5b0a9f9))
* **cache:** req constructor no longer needed in Cache ([5b74cbc](https://github.com/npm/make-fetch-happen/commit/5b74cbc))
* **cache:** standard fetch api calls cacheMode "cache" ([6fba805](https://github.com/npm/make-fetch-happen/commit/6fba805))
* **cache:** was using wrong method for non-GET/HEAD cache invalidation ([810763a](https://github.com/npm/make-fetch-happen/commit/810763a))
* **caching:** a bunch of cache-related fixes ([8ebda1d](https://github.com/npm/make-fetch-happen/commit/8ebda1d))
* **deps:** `cacache[@6](https://github.com/6).3.0` - race condition fixes ([9528442](https://github.com/npm/make-fetch-happen/commit/9528442))
* **freshness:** fix regex for cacheControl matching ([070db86](https://github.com/npm/make-fetch-happen/commit/070db86))
* **freshness:** fixed default freshness heuristic value ([5d29e88](https://github.com/npm/make-fetch-happen/commit/5d29e88))
* **logging:** remove console.log calls ([a1d0a47](https://github.com/npm/make-fetch-happen/commit/a1d0a47))
* **method:** node-fetch guarantees uppercase ([a1d68d6](https://github.com/npm/make-fetch-happen/commit/a1d68d6))
* **opts:** simplified opts handling ([516fd6e](https://github.com/npm/make-fetch-happen/commit/516fd6e))
* **proxy:** pass proxy option directly to ProxyAgent ([3398460](https://github.com/npm/make-fetch-happen/commit/3398460))
* **retry:** false -> {retries: 0} ([297fbb6](https://github.com/npm/make-fetch-happen/commit/297fbb6))
* **retry:** only retry put if body is not a stream ([a24e599](https://github.com/npm/make-fetch-happen/commit/a24e599))
* **retry:** skip retries if body is a stream for ANY method ([780c0f8](https://github.com/npm/make-fetch-happen/commit/780c0f8))


### Features

* **api:** initial implementation -- can make and cache requests ([7d55b49](https://github.com/npm/make-fetch-happen/commit/7d55b49))
* **fetch:** injectable cache, and retry support ([87b84bf](https://github.com/npm/make-fetch-happen/commit/87b84bf))


### BREAKING CHANGES

* **cache:** opts.cache -> opts.cacheManager; opts.cacheMode -> opts.cache
* **fetch:** opts.cache accepts a Cache-like obj or a path. Requests are now retried.
* **api:** actual api implemented
