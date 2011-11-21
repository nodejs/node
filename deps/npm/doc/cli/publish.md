npm-publish(1) -- Publish a package
===================================


## SYNOPSIS

    npm publish <tarball>
    npm publish <folder>

## DESCRIPTION

Publishes a package to the registry so that it can be installed by name.

* `<folder>`:
  A folder containing a package.json file

* `<tarball>`:
  A url or file path to a gzipped tar archive containing a single folder
  with a package.json file inside.

Fails if the package name and version combination already exists in
the registry.  Overwrites when the "--force" flag is set.

## SEE ALSO

* npm-registry(1)
* npm-adduser(1)
* npm-owner(1)
* npm-deprecate(1)
* npm-tag(1)
