# longest-streak [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Count the longest repeating streak of a character.

## Installation

[npm][]:

```bash
npm install longest-streak
```

## Usage

```js
var longestStreak = require('longest-streak');

longestStreak('` foo `` bar `', '`') //=> 2
```

## API

### `longestStreak(value, character)`

Get the count of the longest repeating streak of `character` in `value`.

###### Parameters

*   `value` (`string`) — Content, coerced to string.
*   `character` (`string`) — Single character to look for.

###### Returns

`number` — Number of characters at the place where `character` occurs in
its longest streak in `value`.

###### Throws

*   `Error` — when `character` is not a single character string.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/longest-streak.svg

[travis]: https://travis-ci.org/wooorm/longest-streak

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/longest-streak.svg

[codecov]: https://codecov.io/github/wooorm/longest-streak

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com
