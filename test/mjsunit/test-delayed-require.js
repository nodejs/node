process.mixin(require("common.js"));

setTimeout(function () {
  a = require("fixtures/a.js");
}, 50);

process.addListener("exit", function () {
  assertTrue("A" in a);
  assertEquals("A", a.A());
  assertEquals("D", a.D());
});
