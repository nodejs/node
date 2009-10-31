process.mixin(require("./common"));

var exit_status = -1;

var cat = process.createChildProcess("cat");

cat.addListener("output", function (chunk) { assertEquals(null, chunk); });
cat.addListener("error", function (chunk) { assertEquals(null, chunk); });
cat.addListener("exit", function (status) { exit_status = status; });

cat.kill();

process.addListener("exit", function () {
  assertTrue(exit_status > 0);
});
