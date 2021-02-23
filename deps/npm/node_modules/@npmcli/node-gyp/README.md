# @npmcli/node-gyp

This is the module npm uses to decide whether a package should be built
using [`node-gyp`](https://github.com/nodejs/node-gyp) by default.

## API

* `isNodeGypPackage(path)`

Returns a Promise that resolves to `true` or `false` based on whether the
package at `path` has a `binding.gyp` file.

* `defaultGypInstallScript`

A string with the default string that should be used as the `install`
script for node-gyp packages.
