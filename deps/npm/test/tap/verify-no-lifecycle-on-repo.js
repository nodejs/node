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
  repository: {
    type: 'git',
    url: 'http://example.com'
  },
  scripts: {
    prepublish: 'false'
  }
}

test('setup', function (t) {
  cleanup()
  setup()
  t.end()
})

test('repo', function (t) {
  common.npm(['repo', '--browser=echo'], {cwd: base}, function (er, code, stdout, stderr) {
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
}

function cleanup () {
  rimraf.sync(base)
}
