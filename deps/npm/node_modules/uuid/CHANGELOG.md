# Change Log

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

<a name="3.2.1"></a>
## [3.2.1](https://github.com/kelektiv/node-uuid/compare/v3.1.0...v3.2.1) (2018-01-16)


### Bug Fixes

* use msCrypto if available. Fixes [#241](https://github.com/kelektiv/node-uuid/issues/241) ([#247](https://github.com/kelektiv/node-uuid/issues/247)) ([1fef18b](https://github.com/kelektiv/node-uuid/commit/1fef18b))



<a name="3.2.0"></a>
# [3.2.0](https://github.com/kelektiv/node-uuid/compare/v3.1.0...v3.2.0) (2018-01-16)


### Bug Fixes

* remove mistakenly added typescript dependency, rollback version (standard-version will auto-increment) ([09fa824](https://github.com/kelektiv/node-uuid/commit/09fa824))
* use msCrypto if available. Fixes [#241](https://github.com/kelektiv/node-uuid/issues/241) ([#247](https://github.com/kelektiv/node-uuid/issues/247)) ([1fef18b](https://github.com/kelektiv/node-uuid/commit/1fef18b))


### Features

* Add v3 Support ([#217](https://github.com/kelektiv/node-uuid/issues/217)) ([d94f726](https://github.com/kelektiv/node-uuid/commit/d94f726))



# 3.0.1 (2016-11-28)

  * split uuid versions into separate files

# 3.0.0 (2016-11-17)

  * remove .parse and .unparse

# 2.0.0

  * Removed uuid.BufferClass

# 1.4.0

  * Improved module context detection
  * Removed public RNG functions

# 1.3.2

  * Improve tests and handling of v1() options (Issue #24)
  * Expose RNG option to allow for perf testing with different generators

# 1.3.0

  * Support for version 1 ids, thanks to [@ctavan](https://github.com/ctavan)!
  * Support for node.js crypto API
  * De-emphasizing performance in favor of a) cryptographic quality PRNGs where available and b) more manageable code
