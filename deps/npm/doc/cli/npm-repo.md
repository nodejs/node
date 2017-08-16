npm-repo(1) -- Open package repository page in the browser
========================================================

## SYNOPSIS

    npm repo [<pkg>]

## DESCRIPTION

This command tries to guess at the likely location of a package's
repository URL, and then tries to open it using the `--browser`
config param. If no package name is provided, it will search for
a `package.json` in the current folder and use the `name` property.

## CONFIGURATION

### browser

* Default: OS X: `"open"`, Windows: `"start"`, Others: `"xdg-open"`
* Type: String

The browser that is called by the `npm repo` command to open websites.

## SEE ALSO

* npm-docs(1)
* npm-config(1)
