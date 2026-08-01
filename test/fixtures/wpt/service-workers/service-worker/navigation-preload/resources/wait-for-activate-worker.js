// This worker remains in the installing phase so that the
// navigation preload API can be tested when there is no
// active worker.
importScripts('/resources/testharness.js');
importScripts('helpers.js');

function expect_rejection(promise) {
  return promise.then(
      () => { return Promise.reject('unexpected fulfillment'); },
      err => { assert_equals('InvalidStateError', err.name); });
}

function test_before_activation() {
  const np = self.registration.navigationPreload;
  return expect_rejection(np.enable())
      .then(() => expect_rejection(np.disable()))
      .then(() => expect_rejection(np.setHeaderValue('hi')))
      .then(() => np.getState())
      .then(state => expect_navigation_preload_state(
          state, false, 'true', 'state should be the default'))
      .then(() => 'PASS')
      .catch(err => 'FAIL: ' + err);
}

var resolve_done_promise;
var done_promise = new Promise(resolve => { resolve_done_promise = resolve; });

// Run the test once the page messages this worker.
self.addEventListener('message', e => {
    e.waitUntil(test_before_activation()
        .then(result => {
            e.source.postMessage(result);
            resolve_done_promise();
          }));
  });

// Don't become the active worker until the test is done.
self.addEventListener('install', e => {
    e.waitUntil(done_promise);
  });
