# URL

> Stability: 2 - Stable

The `url` module provides utilities for URL resolution and parsing. It can be
accessed using:

```js
const url = require('url');
```

## URL Strings and URL Objects

A URL string is a structured string containing multiple meaningful components.
When parsed, a URL object is returned containing properties for each of these
components.

The following details each of the components of a parsed URL. The example
`'http://user:pass@host.com:8080/p/a/t/h?query=string#hash'` is used to
illustrate each.

```txt
┌─────────────────────────────────────────────────────────────────────────────┐
│                                    href                                     │
├──────────┬┬───────────┬─────────────────┬───────────────────────────┬───────┤
│ protocol ││   auth    │      host       │           path            │ hash  │
│          ││           ├──────────┬──────┼──────────┬────────────────┤       │
│          ││           │ hostname │ port │ pathname │     search     │       │
│          ││           │          │      │          ├─┬──────────────┤       │
│          ││           │          │      │          │ │    query     │       │
"  http:   // user:pass @ host.com : 8080   /p/a/t/h  ?  query=string   #hash "
│          ││           │          │      │          │ │              │       │
└──────────┴┴───────────┴──────────┴──────┴──────────┴─┴──────────────┴───────┘
(all spaces in the "" line should be ignored -- they're purely for formatting)
```

### urlObject.href

The `href` property is the full URL string that was parsed with both the
`protocol` and `host` components converted to lower-case.

For example: `'http://user:pass@host.com:8080/p/a/t/h?query=string#hash'`

### urlObject.protocol

The `protocol` property identifies the URL's lower-cased protocol scheme.

For example: `'http:'`

### urlObject.slashes

The `slashes` property is a `boolean` with a value of `true` if two ASCII
forward-slash characters (`/`) are required following the colon in the
`protocol`.

### urlObject.host

The `host` property is the full lower-cased host portion of the URL, including
the `port` if specified.

For example: `'host.com:8080'`

### urlObject.auth

The `auth` property is the username and password portion of the URL, also
referred to as "userinfo". This string subset follows the `protocol` and
double slashes (if present) and preceeds the `host` component, delimited by an
ASCII "at sign" (`@`). The format of the string is `{username}[:{password}]`,
with the `[:{password}]` portion being optional.

For example: `'user:pass'`

### urlObject.hostname

The `hostname` property is the lower-cased host name portion of the `host`
component *without* the `port` included.

For example: `'host.com'`

### urlObject.port

The `port` property is the numeric port portion of the `host` component.

For example: `'8080'`

### urlObject.pathname

The `pathname` property consists of the entire path section of the URL. This
is everything following the `host` (including the `port`) and before the start
of the `query` or `hash` components, delimited by either the ASCII question
mark (`?`) or hash (`#`) characters.

For example `'/p/a/t/h'`

No decoding of the path string is performed.

### urlObject.search

The `search` property consists of the entire "query string" portion of the
URL, including the leading ASCII question mark (`?`) character.

For example: `'?query=string'`

No decoding of the query string is performed.

### urlObject.path

The `path` property is a concatenation of the `pathname` and `search`
components.

For example: `'/p/a/t/h?query=string'`

No decoding of the `path` is performed.

### urlObject.query

