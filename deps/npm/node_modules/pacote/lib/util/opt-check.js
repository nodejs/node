'use strict'

const pkg = require('../../package.json')
const silentlog = require('./silentlog')

function PacoteOptions (opts) {
  opts = opts || {}
  this._isPacoteOptions = true
  this.agent = opts.agent
  this.annotate = opts.annotate
  this.auth = opts.auth
  this.scopeTargets = opts.scopeTargets || {}
  this.defaultTag = opts.defaultTag || 'latest'
  this.cache = opts.cache
  this.ca = opts.ca
  this.cert = opts.cert
  this.integrity = opts.integrity
  this.key = opts.key
  this.localAddress = opts.localAddress
  this.log = opts.log || silentlog
  this.memoize = opts.memoize
  this.maxSockets = opts.maxSockets || 10
  this.offline = opts.offline
  this.preferOffline = opts.preferOffline
  this.proxy = opts.proxy
  this.noProxy = opts.noProxy
  this.registry = opts.registry || 'https://registry.npmjs.org'
  this.resolved = opts.resolved
  this.retry = opts.retry // for npm-registry-client
  this.scope = opts.scope
  this.userAgent = opts.userAgent || `${pkg.name}@${pkg.version}/node@${process.version}+${process.arch} (${process.platform})`
  this.where = opts.where
  this.preferOnline = opts.preferOnline
  this.strictSSL = !!opts.strictSSL
  this.isFromCI = !!(
    opts.isFromCI ||
    process.env['CI'] === 'true' ||
    process.env['TDDIUM'] ||
    process.env['JENKINS_URL'] ||
    process.env['bamboo.buildKey']
  )
  this.npmSession = opts.npmSession
  this.refer = opts.referer || opts.refer
  this.projectScope = opts.projectScope
  this.fullMetadata = opts.fullMetadata
  this.alwaysAuth = opts.alwaysAuth
  this.includeDeprecated = opts.includeDeprecated == null
  ? true
  : opts.includeDeprecated

  this.dirPacker = opts.dirPacker || null

  this.uid = opts.uid
  this.gid = opts.gid

  this.dmode = opts.dmode
  this.fmode = opts.fmode
  this.umask = opts.umask
}

module.exports = optCheck
function optCheck (opts) {
  return new PacoteOptions(opts)
}
