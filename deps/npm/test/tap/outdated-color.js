var common = require("../common-tap.js")
var test = require("tap").test
var npm = require("../../")
var mkdirp = require("mkdirp")
var rimraf = require("rimraf")
var mr = require("npm-registry-mock")
var exec = require('child_process').exec
var mr = require("npm-registry-mock")

var pkg = __dirname + '/outdated'
var NPM_BIN = __dirname + '/../../bin/npm-cli.js'
mkdirp.sync(pkg + "/cache")

function hasControlCodes(str) {
  return str.length !== ansiTrim(str).length
}

function ansiTrim (str) {
  var r = new RegExp("\x1b(?:\\[(?:\\d+[ABCDEFGJKSTm]|\\d+;\\d+[Hfm]|" +
        "\\d+;\\d+;\\d+m|6n|s|u|\\?25[lh])|\\w)", "g");
  return str.replace(r, "")
}

// note hard to automate tests for color = true
// as npm kills the color config when it detects
// it's not running in a tty
test("does not use ansi styling", function (t) {
  t.plan(3)
  mr(common.port, function (s) { // create mock registry.
    exec('node ' + NPM_BIN + ' outdated --registry ' + common.registry + ' --color false underscore', {
      cwd: pkg
    }, function(err, stdout) {
      t.ifError(err)
      t.ok(stdout, stdout.length)
      t.ok(!hasControlCodes(stdout))
      s.close()
    })
  })
})

test("cleanup", function (t) {
  rimraf.sync(pkg + "/cache")
  t.end()
})
