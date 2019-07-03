'use strict'
var fs = require('fs')
var path = require('path')

var test = require('tap').test
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')

var common = require('../common-tap')

var pkg = common.pkg
var pathModA = path.join(pkg, 'node_modules', 'moduleA')
var pathModB = path.join(pkg, 'node_modules', 'moduleB')

var modA = {
  name: 'moduleA',
  version: '1.0.0',
  _requiredBy: [ '#USER', '/moduleB' ],
  dependencies: {
    moduleB: '1.0.0'
  }
}
var modB = {
  name: 'moduleB',
  version: '1.0.0',
  _requiredBy: [ '/moduleA' ],
  dependencies: {
    moduleA: '1.0.0'
  }
}

function setup () {
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    '{broken json'
  )
  mkdirp.sync(pathModA)
  fs.writeFileSync(
    path.join(pathModA, 'package.json'),
    JSON.stringify(modA, null, 2)
  )
  mkdirp.sync(pathModB)
  fs.writeFileSync(
    path.join(pathModB, 'package.json'),
    JSON.stringify(modB, null, 2)
  )
}

function cleanup () {
  rimraf.sync(pkg)
}

test('setup', function (t) {
  cleanup()
  setup()
  t.end()
})

test('ls-top-errors', function (t) {
  common.npm(['ls'], {cwd: pkg}, function (er, code, stdout, stderr) {
    t.ifErr(er, 'install finished successfully')
    t.match(stderr, /Failed to parse json/)
    t.end()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})
