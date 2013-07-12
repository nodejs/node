semver(1) -- The semantic versioner for npm
===========================================

## Usage

    $ npm install semver

    semver.valid('1.2.3') // '1.2.3'
    semver.valid('a.b.c') // null
    semver.clean('  =v1.2.3   ') // '1.2.3'
    semver.satisfies('1.2.3', '1.x || >=2.5.0 || 5.0.0 - 7.2.3') // true
    semver.gt('1.2.3', '9.8.7') // false
    semver.lt('1.2.3', '9.8.7') // true

As a command-line utility:

    $ semver -h

    Usage: semver -v <version> [-r <range>]
    Test if version(s) satisfy the supplied range(s),
    and sort them.

    Multiple versions or ranges may be supplied.

    Program exits successfully if any valid version satisfies
    all supplied ranges, and prints all satisfying versions.

    If no versions are valid, or ranges are not satisfied,
    then exits failure.

    Versions are printed in ascending order, so supplying
    multiple versions to the utility will just sort them.

## Versions

A "version" is described by the v2.0.0 specification found at
<http://semver.org/>.

A leading `"="` or `"v"` character is stripped off and ignored.

## Ranges

The following range styles are supported:

* `1.2.3` A specific version.  When nothing else will do.  Note that
  build metadata is still ignored, so `1.2.3+build2012` will satisfy
  this range.
* `>1.2.3` Greater than a specific version.
* `<1.2.3` Less than a specific version.  If there is no prerelease
  tag on the version range, then no prerelease version will be allowed
  either, even though these are technically "less than".
* `>=1.2.3` Greater than or equal to.  Note that prerelease versions
  are NOT equal to their "normal" equivalents, so `1.2.3-beta` will
  not satisfy this range, but `2.3.0-beta` will.
* `<=1.2.3` Less than or equal to.  In this case, prerelease versions
  ARE allowed, so `1.2.3-beta` would satisfy.
* `1.2.3 - 2.3.4` := `>=1.2.3 <=2.3.4`
* `~1.2.3` := `>=1.2.3-0 <1.3.0-0`  "Reasonably close to 1.2.3".  When
  using tilde operators, prerelease versions are supported as well,
  but a prerelease of the next significant digit will NOT be
  satisfactory, so `1.3.0-beta` will not satisfy `~1.2.3`.
* `~1.2` := `>=1.2.0-0 <1.3.0-0` "Any version starting with 1.2"
* `1.2.x` := `>=1.2.0-0 <1.3.0-0` "Any version starting with 1.2"
* `~1` := `>=1.0.0-0 <2.0.0-0` "Any version starting with 1"
* `1.x` := `>=1.0.0-0 <2.0.0-0` "Any version starting with 1"


Ranges can be joined with either a space (which implies "and") or a
`||` (which implies "or").

## Functions

All methods and classes take a final `loose` boolean argument that, if
true, will be more forgiving about not-quite-valid semver strings.
The resulting output will always be 100% strict, of course.

Strict-mode Comparators and Ranges will be strict about the SemVer
strings that they parse.

* valid(v): Return the parsed version, or null if it's not valid.
* inc(v, release): Return the version incremented by the release type
  (major, minor, patch, or build), or null if it's not valid.

### Comparison

* gt(v1, v2): `v1 > v2`
* gte(v1, v2): `v1 >= v2`
* lt(v1, v2): `v1 < v2`
* lte(v1, v2): `v1 <= v2`
* eq(v1, v2): `v1 == v2` This is true if they're logically equivalent,
  even if they're not the exact same string.  You already know how to
  compare strings.
* neq(v1, v2): `v1 != v2` The opposite of eq.
* cmp(v1, comparator, v2): Pass in a comparison string, and it'll call
  the corresponding function above.  `"==="` and `"!=="` do simple
  string comparison, but are included for completeness.  Throws if an
  invalid comparison string is provided.
* compare(v1, v2): Return 0 if v1 == v2, or 1 if v1 is greater, or -1 if
  v2 is greater.  Sorts in ascending order if passed to Array.sort().
* rcompare(v1, v2): The reverse of compare.  Sorts an array of versions
  in descending order when passed to Array.sort().


### Ranges

* validRange(range): Return the valid range or null if it's not valid
* satisfies(version, range): Return true if the version satisfies the
  range.
* maxSatisfying(versions, range): Return the highest version in the list
  that satisfies the range, or null if none of them do.
