// META: title=Web Locks API: Shared Mode
// META: global=window,dedicatedworker,sharedworker,serviceworker

'use strict';

promise_test(async t => {
  const granted = [];
  function log_grant(n) { return () => { granted.push(n); }; }

  await Promise.all([
    navigator.locks.request('a', {mode: 'shared'}, log_grant(1)),
    navigator.locks.request('b', {mode: 'shared'}, log_grant(2)),
    navigator.locks.request('c', {mode: 'shared'}, log_grant(3)),
    navigator.locks.request('a', {mode: 'shared'}, log_grant(4)),
    navigator.locks.request('b', {mode: 'shared'}, log_grant(5)),
    navigator.locks.request('c', {mode: 'shared'}, log_grant(6)),
  ]);

  assert_array_equals(granted, [1, 2, 3, 4, 5, 6]);
}, 'Lock requests are granted in order');

promise_test(async t => {
  let a_acquired = false, a_acquired_again = false;

  await navigator.locks.request('a', {mode: 'shared'}, async lock => {
    a_acquired = true;

    // Since lock is held, this request would be blocked if the
    // lock was not 'shared', causing this test to time out.

    await navigator.locks.request('a', {mode: 'shared'}, lock => {
      a_acquired_again = true;
    });
  });

  assert_true(a_acquired, 'first lock acquired');
  assert_true(a_acquired_again, 'second lock acquired');
}, 'Shared locks are not exclusive');
