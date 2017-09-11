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

