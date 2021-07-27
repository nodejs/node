# libnpm

[`libnpm`](https://github.com/npm/libnpm) is the programmatic API for npm.

For bug reports and support, please head over to [npm.community](https://npm.community).


## Install

`$ npm install libnpm`

## Table of Contents

* [Example](#example)
* [Features](#features)
* [API](#api)
  * Fetching Packages and Their Info
    * [`manifest`](https://www.npmjs.com/package/pacote#manifest)
    * [`packument`](https://www.npmjs.com/package/pacote#packument)
    * [`tarball`](https://www.npmjs.com/package/pacote#tarball)
    * [`extract`](https://www.npmjs.com/package/pacote#extract)
    * [`search`](https://npm.im/libnpmsearch)
  * Package-related Registry APIs
    * [`publish`]()
    * [`unpublish`](#unpublish)
    * [`access`](https://npm.im/libnpmaccess)
  * Account-related Registry APIs
    * [`login`](https://www.npmjs.com/package/npm-profile#login)
    * [`adduser`](https://www.npmjs.com/package/npm-profile#adduser)
    * [`profile`](https://npm.im/npm-profile)
    * [`hook`](https://npm.im/libnpmhook)
    * [`team`](https://npm.im/libnpmteam)
    * [`org`](https://npm.im/libnpmorg)
  * Miscellaneous
    * [`parseArg`](https://npm.im/npm-package-arg)
    * [`config`](https://npm.im/libnpmconfig)
    * [`readJSON`](https://npm.im/read-package-json)
    * [`verifyLock`](https://npm.im/lock-verify)
    * [`getPrefix`](https://npm.im/find-npm-prefix)
    * [`logicalTree`](https://npm.im/npm-logical-tree)
    * [`stringifyPackage`](https://npm.im/stringify-package)
    * [`runScript`](https://www.npmjs.com/package/npm-lifecycle)
    * [`log`](https://npm.im/npmlog)
    * [`fetch`](https://npm.im/npm-registry-fetch) (plain ol' client for registry interaction)
    * [`linkBin`](https://npm.im/bin-links)

### Example

```javascript
await libnpm.manifest('libnpm') // => Manifest { name: 'libnpm', ... }
```

### API

This package re-exports the APIs from other packages for convenience. Refer to
the [table of contents](#table-of-contents) for detailed documentation on each
individual exported API.
