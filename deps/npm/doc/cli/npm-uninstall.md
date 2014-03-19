npm-rm(1) -- Remove a package
=============================

## SYNOPSIS

    npm uninstall <name> [--save|--save-dev|--save-optional]
    npm rm (with any of the previous argument usage)

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

Examples:

    npm uninstall sax --save
    npm uninstall node-tap --save-dev
    npm uninstall dtrace-provider --save-optional

## SEE ALSO

* npm-prune(1)
* npm-install(1)
* npm-folders(5)
* npm-config(1)
* npm-config(7)
* npmrc(5)
