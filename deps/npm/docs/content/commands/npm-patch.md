---
title: npm-patch
section: 1
description: Apply local patches to installed dependencies
---

### Synopsis

```bash
npm patch <pkg>[@<version>]
npm patch add <pkg>[@<version>] [--edit-dir <path>] [--ignore-existing]
npm patch commit <edit-dir> [--patches-dir <dir>] [--keep-edit-dir]
npm patch update <pkg>[@<old-version>] [--to <new-version>] [--patches-dir <dir>]
npm patch ls
npm patch rm <pkg>[@<version>]
```

Note: This command is unaware of workspaces.

### Description

`npm patch` lets you apply small, local modifications to an installed
dependency and have them re-applied automatically on every install. Patches
are declared in the `patchedDependencies` field of your root `package.json`,
stored as plain unified diffs under the `patches/` directory, and recorded with
a content hash in `package-lock.json`.

Because patches are applied during the install itself, they work regardless of
`install-strategy`, apply to transitive dependencies, and are **not** disabled
by `--ignore-scripts`.

The bare form `npm patch <pkg>` is shorthand for `npm patch add <pkg>`. A
package literally named like a subcommand must use the explicit form, e.g.
`npm patch add add`.

* `npm patch add <pkg>[@<version>]`

    Prepares a package for editing. npm extracts a clean copy of the resolved
    package tarball into a temporary directory outside `node_modules` and prints
    its path. Edit the files there, then run `npm patch commit`.

    If more than one version of `<pkg>` is installed, re-run with an exact
    selector such as `npm patch add lodash@4.17.21`.

* `npm patch commit <edit-dir>`

    Diffs the edited directory against a clean copy of the original tarball,
    writes the unified diff to `<patches-dir>/<name>@<version>.patch`, adds the
    entry to `patchedDependencies`, and updates `package-lock.json`.

* `npm patch ls`

    Lists registered patches and how many installed nodes each one matches.

* `npm patch rm <pkg>[@<version>]`

    Removes the matching entries from `patchedDependencies`, deletes the patch
    file when no other entry references it, and updates `package-lock.json`. If
    `<version>` is omitted, all entries for `<pkg>` are removed.

### Failure modes

By default any patch problem is a hard error that aborts the install: a patch
that fails to apply, a registered patch that matches no installed package, a
missing patch file, or a patch whose hash does not match the lockfile.

Two CLI-only flags relax this for one-off cases: `--allow-unused-patches` and
`--ignore-patch-failures`.

### Configuration

#### `patches-dir`

* Default: "patches"
* Type: String

The directory, relative to the project root, where `npm patch commit` writes
patch files for `patchedDependencies`.



#### `allow-unused-patches`

* Default: false
* Type: Boolean

Install even when a registered patch in `patchedDependencies` matches no
installed package. Does not silence patch apply failures.

This flag is only honored when passed on the command line; it is ignored in
`.npmrc` and environment variables, and rejected by `npm ci`.



#### `ignore-patch-failures`

* Default: false
* Type: Boolean

Install even when a registered patch fails to apply, with a warning per
failure. Intended for incident response only.

This flag is only honored when passed on the command line; it is ignored in
`.npmrc` and environment variables, and rejected by `npm ci`.



#### `edit-dir`

* Default: null
* Type: null or Path

Override the temporary directory used by `npm patch add` to prepare a
package for editing.



#### `ignore-existing`

* Default: false
* Type: Boolean

With `npm patch add`, discard a previous unfinished edit directory and start
fresh.



#### `keep-edit-dir`

* Default: false
* Type: Boolean

With `npm patch commit`, do not remove the edit directory after committing
the patch.



#### `to`

* Default: null
* Type: null or String

Used by `npm patch update` to set the version to rebase a patch onto when it
cannot be read from `package-lock.json` — for example an exact-version
selector, or a version that has not been installed yet.



#### `registry`

* Default: "https://registry.npmjs.org/"
* Type: URL

The base URL of the npm registry.


## See Also

* [npm install](/commands/npm-install)
* [npm ci](/commands/npm-ci)
* [package-lock.json](/configuring-npm/package-lock-json)
* [config](/commands/npm-config)
