// META: title=Web Locks API: steal option
// META: script=resources/helpers.js
// META: global=window,dedicatedworker,sharedworker,serviceworker

'use strict';

const never_settled = new Promise(resolve => { /* never */ });

promise_test(async t => {
  const res = uniqueName(t);
  let callback_called = false;
  await navigator.locks.request(res, {steal: true}, lock => {
    callback_called = true;
    assert_not_equals(lock, null, 'Lock should be granted');
  });
  assert_true(callback_called, 'Callback should be called');
}, 'Lock available');

promise_test(async t => {
  const res = uniqueName(t);
  let callback_called = false;

  // Grab and hold the lock.
  navigator.locks.request(res, lock => never_settled).catch(_ => {});

  // Steal it.
  await navigator.locks.request(res, {steal: true}, lock => {
    callback_called = true;
    assert_not_equals(lock, null, 'Lock should be granted');
  });

  assert_true(callback_called, 'Callback should be called');
}, 'Lock not available');

promise_test(async t => {
  const res = uniqueName(t);

  // Grab and hold the lock.
  const promise = navigator.locks.request(res, lock => never_settled);
  const assertion = promise_rejects_dom(
    t, 'AbortError', promise, `Initial request's promise should reject`);

  // Steal it.
  await navigator.locks.request(res, {steal: true}, lock => {});

  await assertion;

}, `Broken lock's release promise rejects`);

promise_test(async t => {
  const res = uniqueName(t);

  // Grab and hold the lock.
  navigator.locks.request(res, lock => never_settled).catch(_ => {});

  // Make a request for it.
  let request_granted = false;
  const promise = navigator.locks.request(res, lock => {
    request_granted = true;
  });

  // Steal it.
  await navigator.locks.request(res, {steal: true}, lock => {
    assert_false(request_granted, 'Steal should override request');
  });

  await promise;
  assert_true(request_granted, 'Request should eventually be granted');

}, `Requested lock's release promise is deferred`);

promise_test(async t => {
  const res = uniqueName(t);

  // Grab and hold the lock.
  navigator.locks.request(res, lock => never_settled).catch(_ => {});

  // Steal it.
  let saw_abort = false;
  const first_steal = navigator.locks.request(
    res, {steal: true}, lock => never_settled).catch(error => {
      saw_abort = true;
    });

  // Steal it again.
  await navigator.locks.request(res, {steal: true}, lock => {});

  await first_steal;
  assert_true(saw_abort, 'First steal should have aborted');

}, 'Last caller wins');
