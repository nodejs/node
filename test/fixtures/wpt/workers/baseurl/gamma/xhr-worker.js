importScripts('/resources/testharness.js');
test(t => {
  var x = new XMLHttpRequest();
  x.open("GET", "test.txt", false);
  x.send();
  assert_equals(x.response, "gamma\n");
});
done();
