common = require("../common");
assert = common.assert

foo0 = "foo0";
global.bar0 = "bar0";

var module = require("../fixtures/global/sub1"),
  keys = module.subGlobalKeys();

var fooBarKeys = keys.filter(
  function (x) { return x.match(/^foo/) || x.match(/^bar/); }
);
fooBarKeys.sort();
assert.equal("bar0,bar1,bar2,foo0,foo1,foo2", fooBarKeys.join(), "global keys not as expected: "+JSON.stringify(keys));

var fooBars = module.subAllFooBars();

assert.equal("foo0", fooBars.foo0, "x from base level not visible in deeper levels.");
assert.equal("bar0", fooBars.bar0, "global.x from base level not visible in deeper levels.");
assert.equal("foo1", fooBars.foo1, "x from medium level not visible in deeper levels.");
assert.equal("bar1", fooBars.bar1, "global.x from medium level not visible in deeper levels.");
