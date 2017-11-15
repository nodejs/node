# markdown-table [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Generate fancy [Markdown][fancy]/ASCII tables.

## Installation

[npm][]:

```bash
npm install markdown-table
```

## Usage

Dependencies:

```javascript
var table = require('markdown-table');
```

Normal usage (defaults to left-alignment):

```javascript
table([
  ['Branch', 'Commit'],
  ['master', '0123456789abcdef'],
  ['staging', 'fedcba9876543210']
]);
```

Yields:

```txt
| Branch  | Commit           |
| ------- | ---------------- |
| master  | 0123456789abcdef |
| staging | fedcba9876543210 |
```

With alignment:

```javascript
table([
  ['Beep', 'No.', 'Boop'],
  ['beep', '1024', 'xyz'],
  ['boop', '3388450', 'tuv'],
  ['foo', '10106', 'qrstuv'],
  ['bar', '45', 'lmno']
], {
  align: ['l', 'c', 'r']
});
```

Yields:

```txt
| Beep |   No.   |   Boop |
| :--- | :-----: | -----: |
| beep |   1024  |    xyz |
| boop | 3388450 |    tuv |
| foo  |  10106  | qrstuv |
| bar  |    45   |   lmno |
```

Alignment on dots:

```javascript
table([
  ['No.'],
  ['0.1.2'],
  ['11.22.33'],
  ['5.6.'],
  ['1.22222'],
], {
  align: '.'
});
```

Yields:

```txt
|    No.      |
| :---------: |
|   0.1.2     |
| 11.22.33    |
|   5.6.      |
|     1.22222 |
```

## API

### `markdownTable(table[, options])`

Turns a given matrix of strings (an array of arrays of strings) into a table.

###### `options`

The following options are available:

*   `options.align` (`string` or `Array.<string>`)
    — One style for all columns, or styles for their respective
    columns.  Each style is either `'l'` (left), `'r'` (right), `'c'`
    (centre), or `'.'` (dot).  Other values are treated as `''`, which
    doesn’t place the colon but does left align.  _Only the lowercased
    first character is used, so `Right` is fine_;
*   `options.delimiter` (`string`, default: `' | '`)
    — Value to insert between cells.  Careful, setting this to a
    non-pipe breaks GitHub Flavoured Markdown;
*   `options.start` (`string`, default: `'| '`)
    — Value to insert at the beginning of every row.
*   `options.end` (`string`, default: `' |'`)
    — Value to insert at the end of every row.
*   `options.rule` (`boolean`, default: `true`)
    — Whether to display a rule between the header and the body of the
    table.  Careful, will break GitHub Flavoured Markdown when `false`;
*   `options.stringLength` ([`Function`][length], default:
    `s => s.length`)
    — Method to detect the length of a cell (see below).
*   `options.pad` (`boolean`, default: `true`)
    — Whether to pad the markdown for table cells to make them the same
    width.  Setting this to false will cause the table rows to
    remain staggered.

### `stringLength(cell)`

ANSI-sequences mess up tables on terminals.  To fix this, you have to
pass in a `stringLength` option to detect the “visible” length of a
cell.

```javascript
var chalk = require('chalk');

function stringLength(cell) {
  return chalk.stripColor(cell).length;
}
```

## Inspiration

The original idea and basic implementation was inspired by James
Halliday’s [text-table][] library.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/markdown-table.svg

[travis]: https://travis-ci.org/wooorm/markdown-table

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/markdown-table.svg

[codecov]: https://codecov.io/github/wooorm/markdown-table

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[fancy]: https://help.github.com/articles/github-flavored-markdown/#tables

[length]: #stringlengthcell

[text-table]: https://github.com/substack/text-table
