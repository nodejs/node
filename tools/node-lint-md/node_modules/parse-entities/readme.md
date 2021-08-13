# parse-entities

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]

Parse HTML character references: fast, spec-compliant, positional information.

## Install

This package is ESM only: Node 12+ is needed to use it and it must be `import`ed
instead of `require`d.

[npm][]:

```sh
npm install parse-entities
```

## Use

```js
import {parseEntities} from 'parse-entities'

parseEntities('alpha &amp bravo')
// => alpha & bravo

parseEntities('charlie &copycat; delta')
// => charlie ©cat; delta

parseEntities('echo &copy; foxtrot &#8800; golf &#x1D306; hotel')
// => echo © foxtrot ≠ golf 𝌆 hotel
```

## API

This package exports the following identifiers: `parseEntities`.
There is no default export.

## `parseEntities(value[, options])`

###### `options.additional`

Additional character to accept (`string?`, default: `''`).
This allows other characters, without error, when following an ampersand.

###### `options.attribute`

Whether to parse `value` as an attribute value (`boolean?`, default: `false`).

###### `options.nonTerminated`

Whether to allow non-terminated entities (`boolean`, default: `true`).
For example, `&copycat` for `©cat`.
This behavior is spec-compliant but can lead to unexpected results.

###### `options.warning`

Error handler ([`Function?`][warning]).

###### `options.text`

Text handler ([`Function?`][text]).

###### `options.reference`

Reference handler ([`Function?`][reference]).

###### `options.warningContext`

Context used when invoking `warning` (`'*'`, optional).

###### `options.textContext`

Context used when invoking `text` (`'*'`, optional).

###### `options.referenceContext`

Context used when invoking `reference` (`'*'`, optional)

###### `options.position`

Starting `position` of `value` (`Position` or `Point`, optional).
Useful when dealing with values nested in some sort of syntax tree.
The default is:

```js
{line: 1, column: 1, offset: 0}
```

##### Returns

`string` — Decoded `value`.

### `function warning(reason, point, code)`

Error handler.

##### Context

`this` refers to `warningContext` when given to `parseEntities`.

##### Parameters

###### `reason`

Human-readable reason the error (`string`).

###### `point`

Place at which the parse error occurred (`Point`).

###### `code`

Machine-readable code for the error (`number`).

The following codes are used:

| Code | Example            | Note                                          |
| ---- | ------------------ | --------------------------------------------- |
| `1`  | `foo &amp bar`     | Missing semicolon (named)                     |
| `2`  | `foo &#123 bar`    | Missing semicolon (numeric)                   |
| `3`  | `Foo &bar baz`     | Ampersand did not start a reference           |
| `4`  | `Foo &#`           | Empty reference                               |
| `5`  | `Foo &bar; baz`    | Unknown entity                                |
| `6`  | `Foo &#128; baz`   | [Disallowed reference][invalid]               |
| `7`  | `Foo &#xD800; baz` | Prohibited: outside permissible unicode range |

### `function text(value, position)`

Text handler.

##### Context

`this` refers to `textContext` when given to `parseEntities`.

##### Parameters

###### `value`

String of content (`string`).

###### `position`

Location at which `value` starts and ends (`Position`).

### `function reference(value, position, source)`

Character reference handler.

##### Context

`this` refers to `referenceContext` when given to `parseEntities`.

##### Parameters

###### `value`

Encoded character reference (`string`).

###### `position`

Location at which `value` starts and ends (`Position`).

###### `source`

Source of character reference (`string`).

## Related

*   [`stringify-entities`](https://github.com/wooorm/stringify-entities)
    — Encode HTML character references
*   [`character-entities`](https://github.com/wooorm/character-entities)
    — Info on character entities
*   [`character-entities-html4`](https://github.com/wooorm/character-entities-html4)
    — Info on HTML4 character entities
*   [`character-entities-legacy`](https://github.com/wooorm/character-entities-legacy)
    — Info on legacy character entities
*   [`character-reference-invalid`](https://github.com/wooorm/character-reference-invalid)
    — Info on invalid numeric character references

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/wooorm/parse-entities/workflows/main/badge.svg

[build]: https://github.com/wooorm/parse-entities/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/parse-entities.svg

[coverage]: https://codecov.io/github/wooorm/parse-entities

[downloads-badge]: https://img.shields.io/npm/dm/parse-entities.svg

[downloads]: https://www.npmjs.com/package/parse-entities

[size-badge]: https://img.shields.io/bundlephobia/minzip/parse-entities.svg

[size]: https://bundlephobia.com/result?p=parse-entities

[npm]: https://docs.npmjs.com/cli/install

[license]: license

[author]: https://wooorm.com

[warning]: #function-warningreason-point-code

[text]: #function-textvalue-position

[reference]: #function-referencevalue-position-source

[invalid]: https://github.com/wooorm/character-reference-invalid
