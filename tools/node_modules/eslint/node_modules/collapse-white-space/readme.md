# collapse-white-space [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Replace multiple white-space characters with a single space.

## Installation

[npm][npm-install]:

```bash
npm install collapse-white-space
```

## Usage

```javascript
var collapse = require('collapse-white-space')

collapse('\tfoo \n\tbar  \t\r\nbaz') //=> ' foo bar baz'
```

## API

### `collapse(value)`

Replace multiple white-space characters in value with a single space.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/collapse-white-space.svg

[travis]: https://travis-ci.org/wooorm/collapse-white-space

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/collapse-white-space.svg

[codecov]: https://codecov.io/github/wooorm/collapse-white-space

[npm-install]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com
