# parse-entities [![Build Status](https://img.shields.io/travis/wooorm/parse-entities.svg?style=flat)](https://travis-ci.org/wooorm/parse-entities) [![Coverage Status](https://img.shields.io/codecov/c/github/wooorm/parse-entities.svg)](https://codecov.io/github/wooorm/parse-entities)

Parse HTML character references: fast, spec-compliant, positional information.

## Installation

[npm](https://docs.npmjs.com/cli/install):

```bash
npm install parse-entities
```

**parse-entities** is also available for [duo](http://duojs.org/#getting-started),
and [bundled](https://github.com/wooorm/parse-entities/releases) for AMD,
CommonJS, and globals (uncompressed and compressed).

## Usage

```js
var decode = require('parse-entities');

decode('alpha &amp bravo');
// alpha & bravo

decode('charlie &copycat; delta');
// charlie ©cat; delta

decode('echo &copy; foxtrot &#8800; golf &#x1D306; hotel');
// echo © foxtrot ≠ golf 𝌆 hotel
```

## API

## parseEntities(value\[, options])

**Parameters**

*   `value` (`string`)
    — Value with entities to parse;

*   `options` (`Object`, optional):

    *   `additional` (`string`, optional, default: `''`)
        — Additional character to accept when following an ampersand (without
        error);

    *   `attribute` (`boolean`, optional, default: `false`)
        — Whether to parse `value` as an attribute value;

    *   `position` (`Location` or `Position`, optional)
        — Starting `position` of `value`, useful when dealing with values
        nested in some sort of syntax tree. The default is:

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

    *   `warning` ([`Function`](#function-warningreason-position-code),
        optional) — Error handler;

    *   `text` ([`Function`](#function-textvalue-location), optional)
        — Text handler;

    *   `reference` ([`Function`](#function-referencevalue-location-source),
        optional) — Reference handler;

    *   `warningContext` (`'*'`, optional)
        — Context used when invoking `warning`;

    *   `textContext` (`'*'`, optional)
        — Context used when invoking `text`;

    *   `referenceContext` (`'*'`, optional)
        — Context used when invoking `reference`.

**Returns**

`string` — Decoded `value`.

### function warning(reason, position, code)

Error handler.

**Context**: `this` refers to `warningContext` when given to `parseEntities`.

**Parameters**

*   `reason` (`string`)
    — Reason (human-readable) for triggering a parse error;

*   `position` (`Position`)
    — Place at which the parse error occurred;

*   `code` (`number`)
    — Identifier of reason for triggering a parse error.

The following codes are used:

| Code | Example            | Note                                                                          |
| ---- | ------------------ | ----------------------------------------------------------------------------- |
| `1`  | `foo &amp bar`     | Missing semicolon (named)                                                     |
| `2`  | `foo &#123 bar`    | Missing semicolon (numeric)                                                   |
| `3`  | `Foo &bar baz`     | Ampersand did not start a reference                                           |
| `4`  | `Foo &#`           | Empty reference                                                               |
| `5`  | `Foo &bar; baz`    | Unknown entity                                                                |
| `6`  | `Foo &#128; baz`   | [Disallowed reference](https://github.com/wooorm/character-reference-invalid) |
| `7`  | `Foo &#xD800; baz` | Prohibited: outside permissible unicode range                                 |

### function text(value, location)

Text handler.

**Context**: `this` refers to `textContext` when given to `parseEntities`.

**Parameters**

*   `value` (`string`) — String of content;
*   `location` (`Location`) — Location at which `value` starts and ends.

### function reference(value, location, source)

Character reference handler.

**Context**: `this` refers to `referenceContext` when given to `parseEntities`.

**Parameters**

*   `value` (`string`) — Encoded character reference;
*   `location` (`Location`) — Location at which `value` starts and ends;
*   `source` (`Location`) — Source of character reference.

## License

[MIT](LICENSE) © [Titus Wormer](http://wooorm.com)
