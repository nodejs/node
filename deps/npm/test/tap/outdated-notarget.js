// Fixes Issue #1770
var common = require('../common-tap.js')
var test = require('tap').test
var npm = require('../../')
var osenv = require('osenv')
var path = require('path')
var fs = require('fs')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var pkg = path.resolve(__dirname, 'outdated-notarget')
var cache = path.resolve(pkg, 'cache')
var mr = require('npm-registry-mock')

test('outdated-target: if no viable version is found, show error', function(t) {
  t.plan(1)
  setup()
  mr({port: common.port}, function(s) {
    npm.load({ cache: cache, registry: common.registry}, function() {
      npm.commands.update(function(er, d) {
        t.equal(er.code, 'ETARGET')
        s.close()
        t.end()
      })
    })
  })
})

test('cleanup', function(t) {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
  t.end()
})

function setup() {
  mkdirp.sync(pkg)
  mkdirp.sync(cache)
  fs.writeFileSync(path.resolve(pkg, 'package.json'), JSON.stringify({
    author: 'Evan Lucas',
    name: 'outdated-notarget',
    version: '0.0.0',
    description: 'Test for outdated-target',
    dependencies: {
      underscore: '~199.7.1'
    }
  }), 'utf8')
  process.chdir(pkg)
}
