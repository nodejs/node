var tap = require("tap")
var normalize = require("../lib/normalize")

var warningMessages = require("../lib/warning_messages.json")
var safeFormat = require("../lib/safe_format")

tap.test("warn if dependency contains anything else but a string", function(t) {
  var a
  var warnings = []
  function warn(w) {
    warnings.push(w)
  }
  normalize(a={
    dependencies: { "a": 123},
    devDependencies: { "b": 456},
    optionalDependencies: { "c": 789}
  }, warn)
  
  var wanted1 = safeFormat(warningMessages.nonStringDependency, "a", 123)
  var wanted2 = safeFormat(warningMessages.nonStringDependency, "b", 456)
  var wanted3 = safeFormat(warningMessages.nonStringDependency, "c", 789)
  t.ok(~warnings.indexOf(wanted1), wanted1)
  t.ok(~warnings.indexOf(wanted2), wanted2)
  t.ok(~warnings.indexOf(wanted3), wanted3)
  t.end()
})

tap.test("warn if bundleDependencies array contains anything else but strings", function(t) {
  var a
  var warnings = []
  function warn(w) {
    warnings.push(w)
  }
  normalize(a={
    bundleDependencies: ["abc", 123, {foo:"bar"}]
  }, warn)
  
  var wanted1 = safeFormat(warningMessages.nonStringBundleDependency, 123)
  var wanted2 = safeFormat(warningMessages.nonStringBundleDependency, {foo:"bar"})
  t.ok(~warnings.indexOf(wanted1), wanted1)
  t.ok(~warnings.indexOf(wanted2), wanted2)
  t.end()
})