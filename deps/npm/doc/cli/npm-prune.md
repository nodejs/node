npm-prune(1) -- Remove extraneous packages
==========================================

## SYNOPSIS

    npm prune [[<@scope>/]<pkg>...] [--production]

## DESCRIPTION

This command removes "extraneous" packages.  If a package name is
provided, then only packages matching one of the supplied names are
removed.

Extraneous packages are packages that are not listed on the parent
package's dependencies list.

If the `--production` flag is specified or the `NODE_ENV` environment
variable is set to `production`, this command will remove the packages
specified in your `devDependencies`. Setting `--production=false` will
negate `NODE_ENV` being set to `production`.

## SEE ALSO

* npm-rm(1)
* npm-folders(5)
* npm-ls(1)
