# comma-separated-tokens [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Parse and stringify comma-separated tokens according to the [spec][].

## Installation

[npm][]:

```bash
npm install comma-separated-tokens
```

## Usage

```javascript
var commaSeparated = require('comma-separated-tokens')

commaSeparated.parse(' a ,b,,d d ') //=> ['a', 'b', '', 'd d']
commaSeparated.stringify(['a', 'b', '', 'd d']) //=> 'a, b, , d d'
```

## API

### `commaSeparated.parse(value)`

Parse comma-separated tokens (`string`) to an array of strings, according
to the [spec][].

### `commaSeparated.stringify(values[, options])`

Compile an array of strings to comma-separated tokens (`string`).
Handles empty items at start or end correctly.
Note that it’s not possible to specify initial or final
white-space per value.

##### `options`

###### `options.padLeft`

Whether to pad a space before a token (`boolean`, default: `true`).

###### `options.padRight`

Whether to pad a space after a token (`boolean`, default: `false`).

## Related

*   [`collapse-white-space`](https://github.com/wooorm/collapse-white-space)
    — Replace multiple white-space characters with a single space
*   [`property-information`](https://github.com/wooorm/property-information)
    — Information on HTML properties
*   [`space-separated-tokens`](https://github.com/wooorm/space-separated-tokens)
    — Parse/stringify space-separated tokens

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/comma-separated-tokens.svg

[travis]: https://travis-ci.org/wooorm/comma-separated-tokens

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/comma-separated-tokens.svg

[codecov]: https://codecov.io/github/wooorm/comma-separated-tokens

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[spec]: https://html.spec.whatwg.org/#comma-separated-tokens
