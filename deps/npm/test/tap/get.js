var common = require('../common-tap.js')
var test = require('tap').test
var npm = require('../../')
var rimraf = require('rimraf')
var path = require('path')
var mr = require('npm-registry-mock')

function nop () {}

var URI = 'https://npm.registry:8043/rewrite'
var TIMEOUT = 3600
var FOLLOW = false
var STALE_OK = true
var TOKEN = 'lolbutts'
var AUTH = { token: TOKEN }
var PARAMS = {
  timeout: TIMEOUT,
  follow: FOLLOW,
  staleOk: STALE_OK,
  auth: AUTH
}
var PKG_DIR = path.resolve(__dirname, 'get-basic')
var BIGCO_SAMPLE = {
  name: '@bigco/sample',
  version: '1.2.3'
}

// mock server reference
var server

var mocks = {
  'get': {
    '/@bigco%2fsample/1.2.3': [200, BIGCO_SAMPLE]
  }
}

test('setup', function (t) {
  mr({port: common.port, mocks: mocks}, function (er, s) {
    t.ifError(er)
    npm.load({registry: common.registry}, function (er) {
      t.ifError(er)
      server = s
      t.end()
    })
  })
})

test('get call contract', function (t) {
  t.throws(function () {
    npm.registry.get(undefined, PARAMS, nop)
  }, 'requires a URI')

  t.throws(function () {
    npm.registry.get([], PARAMS, nop)
  }, 'requires URI to be a string')

  t.throws(function () {
    npm.registry.get(URI, undefined, nop)
  }, 'requires params object')

  t.throws(function () {
    npm.registry.get(URI, '', nop)
  }, 'params must be object')

  t.throws(function () {
    npm.registry.get(URI, PARAMS, undefined)
  }, 'requires callback')

  t.throws(function () {
    npm.registry.get(URI, PARAMS, 'callback')
  }, 'callback must be function')

  t.end()
})

test('basic request', function (t) {
  t.plan(6)

  var versioned = common.registry + '/underscore/1.3.3'
  npm.registry.get(versioned, PARAMS, function (er, data) {
    t.ifError(er, 'loaded specified version underscore data')
    t.equal(data.version, '1.3.3')
  })

  var rollup = common.registry + '/underscore'
  npm.registry.get(rollup, PARAMS, function (er, data) {
    t.ifError(er, 'loaded all metadata')
    t.deepEqual(data.name, 'underscore')
  })

  var scoped = common.registry + '/@bigco%2fsample/1.2.3'
  npm.registry.get(scoped, PARAMS, function (er, data) {
    t.ifError(er, 'loaded all metadata')
    t.equal(data.name, '@bigco/sample')
  })
})

test('cleanup', function (t) {
  server.close()
  rimraf.sync(PKG_DIR)

  t.end()
})
