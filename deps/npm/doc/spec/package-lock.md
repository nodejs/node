# package-lock and npm-shrinkwrap

`npm` can have one of two different lock files:

* `package-lock.json`, which is ordinarily always present and is never published.
* `npm-shrinkwrap.json`, which is created with `npm shrinkwrap` and usually published.

You can only have one of them and in the event that you have both,
`npm-shrinkwrap.json` takes precedence.  The files are exactly the same
format and in fact all the `npm shrinkwrap` command does is rename your
`package-lock.json`.

Through the rest of this document we will refer to the package-lock and
`package-lock.json` but everything also applies to `npm-shrinkwrap.json`.

## File Format

### name

The name of the package this is a package-lock for. This must match what's in `package.json`.

### version

The version of the package this is a package-lock for. This must match what's in `package.json`.

### lockfileVersion *(new)*

An integer version, starting at `1` with the version number of this document
whose semantics were used when generating this `package-lock.json`.

### preserveSymlinks *(new)*

Indicates that the install was done with the environment variable
`NODE_PRESERVE_SYMLINKS` enabled. The installer should insist that the value of this
property match that environment variable.

### dependencies

These are the modules installed in the `node_modules`.  Some of these are
dependencies some of these are transitive dependencies (that is,
dependencies of our dependencies).

This is a mapping of package name to dependency object.  Dependency objects have the
following properties:

#### version *(changed)*

This is a specifier that uniquely identifies this package and should be
usable in fetching a new copy of it.

* bundled dependencies: Regardless of source, this is a version number that is purely for informational purposes.
* registry sources: This is a version number. (eg, `1.2.3`)
* git sources: This is a git specifier with resolved committish. (eg, `git+https://example.com/foo/bar#115311855adb0789a0466714ed48a1499ffea97e`)
* http tarball sources: This is the URL of the tarball. (eg, `https://example.com/example-1.3.0.tgz`)
* local tarball sources: This is the file URL of the tarball. (eg `file:///opt/storage/example-1.3.0.tgz`)
* local link sources: This is the file URL of the link. (eg `file:libs/our-module`)

#### integrity *(new)*

