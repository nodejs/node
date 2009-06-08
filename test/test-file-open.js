include("mjsunit.js");
var assert_count = 0;

function onLoad () {
  var dirname = node.path.dirname(__filename);
  var fixtures = node.path.join(dirname, "fixtures");
  var x = node.path.join(fixtures, "x.txt");

  file = new node.fs.File;
  file.onError = function (method, errno, msg) {
    assertTrue(false); 
  };

  file.open(x, "r", function () {
    assert_count += 1;
    file.close();
  });
};
