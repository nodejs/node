'use strict'
var path = require('path')
var fs = require('graceful-fs')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var test = require('tap').test
var common = require('../common-tap.js')

var base = path.join(__dirname, path.basename(__filename, '.js'))

var baseJSON = {
  name: 'base',
  version: '1.0.0',
  dependencies: {
    a: 'file:a/',
    b: 'file:b/'
  }
}

var aPath = path.join(base, 'a')
var aJSON = {
  name: 'a',
  version: '1.0.0',
  dependencies: {
    b: 'file:../b',
    c: 'file:../c'
  }
}

var bPath = path.join(base, 'b')
var bJSON = {
  name: 'b',
  version: '1.0.0'
}

var cPath = path.join(base, 'c')
var cJSON = {
  name: 'c',
  version: '1.0.0',
  dependencies: {
    b: 'file:../b'
  }
}

test('setup', function (t) {
  cleanup()
  setup()
  t.end()
})

test('install', function (t) {
  common.npm(['install'], {cwd: base}, function (er, code, stdout, stderr) {
    t.ifError(er, 'npm config ran without issue')
    t.is(code, 0, 'exited with a non-error code')
    t.is(stderr, '', 'Ran without errors')
    t.end()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function saveJson (pkgPath, json) {
  mkdirp.sync(pkgPath)
  fs.writeFileSync(path.join(pkgPath, 'package.json'), JSON.stringify(json, null, 2))
}

function setup () {
  saveJson(base, baseJSON)
  saveJson(aPath, aJSON)
  saveJson(bPath, bJSON)
  saveJson(cPath, cJSON)
}

function cleanup () {
  rimraf.sync(base)
}
