var test = require('tap').test

var server = require('./lib/server.js')
var common = require('./lib/common.js')
var client = common.freshClient()

var cache = require('./fixtures/underscore/cache.json')

function nop () {}

var REV = '/-rev/72-47f2986bfd8e8b55068b204588bbf484'
var URI = 'http://localhost:1337/underscore'
var TOKEN = 'of-glad-tidings'
var VERSION = '1.3.2'
var AUTH = {
  token: TOKEN
}
var PARAMS = {
  version: VERSION,
  auth: AUTH
}

test('unpublish call contract', function (t) {
  t.throws(function () {
    client.unpublish(undefined, AUTH, nop)
  }, 'requires a URI')

  t.throws(function () {
    client.unpublish([], AUTH, nop)
  }, 'requires URI to be a string')

  t.throws(function () {
    client.unpublish(URI, undefined, nop)
  }, 'requires params object')

  t.throws(function () {
    client.unpublish(URI, '', nop)
  }, 'params must be object')

  t.throws(function () {
    client.unpublish(URI, AUTH, undefined)
  }, 'requires callback')

  t.throws(function () {
    client.unpublish(URI, AUTH, 'callback')
  }, 'callback must be function')

  t.throws(
    function () {
      var params = {
        version: VERSION
      }
      client.unpublish(URI, params, nop)
    },
    { name: 'AssertionError', message: 'must pass auth to unpublish' },
    'must pass auth to unpublish'
  )

  t.end()
})

test('unpublish a package', function (t) {
  server.expect('GET', '/underscore?write=true', function (req, res) {
    t.equal(req.method, 'GET')

    res.json(cache)
  })

  server.expect('PUT', '/underscore' + REV, function (req, res) {
    t.equal(req.method, 'PUT')

    var b = ''
    req.setEncoding('utf-8')
    req.on('data', function (d) {
      b += d
    })

    req.on('end', function () {
      var updated = JSON.parse(b)
      t.notOk(updated.versions[VERSION])
    })

    res.json(cache)
  })

  server.expect('GET', '/underscore', function (req, res) {
    t.equal(req.method, 'GET')

    res.json(cache)
  })

  server.expect('DELETE', '/underscore/-/underscore-1.3.2.tgz' + REV, function (req, res) {
    t.equal(req.method, 'DELETE')

    res.json({ unpublished: true })
  })

  client.unpublish(URI, PARAMS, function (error) {
    t.ifError(error, 'no errors')

    server.close()
    t.end()
  })
})
