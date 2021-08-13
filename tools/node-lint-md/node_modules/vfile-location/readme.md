# vfile-location

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

Convert between positional (line and column-based) and offsets (range-based)
locations in a [virtual file][vfile].

## Install

This package is [ESM only](https://gist.github.com/sindresorhus/a39789f98801d908bbc7ff3ecc99d99c):
Node 12+ is needed to use it and it must be `import`ed instead of `require`d.

[npm][]:

```sh
npm install vfile-location
```

## Use

```js
import {VFile} from 'vfile'
import {location} from 'vfile-location'

var place = location(new VFile('foo\nbar\nbaz'))

var offset = place.toOffset({line: 3, column: 3}) // => 10
place.toPoint(offset) // => {line: 3, column: 3, offset: 10}
```

## API

This package exports the following identifiers: `place`.
There is no default export.

### `place = location(doc)`

Get transform functions for the given `doc` (`string`) or [`file`][vfile].

Returns an object with [`toOffset`][to-offset] and [`toPoint`][to-point].

### `place.toOffset(point)`

Get the `offset` (`number`) for a line and column-based [`point`][point] in the
bound file.
Returns `-1` when given invalid or out of bounds input.

### `place.toPoint(offset)`

Get the line and column-based [`point`][point] for `offset` in the bound file.

## Contribute

See [`contributing.md`][contributing] in [`vfile/.github`][health] for ways to
get started.
See [`support.md`][support] for ways to get help.

This project has a [code of conduct][coc].
By interacting with this repository, organization, or community you agree to
abide by its terms.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/vfile/vfile-location/workflows/main/badge.svg

[build]: https://github.com/vfile/vfile-location/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/vfile/vfile-location.svg

[coverage]: https://codecov.io/github/vfile/vfile-location

[downloads-badge]: https://img.shields.io/npm/dm/vfile-location.svg

[downloads]: https://www.npmjs.com/package/vfile-location

[size-badge]: https://img.shields.io/bundlephobia/minzip/vfile-location.svg

[size]: https://bundlephobia.com/result?p=vfile-location

[sponsors-badge]: https://opencollective.com/unified/sponsors/badge.svg

[backers-badge]: https://opencollective.com/unified/backers/badge.svg

[collective]: https://opencollective.com/unified

[chat-badge]: https://img.shields.io/badge/chat-discussions-success.svg

[chat]: https://github.com/vfile/vfile/discussions

[npm]: https://docs.npmjs.com/cli/install

[contributing]: https://github.com/vfile/.github/blob/HEAD/contributing.md

[support]: https://github.com/vfile/.github/blob/HEAD/support.md

[health]: https://github.com/vfile/.github

[coc]: https://github.com/vfile/.github/blob/HEAD/code-of-conduct.md

[license]: license

[author]: https://wooorm.com

[vfile]: https://github.com/vfile/vfile

[to-offset]: #placetooffsetpoint

[to-point]: #placetopointoffset

[point]: https://github.com/syntax-tree/unist#point
