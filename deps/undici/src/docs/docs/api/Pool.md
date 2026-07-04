# Pool

<!--introduced_in=v1.0.0-->

<!--type=module-->

<!-- source_link=lib/dispatcher/pool.js -->

> Stability: 2 - Stable

A pool of [`Client`][] instances connected to the same upstream origin. A `Pool`
spreads requests across several connections to the same host, which allows more
than one request to be in flight at a time without enabling pipelining on a
single connection.

`Pool` extends [`Dispatcher`][]. Each [`Client`][] in the pool is created lazily
through a configurable `factory` and shares the same options. Requests are not
guaranteed to be dispatched in the order in which they are invoked.

```mjs
import { Pool } from 'undici'

const pool = new Pool('http://localhost:3000', { connections: 10 })
const { statusCode, body } = await pool.request({ path: '/', method: 'GET' })
console.log(statusCode, await body.text())
await pool.close()
```

## Class: `Pool`

<!-- YAML
added: v1.0.0
changes:
  - version: v4.10.4
    pr-url: https://github.com/nodejs/undici/pull/1114
    description: Reworked as part of the balanced pool implementation.
-->

* Extends: {Dispatcher}

### `new Pool(url[, options])`

<!-- YAML
added: v1.0.0
-->

* `url` {URL|string} The upstream origin. It should only include the protocol,
  hostname, and port.
* `options` {PoolOptions} (optional) Extends {ClientOptions}, so every
  [`Client`][] option is also accepted and forwarded to each pooled client.
  * `factory` {Function} Creates the [`Client`][] (or other [`Dispatcher`][])
    instances used by the pool.
    **Default:** `(origin, opts) => new Client(origin, opts)`.
    * `origin` {URL} The parsed upstream origin.
    * `opts` {Object} The resolved client options.
    * Returns: {Dispatcher}
  * `connections` {number|null} The maximum number of [`Client`][] instances the
    pool may create. When set to `null` the pool creates an unlimited number of
    clients. **Default:** `null`.
  * `clientTtl` {number|null} The amount of time, in milliseconds, before an idle
    [`Client`][] is removed from the pool and closed. When set to `null` clients
    are never removed based on age. **Default:** `null`.

Creates a new `Pool` against `url`. If `connections` is provided it must be a
finite, non-negative number, otherwise an [`InvalidArgumentError`][] is thrown.
The `factory` must be a function, and `connect` (inherited from {ClientOptions})
must be a function or an object when provided.

The connection-related {ClientOptions} (`connect`, `connectTimeout`, `tls`,
`maxCachedSessions`, `socketPath`, `autoSelectFamily`,
`autoSelectFamilyAttemptTimeout`, `allowH2`, and `useH2c`) are used to build the
connector shared by every pooled client.

> [!NOTE]
> `Pool` inherits all {ClientOptions}, including `allowH2` and
> `maxConcurrentStreams`. With the default unlimited `connections`, the pool
> opens a new client - and therefore a new TCP/TLS socket - per concurrent
> dispatch, which defeats HTTP/2 multiplexing over a shared session. To benefit
> from h2 multiplexing on a single session, cap `connections` (for example
> `connections: 1`) so that concurrent requests share a session up to
> `maxConcurrentStreams`.

```mjs
import { Pool } from 'undici'

const pool = new Pool('http://localhost:3000', {
  connections: 128,
  pipelining: 1
})
```

### `pool.closed`

<!-- YAML
added: v1.0.0
-->

* Type: {boolean}

`true` after [`pool.close()`][] has been called.

### `pool.destroyed`

<!-- YAML
added: v1.0.0
changes:
  - version: v2.0.0
    pr-url: https://github.com/nodejs/undici/pull/384
    description: Reflects the state set by `pool.close()`.
  - version: v4.10.4
    pr-url: https://github.com/nodejs/undici/pull/1114
    description: Reworked as part of the balanced pool implementation.
-->

* Type: {boolean}

`true` after [`pool.destroy()`][] has been called, or after [`pool.close()`][]
has been called and the pool shutdown has completed.

### `pool.stats`

<!-- YAML
added: v1.0.0
-->

* Type: {PoolStats}

Aggregate statistics for the pool. See [`PoolStats`][].

### `pool.close([callback])`

<!-- YAML
added: v1.0.0
-->

Implements [`dispatcher.close([callback])`][]. Gracefully closes the pool and
all of its clients, waiting for in-flight requests to complete. Resolves once
every pooled client has closed.

### `pool.destroy([error[, callback]])`

<!-- YAML
added: v1.0.0
-->

