include("mjsunit.js");

var exit_status = -1;

function onLoad () {
  var cat = new node.Process("cat");

  cat.onOutput = function (chunk) { assertEquals(null, chunk); };
  cat.onError = function (chunk) { assertEquals(null, chunk); };
  cat.onExit = function (status) { exit_status = status; };

  cat.kill();
}

function onExit () {
  assertTrue(exit_status > 0);
}
