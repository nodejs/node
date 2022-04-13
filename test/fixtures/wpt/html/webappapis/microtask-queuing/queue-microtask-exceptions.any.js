// META: global=window,worker
"use strict";

setup({
  allow_uncaught_exception: true
});

async_test(t => {
  const error = new Error("boo");
  self.addEventListener("error", t.step_func_done(ev => {
    assert_equals(ev.error, error);
  }));

  queueMicrotask(() => { throw error; });
}, "It rethrows exceptions");
