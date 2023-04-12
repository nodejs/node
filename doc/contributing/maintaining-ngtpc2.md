# ngtcp2 and nghttp3

The ngtcp2 and nghttp3 dependencies provide the core functionality for
QUIC and HTTP/3.

The sources are pulled from:

* ngtcp2: <https://github.com/ngtcp2/ngtcp2>
* nghttp3: <https://github.com/ngtcp2/nghttp3>

In both the `ngtcp2` and `nghttp3` git repos, the active development occurs
in the default branch (currently named `main` in each). Tagged versions do not
always point to the default branch.

We only use a subset of the sources for each.

## Updating

The `nghttp3` library depends on `ngtcp2`. Both should always be updated
together. From `ngtcp2` we only want the contents of the `lib` and `crypto`
directories; from `nghttp3` we only want the contents of the `lib` directory.

After updating either dependency, check if any source files or include
directories have been added or removed and update `ngtcp2.gyp` accordingly.

### Updating ngtcp2

The `tools/dep_updaters/update-ngtcp2.sh` script automates the update of the
ngtcp2 source files.

Check that Node.js still builds and tests.

1. Add ngtcp2:
   ```console
   $ git add deps/ngtcp2
   ```
2. Commit the changes: `git commit`.
3. Add a message like:
   ```text
   deps: update ngtcp2 to <version>

   Updated as described in doc/contributing/maintaining-ngtcp2.md.
   ```

### Updating nghttp3

The `tools/dep_updaters/update-nghttp3.sh` script automates the update of the
nghttp3 source files.

Check that Node.js still builds and tests.

1. Add nghttp3:
   ```console
   $ git add deps/ngtcp2
   ```
2. Commit the changes: `git commit`.
3. Add a message like:
   ```text
   deps: update nghttp3 to <version>

   Updated as described in doc/contributing/maintaining-ngtcp2.md.
   ```
