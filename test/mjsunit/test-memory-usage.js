process.mixin(require("./common"));

var r = memoryUsage();
puts(inspect(r));
assertTrue(r["rss"] > 0);
assertTrue(r["vsize"] > 0);
