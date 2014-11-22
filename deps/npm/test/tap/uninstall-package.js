var test = require("tap").test
  , npm = require("../../")
  , rimraf = require("rimraf")
  , mr = require("npm-registry-mock")
  , common = require("../common-tap.js")
  , path = require("path")
  , pkg = path.join(__dirname, "uninstall-package")

test("returns a list of removed items", function (t) {
  t.plan(1)
  mr(common.port, function (s) {
    setup(function () {
      npm.install(".", function (err) {
        if (err) return t.fail(err)
        npm.uninstall("underscore", "request", "lala", function (err, d) {
          if (err) return t.fail(err)
          t.same(d.sort(), ["underscore", "request"].sort())
          s.close()
          t.end()
        })
      })
    })
  })
})

test("cleanup", function (t) {
  cleanup()
  t.end()
})

function setup (cb) {
  cleanup()
  process.chdir(pkg)
  npm.load({cache: pkg + "/cache", registry: common.registry}, function () {
    cb()
  })
}

function cleanup () {
  rimraf.sync(pkg + "/node_modules")
  rimraf.sync(pkg + "/cache")
}
