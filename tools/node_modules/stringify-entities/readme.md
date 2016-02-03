# stringify-entities [![Build Status](https://img.shields.io/travis/wooorm/stringify-entities.svg?style=flat)](https://travis-ci.org/wooorm/stringify-entities) [![Coverage Status](https://img.shields.io/codecov/c/github/wooorm/stringify-entities.svg)](https://codecov.io/github/wooorm/stringify-entities)

Encode HTML character references and character entities.

*   [x] Very fast;

*   [x] Just the encoding part;

*   [x] Reliable: ``"`"`` characters are escaped to ensure no scripts
    execute in IE6-8.  Additionally, only named entities recognized by HTML4
    are encoded, meaning the infamous `&apos;` (which people think is a
    [virus](http://www.telegraph.co.uk/technology/advice/10516839/Why-do-some-apostrophes-get-replaced-with-andapos.html))
    won’t show up.

## Installation

[npm](https://docs.npmjs.com/cli/install):

```bash
npm install stringify-entities
```

**stringify-entities** is also available for [duo](http://duojs.org/#getting-started),
and [bundled](https://github.com/wooorm/stringify-entities/releases) for AMD,
CommonJS, and globals (uncompressed and compressed).

## Usage

```js
var stringify = require('stringify-entities');

stringify.encode('alpha © bravo ≠ charlie 𝌆 delta');
```

Yields:

```html
alpha &#xA9; bravo &#x2260; charlie &#x1D306; delta
```

&hellip;and with `useNamedReferences: true`.

```js
stringify.encode('alpha © bravo ≠ charlie 𝌆 delta', { useNamedReferences: true });
```

Yields:

```html
alpha &copy; bravo &ne; charlie &#x1D306; delta
```

## API

### stringifyEntities(value\[, options?])

Encode special characters in `value`.

**Parameters**:

*   `value` (`string`) — Value to encode;

*   `options` (`Object?`, optional) — Configuration:

    *   `escapeOnly` (`boolean?`, optional, default: `false`)
        — Whether to only escape possibly dangerous characters
        (`"`, `'`, `<`, `>` `&`, and `` ` ``);

    *   `useNamedReferences` (`boolean?`, optional, default: `false`)
        — Whether to use entities where possible.

**Returns**: `string`, encoded `value`.

### stringifyEntities.escape(value\[, options?])

Escape special characters in `value`.  Shortcut for `stringifyEntities`
with `escapeOnly: true` and `useNamedReferences: true`.

## Support

See [html.spec.whatwg.org](https://html.spec.whatwg.org/multipage/syntax.html#named-character-references).

## License

[MIT](LICENSE) © [Titus Wormer](http://wooorm.com)
