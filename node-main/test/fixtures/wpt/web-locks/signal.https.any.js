// META: title=Web Locks API: AbortSignal integration
// META: script=resources/helpers.js
// META: global=window,dedicatedworker,sharedworker,serviceworker

'use strict';

promise_test(async t => {
  const res = uniqueName(t);

  // These cases should not work:
  for (const signal of ['string', 12.34, false, {}, Symbol(), () => {}, self]) {
    await promise_rejects_js(
      t, TypeError,
      navigator.locks.request(
        res, {signal}, t.unreached_func('callback should not run')),
      'Bindings should throw if the signal option is a not an AbortSignal');
  }
}, 'The signal option must be an AbortSignal');

promise_test(async t => {
  const res = uniqueName(t);
  const controller = new AbortController();
  controller.abort();

  await promise_rejects_dom(
    t, 'AbortError',
    navigator.locks.request(res, {signal: controller.signal},
                            t.unreached_func('callback should not run')),
    'Request should reject with AbortError');
}, 'Passing an already aborted signal aborts');

promise_test(async t => {
  const res = uniqueName(t);

  const controller = new AbortController();
  const reason = 'My dog ate it.';
  controller.abort(reason);

  const promise =
        navigator.locks.request(res, {signal: controller.signal},
                                t.unreached_func('callback should not run'));

  await promise_rejects_exactly(
    t, reason, promise, "Rejection should give the abort reason");
}, 'Passing an already aborted signal rejects with the custom abort reason.');

promise_test(async t => {
  const res = uniqueName(t);

  const controller = new AbortController();
  controller.abort();

  const promise =
        navigator.locks.request(res, {signal: controller.signal},
                                t.unreached_func('callback should not run'));

  await promise_rejects_exactly(
    t, controller.signal.reason, promise,
    "Rejection should give the abort reason");
}, 'Passing an already aborted signal rejects with the default abort reason.');

promise_test(async t => {
  const res = uniqueName(t);

  // Grab a lock and hold it until this subtest completes.
  requestLockAndHold(t, res);

  const controller = new AbortController();

  const promise =
    navigator.locks.request(res, {signal: controller.signal},
                            t.unreached_func('callback should not run'));

  // Verify the request is enqueued:
  const state = await navigator.locks.query();
  assert_equals(state.held.filter(lock => lock.name === res).length, 1,
                'Number of held locks');
  assert_equals(state.pending.filter(lock => lock.name === res).length, 1,
                'Number of pending locks');

  const rejected = promise_rejects_dom(
    t, 'AbortError', promise, 'Request should reject with AbortError');

  controller.abort();

  await rejected;

}, 'An aborted request results in AbortError');

promise_test(async t => {
  const res = uniqueName(t);

  // Grab a lock and hold it until this subtest completes.
  requestLockAndHold(t, res);

  const controller = new AbortController();

  const promise =
    navigator.locks.request(res, {signal: controller.signal}, lock => {});

  // Verify the request is enqueued:
  const state = await navigator.locks.query();
  assert_equals(state.held.filter(lock => lock.name === res).length, 1,
                'Number of held locks');
  assert_equals(state.pending.filter(lock => lock.name === res).length, 1,
                'Number of pending locks');

  const rejected = promise_rejects_dom(
    t, 'AbortError', promise, 'Request should reject with AbortError');

  let callback_called = false;
  t.step_timeout(() => {
    callback_called = true;
    controller.abort();
  }, 10);

  await rejected;
  assert_true(callback_called, 'timeout should have caused the abort');

}, 'Abort after a timeout');

promise_test(async t => {
  const res = uniqueName(t);

  const controller = new AbortController();

  let got_lock = false;
  await navigator.locks.request(
    res, {signal: controller.signal}, async lock => { got_lock = true; });

  assert_true(got_lock, 'Lock should be acquired if abort is not signaled.');

}, 'Signal that is not aborted');

promise_test(async t => {
  const res = uniqueName(t);

  const controller = new AbortController();

  let got_lock = false;
  const p = navigator.locks.request(
    res, {signal: controller.signal}, lock => { got_lock = true; });

  // Even though lock is grantable, this abort should be processed synchronously.
  controller.abort();

  await promise_rejects_dom(t, 'AbortError', p, 'Request should abort');

  assert_false(got_lock, 'Request should be aborted if signal is synchronous');

  await navigator.locks.request(res, lock => { got_lock = true; });
  assert_true(got_lock, 'Subsequent request should not be blocked');

}, 'Synchronously signaled abort');

promise_test(async t => {
  const res = uniqueName(t);

  const controller = new AbortController();

  // Make a promise that resolves when the lock is acquired.
  const [acquired_promise, acquired_func] = makePromiseAndResolveFunc();

  // Request the lock.
  let release_func;
  const released_promise = navigator.locks.request(
    res, {signal: controller.signal}, lock => {
      acquired_func();

      // Hold lock until release_func is called.
      const [waiting_promise, waiting_func] = makePromiseAndResolveFunc();
      release_func = waiting_func;
      return waiting_promise;
    });

  // Wait for the lock to be acquired.
  await acquired_promise;

  // Signal an abort.
  controller.abort();

  // Release the lock.
  release_func('resolved ok');

  assert_equals(await released_promise, 'resolved ok',
                'Lock released promise should not reject');

}, 'Abort signaled after lock granted');

promise_test(async t => {
  const res = uniqueName(t);

  const controller = new AbortController();

  // Make a promise that resolves when the lock is acquired.
  const [acquired_promise, acquired_func] = makePromiseAndResolveFunc();

  // Request the lock.
  let release_func;
  const released_promise = navigator.locks.request(
    res, {signal: controller.signal}, lock => {
      acquired_func();

      // Hold lock until release_func is called.
      const [waiting_promise, waiting_func] = makePromiseAndResolveFunc();
      release_func = waiting_func;
      return waiting_promise;
    });

  // Wait for the lock to be acquired.
  await acquired_promise;

  // Release the lock.
  release_func('resolved ok');

  // Signal an abort.
  controller.abort();

  assert_equals(await released_promise, 'resolved ok',
                'Lock released promise should not reject');

}, 'Abort signaled after lock released');

promise_test(async t => {
  const res = uniqueName(t);

  const controller = new AbortController();
  const first = requestLockAndHold(t, res, { signal: controller.signal });
  const next = navigator.locks.request(res, () => "resolved");
  controller.abort();

  await promise_rejects_dom(t, "AbortError", first, "Request should abort");
  assert_equals(
    await next,
    "resolved",
    "The next request is processed after abort"
  );
}, "Abort should process the next pending lock request");

promise_test(async t => {
  const res = uniqueName(t);

  const controller = new AbortController();
  const promise = requestLockAndHold(t, res, { signal: controller.signal });

  const reason = "My cat handled it";
  controller.abort(reason);

  await promise_rejects_exactly(t, reason, promise, "Rejection should give the abort reason");
}, "Aborted promise should reject with the custom abort reason");

promise_test(async t => {
  const res = uniqueName(t);

  const controller = new AbortController();
  const promise = requestLockAndHold(t, res, { signal: controller.signal });

  controller.abort();

  await promise_rejects_exactly(t, controller.signal.reason, promise, "Should be the same reason");
}, "Aborted promise should reject with the default abort reason");
