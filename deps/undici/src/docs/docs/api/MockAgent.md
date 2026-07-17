# MockAgent

<!--introduced_in=v4.0.0-->
<!--type=module-->
<!-- source_link=lib/mock/mock-agent.js -->

> Stability: 2 - Stable

A `MockAgent` is a {Dispatcher} that intercepts HTTP requests made through
undici and replies with programmed mock responses instead of contacting the
network. It is useful for testing code that performs HTTP requests without
relying on a live server.

A `MockAgent` does not intercept requests on its own. Mock responses are
registered on the {MockClient} or {MockPool} instances returned by
[`mockAgent.get(origin)`][], and requests are routed through them once the
`MockAgent` is set as the dispatcher (for example through
[`setGlobalDispatcher()`][] or a per-request `dispatcher` option).

```mjs
import { MockAgent } from 'undici'

const mockAgent = new MockAgent()
```

## Class: `MockAgent`

<!-- YAML
added: v4.0.0
-->

* Extends: {Dispatcher}

Intercepts HTTP requests made through undici and returns mocked responses.

### `new MockAgent([options])`

<!-- YAML
added: v4.0.0
changes:
  - version: v7.16.0
    pr-url: https://github.com/nodejs/undici/pull/4479
    description: Mock options are always normalized to an object internally.
-->

* `options` {MockAgentOptions} (optional)
* Returns: {MockAgent}

When instantiated, a `MockAgent` is automatically activated. It does not
intercept any request until mock interceptors are registered on the dispatchers
returned by [`mockAgent.get(origin)`][].

#### Parameter: `MockAgentOptions`

* Extends: {AgentOptions}
* `agent` {Dispatcher} (optional) A custom agent to be encapsulated by the
  `MockAgent`. It must implement the `Agent` API (that is, expose a `dispatch`
  function). **Default:** a new {Agent} constructed from `options`.
* `ignoreTrailingSlash` {boolean} (optional) Whether trailing slashes in the
  request path are ignored when matching interceptors. **Default:** `false`.
* `acceptNonStandardSearchParameters` {boolean} (optional) Whether the matcher
  also accepts URLs using non-standard search-parameter syntaxes, such as
  multi-value items written with `[]` (for example `param[]=1&param[]=2`) or
  comma-separated values (for example `param=1,2,3`). **Default:** `false`.
* `enableCallHistory` {boolean} (optional) Whether call history recording is
  enabled. See [`mockAgent.getCallHistory()`][]. **Default:** `false`.

```mjs displayName="Basic instantiation"
import { MockAgent } from 'undici'

const mockAgent = new MockAgent()
```

```mjs displayName="Instantiation with a custom agent"
import { Agent, MockAgent } from 'undici'

const agent = new Agent()

const mockAgent = new MockAgent({ agent })
```

### `mockAgent.get(origin)`

<!-- YAML
added: v4.0.0
-->

* `origin` {string|RegExp|Function} A matcher for the origin to retrieve. The
  function form has the signature `(origin) => boolean`.
* Returns: {MockClient|MockPool}

Creates and retrieves the mock dispatcher used to intercept requests for the
given origin. When the `connections` option of the `MockAgent` is `1`, a
{MockClient} is returned; otherwise a {MockPool} is returned. Subsequent calls
with the same origin return the same instance.

The way `origin` is matched against incoming requests depends on its type:

| Matcher type | Condition to pass            |
| :----------: | ---------------------------- |
| `string`     | Exact match against the value |
| `RegExp`     | The regular expression matches |
| `Function`   | The function returns `true`  |

```mjs displayName="Basic mocked request"
import { MockAgent, setGlobalDispatcher, request } from 'undici'

const mockAgent = new MockAgent()
setGlobalDispatcher(mockAgent)

const mockPool = mockAgent.get('http://localhost:3000')
mockPool.intercept({ path: '/foo' }).reply(200, 'foo')

const { statusCode, body } = await request('http://localhost:3000/foo')

console.log('response received', statusCode) // response received 200

for await (const data of body) {
  console.log('data', data.toString('utf8')) // data foo
}
```

