# Errors

<!--introduced_in=v1.0.0-->
<!--type=module-->
<!-- source_link=lib/core/errors.js -->

> Stability: 2 - Stable

undici raises typed error objects so that failures can be distinguished
programmatically. Every error class is exposed through the `errors` namespace of
the package:

```js
import { errors } from 'undici'

if (err instanceof errors.ConnectTimeoutError) {
  // handle connect timeout
}
```

All errors, except [`HTTPParserError`][], extend [`UndiciError`][]. Each error
carries a stable `code` string (for example `UND_ERR_CONNECT_TIMEOUT`) and a
`name`.

Because the bundled (global) dispatcher may come from a different undici version
than the one you import directly, prefer matching on `error.code` rather than
`instanceof errors.UndiciError`:

```js
if (err.code === 'UND_ERR_CONNECT_TIMEOUT') {
  // handle connect timeout
}
```

## Class: `UndiciError`

<!-- YAML
added: v1.0.0
changes:
  - version: v7.16.0
    pr-url: https://github.com/nodejs/undici/pull/4472
    description: Made `UndiciError` and its subclasses reliable with `instanceof`
                 across realms and undici versions.
-->

* Extends: {Error}

The base class for all undici errors. It is reliable with `instanceof` even when
the error originates from a different undici version, because the check is based
on a well-known symbol rather than the prototype chain.

* `name` {string} Always `'UndiciError'`.
* `code` {string} Always `'UND_ERR'`.

## Class: `ConnectTimeoutError`

<!-- YAML
added: v1.0.0
changes:
  - version: v7.16.0
    pr-url: https://github.com/nodejs/undici/pull/4472
    description: Made the error reliable with `instanceof`.
-->

* Extends: {UndiciError}

The socket was destroyed because establishing the connection exceeded the
`connectTimeout` option.

* `name` {string} Always `'ConnectTimeoutError'`.
* `code` {string} Always `'UND_ERR_CONNECT_TIMEOUT'`.

When `autoSelectFamily` is enabled and every attempted address times out, Node.js
raises an `AggregateError`. undici normalizes these multi-address timeouts into a
`ConnectTimeoutError` so that the error shape is the same regardless of whether
Node.js's per-address timer or undici's `connectTimeout` wins the race. The
original `AggregateError` is preserved on `error.cause`.

## Class: `HeadersTimeoutError`

<!-- YAML
added: v1.0.0
changes:
  - version: v7.16.0
    pr-url: https://github.com/nodejs/undici/pull/4472
    description: Made the error reliable with `instanceof`.
-->

* Extends: {UndiciError}

The socket was destroyed because receiving the response headers exceeded the
`headersTimeout` option.

* `name` {string} Always `'HeadersTimeoutError'`.
* `code` {string} Always `'UND_ERR_HEADERS_TIMEOUT'`.

## Class: `HeadersOverflowError`

<!-- YAML
added: v1.0.0
changes:
  - version: v7.16.0
    pr-url: https://github.com/nodejs/undici/pull/4472
    description: Made the error reliable with `instanceof`.
-->

* Extends: {UndiciError}

The socket was destroyed because the response headers exceeded the maximum
allowed size.

* `name` {string} Always `'HeadersOverflowError'`.
* `code` {string} Always `'UND_ERR_HEADERS_OVERFLOW'`.

## Class: `BodyTimeoutError`

<!-- YAML
added: v1.0.0
changes:
  - version: v7.16.0
    pr-url: https://github.com/nodejs/undici/pull/4472
    description: Made the error reliable with `instanceof`.
-->

* Extends: {UndiciError}

The socket was destroyed because reading the response body exceeded the
`bodyTimeout` option.

* `name` {string} Always `'BodyTimeoutError'`.
* `code` {string} Always `'UND_ERR_BODY_TIMEOUT'`.

## Class: `InvalidArgumentError`

<!-- YAML
added: v1.0.0
changes:
  - version: v7.16.0
    pr-url: https://github.com/nodejs/undici/pull/4472
    description: Made the error reliable with `instanceof`.
-->

* Extends: {UndiciError}

An invalid argument was passed to an undici API.

* `name` {string} Always `'InvalidArgumentError'`.
* `code` {string} Always `'UND_ERR_INVALID_ARG'`.

## Class: `InvalidReturnValueError`

<!-- YAML
added: v1.0.0
changes:
  - version: v7.16.0
    pr-url: https://github.com/nodejs/undici/pull/4472
    description: Made the error reliable with `instanceof`.
-->

* Extends: {UndiciError}

A user-supplied callback or interceptor returned an invalid value.

* `name` {string} Always `'InvalidReturnValueError'`.
* `code` {string} Always `'UND_ERR_INVALID_RETURN_VALUE'`.

## Class: `AbortError`

<!-- YAML
added: v1.0.0
changes:
  - version: v7.16.0
    pr-url: https://github.com/nodejs/undici/pull/4472
    description: Made the error reliable with `instanceof`.
-->

* Extends: {UndiciError}

