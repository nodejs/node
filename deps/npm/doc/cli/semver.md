npm-semver(1) -- The semantic versioner for npm
===============================================

## SYNOPSIS

The npm semantic versioning utility.

## DESCRIPTION

As a node module:

    $ npm install semver

    semver.valid('1.2.3') // '1.2.3'
    semver.valid('a.b.c') // null
    semver.clean('  =v1.2.3   ') // '1.2.3'
    semver.satisfies('1.2.3', '1.x || >=2.5.0 || 5.0.0 - 7.2.3') // true
    semver.gt('1.2.3', '9.8.7') // false
    semver.lt('1.2.3', '9.8.7') // true

As a command-line utility:

    $ npm install semver -g
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

A version is the following things, in this order:

* a number (Major)
* a period
* a number (minor)
* a period
* a number (patch)
* OPTIONAL: a hyphen, followed by a number (build)
* OPTIONAL: a collection of pretty much any non-whitespace characters
  (tag)

A leading `"="` or `"v"` character is stripped off and ignored.

## Comparisons

The ordering of versions is done using the following algorithm, given
two versions and asked to find the greater of the two:

* If the majors are numerically different, then take the one
  with a bigger major number. `2.3.4 > 1.3.4`
* If the minors are numerically different, then take the one
  with the bigger minor number. `2.3.4 > 2.2.4`
* If the patches are numerically different, then take the one with the
  bigger patch number. `2.3.4 > 2.3.3`
* If only one of them has a build number, then take the one with the
  build number.  `2.3.4-0 > 2.3.4`
* If they both have build numbers, and the build numbers are numerically
  different, then take the one with the bigger build number.
  `2.3.4-10 > 2.3.4-9`
* If only one of them has a tag, then take the one without the tag.
  `2.3.4 > 2.3.4-beta`
* If they both have tags, then take the one with the lexicographically
  larger tag.  `2.3.4-beta > 2.3.4-alpha`
* At this point, they're equal.

## Ranges

The following range styles are supported:

* `>1.2.3` Greater than a specific version.
* `<1.2.3` Less than
* `1.2.3 - 2.3.4` := `>=1.2.3 <=2.3.4`
* `~1.2.3` := `>=1.2.3 <1.3.0`
* `~1.2` := `>=1.2.0 <1.3.0`
* `~1` := `>=1.0.0 <2.0.0`
* `1.2.x` := `>=1.2.0 <1.3.0`
* `1.x` := `>=1.0.0 <2.0.0`

Ranges can be joined with either a space (which implies "and") or a
`||` (which implies "or").

## Functions

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

## SEE ALSO

* npm-json(1)
