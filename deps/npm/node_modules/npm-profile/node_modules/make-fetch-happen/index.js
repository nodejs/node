'use strict'

let Cache
const url = require('url')
const CachePolicy = require('http-cache-semantics')
const fetch = require('node-fetch-npm')
const pkg = require('./package.json')
const retry = require('promise-retry')
let ssri
const Stream = require('stream')
const getAgent = require('./agent')
const setWarning = require('./warning')

const isURL = /^https?:/
const USER_AGENT = `${pkg.name}/${pkg.version} (+https://npm.im/${pkg.name})`

const RETRY_ERRORS = [
  'ECONNRESET', // remote socket closed on us
  'ECONNREFUSED', // remote host refused to open connection
  'EADDRINUSE', // failed to bind to a local port (proxy?)
  'ETIMEDOUT' // someone in the transaction is WAY TOO SLOW
  // Known codes we do NOT retry on:
  // ENOTFOUND (getaddrinfo failure. Either bad hostname, or offline)
]

const RETRY_TYPES = [
  'request-timeout'
]

// https://fetch.spec.whatwg.org/#http-network-or-cache-fetch
module.exports = cachingFetch
cachingFetch.defaults = function (_uri, _opts) {
  const fetch = this
  if (typeof _uri === 'object') {
    _opts = _uri
    _uri = null
  }

  function defaultedFetch (uri, opts) {
    const finalOpts = Object.assign({}, _opts || {}, opts || {})
    return fetch(uri || _uri, finalOpts)
  }

  defaultedFetch.defaults = fetch.defaults
  defaultedFetch.delete = fetch.delete
  return defaultedFetch
}

cachingFetch.delete = cacheDelete
function cacheDelete (uri, opts) {
  opts = configureOptions(opts)
  if (opts.cacheManager) {
    const req = new fetch.Request(uri, {
      method: opts.method,
      headers: opts.headers
    })
    return opts.cacheManager.delete(req, opts)
  }
}

function initializeCache (opts) {
  if (typeof opts.cacheManager === 'string') {
    if (!Cache) {
      // Default cacache-based cache
      Cache = require('./cache')
    }

    opts.cacheManager = new Cache(opts.cacheManager, opts)
  }

  opts.cache = opts.cache || 'default'

  if (opts.cache === 'default' && isHeaderConditional(opts.headers)) {
    // If header list contains `If-Modified-Since`, `If-None-Match`,
    // `If-Unmodified-Since`, `If-Match`, or `If-Range`, fetch will set cache
    // mode to "no-store" if it is "default".
    opts.cache = 'no-store'
  }
}

function configureOptions (_opts) {
  const opts = Object.assign({}, _opts || {})
  opts.method = (opts.method || 'GET').toUpperCase()

  if (opts.retry && typeof opts.retry === 'number') {
    opts.retry = { retries: opts.retry }
  }

  if (opts.retry === false) {
    opts.retry = { retries: 0 }
  }

  if (opts.cacheManager) {
    initializeCache(opts)
  }

  return opts
}

function initializeSsri () {
  if (!ssri) {
    ssri = require('ssri')
  }
}

function cachingFetch (uri, _opts) {
  const opts = configureOptions(_opts)

  if (opts.integrity) {
    initializeSsri()
  }

  const isCachable = (opts.method === 'GET' || opts.method === 'HEAD') &&
    opts.cacheManager &&
    opts.cache !== 'no-store' &&
    opts.cache !== 'reload'

  if (isCachable) {
    const req = new fetch.Request(uri, {
      method: opts.method,
      headers: opts.headers
    })

    return opts.cacheManager.match(req, opts).then(res => {
      if (res) {
        const warningCode = (res.headers.get('Warning') || '').match(/^\d+/)
        if (warningCode && +warningCode >= 100 && +warningCode < 200) {
          // https://tools.ietf.org/html/rfc7234#section-4.3.4
          //
          // If a stored response is selected for update, the cache MUST:
          //
          // * delete any Warning header fields in the stored response with
          //   warn-code 1xx (see Section 5.5);
          //
          // * retain any Warning header fields in the stored response with
          //   warn-code 2xx;
          //
          res.headers.delete('Warning')
        }

        if (opts.cache === 'default' && !isStale(req, res)) {
          return res
        }

        if (opts.cache === 'default' || opts.cache === 'no-cache') {
          return conditionalFetch(req, res, opts)
        }

        if (opts.cache === 'force-cache' || opts.cache === 'only-if-cached') {
          //   112 Disconnected operation
          // SHOULD be included if the cache is intentionally disconnected from
          // the rest of the network for a period of time.
          // (https://tools.ietf.org/html/rfc2616#section-14.46)
          setWarning(res, 112, 'Disconnected operation')
          return res
        }
      }

      if (!res && opts.cache === 'only-if-cached') {
        const errorMsg = `request to ${
          uri
        } failed: cache mode is 'only-if-cached' but no cached response available.`

        const err = new Error(errorMsg)
        err.code = 'ENOTCACHED'
        throw err
      }

      // Missing cache entry, or mode is default (if stale), reload, no-store
      return remoteFetch(req.url, opts)
    })
  }

  return remoteFetch(uri, opts)
}

