// META: title=Web Locks API: Exclusive Mode
// META: global=window,dedicatedworker,sharedworker,serviceworker

'use strict';

promise_test(async t => {
  const granted = [];
  function log_grant(n) { return () => { granted.push(n); }; }

  await Promise.all([
    navigator.locks.request('a', log_grant(1)),
    navigator.locks.request('a', log_grant(2)),
    navigator.locks.request('a', log_grant(3))
  ]);
  assert_array_equals(granted, [1, 2, 3]);
}, 'Lock requests are granted in order');

promise_test(async t => {
  const granted = [];
  function log_grant(n) { return () => { granted.push(n); }; }

  let inner_promise;
  await navigator.locks.request('a', async lock => {
    inner_promise = Promise.all([
      // This will be blocked.
      navigator.locks.request('a', log_grant(1)),
      // But this should be grantable immediately.
      navigator.locks.request('b', log_grant(2))
    ]);
  });

  await inner_promise;
  assert_array_equals(granted, [2, 1]);
}, 'Requests for distinct resources can be granted');
