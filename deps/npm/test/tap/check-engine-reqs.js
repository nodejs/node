'use strict'
var path = require('path')
var fs = require('fs')
var test = require('tap').test
var mkdirp = require('mkdirp')
var common = require('../common-tap.js')

var base = common.pkg
var installFrom = path.join(base, 'from')
var installIn = path.join(base, 'in')

var json = {
  name: 'check-engine-reqs',
  version: '0.0.1',
  description: 'fixture',
  engines: {
    node: '1.0.0-not-a-real-version'
  }
}

test('setup', function (t) {
  setup()
  t.end()
})

var INSTALL_OPTS = ['--loglevel', 'silly']
var EXEC_OPTS = {cwd: installIn}
test('install bad engine', function (t) {
  common.npm(['install', '--engine-strict', installFrom].concat(INSTALL_OPTS), EXEC_OPTS, function (err, code) {
    t.ifError(err, 'npm ran without issue')
    t.is(code, 1, 'npm install refused to install a package in itself')
    t.end()
  })
})
test('force install bad engine', function (t) {
  common.npm(['install', '--engine-strict', '--force', installFrom].concat(INSTALL_OPTS), EXEC_OPTS, function (err, code) {
    t.ifError(err, 'npm ran without issue')
    t.is(code, 0, 'npm install happily installed a package in itself with --force')
    t.end()
  })
})

test('warns on bad engine not strict', function (t) {
  common.npm(['install', '--json', installFrom], EXEC_OPTS, function (err, code, stdout, stderr) {
    t.ifError(err, 'npm ran without issue')
    t.is(code, 0, 'result code')
    var result = JSON.parse(stdout)
    t.match(result.warnings[0], /Unsupported engine/, 'reason for optional failure in JSON')
    t.match(result.warnings[0], /1.0.0-not-a-real-version/, 'should print mismatch version info')
    t.match(result.warnings[0], /Not compatible with your version of node/, 'incompatibility message')
    t.done()
  })
})

function setup () {
  mkdirp.sync(path.resolve(installFrom, 'node_modules'))
  fs.writeFileSync(
    path.join(installFrom, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  mkdirp.sync(path.resolve(installIn, 'node_modules'))
  process.chdir(base)
}
