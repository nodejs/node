// META: title=EventTarget.addEventListener

// Step 1.
test(function() {
  const et = new EventTarget();
  assert_equals(et.addEventListener("x", null, false), undefined);
  assert_equals(et.addEventListener("x", null, true), undefined);
  assert_equals(et.addEventListener("x", null), undefined);
}, "Adding a null event listener should succeed");
