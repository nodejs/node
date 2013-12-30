// this is a test for fix #2538

// the false_name-test-package has the name property "test-package" set
// in the package.json and a dependency named "test-package" is also a
// defined dependency of "test-package-with-one-dep".
//
// this leads to a conflict during installation and the fix is covered
// by this test

var test = require("tap").test
  , fs = require("fs")
  , path = require("path")
  , existsSync = fs.existsSync || path.existsSync
  , spawn = require("child_process").spawn
  , npm = require("../../")
  , rimraf = require("rimraf")
  , common = require("../common-tap.js")
  , mr = require("npm-registry-mock")
  , pkg = __dirname + "/false_name"

test("not every pkg.name can be required", function (t) {
  rimraf.sync(pkg + "/cache")

  t.plan(1)
  mr(common.port, function (s) {
    setup(function () {
      npm.install(".", function (err) {
        if (err) return t.fail(err)
        s.close()
        t.ok(existsSync(pkg + "/node_modules/test-package-with-one-dep/" +
          "node_modules/test-package"))
      })
    })
  })
})

test("cleanup", function (t) {
  rimraf.sync(pkg + "/cache")
  rimraf.sync(pkg + "/node_modules")
  t.end()
})

function setup (cb) {
  process.chdir(pkg)
  npm.load({cache: pkg + "/cache", registry: common.registry},
    function () {
      rimraf.sync(pkg + "/node_modules")
      fs.mkdirSync(pkg + "/node_modules")
      cb()
    })
}
