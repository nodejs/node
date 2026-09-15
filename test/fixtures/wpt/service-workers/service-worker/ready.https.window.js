// META: title=Service Worker: navigator.serviceWorker.ready
// META: script=resources/test-helpers.sub.js

test(() => {
  assert_equals(
    navigator.serviceWorker.ready,
    navigator.serviceWorker.ready,
    'repeated access to ready without intervening registrations should return the same Promise object'
  );
}, 'ready returns the same Promise object');

promise_test(async t => {
  const frame = await with_iframe('resources/blank.html?uncontrolled');
  t.add_cleanup(() => frame.remove());

  const promise = frame.contentWindow.navigator.serviceWorker.ready;

  assert_equals(
    Object.getPrototypeOf(promise),
    frame.contentWindow.Promise.prototype,
    'the Promise should be in the context of the related document'
  );
}, 'ready returns a Promise object in the context of the related document');

promise_test(async t => {
  const url = 'resources/empty-worker.js';
  const scope = 'resources/blank.html?ready-controlled';
  const expectedURL = normalizeURL(url);
  const registration = await service_worker_unregister_and_register(t, url, scope);
  t.add_cleanup(() => registration.unregister());

  await wait_for_state(t, registration.installing, 'activated');

  const frame = await with_iframe(scope);
  t.add_cleanup(() => frame.remove());

  const readyReg = await frame.contentWindow.navigator.serviceWorker.ready;

  assert_equals(readyReg.installing, null, 'installing should be null');
  assert_equals(readyReg.waiting, null, 'waiting should be null');
  assert_equals(readyReg.active.scriptURL, expectedURL, 'active after ready should not be null');
  assert_equals(
    frame.contentWindow.navigator.serviceWorker.controller,
    readyReg.active,
    'the controller should be the active worker'
  );
  assert_in_array(
    readyReg.active.state,
    ['activating', 'activated'],
    '.ready should be resolved when the registration has an active worker'
  );
}, 'ready on a controlled document');

promise_test(async t => {
  const url = 'resources/empty-worker.js';
  const scope = 'resources/blank.html?ready-potential-controlled';
  const expected_url = normalizeURL(url);
  const frame = await with_iframe(scope);
  t.add_cleanup(() => frame.remove());

  const registration = await navigator.serviceWorker.register(url, { scope });
  t.add_cleanup(() => registration.unregister());

  const readyReg = await frame.contentWindow.navigator.serviceWorker.ready;

  assert_equals(readyReg.installing, null, 'installing should be null');
  assert_equals(readyReg.waiting, null, 'waiting should be null.')
  assert_equals(readyReg.active.scriptURL, expected_url, 'active after ready should not be null');
  assert_in_array(
    readyReg.active.state,
    ['activating', 'activated'],
    '.ready should be resolved when the registration has an active worker'
  );
  assert_equals(
    frame.contentWindow.navigator.serviceWorker.controller,
    null,
    'uncontrolled document should not have a controller'
  );
}, 'ready on a potential controlled document');

promise_test(async t => {
  const url = 'resources/empty-worker.js';
  const scope = 'resources/blank.html?ready-installing';

  await service_worker_unregister(t, scope);

  const frame = await with_iframe(scope);
  const promise = frame.contentWindow.navigator.serviceWorker.ready;
  navigator.serviceWorker.register(url, { scope });
  const registration = await promise;

  t.add_cleanup(async () => {
    await registration.unregister();
    frame.remove();
  });

  assert_equals(registration.installing, null, 'installing should be null');
  assert_equals(registration.waiting, null, 'waiting should be null');
  assert_not_equals(registration.active, null, 'active after ready should not be null');
  assert_in_array(
    registration.active.state,
    ['activating', 'activated'],
    '.ready should be resolved when the registration has an active worker'
  );
}, 'ready on an iframe whose parent registers a new service worker');

