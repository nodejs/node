# Maintaining undici

The [undici](https://github.com/nodejs/undici) dependency is
an HTTP/1.1 client, written from scratch for Node.js.

## Updating undici

The `tools/dep_updaters/update-undici.sh` script automates the update of the
undici source files.

Check that Node.js still builds and tests.

## Committing undici

1. Add undici:
   ```console
   $ git add deps/undici
   ```
2. Commit the changes: `git commit`.
3. Add a message like:
   ```text
   deps: update undici to <version>

   Updated as described in doc/contributing/maintaining-undici.md.
   ```
