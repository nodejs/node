var common = require('../common-tap.js')
var test = require('tap').test
var npm = require('../../')
var osenv = require('osenv')
var path = require('path')
var fs = require('fs')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var pkg = path.resolve(__dirname, '404-parent')
var mr = require("npm-registry-mock")

test('404-parent: if parent exists, specify parent in error message', function(t) {
  setup()
  rimraf.sync(path.resolve(pkg, 'node_modules'))
  performInstall(function(err) {
    t.ok(err instanceof Error)
    t.pass('error was returned')
    t.ok(err.parent === '404-parent-test')
    t.end()
  })
})

test('cleanup', function(t) {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
  t.end()
})

function setup() {
  mkdirp.sync(pkg)
  mkdirp.sync(path.resolve(pkg, 'cache'))
  fs.writeFileSync(path.resolve(pkg, 'package.json'), JSON.stringify({
    author: 'Evan Lucas',
    name: '404-parent-test',
    version: '0.0.0',
    description: 'Test for 404-parent',
    dependencies: {
      'test-npm-404-parent-test': '*'
    }
  }), 'utf8')
  process.chdir(pkg)
}

function performInstall(cb) {
  mr(common.port, function (s) { // create mock registry.
    npm.load({registry: common.registry}, function() {
      npm.commands.install(pkg, [], function(err) {
        cb(err)
        s.close() // shutdown mock npm server.
      })
    })
  })
}
