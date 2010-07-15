common = require("../common");
assert = common.assert

setTimeout(function () {
  a = require("../fixtures/a");
}, 50);

process.addListener("exit", function () {
  assert.equal(true, "A" in a);
  assert.equal("A", a.A());
  assert.equal("D", a.D());
});