```mjs displayName="Mocked request using a local dispatcher"
import { MockAgent, request } from 'undici'

const mockAgent = new MockAgent()

const mockPool = mockAgent.get('http://localhost:3000')
mockPool.intercept({ path: '/foo' }).reply(200, 'foo')

const { statusCode, body } = await request('http://localhost:3000/foo', {
  dispatcher: mockAgent
})

console.log('response received', statusCode) // response received 200

for await (const data of body) {
  console.log('data', data.toString('utf8')) // data foo
}
```

```mjs displayName="Returning a MockClient"
import { MockAgent, request } from 'undici'

const mockAgent = new MockAgent({ connections: 1 })

const mockClient = mockAgent.get('http://localhost:3000')
mockClient.intercept({ path: '/foo' }).reply(200, 'foo')

const { statusCode, body } = await request('http://localhost:3000/foo', {
  dispatcher: mockClient
})

console.log('response received', statusCode) // response received 200

for await (const data of body) {
  console.log('data', data.toString('utf8')) // data foo
}
```

```mjs displayName="Matching the origin with a regular expression"
import { MockAgent, setGlobalDispatcher, request } from 'undici'

const mockAgent = new MockAgent()
setGlobalDispatcher(mockAgent)

const mockPool = mockAgent.get(new RegExp('http://localhost:3000'))
mockPool.intercept({ path: '/foo' }).reply(200, 'foo')

const { statusCode } = await request('http://localhost:3000/foo')

console.log('response received', statusCode) // response received 200
```

```mjs displayName="Matching the origin with a function"
import { MockAgent, setGlobalDispatcher, request } from 'undici'

const mockAgent = new MockAgent()
setGlobalDispatcher(mockAgent)

const mockPool = mockAgent.get((origin) => origin === 'http://localhost:3000')
mockPool.intercept({ path: '/foo' }).reply(200, 'foo')

const { statusCode } = await request('http://localhost:3000/foo')

console.log('response received', statusCode) // response received 200
```

### `mockAgent.dispatch(options, handler)`

<!-- YAML
added: v4.0.0
-->

* `options` {AgentDispatchOptions}
* `handler` {DispatchHandler}
* Returns: {boolean}

Dispatches a mocked request. This implements
[`Dispatcher.dispatch()`][] for the encapsulated agent: it ensures the mock
dispatcher for `options.origin` exists, records the call in the call history
when enabled, and forwards the request to the underlying agent. It is normally
invoked indirectly through higher-level methods such as
[`mockAgent.request()`][].

```mjs displayName="Using mockAgent.request()"
import { MockAgent } from 'undici'

const mockAgent = new MockAgent()

const mockPool = mockAgent.get('http://localhost:3000')
mockPool.intercept({ path: '/foo' }).reply(200, 'foo')

const { statusCode, body } = await mockAgent.request({
  origin: 'http://localhost:3000',
  path: '/foo',
  method: 'GET'
})

console.log('response received', statusCode) // response received 200

for await (const data of body) {
  console.log('data', data.toString('utf8')) // data foo
}
```

### `mockAgent.close()`

<!-- YAML
added: v4.0.0
-->

* Returns: {Promise<void>} Resolves once the agent and its registered mock pools
  and clients have closed.

Clears the call history, closes the encapsulated agent, and waits for all
registered mock pools and clients to close.

```mjs displayName="Clean up after tests"
import { MockAgent, setGlobalDispatcher } from 'undici'

const mockAgent = new MockAgent()
setGlobalDispatcher(mockAgent)

await mockAgent.close()
```

### `mockAgent.deactivate()`

<!-- YAML
added: v4.0.0
-->

* Returns: {undefined}

Disables mocking on the `MockAgent`. While deactivated, requests are no longer
intercepted.

```mjs displayName="Deactivate mocking"
import { MockAgent, setGlobalDispatcher } from 'undici'

const mockAgent = new MockAgent()
setGlobalDispatcher(mockAgent)

mockAgent.deactivate()
```

### `mockAgent.activate()`

<!-- YAML
added: v4.0.0
-->

* Returns: {undefined}

Enables mocking on the `MockAgent`. A `MockAgent` is activated automatically
when instantiated, so this method is only required after
[`mockAgent.deactivate()`][] has been called.

```mjs displayName="Re-activate mocking"
import { MockAgent, setGlobalDispatcher } from 'undici'

const mockAgent = new MockAgent()
setGlobalDispatcher(mockAgent)

mockAgent.deactivate()
// No mocking will occur

// Later
mockAgent.activate()
```

### `mockAgent.enableNetConnect([matcher])`

