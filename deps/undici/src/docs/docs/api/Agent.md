# Agent

Extends: `undici.Dispatcher`

Agent allows dispatching requests against multiple different origins.

Requests are not guaranteed to be dispatched in order of invocation.

## `new undici.Agent([options])`

Arguments:

* **options** `AgentOptions` (optional)

Returns: `Agent`

### Parameter: `AgentOptions`

Extends: [`PoolOptions`](/docs/docs/api/Pool.md#parameter-pooloptions)

* **factory** `(origin: URL, opts: Object) => Dispatcher` - Default: `(origin, opts) => new Pool(origin, opts)`
* **maxOrigins** `number` (optional) - Default: `Infinity` - Limits the total number of origins that can receive requests at a time, throwing an `MaxOriginsReachedError` error when attempting to dispatch when the max is reached. If `Infinity`, no limit is enforced.

> [!NOTE]
> Like `Pool`, `Agent` inherits all [`ClientOptions`](/docs/docs/api/Client.md#parameter-clientoptions). `allowH2` defaults to `true` and `maxConcurrentStreams` to `100`. The per-origin `Pool` it creates uses the default unlimited `connections`, so concurrent requests to the same origin land on separate `Client` instances and separate TCP/TLS sockets — HTTP/2 multiplexing on a shared session does not apply unless `connections` is set to a small value. See [`PoolOptions`](/docs/docs/api/Pool.md#parameter-pooloptions).

## Instance Properties

### `Agent.closed`

Implements [Client.closed](/docs/docs/api/Client.md#clientclosed)

### `Agent.destroyed`

Implements [Client.destroyed](/docs/docs/api/Client.md#clientdestroyed)

## Instance Methods

### `Agent.close([callback])`

Implements [`Dispatcher.close([callback])`](/docs/docs/api/Dispatcher.md#dispatcherclosecallback-promise).

### `Agent.destroy([error, callback])`

Implements [`Dispatcher.destroy([error, callback])`](/docs/docs/api/Dispatcher.md#dispatcherdestroyerror-callback-promise).

### `Agent.dispatch(options, handler: AgentDispatchOptions)`

Implements [`Dispatcher.dispatch(options, handler)`](/docs/docs/api/Dispatcher.md#dispatcherdispatchoptions-handler).

#### Parameter: `AgentDispatchOptions`

Extends: [`DispatchOptions`](/docs/docs/api/Dispatcher.md#parameter-dispatchoptions)

* **origin** `string | URL`

Implements [`Dispatcher.destroy([error, callback])`](/docs/docs/api/Dispatcher.md#dispatcherdestroyerror-callback-promise).

### `Agent.connect(options[, callback])`

See [`Dispatcher.connect(options[, callback])`](/docs/docs/api/Dispatcher.md#dispatcherconnectoptions-callback).

### `Agent.dispatch(options, handler)`

Implements [`Dispatcher.dispatch(options, handler)`](/docs/docs/api/Dispatcher.md#dispatcherdispatchoptions-handler).

### `Agent.pipeline(options, handler)`

See [`Dispatcher.pipeline(options, handler)`](/docs/docs/api/Dispatcher.md#dispatcherpipelineoptions-handler).

### `Agent.request(options[, callback])`

See [`Dispatcher.request(options [, callback])`](/docs/docs/api/Dispatcher.md#dispatcherrequestoptions-callback).

### `Agent.stream(options, factory[, callback])`

See [`Dispatcher.stream(options, factory[, callback])`](/docs/docs/api/Dispatcher.md#dispatcherstreamoptions-factory-callback).

### `Agent.upgrade(options[, callback])`

See [`Dispatcher.upgrade(options[, callback])`](/docs/docs/api/Dispatcher.md#dispatcherupgradeoptions-callback).

### `Agent.stats()`

Returns an object of stats by origin in the format of `Record<string, TClientStats | TPoolStats>`

See [`PoolStats`](/docs/docs/api/PoolStats.md) and [`ClientStats`](/docs/docs/api/ClientStats.md).
