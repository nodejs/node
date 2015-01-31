npm-dist-tag(1) -- Modify package distribution tags
===================================================

## SYNOPSIS

    npm dist-tag add <pkg>@<version> [<tag>]
    npm dist-tag rm <pkg> <tag>
    npm dist-tag ls [<pkg>]

## DESCRIPTION

Add, remove, and enumerate distribution tags on a package:

* add:
  Tags the specified version of the package with the specified tag, or the
  `--tag` config if not specified.

* rm:
  Clear a tag that is no longer in use from the package.

* ls:
  Show all of the dist-tags for a package, defaulting to the package in
  the curren prefix.

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

This command used to be known as `npm tag`, which only created new tags, and so
had a different syntax.

Tags must share a namespace with version numbers, because they are specified in
the same slot: `npm install <pkg>@<version>` vs `npm install <pkg>@<tag>`.

Tags that can be interpreted as valid semver ranges will be rejected. For
example, `v1.4` cannot be used as a tag, because it is interpreted by semver as
`>=1.4.0 <1.5.0`.  See <https://github.com/npm/npm/issues/6082>.

The simplest way to avoid semver problems with tags is to use tags that do not
begin with a number or the letter `v`.

## SEE ALSO

* npm-tag(1)
* npm-publish(1)
* npm-install(1)
* npm-dedupe(1)
* npm-registry(7)
* npm-config(1)
* npm-config(7)
* npm-tag(3)
* npmrc(5)
