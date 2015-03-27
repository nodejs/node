global.FAKE_WINDOWS = true

var npa = require("../npa.js")
var test = require("tap").test
var path = require("path")

var cases = {
  "C:\\x\\y\\z": {
    raw: "C:\\x\\y\\z",
    scope: null,
    name: null,
    rawSpec: "C:\\x\\y\\z",
    spec: path.resolve("C:\\x\\y\\z"),
    type: "local"
  },
  "foo@C:\\x\\y\\z": {
    raw: "foo@C:\\x\\y\\z",
    scope: null,
    name: "foo",
    rawSpec: "C:\\x\\y\\z",
    spec: path.resolve("C:\\x\\y\\z"),
    type: "local"
  },
  "foo@/foo/bar/baz": {
    raw: "foo@/foo/bar/baz",
    scope: null,
    name: "foo",
    rawSpec: "/foo/bar/baz",
    spec: path.resolve("/foo/bar/baz"),
    type: "local"
  }
}

test("parse a windows path", function (t) {
  Object.keys(cases).forEach(function (c) {
    var expect = cases[c]
    var actual = npa(c)
    t.same(actual, expect, c)
  })
  t.end()
})
