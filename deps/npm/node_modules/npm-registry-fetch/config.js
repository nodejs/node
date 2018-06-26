'use strict'

const pkg = require('./package.json')
const pudding = require('figgy-pudding')
const silentLog = require('./silentlog.js')

const AUTH_REGEX = /^(?:.*:)?(_authToken|username|_password|password|email|always-auth|_auth|otp)$/
const SCOPE_REGISTRY_REGEX = /@.*:registry$/gi
const RegFetchConfig = pudding({
  'agent': {},
  'algorithms': {},
  'body': {},
  'ca': {},
  'cache': {},
  'cert': {},
  'fetch-retries': {},
  'fetch-retry-factor': {},
  'fetch-retry-maxtimeout': {},
  'fetch-retry-mintimeout': {},
  'gid': {},
  'headers': {},
  'integrity': {},
  'is-from-ci': 'isFromCI',
  'isFromCI': {
    default () {
      return (
        process.env['CI'] === 'true' ||
        process.env['TDDIUM'] ||
        process.env['JENKINS_URL'] ||
        process.env['bamboo.buildKey'] ||
        process.env['GO_PIPELINE_NAME']
      )
    }
  },
  'key': {},
  'local-address': {},
  'log': {
    default: silentLog
  },
  'max-sockets': 'maxsockets',
  'maxsockets': {
    default: 12
  },
  'memoize': {},
  'method': {
    default: 'GET'
  },
  'noproxy': {},
  'npm-session': 'npmSession',
  'npmSession': {},
  'offline': {},
  'otp': {},
  'prefer-offline': {},
  'prefer-online': {},
  'projectScope': {},
  'project-scope': 'projectScope',
  'Promise': {},
  'proxy': {},
  'query': {},
  'refer': {},
  'referer': 'refer',
  'registry': {
    default: 'https://registry.npmjs.org/'
  },
  'retry': {},
  'scope': {},
  'spec': {},
  'strict-ssl': {},
  'timeout': {},
  'uid': {},
  'user-agent': {
    default: `${
      pkg.name
    }@${
      pkg.version
    }/node@${
      process.version
    }+${
      process.arch
    } (${
      process.platform
    })`
  }
}, {
  other (key) {
    return key.match(AUTH_REGEX) || key.match(SCOPE_REGISTRY_REGEX)
  }
})

module.exports = config
function config (opts) {
  opts = opts || {}
  return RegFetchConfig(opts, opts.config)
}
