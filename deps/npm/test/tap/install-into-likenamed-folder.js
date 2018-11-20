'use strict'
var path = require('path')
var fs = require('graceful-fs')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var test = require('tap').test
var common = require('../common-tap.js')

var base = path.join(__dirname, path.basename(__filename, '.js'))
var moduleDir = path.join(base, 'example-src')
var destDir = path.join(base, 'example')
var moduleJson = {
  name: 'example',
  version: '1.0.0'
}

function setup () {
  cleanup()
  mkdirp.sync(moduleDir)
  mkdirp.sync(path.join(destDir, 'node_modules'))
  fs.writeFileSync(path.join(moduleDir, 'package.json'), JSON.stringify(moduleJson))
}

function cleanup () {
  rimraf.sync(base)
}

test('setup', function (t) {
  setup()
  t.end()
})

test('like-named', function (t) {
  common.npm(['install', '../example-src'], {cwd: destDir}, function (er, code, stdout, stderr) {
    t.is(code, 0, 'no error code')
    t.is(stderr, '', 'no error output')
    t.end()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})
