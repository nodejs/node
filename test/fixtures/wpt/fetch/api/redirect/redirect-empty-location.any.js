// META: global=window,worker
// META: script=../resources/utils.js

// Tests receiving a redirect response with a Location header with an empty
// value.

const url = RESOURCES_DIR + 'redirect-empty-location.py';

promise_test(t => {
  return promise_rejects_js(t, TypeError, fetch(url, {redirect:'follow'}));
}, 'redirect response with empty Location, follow mode');

promise_test(t => {
  return fetch(url, {redirect:'manual'})
    .then(resp => {
      assert_equals(resp.type, 'opaqueredirect');
      assert_equals(resp.status, 0);
    });
}, 'redirect response with empty Location, manual mode');

done();
