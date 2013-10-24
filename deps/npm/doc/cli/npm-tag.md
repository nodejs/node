npm-tag(1) -- Tag a published version
=====================================

## SYNOPSIS

    npm tag <name>@<version> [<tag>]

## DESCRIPTION

Tags the specified version of the package with the specified tag, or the
`--tag` config if not specified.

A tag can be used when installing packages as a reference to a version instead
of using a specific version number:

    npm install <name>@<tag>

When installing dependencies, a preferred tagged version may be specified:

    npm install --tag <tag>

This also applies to `npm dedupe`.

Publishing a package always sets the "latest" tag to the published version.

## SEE ALSO

* npm-publish(1)
* npm-install(1)
* npm-dedupe(1)
* npm-registry(7)
* npm-config(1)
* npm-config(7)
* npmrc(5)
