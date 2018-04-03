'use strict'
const fs = require('fs')
var path = require('path')
var test = require('tap').test
var mr = require('npm-registry-mock')
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

var server
var fixture = new Tacks(Dir({
  cache: Dir(),
  global: Dir(),
  tmp: Dir(),
  testdir: Dir({
    'package-lock.json': File({
      name: 'http-locks',
      version: '1.0.0',
      lockfileVersion: 1,
      requires: true,
      dependencies: {
        minimist: {
          version: common.registry + '/minimist/-/minimist-0.0.5.tgz',
          integrity: 'sha1-16oye87PUY+RBqxrjwA/o7zqhWY='
        }
      }
    }),
    'package.json': File({
      name: 'http-locks',
      version: '1.0.0',
      dependencies: {
        minimist: common.registry + '/minimist/-/minimist-0.0.5.tgz'
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
  mr({port: common.port, throwOnUnmatched: true}, function (err, s) {
    if (err) throw err
    server = s
    t.done()
  })
})

test('http deps in lock files', function (t) {
  common.npm(['install'], conf, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'command ran ok')
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    const minPackage = JSON.parse(fs.readFileSync(testdir + '/node_modules/minimist/package.json'))
    t.is(minPackage.version, '0.0.5', 'package version was maintained')
    t.done()
  })
})

test('cleanup', function (t) {
  server.close()
  cleanup()
  t.done()
})
