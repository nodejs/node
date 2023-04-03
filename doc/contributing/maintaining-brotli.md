# Maintaining brotli

The [brotli](https://github.com/google/brotli) dependency is used for 
the homonym generic-purpose lossless compression algorithm.

## Updating brotli

The `tools/dep_updaters/update-brotli.sh` script automates the update of the
brotli source files.

Check that Node.js still builds and tests.

## Committing postject

1. Add brotli:
   ```console
   $ git add deps/brotli
   ```
2. Commit the changes: `git commit`.
3. Add a message like:

   ```text
   deps,test: update brotli to <version>

   Updated as described in doc/contributing/maintaining-brotli.md.
   ```
