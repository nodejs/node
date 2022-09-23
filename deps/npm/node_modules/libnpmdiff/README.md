# libnpmdiff

[![npm version](https://img.shields.io/npm/v/libnpmdiff.svg)](https://npm.im/libnpmdiff)
[![license](https://img.shields.io/npm/l/libnpmdiff.svg)](https://npm.im/libnpmdiff)
[![CI - libnpmdiff](https://github.com/npm/cli/actions/workflows/ci-libnpmdiff.yml/badge.svg)](https://github.com/npm/cli/actions/workflows/ci-libnpmdiff.yml)
The registry diff lib.

## Table of Contents

* [Example](#example)
* [Install](#install)
* [Contributing](#contributing)
* [API](#api)
* [LICENSE](#license)

## Example

```js
const libdiff = require('libnpmdiff')

const patch = await libdiff([
  'abbrev@1.1.0',
  'abbrev@1.1.1'
])
console.log(
  patch
)
```

Returns:

```patch
diff --git a/package.json b/package.json
index v1.1.0..v1.1.1 100644
--- a/package.json	
+++ b/package.json	
@@ -1,6 +1,6 @@
 {
   "name": "abbrev",
-  "version": "1.1.0",
+  "version": "1.1.1",
   "description": "Like ruby's abbrev module, but in js",
   "author": "Isaac Z. Schlueter <i@izs.me>",
   "main": "abbrev.js",

```

## Install

`$ npm install libnpmdiff`

### Contributing

The npm team enthusiastically welcomes contributions and project participation!
There's a bunch of things you can do if you want to contribute! The
[Contributor Guide](https://github.com/npm/cli/blob/latest/CONTRIBUTING.md)
outlines the process for community interaction and contribution. Please don't
hesitate to jump in if you'd like to, or even ask us questions if something
isn't clear.

All participants and maintainers in this project are expected to follow the
[npm Code of Conduct](https://docs.npmjs.com/policies/conduct), and just
generally be excellent to each other.

Please refer to the [Changelog](CHANGELOG.md) for project history details, too.

Happy hacking!

### API

#### `> libnpmdif([ a, b ], [opts]) -> Promise<String>`

Fetches the registry tarballs and compare files between a spec `a` and spec `b`. **npm** spec types are usually described in `<pkg-name>@<version>` form but multiple other types are alsos supported, for more info on valid specs take a look at [`npm-package-arg`](https://github.com/npm/npm-package-arg).

**Options**:

- `color <Boolean>`: Should add ANSI colors to string output? Defaults to `false`.
- `tagVersionPrefix <Sring>`: What prefix should be used to define version numbers. Defaults to `v`
- `diffUnified <Number>`: How many lines of code to print before/after each diff. Defaults to `3`.
- `diffFiles <Array<String>>`: If set only prints patches for the files listed in this array (also accepts globs). Defaults to `undefined`.
- `diffIgnoreAllSpace <Boolean>`: Whether or not should ignore changes in whitespace (very useful to avoid indentation changes extra diff lines). Defaults to `false`.
- `diffNameOnly <Boolean>`: Prints only file names and no patch diffs. Defaults to `false`.
- `diffNoPrefix <Boolean>`: If true then skips printing any prefixes in filenames. Defaults to `false`.
- `diffSrcPrefix <String>`: Prefix to be used in the filenames from `a`. Defaults to `a/`.
- `diffDstPrefix <String>`: Prefix to be used in the filenames from `b`. Defaults to `b/`.
- `diffText <Boolean>`: Should treat all files as text and try to print diff for binary files. Defaults to `false`.
- ...`cache`, `registry`, `where` and other common options accepted by [pacote](https://github.com/npm/pacote#options)

Returns a `Promise` that fullfils with a `String` containing the resulting patch diffs.

Throws an error if either `a` or `b` are missing or if trying to diff more than two specs.

## LICENSE

[ISC](./LICENSE)

