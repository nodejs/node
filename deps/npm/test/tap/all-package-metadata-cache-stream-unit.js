'use strict'

require('../common-tap.js')

const getStream = require('get-stream')
const mkdirp = require('mkdirp')
const path = require('path')
const rimraf = require('rimraf')
const Tacks = require('tacks')
const {test} = require('tap')

const {File} = Tacks

const _createCacheEntryStream = require('../../lib/search/all-package-metadata.js')._createCacheEntryStream

const PKG_DIR = path.resolve(__dirname, 'create-cache-entry-stream')
const CACHE_DIR = path.resolve(PKG_DIR, 'cache')

function setup () {
  mkdirp.sync(CACHE_DIR)
}

function cleanup () {
  rimraf.sync(PKG_DIR)
}

test('createCacheEntryStream basic', t => {
  setup()
  const cachePath = path.join(CACHE_DIR, '.cache.json')
  const fixture = new Tacks(File({
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
  return _createCacheEntryStream(cachePath, {}).then(({
    updateStream: stream,
    updatedLatest: latest
  }) => {
    t.equals(latest, 1234, '`latest` correctly extracted')
    t.ok(stream, 'returned a stream')
    return getStream.array(stream).then(results => {
      t.deepEquals(results, [{
        name: 'bar',
        version: '1.0.0'
      }, {
        name: 'foo',
        version: '1.0.0'
      }])
      cleanup()
    })
  })
})

test('createCacheEntryStream empty cache', t => {
  setup()
  const cachePath = path.join(CACHE_DIR, '.cache.json')
  const fixture = new Tacks(File({}))
  fixture.create(cachePath)
  return _createCacheEntryStream(cachePath, {}).then(
    () => { throw new Error('should not succeed') },
    err => {
      t.ok(err, 'returned an error because there was no _updated')
      t.match(err.message, /Empty or invalid stream/, 'useful error message')
      cleanup()
    }
  )
})

test('createCacheEntryStream no entry cache', t => {
  setup()
  const cachePath = path.join(CACHE_DIR, '.cache.json')
  const fixture = new Tacks(File({
    '_updated': 1234
  }))
  fixture.create(cachePath)
  return _createCacheEntryStream(cachePath, {}).then(({
    updateStream: stream,
    updatedLatest: latest
  }) => {
    t.equals(latest, 1234, '`latest` correctly extracted')
    t.ok(stream, 'returned a stream')
    return getStream.array(stream).then(results => {
      t.deepEquals(results, [], 'no results')
      cleanup()
    })
  })
})

test('createCacheEntryStream missing cache', t => {
  setup()
  const cachePath = path.join(CACHE_DIR, '.cache.json')
  return _createCacheEntryStream(cachePath, {}).then(
    () => { throw new Error('should not succeed') },
    err => {
      t.ok(err, 'returned an error because there was no cache')
      t.equals(err.code, 'ENOENT', 'useful error message')
      cleanup()
    }
  )
})
