# ProxyAgent

<!--introduced_in=v4.8.2-->
<!--type=module-->
<!-- source_link=lib/dispatcher/proxy-agent.js -->

> Stability: 2 - Stable

A `ProxyAgent` routes every request through an HTTP, HTTPS, or SOCKS5 proxy
server. It implements the [`Agent`][] API, so it can be used wherever a
dispatcher is accepted, for example with [`request`][] or [`fetch`][], or
installed globally with [`setGlobalDispatcher`][].

For secure (`https:`) endpoints the proxy connection is established through an
HTTP `CONNECT` tunnel. SOCKS5 proxies (`socks5:` or `socks:` URIs) are handled
by delegating to a [`Socks5ProxyAgent`][].

```mjs
import { ProxyAgent, setGlobalDispatcher } from 'undici'

const proxyAgent = new ProxyAgent('http://localhost:8000')
setGlobalDispatcher(proxyAgent)
```

## Class: `ProxyAgent`

<!-- YAML
added: v4.8.2
-->

* Extends: {Dispatcher}

`ProxyAgent` keeps an internal [`Agent`][] that creates per-origin dispatchers
on demand. The proxy connection is set up lazily when the first request to an
origin is dispatched, then reused for subsequent requests to the same origin.

### `new ProxyAgent(options)`

<!-- YAML
added: v4.8.2
-->

