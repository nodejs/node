process.mixin(require("./common"));

var r = process.memoryUsage();
puts(inspect(r));
assertTrue(r["rss"] > 0);
assertTrue(r["vsize"] > 0);
