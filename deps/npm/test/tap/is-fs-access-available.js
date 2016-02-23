'use strict'
var test = require('tap').test
var requireInject = require('require-inject')
var semver = require('semver')

var globalProcess = global.process

function loadIsFsAccessAvailable (newProcess, fs) {
  global.process = newProcess
  var mocks = {fs: fs}
  var isFsAccessAvailable = requireInject('../../lib/install/is-fs-access-available.js', mocks)
  global.process = globalProcess
  return isFsAccessAvailable
}

var fsWithAccess = {access: function () {}}
var fsWithoutAccess = {}

if (semver.lt(process.version, '0.12.0')) {
  test('skipping', function (t) {
    t.pass('skipping all tests on < 0.12.0 due to process not being injectable')
    t.end()
  })
} else {
  test('mac + !fs.access', function (t) {
    var isFsAccessAvailable = loadIsFsAccessAvailable({platform: 'darwin'}, fsWithoutAccess)
    t.is(isFsAccessAvailable, false, 'not available')
    t.end()
  })

  test('mac + fs.access', function (t) {
    var isFsAccessAvailable = loadIsFsAccessAvailable({platform: 'darwin'}, fsWithAccess)
    t.is(isFsAccessAvailable, true, 'available')
    t.end()
  })

  test('windows + !fs.access', function (t) {
    var isFsAccessAvailable = loadIsFsAccessAvailable({platform: 'win32'}, fsWithoutAccess)
    t.is(isFsAccessAvailable, false, 'not available')
    t.end()
  })

  test('windows + fs.access + node 0.12.7', function (t) {
    var isFsAccessAvailable = loadIsFsAccessAvailable({platform: 'win32', version: '0.12.7'}, fsWithAccess)
    t.is(isFsAccessAvailable, false, 'not available')
    t.end()
  })

  test('windows + fs.access + node 2.4.0', function (t) {
    var isFsAccessAvailable = loadIsFsAccessAvailable({platform: 'win32', version: '2.4.0'}, fsWithAccess)
    t.is(isFsAccessAvailable, true, 'available')
    t.end()
  })
}
