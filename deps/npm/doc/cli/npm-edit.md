npm-edit(1) -- Edit an installed package
========================================

## SYNOPSIS

    npm edit <pkg>[/<subpkg>...]

## DESCRIPTION

Selects a (sub)dependency in the current
working directory and opens the package folder in the default editor
(or whatever you've configured as the npm `editor` config -- see
`npm-config(7)`.)

After it has been edited, the package is rebuilt so as to pick up any
changes in compiled packages.

For instance, you can do `npm install connect` to install connect
into your package, and then `npm edit connect` to make a few
changes to your locally installed copy.

## CONFIGURATION

### editor

* Default: `EDITOR` environment variable if set, or `"vi"` on Posix,
  or `"notepad"` on Windows.
* Type: path

The command to run for `npm edit` or `npm config edit`.

## SEE ALSO

* npm-folders(5)
* npm-explore(1)
* npm-install(1)
* npm-config(1)
* npm-config(7)
* npmrc(5)
