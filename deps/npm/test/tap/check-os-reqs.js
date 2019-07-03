'use strict'
var path = require('path')
var fs = require('fs')
var test = require('tap').test
var osenv = require('osenv')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var common = require('../common-tap.js')

var base = common.pkg
var installFrom = path.join(base, 'from')
var installIn = path.join(base, 'in')

var json = {
  name: 'check-os-reqs',
  version: '0.0.1',
  description: 'fixture',
  os: ['fake-os']
}

test('setup', function (t) {
  setup()
  t.end()
})

var INSTALL_OPTS = ['--loglevel', 'silly']
var EXEC_OPTS = {cwd: installIn}

test('install bad os', function (t) {
  common.npm(['install', installFrom].concat(INSTALL_OPTS), EXEC_OPTS, function (err, code) {
    t.ifError(err, 'npm ran without issue')
    t.is(code, 1, 'npm install refused to install a package in itself')
    t.end()
  })
})
test('force install bad os', function (t) {
  common.npm(['install', '--force', installFrom].concat(INSTALL_OPTS), EXEC_OPTS, function (err, code) {
    t.ifError(err, 'npm ran without issue')
    t.is(code, 0, 'npm install happily installed a package in itself with --force')
    t.end()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(base)
}

function setup () {
  cleanup()
  mkdirp.sync(path.resolve(installFrom, 'node_modules'))
  fs.writeFileSync(
    path.join(installFrom, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  mkdirp.sync(path.resolve(installIn, 'node_modules'))
  process.chdir(base)
}
