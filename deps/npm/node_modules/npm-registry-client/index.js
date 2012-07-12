
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

function RegClient (options) {
  // if provided, then the registry needs to be a url.
  // if it's not provided, then we're just using the cache only.
  var registry = options.registry
  if (registry) {
    registry = url.parse(registry)
    if (!registry.protocol) throw new Error(
      'Invalid registry: ' + registry.url)
    this.registry = registry.href
    if (this.registry.slice(-1) !== '/') {
      this.registry += '/'
    }
  } else {
    this.registry = null
  }

  this.retries = options.retries || 2
  this.retryFactor = options.retryFactor || 10
  this.retryMinTimeout = options.retryMinTimeout || 10000
  this.retryMaxTimeout = options.retryMaxTimeout || 60000

  this.cache = options.cache
  if (!this.cache) throw new Error("Cache dir is required")

  this.alwaysAuth = options.alwaysAuth || false

  this.auth = options.auth || null
  if (this.auth) {
    var a = new Buffer(this.auth, "base64").toString()
    a = a.split(":")
    this.username = a.shift()
    this.password = a.join(":")
  } else {
    this.username = options.username
    this.password = options.password

    // if username and password are set, but auth isn't, use them.
    if (this.username && this.password) {
      var a = this.username + ":" + this.password
      this.auth = new Buffer(a, "utf8").toString("base64")
    }
  }

  if (this.auth && !this.alwaysAuth && this.registry) {
    // if we're always authing, then we just send the
    // user/pass on every thing.  otherwise, create a
    // session, and use that.
    this.token = options.token
    this.couchLogin = new CouchLogin(this.registry, this.token)
  }

  this.email = options.email || null
  this.defaultTag = options.tag || "latest"

  this.ca = options.ca || null

  this.strictSSL = options.strictSSL
  if (this.strictSSL === undefined) this.strictSSL = true

  this.userAgent = options.userAgent
  if (this.userAgent === undefined) {
    this.userAgent = 'node/' + process.version
  }

  this.cacheMin = options.cacheMin || 0
  this.cacheMax = options.cacheMax || Infinity

  this.proxy = options.proxy
  this.httpsProxy = options.httpsProxy || options.proxy

  this.log = options.log || npmlog
}

require('fs').readdirSync(__dirname + "/lib").forEach(function (f) {
  if (!f.match(/\.js$/)) return
  RegClient.prototype[f.replace(/\.js$/, '')] = require('./lib/' + f)
})
