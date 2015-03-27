var test = require('tap').test

var server = require('./lib/server.js')
var common = require('./lib/common.js')
var cache = require('./fixtures/underscore/cache.json')

var client = common.freshClient()

function nop () {}

var URI = 'https://npm.registry:8043/rewrite'
var VERSION = '1.3.2'
var MESSAGE = 'uhhh'
var TOKEN = 'lolbutts'
var AUTH = {
  token: TOKEN
}
var PARAMS = {
  version: VERSION,
  message: MESSAGE,
  auth: AUTH
}

test('deprecate call contract', function (t) {
  t.throws(function () {
    client.deprecate(undefined, PARAMS, nop)
  }, 'requires a URI')

  t.throws(function () {
    client.deprecate([], PARAMS, nop)
  }, 'requires URI to be a string')

  t.throws(function () {
    client.deprecate(URI, undefined, nop)
  }, 'requires params object')

  t.throws(function () {
    client.deprecate(URI, '', nop)
  }, 'params must be object')

  t.throws(function () {
    client.deprecate(URI, PARAMS, undefined)
  }, 'requires callback')

  t.throws(function () {
    client.deprecate(URI, PARAMS, 'callback')
  }, 'callback must be function')

  t.throws(
    function () {
      var params = {
        message: MESSAGE,
        auth: AUTH
      }
      client.deprecate(URI, params, nop)
    },
    { name: 'AssertionError', message: 'must pass version to deprecate' },
    'params must include version to deprecate'
  )

  t.throws(
    function () {
      var params = {
        version: VERSION,
        auth: AUTH
      }
      client.deprecate(URI, params, nop)
    },
    { name: 'AssertionError', message: 'must pass message to deprecate' },
    'params must include deprecation message'
  )

  t.throws(
    function () {
      var params = {
        version: VERSION,
        message: MESSAGE
      }
      client.deprecate(URI, params, nop)
    },
    { name: 'AssertionError', message: 'must pass auth to deprecate' },
    'params must include auth'
  )

  t.test('malformed semver in deprecation', function (t) {
    var params = {
      version: '-9001',
      message: MESSAGE,
      auth: AUTH
    }
    client.deprecate(URI, params, function (err) {
      t.equal(
        err && err.message,
        'invalid version range: -9001',
        'got expected semver validation failure'
      )
      t.end()
    })
  })

  t.end()
})

test('deprecate a package', function (t) {
  server.expect('GET', '/underscore?write=true', function (req, res) {
    t.equal(req.method, 'GET')

    res.json(cache)
  })

  server.expect('PUT', '/underscore', function (req, res) {
    t.equal(req.method, 'PUT')

    var b = ''
    req.setEncoding('utf8')
    req.on('data', function (d) {
      b += d
    })

    req.on('end', function () {
      var updated = JSON.parse(b)

      var undeprecated = [
        '1.0.3', '1.0.4', '1.1.0', '1.1.1', '1.1.2', '1.1.3', '1.1.4', '1.1.5', '1.1.6',
        '1.1.7', '1.2.0', '1.2.1', '1.2.2', '1.2.3', '1.2.4', '1.3.0', '1.3.1', '1.3.3'
      ]
      for (var i = 0; i < undeprecated.length; i++) {
        var current = undeprecated[i]
        t.notEqual(
          updated.versions[current].deprecated,
          MESSAGE,
          current + ' not deprecated'
        )
      }

      t.equal(
        updated.versions[VERSION].deprecated,
        MESSAGE,
        VERSION + ' deprecated'
      )
      res.statusCode = 201
      res.json({ deprecated: true })
    })
  })

  client.deprecate(
    common.registry + '/underscore',
    PARAMS,
    function (er, data) {
      t.ifError(er)
      t.ok(data.deprecated, 'was deprecated')

      t.end()
    }
  )
})