This is a [Standard Subresource
Integrity](https://w3c.github.io/webappsec/specs/subresourceintegrity/) for
this resource.

* For bundled dependencies this is not included, regardless of source.
* For registry sources, this is the `integrity` that the registry provided, or if one wasn't provided the SHA1 in `shasum`.
* For git sources this is the specific commit hash we cloned from.
* For remote tarball sources this is an integrity based on a SHA512 of
  the file.
* For local tarball sources: This is an integrity field based on the SHA512 of the file.

#### resolved

* For bundled dependencies this is not included, regardless of source.
* For registry sources this is path of the tarball relative to the registry
  URL.  If the tarball URL isn't on the same server as the registry URL then
  this is a complete URL.

#### link *(new)*

If this module was symlinked in development but had semver in the
`package.json` then this is the relative path of that link.

Discussion of the semantics of this will go in the symlinks RFC.

Implementation note: To be implemented post npm@5.

#### bundled *(new)*

If true, this is the bundled dependency and will be installed by the parent
module.  When installing, this module will be extracted from the parent
module during the extract phase, not installed as a separate dependency.

#### dev

If true then this dependency is either a development dependency ONLY of the
top level module or a transitive dependency of one.  This is false for
dependencies that are both a development dependency of the top level and a
transitive dependency of a non-development dependency of the top level.

#### optional

If true then this dependency is either an optional dependency ONLY of the
top level module or a transitive dependency of one.  This is false for
dependencies that are both an optional dependency of the top level and a
transitive dependency of a non-optional dependency of the top level.

All optional dependencies should be included even if they're uninstallable
on the current platform.

#### from

This is a record of what specifier was used to originally install this
package.  This should be used only for git dependencies.

#### requires

This is a mapping of module name to version.  This is a list of everything
this module requires, regardless of where it will be installed.  The version
should match via normal matching rules a dependency either in our
`dependencies` or in a level higher than us.

#### dependencies

Exactly like `dependencies` at the top level, this is a list of modules to
install in the `node_modules` of this module.

## Generating

### `npm init`

If neither a `package-lock.json` nor an `npm-shrinkwrap.json` exist then
`npm init` will create a `package-lock.json`.  This is functionally
equivalent to running `npm shrinkwrap` after the current init completes and
renaming the result to `package-lock.json`.

### `npm install --save`

If either an `npm-shrinkwrap.json` or a `package-lock.json` exists then it
will be updated.

If neither exist then a `package-lock.json` should be generated.

If a `package.json` does not exist, it should be generated.  The generated
`package.json` should be empty, as in:

```
{
   "dependencies": {
   }
}
```

If the user wants to get a default package name/version added they can run `npm init`.

### `npm shrinkwrap`

If a `package-lock.json` exists, rename it to `npm-shrinkwrap.json`.
Refresh the data from the installer's ideal tree.

The top level `name` and `version` come from the `package.json`.  It is an
error if either are missing or invalid.

#### dependencies.dev

This is `true` if this dependency is ONLY installed to fulfill either a top
level development dependency, or one of its transitive dependencies.

Given:
```
B (Dev) → C
```

Then both B and C would be `dev: true`.

Given:
```
A → B → C
B (Dev) -> C
```

Then all dependencies would be `dev: false`.

#### dependencies.optional

This is `true` if this dependency is ONLY ever either an optional dependency
or a transitive dependency of optional dependencies.

Given:
```
A (Opt) → B → C
```

Then all three of A, B and C would be flagged as optional.

Given:
```
A (Opt) → B → C
D → C
```

Then A and B would be flagged as optional, but C would not be.

Given:
```
A (Opt) → B → C
D → A
```

Then none would be flagged as optional.

## Installing

If the `packageIntegrity` in the `package-lock.json` differs from the one
computed from the `package.json` then places where the `package.json` is
incompatible with the `package-lock.json` a new module should be installed.
That is, while the `package-lock.json` ordinarily defines the state of your
project, if your `package.json` is edited independently it will take
precedence.

The `package-lock.json` describes the exact tree that `npm` should create.
Any deviation between the `package.json` and the shrinkwrap/lock should
result in a warning be issued.  This includes:

* Modules in `package.json` but missing from the `package-lock.json`
* Modules in the `package-lock.json` but missing from the `package.json`.
* Modules in `package.json` whose specifiers don't match the version in `package-lock.json`.

Warn if the `lockfileVersion` in the `package-lock.json` is for a different
major version than we implement.

Module resolution from package-lock data works as such:

* If install was run with `--resolve-links` and a dependency has a `link`
  property then a symlink is made using that.  If the version of the
  destination can not be matched to the package-lock and/or the package.json
  then a warning will be issued.

* Otherwise, if a `integrity` is available then we try to install it from the cache using it.

If `integrity` is unavailable or we are unable to locate a module from the `integrity` then:

* If `lockfileVersion` is set:
  * Install using the value of `version` and validate the result against the
    `integrity`.
* Otherwise, try these in turn and validate the result against the `integrity`:
  * `resolved`, then `from`, then `version.
  * `from` can be either `package@specifier` or just `specifier`.

Regardless of how the module is installed the metadata in the installed
module should be identical to what it would have been if the module were
installed w/o a package-lock.

## Implied Changes To Other Commands

### `npm rm --save`

Currently if you ask to remove a package that's both a direct and a
transitive dependency, we'll remove the package from `node_modules` even if
this results in a broken tree.  This was chosen at the time because we felt
that users would expect `npm rm pkgname` to be equivalent of
`rm -rf node_modules/pkgname`.

As you are no longer going to be allowed to put your `node_modules` in a
state that's not a valid package-lock, this means this behavior is no longer
valid.  Instead we should follow normal rules, removing it from the
dependencies for the top level but only removing the module on disk if
nothing requires it any more.

## Additional fields / Adding new fields

Installers should ignore any field they aren't aware of.  It's not an error
to have additional properities in the package-lock or lock file.

Installers that want to add new fields should either have one added via RFC
in the npm issue tracker and an accompanying documentation PR, or should prefix
it with the name of their project.
