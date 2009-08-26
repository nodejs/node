include("mjsunit.js");

var exit_status = -1;

var cat = node.createProcess("cat");

cat.addListener("output", function (chunk) { assertEquals(null, chunk); });
cat.addListener("error", function (chunk) { assertEquals(null, chunk); });
cat.addListener("exit", function (status) { exit_status = status; });

cat.kill();

function onExit () {
  assertTrue(exit_status > 0);
}
