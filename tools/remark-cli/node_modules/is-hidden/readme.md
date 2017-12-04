# is-hidden [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Check if `filename` is hidden (starts with a dot).

## Installation

[npm][]:

```bash
npm install is-hidden
```

## Usage

```javascript
var hidden = require('is-hidden');

hidden('.git'); //=> true
hidden('readme.md'); //=> false
```

## API

### `hidden(filename)`

Check if `filename` is hidden (starts with a dot).

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/is-hidden.svg

[travis]: https://travis-ci.org/wooorm/is-hidden

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/is-hidden.svg

[codecov]: https://codecov.io/github/wooorm/is-hidden

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com
