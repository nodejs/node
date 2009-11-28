process.mixin(require("./common"));

debug("load test-module-loading.js");

var a = require("./fixtures/a");
var d = require("./fixtures/b/d");
var d2 = require("./fixtures/b/d");
// Absolute
var d3 = require(require('path').dirname(__filename)+"/fixtures/b/d");
// Relative
var d4 = require("../mjsunit/fixtures/b/d");

assert.equal(false, false, "testing the test program.");

assert.equal(true, a.A instanceof Function);
assert.equal("A", a.A());

assert.equal(true, a.C instanceof Function);
assert.equal("C", a.C());

assert.equal(true, a.D instanceof Function);
assert.equal("D", a.D());

assert.equal(true, d.D instanceof Function);
assert.equal("D", d.D());

assert.equal(true, d2.D instanceof Function);
assert.equal("D", d2.D());

assert.equal(true, d3.D instanceof Function);
assert.equal("D", d3.D());

assert.equal(true, d4.D instanceof Function);
assert.equal("D", d4.D());

process.addListener("exit", function () {
  assert.equal(true, a.A instanceof Function);
  assert.equal("A done", a.A());

  assert.equal(true, a.C instanceof Function);
  assert.equal("C done", a.C());

  assert.equal(true, a.D instanceof Function);
  assert.equal("D done", a.D());

  assert.equal(true, d.D instanceof Function);
  assert.equal("D done", d.D());

  assert.equal(true, d2.D instanceof Function);
  assert.equal("D done", d2.D());

  puts("exit");
});
