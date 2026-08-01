importScripts('../../resources/test-helpers.sub.js');
importScripts('../../resources/worker-testharness.js');

// TODO(nhiroki): stop using global states because service workers can be killed
// at any point. Instead, we could post a message to the page on each event via
// Client object (http://crbug.com/558244).
var events_seen = [];

// TODO(nhiroki): Move these assertions to registration-attribute.html because
// an assertion failure on the worker is not shown on the result page and
// handled as timeout. See registration-attribute-newer-worker.js for example.

assert_equals(
  self.registration.scope,
  normalizeURL('scope/registration-attribute'),
  'On worker script evaluation, registration attribute should be set');
assert_equals(
  self.registration.installing,
  null,
  'On worker script evaluation, installing worker should be null');
assert_equals(
  self.registration.waiting,
  null,
  'On worker script evaluation, waiting worker should be null');
assert_equals(
  self.registration.active,
  null,
  'On worker script evaluation, active worker should be null');

self.registration.addEventListener('updatefound', function() {
    events_seen.push('updatefound');

    assert_equals(
      self.registration.scope,
      normalizeURL('scope/registration-attribute'),
      'On updatefound event, registration attribute should be set');
    assert_equals(
      self.registration.installing.scriptURL,
      normalizeURL('registration-attribute-worker.js'),
      'On updatefound event, installing worker should be set');
    assert_equals(
      self.registration.waiting,
      null,
      'On updatefound event, waiting worker should be null');
    assert_equals(
      self.registration.active,
      null,
      'On updatefound event, active worker should be null');

    assert_equals(
      self.registration.installing.state,
      'installing',
      'On updatefound event, worker should be in the installing state');

    var worker = self.registration.installing;
    self.registration.installing.addEventListener('statechange', function() {
        events_seen.push('statechange(' + worker.state + ')');
      });
  });

self.addEventListener('install', function(e) {
    events_seen.push('install');

    assert_equals(
      self.registration.scope,
      normalizeURL('scope/registration-attribute'),
      'On install event, registration attribute should be set');
    assert_equals(
      self.registration.installing.scriptURL,
      normalizeURL('registration-attribute-worker.js'),
      'On install event, installing worker should be set');
    assert_equals(
      self.registration.waiting,
      null,
      'On install event, waiting worker should be null');
    assert_equals(
      self.registration.active,
      null,
      'On install event, active worker should be null');

    assert_equals(
      self.registration.installing.state,
      'installing',
      'On install event, worker should be in the installing state');
  });

self.addEventListener('activate', function(e) {
    events_seen.push('activate');

    assert_equals(
      self.registration.scope,
      normalizeURL('scope/registration-attribute'),
      'On activate event, registration attribute should be set');
    assert_equals(
      self.registration.installing,
      null,
      'On activate event, installing worker should be null');
    assert_equals(
      self.registration.waiting,
      null,
      'On activate event, waiting worker should be null');
    assert_equals(
      self.registration.active.scriptURL,
      normalizeURL('registration-attribute-worker.js'),
      'On activate event, active worker should be set');

    assert_equals(
      self.registration.active.state,
      'activating',
      'On activate event, worker should be in the activating state');
  });

self.addEventListener('fetch', function(e) {
    events_seen.push('fetch');

    assert_equals(
      self.registration.scope,
      normalizeURL('scope/registration-attribute'),
      'On fetch event, registration attribute should be set');
    assert_equals(
      self.registration.installing,
      null,
      'On fetch event, installing worker should be null');
    assert_equals(
      self.registration.waiting,
      null,
      'On fetch event, waiting worker should be null');
    assert_equals(
      self.registration.active.scriptURL,
      normalizeURL('registration-attribute-worker.js'),
      'On fetch event, active worker should be set');

    assert_equals(
      self.registration.active.state,
      'activated',
      'On fetch event, worker should be in the activated state');

    e.respondWith(new Response(events_seen));
  });
