# Socks5ProxyAgent

<!--introduced_in=v7.23.0-->
<!--type=module-->
<!-- source_link=lib/dispatcher/socks5-proxy-agent.js -->

> Stability: 1 - Experimental

A [`Dispatcher`][] implementation that routes requests through a [SOCKS5][]
proxy server. It tunnels both HTTP and HTTPS traffic over the proxy, supports
optional username/password authentication, and pools connections per origin so
that subsequent requests to the same host reuse the established tunnel.

DNS resolution is delegated to the proxy: target host names are sent to the
proxy as domain names rather than being resolved locally. For HTTPS targets the
TLS session is negotiated end-to-end through the tunnel, so the proxy only sees
encrypted bytes.

Import it from `'undici'`:

```mjs
import { Socks5ProxyAgent } from 'undici'
```

Constructing the agent emits a one-time `ExperimentalWarning`, reflecting the
experimental status of SOCKS5 support.

## Class: `Socks5ProxyAgent`

<!-- YAML
added: v7.23.0
-->

> Stability: 1 - Experimental

* Extends: {Dispatcher}

Routes dispatched requests through a SOCKS5 proxy. Register an instance as the
global dispatcher with [`setGlobalDispatcher()`][], or pass it per request via
the `dispatcher` option.

### `new Socks5ProxyAgent(proxyUrl[, options])`

<!-- YAML
added: v7.23.0
-->

> Stability: 1 - Experimental

* `proxyUrl` {string|URL} The SOCKS5 proxy server URL. Must use the `socks5:` or
  `socks:` protocol. Credentials may be embedded in the URL userinfo (for
  example `socks5://user:pass@host:1080`).
* `options` {Object} (optional) Extends {PoolOptions}; the pool options are
  applied to the per-origin pools created behind the tunnel.
  * `headers` {Object} Additional headers to send with the proxy connection.
    **Default:** `{}`.
  * `username` {string} Username for SOCKS5 authentication. Takes precedence over
    a username embedded in `proxyUrl`. **Default:** the URL username, if any.
  * `password` {string} Password for SOCKS5 authentication. Takes precedence over
    a password embedded in `proxyUrl`. **Default:** the URL password, if any.
  * `connect` {Function} Custom connector used to open the socket to the proxy.
    **Default:** a connector built from `proxyTls`.
  * `proxyTls` {BuildOptions} TLS options for the connection to the proxy itself
    (SOCKS5 over TLS). When set, the proxy connection is established over TLS and
    `servername` defaults to the proxy host name.
  * `requestTls` {BuildOptions} TLS options applied to the end-to-end connection
    to an HTTPS target through the tunnel, such as `ca`, `cert`, `key`,
    `rejectUnauthorized`, and `servername`. `servername` defaults to the target
    host name.

Throws an `InvalidArgumentError` if `proxyUrl` is missing or does not use the
`socks5:` or `socks:` protocol.

```mjs
import { Socks5ProxyAgent } from 'undici'

// Without authentication.
const agent = new Socks5ProxyAgent('socks5://localhost:1080')

// With credentials in the URL.
const authAgent = new Socks5ProxyAgent('socks5://user:pass@localhost:1080')

// With credentials and pool options.
const configuredAgent = new Socks5ProxyAgent('socks5://localhost:1080', {
  username: 'user',
  password: 'pass',
  connections: 10
})
```

Using the agent as the global dispatcher:

```mjs
import { setGlobalDispatcher, request, Socks5ProxyAgent } from 'undici'

const agent = new Socks5ProxyAgent('socks5://localhost:1080')
setGlobalDispatcher(agent)

const { statusCode, body } = await request('http://localhost:3000/foo')
console.log('response received', statusCode)

for await (const data of body) {
  console.log('data', data.toString('utf8'))
}
```

Using the agent per request:

```mjs
import { request, Socks5ProxyAgent } from 'undici'

const agent = new Socks5ProxyAgent('socks5://localhost:1080')

const { statusCode, body } = await request('http://localhost:3000/foo', {
  dispatcher: agent
})

console.log('response received', statusCode)
```

