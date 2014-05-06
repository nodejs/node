var common = require("../common-tap.js")
var test = require("tap").test
var path = require("path")
var cwd = path.resolve(__dirname, "..", "..")
var testscript = require("../../package.json").scripts.test
var tsregexp = testscript.replace(/([\[\.\*\]])/g, "\\$1")

test("default", function (t) {
  common.npm(["run"], { cwd: cwd }, function (er, code, so, se) {
    if (er) throw er
    t.notOk(code)
    t.similar(so, new RegExp("\\n  test\\n    " + tsregexp + "\\n"))
    t.end()
  })
})

test("parseable", function (t) {
  common.npm(["run", "-p"], { cwd: cwd }, function (er, code, so, se) {
    if (er) throw er
    t.notOk(code)
    t.similar(so, new RegExp("\\ntest:" + tsregexp + "\\n"))
    t.end()
  })
})

test("parseable", function (t) {
  common.npm(["run", "--json"], { cwd: cwd }, function (er, code, so, se) {
    if (er) throw er
    t.notOk(code)
    t.equal(JSON.parse(so).test, testscript)
    t.end()
  })
})
