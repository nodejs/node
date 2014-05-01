var common = require('../common-tap.js')
var fs = require("fs")
var path = require("path")
var test = require("tap").test
var rimraf = require("rimraf")
var npm = require("../../")
var mr = require("npm-registry-mock")
var pkg = __dirname + "/peer-deps-without-package-json"

var js = fs.readFileSync(path.join(pkg, "file-js.js"), "utf8")
test("installing a peerDependencies-using package without a package.json present (GH-3049)", function (t) {

  rimraf.sync(pkg + "/node_modules")
  rimraf.sync(pkg + "/cache")

  fs.mkdirSync(pkg + "/node_modules")
  process.chdir(pkg)

  var customMocks = {
    "get": {
      "/ok.js": [200, js],
    }
  }
  mr({port: common.port, mocks: customMocks}, function (s) { // create mock registry.
    npm.load({
      registry: common.registry,
      cache: pkg + "/cache"
    }, function () {
      npm.install(common.registry + "/ok.js", function (err) {
        if (err) {
          t.fail(err)
        } else {
          t.ok(fs.existsSync(pkg + "/node_modules/npm-test-peer-deps-file"))
          t.ok(fs.existsSync(pkg + "/node_modules/underscore"))
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
