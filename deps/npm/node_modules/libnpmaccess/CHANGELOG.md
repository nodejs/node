# Change Log

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

<a name="3.0.2"></a>
## [3.0.2](https://github.com/npm/libnpmaccess/compare/v3.0.1...v3.0.2) (2019-07-16)



<a name="3.0.1"></a>
## [3.0.1](https://github.com/npm/libnpmaccess/compare/v3.0.0...v3.0.1) (2018-11-12)


### Bug Fixes

* **ls-packages:** fix confusing splitEntity arg check ([1769090](https://github.com/npm/libnpmaccess/commit/1769090))



<a name="3.0.0"></a>
# [3.0.0](https://github.com/npm/libnpmaccess/compare/v2.0.1...v3.0.0) (2018-08-24)


### Features

* **api:** overhaul API ergonomics ([1faf00a](https://github.com/npm/libnpmaccess/commit/1faf00a))


### BREAKING CHANGES

* **api:** all API calls where scope and team were separate, or
where team was an extra, optional argument should now use a
fully-qualified team name instead, in the `scope:team` format.



<a name="2.0.1"></a>
## [2.0.1](https://github.com/npm/libnpmaccess/compare/v2.0.0...v2.0.1) (2018-08-24)



<a name="2.0.0"></a>
# [2.0.0](https://github.com/npm/libnpmaccess/compare/v1.2.2...v2.0.0) (2018-08-21)


### Bug Fixes

* **json:** stop trying to parse response JSON ([20fdd84](https://github.com/npm/libnpmaccess/commit/20fdd84))
* **lsPackages:** team URL was wrong D: ([b52201c](https://github.com/npm/libnpmaccess/commit/b52201c))


### BREAKING CHANGES

* **json:** use cases where registries were returning JSON
strings in the response body will no longer have an effect. All
API functions except for lsPackages and lsCollaborators will return
`true` on completion.



<a name="1.2.2"></a>
## [1.2.2](https://github.com/npm/libnpmaccess/compare/v1.2.1...v1.2.2) (2018-08-20)


### Bug Fixes

* **docs:** tiny doc hiccup fix ([106396f](https://github.com/npm/libnpmaccess/commit/106396f))



<a name="1.2.1"></a>
## [1.2.1](https://github.com/npm/libnpmaccess/compare/v1.2.0...v1.2.1) (2018-08-20)


### Bug Fixes

* **docs:** document the stream interfaces ([c435aa2](https://github.com/npm/libnpmaccess/commit/c435aa2))



<a name="1.2.0"></a>
# [1.2.0](https://github.com/npm/libnpmaccess/compare/v1.1.0...v1.2.0) (2018-08-20)


### Bug Fixes

* **readme:** fix up appveyor badge url ([42b45a1](https://github.com/npm/libnpmaccess/commit/42b45a1))


### Features

* **streams:** add streaming result support for lsPkg and lsCollab ([0f06f46](https://github.com/npm/libnpmaccess/commit/0f06f46))



<a name="1.1.0"></a>
# [1.1.0](https://github.com/npm/libnpmaccess/compare/v1.0.0...v1.1.0) (2018-08-17)


### Bug Fixes

* **2fa:** escape package names correctly ([f2d83fe](https://github.com/npm/libnpmaccess/commit/f2d83fe))
* **grant:** fix permissions validation ([07f7435](https://github.com/npm/libnpmaccess/commit/07f7435))
* **ls-collaborators:** fix package name escaping + query ([3c02858](https://github.com/npm/libnpmaccess/commit/3c02858))
* **ls-packages:** add query + fix fallback request order ([bdc4791](https://github.com/npm/libnpmaccess/commit/bdc4791))
* **node6:** stop using Object.entries() ([4fec03c](https://github.com/npm/libnpmaccess/commit/4fec03c))
* **public/restricted:** body should be string, not bool ([cffc727](https://github.com/npm/libnpmaccess/commit/cffc727))
* **readme:** fix up title and badges ([2bd6113](https://github.com/npm/libnpmaccess/commit/2bd6113))
* **specs:** require specs to be registry specs ([7892891](https://github.com/npm/libnpmaccess/commit/7892891))


### Features

* **test:** add 100% coverage test suite ([22b5dec](https://github.com/npm/libnpmaccess/commit/22b5dec))



<a name="1.0.0"></a>
# 1.0.0 (2018-08-17)


### Bug Fixes

* **test:** -100 is apparently bad now ([a5ab879](https://github.com/npm/libnpmaccess/commit/a5ab879))


### Features

* **impl:** initial implementation of api ([7039390](https://github.com/npm/libnpmaccess/commit/7039390))
