npm-unpublish(1) -- Remove a package from the registry
======================================================

## SYNOPSIS

    npm unpublish <name>[@<version>]

## WARNING

**It is generally considered bad behavior to remove versions of a library
that others are depending on!**

Consider using the `deprecate` command
instead, if your intent is to encourage users to upgrade.

There is plenty of room on the registry.

## DESCRIPTION

This removes a package version from the registry, deleting its
entry and removing the tarball.

If no version is specified, or if all versions are removed then
the root package entry is removed from the registry entirely.

## SEE ALSO

* npm-deprecate(1)
* npm-publish(1)
* npm-registry(1)
* npm-adduser(1)
* npm-owner(1)
