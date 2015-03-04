# Query String

    Stability: 2 - Stable

<!--name=querystring-->

This module provides utilities for dealing with query strings.
It provides the following methods:

## querystring.stringify(obj[, sep][, eq][, options])

Serialize an object to a query string.
Optionally override the default separator (`'&'`) and assignment (`'='`)
characters.

Options object may contain `encodeURIComponent` property (`querystring.escape` by default),
it can be used to encode string with `non-utf8` encoding if necessary.

Example:

    querystring.stringify({ foo: 'bar', baz: ['qux', 'quux'], corge: '' })
    // returns
    'foo=bar&baz=qux&baz=quux&corge='

    querystring.stringify({foo: 'bar', baz: 'qux'}, ';', ':')
    // returns
    'foo:bar;baz:qux'

    // Suppose gbkEncodeURIComponent function already exists,
    // it can encode string with `gbk` encoding
    querystring.stringify({ w: '中文', foo: 'bar' }, null, null,
      { encodeURIComponent: gbkEncodeURIComponent })
    // returns
    'w=%D6%D0%CE%C4&foo=bar'

## querystring.parse(str[, sep][, eq][, options])

Deserialize a query string to an object.
Optionally override the default separator (`'&'`) and assignment (`'='`)
characters.

Options object may contain `maxKeys` property (equal to 1000 by default), it'll
be used to limit processed keys. Set it to 0 to remove key count limitation.

Options object may contain `decodeURIComponent` property (`querystring.unescape` by default),
it can be used to decode a `non-utf8` encoding string if necessary.

Example:

    querystring.parse('foo=bar&baz=qux&baz=quux&corge')
    // returns
    { foo: 'bar', baz: ['qux', 'quux'], corge: '' }

    // Suppose gbkDecodeURIComponent function already exists,
    // it can decode `gbk` encoding string
    querystring.parse('w=%D6%D0%CE%C4&foo=bar', null, null,
      { decodeURIComponent: gbkDecodeURIComponent })
    // returns
    { w: '中文', foo: 'bar' }

## querystring.escape

The escape function used by `querystring.stringify`,
provided so that it could be overridden if necessary.

## querystring.unescape

The unescape function used by `querystring.parse`,
provided so that it could be overridden if necessary.

It will try to use `decodeURIComponent` in the first place,
but if that fails it falls back to a safer equivalent that
doesn't throw on malformed URLs.
