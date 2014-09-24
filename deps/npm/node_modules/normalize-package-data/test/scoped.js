var test = require("tap").test

var fixNameField = require("../lib/fixer.js").fixNameField

test("a simple scoped module has a valid name", function (t) {
  var data = {name : "@org/package"}
  fixNameField(data, false)
  t.equal(data.name, "@org/package", "name was unchanged")

  t.end()
})

test("'org@package' is not a valid name", function (t) {
  t.throws(function () {
    fixNameField({name : "org@package"}, false)
  }, "blows up as expected")

  t.end()
})

test("'org=package' is not a valid name", function (t) {
  t.throws(function () {
    fixNameField({name : "org=package"}, false)
  }, "blows up as expected")

  t.end()
})

test("'@org=sub/package' is not a valid name", function (t) {
  t.throws(function () {
    fixNameField({name : "@org=sub/package"}, false)
  }, "blows up as expected")

  t.end()
})

test("'@org/' is not a valid name", function (t) {
  t.throws(function () {
    fixNameField({name : "@org/"}, false)
  }, "blows up as expected")

  t.end()
})

test("'@/package' is not a valid name", function (t) {
  t.throws(function () {
    fixNameField({name : "@/package"}, false)
  }, "blows up as expected")

  t.end()
})
