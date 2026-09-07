importScripts("/resources/testharness.js");

test(function() {
  var rv = postMessage(1);
  assert_equals(rv, undefined);
}, "return value of postMessage");

done();
