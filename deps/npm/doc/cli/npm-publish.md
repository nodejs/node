npm-publish(1) -- Publish a package
===================================


## SYNOPSIS

    npm publish [<tarball>|<folder>] [--tag <tag>] [--access <public|restricted>]

    Publishes '.' if no argument supplied
    Sets tag 'latest' if no --tag specified

## DESCRIPTION

Publishes a package to the registry so that it can be installed by name. See
`npm-developers(7)` for details on what's included in the published package, as
well as details on how the package is built.

By default npm will publish to the public registry. This can be overridden by
specifying a different default registry or using a `npm-scope(7)` in the name
(see `package.json(5)`).

* `<folder>`:
  A folder containing a package.json file

* `<tarball>`:
  A url or file path to a gzipped tar archive containing a single folder
  with a package.json file inside.

* `[--tag <tag>]`
  Registers the published package with the given tag, such that `npm install
  <name>@<tag>` will install this version.  By default, `npm publish` updates
  and `npm install` installs the `latest` tag.

* `[--access <public|restricted>]`
  Tells the registry whether this package should be published as public or
  restricted. Only applies to scoped packages, which default to `restricted`.
  If you don't have a paid account, you must publish with `--access public`
  to publish scoped packages.

Fails if the package name and version combination already exists in
the specified registry.

Once a package is published with a given name and version, that
specific name and version combination can never be used again, even if
it is removed with npm-unpublish(1).

## SEE ALSO

* npm-registry(7)
* npm-scope(7)
* npm-adduser(1)
* npm-owner(1)
* npm-deprecate(1)
* npm-tag(1)
