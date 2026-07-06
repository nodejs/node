# MockPool

<!--introduced_in=v4.0.0-->
<!--type=module-->
<!-- source_link=lib/mock/mock-pool.js -->

> Stability: 2 - Stable

A `MockPool` is a [`Pool`][] that intercepts requests matching registered routes
and replies with mocked responses instead of contacting the network. It is
created by a [`MockAgent`][] and exposes the same interception API as
[`MockClient`][].

A `MockPool` is not constructed directly in most cases; instead it is obtained
through [`mockAgent.get(origin)`][]. Once registered as the global dispatcher,
any request whose origin matches the pool is matched against the mocks it holds.

```mjs
import { MockAgent } from 'undici'

const mockAgent = new MockAgent()
const mockPool = mockAgent.get('http://localhost:3000')
```

```cjs
const { MockAgent } = require('undici')

const mockAgent = new MockAgent()
const mockPool = mockAgent.get('http://localhost:3000')
```

## Class: `MockPool`

<!-- YAML
added: v4.0.0
-->

* Extends: {Pool}

Extends [`Pool`][] and implements the `Interceptable` interface, allowing
requests made through it to be matched against registered mocks.

### `new MockPool(origin[, options])`

<!-- YAML
added: v4.0.0
-->

* `origin` {string} The origin to associate this mock pool with. It should only
  include the protocol, hostname, and port.
* `options` {MockPoolOptions} Extends the [`Pool`][] options.
* Returns: {MockPool}

The `agent` option is required and must implement the `Agent` interface;
otherwise an `InvalidArgumentError` is thrown.

#### Parameter: `MockPoolOptions`

Extends: `PoolOptions`

* `agent` {MockAgent} The agent to associate this mock pool with.
* `ignoreTrailingSlash` {boolean} Whether trailing slashes should be ignored
  when matching the path of intercepted requests. **Default:** `false`.

### `mockPool.intercept(options)`

<!-- YAML
added: v4.0.0
-->

* `options` {MockPoolInterceptOptions} The matching criteria for the requests to
  intercept.
* Returns: {MockInterceptor} The interceptor used to define the mocked reply.

Registers a route on the mock pool and returns a [`MockInterceptor`][] for any
matching request that uses the same origin as this mock pool. The interceptor is
then used to define the reply, for example with `reply()` or `replyWithError()`.

Each `intercept()` call is consumed by a single matching request. To match more
than one request, call `intercept()` once per expected request, or use
`persist()` or `times()` on the returned [`MockScope`][]. When no registered
mock matches a request, a real request is attempted unless network connections
have been disabled on the [`MockAgent`][], in which case a `MockNotMatchedError`
is thrown.

A request is intercepted only when every defined matcher passes. The accepted
matcher forms behave as follows:

| Matcher type | Condition to pass            |
| :----------: | ---------------------------- |
| `string`     | Exact match against the value |
| `RegExp`     | The expression must match    |
| `Function`   | The function must return `true` |

#### Parameter: `MockPoolInterceptOptions`

* `path` {string|RegExp|Function} A matcher for the HTTP request path. The
  function form has the signature `(path: string) => boolean`. When a `RegExp`
  or function is used, it matches against the request path including all query
  parameters in alphabetical order. When a `string` is used, query parameters
  can instead be supplied through `query`.
* `method` {string|RegExp|Function} A matcher for the HTTP request method. The
  function form has the signature `(method: string) => boolean`.
  **Default:** `'GET'`.
* `body` {string|RegExp|Function} A matcher for the HTTP request body. The
  function form has the signature `(body: string) => boolean`.
* `headers` {Object|Function} A matcher for the HTTP request headers, given
  either as a map of header name to a `string`, `RegExp`, or
  `(value: string) => boolean` matcher, or as a single function that receives
  all headers and returns a `boolean`. When a map is used, the request must
  match every defined header; extra headers not listed here do not affect
  matching.
* `query` {Object} A matcher for the HTTP request query string parameters. Only
  applies when a `string` was provided for `path`.
* `ignoreTrailingSlash` {boolean} Whether a trailing slash on `path` is ignored
  when matching. Inherited from the mock pool's `ignoreTrailingSlash` option
  when not set.

#### Return: `MockInterceptor`

The reply behaviour of a matching request is defined through the returned
`MockInterceptor`.

