var common = require("../common-tap.js")
var existsSync = require("fs").existsSync
var join = require("path").join
var exec = require("child_process").exec

var test = require("tap").test
var rimraf = require("rimraf")
var mkdirp = require("mkdirp")

var pkg = join(__dirname, "install-scoped")
var work = join(__dirname, "install-scoped-TEST")
var modules = join(work, "node_modules")

var EXEC_OPTS = {}

test("setup", function (t) {
  mkdirp.sync(modules)
  process.chdir(work)

  t.end()
})

test("installing package with links", function (t) {
  common.npm(["install", pkg], EXEC_OPTS, function (err, code) {
    t.ifError(err, "install ran to completion without error")
    t.notOk(code, "npm install exited with code 0")

    t.ok(
      existsSync(join(modules, "@scoped", "package", "package.json")),
      "package installed"
    )
    t.ok(existsSync(join(modules, ".bin")), "binary link directory exists")

    var hello = join(modules, ".bin", "hello")
    t.ok(existsSync(hello), "binary link exists")

    exec("node " + hello, function (err, stdout, stderr) {
      t.ifError(err, "command ran fine")
      t.notOk(stderr, "got no error output back")
      t.equal(stdout, "hello blrbld\n", "output was as expected")

      t.end()
    })
  })
})

test("cleanup", function (t) {
  process.chdir(__dirname)
  rimraf.sync(work)
  t.end()
})
