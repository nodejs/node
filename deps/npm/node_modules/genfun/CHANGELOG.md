# Change Log

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

<a name="5.0.0"></a>
# [5.0.0](https://github.com/zkat/genfun/compare/v4.0.1...v5.0.0) (2017-12-12)


### Bug Fixes

* **license:** relicense to MIT ([857e720](https://github.com/zkat/genfun/commit/857e720))
* **platforms:** drop support for node 4 and 7 ([2cdbe32](https://github.com/zkat/genfun/commit/2cdbe32))


### BREAKING CHANGES

* **platforms:** node 4 and node 7 are no longer officially supported
* **license:** license changed from CC0-1.0 to MIT



<a name="4.0.1"></a>
## [4.0.1](https://github.com/zkat/genfun/compare/v4.0.0...v4.0.1) (2017-04-16)


### Bug Fixes

* **cache:** stop side-effecting cached applicableMethods ([09cee84](https://github.com/zkat/genfun/commit/09cee84))



<a name="4.0.0"></a>
# [4.0.0](https://github.com/zkat/genfun/compare/v3.2.1...v4.0.0) (2017-04-16)


### Bug Fixes

* **genfun:** make internal properties private ([e855c72](https://github.com/zkat/genfun/commit/e855c72))
* **perf:** short-circuit default methods ([7a9b06b](https://github.com/zkat/genfun/commit/7a9b06b))


### Features

* **addMethod:** default-method shortcut syntax for gf.add ([40a3ebb](https://github.com/zkat/genfun/commit/40a3ebb))
* **genfun:** default method and name opts + default shortcut ([0a40939](https://github.com/zkat/genfun/commit/0a40939))
* **genfun:** now with inheritance! ([74abcc2](https://github.com/zkat/genfun/commit/74abcc2))
* **nextMethod:** arg-based nextMethod calls ([17a0b35](https://github.com/zkat/genfun/commit/17a0b35))
* **noNext:** allow users to disable nextMethod functionality ([cc00d95](https://github.com/zkat/genfun/commit/cc00d95))


### BREAKING CHANGES

* **nextMethod:** next methods are now passed in as arguments. context/callNextMethod/etc are all gone.
