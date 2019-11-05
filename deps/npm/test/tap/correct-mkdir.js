/* eslint-disable camelcase */
var t = require('tap')
var test = t.test
var assert = require('assert')
var requireInject = require('require-inject')
const common = require('../common-tap.js')
var cache_dir = common.pkg
common.skipIfWindows('windows does not use correct-mkdir behavior')

test('correct-mkdir: no race conditions', function (t) {
  var mock_fs = {}
  var did_hook = false
  mock_fs.lstat = function (path, cb) {
    if (path === cache_dir) {
      // Return a non-matching owner
      cb(null, {
        uid: +process.getuid() + 1,
        isDirectory: function () {
          return true
        }
      })
      if (!did_hook) {
        did_hook = true
        doHook()
      }
    } else {
      assert.ok(false, 'Unhandled stat path: ' + path)
    }
  }
  var chown_in_progress = 0
  var mock_chownr = function (path, uid, gid, cb) {
    ++chown_in_progress
    process.nextTick(function () {
      --chown_in_progress
      cb(null)
    })
  }
  var mocks = {
    'graceful-fs': mock_fs,
    'chownr': mock_chownr,
    'infer-owner': requireInject('infer-owner', { fs: mock_fs })
  }
  var correctMkdir = requireInject('../../lib/utils/correct-mkdir.js', mocks)

  var calls_in_progress = 3
  function handleCallFinish () {
    t.equal(chown_in_progress, 0, 'should not return while chown still in progress')
    if (!--calls_in_progress) {
      t.end()
    }
  }
  function doHook () {
    // This is fired during the first correctMkdir call, after the stat has finished
    // but before the chownr has finished
    // Buggy old code will fail and return a cached value before initial call is done
    correctMkdir(cache_dir, handleCallFinish)
  }
  // Initial call
  correctMkdir(cache_dir, handleCallFinish)
  // Immediate call again in case of race condition there
  correctMkdir(cache_dir, handleCallFinish)
})

test('correct-mkdir: ignore ENOENTs from chownr', function (t) {
  var mock_fs = {}
  mock_fs.lstat = function (path, cb) {
    if (path === cache_dir) {
      cb(null, {
        isDirectory: function () {
          return true
        }
      })
    } else {
      assert.ok(false, 'Unhandled stat path: ' + path)
    }
  }
  var mock_chownr = function (path, uid, gid, cb) {
    cb(Object.assign(new Error(), {code: 'ENOENT'}))
  }
  var mocks = {
    'graceful-fs': mock_fs,
    'chownr': mock_chownr
  }
  var correctMkdir = requireInject('../../lib/utils/correct-mkdir.js', mocks)

  function handleCallFinish (err) {
    t.ifErr(err, 'chownr\'s ENOENT errors were ignored')
    t.end()
  }
  correctMkdir(cache_dir, handleCallFinish)
})

// NEED TO RUN LAST

// These test checks that Windows users are protected by crashes related to
// unexpectedly having a UID/GID other than 0 if a user happens to add these
// variables to their environment. There are assumptions in correct-mkdir
// that special-case Windows by checking on UID-related things.
test('correct-mkdir: SUDO_UID and SUDO_GID non-Windows', function (t) {
  process.env.SUDO_UID = 999
  process.env.SUDO_GID = 999
  process.getuid = function () { return 0 }
  process.getgid = function () { return 0 }
  var mock_fs = {}
  mock_fs.lstat = function (path, cb) {
    if (path === cache_dir) {
      cb(null, {
        uid: 0,
        isDirectory: function () {
          return true
        }
      })
    } else {
      assert.ok(false, 'Unhandled stat path: ' + path)
    }
  }
  var mock_chownr = function (path, uid, gid, cb) {
    t.is(uid, +process.env.SUDO_UID, 'using the environment\'s UID')
    t.is(gid, +process.env.SUDO_GID, 'using the environment\'s GID')
    cb(null, {})
  }
  var mocks = {
    'graceful-fs': mock_fs,
    'chownr': mock_chownr
  }
  var correctMkdir = requireInject('../../lib/utils/correct-mkdir.js', mocks)

  function handleCallFinish () {
    t.end()
  }
  correctMkdir(cache_dir, handleCallFinish)
})

test('correct-mkdir: SUDO_UID and SUDO_GID Windows', function (t) {
  process.env.SUDO_UID = 999
  process.env.SUDO_GID = 999
  delete process.getuid
  delete process.getgid
  var mock_fs = {}
  mock_fs.lstat = function (path, cb) {
    if (path === cache_dir) {
      cb(null, {
        uid: 0,
        isDirectory: function () {
          return true
        }
      })
    } else {
      assert.ok(false, 'Unhandled stat path: ' + path)
    }
  }
  var mock_chownr = function (path, uid, gid, cb) {
    t.fail('chownr should not be called at all on Windows')
    cb(new Error('nope'))
  }
  var mocks = {
    'graceful-fs': mock_fs,
    'chownr': mock_chownr
  }
  var correctMkdir = requireInject('../../lib/utils/correct-mkdir.js', mocks)

  function handleCallFinish (err) {
    t.ifErr(err, 'chownr was not called because Windows')
    t.end()
  }
  correctMkdir(cache_dir, handleCallFinish)
})
