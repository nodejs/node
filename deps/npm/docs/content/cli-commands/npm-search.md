---
section: cli-commands 
title: npm-search
description: Search for packages
---

# npm-search(1)

## Search for packages

### Synopsis

```bash
npm search [-l|--long] [--json] [--parseable] [--no-description] [search terms ...]

aliases: s, se, find
```

### Description

Search the registry for packages matching the search terms. `npm search`
performs a linear, incremental, lexically-ordered search through package
metadata for all files in the registry. If color is enabled, it will further
highlight the matches in the results.

Additionally, using the `--searchopts` and `--searchexclude` options paired with
more search terms will respectively include and exclude further patterns. The
main difference between `--searchopts` and the standard search terms is that the
former does not highlight results in the output and can be used for more
fine-grained filtering. Additionally, both of these can be added to `.npmrc` for
default search filtering behavior.

Search also allows targeting of maintainers in search results, by prefixing
their npm username with `=`.

If a term starts with `/`, then it's interpreted as a regular expression and
supports standard JavaScript RegExp syntax. A trailing `/` will be ignored in
this case. (Note that many regular expression characters must be escaped or
quoted in most shells.)

### A Note on caching

### Configuration

#### description

* Default: true
* Type: Boolean

Used as `--no-description`, disables search matching in package descriptions and
suppresses display of that field in results.

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
lines. When disabled (default) search results are truncated to fit
neatly on a single line. Modules with extremely long names will
fall on multiple lines.

#### searchopts

* Default: ""
* Type: String

Space-separated options that are always passed to search.

#### searchexclude

* Default: ""
* Type: String

Space-separated options that limit the results from search.

#### searchstaleness

* Default: 900 (15 minutes)
* Type: Number

The age of the cache, in seconds, before another registry request is made.

#### registry

 * Default: https://registry.npmjs.org/
 * Type: url

Search the specified registry for modules. If you have configured npm to point
to a different default registry, such as your internal private module
repository, `npm search` will default to that registry when searching. Pass a
different registry url such as the default above in order to override this
setting.

### See Also

* [npm registry](/using-npm/registry)
* [npm config](/cli-commands/npm-config)
* [npmrc](/configuring-npm/npmrc)
* [npm view](/cli-commands/npm-view)
