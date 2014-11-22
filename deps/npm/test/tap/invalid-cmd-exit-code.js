var test = require("tap").test
var common = require("../common-tap.js")

var opts = { cwd: process.cwd() }

test("npm asdf should return exit code 1", function (t) {
  common.npm(["asdf"], opts, function (er, c) {
    if (er) throw er
    t.ok(c, "exit code should not be zero")
    t.end()
  })
})

test("npm help should return exit code 0", function (t) {
  common.npm(["help"], opts, function (er, c) {
    if (er) throw er
    t.equal(c, 0, "exit code should be 0")
    t.end()
  })
})

test("npm help fadf should return exit code 0", function (t) {
  common.npm(["help", "fadf"], opts, function (er, c) {
    if (er) throw er
    t.equal(c, 0, "exit code should be 0")
    t.end()
  })
})
