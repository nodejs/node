process.mixin(require("./common"));

setTimeout(function () {
  a = require("./fixtures/a");
}, 50);

process.addListener("exit", function () {
  assertTrue("A" in a);
  assertEquals("A", a.A());
  assertEquals("D", a.D());
});
