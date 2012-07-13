var test = require('tap').test
var lockFile = require('../lockfile.js')
var path = require('path')
var fs = require('fs')

test('setup', function (t) {
  try { lockFile.unlockSync('basic-lock') } catch (er) {}
  try { lockFile.unlockSync('sync-lock') } catch (er) {}
  try { lockFile.unlockSync('never-forget') } catch (er) {}
  try { lockFile.unlockSync('stale-lock') } catch (er) {}
  try { lockFile.unlockSync('watch-lock') } catch (er) {}
  try { lockFile.unlockSync('retry-lock') } catch (er) {}
  t.end()
})

test('basic test', function (t) {
  lockFile.check('basic-lock', function (er, locked) {
    if (er) throw er
    t.notOk(locked)
    lockFile.lock('basic-lock', function (er) {
      if (er) throw er
      lockFile.lock('basic-lock', function (er) {
        t.ok(er)
        lockFile.check('basic-lock', function (er, locked) {
          if (er) throw er
          t.ok(locked)
          lockFile.unlock('basic-lock', function (er) {
            if (er) throw er
            lockFile.check('basic-lock', function (er, locked) {
              if (er) throw er
              t.notOk(locked)
              t.end()
            })
          })
        })
      })
    })
  })
})

test('sync test', function (t) {
  var locked
  locked = lockFile.checkSync('sync-lock')
  t.notOk(locked)
  lockFile.lockSync('sync-lock')
  locked = lockFile.checkSync('sync-lock')
  t.ok(locked)
  lockFile.unlockSync('sync-lock')
  locked = lockFile.checkSync('sync-lock')
  t.notOk(locked)
  t.end()
})

test('exit cleanup test', function (t) {
  var child = require.resolve('./fixtures/child.js')
  var node = process.execPath
  var spawn = require('child_process').spawn
  spawn(node, [child]).on('exit', function () {
    setTimeout(function () {
      var locked = lockFile.checkSync('never-forget')
      t.notOk(locked)
      t.end()
    }, 100)
  })
})

test('error exit cleanup test', function (t) {
  var child = require.resolve('./fixtures/bad-child.js')
  var node = process.execPath
  var spawn = require('child_process').spawn
  spawn(node, [child]).on('exit', function () {
    setTimeout(function () {
      var locked = lockFile.checkSync('never-forget')
      t.notOk(locked)
      t.end()
    }, 100)
  })
})


test('staleness test', function (t) {
  lockFile.lock('stale-lock', function (er) {
    if (er) throw er

    var opts = { stale: 1 }
    setTimeout(next, 10)
    function next () {
      lockFile.check('stale-lock', opts, function (er, locked) {
        if (er) throw er
        t.notOk(locked)
        lockFile.lock('stale-lock', opts, function (er) {
          if (er) throw er
          lockFile.unlock('stale-lock', function (er) {
            if (er) throw er
            t.end()
          })
        })
      })
    }
  })
})

test('staleness sync test', function (t) {
  var opts = { stale: 1 }
  lockFile.lockSync('stale-lock')
  setTimeout(next, 10)
  function next () {
    var locked
    locked = lockFile.checkSync('stale-lock', opts)
    t.notOk(locked)
    lockFile.lockSync('stale-lock', opts)
    lockFile.unlockSync('stale-lock')
    t.end()
  }
})

test('watch test', function (t) {
  var opts = { wait: 100 }
  var fdx
  lockFile.lock('watch-lock', function (er, fd1) {
    if (er) throw er
    setTimeout(unlock, 10)
    function unlock () {
      console.error('unlocking it')
      lockFile.unlockSync('watch-lock')
      // open another file, so the fd gets reused
      // so we can know that it actually re-opened it fresh,
      // rather than just getting the same lock as before.
      fdx = fs.openSync('x', 'w')
      fdy = fs.openSync('x', 'w')
    }

    // should have gotten a new fd
    lockFile.lock('watch-lock', opts, function (er, fd2) {
      if (er) throw er
      t.notEqual(fd1, fd2)
      fs.closeSync(fdx)
      fs.closeSync(fdy)
      fs.unlinkSync('x')
      lockFile.unlockSync('watch-lock')
      t.end()
    })
  })
})

test('retries', function (t) {
  // next 5 opens will fail.
  var opens = 5
  fs._open = fs.open
  fs.open = function (path, mode, cb) {
    if (--opens === 0) {
      fs.open = fs._open
      return fs.open(path, mode, cb)
    }
    var er = new Error('bogus')
    // to be, or not to be, that is the question.
    er.code = opens % 2 ? 'EEXIST' : 'ENOENT'
    process.nextTick(cb.bind(null, er))
  }

  lockFile.lock('retry-lock', { retries: opens }, function (er, fd) {
    if (er) throw er
    t.equal(opens, 0)
    t.ok(fd)
    lockFile.unlockSync('retry-lock')
    t.end()
  })
})

test('retryWait', function (t) {
  // next 5 opens will fail.
  var opens = 5
  fs._open = fs.open
  fs.open = function (path, mode, cb) {
    if (--opens === 0) {
      fs.open = fs._open
      return fs.open(path, mode, cb)
    }
    var er = new Error('bogus')
    // to be, or not to be, that is the question.
    er.code = opens % 2 ? 'EEXIST' : 'ENOENT'
    process.nextTick(cb.bind(null, er))
  }

  var opts = { retries: opens, retryWait: 100 }
  lockFile.lock('retry-lock', opts, function (er, fd) {
    if (er) throw er
    t.equal(opens, 0)
    t.ok(fd)
    lockFile.unlockSync('retry-lock')
    t.end()
  })
})

test('retry sync', function (t) {
  // next 5 opens will fail.
  var opens = 5
  fs._openSync = fs.openSync
  fs.openSync = function (path, mode) {
    if (--opens === 0) {
      fs.openSync = fs._openSync
      return fs.openSync(path, mode)
    }
    var er = new Error('bogus')
    // to be, or not to be, that is the question.
    er.code = opens % 2 ? 'EEXIST' : 'ENOENT'
    throw er
  }

  var opts = { retries: opens }
  lockFile.lockSync('retry-lock', opts)
  t.equal(opens, 0)
  lockFile.unlockSync('retry-lock')
  t.end()
})

test('cleanup', function (t) {
  try { lockFile.unlockSync('basic-lock') } catch (er) {}
  try { lockFile.unlockSync('sync-lock') } catch (er) {}
  try { lockFile.unlockSync('never-forget') } catch (er) {}
  try { lockFile.unlockSync('stale-lock') } catch (er) {}
  try { lockFile.unlockSync('watch-lock') } catch (er) {}
  try { lockFile.unlockSync('retry-lock') } catch (er) {}
  t.end()
})

