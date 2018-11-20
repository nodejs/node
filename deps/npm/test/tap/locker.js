var test = require("tap").test
  , path = require("path")
  , fs = require("graceful-fs")
  , crypto = require("crypto")
  , rimraf = require("rimraf")
  , osenv = require("osenv")
  , mkdirp = require("mkdirp")
  , npm = require("../../")
  , locker = require("../../lib/utils/locker.js")
  , lock = locker.lock
  , unlock = locker.unlock

var pkg = path.join(__dirname, "/locker")
  , cache = path.join(pkg, "/cache")
  , tmp = path.join(pkg, "/tmp")
  , nm = path.join(pkg, "/node_modules")

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}

test("setup", function (t) {
  cleanup()
  mkdirp.sync(cache)
  mkdirp.sync(tmp)
  t.end()
})

test("locking file puts lock in correct place", function (t) {
  npm.load({cache: cache, tmpdir: tmp}, function (er) {
    t.ifError(er, "npm bootstrapped OK")

    var n = "correct"
      , c = n.replace(/[^a-zA-Z0-9]+/g, "-").replace(/^-+|-+$/g, "")
      , p = path.resolve(nm, n)
      , h = crypto.createHash("sha1").update(p).digest("hex")
      , l = c.substr(0, 24)+"-"+h.substr(0, 16)+".lock"
      , v = path.join(cache, "_locks",  l)

    lock(nm, n, function (er) {
      t.ifError(er, "locked path")

      fs.exists(v, function (found) {
        t.ok(found, "lock found OK")

        unlock(nm, n, function (er) {
          t.ifError(er, "unlocked path")

          fs.exists(v, function (found) {
            t.notOk(found, "lock deleted OK")
            t.end()
          })
        })
      })
    })
  })
})

test("unlocking out of order errors out", function (t) {
  npm.load({cache: cache, tmpdir: tmp}, function (er) {
    t.ifError(er, "npm bootstrapped OK")

    var n = "busted"
      , c = n.replace(/[^a-zA-Z0-9]+/g, "-").replace(/^-+|-+$/g, "")
      , p = path.resolve(nm, n)
      , h = crypto.createHash("sha1").update(p).digest("hex")
      , l = c.substr(0, 24)+"-"+h.substr(0, 16)+".lock"
      , v = path.join(cache, "_locks",  l)

    fs.exists(v, function (found) {
      t.notOk(found, "no lock to unlock")

      t.throws(function () {
        unlock(nm, n, function () {
          t.fail("shouldn't get here")
          t.end()
        })
      }, "blew up as expected")

      t.end()
    })
  })
})

test("cleanup", function (t) {
  cleanup()
  t.end()
})
