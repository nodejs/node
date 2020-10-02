# Change Log

## v4.0

* Remove `checkCycle` and `checkGit`, as they are no longer used in npm v7
* Synchronous throw-or-return API instead of taking a callback needlessly
* Modernize code and drop support for node versions less than 10

## v3 2016-01-12

* Change error messages to be more informative.
* checkEngine, when not in strict mode, now calls back with the error
  object as the second argument instead of warning.
* checkCycle no longer logs when cycle errors are found.

## v2 2015-01-20

* Remove checking of engineStrict in the package.json
