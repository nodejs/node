'use strict'
var test = require('tap').test
var npm = require('../../lib/npm')
var stream = require('readable-stream')

// set up environment
require('../common-tap.js')

var moduleName = 'xyzzy-wibble'
var testModule = {
  name: moduleName,
  'dist-tags': {
    latest: '1.3.0-a'
  },
  versions: {
    '1.0.0-a': {
      name: moduleName,
      version: '1.0.0-a',
      dist: {
        shasum: 'da39a3ee5e6b4b0d3255bfef95601890afd80709',
        tarball: 'http://registry.npmjs.org/aproba/-/xyzzy-wibble-1.0.0-a.tgz'
      }
    },
    '1.1.0-a': {
      name: moduleName,
      version: '1.1.0-a',
      dist: {
        shasum: 'da39a3ee5e6b4b0d3255bfef95601890afd80709',
        tarball: 'http://registry.npmjs.org/aproba/-/xyzzy-wibble-1.1.0-a.tgz'
      }
    },
    '1.2.0-a': {
      name: moduleName,
      version: '1.2.0-a',
      dist: {
        shasum: 'da39a3ee5e6b4b0d3255bfef95601890afd80709',
        tarball: 'http://registry.npmjs.org/aproba/-/xyzzy-wibble-1.2.0-a.tgz'
      }
    },
    '1.3.0-a': {
      name: moduleName,
      version: '1.3.0-a',
      dist: {
        shasum: 'da39a3ee5e6b4b0d3255bfef95601890afd80709',
        tarball: 'http://registry.npmjs.org/aproba/-/xyzzy-wibble-1.3.0-a.tgz'
      }
    }
  }
}

test('setup', function (t) {
  npm.load(function () {
    npm.config.set('loglevel', 'silly')
    npm.registry = {
      get: function (uri, opts, cb) {
        setImmediate(function () {
          cb(null, testModule, null, {statusCode: 200})
        })
      },
      fetch: function (u, opts, cb) {
        setImmediate(function () {
          var empty = new stream.Readable()
          empty.push(null)
          cb(null, empty)
        })
      }
    }
    t.end()
  })
})

test('splat', function (t) {
  t.plan(3)
  var addNamed = require('../../lib/cache/add-named.js')
  addNamed('xyzzy-wibble', '*', testModule, function (err, pkg) {
    t.error(err, 'Succesfully resolved a splat package')
    t.is(pkg.name, moduleName)
    t.is(pkg.version, testModule['dist-tags'].latest)
  })
})