The operation was aborted. This is the base class of [`RequestAbortedError`][].

* `name` {string} Always `'AbortError'`.
* `code` {string} Always `'UND_ERR_ABORT'`.

## Class: `RequestAbortedError`

<!-- YAML
added: v1.0.0
changes:
  - version: v7.16.0
    pr-url: https://github.com/nodejs/undici/pull/4472
    description: Made the error reliable with `instanceof`.
-->

* Extends: {AbortError}

The request was aborted by the user, typically through an `AbortSignal`.

* `name` {string} Always `'AbortError'`.
* `code` {string} Always `'UND_ERR_ABORTED'`.

## Class: `InformationalError`

<!-- YAML
added: v1.0.0
changes:
  - version: v7.16.0
    pr-url: https://github.com/nodejs/undici/pull/4472
    description: Made the error reliable with `instanceof`.
-->

* Extends: {UndiciError}

An expected error carrying a reason, used internally to surface a cause for a
failed or retried request (for example as the `cause` of another error).

* `name` {string} Always `'InformationalError'`.
* `code` {string} Always `'UND_ERR_INFO'`.

## Class: `RequestContentLengthMismatchError`

<!-- YAML
added: v1.0.0
changes:
  - version: v7.16.0
    pr-url: https://github.com/nodejs/undici/pull/4472
    description: Made the error reliable with `instanceof`.
-->

* Extends: {UndiciError}

The length of the request body did not match the `content-length` header.

* `name` {string} Always `'RequestContentLengthMismatchError'`.
* `code` {string} Always `'UND_ERR_REQ_CONTENT_LENGTH_MISMATCH'`.

## Class: `ResponseContentLengthMismatchError`

<!-- YAML
added: v1.0.0
changes:
  - version: v7.16.0
    pr-url: https://github.com/nodejs/undici/pull/4472
    description: Made the error reliable with `instanceof`.
-->

* Extends: {UndiciError}

The length of the response body did not match the `content-length` header.

* `name` {string} Always `'ResponseContentLengthMismatchError'`.
* `code` {string} Always `'UND_ERR_RES_CONTENT_LENGTH_MISMATCH'`.

## Class: `ClientDestroyedError`

<!-- YAML
added: v1.0.0
changes:
  - version: v7.16.0
    pr-url: https://github.com/nodejs/undici/pull/4472
    description: Made the error reliable with `instanceof`.
-->

* Extends: {UndiciError}

An operation was attempted on a client that has already been destroyed.

* `name` {string} Always `'ClientDestroyedError'`.
* `code` {string} Always `'UND_ERR_DESTROYED'`.

## Class: `ClientClosedError`

<!-- YAML
added: v1.0.0
changes:
  - version: v7.16.0
    pr-url: https://github.com/nodejs/undici/pull/4472
    description: Made the error reliable with `instanceof`.
-->

* Extends: {UndiciError}

An operation was attempted on a client that has already been closed.

* `name` {string} Always `'ClientClosedError'`.
* `code` {string} Always `'UND_ERR_CLOSED'`.

## Class: `SocketError`

<!-- YAML
added: v1.0.0
changes:
  - version: v7.16.0
    pr-url: https://github.com/nodejs/undici/pull/4472
    description: Made the error reliable with `instanceof`.
-->

* Extends: {UndiciError}

An error occurred on the underlying socket.

* `name` {string} Always `'SocketError'`.
* `code` {string} Always `'UND_ERR_SOCKET'`.
* `socket` {SocketInfo|null} Metadata about the socket at the time of the error,
  or `null` when no socket information is available.

The `socket` property, when present, holds the following fields:

```ts
interface SocketInfo {
  localAddress?: string
  localPort?: number
  remoteAddress?: string
  remotePort?: number
  remoteFamily?: string
  timeout?: number
  bytesWritten?: number
  bytesRead?: number
}
```

## Class: `NotSupportedError`

<!-- YAML
added: v1.0.0
changes:
  - version: v7.16.0
    pr-url: https://github.com/nodejs/undici/pull/4472
    description: Made the error reliable with `instanceof`.
-->

* Extends: {UndiciError}

Unsupported functionality was requested.

* `name` {string} Always `'NotSupportedError'`.
* `code` {string} Always `'UND_ERR_NOT_SUPPORTED'`.

## Class: `BalancedPoolMissingUpstreamError`

<!-- YAML
added: v1.0.0
changes:
  - version: v7.16.0
    pr-url: https://github.com/nodejs/undici/pull/4472
    description: Made the error reliable with `instanceof`.
-->

* Extends: {UndiciError}

No upstream has been added to the [`BalancedPool`][].

* `name` {string} Always `'MissingUpstreamError'`.
* `code` {string} Always `'UND_ERR_BPL_MISSING_UPSTREAM'`.

## Class: `HTTPParserError`

<!-- YAML
added: v4.0.0
-->

* Extends: {Error}

An error occurred while parsing the HTTP response. Unlike the other error
classes, `HTTPParserError` extends {Error} directly rather than
[`UndiciError`][].

