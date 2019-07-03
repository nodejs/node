'use strict'
var path = require('path')
var test = require('tap').test
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir
var common = require('../common-tap.js')

var basedir = common.pkg
var testdir = path.join(basedir, 'testdir')
var cachedir = path.join(basedir, 'cache')
var globaldir = path.join(basedir, 'global')
var tmpdir = path.join(basedir, 'tmp')

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

var fixture = new Tacks(Dir({
  cache: Dir(),
  global: Dir(),
  tmp: Dir(),
  testdir: Dir({
    node_modules: Dir({
    }),
    package: Dir({
      node_modules: Dir({
        'bundled-dep': Dir({
          'package.json': File({
            name: 'bundled-dep',
            version: '0.0.0'
          })
        })
      }),
      'package.json': File({
        name: '@scope/package',
        version: '0.0.0',
        dependencies: {
          'bundled-dep': '*'
        },
        bundledDependencies: [
          'bundled-dep'
        ]
      })
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

test('install dependencies of bundled dependencies', function (t) {
  common.npm(['install', './package'], conf, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'command ran ok')
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    t.done()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.done()
})
