// META: title=Web Locks API: Mixed Modes
// META: script=resources/helpers.js
// META: global=window,dedicatedworker,sharedworker,serviceworker

'use strict';

promise_test(async t => {
  let unblock;
  const blocked = new Promise(r => { unblock = r; });

  const granted = [];

  // These should be granted immediately, and held until unblocked.
  navigator.locks.request('a', {mode: 'shared'}, async lock => {
    granted.push('a-shared-1'); await blocked; });
  navigator.locks.request('a', {mode: 'shared'}, async lock => {
    granted.push('a-shared-2'); await blocked; });
  navigator.locks.request('a', {mode: 'shared'}, async lock => {
    granted.push('a-shared-3'); await blocked; });

  // This should be blocked.
  let exclusive_lock;
  const exclusive_request = navigator.locks.request('a', async lock => {
    granted.push('a-exclusive');
    exclusive_lock = lock;
  });

  // This should be granted immediately (different name).
  await navigator.locks.request('b', {mode: 'exclusive'}, lock => {
    granted.push('b-exclusive'); });

  assert_array_equals(
    granted, ['a-shared-1', 'a-shared-2', 'a-shared-3', 'b-exclusive']);

  // Release the shared locks granted above.
  unblock();

  // Now the blocked request can be granted.
  await exclusive_request;
  assert_equals(exclusive_lock.mode, 'exclusive');

  assert_array_equals(
    granted,
    ['a-shared-1', 'a-shared-2', 'a-shared-3', 'b-exclusive', 'a-exclusive']);

}, 'Lock requests are granted in order');

promise_test(async t => {
  const res = uniqueName(t);

  let [promise, resolve] = makePromiseAndResolveFunc();

  const exclusive = navigator.locks.request(res, () => promise);
  for (let i = 0; i < 5; i++) {
    requestLockAndHold(t, res, { mode: "shared" });
  }

  let answer = await navigator.locks.query();
  assert_equals(answer.held.length, 1, "An exclusive lock is held");
  assert_equals(answer.pending.length, 5, "Requests for shared locks are pending");
  resolve();
  await exclusive;

  answer = await navigator.locks.query();
  assert_equals(answer.held.length, 5, "Shared locks are held");
  assert_true(answer.held.every(l => l.mode === "shared"), "All held locks are shared ones");
}, 'Releasing exclusive lock grants multiple shared locks');

promise_test(async t => {
  const res = uniqueName(t);

  let [sharedPromise, sharedResolve] = makePromiseAndResolveFunc();
  let [exclusivePromise, exclusiveResolve] = makePromiseAndResolveFunc();

  const sharedReleasedPromise = Promise.all(new Array(5).fill(0).map(
    () => navigator.locks.request(res, { mode: "shared" }, () => sharedPromise))
  );
  const exclusiveReleasedPromise = navigator.locks.request(res, () => exclusivePromise);
  for (let i = 0; i < 5; i++) {
    requestLockAndHold(t, res, { mode: "shared" });
  }

  let answer = await navigator.locks.query();
  assert_equals(answer.held.length, 5, "Shared locks are held");
  assert_true(answer.held.every(l => l.mode === "shared"), "All held locks are shared ones");
  sharedResolve();
  await sharedReleasedPromise;

  answer = await navigator.locks.query();
  assert_equals(answer.held.length, 1, "An exclusive lock is held");
  assert_equals(answer.held[0].mode, "exclusive");
  exclusiveResolve();
  await exclusiveReleasedPromise;

  answer = await navigator.locks.query();
  assert_equals(answer.held.length, 5, "The next shared locks are held");
  assert_true(answer.held.every(l => l.mode === "shared"), "All held locks are shared ones");
}, 'An exclusive lock between shared locks');
