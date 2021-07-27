# libnpmpublish [![npm version](https://img.shields.io/npm/v/libnpmpublish.svg)](https://npm.im/libnpmpublish) [![license](https://img.shields.io/npm/l/libnpmpublish.svg)](https://npm.im/libnpmpublish) [![Travis](https://img.shields.io/travis/npm/libnpmpublish.svg)](https://travis-ci.org/npm/libnpmpublish) [![AppVeyor](https://ci.appveyor.com/api/projects/status/github/zkat/libnpmpublish?svg=true)](https://ci.appveyor.com/project/zkat/libnpmpublish) [![Coverage Status](https://coveralls.io/repos/github/npm/libnpmpublish/badge.svg?branch=latest)](https://coveralls.io/github/npm/libnpmpublish?branch=latest)

[`libnpmpublish`](https://github.com/npm/libnpmpublish) is a Node.js library for
programmatically publishing and unpublishing npm packages. It does not take care
of packing tarballs from source code, but once you have a tarball, it can take
care of putting it up on a nice registry for you.

## Example

```js
const { publish, unpublish } = require('libnpmpublish')

```

## Install

`$ npm install libnpmpublish`

## Table of Contents

* [Example](#example)
* [Install](#install)
* [API](#api)
  * [publish/unpublish opts](#opts)
  * [`publish()`](#publish)
  * [`unpublish()`](#unpublish)

### API

#### <a name="opts"></a> `opts` for `libnpmpublish` commands

`libnpmpublish` uses [`npm-registry-fetch`](https://npm.im/npm-registry-fetch).
Most options are passed through directly to that library, so please refer to
[its own `opts`
documentation](https://www.npmjs.com/package/npm-registry-fetch#fetch-options)
for options that can be passed in.

A couple of options of note for those in a hurry:

* `opts.token` - can be passed in and will be used as the authentication token for the registry. For other ways to pass in auth details, see the n-r-f docs.
* `opts.Promise` - If you pass this in, the Promises returned by `libnpmpublish` commands will use this Promise class instead. For example: `{Promise: require('bluebird')}`

#### <a name="publish"></a> `> libpub.publish(pkgJson, tarData, [opts]) -> Promise`

Publishes `tarData` to the appropriate configured registry. `pkgJson` should be
the parsed `package.json` for the package that is being published.

`tarData` can be a Buffer, a base64-encoded string, or a binary stream of data.
Note that publishing itself can't be streamed, so the entire stream will be
consumed into RAM before publishing (and are thus limited in how big they can
be).

Since `libnpmpublish` does not generate tarballs itself, one way to build your
own tarball for publishing is to do `npm pack` in the directory you wish to
pack. You can then `fs.createReadStream('my-proj-1.0.0.tgz')` and pass that to
`libnpmpublish`, along with `require('./package.json')`.

`publish()` does its best to emulate legacy publish logic in the standard npm
client, and so should generally be compatible with any registry the npm CLI has
been able to publish to in the past.

If `opts.npmVersion` is passed in, it will be used as the `_npmVersion` field in
the outgoing packument. It's recommended you add your own user agent string in
there!

If `opts.algorithms` is passed in, it should be an array of hashing algorithms
to generate `integrity` hashes for. The default is `['sha512']`, which means you
end up with `dist.integrity = 'sha512-deadbeefbadc0ffee'`. Any algorithm
supported by your current node version is allowed -- npm clients that do not
support those algorithms will simply ignore the unsupported hashes.

If `opts.access` is passed in, it must be one of `public` or `restricted`.
Unscoped packages cannot be `restricted`, and the registry may agree or disagree
with whether you're allowed to publish a restricted package.

##### Example

```javascript
const pkg = require('./dist/package.json')
const tarball = fs.createReadStream('./dist/pkg-1.0.1.tgz')
await libpub.publish(pkg, tarball, {
  npmVersion: 'my-pub-script@1.0.2',
  token: 'my-auth-token-here'
})
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