* `name` {string} Always `'HTTPParserError'`.
* `code` {string} An `HPE_`-prefixed parser error code, or `undefined` when no
  code was provided.
* `data` {string} The raw data associated with the parser failure, or
  `undefined` when none was provided.

## Class: `ResponseExceededMaxSizeError`

<!-- YAML
added: v1.0.0
changes:
  - version: v7.16.0
    pr-url: https://github.com/nodejs/undici/pull/4472
    description: Made the error reliable with `instanceof`.
-->

* Extends: {UndiciError}

The response body exceeded the maximum allowed size.

* `name` {string} Always `'ResponseExceededMaxSizeError'`.
* `code` {string} Always `'UND_ERR_RES_EXCEEDED_MAX_SIZE'`.

## Class: `RequestRetryError`

<!-- YAML
added: v1.0.0
changes:
  - version: v7.16.0
    pr-url: https://github.com/nodejs/undici/pull/4472
    description: Made the error reliable with `instanceof`.
-->

* Extends: {UndiciError}

A request failed and could not be retried, for example because the body is
stateful (a stream or `AsyncIterable`) and cannot be replayed.

* `name` {string} Always `'RequestRetryError'`.
* `code` {string} Always `'UND_ERR_REQ_RETRY'`.
* `statusCode` {number} The HTTP status code of the failed response.
* `data` {Object} Retry metadata.
  * `count` {number} The number of retry attempts that were made.
* `headers` {Record<string, string|string[]>} The response headers.

### `new RequestRetryError(message, code, options)`

* `message` {string} The error message.
* `code` {number} The HTTP status code of the failed response.
* `options` {Object}
  * `headers` {Object|string[]|null} The response headers. (optional)
  * `data` {Object|string|null} Retry metadata. (optional)

## Class: `ResponseError`

<!-- YAML
added: v1.0.0
changes:
  - version: v7.16.0
    pr-url: https://github.com/nodejs/undici/pull/4472
    description: Made the error reliable with `instanceof`.
-->

* Extends: {UndiciError}

The response returned an error status code. This is raised, for example, when the
`throwOnError` option is enabled.

* `name` {string} Always `'ResponseError'`.
* `code` {string} Always `'UND_ERR_RESPONSE'`.
* `statusCode` {number} The HTTP status code of the response.
* `body` {Object|string|null} The response body.
* `headers` {Object|string[]|null} The response headers.

### `new ResponseError(message, code, options)`

* `message` {string} The error message.
* `code` {number} The HTTP status code of the response.
* `options` {Object}
  * `headers` {Object|string[]|null} The response headers. (optional)
  * `body` {Object|string|null} The response body. (optional)

## Class: `SecureProxyConnectionError`

<!-- YAML
added: v1.0.0
changes:
  - version: v7.16.0
    pr-url: https://github.com/nodejs/undici/pull/4472
    description: Made the error reliable with `instanceof`.
-->

* Extends: {UndiciError}

A TLS connection to a proxy failed.

* `name` {string} Always `'SecureProxyConnectionError'`.
* `code` {string} Always `'UND_ERR_PRX_TLS'`.
* `cause` {Error} The underlying error that caused the proxy connection to fail.

### `new SecureProxyConnectionError(cause[, message[, options]])`

* `cause` {Error} The underlying error. (optional)
* `message` {string} The error message. (optional)
* `options` {Object} Additional `Error` options merged with `cause`. (optional)

## Class: `MaxOriginsReachedError`

<!-- YAML
added: v1.0.0
changes:
  - version: v7.16.0
    pr-url: https://github.com/nodejs/undici/pull/4365
    description: Added to support capping the number of origins in `Agent`.
-->

* Extends: {UndiciError}

The maximum number of allowed origins has been reached.

* `name` {string} Always `'MaxOriginsReachedError'`.
* `code` {string} Always `'UND_ERR_MAX_ORIGINS_REACHED'`.

## Class: `Socks5ProxyError`

<!-- YAML
added: v7.23.0
-->

* Extends: {UndiciError}

An error occurred during SOCKS5 proxy negotiation.

* `name` {string} Always `'Socks5ProxyError'`.
* `code` {string} The error code. **Default:** `'UND_ERR_SOCKS5'`.

### `new Socks5ProxyError([message[, code]])`

* `message` {string} The error message. (optional)
* `code` {string} The error code. **Default:** `'UND_ERR_SOCKS5'`. (optional)

## Class: `MessageSizeExceededError`

<!-- YAML
added: v1.0.0
-->

* Extends: {UndiciError}

A decompressed WebSocket message exceeded the maximum allowed size.

* `name` {string} Always `'MessageSizeExceededError'`.
* `code` {string} Always `'UND_ERR_WS_MESSAGE_SIZE_EXCEEDED'`.

[`BalancedPool`]: BalancedPool.md#class-balancedpool
[`HTTPParserError`]: #class-httpparsererror
[`RequestAbortedError`]: #class-requestabortederror
[`UndiciError`]: #class-undicierror
