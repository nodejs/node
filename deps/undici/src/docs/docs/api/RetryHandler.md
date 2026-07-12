# RetryHandler

<!--introduced_in=v5.28.0-->
<!--type=module-->
<!-- source_link=lib/handler/retry-handler.js -->

> Stability: 2 - Stable

A [`Dispatcher`][] handler that automatically retries a request when it fails
with a recoverable network error or an eligible HTTP status code. It wraps an
inner handler and re-dispatches the request, applying an exponential backoff and
honouring the `Retry-After` response header. When a response is partially
consumed before the failure, the handler resumes the download with a `Range`
request guarded by the original `ETag`.

The handler is most often used indirectly through [`RetryAgent`][], but it can
also be supplied directly to [`dispatcher.dispatch()`][] for fine-grained
control over the retry behaviour.

```mjs
import { RetryHandler } from 'undici'
```

## Class: `RetryHandler`

<!-- YAML
added: v5.28.0
-->

* Extends: {DispatchHandler}

Implements the [`DispatchHandler`][] interface. An instance is consumed by a
single dispatch call and forwards the dispatch lifecycle to the inner `handler`,
re-issuing the request through the supplied `dispatch` function whenever a retry
is warranted.

> **Note:** The `RetryHandler` does not retry over stateful bodies (for example
> streams or `AsyncIterable`), because once consumed they cannot be replayed. In
> these situations the body is identified as stateful and the request is rejected
> with the `UND_ERR_REQ_RETRY` error instead of being retried.

### `new RetryHandler(options, retryHandlers)`

<!-- YAML
added: v5.28.0
changes:
  - version: v7.0.0
    pr-url: https://github.com/nodejs/undici/pull/3883
    description: Reimplemented on top of the new dispatch lifecycle hooks.
-->

* `options` {DispatchOptions} The dispatch options for the request, extended with
  an optional `retryOptions` field. Type is
  `DispatchOptions & { retryOptions?: RetryOptions }`.
  * `retryOptions` {RetryOptions} (optional) Configuration controlling when and
    how the request is retried. See [`RetryOptions`](#retryoptions).
* `retryHandlers` {RetryHandlers} The handlers used to drive the retry loop. See
  [`RetryHandlers`](#parameter-retryhandlers).
* Returns: {RetryHandler}

#### `RetryOptions`

* `throwOnError` {boolean} When `true`, an error is thrown on the last retry
  attempt and propagated to the inner handler; when `false`, the failing response
  is passed through instead, which is useful when the error body is needed or a
  custom error handler is in place. **Default:** `true`.
* `retry` {Function} Callback invoked on every retry iteration to decide whether
  another attempt should be made. It receives the error, the retry context, and a
  callback. Call the callback with an `Error` to stop retrying, or with `null` to
  schedule another attempt. **Default:** the built-in retry strategy described
  below.
  * `err` {Error} The error that triggered the retry.
  * `context` {RetryContext} The current retry context. See
    [`RetryContext`](#retrycontext).
  * `callback` {Function} Signals the outcome of this iteration.
    * `result` {Error|null} (optional) An `Error` to abort retrying, or `null` to
      retry.
* `maxRetries` {number} Maximum number of retries allowed. **Default:** `5`.
* `maxTimeout` {number} Maximum number of milliseconds to wait between retries.
  **Default:** `30000` (30 seconds).
* `minTimeout` {number} Initial number of milliseconds to wait before the first
  retry. **Default:** `500` (half a second).
* `timeoutFactor` {number} Multiplier applied to the timeout between successive
  retries to produce an exponential backoff. **Default:** `2`.
* `retryAfter` {boolean} When `true`, the delay before the next retry is inferred
  from the `Retry-After` response header when present. **Default:** `true`.
* `methods` {string[]} HTTP methods that are eligible for retrying. **Default:**
  `['GET', 'HEAD', 'OPTIONS', 'PUT', 'DELETE', 'TRACE']`.
* `statusCodes` {number[]} HTTP status codes that trigger a retry. **Default:**
  `[500, 502, 503, 504, 429]`.
* `errorCodes` {string[]} Network error codes that trigger a retry. **Default:**
  `['ECONNRESET', 'ECONNREFUSED', 'ENOTFOUND', 'ENETDOWN', 'ENETUNREACH', 'EHOSTDOWN', 'EHOSTUNREACH', 'EPIPE', 'UND_ERR_SOCKET']`.

The default `retry` strategy computes the delay before the next attempt as
`minTimeout * timeoutFactor ** (counter - 1)`, capped at `maxTimeout`. When a
`Retry-After` header is present it takes precedence (interpreted as seconds, or as
an HTTP date), still capped at `maxTimeout`. The default strategy stops retrying
once `counter` exceeds `maxRetries`, when the error code is not in `errorCodes`,
when the method is not in `methods`, or when the response status code is not in
`statusCodes`.

#### `RetryContext`

* `state` {RetryState} The current retry state. See [`RetryState`](#retrystate).
* `opts` {DispatchOptions} The dispatch options passed to the handler, including
  the resolved `retryOptions`. Type is
  `DispatchOptions & { retryOptions?: RetryOptions }`.

The context object passed as the second argument to the `retry` callback.

#### `RetryState`

* `counter` {number} The current retry attempt, starting at `1` for the first
  retry.

Represents the retry state for a given request.

#### Parameter: `RetryHandlers`

* `dispatch` {Function} The dispatch function called to (re-)issue the request on
  every attempt. Type is `(options, handler) => boolean`.
* `handler` {DispatchHandler} The inner handler invoked once the request succeeds
  or the retries are exhausted.

### Examples

```mjs
import { Client, RetryHandler } from 'undici'

const client = new Client(`http://localhost:${server.address().port}`)
const chunks = []

const handler = new RetryHandler(
  {
    ...dispatchOptions,
    retryOptions: {
      // Custom retry decision function.
      retry (err, { state, opts }, callback) {
        if (err.code === 'UND_ERR_DESTROYED') {
          callback(err)
          return
        }

        if (err.statusCode === 206) {
          callback(err)
          return
        }

        setTimeout(() => callback(null), 1000)
      }
    }
  },
  {
    dispatch (...args) {
      return client.dispatch(...args)
    },
    handler: {
      onRequestStart () {},
      onResponseStart (controller, status, headers) {
        // Do something with the response headers.
      },
      onResponseData (controller, chunk) {
        chunks.push(chunk)
      },
      onResponseEnd () {},
      onResponseError (controller, err) {
        // Handle the error.
      }
    }
  }
)

client.dispatch(dispatchOptions, handler)
```

A minimal handler that relies entirely on the default retry options:

```mjs
import { Client, RetryHandler } from 'undici'

const client = new Client(`http://localhost:${server.address().port}`)

const handler = new RetryHandler(dispatchOptions, {
  dispatch: client.dispatch.bind(client),
  handler: {
    onRequestStart () {},
    onResponseStart (controller, status, headers) {},
    onResponseData (controller, chunk) {},
    onResponseEnd () {},
    onResponseError (controller, err) {}
  }
})

client.dispatch(dispatchOptions, handler)
```

[`Dispatcher`]: Dispatcher.md#class-dispatcher
[`DispatchHandler`]: Dispatcher.md#parameter-dispatchhandler
[`RetryAgent`]: RetryAgent.md#class-retryagent
[`dispatcher.dispatch()`]: Dispatcher.md#dispatcherdispatchoptions-handler