function iterableToObject (iter) {
  const obj = {}
  for (let k of iter.keys()) {
    obj[k] = iter.get(k)
  }
  return obj
}

function makePolicy (req, res) {
  const _req = {
    url: req.url,
    method: req.method,
    headers: iterableToObject(req.headers)
  }
  const _res = {
    status: res.status,
    headers: iterableToObject(res.headers)
  }

  return new CachePolicy(_req, _res, { shared: false })
}

// https://tools.ietf.org/html/rfc7234#section-4.2
function isStale (req, res) {
  if (!res) {
    return null
  }

  const _req = {
    url: req.url,
    method: req.method,
    headers: iterableToObject(req.headers)
  }

  const policy = makePolicy(req, res)

  const responseTime = res.headers.get('x-local-cache-time') ||
    res.headers.get('date') ||
    0

  policy._responseTime = new Date(responseTime)

  const bool = !policy.satisfiesWithoutRevalidation(_req)
  return bool
}

function mustRevalidate (res) {
  return (res.headers.get('cache-control') || '').match(/must-revalidate/i)
}

function conditionalFetch (req, cachedRes, opts) {
  const _req = {
    url: req.url,
    method: req.method,
    headers: Object.assign({}, opts.headers || {})
  }

  const policy = makePolicy(req, cachedRes)
  opts.headers = policy.revalidationHeaders(_req)

  return remoteFetch(req.url, opts)
    .then(condRes => {
      const revalidatedPolicy = policy.revalidatedPolicy(_req, {
        status: condRes.status,
        headers: iterableToObject(condRes.headers)
      })

      if (condRes.status >= 500 && !mustRevalidate(cachedRes)) {
        //   111 Revalidation failed
        // MUST be included if a cache returns a stale response because an
        // attempt to revalidate the response failed, due to an inability to
        // reach the server.
        // (https://tools.ietf.org/html/rfc2616#section-14.46)
        setWarning(cachedRes, 111, 'Revalidation failed')
        return cachedRes
      }

      if (condRes.status === 304) { // 304 Not Modified
        condRes.body = cachedRes.body
        return opts.cacheManager.put(req, condRes, opts)
          .then(newRes => {
            newRes.headers = new fetch.Headers(revalidatedPolicy.policy.responseHeaders())
            return newRes
          })
      }

      return condRes
    })
    .then(res => res)
    .catch(err => {
      if (mustRevalidate(cachedRes)) {
        throw err
      } else {
        //   111 Revalidation failed
        // MUST be included if a cache returns a stale response because an
        // attempt to revalidate the response failed, due to an inability to
        // reach the server.
        // (https://tools.ietf.org/html/rfc2616#section-14.46)
        setWarning(cachedRes, 111, 'Revalidation failed')
        //   199 Miscellaneous warning
        // The warning text MAY include arbitrary information to be presented to
        // a human user, or logged. A system receiving this warning MUST NOT take
        // any automated action, besides presenting the warning to the user.
        // (https://tools.ietf.org/html/rfc2616#section-14.46)
        setWarning(
          cachedRes,
          199,
          `Miscellaneous Warning ${err.code}: ${err.message}`
        )

        return cachedRes
      }
    })
}

function remoteFetchHandleIntegrity (res, integrity) {
  const oldBod = res.body
  const newBod = ssri.integrityStream({
    integrity
  })
  oldBod.pipe(newBod)
  res.body = newBod
  oldBod.once('error', err => {
    newBod.emit('error', err)
  })
  newBod.once('error', err => {
    oldBod.emit('error', err)
  })
}

