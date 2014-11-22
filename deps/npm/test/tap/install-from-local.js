var common = require("../common-tap")
var test = require("tap").test
var path = require("path")
var fs = require("fs")
var rimraf = require("rimraf")
var pkg = path.join(__dirname, "install-from-local", "package-with-local-paths")

var EXEC_OPTS = { }

test("setup", function (t) {
  process.chdir(pkg)
  t.end()
})

test('"npm install" should install local packages', function (t) {
  common.npm(["install", "."], EXEC_OPTS, function (err, code) {
    t.ifError(err, "error should not exist")
    t.notOk(code, "npm install exited with code 0")
    var dependencyPackageJson = path.resolve(pkg, "node_modules/package-local-dependency/package.json")
    t.ok(
      JSON.parse(fs.readFileSync(dependencyPackageJson, "utf8")),
      "package with local dependency installed"
    )

    var devDependencyPackageJson = path.resolve(pkg, "node_modules/package-local-dev-dependency/package.json")
    t.ok(
      JSON.parse(fs.readFileSync(devDependencyPackageJson, "utf8")),
      "package with local dev dependency installed"
    )

    t.end()
  })
})

test("cleanup", function (t) {
  process.chdir(__dirname)
  rimraf.sync(path.resolve(pkg, "node_modules"))
  t.end()
})
