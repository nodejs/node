npm-prune(1) -- Remove extraneous packages
==========================================

## SYNOPSIS

    npm prune [<name> [<name ...]]
    npm prune [<name> [<name ...]] [--production]

## DESCRIPTION

This command removes "extraneous" packages.  If a package name is
provided, then only packages matching one of the supplied names are
removed.

Extraneous packages are packages that are not listed on the parent
package's dependencies list.

If the `--production` flag is specified, this command will remove the
packages specified in your `devDependencies`.

## SEE ALSO

* npm-rm(1)
* npm-folders(5)
* npm-ls(1)
