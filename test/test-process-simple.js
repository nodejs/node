include("mjsunit.js");

var cat = new node.Process("cat");

var response = "";
var exit_status = -1;

cat.onOutput = function (chunk) {
  if (chunk) response += chunk;
  if (response === "hello world") cat.close();
};
cat.onError = function (chunk) {
  assertEquals(null, chunk);
};
cat.onExit = function (status) { exit_status = status; };

function onLoad () {
  cat.write("hello");
  cat.write(" ");
  cat.write("world");
}

function onExit () {
  assertEquals(0, exit_status);
  assertEquals("hello world", response);
}
