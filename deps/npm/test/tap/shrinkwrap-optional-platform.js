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
  env: Object.assign({}, process.env, {
    npm_config_cache: cachedir,
    npm_config_tmp: tmpdir,
    npm_config_prefix: globaldir,
    npm_config_registry: common.registry,
    npm_config_loglevel: 'warn'
  })
}

var fixture = new Tacks(Dir({
  cache: Dir(),
  global: Dir(),
  tmp: Dir(),
  testdir: Dir({
    mod1: Dir({
      'package.json': File({
        name: 'mod1',
        version: '1.0.0',
        scripts: {},
        'optionalDependencies': {
          'mod2': 'file:../mod2'
        },
        os: ['nosuchos']
      })
    }),
    mod2: Dir({
      'package.json': File({
        name: 'mod2',
        version: '1.0.0',
        scripts: {},
        os: ['nosuchos']
      })
    }),
    'npm-shrinkwrap.json': File({
      name: 'shrinkwrap-optional-platform',
      version: '1.0.0',
      dependencies: {
        mod1: {
          version: '1.0.0',
          resolved: 'file:mod1',
          optional: true
        },
        mod2: {
          version: '1.0.0',
          resolved: 'file:mod2',
          optional: true
        }
      }
    }),
    'package.json': File({
      name: 'shrinkwrap-optional-platform',
      version: '1.0.0',
      optionalDependencies: {
        mod1: 'file:mod1'
      },
      description: 'x',
      repository: 'x',
      license: 'Artistic-2.0'
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

test('example', function (t) {
  common.npm(['install'], conf, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'install ran ok')
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    t.notMatch(stderr, /Exit status 1/, 'did not try to install opt dep')
    t.done()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.done()
})
