# vfile-message

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

Create [vfile][] messages.

## Install

This package is [ESM only](https://gist.github.com/sindresorhus/a39789f98801d908bbc7ff3ecc99d99c):
Node 12+ is needed to use it and it must be `import`ed instead of `require`d.

[npm][]:

```bash
npm install vfile-message
```

## Use

```js
import {VFileMessage} from 'vfile-message'

var message = new VFileMessage(
  '`braavo` is misspelt; did you mean `bravo`?',
  {line: 1, column: 8},
  'spell:typo'
)

console.log(message)
```

Yields:

```txt
[1:8: `braavo` is misspelt; did you mean `bravo`?] {
  reason: '`braavo` is misspelt; did you mean `bravo`?',
  line: 1,
  column: 8,
  source: 'spell',
  ruleId: 'typo',
  position: {start: {line: 1, column: 8}, end: {line: null, column: null}}
}
```

## API

This package exports the following identifiers: `VFileMessage`.
There is no default export.

### `VFileMessage(reason[, place][, origin])`

Constructor of a message for `reason` at `place` from `origin`.
When an error is passed in as `reason`, copies the stack.

##### Parameters

###### `reason`

Reason for message (`string` or `Error`).
Uses the stack and message of the error if given.

###### `place`

Place at which the message occurred in a file ([`Node`][node],
[`Position`][position], or [`Point`][point], optional).

###### `origin`

Place in code the message originates from (`string`, optional).

Can either be the [`ruleId`][ruleid] (`'rule'`), or a string with both a
[`source`][source] and a [`ruleId`][ruleid] delimited with a colon
(`'source:rule'`).

##### Extends

[`Error`][error].

##### Returns

An instance of itself.

##### Properties

###### `reason`

Reason for message (`string`).

###### `fatal`

If `true`, marks associated file as no longer processable (`boolean?`).
If `false`, necessitates a (potential) change.
The value can also be `null` or `undefined`.

###### `line`

Starting line of error (`number?`).

###### `column`

Starting column of error (`number?`).

###### `position`

Full range information, when available ([`Position`][position]).
Has `start` and `end` properties, both set to an object with `line` and
`column`, set to `number?`.

###### `source`

Namespace of message (`string?`).

###### `ruleId`

Category of message (`string?`).

###### `stack`

Stack of message (`string?`).

##### Custom properties

It’s OK to store custom data directly on the `VFileMessage`, some of those are
handled by [utilities][util].

###### `file`

You may add a `file` property with a path of a file (used throughout the
[**VFile**][vfile] ecosystem).

###### `note`

You may add a `note` property with a long form description of the message
(supported by [`vfile-reporter`][reporter]).

###### `url`

You may add a `url` property with a link to documentation for the message.

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

[build-badge]: https://github.com/vfile/vfile-message/workflows/main/badge.svg

[build]: https://github.com/vfile/vfile-message/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/vfile/vfile-message.svg

[coverage]: https://codecov.io/github/vfile/vfile-message

[downloads-badge]: https://img.shields.io/npm/dm/vfile-message.svg

[downloads]: https://www.npmjs.com/package/vfile-message

[size-badge]: https://img.shields.io/bundlephobia/minzip/vfile-message.svg

[size]: https://bundlephobia.com/result?p=vfile-message

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

[error]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Error

[node]: https://github.com/syntax-tree/unist#node

[position]: https://github.com/syntax-tree/unist#position

[point]: https://github.com/syntax-tree/unist#point

[vfile]: https://github.com/vfile/vfile

[util]: https://github.com/vfile/vfile#utilities

[reporter]: https://github.com/vfile/vfile-reporter

[ruleid]: #ruleid

[source]: #source
