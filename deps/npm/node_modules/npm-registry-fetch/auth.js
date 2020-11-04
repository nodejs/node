'use strict'

const defaultOpts = require('./default-opts.js')
const url = require('url')

module.exports = getAuth
function getAuth (registry, opts_ = {}) {
  if (!registry)
    throw new Error('registry is required')
  const opts = opts_.forceAuth ? opts_.forceAuth : { ...defaultOpts, ...opts_ }
  const AUTH = {}
  const regKey = registry && registryKey(registry)
  const doKey = (key, alias) => addKey(opts, AUTH, regKey, key, alias)
  doKey('token')
  doKey('_authToken', 'token')
  doKey('username')
  doKey('password')
  doKey('_password', 'password')
  doKey('email')
  doKey('_auth')
  doKey('otp')
  doKey('always-auth', 'alwaysAuth')
  if (AUTH.password)
    AUTH.password = Buffer.from(AUTH.password, 'base64').toString('utf8')

  if (AUTH._auth && !(AUTH.username && AUTH.password)) {
    let auth = Buffer.from(AUTH._auth, 'base64').toString()
    auth = auth.split(':')
    AUTH.username = auth.shift()
    AUTH.password = auth.join(':')
  }
  AUTH.alwaysAuth = AUTH.alwaysAuth === 'false' ? false : !!AUTH.alwaysAuth
  return AUTH
}

function addKey (opts, obj, scope, key, objKey) {
  if (opts[key])
    obj[objKey || key] = opts[key]

  if (scope && opts[`${scope}:${key}`])
    obj[objKey || key] = opts[`${scope}:${key}`]
}

// Called a nerf dart in the main codebase. Used as a "safe"
// key when fetching registry info from config.
function registryKey (registry) {
  const parsed = new url.URL(registry)
  const formatted = url.format({
    protocol: parsed.protocol,
    host: parsed.host,
    pathname: parsed.pathname,
    slashes: true,
  })
  return url.format(new url.URL('.', formatted)).replace(/^[^:]+:/, '')
}
