# markdown-escapes [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

<!--lint disable heading-increment no-duplicate-headings-->

List of escapable characters in markdown.

## Installation

[npm][npm-install]:

```bash
npm install markdown-escapes
```

## Usage

```javascript
var escapes = require('markdown-escapes');

// Access by property:
escapes.commonmark;
// ['\\', '`', ..., '@', '^']

// Access by options object:
escapes({gfm: true});
// ['\\', '`', ..., '~', '|']
```

## API

### `escapes([options])`

Get escapes.  Supports `options.commonmark` and `options.gfm`, which
when `true` returns the extra escape characters supported by those
flavours.

###### Returns

`Array.<string>`.

### `escapes.default`

List of default escapable characters.

### `escapes.gfm`

List of escapable characters in GFM (which includes all `default`s).

### `escapes.commonmark`

List of escapable characters in CommonMark (which includes all `gfm`s).

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/markdown-escapes.svg

[travis]: https://travis-ci.org/wooorm/markdown-escapes

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/markdown-escapes.svg

[codecov]: https://codecov.io/github/wooorm/markdown-escapes

[npm-install]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com
