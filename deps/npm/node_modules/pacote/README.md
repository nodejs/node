# pacote [![npm version](https://img.shields.io/npm/v/pacote.svg)](https://npm.im/pacote) [![license](https://img.shields.io/npm/l/pacote.svg)](https://npm.im/pacote) [![Travis](https://img.shields.io/travis/zkat/pacote.svg)](https://travis-ci.org/zkat/pacote) [![AppVeyor](https://ci.appveyor.com/api/projects/status/github/zkat/pacote?svg=true)](https://ci.appveyor.com/project/zkat/pacote) [![Coverage Status](https://coveralls.io/repos/github/zkat/pacote/badge.svg?branch=latest)](https://coveralls.io/github/zkat/pacote?branch=latest)

[`pacote`](https://github.com/zkat/pacote) is a Node.js library for downloading
[npm](https://npmjs.org)-compatible packages. It supports all package specifier
syntax that `npm install` and its ilk support. It transparently caches anything
needed to reduce excess operations, using [`cacache`](https://npm.im/cacache).

## Install

`$ npm install --save pacote`

## Table of Contents

* [Example](#example)
* [Features](#features)
* [Contributing](#contributing)
* [API](#api)
  * [`manifest`](#manifest)
  * [`extract`](#extract)
  * [`tarball`](#tarball)
  * [`tarball.stream`](#tarball-stream)
  * [`tarball.toFile`](#tarball-to-file)
  * [`prefetch`](#prefetch)
  * [`options`](#options)
  * [`clearMemoized`](#clearMemoized)

### Example

```javascript
const pacote = require('pacote')

pacote.manifest('pacote@^1').then(pkg => {
  console.log('package manifest for registry pkg:', pkg)
  // { "name": "pacote", "version": "1.0.0", ... }
})

pacote.extract('http://hi.com/pkg.tgz', './here').then(() => {
  console.log('remote tarball contents extracted to ./here')
})
```

### Features

* Handles all package types [npm](https://npm.im/npm) does
* [high-performance, reliable, verified local cache](https://npm.im/cacache)
* offline mode
* authentication support (private git, private npm registries, etc)
* github, gitlab, and bitbucket-aware
* semver range support for git dependencies

### Contributing

The pacote team enthusiastically welcomes contributions and project participation! There's a bunch of things you can do if you want to contribute! The [Contributor Guide](CONTRIBUTING.md) has all the information you need for everything from reporting bugs to contributing entire new features. Please don't hesitate to jump in if you'd like to, or even ask us questions if something isn't clear.

### API

#### <a name="manifest"></a> `> pacote.manifest(spec, [opts])`

Fetches the *manifest* for a package. Manifest objects are similar and based
on the `package.json` for that package, but with pre-processed and limited
fields. The object has the following shape:

```javascript
{
  "name": PkgName,
  "version": SemverString,
  "dependencies": { PkgName: SemverString },
  "optionalDependencies": { PkgName: SemverString },
  "devDependencies": { PkgName: SemverString },
  "peerDependencies": { PkgName: SemverString },
  "bundleDependencies": false || [PkgName],
  "bin": { BinName: Path },
  "_resolved": TarballSource, // different for each package type
  "_integrity": SubresourceIntegrityHash,
  "_shrinkwrap": null || ShrinkwrapJsonObj
}
```

Note that depending on the spec type, some additional fields might be present.
For example, packages from `registry.npmjs.org` have additional metadata
appended by the registry.

##### Example

```javascript
pacote.manifest('pacote@1.0.0').then(pkgJson => {
  // fetched `package.json` data from the registry
})
```

#### <a name="extract"></a> `> pacote.extract(spec, destination, [opts])`

Extracts package data identified by `<spec>` into a directory named
`<destination>`, which will be created if it does not already exist.

If `opts.digest` is provided and the data it identifies is present in the cache,
`extract` will bypass most of its operations and go straight to extracting the
tarball.

##### Example

```javascript
pacote.extract('pacote@1.0.0', './woot', {
  digest: 'deadbeef'
}).then(() => {
  // Succeeds as long as `pacote@1.0.0` still exists somewhere. Network and
  // other operations are bypassed entirely if `digest` is present in the cache.
})
```

#### <a name="tarball"></a> `> pacote.tarball(spec, [opts])`

Fetches package data identified by `<spec>` and returns the data as a buffer.

This API has two variants:

* `pacote.tarball.stream(spec, [opts])` - Same as `pacote.tarball`, except it returns a stream instead of a Promise.
* `pacote.tarball.toFile(spec, dest, [opts])` - Instead of returning data directly, data will be written directly to `dest`, and create any required directories along the way.

##### Example

```javascript
pacote.tarball('pacote@1.0.0', { cache: './my-cache' }).then(data => {
  // data is the tarball data for pacote@1.0.0
})
```

#### <a name="tarball-stream"></a> `> pacote.tarball.stream(spec, [opts])`

Same as `pacote.tarball`, except it returns a stream instead of a Promise.

##### Example

```javascript
pacote.tarball.stream('pacote@1.0.0')
.pipe(fs.createWriteStream('./pacote-1.0.0.tgz'))
```

#### <a name="tarball-to-file"></a> `> pacote.tarball.toFile(spec, dest, [opts])`

Like `pacote.tarball`, but instead of returning data directly, data will be
written directly to `dest`, and create any required directories along the way.

##### Example

```javascript
pacote.tarball.toFile('pacote@1.0.0', './pacote-1.0.0.tgz')
.then(() => /* pacote tarball written directly to ./pacote-1.0.0.tgz */)
```

#### <a name="prefetch"></a> `> pacote.prefetch(spec, [opts])`

##### THIS API IS DEPRECATED. USE `pacote.tarball()` INSTEAD

Fetches package data identified by `<spec>`, usually for the purpose of warming
up the local package cache (with `opts.cache`). It does not return anything.

##### Example

```javascript
pacote.prefetch('pacote@1.0.0', { cache: './my-cache' }).then(() => {
  // ./my-cache now has both the manifest and tarball for `pacote@1.0.0`.
})
```

#### <a name="clearMemoized"></a> `> pacote.clearMemoized()`

This utility function can be used to force pacote to release its references
to any memoized data in its various internal caches. It might help free
some memory.

```javascript
pacote.manifest(...).then(() => pacote.clearMemoized)

```

#### <a name="options"></a> `> options`

##### `opts.integrity`

If provided, pacote will confirm that the relevant integrity hash for each
operation's results matches the given digest. The call will return `EINTEGRITY`
if the check fails.

Additionally, `pacote.extract` will use this integrity string check the cache
directly for matching contents before performing any other operations.

##### `opts.cache`
##### `opts.cacheUid`/`opts.cacheGid`
##### `opts.uid`/`opts.gid`
##### `opts.scope`
##### `opts.registry`
##### `opts.@somescope:registry`
##### `opts.auth`
##### `opts.log`
##### `opts.maxSockets`

Default: `silentNpmLog`

An [`npmlog`](https://npm.im/npmlog)-compatible logger. Will be used to log
various events at the levels specified by `npmlog`.
