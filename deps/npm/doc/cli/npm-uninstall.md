npm-rm(1) -- Remove a package
=============================

## SYNOPSIS

    npm uninstall [<@scope>/]<pkg>[@<version>]... [--save|--save-dev|--save-optional]

    aliases: remove, rm, r, un, unlink

## DESCRIPTION

This uninstalls a package, completely removing everything npm installed
on its behalf.

Example:

    npm uninstall sax

In global mode (ie, with `-g` or `--global` appended to the command),
it uninstalls the current package context as a global package.

`npm uninstall` takes 3 exclusive, optional flags which save or update
the package version in your main package.json:

* `--save`: Package will be removed from your `dependencies`.

* `--save-dev`: Package will be removed from your `devDependencies`.

* `--save-optional`: Package will be removed from your `optionalDependencies`.

Further, if you have an `npm-shrinkwrap.json` then it will be updated as
well.

Scope is optional and follows the usual rules for `npm-scope(7)`.

Examples:

    npm uninstall sax --save
    npm uninstall @myorg/privatepackage --save
    npm uninstall node-tap --save-dev
    npm uninstall dtrace-provider --save-optional

## SEE ALSO

* npm-prune(1)
* npm-install(1)
* npm-folders(5)
* npm-config(1)
* npm-config(7)
* npmrc(5)
