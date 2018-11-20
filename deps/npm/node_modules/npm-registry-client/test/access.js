var test = require('tap').test

var server = require('./lib/server.js')
var common = require('./lib/common.js')
var client = common.freshClient()

function nop () {}

var URI = 'http://localhost:1337/-/package/underscore/access'
var TOKEN = 'foo'
var AUTH = {
  token: TOKEN
}
var LEVEL = 'public'
var PARAMS = {
  level: LEVEL,
  auth: AUTH
}

test('access call contract', function (t) {
  t.throws(function () {
    client.access(undefined, AUTH, nop)
  }, 'requires a URI')

  t.throws(function () {
    client.access([], PARAMS, nop)
  }, 'requires URI to be a string')

  t.throws(function () {
    client.access(URI, undefined, nop)
  }, 'requires params object')

  t.throws(function () {
    client.access(URI, '', nop)
  }, 'params must be object')

  t.throws(function () {
    client.access(URI, PARAMS, undefined)
  }, 'requires callback')

  t.throws(function () {
    client.access(URI, PARAMS, 'callback')
  }, 'callback must be function')

  t.throws(
    function () {
      var params = {
        auth: AUTH
      }
      client.access(URI, params, nop)
    },
    { name: 'AssertionError', message: 'must pass level to access' },
    'access must include level'
  )

  t.throws(
    function () {
      var params = {
        level: LEVEL
      }
      client.access(URI, params, nop)
    },
    { name: 'AssertionError', message: 'must pass auth to access' },
    'access must include auth'
  )

  t.end()
})

test('set access level on a package', function (t) {
  server.expect('POST', '/-/package/underscore/access', function (req, res) {
    t.equal(req.method, 'POST')

    var b = ''
    req.setEncoding('utf8')
    req.on('data', function (d) {
      b += d
    })

    req.on('end', function () {
      var updated = JSON.parse(b)

      t.deepEqual(updated, { access: 'public' })

      res.statusCode = 201
      res.json({ accessChanged: true })
    })
  })

  client.access(URI, PARAMS, function (error, data) {
    t.ifError(error, 'no errors')
    t.ok(data.accessChanged, 'access level set')

    t.end()
  })
})
