include("mjsunit.js");
var status = null;

function onLoad () {
  var dirname = node.path.dirname(__filename);
  var fixtures = node.path.join(dirname, "fixtures");
  var filename = node.path.join(fixtures, "does_not_exist.txt");
  node.fs.cat(filename, "raw", function (s) { status = s });
}

function onExit () {
  assertTrue(status != 0);
}
