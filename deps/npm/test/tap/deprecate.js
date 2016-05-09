var fs = require('fs')
var path = require('path')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var mr = require('npm-registry-mock')
var semver = require('semver')

var test = require('tap').test
var common = require('../common-tap.js')

var pkg = path.resolve(__dirname, 'deprecate')
var server

var cache = {
  '_id': 'cond',
  '_rev': '19-d458a706de1740662cd7728d7d7ddf07',
  'name': 'cond',
  'time': {
    'modified': '2015-02-13T07:33:58.120Z',
    'created': '2014-03-16T20:52:52.236Z',
    '0.0.0': '2014-03-16T20:52:52.236Z',
    '0.0.1': '2014-03-16T21:12:33.393Z',
    '0.0.2': '2014-03-16T21:15:25.430Z'
  },
  'versions': {
    '0.0.0': {},
    '0.0.1': {},
    '0.0.2': {}
  },
  'dist-tags': {
    'latest': '0.0.2'
  },
  'description': 'Restartable error handling system',
  'license': 'CC0'
}

test('setup', function (t) {
  mr({port: common.port}, function (err, s) {
    t.ifError(err, 'registry mocked successfully')
    server = s
    t.ok(true)
    t.end()
  })
})

test('npm deprecate an unscoped package', function (t) {
  var deprecated = JSON.parse(JSON.stringify(cache))
  deprecated.versions = {
    '0.0.0': {},
    '0.0.1': { deprecated: 'make it dead' },
    '0.0.2': {}
  }
  server.get('/cond?write=true').reply(200, cache)
  server.put('/cond', deprecated).reply(201, { deprecated: true })
  common.npm([
    'deprecate',
    'cond@0.0.1',
    'make it dead',
    '--registry', common.registry,
    '--loglevel', 'silent'
  ], {},
  function (er, code, stdout, stderr) {
    t.ifError(er, 'npm deprecate')
    t.equal(stderr, '', 'no error output')
    t.equal(code, 0, 'exited OK')
    t.end()
  })
})

test('npm deprecate a scoped package', function (t) {
  var cacheCopy = JSON.parse(JSON.stringify(cache))
  cacheCopy.name = '@scope/cond'
  cacheCopy._id = '@scope/cond'
  var deprecated = JSON.parse(JSON.stringify(cacheCopy))
  deprecated.versions = {
    '0.0.0': {},
    '0.0.1': { deprecated: 'make it dead' },
    '0.0.2': {}
  }
  server.get('/@scope%2fcond?write=true').reply(200, cacheCopy)
  server.put('/@scope%2fcond', deprecated).reply(201, { deprecated: true })
  common.npm([
    'deprecate',
    '@scope/cond@0.0.1',
    'make it dead',
    '--registry', common.registry,
    '--loglevel', 'silent'
  ], {},
  function (er, code, stdout, stderr) {
    t.ifError(er, 'npm deprecate')
    t.equal(stderr, '', 'no error output')
    t.equal(code, 0, 'exited OK')
    t.end()
  })
})

test('npm deprecate semver range', function (t) {
  var deprecated = JSON.parse(JSON.stringify(cache))
  deprecated.versions = {
    '0.0.0': { deprecated: 'make it dead' },
    '0.0.1': { deprecated: 'make it dead' },
    '0.0.2': {}
  }
  server.get('/cond?write=true').reply(200, cache)
  server.put('/cond', deprecated).reply(201, { deprecated: true })
  common.npm([
    'deprecate',
    'cond@<0.0.2',
    'make it dead',
    '--registry', common.registry,
    '--loglevel', 'silent'
  ], {},
  function (er, code, stdout, stderr) {
    t.ifError(er, 'npm deprecate')
    t.equal(stderr, '', 'no error output')
    t.equal(code, 0, 'exited OK')
    t.end()
  })
})

test('npm deprecate bad semver range', function (t) {
  common.npm([
    'deprecate',
    'cond@-9001',
    'make it dead',
    '--registry', common.registry
  ], {},
  function (er, code, stdout, stderr) {
    t.equal(code, 1, 'errored')
    t.match(stderr, /invalid version range/, 'bad semver')
    t.end()
  })
})

test('npm deprecate a package with no semver range', function (t) {
  var deprecated = JSON.parse(JSON.stringify(cache))
  deprecated.versions = {
    '0.0.0': { deprecated: 'make it dead' },
    '0.0.1': { deprecated: 'make it dead' },
    '0.0.2': { deprecated: 'make it dead' }
  }
  server.get('/cond?write=true').reply(200, cache)
  server.put('/cond', deprecated).reply(201, { deprecated: true })
  common.npm([
    'deprecate',
    'cond',
    'make it dead',
    '--registry', common.registry,
    '--loglevel', 'silent'
  ], {},
  function (er, code, stdout, stderr) {
    t.ifError(er, 'npm deprecate')
    t.equal(stderr, '', 'no error output')
    t.equal(code, 0, 'exited OK')
    t.end()
  })
})

test('cleanup', function (t) {
  server.close()
  t.ok(true)
  t.end()
})