* `reply(statusCode[, data[, responseOptions]])` {Function} Defines the reply
  for a matching request.
  * `statusCode` {number} The status code of the mocked reply.
  * `data` {string|Buffer|Object|Function} The body of the mocked reply.
    Objects are serialized to JSON; strings and `Buffer`s are sent as-is. The
    function form has the signature
    `(opts: MockResponseCallbackOptions) => string | Buffer | Object` and is
    invoked with the incoming request to compute the reply body.
  * `responseOptions` {MockResponseOptions} Additional reply options.
    **Default:** `{}`.
  * Returns: {MockScope}
* `reply(callback)` {Function} Defines the reply for a matching request,
  computing all reply options dynamically rather than just the body.
  * `callback` {Function} A `(opts: MockResponseCallbackOptions) =>
    { statusCode, data, responseOptions }` function invoked with the incoming
    request.
  * Returns: {MockScope}
* `replyWithError(error)` {Function} Defines an error for a matching request to
  throw.
  * `error` {Error} The error thrown when a request matches.
  * Returns: {MockScope}
* `defaultReplyHeaders(headers)` {Function} Sets default headers included on
  every subsequent reply defined on this interceptor, in addition to any headers
  set on a specific reply.
  * `headers` {Object} A map of header name to value.
  * Returns: {MockInterceptor}
* `defaultReplyTrailers(trailers)` {Function} Sets default trailers included on
  every subsequent reply defined on this interceptor, in addition to any
  trailers set on a specific reply.
  * `trailers` {Object} A map of trailer name to value.
  * Returns: {MockInterceptor}
* `replyContentLength()` {Function} Sets an automatically calculated
  `content-length` header on every subsequent reply defined on this interceptor.
  * Returns: {MockInterceptor}

By default, `reply()` and `replyWithError()` define the behaviour for the first
matching request only; subsequent requests are not affected unless `persist()`
or `times()` is used on the returned [`MockScope`][].

#### Parameter: `MockResponseCallbackOptions`

The argument passed to the `reply()` data and options callbacks.

* `path` {string} The path of the intercepted request.
* `method` {string} The method of the intercepted request.
* `headers` {Object|Headers} The headers of the intercepted request.
* `origin` {string} The origin of the intercepted request.
* `body` {string|null} The body of the intercepted request.

#### Parameter: `MockResponseOptions`

* `headers` {Object} Headers to include on the mocked reply.
* `trailers` {Object} Trailers to include on the mocked reply.

#### Return: `MockScope`

A `MockScope` is associated with a single [`MockInterceptor`][] and configures
how many times the defined reply is used.

* `delay(waitInMs)` {Function} Delays the associated reply by a set amount of
  time.
  * `waitInMs` {number} The delay in milliseconds.
  * Returns: {MockScope}
* `persist()` {Function} Makes the associated reply match indefinitely, so every
  matching request receives the defined response.
  * Returns: {MockScope}
* `times(repeatTimes)` {Function} Makes the associated reply match a fixed number
  of times. This is overridden by `persist()`.
  * `repeatTimes` {number} The number of matching requests the reply applies to.
  * Returns: {MockScope}

The following examples show common interception patterns.

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

```mjs displayName="Reply with a data callback"
import { MockAgent, setGlobalDispatcher, request } from 'undici'

const mockAgent = new MockAgent()
setGlobalDispatcher(mockAgent)

const mockPool = mockAgent.get('http://localhost:3000')

mockPool.intercept({
  path: '/echo',
  method: 'GET'
}).reply(200, ({ headers }) => ({ message: headers.message }))

const { statusCode, body } = await request('http://localhost:3000/echo', {
  headers: { message: 'hello world!' }
})

console.log('response received', statusCode) // response received 200

for await (const data of body) {
  console.log('data', data.toString('utf8')) // {"message":"hello world!"}
}
```

```mjs displayName="Reply with an options callback"
import { MockAgent, setGlobalDispatcher, request } from 'undici'

const mockAgent = new MockAgent()
setGlobalDispatcher(mockAgent)

const mockPool = mockAgent.get('http://localhost:3000')

mockPool.intercept({
  path: '/echo',
  method: 'GET'
}).reply(({ headers }) => ({
  statusCode: 200,
  data: { message: headers.message }
}))

const { statusCode, body } = await request('http://localhost:3000/echo', {
  headers: { message: 'hello world!' }
})

console.log('response received', statusCode) // response received 200

for await (const data of body) {
  console.log('data', data.toString('utf8')) // {"message":"hello world!"}
}
```

