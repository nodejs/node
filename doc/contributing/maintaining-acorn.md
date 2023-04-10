# Maintaining acorn

The [acorn](https://github.com/acornjs/acorn) dependency is a JavaScript parser.
[acorn-walk](https://github.com/acornjs/acorn/tree/master/acorn-walk) is
an abstract syntax tree walker for the ESTree format.

## Updating acorn

The `tools/dep_updaters/update-acorn.sh` script automates the update of the
acorn source files.

Check that Node.js still builds and tests.

## Committing acorn

1. Add acorn:
   ```console
   $ git add deps/acorn
   ```
2. Commit the changes: `git commit`.
3. Add a message like:
   ```text
   deps: update acorn to <version>

   Updated as described in doc/contributing/maintaining-acorn.md.
   ```

## Updating acorn-walk

The `tools/dep_updaters/update-acorn-walk.sh` script automates the update of the
acorn-walk source files.

Check that Node.js still builds and tests.

## Committing acorn-walk

1. Add acorn-walk:
   ```console
   $ git add deps/acorn-walk
   ```
2. Commit the changes: `git commit`.
3. Add a message like:
   ```text
   deps: update acorn-walk to <version>

   Updated as described in doc/contributing/maintaining-acorn.md.
   ```
