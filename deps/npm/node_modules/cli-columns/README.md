# `cli-columns`

[![NPM version][npm-img]][npm-url] [![Downloads][downloads-img]][npm-url] [![Build Status][travis-img]][travis-url] [![Coverage Status][coveralls-img]][coveralls-url] [![Chat][gitter-img]][gitter-url] [![Tip][amazon-img]][amazon-url]

Columnated lists for the CLI. Unicode and ANSI safe.

## Install

    $ npm install --save cli-columns

## Usage

```js
const chalk = require('chalk');
const columns = require('.');

const values = [
    'blue' + chalk.bgBlue('berry'),
    '笔菠萝' + chalk.yellow('苹果笔'),
    chalk.red('apple'), 'pomegranate',
    'durian', chalk.green('star fruit'),
    'パイナップル', 'apricot', 'banana',
    'pineapple', chalk.bgRed.yellow('orange')
];

console.log(columns(values));
```

<img alt="screenshot" src="https://user-images.githubusercontent.com/155164/28672800-bd415c86-72ae-11e7-855c-6f6aa108921b.png">

## API

### columns(values [, options]): String

- `values` `{Array<String>}` Array of strings to display.
- `options` `{Object}`
  - `character` `{String}` (default: `' '`) Padding character.
  - `newline` `{String}` (default: `'\n'`) Newline character.
  - `padding` `{Number}` (default: `2`) Space between columns.
  - `sort` `{Boolean}` (default: `true`) Whether to sort results.
  - `width` `{Number}` (default: `process.stdout.columns`) Max width of list.

Sorts and formats a list of values into columns suitable to display in a given width.

## Contribute

Standards for this project, including tests, code coverage, and semantics are enforced with a build tool. Pull requests must include passing tests with 100% code coverage and no linting errors.

### Test

    $ npm test

----

© Shannon Moeller <me@shannonmoeller.com> (shannonmoeller.com)

Licensed under [MIT](http://shannonmoeller.com/mit.txt)

[amazon-img]:    https://img.shields.io/badge/amazon-tip_jar-yellow.svg?style=flat-square
[amazon-url]:    https://www.amazon.com/gp/registry/wishlist/1VQM9ID04YPC5?sort=universal-price
[coveralls-img]: http://img.shields.io/coveralls/shannonmoeller/cli-columns/master.svg?style=flat-square
[coveralls-url]: https://coveralls.io/r/shannonmoeller/cli-columns
[downloads-img]: http://img.shields.io/npm/dm/cli-columns.svg?style=flat-square
[gitter-img]:    http://img.shields.io/badge/gitter-join_chat-1dce73.svg?style=flat-square
[gitter-url]:    https://gitter.im/shannonmoeller/shannonmoeller
[npm-img]:       http://img.shields.io/npm/v/cli-columns.svg?style=flat-square
[npm-url]:       https://npmjs.org/package/cli-columns
[travis-img]:    http://img.shields.io/travis/shannonmoeller/cli-columns.svg?style=flat-square
[travis-url]:    https://travis-ci.org/shannonmoeller/cli-columns
