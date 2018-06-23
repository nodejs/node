# kebab-case

[![Build status][travis-image]][travis-url] [![NPM version][npm-image]][npm-url] [![XO code style][codestyle-image]][codestyle-url]

> Convert a string to kebab-case, i.e. its dash separated form

The difference between `kebab-case` and e.g. [`dashify`](https://www.npmjs.com/package/dashify) is that this
module doesn't modify the string in any other way than transforming uppercased letters to their lowercased
counterparts prefixed with `-`. Thanks to this there's also a [`reverse`](#kebabcasereversestr) function to
do the opposite, i.e. get back the original value.

This is used in [Unistyle](https://github.com/joakimbeng/unistyle) to transform JavaScript CSS properties
to their CSS counterparts without losing a possible browser prefix, e.g: `WebkitTransform -> -webkit-transform`.

## Installation

Install `kebab-case` using [npm](https://www.npmjs.com/):

```bash
npm install --save kebab-case
```

## Usage

### Module usage

```javascript
var kebabCase = require('kebab-case');

kebabCase('WebkitTransform');
// "-webkit-transform"
kebabCase.reverse('-webkit-transform');
// "WebkitTransform"
```

## API

### `kebabCase(str)`

| Name | Type | Description |
|------|------|-------------|
| str | `String` | The string to convert |

Returns: `String`, the kebab cased string.

### `kebabCase.reverse(str)`

| Name | Type | Description |
|------|------|-------------|
| str | `String` | The string to convert back |

Returns: `String`, the "unkebab cased" string.

## License

MIT © [Joakim Carlstein](http://joakim.beng.se/)

[npm-url]: https://npmjs.org/package/kebab-case
[npm-image]: https://badge.fury.io/js/kebab-case.svg
[travis-url]: https://travis-ci.org/joakimbeng/kebab-case
[travis-image]: https://travis-ci.org/joakimbeng/kebab-case.svg?branch=master
[codestyle-url]: https://github.com/sindresorhus/xo
[codestyle-image]: https://img.shields.io/badge/code_style-XO-5ed9c7.svg?style=flat
