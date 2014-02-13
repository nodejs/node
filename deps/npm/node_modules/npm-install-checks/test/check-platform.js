var test = require("tap").test
var c = require("../index.js").checkPlatform

test("target cpu wrong", function (t) {
  var target = {}
  target.cpu = "enten-cpu"
  target.os = "any"
  c(target, false, function (err) {
    t.ok(err, "error present")
    t.equal(err.code, "EBADPLATFORM")
    t.end()
  })
})

test("os wrong", function (t) {
  var target = {}
  target.cpu = "any"
  target.os = "enten-os"
  c(target, false, function (err) {
    t.ok(err, "error present")
    t.equal(err.code, "EBADPLATFORM")
    t.end()
  })
})

test("nothing wrong", function (t) {
  var target = {}
  target.cpu = "any"
  target.os = "any"
  c(target, false, function (err) {
    t.notOk(err, "no error present")
    t.end()
  })
})

test("force", function (t) {
  var target = {}
  target.cpu = "enten-cpu"
  target.os = "any"
  c(target, true, function (err) {
    t.notOk(err, "no error present")
    t.end()
  })
})
