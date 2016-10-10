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

test('distTags.update call contract', function (t) {
  t.throws(function () {
    client.distTags.update(undefined, AUTH, nop)
  }, 'requires a URI')

  t.throws(function () {
    client.distTags.update([], PARAMS, nop)
  }, 'requires URI to be a string')

  t.throws(function () {
    client.distTags.update(BASE_URL, undefined, nop)
  }, 'requires params object')

  t.throws(function () {
    client.distTags.update(BASE_URL, '', nop)
  }, 'params must be object')

  t.throws(function () {
    client.distTags.update(BASE_URL, PARAMS, undefined)
  }, 'requires callback')

  t.throws(function () {
    client.distTags.update(BASE_URL, PARAMS, 'callback')
  }, 'callback must be function')

  t.throws(
    function () {
      var params = { distTags: DIST_TAGS, auth: AUTH }
      client.distTags.update(BASE_URL, params, nop)
    },
    {
      name: 'AssertionError',
      message: 'must pass package name to distTags.update'
    },
    'distTags.update must include package name'
  )

  t.throws(
    function () {
      var params = { 'package': PACKAGE, auth: AUTH }
      client.distTags.update(BASE_URL, params, nop)
    },
    {
      name: 'AssertionError',
      message: 'must pass distTags map to distTags.update'
    },
    'distTags.update must include dist-tags'
  )

  t.throws(
    function () {
      var params = { 'package': PACKAGE, distTags: DIST_TAGS }
      client.distTags.update(BASE_URL, params, nop)
    },
    { name: 'AssertionError', message: 'must pass auth to distTags.update' },
    'distTags.update must include auth'
  )

  t.end()
})

test('update dist-tags for a package', function (t) {
  server.expect('POST', URI, function (req, res) {
    t.equal(req.method, 'POST')

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

  client.distTags.update(BASE_URL, PARAMS, function (error, data) {
    t.ifError(error, 'no errors')
    t.ok(data.a && data.b, 'dist-tags set')

    server.close()
    t.end()
  })
})