```mjs displayName="Multiple intercepts"
import { MockAgent, setGlobalDispatcher, request } from 'undici'

const mockAgent = new MockAgent()
setGlobalDispatcher(mockAgent)

const mockPool = mockAgent.get('http://localhost:3000')

mockPool.intercept({ path: '/foo', method: 'GET' }).reply(200, 'foo')
mockPool.intercept({ path: '/hello', method: 'GET' }).reply(200, 'hello')

const fooResult = await request('http://localhost:3000/foo')
for await (const data of fooResult.body) {
  console.log('data', data.toString('utf8')) // data foo
}

const helloResult = await request('http://localhost:3000/hello')
for await (const data of helloResult.body) {
  console.log('data', data.toString('utf8')) // data hello
}
```

```mjs displayName="Match query, body, request headers; reply with headers and trailers"
import { MockAgent, setGlobalDispatcher, request } from 'undici'

const mockAgent = new MockAgent()
setGlobalDispatcher(mockAgent)

const mockPool = mockAgent.get('http://localhost:3000')

mockPool.intercept({
  path: '/foo?hello=there&see=ya',
  method: 'POST',
  body: 'form1=data1&form2=data2',
  headers: {
    'User-Agent': 'undici',
    Host: 'example.com'
  }
}).reply(200, { foo: 'bar' }, {
  headers: { 'content-type': 'application/json' },
  trailers: { 'Content-MD5': 'test' }
})

const { statusCode, headers, trailers, body } = await request(
  'http://localhost:3000/foo?hello=there&see=ya',
  {
    method: 'POST',
    body: 'form1=data1&form2=data2',
    headers: {
      foo: 'bar',
      'User-Agent': 'undici',
      Host: 'example.com'
    }
  }
)

console.log('response received', statusCode) // response received 200
console.log('headers', headers) // { 'content-type': 'application/json' }

for await (const data of body) {
  console.log('data', data.toString('utf8')) // {"foo":"bar"}
}

console.log('trailers', trailers) // { 'content-md5': 'test' }
```

```mjs displayName="Different matcher forms"
import { MockAgent, setGlobalDispatcher, request } from 'undici'

const mockAgent = new MockAgent()
setGlobalDispatcher(mockAgent)

const mockPool = mockAgent.get('http://localhost:3000')

mockPool.intercept({
  path: '/foo',
  method: /^GET$/,
  body: (value) => value === 'form=data',
  headers: {
    'User-Agent': 'undici',
    Host: /^example\.com$/
  }
}).reply(200, 'foo')

const { statusCode, body } = await request('http://localhost:3000/foo', {
  method: 'GET',
  body: 'form=data',
  headers: {
    foo: 'bar',
    'User-Agent': 'undici',
    Host: 'example.com'
  }
})

console.log('response received', statusCode) // response received 200

for await (const data of body) {
  console.log('data', data.toString('utf8')) // data foo
}
```

```mjs displayName="Reply with an error"
import { MockAgent, setGlobalDispatcher, request } from 'undici'

const mockAgent = new MockAgent()
setGlobalDispatcher(mockAgent)

const mockPool = mockAgent.get('http://localhost:3000')

mockPool.intercept({
  path: '/foo',
  method: 'GET'
}).replyWithError(new Error('kaboom'))

try {
  await request('http://localhost:3000/foo', { method: 'GET' })
} catch (error) {
  console.error(error.message) // kaboom
}
```

```mjs displayName="Default reply headers"
import { MockAgent, setGlobalDispatcher, request } from 'undici'

const mockAgent = new MockAgent()
setGlobalDispatcher(mockAgent)

const mockPool = mockAgent.get('http://localhost:3000')

mockPool.intercept({ path: '/foo', method: 'GET' })
  .defaultReplyHeaders({ foo: 'bar' })
  .reply(200, 'foo')

const { headers } = await request('http://localhost:3000/foo')

console.log('headers', headers) // headers { foo: 'bar' }
```

```mjs displayName="Default reply trailers"
import { MockAgent, setGlobalDispatcher, request } from 'undici'

const mockAgent = new MockAgent()
setGlobalDispatcher(mockAgent)

const mockPool = mockAgent.get('http://localhost:3000')

mockPool.intercept({ path: '/foo', method: 'GET' })
  .defaultReplyTrailers({ foo: 'bar' })
  .reply(200, 'foo')

const { trailers } = await request('http://localhost:3000/foo')

console.log('trailers', trailers) // trailers { foo: 'bar' }
```

```mjs displayName="Automatic content-length"
import { MockAgent, setGlobalDispatcher, request } from 'undici'

const mockAgent = new MockAgent()
setGlobalDispatcher(mockAgent)

const mockPool = mockAgent.get('http://localhost:3000')

mockPool.intercept({ path: '/foo', method: 'GET' })
  .replyContentLength()
  .reply(200, 'foo')

const { headers } = await request('http://localhost:3000/foo')

console.log('headers', headers) // headers { 'content-length': '3' }
```

