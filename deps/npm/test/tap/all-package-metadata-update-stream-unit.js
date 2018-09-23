'use strict'

var common = require('../common-tap.js')
var npm = require('../../')
var test = require('tap').test
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var path = require('path')
var mr = require('npm-registry-mock')
var ms = require('mississippi')

var _createEntryUpdateStream = require('../../lib/search/all-package-metadata.js')._createEntryUpdateStream

var ALL = common.registry + '/-/all'
var PKG_DIR = path.resolve(__dirname, 'create-entry-update-stream')
var CACHE_DIR = path.resolve(PKG_DIR, 'cache')

var server

function setup () {
  mkdirp.sync(CACHE_DIR)
}

function cleanup () {
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

test('createEntryUpdateStream full request', function (t) {
  setup()
  server.get('/-/all').once().reply(200, {
    '_updated': 1234,
    'bar': { name: 'bar', version: '1.0.0' },
    'foo': { name: 'foo', version: '1.0.0' }
  }, {
    date: Date.now() // should never be used.
  })
  _createEntryUpdateStream(ALL, {}, 600, 0, function (err, stream, latest) {
    if (err) throw err
    t.equals(latest, 1234, '`latest` correctly extracted')
    t.ok(stream, 'returned a stream')
    var results = []
    stream.on('data', function (pkg) {
      results.push(pkg)
    })
    ms.finished(stream, function (err) {
      if (err) throw err
      t.deepEquals(results, [{
        name: 'bar',
        version: '1.0.0'
      }, {
        name: 'foo',
        version: '1.0.0'
      }])
      server.done()
      cleanup()
      t.end()
    })
  })
})

test('createEntryUpdateStream partial update', function (t) {
  setup()
  var now = Date.now()
  server.get('/-/all/since?stale=update_after&startkey=1234').once().reply(200, {
    'bar': { name: 'bar', version: '1.0.0' },
    'foo': { name: 'foo', version: '1.0.0' }
  }, {
    date: (new Date(now)).toISOString()
  })
  _createEntryUpdateStream(ALL, {}, 600, 1234, function (err, stream, latest) {
    if (err) throw err
    t.equals(latest, now, '`latest` correctly extracted from header')
    t.ok(stream, 'returned a stream')
    var results = []
    stream.on('data', function (pkg) {
      results.push(pkg)
    })
    ms.finished(stream, function (err) {
      if (err) throw err
      t.deepEquals(results, [{
        name: 'bar',
        version: '1.0.0'
      }, {
        name: 'foo',
        version: '1.0.0'
      }])
      server.done()
      cleanup()
      t.end()
    })
  })
})

test('createEntryUpdateStream authed request', function (t) {
  setup()
  var token = 'thisisanauthtoken'
  server.get('/-/all', { authorization: 'Bearer ' + token }).once().reply(200, {
    '_updated': 1234,
    'bar': { name: 'bar', version: '1.0.0' },
    'foo': { name: 'foo', version: '1.0.0' }
  }, {
    date: Date.now() // should never be used.
  })
  _createEntryUpdateStream(ALL, { token: token }, 600, 0, function (err, stream, latest) {
    if (err) throw err
    t.equals(latest, 1234, '`latest` correctly extracted')
    t.ok(stream, 'returned a stream')
    var results = []
    stream.on('data', function (pkg) {
      results.push(pkg)
    })
    ms.finished(stream, function (err) {
      if (err) throw err
      t.deepEquals(results, [{
        name: 'bar',
        version: '1.0.0'
      }, {
        name: 'foo',
        version: '1.0.0'
      }])
      server.done()
      cleanup()
      t.end()
    })
  })
})

test('createEntryUpdateStream bad auth', function (t) {
  setup()
  var token = 'thisisanauthtoken'
  server.get('/-/all', { authorization: 'Bearer ' + token }).once().reply(401, {
    error: 'unauthorized search request'
  })
  _createEntryUpdateStream(ALL, { token: token }, 600, 0, function (err, stream, latest) {
    t.ok(err, 'got an error from auth failure')
    t.notOk(stream, 'no stream returned')
    t.notOk(latest, 'no latest returned')
    t.match(err, /unauthorized/, 'failure message from request used')
    server.done()
    cleanup()
    t.end()
  })
})

test('createEntryUpdateStream not stale', function (t) {
  setup()
  var now = Date.now()
  var staleness = 600
  _createEntryUpdateStream(ALL, {}, staleness, now, function (err, stream, latest) {
    t.ifError(err, 'completed successfully')
    t.notOk(stream, 'no stream returned')
    t.notOk(latest, 'no latest returned')
    server.done()
    cleanup()
    t.end()
  })
})

test('cleanup', function (t) {
  cleanup()
  server.close()
  t.pass('all done')
  t.done()
})
