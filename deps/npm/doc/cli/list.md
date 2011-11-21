npm-ls(1) -- List installed packages
======================================

## SYNOPSIS

    npm list
    npm ls
    npm la
    npm ll

## DESCRIPTION

This command will print to stdout all the versions of packages that are
installed, as well as their dependencies, in a tree-structure.

It does not take positional arguments, though you may set config flags
like with any other command, such as `-g` to list global packages.

It will print out extraneous, missing, and invalid packages.

When run as `ll` or `la`, it shows extended information by default.

## CONFIGURATION

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

## SEE ALSO

* npm-config(1)
* npm-folders(1)
* npm-install(1)
* npm-link(1)
* npm-prune(1)
* npm-outdated(1)
* npm-update(1)
