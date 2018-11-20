# Change Log

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

<a name="3.0.0"></a>
# [3.0.0](https://github.com/zkat/make-fetch-happen/compare/v2.6.0...v3.0.0) (2018-03-12)


### Bug Fixes

* **license:** switch to ISC ([#49](https://github.com/zkat/make-fetch-happen/issues/49)) ([bf90c6d](https://github.com/zkat/make-fetch-happen/commit/bf90c6d))
* **standard:** standard@11 update ([ff0aa70](https://github.com/zkat/make-fetch-happen/commit/ff0aa70))


### BREAKING CHANGES

* **license:** license changed from CC0 to ISC.



<a name="2.6.0"></a>
# [2.6.0](https://github.com/zkat/make-fetch-happen/compare/v2.5.0...v2.6.0) (2017-11-14)


### Bug Fixes

* **integrity:** disable node-fetch compress when checking integrity (#42) ([a7cc74c](https://github.com/zkat/make-fetch-happen/commit/a7cc74c))


### Features

* **onretry:** Add `options.onRetry` (#48) ([f90ccff](https://github.com/zkat/make-fetch-happen/commit/f90ccff))



<a name="2.5.0"></a>
# [2.5.0](https://github.com/zkat/make-fetch-happen/compare/v2.4.13...v2.5.0) (2017-08-24)


### Bug Fixes

* **agent:** support timeout durations greater than 30 seconds ([04875ae](https://github.com/zkat/make-fetch-happen/commit/04875ae)), closes [#35](https://github.com/zkat/make-fetch-happen/issues/35)


### Features

* **cache:** export cache deletion functionality (#40) ([3da4250](https://github.com/zkat/make-fetch-happen/commit/3da4250))



<a name="2.4.13"></a>
## [2.4.13](https://github.com/zkat/make-fetch-happen/compare/v2.4.12...v2.4.13) (2017-06-29)


### Bug Fixes

* **deps:** bump other deps for bugfixes ([eab8297](https://github.com/zkat/make-fetch-happen/commit/eab8297))
* **proxy:** bump proxy deps with bugfixes (#32) ([632f860](https://github.com/zkat/make-fetch-happen/commit/632f860)), closes [#32](https://github.com/zkat/make-fetch-happen/issues/32)



<a name="2.4.12"></a>
## [2.4.12](https://github.com/zkat/make-fetch-happen/compare/v2.4.11...v2.4.12) (2017-06-06)


### Bug Fixes

* **cache:** encode x-local-cache-etc headers to be header-safe ([dc9fb1b](https://github.com/zkat/make-fetch-happen/commit/dc9fb1b))



<a name="2.4.11"></a>
## [2.4.11](https://github.com/zkat/make-fetch-happen/compare/v2.4.10...v2.4.11) (2017-06-05)


### Bug Fixes

* **deps:** bump deps with ssri fix ([bef1994](https://github.com/zkat/make-fetch-happen/commit/bef1994))



<a name="2.4.10"></a>
## [2.4.10](https://github.com/zkat/make-fetch-happen/compare/v2.4.9...v2.4.10) (2017-05-31)


### Bug Fixes

* **deps:** bump dep versions with bugfixes ([0af4003](https://github.com/zkat/make-fetch-happen/commit/0af4003))
* **proxy:** use auth parameter for proxy authentication (#30) ([c687306](https://github.com/zkat/make-fetch-happen/commit/c687306))



<a name="2.4.9"></a>
## [2.4.9](https://github.com/zkat/make-fetch-happen/compare/v2.4.8...v2.4.9) (2017-05-25)


### Bug Fixes

* **cache:** use the passed-in promise for resolving cache stuff ([4c46257](https://github.com/zkat/make-fetch-happen/commit/4c46257))



<a name="2.4.8"></a>
## [2.4.8](https://github.com/zkat/make-fetch-happen/compare/v2.4.7...v2.4.8) (2017-05-25)


### Bug Fixes

* **cache:** pass uid/gid/Promise through to cache ([a847c92](https://github.com/zkat/make-fetch-happen/commit/a847c92))



<a name="2.4.7"></a>
## [2.4.7](https://github.com/zkat/make-fetch-happen/compare/v2.4.6...v2.4.7) (2017-05-24)


### Bug Fixes

* **deps:** pull in various fixes from deps ([fc2a587](https://github.com/zkat/make-fetch-happen/commit/fc2a587))



<a name="2.4.6"></a>
## [2.4.6](https://github.com/zkat/make-fetch-happen/compare/v2.4.5...v2.4.6) (2017-05-24)


### Bug Fixes

* **proxy:** choose agent for http(s)-proxy by protocol of destUrl ([ea4832a](https://github.com/zkat/make-fetch-happen/commit/ea4832a))
* **proxy:** make socks proxy working ([1de810a](https://github.com/zkat/make-fetch-happen/commit/1de810a))
* **proxy:** revert previous proxy solution ([563b0d8](https://github.com/zkat/make-fetch-happen/commit/563b0d8))



<a name="2.4.5"></a>
## [2.4.5](https://github.com/zkat/make-fetch-happen/compare/v2.4.4...v2.4.5) (2017-05-24)


### Bug Fixes

* **proxy:** use the destination url when determining agent ([1a714e7](https://github.com/zkat/make-fetch-happen/commit/1a714e7))



<a name="2.4.4"></a>
## [2.4.4](https://github.com/zkat/make-fetch-happen/compare/v2.4.3...v2.4.4) (2017-05-23)


### Bug Fixes

* **redirect:** handle redirects explicitly  (#27) ([4c4af54](https://github.com/zkat/make-fetch-happen/commit/4c4af54))



<a name="2.4.3"></a>
## [2.4.3](https://github.com/zkat/make-fetch-happen/compare/v2.4.2...v2.4.3) (2017-05-06)


### Bug Fixes

* **redirect:** redirects now delete authorization if hosts fail to match ([c071805](https://github.com/zkat/make-fetch-happen/commit/c071805))



<a name="2.4.2"></a>
## [2.4.2](https://github.com/zkat/make-fetch-happen/compare/v2.4.1...v2.4.2) (2017-05-04)


### Bug Fixes

* **cache:** reduce race condition window by checking for content ([24544b1](https://github.com/zkat/make-fetch-happen/commit/24544b1))
* **match:** Rewrite the conditional stream logic (#25) ([66bba4b](https://github.com/zkat/make-fetch-happen/commit/66bba4b))



<a name="2.4.1"></a>
## [2.4.1](https://github.com/zkat/make-fetch-happen/compare/v2.4.0...v2.4.1) (2017-04-28)


### Bug Fixes

* **memoization:** missed spots + allow passthrough of memo objs ([ac0cd12](https://github.com/zkat/make-fetch-happen/commit/ac0cd12))



<a name="2.4.0"></a>
# [2.4.0](https://github.com/zkat/make-fetch-happen/compare/v2.3.0...v2.4.0) (2017-04-28)


### Bug Fixes

* **memoize:** cacache had a broken memoizer ([8a9ed4c](https://github.com/zkat/make-fetch-happen/commit/8a9ed4c))


### Features

* **memoization:** only slurp stuff into memory if opts.memoize is not false ([0744adc](https://github.com/zkat/make-fetch-happen/commit/0744adc))



<a name="2.3.0"></a>
# [2.3.0](https://github.com/zkat/make-fetch-happen/compare/v2.2.6...v2.3.0) (2017-04-27)


### Features

* **agent:** added opts.strictSSL and opts.localAddress ([c35015a](https://github.com/zkat/make-fetch-happen/commit/c35015a))
* **proxy:** Added opts.noProxy and NO_PROXY support ([f45c915](https://github.com/zkat/make-fetch-happen/commit/f45c915))



<a name="2.2.6"></a>
## [2.2.6](https://github.com/zkat/make-fetch-happen/compare/v2.2.5...v2.2.6) (2017-04-26)


### Bug Fixes

* **agent:** check uppercase & lowercase proxy env (#24) ([acf2326](https://github.com/zkat/make-fetch-happen/commit/acf2326)), closes [#22](https://github.com/zkat/make-fetch-happen/issues/22)
* **deps:** switch to node-fetch-npm and stop bundling ([3db603b](https://github.com/zkat/make-fetch-happen/commit/3db603b))



<a name="2.2.5"></a>
## [2.2.5](https://github.com/zkat/make-fetch-happen/compare/v2.2.4...v2.2.5) (2017-04-23)


### Bug Fixes

* **deps:** bump cacache and use its size feature ([926c1d3](https://github.com/zkat/make-fetch-happen/commit/926c1d3))



<a name="2.2.4"></a>
## [2.2.4](https://github.com/zkat/make-fetch-happen/compare/v2.2.3...v2.2.4) (2017-04-18)


### Bug Fixes

* **integrity:** hash verification issues fixed ([07f9402](https://github.com/zkat/make-fetch-happen/commit/07f9402))



<a name="2.2.3"></a>
## [2.2.3](https://github.com/zkat/make-fetch-happen/compare/v2.2.2...v2.2.3) (2017-04-18)


### Bug Fixes

* **staleness:** responses older than 8h were never stale :< ([b54dd75](https://github.com/zkat/make-fetch-happen/commit/b54dd75))
* **warning:** remove spurious warning, make format more spec-compliant ([2e4f6bb](https://github.com/zkat/make-fetch-happen/commit/2e4f6bb))



<a name="2.2.2"></a>
## [2.2.2](https://github.com/zkat/make-fetch-happen/compare/v2.2.1...v2.2.2) (2017-04-12)


### Bug Fixes

* **retry:** stop retrying 404s ([6fafd53](https://github.com/zkat/make-fetch-happen/commit/6fafd53))



<a name="2.2.1"></a>
## [2.2.1](https://github.com/zkat/make-fetch-happen/compare/v2.2.0...v2.2.1) (2017-04-10)


### Bug Fixes

* **deps:** move test-only deps to devDeps ([2daaf80](https://github.com/zkat/make-fetch-happen/commit/2daaf80))



<a name="2.2.0"></a>
# [2.2.0](https://github.com/zkat/make-fetch-happen/compare/v2.1.0...v2.2.0) (2017-04-09)


### Bug Fixes

* **cache:** treat caches as private ([57b7dc2](https://github.com/zkat/make-fetch-happen/commit/57b7dc2))


### Features

* **retry:** accept shorthand retry settings ([dfed69d](https://github.com/zkat/make-fetch-happen/commit/dfed69d))



<a name="2.1.0"></a>
# [2.1.0](https://github.com/zkat/make-fetch-happen/compare/v2.0.4...v2.1.0) (2017-04-09)


### Features

* **cache:** cache now obeys Age and a variety of other things (#13) ([7b9652d](https://github.com/zkat/make-fetch-happen/commit/7b9652d))



<a name="2.0.4"></a>
## [2.0.4](https://github.com/zkat/make-fetch-happen/compare/v2.0.3...v2.0.4) (2017-04-09)


### Bug Fixes

* **agent:** accept Request as fetch input, not just strings ([b71669a](https://github.com/zkat/make-fetch-happen/commit/b71669a))



<a name="2.0.3"></a>
## [2.0.3](https://github.com/zkat/make-fetch-happen/compare/v2.0.2...v2.0.3) (2017-04-09)


### Bug Fixes

* **deps:** seriously ([c29e7e7](https://github.com/zkat/make-fetch-happen/commit/c29e7e7))



<a name="2.0.2"></a>
## [2.0.2](https://github.com/zkat/make-fetch-happen/compare/v2.0.1...v2.0.2) (2017-04-09)


### Bug Fixes

* **deps:** use bundleDeps instead ([c36ebf0](https://github.com/zkat/make-fetch-happen/commit/c36ebf0))



<a name="2.0.1"></a>
## [2.0.1](https://github.com/zkat/make-fetch-happen/compare/v2.0.0...v2.0.1) (2017-04-09)


### Bug Fixes

* **deps:** make sure node-fetch tarball included in release ([3bf49d1](https://github.com/zkat/make-fetch-happen/commit/3bf49d1))



<a name="2.0.0"></a>
# [2.0.0](https://github.com/zkat/make-fetch-happen/compare/v1.7.0...v2.0.0) (2017-04-09)


### Bug Fixes

* **deps:** manually pull in newer node-fetch to avoid babel prod dep ([66e5e87](https://github.com/zkat/make-fetch-happen/commit/66e5e87))
* **retry:** be more specific about when we retry ([a47b782](https://github.com/zkat/make-fetch-happen/commit/a47b782))


### Features

* **agent:** add ca/cert/key support to auto-agent (#15) ([57585a7](https://github.com/zkat/make-fetch-happen/commit/57585a7))


### BREAKING CHANGES

* **agent:** pac proxies are no longer supported.
* **retry:** Retry logic has changes.

* 404s, 420s, and 429s all retry now.
* ENOTFOUND no longer retries.
* Only ECONNRESET, ECONNREFUSED, EADDRINUSE, ETIMEDOUT, and `request-timeout` errors are retried.



<a name="1.7.0"></a>
# [1.7.0](https://github.com/zkat/make-fetch-happen/compare/v1.6.0...v1.7.0) (2017-04-08)


### Features

* **cache:** add useful headers to inform users about cached data ([9bd7b00](https://github.com/zkat/make-fetch-happen/commit/9bd7b00))



<a name="1.6.0"></a>
# [1.6.0](https://github.com/zkat/make-fetch-happen/compare/v1.5.1...v1.6.0) (2017-04-06)


### Features

* **agent:** better, keepalive-supporting, default http agents ([16277f6](https://github.com/zkat/make-fetch-happen/commit/16277f6))



<a name="1.5.1"></a>
## [1.5.1](https://github.com/zkat/make-fetch-happen/compare/v1.5.0...v1.5.1) (2017-04-05)


### Bug Fixes

* **cache:** bump cacache for its fixed error messages ([2f2b916](https://github.com/zkat/make-fetch-happen/commit/2f2b916))
* **cache:** fix handling of errors in cache reads ([5729222](https://github.com/zkat/make-fetch-happen/commit/5729222))



<a name="1.5.0"></a>
# [1.5.0](https://github.com/zkat/make-fetch-happen/compare/v1.4.0...v1.5.0) (2017-04-04)


### Features

* **retry:** retry requests on 408 timeouts, too ([8d8b5bd](https://github.com/zkat/make-fetch-happen/commit/8d8b5bd))



<a name="1.4.0"></a>
# [1.4.0](https://github.com/zkat/make-fetch-happen/compare/v1.3.1...v1.4.0) (2017-04-04)


### Bug Fixes

* **cache:** stop relying on BB.catch ([2b04494](https://github.com/zkat/make-fetch-happen/commit/2b04494))


### Features

* **retry:** report retry attempt number as extra header ([fd50927](https://github.com/zkat/make-fetch-happen/commit/fd50927))



<a name="1.3.1"></a>
## [1.3.1](https://github.com/zkat/make-fetch-happen/compare/v1.3.0...v1.3.1) (2017-04-04)


### Bug Fixes

* **cache:** pretend cache entry is missing on ENOENT ([9c2bb26](https://github.com/zkat/make-fetch-happen/commit/9c2bb26))



<a name="1.3.0"></a>
# [1.3.0](https://github.com/zkat/make-fetch-happen/compare/v1.2.1...v1.3.0) (2017-04-04)


### Bug Fixes

* **cache:** if metadata is missing for some odd reason, ignore the entry ([a021a6b](https://github.com/zkat/make-fetch-happen/commit/a021a6b))


### Features

* **cache:** add special headers when request was loaded straight from cache ([8a7dbd1](https://github.com/zkat/make-fetch-happen/commit/8a7dbd1))
* **cache:** allow configuring algorithms to be calculated on insertion ([bf4a0f2](https://github.com/zkat/make-fetch-happen/commit/bf4a0f2))



<a name="1.2.1"></a>
## [1.2.1](https://github.com/zkat/make-fetch-happen/compare/v1.2.0...v1.2.1) (2017-04-03)


### Bug Fixes

* **integrity:** update cacache and ssri and change EBADCHECKSUM -> EINTEGRITY ([b6cf6f6](https://github.com/zkat/make-fetch-happen/commit/b6cf6f6))



<a name="1.2.0"></a>
# [1.2.0](https://github.com/zkat/make-fetch-happen/compare/v1.1.0...v1.2.0) (2017-04-03)


### Features

* **integrity:** full Subresource Integrity support (#10) ([a590159](https://github.com/zkat/make-fetch-happen/commit/a590159))



<a name="1.1.0"></a>
# [1.1.0](https://github.com/zkat/make-fetch-happen/compare/v1.0.1...v1.1.0) (2017-04-01)


### Features

* **opts:** fetch.defaults() for default options ([522a65e](https://github.com/zkat/make-fetch-happen/commit/522a65e))



<a name="1.0.1"></a>
## [1.0.1](https://github.com/zkat/make-fetch-happen/compare/v1.0.0...v1.0.1) (2017-04-01)



<a name="1.0.0"></a>
# 1.0.0 (2017-04-01)


### Bug Fixes

* **cache:** default on cache-control header ([b872a2c](https://github.com/zkat/make-fetch-happen/commit/b872a2c))
* standard stuff and cache matching ([753f2c2](https://github.com/zkat/make-fetch-happen/commit/753f2c2))
* **agent:** nudge around things with opts.agent ([ed62b57](https://github.com/zkat/make-fetch-happen/commit/ed62b57))
* **agent:** {agent: false} has special behavior ([b8cc923](https://github.com/zkat/make-fetch-happen/commit/b8cc923))
* **cache:** invalidation on non-GET ([fe78fac](https://github.com/zkat/make-fetch-happen/commit/fe78fac))
* **cache:** make force-cache and only-if-cached work as expected ([f50e9df](https://github.com/zkat/make-fetch-happen/commit/f50e9df))
* **cache:** more spec compliance ([d5a56db](https://github.com/zkat/make-fetch-happen/commit/d5a56db))
* **cache:** only cache 200 gets ([0abb25a](https://github.com/zkat/make-fetch-happen/commit/0abb25a))
* **cache:** only load cache code if cache opt is a string ([250fcd5](https://github.com/zkat/make-fetch-happen/commit/250fcd5))
* **cache:** oops ([e3fa15a](https://github.com/zkat/make-fetch-happen/commit/e3fa15a))
* **cache:** refactored warning removal into main file ([5b0a9f9](https://github.com/zkat/make-fetch-happen/commit/5b0a9f9))
* **cache:** req constructor no longer needed in Cache ([5b74cbc](https://github.com/zkat/make-fetch-happen/commit/5b74cbc))
* **cache:** standard fetch api calls cacheMode "cache" ([6fba805](https://github.com/zkat/make-fetch-happen/commit/6fba805))
* **cache:** was using wrong method for non-GET/HEAD cache invalidation ([810763a](https://github.com/zkat/make-fetch-happen/commit/810763a))
* **caching:** a bunch of cache-related fixes ([8ebda1d](https://github.com/zkat/make-fetch-happen/commit/8ebda1d))
* **deps:** `cacache[@6](https://github.com/6).3.0` - race condition fixes ([9528442](https://github.com/zkat/make-fetch-happen/commit/9528442))
* **freshness:** fix regex for cacheControl matching ([070db86](https://github.com/zkat/make-fetch-happen/commit/070db86))
* **freshness:** fixed default freshness heuristic value ([5d29e88](https://github.com/zkat/make-fetch-happen/commit/5d29e88))
* **logging:** remove console.log calls ([a1d0a47](https://github.com/zkat/make-fetch-happen/commit/a1d0a47))
* **method:** node-fetch guarantees uppercase ([a1d68d6](https://github.com/zkat/make-fetch-happen/commit/a1d68d6))
* **opts:** simplified opts handling ([516fd6e](https://github.com/zkat/make-fetch-happen/commit/516fd6e))
* **proxy:** pass proxy option directly to ProxyAgent ([3398460](https://github.com/zkat/make-fetch-happen/commit/3398460))
* **retry:** false -> {retries: 0} ([297fbb6](https://github.com/zkat/make-fetch-happen/commit/297fbb6))
* **retry:** only retry put if body is not a stream ([a24e599](https://github.com/zkat/make-fetch-happen/commit/a24e599))
* **retry:** skip retries if body is a stream for ANY method ([780c0f8](https://github.com/zkat/make-fetch-happen/commit/780c0f8))


### Features

* **api:** initial implementation -- can make and cache requests ([7d55b49](https://github.com/zkat/make-fetch-happen/commit/7d55b49))
* **fetch:** injectable cache, and retry support ([87b84bf](https://github.com/zkat/make-fetch-happen/commit/87b84bf))


### BREAKING CHANGES

* **cache:** opts.cache -> opts.cacheManager; opts.cacheMode -> opts.cache
* **fetch:** opts.cache accepts a Cache-like obj or a path. Requests are now retried.
* **api:** actual api implemented
