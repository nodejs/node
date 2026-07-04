# BalancedPool

<!--introduced_in=v4.8.0-->
<!--type=module-->
<!-- source_link=lib/dispatcher/balanced-pool.js -->

> Stability: 2 - Stable

A `BalancedPool` distributes requests across a set of {Pool} instances, each
connected to a different upstream. It uses a weighted round-robin algorithm:
every upstream starts with the same weight, and the weight is adjusted up on
successful connections and down on connection errors, so that healthy upstreams
receive a larger share of traffic.

Requests are not guaranteed to be dispatched in the order they are issued.

```mjs
import { BalancedPool } from 'undici'

const pool = new BalancedPool([
  'http://localhost:3000',
  'http://localhost:3001',
  'http://localhost:3002',
])

const { statusCode, body } = await pool.request({
  path: '/',
  method: 'GET',
})
console.log('response received', statusCode)
for await (const chunk of body) {
  console.log('data', chunk.toString())
}
```

## Class: `BalancedPool`

<!-- YAML
added: v4.8.0
changes:
  - version: v4.10.4
    pr-url: https://github.com/nodejs/undici/pull/1114
    description: Introduced the `BalancedPool` class.
-->

* Extends: {Dispatcher}

A pool of {Pool} instances connected to multiple upstreams.

### `new BalancedPool(upstreams[, options])`

<!-- YAML
added: v4.8.0
changes:
  - version: v4.15.0
    pr-url: https://github.com/nodejs/undici/pull/1237
    description: Added the `factory` option.
-->

* `upstreams` {URL|string|URL[]|string[]} One or more upstream origins. Each
  value should only include the **protocol, hostname, and port**. **Default:**
  `[]`.
* `options` {BalancedPoolOptions} (optional)

The options passed to the constructor are forwarded to every {Pool} instance
that is created for an upstream.

#### Parameter: `BalancedPoolOptions`

Extends: {PoolOptions}

In addition to all {PoolOptions}, the following options are supported:

* `factory` {Function} A function used to create the {Pool} instance for each
  upstream. **Default:** `(origin, opts) => new Pool(origin, opts)`.
  * `origin` {URL|string} The upstream origin.
  * `opts` {Object} The options object derived from the constructor options.
  * Returns: {Dispatcher}
* `maxWeightPerServer` {number} The maximum weight any single upstream can
  reach. Successful connections increase an upstream's weight up to this value.
  **Default:** `100`.
* `errorPenalty` {number} The amount by which an upstream's weight is decreased
  on a connection error and increased on a successful connection. **Default:**
  `15`.

### `balancedPool.addUpstream(upstream)`

<!-- YAML
added: v4.8.0
-->

* `upstream` {URL|string} The upstream origin to add. It should only include the
  **protocol, hostname, and port**.
* Returns: {BalancedPool} The `BalancedPool` instance, for chaining.

Adds an upstream. If an open, non-destroyed upstream with the same origin is
already present, the call is a no-op.

### `balancedPool.removeUpstream(upstream)`

<!-- YAML
added: v4.8.0
-->

* `upstream` {URL|string} The upstream origin to remove. It should only include
  the **protocol, hostname, and port**.
* Returns: {BalancedPool} The `BalancedPool` instance, for chaining.

Removes an upstream that was previously added. If no matching upstream is found,
the call is a no-op.

### `balancedPool.getUpstream(upstream)`

<!-- YAML
added: v7.17.0
-->

* `upstream` {URL|string} The upstream origin to look up. It should only include
  the **protocol, hostname, and port**.
* Returns: {Pool|undefined} The {Pool} instance managing the given upstream, or
  `undefined` if it is not present.

Returns the {Pool} instance for the given upstream origin, if it is open and not
destroyed.

### `balancedPool.upstreams`

<!-- YAML
added: v4.8.0
-->

* Type: {string[]} The origins of the upstreams that are currently open and not
  destroyed.

### `balancedPool.closed`

<!-- YAML
added: v4.8.0
changes:
  - version: v4.10.4
    pr-url: https://github.com/nodejs/undici/pull/1114
    description: Introduced the `BalancedPool` class.
-->

* Type: {boolean} `true` after `balancedPool.close()` has been called.

Implements [`client.closed`][].

### `balancedPool.destroyed`

