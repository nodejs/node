// META: title=serviceWorker on service worker global
// META: global=serviceworker

test(() => {
  assert_equals(registration.installing, null, 'registration.installing');
  assert_equals(registration.waiting, null, 'registration.waiting');
  assert_equals(registration.active, null, 'registration.active');
  assert_true('serviceWorker' in self, 'self.serviceWorker exists');
  assert_equals(serviceWorker.state, 'parsed', 'serviceWorker.state');
  assert_readonly(self, 'serviceWorker', `self.serviceWorker is read only`);
}, 'First run');

// Cache this for later tests.
const initialServiceWorker = self.serviceWorker;

async_test((t) => {
  assert_true('serviceWorker' in self, 'self.serviceWorker exists');
  serviceWorker.postMessage({ messageTest: true });

  // The rest of the test runs once this receives the above message.
  addEventListener('message', t.step_func((event) => {
    // Ignore unrelated messages.
    if (!event.data.messageTest) return;
    assert_equals(event.source, serviceWorker, 'event.source');
    t.done();
  }));
}, 'Can post message to self during startup');

// The test is registered now so there isn't a race condition when collecting tests, but the asserts
// don't happen until the 'install' event fires.
async_test((t) => {
  addEventListener('install', t.step_func_done(() => {
    assert_true('serviceWorker' in self, 'self.serviceWorker exists');
    assert_equals(serviceWorker, initialServiceWorker, `self.serviceWorker hasn't changed`);
    assert_equals(registration.installing, serviceWorker, 'registration.installing');
    assert_equals(registration.waiting, null, 'registration.waiting');
    assert_equals(registration.active, null, 'registration.active');
    assert_equals(serviceWorker.state, 'installing', 'serviceWorker.state');
  }));
}, 'During install');

// The test is registered now so there isn't a race condition when collecting tests, but the asserts
// don't happen until the 'activate' event fires.
async_test((t) => {
  addEventListener('activate', t.step_func_done(() => {
    assert_true('serviceWorker' in self, 'self.serviceWorker exists');
    assert_equals(serviceWorker, initialServiceWorker, `self.serviceWorker hasn't changed`);
    assert_equals(registration.installing, null, 'registration.installing');
    assert_equals(registration.waiting, null, 'registration.waiting');
    assert_equals(registration.active, serviceWorker, 'registration.active');
    assert_equals(serviceWorker.state, 'activating', 'serviceWorker.state');
  }));
}, 'During activate');