<!-- YAML
added: v4.0.0
-->

* `matcher` {string|RegExp|Function} (optional) A host matcher. When a string is
  used it should only contain the hostname and, optionally, the port. The
  function form has the signature `(host) => boolean`. When omitted, all
  non-matching requests are allowed to perform a real request.
* Returns: {undefined}

Defines host matchers so that requests that are not intercepted by a mock
dispatcher are allowed to perform a real HTTP request. Calling this method
multiple times with a string appends each value to the list of allowed hosts.

```mjs displayName="Allow all non-matching requests"
import { MockAgent, setGlobalDispatcher, request } from 'undici'

const mockAgent = new MockAgent()
setGlobalDispatcher(mockAgent)

mockAgent.enableNetConnect()

await request('http://example.com')
// A real request is made
```

```mjs displayName="Allow requests matching specific hosts"
import { MockAgent, setGlobalDispatcher, request } from 'undici'

const mockAgent = new MockAgent()
setGlobalDispatcher(mockAgent)

mockAgent.enableNetConnect('example-1.com')
mockAgent.enableNetConnect('example-2.com:8080')

await request('http://example-1.com')
// A real request is made

await request('http://example-2.com:8080')
// A real request is made

await request('http://example-3.com')
// Will throw
```

```mjs displayName="Allow requests matching a regular expression or function"
import { MockAgent, setGlobalDispatcher, request } from 'undici'

const mockAgent = new MockAgent()
setGlobalDispatcher(mockAgent)

mockAgent.enableNetConnect(new RegExp('example.com'))
mockAgent.enableNetConnect((host) => host === 'example.org')

await request('http://example.com')
// A real request is made
```

### `mockAgent.disableNetConnect()`

<!-- YAML
added: v4.0.0
-->

* Returns: {undefined}

Causes every request that is not matched by a mock interceptor to throw,
disallowing real HTTP requests.

```mjs displayName="Throw on every non-matching request"
import { MockAgent, request } from 'undici'

const mockAgent = new MockAgent()

mockAgent.disableNetConnect()

await request('http://example.com')
// Will throw
```

### `mockAgent.enableCallHistory()`

<!-- YAML
added: v7.5.0
-->

* Returns: {MockAgent} The same `MockAgent` instance, for chaining.

Enables call history recording. Once enabled, subsequent calls are registered
and can be retrieved through [`mockAgent.getCallHistory()`][]. Call history can
also be enabled at construction time with the `enableCallHistory` option.

```mjs
import { MockAgent } from 'undici'

const mockAgent = new MockAgent()

mockAgent.enableCallHistory()
```

### `mockAgent.disableCallHistory()`

<!-- YAML
added: v7.5.0
-->

* Returns: {MockAgent} The same `MockAgent` instance, for chaining.

Disables call history recording. Subsequent calls are no longer registered.

```mjs
import { MockAgent } from 'undici'

const mockAgent = new MockAgent({ enableCallHistory: true })

mockAgent.disableCallHistory()
```

### `mockAgent.getCallHistory()`

<!-- YAML
added: v7.5.0
-->

* Returns: {MockCallHistory|undefined} The {MockCallHistory} instance, or
  `undefined` when call history is not enabled.

Returns the call history instance, which records every request made through the
`MockAgent` (whether intercepted or not). Call history is not enabled by
default; enable it with the `enableCallHistory` option or
[`mockAgent.enableCallHistory()`][].

```mjs displayName="Inspect recorded calls"
import { MockAgent, setGlobalDispatcher, request } from 'undici'

const mockAgent = new MockAgent({ enableCallHistory: true })
setGlobalDispatcher(mockAgent)

await request('http://example.com', { query: { item: 1 } })

mockAgent.getCallHistory()?.firstCall()
// Returns
// MockCallHistoryLog {
//   body: undefined,
//   headers: undefined,
//   method: 'GET',
//   origin: 'http://example.com',
//   fullUrl: 'http://example.com/?item=1',
//   path: '/',
//   searchParams: { item: '1' },
//   protocol: 'http:',
//   host: 'example.com',
//   port: ''
// }
```

### `mockAgent.clearCallHistory()`

<!-- YAML
added: v7.5.0
-->

* Returns: {undefined}

Clears the call history, deleting every recorded {MockCallHistoryLog} on the
{MockCallHistory} instance. It is a no-op when call history has never been
enabled.

