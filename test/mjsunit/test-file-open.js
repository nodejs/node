include("mjsunit.js");

var got_error = false;
var opened = false;
var closed = false;

function onLoad () {
  var dirname = node.path.dirname(__filename);
  var fixtures = node.path.join(dirname, "fixtures");
  var x = node.path.join(fixtures, "x.txt");

  file = new node.fs.File;
  file.onError = function () { got_error = true };

  file.open(x, "r", function () {
    opened = true
    file.close(function () {
      closed = true;
    });
  });
}

function onExit () {
  assertFalse(got_error);
  assertTrue(opened);
  assertTrue(closed);
}
