'use strict'
var test = require('tap').test
var log = require('npmlog')

// We use requireInject to get a fresh copy of
// the npm singleton each time we require it.
// If we didn't, we'd have shared state between
// these various tests.
var requireInject = require('require-inject')

// Make sure existing environment vars don't muck up the test
process.env = {}

function hasOnlyAscii (s) {
  return /^[\000-\177]*$/.test(s)
}

test('disabled', function (t) {
  t.plan(1)
  var npm = requireInject('../../lib/npm.js', {})
  npm.load({progress: false}, function () {
    t.is(log.progressEnabled, false, 'should be disabled')
  })
})

test('enabled', function (t) {
  t.plan(1)
  var npm = requireInject('../../lib/npm.js', {})
  npm.load({progress: true}, function () {
    t.is(log.progressEnabled, true, 'should be enabled')
  })
})

test('default', function (t) {
  t.plan(1)
  var npm = requireInject('../../lib/npm.js', {})
  npm.load({}, function () {
    t.is(log.progressEnabled, true, 'should be enabled')
  })
})

test('default-travis', function (t) {
  t.plan(1)
  global.process.env.TRAVIS = 'true'
  var npm = requireInject('../../lib/npm.js', {})
  npm.load({}, function () {
    t.is(log.progressEnabled, false, 'should be disabled')
    delete global.process.env.TRAVIS
  })
})

test('default-ci', function (t) {
  t.plan(1)
  global.process.env.CI = 'true'
  var npm = requireInject('../../lib/npm.js', {})
  npm.load({}, function () {
    t.is(log.progressEnabled, false, 'should be disabled')
    delete global.process.env.CI
  })
})

test('unicode-true', function (t) {
  t.plan(6)
  var npm = requireInject('../../lib/npm.js', {})
  npm.load({unicode: true}, function () {
    Object.keys(log.gauge.theme).forEach(function (key) {
      t.notOk(hasOnlyAscii(log.gauge.theme[key]), 'only unicode')
    })
  })
})

test('unicode-false', function (t) {
  t.plan(6)
  var npm = requireInject('../../lib/npm.js', {})
  npm.load({unicode: false}, function () {
    Object.keys(log.gauge.theme).forEach(function (key) {
      t.ok(hasOnlyAscii(log.gauge.theme[key]), 'only ASCII')
    })
  })
})
