# RedirectHandler

<!--introduced_in=v4.0.0-->
<!--type=module-->
<!-- source_link=lib/handler/redirect-handler.js -->

> Stability: 2 - Stable

A dispatch handler that follows HTTP redirects on behalf of another handler. It
wraps a downstream `handler` and a `dispatch` function: when a redirectable
response is received it transparently re-dispatches the request to the new
location, forwarding the lifecycle callbacks to the wrapped `handler` only once a
final (non-redirect) response is reached.

Redirects are followed for the `300`, `301`, `302`, `303`, `307`, and `308`
status codes, up to `maxRedirections` hops. Following the relevant HTTP
specifications, the method is downgraded to `GET` for `301`/`302` `POST`
responses and for `303` responses with any method other than `HEAD`, request
bodies that have already been consumed are not replayed, and headers that refer
to the original URL (such as `host`) are stripped on each hop, with additional
credential headers removed on cross-origin redirects.

```mjs
import { RedirectHandler } from 'undici'
```

```cjs
const { RedirectHandler } = require('undici')
```

Most users do not construct `RedirectHandler` directly. The `maxRedirections`
option on [`Dispatcher`][] methods and the higher-level clients use it
internally.

## Class: `RedirectHandler`

<!-- YAML
added: v4.0.0
-->

* Extends: {DispatchHandler}

Implements the [`DispatchHandler`][] interface, delegating every callback to the
wrapped `handler` while intercepting redirect responses.

### `new RedirectHandler(dispatch, maxRedirections, opts, handler)`

<!-- YAML
added: v4.0.0
-->

* `dispatch` {Function} The dispatch function used to issue the original request
  and every subsequent redirect. Called as `dispatch(options, handler)`.
* `maxRedirections` {number|null} The maximum number of redirects to follow. Must
  be a non-negative integer or `null`. When `null` or `0`, no redirects are
  followed and redirect responses are passed through to the wrapped `handler`.
* `opts` {DispatchOptions} The dispatch options for the request. In addition to
  the standard dispatch options, the following redirect-specific fields are
  recognized:
  * `throwOnMaxRedirect` {boolean} When `true`, an error is thrown once
    `maxRedirections` is reached instead of returning the last redirect response.
    **Default:** `false`.
  * `stripHeadersOnRedirect` {string[]} Header names to remove from the request on
    every redirect hop. **Default:** `null`.
  * `stripHeadersOnCrossOriginRedirect` {string[]} Additional header names to
    remove from the request when a redirect points to a different origin.
    **Default:** `null`.
* `handler` {DispatchHandler} The downstream handler that receives the lifecycle
  callbacks for the final response.

Throws an [`InvalidArgumentError`][] when `maxRedirections` is not a non-negative
integer, when `throwOnMaxRedirect` is not a boolean, or when either
`stripHeadersOnRedirect` or `stripHeadersOnCrossOriginRedirect` is not an array of
header-name strings.

```mjs
import { Client, RedirectHandler } from 'undici'

const client = new Client('http://example.com')
const dispatch = client.dispatch.bind(client)

const handler = new RedirectHandler(dispatch, 5, {
  method: 'GET',
  path: '/',
  origin: 'http://example.com',
}, {
  onRequestStart () {},
  onResponseStart (controller, statusCode, headers) {
    console.log(statusCode)
  },
  onResponseData (controller, chunk) {},
  onResponseEnd () {},
})

dispatch({ method: 'GET', path: '/', origin: 'http://example.com' }, handler)
```

### Static method: `RedirectHandler.buildDispatch(dispatcher, maxRedirections)`

<!-- YAML
added: v7.0.0
-->

* `dispatcher` {Dispatcher} The dispatcher whose `dispatch` method is wrapped.
* `maxRedirections` {number|null} The maximum number of redirects to follow. Must
  be a non-negative integer or `null`.
* Returns: {Function} A `dispatch(options, handler)` function that wraps each call
  in a `RedirectHandler` before delegating to `dispatcher`.

Builds a redirect-aware dispatch function from an existing dispatcher. The
returned function has the same shape as `dispatcher.dispatch` but follows
redirects automatically. Throws an [`InvalidArgumentError`][] when
`maxRedirections` is not a non-negative integer.

