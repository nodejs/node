# Changelog

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

## [8.0.0](https://github.com/npm/npm-package-arg/compare/v7.0.0...v8.0.0) (2019-12-15)


### ⚠ BREAKING CHANGES

* Dropping support for node 6 and 8.  It'll probably
still work on those versions, but they are no longer supported or
tested, since npm v7 is moving away from them.

* drop support for node 6 and 8 ([ba85e68](https://github.com/npm/npm-package-arg/commit/ba85e68555d6270f672c3d59da17672f744d0376))

<a name="7.0.0"></a>
# [7.0.0](https://github.com/npm/npm-package-arg/compare/v6.1.1...v7.0.0) (2019-11-11)


### deps

* bump hosted-git-info to 3.0.2 ([68a4fc3](https://github.com/npm/npm-package-arg/commit/68a4fc3)), closes [/github.com/npm/hosted-git-info/pull/38#issuecomment-520243803](https://github.com//github.com/npm/hosted-git-info/pull/38/issues/issuecomment-520243803)


### BREAKING CHANGES

* this drops support for ancient node versions.



<a name="6.1.1"></a>
## [6.1.1](https://github.com/npm/npm-package-arg/compare/v6.1.0...v6.1.1) (2019-08-21)


### Bug Fixes

* preserve drive letter on windows git file:// urls ([3909203](https://github.com/npm/npm-package-arg/commit/3909203))



<a name="6.1.0"></a>
# [6.1.0](https://github.com/npm/npm-package-arg/compare/v6.0.0...v6.1.0) (2018-04-10)


### Bug Fixes

* **git:** Fix gitRange for git+ssh for private git ([#33](https://github.com/npm/npm-package-arg/issues/33)) ([647a0b3](https://github.com/npm/npm-package-arg/commit/647a0b3))


### Features

* **alias:** add `npm:` registry alias spec ([#34](https://github.com/npm/npm-package-arg/issues/34)) ([ab99f8e](https://github.com/npm/npm-package-arg/commit/ab99f8e))
