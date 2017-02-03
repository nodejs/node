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
(all spaces in the "" line should be ignored -- they are purely for formatting)
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
double slashes (if present) and precedes the `host` component, delimited by an
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

The `query` property is either the query string without the leading ASCII
question mark (`?`), or an object returned by the [`querystring`][] module's
`parse()` method. Whether the `query` property is a string or object is
determined by the `parseQueryString` argument passed to `url.parse()`.

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
* If either of the following conditions is true, then the literal string `//`
  will be appended to `result`:
    * `urlObject.slashes` property is true;
    * `urlObject.protocol` begins with `http`, `https`, `ftp`, `gopher`, or
      `file`;
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

## url.format(URL[, options])

> Stability: 1 - Experimental

* `URL` {URL} A [WHATWG URL][] object
* `options` {Object}
  * `auth` {Boolean} `true` if the serialized URL string should include the
    username and password, `false` otherwise. Defaults to `true`.
  * `fragment` {Boolean} `true` if the serialized URL string should include the
    fragment, `false` otherwise. Defaults to `true`.
  * `search` {Boolean} `true` if the serialized URL string should include the
    search query, `false` otherwise. Defaults to `true`.
  * `unicode` (Boolean) `true` if Unicode characters appearing in the host
    component of the URL string should be encoded directly as opposed to being
    Punycode encoded. Defaults to `false`.

Returns a customizable serialization of a URL String representation of a
[WHATWG URL][] object.

The URL object has both a `toString()` method and `href` property that return
string serializations of the URL. These are not, however, customizable in
any way. The `url.format(URL[, options])` method allows for basic customization
of the output.

For example:

```js
const myURL = new URL('https://a:b@你好你好?abc#foo');

console.log(myURL.href);
  // Prints https://a:b@xn--6qqa088eba/?abc#foo

console.log(myURL.toString());
  // Prints https://a:b@xn--6qqa088eba/?abc#foo

console.log(url.format(myURL, {fragment: false, unicode: true, auth: false}));
  // Prints 'https://你好你好?abc'
```

*Note*: This variation of the `url.format()` method is currently considered to
be experimental.

## url.parse(urlString[, parseQueryString[, slashesDenoteHost]])
<!-- YAML
added: v0.1.25
-->

* `urlString` {String} The URL string to parse.
* `parseQueryString` {Boolean} If `true`, the `query` property will always
  be set to an object returned by the [`querystring`][] module's `parse()`
  method. If `false`, the `query` property on the returned URL object will be an
  unparsed, undecoded string. Defaults to `false`.
* `slashesDenoteHost` {Boolean} If `true`, the first token after the literal
  string `//` and preceding the next `/` will be interpreted as the `host`.
  For instance, given `//foo/bar`, the result would be
  `{host: 'foo', pathname: '/bar'}` rather than `{pathname: '//foo/bar'}`.
  Defaults to `false`.

The `url.parse()` method takes a URL string, parses it, and returns a URL
object.

## url.resolve(from, to)
<!-- YAML
added: v0.1.25
-->

* `from` {String} The Base URL being resolved against.
* `to` {String} The HREF URL being resolved.

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

## The WHATWG URL API

> Stability: 1 - Experimental

The `url` module provides an *experimental* implementation of the
[WHATWG URL Standard][] as an alternative to the existing `url.parse()` API.

```js
const URL = require('url').URL;
const myURL = new URL('https://example.org/foo');

console.log(myURL.href);     // https://example.org/foo
console.log(myURL.protocol); // https:
console.log(myURL.hostname); // example.org
console.log(myURL.pathname); // /foo
```

*Note*: Using the `delete` keyword (e.g. `delete myURL.protocol`,
`delete myURL.pathname`, etc) has no effect but will still return `true`.

### Class: URL
#### Constructor: new URL(input[, base])

* `input` {String} The input URL to parse
* `base` {String | URL} The base URL to resolve against if the `input` is not
  absolute.

