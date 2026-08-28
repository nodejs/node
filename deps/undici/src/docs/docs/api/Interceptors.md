# Interceptors

Undici ships with a set of built-in interceptors that can be composed via
`dispatcher.compose()` to add cross-cutting behaviour such as automatic
retries, response decompression, redirect following, DNS caching, and more.

## Usage

```js
import { Agent, interceptors } from 'undici'

const { retry, redirect, decompress, dump, responseError, dns, cache, deduplicate } = interceptors

const agent = new Agent().compose([
  retry({ maxRetries: 3 }),
  redirect({ maxRedirections: 5 }),
  decompress()
])

const response = await agent.request({ origin: 'https://example.com', path: '/', method: 'GET' })
```

You can also apply interceptors to a single `Client` or `Pool`:

```js
import { Client, interceptors } from 'undici'

const client = new Client('https://example.com').compose(
  interceptors.retry({ maxRetries: 2 })
)
```

---

## Custom interceptors

Custom interceptors use the same shape as
[`dispatcher.compose()`](./Dispatcher.md#dispatchercomposeinterceptors-interceptor):
an interceptor takes a `dispatch` function and returns another dispatch-like
function with the same `(options, handler)` signature.

When an interceptor wraps the handler, forward the callbacks that it does not
handle itself. The complete handler callback list is documented under
[`dispatcher.dispatch(options, handler)`](./Dispatcher.md#dispatcherdispatchoptions-handler).

```js
import { Agent } from 'undici'

const timingInterceptor = dispatch => {
  return (options, handler) => {
    const started = performance.now()

    return dispatch(options, {
      ...handler,
      onResponseStart (controller, statusCode, headers, statusMessage) {
        const duration = Math.round(performance.now() - started)
        const method = options.method ?? 'GET'
        const origin = options.origin ?? ''

        console.log(`${method} ${origin}${options.path} -> ${statusCode} in ${duration}ms`)

        return handler.onResponseStart?.(
          controller,
          statusCode,
          headers,
          statusMessage
        )
      },
      onResponseError (controller, error) {
        const duration = Math.round(performance.now() - started)

        console.error(`request failed after ${duration}ms`, error)

        return handler.onResponseError?.(controller, error)
      }
    })
  }
}

const dispatcher = new Agent().compose(timingInterceptor)

const { body } = await dispatcher.request({
  origin: 'https://example.com',
  path: '/',
  method: 'GET'
})

await body.dump()
await dispatcher.close()
```

---

## `interceptors.dump([opts])`

Reads and discards the response body up to a configurable size limit. Useful
for keeping a connection alive after an error response without reading the
body yourself.

**Parameters**

* `opts` {Object} (optional)
  * `maxSize` {number} Maximum number of bytes to read and discard. Responses
    whose `Content-Length` exceeds this value are aborted. **Default:**
    `1_048_576` (1 MiB).

Per-request override: set `dumpMaxSize` on the dispatch options to override
the global `maxSize` for a specific request.

**Returns:** {Dispatcher.DispatcherComposeInterceptor}

**Example**

```js
import { Agent, interceptors } from 'undici'

const agent = new Agent().compose(
  interceptors.dump({ maxSize: 128 * 1024 }) // discard up to 128 KiB
)
```

---

## `interceptors.retry([opts])`

Automatically retries failed requests using the same options accepted by
[`RetryHandler`](./RetryHandler.md).

**Parameters**

* `opts` {RetryHandler.RetryOptions} (optional) Global retry options applied
  to every request. Individual requests can override via `opts.retryOptions`.
  See [`RetryOptions`](./RetryHandler.md#retryoptions) for the full list of
  accepted fields.

**Returns:** {Dispatcher.DispatcherComposeInterceptor}

**Example**

```js
import { Agent, interceptors } from 'undici'

const agent = new Agent().compose(
  interceptors.retry({
    maxRetries: 5,
    minTimeout: 200,
    maxTimeout: 5000,
    timeoutFactor: 2,
    statusCodes: [429, 502, 503, 504]
  })
)
```

---

## `interceptors.redirect([opts])`

Follows HTTP redirects (3xx responses) automatically.

**Parameters**

* `opts` {Object} (optional)
  * `maxRedirections` {number} Maximum number of redirects to follow. Passing
    `0` disables redirect following entirely. **Default:** `undefined`
    (inherits from the per-request `maxRedirections` option).
  * `throwOnMaxRedirect` {boolean} When `true`, throws an error once the
    redirect limit is reached instead of returning the final redirect response.
    **Default:** `false`.
  * `stripHeadersOnRedirect` {string[]} List of header names to remove from
    the request when following any redirect. **Default:** `[]`.
  * `stripHeadersOnCrossOriginRedirect` {string[]} List of header names to
    remove from the request when following a cross-origin redirect (i.e. the
    redirect target has a different origin). Useful for stripping
    `Authorization` on cross-origin hops. **Default:** `[]`.

Per-request override: any of the four options above can also be set directly
on the dispatch options to override the interceptor defaults for a specific
request.

**Returns:** {Dispatcher.DispatcherComposeInterceptor}

**Example**

```js
import { Agent, interceptors } from 'undici'

const agent = new Agent().compose(
  interceptors.redirect({
    maxRedirections: 10,
    throwOnMaxRedirect: true,
    stripHeadersOnCrossOriginRedirect: ['authorization', 'cookie']
  })
)
```

---

## `interceptors.decompress([opts])`

Automatically decompresses response bodies encoded with `gzip`, `x-gzip`,
`br` (Brotli), `deflate`, `compress`, `x-compress`, or `zstd`.

> **Experimental:** This interceptor is experimental and subject to change.
> A one-time `ExperimentalWarning` is emitted on first use.

**Parameters**

* `opts` {Object} (optional)
  * `skipStatusCodes` {number[]} Status codes for which decompression is
    skipped. **Default:** `[204, 304]`.
  * `skipErrorResponses` {boolean} When `true`, responses with a status code
    >= 400 are not decompressed. **Default:** `true`.

**Returns:** {Dispatcher.DispatcherComposeInterceptor}

**Example**

```js
import { Agent, interceptors } from 'undici'

const agent = new Agent().compose(
  interceptors.decompress({
    skipStatusCodes: [204, 304],
    skipErrorResponses: false // decompress error bodies too
  })
)
```

---

## `interceptors.responseError([opts])`

Converts 4xx/5xx responses into thrown `ResponseError` instances, making it
easy to handle HTTP errors with a standard `try/catch` block.

The error body is automatically decoded for `application/json` and
`text/plain` responses. For JSON responses the body is parsed and exposed as
`error.body`.

**Parameters**

* `opts` {Object} (optional) — currently reserved for future use; may be
  omitted.

**Returns:** {Dispatcher.DispatcherComposeInterceptor}

**Example**

```js
import { Agent, interceptors, errors } from 'undici'

const agent = new Agent().compose(interceptors.responseError())

try {
  await agent.request({ origin: 'https://example.com', path: '/not-found', method: 'GET' })
} catch (err) {
  if (err instanceof errors.ResponseError) {
    console.error(err.status, err.body)
  }
}
```

---

## `interceptors.dns([opts])`

Caches DNS lookups so that repeated requests to the same origin reuse the
resolved IP address instead of performing a fresh lookup every time. Supports
dual-stack (IPv4 + IPv6) and custom lookup/storage implementations.

**Parameters**

* `opts` {Object} (optional)
  * `maxTTL` {number} Maximum number of milliseconds a DNS record is cached,
    regardless of the TTL returned by the resolver. **Default:** `0`
    (use the TTL from the DNS record).
  * `maxItems` {number} Maximum number of origins to cache simultaneously.
    Oldest entries are evicted when the limit is reached. **Default:**
    `Infinity`.
  * `dualStack` {boolean} When `true`, both IPv4 (`A`) and IPv6 (`AAAA`)
    records are looked up and the interceptor picks between them based on
    `affinity`. **Default:** `true`.
  * `affinity` {4 | 6 | null} Preferred IP family when `dualStack` is
    enabled. `null` lets the interceptor alternate between families.
    **Default:** `null`.
  * `lookup` {Function} (optional) Custom DNS resolution function with the
    same signature as `node:dns`'s `lookup` callback form:
    `(origin, options, callback) => void`.
  * `pick` {Function} (optional) Custom record-selection function called with
    `(origin, records, affinity)` to choose which resolved address to use.
  * `storage` {DNSStorage} (optional) Custom storage backend. Must implement
    `get`, `set`, `delete`, `full`, and `size`.

**`DNSStorage` interface**

```ts
interface DNSStorage {
  size: number
  get(origin: string): DNSInterceptorOriginRecords | null
  set(origin: string, records: DNSInterceptorOriginRecords | null, options: { ttl: number }): void
  delete(origin: string): void
  full(): boolean
}
```

**Returns:** {Dispatcher.DispatcherComposeInterceptor}

**Example**

```js
import { Agent, interceptors } from 'undici'

const agent = new Agent().compose(
  interceptors.dns({
    maxTTL: 60_000, // cache for at most 60 seconds
    dualStack: true,
    affinity: 4     // prefer IPv4
  })
)
```

---

## `interceptors.cache([opts])`

Caches HTTP responses according to RFC 9111 (HTTP Caching). See
[`CacheStore`](./CacheStore.md) for information on providing a custom
backing store.

**Parameters**

* `opts` {CacheHandler.CacheOptions} (optional) See the
  [`CacheStore`](./CacheStore.md) documentation for accepted fields.

**Returns:** {Dispatcher.DispatcherComposeInterceptor}

**Example**

```js
import { Agent, interceptors, cacheStores } from 'undici'

const agent = new Agent().compose(
  interceptors.cache({ store: new cacheStores.MemoryCacheStore() })
)
```

---

## `interceptors.deduplicate([opts])`

Deduplicates concurrent identical requests so that only one is sent over the
wire. All waiting callers receive the same response once the in-flight request
completes. Only safe HTTP methods (e.g. `GET`, `HEAD`) may be deduplicated.

**Parameters**

* `opts` {Object} (optional)
  * `methods` {string[]} HTTP methods to deduplicate. Must be safe HTTP
    methods (`GET`, `HEAD`, `OPTIONS`, `TRACE`). **Default:** `['GET']`.
  * `skipHeaderNames` {string[]} Header names whose presence in a request
    causes it to bypass deduplication entirely. Matching is
    case-insensitive. **Default:** `[]`.
  * `excludeHeaderNames` {string[]} Header names to exclude from the
    deduplication key. Requests that differ only in these headers are still
    deduplicated together. Useful for headers like `x-request-id` that vary
    per request but should not prevent deduplication. Matching is
    case-insensitive. **Default:** `[]`.
  * `maxBufferSize` {number} Maximum number of bytes buffered per paused
    waiting handler. If a waiting handler exceeds this threshold it is failed
    with an `AbortError` to prevent unbounded memory growth. **Default:**
    `5_242_880` (5 MiB).

**Returns:** {Dispatcher.DispatcherComposeInterceptor}

**Example**

```js
import { Agent, interceptors } from 'undici'

const agent = new Agent().compose(
  interceptors.deduplicate({
    methods: ['GET', 'HEAD'],
    excludeHeaderNames: ['x-request-id', 'x-trace-id'],
    maxBufferSize: 2 * 1024 * 1024
  })
)
```

---

## Composing multiple interceptors

Interceptors are applied in the order they appear in the `compose()` call.
The first interceptor in the array wraps the outermost layer.

```js
import { Agent, interceptors } from 'undici'

const agent = new Agent().compose([
  interceptors.dns({ maxTTL: 30_000 }),
  interceptors.retry({ maxRetries: 3 }),
  interceptors.redirect({ maxRedirections: 5 }),
  interceptors.decompress(),
  interceptors.responseError()
])
```

In the example above the request flow is:

1. **dns** — resolves and caches the target IP
2. **retry** — retries the dispatch on transient failures
3. **redirect** — follows any 3xx redirects
4. **decompress** — decompresses the response body
5. **responseError** — converts 4xx/5xx into thrown errors
