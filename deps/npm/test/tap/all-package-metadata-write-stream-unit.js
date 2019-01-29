'use strict'

const common = require('../common-tap.js')
const getStream = require('get-stream')
const npm = require('../../')
const test = require('tap').test
const mkdirp = require('mkdirp')
const rimraf = require('rimraf')
const path = require('path')
const fs = require('fs')
const ms = require('mississippi')

const _createCacheWriteStream = require('../../lib/search/all-package-metadata.js')._createCacheWriteStream

const PKG_DIR = path.resolve(__dirname, 'create-cache-write-stream')
const CACHE_DIR = path.resolve(PKG_DIR, 'cache')

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
  return _createCacheWriteStream(cachePath, latest, {
    cache: CACHE_DIR
  }).then(stream => {
    t.ok(stream, 'returned a stream')
    stream = ms.pipeline.obj(srcStream, stream)
    return getStream.array(stream)
  }).then(results => {
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
  })
})

test('createCacheEntryStream no entries', function (t) {
  setup()
  const cachePath = path.join(CACHE_DIR, '.cache.json')
  var latest = 12345
  const src = []
  const srcStream = fromArray(src)
  return _createCacheWriteStream(cachePath, latest, {
    cache: CACHE_DIR
  }).then(stream => {
    t.ok(stream, 'returned a stream')
    stream = ms.pipeline.obj(srcStream, stream)
    stream.resume()
    return getStream(stream)
  }).then(() => {
    const fileData = JSON.parse(fs.readFileSync(cachePath))
    t.ok(fileData, 'cache file exists and has stuff in it')
    cleanup()
  })
})

test('createCacheEntryStream missing cache dir', function (t) {
  setup()
  var cachePath = path.join(CACHE_DIR, '.cache.json')
  var latest = 12345
  var src = []
  var srcStream = fromArray(src)
  return _createCacheWriteStream(cachePath, latest, {
    cache: CACHE_DIR
  }).then(stream => {
    t.ok(stream, 'returned a stream')
    stream = ms.pipeline.obj(srcStream, stream)
    return getStream.array(stream)
  }).then(res => {
    t.deepEqual(res, [], 'no data returned')
    var fileData = JSON.parse(fs.readFileSync(cachePath))
    t.ok(fileData, 'cache contents written to the right file')
    t.deepEquals(fileData, {
      '_updated': latest
    }, 'cache still contains `_updated`')
    cleanup()
  })
})
