include("mjsunit.js");

var cat = new node.Process("cat");

var response = "";
var exit_status = -1;

cat.addListener("Output", function (chunk) {
  if (chunk) {
    response += chunk;
    if (response === "hello world") cat.close();
  }
});
cat.addListener("Error", function (chunk) {
  assertEquals(null, chunk);
});
cat.addListener("Exit", function (status) { exit_status = status; });

function onLoad () {
  cat.write("hello");
  cat.write(" ");
  cat.write("world");
}

function onExit () {
  assertEquals(0, exit_status);
  assertEquals("hello world", response);
}
