'use strict'
var path = require('path')
var test = require('tap').test
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir
var extend = Object.assign || require('util')._extend
var common = require('../common-tap.js')

var basedir = path.join(__dirname, path.basename(__filename, '.js'))
var testdir = path.join(basedir, 'testdir')
var cachedir = path.join(basedir, 'cache')
var globaldir = path.join(basedir, 'global')
var tmpdir = path.join(basedir, 'tmp')

var conf = {
  cwd: testdir,
  env: extend({
    npm_config_cache: cachedir,
    npm_config_tmp: tmpdir,
    npm_config_prefix: globaldir,
    npm_config_registry: common.registry,
    npm_config_loglevel: 'warn'
  }, process.env)
}

var cycler = {
  name: 'cycler',
  version: '1.0.0',
  scripts: {
    uninstall: 'echo #UNINSTALL#',
    install: 'echo #INSTALL#'
  }
}

var fixture = new Tacks(Dir({
  cache: Dir(),
  global: Dir(),
  tmp: Dir(),
  testdir: Dir({
    'cycler': Dir({
      'package.json': File(cycler)
    }),
    node_modules: Dir({
      'cycler': Dir({
        'package.json': File(cycler)
      })
    }),
    'package.json': File({
      name: 'upgrade-lifecycles',
      version: '1.0.0',
      dependencies: {
        'cycler': 'file:cycler'
      }
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
  t.done()
})

test('upgrade-lifecycles', function (t) {
  common.npm(['install', 'file:cycler'], conf, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'command ran ok')

    t.comment(stdout.trim())
    t.comment(stderr.trim())
    t.match(stdout, /#INSTALL#/, 'ran install lifecycle')
    t.match(stdout, /#UNINSTALL#/, 'ran uninstall lifecycle')
    t.done()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.done()
})
