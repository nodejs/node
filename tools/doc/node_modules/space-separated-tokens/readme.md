# space-separated-tokens [![Build Status][build-badge]][build-page] [![Coverage Status][coverage-badge]][coverage-page]

Parse and stringify space-separated tokens according to the [spec][].

## Installation

[npm][]:

```bash
npm install space-separated-tokens
```

## Usage

```javascript
var spaceSeparated = require('space-separated-tokens')

spaceSeparated.parse(' foo\tbar\nbaz  ')
//=> ['foo', 'bar', 'baz']

spaceSeparated.stringify(['foo', 'bar', 'baz'])
//=> 'foo bar baz'
```

## API

### `spaceSeparated.parse(value)`

Parse space-separated tokens to an array of strings, according to the [spec][].

###### Parameters

*   `value` (`string`) — space-separated tokens.

###### Returns

`Array.<string>` — List of tokens.

### `spaceSeparated.stringify(values)`

Compile an array of strings to space-separated tokens.
Note that it’s not possible to specify empty or white-space only values.

###### Parameters

*   `values` (`Array.<string>`) — List of tokens.

###### Returns

`string` — Space-separated tokens.

## Related

*   [`collapse-white-space`](https://github.com/wooorm/collapse-white-space)
    — Replace multiple white-space characters with a single space
*   [`property-information`](https://github.com/wooorm/property-information)
    — Information on HTML properties
*   [`comma-separated-tokens`](https://github.com/wooorm/comma-separated-tokens)
    — Parse/stringify comma-separated tokens

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definition -->

[build-badge]: https://img.shields.io/travis/wooorm/space-separated-tokens.svg

[build-page]: https://travis-ci.org/wooorm/space-separated-tokens

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/space-separated-tokens.svg

[coverage-page]: https://codecov.io/github/wooorm/space-separated-tokens?branch=master

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[spec]: https://html.spec.whatwg.org/#space-separated-tokens
