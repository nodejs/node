// META: title=Web Locks API: Lock Attributes
// META: global=window,dedicatedworker,sharedworker,serviceworker

'use strict';

promise_test(async t => {
  await navigator.locks.request('resource', lock => {
    assert_equals(lock.name, 'resource');
    assert_equals(lock.mode, 'exclusive');
  });
}, 'Lock attributes reflect requested properties (exclusive)');

promise_test(async t => {
  await navigator.locks.request('resource', {mode: 'shared'}, lock => {
    assert_equals(lock.name, 'resource');
    assert_equals(lock.mode, 'shared');
  });
}, 'Lock attributes reflect requested properties (shared)');