HTTPS targets are tunnelled and encrypted end-to-end:

```mjs
import { request, Socks5ProxyAgent } from 'undici'

const agent = new Socks5ProxyAgent('socks5://localhost:1080')

const { statusCode, body } = await request('https://api.example.com/data', {
  dispatcher: agent
})

console.log('Response status:', statusCode)
console.log('Response data:', await body.json())
```

### `socks5ProxyAgent.close()`

<!-- YAML
added: v7.23.0
-->

> Stability: 1 - Experimental

* Returns: {Promise<void>} Resolves once every per-origin pool has been closed.

Gracefully closes the agent, waiting for all underlying pools and their
connections to drain and close before resolving.

```mjs
import { setGlobalDispatcher, Socks5ProxyAgent } from 'undici'

const agent = new Socks5ProxyAgent('socks5://localhost:1080')
setGlobalDispatcher(agent)

// ... make requests ...

await agent.close()
```

### `socks5ProxyAgent.destroy([err])`

<!-- YAML
added: v7.23.0
-->

> Stability: 1 - Experimental

* `err` {Error} (optional) The error to reject in-flight requests with.
* Returns: {Promise<void>} Resolves once every per-origin pool has been
  destroyed.

Forcibly destroys the agent and all underlying connections immediately, without
waiting for in-flight requests to complete.

```mjs
import { Socks5ProxyAgent } from 'undici'

const agent = new Socks5ProxyAgent('socks5://localhost:1080')

await agent.destroy()
```

### `socks5ProxyAgent.dispatch(options, handlers)`

<!-- YAML
added: v7.23.0
-->

> Stability: 1 - Experimental

* `options` {DispatchOptions}
* `handlers` {DispatchHandler}
* Returns: {boolean} `false` if the dispatcher is busy and the request should be
  retried later, otherwise `true`.

Routes a request through the SOCKS5 tunnel. The agent maintains a [`Pool`][] per
target origin; the first request to a given origin establishes the tunnel and
upgrades to TLS when the target is HTTPS, and later requests reuse the pool.

This is the lower-level entry point implementing
[`Dispatcher.dispatch(options, handlers)`][]. Prefer
[`socks5ProxyAgent.request()`][] for most use cases.

### `socks5ProxyAgent.request(options[, callback])`

<!-- YAML
added: v7.23.0
-->

> Stability: 1 - Experimental

* `options` {DispatchOptions}
* `callback` {Function} (optional)
* Returns: {Promise|void} A promise resolving to the response when `callback` is
  omitted.

Performs a request through the SOCKS5 proxy. Inherited from
[`Dispatcher.request(options[, callback])`][]; see that method for the full set
of options and the shape of the resolved response.

```mjs
import { fetch, Socks5ProxyAgent } from 'undici'

const agent = new Socks5ProxyAgent('socks5://localhost:1080')

const response = await fetch('http://localhost:3000/api/users', {
  dispatcher: agent
})

console.log('Response status:', response.status)
console.log('Response data:', await response.text())
```

## Debugging

SOCKS5 proxy activity can be traced with the `undici:socks5-proxy` debug scope:

```bash
NODE_DEBUG=undici:socks5-proxy node script.js
```

This logs the SOCKS5 handshake, authentication, and tunnel establishment steps.

[SOCKS5]: https://www.rfc-editor.org/rfc/rfc1928
[`Dispatcher.dispatch(options, handlers)`]: Dispatcher.md#dispatcherdispatchoptions-handler
[`Dispatcher.request(options[, callback])`]: Dispatcher.md#dispatcherrequestoptions-callback
[`Dispatcher`]: Dispatcher.md#class-dispatcher
[`Pool`]: Pool.md#class-pool
[`setGlobalDispatcher()`]: Dispatcher.md#setglobaldispatcherdispatcher
[`socks5ProxyAgent.request()`]: #socks5proxyagentrequestoptions-callback
