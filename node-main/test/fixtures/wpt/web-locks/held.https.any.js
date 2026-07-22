// META: title=Web Locks API: Lock held until callback result resolves
// META: script=resources/helpers.js
// META: global=window,dedicatedworker,sharedworker,serviceworker

'use strict';

// For uncaught rejections.
setup({allow_uncaught_exception: true});

function snooze(t, ms) { return new Promise(r => t.step_timeout(r, ms)); }

promise_test(async t => {
  const res = uniqueName(t);
  const p = navigator.locks.request(res, lock => 123);
  assert_equals(Promise.resolve(p), p, 'request() result is a Promise');
  assert_equals(await p, 123, 'promise resolves to the returned value');
}, 'callback\'s result is promisified if not async');

promise_test(async t => {
  const res = uniqueName(t);
  // Resolved when the lock is granted.
  let granted;
  const lock_granted_promise = new Promise(r => { granted = r; });

  // Lock is held until this is resolved.
  let resolve;
  const lock_release_promise = new Promise(r => { resolve = r; });

  const order = [];

  navigator.locks.request(res, lock => {
    granted(lock);
    return lock_release_promise;
  });
  await lock_granted_promise;

  await Promise.all([
    snooze(t, 50).then(() => {
      order.push('1st lock released');
      resolve();
    }),
    navigator.locks.request(res, () => {
      order.push('2nd lock granted');
    })
  ]);

  assert_array_equals(order, ['1st lock released', '2nd lock granted']);
}, 'lock is held until callback\'s returned promise resolves');

promise_test(async t => {
  const res = uniqueName(t);
  // Resolved when the lock is granted.
  let granted;
  const lock_granted_promise = new Promise(r => { granted = r; });

  // Lock is held until this is rejected.
  let reject;
  const lock_release_promise = new Promise((_, r) => { reject = r; });

  const order = [];

  navigator.locks.request(res, lock => {
    granted(lock);
    return lock_release_promise;
  });
  await lock_granted_promise;

  await Promise.all([
    snooze(t, 50).then(() => {
      order.push('reject');
      reject(new Error('this uncaught rejection is expected'));
    }),
    navigator.locks.request(res, () => {
      order.push('2nd lock granted');
    })
  ]);

  assert_array_equals(order, ['reject', '2nd lock granted']);
}, 'lock is held until callback\'s returned promise rejects');

promise_test(async t => {
  const res = uniqueName(t);
  let callback_called = false;
  await navigator.locks.request(res, async lock => {
    await navigator.locks.request(res, {ifAvailable: true}, lock => {
      callback_called = true;
      assert_equals(lock, null, 'lock request should fail if held');
    });
  });
  assert_true(callback_called, 'callback should have executed');
}, 'held lock prevents the same client from acquiring it');
