# `file:` specifiers

`specifier` refers to the value part of the `package.json`'s `dependencies`
object.  This is a semver expression for registry dependencies and
URLs and URL-like strings for other types.

### Dependency Specifiers

* A `file:` specifier is either an absolute path (eg `/path/to/thing`, `d:\path\to\thing`):
  * An absolute `file:///absolute/path` with any number of leading slashes
    being treated as a single slash. That is, `file:/foo/bar` and
    `file:///foo/bar` reference the same package.
* … or a relative path (eg `../path/to/thing`, `path\to\subdir`).  Leading
  slashes on a file specifier will be removed, that is 'file://../foo/bar`
  references the same package as same as `file:../foo/bar`.  The latter is
  considered canonical.
* Attempting to install a specifer that has a windows drive letter will
  produce an error on non-Windows systems.
* A valid `file:` specifier points is:
  * a valid package file. That is, a `.tar`, `.tar.gz` or `.tgz` containing
  `<dir>/package.json`.
  * OR, a directory that contains a `package.json`

Relative specifiers are relative to the file they were found in, or, if
provided on the command line, the CWD that the command was run from.

An absolute specifier found in a `package.json` or `npm-shrinkwrap.json` is
probably an error as it's unlikely to be portable between computers and
should warn.

A specifier provided as a command line argument that is on a different drive
is an error.  That is, `npm install file:d:/foo/bar` is an error if the
current drive is `c`.  The point of this rule is that if we can't produce a
relative path then it's an error.

### Specifier Disambiguation

On the command line, plain paths are allowed.  These paths can be ambiguous
as they could be a path, a plain package name or a github shortcut.  This
ambiguity is resolved by checking to see if either a directory exists that
contains a `package.json`.  If either is the case then the specifier is a
file specifier, otherwise it's a registry or github specifier.

### Specifier Matching

A specifier is considered to match a dependency on disk when the `realpath`
of the fully resolved specifier matches the `realpath` of the package on disk.

### Saving File Specifiers

When saving to both `package.json` and `npm-shrinkwrap.json` they will be
saved using the `file:../relative/path` form, and the relative path will be
relative to the project's root folder.  This is particularly important to
note for the `npm-shrinkwrap.json` as it means the specifier there will
be different then the original `package.json` (where it was relative to that
`package.json`).

# No, for `file:` type specifiers, we SHOULD shrinkwrap.  Other symlinks we
# should not. Other symlinks w/o the link spec should be an error.

When shrinkwrapping file specifiers, the contents of the destination
package's `node_modules` WILL NOT be included in the shrinkwrap. If you want to lock
down the destination package's `node_modules` you should create a shrinkwrap for it
separately.

This is necessary to support the mono repo use case where many projects file
to the same package.  If each project included its own npm-shrinkwrap.json
then they would each have their own distinct set of transitive dependencies
and they'd step on each other any time you ran an install in one or the other.

NOTE: This should not have an effect on shrinkwrapping of other sorts of
shrinkwrapped packages.

### Installation

#### File type specifiers pointing at tarballs

File-type specifiers pointing at a `.tgz` or `.tar.gz or `.tar` file will
install it as a package file in the same way we would a remote tarball.  The
checksum of the package file should be recorded so that we can check for updates.

#### File type specifers pointing at directories

File-type specifiers that point at directories will necessarily not do
anything for `fetch` and `extract` phases.

The symlink should be created during the `finalize` phase.

The `preinstall` for file-type specifiers MUST be run AFTER the
`finalize` phase as the symlink may be a relative path reaching outside the
current project root and a symlink that resolves in `.staging` won't resolve
in the package's final resting place.

If the module is inside the package root that we're running the install for then
dependencies of the linked package will be hoisted to the top level as usual.

If the module is outside the package root then dependencies will be installed inside
the linked module's `node_modules` folder.

### Removal

Removal should remove the symlink.

Removal MUST NOT remove the transitive dependencies IF they're installed in
the linked module's `node_modules` folder.

### Listing

In listings they should not include a version as the version is not
something `npm` is concerned about.  This also makes them easily
distinguishable from symlinks of packages that have other dependency
specifiers.

If you had run:

```
npm install --save file:../a
```

And then run:
```
npm ls
```

You would see:

```
example-package@1.0.0 /path/to/example-package
└── a → file:../a
```

```
example-package@1.0.0 /path/to/example-package
+-- a -> file:../a
```

Of note here: No version is included as the relavent detail is WHERE the
package came from, not what version happened to be in that path.

### Outdated

Local specifiers should only show up in `npm outdated` if they're missing
and when they do, they should be reported as:

```
Package  Current  Wanted  Latest  Location
a        MISSING   LOCAL   LOCAL  example-package
```

### Updating

If a dependency with a local specifier is already installed then `npm
update` shouldn't do anything.  If one is missing then it should be
installed as if you ran `npm install`.
