# Maintaining postject

The [postject](https://github.com/nodejs/postject) dependency is used for the
[Single Executable strategic initiative](https://github.com/nodejs/single-executable).

## Updating postject

The `tools/dep_updaters/update-postject.sh` script automates the update of the
postject source files.

Check that Node.js still builds and tests.

## Committing postject

Add postject: `git add --all test/fixtures/postject-copy`

Commit the changes with a message like:

```text
deps: update postject to 1.0.0-alpha.4

Updated as described in doc/contributing/maintaining-postject.md.
```
