var common = require("../common-tap.js")
var fs = require("fs")
var test = require("tap").test
var rimraf = require("rimraf")
var npm = require("../../")

var mr = require("npm-registry-mock")
// config
var pkg = __dirname + '/outdated'

var path = require("path")

test("it should not throw", function (t) {
  cleanup()
  process.chdir(pkg)

  var originalLog = console.log
  var output = []
  var expOut = [ path.resolve(__dirname, "outdated/node_modules/underscore")
               , path.resolve(__dirname, "outdated/node_modules/underscore")
               + ":underscore@1.3.1"
               + ":underscore@1.3.1"
               + ":underscore@1.5.1" ]
  var expData = [ [ path.resolve(__dirname, "outdated")
                  , "underscore"
                  , "1.3.1"
                  , "1.3.1"
                  , "1.5.1"
                  , "1.3.1" ] ]

  console.log = function () {
    output.push.apply(output, arguments)
  }
  mr(common.port, function (s) {
    npm.load({
      cache: pkg + "/cache",
      loglevel: 'silent',
      parseable: true,
      registry: common.registry }
    , function () {
      npm.install(".", function (err) {
        npm.outdated(function (er, d) {
          console.log = originalLog
          t.same(output, expOut)
          t.same(d, expData)
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
