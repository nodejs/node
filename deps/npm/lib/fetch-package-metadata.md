fetch-package-metadata
----------------------

    const fetchPackageMetadata = require("npm/lib/fetch-package-metadata")
    fetchPackageMetadata(spec, contextdir, callback)

This will get package metadata (and if possible, ONLY package metadata) for
a specifier as passed to `npm install` et al, eg `npm@next` or `npm@^2.0.3`

## fetchPackageMetadata(*spec*, *contextdir*, *tracker*, *callback*)

* *spec* **string** | **object** -- The package specifier, can be anything npm can
  understand (see [realize-package-specifier]), or it can be the result from
  realize-package-specifier or npm-package-arg (for non-local deps).

* *contextdir* **string** -- The directory from which relative paths to
  local packages should be resolved.

* *tracker* **object** -- **(optional)** An are-we-there-yet tracker group as
  provided by `npm.log.newGroup()`.

* *callback* **function (er, package)** -- Called when the package information
  has been loaded. `package` is the object for of the `package.json`
  matching the requested spec.  In the case of named packages, it comes from
  the registry and thus may not exactly match what's found in the associated
  tarball.

[realize-package-specifier]: (https://github.com/npm/realize-package-specifier)

In the case of tarballs and git repos, it will use the cache to download
them in order to get the package metadata.  For named packages, only the
metadata is downloaded (eg https://registry.npmjs.org/package).  For local
directories, the package.json is read directly.  For local tarballs, the
tarball is streamed in memory and just the package.json is extracted from
it.  (Due to the nature of tars, having the package.json early in the file
will result in it being loaded faster– the extractor short-circuits the
uncompress/untar streams as best as it can.)
