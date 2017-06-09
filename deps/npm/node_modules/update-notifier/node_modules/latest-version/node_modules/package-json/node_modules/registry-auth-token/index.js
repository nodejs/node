var url = require('url')
var base64 = require('./base64')

var decodeBase64 = base64.decodeBase64
var encodeBase64 = base64.encodeBase64

var tokenKey = ':_authToken'
var userKey = ':username'
var passwordKey = ':_password'

module.exports = function getRegistryAuthInfo(registryUrl, opts) {
  var options = opts || {}
  var npmrc = require('rc')('npm', {registry: 'https://registry.npmjs.org/'})
  var parsed = url.parse(registryUrl || npmrc.registry, false, true)
  var pathname

  while (pathname !== '/') {
    pathname = parsed.pathname || '/'

    var regUrl = '//' + parsed.host + pathname.replace(/\/$/, '')
    var authInfo = getAuthInfoForUrl(regUrl, npmrc)
    if (authInfo) {
      return authInfo
    }

    // break if not recursive
    if (!options.recursive) {
      return undefined
    }

    parsed.pathname = url.resolve(normalizePath(pathname), '..') || '/'
  }

  return undefined
}

function normalizePath(path) {
  return path[path.length - 1] === '/' ? path : path + '/'
}

function getAuthInfoForUrl(regUrl, npmrc) {
  // try to get bearer token
  var bearerAuth = getBearerToken(npmrc[regUrl + tokenKey] || npmrc[regUrl + '/' + tokenKey])
  if (bearerAuth) {
    return bearerAuth
  }

  // try to get basic token
  var username = npmrc[regUrl + userKey] || npmrc[regUrl + '/' + userKey]
  var password = npmrc[regUrl + passwordKey] || npmrc[regUrl + '/' + passwordKey]
  var basicAuth = getTokenForUsernameAndPassword(username, password)
  if (basicAuth) {
    return basicAuth
  }

  return undefined
}

function getBearerToken(tok) {
  if (!tok) {
    return undefined
  }

  // check if bearer token
  var token = tok.replace(/^\$\{?([^}]*)\}?$/, function (fullMatch, envVar) {
    return process.env[envVar]
  })

  return {token: token, type: 'Bearer'}
}

function getTokenForUsernameAndPassword(username, password) {
  if (!username || !password) {
    return undefined
  }

  // passwords are base64 encoded, so we need to decode it
  // See https://github.com/npm/npm/blob/v3.10.6/lib/config/set-credentials-by-uri.js#L26
  var pass = decodeBase64(password)

  // a basic auth token is base64 encoded 'username:password'
  // See https://github.com/npm/npm/blob/v3.10.6/lib/config/get-credentials-by-uri.js#L70
  var token = encodeBase64(username + ':' + pass)

  // we found a basicToken token so let's exit the loop
  return {
    token: token,
    type: 'Basic',
    password: pass,
    username: username
  }
}
