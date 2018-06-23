# hast-util-is-element [![Build Status][build-badge]][build-page] [![Coverage Status][coverage-badge]][coverage-page]

Check if a [node][] is a (certain) [**HAST**][hast] [element][].

## Installation

[npm][]:

```bash
npm install hast-util-is-element
```

## Usage

```javascript
var is = require('hast-util-is-element')

is({type: 'text', value: 'foo'}) // => false

is({type: 'element', tagName: 'a'}, 'a') // => true

is({type: 'element', tagName: 'a'}, ['a', 'area']) // => true
```

## API

### `isElement(node[, tagName|tagNames])`

Check if a [node][] is a (certain) [**HAST**][hast] [element][].

When not given a second parameter, asserts if `node` is an element,
otherwise asserts `node` is an element whose `tagName` matches / is
included in the second parameter.

###### Parameters

*   `node` (`*`) — Value to check;
*   `tagName` (`string`, optional) — Value `node`s `tagName` must match;
*   `tagNames` (`Array.<string>`, optional) — Value including `node`s `tagName`.

###### Returns

`boolean` — whether `node` passes the test.

###### Throws

`Error` — When the second parameter is given but invalid.

## Contribute

See [`contributing.md` in `syntax-tree/hast`][contributing] for ways to get
started.

This organisation has a [Code of Conduct][coc].  By interacting with this
repository, organisation, or community you agree to abide by its terms.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definition -->

[build-badge]: https://img.shields.io/travis/syntax-tree/hast-util-is-element.svg

[build-page]: https://travis-ci.org/syntax-tree/hast-util-is-element

[coverage-badge]: https://img.shields.io/codecov/c/github/syntax-tree/hast-util-is-element.svg

[coverage-page]: https://codecov.io/github/syntax-tree/hast-util-is-element?branch=master

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[hast]: https://github.com/syntax-tree/hast

[node]: https://github.com/syntax-tree/unist#node

[element]: https://github.com/syntax-tree/hast#element

[contributing]: https://github.com/syntax-tree/hast/blob/master/contributing.md

[coc]: https://github.com/syntax-tree/hast/blob/master/code-of-conduct.md
