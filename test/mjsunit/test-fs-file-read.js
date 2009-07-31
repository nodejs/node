include("mjsunit.js");

var dirname = node.path.dirname(__filename);
var fixtures = node.path.join(dirname, "fixtures");
var x = node.path.join(fixtures, "x.txt");

var contents = "";

var f = new node.fs.File({ encoding: "utf8" });
f.open(x, "r+");
f.read(10000).addCallback(function (chunk) {
  contents = chunk;
  puts("got chunk: " + JSON.stringify(chunk));
});
f.close();


function onExit () {
  assertEquals("xyz\n", contents);
}
