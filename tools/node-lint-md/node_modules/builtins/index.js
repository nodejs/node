'use strict'

var semver = require('semver')

module.exports = function ({
  version = process.version,
  experimental = false
} = {}) {
  var coreModules = [
    'assert',
    'buffer',
    'child_process',
    'cluster',
    'console',
    'constants',
    'crypto',
    'dgram',
    'dns',
    'domain',
    'events',
    'fs',
    'http',
    'https',
    'module',
    'net',
    'os',
    'path',
    'punycode',
    'querystring',
    'readline',
    'repl',
    'stream',
    'string_decoder',
    'sys',
    'timers',
    'tls',
    'tty',
    'url',
    'util',
    'vm',
    'zlib'
  ]

  if (semver.lt(version, '6.0.0')) coreModules.push('freelist')
  if (semver.gte(version, '1.0.0')) coreModules.push('v8')
  if (semver.gte(version, '1.1.0')) coreModules.push('process')
  if (semver.gte(version, '8.0.0')) coreModules.push('inspector')
  if (semver.gte(version, '8.1.0')) coreModules.push('async_hooks')
  if (semver.gte(version, '8.4.0')) coreModules.push('http2')
  if (semver.gte(version, '8.5.0')) coreModules.push('perf_hooks')
  if (semver.gte(version, '10.0.0')) coreModules.push('trace_events')

  if (
    semver.gte(version, '10.5.0') &&
    (experimental || semver.gte(version, '12.0.0'))
  ) {
    coreModules.push('worker_threads')
  }
  if (semver.gte(version, '12.16.0') && experimental) {
    coreModules.push('wasi')
  }
  
  return coreModules
}
