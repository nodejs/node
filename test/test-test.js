puts(__filename);
include("mjsunit");
puts(__filename);

function on_load () {
  assertFalse(false, "testing the test program.");
  puts("i think everything is okay.");
  //mjsunit.assertEquals("test-test.js", __file__);
}
