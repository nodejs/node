var test = require('tap').test

var server = require('./lib/server.js')
var common = require('./lib/common.js')
var client = common.freshClient()

function nop () {}

var BASE_URL = 'http://localhost:1337/'
var URI = '/-/package/underscore/dist-tags/test'
var TOKEN = 'foo'
var AUTH = {
  token: TOKEN
}
var PACKAGE = 'underscore'
var DIST_TAG = 'test'
var PARAMS = {
  'package': PACKAGE,
  distTag: DIST_TAG,
  auth: AUTH
}

test('distTags.rm call contract', function (t) {
  t.throws(function () {
    client.distTags.rm(undefined, AUTH, nop)
  }, 'requires a URI')

  t.throws(function () {
    client.distTags.rm([], PARAMS, nop)
  }, 'requires URI to be a string')

  t.throws(function () {
    client.distTags.rm(BASE_URL, undefined, nop)
  }, 'requires params object')

  t.throws(function () {
    client.distTags.rm(BASE_URL, '', nop)
  }, 'params must be object')

  t.throws(function () {
    client.distTags.rm(BASE_URL, PARAMS, undefined)
  }, 'requires callback')

  t.throws(function () {
    client.distTags.rm(BASE_URL, PARAMS, 'callback')
  }, 'callback must be function')

  t.throws(
    function () {
      var params = {
        distTag: DIST_TAG,
        auth: AUTH
      }
      client.distTags.rm(BASE_URL, params, nop)
    },
    {
      name: 'AssertionError',
      message: 'must pass package name to distTags.rm'
    },
    'distTags.rm must include package name'
  )

  t.throws(
    function () {
      var params = {
        'package': PACKAGE,
        auth: AUTH
      }
      client.distTags.rm(BASE_URL, params, nop)
    },
    {
      name: 'AssertionError',
      message: 'must pass package distTag name to distTags.rm'
    },
    'distTags.rm must include dist-tag'
  )

  t.throws(
    function () {
      var params = {
        'package': PACKAGE,
        distTag: DIST_TAG
      }
      client.distTags.rm(BASE_URL, params, nop)
    },
    { name: 'AssertionError', message: 'must pass auth to distTags.rm' },
    'distTags.rm must include auth'
  )

  t.end()
})

test('remove a dist-tag from a package', function (t) {
  server.expect('DELETE', URI, function (req, res) {
    t.equal(req.method, 'DELETE')

    var b = ''
    req.setEncoding('utf8')
    req.on('data', function (d) {
      b += d
    })

    req.on('end', function () {
      t.notOk(b, 'got no message body')

      res.statusCode = 200
      res.json({})
    })
  })

  client.distTags.rm(BASE_URL, PARAMS, function (error, data) {
    t.ifError(error, 'no errors')
    t.notOk(data.test, 'dist-tag removed')

    t.end()
  })
})
