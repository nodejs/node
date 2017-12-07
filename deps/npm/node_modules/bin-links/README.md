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

### Example

```javascript
// todo
```

### Features

* Links bin files listed under the `bin` property of pkg to the node_modules/.bin
directory of the installing environment.
* Links man files listed under the `man` property of pkg to the share/man directory
of the provided optional directory prefix.

### Contributing

The npm team enthusiastically welcomes contributions and project participation!
There's a bunch of things you can do if you want to contribute! The [Contributor
Guide](CONTRIBUTING.md) has all the information you need for everything from
reporting bugs to contributing entire new features. Please don't hesitate to
jump in if you'd like to, or even ask us questions if something isn't clear.

### API

#### <a name="binLinks"></a> `> binLinks(pkg, folder, global, opts, cb)`

##### Example

```javascript
binLinks(pkg, folder, global, opts, cb)
```