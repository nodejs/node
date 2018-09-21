'use strict'

require('../common-tap.js')
var test = require('tap').test
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var path = require('path')
var ms = require('mississippi')
var Tacks = require('tacks')
var File = Tacks.File

var _createCacheEntryStream = require('../../lib/search/all-package-metadata.js')._createCacheEntryStream

var PKG_DIR = path.resolve(__dirname, 'create-cache-entry-stream')
var CACHE_DIR = path.resolve(PKG_DIR, 'cache')

function setup () {
  mkdirp.sync(CACHE_DIR)
}

function cleanup () {
  rimraf.sync(PKG_DIR)
}

test('createCacheEntryStream basic', function (t) {
  setup()
  var cachePath = path.join(CACHE_DIR, '.cache.json')
  var fixture = new Tacks(File({
    '_updated': 1234,
    bar: {
      name: 'bar',
      version: '1.0.0'
    },
    foo: {
      name: 'foo',
      version: '1.0.0'
    }
  }))
  fixture.create(cachePath)
  _createCacheEntryStream(cachePath, function (err, stream, latest) {
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
      cleanup()
      t.done()
    })
  })
})

test('createCacheEntryStream empty cache', function (t) {
  setup()
  var cachePath = path.join(CACHE_DIR, '.cache.json')
  var fixture = new Tacks(File({}))
  fixture.create(cachePath)
  _createCacheEntryStream(cachePath, function (err, stream, latest) {
    t.ok(err, 'returned an error because there was no _updated')
    t.match(err.message, /Empty or invalid stream/, 'useful error message')
    t.notOk(stream, 'no stream returned')
    t.notOk(latest, 'no latest returned')
    cleanup()
    t.done()
  })
})

test('createCacheEntryStream no entry cache', function (t) {
  setup()
  var cachePath = path.join(CACHE_DIR, '.cache.json')
  var fixture = new Tacks(File({
    '_updated': 1234
  }))
  fixture.create(cachePath)
  _createCacheEntryStream(cachePath, function (err, stream, latest) {
    if (err) throw err
    t.equals(latest, 1234, '`latest` correctly extracted')
    t.ok(stream, 'returned a stream')
    var results = []
    stream.on('data', function (pkg) {
      results.push(pkg)
    })
    ms.finished(stream, function (err) {
      if (err) throw err
      t.deepEquals(results, [], 'no results')
      cleanup()
      t.done()
    })
  })
})

test('createCacheEntryStream missing cache', function (t) {
  setup()
  var cachePath = path.join(CACHE_DIR, '.cache.json')
  _createCacheEntryStream(cachePath, function (err, stream, latest) {
    t.ok(err, 'returned an error because there was no cache')
    t.equals(err.code, 'ENOENT', 'useful error message')
    t.notOk(stream, 'no stream returned')
    t.notOk(latest, 'no latest returned')
    cleanup()
    t.done()
  })
})

test('createCacheEntryStream bad syntax', function (t) {
  setup()
  var cachePath = path.join(CACHE_DIR, '.cache.json')
  var fixture = new Tacks(File('{"_updated": 1234, uh oh'))
  fixture.create(cachePath)
  _createCacheEntryStream(cachePath, function (err, stream, latest) {
    if (err) throw err
    t.equals(latest, 1234, '`latest` correctly extracted')
    t.ok(stream, 'returned a stream')
    var results = []
    stream.on('data', function (pkg) {
      results.push(pkg)
    })
    ms.finished(stream, function (err) {
      t.ok(err, 'stream errored')
      t.match(err.message, /Invalid JSON/i, 'explains there\'s a syntax error')
      t.deepEquals(results, [], 'no results')
      cleanup()
      t.done()
    })
  })
})
