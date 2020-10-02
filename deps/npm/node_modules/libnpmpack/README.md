# libnpmpack

[![npm version](https://img.shields.io/npm/v/libnpmpack.svg)](https://npm.im/libnpmpack)
[![license](https://img.shields.io/npm/l/libnpmpack.svg)](https://npm.im/libnpmpack)
[![GitHub Actions](https://github.com/npm/libnpmpack/workflows/Node%20CI/badge.svg)](https://github.com/npm/libnpmpack/actions?query=workflow%3A%22Node+CI%22)
[![Coverage Status](https://coveralls.io/repos/github/npm/libnpmpack/badge.svg?branch=latest)](https://coveralls.io/github/npm/libnpmpack?branch=latest)

[`libnpmpack`](https://github.com/npm/libnpmpack) is a Node.js library for
programmatically packing tarballs from a local directory or from a registry or github spec. If packing from a local source, `libnpmpack` will also run the `prepack` and `postpack` lifecycles.

## Table of Contents

* [Example](#example)
* [Install](#install)
* [API](#api)
  * [`pack()`](#pack)

## Example

```js
const pack = require('libnpmpack')
```

## Install

`$ npm install libnpmpack`

### API

#### <a name="pack"></a> `> pack(spec, [opts]) -> Promise`

Packs a tarball from a local directory or from a registry or github spec and returns a Promise that resolves to the tarball data Buffer, with from, resolved, and integrity fields attached.

If no options are passed, the tarball file will be saved on the same directory from which `pack` was called in.

`libnpmpack` uses [`pacote`](https://npm.im/pacote).
Most options are passed through directly to that library, so please refer to
[its own `opts`
documentation](https://www.npmjs.com/package/pacote#options)
for options that can be passed in.

##### Examples

```javascript
// packs from cwd
const tarball = await pack()

// packs from a local directory
const localTar = await pack('/Users/claudiahdz/projects/my-cool-pkg')

// packs from a registry spec
const registryTar = await pack('abbrev@1.0.3')

// packs from a github spec
const githubTar = await pack('isaacs/rimraf#PR-192')
```
