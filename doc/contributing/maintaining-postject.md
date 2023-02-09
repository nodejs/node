# Maintaining postject

The [postject](https://github.com/nodejs/postject) dependency is used for the
[Single Executable strategic initiative](https://github.com/nodejs/single-executable).

## Updating postject

The `tools/dep_updaters/update-postject.sh` script automates the update of the
postject source files.

Check that Node.js still builds and tests.

## Committing postject

1. Add postject:
   ```console
   $ git add test/fixtures/postject-copy
   $ git add deps/postject
   ```
2. Commit the changes: `git commit`.
3. Add a message like:

   ```text
   deps,test: update postject to <version>

   Updated as described in doc/contributing/maintaining-postject.md.
   ```
