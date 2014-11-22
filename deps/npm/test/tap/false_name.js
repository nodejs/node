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
  , rimraf = require("rimraf")
  , common = require("../common-tap.js")
  , mr = require("npm-registry-mock")
  , pkg = path.resolve(__dirname, "false_name")
  , cache = path.resolve(pkg, "cache")
  , nodeModules = path.resolve(pkg, "node_modules")

var EXEC_OPTS = { cwd: pkg }

test("setup", function(t) {
  cleanup()
  fs.mkdirSync(nodeModules)
  t.end()
})

test("not every pkg.name can be required", function (t) {
  t.plan(3)
  mr(common.port, function (s) {
    common.npm([
      "install", ".",
      "--cache", cache,
      "--registry", common.registry
    ], EXEC_OPTS, function (err, code) {
      s.close()
      t.ifErr(err, "install finished without error")
      t.equal(code, 0, "install exited ok")
      t.ok(existsSync(path.resolve(pkg,
        "node_modules/test-package-with-one-dep",
        "node_modules/test-package")))
    })
  })
})

function cleanup() {
  rimraf.sync(cache)
  rimraf.sync(nodeModules)
}

test("cleanup", function (t) {
  cleanup()
  t.end()
})
