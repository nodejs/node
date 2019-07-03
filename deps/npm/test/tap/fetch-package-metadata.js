'use strict'
var path = require('path')
var mkdirp = require('mkdirp')

var mr = require('npm-registry-mock')
var npa = require('npm-package-arg')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')
var npm = require('../../lib/npm.js')

var pkg = common.pkg

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
  t.plan(4)

  var fetchPackageMetadata = require('../../lib/fetch-package-metadata')

  var testPackage = npa('test-package@>=0.0.0')

  mr({ port: common.port }, thenFetchMetadata)

  var server
  function thenFetchMetadata (err, s) {
    t.ifError(err, 'setup mock registry')
    server = s

    fetchPackageMetadata(testPackage, __dirname, thenVerifyMetadata)
  }

  function thenVerifyMetadata (err, pkg) {
    t.ifError(err, 'fetched metadata')

    t.equals(pkg._resolved, 'http://localhost:' + common.port + '/test-package/-/test-package-0.0.0.tgz', '_resolved')
    t.equals(pkg._integrity, 'sha1-sNMrbEXCWcV4uiADdisgUTG9+9E=', '_integrity')
    server.close()
    t.end()
  }
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})