* `options` {ProxyAgentOptions|string|URL} The proxy URI, or an options object.
  Required. When a `string` or {URL} is passed it is used as the proxy `uri`.
  The options object extends {AgentOptions} (omitting `connect`).
  * `uri` {string} The URI of the proxy server. Required when `options` is an
    object. Parsed into a {URL} according to the
    [WHATWG URL Specification][].
  * `token` {string} A pre-formatted `proxy-authorization` header value sent to
    the proxy on every request, for example `` `Bearer ${apiKey}` ``.
  * `auth` {string} A base64-encoded `username:password` credential sent as a
    `Basic` `proxy-authorization` header. Cannot be combined with `token`.
  * `headers` {Object} Additional headers sent to the proxy server on the
    `CONNECT` request (or on every request for an untunneled HTTP proxy).
    **Default:** `{}`.
  * `requestTls` {BuildOptions} TLS options for the connection to the request
    endpoint, passed to the connector builder.
  * `proxyTls` {BuildOptions} TLS options for the connection to the proxy
    server, passed to the connector builder.
  * `clientFactory` {Function} Builds the {Dispatcher} used to talk to the proxy
    server itself. **Default:** `(origin, opts) => new Pool(origin, opts)`.
    * `origin` {URL} The proxy origin.
    * `opts` {Object} The resolved options for the dispatcher.
    * Returns: {Dispatcher}
  * `proxyTunnel` {boolean} Forces tunneling through the proxy. By default,
    Undici detects tunneling based on the request protocol. If the target
    endpoint uses HTTPS, Undici establishes a `CONNECT` tunnel through the proxy
    (after the TLS handshake to the proxy itself when the proxy URL is HTTPS).
    If the target endpoint uses plain HTTP, Undici forwards the request to the
    proxy using an HTTP/1.1 absolute-form request target (over TLS when the
    proxy URL is HTTPS), as required by
    [RFC 9112 §3.2.2](https://www.rfc-editor.org/rfc/rfc9112.html#name-absolute-form).
    This non-tunneled forwarding path does not negotiate HTTP/2 with the proxy.
    Set `proxyTunnel` to `true` to force tunneling for plain HTTP requests as
    well. Currently, there is no way to facilitate HTTP/1.1 IP tunneling as
    described in
    [RFC 9484](https://www.rfc-editor.org/rfc/rfc9484.html#name-http-11-request).

Throws an {InvalidArgumentError} when no proxy URI is provided, when
`clientFactory` is not a function, or when both `auth` and `token` are supplied.

The `proxy-authorization` header is derived, in order of precedence, from
`token`, then `auth`, then the `username` and `password` embedded in the proxy
`uri`. Credentials in the URI are URL-decoded before being base64-encoded.

> [!NOTE]
> Unless {AgentOptions} `connections` is set to `0`, the non-standard
> `proxy-connection: keep-alive` header is added to the tunnel `CONNECT`
> request. This includes the default case where `connections` is unset.

The `auth` option is deprecated; use `token` instead.

```mjs
import { ProxyAgent } from 'undici'

// A string or URL is treated as the proxy uri.
const a = new ProxyAgent('http://localhost:8000')
const b = new ProxyAgent(new URL('http://localhost:8000'))

// An options object requires a `uri`.
const c = new ProxyAgent({ uri: 'http://localhost:8000' })

// With proxy TLS settings.
const d = new ProxyAgent({
  uri: 'https://secure.proxy.server',
  proxyTls: {
    signal: AbortSignal.timeout(1000),
  },
})
```

#### Example - proxy request with the global dispatcher

```mjs
import { setGlobalDispatcher, request, ProxyAgent } from 'undici'

const proxyAgent = new ProxyAgent('http://localhost:8000')
setGlobalDispatcher(proxyAgent)

const { statusCode, body } = await request('http://localhost:3000/foo')

console.log('response received', statusCode)
console.log('data', await body.text())
```

#### Example - proxy request with a local dispatcher

```mjs
import { ProxyAgent, request } from 'undici'

const proxyAgent = new ProxyAgent('http://localhost:8000')

const { statusCode, body } = await request('http://localhost:3000/foo', {
  dispatcher: proxyAgent,
})

console.log('response received', statusCode)
console.log('data', await body.text())
```

#### Example - proxy request with authentication

```mjs
import { setGlobalDispatcher, request, ProxyAgent } from 'undici'

const proxyAgent = new ProxyAgent({
  uri: 'http://localhost:8000',
  token: `Basic ${Buffer.from('username:password').toString('base64')}`,
})
setGlobalDispatcher(proxyAgent)

const { statusCode } = await request('http://localhost:3000/foo')

console.log('response received', statusCode)
```

### `proxyAgent.dispatch(options, handler)`

<!-- YAML
added: v4.8.2
-->

* `options` {AgentDispatchOptions} Extends {DispatchOptions}.
  * `origin` {string|URL} The origin to dispatch the request against. Required.
* `handler` {DispatchHandler}
* Returns: {boolean} `false` if the dispatcher is busy and the caller should
  wait for the [`'drain'`][] event before dispatching again.

Dispatches the request through the proxy. Throws an {InvalidArgumentError} if
the request headers contain a `proxy-authorization` header, which must instead
be supplied through the constructor's `token` or `auth` option. A `host` header
is derived from `options.origin` when one is not already present. Implements
[`Dispatcher.dispatch()`][].

### `proxyAgent.close()`

<!-- YAML
added: v4.8.2
changes:
  - version: v7.10.0
    pr-url: https://github.com/nodejs/undici/pull/4180
    description: Untunneled HTTP-to-HTTP proxy connections match `curl` behavior.
-->

* Returns: {Promise<void>}

Closes the proxy agent and waits for its internal agent and proxy client (when
present) to close before resolving. Implements [`Dispatcher.close()`][].

```mjs
import { ProxyAgent, setGlobalDispatcher } from 'undici'

const proxyAgent = new ProxyAgent('http://localhost:8000')
setGlobalDispatcher(proxyAgent)

await proxyAgent.close()
```

### `proxyAgent.request(options[, callback])`

<!-- YAML
added: v5.0.0
-->

See [`Dispatcher.request()`][]. The request is routed through the proxy.

#### Example - proxy request with `fetch`

```mjs
import { ProxyAgent, fetch } from 'undici'

const proxyAgent = new ProxyAgent('http://localhost:8000')

const response = await fetch('http://localhost:3000/foo', {
  dispatcher: proxyAgent,
  method: 'GET',
})

console.log('response status', response.status)
console.log('response data', await response.text())
```

#### Example - HTTPS tunneling

```mjs
import { ProxyAgent, fetch } from 'undici'

const proxyAgent = new ProxyAgent('https://secure.proxy.server')

const response = await fetch('https://secure.endpoint.com/api/data', {
  dispatcher: proxyAgent,
  method: 'GET',
})

console.log('response status', response.status)
console.log('response data', await response.json())
```

[WHATWG URL Specification]: https://url.spec.whatwg.org
[`'drain'`]: Dispatcher.md#event-drain
[`Agent`]: Agent.md#class-agent
[`Dispatcher.close()`]: Dispatcher.md#dispatcherclosecallback-promise
[`Dispatcher.dispatch()`]: Dispatcher.md#dispatcherdispatchoptions-handler
[`Dispatcher.request()`]: Dispatcher.md#dispatcherrequestoptions-callback
[`Socks5ProxyAgent`]: Socks5ProxyAgent.md#class-socks5proxyagent
[`fetch`]: Fetch.md
[`request`]: Dispatcher.md#dispatcherrequestoptions-callback
[`setGlobalDispatcher`]: Dispatcher.md#setglobaldispatcherdispatcher
