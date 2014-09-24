npm-ls(1) -- List installed packages
======================================

## SYNOPSIS

    npm list [[@<scope>/]<pkg> ...]
    npm ls [[@<scope>/]<pkg> ...]
    npm la [[@<scope>/]<pkg> ...]
    npm ll [[@<scope>/]<pkg> ...]

## DESCRIPTION

This command will print to stdout all the versions of packages that are
installed, as well as their dependencies, in a tree-structure.

Positional arguments are `name@version-range` identifiers, which will
limit the results to only the paths to the packages named.  Note that
nested packages will *also* show the paths to the specified packages.
For example, running `npm ls promzard` in npm's source tree will show:

    npm@@VERSION@ /path/to/npm
    └─┬ init-package-json@0.0.4
      └── promzard@0.1.5

It will print out extraneous, missing, and invalid packages.

If a project specifies git urls for dependencies these are shown
in parentheses after the name@version to make it easier for users to
recognize potential forks of a project.

When run as `ll` or `la`, it shows extended information by default.

## CONFIGURATION

### json

* Default: false
* Type: Boolean

Show information in JSON format.

### long

* Default: false
* Type: Boolean

Show extended information.

### parseable

* Default: false
* Type: Boolean

Show parseable output instead of tree view.

### global

* Default: false
* Type: Boolean

List packages in the global install prefix instead of in the current
project.

### depth

* Type: Int

Max display depth of the dependency tree.

## SEE ALSO

* npm-config(1)
* npm-config(7)
* npmrc(5)
* npm-folders(5)
* npm-install(1)
* npm-link(1)
* npm-prune(1)
* npm-outdated(1)
* npm-update(1)
