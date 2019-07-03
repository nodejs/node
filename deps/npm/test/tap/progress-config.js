'use strict'
var test = require('tap').test
var log = require('npmlog')
var fs = require('graceful-fs')
const common = require('../common-tap.js')
var configName = common.pkg + '-npmrc'

// We use requireInject to get a fresh copy of
// the npm singleton each time we require it.
// If we didn't, we'd have shared state between
// these various tests.
var requireInject = require('require-inject')

// Make sure existing environment vars don't muck up the test
process.env = {}
// Pretend that stderr is a tty regardless, so we can get consistent
// results.
process.stderr.isTTY = true

test('setup', function (t) {
  fs.writeFileSync(configName, '')
  t.done()
})

function getFreshNpm () {
  return requireInject.withEmptyCache('../../lib/npm.js', {npmlog: log})
}

test('disabled', function (t) {
  t.plan(1)
  var npm = getFreshNpm()
  npm.load({userconfig: configName, progress: false}, function () {
    t.is(log.progressEnabled, false, 'should be disabled')
  })
})

test('enabled', function (t) {
  t.plan(1)
  var npm = getFreshNpm()
  npm.load({userconfig: configName, progress: true}, function () {
    t.is(log.progressEnabled, true, 'should be enabled')
  })
})

test('default', function (t) {
  t.plan(1)
  var npm = getFreshNpm()
  npm.load({userconfig: configName}, function () {
    t.is(log.progressEnabled, true, 'should be enabled')
  })
})

test('default-travis', function (t) {
  t.plan(1)
  process.env.TRAVIS = 'true'
  var npm = getFreshNpm()
  npm.load({userconfig: configName}, function () {
    t.is(log.progressEnabled, false, 'should be disabled')
    delete process.env.TRAVIS
  })
})

test('default-ci', function (t) {
  t.plan(1)
  process.env.CI = 'true'
  var npm = getFreshNpm()
  npm.load({userconfig: configName}, function () {
    t.is(log.progressEnabled, false, 'should be disabled')
    delete process.env.CI
  })
})

test('unicode-true', function (t) {
  var npm = getFreshNpm()
  npm.load({userconfig: configName, unicode: true}, function () {
    t.is(log.gauge._theme.hasUnicode, true, 'unicode will be selected')
    t.done()
  })
})

test('unicode-false', function (t) {
  var npm = getFreshNpm()
  npm.load({userconfig: configName, unicode: false}, function () {
    t.is(log.gauge._theme.hasUnicode, false, 'unicode will NOT be selected')
    t.done()
  })
})

test('cleanup', function (t) {
  fs.unlinkSync(configName)
  t.done()
})
