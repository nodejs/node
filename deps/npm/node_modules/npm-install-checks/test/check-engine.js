var test = require("tap").test
var c = require("../index.js").checkEngine

test("no engine defined", function (t) {
  c({ engines: {}}, "1.1.2", "0.2.1", false, true, function (err) {
    t.notOk(err, "no error present")
    t.end()
  })
})

test("node version too old", function (t) {
  var target = { engines: { node: "0.10.24" }}
  c(target, "1.1.2", "0.10.18", false, true, function (err) {
    t.ok(err, "returns an error")
    t.equals(err.required.node, "0.10.24")
    t.end()
  })
})

test("npm version too old", function (t) {
  var target = { engines: { npm: "^1.4.6" }}
    c(target, "1.3.2", "0.2.1", false, true, function (err) {
      t.ok(err, "returns an error")
      t.equals(err.required.npm, "^1.4.6")
      t.end()
    })
})

test("strict=false w/engineStrict json does not return an error", function (t) {
  var target = { engines: { npm: "1.3.6" }, engineStrict: true }
  c(target, "1.4.2", "0.2.1", false, false, function (err) {
    t.notOk(err, "returns no error")
    t.end()
  })
})
