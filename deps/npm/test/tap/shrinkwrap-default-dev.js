'use strict'
var path = require('path')
var test = require('tap').test
var fs = require('fs')
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
  env: extend(extend({}, process.env), {
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
    node_modules: Dir({
      '@npmtest': Dir({
        example: Dir({
          'package.json': File({
            name: '@npmtest/example',
            version: '1.0.0'
          })
        })
      })
    }),
    'package.json': File({
      name: 'shrinkwrap-default-dev',
      version: '1.0.0',
      devDependencies: {
        '@npmtest/example': '1.0.0'
      }
    })
  })
}))

var shrinkwrapPath = path.join(testdir, 'npm-shrinkwrap.json')
var shrinkwrapWithDev = {
  name: 'shrinkwrap-default-dev',
  version: '1.0.0',
  dependencies: {
    '@npmtest/example': {
      'version': '1.0.0',
      'dev': true
    }
  }
}
var shrinkwrapWithoutDev = {
  name: 'shrinkwrap-default-dev',
  version: '1.0.0',
  dependencies: {}
}

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

test('shrinkwrap-default-dev', function (t) {
  common.npm(['shrinkwrap'], conf, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'command ran ok')
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    var swrap = JSON.parse(fs.readFileSync(shrinkwrapPath))
    t.isDeeply(swrap, shrinkwrapWithDev, 'Shrinkwrap included dev deps by default')
    t.done()
  })
})

test('shrinkwrap-only-prod', function (t) {
  fs.unlinkSync(shrinkwrapPath)
  common.npm(['shrinkwrap', '--only=prod'], conf, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'command ran ok')
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    var swrap = JSON.parse(fs.readFileSync(shrinkwrapPath))
    t.isDeeply(swrap, shrinkwrapWithoutDev, 'Shrinkwrap did not include dev deps with --only=prod')
    t.done()
  })
})

test('shrinkwrap-production', function (t) {
  fs.unlinkSync(shrinkwrapPath)
  common.npm(['shrinkwrap', '--production'], conf, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'command ran ok')
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    var swrap = JSON.parse(fs.readFileSync(shrinkwrapPath))
    t.isDeeply(swrap, shrinkwrapWithoutDev, 'Shrinkwrap did not include dev deps with --production')
    t.done()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.done()
})
