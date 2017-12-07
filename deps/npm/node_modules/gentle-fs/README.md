# gentle-fs [![npm version](https://img.shields.io/npm/v/gentle-fs.svg)](https://npm.im/gentle-fs) [![license](https://img.shields.io/npm/l/gentle-fs.svg)](https://npm.im/gentle-fs) [![Travis](https://img.shields.io/travis/npm/gentle-fs.svg)](https://travis-ci.org/npm/gentle-fs) [![AppVeyor](https://ci.appveyor.com/api/projects/status/github/npm/gentle-fs?svg=true)](https://ci.appveyor.com/project/npm/gentle-fs) [![Coverage Status](https://coveralls.io/repos/github/npm/gentle-fs/badge.svg?branch=latest)](https://coveralls.io/github/npm/gentle-fs?branch=latest)

[`gentle-fs`](https://github.com/npm/gentle-fs) is a standalone library for
"gently" remove or link directories.

## Install

`$ npm install gentle-fs`

## Table of Contents

* [Example](#example)
* [Features](#features)
* [Contributing](#contributing)
* [API](#api)
  * [`rm`](#rm)
  * [`link`](#link)
  * [`linkIfExists`](#linkIfExists)

### Example

```javascript
// todo
```

### Features

* Performs filesystem operations "gently". Please see details in the API specs below
for a more precise definition of "gently".

### Contributing

The npm team enthusiastically welcomes contributions and project participation!
There's a bunch of things you can do if you want to contribute! The [Contributor
Guide](CONTRIBUTING.md) has all the information you need for everything from
reporting bugs to contributing entire new features. Please don't hesitate to
jump in if you'd like to, or even ask us questions if something isn't clear.

### API

#### <a name="rm"></a> `> rm(target, opts, cb)`

Will delete all directories between `target` and `opts.base`, as long as they are empty.
That is, if `target` is `/a/b/c/d/e` and `base` is `/a/b`, but `/a/b/c` has other files
besides the `d` directory inside of it, `/a/b/c` will remain.

##### Example

```javascript
rm(target, opts, cb)
```

#### <a name="link"></a> `> link(from, to, opts, cb)`

If `from` is a real directory, and `from` is not the same directory as `to`, will
symlink `from` to `to`, while also gently [`rm`](#rm)ing the `to` directory,
and then call the callback. Otherwise, will call callback with an `Error`.

##### Example

```javascript
link(from, to, opts, cb)
```

#### <a name="linkIfExists"></a> `> linkIfExists(from, to, opts, cb)`

Performs the same operation as [`link`](#link), except does nothing when `from` is the
same as `to`, and calls the callback.

##### Example

```javascript
linkIfExists(from, to, opts, cb)
```
