'use strict'
var fs = require('graceful-fs')
var path = require('path')
var mkdirpSync = require('mkdirp').sync
var rimraf = require('rimraf')
var test = require('tap').test
var common = require('../common-tap.js')

var base = path.join(__dirname, path.basename(__filename, '.js'))
var cruft = path.join(base, 'node_modules', 'cruuuft')
var pkg = {
  name: 'example',
  version: '1.0.0',
  dependencies: {}
}

function setup () {
  mkdirpSync(path.dirname(cruft))
  fs.writeFileSync(cruft, 'this is some cruft for sure')
  fs.writeFileSync(path.join(base, 'package.json'), JSON.stringify(pkg))
}

function cleanup () {
  rimraf.sync(base)
}

test('setup', function (t) {
  cleanup()
  setup()
  t.done()
})

test('cruft', function (t) {
  common.npm(['ls'], {cwd: base}, function (er, code, stdout, stderr) {
    t.is(stderr, '', 'no warnings or errors from ls')
    t.done()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.done()
})
