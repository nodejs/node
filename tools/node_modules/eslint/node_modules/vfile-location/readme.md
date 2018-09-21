# vfile-location [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Convert between positions (line and column-based) and offsets
(range-based) locations in a [virtual file][vfile].

## Installation

[npm][]:

```bash
npm install vfile-location
```

## Usage

```js
var vfile = require('vfile')
var vfileLocation = require('vfile-location')

var location = vfileLocation(vfile('foo\nbar\nbaz'))

var offset = location.toOffset({line: 3, column: 3}) // => 10
location.toPosition(offset) // => {line: 3, column: 3, offset: 10}
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

## Contribute

See [`contributing.md` in `vfile/vfile`][contributing] for ways to get started.

This organisation has a [Code of Conduct][coc].  By interacting with this
repository, organisation, or community you agree to abide by its terms.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/vfile/vfile-location.svg

[travis]: https://travis-ci.org/vfile/vfile-location

[codecov-badge]: https://img.shields.io/codecov/c/github/vfile/vfile-location.svg

[codecov]: https://codecov.io/github/vfile/vfile-location

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[vfile]: https://github.com/vfile/vfile

[to-offset]: #locationtooffsetposition

[to-position]: #locationtopositionoffset

[position]: https://github.com/syntax-tree/unist#position

[contributing]: https://github.com/vfile/vfile/blob/master/contributing.md

[coc]: https://github.com/vfile/vfile/blob/master/code-of-conduct.md
