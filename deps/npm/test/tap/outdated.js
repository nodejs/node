var common = require("../common-tap.js")
var fs = require("fs")
var test = require("tap").test
var rimraf = require("rimraf")
var npm = require("../../")

var mr = require("npm-registry-mock")
// config
var pkg = __dirname + '/outdated'

test("it should not throw", function (t) {
  cleanup()
  process.chdir(pkg)

  mr(common.port, function (s) {
    npm.load({
      cache: pkg + "/cache",
      loglevel: 'silent',
      registry: common.registry }
    , function () {
      npm.install(".", function (err) {
        npm.outdated(function (er, d) {
          console.log(d)
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

function cleanup () {
  rimraf.sync(pkg + "/node_modules")
  rimraf.sync(pkg + "/cache")
}
