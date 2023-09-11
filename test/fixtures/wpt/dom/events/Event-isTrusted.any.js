test(function() {
  var desc1 = Object.getOwnPropertyDescriptor(new Event("x"), "isTrusted");
  assert_not_equals(desc1, undefined);
  assert_equals(typeof desc1.get, "function");

  var desc2 = Object.getOwnPropertyDescriptor(new Event("x"), "isTrusted");
  assert_not_equals(desc2, undefined);
  assert_equals(typeof desc2.get, "function");

  assert_equals(desc1.get, desc2.get);
});
