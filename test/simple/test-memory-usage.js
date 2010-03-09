require("../common");

var r = process.memoryUsage();
puts(inspect(r));
assert.equal(true, r["rss"] > 0);
assert.equal(true, r["vsize"] > 0);
