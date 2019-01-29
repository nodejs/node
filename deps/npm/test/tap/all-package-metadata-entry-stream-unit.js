'use strict'

const common = require('../common-tap.js')
const getStream = require('get-stream')
const mkdirp = require('mkdirp')
const mr = require('npm-registry-mock')
const npm = require('../../')
const path = require('path')
const rimraf = require('rimraf')
const Tacks = require('tacks')
const test = require('tap').test

const {File} = Tacks

const _createEntryStream = require('../../lib/search/all-package-metadata.js')._createEntryStream

const PKG_DIR = path.resolve(__dirname, 'create-entry-update-stream')
const CACHE_DIR = path.resolve(PKG_DIR, 'cache')

let server

function setup () {
  mkdirp.sync(CACHE_DIR)
}

function cleanup () {
  rimraf.sync(PKG_DIR)
}

test('setup', t => {
  cleanup()
  mr({port: common.port, throwOnUnmatched: true}, (err, s) => {
    t.ifError(err, 'registry mocked successfully')
    npm.load({ cache: CACHE_DIR, registry: common.registry }, err => {
      t.ifError(err, 'npm loaded successfully')
      server = s
      t.pass('all set up')
      t.done()
    })
  })
})

test('createEntryStream full request', t => {
  setup()
  const cachePath = path.join(CACHE_DIR, '.cache.json')
  const dataTime = +(new Date())
  server.get('/-/all').once().reply(200, {
    '_updated': dataTime,
    'bar': { name: 'bar', version: '1.0.0' },
    'foo': { name: 'foo', version: '1.0.0' }
  }, {
    date: 1234 // should never be used.
  })
  return _createEntryStream(cachePath, 600, {
    registry: common.registry
  }).then(({
    entryStream: stream,
    latest,
    newEntries
  }) => {
    t.equals(latest, dataTime, '`latest` correctly extracted')
    t.ok(newEntries, 'new entries need to be written to cache')
    t.ok(stream, 'returned a stream')
    return getStream.array(stream)
  }).then(results => {
    t.deepEquals(results, [{
      name: 'bar',
      version: '1.0.0'
    }, {
      name: 'foo',
      version: '1.0.0'
    }])
    server.done()
    cleanup()
  })
})

test('createEntryStream cache only', function (t) {
  setup()
  const now = Date.now()
  const cacheTime = now - 100000
  const cachePath = path.join(CACHE_DIR, '.cache.json')
  const fixture = new Tacks(File({
    '_updated': cacheTime,
    bar: { name: 'bar', version: '1.0.0' },
    cool: { name: 'cool', version: '1.0.0' },
    foo: { name: 'foo', version: '2.0.0' },
    other: { name: 'other', version: '1.0.0' }
  }))
  fixture.create(cachePath)
  return _createEntryStream(cachePath, 600, {
    registry: common.registry
  }).then(({
    entryStream: stream,
    latest,
    newEntries
  }) => {
    t.equals(latest, cacheTime, '`latest` is cache time')
    t.ok(stream, 'returned a stream')
    t.notOk(newEntries, 'cache only means no need to write to cache')
    return getStream.array(stream)
  }).then(results => {
    t.deepEquals(
      results.map(function (pkg) { return pkg.name }),
      ['bar', 'cool', 'foo', 'other'],
      'packages deduped and sorted'
    )
    server.done()
    cleanup()
  })
})

test('createEntryStream merged stream', function (t) {
  setup()
  const now = Date.now()
  const cacheTime = now - 6000000
  server.get('/-/all/since?stale=update_after&startkey=' + cacheTime).once().reply(200, {
    'bar': { name: 'bar', version: '2.0.0' },
    'car': { name: 'car', version: '1.0.0' },
    'foo': { name: 'foo', version: '1.0.0' }
  }, {
    date: (new Date(now)).toISOString()
  })
  const cachePath = path.join(CACHE_DIR, '.cache.json')
  const fixture = new Tacks(File({
    '_updated': cacheTime,
    bar: { name: 'bar', version: '1.0.0' },
    cool: { name: 'cool', version: '1.0.0' },
    foo: { name: 'foo', version: '2.0.0' },
    other: { name: 'other', version: '1.0.0' }
  }))
  fixture.create(cachePath)
  return _createEntryStream(cachePath, 600, {
    registry: common.registry
  }).then(({
    entryStream: stream,
    latest,
    newEntries
  }) => {
    t.equals(latest, now, '`latest` correctly extracted from header')
    t.ok(stream, 'returned a stream')
    t.ok(newEntries, 'cache update means entries should be written')
    return getStream.array(stream)
  }).then(results => {
    t.deepEquals(
      results.map(function (pkg) { return pkg.name }),
      ['bar', 'car', 'cool', 'foo', 'other'],
      'packages deduped and sorted'
    )
    t.deepEquals(results[0], {
      name: 'bar',
      version: '2.0.0'
    }, 'update stream version wins on dedupe')
    t.deepEquals(results[3], {
      name: 'foo',
      version: '1.0.0'
    }, 'update stream version wins on dedupe even when the newer one has a lower semver.')
    server.done()
    cleanup()
  })
})

test('createEntryStream no sources', function (t) {
  setup()
  const cachePath = path.join(CACHE_DIR, '.cache.json')
  server.get('/-/all').once().reply(404, {})
  return _createEntryStream(cachePath, 600, {
    registry: common.registry
  }).then(({
    entryStream: stream,
    latest,
    newEntries
  }) => {
    throw new Error('should not succeed')
  }, err => {
    t.ok(err, 'no sources, got an error')
    t.match(err.message, /No search sources available/, 'useful error message')
  }).then(() => {
    server.done()
    cleanup()
  })
})

test('cleanup', function (t) {
  cleanup()
  server.close()
  t.pass('all done')
  t.done()
})
