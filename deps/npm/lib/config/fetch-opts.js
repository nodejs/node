'use strict'

const url = require('url')

module.exports.fromPacote = fromPacote

function fromPacote (opts) {
  return {
    cache: getCacheMode(opts),
    cacheManager: opts.cache,
    ca: opts.ca,
    cert: opts.cert,
    headers: getHeaders('', opts.registry, opts),
    key: opts.key,
    localAddress: opts.localAddress,
    maxSockets: opts.maxSockets,
    proxy: opts.proxy,
    referer: opts.refer,
    retry: opts.retry,
    strictSSL: !!opts.strictSSL,
    timeout: opts.timeout,
    uid: opts.uid,
    gid: opts.gid
  }
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
    'npm-session': opts.npmSession,
    'user-agent': opts.userAgent,
    'referer': opts.refer
  }, opts.headers)
  // check for auth settings specific to this registry
  let auth = (
    opts.auth &&
    opts.auth[registryKey(registry)]
  ) || opts.auth
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
  } else if (shouldAuth && auth._auth) {
    headers.authorization = `Basic ${auth._auth}`
  }
  return headers
}

function registryKey (registry) {
  const parsed = url.parse(registry)
  const formatted = url.format({
    host: parsed.host,
    pathname: parsed.pathname,
    slashes: parsed.slashes
  })
  return url.resolve(formatted, '.')
}
