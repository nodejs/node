var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = path.join(__dirname, 'install-at-sub-path-locally')

var EXEC_OPTS = { cwd: pkg, stdio: [0, 1, 2] }

var json = {
  name: 'install-at-sub-path-locally-mock',
  version: '0.0.0'
}

var target = '../package@1.2.3'

test('setup', function (t) {
  cleanup()
  t.end()
})

test('\'npm install ../package@1.2.3\' should install local pkg from sub path', function (t) {
  setup()
  common.npm(['install', '--loglevel=silent', target], EXEC_OPTS, function (err, code) {
    if (err) throw err
    var p = path.resolve(pkg, 'node_modules/install-at-sub-path-locally-mock/package.json')
    t.equal(code, 0, 'npm install exited with code')
    t.ok(JSON.parse(fs.readFileSync(p, 'utf8')))
    t.end()
  })
})

test('\'running npm install ../package@1.2.3\' should not break on sub path re-install', function (t) {
  common.npm(['install', '--loglevel=silent', target], EXEC_OPTS, function (err, code) {
    if (err) throw err
    var p = path.resolve(pkg, 'node_modules/install-at-sub-path-locally-mock/package.json')
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
  rimraf.sync(path.resolve(pkg, target))
}

function setup () {
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
