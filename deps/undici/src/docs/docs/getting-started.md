# Getting Started

<!--type=misc-->

## Installation

```bash
npm install undici
```

## Fetch

The quickest way to get started is with `fetch`, which follows the
[Fetch Standard](https://fetch.spec.whatwg.org/) and works the same way as
the browser API:

```mjs
import { fetch } from 'undici'

const res = await fetch('https://example.com')
const data = await res.json()
console.log(data)
```

### Using the Request object

undici also exports a `Request` class that follows the Fetch Standard:

```mjs
import { fetch, Request } from 'undici'

const req = new Request('https://example.com', {
  method: 'POST',
  headers: { 'content-type': 'application/json' },
  body: JSON.stringify({ hello: 'world' })
})
const res = await fetch(req)
console.log(res.status)
```

### Streaming the response

`res.body` is a web `ReadableStream`. Use `pipeline` from
`node:stream/promises` to stream it to a file:

```mjs
import { fetch } from 'undici'
import { pipeline } from 'node:stream/promises'
import { createWriteStream } from 'node:fs'

const res = await fetch('https://example.com/large-file.zip')
await pipeline(res.body, createWriteStream('./file.zip'))
```

> Always consume or cancel the response body. In Node.js, garbage collection
> is not aggressive enough to release connections promptly, so leaving a body
> unread can cause connection leaks and stalled requests. See
> [Specification Compliance - Garbage Collection](/#garbage-collection)
> for details.

For more on `fetch`, see [API Reference: Fetch](api/Fetch.md).

## Dispatchers: Connection reuse and pooling

By default, `fetch`, `request`, `stream`, and `pipeline` create a new connection
for each call. For applications that make many requests to the same origin,
this is wasteful. undici provides **dispatchers** that manage connections
internally.

### `Agent` — for requests to multiple origins

`Agent` is the most general-purpose dispatcher. It pools connections per-origin
and is the recommended default for most applications. Use it with
`setGlobalDispatcher` to affect all undici calls globally:

```js
import { Agent, setGlobalDispatcher, fetch } from 'undici'

const agent = new Agent({
  keepAliveTimeout: 30_000,
  keepAliveMaxTimeout: 600_000
})
setGlobalDispatcher(agent)

// All subsequent fetch/request/stream/pipeline calls reuse connections
const res = await fetch('https://api.example.com/data')
```

You can also pass a dispatcher per-request:

```js
await fetch('https://api.example.com/data', { dispatcher: agent })
```

### `Pool` — for requests to a single origin

`Pool` manages a fixed set of connections to one origin. It gives you explicit
control over concurrency:

```js
import { Pool, request } from 'undici'

const pool = new Pool('https://api.example.com', { connections: 10 })

const { body } = await request('https://api.example.com/data', {
  dispatcher: pool
})
const data = await body.json()

pool.close()
```

### `Client` — for a single connection

`Client` maps to a single TCP connection. It supports pipelining (sending
multiple requests before responses arrive), which should only be enabled for
trusted remote servers:

```js
import { Client } from 'undici'

const client = new Client('https://api.example.com', {
  pipelining: 5
})

const { body } = await client.request({ path: '/', method: 'GET' })
await body.dump()

client.close()
```

For more on dispatcher options and lifecycle, see:
- [API Reference: Agent](api/Agent.md)
- [API Reference: Pool](api/Pool.md)
- [API Reference: Client](api/Client.md)

## Timeouts

undici applies timeouts at two levels:

- **`headersTimeout`** — time to wait for response headers (default: 300s).
- **`bodyTimeout`** — time between consecutive body chunks (default: 300s).

Set these on the dispatcher or per-request:

```js
import { Agent, setGlobalDispatcher } from 'undici'

const agent = new Agent({
  headersTimeout: 5_000,
  bodyTimeout: 30_000
})

setGlobalDispatcher(agent)
```

Timeout errors are thrown as `HeadersTimeoutError` and `BodyTimeoutError`.
See [API Reference: Errors](api/Errors.md) for the full list.

## Error handling

undici exposes structured errors via `error.code`:

```js
import { request, errors } from 'undici'

try {
  const { body } = await request('https://example.com')
  await body.json()
} catch (err) {
  switch (err.code) {
    case 'UND_ERR_CONNECT_TIMEOUT':
      console.error('Connection timed out')
      break
    case 'UND_ERR_HEADERS_TIMEOUT':
      console.error('Headers timed out')
      break
    case 'UND_ERR_BODY_TIMEOUT':
      console.error('Body timed out')
      break
    case 'UND_ERR_ABORTED':
      console.error('Request was aborted')
      break
    default:
      console.error(err)
  }
}
```

### Aborting requests

```js
import { request } from 'undici'

const ac = new AbortController()

setTimeout(() => ac.abort(), 1000)

try {
  const { body } = await request('https://example.com', {
    signal: ac.signal
  })
  await body.dump()
} catch (err) {
  console.error(err.code) // UND_ERR_ABORTED
}
```

## Common patterns

### Proxies

Use `ProxyAgent` for HTTP(S) proxies, or `EnvHttpProxyAgent` to pick up
proxy settings from environment variables:

```js
import { ProxyAgent, setGlobalDispatcher } from 'undici'

const proxy = new ProxyAgent('http://proxy.internal:8080')
setGlobalDispatcher(proxy)
```

See [Best Practices: Proxy](best-practices/proxy.md) and
[API Reference: ProxyAgent](api/ProxyAgent.md).

### Mocking in tests

```js
import { MockAgent, setGlobalDispatcher, request } from 'undici'

const mockAgent = new MockAgent()
setGlobalDispatcher(mockAgent)

const mockPool = mockAgent.get('https://api.example.com')
mockPool.intercept({ path: '/users' }).reply(200, [{ id: 1 }])

const { body } = await request('https://api.example.com/users')
console.log(await body.json())
```

See [Best Practices: Mocking Request](best-practices/mocking-request.md)
and [API Reference: MockAgent](api/MockAgent.md).

### Testing with undici

For test suites, set short keep-alive timeouts to avoid slow teardowns:

```js
import { Agent, setGlobalDispatcher } from 'undici'

const agent = new Agent({
  keepAliveTimeout: 10,
  keepAliveMaxTimeout: 10
})
setGlobalDispatcher(agent)
```

See [Best Practices: Writing Tests](best-practices/writing-tests.md).

### Customizing the global fetch

You can override Node.js's built-in globals with `install()`:

```js
import { install } from 'undici'

install()

// Global fetch, Headers, Response, Request, and FormData
// now come from undici, not the Node.js bundle
const res = await fetch('https://example.com')
```

See [API Reference: Global Installation](api/GlobalInstallation.md).

## Further reading

- [Undici vs. Built-in Fetch](best-practices/undici-vs-builtin-fetch.md) —
  when to install undici vs using Node.js built-in fetch
- [API Reference](api/Dispatcher.md) — full dispatcher API documentation
- [Examples](/examples/) — runnable code examples
