npm-search(1) -- Search for packages
====================================

## SYNOPSIS

    npm search [--long] [search terms ...]

    aliases: s, se

## DESCRIPTION

Search the registry for packages matching the search terms.

If a term starts with `/`, then it's interpreted as a regular expression.
A trailing `/` will be ignored in this case.  (Note that many regular
expression characters must be escaped or quoted in most shells.)

## CONFIGURATION

### long

* Default: false
* Type: Boolean

Display full package descriptions and other long text across multiple
lines. When disabled (default) search results are truncated to fit
neatly on a single line. Modules with extremely long names will
fall on multiple lines.

## SEE ALSO

* npm-registry(7)
* npm-config(1)
* npm-config(7)
* npmrc(5)
* npm-view(1)
