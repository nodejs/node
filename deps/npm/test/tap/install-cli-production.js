var common = require("../common-tap.js")
var test = require("tap").test
var path = require("path")
var fs = require("fs")
var rimraf = require("rimraf")
var mkdirp = require("mkdirp")
var pkg = path.join(__dirname, "install-cli-production")

var EXEC_OPTS = {
  cwd: pkg
}

test("setup", function(t) {
  mkdirp.sync(pkg)
  mkdirp.sync(path.resolve(pkg, "node_modules"))
  process.chdir(pkg)
  t.end()
})

test("\"npm install --production\" should install dependencies", function(t) {
  common.npm(["install", "--production"], EXEC_OPTS, function(err, code) {
    t.ifError(err, "install production successful")
    t.equal(code, 0, "npm install exited with code")
    var p = path.resolve(pkg, "node_modules/dependency/package.json")
    t.ok(JSON.parse(fs.readFileSync(p, "utf8")))
    t.end()
  })
})

test("\"npm install --production\" should not install dev dependencies", function(t) {
  common.npm(["install", "--production"], EXEC_OPTS, function(err, code) {
    t.ifError(err, "install production successful")
    t.equal(code, 0, "npm install exited with code")
    var p = path.resolve(pkg, "node_modules/dev-dependency/package.json")
    t.ok(!fs.existsSync(p), "")
    t.end()
  })
})

test("cleanup", function(t) {
  process.chdir(__dirname)
  rimraf.sync(path.resolve(pkg, "node_modules"))
  t.end()
})
