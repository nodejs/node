// META: global=window,dedicatedworker,shadowrealm

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
