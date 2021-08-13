# markdown-table

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]

Generate fancy [Markdown][fancy] tables.

## Install

This package is [ESM only](https://gist.github.com/sindresorhus/a39789f98801d908bbc7ff3ecc99d99c):
Node 12+ is needed to use it and it must be `import`ed instead of `require`d.

[npm][]:

```sh
npm install markdown-table
```

## Use

Typical usage (defaults to align left):

```js
import {markdownTable} from 'markdown-table'

markdownTable([
  ['Branch', 'Commit'],
  ['main', '0123456789abcdef'],
  ['staging', 'fedcba9876543210']
])
```

Yields:

```markdown
| Branch  | Commit           |
| ------- | ---------------- |
| main    | 0123456789abcdef |
| staging | fedcba9876543210 |
```

With align:

```js
markdownTable(
  [
    ['Beep', 'No.', 'Boop'],
    ['beep', '1024', 'xyz'],
    ['boop', '3388450', 'tuv'],
    ['foo', '10106', 'qrstuv'],
    ['bar', '45', 'lmno']
  ],
  {align: ['l', 'c', 'r']}
)
```

Yields:

```markdown
| Beep |   No.   |   Boop |
| :--- | :-----: | -----: |
| beep |   1024  |    xyz |
| boop | 3388450 |    tuv |
| foo  |  10106  | qrstuv |
| bar  |    45   |   lmno |
```

## API

This package exports the following identifiers: `markdownTable`.
There is no default export.

### `markdownTable(table[, options])`

Turns a given matrix of strings (an array of arrays of strings) into a table.

##### `options`

###### `options.align`

One style for all columns, or styles for their respective columns (`string` or
`string[]`).
Each style is either `'l'` (left), `'r'` (right), or `'c'` (center).
Other values are treated as `''`, which doesn’t place the colon in the alignment
row but does align left.
*Only the lowercased first character is used, so `Right` is fine.*

###### `options.padding`

Whether to add a space of padding between delimiters and cells (`boolean`,
default: `true`).

When `true`, there is padding:

```markdown
| Alpha | B     |
| ----- | ----- |
| C     | Delta |
```

When `false`, there is no padding:

```markdown
|Alpha|B    |
|-----|-----|
|C    |Delta|
```

###### `options.delimiterStart`

Whether to begin each row with the delimiter (`boolean`, default: `true`).

Note: please don’t use this: it could create fragile structures that aren’t
understandable to some Markdown parsers.

When `true`, there are starting delimiters:

```markdown
| Alpha | B     |
| ----- | ----- |
| C     | Delta |
```

When `false`, there are no starting delimiters:

```markdown
Alpha | B     |
----- | ----- |
C     | Delta |
```

###### `options.delimiterEnd`

Whether to end each row with the delimiter (`boolean`, default: `true`).

Note: please don’t use this: it could create fragile structures that aren’t
understandable to some Markdown parsers.

When `true`, there are ending delimiters:

```markdown
| Alpha | B     |
| ----- | ----- |
| C     | Delta |
```

When `false`, there are no ending delimiters:

```markdown
| Alpha | B
| ----- | -----
| C     | Delta
```

###### `options.alignDelimiters`

Whether to align the delimiters (`boolean`, default: `true`).
By default, they are aligned:

```markdown
| Alpha | B     |
| ----- | ----- |
| C     | Delta |
```

Pass `false` to make them staggered:

```markdown
| Alpha | B |
| - | - |
| C | Delta |
```

###### `options.stringLength`

Method to detect the length of a cell (`Function`, default: `s => s.length`).

Full-width characters and ANSI-sequences all mess up delimiter alignment
when viewing the Markdown source.
To fix this, you have to pass in a `stringLength` option to detect the “visible”
length of a cell (note that what is and isn’t visible depends on your editor).

Without such a function, the following:

```js
markdownTable([
  ['Alpha', 'Bravo'],
  ['中文', 'Charlie'],
  ['👩‍❤️‍👩', 'Delta']
])
```

Yields:

```markdown
| Alpha | Bravo |
| - | - |
| 中文 | Charlie |
| 👩‍❤️‍👩 | Delta |
```

With [`string-width`][string-width]:

```js
import stringWidth from 'string-width'

markdownTable(
  [
    ['Alpha', 'Bravo'],
    ['中文', 'Charlie'],
    ['👩‍❤️‍👩', 'Delta']
  ],
  {stringLength: width}
)
```

Yields:

```markdown
| Alpha | Bravo   |
| ----- | ------- |
| 中文  | Charlie |
| 👩‍❤️‍👩    | Delta   |
```

## Inspiration

The original idea and basic implementation was inspired by James Halliday’s
[`text-table`][text-table] library.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/wooorm/markdown-table/workflows/main/badge.svg

[build]: https://github.com/wooorm/markdown-table/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/markdown-table.svg

[coverage]: https://codecov.io/github/wooorm/markdown-table

[downloads-badge]: https://img.shields.io/npm/dm/markdown-table.svg

[downloads]: https://www.npmjs.com/package/markdown-table

[size-badge]: https://img.shields.io/bundlephobia/minzip/markdown-table.svg

[size]: https://bundlephobia.com/result?p=markdown-table

[npm]: https://docs.npmjs.com/cli/install

[license]: license

[author]: https://wooorm.com

[fancy]: https://help.github.com/articles/github-flavored-markdown/#tables

[text-table]: https://github.com/substack/text-table

[string-width]: https://github.com/sindresorhus/string-width
