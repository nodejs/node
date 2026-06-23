## @npmcli/agent

A pair of Agent implementations for nodejs that provide consistent keep-alives, granular timeouts, dns caching, and proxy support.

### Usage

```js
const { getAgent, HttpAgent } = require('@npmcli/agent')
const fetch = require('minipass-fetch')

const main = async () => {
    // if you know what agent you need, you can create one directly
    const agent = new HttpAgent(agentOptions)
    // or you can use the getAgent helper, it will determine and create an Agent
    // instance for you as well as reuse that agent for new requests as appropriate
    const agent = getAgent('https://registry.npmjs.org/npm', agentOptions)
    // minipass-fetch is just an example, this will work for any http client that
    // supports node's Agents
    const res = await fetch('https://registry.npmjs.org/npm', { agent })
}

main()
```

### Options

All options supported by the node Agent implementations are supported here, see [the docs](https://nodejs.org/api/http.html#new-agentoptions) for those.

Options that have been added by this module include:

- `family`: what tcp family to use, can be `4` for IPv4, `6` for IPv6 or `0` for both.
- `proxy`: a URL to a supported proxy, currently supports `HTTP CONNECT` based http/https proxies as well as socks4 and 5.
- `dns`: configuration for the built-in dns cache
    - `ttl`: how long (in milliseconds) to keep cached dns entries, defaults to `5 * 60 * 100 (5 minutes)`
    - `lookup`: optional function to override how dns lookups are performed, defaults to `require('dns').lookup`
- `timeouts`: a set of granular timeouts, all default to `0`
    - `connection`: time between initiating connection and actually connecting
    - `idle`: time between data packets (if a top level `timeout` is provided, it will be copied here)
    - `response`: time between sending a request and receiving a response
    - `transfer`: time between starting to receive a request and consuming the response fully
