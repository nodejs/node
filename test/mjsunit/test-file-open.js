include("mjsunit.js");

var got_error = false;
var opened = false;
var closed = false;

function onLoad () {
  var dirname = node.path.dirname(__filename);
  var fixtures = node.path.join(dirname, "fixtures");
  var x = node.path.join(fixtures, "x.txt");

  file = new node.fs.File;
  file.addListener("error", function () { got_error = true });

  file.open(x, "r").addCallback(function () {
    opened = true
    file.close().addCallback(function () {
      closed = true;
    });
  });

  puts("hey!");
}

function onExit () {
  assertFalse(got_error);
  assertTrue(opened);
  assertTrue(closed);
}