```mjs displayName="Persisted reply"
import { MockAgent, setGlobalDispatcher, request } from 'undici'

const mockAgent = new MockAgent()
setGlobalDispatcher(mockAgent)

const mockPool = mockAgent.get('http://localhost:3000')

mockPool.intercept({ path: '/foo', method: 'GET' }).reply(200, 'foo').persist()

await request('http://localhost:3000/foo') // Matches and returns the mock
await request('http://localhost:3000/foo') // Matches again, indefinitely
```

```mjs displayName="Reply a fixed number of times"
import { MockAgent, setGlobalDispatcher, request } from 'undici'

const mockAgent = new MockAgent()
setGlobalDispatcher(mockAgent)

const mockPool = mockAgent.get('http://localhost:3000')

mockPool.intercept({ path: '/foo', method: 'GET' }).reply(200, 'foo').times(2)

await request('http://localhost:3000/foo') // Matches and returns the mock
await request('http://localhost:3000/foo') // Matches and returns the mock
await request('http://localhost:3000/foo') // No match; a real request is attempted
```

```mjs displayName="Match the path with a function"
import { MockAgent, setGlobalDispatcher, request } from 'undici'
import querystring from 'node:querystring'

const mockAgent = new MockAgent()
setGlobalDispatcher(mockAgent)

const mockPool = mockAgent.get('http://localhost:3000')

const matchPath = (requestPath) => {
  const [pathname, search] = requestPath.split('?')
  const requestQuery = querystring.parse(search)

  if (!pathname.startsWith('/foo')) {
    return false
  }

  return requestQuery.foo === 'bar'
}

mockPool.intercept({ path: matchPath, method: 'GET' }).reply(200, 'foo')

await request('http://localhost:3000/foo?foo=bar') // Matches and returns the mock
```

### `mockPool.close()`

<!-- YAML
added: v4.0.0
-->

* Returns: {Promise} Fulfills with `undefined` once the mock pool is closed.

Closes the mock pool, gracefully waiting for any enqueued requests to complete,
and removes it from the associated [`MockAgent`][].

```mjs
import { MockAgent } from 'undici'

const mockAgent = new MockAgent()
const mockPool = mockAgent.get('http://localhost:3000')

await mockPool.close()
```

### `mockPool.dispatch(options, handlers)`

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

### `mockPool.request(options[, callback])`

<!-- YAML
added: v4.0.0
-->

* `options` {DispatchOptions}
* `callback` {Function} (optional) Invoked with the response when no `Promise`
  is requested.
* Returns: {Promise} Fulfills with the mocked response when no `callback` is
  provided.

Performs a request and resolves it against the registered mocks. Inherited from
[`Pool`][]; see [`dispatcher.request(options[, callback])`][] for the full
parameter and return value documentation.

```mjs
import { MockAgent } from 'undici'

const mockAgent = new MockAgent()
const mockPool = mockAgent.get('http://localhost:3000')
mockPool.intercept({ path: '/foo', method: 'GET' }).reply(200, 'foo')

const { statusCode, body } = await mockPool.request({
  origin: 'http://localhost:3000',
  path: '/foo',
  method: 'GET'
})

console.log('response received', statusCode) // response received 200

for await (const data of body) {
  console.log('data', data.toString('utf8')) // data foo
}
```

### `mockPool.cleanMocks()`

<!-- YAML
added: v7.11.0
-->

* Returns: {undefined}

Removes all registered interceptors from the mock pool. Pending mocks defined
before this call no longer match incoming requests.

```mjs
import { MockAgent } from 'undici'

const mockAgent = new MockAgent()
const mockPool = mockAgent.get('http://localhost:3000')

mockPool.intercept({ path: '/foo' }).reply(200, 'foo')
mockPool.cleanMocks()
```

[`MockAgent`]: MockAgent.md#class-mockagent
[`MockClient`]: MockClient.md#class-mockclient
[`MockInterceptor`]: MockPool.md#return-mockinterceptor
[`MockScope`]: MockPool.md#return-mockscope
[`Pool`]: Pool.md#class-pool
[`dispatcher.dispatch(options, handlers)`]: Dispatcher.md#dispatcherdispatchoptions-handler
[`dispatcher.request(options[, callback])`]: Dispatcher.md#dispatcherrequestoptions-callback
[`mockAgent.get(origin)`]: MockAgent.md#mockagentgetorigin
