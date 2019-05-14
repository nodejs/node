# stringify-package [![npm version](https://img.shields.io/npm/v/stringify-package.svg)](https://npm.im/stringify-package) [![license](https://img.shields.io/npm/l/stringify-package.svg)](https://npm.im/stringify-package) [![Travis](https://img.shields.io/travis/npm/stringify-package/latest.svg)](https://travis-ci.org/npm/stringify-package) [![AppVeyor](https://img.shields.io/appveyor/ci/npm/stringify-package/latest.svg)](https://ci.appveyor.com/project/npm/stringify-package) [![Coverage Status](https://coveralls.io/repos/github/npm/stringify-package/badge.svg?branch=latest)](https://coveralls.io/github/npm/stringify-package?branch=latest)

[`stringify-package`](https://github.com/npm/stringify-package) is a standalone
library for writing out package data as a JSON file. It is extracted from npm.

## Install

`$ npm install stringify-package`

## Table of Contents

* [Example](#example)
* [Features](#features)
* [Contributing](#contributing)
* [API](#api)
  * [`stringifyPackage`](#stringifypackage)

### Example

```javascript
const fs = require('fs')
const pkg = { /* ... */ }

fs.writeFile('package.json', stringifyPackage(pkg), 'utf8', cb(err) => {
    // ...
})
```

### Features

* Ensures consistent file indentation
  To match existing file indentation,
  [`detect-indent`](https://npm.im/detect-indent) is recommended.

* Ensures consistent newlines
  To match existing newline characters,
  [`detect-newline`](https://npm.im/detect-newline) is recommended.

### Contributing

The npm team enthusiastically welcomes contributions and project participation!
There's a bunch of things you can do if you want to contribute! The [Contributor
Guide](CONTRIBUTING.md) has all the information you need for everything from
reporting bugs to contributing entire new features. Please don't hesitate to
jump in if you'd like to, or even ask us questions if something isn't clear.

### API

### <a name="stringifypackage"></a> `> stringifyPackage(data, indent, newline) -> String`

#### Arguments

* `data` - the package data as an object to be stringified
* `indent` - the number of spaces to use for each level of indentation (defaults to 2)
* `newline` - the character(s) to be used as a line terminator
