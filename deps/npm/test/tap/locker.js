var test = require('tap').test
var path = require('path')
var fs = require('graceful-fs')
var crypto = require('crypto')
var rimraf = require('rimraf')
var osenv = require('osenv')
var mkdirp = require('mkdirp')
var npm = require('../../')
var locker = require('../../lib/utils/locker.js')
var lock = locker.lock
var unlock = locker.unlock

const common = require('../common-tap.js')
var pkg = common.pkg
var cache = path.join(pkg, '/cache')
var tmp = path.join(pkg, '/tmp')
var nm = path.join(pkg, '/node_modules')

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}

test('setup', function (t) {
  cleanup()
  mkdirp.sync(cache)
  mkdirp.sync(tmp)
  t.end()
})

test('locking file puts lock in correct place', function (t) {
  npm.load({cache: cache, tmpdir: tmp}, function (er) {
    t.ifError(er, 'npm bootstrapped OK')

    var n = 'correct'
    var c = n.replace(/[^a-zA-Z0-9]+/g, '-').replace(/^-+|-+$/g, '')
    var p = path.resolve(nm, n)
    var h = crypto.createHash('sha1').update(p).digest('hex')
    var l = c.substr(0, 24) + '-' + h.substr(0, 16) + '.lock'
    var v = path.join(cache, '_locks', l)

    lock(nm, n, function (er) {
      t.ifError(er, 'locked path')

      fs.exists(v, function (found) {
        t.ok(found, 'lock found OK')

        unlock(nm, n, function (er) {
          t.ifError(er, 'unlocked path')

          fs.exists(v, function (found) {
            t.notOk(found, 'lock deleted OK')
            t.end()
          })
        })
      })
    })
  })
})

test('unlocking out of order errors out', function (t) {
  npm.load({cache: cache, tmpdir: tmp}, function (er) {
    t.ifError(er, 'npm bootstrapped OK')

    var n = 'busted'
    var c = n.replace(/[^a-zA-Z0-9]+/g, '-').replace(/^-+|-+$/g, '')
    var p = path.resolve(nm, n)
    var h = crypto.createHash('sha1').update(p).digest('hex')
    var l = c.substr(0, 24) + '-' + h.substr(0, 16) + '.lock'
    var v = path.join(cache, '_locks', l)

    fs.exists(v, function (found) {
      t.notOk(found, 'no lock to unlock')

      t.throws(function () {
        unlock(nm, n, function () {
          t.fail("shouldn't get here")
          t.end()
        })
      }, 'blew up as expected')

      t.end()
    })
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})
