# Change Log

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

<a name="1.3.2"></a>
## [1.3.2](https://github.com/npm/npm-audit-report/compare/v1.3.1...v1.3.2) (2018-12-18)


### Bug Fixes

* **parseable:** add support for critical vulns and more resolves on update/install action ([#28](https://github.com/npm/npm-audit-report/issues/28)) ([5e27893](https://github.com/npm/npm-audit-report/commit/5e27893))
* **security:** audit fix ([ff9faf3](https://github.com/npm/npm-audit-report/commit/ff9faf3))
* **urls:** Replace hardcoded URL to advisory with a URL from audit response ([#34](https://github.com/npm/npm-audit-report/issues/34)) ([e2fe95b](https://github.com/npm/npm-audit-report/commit/e2fe95b))



<a name="1.3.1"></a>
## [1.3.1](https://github.com/npm/npm-audit-report/compare/v1.3.0...v1.3.1) (2018-07-10)



<a name="1.3.0"></a>
# [1.3.0](https://github.com/npm/npm-audit-report/compare/v1.2.1...v1.3.0) (2018-07-09)


### Bug Fixes

* **deps:** remove object.values dependency ([2c5374a](https://github.com/npm/npm-audit-report/commit/2c5374a))
* **detail:** Fix info-level severity ([#18](https://github.com/npm/npm-audit-report/issues/18)) ([807db5a](https://github.com/npm/npm-audit-report/commit/807db5a))
* **tests:** a test should not cause side-effects in other tests ([#23](https://github.com/npm/npm-audit-report/issues/23)) ([a94449f](https://github.com/npm/npm-audit-report/commit/a94449f))


### Features

* **output:** add `parseable` tabular output format support ([#21](https://github.com/npm/npm-audit-report/issues/21)) ([1c9aaf4](https://github.com/npm/npm-audit-report/commit/1c9aaf4))



<a name="1.2.1"></a>
## [1.2.1](https://github.com/npm/npm-audit-report/compare/v1.2.0...v1.2.1) (2018-05-17)


### Bug Fixes

* **detail:** count id+path instead of just id ([99880fd](https://github.com/npm/npm-audit-report/commit/99880fd))



<a name="1.2.0"></a>
# [1.2.0](https://github.com/npm/npm-audit-report/compare/v1.1.0...v1.2.0) (2018-05-16)


### Bug Fixes

* **full-report:** Fix install flag for devDependencies ([#14](https://github.com/npm/npm-audit-report/issues/14)) ([30e5f30](https://github.com/npm/npm-audit-report/commit/30e5f30))


### Features

* **detail:** consistified full report with install report ([#15](https://github.com/npm/npm-audit-report/issues/15)) ([6df6810](https://github.com/npm/npm-audit-report/commit/6df6810))
* **install:** include `npm audit` recommendation too ([32fb153](https://github.com/npm/npm-audit-report/commit/32fb153))



<a name="1.1.0"></a>
# [1.1.0](https://github.com/npm/npm-audit-report/compare/v1.0.9...v1.1.0) (2018-05-10)


### Bug Fixes

* **install:** not enough data for this conditional ([6ddc30c](https://github.com/npm/npm-audit-report/commit/6ddc30c))


### Features

* **report:** compress and reformat human-readable install report ([74d5203](https://github.com/npm/npm-audit-report/commit/74d5203))
