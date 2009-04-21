include("mjsunit");
var a = require("fixtures/a");

function onLoad () {
  assertFalse(false, "testing the test program.");

  assertInstanceof(a.A, Function);
  assertEquals("A", a.A());

  assertInstanceof(a.C, Function);
  assertEquals("C", a.C());
}
