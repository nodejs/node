# Dependency update scripts

This folder contains scripts used to automatically update a Node.js dependency.
These scripts are usually run by CI (see `.github/workflows/tools.yml`) in order
to download a new dependency version, and replace the old version with it.

Since these scripts only update to the upstream code, changes might be needed in
this repository in order to successfully update (e.g: changing API calls to
conform to upstream changes, updating GYP build files, etc.)

## libuv

The `update-libuv.sh` script takes the target version to update as its only
argument, downloads it from the [GitHub repo](https://github.com/libuv/libuv)
and uses it to replace the contents of `deps/uv/`. The contents are replaced
entirely except for the `*.gyp` and `*.gypi` build files, which are part of the
Node.js build definitions and are not present in the upstream repo.

For example, in order to update to version `1.44.2`, the following command can
be run:

```bash
./tools/dep_updaters/update-libuv.sh 1.44.2
```

Once the script has run (either manually, or by CI in which case a PR will have
been created with the changes), do the following:

1. Check the [changelog](https://github.com/libuv/libuv/blob/v1.x/ChangeLog) for
   things that might require changes in Node.js.
2. If necessary, update `common.gypi` and `uv.gyp` with build-related changes.
3. Check that Node.js compiles without errors and the tests pass.
4. Create a commit for the update and in the commit message include the
   important/relevant items from the changelog (see [`c61870c`][] for an
   example).

[`c61870c`]: https://github.com/nodejs/node/commit/c61870c376e2f5b0dbaa939972c46745e21cdbdd

## simdutf

The `update-simdutf.sh` script takes the target version to update as its only
argument, downloads it from the [GitHub repo](https://github.com/simdutf/simdutf)
and uses it to replace the contents of `deps/simdutf/`. The contents are replaced
entirely except for the `*.gyp` and `*.gypi` build files, which are part of the
Node.js build definitions and are not present in the upstream repo.

For example, in order to update to version `2.0.7`, the following command can
be run:

```bash
./tools/dep_updaters/update-simdutf.sh 2.0.7
```

Once the script has run (either manually, or by CI in which case a PR will have
been created with the changes), do the following:

1. Check the [changelog](https://github.com/simdutf/simdutf/releases/tag/v2.0.7) for
   things that might require changes in Node.js.
2. If necessary, update `simdutf.gyp` with build-related changes.
3. Check that Node.js compiles without errors and the tests pass.
4. Create a commit for the update and in the commit message include the
   important/relevant items from the changelog.

## postject

The `update-postject.sh` script downloads postject from the [npm package](http://npmjs.com/package/postject)
and uses it to replace the contents of `test/fixtures/postject-copy`.

In order to update, the following command can be run:

```bash
./tools/dep_updaters/update-postject.sh
```

Once the script has run (either manually, or by CI in which case a PR will have
been created with the changes), do the following:

1. Check the [changelog](https://github.com/nodejs/postject/releases/tag/v1.0.0-alpha.4)
   for things that might require changes in Node.js.
2. Check that Node.js compiles without errors and the tests pass.
3. Create a commit for the update and in the commit message include the
   important/relevant items from the changelog.
