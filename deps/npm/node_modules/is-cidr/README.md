# is-cidr

Check if a string is a valid CIDR

[![semantic-release](https://img.shields.io/badge/%20%20%F0%9F%93%A6%F0%9F%9A%80-semantic--release-e10079.svg?style=flat-square)](https://github.com/semantic-release/semantic-release)
[![version](https://img.shields.io/npm/v/is-cidr.svg?style=flat-square)](http://npm.im/is-cidr)
[![MIT License](https://img.shields.io/npm/l/is-cidr.svg?style=flat-square)](http://opensource.org/licenses/MIT)
[![travis build](https://img.shields.io/travis/flipjs/is-cidr.svg?style=flat-square)](https://travis-ci.org/flipjs/is-cidr)
[![js-standard-style](https://img.shields.io/badge/code%20style-standard-brightgreen.svg?style=flat-square)](https://github.com/feross/standard)
[![Commitizen friendly](https://img.shields.io/badge/commitizen-friendly-brightgreen.svg?style=flat-square)](http://commitizen.github.io/cz-cli/)
[![downloads](https://img.shields.io/npm/dm/is-cidr.svg?style=flat-square)](http://npm-stat.com/charts.html?package=is-cidr&from=2016-03-24)

## Install

```sh
$ npm install --save is-cidr
```

## Usage

```js
import isCidr from 'is-cidr' // default is isCidrV4
import { isCidrV4, isCidrV6 } from 'is-cidr'
// OR
var isCidrV4 = require('is-cidr').isCidrV4
var isCidrV6 = require('is-cidr').isCidrV6

// is a CIDR v4
isCidr('18.101.25.153/24') // true

// is not a CIDR v4
isCidrV4('999.999.999.999/12') // false

// is a CIDR v6
isCidrV6('fe80:0000:0000:0000:0204:61ff:fe9d:f156') // true

// is not a CIDR v6
isCidrV6('fe80:0000:0000:0000:0204:61ff:fe9d:f156/a') // false
```

## API

### isCidr(string)

Check if a string is CIDR IPv4.

### isCidrV4(string)

Check if a string is CIDR IPv4.

### isCidrV6(string)

Check if a string is CIDR IPv6.

## License

MIT © [Felipe Apostol](https://github.com/flipjs)

