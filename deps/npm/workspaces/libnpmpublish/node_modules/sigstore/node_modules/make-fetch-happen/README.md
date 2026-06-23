# make-fetch-happen
[![npm version](https://img.shields.io/npm/v/make-fetch-happen.svg)](https://npm.im/make-fetch-happen) [![license](https://img.shields.io/npm/l/make-fetch-happen.svg)](https://npm.im/make-fetch-happen) [![Coverage Status](https://coveralls.io/repos/github/npm/make-fetch-happen/badge.svg?branch=latest)](https://coveralls.io/github/npm/make-fetch-happen?branch=latest)

[`make-fetch-happen`](https://github.com/npm/make-fetch-happen) is a Node.js
library that wraps [`minipass-fetch`](https://github.com/npm/minipass-fetch) with additional
features [`minipass-fetch`](https://github.com/npm/minipass-fetch) doesn't intend to include, including HTTP Cache support, request
pooling, proxies, retries, [and more](#features)!

## Install

`$ npm install --save make-fetch-happen`

## Table of Contents

* [Example](#example)
* [Features](#features)
* [Contributing](#contributing)
* [API](#api)
  * [`fetch`](#fetch)
  * [`fetch.defaults`](#fetch-defaults)
  * [`minipass-fetch` options](#minipass-fetch-options)
  * [`make-fetch-happen` options](#extra-options)
    * [`opts.cachePath`](#opts-cache-path)
    * [`opts.cache`](#opts-cache)
    * [`opts.cacheAdditionalHeaders`](#opts-cache-additional-headers)
    * [`opts.proxy`](#opts-proxy)
    * [`opts.noProxy`](#opts-no-proxy)
    * [`opts.ca, opts.cert, opts.key`](#https-opts)
    * [`opts.maxSockets`](#opts-max-sockets)
    * [`opts.retry`](#opts-retry)
    * [`opts.onRetry`](#opts-onretry)
    * [`opts.integrity`](#opts-integrity)
    * [`opts.dns`](#opts-dns)
* [Message From Our Sponsors](#wow)

### Example

```javascript
const fetch = require('make-fetch-happen').defaults({
  cachePath: './my-cache' // path where cache will be written (and read)
})

fetch('https://registry.npmjs.org/make-fetch-happen').then(res => {
  return res.json() // download the body as JSON
}).then(body => {
  console.log(`got ${body.name} from web`)
  return fetch('https://registry.npmjs.org/make-fetch-happen', {
    cache: 'no-cache' // forces a conditional request
  })
}).then(res => {
  console.log(res.status) // 304! cache validated!
  return res.json().then(body => {
    console.log(`got ${body.name} from cache`)
  })
})
```

### Features

* Builds around [`minipass-fetch`](https://npm.im/minipass-fetch) for the core [`fetch` API](https://fetch.spec.whatwg.org) implementation
* Request pooling out of the box
* Quite fast, really
* Automatic HTTP-semantics-aware request retries
* Cache-fallback automatic "offline mode"
* Built-in request caching following full HTTP caching rules (`Cache-Control`, `ETag`, `304`s, cache fallback on error, etc).
* Node.js Stream support
* Transparent gzip and deflate support
* [Subresource Integrity](https://developer.mozilla.org/en-US/docs/Web/Security/Subresource_Integrity) support
* Proxy support (http, https, socks, socks4, socks5. via [`@npmcli/agent`](https://npm.im/@npmcli/agent))
* DNS cache (via ([`@npmcli/agent`](https://npm.im/@npmcli/agent))

#### <a name="fetch"></a> `> fetch(uriOrRequest, [opts]) -> Promise<Response>`

This function implements most of the [`fetch` API](https://developer.mozilla.org/en-US/docs/Web/API/WindowOrWorkerGlobalScope/fetch): given a `uri` string or a `Request` instance, it will fire off an http request and return a Promise containing the relevant response.

If `opts` is provided, the [`minipass-fetch`-specific options](#minipass-fetch-options) will be passed to that library. There are also [additional options](#extra-options) specific to make-fetch-happen that add various features, such as HTTP caching, integrity verification, proxy support, and more.

##### Example

```javascript
fetch('https://google.com').then(res => res.buffer())
```

#### <a name="fetch-defaults"></a> `> fetch.defaults([defaultUrl], [defaultOpts])`

Returns a new `fetch` function that will call `make-fetch-happen` using `defaultUrl` and `defaultOpts` as default values to any calls.

A defaulted `fetch` will also have a `.defaults()` method, so they can be chained.

##### Example

```javascript
const fetch = require('make-fetch-happen').defaults({
  cachePath: './my-local-cache'
})

fetch('https://registry.npmjs.org/make-fetch-happen') // will always use the cache
```

#### <a name="minipass-fetch-options"></a> `> minipass-fetch options`

The following options for `minipass-fetch` are used as-is:

* method
* body
* redirect
* follow
* timeout
* compress
* size

These other options are modified or augmented by make-fetch-happen:

* headers - Default `User-Agent` set to make-fetch happen. `Connection` is set to `keep-alive` or `close` automatically depending on `opts.agent`.

For more details, see [the documentation for `minipass-fetch` itself](https://github.com/npm/minipass-fetch#options).

#### <a name="extra-options"></a> `> make-fetch-happen options`

make-fetch-happen augments the `minipass-fetch` API with additional features available through extra options. The following extra options are available:

* [`opts.cachePath`](#opts-cache-path) - Cache target to read/write
* [`opts.cache`](#opts-cache) - `fetch` cache mode. Controls cache *behavior*.
* [`opts.cacheAdditionalHeaders`](#opts-cache-additional-headers) - Store additional headers in the cache
* [`opts.proxy`](#opts-proxy) - Proxy agent
* [`opts.noProxy`](#opts-no-proxy) - Domain segments to disable proxying for.
* [`opts.ca, opts.cert, opts.key, opts.strictSSL`](#https-opts)
* [`opts.localAddress`](#opts-local-address)
* [`opts.maxSockets`](#opts-max-sockets)
* [`opts.retry`](#opts-retry) - Request retry settings
* [`opts.onRetry`](#opts-onretry) - a function called whenever a retry is attempted
* [`opts.integrity`](#opts-integrity) - [Subresource Integrity](https://developer.mozilla.org/en-US/docs/Web/Security/Subresource_Integrity) metadata.
* [`opts.dns`](#opts-dns) - DNS cache options
* [`opts.agent`](#opts-agent) - http/https/proxy/socks agent options.  See [`@npmcli/agent`](https://npm.im/@npmcli/agent) for more info.

#### <a name="opts-cache-path"></a> `> opts.cachePath`

A string `Path` to be used as the cache root for [`cacache`](https://npm.im/cacache).

**NOTE**: Requests will not be cached unless their response bodies are consumed. You will need to use one of the `res.json()`, `res.buffer()`, etc methods on the response, or drain the `res.body` stream, in order for it to be written.

The default cache manager also adds the following headers to cached responses:

* `X-Local-Cache`: Path to the cache the content was found in
* `X-Local-Cache-Key`: Unique cache entry key for this response
* `X-Local-Cache-Mode`: Always `stream` to indicate how the response was read from cacache
* `X-Local-Cache-Hash`: Specific integrity hash for the cached entry
* `X-Local-Cache-Status`: One of `miss`, `hit`, `stale`, `revalidated`, `updated`, or `skip` to signal how the response was created
* `X-Local-Cache-Time`: UTCString of the cache insertion time for the entry

Using [`cacache`](https://npm.im/cacache), a call like this may be used to
manually fetch the cached entry:

```javascript
const h = response.headers
cacache.get(h.get('x-local-cache'), h.get('x-local-cache-key'))

// grab content only, directly:
cacache.get.byDigest(h.get('x-local-cache'), h.get('x-local-cache-hash'))
```

##### Example

```javascript
fetch('https://registry.npmjs.org/make-fetch-happen', {
  cachePath: './my-local-cache'
}) // -> 200-level response will be written to disk
```

#### <a name="opts-cache"></a> `> opts.cache`

This option follows the standard `fetch` API cache option. This option will do nothing if [`opts.cachePath`](#opts-cache-path) is null. The following values are accepted (as strings):

* `default` - Fetch will inspect the HTTP cache on the way to the network. If there is a fresh response it will be used. If there is a stale response a conditional request will be created, and a normal request otherwise. It then updates the HTTP cache with the response. If the revalidation request fails (for example, on a 500 or if you're offline), the stale response will be returned.
* `no-store` - Fetch behaves as if there is no HTTP cache at all.
* `reload` - Fetch behaves as if there is no HTTP cache on the way to the network. Ergo, it creates a normal request and updates the HTTP cache with the response.
* `no-cache` - Fetch creates a conditional request if there is a response in the HTTP cache and a normal request otherwise. It then updates the HTTP cache with the response.
* `force-cache` - Fetch uses any response in the HTTP cache matching the request, not paying attention to staleness. If there was no response, it creates a normal request and updates the HTTP cache with the response.
* `only-if-cached` - Fetch uses any response in the HTTP cache matching the request, not paying attention to staleness. If there was no response, it returns a network error. (Can only be used when request’s mode is "same-origin". Any cached redirects will be followed assuming request’s redirect mode is "follow" and the redirects do not violate request’s mode.)

(Note: option descriptions are taken from https://fetch.spec.whatwg.org/#http-network-or-cache-fetch)

##### Example

```javascript
const fetch = require('make-fetch-happen').defaults({
  cachePath: './my-cache'
})

// Will error with ENOTCACHED if we haven't already cached this url
fetch('https://registry.npmjs.org/make-fetch-happen', {
  cache: 'only-if-cached'
})

// Will refresh any local content and cache the new response
fetch('https://registry.npmjs.org/make-fetch-happen', {
  cache: 'reload'
})

// Will use any local data, even if stale. Otherwise, will hit network.
fetch('https://registry.npmjs.org/make-fetch-happen', {
  cache: 'force-cache'
})
```

#### <a name="opts-cache-additional-headers"></a> `> opts.cacheAdditionalHeaders`

The following headers are always stored in the cache when present:

- `cache-control`
- `content-encoding`
- `content-language`
- `content-type`
- `date`
- `etag`
- `expires`
- `last-modified`
- `link`
- `location`
- `pragma`
- `vary`

This option allows a user to store additional custom headers in the cache.


##### Example

```javascript
fetch('https://registry.npmjs.org/make-fetch-happen', {
  cacheAdditionalHeaders: ['my-custom-header'],
})
```

#### <a name="opts-proxy"></a> `> opts.proxy`

A string or `new url.URL()`-d URI to proxy through. Different Proxy handlers will be
used depending on the proxy's protocol.

Additionally, `process.env.HTTP_PROXY`, `process.env.HTTPS_PROXY`, and
`process.env.PROXY` are used if present and no `opts.proxy` value is provided.

(Pending) `process.env.NO_PROXY` may also be configured to skip proxying requests for all, or specific domains.

##### Example

```javascript
fetch('https://registry.npmjs.org/make-fetch-happen', {
  proxy: 'https://corporate.yourcompany.proxy:4445'
})

fetch('https://registry.npmjs.org/make-fetch-happen', {
  proxy: {
    protocol: 'https:',
    hostname: 'corporate.yourcompany.proxy',
    port: 4445
  }
})
```

#### <a name="opts-no-proxy"></a> `> opts.noProxy`

If present, should be a comma-separated string or an array of domain extensions
that a proxy should _not_ be used for.

This option may also be provided through `process.env.NO_PROXY`.

#### <a name="https-opts"></a> `> opts.ca, opts.cert, opts.key, opts.strictSSL`

These values are passed in directly to the HTTPS agent and will be used for both
proxied and unproxied outgoing HTTPS requests. They mostly correspond to the
same options the `https` module accepts, which will be themselves passed to
`tls.connect()`. `opts.strictSSL` corresponds to `rejectUnauthorized`.

#### <a name="opts-local-address"></a> `> opts.localAddress`

Passed directly to `http` and `https` request calls. Determines the local
address to bind to.

#### <a name="opts-max-sockets"></a> `> opts.maxSockets`

Default: 15

Maximum number of active concurrent sockets to use for the underlying
Http/Https/Proxy agents. This setting applies once per spawned agent.

15 is probably a _pretty good value_ for most use-cases, and balances speed
with, uh, not knocking out people's routers. 🤓

#### <a name="opts-retry"></a> `> opts.retry`

An object that can be used to tune request retry settings. Retries will only be attempted on the following conditions:

* Request method is NOT `POST` AND
* Request status is one of: `408`, `420`, `429`, or any status in the 500-range. OR
* Request errored with `ECONNRESET`, `ECONNREFUSED`, `EADDRINUSE`, `ETIMEDOUT`, or the `fetch` error `request-timeout`.

The following are worth noting as explicitly not retried:

* `getaddrinfo ENOTFOUND` and will be assumed to be either an unreachable domain or the user will be assumed offline. If a response is cached, it will be returned immediately.

If `opts.retry` is `false`, it is equivalent to `{retries: 0}`

If `opts.retry` is a number, it is equivalent to `{retries: num}`

The following retry options are available if you want more control over it:

* retries
* factor
* minTimeout
* maxTimeout
* randomize

For details on what each of these do, refer to the [`retry`](https://npm.im/retry) documentation.

##### Example

```javascript
fetch('https://flaky.site.com', {
  retry: {
    retries: 10,
    randomize: true
  }
})

fetch('http://reliable.site.com', {
  retry: false
})

fetch('http://one-more.site.com', {
  retry: 3
})
```

#### <a name="opts-onretry"></a> `> opts.onRetry`

A function called with the response or error which caused the retry whenever one is attempted.

##### Example

```javascript
fetch('https://flaky.site.com', {
  onRetry(cause) {
    console.log('we will retry because of', cause)
  }
})
```

#### <a name="opts-integrity"></a> `> opts.integrity`

Matches the response body against the given [Subresource Integrity](https://developer.mozilla.org/en-US/docs/Web/Security/Subresource_Integrity) metadata. If verification fails, the request will fail with an `EINTEGRITY` error.

`integrity` may either be a string or an [`ssri`](https://npm.im/ssri) `Integrity`-like.

##### Example

```javascript
fetch('https://registry.npmjs.org/make-fetch-happen/-/make-fetch-happen-1.0.0.tgz', {
  integrity: 'sha1-o47j7zAYnedYFn1dF/fR9OV3z8Q='
}) // -> ok

fetch('https://malicious-registry.org/make-fetch-happen/-/make-fetch-happen-1.0.0.tgz', {
  integrity: 'sha1-o47j7zAYnedYFn1dF/fR9OV3z8Q='
}) // Error: EINTEGRITY
```
