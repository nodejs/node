npm-search(1) -- Search for packages
====================================

## SYNOPSIS

    npm search [search terms ...]

## DESCRIPTION

Search the registry for packages matching the search terms.

If a term starts with `/`, then it's interpreted as a regular expression.
A trailing `/` will be ignored in this case.  (Note that many regular
expression characters must be escaped or quoted in most shells.)

## SEE ALSO

* npm-registry(1)
* npm-config(1)
* npm-view(1)
