// META: title=Web Locks API: Mixed Modes
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
