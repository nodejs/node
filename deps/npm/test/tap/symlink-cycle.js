'use strict'
var fs = require('fs')
var path = require('path')
var test = require('tap').test
var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var writeFileSync = require('fs').writeFileSync
var common = require('../common-tap.js')

var base = path.join(__dirname, path.basename(__filename, '.js'))
var cycle = path.join(base, 'cycle')

var cycleJSON = {
  name: 'cycle',
  version: '1.0.0',
  description: '',
  main: 'index.js',
  scripts: {
    test: 'echo "Error: no test specified" && exit 1'
  },
  dependencies: {
    'cycle': '*'
  },
  author: '',
  license: 'ISC'
}

test('setup', function (t) {
  setup()
  t.end()
})

test('ls', function (t) {
  process.chdir(cycle)
  common.npm(['ls'], {}, function (err, code, stdout, stderr) {
    t.ifError(err, 'installed w/o error')
    t.is(stderr, '', 'no warnings printed to stderr')
    t.end()
  })
})

test('cleanup', function (t) {
  process.chdir(osenv.tmpdir())
  cleanup()
  t.end()
})

function cleanup () {
  rimraf.sync(base)
}

function setup () {
  cleanup()
  mkdirp.sync(path.join(cycle, 'node_modules'))
  writeFileSync(
    path.join(cycle, 'package.json'),
    JSON.stringify(cycleJSON, null, 2)
  )
  fs.symlinkSync(cycle, path.join(cycle, 'node_modules', 'cycle'), 'junction')
}
