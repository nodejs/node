var test = require('tap').test

var server = require('./lib/server.js')
var common = require('./lib/common.js')
var client = common.freshClient()

function nop () {}

var BASE_URL = 'http://localhost:1337/'
var URI = '/-/package/underscore/dist-tags'
var TOKEN = 'foo'
var AUTH = {
  token: TOKEN
}
var PACKAGE = 'underscore'
var DIST_TAGS = {
  'a': '8.0.8',
  'b': '3.0.3'
}
var PARAMS = {
  'package': PACKAGE,
  distTags: DIST_TAGS,
  auth: AUTH
}

test('distTags.set call contract', function (t) {
  t.throws(function () {
    client.distTags.set(undefined, AUTH, nop)
  }, 'requires a URI')

  t.throws(function () {
    client.distTags.set([], PARAMS, nop)
  }, 'requires URI to be a string')

  t.throws(function () {
    client.distTags.set(BASE_URL, undefined, nop)
  }, 'requires params object')

  t.throws(function () {
    client.distTags.set(BASE_URL, '', nop)
  }, 'params must be object')

  t.throws(function () {
    client.distTags.set(BASE_URL, PARAMS, undefined)
  }, 'requires callback')

  t.throws(function () {
    client.distTags.set(BASE_URL, PARAMS, 'callback')
  }, 'callback must be function')

  t.throws(
    function () {
      var params = {
        distTags: DIST_TAGS,
        auth: AUTH
      }
      client.distTags.set(BASE_URL, params, nop)
    },
    {
      name: 'AssertionError',
      message: 'must pass package name to distTags.set'
    },
    'distTags.set must include package name'
  )

  t.throws(
    function () {
      var params = {
        'package': PACKAGE,
        auth: AUTH
      }
      client.distTags.set(BASE_URL, params, nop)
    },
    {
      name: 'AssertionError',
      message: 'must pass distTags map to distTags.set'
    },
    'distTags.set must include dist-tags'
  )

  t.throws(
    function () {
      var params = {
        'package': PACKAGE,
        distTags: DIST_TAGS
      }
      client.distTags.set(BASE_URL, params, nop)
    },
    { name: 'AssertionError', message: 'must pass auth to distTags.set' },
    'distTags.set must include auth'
  )

  t.end()
})

test('set dist-tags for a package', function (t) {
  server.expect('PUT', URI, function (req, res) {
    t.equal(req.method, 'PUT')

    var b = ''
    req.setEncoding('utf8')
    req.on('data', function (d) {
      b += d
    })

    req.on('end', function () {
      var d = JSON.parse(b)
      t.deepEqual(d, DIST_TAGS, 'got back tags')

      res.statusCode = 200
      res.json(DIST_TAGS)
    })
  })

  client.distTags.set(BASE_URL, PARAMS, function (error, data) {
    t.ifError(error, 'no errors')
    t.ok(data.a && data.b, 'dist-tags set')

    server.close()
    t.end()
  })
})
