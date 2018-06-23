# hast-util-parse-selector [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Parse a simple CSS selector to a [HAST][] node.

## Installation

[npm][]:

```bash
npm install hast-util-parse-selector
```

## Usage

```javascript
var parseSelector = require('hast-util-parse-selector')

console.log(parseSelector('.quux#bar.baz.qux'))
```

Yields:

```js
{ type: 'element',
  tagName: 'div',
  properties: { id: 'bar', className: [ 'quux', 'baz', 'qux' ] },
  children: [] }
```

## API

### `parseSelector([selector])`

Parse a CSS `selector` to a [HAST][] node.

###### `selector`

`string`, optional — Can contain a tag-name (`foo`), classes (`.bar`),
and an ID (`#baz`).  Multiple classes are allowed.  Uses the last ID if
multiple IDs are found.

###### Returns

[`Node`][hast].

## Contribute

See [`contributing.md` in `syntax-tree/hast`][contributing] for ways to get
started.

This organisation has a [Code of Conduct][coc].  By interacting with this
repository, organisation, or community you agree to abide by its terms.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/syntax-tree/hast-util-parse-selector.svg

[travis]: https://travis-ci.org/syntax-tree/hast-util-parse-selector

[codecov-badge]: https://img.shields.io/codecov/c/github/syntax-tree/hast-util-parse-selector.svg

[codecov]: https://codecov.io/github/syntax-tree/hast-util-parse-selector

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[hast]: https://github.com/syntax-tree/hast

[contributing]: https://github.com/syntax-tree/hast/blob/master/contributing.md

[coc]: https://github.com/syntax-tree/hast/blob/master/code-of-conduct.md
