# EnvHttpProxyAgent

<!--introduced_in=v6.14.0-->

<!--type=module-->

<!-- source_link=lib/dispatcher/env-http-proxy-agent.js -->

> Stability: 2 - Stable

`EnvHttpProxyAgent` is a [`Dispatcher`][] that reads its proxy configuration from
the `http_proxy`, `https_proxy`, and `no_proxy` environment variables and routes
requests through the appropriate proxy automatically. Import it from `undici`:

```mjs
import { EnvHttpProxyAgent } from 'undici'
```

When `http_proxy` and `https_proxy` are both set, `http_proxy` is used for HTTP
requests and `https_proxy` is used for HTTPS requests. If only `http_proxy` is
set, it is used for both HTTP and HTTPS requests. If only `https_proxy` is set,
it is used only for HTTPS requests.

`no_proxy` is a comma- or space-separated list of hosts that must not be
proxied. Each entry may include a leading dot or `*.` wildcard (for example
`.example.com`) to match subdomains, and an optional `:port` suffix to restrict
the match to a specific port. A request bypasses the proxy when its host equals
an entry or is a subdomain of one. Setting `no_proxy` to `*` bypasses the proxy
for every request.

The uppercase variants `HTTP_PROXY`, `HTTPS_PROXY`, and `NO_PROXY` are also
honored. When both the lowercase and uppercase forms of a variable are set, the
lowercase form takes precedence and the uppercase form is ignored. The `no_proxy`
value is re-read from the environment on demand, so changes to it after the agent
is created take effect on subsequent requests; this does not apply when `noProxy`
is passed as an option.

## Class: `EnvHttpProxyAgent`

<!-- YAML
added: v6.14.0
-->

* Extends: {Dispatcher}

### `new EnvHttpProxyAgent([options])`

<!-- YAML
added: v6.14.0
-->

* `options` {EnvHttpProxyAgentOptions} Extends the [`ProxyAgent`][] options
  (except `uri`). (optional)
  * `httpProxy` {string} When set, overrides the `http_proxy` and `HTTP_PROXY`
    environment variables. (optional)
  * `httpsProxy` {string} When set, overrides the `https_proxy` and
    `HTTPS_PROXY` environment variables. (optional)
  * `noProxy` {string} When set, overrides the `no_proxy` and `NO_PROXY`
    environment variables. (optional)
* Returns: {EnvHttpProxyAgent}

Creates a new `EnvHttpProxyAgent`. Any options other than `httpProxy`,
`httpsProxy`, and `noProxy` are forwarded to the underlying [`Agent`][] and
[`ProxyAgent`][] instances. Constructing the agent only configures it; it does
not affect any request until it is registered as a dispatcher, either globally
with `setGlobalDispatcher()` or per-request through the `dispatcher` option.

```mjs
import { EnvHttpProxyAgent } from 'undici'

const envHttpProxyAgent = new EnvHttpProxyAgent()
// or, overriding the environment variables explicitly:
const envHttpProxyAgentWithOpts = new EnvHttpProxyAgent({
  httpProxy: 'my.proxy.server:8080',
  httpsProxy: 'my.proxy.server:8443',
  noProxy: 'localhost'
})
```

#### Example - Proxied `fetch()` with the global dispatcher

```mjs
import { setGlobalDispatcher, fetch, EnvHttpProxyAgent } from 'undici'

const envHttpProxyAgent = new EnvHttpProxyAgent()
setGlobalDispatcher(envHttpProxyAgent)

const response = await fetch('http://localhost:3000/foo')

console.log('response received', response.status) // response received 200

const data = await response.json() // data { foo: 'bar' }
```

#### Example - Proxied `request()` with the global dispatcher

```mjs
import { setGlobalDispatcher, request, EnvHttpProxyAgent } from 'undici'

const envHttpProxyAgent = new EnvHttpProxyAgent()
setGlobalDispatcher(envHttpProxyAgent)

const { statusCode, body } = await request('http://localhost:3000/foo')

console.log('response received', statusCode) // response received 200

for await (const data of body) {
  console.log('data', data.toString('utf8')) // data foo
}
```

#### Example - Proxied `request()` with a local dispatcher

```mjs
import { EnvHttpProxyAgent, request } from 'undici'

const envHttpProxyAgent = new EnvHttpProxyAgent()

const { statusCode, body } = await request('http://localhost:3000/foo', {
  dispatcher: envHttpProxyAgent
})

console.log('response received', statusCode) // response received 200

for await (const data of body) {
  console.log('data', data.toString('utf8')) // data foo
}
```

#### Example - Proxied `fetch()` with a local dispatcher

```mjs
import { EnvHttpProxyAgent, fetch } from 'undici'

const envHttpProxyAgent = new EnvHttpProxyAgent()

const response = await fetch('http://localhost:3000/foo', {
  dispatcher: envHttpProxyAgent
})

console.log('response received', response.status) // response received 200

const data = await response.json() // data { foo: 'bar' }
```

### `envHttpProxyAgent.dispatch(options, handler)`

<!-- YAML
added: v6.14.0
-->

* `options` {AgentDispatchOptions}
  * `origin` {string|URL}
* `handler` {DispatchHandler}
* Returns: {boolean}

Dispatches a request through the proxy selected for `options.origin`. The origin's
protocol, host, and port are compared against the `no_proxy` configuration: when
the host should be proxied, the request is routed to the HTTPS proxy agent for
`https:` origins or the HTTP proxy agent otherwise; when the host is excluded by
`no_proxy`, the request is sent directly through a non-proxying [`Agent`][].

This method is not normally called directly. Use the higher-level
[`Dispatcher`][] methods such as [`dispatcher.request()`][] or the top-level
`request()` and `fetch()` helpers instead.

[`Agent`]: Agent.md#class-agent
[`Dispatcher`]: Dispatcher.md#class-dispatcher
[`ProxyAgent`]: ProxyAgent.md#class-proxyagent
[`dispatcher.request()`]: Dispatcher.md#dispatcherrequestoptions-callback
