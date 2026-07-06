# Fetch

<!--introduced_in=v1.0.0-->
<!--type=module-->
<!-- source_link=lib/web/fetch/index.js -->

> Stability: 2 - Stable

undici implements the [WHATWG Fetch Standard][], providing `fetch()` together
with the `Request`, `Response`, `Headers`, and `FormData` classes that mirror
the browser APIs. The implementation follows the standard, so the
[MDN Fetch documentation][] applies as well.

```mjs
import { fetch, Request, Response, Headers, FormData } from 'undici'

const response = await fetch('https://example.com')
const text = await response.text()
```

When mixing these classes, keep them from the same implementation: use the
global `fetch()` with the global `FormData`, `Request`, `Response`, and
`Headers`, and use undici's `fetch()` with undici's classes. Passing a value
created by one implementation to the other can throw.

## `fetch(input[, init])`

<!-- YAML
added: v5.6.0
changes:
  - version: v5.6.1
    pr-url: https://github.com/nodejs/undici/pull/1529
    description: No default value is assigned to `init.method`.
-->

* `input` {string|URL|Request} The resource to fetch. A string or {URL} is
  treated as the URL to request; a {Request} is used as the request template.
* `init` {RequestInit} (optional) An options object that customizes the
  request.
  * `body` {string|Buffer|Uint8Array|Blob|FormData|URLSearchParams|ReadableStream|null}
    The request body. **Default:** `null`.
  * `cache` {string} The cache mode. One of `'default'`, `'force-cache'`,
    `'no-cache'`, `'no-store'`, `'only-if-cached'`, or `'reload'`.
  * `credentials` {string} How credentials are sent. One of `'omit'`,
    `'include'`, or `'same-origin'`.
  * `dispatcher` {Dispatcher} The {Dispatcher} used to perform the request.
    **Default:** the global dispatcher.
  * `duplex` {string} The duplex mode of the request. Must be `'half'` when a
    streaming `body` is provided.
  * `headers` {Headers|Object|Array} The request headers, as a {Headers}
    instance, a plain object, or an array of `[name, value]` pairs.
  * `integrity` {string} The subresource integrity metadata of the request.
  * `keepalive` {boolean} Whether the connection may outlive the page.
    **Default:** `false`.
  * `method` {string} The request method, for example `'GET'` or `'POST'`.
  * `mode` {string} The request mode. One of `'cors'`, `'navigate'`,
    `'no-cors'`, or `'same-origin'`.
  * `redirect` {string} How redirects are handled. One of `'error'`,
    `'follow'`, or `'manual'`.
  * `referrer` {string} The request referrer.
  * `referrerPolicy` {string} The referrer policy. One of `''`,
    `'no-referrer'`, `'no-referrer-when-downgrade'`, `'origin'`,
    `'origin-when-cross-origin'`, `'same-origin'`, `'strict-origin'`,
    `'strict-origin-when-cross-origin'`, or `'unsafe-url'`.
  * `signal` {AbortSignal|null} An {AbortSignal} used to abort the request.
    **Default:** `null`.
  * `window` {null} Can only be `null`; reserved by the standard.
* Returns: {Promise} Fulfills with a {Response} once the response headers have
  been received.

