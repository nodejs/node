---
title: npm-search
section: 1
description: Search for packages
---

### Synopsis

```bash
npm search [-l|--long] [--json] [--parseable] [--no-description] [search terms ...]

aliases: s, se, find
```

### Description

Search the registry for packages matching the search terms. `npm search`
performs a linear, incremental, lexically-ordered search through package
metadata for all files in the registry. If your terminal has color
support, it will further highlight the matches in the results.  This can
be disabled with the config item `color`

Additionally, using the `--searchopts` and `--searchexclude` options
paired with more search terms will include and exclude further patterns.
The main difference between `--searchopts` and the standard search terms
is that the former does not highlight results in the output and you can
use them more fine-grained filtering. Additionally, you can add both of
these to your config to change default search filtering behavior.

Search also allows targeting of maintainers in search results, by prefixing
their npm username with `=`.

If a term starts with `/`, then it's interpreted as a regular expression
and supports standard JavaScript RegExp syntax. In this case search will
ignore a trailing `/` .  (Note you must escape or quote many regular
expression characters in most shells.)

### Configuration

All of the following can be defined in a `.npmrc` file, or passed as
parameters to the cli prefixed with `--` (e.g. `--json`)

#### description

* Default: true
* Type: Boolean

#### color

 * Default: true
 * Type: Boolean

Used as `--no-color`, disables color highlighting of matches in the
results.

#### json

* Default: false
* Type: Boolean

Output search results as a JSON array.

#### parseable

* Default: false
* Type: Boolean

Output search results as lines with tab-separated columns.

#### long

* Default: false
* Type: Boolean

Display full package descriptions and other long text across multiple
lines. When disabled (which is the default) the output will
truncate search results to fit neatly on a single line. Modules with
extremely long names will fall on multiple lines.

#### searchopts

* Default: ""
* Type: String

Space-separated options that are always passed to search.

#### searchexclude

* Default: ""
* Type: String

Space-separated options that limit the results from search.

#### registry

 * Default: https://registry.npmjs.org/
 * Type: url

Search the specified registry for modules. If you have configured npm to
point to a different default registry (such as your internal private
module repository), `npm search` will also default to that registry when
searching.

### A note on caching

The npm cli caches search results with the same terms and options
locally in its cache. You can use the following to change how and when
the cli uses this cache. See [`npm cache`](/commands/npm-cache) for more
on how the cache works.

#### prefer-online

Forces staleness checks for cached searches, making the cli look for
updates immediately even for fresh search results.

#### prefer-offline

Bypasses staleness checks for cached searches.  Missing data will still
be requested from the server. To force full offline mode, use `offline`.

#### offline

Forces full offline mode. Any searches not locally cached will result in
an error.

### See Also

* [npm registry](/using-npm/registry)
* [npm config](/commands/npm-config)
* [npmrc](/configuring-npm/npmrc)
* [npm view](/commands/npm-view)
* [npm cache](/commands/npm-cache)
* https://npm.im/npm-registry-fetch
