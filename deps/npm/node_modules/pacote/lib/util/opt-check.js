'use strict'

const figgyPudding = require('figgy-pudding')
const logger = require('./proclog.js')

const AUTH_REGEX = /^(?:.*:)?(token|_authToken|username|_password|password|email|always-auth|_auth|otp)$/
const SCOPE_REGISTRY_REGEX = /@.*:registry$/gi
module.exports = figgyPudding({
  annotate: {},
  cache: {},
  defaultTag: 'tag',
  dirPacker: {},
  dmode: {},
  'enjoy-by': 'enjoyBy',
  enjoyBy: {},
  before: 'enjoyBy',
  fmode: {},
  'fetch-retries': { default: 2 },
  'fetch-retry-factor': { default: 10 },
  'fetch-retry-maxtimeout': { default: 60000 },
  'fetch-retry-mintimeout': { default: 10000 },
  fullMetadata: 'full-metadata',
  'full-metadata': { default: false },
  gid: {},
  git: {},
  includeDeprecated: { default: true },
  'include-deprecated': 'includeDeprecated',
  integrity: {},
  log: { default: logger },
  memoize: {},
  offline: {},
  preferOffline: 'prefer-offline',
  'prefer-offline': {},
  preferOnline: 'prefer-online',
  'prefer-online': {},
  registry: { default: 'https://registry.npmjs.org/' },
  resolved: {},
  retry: {},
  scope: {},
  tag: { default: 'latest' },
  uid: {},
  umask: {},
  where: {}
}, {
  other (key) {
    return key.match(AUTH_REGEX) || key.match(SCOPE_REGISTRY_REGEX)
  }
})
