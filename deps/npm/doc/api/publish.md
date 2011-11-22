npm-publish(3) -- Publish a package
===================================

## SYNOPSIS

    npm.commands.publish([packages,] callback)

## DESCRIPTION

Publishes a package to the registry so that it can be installed by name.
Possible values in the 'packages' array are:

* `<folder>`:
  A folder containing a package.json file

* `<tarball>`:
  A url or file path to a gzipped tar archive containing a single folder
  with a package.json file inside.

If the package array is empty, npm will try to publish something in the
current working directory.

This command could fails if one of the packages specified already exists in
the registry.  Overwrites when the "force" environment variable is set.

## SEE ALSO

* npm-registry(1)
* npm-adduser(1)
* npm-owner(3)
