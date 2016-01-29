var test = require('tap').test
var assert = require('assert')
var path = require('path')
var requireInject = require('require-inject')
var cache_dir = path.resolve(__dirname, 'correct-mkdir')

test('correct-mkdir: no race conditions', function (t) {
  var mock_fs = {}
  var did_hook = false
  mock_fs.stat = function (path, cb) {
    if (path === cache_dir) {
      // Return a non-matching owner
      cb(null, {
        uid: +process.uid + 1,
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
    'chownr': mock_chownr
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
