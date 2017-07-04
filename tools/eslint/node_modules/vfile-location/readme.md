# vfile-location [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Convert between positions (line and column-based) and offsets
(range-based) locations in a [virtual file][vfile].

## Installation

[npm][npm-install]:

```bash
npm install vfile-location
```

## Usage

```js
var vfile = require('vfile');
var vfileLocation = require('vfile-location');
var location = vfileLocation(vfile('foo\nbar\nbaz'));

var offset = location.toOffset({line: 3, column: 3});
var position = location.toPosition(offset);
```

Yields:

```js
10
{
  "line": 3,
  "column": 3,
  "offset": 10
}
```

## API

### `location = vfileLocation(doc)`

Get transform functions for the given `doc` (`string`) or
[`file`][vfile].

Returns an object with [`toOffset`][to-offset] and
[`toPosition`][to-position].

### `location.toOffset(position)`

Get the `offset` (`number`) for a line and column-based
[`position`][position] in the bound file.  Returns `-1`
when given invalid or out of bounds input.

### `location.toPosition(offset)`

Get the line and column-based [`position`][position] for `offset` in
the bound file.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/vfile-location.svg

[travis]: https://travis-ci.org/wooorm/vfile-location

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/vfile-location.svg

[codecov]: https://codecov.io/github/wooorm/vfile-location

[npm-install]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[vfile]: https://github.com/wooorm/vfile

[to-offset]: #locationtooffsetposition

[to-position]: #locationtopositionoffset

[position]: https://github.com/wooorm/unist#position
