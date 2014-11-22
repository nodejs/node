var common = require("../common-tap.js")
var test = require("tap").test
var path = require("path")
var fs = require("fs")
var rimraf = require("rimraf")
var mkdirp = require("mkdirp")
var pkg = path.join(__dirname, "install-at-locally")

var EXEC_OPTS = { }

test("setup", function (t) {
  mkdirp.sync(pkg)
  mkdirp.sync(path.resolve(pkg, "node_modules"))
  process.chdir(pkg)
  t.end()
})

test("\"npm install ./package@1.2.3\" should install local pkg", function(t) {
  common.npm(["install", "./package@1.2.3"], EXEC_OPTS, function(err, code) {
    var p = path.resolve(pkg, "node_modules/install-at-locally/package.json")
    t.ifError(err, "install local package successful")
    t.equal(code, 0, "npm install exited with code")
    t.ok(JSON.parse(fs.readFileSync(p, "utf8")))
    t.end()
  })
})

test("\"npm install install/at/locally@./package@1.2.3\" should install local pkg", function(t) {
  common.npm(["install", "./package@1.2.3"], EXEC_OPTS, function(err, code) {
    var p = path.resolve(pkg, "node_modules/install-at-locally/package.json")
    t.ifError(err, "install local package in explicit directory successful")
    t.equal(code, 0, "npm install exited with code")
    t.ok(JSON.parse(fs.readFileSync(p, "utf8")))
    t.end()
  })
})

test("cleanup", function(t) {
  process.chdir(__dirname)
  rimraf.sync(path.resolve(pkg, "node_modules"))
  t.end()
})