Starts the process of fetching a resource from the network and returns a
promise that fulfills with a {Response}. The promise rejects only on network
failures; an HTTP error status such as `404` still fulfills the promise, so
inspect [`response.ok`](#responseok) to detect failures.

```mjs
import { fetch } from 'undici'

const response = await fetch('https://example.com', {
  method: 'POST',
  headers: { 'content-type': 'application/json' },
  body: JSON.stringify({ hello: 'world' }),
})

console.log(response.status)
console.log(await response.json())
```

To route the request through a custom {Dispatcher} (for example a `ProxyAgent`
or `Agent` with specific options), pass it as `init.dispatcher`.

```mjs
import { fetch, Agent } from 'undici'

const response = await fetch('https://example.com', {
  dispatcher: new Agent({ connect: { rejectUnauthorized: false } }),
})
```

## Class: `FormData`

<!-- YAML
added: v4.4.4
-->

A set of key/value pairs representing form fields and their values, suitable
for use as a `fetch()` request `body`. The implementation follows the
[WHATWG Fetch Standard][]; see the [MDN `FormData` documentation][].

When using `FormData` as a request body, keep `fetch` and `FormData` from the
same implementation: use the global `FormData` with the global `fetch()`, and
undici's `FormData` with undici's `fetch()`.

### `new FormData()`

<!-- YAML
added: v4.4.4
-->

Creates a new, empty `FormData` instance. Passing any argument other than
`undefined` throws; in particular, an `HTMLFormElement` argument is not
supported in this environment.

### `formData.append(name, value[, filename])`

<!-- YAML
added: v1.0.0
-->

* `name` {string} The name of the field.
* `value` {string|Blob} The value of the field.
* `filename` {string} The filename reported to the server when `value` is a
  {Blob}. (optional)

Appends a new value to an existing key, or adds the key if it does not exist.
Unlike [`formData.set()`](#formdatasetname-value-filename), `append()` keeps
any existing values for `name`.

### `formData.delete(name)`

<!-- YAML
added: v1.0.0
-->

* `name` {string} The name of the field to remove.

Deletes all values associated with `name`.

### `formData.get(name)`

<!-- YAML
added: v1.0.0
-->

* `name` {string} The name of the field to read.
* Returns: {string|File|null} The first value associated with `name`, or
  `null` if there is none.

### `formData.getAll(name)`

<!-- YAML
added: v1.0.0
-->

* `name` {string} The name of the field to read.
* Returns: {Array} All values associated with `name`, as an array of
  {string} and {File} entries.

### `formData.has(name)`

<!-- YAML
added: v1.0.0
-->

* `name` {string} The name of the field to look up.
* Returns: {boolean} `true` if at least one value is associated with `name`.

### `formData.set(name, value[, filename])`

<!-- YAML
added: v1.0.0
-->

* `name` {string} The name of the field.
* `value` {string|Blob} The value of the field.
* `filename` {string} The filename reported to the server when `value` is a
  {Blob}. (optional)

Sets a new value for an existing key, or adds the key if it does not exist,
replacing any values previously associated with `name`.

## Class: `Response`

<!-- YAML
added: v4.4.0
-->

* Extends: {BodyMixin}

Represents the response to a request. Instances are typically obtained by
awaiting `fetch()`, but can also be constructed directly. The implementation
follows the [WHATWG Fetch Standard][]; see the
[MDN `Response` documentation][]. `Response` inherits the body-reading methods
described in [Body mixin](#body-mixin).

### `new Response([body[, init]])`

<!-- YAML
added: v4.4.0
-->

* `body` {string|Buffer|Uint8Array|Blob|FormData|URLSearchParams|ReadableStream|null}
  The response body. **Default:** `null`.
* `init` {ResponseInit} (optional)
  * `status` {number} The response status code. **Default:** `200`.
  * `statusText` {string} The response status message. **Default:** `''`.
  * `headers` {Headers|Object|Array} The response headers.

Creates a new `Response`.

### Static method: `Response.error()`

<!-- YAML
added: v1.0.0
-->

* Returns: {Response} A network-error response.

Returns a new `Response` representing a network error, with its
[`type`](#responsetype) set to `'error'`.

### Static method: `Response.json(data[, init])`

<!-- YAML
added: v1.0.0
-->

* `data` {any} The value to serialize as JSON.
* `init` {ResponseInit} (optional)
  * `status` {number} The response status code. **Default:** `200`.
  * `statusText` {string} The response status message. **Default:** `''`.
  * `headers` {Headers|Object|Array} The response headers.
* Returns: {Response} A response whose body is the JSON serialization of
  `data` and whose `Content-Type` is `application/json`.

```mjs
import { Response } from 'undici'

const response = Response.json({ ok: true }, { status: 201 })
```

### Static method: `Response.redirect(url[, status])`

<!-- YAML
added: v1.0.0
-->

* `url` {string|URL} The URL to redirect to.
* `status` {number} The redirect status code. One of `301`, `302`, `303`,
  `307`, or `308`. **Default:** `302`.
* Returns: {Response} A redirect response with the `Location` header set to
  `url`.

### `response.clone()`

<!-- YAML
added: v1.0.0
-->

* Returns: {Response} A copy of the response.

Creates a clone of the response. Throws a `TypeError` if the body has already
been read or is locked.

### `response.type`

<!-- YAML
added: v1.0.0
-->

* Type: {string} The response type. One of `'basic'`, `'cors'`, `'default'`,
  `'error'`, `'opaque'`, or `'opaqueredirect'`.

### `response.url`

<!-- YAML
added: v1.0.0
-->

* Type: {string} The final URL of the response after any redirects, or the
  empty string if not available.

### `response.redirected`

<!-- YAML
added: v1.0.0
-->

* Type: {boolean} `true` if the response is the result of one or more
  redirects.

### `response.status`

<!-- YAML
added: v1.0.0
-->

* Type: {number} The HTTP status code of the response.

### `response.ok`

<!-- YAML
added: v1.0.0
-->

* Type: {boolean} `true` when [`status`](#responsestatus) is in the range
  `200`–`299`.

### `response.statusText`

<!-- YAML
added: v1.0.0
-->

* Type: {string} The status message corresponding to the status code.

### `response.headers`

<!-- YAML
added: v1.0.0
-->

* Type: {Headers} The {Headers} object associated with the response.

## Class: `Request`

<!-- YAML
added: v1.0.0
-->

* Extends: {BodyMixin}

Represents a resource request. Instances can be passed to `fetch()` in place
of a URL string. The implementation follows the [WHATWG Fetch Standard][]; see
the [MDN `Request` documentation][]. `Request` inherits the body-reading
methods described in [Body mixin](#body-mixin).

### `new Request(input[, init])`

<!-- YAML
added: v1.0.0
-->

* `input` {string|URL|Request} The resource to request, as a URL string,
  {URL}, or another {Request} to copy.
* `init` {RequestInit} (optional) An options object with the same fields as
  the `init` argument of [`fetch()`](#fetchinput-init).
* Returns: {Request}

Creates a new `Request`.

### `request.clone()`

<!-- YAML
added: v1.0.0
-->

* Returns: {Request} A copy of the request.

Creates a clone of the request. Throws a `TypeError` if the body has already
been read or is locked.

### `request.method`

<!-- YAML
added: v1.0.0
-->

* Type: {string} The request method, for example `'GET'`.

### `request.url`

<!-- YAML
added: v1.0.0
-->

* Type: {string} The serialized URL of the request.

### `request.headers`

<!-- YAML
added: v1.0.0
-->

* Type: {Headers} The {Headers} object associated with the request.

### `request.destination`

<!-- YAML
added: v1.0.0
-->

* Type: {string} The request destination, indicating the type of content being
  requested, for example `''`, `'image'`, or `'script'`.

### `request.referrer`

<!-- YAML
added: v1.0.0
-->

* Type: {string} The referrer of the request. May be `'about:client'` or a URL
  string.

### `request.referrerPolicy`

<!-- YAML
added: v1.0.0
-->

* Type: {string} The referrer policy of the request.

### `request.mode`

<!-- YAML
added: v1.0.0
-->

* Type: {string} The mode of the request. One of `'cors'`, `'navigate'`,
  `'no-cors'`, or `'same-origin'`.

### `request.credentials`

<!-- YAML
added: v1.0.0
-->

* Type: {string} The credentials mode of the request. One of `'omit'`,
  `'include'`, or `'same-origin'`.

### `request.cache`

<!-- YAML
added: v1.0.0
-->

* Type: {string} The cache mode of the request.

### `request.redirect`

<!-- YAML
added: v1.0.0
-->

* Type: {string} The redirect mode of the request. One of `'error'`,
  `'follow'`, or `'manual'`.

### `request.integrity`

<!-- YAML
added: v1.0.0
-->

* Type: {string} The subresource integrity metadata of the request.

### `request.keepalive`

<!-- YAML
added: v1.0.0
-->

* Type: {boolean} Whether the request may outlive the environment that created
  it.

### `request.isReloadNavigation`

<!-- YAML
added: v1.0.0
-->

* Type: {boolean} `true` if the request is a reload navigation.

### `request.isHistoryNavigation`

<!-- YAML
added: v1.0.0
-->

* Type: {boolean} `true` if the request is a history navigation.

### `request.signal`

<!-- YAML
added: v1.0.0
-->

* Type: {AbortSignal} The {AbortSignal} associated with the request.

### `request.duplex`

<!-- YAML
added: v1.0.0
-->

* Type: {string} The duplex mode of the request. Always `'half'`.

## Class: `Headers`

<!-- YAML
added: v4.4.0
-->

Represents the header list of a request or response and provides methods to
read and modify it. The implementation follows the [WHATWG Fetch Standard][];
see the [MDN `Headers` documentation][]. `Headers` is iterable, yielding
`[name, value]` pairs sorted by name.

### `new Headers([init])`

<!-- YAML
added: v4.4.0
-->

* `init` {Headers|Object|Array} (optional) Initial headers, as a {Headers}
  instance, a plain object of name/value pairs, or an array of `[name, value]`
  pairs.

Creates a new `Headers` object.

### `headers.append(name, value)`

<!-- YAML
added: v1.0.0
-->

* `name` {string} The name of the header.
* `value` {string} The value of the header.

Appends a value to a header, or adds the header if it does not exist. Existing
values for `name` are preserved.

### `headers.delete(name)`

<!-- YAML
added: v1.0.0
-->

* `name` {string} The name of the header to remove.

Removes the header named `name`.

### `headers.get(name)`

<!-- YAML
added: v1.0.0
-->

* `name` {string} The name of the header to read.
* Returns: {string|null} The combined values of the header, or `null` if it is
  not present.

### `headers.has(name)`

<!-- YAML
added: v1.0.0
-->

* `name` {string} The name of the header to look up.
* Returns: {boolean} `true` if the header is present.

### `headers.set(name, value)`

<!-- YAML
added: v1.0.0
-->

* `name` {string} The name of the header.
* `value` {string} The value of the header.

Sets a header to a single value, replacing any existing values for `name`.

### `headers.getSetCookie()`

<!-- YAML
added: v1.0.0
-->

* Returns: {string[]} An array of the values of all `Set-Cookie` headers.

Returns each `Set-Cookie` header as a separate string, without combining them.

## Body mixin

`Request` and `Response` both extend {BodyMixin}, which provides methods and
properties for reading a body. Each consuming method reads the body once; after
the body has been consumed, [`bodyUsed`](#bodybodyused) becomes `true` and
calling another consuming method throws a `TypeError`.

### `body.arrayBuffer()`

<!-- YAML
added: v1.0.0
-->

* Returns: {Promise} Fulfills with an {ArrayBuffer} containing the body bytes.

### `body.blob()`

<!-- YAML
added: v1.0.0
-->

* Returns: {Promise} Fulfills with a {Blob} containing the body.

### `body.bytes()`

<!-- YAML
added: v1.0.0
-->

* Returns: {Promise} Fulfills with a {Uint8Array} containing the body bytes.

### `body.formData()`

<!-- YAML
added: v1.0.0
-->

> Stability: 0 - Deprecated

* Returns: {Promise} Fulfills with a {FormData} parsed from the body.

Buffers and parses the entire body as `multipart/form-data` or
`application/x-www-form-urlencoded`. Because multipart parsing has inherent
security risks and the whole body is buffered, this method must only be called
on responses from trusted servers.

For responses from untrusted or user-controlled servers, use a dedicated
streaming parser such as [@fastify/busboy][] and apply application-specific
limits:

```mjs
import { Busboy } from '@fastify/busboy'
import { Readable } from 'node:stream'

const response = await fetch('...')
const busboy = new Busboy({
  headers: { 'content-type': response.headers.get('content-type') },
})

// Handle the events emitted by `busboy`.

Readable.fromWeb(response.body).pipe(busboy)
```

### `body.json()`

<!-- YAML
added: v1.0.0
-->

* Returns: {Promise} Fulfills with the result of parsing the body as JSON.

### `body.text()`

<!-- YAML
added: v1.0.0
-->

* Returns: {Promise} Fulfills with a {string} containing the body decoded as
  UTF-8.

### `body.textStream()`

<!-- YAML
added: v1.0.0
-->

> Stability: 1 - Experimental

* Returns: {ReadableStream} A {ReadableStream} of {string} chunks produced by
  decoding the body as UTF-8.

An undici-specific extension that exposes the body as a stream of decoded text
chunks rather than buffering it. It is not part of the [WHATWG Fetch
Standard][].

### `body.body`

<!-- YAML
added: v1.0.0
-->

* Type: {ReadableStream|null} The body as a {ReadableStream}, or `null` if the
  message has no body.

### `body.bodyUsed`

<!-- YAML
added: v1.0.0
-->

* Type: {boolean} `true` once the body has been read.

[@fastify/busboy]: https://www.npmjs.com/package/@fastify/busboy
[MDN Fetch documentation]: https://developer.mozilla.org/en-US/docs/Web/API/fetch
[MDN `FormData` documentation]: https://developer.mozilla.org/en-US/docs/Web/API/FormData
[MDN `Headers` documentation]: https://developer.mozilla.org/en-US/docs/Web/API/Headers
[MDN `Request` documentation]: https://developer.mozilla.org/en-US/docs/Web/API/Request
[MDN `Response` documentation]: https://developer.mozilla.org/en-US/docs/Web/API/Response
[WHATWG Fetch Standard]: https://fetch.spec.whatwg.org/
