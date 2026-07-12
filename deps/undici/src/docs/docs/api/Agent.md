# Agent

<!--introduced_in=v3.2.0-->
<!--type=module-->
<!-- source_link=lib/dispatcher/agent.js -->

> Stability: 2 - Stable

An `Agent` dispatches requests against multiple different origins, creating and
reusing a per-origin [`Pool`][] (or [`Client`][]) on demand. It is the default
dispatcher used by [`request`][], [`stream`][], and [`fetch`][] when no explicit
dispatcher is supplied.

Requests are not guaranteed to be dispatched in their order of invocation.

```mjs
import { Agent, setGlobalDispatcher } from 'undici'

const agent = new Agent({ connections: 10 })
setGlobalDispatcher(agent)
```

## Class: `Agent`

<!-- YAML
added: v3.2.0
-->

* Extends: {Dispatcher}

`Agent` keeps an internal map of origins to dispatchers. The first request to a
given origin lazily creates a dispatcher through the configured `factory`, and
subsequent requests to the same origin reuse it. Idle dispatchers are closed
automatically once they have no open connections and are no longer busy.

### `new Agent([options])`

<!-- YAML
added: v3.2.0
changes:
  - version: v7.16.0
    pr-url: https://github.com/nodejs/undici/pull/4365
    description: Added the `maxOrigins` option to cap the number of origins.
-->

* `options` {AgentOptions} (optional) Extends {PoolOptions}.
  * `factory` {Function} Builds the dispatcher used for a given origin. When the
    resolved options contain `connections: 1`, the default factory creates a
    {Client}; otherwise it creates a {Pool}. **Default:** `(origin, opts) => new Pool(origin, opts)`.
    * `origin` {URL|string} The origin to create a dispatcher for.
    * `opts` {Object} The resolved options for the new dispatcher.
    * Returns: {Dispatcher}
  * `maxOrigins` {number} Limits the total number of distinct origins that may
    receive requests at the same time. A {MaxOriginsReachedError} is thrown when
    a request targets a new origin once the limit is reached. When set to
    `Infinity`, no limit is enforced. Must be a number greater than `0`.
    **Default:** `Infinity`.

`Agent` inherits all {PoolOptions} (and therefore all {ClientOptions}). The
per-origin {Pool} it creates uses the default unlimited `connections`, so
concurrent requests to the same origin are spread across separate {Client}
instances on separate sockets.

> [!NOTE]
> Because each concurrent request to an origin may use a different {Client},
> HTTP/2 multiplexing on a shared session does not apply unless `connections` is
> set to a small value (for example `connections: 1`). See {PoolOptions} and
> {ClientOptions} for the full set of inherited options such as `allowH2`
> (default `true`) and `maxConcurrentStreams` (default `100`).

### `agent.closed`

<!-- YAML
added: v3.2.0
-->

* Type: {boolean}

`true` after [`agent.close()`][] has been called.

### `agent.destroyed`

<!-- YAML
added: v3.2.0
-->

* Type: {boolean}

`true` after [`agent.destroy()`][] has been called, or after [`agent.close()`][]
has been called and the shutdown has completed.

### `agent.stats`

<!-- YAML
added: v7.9.0
-->

* Type: {Record<string, ClientStats|PoolStats>}

Aggregate statistics for the agent, keyed by origin. Each value is the
[`ClientStats`][] or [`PoolStats`][] object reported by the dispatcher currently
serving that origin. Origins whose dispatcher does not expose `stats` are
omitted.

```mjs
import { Agent } from 'undici'

const agent = new Agent()
await agent.request({ origin: 'http://localhost:3000', path: '/', method: 'GET' })
console.log(agent.stats)
// { 'http://localhost:3000': PoolStats { ... } }
```

### `agent.dispatch(options, handler)`

<!-- YAML
added: v3.2.0
-->

* `options` {AgentDispatchOptions} Extends {DispatchOptions}.
  * `origin` {string|URL} The origin to dispatch the request against. Required.
* `handler` {DispatchHandler}
* Returns: {boolean} `false` if the dispatcher is busy and the caller should
  wait for the [`'drain'`][] event before dispatching again.

Routes the request to the dispatcher for `options.origin`, creating one through
the `factory` if needed. Throws an {InvalidArgumentError} when `options.origin`
is not a non-empty string or {URL}, and a {MaxOriginsReachedError} when a new
origin would exceed `maxOrigins`. Implements [`Dispatcher.dispatch()`][].

### `agent.close([callback])`

<!-- YAML
added: v3.2.0
-->

* `callback` {Function} (optional) Invoked once every per-origin dispatcher has
  closed. When omitted, a {Promise} is returned.
* Returns: {Promise|void}

