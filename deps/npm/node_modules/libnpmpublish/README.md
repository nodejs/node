# libnpmpublish

[![npm version](https://img.shields.io/npm/v/libnpmpublish.svg)](https://npm.im/libnpmpublish)
[![license](https://img.shields.io/npm/l/libnpmpublish.svg)](https://npm.im/libnpmpublish)
[![GitHub Actions](https://github.com/npm/libnpmpublish/workflows/Node%20CI/badge.svg)](https://github.com/npm/libnpmpublish/actions?query=workflow%3A%22Node+CI%22)
[![Coverage Status](https://coveralls.io/repos/github/npm/libnpmpublish/badge.svg?branch=latest)](https://coveralls.io/github/npm/libnpmpublish?branch=latest)

[`libnpmpublish`](https://github.com/npm/libnpmpublish) is a Node.js library for
programmatically publishing and unpublishing npm packages. It takes care
of packing tarballs from source code and putting it up on a nice registry for you.

## Table of Contents

* [Example](#example)
* [Install](#install)
* [API](#api)
  * [publish/unpublish opts](#opts)
  * [`publish()`](#publish)
  * [`unpublish()`](#unpublish)

## Example

```js
const { publish, unpublish } = require('libnpmpublish')

```

## Install

`$ npm install libnpmpublish`

### API

#### <a name="opts"></a> `opts` for `libnpmpublish` commands

`libnpmpublish` uses [`npm-registry-fetch`](https://npm.im/npm-registry-fetch).
Most options are passed through directly to that library, so please refer to
[its own `opts`
documentation](https://www.npmjs.com/package/npm-registry-fetch#fetch-options)
for options that can be passed in.

A couple of options of note for those in a hurry:
* `opts.defaultTag` - registers the published package with the given tag, defaults to `latest`.

* `opts.access` - tells the registry whether this package should be published as public or restricted. Only applies to scoped packages, which default to restricted.

* `opts.token` - can be passed in and will be used as the authentication token for the registry. For other ways to pass in auth details, see the n-r-f docs.

#### <a name="publish"></a> `> libpub.publish(path, pkgJson, [opts]) -> Promise`

Packs a tarball located in `path` and publishes to the appropriate configured registry. `pkgJson` should be
the parsed `package.json` for the package that is being published.

If `opts.npmVersion` is passed in, it will be used as the `_npmVersion` field in
the outgoing packument. It's recommended you add your own user agent string in
there!

If `opts.algorithms` is passed in, it should be an array of hashing algorithms
to generate `integrity` hashes for. The default is `['sha512']`, which means you
end up with `dist.integrity = 'sha512-deadbeefbadc0ffee'`. Any algorithm
supported by your current node version is allowed -- npm clients that do not
support those algorithms will simply ignore the unsupported hashes.

##### Example

```javascript
const path = '/a/path/to/your/source/code'
await libpub.publish(path, {
  npmVersion: 'my-pub-script@1.0.2',
  token: 'my-auth-token-here'
}, opts)
// Package has been published to the npm registry.
```

#### <a name="unpublish"></a> `> libpub.unpublish(spec, [opts]) -> Promise`

Unpublishes `spec` from the appropriate registry. The registry in question may
have its own limitations on unpublishing.

`spec` should be either a string, or a valid
[`npm-package-arg`](https://npm.im/npm-package-arg) parsed spec object. For
legacy compatibility reasons, only `tag` and `version` specs will work as
expected. `range` specs will fail silently in most cases.

##### Example

```javascript
await libpub.unpublish('lodash', { token: 'i-am-the-worst'})
//
// `lodash` has now been unpublished, along with all its versions, and the world
// devolves into utter chaos.
//
// That, or we all go home to our friends and/or family and have a nice time
// doing nothing having to do with programming or JavaScript and realize our
// lives are just so much happier now, and we just can't understand why we ever
// got so into this JavaScript thing but damn did it pay well. I guess you'll
// settle for gardening or something.
```
