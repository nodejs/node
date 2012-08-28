
// utilities for working with the js-registry site.

module.exports = RegClient

var fs = require('fs')
, url = require('url')
, path = require('path')
, CouchLogin = require('couch-login')
, npmlog

try {
  npmlog = require("npmlog")
} catch (er) {
  npmlog = { error: noop, warn: noop, info: noop,
             verbose: noop, silly: noop, http: noop,
             pause: noop, resume: noop }
}

function noop () {}

function RegClient (conf) {
  // accept either a plain-jane object, or a npmconf object
  // with a "get" method.
  if (typeof conf.get !== 'function') {
    var data = conf
    conf = { get: function (k) { return data[k] }
           , set: function (k, v) { data[k] = v }
           , del: function (k) { delete data[k] } }
  }

  this.conf = conf

  // if provided, then the registry needs to be a url.
  // if it's not provided, then we're just using the cache only.
  var registry = conf.get('registry')
  if (registry) {
    registry = url.parse(registry)
    if (!registry.protocol) throw new Error(
      'Invalid registry: ' + registry.url)
    registry = registry.href
    if (registry.slice(-1) !== '/') {
      registry += '/'
    }
    this.conf.set('registry', registry)
  } else {
    registry = null
  }

  if (!conf.get('cache')) throw new Error("Cache dir is required")

  var auth = this.conf.get('_auth')
  var alwaysAuth = this.conf.get('always-auth')
  if (auth && !alwaysAuth && registry) {
    // if we're always authing, then we just send the
    // user/pass on every thing.  otherwise, create a
    // session, and use that.
    var token = this.conf.get('_token')
    this.couchLogin = new CouchLogin(registry, token)
    this.couchLogin.proxy = this.conf.get('proxy')
  }

  this.log = conf.log || conf.get('log') || npmlog
}

require('fs').readdirSync(__dirname + "/lib").forEach(function (f) {
  if (!f.match(/\.js$/)) return
  RegClient.prototype[f.replace(/\.js$/, '')] = require('./lib/' + f)
})
