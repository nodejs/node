'use strict'
var path = require('path')
var fs = require('graceful-fs')
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
var metricsFile = path.join(cachedir, 'anonymous-cli-metrics.json')

var conf = {
  cwd: testdir,
  env: extend(extend({}, process.env), {
    npm_config_cache: cachedir,
    npm_config_tmp: tmpdir,
    npm_config_prefix: globaldir,
    npm_config_registry: common.registry,
    npm_config_metrics_registry: common.registry,
    npm_config_loglevel: 'warn'
  })
}

var server
var fixture = new Tacks(Dir({
  cache: Dir(),
  global: Dir(),
  tmp: Dir(),
  testdir: Dir({
    failure: Dir({
      'package.json': File({
        name: 'failure',
        version: '1.0.0',
        scripts: {
          preinstall: 'false'
        }
      })
    }),
    success: Dir({
      'package.json': File({
        name: 'success',
        version: '1.0.0'
      })
    }),
    slow: Dir({
      'package.json': File({
        name: 'slow',
        version: '1.0.0',
        scripts: {
          preinstall: "node -e 'setTimeout(function(){}, 500)'"
        }
      })
    }),
    'package.json': File({
      name: 'anon-cli-metrics-test',
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

test('setup', function (t) {
  setup()
  mr({port: common.port, throwOnUnmatched: true}, function (err, s) {
    if (err) throw err
    server = s
    server.filteringPathRegEx(/([/]-[/]npm[/]anon-metrics[/]v1[/]).*/, '$1:id')
    server.filteringRequestBody(function (body) {
      var metrics = typeof body === 'string' ? JSON.parse(body) : body
      delete metrics.from
      delete metrics.to
      return JSON.stringify(metrics)
    })
    t.done()
  })
})

test('record success', function (t) {
  common.npm(['install', '--no-send-metrics', 'success'], conf, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'always succeeding install succeeded')
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    var data = JSON.parse(fs.readFileSync(metricsFile))
    t.is(data.metrics.successfulInstalls, 1)
    t.is(data.metrics.failedInstalls, 0)
    t.done()
  })
})

test('record failure', function (t) {
  server.put('/-/npm/anon-metrics/v1/:id', {
    successfulInstalls: 1,
    failedInstalls: 0
  }).reply(500, {ok: false})
  common.npm(['install', '--send-metrics', 'failure'], conf, function (err, code, stdout, stderr) {
    if (err) throw err
    t.notEqual(code, 0, 'always failing install fails')
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    var data = JSON.parse(fs.readFileSync(metricsFile))
    t.is(data.metrics.successfulInstalls, 1)
    t.is(data.metrics.failedInstalls, 1)
    t.done()
  })
})

test('report', function (t) {
  console.log('setup')

  server.put('/-/npm/anon-metrics/v1/:id', {
    successfulInstalls: 1,
    failedInstalls: 1
  }).reply(200, {ok: true})
  common.npm(['install', '--send-metrics', 'slow'], conf, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'command ran ok')
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    // todo check mock registry for post
    var data = JSON.parse(fs.readFileSync(metricsFile))
    t.is(data.metrics.successfulInstalls, 1)
    t.is(data.metrics.failedInstalls, 0)
    t.done()
  })
})

test('cleanup', function (t) {
  server.close()
  cleanup()
  t.done()
})