Implements [`dispatcher.destroy([error[, callback]])`][]. Forcefully destroys
the pool and all of its clients, aborting any in-flight requests with `error`.

### `pool.connect(options[, callback])`

<!-- YAML
added: v1.0.0
-->

See [`dispatcher.connect(options[, callback])`][]. Performs an HTTP `CONNECT`
request through one of the pooled clients.

### `pool.dispatch(options, handler)`

<!-- YAML
added: v1.3.0
changes:
  - version: v4.10.4
    pr-url: https://github.com/nodejs/undici/pull/1114
    description: Reworked as part of the balanced pool implementation.
-->

Implements [`dispatcher.dispatch(options, handler)`][]. Selects an available
client - creating a new one through `factory` when none is free and the
`connections` limit has not been reached - and dispatches the request to it.

### `pool.pipeline(options, handler)`

<!-- YAML
added: v1.0.0
-->

See [`dispatcher.pipeline(options, handler)`][]. Sends a request and returns a
{Duplex} stream that pipes the request body to the response body through
`handler`.

### `pool.request(options[, callback])`

<!-- YAML
added: v1.0.0
-->

See [`dispatcher.request(options[, callback])`][]. Performs a request through one
of the pooled clients.

```mjs
import { Pool } from 'undici'

const pool = new Pool('http://localhost:3000', { connections: 10 })
const { statusCode, headers, body } = await pool.request({
  path: '/',
  method: 'GET'
})
console.log(statusCode, headers)
console.log(await body.text())
await pool.close()
```

### `pool.stream(options, factory[, callback])`

<!-- YAML
added: v1.0.0
-->

See [`dispatcher.stream(options, factory[, callback])`][]. Performs a request and
writes the response body into the {Writable} stream returned by `factory`.

### `pool.upgrade(options[, callback])`

<!-- YAML
added: v1.0.0
-->

See [`dispatcher.upgrade(options[, callback])`][]. Upgrades a connection
(for example to a WebSocket) through one of the pooled clients.

### Event: `'connect'`

<!-- YAML
added: v1.0.0
-->

* `origin` {URL}
* `targets` {Array} The pool followed by the client that connected,
  `[pool, ...targets]`.

Emitted by a pooled client when a socket has connected. See
[`Dispatcher` Event: `'connect'`][]. When `clientTtl` is set, the connecting
client's time-to-live timer starts on this event.

### Event: `'disconnect'`

<!-- YAML
added: v1.0.0
-->

* `origin` {URL}
* `targets` {Array} The pool followed by the client that disconnected,
  `[pool, ...targets]`.
* `error` {Error}

Emitted by a pooled client when a socket has disconnected. See
[`Dispatcher` Event: `'disconnect'`][].

### Event: `'drain'`

<!-- YAML
added: v1.0.0
-->

* `origin` {URL}
* `targets` {Array} The pool followed by the client that drained,
  `[pool, ...targets]`.

Emitted when a pooled client is no longer busy and can accept more requests. See
[`Dispatcher` Event: `'drain'`][].

[`Client`]: Client.md#class-client
[`Dispatcher` Event: `'connect'`]: Dispatcher.md#event-connect
[`Dispatcher` Event: `'disconnect'`]: Dispatcher.md#event-disconnect
[`Dispatcher` Event: `'drain'`]: Dispatcher.md#event-drain
[`Dispatcher`]: Dispatcher.md#class-dispatcher
[`InvalidArgumentError`]: Errors.md#class-invalidargumenterror
[`PoolStats`]: PoolStats.md
[`dispatcher.close([callback])`]: Dispatcher.md#dispatcherclosecallback
[`dispatcher.connect(options[, callback])`]: Dispatcher.md#dispatcherconnectoptions-callback
[`dispatcher.destroy([error[, callback]])`]: Dispatcher.md#dispatcherdestroyerror-callback
[`dispatcher.dispatch(options, handler)`]: Dispatcher.md#dispatcherdispatchoptions-handler
[`dispatcher.pipeline(options, handler)`]: Dispatcher.md#dispatcherpipelineoptions-handler
[`dispatcher.request(options[, callback])`]: Dispatcher.md#dispatcherrequestoptions-callback
[`dispatcher.stream(options, factory[, callback])`]: Dispatcher.md#dispatcherstreamoptions-factory-callback
[`dispatcher.upgrade(options[, callback])`]: Dispatcher.md#dispatcherupgradeoptions-callback
[`pool.close()`]: #poolclosecallback
[`pool.destroy()`]: #pooldestroyerror-callback
