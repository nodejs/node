'use strict'
var path = require('path')
var test = require('tap').test
var mr = require('npm-registry-mock')
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir
var chain = require('slide').chain
var common = require('../common-tap.js')

var basedir = common.pkg
var testdir = path.join(basedir, 'testdir')
var cachedir = path.join(basedir, 'cache')
var globaldir = path.join(basedir, 'global')
var tmpdir = path.join(basedir, 'tmp')

var ciKeys = ['CI', 'TDDIUM', 'JENKINS_URL', 'bamboo.buildKey']

var filteredEnv = common.newEnv().delete(ciKeys)

var conf = function (extraEnv) {
  return {
    cwd: testdir,
    env: filteredEnv.clone().extend(extraEnv, {
      npm_config_cache: cachedir,
      npm_config_tmp: tmpdir,
      npm_config_prefix: globaldir,
      npm_config_registry: common.registry,
      npm_config_loglevel: 'warn'
    })
  }
}

var server
var fixture = new Tacks(Dir({
  cache: Dir(),
  global: Dir(),
  tmp: Dir(),
  testdir: Dir({
    'package.json': File({
      name: 'example',
      version: '1.0.0'
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

var ciHeaderTestCI = {
  'time': {'1.0.0': ''},
  'dist-tags': {'latest': '1.0.0'},
  'versions': {
    '1.0.0': {
      name: 'ci-header-test',
      version: '1.0.0',
      gotCI: true
    }
  }
}
var ciHeaderTestNotCI = {
  'time': {'1.0.0': ''},
  'dist-tags': {'latest': '1.0.0'},
  'versions': {
    '1.0.0': {
      name: 'ci-header-test',
      version: '1.0.0',
      gotCI: false
    }
  }
}

var ciHeaderTestNoCI = {
  'time': {'1.0.0': ''},
  'dist-tags': {'latest': '1.0.0'},
  'versions': {
    '1.0.0': {
      name: 'ci-header-test',
      version: '1.0.0',
      gotCI: null
    }
  }
}

test('setup', function (t) {
  setup()
  mr({port: common.port, throwOnUnmatched: true}, function (err, s) {
    if (err) throw err
    server = s
    server.get('/ci-header-test', {
      'NPM-In-CI': 'true'
    }).many().reply('200', ciHeaderTestCI)
    server.get('/ci-header-test', {
      'NPM-In-CI': 'false'
    }).many().reply('200', ciHeaderTestNotCI)
    server.get('/ci-header-test', {}).many().reply('200', ciHeaderTestNoCI)
    t.done()
  })
})

test('various-ci', function (t) {
  var todo = ciKeys.map(function (key) { return [checkKey, key] })
  return chain(todo, t.done)

  function checkKey (key, next) {
    var env = {}
    env[key] = 'true'

    common.npm(['view', '--cache-min=0', 'ci-header-test', 'gotCI'], conf(env), function (err, code, stdout, stderr) {
      if (err) throw err
      t.is(code, 0, key + ' command ran ok')
      if (stderr.trim()) t.comment(stderr.trim())
      t.is(stdout.trim(), 'true', key + ' set results in CI header')
      next()
    })
  }
})

test('no-ci', function (t) {
  common.npm(['view', '--cache-min=0', 'ci-header-test', 'gotCI'], conf(), function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'No CI env, command ran ok')
    if (stderr.trim()) t.comment(stderr.trim())
    t.is(stdout.trim(), 'false', 'No CI env, not in CI')
    t.done()
  })
})

test('cleanup', function (t) {
  server.close()
  cleanup()
  t.done()
})