```mjs
import { Client, RedirectHandler } from 'undici'

const client = new Client('http://example.com')
const dispatch = RedirectHandler.buildDispatch(client, 5)

dispatch({ method: 'GET', path: '/', origin: 'http://example.com' }, {
  onRequestStart () {},
  onResponseStart (controller, statusCode) {
    console.log(statusCode)
  },
  onResponseData () {},
  onResponseEnd () {},
})
```

### `redirectHandler.onRequestStart(controller, context)`

<!-- YAML
added: v7.0.0
-->

* `controller` {DispatchController} The controller for the in-flight request.
* `context` {Object} The dispatch context.

Forwards the request-start event to the wrapped `handler`, augmenting `context`
with the current redirect `history`.

### `redirectHandler.onRequestUpgrade(controller, statusCode, headers, socket)`

<!-- YAML
added: v7.0.0
-->

* `controller` {DispatchController} The controller for the in-flight request.
* `statusCode` {number} The HTTP status code of the upgrade response.
* `headers` {Object} The response headers.
* `socket` {Duplex} The upgraded socket.

Forwards a connection upgrade to the wrapped `handler`.

### `redirectHandler.onResponseStart(controller, statusCode, headers, statusMessage)`

<!-- YAML
added: v7.0.0
-->

* `controller` {DispatchController} The controller for the in-flight request.
* `statusCode` {number} The HTTP status code of the response.
* `headers` {Object} The response headers.
* `statusMessage` {string} The HTTP status message.

Inspects the response headers to decide whether to follow a redirect. When the
status code is redirectable, the `maxRedirections` limit has not been reached, and
the request body has not been consumed, the `location` property is set, the method
and headers are adjusted for the next hop, and the response is not forwarded.
Otherwise the event is forwarded to the wrapped `handler`.

Throws when `throwOnMaxRedirect` is `true` and the number of recorded redirects
has reached `maxRedirections`, and throws an [`InvalidArgumentError`][] if a
redirect loop is detected (for example when a [`Client`][] or [`Pool`][] is used
for a cross-origin redirect; use an [`Agent`][] instead).

### `redirectHandler.onResponseData(controller, chunk)`

<!-- YAML
added: v7.0.0
-->

* `controller` {DispatchController} The controller for the in-flight request.
* `chunk` {Buffer} A chunk of the response body.

Forwards body data to the wrapped `handler`. While a redirect is pending
(`location` is set), the `3xx` response body is ignored.

### `redirectHandler.onResponseEnd(controller, trailers)`

<!-- YAML
added: v7.0.0
-->

* `controller` {DispatchController} The controller for the in-flight request.
* `trailers` {Object} The response trailers.

Completes the response. When a redirect is pending, the request is re-dispatched
to the new `location`; otherwise the end event is forwarded to the wrapped
`handler`.

### `redirectHandler.onResponseError(controller, error)`

<!-- YAML
added: v7.0.0
-->

* `controller` {DispatchController} The controller for the in-flight request.
* `error` {Error} The error that occurred.

Forwards the error to the wrapped `handler`.

### `redirectHandler.location`

<!-- YAML
added: v4.0.0
-->

* Type: {string|null}

The `Location` header value of the redirect currently being followed, or `null`
when no redirect is pending and the response is being passed through to the
wrapped `handler`.

### `redirectHandler.opts`

<!-- YAML
added: v7.12.0
-->

* Type: {DispatchOptions}

A copy of the dispatch options for the next request. It excludes the redirect
control fields (`maxRedirections`, `stripHeadersOnRedirect`, and
`stripHeadersOnCrossOriginRedirect`) and is mutated on each hop to reflect the
updated method, path, origin, and headers.

### `redirectHandler.maxRedirections`

<!-- YAML
added: v4.0.0
-->

* Type: {number|null}

The maximum number of redirects this handler will follow.

### `redirectHandler.handler`

<!-- YAML
added: v4.0.0
-->

* Type: {DispatchHandler}

The wrapped downstream handler that receives the lifecycle callbacks for the final
response.

### `redirectHandler.history`

<!-- YAML
added: v4.0.0
-->

* Type: {URL[]}

The ordered list of URLs visited while following redirects. It is used to enforce
`maxRedirections` and to detect redirect loops.

[`Agent`]: Agent.md#class-agent
[`Client`]: Client.md#class-client
[`Dispatcher`]: Dispatcher.md#class-dispatcher
[`DispatchHandler`]: Dispatcher.md#parameter-dispatchhandler
[`InvalidArgumentError`]: Errors.md#class-invalidargumenterror
[`Pool`]: Pool.md#class-pool
