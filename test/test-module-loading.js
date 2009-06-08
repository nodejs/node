include("mjsunit.js");
var a = require("fixtures/a.js");
var d = require("fixtures/b/d.js");
var d2 = require("fixtures/b/d.js");

function onLoad () {
  assertFalse(false, "testing the test program.");

  assertInstanceof(a.A, Function);
  assertEquals("A", a.A());

  assertInstanceof(a.C, Function);
  assertEquals("C", a.C());

  assertInstanceof(a.D, Function);
  assertEquals("D", a.D());

  assertInstanceof(d.D, Function);
  assertEquals("D", d.D());

  assertInstanceof(d2.D, Function);
  assertEquals("D", d2.D());
}
