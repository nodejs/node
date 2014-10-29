var common = require('../common-tap.js')
var test = require('tap').test
var npm = require('../../')
var path = require('path')
var fs = require('fs')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var pkg = path.join(__dirname, 'install-at-locally')

test("setup", function (t) {
  mkdirp.sync(pkg)
  mkdirp.sync(path.resolve(pkg, 'node_modules'))
  process.chdir(pkg)
  t.end()
})

test('"npm install ./package@1.2.3" should install local pkg', function(t) {
  npm.load(function() {
    npm.commands.install(['./package@1.2.3'], function(err) {
      var p = path.resolve(pkg, 'node_modules/install-at-locally/package.json')
      t.ok(JSON.parse(fs.readFileSync(p, 'utf8')))
      t.end()
    })
  })
})

test('"npm install install/at/locally@./package@1.2.3" should install local pkg', function(t) {
  npm.load(function() {
    npm.commands.install(['./package@1.2.3'], function(err) {
      var p = path.resolve(pkg, 'node_modules/install-at-locally/package.json')
      t.ok(JSON.parse(fs.readFileSync(p, 'utf8')))
      t.end()
    })
  })
})

test('cleanup', function(t) {
  process.chdir(__dirname)
  rimraf.sync(path.resolve(pkg, 'node_modules'))
  t.end()
})

