'use strict'
var path = require('path')
var mkdirp = require('mkdirp')

var mr = require('npm-registry-mock')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')
var npm = require('../../lib/npm.js')

var pkg = path.resolve(__dirname, path.basename(__filename, '.js'))

function setup (cb) {
  cleanup()
  mkdirp.sync(pkg)
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}

test('setup', function (t) {
  setup()
  process.chdir(pkg)

  var opts = {
    cache: path.resolve(pkg, 'cache'),
    registry: common.registry,
    // important to make sure devDependencies don't get stripped
    dev: true
  }
  npm.load(opts, t.end)
})

test('fetch-package-metadata provides resolved metadata', function (t) {
  t.plan(5)

  var fetchPackageMetadata = require('../../lib/fetch-package-metadata')

  var testPackage = {
    raw: 'test-package@>=0.0.0',
    scope: null,
    name: 'test-package',
    rawSpec: '>=0.0.0',
    spec: '>=0.0.0',
    type: 'range'
  }

  mr({ port: common.port }, thenFetchMetadata)

  var server
  function thenFetchMetadata (err, s) {
    t.ifError(err, 'setup mock registry')
    server = s

    fetchPackageMetadata(testPackage, __dirname, thenVerifyMetadata)
  }

  function thenVerifyMetadata (err, pkg) {
    t.ifError(err, 'fetched metadata')

    t.equals(pkg._resolved, 'http://localhost:1337/test-package/-/test-package-0.0.0.tgz', '_resolved')
    t.equals(pkg._shasum, 'b0d32b6c45c259c578ba2003762b205131bdfbd1', '_shasum')
    t.equals(pkg._from, 'test-package@>=0.0.0', '_from')
    server.close()
    t.end()
  }
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})
