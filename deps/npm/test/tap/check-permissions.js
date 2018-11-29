'use strict'
var fs = require('fs')
var path = require('path')
var test = require('tap').test
var rimraf = require('rimraf')
var writable = require('../../lib/install/writable.js').fsAccessImplementation
var writableFallback = require('../../lib/install/writable.js').fsOpenImplementation
var exists = require('../../lib/install/exists.js').fsAccessImplementation
var existsFallback = require('../../lib/install/exists.js').fsStatImplementation

const common = require('../common-tap.js')
var testBase = common.pkg
var existingDir = path.resolve(testBase, 'exists')
var nonExistingDir = path.resolve(testBase, 'does-not-exist')
var writableDir = path.resolve(testBase, 'writable')
var nonWritableDir = path.resolve(testBase, 'non-writable')

test('setup', function (t) {
  cleanup()
  setup()
  t.end()
})

test('exists', function (t) {
  t.plan(2)
  // fs.access first introduced in node 0.12 / io.js
  if (fs.access) {
    existsTests(t, exists)
  } else {
    t.pass('# skip fs.access not available in this version')
    t.pass('# skip fs.access not available in this version')
  }
})

test('exists-fallback', function (t) {
  t.plan(2)
  existsTests(t, existsFallback)
})

test('writable', function (t) {
  t.plan(2)
  // fs.access first introduced in node 0.12 / io.js
  if (fs.access) {
    writableTests(t, writable)
  } else {
    t.pass('# skip fs.access not available in this version')
    t.pass('# skip fs.access not available in this version')
  }
})

test('writable-fallback', function (t) {
  t.plan(2)
  writableTests(t, writableFallback)
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function setup () {
  fs.mkdirSync(testBase)
  fs.mkdirSync(existingDir)
  fs.mkdirSync(writableDir)
  fs.mkdirSync(nonWritableDir)
  fs.chmodSync(nonWritableDir, '555')
}

function existsTests (t, exists) {
  exists(existingDir, function (er) {
    t.error(er, 'exists dir is exists')
  })
  exists(nonExistingDir, function (er) {
    t.ok(er, 'non-existing dir resulted in an error')
  })
}

function writableTests (t, writable) {
  writable(writableDir, function (er) {
    t.error(er, 'writable dir is writable')
  })
  if (process.platform === 'win32') {
    t.pass('windows folders cannot be read-only')
  } else if (process.getuid && process.getuid() === 0) {
    t.pass('root is not blocked by read-only dirs')
  } else {
    writable(nonWritableDir, function (er) {
      t.ok(er, 'non-writable dir resulted in an error')
    })
  }
}

function cleanup () {
  rimraf.sync(testBase)
}
