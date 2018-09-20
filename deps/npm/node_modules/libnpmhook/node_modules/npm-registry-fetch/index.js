'use strict'

const Buffer = require('safe-buffer').Buffer

const checkResponse = require('./check-response.js')
const config = require('./config.js')
const getAuth = require('./auth.js')
const fetch = require('make-fetch-happen')
const npa = require('npm-package-arg')
const qs = require('querystring')
const url = require('url')

module.exports = regFetch
function regFetch (uri, opts) {
  opts = config(opts)
  const registry = (
    (opts.get('spec') && pickRegistry(opts.get('spec'), opts)) ||
    opts.get('registry') ||
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
  let body = opts.get('body')
  const bodyIsStream = body &&
    typeof body === 'object' &&
    typeof body.pipe === 'function'
  if (body && !bodyIsStream && typeof body !== 'string' && !Buffer.isBuffer(body)) {
    headers['content-type'] = headers['content-type'] || 'application/json'
    body = JSON.stringify(body)
  } else if (body && !headers['content-type']) {
    headers['content-type'] = 'application/octet-stream'
  }
  if (opts.get('query')) {
    let q = opts.get('query')
    if (typeof q === 'string') {
      q = qs.parse(q)
    }
    const parsed = url.parse(uri)
    parsed.search = '?' + qs.stringify(
      parsed.query
        ? Object.assign(qs.parse(parsed.query), q)
        : q
    )
    uri = url.format(parsed)
  }
  return fetch(uri, {
    agent: opts.get('agent'),
    algorithms: opts.get('algorithms'),
    body,
    cache: getCacheMode(opts),
    cacheManager: opts.get('cache'),
    ca: opts.get('ca'),
    cert: opts.get('cert'),
    headers,
    integrity: opts.get('integrity'),
    key: opts.get('key'),
    localAddress: opts.get('local-address'),
    maxSockets: opts.get('maxsockets'),
    memoize: opts.get('memoize'),
    method: opts.get('method') || 'GET',
    noProxy: opts.get('no-proxy') || opts.get('noproxy'),
    Promise: opts.get('Promise'),
    proxy: opts.get('https-proxy') || opts.get('proxy'),
    referer: opts.get('refer'),
    retry: opts.get('retry') || {
      retries: opts.get('fetch-retries'),
      factor: opts.get('fetch-retry-factor'),
      minTimeout: opts.get('fetch-retry-mintimeout'),
      maxTimeout: opts.get('fetch-retry-maxtimeout')
    },
    strictSSL: !!opts.get('strict-ssl'),
    timeout: opts.get('timeout'),
    uid: opts.get('uid'),
    gid: opts.get('gid')
  }).then(res => checkResponse(
    opts.get('method') || 'GET', res, registry, startTime, opts
  ))
}

module.exports.json = fetchJSON
function fetchJSON (uri, opts) {
  return regFetch(uri, opts).then(res => res.json())
}

module.exports.pickRegistry = pickRegistry
function pickRegistry (spec, opts) {
  spec = npa(spec)
  opts = config(opts)
  let registry = spec.scope &&
    opts.get(spec.scope.replace(/^@?/, '@') + ':registry')

  if (!registry && opts.get('scope')) {
    registry = opts.get(
      opts.get('scope').replace(/^@?/, '@') + ':registry'
    )
  }

  if (!registry) {
    registry = opts.get('registry') || 'https://registry.npmjs.org/'
  }

  return registry
}

function getCacheMode (opts) {
  return opts.get('offline')
    ? 'only-if-cached'
    : opts.get('prefer-offline')
      ? 'force-cache'
      : opts.get('prefer-online')
        ? 'no-cache'
        : 'default'
}

function getHeaders (registry, uri, opts) {
  const headers = Object.assign({
    'npm-in-ci': !!(
      opts.get('is-from-ci') ||
      process.env['CI'] === 'true' ||
      process.env['TDDIUM'] ||
      process.env['JENKINS_URL'] ||
      process.env['bamboo.buildKey'] ||
      process.env['GO_PIPELINE_NAME']
    ),
    'npm-scope': opts.get('project-scope'),
    'npm-session': opts.get('npm-session'),
    'user-agent': opts.get('user-agent'),
    'referer': opts.get('refer')
  }, opts.get('headers'))

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
