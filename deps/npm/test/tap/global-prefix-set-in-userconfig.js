var common = require("../common-tap.js")
var test = require("tap").test
var rimraf = require("rimraf")
var prefix = __filename.replace(/\.js$/, "")
var rcfile = __filename.replace(/\.js$/, ".npmrc")
var fs = require("fs")
var conf = "prefix = " + prefix + "\n"

test("setup", function (t) {
  rimraf.sync(prefix)
  fs.writeFileSync(rcfile, conf)
  t.pass("ready")
  t.end()
})

test("run command", function (t) {
  var args = ["prefix", "-g", "--userconfig=" + rcfile]
  common.npm(args, {env: {}}, function (er, code, so, se) {
    if (er) throw er
    t.equal(code, 0)
    t.equal(so.trim(), prefix)
    t.end()
  })
})

test("made dir", function (t) {
  t.ok(fs.statSync(prefix).isDirectory())
  t.end()
})

test("cleanup", function (t) {
  rimraf.sync(prefix)
  rimraf.sync(rcfile)
  t.pass("clean")
  t.end()
})
