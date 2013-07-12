npm-unpublish(3) -- Remove a package from the registry
======================================================

## SYNOPSIS

    npm.commands.unpublish(package, callback)

## DESCRIPTION

This removes a package version from the registry, deleting its
entry and removing the tarball.

The package parameter must be defined.

Only the first element in the package parameter is used.  If there is no first
element, then npm assumes that the package at the current working directory
is what is meant.

If no version is specified, or if all versions are removed then
the root package entry is removed from the registry entirely.
