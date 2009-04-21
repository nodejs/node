include("mjsunit");
var a = require("fixtures/a");

function on_load () {
  stderr.puts("hello world");
  assertFalse(false, "testing the test program.");

  assertInstanceof(a.A, Function);
  assertEquals("A", a.A());

  assertInstanceof(a.C, Function);
  assertEquals("C", a.C());
}
