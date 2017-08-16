// utilities for working with the js-registry site.

module.exports = RegClient

var npmlog
try {
  npmlog = require('npmlog')
} catch (er) {
  npmlog = {
    error: noop,
    warn: noop,
    info: noop,
    verbose: noop,
    silly: noop,
    http: noop,
    pause: noop,
    resume: noop
  }
}

function noop () {}

function RegClient (config) {
  this.config = Object.create(config || {})

  this.config.proxy = this.config.proxy || {}
  if (!this.config.proxy.https && this.config.proxy.http) {
    this.config.proxy.https = this.config.proxy.http
  }

  this.config.ssl = this.config.ssl || {}
  if (this.config.ssl.strict === undefined) this.config.ssl.strict = true

  this.config.retry = this.config.retry || {}
  if (typeof this.config.retry.retries !== 'number') this.config.retry.retries = 2
  if (typeof this.config.retry.factor !== 'number') this.config.retry.factor = 10
  if (typeof this.config.retry.minTimeout !== 'number') this.config.retry.minTimeout = 10000
  if (typeof this.config.retry.maxTimeout !== 'number') this.config.retry.maxTimeout = 60000
  if (typeof this.config.maxSockets !== 'number') this.config.maxSockets = 50

  this.config.userAgent = this.config.userAgent || 'node/' + process.version
  this.config.defaultTag = this.config.defaultTag || 'latest'

  this.log = this.config.log || npmlog
  delete this.config.log

  var client = this
  client.access = require('./lib/access')
  client.adduser = require('./lib/adduser')
  client.attempt = require('./lib/attempt')
  client.authify = require('./lib/authify')
  client.deprecate = require('./lib/deprecate')
  client.distTags = Object.create(client)
  client.distTags.add = require('./lib/dist-tags/add')
  client.distTags.fetch = require('./lib/dist-tags/fetch')
  client.distTags.rm = require('./lib/dist-tags/rm')
  client.distTags.set = require('./lib/dist-tags/set')
  client.distTags.update = require('./lib/dist-tags/update')
  client.fetch = require('./lib/fetch')
  client.get = require('./lib/get')
  client.initialize = require('./lib/initialize')
  client.logout = require('./lib/logout')
  client.org = require('./lib/org')
  client.ping = require('./lib/ping')
  client.publish = require('./lib/publish')
  client.request = require('./lib/request')
  client.sendAnonymousCLIMetrics = require('./lib/send-anonymous-CLI-metrics')
  client.star = require('./lib/star')
  client.stars = require('./lib/stars')
  client.tag = require('./lib/tag')
  client.team = require('./lib/team')
  client.unpublish = require('./lib/unpublish')
  client.whoami = require('./lib/whoami')
}
