# vfile-message [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Create [vfile][] messages.

## Installation

[npm][]:

```bash
npm install vfile-message
```

## Usage

```js
var VMessage = require('vfile-message');

var message = new VMessage('`braavo` is misspelt; did you mean `bravo`?', {line: 1, column: 8}, 'spell:typo');

console.log(message);
```

Yields:

```js
{ [1:8: `braavo` is misspelt; did you mean `bravo`?]
  reason: '`braavo` is misspelt; did you mean `bravo`?',
  fatal: null,
  line: 1,
  column: 8,
  location:
   { start: { line: 1, column: 8 },
     end: { line: null, column: null } },
  source: 'spell',
  ruleId: 'typo' }
```

## API

### `VMessage(reason[, position][, origin])`

Constructor of a message for `reason` at `position` from `origin`.  When
an error is passed in as `reason`, copies the stack.

##### Parameters

###### `reason`

Reason for message (`string` or `Error`).  Uses the stack and message of the
error if given.

###### `position`

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

If `true`, marks associated file as no longer processable (`boolean?`).  If
`false`, necessitates a (potential) change.  The value can also be `null` or
`undefined`.

###### `line`

Starting line of error (`number?`).

###### `column`

Starting column of error (`number?`).

###### `location`

Full range information, when available ([`Position`][position]).  Has `start`
and `end` properties, both set to an object with `line` and `column`, set to
`number?`.

###### `source`

Namespace of warning (`string?`).

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

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/vfile/vfile-message.svg

[travis]: https://travis-ci.org/vfile/vfile-message

[codecov-badge]: https://img.shields.io/codecov/c/github/vfile/vfile-message.svg

[codecov]: https://codecov.io/github/vfile/vfile-message

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[error]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Error

[node]: https://github.com/syntax-tree/unist#node

[position]: https://github.com/syntax-tree/unist#position

[point]: https://github.com/syntax-tree/unist#point

[vfile]: https://github.com/vfile/vfile

[util]: https://github.com/vfile/vfile#utilities

[reporter]: https://github.com/vfile/vfile-reporter

[ruleid]: #ruleid

[source]: #source
