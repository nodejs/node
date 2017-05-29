'use strict'

const BB = require('bluebird')
const Buffer = require('safe-buffer').Buffer

const checkWarnings = require('./check-warning-header')
const fetch = require('make-fetch-happen')
const registryKey = require('./registry-key')
const url = require('url')

module.exports = regFetch
function regFetch (uri, registry, opts) {
  const startTime = Date.now()
  return fetch(uri, {
    agent: opts.agent,
    algorithms: opts.algorithms,
    cache: getCacheMode(opts),
    cacheManager: opts.cache,
    headers: getHeaders(uri, registry, opts),
    integrity: opts.integrity,
    memoize: opts.memoize,
    noProxy: opts.noProxy,
    Promise: BB,
    proxy: opts.proxy,
    referer: opts.refer,
    retry: opts.retry,
    timeout: opts.timeout,
    uid: opts.uid,
    gid: opts.gid
  }).then(res => {
    if (res.headers.has('npm-notice') && !res.headers.has('x-local-cache')) {
      opts.log.warn('notice', res.headers.get('npm-notice'))
    }
    checkWarnings(res, registry, opts)
    if (res.status >= 400) {
      const err = new Error(`${res.status} ${res.statusText}: ${
        opts.spec ? opts.spec : uri
      }`)
      err.code = `E${res.status}`
      err.uri = uri
      err.response = res
      err.spec = opts.spec
      logRequest(uri, res, startTime, opts)
      throw err
    } else {
      res.body.on('end', () => logRequest(uri, res, startTime, opts))
      return res
    }
  })
}

function logRequest (uri, res, startTime, opts) {
  const elapsedTime = Date.now() - startTime
  const attempt = res.headers.get('x-fetch-attempts')
  const attemptStr = attempt && attempt > 1 ? ` attempt #${attempt}` : ''
  const cacheStr = res.headers.get('x-local-cache') ? ' (from cache)' : ''
  opts.log.http(
    'fetch',
    `GET ${res.status} ${uri} ${elapsedTime}ms${attemptStr}${cacheStr}`
  )
}

function getCacheMode (opts) {
  return opts.offline
  ? 'only-if-cached'
  : opts.preferOffline
  ? 'force-cache'
  : opts.preferOnline
  ? 'no-cache'
  : 'default'
}

function getHeaders (uri, registry, opts) {
  const headers = Object.assign({
    'npm-in-ci': opts.isFromCI,
    'npm-scope': opts.projectScope,
    'user-agent': opts.userAgent,
    'referer': opts.refer
  }, opts.headers)
  const auth = (
    opts.auth &&
    opts.auth[registryKey(registry)]
  )
  // If a tarball is hosted on a different place than the manifest, only send
  // credentials on `alwaysAuth`
  const shouldAuth = auth && (
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
  }
  return headers
}
