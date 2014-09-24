var test = require("tap").test
var rimraf = require("rimraf")
var common = require("../common-tap.js")
var path = require("path")
var fs = require("fs")

var pkg = path.resolve(__dirname, "optional-metadep-rollback-collision")
var nm = path.resolve(pkg, "node_modules")
var cache = path.resolve(pkg, "cache")
var pidfile = path.resolve(pkg, "child.pid")

test("setup", function (t) {
  cleanup()
  t.end()
})

test("go go test racer", function (t) {
  common.npm(["install", "--prefix=" + pkg, "--fetch-retries=0", "--cache=" + cache], {
    cwd: pkg,
    env: {
      PATH: process.env.PATH,
      Path: process.env.Path,
      "npm_config_loglevel": "silent"
    },
    stdio: [ 0, "pipe", 2 ]
  }, function (er, code, sout) {
    if (er) throw er
    t.equal(code, 0)
    t.equal(sout, "ok\nok\n")
    t.notOk(/not ok/.test(sout), "should not contain the string 'not ok'")
    t.end()
  })
})

test("verify results", function (t) {
  t.throws(function () {
    fs.statSync(nm)
  })
  t.end()
})

test("cleanup", function (t) {
  cleanup()
  t.end()
})

function cleanup () {
  try {
    var pid = +fs.readFileSync(pidfile)
    process.kill(pid, "SIGKILL")
  } catch (er) {}

  rimraf.sync(cache)
  rimraf.sync(nm)
  rimraf.sync(pidfile)
}
