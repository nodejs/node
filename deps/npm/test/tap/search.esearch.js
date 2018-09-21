var common = require('../common-tap.js')
var finished = require('mississippi').finished
var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var npm = require('../../')
var osenv = require('osenv')
var path = require('path')
var rimraf = require('rimraf')
var test = require('tap').test

var SEARCH = '/-/v1/search'
var PKG_DIR = path.resolve(__dirname, 'create-entry-update-stream')
var CACHE_DIR = path.resolve(PKG_DIR, 'cache')

var esearch = require('../../lib/search/esearch')

var server

function setup () {
  cleanup()
  mkdirp.sync(CACHE_DIR)
  process.chdir(CACHE_DIR)
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(PKG_DIR)
}

test('setup', function (t) {
  mr({port: common.port, throwOnUnmatched: true}, function (err, s) {
    t.ifError(err, 'registry mocked successfully')
    npm.load({ cache: CACHE_DIR, registry: common.registry }, function (err) {
      t.ifError(err, 'npm loaded successfully')
      server = s
      t.pass('all set up')
      t.done()
    })
  })
})

test('basic test', function (t) {
  setup()
  server.get(SEARCH + '?text=oo&size=1').once().reply(200, {
    objects: [
      { package: { name: 'cool', version: '1.0.0' } },
      { package: { name: 'foo', version: '2.0.0' } }
    ]
  })
  var results = []
  var s = esearch({
    include: ['oo'],
    limit: 1
  })
  t.ok(s, 'got a stream')
  s.on('data', function (d) {
    results.push(d)
  })
  finished(s, function (err) {
    if (err) { throw err }
    t.ok(true, 'stream finished without error')
    t.deepEquals(results, [{
      name: 'cool',
      version: '1.0.0',
      description: null,
      maintainers: null,
      keywords: null,
      date: null
    }, {
      name: 'foo',
      version: '2.0.0',
      description: null,
      maintainers: null,
      keywords: null,
      date: null
    }])
    server.done()
    t.done()
  })
})

test('only returns certain fields for each package', function (t) {
  setup()
  var date = new Date()
  server.get(SEARCH + '?text=oo&size=1').once().reply(200, {
    objects: [{
      package: {
        name: 'cool',
        version: '1.0.0',
        description: 'desc',
        maintainers: [
          {username: 'x', email: 'a@b.c'},
          {username: 'y', email: 'c@b.a'}
        ],
        keywords: ['a', 'b', 'c'],
        date: date.toISOString(),
        extra: 'lol'
      }
    }]
  })
  var results = []
  var s = esearch({
    include: ['oo'],
    limit: 1
  })
  t.ok(s, 'got a stream')
  s.on('data', function (d) {
    results.push(d)
  })
  finished(s, function (err) {
    if (err) { throw err }
    t.ok(true, 'stream finished without error')
    t.deepEquals(results, [{
      name: 'cool',
      version: '1.0.0',
      description: 'desc',
      maintainers: [
        {username: 'x', email: 'a@b.c'},
        {username: 'y', email: 'c@b.a'}
      ],
      keywords: ['a', 'b', 'c'],
      date: date
    }])
    server.done()
    t.done()
  })
})

test('accepts a limit option', function (t) {
  setup()
  server.get(SEARCH + '?text=oo&size=3').once().reply(200, {
    objects: [
      { package: { name: 'cool', version: '1.0.0' } },
      { package: { name: 'cool', version: '1.0.0' } }
    ]
  })
  var results = 0
  var s = esearch({
    include: ['oo'],
    limit: 3
  })
  s.on('data', function () { results++ })
  finished(s, function (err) {
    if (err) { throw err }
    t.ok(true, 'request sent with correct size')
    t.equal(results, 2, 'behaves fine with fewer results than size')
    server.done()
    t.done()
  })
})

test('passes foo:bar syntax params directly', function (t) {
  setup()
  server.get(SEARCH + '?text=foo%3Abar&size=1').once().reply(200, {
    objects: []
  })
  var s = esearch({
    include: ['foo:bar'],
    limit: 1
  })
  s.on('data', function () {})
  finished(s, function (err) {
    if (err) { throw err }
    t.ok(true, 'request sent with correct params')
    server.done()
    t.done()
  })
})

test('space-separates and URI-encodes multiple search params', function (t) {
  setup()
  server.get(SEARCH + '?text=foo%20bar%3Abaz%20quux%3F%3D&size=1').once().reply(200, {
    objects: []
  })
  var s = esearch({
    include: ['foo', 'bar:baz', 'quux?='],
    limit: 1
  })
  s.on('data', function () {})
  finished(s, function (err) {
    if (err) { throw err }
    t.ok(true, 'request sent with correct params')
    server.done()
    t.done()
  })
})

test('cleanup', function (t) {
  server.close()
  cleanup()
  t.done()
})