Gracefully closes the agent and every dispatcher it owns, waiting for in-flight
requests to complete. Implements [`Dispatcher.close()`][].

### `agent.destroy([error[, callback]])`

<!-- YAML
added: v3.2.0
-->

* `error` {Error} (optional) The error to abort pending requests with.
* `callback` {Function} (optional) Invoked once every per-origin dispatcher has
  been destroyed. When omitted, a {Promise} is returned.
* Returns: {Promise|void}

Forcefully destroys the agent and every dispatcher it owns, aborting all pending
requests. Implements [`Dispatcher.destroy()`][].

### `agent.connect(options[, callback])`

<!-- YAML
added: v3.2.0
-->

See [`Dispatcher.connect()`][]. The request is routed to the dispatcher for
`options.origin`.

### `agent.pipeline(options, handler)`

<!-- YAML
added: v3.2.0
-->

See [`Dispatcher.pipeline()`][]. The request is routed to the dispatcher for
`options.origin`.

### `agent.request(options[, callback])`

<!-- YAML
added: v3.2.0
-->

See [`Dispatcher.request()`][]. The request is routed to the dispatcher for
`options.origin`.

```mjs
import { Agent } from 'undici'

const agent = new Agent()
const { statusCode, body } = await agent.request({
  origin: 'http://localhost:3000',
  path: '/',
  method: 'GET',
})

console.log('response received', statusCode)
console.log('data', await body.text())
```

### `agent.stream(options, factory[, callback])`

<!-- YAML
added: v3.2.0
-->

See [`Dispatcher.stream()`][]. The request is routed to the dispatcher for
`options.origin`.

### `agent.upgrade(options[, callback])`

<!-- YAML
added: v3.2.0
-->

See [`Dispatcher.upgrade()`][]. The request is routed to the dispatcher for
`options.origin`.

### Event: `'connect'`

<!-- YAML
added: v6.6.0
-->

* `origin` {URL}
* `targets` {Array<Dispatcher>} The dispatchers involved, starting with this
  `Agent` followed by the per-origin dispatcher chain.

Emitted when a socket has been created for an origin and is ready to receive
requests. Forwarded from the underlying dispatcher's [`'connect'`][] event.

### Event: `'disconnect'`

<!-- YAML
added: v6.6.0
-->

* `origin` {URL}
* `targets` {Array<Dispatcher>} The dispatchers involved, starting with this
  `Agent`.
* `error` {Error}

Emitted when a socket for an origin has disconnected. Forwarded from the
underlying dispatcher's [`'disconnect'`][] event.

### Event: `'connectionError'`

<!-- YAML
added: v6.6.0
-->

* `origin` {URL}
* `targets` {Array<Dispatcher>} The dispatchers involved, starting with this
  `Agent`.
* `error` {Error}

Emitted when a connection attempt for an origin fails. Forwarded from the
underlying dispatcher's [`'connectionError'`][] event.

### Event: `'drain'`

<!-- YAML
added: v6.6.0
-->

* `origin` {URL}
* `targets` {Array<Dispatcher>} The dispatchers involved, starting with this
  `Agent`.

Emitted when an origin's dispatcher is no longer busy and can accept more
requests. Forwarded from the underlying dispatcher's [`'drain'`][] event.

[`'connect'`]: Dispatcher.md#event-connect
[`'connectionError'`]: Dispatcher.md#event-connectionerror
[`'disconnect'`]: Dispatcher.md#event-disconnect
[`'drain'`]: Dispatcher.md#event-drain
[`Client`]: Client.md#class-client
[`ClientStats`]: ClientStats.md
[`Dispatcher.close()`]: Dispatcher.md#dispatcherclosecallback-promise
[`Dispatcher.connect()`]: Dispatcher.md#dispatcherconnectoptions-callback
[`Dispatcher.destroy()`]: Dispatcher.md#dispatcherdestroyerror-callback-promise
[`Dispatcher.dispatch()`]: Dispatcher.md#dispatcherdispatchoptions-handler
[`Dispatcher.pipeline()`]: Dispatcher.md#dispatcherpipelineoptions-handler
[`Dispatcher.request()`]: Dispatcher.md#dispatcherrequestoptions-callback
[`Dispatcher.stream()`]: Dispatcher.md#dispatcherstreamoptions-factory-callback
[`Dispatcher.upgrade()`]: Dispatcher.md#dispatcherupgradeoptions-callback
[`Pool`]: Pool.md#class-pool
[`PoolStats`]: PoolStats.md
[`agent.close()`]: #agentclosecallback
[`agent.destroy()`]: #agentdestroyerror-callback
[`fetch`]: Fetch.md
[`request`]: Dispatcher.md#dispatcherrequestoptions-callback
[`stream`]: Dispatcher.md#dispatcherstreamoptions-factory-callback
