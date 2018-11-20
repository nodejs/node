var test = require('tap').test

var server = require('./lib/server.js')
var common = require('./lib/common.js')
var client = common.freshClient()

function nop () {}

var URI = 'https://npm.registry:8043/rewrite'
var USERNAME = 'sample'
var PASSWORD = '%1234@asdf%'
var EMAIL = 'i@izs.me'
var AUTH = {
  username: USERNAME,
  password: PASSWORD,
  email: EMAIL
}
var PARAMS = {
  username: USERNAME,
  auth: AUTH
}
var USERS = [
  'benjamincoe',
  'seldo',
  'ceejbot'
]

test('stars call contract', function (t) {
  t.throws(function () {
    client.stars(undefined, PARAMS, nop)
  }, 'requires a URI')

  t.throws(function () {
    client.stars([], PARAMS, nop)
  }, 'requires URI to be a string')

  t.throws(function () {
    client.stars(URI, undefined, nop)
  }, 'requires params object')

  t.throws(function () {
    client.stars(URI, '', nop)
  }, 'params must be object')

  t.throws(function () {
    client.stars(URI, PARAMS, undefined)
  }, 'requires callback')

  t.throws(function () {
    client.stars(URI, PARAMS, 'callback')
  }, 'callback must be function')

  t.test('no username anywhere', function (t) {
    var params = {}
    client.stars(URI, params, function (err) {
      t.equal(
        err && err.message,
        'must pass either username or auth to stars',
        'username must not be empty')
      t.end()
    })
  })

  t.end()
})

test('get the stars for a package', function (t) {
  server.expect('GET', '/-/_view/starredByUser?key=%22sample%22', function (req, res) {
    t.equal(req.method, 'GET')

    res.json(USERS)
  })

  client.stars('http://localhost:1337/', PARAMS, function (er, info) {
    t.ifError(er, 'no errors')
    t.deepEqual(info, USERS, 'got the list of users')

    t.end()
  })
})
