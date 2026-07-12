// META: global=window,dedicatedworker

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
