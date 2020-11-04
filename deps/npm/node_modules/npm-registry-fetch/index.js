'use strict'

const checkResponse = require('./check-response.js')
const getAuth = require('./auth.js')
const fetch = require('make-fetch-happen')
const JSONStream = require('minipass-json-stream')
const npa = require('npm-package-arg')
const qs = require('querystring')
const url = require('url')
const zlib = require('minizlib')
const Minipass = require('minipass')

const defaultOpts = require('./default-opts.js')

// WhatWG URL throws if it's not fully resolved
const urlIsValid = u => {
  try {
    return !!new url.URL(u)
  } catch (_) {
    return false
  }
}

module.exports = regFetch
function regFetch (uri, /* istanbul ignore next */ opts_ = {}) {
  const opts = {
    ...defaultOpts,
    ...opts_,
  }
  const registry = opts.registry = (
    (opts.spec && pickRegistry(opts.spec, opts)) ||
    opts.registry ||
    /* istanbul ignore next */
    'https://registry.npmjs.org/'
  )

  if (!urlIsValid(uri)) {
    uri = `${
      registry.trim().replace(/\/?$/g, '')
    }/${
      uri.trim().replace(/^\//, '')
    }`
  }

  const method = opts.method || 'GET'

  // through that takes into account the scope, the prefix of `uri`, etc
  const startTime = Date.now()
  const headers = getHeaders(registry, uri, opts)
  let body = opts.body
  const bodyIsStream = Minipass.isStream(body)
  const bodyIsPromise = body &&
    typeof body === 'object' &&
    typeof body.then === 'function'

  if (body && !bodyIsStream && !bodyIsPromise && typeof body !== 'string' && !Buffer.isBuffer(body)) {
    headers['content-type'] = headers['content-type'] || 'application/json'
    body = JSON.stringify(body)
  } else if (body && !headers['content-type'])
    headers['content-type'] = 'application/octet-stream'

  if (opts.gzip) {
    headers['content-encoding'] = 'gzip'
    if (bodyIsStream) {
      const gz = new zlib.Gzip()
      body.on('error', /* istanbul ignore next: unlikely and hard to test */
        err => gz.emit('error', err))
      body = body.pipe(gz)
    } else if (!bodyIsPromise)
      body = new zlib.Gzip().end(body).concat()
  }

  const parsed = new url.URL(uri)

  if (opts.query) {
    const q = typeof opts.query === 'string' ? qs.parse(opts.query)
      : opts.query

    Object.keys(q).forEach(key => {
      if (q[key] !== undefined)
        parsed.searchParams.set(key, q[key])
    })
    uri = url.format(parsed)
  }

  if (parsed.searchParams.get('write') === 'true' && method === 'GET') {
    // do not cache, because this GET is fetching a rev that will be
    // used for a subsequent PUT or DELETE, so we need to conditionally
    // update cache.
    opts.offline = false
    opts.preferOffline = false
    opts.preferOnline = true
  }

  const doFetch = (body) => fetch(uri, {
    agent: opts.agent,
    algorithms: opts.algorithms,
    body,
    cache: getCacheMode(opts),
    cacheManager: opts.cache,
    ca: opts.ca,
    cert: opts.cert,
    headers,
    integrity: opts.integrity,
    key: opts.key,
    localAddress: opts.localAddress,
    maxSockets: opts.maxSockets,
    memoize: opts.memoize,
    method: method,
    noProxy: opts.noProxy,
    proxy: opts.httpsProxy || opts.proxy,
    retry: opts.retry ? opts.retry : {
      retries: opts.fetchRetries,
      factor: opts.fetchRetryFactor,
      minTimeout: opts.fetchRetryMintimeout,
      maxTimeout: opts.fetchRetryMaxtimeout,
    },
    strictSSL: opts.strictSSL,
    timeout: opts.timeout || 30 * 1000,
  }).then(res => checkResponse(
    method, res, registry, startTime, opts
  ))

  return Promise.resolve(body).then(doFetch)
}

module.exports.json = fetchJSON
function fetchJSON (uri, opts) {
  return regFetch(uri, opts).then(res => res.json())
}

module.exports.json.stream = fetchJSONStream
function fetchJSONStream (uri, jsonPath, /* istanbul ignore next */ opts_ = {}) {
  const opts = { ...defaultOpts, ...opts_ }
  const parser = JSONStream.parse(jsonPath, opts.mapJSON)
  regFetch(uri, opts).then(res =>
    res.body.on('error',
      /* istanbul ignore next: unlikely and difficult to test */
      er => parser.emit('error', er)).pipe(parser)
  ).catch(er => parser.emit('error', er))
  return parser
}

module.exports.pickRegistry = pickRegistry
function pickRegistry (spec, opts = {}) {
  spec = npa(spec)
  let registry = spec.scope &&
    opts[spec.scope.replace(/^@?/, '@') + ':registry']

  if (!registry && opts.scope)
    registry = opts[opts.scope.replace(/^@?/, '@') + ':registry']

  if (!registry)
    registry = opts.registry || 'https://registry.npmjs.org/'

  return registry
}

function getCacheMode (opts) {
  return opts.offline ? 'only-if-cached'
    : opts.preferOffline ? 'force-cache'
    : opts.preferOnline ? 'no-cache'
    : 'default'
}

function getHeaders (registry, uri, opts) {
  const headers = Object.assign({
    'npm-in-ci': !!opts.isFromCI,
    'user-agent': opts.userAgent,
  }, opts.headers || {})

  if (opts.projectScope)
    headers['npm-scope'] = opts.projectScope

  if (opts.npmSession)
    headers['npm-session'] = opts.npmSession

  if (opts.npmCommand)
    headers['npm-command'] = opts.npmCommand

  const auth = getAuth(registry, opts)
  // If a tarball is hosted on a different place than the manifest, only send
  // credentials on `alwaysAuth`
  const shouldAuth = (
    auth.alwaysAuth ||
    new url.URL(uri).host === new url.URL(registry).host
  )
  if (shouldAuth && auth.token)
    headers.authorization = `Bearer ${auth.token}`
  else if (shouldAuth && auth.username && auth.password) {
    const encoded = Buffer.from(
      `${auth.username}:${auth.password}`, 'utf8'
    ).toString('base64')
    headers.authorization = `Basic ${encoded}`
  } else if (shouldAuth && auth._auth)
    headers.authorization = `Basic ${auth._auth}`

  if (shouldAuth && auth.otp)
    headers['npm-otp'] = auth.otp

  return headers
}
