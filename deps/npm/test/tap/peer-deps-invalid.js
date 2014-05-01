var common = require('../common-tap.js')
var fs = require("fs")
var path = require("path")
var test = require("tap").test
var rimraf = require("rimraf")
var npm = require("../../")
var mr = require("npm-registry-mock")
var pkg = __dirname + "/peer-deps-invalid"

var okFile = fs.readFileSync(path.join(pkg, "file-ok.js"), "utf8")
var failFile = fs.readFileSync(path.join(pkg, "file-fail.js"), "utf8")

test("installing dependencies that have conflicting peerDependencies", function (t) {
  rimraf.sync(pkg + "/node_modules")
  rimraf.sync(pkg + "/cache")
  process.chdir(pkg)

  var customMocks = {
    "get": {
      "/ok.js": [200, okFile],
      "/invalid.js": [200, failFile]
    }
  }
  mr({port: common.port, mocks: customMocks}, function (s) { // create mock registry.
    npm.load({
      cache: pkg + "/cache",
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
  rimraf.sync(pkg + "/node_modules")
  rimraf.sync(pkg + "/cache")
  t.end()
})
