var test = require("tap").test

var vacuum = require("../vacuum.js")

test("vacuum throws on missing parameters", function (t) {
  t.throws(vacuum, "called with no parameters")
  t.throws(function () { vacuum("directory", {}) }, "called with no callback")

  t.end()
})

test('vacuum throws on incorrect types ("Forrest is pedantic" section)', function (t) {
  t.throws(function () {
    vacuum({}, {}, function () {})
  }, "called with path parameter of incorrect type")
  t.throws(function () {
    vacuum("directory", "directory", function () {})
  }, "called with options of wrong type")
  t.throws(function () {
    vacuum("directory", {}, "whoops")
  }, "called with callback that isn't callable")

  t.end()
})