The `query` property is either the "params" portion of the query string (
everything *except* the leading ASCII question mark (`?`), or an object
returned by the [`querystring`][] module's `parse()` method:

For example: `'query=string'` or `{'query': 'string'}`

If returned as a string, no decoding of the query string is performed. If
returned as an object, both keys and values are decoded.

### urlObject.hash

The `hash` property consists of the "fragment" portion of the URL including
the leading ASCII hash (`#`) character.

For example: `'#hash'`

## url.format(urlObject)
<!-- YAML
added: v0.1.25
-->

* `urlObject` {Object | String} A URL object (as returned by `url.parse()` or
  constructed otherwise). If a string, it is converted to an object by passing
  it to `url.parse()`.

The `url.format()` method returns a formatted URL string derived from
`urlObject`.

If `urlObject` is not an object or a string, `url.parse()` will throw a
[`TypeError`][].

The formatting process operates as follows:

* A new empty string `result` is created.
* If `urlObject.protocol` is a string, it is appended as-is to `result`.
* Otherwise, if `urlObject.protocol` is not `undefined` and is not a string, an
  [`Error`][] is thrown.
* For all string values of `urlObject.protocol` that *do not end* with an ASCII
  colon (`:`) character, the literal string `:` will be appended to `result`.
* If either the `urlObject.slashes` property is true, `urlObject.protocol`
  begins with one of `http`, `https`, `ftp`, `gopher`, or `file`, or
  `urlObject.protocol` is `undefined`, the literal string `//` will be appended
  to `result`.
* If the value of the `urlObject.auth` property is truthy, and either
  `urlObject.host` or `urlObject.hostname` are not `undefined`, the value of
  `urlObject.auth` will be coerced into a string and appended to `result`
   followed by the literal string `@`.
* If the `urlObject.host` property is `undefined` then:
  * If the `urlObject.hostname` is a string, it is appended to `result`.
  * Otherwise, if `urlObject.hostname` is not `undefined` and is not a string,
    an [`Error`][] is thrown.
  * If the `urlObject.port` property value is truthy, and `urlObject.hostname`
    is not `undefined`:
    * The literal string `:` is appended to `result`, and
    * The value of `urlObject.port` is coerced to a string and appended to
      `result`.
* Otherwise, if the `urlObject.host` property value is truthy, the value of
  `urlObject.host` is coerced to a string and appended to `result`.
* If the `urlObject.pathname` property is a string that is not an empty string:
  * If the `urlObject.pathname` *does not start* with an ASCII forward slash
    (`/`), then the literal string '/' is appended to `result`.
  * The value of `urlObject.pathname` is appended to `result`.
* Otherwise, if `urlObject.pathname` is not `undefined` and is not a string, an
  [`Error`][] is thrown.
* If the `urlObject.search` property is `undefined` and if the `urlObject.query`
  property is an `Object`, the literal string `?` is appended to `result`
  followed by the output of calling the [`querystring`][] module's `stringify()`
  method passing the value of `urlObject.query`.
* Otherwise, if `urlObject.search` is a string:
  * If the value of `urlObject.search` *does not start* with the ASCII question
    mark (`?`) character, the literal string `?` is appended to `result`.
  * The value of `urlObject.search` is appended to `result`.
* Otherwise, if `urlObject.search` is not `undefined` and is not a string, an
  [`Error`][] is thrown.
* If the `urlObject.hash` property is a string:
  * If the value of `urlObject.hash` *does not start* with the ASCII hash (`#`)
    character, the literal string `#` is appended to `result`.
  * The value of `urlObject.hash` is appended to `result`.
* Otherwise, if the `urlObject.hash` property is not `undefined` and is not a
  string, an [`Error`][] is thrown.
* `result` is returned.


## url.parse(urlString[, parseQueryString[, slashesDenoteHost]])
<!-- YAML
added: v0.1.25
-->

* `urlString` {string} The URL string to parse.
* `parseQueryString` {boolean} If `true`, the `query` property will always
  be set to an object returned by the [`querystring`][] module's `parse()`
  method. If `false`, the `query` property on the returned URL object will be an
  unparsed, undecoded string. Defaults to `false`.
* `slashesDenoteHost` {boolean} If `true`, the first token after the literal
  string `//` and preceeding the next `/` will be interpreted as the `host`.
  For instance, given `//foo/bar`, the result would be
  `{host: 'foo', pathname: '/bar'}` rather than `{pathname: '//foo/bar'}`.
  Defaults to `false`.

The `url.parse()` method takes a URL string, parses it, and returns a URL
object.

## url.resolve(from, to)
<!-- YAML
added: v0.1.25
-->

* `from` {string} The Base URL being resolved against.
* `to` {string} The HREF URL being resolved.

The `url.resolve()` method resolves a target URL relative to a base URL in a
manner similar to that of a Web browser resolving an anchor tag HREF.

For example:

```js
url.resolve('/one/two/three', 'four')         // '/one/two/four'
url.resolve('http://example.com/', '/one')    // 'http://example.com/one'
url.resolve('http://example.com/one', '/two') // 'http://example.com/two'
```

## Escaped Characters

URLs are only permitted to contain a certain range of characters. Spaces (`' '`)
and the following characters will be automatically escaped in the
properties of URL objects:

```txt
< > " ` \r \n \t { } | \ ^ '
```

For example, the ASCII space character (`' '`) is encoded as `%20`. The ASCII
forward slash (`/`) character is encoded as `%3C`.


[`Error`]: errors.html#errors_class_error
[`querystring`]: querystring.html
[`TypeError`]: errors.html#errors_class_typeerror
