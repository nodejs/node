# RoundRobinPool

<!--introduced_in=v7.17.0-->
<!--type=module-->
<!-- source_link=lib/dispatcher/round-robin-pool.js -->

> Stability: 2 - Stable

A pool of [`Client`][] instances connected to the same upstream target that
selects clients in a round-robin fashion.

Unlike [`Pool`][], which always reuses the first available client,
`RoundRobinPool` cycles through its clients so that requests are distributed
evenly across every open connection. This is useful when the upstream target is
fronted by a load balancer that distributes TCP connections across multiple
backend servers (for example, a Kubernetes Service): each connection is pinned
to a backend by the load balancer, and `RoundRobinPool` spreads requests across
those connections so every backend receives a comparable share of traffic.

Requests are not guaranteed to be dispatched in the order they were invoked.

```mjs
import { RoundRobinPool } from 'undici'

const pool = new RoundRobinPool('http://localhost:3000', { connections: 10 })
```

`RoundRobinPool` distributes HTTP requests evenly across TCP connections, not
across backend servers directly. Even backend distribution therefore depends on
the load balancer assigning different connections to different backends (for
example, round-robin, random, or least-connections without client affinity). If
the load balancer pins all connections from one source to the same backend (for
example, source-IP affinity or sticky sessions), consider [`BalancedPool`][]
with the individual backend addresses instead.

## Class: `RoundRobinPool`

<!-- YAML
added: v7.17.0
-->

* Extends: {Dispatcher}

### `new RoundRobinPool(url[, options])`

<!-- YAML
added: v7.17.0
-->

* `url` {URL|string} The upstream target. It should only include the
  **protocol, hostname, and port**.
* `options` {RoundRobinPoolOptions} (optional)

#### Parameter: `RoundRobinPoolOptions`

Extends: {ClientOptions}

* `factory` {Function} A function used to create the underlying {Client}
  instances. **Default:** `(origin, opts) => new Client(origin, opts)`.
  * `origin` {URL}
  * `opts` {Object}
  * Returns: {Dispatcher}
* `connections` {number|null} (optional) The maximum number of {Client}
  instances to create. When set to `null`, the `RoundRobinPool` instance creates
  an unlimited number of {Client} instances. **Default:** `null`.
* `clientTtl` {number|null} (optional) The amount of time, in milliseconds,
  before a {Client} instance is removed from the `RoundRobinPool` and closed.
  When set to `null`, {Client} instances are not removed or closed based on age.
  **Default:** `null`.

`RoundRobinPool` inherits all [`Client`][] options. A [`Client`][] instance is
created lazily on the first dispatch and additional instances are created on
demand, up to `connections`, when every existing client is busy.

### `roundRobinPool.closed`

<!-- YAML
added: v7.17.0
-->

* Type: {boolean}

`true` after `roundRobinPool.close()` has been called.

### `roundRobinPool.destroyed`

<!-- YAML
added: v7.17.0
-->

* Type: {boolean}

`true` after `roundRobinPool.destroy()` has been called, or after
`roundRobinPool.close()` has been called and the pool shutdown has completed.

### `roundRobinPool.stats`

<!-- YAML
added: v7.17.0
-->

* Type: {PoolStats}

Aggregate connection statistics for the pool. See [`PoolStats`][].

### `roundRobinPool.close([callback])`

<!-- YAML
added: v7.17.0
-->

Closes the pool and gracefully waits for enqueued requests to complete before
resolving. Implements [`dispatcher.close([callback])`][].

### `roundRobinPool.destroy([error[, callback]])`

<!-- YAML
added: v7.17.0
-->

Destroys the pool abruptly. All pending and running requests are aborted with
the given `error`. Implements [`dispatcher.destroy([error[, callback]])`][].

### `roundRobinPool.connect(options[, callback])`

<!-- YAML
added: v7.17.0
-->

Starts two-way communications with the requested resource using
[HTTP CONNECT][]. See [`dispatcher.connect(options[, callback])`][].

### `roundRobinPool.dispatch(options, handler)`

<!-- YAML
added: v7.17.0
-->

Dispatches a request through the next client selected in round-robin order.
Implements [`dispatcher.dispatch(options, handler)`][].

### `roundRobinPool.pipeline(options, handler)`

<!-- YAML
added: v7.17.0
-->

For easy use with [`stream.pipeline`][]. See
[`dispatcher.pipeline(options, handler)`][].

### `roundRobinPool.request(options[, callback])`

<!-- YAML
added: v7.17.0
-->

Performs an HTTP request. See [`dispatcher.request(options[, callback])`][].

### `roundRobinPool.stream(options, factory[, callback])`

<!-- YAML
added: v7.17.0
-->

A faster version of [`roundRobinPool.request()`][]. See
[`dispatcher.stream(options, factory[, callback])`][].

### `roundRobinPool.upgrade(options[, callback])`

<!-- YAML
added: v7.17.0
-->

Upgrades a connection to a different protocol. See
[`dispatcher.upgrade(options[, callback])`][].

### Event: `'connect'`

<!-- YAML
added: v7.17.0
-->

See [Dispatcher Event: `'connect'`][].

### Event: `'disconnect'`

<!-- YAML
added: v7.17.0
-->

See [Dispatcher Event: `'disconnect'`][].

### Event: `'drain'`

<!-- YAML
added: v7.17.0
-->

See [Dispatcher Event: `'drain'`][].

## Example

```mjs
import { RoundRobinPool } from 'undici'

const pool = new RoundRobinPool('http://localhost:3000', {
  connections: 10
})

// Requests are distributed evenly across all 10 connections.
for (let i = 0; i < 100; i++) {
  const { body } = await pool.request({
    path: '/api/data',
    method: 'GET'
  })
  console.log(await body.json())
}

await pool.close()
```

[HTTP CONNECT]: https://developer.mozilla.org/docs/Web/HTTP/Methods/CONNECT
[`BalancedPool`]: BalancedPool.md#class-balancedpool
[`Client`]: Client.md#class-client
[`Pool`]: Pool.md#class-pool
[`PoolStats`]: PoolStats.md#class-poolstats
[`dispatcher.close([callback])`]: Dispatcher.md#dispatcherclosecallback-promise
[`dispatcher.connect(options[, callback])`]: Dispatcher.md#dispatcherconnectoptions-callback
[`dispatcher.destroy([error[, callback]])`]: Dispatcher.md#dispatcherdestroyerror-callback-promise
[`dispatcher.dispatch(options, handler)`]: Dispatcher.md#dispatcherdispatchoptions-handler
[`dispatcher.pipeline(options, handler)`]: Dispatcher.md#dispatcherpipelineoptions-handler
[`dispatcher.request(options[, callback])`]: Dispatcher.md#dispatcherrequestoptions-callback
[`dispatcher.stream(options, factory[, callback])`]: Dispatcher.md#dispatcherstreamoptions-factory-callback
[`dispatcher.upgrade(options[, callback])`]: Dispatcher.md#dispatcherupgradeoptions-callback
[`roundRobinPool.request()`]: #roundrobinpoolrequestoptions-callback
[`stream.pipeline`]: https://nodejs.org/api/stream.html#streampipelinesource-transforms-destination-callback
[Dispatcher Event: `'connect'`]: Dispatcher.md#event-connect
[Dispatcher Event: `'disconnect'`]: Dispatcher.md#event-disconnect
[Dispatcher Event: `'drain'`]: Dispatcher.md#event-drain
