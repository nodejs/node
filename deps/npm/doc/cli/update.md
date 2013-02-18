npm-update(1) -- Update a package
=================================

## SYNOPSIS

    npm update [-g] [<name> [<name> ...]]

## DESCRIPTION

This command will update all the packages listed to the latest version
(specified by the `tag` config).

It will also install missing packages.

If the `-g` flag is specified, this command will update globally installed packages.
If no package name is specified, all packages in the specified location (global or local) will be updated.

## SEE ALSO

* npm-install(1)
* npm-outdated(1)
* npm-registry(1)
* npm-folders(1)
* npm-list(1)