promise_test(async t => {
  const scope = 'resources/register-iframe.html';
  const frame = await with_iframe(scope);

  const registration = await frame.contentWindow.navigator.serviceWorker.ready;

  t.add_cleanup(async () => {
    await registration.unregister();
    frame.remove();
  });

  assert_equals(registration.installing, null, 'installing should be null');
  assert_equals(registration.waiting, null, 'waiting should be null');
  assert_not_equals(registration.active, null, 'active after ready should not be null');
  assert_in_array(
    registration.active.state,
    ['activating', 'activated'],
    '.ready should be resolved with "active worker"'
  );
 }, 'ready on an iframe that installs a new service worker');

promise_test(async t => {
  const url = 'resources/empty-worker.js';
  const matchedScope = 'resources/blank.html?ready-after-match';
  const longerMatchedScope = 'resources/blank.html?ready-after-match-longer';

  await service_worker_unregister(t, matchedScope);
  await service_worker_unregister(t, longerMatchedScope);

  const frame = await with_iframe(longerMatchedScope);
  const registration = await navigator.serviceWorker.register(url, { scope: matchedScope });

  t.add_cleanup(async () => {
    await registration.unregister();
    frame.remove();
  });

  await wait_for_state(t, registration.installing, 'activated');

  const longerRegistration = await navigator.serviceWorker.register(url, { scope: longerMatchedScope });

  t.add_cleanup(() => longerRegistration.unregister());

  const readyReg = await frame.contentWindow.navigator.serviceWorker.ready;

  assert_equals(
    readyReg.scope,
    normalizeURL(longerMatchedScope),
    'longer matched registration should be returned'
  );
  assert_equals(
    frame.contentWindow.navigator.serviceWorker.controller,
    null,
    'controller should be null'
  );
}, 'ready after a longer matched registration registered');

promise_test(async t => {
  const url = 'resources/empty-worker.js';
  const matchedScope = 'resources/blank.html?ready-after-resolve';
  const longerMatchedScope = 'resources/blank.html?ready-after-resolve-longer';
  const registration = await service_worker_unregister_and_register(t, url, matchedScope);
  t.add_cleanup(() => registration.unregister());

  await wait_for_state(t, registration.installing, 'activated');

  const frame = await with_iframe(longerMatchedScope);
  t.add_cleanup(() => frame.remove());

  const readyReg1 = await frame.contentWindow.navigator.serviceWorker.ready;

  assert_equals(
    readyReg1.scope,
    normalizeURL(matchedScope),
    'matched registration should be returned'
  );

  const longerReg = await navigator.serviceWorker.register(url, { scope: longerMatchedScope });
  t.add_cleanup(() => longerReg.unregister());

  const readyReg2 = await frame.contentWindow.navigator.serviceWorker.ready;

  assert_equals(
    readyReg2.scope,
    normalizeURL(matchedScope),
    'ready should only be resolved once'
  );
}, 'access ready after it has been resolved');

promise_test(async t => {
  const url1 = 'resources/empty-worker.js';
  const url2 = url1 + '?2';
  const matchedScope = 'resources/blank.html?ready-after-unregister';
  const reg1 = await service_worker_unregister_and_register(t, url1, matchedScope);
  t.add_cleanup(() => reg1.unregister());

  await wait_for_state(t, reg1.installing, 'activating');

  const frame = await with_iframe(matchedScope);
  t.add_cleanup(() => frame.remove());

  await reg1.unregister();

  // Ready promise should be pending, waiting for a new registration to arrive
  const readyPromise = frame.contentWindow.navigator.serviceWorker.ready;

  const reg2 = await navigator.serviceWorker.register(url2, { scope: matchedScope });
  t.add_cleanup(() => reg2.unregister());

  const readyReg = await readyPromise;

  // Wait for registration update, since it comes from another global, the states are racy.
  await wait_for_state(t, reg2.installing || reg2.waiting || reg2.active, 'activated');

  assert_equals(readyReg.active.scriptURL, reg2.active.scriptURL, 'Resolves with the second registration');
  assert_not_equals(reg1, reg2, 'Registrations should be different');
}, 'resolve ready after unregistering');
