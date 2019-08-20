'use strict'

const common = require('../common-tap.js')
const getStream = require('get-stream')
const npm = require('../../')
const test = require('tap').test
const mkdirp = require('mkdirp')
const mr = require('npm-registry-mock')

var _createEntryUpdateStream = require('../../lib/search/all-package-metadata.js')._createEntryUpdateStream

var server

// this test uses a fresh cache for each test block
// create them all in common.cache so that we can verify
// them for root-owned files in sudotest
let CACHE_DIR
let cacheCounter = 1
function setup () {
  CACHE_DIR = common.cache + '/' + cacheCounter++
  mkdirp.sync(CACHE_DIR)
  fixOwner(CACHE_DIR)
}

const chownr = require('chownr')

const fixOwner = (
  process.getuid && process.getuid() === 0 &&
  process.env.SUDO_UID && process.env.SUDO_GID
) ? (path) => chownr.sync(path, +process.env.SUDO_UID, +process.env.SUDO_GID)
  : () => {}

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
  return _createEntryUpdateStream(600, 0, {
    registry: common.registry
  }).then(({
    updateStream: stream,
    updatedLatest: latest
  }) => {
    t.equals(latest, 1234, '`latest` correctly extracted')
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
  return _createEntryUpdateStream(600, 1234, {
    registry: common.registry
  }).then(({
    updateStream: stream,
    updatedLatest: latest
  }) => {
    t.equals(latest, now, '`latest` correctly extracted from header')
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
  return _createEntryUpdateStream(600, 0, {
    registry: common.registry,
    token
  }).then(({
    updateStream: stream,
    updatedLatest: latest
  }) => {
    t.equals(latest, 1234, '`latest` correctly extracted')
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
  })
})

test('createEntryUpdateStream bad auth', function (t) {
  setup()
  var token = 'thisisanauthtoken'
  server.get('/-/all', { authorization: 'Bearer ' + token }).once().reply(401, {
    error: 'unauthorized search request'
  })
  return _createEntryUpdateStream(600, 0, {
    registry: common.registry,
    token
  }).then(() => {
    throw new Error('should not succeed')
  }, err => {
    t.ok(err, 'got an error from auth failure')
    t.match(err, /unauthorized/, 'failure message from request used')
  }).then(() => {
    server.done()
  })
})

test('createEntryUpdateStream not stale', function (t) {
  setup()
  var now = Date.now()
  var staleness = 600
  return _createEntryUpdateStream(staleness, now, {
    registry: common.registry
  }).then(({
    updateStream: stream,
    updatedLatest: latest
  }) => {
    t.notOk(stream, 'no stream returned')
    t.notOk(latest, 'no latest returned')
    server.done()
    t.end()
  })
})

test('cleanup', function (t) {
  server.close()
  t.done()
})
