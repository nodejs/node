module.exports = access

var assert = require('assert')
var url = require('url')
var npa = require('npm-package-arg')
var subcommands = {}

function access (sub, uri, params, cb) {
  accessAssertions(sub, uri, params, cb)
  return subcommands[sub].call(this, uri, params, cb)
}

subcommands.public = function (uri, params, cb) {
  return setAccess.call(this, 'public', uri, params, cb)
}
subcommands.restricted = function (uri, params, cb) {
  return setAccess.call(this, 'restricted', uri, params, cb)
}

function setAccess (access, uri, params, cb) {
  return this.request(apiUri(uri, 'package', params.package, 'access'), {
    method: 'POST',
    auth: params.auth,
    body: JSON.stringify({ access: access })
  }, cb)
}

subcommands.grant = function (uri, params, cb) {
  var reqUri = apiUri(uri, 'team', params.scope, params.team, 'package')
  return this.request(reqUri, {
    method: 'PUT',
    auth: params.auth,
    body: JSON.stringify({
      permissions: params.permissions,
      package: params.package
    })
  }, cb)
}

subcommands.revoke = function (uri, params, cb) {
  var reqUri = apiUri(uri, 'team', params.scope, params.team, 'package')
  return this.request(reqUri, {
    method: 'DELETE',
    auth: params.auth,
    body: JSON.stringify({
      package: params.package
    })
  }, cb)
}

subcommands['ls-packages'] = function (uri, params, cb, type) {
  type = type || (params.team ? 'team' : 'org')
  var client = this
  var uriParams = '?format=cli'
  var reqUri = apiUri(uri, type, params.scope, params.team, 'package')
  return client.request(reqUri + uriParams, {
    method: 'GET',
    auth: params.auth
  }, function (err, perms) {
    if (err && err.statusCode === 404 && type === 'org') {
      subcommands['ls-packages'].call(client, uri, params, cb, 'user')
    } else {
      cb(err, perms && translatePermissions(perms))
    }
  })
}

subcommands['ls-collaborators'] = function (uri, params, cb) {
  var uriParams = '?format=cli'
  if (params.user) {
    uriParams += ('&user=' + encodeURIComponent(params.user))
  }
  var reqUri = apiUri(uri, 'package', params.package, 'collaborators')
  return this.request(reqUri + uriParams, {
    method: 'GET',
    auth: params.auth
  }, function (err, perms) {
    cb(err, perms && translatePermissions(perms))
  })
}

subcommands.edit = function () {
  throw new Error('edit subcommand is not implemented yet')
}

function apiUri (registryUri) {
  var path = Array.prototype.slice.call(arguments, 1)
    .filter(function (x) { return x })
    .map(encodeURIComponent)
    .join('/')
  return url.resolve(registryUri, '-/' + path)
}

function accessAssertions (subcommand, uri, params, cb) {
  assert(subcommands.hasOwnProperty(subcommand),
    'access subcommand must be one of ' +
         Object.keys(subcommands).join(', '))
  typeChecks({
    'uri': [uri, 'string'],
    'params': [params, 'object'],
    'auth': [params.auth, 'object'],
    'callback': [cb, 'function']
  })
  if (contains([
    'public', 'restricted'
  ], subcommand)) {
    typeChecks({ 'package': [params.package, 'string'] })
    assert(!!npa(params.package).scope,
      'access commands are only accessible for scoped packages')
  }
  if (contains(['grant', 'revoke', 'ls-packages'], subcommand)) {
    typeChecks({ 'scope': [params.scope, 'string'] })
  }
  if (contains(['grant', 'revoke'], subcommand)) {
    typeChecks({ 'team': [params.team, 'string'] })
  }
  if (subcommand === 'grant') {
    typeChecks({ 'permissions': [params.permissions, 'string'] })
    assert(params.permissions === 'read-only' ||
           params.permissions === 'read-write',
    'permissions must be either read-only or read-write')
  }
}

function typeChecks (specs) {
  Object.keys(specs).forEach(function (key) {
    var checks = specs[key]
    /* eslint valid-typeof:0 */
    assert(typeof checks[0] === checks[1],
      key + ' is required and must be of type ' + checks[1])
  })
}

function contains (arr, item) {
  return arr.indexOf(item) !== -1
}

function translatePermissions (perms) {
  var newPerms = {}
  for (var key in perms) {
    if (perms.hasOwnProperty(key)) {
      if (perms[key] === 'read') {
        newPerms[key] = 'read-only'
      } else if (perms[key] === 'write') {
        newPerms[key] = 'read-write'
      } else {
        // This shouldn't happen, but let's not break things
        // if the API starts returning different things.
        newPerms[key] = perms[key]
      }
    }
  }
  return newPerms
}
