npm-prune(3) -- Remove extraneous packages
==========================================

## SYNOPSIS

    npm.commands.prune([packages,] callback)

## DESCRIPTION

This command removes "extraneous" packages.

The first parameter is optional, and it specifies packages to be removed.

No packages are specified, then all packages will be checked.

Extraneous packages are packages that are not listed on the parent
package's dependencies list.
