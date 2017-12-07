# npm-lifecycle [![npm version](https://img.shields.io/npm/v/npm-lifecycle.svg)](https://npm.im/npm-lifecycle) [![license](https://img.shields.io/npm/l/npm-lifecycle.svg)](https://npm.im/npm-lifecycle) [![Travis](https://img.shields.io/travis/npm/lifecycle.svg)](https://travis-ci.org/npm/lifecycle) [![AppVeyor](https://ci.appveyor.com/api/projects/status/github/npm/lifecycle?svg=true)](https://ci.appveyor.com/project/npm/lifecycle) [![Coverage Status](https://coveralls.io/repos/github/npm/lifecycle/badge.svg?branch=latest)](https://coveralls.io/github/npm/lifecycle?branch=latest)

[`npm-lifecycle`](https://github.com/npm/lifecycle) is a standalone library for
executing packages' lifecycle scripts. It is extracted from npm itself and
intended to be fully compatible with the way npm executes individual scripts.

## Install

`$ npm install npm-lifecycle`

## Table of Contents

* [Example](#example)
* [Features](#features)
* [Contributing](#contributing)
* [API](#api)
  * [`lifecycle`](#lifecycle)

### Example

```javascript
// idk yet
```

### Features

* something cool

### Contributing

The npm team enthusiastically welcomes contributions and project participation!
There's a bunch of things you can do if you want to contribute! The [Contributor
Guide](CONTRIBUTING.md) has all the information you need for everything from
reporting bugs to contributing entire new features. Please don't hesitate to
jump in if you'd like to, or even ask us questions if something isn't clear.

### API

#### <a name="lifecycle"></a> `> lifecycle(name, pkg, wd, [opts]) -> Promise`

##### Arguments

* `opts.stdio` - the [stdio](https://nodejs.org/api/child_process.html#child_process_options_stdio)
passed to the child process. `[0, 1, 2]` by default.

##### Example

```javascript
lifecycle()
```
