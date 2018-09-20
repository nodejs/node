'use strict'

module.exports = org

var assert = require('assert')
var url = require('url')

var subcommands = {}

function org (subcommand, uri, params, cb) {
  orgAssertions(subcommand, uri, params, cb)
  return subcommands[subcommand].call(this, uri, params, cb)
}

subcommands.set = subcommands.add = function (uri, params, cb) {
  return this.request(apiUri(uri, 'org', params.org, 'user'), {
    method: 'PUT',
    auth: params.auth,
    body: JSON.stringify({
      user: params.user,
      role: params.role
    })
  }, cb)
}

subcommands.rm = function (uri, params, cb) {
  return this.request(apiUri(uri, 'org', params.org, 'user'), {
    method: 'DELETE',
    auth: params.auth,
    body: JSON.stringify({
      user: params.user
    })
  }, cb)
}

subcommands.ls = function (uri, params, cb) {
  return this.request(apiUri(uri, 'org', params.org, 'user'), {
    method: 'GET',
    auth: params.auth
  }, cb)
}

function apiUri (registryUri) {
  var path = Array.prototype.slice.call(arguments, 1)
    .map(encodeURIComponent)
    .join('/')
  return url.resolve(registryUri, '-/' + path)
}

function orgAssertions (subcommand, uri, params, cb) {
  assert(subcommand, 'subcommand is required')
  assert(subcommands.hasOwnProperty(subcommand),
    'org subcommand must be one of ' + Object.keys(subcommands))
  assert(typeof uri === 'string', 'registry URI is required')
  assert(typeof params === 'object', 'params are required')
  assert(typeof params.auth === 'object', 'auth is required')
  assert(!cb || typeof cb === 'function', 'callback must be a function')
  assert(typeof params.org === 'string', 'org name is required')
  if (subcommand === 'rm' || subcommand === 'add' || subcommand === 'set') {
    assert(typeof params.user === 'string', 'user is required')
  }
}
