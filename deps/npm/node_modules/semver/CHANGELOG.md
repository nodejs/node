# changes log

## 7.3.0

* Add `subset(r1, r2)` method to determine if `r1` range is entirely
  contained by `r2` range.

## 7.2.3

* Fix handling of `includePrelease` mode where version ranges like `1.0.0 -
  2.0.0` would include `3.0.0-pre` and not `1.0.0-pre`.

## 7.2.2

* Fix bug where `2.0.0-pre` would be included in `^1.0.0` if
  `includePrerelease` was set to true.

## 7.2.0

* Add `simplifyRange` method to attempt to generate a more human-readable
  range expression that is equivalent to a supplied range, for a given set
  of versions.

## 7.1.2

* Remove fancy lazy-loading logic, as it was causing problems for webpack
  users.

## 7.1.0

* Add `require('semver/preload')` to load the entire module without using
  lazy getter methods.

## 7.0.0

* Refactor module into separate files for better tree-shaking
* Drop support for very old node versions, use const/let, `=>` functions,
  and classes.

## 6.3.0

* Expose the token enum on the exports

## 6.2.0

* Coerce numbers to strings when passed to semver.coerce()
* Add `rtl` option to coerce from right to left

## 6.1.3

* Handle X-ranges properly in includePrerelease mode

## 6.1.2

* Do not throw when testing invalid version strings

## 6.1.1

* Add options support for semver.coerce()
* Handle undefined version passed to Range.test

## 6.1.0

* Add semver.compareBuild function
* Support `*` in semver.intersects

## 6.0

* Fix `intersects` logic.

    This is technically a bug fix, but since it is also a change to behavior
    that may require users updating their code, it is marked as a major
    version increment.

## 5.7

* Add `minVersion` method

## 5.6

* Move boolean `loose` param to an options object, with
  backwards-compatibility protection.
* Add ability to opt out of special prerelease version handling with
  the `includePrerelease` option flag.

## 5.5

* Add version coercion capabilities

## 5.4

* Add intersection checking

## 5.3

* Add `minSatisfying` method

## 5.2

* Add `prerelease(v)` that returns prerelease components

## 5.1

* Add Backus-Naur for ranges
* Remove excessively cute inspection methods

## 5.0

* Remove AMD/Browserified build artifacts
* Fix ltr and gtr when using the `*` range
* Fix for range `*` with a prerelease identifier
