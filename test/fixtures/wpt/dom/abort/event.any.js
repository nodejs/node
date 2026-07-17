// META: global=window,dedicatedworker,shadowrealm

test(t => {
  const c = new AbortController(),
        s = c.signal;
  let state = "begin";

  assert_false(s.aborted);
  assert_true("reason" in s, "signal has reason property");
  assert_equals(s.reason, undefined, "signal.reason is initially undefined");

  s.addEventListener("abort",
    t.step_func(e => {
      assert_equals(state, "begin");
      state = "aborted";
    })
  );
  c.abort();

  assert_equals(state, "aborted");
  assert_true(s.aborted);
  assert_true(s.reason instanceof DOMException, "signal.reason is DOMException");
  assert_equals(s.reason.name, "AbortError", "signal.reason is AbortError");

  c.abort();
}, "AbortController abort() should fire event synchronously");

test(t => {
  const controller = new AbortController();
  const signal = controller.signal;
  assert_equals(controller.signal, signal,
                "value of controller.signal should not have changed");
  controller.abort();
  assert_equals(controller.signal, signal,
                "value of controller.signal should still not have changed");
}, "controller.signal should always return the same object");

test(t => {
  const controller = new AbortController();
  const signal = controller.signal;
  let eventCount = 0;
  signal.onabort = () => {
    ++eventCount;
  };
  controller.abort();
  assert_true(signal.aborted);
  assert_equals(eventCount, 1, "event handler should have been called once");
  controller.abort();
  assert_true(signal.aborted);
  assert_equals(eventCount, 1,
                "event handler should not have been called again");
}, "controller.abort() should do nothing the second time it is called");

test(t => {
  const controller = new AbortController();
  controller.abort();
  controller.signal.onabort =
      t.unreached_func("event handler should not be called");
}, "event handler should not be called if added after controller.abort()");

test(t => {
  const controller = new AbortController();
  const signal = controller.signal;
  signal.onabort = t.step_func(e => {
    assert_equals(e.type, "abort", "event type should be abort");
    assert_equals(e.target, signal, "event target should be signal");
    assert_false(e.bubbles, "event should not bubble");
    assert_true(e.isTrusted, "event should be trusted");
  });
  controller.abort();
}, "the abort event should have the right properties");

test(t => {
  const controller = new AbortController();
  const signal = controller.signal;

  assert_true("reason" in signal, "signal has reason property");
  assert_equals(signal.reason, undefined, "signal.reason is initially undefined");

  const reason = Error("hello");
  controller.abort(reason);

  assert_true(signal.aborted, "signal.aborted");
  assert_equals(signal.reason, reason, "signal.reason");
}, "AbortController abort(reason) should set signal.reason");

test(t => {
  const controller = new AbortController();
  const signal = controller.signal;

  assert_true("reason" in signal, "signal has reason property");
  assert_equals(signal.reason, undefined, "signal.reason is initially undefined");

  controller.abort();

  assert_true(signal.aborted, "signal.aborted");
  assert_true(signal.reason instanceof DOMException, "signal.reason is DOMException");
  assert_equals(signal.reason.name, "AbortError", "signal.reason is AbortError");
}, "aborting AbortController without reason creates an \"AbortError\" DOMException");

test(t => {
  const controller = new AbortController();
  const signal = controller.signal;

  assert_true("reason" in signal, "signal has reason property");
  assert_equals(signal.reason, undefined, "signal.reason is initially undefined");

  controller.abort(undefined);

  assert_true(signal.aborted, "signal.aborted");
  assert_true(signal.reason instanceof DOMException, "signal.reason is DOMException");
  assert_equals(signal.reason.name, "AbortError", "signal.reason is AbortError");
}, "AbortController abort(undefined) creates an \"AbortError\" DOMException");

test(t => {
  const controller = new AbortController();
  const signal = controller.signal;

  assert_true("reason" in signal, "signal has reason property");
  assert_equals(signal.reason, undefined, "signal.reason is initially undefined");

  controller.abort(null);

  assert_true(signal.aborted, "signal.aborted");
  assert_equals(signal.reason, null, "signal.reason");
}, "AbortController abort(null) should set signal.reason");

test(t => {
  const signal = AbortSignal.abort();

  assert_true(signal.aborted, "signal.aborted");
  assert_true(signal.reason instanceof DOMException, "signal.reason is DOMException");
  assert_equals(signal.reason.name, "AbortError", "signal.reason is AbortError");
}, "static aborting signal should have right properties");

test(t => {
  const reason = Error("hello");
  const signal = AbortSignal.abort(reason);

  assert_true(signal.aborted, "signal.aborted");
  assert_equals(signal.reason, reason, "signal.reason");
}, "static aborting signal with reason should set signal.reason");

test(t => {
  const reason = new Error('boom');
  const message = reason.message;
  const signal = AbortSignal.abort(reason);
  assert_true(signal.aborted);
  assert_throws_exactly(reason, () => signal.throwIfAborted());
  assert_equals(reason.message, message,
                "abort.reason should not be changed by throwIfAborted()");
}, "throwIfAborted() should throw abort.reason if signal aborted");

test(t => {
  const signal = AbortSignal.abort('hello');
  assert_true(signal.aborted);
  assert_throws_exactly('hello', () => signal.throwIfAborted());
}, "throwIfAborted() should throw primitive abort.reason if signal aborted");

test(t => {
  const controller = new AbortController();
  assert_false(controller.signal.aborted);
  controller.signal.throwIfAborted();
}, "throwIfAborted() should not throw if signal not aborted");

test(t => {
  const signal = AbortSignal.abort();

  assert_true(
    signal.reason instanceof DOMException,
    "signal.reason is a DOMException"
  );
  assert_equals(
    signal.reason,
    signal.reason,
    "signal.reason returns the same DOMException"
  );
}, "AbortSignal.reason returns the same DOMException");

test(t => {
  const controller = new AbortController();
  controller.abort();

  assert_true(
    controller.signal.reason instanceof DOMException,
    "signal.reason is a DOMException"
  );
  assert_equals(
    controller.signal.reason,
    controller.signal.reason,
    "signal.reason returns the same DOMException"
  );
}, "AbortController.signal.reason returns the same DOMException");

done();