<!-- YAML
added: v4.8.0
changes:
  - version: v4.10.4
    pr-url: https://github.com/nodejs/undici/pull/1114
    description: Introduced the `BalancedPool` class.
-->

* Type: {boolean} `true` after `balancedPool.destroy()` has been called, or after
  `balancedPool.close()` has been called and the shutdown has completed.

Implements [`client.destroyed`][].

### `balancedPool.stats`

<!-- YAML
added: v4.8.0
-->

* Type: {PoolStats}

Returns a {PoolStats} instance aggregating the statistics of the underlying
pools.

### `balancedPool.close([callback])`

<!-- YAML
added: v4.8.0
-->

Implements [`Dispatcher.close([callback])`][].

### `balancedPool.destroy([error[, callback]])`

<!-- YAML
added: v4.8.0
-->

Implements [`Dispatcher.destroy([error, callback])`][].

### `balancedPool.connect(options[, callback])`

<!-- YAML
added: v4.8.0
-->

See [`Dispatcher.connect(options[, callback])`][].

### `balancedPool.dispatch(options, handlers)`

<!-- YAML
added: v4.8.0
changes:
  - version: v4.10.4
    pr-url: https://github.com/nodejs/undici/pull/1114
    description: Introduced the `BalancedPool` class.
-->

Implements [`Dispatcher.dispatch(options, handler)`][].

### `balancedPool.pipeline(options, handler)`

<!-- YAML
added: v4.8.0
-->

See [`Dispatcher.pipeline(options, handler)`][].

### `balancedPool.request(options[, callback])`

<!-- YAML
added: v4.8.0
-->

See [`Dispatcher.request(options[, callback])`][].

### `balancedPool.stream(options, factory[, callback])`

<!-- YAML
added: v4.8.0
-->

See [`Dispatcher.stream(options, factory[, callback])`][].

### `balancedPool.upgrade(options[, callback])`

<!-- YAML
added: v4.8.0
-->

See [`Dispatcher.upgrade(options[, callback])`][].

### Event: `'connect'`

<!-- YAML
added: v4.8.0
changes:
  - version: v4.10.4
    pr-url: https://github.com/nodejs/undici/pull/1114
    description: Introduced the `BalancedPool` class.
  - version: v5.8.0
    pr-url: https://github.com/nodejs/undici/pull/1069
    description: Adopted weighted round-robin selection across upstreams.
-->

See [Dispatcher Event: `'connect'`][].

### Event: `'disconnect'`

<!-- YAML
added: v4.8.0
changes:
  - version: v4.10.4
    pr-url: https://github.com/nodejs/undici/pull/1114
    description: Introduced the `BalancedPool` class.
  - version: v5.8.0
    pr-url: https://github.com/nodejs/undici/pull/1069
    description: Adopted weighted round-robin selection across upstreams.
-->

See [Dispatcher Event: `'disconnect'`][].

### Event: `'drain'`

<!-- YAML
added: v4.8.0
changes:
  - version: v4.10.4
    pr-url: https://github.com/nodejs/undici/pull/1114
    description: Introduced the `BalancedPool` class.
-->

See [Dispatcher Event: `'drain'`][].

[Dispatcher Event: `'connect'`]: Dispatcher.md#event-connect
[Dispatcher Event: `'disconnect'`]: Dispatcher.md#event-disconnect
[Dispatcher Event: `'drain'`]: Dispatcher.md#event-drain
[`Dispatcher.close([callback])`]: Dispatcher.md#dispatcherclosecallback-promise
[`Dispatcher.connect(options[, callback])`]: Dispatcher.md#dispatcherconnectoptions-callback
[`Dispatcher.destroy([error, callback])`]: Dispatcher.md#dispatcherdestroyerror-callback-promise
[`Dispatcher.dispatch(options, handler)`]: Dispatcher.md#dispatcherdispatchoptions-handler
[`Dispatcher.pipeline(options, handler)`]: Dispatcher.md#dispatcherpipelineoptions-handler
[`Dispatcher.request(options[, callback])`]: Dispatcher.md#dispatcherrequestoptions-callback
[`Dispatcher.stream(options, factory[, callback])`]: Dispatcher.md#dispatcherstreamoptions-factory-callback
[`Dispatcher.upgrade(options[, callback])`]: Dispatcher.md#dispatcherupgradeoptions-callback
[`client.closed`]: Client.md#clientclosed
[`client.destroyed`]: Client.md#clientdestroyed
