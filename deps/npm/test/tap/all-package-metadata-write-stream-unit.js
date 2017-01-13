'use strict'

var common = require('../common-tap.js')
var npm = require('../../')
var test = require('tap').test
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var path = require('path')
var fs = require('fs')
var ms = require('mississippi')

var _createCacheWriteStream = require('../../lib/search/all-package-metadata.js')._createCacheWriteStream

var PKG_DIR = path.resolve(__dirname, 'create-cache-write-stream')
var CACHE_DIR = path.resolve(PKG_DIR, 'cache')

function setup () {
  mkdirp.sync(CACHE_DIR)
}

function cleanup () {
  rimraf.sync(PKG_DIR)
}

function fromArray (array) {
  var idx = 0
  return ms.from.obj(function (size, next) {
    next(null, array[idx++] || null)
  })
}

test('setup', function (t) {
  // This is pretty much only used for `getCacheStat` in the implementation
  npm.load({ cache: CACHE_DIR, registry: common.registry }, function (err) {
    t.ifError(err, 'npm successfully loaded')
    t.done()
  })
})

test('createCacheEntryStream basic', function (t) {
  setup()
  var cachePath = path.join(CACHE_DIR, '.cache.json')
  var latest = 12345
  var src = [
    { name: 'bar', version: '1.0.0' },
    { name: 'foo', version: '1.0.0' }
  ]
  var srcStream = fromArray(src)
  _createCacheWriteStream(cachePath, latest, function (err, stream) {
    if (err) throw err
    t.ok(stream, 'returned a stream')
    stream = ms.pipeline.obj(srcStream, stream)
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
      var fileData = JSON.parse(fs.readFileSync(cachePath))
      t.ok(fileData, 'cache contents written to the right file')
      t.deepEquals(fileData, {
        '_updated': latest,
        bar: {
          name: 'bar',
          version: '1.0.0'
        },
        foo: {
          name: 'foo',
          version: '1.0.0'
        }
      }, 'cache contents based on what was written')
      cleanup()
      t.done()
    })
  })
})

test('createCacheEntryStream no entries', function (t) {
  cleanup() // wipe out the cache dir
  var cachePath = path.join(CACHE_DIR, '.cache.json')
  var latest = 12345
  var src = []
  var srcStream = fromArray(src)
  _createCacheWriteStream(cachePath, latest, function (err, stream) {
    if (err) throw err
    t.ok(stream, 'returned a stream')
    stream = ms.pipeline.obj(srcStream, stream)
    stream.resume()
    ms.finished(stream, function (err) {
      if (err) throw err
      var fileData = JSON.parse(fs.readFileSync(cachePath))
      t.ok(fileData, 'cache file exists and has stuff in it')
      cleanup()
      t.done()
    })
  })
})

test('createCacheEntryStream missing cache dir', function (t) {
  setup()
  var cachePath = path.join(CACHE_DIR, '.cache.json')
  var latest = 12345
  var src = []
  var srcStream = fromArray(src)
  _createCacheWriteStream(cachePath, latest, function (err, stream) {
    if (err) throw err
    t.ok(stream, 'returned a stream')
    stream = ms.pipeline.obj(srcStream, stream)
    stream.on('data', function (pkg) {
      t.notOk(pkg, 'stream should not have output any data')
    })
    ms.finished(stream, function (err) {
      if (err) throw err
      var fileData = JSON.parse(fs.readFileSync(cachePath))
      t.ok(fileData, 'cache contents written to the right file')
      t.deepEquals(fileData, {
        '_updated': latest
      }, 'cache still contains `_updated`')
      cleanup()
      t.done()
    })
  })
})
