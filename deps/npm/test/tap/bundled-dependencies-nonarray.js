'use strict'
var Bluebird = require('bluebird')
var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var dir = common.pkg
var pkg = path.resolve(dir, 'pkg-with-bundled')
var dep = path.resolve(dir, 'a-bundled-dep')

var pj = JSON.stringify({
  name: 'pkg-with-bundled',
  version: '1.0.0',
  dependencies: {
    'a-bundled-dep': 'file:../a-bundled-dep-2.0.0.tgz'
  },
  bundledDependencies: {
    'a-bundled-dep': 'file:../a-bundled-dep-2.0.0.tgz'
  }
}, null, 2) + '\n'

var pjDep = JSON.stringify({
  name: 'a-bundled-dep',
  version: '2.0.0'
}, null, 2) + '\n'

test('setup', function (t) {
  bootstrap()
  t.end()
})

test('handles non-array bundleddependencies', function (t) {
  return Bluebird.try(() => {
    return common.npm(['pack', 'a-bundled-dep/'], {cwd: dir, stdio: [0, 1, 2]})
  }).spread((code) => {
    t.is(code, 0, 'built a-bundled-dep')
    return common.npm(['install'], {cwd: pkg, stdio: [0, 1, 2]})
  }).spread((code) => {
    t.is(code, 0, 'prepared pkg-with-bundled')
    return common.npm(['pack', 'pkg-with-bundled/'], {cwd: dir, stdio: [0, 1, 'pipe']})
  }).spread((code, _, stderr) => {
    t.equal(code, 0, 'exited with a error code')
    t.equal(stderr, '')
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function bootstrap () {
  cleanup()
  mkdirp.sync(dir)
  mkdirp.sync(path.join(dir, 'node_modules'))

  mkdirp.sync(pkg)
  fs.writeFileSync(path.resolve(pkg, 'package.json'), pj)

  mkdirp.sync(dep)
  fs.writeFileSync(path.resolve(dep, 'package.json'), pjDep)
}

function cleanup () {
  rimraf.sync(dir)
}
