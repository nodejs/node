npm-publish(1) -- Publish a package
===================================


## SYNOPSIS

    npm publish <tarball> [--tag <tag>]
    npm publish <folder> [--tag <tag>]

## DESCRIPTION

Publishes a package to the registry so that it can be installed by name.

* `<folder>`:
  A folder containing a package.json file

* `<tarball>`:
  A url or file path to a gzipped tar archive containing a single folder
  with a package.json file inside.

* `[--tag <tag>]`
  Registers the published package with the given tag, such that `npm install
  <name>@<tag>` will install this version.  By default, `npm publish` updates
  and `npm install` installs the `latest` tag.

Fails if the package name and version combination already exists in
the registry.

Once a package is published with a given name and version, that
specific name and version combination can never be used again, even if
it is removed with npm-unpublish(1).

## SEE ALSO

* npm-registry(7)
* npm-adduser(1)
* npm-owner(1)
* npm-deprecate(1)
* npm-tag(1)
