# Query String

> Stability: 2 - Stable

<!--name=querystring-->

The `querystring` module provides utilities for parsing and formatting URL
query strings. It can be accessed using:

```js
const querystring = require('querystring');
```

## querystring.escape(str)
<!-- YAML
added: v0.1.25
-->

* `str` {String}

The `querystring.escape()` method performs URL percent-encoding on the given
`str` in a manner that is optimized for the specific requirements of URL
query strings.

The `querystring.escape()` method is used by `querystring.stringify()` and is
generally not expected to be used directly. It is exported primarily to allow
application code to provide a replacement percent-encoding implementation if
necessary by assigning `querystring.escape` to an alternative function.

## querystring.parse(str[, sep[, eq[, options]]])
<!-- YAML
added: v0.1.25
-->

* `str` {String} The URL query string to parse
* `sep` {String} The substring used to delimit key and value pairs in the
  query string. Defaults to `'&'`.
* `eq` {String}. The substring used to delimit keys and values in the
  query string. Defaults to `'='`.
* `options` {Object}
  * `decodeURIComponent` {Function} The function to use when decoding
    percent-encoded characters in the query string. Defaults to
    `querystring.unescape()`.
  * `maxKeys` {number} Specifies the maximum number of keys to parse.
    Defaults to `1000`. Specify `0` to remove key counting limitations.

The `querystring.parse()` method parses a URL query string (`str`) into a
collection of key and value pairs.

For example, the query string `'foo=bar&abc=xyz&abc=123'` is parsed into:

```js
{
  foo: 'bar',
  abc: ['xyz', '123']
}
```

*Note*: The object returned by the `querystring.parse()` method _does not_
prototypically extend from the JavaScript `Object`. This means that the
typical `Object` methods such as `obj.toString()`, `obj.hasOwnProperty()`,
and others are not defined and *will not work*.

By default, percent-encoded characters within the query string will be assumed
to use UTF-8 encoding. If an alternative character encoding is used, then an
alternative `decodeURIComponent` option will need to be specified as illustrated
in the following example:

```js
// Assuming gbkDecodeURIComponent function already exists...

querystring.parse('w=%D6%D0%CE%C4&foo=bar', null, null,
  { decodeURIComponent: gbkDecodeURIComponent })
```

## querystring.stringify(obj[, sep[, eq[, options]]])
<!-- YAML
added: v0.1.25
-->

* `obj` {Object} The object to serialize into a URL query string
* `sep` {String} The substring used to delimit key and value pairs in the
  query string. Defaults to `'&'`.
* `eq` {String}. The substring used to delimit keys and values in the
  query string. Defaults to `'='`.
* `options`
  * `encodeURIComponent` {Function} The function to use when converting
    URL-unsafe characters to percent-encoding in the query string. Defaults to
    `querystring.escape()`.

The `querystring.stringify()` method produces a URL query string from a
given `obj` by iterating through the object's "own properties".

For example:

```js
querystring.stringify({ foo: 'bar', baz: ['qux', 'quux'], corge: '' })
// returns 'foo=bar&baz=qux&baz=quux&corge='

querystring.stringify({foo: 'bar', baz: 'qux'}, ';', ':')
// returns 'foo:bar;baz:qux'
```

By default, characters requiring percent-encoding within the query string will
be encoded as UTF-8. If an alternative encoding is required, then an alternative
`encodeURIComponent` option will need to be specified as illustrated in the
following example:

```js
// Assuming gbkEncodeURIComponent function already exists,

querystring.stringify({ w: '中文', foo: 'bar' }, null, null,
  { encodeURIComponent: gbkEncodeURIComponent })
```

## querystring.unescape(str)
<!-- YAML
added: v0.1.25
-->
* `str` {String}


The `querystring.unescape()` method performs decoding of URL percent-encoded
characters on the given `str`.

The `querystring.unescape()` method is used by `querystring.parse()` and is
generally not expected to be used directly. It is exported primarily to allow
application code to provide a replacement decoding implementation if
necessary by assigning `querystring.unescape` to an alternative function.

By default, the `querystring.unescape()` method will attempt to use the
JavaScript built-in `decodeURIComponent()` method to decode. If that fails,
a safer equivalent that does not throw on malformed URLs will be used.
