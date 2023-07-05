function timeout_trampoline(t, timeout, message) {
  t.step_timeout(function() {
    // Yield in case we managed to be called before the second interval callback.
    t.step_timeout(function() {
      assert_unreached(message);
    }, timeout);
  }, timeout);
}

async_test(function(t) {
  let ctr = 0;
  let h = setInterval(t.step_func(function() {
    if (++ctr == 2) {
      clearInterval(h);
      t.done();
      return;
    }
  }) /* no interval */);

  timeout_trampoline(t, 100, "Expected setInterval callback to be called two times");
}, "Calling setInterval with no interval should be the same as if called with 0 interval");

async_test(function(t) {
  let ctr = 0;
  let h = setInterval(t.step_func(function() {
    if (++ctr == 2) {
      clearInterval(h);
      t.done();
      return;
    }
  }),  undefined);

  timeout_trampoline(t, 100, "Expected setInterval callback to be called two times");
}, "Calling setInterval with undefined interval should be the same as if called with 0 interval");
