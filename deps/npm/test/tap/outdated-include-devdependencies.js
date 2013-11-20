var common = require("../common-tap.js")
var test = require("tap").test
var npm = require("../../")
var mkdirp = require("mkdirp")
var rimraf = require("rimraf")
var mr = require("npm-registry-mock")

// config
var pkg = __dirname + '/outdated-include-devdependencies'
mkdirp.sync(pkg + "/cache")

test("includes devDependencies in outdated", function (t) {
  process.chdir(pkg)
  mr(common.port, function (s) {
    npm.load({cache: pkg + "/cache", registry: common.registry}, function () {
      npm.outdated(function (er, d) {
        t.equal("1.5.1", d[0][3])
        s.close()
        t.end()
      })
    })
  })
})

test("cleanup", function (t) {
  rimraf.sync(pkg + "/cache")
  t.end()
})
