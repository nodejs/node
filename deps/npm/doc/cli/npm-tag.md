npm-tag(1) -- Tag a published version
=====================================

## SYNOPSIS

    npm tag <name>@<version> [<tag>]

## DESCRIPTION

THIS COMMAND IS DEPRECATED. See npm-dist-tag(1) for details.

Tags the specified version of the package with the specified tag, or the
`--tag` config if not specified.

A tag can be used when installing packages as a reference to a version instead
of using a specific version number:

    npm install <name>@<tag>

When installing dependencies, a preferred tagged version may be specified:

    npm install --tag <tag>

This also applies to `npm dedupe`.

Publishing a package always sets the "latest" tag to the published version.

## PURPOSE

Tags can be used to provide an alias instead of version numbers.  For
example, `npm` currently uses the tag "next" to identify the upcoming
version, and the tag "latest" to identify the current version.

A project might choose to have multiple streams of development, e.g.,
"stable", "canary".

## CAVEATS

Tags must share a namespace with version numbers, because they are
specified in the same slot: `npm install <pkg>@<version>` vs `npm
install <pkg>@<tag>`.

Tags that can be interpreted as valid semver ranges will be
rejected. For example, `v1.4` cannot be used as a tag, because it is
interpreted by semver as `>=1.4.0 <1.5.0`.  See
<https://github.com/npm/npm/issues/6082>.

The simplest way to avoid semver problems with tags is to use tags
that do not begin with a number or the letter `v`.

## SEE ALSO

* npm-publish(1)
* npm-install(1)
* npm-dedupe(1)
* npm-registry(7)
* npm-config(1)
* npm-config(7)
* npm-tag(3)
* npmrc(5)