function remoteFetch (uri, opts) {
  const agent = getAgent(uri, opts)
  const headers = Object.assign({
    'connection': agent ? 'keep-alive' : 'close',
    'user-agent': USER_AGENT
  }, opts.headers || {})

  const reqOpts = {
    agent,
    body: opts.body,
    compress: opts.compress,
    follow: opts.follow,
    headers: new fetch.Headers(headers),
    method: opts.method,
    redirect: 'manual',
    size: opts.size,
    counter: opts.counter,
    timeout: opts.timeout
  }

  return retry(
    (retryHandler, attemptNum) => {
      const req = new fetch.Request(uri, reqOpts)
      return fetch(req)
        .then(res => {
          res.headers.set('x-fetch-attempts', attemptNum)

          if (opts.integrity) {
            remoteFetchHandleIntegrity(res, opts.integrity)
          }

          const isStream = req.body instanceof Stream

          if (opts.cacheManager) {
            const isMethodGetHead = req.method === 'GET' ||
              req.method === 'HEAD'

            const isCachable = opts.cache !== 'no-store' &&
              isMethodGetHead &&
              makePolicy(req, res).storable() &&
              res.status === 200 // No other statuses should be stored!

            if (isCachable) {
              return opts.cacheManager.put(req, res, opts)
            }

            if (!isMethodGetHead) {
              return opts.cacheManager.delete(req).then(() => {
                if (res.status >= 500 && req.method !== 'POST' && !isStream) {
                  return retryHandler(res)
                }

                return res
              })
            }
          }

          const isRetriable = req.method !== 'POST' &&
            !isStream && (
              res.status === 408 || // Request Timeout
              res.status === 420 || // Enhance Your Calm (usually Twitter rate-limit)
              res.status === 429 || // Too Many Requests ("standard" rate-limiting)
              res.status >= 500 // Assume server errors are momentary hiccups
            )

          if (isRetriable) {
            return retryHandler(res)
          }

          if (!fetch.isRedirect(res.status) || opts.redirect === 'manual') {
            return res
          }

          // handle redirects - matches behavior of npm-fetch: https://github.com/bitinn/node-fetch
          if (opts.redirect === 'error') {
            const err = new Error(`redirect mode is set to error: ${uri}`)
            err.code = 'ENOREDIRECT'
            throw err
          }

          if (!res.headers.get('location')) {
            const err = new Error(`redirect location header missing at: ${uri}`)
            err.code = 'EINVALIDREDIRECT'
            throw err
          }

          if (req.counter >= req.follow) {
            const err = new Error(`maximum redirect reached at: ${uri}`)
            err.code = 'EMAXREDIRECT'
            throw err
          }

          const resolvedUrl = url.resolve(req.url, res.headers.get('location'))
          let redirectURL = url.parse(resolvedUrl)

          if (isURL.test(res.headers.get('location'))) {
            redirectURL = url.parse(res.headers.get('location'))
          }

          // Remove authorization if changing hostnames (but not if just
          // changing ports or protocols).  This matches the behavior of request:
          // https://github.com/request/request/blob/b12a6245/lib/redirect.js#L134-L138
          if (url.parse(req.url).hostname !== redirectURL.hostname) {
            req.headers.delete('authorization')
          }

          // for POST request with 301/302 response, or any request with 303 response,
          // use GET when following redirect
          if (res.status === 303 ||
            ((res.status === 301 || res.status === 302) && req.method === 'POST')) {
            opts.method = 'GET'
            opts.body = null
            req.headers.delete('content-length')
          }

          opts.headers = {}
          req.headers.forEach((value, name) => {
            opts.headers[name] = value
          })

          opts.counter = ++req.counter
          return cachingFetch(resolvedUrl, opts)
        })
        .catch(err => {
          const code = err.code === 'EPROMISERETRY' ? err.retried.code : err.code

          const isRetryError = RETRY_ERRORS.indexOf(code) === -1 &&
            RETRY_TYPES.indexOf(err.type) === -1

          if (req.method === 'POST' || isRetryError) {
            throw err
          }

          return retryHandler(err)
        })
    },
    opts.retry
  ).catch(err => {
    if (err.status >= 400) {
      return err
    }

    throw err
  })
}

function isHeaderConditional (headers) {
  if (!headers || typeof headers !== 'object') {
    return false
  }

  const modifiers = [
    'if-modified-since',
    'if-none-match',
    'if-unmodified-since',
    'if-match',
    'if-range'
  ]

  return Object.keys(headers)
    .some(h => modifiers.indexOf(h.toLowerCase()) !== -1)
}
