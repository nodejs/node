# make-fetch-happen [![npm version](https://img.shields.io/npm/v/make-fetch-happen.svg)](https://npm.im/make-fetch-happen) [![license](https://img.shields.io/npm/l/make-fetch-happen.svg)](https://npm.im/make-fetch-happen) [![Travis](https://img.shields.io/travis/zkat/make-fetch-happen.svg)](https://travis-ci.org/zkat/make-fetch-happen) [![AppVeyor](https://ci.appveyor.com/api/projects/status/github/zkat/make-fetch-happen?svg=true)](https://ci.appveyor.com/project/zkat/make-fetch-happen) [![Coverage Status](https://coveralls.io/repos/github/zkat/make-fetch-happen/badge.svg?branch=latest)](https://coveralls.io/github/zkat/make-fetch-happen?branch=latest)


[`make-fetch-happen`](https://github.com/zkat/make-fetch-happen) is a Node.js
library that wraps [`node-fetch`](https://npm.im/node-fetch) with additional
features it doesn't intend to include, including HTTP Cache support, request
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
  * [`node-fetch` options](#node-fetch-options)
  * [`make-fetch-happen` options](#extra-options)
    * [`opts.cacheManager`](#opts-cache-manager)
    * [`opts.cache`](#opts-cache)
    * [`opts.proxy`](#opts-proxy)
    * [`opts.noProxy`](#opts-no-proxy)
    * [`opts.ca, opts.cert, opts.key`](#https-opts)
    * [`opts.maxSockets`](#opts-max-sockets)
    * [`opts.retry`](#opts-retry)
    * [`opts.integrity`](#opts-integrity)
* [Message From Our Sponsors](#wow)

### Example

```javascript
const fetch = require('make-fetch-happen').defaults({
  cacheManager: './my-cache' // path where cache will be written (and read)
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

* Builds around [`node-fetch`](https://npm.im/node-fetch) for the core [`fetch` API](https://fetch.spec.whatwg.org) implementation
* Request pooling out of the box
* Quite fast, really
* Automatic HTTP-semantics-aware request retries
* Cache-fallback automatic "offline mode"
* Proxy support (http, https, socks, socks4, socks5)
* Built-in request caching following full HTTP caching rules (`Cache-Control`, `ETag`, `304`s, cache fallback on error, etc).
* Customize cache storage with any [Cache API](https://developer.mozilla.org/en-US/docs/Web/API/Cache)-compliant `Cache` instance. Cache to Redis!
* Node.js Stream support
* Transparent gzip and deflate support
* [Subresource Integrity](https://developer.mozilla.org/en-US/docs/Web/Security/Subresource_Integrity) support
* Literally punches nazis
* (PENDING) Range request caching and resuming

### Contributing

The make-fetch-happen team enthusiastically welcomes contributions and project participation! There's a bunch of things you can do if you want to contribute! The [Contributor Guide](CONTRIBUTING.md) has all the information you need for everything from reporting bugs to contributing entire new features. Please don't hesitate to jump in if you'd like to, or even ask us questions if something isn't clear.

All participants and maintainers in this project are expected to follow [Code of Conduct](CODE_OF_CONDUCT.md), and just generally be excellent to each other.

Please refer to the [Changelog](CHANGELOG.md) for project history details, too.

Happy hacking!

### API

#### <a name="fetch"></a> `> fetch(uriOrRequest, [opts]) -> Promise<Response>`

This function implements most of the [`fetch` API](https://developer.mozilla.org/en-US/docs/Web/API/WindowOrWorkerGlobalScope/fetch): given a `uri` string or a `Request` instance, it will fire off an http request and return a Promise containing the relevant response.

If `opts` is provided, the [`node-fetch`-specific options](#node-fetch-options) will be passed to that library. There are also [additional options](#extra-options) specific to make-fetch-happen that add various features, such as HTTP caching, integrity verification, proxy support, and more.

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
  cacheManager: './my-local-cache'
})

fetch('https://registry.npmjs.org/make-fetch-happen') // will always use the cache
```

#### <a name="node-fetch-options"></a> `> node-fetch options`

The following options for `node-fetch` are used as-is:

* method
* body
* redirect
* follow
* timeout
* compress
* size

These other options are modified or augmented by make-fetch-happen:

* headers - Default `User-Agent` set to make-fetch happen. `Connection` is set to `keep-alive` or `close` automatically depending on `opts.agent`.
* agent
  * If agent is null, an http or https Agent will be automatically used. By default, these will be `http.globalAgent` and `https.globalAgent`.
  * If [`opts.proxy`](#opts-proxy) is provided and `opts.agent` is null, the agent will be set to an appropriate proxy-handling agent.
  * If `opts.agent` is an object, it will be used as the request-pooling agent argument for this request.
  * If `opts.agent` is `false`, it will be passed as-is to the underlying request library. This causes a new Agent to be spawned for every request.

For more details, see [the documentation for `node-fetch` itself](https://github.com/bitinn/node-fetch#options).

#### <a name="extra-options"></a> `> make-fetch-happen options`

make-fetch-happen augments the `node-fetch` API with additional features available through extra options. The following extra options are available:

* [`opts.cacheManager`](#opts-cache-manager) - Cache target to read/write
* [`opts.cache`](#opts-cache) - `fetch` cache mode. Controls cache *behavior*.
* [`opts.proxy`](#opts-proxy) - Proxy agent
* [`opts.noProxy`](#opts-no-proxy) - Domain segments to disable proxying for.
* [`opts.ca, opts.cert, opts.key, opts.strictSSL`](#https-opts)
* [`opts.localAddress`](#opts-local-address)
* [`opts.maxSockets`](#opts-max-sockets)
* [`opts.retry`](#opts-retry) - Request retry settings
* [`opts.integrity`](#opts-integrity) - [Subresource Integrity](https://developer.mozilla.org/en-US/docs/Web/Security/Subresource_Integrity) metadata.

#### <a name="opts-cache-manager"></a> `> opts.cacheManager`

Either a `String` or a `Cache`. If the former, it will be assumed to be a `Path` to be used as the cache root for [`cacache`](https://npm.im/cacache).

If an object is provided, it will be assumed to be a compliant [`Cache` instance](https://developer.mozilla.org/en-US/docs/Web/API/Cache). Only `Cache.match()`, `Cache.put()`, and `Cache.delete()` are required. Options objects will not be passed in to `match()` or `delete()`.

By implementing this API, you can customize the storage backend for make-fetch-happen itself -- for example, you could implement a cache that uses `redis` for caching, or simply keeps everything in memory. Most of the caching logic exists entirely on the make-fetch-happen side, so the only thing you need to worry about is reading, writing, and deleting, as well as making sure `fetch.Response` objects are what gets returned.

You can refer to `cache.js` in the make-fetch-happen source code for a reference implementation.

**NOTE**: Requests will not be cached unless their response bodies are consumed. You will need to use one of the `res.json()`, `res.buffer()`, etc methods on the response, or drain the `res.body` stream, in order for it to be written.

The default cache manager also adds the following headers to cached responses:

* `X-Local-Cache`: Path to the cache the content was found in
* `X-Local-Cache-Key`: Unique cache entry key for this response
* `X-Local-Cache-Hash`: Specific integrity hash for the cached entry
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
  cacheManager: './my-local-cache'
}) // -> 200-level response will be written to disk

fetch('https://npm.im/cacache', {
  cacheManager: new MyCustomRedisCache(process.env.PORT)
}) // -> 200-level response will be written to redis
```

A possible (minimal) implementation for `MyCustomRedisCache`:

```javascript
const bluebird = require('bluebird')
const redis = require("redis")
bluebird.promisifyAll(redis.RedisClient.prototype)
class MyCustomRedisCache {
  constructor (opts) {
    this.redis = redis.createClient(opts)
  }
  match (req) {
    return this.redis.getAsync(req.url).then(res => {
      if (res) {
        const parsed = JSON.parse(res)
        return new fetch.Response(parsed.body, {
          url: req.url,
          headers: parsed.headers,
          status: 200
        })
      }
    })
  }
  put (req, res) {
    return res.buffer().then(body => {
      return this.redis.setAsync(req.url, JSON.stringify({
        body: body,
        headers: res.headers.raw()
      }))
    }).then(() => {
      // return the response itself
      return res
    })
  }
  'delete' (req) {
    return this.redis.unlinkAsync(req.url)
  }
}
```

#### <a name="opts-cache"></a> `> opts.cache`

This option follows the standard `fetch` API cache option. This option will do nothing if [`opts.cacheManager`](#opts-cache-manager) is null. The following values are accepted (as strings):

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
  cacheManager: './my-cache'
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

#### <a name="opts-proxy"></a> `> opts.proxy`

A string or `url.parse`-d URI to proxy through. Different Proxy handlers will be
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
* `ECONNRESET` currently has no support for restarting. It will eventually be supported but requires a bit more juggling due to streaming.

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

#### <a name="opts-integrity"></a> `> opts.integrity`

Matches the response body against the given [Subresource Integrity](https://developer.mozilla.org/en-US/docs/Web/Security/Subresource_Integrity) metadata. If verification fails, the request will fail with an `EINTEGRITY` error.

`integrity` may either be a string or an [`ssri`](https://npm.im/ssri) `Integrity`-like.

##### Example

```javascript
fetch('https://registry.npmjs.org/make-fetch-happen/-/make-fetch-happen-1.0.0.tgz', {
  integrity: 'sha1-o47j7zAYnedYFn1dF/fR9OV3z8Q='
}) // -> ok

fetch('https://malicious-registry.org/make-fetch-happen/-/make-fetch-happen-1.0.0.tgz'. {
  integrity: 'sha1-o47j7zAYnedYFn1dF/fR9OV3z8Q='
}) // Error: EINTEGRITY
```

### <a name="wow"></a> Message From Our Sponsors

![](stop.gif)

![](happening.gif)
