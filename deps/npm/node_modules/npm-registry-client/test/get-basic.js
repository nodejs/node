var test = require('tap').test

var server = require('./lib/server.js')
var common = require('./lib/common.js')
var client = common.freshClient()

var us = require('./fixtures/underscore/1.3.3/cache.json')
var usroot = require('./fixtures/underscore/cache.json')

function nop () {}

var URI = 'https://npm.registry:8043/rewrite'
var TIMEOUT = 3600
var FOLLOW = false
var STALE_OK = true
var TOKEN = 'lolbutts'
var AUTH = {
  token: TOKEN
}
var PARAMS = {
  timeout: TIMEOUT,
  follow: FOLLOW,
  staleOk: STALE_OK,
  auth: AUTH
}

test('get call contract', function (t) {
  t.throws(function () {
    client.get(undefined, PARAMS, nop)
  }, 'requires a URI')

  t.throws(function () {
    client.get([], PARAMS, nop)
  }, 'requires URI to be a string')

  t.throws(function () {
    client.get(URI, undefined, nop)
  }, 'requires params object')

  t.throws(function () {
    client.get(URI, '', nop)
  }, 'params must be object')

  t.throws(function () {
    client.get(URI, PARAMS, undefined)
  }, 'requires callback')

  t.throws(function () {
    client.get(URI, PARAMS, 'callback')
  }, 'callback must be function')

  t.end()
})

test('basic request', function (t) {
  server.expect('/underscore/1.3.3', function (req, res) {
    res.json(us)
  })

  server.expect('/underscore', function (req, res) {
    res.json(usroot)
  })

  server.expect('/@bigco%2funderscore', function (req, res) {
    res.json(usroot)
  })

  t.plan(3)
  client.get('http://localhost:1337/underscore/1.3.3', PARAMS, function (er, data) {
    t.deepEqual(data, us)
  })

  client.get('http://localhost:1337/underscore', PARAMS, function (er, data) {
    t.deepEqual(data, usroot)
  })

  client.get('http://localhost:1337/@bigco%2funderscore', PARAMS, function (er, data) {
    t.deepEqual(data, usroot)
  })
})

test('cleanup', function (t) {
  server.close()
  t.end()
})
