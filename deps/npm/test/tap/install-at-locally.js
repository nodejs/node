var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = path.join(__dirname, 'install-at-locally')

var EXEC_OPTS = { cwd: pkg }

var json = {
  name: 'install-at-locally',
  version: '0.0.0'
}

test('setup', function (t) {
  cleanup()
  t.end()
})

test('\'npm install ./package@1.2.3\' should install local pkg', function (t) {
  var target = './package@1.2.3'
  setup(target)
  common.npm(['install', target], EXEC_OPTS, function (err, code) {
    var p = path.resolve(pkg, 'node_modules/install-at-locally/package.json')
    t.ifError(err, 'install local package successful')
    t.equal(code, 0, 'npm install exited with code')
    t.ok(JSON.parse(fs.readFileSync(p, 'utf8')))
    t.end()
  })
})

test('\'npm install install/at/locally@./package@1.2.3\' should install local pkg', function (t) {
  var target = 'install/at/locally@./package@1.2.3'
  setup(target)
  common.npm(['install', target], EXEC_OPTS, function (err, code) {
    var p = path.resolve(pkg, 'node_modules/install-at-locally/package.json')
    t.ifError(err, 'install local package in explicit directory successful')
    t.equal(code, 0, 'npm install exited with code')
    t.ok(JSON.parse(fs.readFileSync(p, 'utf8')))
    t.end()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}

function setup (target) {
  cleanup()
  var root = path.resolve(pkg, target)
  mkdirp.sync(root)
  fs.writeFileSync(
    path.join(root, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  mkdirp.sync(path.resolve(pkg, 'node_modules'))
  process.chdir(pkg)
}
