# Maintaining c-ares

Updates to the c-ares dependency involve the following steps:

1. Downloading the source archive for the new version.
2. Unpacking the source in a temporary workspace directory.
3. Removing the `test` directory (to save disk space).
4. Copying over the existing `.gitignore`, pre-generated `config` directory and
   `cares.gyp` files.
5. Replacing the existing `deps/cares` with the workspace directory.
6. Modifying the `cares.gyp` file for file additions/deletions.
7. Rebuilding the main Node.js `LICENSE`.

## Running the update script

The `tools/update-cares.sh` script automates the update of the c-ares source
files, preserving the existing files added by Node.js.

In the following examples, `x.y.z` should match the c-ares version to update to.

```console
./tools/update-cares.sh x.y.z
```

e.g.

```console
./tools/update-cares.sh 1.18.1
```

## Check that Node.js still builds and tests

It may be necessary to update `deps/cares/cares.gyp` if any significant changes
have occurred upstream.

## Rebuild the main Node.js license

Run the `tools/license-builder.sh` script to rebuild the main Node.js `LICENSE`
file. This may result in no changes if c-ares' license has not changed.

```console
./tools/license-builder.sh
```

If the updated `LICENSE` contains changes for other dependencies, those should
be done in a separate pull request first.

## Commit the changes

```console
git add -A deps/cares
```

Add the rebuilt `LICENSE` if it has been updated.

```console
git add LICENSE
```

Commit the changes with a message like

```text
deps: update c-ares to x.y.z

Updated as described in doc/guides/maintaining-c-ares.md.
```
