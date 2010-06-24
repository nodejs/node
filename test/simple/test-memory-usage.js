require("../common");

var r = process.memoryUsage();
console.log(inspect(r));
assert.equal(true, r["rss"] > 0);
assert.equal(true, r["vsize"] > 0);
