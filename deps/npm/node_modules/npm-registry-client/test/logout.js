var test = require('tap').test

var server = require('./lib/server.js')
var common = require('./lib/common.js')
var client = common.freshClient()

function nop () {}

var URI = 'http://localhost:1337/rewrite'
var TOKEN = 'b00b00feed'
var PARAMS = {
  auth: {
    token: TOKEN
  }
}

test('logout call contract', function (t) {
  t.throws(function () {
    client.logout(undefined, PARAMS, nop)
  }, 'requires a URI')

  t.throws(function () {
    client.logout([], PARAMS, nop)
  }, 'requires URI to be a string')

  t.throws(function () {
    client.logout(URI, undefined, nop)
  }, 'requires params object')

  t.throws(function () {
    client.logout(URI, '', nop)
  }, 'params must be object')

  t.throws(function () {
    client.logout(URI, PARAMS, undefined)
  }, 'requires callback')

  t.throws(function () {
    client.logout(URI, PARAMS, 'callback')
  }, 'callback must be function')

  t.throws(
    function () {
      var params = {
        auth: {}
      }
      client.logout(URI, params, nop)
    },
    { name: 'AssertionError', message: 'can only log out for token auth' },
    'auth must include token'
  )

  t.end()
})

test('log out from a token-based registry', function (t) {
  server.expect('DELETE', '/-/user/token/' + TOKEN, function (req, res) {
    t.equal(req.method, 'DELETE')
    t.equal(req.headers.authorization, 'Bearer ' + TOKEN, 'request is authed')

    res.json({message: 'ok'})
  })

  client.logout(URI, PARAMS, function (er) {
    t.ifError(er, 'no errors')

    t.end()
  })
})

test('cleanup', function (t) {
  server.close()
  t.end()
})
