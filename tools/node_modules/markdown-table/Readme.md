# markdown-table [![Build Status](https://img.shields.io/travis/wooorm/markdown-table.svg?style=flat)](https://travis-ci.org/wooorm/markdown-table) [![Coverage Status](https://img.shields.io/coveralls/wooorm/markdown-table.svg?style=flat)](https://coveralls.io/r/wooorm/markdown-table?branch=master)

Generate fancy [Markdown](https://help.github.com/articles/github-flavored-markdown/#tables)/ASCII tables.

## Installation

[npm](https://docs.npmjs.com/cli/install):

```bash
$ npm install markdown-table
```

[Component.js](https://github.com/componentjs/component):

```bash
$ component install wooorm/markdown-table
```

[Bower](http://bower.io/#install-packages):

```bash
$ bower install markdown-table
```

[Duo](http://duojs.org/#getting-started):

```javascript
var table = require('wooorm/markdown-table');
```

## Usage

```javascript
var table = require('markdown-table');

/**
 * Normal usage (defaults to left-alignment):
 */

table([
    ['Branch', 'Commit'],
    ['master', '0123456789abcdef'],
    ['staging', 'fedcba9876543210']
]);
/*
 * | Branch  | Commit           |
 * | ------- | ---------------- |
 * | master  | 0123456789abcdef |
 * | staging | fedcba9876543210 |
 */

/**
 * With alignment:
 */

table([
    ['Beep', 'No.', 'Boop'],
    ['beep', '1024', 'xyz'],
    ['boop', '3388450', 'tuv'],
    ['foo', '10106', 'qrstuv'],
    ['bar', '45', 'lmno']
], {
    'align': ['l', 'c', 'r']
});
/*
 * | Beep |   No.   |   Boop |
 * | :--- | :-----: | -----: |
 * | beep |   1024  |    xyz |
 * | boop | 3388450 |    tuv |
 * | foo  |  10106  | qrstuv |
 * | bar  |    45   |   lmno |
 */

/**
 * Alignment on dots:
 */

table([
    ['No.'],
    ['0.1.2'],
    ['11.22.33'],
    ['5.6.'],
    ['1.22222'],
], {
    'align': '.'
});
/*
 * |    No.      |
 * | :---------: |
 * |   0.1.2     |
 * | 11.22.33    |
 * |   5.6.      |
 * |     1.22222 |
 */
```

## API

### markdownTable(table, options?)

Turns a given matrix of strings (an array of arrays of strings) into a table.

The following options are available:

- `options.align`  — String or array of strings, the strings being either `"l"` (left), `"r"` (right), `c` (center), or `.` (dot). Other values are treated as `""`, which doesn’t place the colon but does left align. _Only the lowercased first character is used, so `Right` is fine_;
- `options.delimiter` — Value to insert between cells. Carefull, non-pipe values will break GitHub Flavored Markdown;
- `options.start` — Value to insert at the beginning of every row.
- `options.end` — Value to insert at the end of every row.
- `options.rule` — Whether to display a rule between the header and the body of the table. Carefull, will break GitHub Flavored Markdown when `false`;
- `options.stringLength` — The method to detect the length of a cell (see below).

### options.stringLength(cell)

ANSI-sequences mess up tables on terminals. To fix this, you have to pass in a `stringLength` option to detect the “visible” length of a cell.

```javascript
var chalk = require('chalk');

function stringLength(cell) {
    return chalk.stripColor(cell).length;
}
```

See the [tests for an example](test.js#L368-L375).

## Inspiration

The original idea and basic implementation was inspired by James Halliday's [text-table](https://github.com/substack/text-table) library.

## License

[MIT](LICENSE) © [Titus Wormer](http://wooorm.com)
