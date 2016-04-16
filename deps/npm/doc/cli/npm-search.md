npm-search(1) -- Search for packages
====================================

## SYNOPSIS

    npm search [-l|--long] [search terms ...]

    aliases: s, se, find

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

### registry

 * Default: https://registry.npmjs.org/
 * Type   : url

Search the specified registry for modules. If you have configured npm to point to a different default registry,
such as your internal private module repository, `npm search` will default to that registry when searching.
Pass a different registry url such as the default above in order to override this setting.

## SEE ALSO

* npm-registry(7)
* npm-config(1)
* npm-config(7)
* npmrc(5)
* npm-view(1)
