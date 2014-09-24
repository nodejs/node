var test = require("tap").test

var some = require("../some.js")

var NOP = function () {}

test("some() called with bogus parameters", function (t) {
  t.throws(function () {
    some()
  }, "throws when called with no parameters")

  t.throws(function () {
    some(null, NOP, NOP)
  }, "throws when called with no list")

  t.throws(function () {
    some([], null, NOP)
  }, "throws when called with no predicate")

  t.throws(function () {
    some([], NOP, null)
  }, "throws when called with no callback")

  t.throws(function () {
    some({}, NOP, NOP)
  }, "throws when called with wrong list type")

  t.throws(function () {
    some([], "ham", NOP)
  }, "throws when called with wrong test type")

  t.throws(function () {
    some([], NOP, "ham")
  }, "throws when called with wrong test type")

  t.end()
})
