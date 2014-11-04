var common = require("../common-tap")
var existsSync = require("fs").existsSync
var join = require("path").join

var test = require("tap").test
var rimraf = require("rimraf")
var mkdirp = require("mkdirp")

var pkg = join(__dirname, "install-from-local", "package-with-scoped-paths")
var modules = join(pkg, "node_modules")

var EXEC_OPTS = {
  cwd : pkg
}

test("setup", function (t) {
  rimraf.sync(modules)
  rimraf.sync(join(pkg, "cache"))
  process.chdir(pkg)
  mkdirp.sync(modules)
  t.end()
})

test("installing already installed local scoped package", function (t) {
  common.npm(["install", "--loglevel", "silent"], EXEC_OPTS, function (err, code, stdout) {
    var installed = parseNpmInstallOutput(stdout)
    t.ifError(err, "error should not exist")
    t.notOk(code, "npm install exited with code 0")
    t.ifError(err, "install ran to completion without error")
    t.ok(
      existsSync(join(modules, "@scoped", "package", "package.json")),
      "package installed"
    )
    t.ok(
      contains(installed, "node_modules/@scoped/package"),
      "installed @scoped/package"
    )
    t.ok(
      contains(installed, "node_modules/package-local-dependency"),
      "installed package-local-dependency"
    )

    common.npm(["install", "--loglevel", "silent"], EXEC_OPTS, function (err, code, stdout) {
      installed = parseNpmInstallOutput(stdout)
      t.ifError(err, "error should not exist")
      t.notOk(code, "npm install exited with code 0")

      t.ifError(err, "install ran to completion without error")

      t.ok(
        existsSync(join(modules, "@scoped", "package", "package.json")),
        "package installed"
      )

      t.notOk(
        contains(installed, "node_modules/@scoped/package"),
        "did not reinstall @scoped/package"
      )
      t.notOk(
        contains(installed, "node_modules/package-local-dependency"),
        "did not reinstall package-local-dependency"
      )
      t.end()
    })
  })
})

test("cleanup", function (t) {
  process.chdir(__dirname)
  rimraf.sync(join(modules))
  rimraf.sync(join(pkg, "cache"))
  t.end()
})

function contains(list, element) {
  for (var i=0; i < list.length; ++i) {
    if (list[i] === element) {
      return true
    }
  }
  return false
}

function parseNpmInstallOutput(stdout) {
  return stdout.trim().split(/\n\n|\s+/)
}
