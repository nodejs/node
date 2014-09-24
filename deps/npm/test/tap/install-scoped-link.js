var exec = require("child_process").exec
var existsSync = require("fs").existsSync
var join = require("path").join
// var resolve = require("path").resolve

var test = require("tap").test
var rimraf = require("rimraf")
var mkdirp = require("mkdirp")

var npm = require("../../")

var pkg = join(__dirname, "install-scoped")
var work = join(__dirname, "install-scoped-TEST")
var modules = join(work, "node_modules")

test("setup", function (t) {
  mkdirp.sync(modules)
  process.chdir(work)

  t.end()
})

test("installing package with links", function (t) {
  npm.load(function() {
    npm.commands.install([pkg], function (err) {
      t.ifError(err, "install ran to completion without error")

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
})

test("cleanup", function(t) {
  process.chdir(__dirname)
  rimraf.sync(work)
  t.end()
})
