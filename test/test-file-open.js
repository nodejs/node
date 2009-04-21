include("mjsunit");
var assert_count = 0;

function onload () {
  var fixtures = node.path.join(script.dirname, "fixtures");
  var x = node.path.join(fixtures, "x.txt");

  file = new File;
  file.open(x, "r", function (status) {
    assertTrue(status == 0);
    assert_count += 1;
  });
};
