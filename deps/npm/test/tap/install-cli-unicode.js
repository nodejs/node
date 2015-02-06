var common = require("../common-tap.js")
var test = require("tap").test
var mr = require("npm-registry-mock")
var path = require("path")

var pkg = path.resolve(__dirname, "install-cli")

function hasOnlyAscii (s) {
  return /^[\000-\177]*$/.test(s)
}

var EXEC_OPTS = {
  cwd : pkg
}

test("does not use unicode with --unicode false", function (t) {
  t.plan(5)
  mr({port : common.port}, function (er, s) {
    common.npm(["install", "--unicode", "false", "read"], EXEC_OPTS, function (err, code, stdout) {
      t.ifError(err, "install package read without unicode success")
      t.notOk(code, "npm install exited with code 0")
      t.ifError(err)
      t.ok(stdout, stdout.length)
      t.ok(hasOnlyAscii(stdout))
      s.close()
    })
  })
})

test("cleanup", function (t) {
  mr({port : common.port}, function (er, s) {
    common.npm(["uninstall", "read"], EXEC_OPTS, function (err, code) {
      t.ifError(err, "uninstall read package success")
      t.notOk(code, "npm uninstall exited with code 0")
      s.close()
    })
  })
  t.end()
})
