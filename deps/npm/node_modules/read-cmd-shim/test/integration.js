'use strict'
var path = require('path')
var fs = require('graceful-fs')
var test = require('tap').test
var rimraf = require('rimraf')
var cmdShim = require('cmd-shim')
var readCmdShim = require('../index.js')
var workDir = path.join(__dirname, path.basename(__filename, '.js'))
var testShbang = path.join(workDir, 'test-shbang')
var testShbangCmd = testShbang + '.cmd'
var testShim = path.join(workDir, 'test')
var testShimCmd = testShim + '.cmd'

test('setup', function (t) {
  rimraf.sync(workDir)
  fs.mkdirSync(workDir)
  fs.writeFileSync(testShbang + '.js', '#!/usr/bin/env node\ntrue')
  cmdShim(__filename, testShim, function (er) {
    t.error(er)
    cmdShim(testShbang + '.js', testShbang, function (er) {
      t.error(er)
      t.done()
    })
  })
})

test('async-read-no-shbang', function (t) {
  t.plan(2)
  readCmdShim(testShimCmd, function (er, dest) {
    t.error(er)
    t.is(dest, '..\\basic.js')
    t.done()
  })
})

test('sync-read-no-shbang', function (t) {
  t.plan(1)
  var dest = readCmdShim.sync(testShimCmd)
  t.is(dest, '..\\basic.js')
  t.done()
})

test('async-read-shbang', function (t) {
  t.plan(2)
  readCmdShim(testShbangCmd, function (er, dest) {
    t.error(er)
    t.is(dest, 'test-shbang.js')
    t.done()
  })
})

test('sync-read-shbang', function (t) {
  t.plan(1)
  var dest = readCmdShim.sync(testShbangCmd)
  t.is(dest, 'test-shbang.js')
  t.done()
})

test('async-read-no-shbang-cygwin', function (t) {
  t.plan(2)
  readCmdShim(testShim, function (er, dest) {
    t.error(er)
    t.is(dest, '../basic.js')
    t.done()
  })
})

test('sync-read-no-shbang-cygwin', function (t) {
  t.plan(1)
  var dest = readCmdShim.sync(testShim)
  t.is(dest, '../basic.js')
  t.done()
})

test('async-read-shbang-cygwin', function (t) {
  t.plan(2)
  readCmdShim(testShbang, function (er, dest) {
    t.error(er)
    t.is(dest, 'test-shbang.js')
    t.done()
  })
})

test('sync-read-shbang-cygwin', function (t) {
  t.plan(1)
  var dest = readCmdShim.sync(testShbang)
  t.is(dest, 'test-shbang.js')
  t.done()
})

test('async-read-dir', function (t) {
  t.plan(2)
  readCmdShim(workDir, function (er) {
    t.ok(er)
    t.is(er.code, 'EISDIR', "cmd-shims can't be directories")
    t.done()
  })
})

test('sync-read-dir', function (t) {
  t.plan(1)
  t.throws(function () { readCmdShim.sync(workDir) }, "cmd-shims can't be directories")
  t.done()
})

test('async-read-not-there', function (t) {
  t.plan(2)
  readCmdShim('/path/to/nowhere', function (er, dest) {
    t.ok(er, 'missing files throw errors')
    t.is(er.code, 'ENOENT', "cmd-shim file doesn't exist")
    t.done()
  })
})

test('sync-read-not-there', function (t) {
  t.plan(1)
  t.throws(function () { readCmdShim.sync('/path/to/nowhere') }, "cmd-shim file doesn't exist")
  t.done()
})

test('async-read-not-shim', function (t) {
  t.plan(2)
  readCmdShim(__filename, function (er, dest) {
    t.ok(er)
    t.is(er.code, 'ENOTASHIM', 'shim file specified is not a shim')
    t.done()
  })
})

test('sync-read-not-shim', function (t) {
  t.plan(1)
  t.throws(function () { readCmdShim.sync(__filename) }, 'shim file specified is not a shim')
  t.done()
})

test('cleanup', function (t) {
  rimraf.sync(workDir)
  t.done()
})
