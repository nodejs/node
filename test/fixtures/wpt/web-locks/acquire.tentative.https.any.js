// META: title=Web Locks API: navigator.locks.request method
// META: script=resources/helpers.js
// META: global=window,dedicatedworker,sharedworker,serviceworker

'use strict';

promise_test(async t => {
  const res = uniqueName(t);
  await promise_rejects_js(t, TypeError, navigator.locks.request());
  await promise_rejects_js(t, TypeError, navigator.locks.request(res));
}, 'navigator.locks.request requires a name and a callback');

promise_test(async t => {
  const res = uniqueName(t);
  await promise_rejects_js(
    t, TypeError,
    navigator.locks.request(res, {mode: 'foo'}, lock => {}));
  await promise_rejects_js(
    t, TypeError,
    navigator.locks.request(res, {mode: null }, lock => {}));
  assert_equals(await navigator.locks.request(
    res, {mode: 'exclusive'}, lock => lock.mode), 'exclusive',
                'mode is exclusive');
  assert_equals(await navigator.locks.request(
    res, {mode: 'shared'}, lock => lock.mode), 'shared',
                'mode is shared');
}, 'mode must be "shared" or "exclusive"');

promise_test(async t => {
  const res = uniqueName(t);
  await promise_rejects_dom(
    t, 'NotSupportedError',
    navigator.locks.request(
      res, {steal: true, ifAvailable: true}, lock => {}),
    "A NotSupportedError should be thrown if both " +
    "'steal' and 'ifAvailable' are specified.");
}, "The 'steal' and 'ifAvailable' options are mutually exclusive");

promise_test(async t => {
  const res = uniqueName(t);
  await promise_rejects_dom(
    t, 'NotSupportedError',
    navigator.locks.request(res, {mode: 'shared', steal: true}, lock => {}),
    'Request with mode=shared and steal=true should fail');
}, "The 'steal' option must be used with exclusive locks");

promise_test(async t => {
  const res = uniqueName(t);
  const controller = new AbortController();
  await promise_rejects_dom(
    t, 'NotSupportedError',
    navigator.locks.request(
      res, {signal: controller.signal, steal: true}, lock => {}),
    'Request with signal and steal=true should fail');
}, "The 'signal' and 'steal' options are mutually exclusive");

promise_test(async t => {
  const res = uniqueName(t);
  const controller = new AbortController();
  await promise_rejects_dom(
    t, 'NotSupportedError',
    navigator.locks.request(
      res, {signal: controller.signal, ifAvailable: true}, lock => {}),
    'Request with signal and ifAvailable=true should fail');
}, "The 'signal' and 'ifAvailable' options are mutually exclusive");

promise_test(async t => {
  const res = uniqueName(t);
  await promise_rejects_js(
    t, TypeError, navigator.locks.request(res, undefined));
  await promise_rejects_js(
    t, TypeError, navigator.locks.request(res, null));
  await promise_rejects_js(
    t, TypeError, navigator.locks.request(res, 123));
  await promise_rejects_js(
    t, TypeError, navigator.locks.request(res, 'abc'));
  await promise_rejects_js(
    t, TypeError, navigator.locks.request(res, []));
  await promise_rejects_js(
    t, TypeError, navigator.locks.request(res, {}));
  await promise_rejects_js(
    t, TypeError, navigator.locks.request(res, new Promise(r => {})));
}, 'callback must be a function');

promise_test(async t => {
  const res = uniqueName(t);
  let release;
  const promise = new Promise(r => { release = r; });

  let returned = navigator.locks.request(res, lock => { return promise; });

  const order = [];

  returned.then(() => { order.push('returned'); });
  promise.then(() => { order.push('holding'); });

  release();

  await Promise.all([returned, promise]);

  assert_array_equals(order, ['holding', 'returned']);

}, 'navigator.locks.request\'s returned promise resolves after' +
   ' lock is released');

promise_test(async t => {
  const res = uniqueName(t);
  const test_error = {name: 'test'};
  const p = navigator.locks.request(res, lock => {
    throw test_error;
  });
  assert_equals(Promise.resolve(p), p, 'request() result is a Promise');
  await promise_rejects_exactly(t, test_error, p, 'result should reject');
}, 'Returned Promise rejects if callback throws synchronously');

promise_test(async t => {
  const res = uniqueName(t);
  const test_error = {name: 'test'};
  const p = navigator.locks.request(res, async lock => {
    throw test_error;
  });
  assert_equals(Promise.resolve(p), p, 'request() result is a Promise');
  await promise_rejects_exactly(t, test_error, p, 'result should reject');
}, 'Returned Promise rejects if callback throws asynchronously');
