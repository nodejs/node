# Query String

    Stability: 3 - Stable

<!--name=querystring-->

This module provides utilities for dealing with query strings.
It provides the following methods:

## querystring.stringify(obj, [sep], [eq])

Serialize an object to a query string.
Optionally override the default separator (`'&'`) and assignment (`'='`)
characters.

Example:

    querystring.stringify({ foo: 'bar', baz: ['qux', 'quux'], corge: '' })
    // returns
    'foo=bar&baz=qux&baz=quux&corge='

    querystring.stringify({foo: 'bar', baz: 'qux'}, ';', ':')
    // returns
    'foo:bar;baz:qux'

## querystring.parse(str, [sep], [eq])

Deserialize a query string to an object.
Optionally override the default separator (`'&'`) and assignment (`'='`)
characters.

Example:

    querystring.parse('foo=bar&baz=qux&baz=quux&corge')
    // returns
    { foo: 'bar', baz: ['qux', 'quux'], corge: '' }

## querystring.escape

The escape function used by `querystring.stringify`,
provided so that it could be overridden if necessary.

## querystring.unescape

The unescape function used by `querystring.parse`,
provided so that it could be overridden if necessary.
