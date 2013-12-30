npm-bugs(1) -- Bugs for a package in a web browser maybe
========================================================

## SYNOPSIS

    npm bugs <pkgname>
    npm bugs (with no args in a package dir)

## DESCRIPTION

This command tries to guess at the likely location of a package's
bug tracker URL, and then tries to open it using the `--browser`
config param. If no package name is provided, it will search for
a `package.json` in the current folder and use the `name` property.

## CONFIGURATION

### browser

* Default: OS X: `"open"`, Windows: `"start"`, Others: `"xdg-open"`
* Type: String

The browser that is called by the `npm bugs` command to open websites.

### registry

* Default: https://registry.npmjs.org/
* Type: url

The base URL of the npm package registry.


## SEE ALSO

* npm-docs(1)
* npm-view(1)
* npm-publish(1)
* npm-registry(7)
* npm-config(1)
* npm-config(7)
* npmrc(5)
* package.json(5)
