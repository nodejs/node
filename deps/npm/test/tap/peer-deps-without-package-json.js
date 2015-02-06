var common = require("../common-tap")
var fs = require("fs")
var path = require("path")
var test = require("tap").test
var rimraf = require("rimraf")
var npm = require("../../")
var mr = require("npm-registry-mock")
var pkg = path.resolve(__dirname, "peer-deps-without-package-json")
var cache = path.resolve(pkg, "cache")
var nodeModules = path.resolve(pkg, "node_modules")

test("installing a peerDependencies-using package without a package.json present (GH-3049)", function (t) {

  rimraf.sync(nodeModules)
  rimraf.sync(cache)

  fs.mkdirSync(nodeModules)
  process.chdir(pkg)

  var customMocks = {
    "get": {
      "/ok.js": [200, path.join(pkg, "file-js.js")]
    }
  }
  mr({port: common.port, mocks: customMocks}, function (err, s) {
    t.ifError(err, "mock registry booted")
    npm.load({
      registry: common.registry,
      cache: cache
    }, function () {
      npm.install(common.registry + "/ok.js", function (err) {
        t.ifError(err, "installed ok.js")

        t.ok(
          fs.existsSync(path.join(nodeModules, "/npm-test-peer-deps-file")),
          "passive peer dep installed"
        )
        t.ok(
          fs.existsSync(path.join(nodeModules, "/underscore")),
          "underscore installed"
        )

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
