module.exports = team

var assert = require('assert')
var url = require('url')

var subcommands = {}

function team (sub, uri, params, cb) {
  teamAssertions(sub, uri, params, cb)
  return subcommands[sub].call(this, uri, params, cb)
}

subcommands.create = function (uri, params, cb) {
  return this.request(apiUri(uri, 'org', params.scope, 'team'), {
    method: 'PUT',
    auth: params.auth,
    body: JSON.stringify({
      name: params.team
    })
  }, cb)
}

subcommands.destroy = function (uri, params, cb) {
  return this.request(apiUri(uri, 'team', params.scope, params.team), {
    method: 'DELETE',
    auth: params.auth
  }, cb)
}

subcommands.add = function (uri, params, cb) {
  return this.request(apiUri(uri, 'team', params.scope, params.team, 'user'), {
    method: 'PUT',
    auth: params.auth,
    body: JSON.stringify({
      user: params.user
    })
  }, cb)
}

subcommands.rm = function (uri, params, cb) {
  return this.request(apiUri(uri, 'team', params.scope, params.team, 'user'), {
    method: 'DELETE',
    auth: params.auth,
    body: JSON.stringify({
      user: params.user
    })
  }, cb)
}

subcommands.ls = function (uri, params, cb) {
  var uriParams = '?format=cli'
  if (params.team) {
    var reqUri = apiUri(
      uri, 'team', params.scope, params.team, 'user') + uriParams
    return this.request(reqUri, {
      method: 'GET',
      auth: params.auth
    }, cb)
  } else {
    return this.request(apiUri(uri, 'org', params.scope, 'team') + uriParams, {
      method: 'GET',
      auth: params.auth
    }, cb)
  }
}

// TODO - we punted this to v2
// subcommands.edit = function (uri, params, cb) {
//   return this.request(apiUri(uri, 'team', params.scope, params.team, 'user'), {
//     method: 'POST',
//     auth: params.auth,
//     body: JSON.stringify({
//       users: params.users
//     })
//   }, cb)
// }

function apiUri (registryUri) {
  var path = Array.prototype.slice.call(arguments, 1)
    .map(encodeURIComponent)
    .join('/')
  return url.resolve(registryUri, '-/' + path)
}

function teamAssertions (subcommand, uri, params, cb) {
  assert(subcommand, 'subcommand is required')
  assert(subcommands.hasOwnProperty(subcommand),
         'team subcommand must be one of ' + Object.keys(subcommands))
  assert(typeof uri === 'string', 'registry URI is required')
  assert(typeof params === 'object', 'params are required')
  assert(typeof params.auth === 'object', 'auth is required')
  assert(typeof params.scope === 'string', 'scope is required')
  assert(!cb || typeof cb === 'function', 'callback must be a function')
  if (subcommand !== 'ls') {
    assert(typeof params.team === 'string', 'team name is required')
  }
  if (subcommand === 'rm' || subcommand === 'add') {
    assert(typeof params.user === 'string', 'user is required')
  }
  if (subcommand === 'edit') {
    assert(typeof params.users === 'object' &&
           params.users.length != null,
           'users is required')
  }
}
