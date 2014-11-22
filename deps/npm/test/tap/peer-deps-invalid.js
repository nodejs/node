var common = require("../common-tap")
var fs = require("fs")
var path = require("path")
var test = require("tap").test
var rimraf = require("rimraf")
var npm = require("../../")
var mr = require("npm-registry-mock")
var pkg = path.resolve(__dirname, "peer-deps-invalid")
var cache = path.resolve(pkg, "cache")
var nodeModules = path.resolve(pkg, "node_modules")

var okFile = fs.readFileSync(path.join(pkg, "file-ok.js"), "utf8")
var failFile = fs.readFileSync(path.join(pkg, "file-fail.js"), "utf8")

test("installing dependencies that have conflicting peerDependencies", function (t) {
  rimraf.sync(nodeModules)
  rimraf.sync(cache)
  process.chdir(pkg)

  var customMocks = {
    "get": {
      "/ok.js": [200, okFile],
      "/invalid.js": [200, failFile]
    }
  }
  mr({port: common.port, mocks: customMocks}, function (s) { // create mock registry.
    npm.load({
      cache: cache,
      registry: common.registry
    }, function () {
      npm.commands.install([], function (err) {
        if (!err) {
          t.fail("No error!")
        } else {
          t.equal(err.code, "EPEERINVALID")
        }
        t.end()
        s.close() // shutdown mock registry.
      })
    })
  })
})

test("cleanup", function (t) {
  rimraf.sync(nodeModules)
  rimraf.sync(cache)
  t.end()
})
