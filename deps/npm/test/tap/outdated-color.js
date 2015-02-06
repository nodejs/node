var common = require("../common-tap.js")
var test = require("tap").test
var mkdirp = require("mkdirp")
var rimraf = require("rimraf")
var mr = require("npm-registry-mock")
var path = require("path")

var pkg = path.resolve(__dirname, "outdated")
var cache = path.resolve(pkg, "cache")
mkdirp.sync(cache)

var EXEC_OPTS = {
  cwd: pkg
}

function hasControlCodes(str) {
  return str.length !== ansiTrim(str).length
}

function ansiTrim (str) {
  var r = new RegExp("\x1b(?:\\[(?:\\d+[ABCDEFGJKSTm]|\\d+;\\d+[Hfm]|" +
        "\\d+;\\d+;\\d+m|6n|s|u|\\?25[lh])|\\w)", "g")
  return str.replace(r, "")
}

// note hard to automate tests for color = true
// as npm kills the color config when it detects
// it"s not running in a tty
test("does not use ansi styling", function (t) {
  t.plan(4)
  mr({port : common.port}, function (er, s) { // create mock registry.
    common.npm(
    [
      "outdated",
      "--registry", common.registry,
      "underscore"
    ],
    EXEC_OPTS,
    function (err, code, stdout) {
      t.ifError(err)
      t.notOk(code, "npm outdated exited with code 0")
      t.ok(stdout, stdout.length)
      t.ok(!hasControlCodes(stdout))
      s.close()
    })
  })
})

test("cleanup", function (t) {
  rimraf.sync(cache)
  t.end()
})
