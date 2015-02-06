var path = require("path")

var mr = require("npm-registry-mock")
var rimraf = require("rimraf")
var test = require("tap").test

var common = require("../common-tap.js")
var npm = require("../../")

// config
var pkg = path.resolve(__dirname, "outdated")
var cache = path.resolve(pkg, "cache")
var nodeModules = path.resolve(pkg, "node_modules")

test("it should not throw", function (t) {
  cleanup()
  process.chdir(pkg)

  var originalLog = console.log
  var output = []
  var expOut = [ path.resolve(__dirname, "outdated/node_modules/underscore"),
                 path.resolve(__dirname, "outdated/node_modules/underscore")
               + ":underscore@1.3.1"
               + ":underscore@1.3.1"
               + ":underscore@1.5.1"
               + ":dependencies" ]
  var expData = [ [ path.resolve(__dirname, "outdated"),
                    "underscore",
                    "1.3.1",
                    "1.3.1",
                    "1.5.1",
                    "1.3.1",
                    "dependencies" ] ]

  console.log = function () {
    output.push.apply(output, arguments)
  }
  mr({port : common.port}, function (er, s) {
    npm.load({
      cache : "cache",
      loglevel : "silent",
      parseable : true,
      registry : common.registry
    },
    function () {
      npm.install(".", function (err) {
        t.ifError(err, "install success")
        npm.config.set("long", true)
        npm.outdated(function (er, d) {
          t.ifError(er, "outdated success")
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
  rimraf.sync(nodeModules)
  rimraf.sync(cache)
}
