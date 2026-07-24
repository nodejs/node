# RetryAgent

<!--introduced_in=v6.7.0-->
<!--type=module-->
<!-- source_link=lib/dispatcher/retry-agent.js -->

> Stability: 2 - Stable

A [`Dispatcher`][] that automatically retries failed requests by wrapping
another dispatcher with the retry logic of [`RetryHandler`][]. Requests that fail
with a retryable status code or transport error are re-dispatched according to
the configured retry policy.

```mjs
import { RetryAgent } from 'undici'
```

## Class: `RetryAgent`

<!-- YAML
added: v6.7.0
-->

* Extends: {Dispatcher}

Wraps an existing dispatcher and applies retry behaviour to every request it
dispatches. Lifecycle methods such as [`retryAgent.close()`][] and
[`retryAgent.destroy()`][] are forwarded to the wrapped dispatcher.

### `new RetryAgent(dispatcher[, options])`

<!-- YAML
added: v6.7.0
-->

* `dispatcher` {Dispatcher} The dispatcher to wrap and add retry behaviour to.
* `options` {RetryOptions} (optional)
  * `throwOnError` {boolean} When `true`, an error is thrown on the final retry
    attempt, preventing the following handlers from being called and destroying
    the socket. Disable it when you need the response body for error responses
    or when you provide a custom error handler. **Default:** `true`.
  * `retry` {Function} Callback invoked on every retry iteration. It receives the
    error, the current retry state, and a callback. Pass an error to the callback
    to stop retrying, or call it with no argument to perform the retry.
    * `err` {Error} The error that caused the retry attempt.
    * `context` {RetryContext} The current retry context.
      * `state` {RetryState} The mutable retry state.
        * `counter` {number} The number of retries performed so far.
      * `opts` {Object} The options passed to the retry handler, combining the
        dispatch options with `retryOptions`.
    * `callback` {Function} Called to continue. Pass an `Error` to stop retrying,
      or `null`/`undefined` to perform the next retry.
  * `maxRetries` {number} Maximum number of retries to allow. **Default:** `5`.
  * `maxTimeout` {number} Maximum number of milliseconds to wait between retries.
    **Default:** `30000` (30 seconds).
  * `minTimeout` {number} Initial number of milliseconds to wait before the first
    retry. **Default:** `500` (half a second).
  * `timeoutFactor` {number} Factor by which the timeout is multiplied between
    retries. **Default:** `2`.
  * `retryAfter` {boolean} When `true`, the delay between retries is inferred from
    the `Retry-After` response header when present. **Default:** `true`.
  * `methods` {string[]} HTTP methods that are eligible for retry.
    **Default:** `['GET', 'HEAD', 'OPTIONS', 'PUT', 'DELETE', 'TRACE']`.
  * `statusCodes` {number[]} HTTP status codes that trigger a retry.
    **Default:** `[500, 502, 503, 504, 429]`.
  * `errorCodes` {string[]} Transport error codes that trigger a retry.
    **Default:** `['ECONNRESET', 'ECONNREFUSED', 'ENOTFOUND', 'ENETDOWN', 'ENETUNREACH', 'EHOSTDOWN', 'EHOSTUNREACH', 'EPIPE', 'UND_ERR_SOCKET']`.

Creates a new `RetryAgent` that wraps `dispatcher`. The `options` are stored and
used to construct a [`RetryHandler`][] for each dispatched request.

```mjs
import { Agent, RetryAgent } from 'undici'

const agent = new RetryAgent(new Agent(), {
  maxRetries: 3,
  minTimeout: 1000,
  timeoutFactor: 2,
})

const res = await agent.request({
  method: 'GET',
  origin: 'http://example.com',
  path: '/',
})

console.log(res.statusCode)
console.log(await res.body.text())
```

### `retryAgent.dispatch(options, handler)`

<!-- YAML
added: v6.7.0
-->

* `options` {DispatchOptions} The dispatch options, identical to
  [`dispatcher.dispatch()`][].
* `handler` {DispatchHandler} The dispatch handler.
* Returns: {boolean}

Dispatches `options` through the wrapped dispatcher, installing a
[`RetryHandler`][] that applies the configured retry policy before delegating to
`handler`. Returns `false` if the dispatcher is busy and the caller should wait
before dispatching again, otherwise `true`.

### `retryAgent.close()`

<!-- YAML
added: v6.7.0
-->

* Returns: {Promise} Resolves when the wrapped dispatcher has closed.

Gracefully closes the wrapped dispatcher, waiting for in-flight requests to
complete. Equivalent to calling [`dispatcher.close()`][] on the wrapped
dispatcher.

### `retryAgent.destroy()`

<!-- YAML
added: v6.7.0
-->

* Returns: {Promise} Resolves when the wrapped dispatcher has been destroyed.

Forcefully destroys the wrapped dispatcher, aborting all pending requests.
Equivalent to calling [`dispatcher.destroy()`][] on the wrapped dispatcher.

[`Dispatcher`]: Dispatcher.md#class-dispatcher
[`RetryHandler`]: RetryHandler.md#class-retryhandler
[`dispatcher.close()`]: Dispatcher.md#dispatcherclosecallback
[`dispatcher.destroy()`]: Dispatcher.md#dispatcherdestroyerror-callback
[`dispatcher.dispatch()`]: Dispatcher.md#dispatcherdispatchoptions-handler
[`retryAgent.close()`]: #retryagentclose
[`retryAgent.destroy()`]: #retryagentdestroy
