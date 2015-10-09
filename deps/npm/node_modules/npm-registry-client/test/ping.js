var test = require('tap').test

var server = require('./lib/server.js')
var common = require('./lib/common.js')
var client = common.freshClient()

function nop () {}

var TOKEN = 'not-bad-meaning-bad-but-bad-meaning-wombat'
var AUTH = { token: TOKEN }
var PARAMS = { auth: AUTH }
var DEP_USER = 'username'
var HOST = 'localhost'

test('ping call contract', function (t) {
  t.throws(function () {
    client.ping(undefined, AUTH, nop)
  }, 'requires a URI')

  t.throws(function () {
    client.ping([], AUTH, nop)
  }, 'requires URI to be a string')

  t.throws(function () {
    client.ping(common.registry, undefined, nop)
  }, 'requires params object')

  t.throws(function () {
    client.ping(common.registry, '', nop)
  }, 'params must be object')

  t.throws(function () {
    client.ping(common.registry, AUTH, undefined)
  }, 'requires callback')

  t.throws(function () {
    client.ping(common.registry, AUTH, 'callback')
  }, 'callback must be function')

  t.throws(
    function () {
      var params = {}
      client.ping(common.registry, params, nop)
    },
    { name: 'AssertionError', message: 'must pass auth to ping' },
    'must pass auth to ping'
  )

  t.end()
})

test('ping', function (t) {
  server.expect('GET', '/-/ping?write=true', function (req, res) {
    t.equal(req.method, 'GET')
    res.statusCode = 200
    res.json({
      ok: true,
      host: HOST,
      peer: HOST,
      username: DEP_USER
    })
  })

  client.ping(common.registry, PARAMS, function (error, found) {
    t.ifError(error, 'no errors')
    var wanted = {
      ok: true,
      host: HOST,
      peer: HOST,
      username: DEP_USER
    }
    t.same(found, wanted)
    server.close()
    t.end()
  })
})
