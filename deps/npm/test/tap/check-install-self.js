'use strict'
var path = require('path')
var fs = require('fs')
var test = require('tap').test
var osenv = require('osenv')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var common = require('../common-tap.js')

var base = path.join(__dirname, path.basename(__filename, '.js'))
var installFrom = path.join(base, 'from')
var installIn = path.join(base, 'in')

var json = {
  name: 'check-install-self',
  version: '0.0.1',
  description: 'fixture'
}

test('setup', function (t) {
  setup()
  t.end()
})

var EXEC_OPTS = {cwd: installIn}

test('install self', function (t) {
  common.npm(['install', installFrom], EXEC_OPTS, function (err, code, stdout, stderr) {
    if (err) throw err
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    t.is(code, 1, 'npm install refused to install a package in itself')
    t.end()
  })
})
test('force install self', function (t) {
  common.npm(['install', '--force', installFrom], EXEC_OPTS, function (err, code, stdout, stderr) {
    if (err) throw err
    t.comment(stdout.trim())
    t.comment(stderr.trim())
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
  fs.writeFileSync(
    path.join(installIn, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  process.chdir(base)
}
