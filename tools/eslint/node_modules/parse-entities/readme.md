# parse-entities [![Build Status][build-badge]][build-status] [![Coverage Status][coverage-badge]][coverage-status]

Parse HTML character references: fast, spec-compliant, positional
information.

## Installation

[npm][]:

```bash
npm install parse-entities
```

## Usage

```js
var decode = require('parse-entities');

decode('alpha &amp bravo');
//=> alpha & bravo

decode('charlie &copycat; delta');
//=> charlie ©cat; delta

decode('echo &copy; foxtrot &#8800; golf &#x1D306; hotel');
//=> echo © foxtrot ≠ golf 𝌆 hotel
```

## API

## `parseEntities(value[, options])`

###### `options`

*   `additional` (`string`, optional, default: `''`)
    — Additional character to accept when following an ampersand (without
    error)
*   `attribute` (`boolean`, optional, default: `false`)
    — Whether to parse `value` as an attribute value
*   `nonTerminated` (`boolean`, default: `true`)
    — Whether to allow non-terminated entities, such as `&copycat` to
    `©cat`.  This behaviour is spec-compliant but can lead to unexpected
    results
*   `warning` ([`Function`][warning], optional)
    — Error handler
*   `text` ([`Function`][text], optional)
    — Text handler
*   `reference` ([`Function`][reference],
    optional) — Reference handler
*   `warningContext` (`'*'`, optional)
    — Context used when invoking `warning`
*   `textContext` (`'*'`, optional)
    — Context used when invoking `text`
*   `referenceContext` (`'*'`, optional)
    — Context used when invoking `reference`
*   `position` (`Location` or `Position`, optional)
    — Starting `position` of `value`, useful when dealing with values
    nested in some sort of syntax tree.  The default is:

    ```json
    {
      "start": {
        "line": 1,
        "column": 1,
        "offset": 0
      },
      "indent": []
    }
    ```

###### Returns

`string` — Decoded `value`.

### `function warning(reason, position, code)`

Error handler.

###### Context

`this` refers to `warningContext` when given to `parseEntities`.

###### Parameters

*   `reason` (`string`)
    — Reason (human-readable) for triggering a parse error
*   `position` (`Position`)
    — Place at which the parse error occurred
*   `code` (`number`)
    — Identifier of reason for triggering a parse error

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

###### `function text(value, location)`

Text handler.

###### Context

`this` refers to `textContext` when given to `parseEntities`.

###### Parameters

*   `value` (`string`) — String of content
*   `location` (`Location`) — Location at which `value` starts and ends

### `function reference(value, location, source)`

Character reference handler.

###### Context

`this` refers to `referenceContext` when given to `parseEntities`.

###### Parameters

*   `value` (`string`) — Encoded character reference
*   `location` (`Location`) — Location at which `value` starts and ends
*   `source` (`Location`) — Source of character reference

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://img.shields.io/travis/wooorm/parse-entities.svg

[build-status]: https://travis-ci.org/wooorm/parse-entities

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/parse-entities.svg

[coverage-status]: https://codecov.io/github/wooorm/parse-entities

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[warning]: #function-warningreason-position-code

[text]: #function-textvalue-location

[reference]: #function-referencevalue-location-source

[invalid]: https://github.com/wooorm/character-reference-invalid
