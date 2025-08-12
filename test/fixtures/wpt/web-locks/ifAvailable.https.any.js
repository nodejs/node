// META: title=Web Locks API: ifAvailable option
// META: script=resources/helpers.js
// META: global=window,dedicatedworker,sharedworker,serviceworker

'use strict';

promise_test(async t => {
  const res = self.uniqueName(t);
  let callback_called = false;
  await navigator.locks.request(res, {ifAvailable: true}, async lock => {
    callback_called = true;
    assert_not_equals(lock, null, 'lock should be granted');
  });
  assert_true(callback_called, 'callback should be called');
}, 'Lock request with ifAvailable - lock available');

promise_test(async t => {
  const res = self.uniqueName(t);
  let callback_called = false;
  await navigator.locks.request(res, async lock => {
    // Request would time out if |ifAvailable| was not specified.
    const result = await navigator.locks.request(
      res, {ifAvailable: true}, async lock => {
        callback_called = true;
        assert_equals(lock, null, 'lock should not be granted');
        return 123;
      });
    assert_equals(result, 123, 'result should be value returned by callback');
  });
  assert_true(callback_called, 'callback should be called');
}, 'Lock request with ifAvailable - lock not available');

promise_test(async t => {
  const res = self.uniqueName(t);
  let callback_called = false;
  await navigator.locks.request(res, async lock => {
    try {
      // Request would time out if |ifAvailable| was not specified.
      await navigator.locks.request(res, {ifAvailable: true}, async lock => {
        callback_called = true;
        assert_equals(lock, null, 'lock should not be granted');
        throw 123;
      });
      assert_unreached('call should throw');
    } catch (ex) {
      assert_equals(ex, 123, 'ex should be value thrown by callback');
    }
  });
  assert_true(callback_called, 'callback should be called');
}, 'Lock request with ifAvailable - lock not available, callback throws');

promise_test(async t => {
  const res = self.uniqueName(t);
  let callback_called = false;
  await navigator.locks.request(res, async lock => {
    // Request with a different name - should be grantable.
    await navigator.locks.request('different', {ifAvailable: true}, async lock => {
      callback_called = true;
      assert_not_equals(lock, null, 'lock should be granted');
    });
  });
  assert_true(callback_called, 'callback should be called');
}, 'Lock request with ifAvailable - unrelated lock held');

promise_test(async t => {
  const res = self.uniqueName(t);
  let callback_called = false;
  await navigator.locks.request(res, {mode: 'shared'}, async lock => {
    await navigator.locks.request(
      res, {mode: 'shared', ifAvailable: true}, async lock => {
        callback_called = true;
        assert_not_equals(lock, null, 'lock should be granted');
      });
  });
  assert_true(callback_called, 'callback should be called');
}, 'Shared lock request with ifAvailable - shared lock held');

promise_test(async t => {
  const res = self.uniqueName(t);
  let callback_called = false;
  await navigator.locks.request(res, {mode: 'shared'}, async lock => {
    // Request would time out if |ifAvailable| was not specified.
    await navigator.locks.request(res, {ifAvailable: true}, async lock => {
      callback_called = true;
      assert_equals(lock, null, 'lock should not be granted');
    });
  });
  assert_true(callback_called, 'callback should be called');
}, 'Exclusive lock request with ifAvailable - shared lock held');

promise_test(async t => {
  const res = self.uniqueName(t);
  let callback_called = false;
  await navigator.locks.request(res, async lock => {
    // Request would time out if |ifAvailable| was not specified.
    await navigator.locks.request(
      res, {mode: 'shared', ifAvailable: true}, async lock => {
        callback_called = true;
        assert_equals(lock, null, 'lock should not be granted');
      });
  });
  assert_true(callback_called, 'callback should be called');
}, 'Shared lock request with ifAvailable - exclusive lock held');

promise_test(async t => {
  const res = self.uniqueName(t);
  let callback_called = false;
  await navigator.locks.request(res, async lock => {
    callback_called = true;
    const test_error = {name: 'test'};
    const p = navigator.locks.request(
      res, {ifAvailable: true}, lock => {
        assert_equals(lock, null, 'lock should not be available');
        throw test_error;
      });
    assert_equals(Promise.resolve(p), p, 'request() result is a Promise');
    await promise_rejects_exactly(t, test_error, p, 'result should reject');
  });
  assert_true(callback_called, 'callback should be called');
}, 'Returned Promise rejects if callback throws synchronously');

promise_test(async t => {
  const res = self.uniqueName(t);
  let callback_called = false;
  await navigator.locks.request(res, async lock => {
    callback_called = true;
    const test_error = {name: 'test'};
    const p = navigator.locks.request(
      res, {ifAvailable: true}, async lock => {
        assert_equals(lock, null, 'lock should not be available');
        throw test_error;
      });
    assert_equals(Promise.resolve(p), p, 'request() result is a Promise');
    await promise_rejects_exactly(t, test_error, p, 'result should reject');
  });
  assert_true(callback_called, 'callback should be called');
}, 'Returned Promise rejects if async callback yields rejected promise');

// Regression test for: https://crbug.com/840994
promise_test(async t => {
  const res1 = self.uniqueName(t);
  const res2 = self.uniqueName(t);
  let callback1_called = false;
  await navigator.locks.request(res1, async lock => {
    callback1_called = true;
    let callback2_called = false;
    await navigator.locks.request(res2, async lock => {
      callback2_called = true;
    });
    assert_true(callback2_called, 'callback2 should be called');

    let callback3_called = false;
    await navigator.locks.request(res2, {ifAvailable: true}, async lock => {
      callback3_called = true;
      // This request would fail if the "is this grantable?" test
      // failed, e.g. due to the release without a pending request
      // skipping steps.
      assert_not_equals(lock, null, 'Lock should be available');
    });
    assert_true(callback3_called, 'callback2 should be called');
  });
  assert_true(callback1_called, 'callback1 should be called');
}, 'Locks are available once previous release is processed');
