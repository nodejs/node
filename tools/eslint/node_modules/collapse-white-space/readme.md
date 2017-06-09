# collapse-white-space [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

<!--lint disable heading-increment list-item-spacing no-duplicate-headings-->

Replace multiple white-space characters with a single space.

## Installation

[npm][npm-install]:

```bash
npm install collapse-white-space
```

## Usage

Dependencies.

```javascript
var collapse = require('collapse-white-space');
```

Collapse white space:

```javascript
var result = collapse('\tfoo \n\tbar  \t\r\nbaz');
```

Yields:

```text
 foo bar baz
```

## API

### `collapse(value)`

Replace multiple white-space characters with a single space.

###### Parameters

*   `value` (`string`) — Value with uncollapsed white-space, coerced to string.

###### Returns

`string` — Value with collapsed white-space.

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
