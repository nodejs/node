include("mjsunit.js");

var exit_status = -1;

function onLoad () {
  var cat = new node.Process("cat");

  cat.addListener("Output", function (chunk) { assertEquals(null, chunk); });
  cat.addListener("Error", function (chunk) { assertEquals(null, chunk); });
  cat.addListener("Exit", function (status) { exit_status = status; });

  cat.kill();
}

function onExit () {
  assertTrue(exit_status > 0);
}