```mjs
import { MockAgent } from 'undici'

const mockAgent = new MockAgent({ enableCallHistory: true })

mockAgent.clearCallHistory()
```

### `mockAgent.pendingInterceptors()`

<!-- YAML
added: v5.1.0
-->

* Returns: {PendingInterceptor[]} A `PendingInterceptor` is a `MockDispatch`
  with an additional `origin` {string} property.

Returns the interceptors registered on the `MockAgent` that are still pending.
An interceptor is pending when it meets one of the following criteria:

* It is registered with neither `.times(<number>)` nor `.persist()` and has not
  been invoked.
* It is persistent (registered with `.persist()`) and has not been invoked.
* It is registered with `.times(<number>)` and has not been invoked `<number>`
  times.

```mjs displayName="List pending interceptors"
import { MockAgent } from 'undici'

const mockAgent = new MockAgent()
mockAgent.disableNetConnect()

mockAgent
  .get('https://example.com')
  .intercept({ method: 'GET', path: '/' })
  .reply(200)

const pendingInterceptors = mockAgent.pendingInterceptors()
// Returns [
//   {
//     timesInvoked: 0,
//     times: 1,
//     persist: false,
//     consumed: false,
//     pending: true,
//     path: '/',
//     method: 'GET',
//     body: undefined,
//     headers: undefined,
//     data: {
//       error: null,
//       statusCode: 200,
//       data: '',
//       headers: {},
//       trailers: {}
//     },
//     origin: 'https://example.com'
//   }
// ]
```

### `mockAgent.assertNoPendingInterceptors([options])`

<!-- YAML
added: v5.1.0
-->

* `options` {Object} (optional)
  * `pendingInterceptorsFormatter` {Object} An object exposing a
    `format(pendingInterceptors)` method used to render the pending interceptors
    in the thrown error message. **Default:** a built-in formatter that prints a
    table.
* Returns: {undefined}

Throws an {UndiciError} when the `MockAgent` has any pending interceptors. The
criteria for an interceptor being pending are the same as for
[`mockAgent.pendingInterceptors()`][].

```mjs displayName="Assert there are no pending interceptors"
import { MockAgent } from 'undici'

const mockAgent = new MockAgent()
mockAgent.disableNetConnect()

mockAgent
  .get('https://example.com')
  .intercept({ method: 'GET', path: '/' })
  .reply(200)

mockAgent.assertNoPendingInterceptors()
// Throws an UndiciError with the following message:
//
// 1 interceptor is pending:
//
// ┌─────────┬────────┬───────────────────────┬──────┬─────────────┬────────────┬─────────────┬───────────┐
// │ (index) │ Method │        Origin         │ Path │ Status code │ Persistent │ Invocations │ Remaining │
// ├─────────┼────────┼───────────────────────┼──────┼─────────────┼────────────┼─────────────┼───────────┤
// │    0    │ 'GET'  │ 'https://example.com' │ '/'  │     200     │    '❌'    │      0      │     1     │
// └─────────┴────────┴───────────────────────┴──────┴─────────────┴────────────┴─────────────┴───────────┘
```

### `mockAgent.isMockActive`

<!-- YAML
added: v5.3.0
-->

* Type: {boolean} `true` while mocking is active, `false` after
  [`mockAgent.deactivate()`][] has been called.

A read-only property indicating whether mocking is currently active on the
`MockAgent`.

```mjs
import { MockAgent } from 'undici'

const mockAgent = new MockAgent()

console.log(mockAgent.isMockActive) // true

mockAgent.deactivate()

console.log(mockAgent.isMockActive) // false
```

[`Dispatcher.dispatch()`]: Dispatcher.md#dispatcherdispatchoptions-handler
[`mockAgent.deactivate()`]: #mockagentdeactivate
[`mockAgent.enableCallHistory()`]: #mockagentenablecallhistory
[`mockAgent.get(origin)`]: #mockagentgetorigin
[`mockAgent.getCallHistory()`]: #mockagentgetcallhistory
[`mockAgent.pendingInterceptors()`]: #mockagentpendinginterceptors
[`mockAgent.request()`]: Dispatcher.md#dispatcherrequestoptions-callback
[`setGlobalDispatcher()`]: Dispatcher.md#setglobaldispatcherdispatcher
