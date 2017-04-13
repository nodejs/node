'use strict'
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

var expected = {
  name: 'npm-test-peer-deps-toplevel',
  version: '0.0.0',
  problems: [
    'peer dep missing: mkdirp@*, required by npm-test-peer-deps-toplevel@0.0.0',
    'peer dep missing: request@0.9.x, required by npm-test-peer-deps@0.0.0'
  ],
  dependencies: {
    'npm-test-peer-deps': {
      version: '0.0.0',
      from: 'npm-test-peer-deps@*',
      resolved: common.registry + '/npm-test-peer-deps/-/npm-test-peer-deps-0.0.0.tgz',
      dependencies: {
        underscore: {
          version: '1.3.1',
          from: 'underscore@1.3.1',
          resolved: common.registry + '/underscore/-/underscore-1.3.1.tgz'
        }
      }
    },
    mkdirp: {
      peerMissing: true,
      required: {
        _id: 'mkdirp@*',
        name: 'mkdirp',
        version: '*',
        peerMissing: [
          {requiredBy: 'npm-test-peer-deps-toplevel@0.0.0', requires: 'mkdirp@*'}
        ],
        dependencies: {}
      }
    },
    request: {
      peerMissing: true,
      required: {
        _id: 'request@0.9.x',
        dependencies: {},
        name: 'request',
        peerMissing: [
          {requiredBy: 'npm-test-peer-deps@0.0.0', requires: 'request@0.9.x'}
        ],
        version: '0.9.x'
      }
    }
  }
}

var server
var fixture = new Tacks(Dir({
  cache: Dir(),
  global: Dir(),
  tmp: Dir(),
  testdir: Dir({
    'package.json': File({
      name: 'npm-test-peer-deps-toplevel',
      version: '0.0.0',
      dependencies: {
        'npm-test-peer-deps': '*'
      },
      peerDependencies: {
        mkdirp: '*'
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

test('installs the peer dependency directory structure', function (t) {
  common.npm(['install'], conf, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'install ran ok even w/ missing peeer deps')
    t.comment(stdout.trim())
    t.comment(stderr.trim())

    common.npm(['ls', '--json'], conf, function (err, code, stdout, stderr) {
      if (err) throw err
      t.is(code, 1, 'missing peer deps _are_ an ls error though')
      t.comment(stderr.trim())
      var results = JSON.parse(stdout)

      t.deepEqual(results, expected, 'got expected output from ls')
      t.done()
    })
  })
})

test('cleanup', function (t) {
  server.close()
  cleanup()
  t.done()
})

