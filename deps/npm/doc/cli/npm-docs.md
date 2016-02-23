npm-docs(1) -- Docs for a package in a web browser maybe
========================================================

## SYNOPSIS

    npm docs [<pkgname> [<pkgname> ...]]
    npm docs .
    npm home [<pkgname> [<pkgname> ...]]
    npm home .

## DESCRIPTION

This command tries to guess at the likely location of a package's
documentation URL, and then tries to open it using the `--browser`
config param. You can pass multiple package names at once. If no
package name is provided, it will search for a `package.json` in
the current folder and use the `name` property.

## CONFIGURATION

### browser

* Default: OS X: `"open"`, Windows: `"start"`, Others: `"xdg-open"`
* Type: String

The browser that is called by the `npm docs` command to open websites.

### registry

* Default: https://registry.npmjs.org/
* Type: url

The base URL of the npm package registry.


## SEE ALSO

* npm-view(1)
* npm-publish(1)
* npm-registry(7)
* npm-config(1)
* npm-config(7)
* npmrc(5)
* package.json(5)
