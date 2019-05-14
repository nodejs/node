'use strict'
var path = require('path')
var test = require('tap').test
var mr = require('npm-registry-mock')
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir
var common = require('../common-tap.js')

var basedir = path.join(__dirname, path.basename(__filename, '.js'))
var testdir = path.join(basedir, 'testdir')
var cachedir = path.join(basedir, 'cache')
var globaldir = path.join(basedir, 'global')
var tmpdir = path.join(basedir, 'tmp')
var escapeArg = require('../../lib/utils/escape-arg.js')

var conf = {
  cwd: testdir,
  env: Object.assign({
    npm_config_cache: cachedir,
    npm_config_tmp: tmpdir,
    npm_config_prefix: globaldir,
    npm_config_registry: common.registry,
    npm_config_loglevel: 'warn'
  }, process.env)
}

var server
var fixture = new Tacks(Dir({
  cache: Dir(),
  global: Dir(),
  tmp: Dir(),
  testdir: Dir({
    node_modules: Dir({}),
    'package.json': File({
      name: '13252',
      version: '1.0.0',
      scripts: {
        // add this to the end of the command to preserve the debug log:
        // || mv npm-debug.log real-debug.log
        // removed for windows compat reasons
        abc: escapeArg(common.nodeBin) + ' ' + escapeArg(common.bin) + ' shrinkwrap',
        shrinkwrap: escapeArg(common.nodeBin) + ' scripts/shrinkwrap.js'
      }
    }),
    scripts: Dir({
      'shrinkwrap.js': File(
        'console.log("OK " + process.cwd())'
      )
    })
  })
}))

function setup () {
  cleanup()
  fixture.create(basedir)
}

function cleanup () {
  fixture.remove(basedir)
}

test('setup', function (t) {
  setup()
  mr({port: common.port, throwOnUnmatched: true}, function (err, s) {
    if (err) throw err
    server = s
    t.done()
  })
})

test('shrinkwrap-lifecycle-cwd', function (t) {
  common.npm(['run', 'abc'], conf, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'command ran ok')
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    t.match(stdout.trim(), 'OK ' + testdir, 'got output from lifecycle script')
    t.is(stderr.trim().length, 0, 'no errors')
    t.done()
  })
})

test('cleanup', function (t) {
  server.close()
  cleanup()
  t.done()
})