Creates a new `URL` object by parsing the `input` relative to the `base`. If
`base` is passed as a string, it will be parsed equivalent to `new URL(base)`.

```js
const myURL = new URL('/foo', 'https://example.org/');
  // https://example.org/foo
```

A `TypeError` will be thrown if the `input` or `base` are not valid URLs. Note
that an effort will be made to coerce the given values into strings. For
instance:

```js
const myURL = new URL({toString: () => 'https://example.org/'});
  // https://example.org/
```

Unicode characters appearing within the hostname of `input` will be
automatically converted to ASCII using the [Punycode][] algorithm.

```js
const myURL = new URL('https://你好你好');
  // https://xn--6qqa088eba
```

Additional [examples of parsed URLs][] may be found in the WHATWG URL Standard.

#### url.hash

Gets and sets the fragment portion of the URL.

```js
const myURL = new URL('https://example.org/foo#bar');
console.log(myURL.hash);
  // Prints #bar

myURL.hash = 'baz';
console.log(myURL.href);
  // Prints https://example.org/foo#baz
```

Invalid URL characters included in the value assigned to the `hash` property
are [percent-encoded](#whatwg-percent-encoding). Note that the selection of
which characters to percent-encode may vary somewhat from what the
[`url.parse()`][] and [`url.format()`][] methods would produce.

#### url.host

Gets and sets the host portion of the URL.

```js
const myURL = new URL('https://example.org:81/foo');
console.log(myURL.host);
  // Prints example.org:81

myURL.host = 'example.com:82';
console.log(myURL.href);
  // Prints https://example.com:82/foo
```

Invalid host values assigned to the `host` property are ignored.

#### url.hostname

Gets and sets the hostname portion of the URL. The key difference between
`url.host` and `url.hostname` is that `url.hostname` does *not* include the
port.

```js
const myURL = new URL('https://example.org:81/foo');
console.log(myURL.hostname);
  // Prints example.org

myURL.hostname = 'example.com:82';
console.log(myURL.href);
  // Prints https://example.com:81/foo
```

Invalid hostname values assigned to the `hostname` property are ignored.

#### url.href

Gets and sets the serialized URL.

```js
const myURL = new URL('https://example.org/foo');
console.log(myURL.href);
  // Prints https://example.org/foo

myURL.href = 'https://example.com/bar'
  // Prints https://example.com/bar
```

Setting the value of the `href` property to a new value is equivalent to
creating a new `URL` object using `new URL(value)`. Each of the `URL` object's
properties will be modified.

If the value assigned to the `href` property is not a valid URL, a `TypeError`
will be thrown.

#### url.origin

Gets the read-only serialization of the URL's origin. Unicode characters that
may be contained within the hostname will be encoded as-is without [Punycode][]
encoding.

```js
const myURL = new URL('https://example.org/foo/bar?baz');
console.log(myURL.origin);
  // Prints https://example.org
```

```js
const idnURL = new URL('https://你好你好');
console.log(idnURL.origin);
  // Prints https://你好你好

console.log(idnURL.hostname);
  // Prints xn--6qqa088eba
```

#### url.password

Gets and sets the password portion of the URL.

```js
const myURL = new URL('https://abc:xyz@example.com');
console.log(myURL.password);
  // Prints xyz

myURL.password = '123';
console.log(myURL.href);
  // Prints https://abc:123@example.com
```

Invalid URL characters included in the value assigned to the `password` property
are [percent-encoded](#whatwg-percent-encoding). Note that the selection of
which characters to percent-encode may vary somewhat from what the
[`url.parse()`][] and [`url.format()`][] methods would produce.

#### url.pathname

Gets and sets the path portion of the URL.

```js
const myURL = new URL('https://example.org/abc/xyz?123');
console.log(myURL.pathname);
  // Prints /abc/xyz

myURL.pathname = '/abcdef';
console.log(myURL.href);
  // Prints https://example.org/abcdef?123
```

Invalid URL characters included in the value assigned to the `pathname`
property are [percent-encoded](#whatwg-percent-encoding). Note that the
selection of which characters to percent-encode may vary somewhat from what the
[`url.parse()`][] and [`url.format()`][] methods would produce.

#### url.port

Gets and sets the port portion of the URL. When getting the port, the value
is returned as a String.

```js
const myURL = new URL('https://example.org:8888');
console.log(myURL.port);
  // Prints 8888

myURL.port = 1234;
console.log(myURL.href);
  // Prints https://example.org:1234
```

The port value may be set as either a number or as a String containing a number
in the range `0` to `65535` (inclusive). Setting the value to the default port
of the `URL` objects given `protocol` will result in the `port` value becoming
the empty string (`''`).

Invalid URL port values assigned to the `port` property are ignored.

#### url.protocol

Gets and sets the protocol portion of the URL.

```js
const myURL = new URL('https://example.org');
console.log(myURL.protocol);
  // Prints http:

myURL.protocol = 'ftp';
console.log(myURL.href);
  // Prints ftp://example.org
```

Invalid URL protocol values assigned to the `protocol` property are ignored.

#### url.search

Gets and sets the serialized query portion of the URL.

```js
const myURL = new URL('https://example.org/abc?123');
console.log(myURL.search);
  // Prints ?123

myURL.search = 'abc=xyz';
console.log(myURL.href);
  // Prints https://example.org/abc?abc=xyz
```

Any invalid URL characters appearing in the value assigned the `search`
property will be [percent-encoded](#whatwg-percent-encoding). Note that the
selection of which characters to percent-encode may vary somewhat from what the
[`url.parse()`][] and [`url.format()`][] methods would produce.

#### url.searchParams

Gets a [`URLSearchParams`](#url_class_urlsearchparams) object representing the
query parameters of the URL.

#### url.username

Gets and sets the username portion of the URL.

```js
const myURL = new URL('https://abc:xyz@example.com');
console.log(myURL.username);
  // Prints abc

myURL.username = '123';
console.log(myURL.href);
  // Prints https://123:xyz@example.com
```

Any invalid URL characters appearing in the value assigned the `username`
property will be [percent-encoded](#whatwg-percent-encoding). Note that the
selection of which characters to percent-encode may vary somewhat from what the
[`url.parse()`][] and [`url.format()`][] methods would produce.

#### url.toString()

The `toString()` method on the `URL` object returns the serialized URL. The
value returned is equivalent to that of `url.href`.

### Class: URLSearchParams

The `URLSearchParams` object provides read and write access to the query of a
`URL`. The `URLSearchParams` class can also be used standalone with one of the
four following constructors.

```js
const { URL, URLSearchParams } = require('url');

const myURL = new URL('https://example.org/?abc=123');
console.log(myURL.searchParams.get('abc'));
  // Prints 123

myURL.searchParams.append('abc', 'xyz');
console.log(myURL.href);
  // Prints https://example.org/?abc=123&abc=xyz

myURL.searchParams.delete('abc');
myURL.searchParams.set('a', 'b');
console.log(myURL.href);
  // Prints https://example.org/?a=b

const newSearchParams = new URLSearchParams(myURL.searchParams);
// The above is equivalent to
// const newSearchParams = new URLSearchParams(myURL.search);

newSearchParams.append('a', 'c');
console.log(myURL.href);
  // Prints https://example.org/?a=b
console.log(newSearchParams.toString());
  // Prints a=b&a=c

// newSearchParams.toString() is implicitly called
myURL.search = newSearchParams;
console.log(myURL.href);
  // Prints https://example.org/?a=b&a=c
newSearchParams.delete('a');
console.log(myURL.href);
  // Prints https://example.org/?a=b&a=c
```

#### Constructor: new URLSearchParams()

Instantiate a new empty `URLSearchParams` object.

#### Constructor: new URLSearchParams(string)

* `string` {String} A query string

Parse the `string` as a query string, and use it to instantiate a new
`URLSearchParams` object. A leading `'?'`, if present, is ignored.

```js
const { URLSearchParams } = require('url');
let params;

params = new URLSearchParams('user=abc&query=xyz');
console.log(params.get('user'));
  // Prints 'abc'
console.log(params.toString());
  // Prints 'user=abc&query=xyz'

params = new URLSearchParams('?user=abc&query=xyz');
console.log(params.toString());
  // Prints 'user=abc&query=xyz'
```

#### Constructor: new URLSearchParams(obj)

* `obj` {Object} An object representing a collection of key-value pairs

Instantiate a new `URLSearchParams` object with a query hash map. The key and
value of each property of `obj` are always coerced to strings.

*Note*: Unlike [`querystring`][] module, duplicate keys in the form of array
values are not allowed. Arrays are stringified using [`array.toString()`][],
which simply joins all array elements with commas.

```js
const { URLSearchParams } = require('url');
const params = new URLSearchParams({
  user: 'abc',
  query: ['first', 'second']
});
console.log(params.getAll('query'));
  // Prints ['first,second']
console.log(params.toString());
  // Prints 'user=abc&query=first%2Csecond'
```

#### Constructor: new URLSearchParams(iterable)

* `iterable` {Iterable} An iterable object whose elements are key-value pairs

Instantiate a new `URLSearchParams` object with an iterable map in a way that
is similar to [`Map`][]'s constructor. `iterable` can be an Array or any
iterable object. That means `iterable` can be another `URLSearchParams`, in
which case the constructor will simply create a clone of the provided
`URLSearchParams`.  Elements of `iterable` are key-value pairs, and can
themselves be any iterable object.

Duplicate keys are allowed.

```js
const { URLSearchParams } = require('url');
let params;

// Using an array
params = new URLSearchParams([
  ['user', 'abc'],
  ['query', 'first'],
  ['query', 'second']
]);
console.log(params.toString());
  // Prints 'user=abc&query=first&query=second'

// Using a Map object
const map = new Map();
map.set('user', 'abc');
map.set('query', 'xyz');
params = new URLSearchParams(map);
console.log(params.toString());
  // Prints 'user=abc&query=xyz'

// Using a generator function
function* getQueryPairs() {
  yield ['user', 'abc'];
  yield ['query', 'first'];
  yield ['query', 'second'];
}
params = new URLSearchParams(getQueryPairs());
console.log(params.toString());
  // Prints 'user=abc&query=first&query=second'

// Each key-value pair must have exactly two elements
new URLSearchParams([
  ['user', 'abc', 'error']
]);
  // Throws TypeError: Each query pair must be a name/value tuple
```

#### urlSearchParams.append(name, value)

* `name` {String}
* `value` {String}

Append a new name-value pair to the query string.

#### urlSearchParams.delete(name)

* `name` {String}

Remove all name-value pairs whose name is `name`.

#### urlSearchParams.entries()

* Returns: {Iterator}

Returns an ES6 Iterator over each of the name-value pairs in the query.
Each item of the iterator is a JavaScript Array. The first item of the Array
is the `name`, the second item of the Array is the `value`.

Alias for `urlSearchParams\[\@\@iterator\]()`.

#### urlSearchParams.forEach(fn)

* `fn` {Function} Function invoked for each name-value pair in the query.

Iterates over each name-value pair in the query and invokes the given function.

```js
const URL = require('url').URL;
const myURL = new URL('https://example.org/?a=b&c=d');
myURL.searchParams.forEach((value, name) => {
  console.log(name, value);
});
```

#### urlSearchParams.get(name)

* `name` {String}
* Returns: {String} or `null` if there is no name-value pair with the given
  `name`.

Returns the value of the first name-value pair whose name is `name`.

#### urlSearchParams.getAll(name)

* `name` {String}
* Returns: {Array}

Returns the values of all name-value pairs whose name is `name`.

#### urlSearchParams.has(name)

* `name` {String}
* Returns: {Boolean}

Returns `true` if there is at least one name-value pair whose name is `name`.

#### urlSearchParams.keys()

* Returns: {Iterator}

Returns an ES6 Iterator over the names of each name-value pair.

#### urlSearchParams.set(name, value)

* `name` {String}
* `value` {String}

Remove any existing name-value pairs whose name is `name` and append a new
name-value pair.

#### urlSearchParams.toString()

* Returns: {String}

Returns the search parameters serialized as a URL-encoded string.

#### urlSearchParams.values()

* Returns: {Iterator}

Returns an ES6 Iterator over the values of each name-value pair.

#### urlSearchParams\[\@\@iterator\]()

* Returns: {Iterator}

Returns an ES6 Iterator over each of the name-value pairs in the query string.
Each item of the iterator is a JavaScript Array. The first item of the Array
is the `name`, the second item of the Array is the `value`.

Alias for `urlSearchParams.entries()`.

### require('url').domainToAscii(domain)

* `domain` {String}
* Returns: {String}

Returns the [Punycode][] ASCII serialization of the `domain`.

*Note*: The `require('url').domainToAscii()` method is introduced as part of
the new `URL` implementation but is not part of the WHATWG URL standard.

### require('url').domainToUnicode(domain)

* `domain` {String}
* Returns: {String}

Returns the Unicode serialization of the `domain`.

*Note*: The `require('url').domainToUnicode()` API is introduced as part of the
the new `URL` implementation but is not part of the WHATWG URL standard.

<a id="whatwg-percent-encoding"></a>
### Percent-Encoding in the WHATWG URL Standard

URLs are permitted to only contain a certain range of characters. Any character
falling outside of that range must be encoded. How such characters are encoded,
and which characters to encode depends entirely on where the character is
located within the structure of the URL. The WHATWG URL Standard uses a more
selective and fine grained approach to selecting encoded characters than that
used by the older [`url.parse()`][] and [`url.format()`][] methods.

The WHATWG algorithm defines three "encoding sets" that describe ranges of
characters that must be percent-encoded:

* The *simple encode set* includes code points in range U+0000 to U+001F
  (inclusive) and all code points greater than U+007E.

* The *default encode set* includes the *simple encode set* and code points
  U+0020, U+0022, U+0023, U+003C, U+003E, U+003F, U+0060, U+007B, and U+007D.

* The *userinfo encode set* includes the *default encode set* and code points
  U+002F, U+003A, U+003B, U+003D, U+0040, U+005B, U+005C, U+005D, U+005E, and
  U+007C.

The *simple encode set* is used primary for URL fragments and certain specific
conditions for the path. The *userinfo encode set* is used specifically for
username and passwords encoded within the URL. The *default encode set* is used
for all other cases.

When non-ASCII characters appear within a hostname, the hostname is encoded
using the [Punycode][] algorithm. Note, however, that a hostname *may* contain
*both* Punycode encoded and percent-encoded characters. For example:

```js
const URL = require('url').URL;
const myURL = new URL('https://%CF%80.com/foo');
console.log(myURL.href);
  // Prints https://xn--1xa.com/foo
console.log(myURL.origin);
  // Prints https://π.com
```


[`Error`]: errors.html#errors_class_error
[`querystring`]: querystring.html
[`TypeError`]: errors.html#errors_class_typeerror
[WHATWG URL Standard]: https://url.spec.whatwg.org/
[examples of parsed URLs]: https://url.spec.whatwg.org/#example-url-parsing
[`url.parse()`]: #url_url_parse_urlstring_parsequerystring_slashesdenotehost
[`url.format()`]: #url_url_format_urlobject
[Punycode]: https://tools.ietf.org/html/rfc5891#section-4.4
[`Map`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Map
[`array.toString()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/toString
[WHATWG URL]: #url_the_whatwg_url_api
