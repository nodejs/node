include("mjsunit");
var a = require("fixtures/a");

function on_load () {
  assertFalse(false, "testing the test program.");

  assertInstanceof(a.A, Function);
  assertEquals("A", a.A());

  assertInstanceof(a.C, Function);
  assertEquals("C", a.C());
}
