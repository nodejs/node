var common = require("../common-tap.js")
var test = require("tap").test
var npm = require("../../")
var mkdirp = require("mkdirp")
var rimraf = require("rimraf")
var mr = require("npm-registry-mock")
var path = require("path")

// config
var pkg = path.resolve(__dirname, "outdated-include-devdependencies")
var cache = path.resolve(pkg, "cache")
mkdirp.sync(cache)

test("includes devDependencies in outdated", function (t) {
  process.chdir(pkg)
  mr({port : common.port}, function (er, s) {
    npm.load({cache: cache, registry: common.registry}, function () {
      npm.outdated(function (er, d) {
        t.equal("1.5.1", d[0][3])
        s.close()
        t.end()
      })
    })
  })
})

test("cleanup", function (t) {
  rimraf.sync(cache)
  t.end()
})
