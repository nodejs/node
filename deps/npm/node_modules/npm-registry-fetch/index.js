'use strict'

const Buffer = require('safe-buffer').Buffer

const checkResponse = require('./check-response.js')
const config = require('./config.js')
const getAuth = require('./auth.js')
const fetch = require('make-fetch-happen')
const JSONStream = require('JSONStream')
const npa = require('npm-package-arg')
const {PassThrough} = require('stream')
const qs = require('querystring')
const url = require('url')
const zlib = require('zlib')

module.exports = regFetch
function regFetch (uri, opts) {
  opts = config(opts)
  const registry = (
    (opts.spec && pickRegistry(opts.spec, opts)) ||
    opts.registry ||
    'https://registry.npmjs.org/'
  )
  uri = url.parse(uri).protocol
    ? uri
    : `${
      registry.trim().replace(/\/?$/g, '')
    }/${
      uri.trim().replace(/^\//, '')
    }`
  // through that takes into account the scope, the prefix of `uri`, etc
  const startTime = Date.now()
  const headers = getHeaders(registry, uri, opts)
  let body = opts.body
  const bodyIsStream = body &&
    typeof body === 'object' &&
    typeof body.pipe === 'function'
  if (body && !bodyIsStream && typeof body !== 'string' && !Buffer.isBuffer(body)) {
    headers['content-type'] = headers['content-type'] || 'application/json'
    body = JSON.stringify(body)
  } else if (body && !headers['content-type']) {
    headers['content-type'] = 'application/octet-stream'
  }
  if (opts.gzip) {
    headers['content-encoding'] = 'gzip'
    if (bodyIsStream) {
      const gz = zlib.createGzip()
      body.on('error', err => gz.emit('error', err))
      body = body.pipe(gz)
    } else {
      body = new opts.Promise((resolve, reject) => {
        zlib.gzip(body, (err, gz) => err ? reject(err) : resolve(gz))
      })
    }
  }
  if (opts.query) {
    let q = opts.query
    if (typeof q === 'string') {
      q = qs.parse(q)
    }
    Object.keys(q).forEach(key => {
      if (q[key] === undefined) {
        delete q[key]
      }
    })
    if (Object.keys(q).length) {
      const parsed = url.parse(uri)
      parsed.search = '?' + qs.stringify(
        parsed.query
          ? Object.assign(qs.parse(parsed.query), q)
          : q
      )
      uri = url.format(parsed)
    }
  }
  return opts.Promise.resolve(body).then(body => fetch(uri, {
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
    localAddress: opts['local-address'],
    maxSockets: opts.maxsockets,
    memoize: opts.memoize,
    method: opts.method || 'GET',
    noProxy: opts['no-proxy'] || opts.noproxy,
    Promise: opts.Promise,
    proxy: opts['https-proxy'] || opts.proxy,
    referer: opts.refer,
    retry: opts.retry != null ? opts.retry : {
      retries: opts['fetch-retries'],
      factor: opts['fetch-retry-factor'],
      minTimeout: opts['fetch-retry-mintimeout'],
      maxTimeout: opts['fetch-retry-maxtimeout']
    },
    strictSSL: !!opts['strict-ssl'],
    timeout: opts.timeout
  }).then(res => checkResponse(
    opts.method || 'GET', res, registry, startTime, opts
  )))
}

module.exports.json = fetchJSON
function fetchJSON (uri, opts) {
  return regFetch(uri, opts).then(res => res.json())
}

module.exports.json.stream = fetchJSONStream
function fetchJSONStream (uri, jsonPath, opts) {
  opts = config(opts)
  const parser = JSONStream.parse(jsonPath, opts.mapJson)
  const pt = parser.pipe(new PassThrough({objectMode: true}))
  parser.on('error', err => pt.emit('error', err))
  regFetch(uri, opts).then(res => {
    res.body.on('error', err => parser.emit('error', err))
    res.body.pipe(parser)
  }, err => pt.emit('error', err))
  return pt
}

module.exports.pickRegistry = pickRegistry
function pickRegistry (spec, opts) {
  spec = npa(spec)
  opts = config(opts)
  let registry = spec.scope &&
    opts[spec.scope.replace(/^@?/, '@') + ':registry']

  if (!registry && opts.scope) {
    registry = opts[opts.scope.replace(/^@?/, '@') + ':registry']
  }

  if (!registry) {
    registry = opts.registry || 'https://registry.npmjs.org/'
  }

  return registry
}

function getCacheMode (opts) {
  return opts.offline
    ? 'only-if-cached'
    : opts['prefer-offline']
      ? 'force-cache'
      : opts['prefer-online']
        ? 'no-cache'
        : 'default'
}

function getHeaders (registry, uri, opts) {
  const headers = Object.assign({
    'npm-in-ci': !!(
      opts['is-from-ci'] ||
      process.env['CI'] === 'true' ||
      process.env['TDDIUM'] ||
      process.env['JENKINS_URL'] ||
      process.env['bamboo.buildKey'] ||
      process.env['GO_PIPELINE_NAME']
    ),
    'npm-scope': opts['project-scope'],
    'npm-session': opts['npm-session'],
    'user-agent': opts['user-agent'],
    'referer': opts.refer
  }, opts.headers)

  const auth = getAuth(registry, opts)
  // If a tarball is hosted on a different place than the manifest, only send
  // credentials on `alwaysAuth`
  const shouldAuth = (
    auth.alwaysAuth ||
    url.parse(uri).host === url.parse(registry).host
  )
  if (shouldAuth && auth.token) {
    headers.authorization = `Bearer ${auth.token}`
  } else if (shouldAuth && auth.username && auth.password) {
    const encoded = Buffer.from(
      `${auth.username}:${auth.password}`, 'utf8'
    ).toString('base64')
    headers.authorization = `Basic ${encoded}`
  } else if (shouldAuth && auth._auth) {
    headers.authorization = `Basic ${auth._auth}`
  }
  if (shouldAuth && auth.otp) {
    headers['npm-otp'] = auth.otp
  }
  return headers
}
