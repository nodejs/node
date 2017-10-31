# Tar Pack

Package and un-package modules of some sort (in tar/gz bundles).  This is mostly useful for package managers.  Note that it doesn't check for or touch `package.json` so it can be used even if that's not the way you store your package info.

[![Build Status](https://img.shields.io/travis/ForbesLindesay/tar-pack/master.svg)](https://travis-ci.org/ForbesLindesay/tar-pack)
[![Dependency Status](https://img.shields.io/david/ForbesLindesay/tar-pack.svg)](https://david-dm.org/ForbesLindesay/tar-pack)
[![NPM version](https://img.shields.io/npm/v/tar-pack.svg)](https://www.npmjs.com/package/tar-pack)

## Installation

    $ npm install tar-pack

## API

### pack(folder|packer, [options])

Pack the folder at `folder` into a gzipped tarball and return the tgz as a stream.  Files ignored by `.gitignore` will not be in the package.

You can optionally pass a `fstream.DirReader` directly, instead of folder.  For example, to create an npm package, do:

```js
pack(require("fstream-npm")(folder), [options])
```

Options:

 - `noProprietary` (defaults to `false`) Set this to `true` to prevent any proprietary attributes being added to the tarball.  These attributes are allowed by the spec, but may trip up some poorly written tarball parsers.
 - `fromBase` (defaults to `false`) Set this to `true` to make sure your tarballs root is the directory you pass in.
 - `ignoreFiles` (defaults to `['.gitignore']`) These files can specify files to be excluded from the package using the syntax of `.gitignore`.  This option is ignored if you parse a `fstream.DirReader` instead of a string for folder.
 - `filter` (defaults to `entry => true`) A function that takes an entry and returns `true` if it should be included in the package and `false` if it should not.  Entryies are of the form `{path, basename, dirname, type}` where (type is "Directory" or "File").  This function is ignored if you parse a `fstream.DirReader` instead of a string for folder.

Example:

```js
var write = require('fs').createWriteStream
var pack = require('tar-pack').pack
pack(process.cwd())
  .pipe(write(__dirname + '/package.tar.gz'))
  .on('error', function (err) {
    console.error(err.stack)
  })
  .on('close', function () {
    console.log('done')
  })
```

### unpack(folder, [options,] cb)

Return a stream that unpacks a tarball into a folder at `folder`.  N.B. the output folder will be removed first if it already exists.

The callback is called with an optional error and, as its second argument, a string which is one of:

 - `'directory'`, indicating that the extracted package was a directory (either `.tar.gz` or `.tar`)
 - `'file'`, incating that the extracted package was just a single file (extracted to `defaultName`, see options)

Basic Options:

 - `defaultName` (defaults to `index.js`) If the package is a single file, rather than a tarball, it will be "extracted" to this file name, set to `false` to disable.

Advanced Options (you probably don't need any of these):

 - `gid` - (defaults to `null`) the `gid` to use when writing files
 - `uid` - (defaults to `null`) the `uid` to use when writing files
 - `dmode` - (defaults to `0777`) The mode to use when creating directories
 - `fmode` - (defaults to `0666`) The mode to use when creating files
 - `unsafe` - (defaults to `false`) (on non win32 OSes it overrides `gid` and `uid` with the current processes IDs)
 - `strip` - (defaults to `1`) Number of path segments to strip from the root when extracting
 - `keepFiles` - (defaults to `false`) Set this to `true` to prevent target directory to be removed. Extracted files overwrite existing files.

Example:

```js
var read = require('fs').createReadStream
var unpack = require('tar-pack').unpack
read(process.cwd() + '/package.tar.gz')
  .pipe(unpack(__dirname + '/package/', function (err) {
    if (err) console.error(err.stack)
    else console.log('done')
  }))
```

## License

  BSD
