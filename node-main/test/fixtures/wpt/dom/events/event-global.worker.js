importScripts("/resources/testharness.js");
test(t => {
  let seen = false;
  const event = new Event("hi");
  assert_equals(self.event, undefined);
  self.addEventListener("hi", t.step_func(e => {
    seen = true;
    assert_equals(self.event, undefined);
    assert_equals(e, event);
  }));
  self.dispatchEvent(event);
  assert_true(seen);
}, "There's no self.event (that's why we call it window.event) in workers");
done();
