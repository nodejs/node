# Change Log

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

<a name="2.2.1"></a>
## [2.2.1](https://github.com/npm/gentle-fs/compare/v2.2.0...v2.2.1) (2019-08-15)


### Bug Fixes

* **link:** properly detect that we should chown the link ([1c69beb](https://github.com/npm/gentle-fs/commit/1c69beb))



<a name="2.2.0"></a>
# [2.2.0](https://github.com/npm/gentle-fs/compare/v2.1.0...v2.2.0) (2019-08-14)


### Bug Fixes

* don't chown if we didn't make any dirs ([c4df8a8](https://github.com/npm/gentle-fs/commit/c4df8a8))


### Features

* export mkdir method ([4891c09](https://github.com/npm/gentle-fs/commit/4891c09))



<a name="2.1.0"></a>
# [2.1.0](https://github.com/npm/gentle-fs/compare/v2.0.1...v2.1.0) (2019-08-14)


### Features

* infer ownership of created dirs and links ([0dd2879](https://github.com/npm/gentle-fs/commit/0dd2879))



<a name="2.0.1"></a>
## [2.0.1](https://github.com/npm/gentle-fs/compare/v2.0.0...v2.0.1) (2017-11-28)


### Bug Fixes

* **pkglock:** regenerate lock to fix version issues ([a41cbd0](https://github.com/npm/gentle-fs/commit/a41cbd0))
* **rm:** make sure logic for vacuuming matches original npm ([673e82c](https://github.com/npm/gentle-fs/commit/673e82c))



<a name="2.0.0"></a>
# [2.0.0](https://github.com/npm/gentle-fs/compare/v1.0.2...v2.0.0) (2017-10-07)


### Features

* **api:** switch to callbacks for now. ([236e886](https://github.com/npm/gentle-fs/commit/236e886))


### BREAKING CHANGES

* **api:** switches from Promises to callbacks for async control flow.



<a name="1.0.2"></a>
## [1.0.2](https://github.com/npm/gentle-fs/compare/v1.0.1...v1.0.2) (2017-10-07)


### Bug Fixes

* **link:** properly resolve linkIfExists promise ([f06acf2](https://github.com/npm/gentle-fs/commit/f06acf2))



<a name="1.0.1"></a>
## [1.0.1](https://github.com/npm/gentle-fs/compare/v1.0.0...v1.0.1) (2017-09-25)


### Bug Fixes

* **files:** include required lib files in tarball ([e0be6a8](https://github.com/npm/gentle-fs/commit/e0be6a8))



<a name="1.0.0"></a>
# 1.0.0 (2017-09-15)


### Bug Fixes

* **docs:** Flesh out API descriptions. ([b056a34](https://github.com/npm/gentle-fs/commit/b056a34))
* **docs:** update usage examples ([5517ff5](https://github.com/npm/gentle-fs/commit/5517ff5))


### BREAKING CHANGES

* **docs:** api docs added
