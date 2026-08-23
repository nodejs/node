importScripts('/resources/testharness.js');
importScripts("script.js");
test(() => {
  assert_equals(result, "gamma/script.js");
});
done();
