'use strict'

const common = require('../common-tap.js')

const getStream = require('get-stream')
const mkdirp = require('mkdirp')
const path = require('path')
const Tacks = require('tacks')
const {test} = require('tap')

const {File} = Tacks

const _createCacheEntryStream = require('../../lib/search/all-package-metadata.js')._createCacheEntryStream

// this test uses a fresh cache for each test block
// create them all in common.cache so that we can verify
// them for root-owned files in sudotest
let CACHE_DIR
let cacheCounter = 1
const chownr = require('chownr')
function setup () {
  CACHE_DIR = common.cache + '/' + cacheCounter++
  mkdirp.sync(CACHE_DIR)
  fixOwner(CACHE_DIR)
}

const fixOwner = (
  process.getuid && process.getuid() === 0 &&
  process.env.SUDO_UID && process.env.SUDO_GID
) ? (path) => chownr.sync(path, +process.env.SUDO_UID, +process.env.SUDO_GID)
  : () => {}

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
  fixOwner(cachePath)
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
    })
  })
})

test('createCacheEntryStream empty cache', t => {
  setup()
  const cachePath = path.join(CACHE_DIR, '.cache.json')
  const fixture = new Tacks(File({}))
  fixture.create(cachePath)
  fixOwner(cachePath)
  return _createCacheEntryStream(cachePath, {}).then(
    () => { throw new Error('should not succeed') },
    err => {
      t.ok(err, 'returned an error because there was no _updated')
      t.match(err.message, /Empty or invalid stream/, 'useful error message')
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
  fixOwner(cachePath)
  return _createCacheEntryStream(cachePath, {}).then(({
    updateStream: stream,
    updatedLatest: latest
  }) => {
    t.equals(latest, 1234, '`latest` correctly extracted')
    t.ok(stream, 'returned a stream')
    return getStream.array(stream).then(results => {
      t.deepEquals(results, [], 'no results')
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
    }
  )
})
