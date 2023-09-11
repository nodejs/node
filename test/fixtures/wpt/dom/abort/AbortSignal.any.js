test(t => {
  const signal = AbortSignal.abort();
  assert_true(signal instanceof AbortSignal, "returned object is an AbortSignal");
  assert_true(signal.aborted, "returned signal is already aborted");
}, "the AbortSignal.abort() static returns an already aborted signal");

async_test(t => {
  const s = AbortSignal.abort();
  s.addEventListener("abort", t.unreached_func("abort event listener called"));
  s.onabort = t.unreached_func("abort event handler called");
  t.step_timeout(() => { t.done(); }, 2000);
}, "signal returned by AbortSignal.abort() should not fire abort event");

test(t => {
  const signal = AbortSignal.timeout(0);
  assert_true(signal instanceof AbortSignal, "returned object is an AbortSignal");
  assert_false(signal.aborted, "returned signal is not already aborted");
}, "AbortSignal.timeout() returns a non-aborted signal");

async_test(t => {
  const signal = AbortSignal.timeout(5);
  signal.onabort = t.step_func_done(() => {
    assert_true(signal.aborted, "signal is aborted");
    assert_true(signal.reason instanceof DOMException, "signal.reason is a DOMException");
    assert_equals(signal.reason.name, "TimeoutError", "signal.reason is a TimeoutError");
  });
}, "Signal returned by AbortSignal.timeout() times out");

async_test(t => {
  let result = "";
  for (const value of ["1", "2", "3"]) {
    const signal = AbortSignal.timeout(5);
    signal.onabort = t.step_func(() => { result += value; });
  }

  const signal = AbortSignal.timeout(5);
  signal.onabort = t.step_func_done(() => {
    assert_equals(result, "123", "Timeout order should be 123");
  });
}, "AbortSignal timeouts fire in order");
