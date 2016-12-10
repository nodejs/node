var common = require('../common-tap.js')
var test = require('tap').test
var npm = require('../../lib/npm.js')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var path = require('path')
var fs = require('fs')
var mr = require('npm-registry-mock')

var osenv = require('osenv')

var PKG_DIR = path.resolve(__dirname, 'publish-invalid-semver-tag')
var CACHE_DIR = path.resolve(PKG_DIR, 'cache')

var DEFAULT_PKG = {
  'name': 'examples',
  'version': '1.2.3'
}

var mockServer

function resetPackage (options) {
  rimraf.sync(CACHE_DIR)
  mkdirp.sync(CACHE_DIR)

  fs.writeFileSync(path.resolve(PKG_DIR, 'package.json'), DEFAULT_PKG)
}

test('setup', function (t) {
  process.chdir(osenv.tmpdir())
  mkdirp.sync(PKG_DIR)
  process.chdir(PKG_DIR)

  resetPackage({})

  mr({ port: common.port }, function (er, server) {
    npm.load({
      cache: CACHE_DIR,
      registry: common.registry,
      cwd: PKG_DIR
    }, function (err) {
      t.ifError(err, 'started server')
      mockServer = server

      t.end()
    })
  })
})

test('attempt publish with semver-like version', function (t) {
  resetPackage({})

  npm.config.set('tag', 'v1.x')
  npm.commands.publish([], function (err) {
    t.notEqual(err, null)
    t.same(err.message, 'Tag name must not be a valid SemVer range: v1.x')
    t.end()
  })
})

test('attempt publish with semver-like version', function (t) {
  resetPackage({})

  npm.config.set('tag', '1.2.3')
  npm.commands.publish([], function (err) {
    t.notEqual(err, null)
    t.same(err.message, 'Tag name must not be a valid SemVer range: 1.2.3')
    t.end()
  })
})

test('cleanup', function (t) {
  mockServer.close()

  process.chdir(osenv.tmpdir())
  rimraf.sync(PKG_DIR)

  t.end()
})
