# npm-pick-manifest [![npm version](https://img.shields.io/npm/v/npm-pick-manifest.svg)](https://npm.im/npm-pick-manifest) [![license](https://img.shields.io/npm/l/npm-pick-manifest.svg)](https://npm.im/npm-pick-manifest) [![Travis](https://img.shields.io/travis/npm/npm-pick-manifest.svg)](https://travis-ci.org/npm/npm-pick-manifest) [![AppVeyor](https://ci.appveyor.com/api/projects/status/github/npm/npm-pick-manifest?svg=true)](https://ci.appveyor.com/project/npm/npm-pick-manifest) [![Coverage Status](https://coveralls.io/repos/github/npm/npm-pick-manifest/badge.svg?branch=latest)](https://coveralls.io/github/npm/npm-pick-manifest?branch=latest)

[`npm-pick-manifest`](https://github.com/npm/npm-pick-manifest) is a standalone
implementation of [npm](https://npmjs.com)'s semver range resolution algorithm.

## Install

`$ npm install --save npm-pick-manifest`

## Table of Contents

* [Example](#example)
* [Features](#features)
* [Contributing](#contributing)
* [API](#api)
  * [`pickManifest()`](#pick-manifest)

### Example

```javascript
const pickManifest = require('npm-pick-manifest')

fetch('https://registry.npmjs.org/npm-pick-manifest').then(res => {
  return res.json()
}).then(packument => {
  return pickManifest(packument, '^1.0.0')
}) // get same manifest as npm would get if you `npm i npm-pick-manifest@^1.0.0`
```

### Features

* Uses npm's exact semver resolution algorithm
* Supports ranges, tags, and versions

### Contributing

The npm-pick-manifest team enthusiastically welcomes contributions and project participation!
There's a bunch of things you can do if you want to contribute! The [Contributor
Guide](CONTRIBUTING.md) has all the information you need for everything from
reporting bugs to contributing entire new features. Please don't hesitate to
jump in if you'd like to, or even ask us questions if something isn't clear.

### API

#### <a name="pick-manifest"></a> `> pickManifest(packument, selector, [opts]) -> manifest`

Returns the manifest that matches `selector`, or throws an error.

Packuments are anything returned by metadata URLs from the npm registry. That
is, they're objects with the following shape (only fields used by
`npm-pick-manifest` included):

```javascript
{
  name: 'some-package',
  'dist-tags': {
    foo: '1.0.1'
  },
  versions: {
    '1.0.0': { version: '1.0.0' },
    '1.0.1': { version: '1.0.1' },
    '1.0.2': { version: '1.0.2' },
    '2.0.0': { version: '2.0.0' }
  }
}
```

The algorithm will follow npm's algorithm for semver resolution, and only `tag`,
`range`, and `version` selectors are supported.

The function will throw `ETARGET` if there was no matching manifest, and
`ENOVERSIONS` if the packument object has no valid versions in `versions`.

If `opts.defaultTag` is provided, it will be used instead of `latest`. That is,
if that tag matches the selector, it will be used, even if a higher available
version matches the range.

If `opts.enjoyBy` is provided, it should be something that can be passed to `new
Date(x)`, such as a `Date` object or a timestamp string. It will be used to
filter the selected versions such that only versions less than or equal to
`enjoyBy` are considered.

If `opts.includeDeprecated` passed in as true, deprecated versions will be
selected. By default, deprecated versions other than `defaultTag` are ignored.
