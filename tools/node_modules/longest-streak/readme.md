# longest-streak [![Build Status](https://img.shields.io/travis/wooorm/longest-streak.svg?style=flat)](https://travis-ci.org/wooorm/longest-streak) [![Coverage Status](https://img.shields.io/coveralls/wooorm/longest-streak.svg?style=flat)](https://coveralls.io/r/wooorm/longest-streak?branch=master)

Count the longest repeating streak of a character.

## Installation

[npm](https://docs.npmjs.com/cli/install):

```bash
npm install longest-streak
```

**longest-streak** is also available for [bower](http://bower.io/#install-packages),
[component](https://github.com/componentjs/component), [duo](http://duojs.org/#getting-started),
and for AMD, CommonJS, and globals ([uncompressed](longest-streak.js) and
[compressed](longest-streak.min.js)).

## Usage

Dependencies.

```javascript
var longestStreak = require('longest-streak');
```

Process:

```javascript
longestStreak('` foo `` bar `', '`') // 2
```

## API

### longestStreak(value, character)

Get the count of the longest repeating streak of `character` in `value`.

Parameters:

*   `value` (`string`) — Content, coerced to string.
*   `character` (`string`) — Single character to look for.

Returns: `number` — Number of characters at the place where `character`
occurs in its longest streak in `value`.

Throws:

*   `Error` — when `character` is not a single character string.

## License

[MIT](LICENSE) @ [Titus Wormer](http://wooorm.com)
