# Maintaining merve

The [merve](https://github.com/nodejs/node/tree/HEAD/deps/merve)
dependency is used within the Node.js ESM implementation to detect the
named exports of a CommonJS module.

It is used within
[`node:internal/modules/esm/translators`](https://github.com/nodejs/node/blob/HEAD/lib/internal/modules/esm/translators.js)
where it is exposed via an internal binding.

## Updating merve

The `tools/dep_updaters/update-merve.sh` script automates the update of the
merve dependency. It fetches the latest release from GitHub and updates the
files in the `deps/merve` directory.

To update merve manually:

* Check the [merve releases][] for a new version.
* Download the latest single-header release.
* Replace the files in `deps/merve` (preserving `merve.gyp`).
* Update the link to merve in the list at the end of
  [doc/api/esm.md](../../api/esm.md)
  to point to the updated version.
* Create a PR adding the files in the deps/merve that were modified.

If updates are needed to merve for Node.js, first PR those updates into
[anonrig/merve][],
request a release and then pull in the updated version once available.

[anonrig/merve]: https://github.com/anonrig/merve
[merve releases]: https://github.com/anonrig/merve/releases
