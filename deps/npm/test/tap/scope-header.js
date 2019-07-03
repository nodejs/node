'use strict'
var path = require('path')
var test = require('tap').test
var mr = require('npm-registry-mock')
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir
var common = require('../common-tap.js')

var basedir = common.pkg
var testdir = path.join(basedir, 'testdir')
var withScope = path.join(testdir, 'with-scope')
var withoutScope = path.join(testdir, 'without-scope')
var onlyProjectScope = path.join(testdir, 'only-project-scope')
var cachedir = path.join(basedir, 'cache')
var globaldir = path.join(basedir, 'global')
var tmpdir = path.join(basedir, 'tmp')

var env = common.newEnv().extend({
  npm_config_cache: cachedir,
  npm_config_tmp: tmpdir,
  npm_config_prefix: globaldir,
  npm_config_registry: common.registry,
  npm_config_loglevel: 'warn'
})

var conf = function (cwd) {
  return {
    cwd: cwd || testdir,
    env: env
  }
}
var server
var fixture = new Tacks(Dir({
  cache: Dir(),
  global: Dir(),
  tmp: Dir(),
  testdir: Dir({
    'with-scope': Dir({
      'package.json': File({
        name: '@example/with-scope',
        version: '1.0.0'
      })
    }),
    'without-scope': Dir({
      'package.json': File({
        name: 'without-scope',
        version: '1.0.0'
      })
    }),
    'only-project-scope': Dir({
      '.npmrc': File('@example:registry=thisisinvalid\n'),
      'package.json': File({
        name: '@example/only-project-scope',
        version: '1.0.0'
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

var scopeHeaderTestGotScope = {
  'time': {'1.0.0': ''},
  'dist-tags': {'latest': '1.0.0'},
  'versions': {
    '1.0.0': {
      name: 'scope-header-test',
      version: '1.0.0',
      gotScope: true
    }
  }
}
var scopeHeaderTestNoScope = {
  'time': {'1.0.0': ''},
  'dist-tags': {'latest': '1.0.0'},
  'versions': {
    '1.0.0': {
      name: 'scope-header-test',
      version: '1.0.0',
      gotScope: false
    }
  }
}

test('setup', function (t) {
  setup()
  mr({port: common.port, throwOnUnmatched: true}, function (err, s) {
    if (err) throw err
    server = s
    server.get('/scope-header-test', {
      'NPM-Scope': '@example'
    }).many().reply('200', scopeHeaderTestGotScope)
    server.get('/scope-header-test', {}).many().reply('200', scopeHeaderTestNoScope)
    t.done()
  })
})

test('with-scope', function (t) {
  common.npm(['view', '--cache-min=0', 'scope-header-test', 'gotScope'], conf(withScope), function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'command ran ok')
    t.comment(stderr.trim())
    t.is(stdout.trim(), 'true', 'got version matched to scope header')
    t.done()
  })
})

test('without-scope', function (t) {
  common.npm(['view', '--cache-min=0', 'scope-header-test', 'gotScope'], conf(withoutScope), function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'command ran ok')
    t.comment(stderr.trim())
    t.is(stdout.trim(), 'false', 'got version matched to NO scope header')
    t.done()
  })
})

test('scope can be forced by having an explicit one', function (t) {
  common.npm([
    'view',
    '--cache-min=0',
    '--scope=example',
    'scope-header-test',
    'gotScope'
  ], conf(withoutScope), function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'command ran ok')
    t.comment(stderr.trim())
    t.is(stdout.trim(), 'true', 'got version matched to scope header')
    t.done()
  })
})

test('only the project scope is used', function (t) {
  common.npm([
    'view',
    '--cache-min=0',
    'scope-header-test', 'gotScope'
  ], conf(onlyProjectScope), function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'command ran ok')
    t.comment(stderr.trim())
    t.is(stdout.trim(), 'true', 'got version scope')
    t.done()
  })
})

test('cleanup', function (t) {
  server.close()
  cleanup()
  t.done()
})
