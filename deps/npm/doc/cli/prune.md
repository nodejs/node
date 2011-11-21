npm-prune(1) -- Remove extraneous packages
==========================================

## SYNOPSIS

    npm prune [<name> [<name ...]]

## DESCRIPTION

This command removes "extraneous" packages.  If a package name is
provided, then only packages matching one of the supplied names are
removed.

Extraneous packages are packages that are not listed on the parent
package's dependencies list.

## SEE ALSO

* npm-rm(1)
* npm-folders(1)
* npm-list(1)
