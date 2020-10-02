# bin-links [![npm version](https://img.shields.io/npm/v/bin-links.svg)](https://npm.im/bin-links) [![license](https://img.shields.io/npm/l/bin-links.svg)](https://npm.im/bin-links) [![Travis](https://img.shields.io/travis/npm/bin-links.svg)](https://travis-ci.org/npm/bin-links) [![AppVeyor](https://ci.appveyor.com/api/projects/status/github/npm/bin-links?svg=true)](https://ci.appveyor.com/project/npm/bin-links) [![Coverage Status](https://coveralls.io/repos/github/npm/bin-links/badge.svg?branch=latest)](https://coveralls.io/github/npm/bin-links?branch=latest)

[`bin-links`](https://github.com/npm/bin-links) is a standalone library that links
binaries and man pages for Javascript packages

## Install

`$ npm install bin-links`

## Table of Contents

* [Example](#example)
* [Features](#features)
* [Contributing](#contributing)
* [API](#api)
  * [`binLinks`](#binLinks)
  * [`binLinks.getPaths()`](#getPaths)
  * [`binLinks.checkBins()`](#checkBins)

### Example

```javascript
const binLinks = require('bin-links')
const readPackageJson = require('read-package-json-fast')
binLinks({
  path: '/path/to/node_modules/some-package',
  pkg: readPackageJson('/path/to/node_modules/some-package/package.json'),

  // true if it's a global install, false for local.  default: false
  global: true,

  // true if it's the top level package being installed, false otherwise
  top: true,

  // true if you'd like to recklessly overwrite files.
  force: true,
})
```

### Features

* Links bin files listed under the `bin` property of pkg to the
  `node_modules/.bin` directory of the installing environment.  (Or
  `${prefix}/bin` for top level global packages on unix, and `${prefix}`
  for top level global packages on Windows.)
* Links man files listed under the `man` property of pkg to the share/man
  directory.  (This is only done for top-level global packages on Unix
  systems.)

### Contributing

The npm team enthusiastically welcomes contributions and project participation!
There's a bunch of things you can do if you want to contribute! The [Contributor
Guide](CONTRIBUTING.md) has all the information you need for everything from
reporting bugs to contributing entire new features. Please don't hesitate to
jump in if you'd like to, or even ask us questions if something isn't clear.

### API

#### <a name="binLinks"></a> `> binLinks({path, pkg, force, global, top})`

Returns a Promise that resolves when the requisite things have been linked.

#### <a name="getPaths"></a> `> binLinks.getPaths({path, pkg, global, top })`

Returns an array of all the paths of links and shims that _might_ be
created (assuming that they exist!) for the package at the specified path.

Does not touch the filesystem.

#### <a name="checkBins"></a> `> binLinks.checkBins({path, pkg, global, top, force })`

Checks if there are any conflicting bins which will prevent the linking of
bins for the given package.  Returns a Promise that resolves with no value
if the way is clear, and rejects if there's something in the way.

Always returns successfully if `global` or `top` are false, or if `force`
is true, or if the `pkg` object does not contain any bins to link.

Note that changes to the file system _may_ still cause the `binLinks`
method to fail even if this method succeeds.  Does not check for
conflicting `man` links.

Reads from the filesystem but does not make any changes.

##### Example

```javascript
binLinks({path, pkg, force, global, top}).then(() => console.log('bins linked!'))
```
