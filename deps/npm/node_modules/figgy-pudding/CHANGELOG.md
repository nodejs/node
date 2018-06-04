# Change Log

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

<a name="3.1.0"></a>
# [3.1.0](https://github.com/zkat/figgy-pudding/compare/v3.0.0...v3.1.0) (2018-04-08)


### Features

* **opts:** allow direct option fetching without .get() ([ca77aad](https://github.com/zkat/figgy-pudding/commit/ca77aad))



<a name="3.0.0"></a>
# [3.0.0](https://github.com/zkat/figgy-pudding/compare/v2.0.1...v3.0.0) (2018-04-06)


### Bug Fixes

* **ci:** oops -- forgot to update CI config ([7a40563](https://github.com/zkat/figgy-pudding/commit/7a40563))
* **get:** make provider lookup order like Object.assign ([33ff89b](https://github.com/zkat/figgy-pudding/commit/33ff89b))


### Features

* **concat:** add .concat() method to opts ([d310fce](https://github.com/zkat/figgy-pudding/commit/d310fce))


### meta

* drop support for node@4 and node@7 ([9f8a61c](https://github.com/zkat/figgy-pudding/commit/9f8a61c))


### BREAKING CHANGES

* node@4 and node@7 are no longer supported
* **get:** shadow order for properties in providers is reversed



<a name="2.0.1"></a>
## [2.0.1](https://github.com/zkat/figgy-pudding/compare/v2.0.0...v2.0.1) (2018-03-16)


### Bug Fixes

* **opts:** ignore non-object providers ([7b9c0f8](https://github.com/zkat/figgy-pudding/commit/7b9c0f8))



<a name="2.0.0"></a>
# [2.0.0](https://github.com/zkat/figgy-pudding/compare/v1.0.0...v2.0.0) (2018-03-16)


### Features

* **api:** overhauled API with new opt handling concept ([e6cc929](https://github.com/zkat/figgy-pudding/commit/e6cc929))
* **license:** relicense to ISC ([87479aa](https://github.com/zkat/figgy-pudding/commit/87479aa))


### BREAKING CHANGES

* **license:** the license has been changed from CC0-1.0 to ISC.
* **api:** this is a completely different approach than previously
used by this library. See the readme for the new API and an explanation.
