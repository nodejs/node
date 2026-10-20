# MockClient

<!--introduced_in=v4.0.0-->
<!--type=module-->
<!-- source_link=lib/mock/mock-client.js -->

> Stability: 2 - Stable

A `MockClient` is a [`Client`][] that intercepts requests matching registered
routes and replies with mocked responses instead of contacting the network.
It is created by a [`MockAgent`][] when the agent is configured with a single
connection, and it exposes the same interception API as [`MockPool`][].

A `MockClient` is not constructed directly in most cases; instead it is obtained
through [`mockAgent.get(origin)`][] once the agent has `connections` set to `1`.

```mjs
import { MockAgent } from 'undici'

// `connections: 1` makes the agent hand out `MockClient` instances.
const mockAgent = new MockAgent({ connections: 1 })
const mockClient = mockAgent.get('http://localhost:3000')
```

```cjs
const { MockAgent } = require('undici')

const mockAgent = new MockAgent({ connections: 1 })
const mockClient = mockAgent.get('http://localhost:3000')
```

## Class: `MockClient`

<!-- YAML
added: v4.0.0
-->

* Extends: {Client}

Extends [`Client`][] and implements the `Interceptable` interface, allowing
requests made through it to be matched against registered mocks.

### `new MockClient(origin[, options])`

<!-- YAML
added: v4.0.0
-->

* `origin` {string} The origin to associate this mock client with. It should
  only include the protocol, hostname, and port.
* `options` {MockClientOptions} Extends the [`Client`][] options.
* Returns: {MockClient}

The `agent` option is required and must implement the `Agent` interface;
otherwise an `InvalidArgumentError` is thrown.

#### Parameter: `MockClientOptions`

Extends: `ClientOptions`

* `agent` {MockAgent} The agent to associate this mock client with.
* `ignoreTrailingSlash` {boolean} Whether trailing slashes should be ignored
  when matching the path of intercepted requests. **Default:** `false`.

### `mockClient.intercept(options)`

<!-- YAML
added: v4.0.0
-->

* `options` {MockInterceptor.Options} The matching criteria for the requests to
  intercept.
  * `path` {string|RegExp|Function} The path to match against. A function
    receives the request path as a `string` and returns a `boolean`.
  * `method` {string|RegExp|Function} The method to match against. A function
    receives the request method as a `string` and returns a `boolean`.
    **Default:** `'GET'`.
  * `body` {string|RegExp|Function} The body to match against. A function
    receives the request body as a `string` and returns a `boolean`.
  * `headers` {Object|Function} The headers to match against, as a map of header
    name to a `string`, `RegExp`, or matching function, or a single function
    that receives all headers and returns a `boolean`.
  * `query` {Object} The query parameters to match against.
  * `ignoreTrailingSlash` {boolean} Whether a trailing slash on `path` is
    ignored when matching. Inherited from the mock client's
    `ignoreTrailingSlash` option when not set.
* Returns: {MockInterceptor} The interceptor used to define the mocked reply.

Registers a route on the mock client and returns a [`MockInterceptor`][] for any
matching request that uses the same origin as this mock client. The interceptor
is then used to define the reply, for example with `reply()` or
`replyWithError()`.

```mjs
import { MockAgent } from 'undici'

const mockAgent = new MockAgent({ connections: 1 })
const mockClient = mockAgent.get('http://localhost:3000')

mockClient.intercept({ path: '/foo', method: 'GET' }).reply(200, 'foo')
```

### `mockClient.cleanMocks()`

<!-- YAML
added: v7.11.0
-->

Removes all registered interceptors from the mock client. Pending mocks defined
before this call no longer match incoming requests.

```mjs
import { MockAgent } from 'undici'

const mockAgent = new MockAgent({ connections: 1 })
const mockClient = mockAgent.get('http://localhost:3000')

mockClient.intercept({ path: '/foo' }).reply(200, 'foo')
mockClient.cleanMocks()
```

### `mockClient.close()`

<!-- YAML
added: v4.0.0
-->

* Returns: {Promise} Fulfills with `undefined` once the mock client is closed.

Closes the mock client, gracefully waiting for any enqueued requests to
complete, and removes it from the associated [`MockAgent`][].

```mjs
import { MockAgent } from 'undici'

const mockAgent = new MockAgent({ connections: 1 })
const mockClient = mockAgent.get('http://localhost:3000')

await mockClient.close()
```

### `mockClient.dispatch(options, handlers)`

<!-- YAML
added: v4.0.0
-->

* `options` {DispatchOptions} The request options.
* `handlers` {DispatchHandler} The handlers invoked over the request lifecycle.
* Returns: {boolean} `false` if the dispatcher is busy and the caller should
  wait before dispatching further requests, otherwise `true`.

Dispatches a request, matching it against the registered mocks. This override of
[`dispatcher.dispatch(options, handlers)`][] is what drives the mocking
behaviour for every higher-level method such as `request`.

### `mockClient.request(options[, callback])`

<!-- YAML
added: v4.0.0
-->

* `options` {DispatchOptions}
* `callback` {Function} (optional) Invoked with the response when no `Promise`
  is requested.
* Returns: {Promise} Fulfills with the mocked response when no `callback` is
  provided.

Performs a request and resolves it against the registered mocks. Inherited from
[`Client`][]; see [`dispatcher.request(options[, callback])`][] for the full
parameter and return value documentation.

```mjs
import { MockAgent } from 'undici'

const mockAgent = new MockAgent({ connections: 1 })
const mockClient = mockAgent.get('http://localhost:3000')
mockClient.intercept({ path: '/foo' }).reply(200, 'foo')

const { statusCode, body } = await mockClient.request({
  origin: 'http://localhost:3000',
  path: '/foo',
  method: 'GET'
})

console.log('response received', statusCode) // response received 200

for await (const data of body) {
  console.log('data', data.toString('utf8')) // data foo
}
```

[`Client`]: Client.md#class-client
[`MockAgent`]: MockAgent.md#class-mockagent
[`MockInterceptor`]: MockPool.md#return-mockinterceptor
[`MockPool`]: MockPool.md#class-mockpool
[`dispatcher.dispatch(options, handlers)`]: Dispatcher.md#dispatcherdispatchoptions-handler
[`dispatcher.request(options[, callback])`]: Dispatcher.md#dispatcherrequestoptions-callback
[`mockAgent.get(origin)`]: MockAgent.md#mockagentgetorigin
