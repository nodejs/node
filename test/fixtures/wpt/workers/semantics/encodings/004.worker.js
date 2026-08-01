importScripts("/resources/testharness.js");
test(function() {
  assert_equals("ÿ", "\ufffd");
}, "Decoding invalid utf-8");
done();
