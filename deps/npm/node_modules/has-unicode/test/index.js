"use strict"
var test = require("tap").test
var requireInject = require("require-inject")

test("Windows", function (t) {
  t.plan(1)
  var hasUnicode = requireInject("../index.js", {
    os: { type: function () { return "Windows_NT" } }
  })
  t.is(hasUnicode(), false, "Windows is assumed NOT to be unicode aware")
})
test("Unix Env", function (t) {
  t.plan(3)
  var hasUnicode = requireInject("../index.js", {
    os: { type: function () { return "Linux" } },
    child_process: { exec: function (cmd,cb) { cb(new Error("not available")) } }
  })
  process.env.LANG = "en_US.UTF-8"
  process.env.LC_ALL = null
  t.is(hasUnicode(), true, "Linux with a UTF8 language")
  process.env.LANG = null
  process.env.LC_ALL = "en_US.UTF-8"
  t.is(hasUnicode(), true, "Linux with UTF8 locale")
  process.env.LC_ALL = null
  t.is(hasUnicode(), false, "Linux without UTF8 language or locale")
})
