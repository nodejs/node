# longest-streak

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]

Count the longest repeating streak of a character.

## Install

This package is ESM only: Node 12+ is needed to use it and it must be `import`ed
instead of `require`d.

[npm][]:

```sh
npm install longest-streak
```

## Use

```js
import {longestStreak} from 'longest-streak'

longestStreak('` foo `` bar `', '`') // => 2
```

## API

This package exports the following identifiers: `longestStreak`.
There is no default export.

### `longestStreak(value, character)`

Get the count of the longest repeating streak of `character` in `value`.

###### Parameters

*   `value` (`string`) — Content, coerced to string.
*   `character` (`string`) — Single character to look for.

###### Returns

`number` — Count of most frequent adjacent `character`s in `value`

###### Throws

*   `Error` — when `character` is not a single character string.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/wooorm/longest-streak/workflows/main/badge.svg

[build]: https://github.com/wooorm/longest-streak/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/longest-streak.svg

[coverage]: https://codecov.io/github/wooorm/longest-streak

[downloads-badge]: https://img.shields.io/npm/dm/longest-streak.svg

[downloads]: https://www.npmjs.com/package/longest-streak

[size-badge]: https://img.shields.io/bundlephobia/minzip/longest-streak.svg

[size]: https://bundlephobia.com/result?p=longest-streak

[npm]: https://docs.npmjs.com/cli/install

[license]: license

[author]: https://wooorm.com
