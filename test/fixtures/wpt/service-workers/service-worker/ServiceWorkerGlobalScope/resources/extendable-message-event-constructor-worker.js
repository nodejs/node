importScripts('/resources/testharness.js');

const TEST_OBJECT = { wanwan: 123 };
const CHANNEL1 = new MessageChannel();
const CHANNEL2 = new MessageChannel();
const PORTS = [CHANNEL1.port1, CHANNEL1.port2, CHANNEL2.port1];
function createEvent(initializer) {
  if (initializer === undefined)
    return new ExtendableMessageEvent('type');
  return new ExtendableMessageEvent('type', initializer);
}

// These test cases are mostly copied from the following file in the Chromium
// project (as of commit 848ad70823991e0f12b437d789943a4ab24d65bb):
// third_party/WebKit/LayoutTests/fast/events/constructors/message-event-constructor.html

test(function() {
  assert_false(createEvent().bubbles);
  assert_false(createEvent().cancelable);
  assert_equals(createEvent().data, null);
  assert_equals(createEvent().origin, '');
  assert_equals(createEvent().lastEventId, '');
  assert_equals(createEvent().source, null);
  assert_array_equals(createEvent().ports, []);
}, 'no initializer specified');

test(function() {
  assert_false(createEvent({ bubbles: false }).bubbles);
  assert_true(createEvent({ bubbles: true }).bubbles);
}, '`bubbles` is specified');

test(function() {
  assert_false(createEvent({ cancelable: false }).cancelable);
  assert_true(createEvent({ cancelable: true }).cancelable);
}, '`cancelable` is specified');

test(function() {
  assert_equals(createEvent({ data: TEST_OBJECT }).data, TEST_OBJECT);
  assert_equals(createEvent({ data: undefined }).data, null);
  assert_equals(createEvent({ data: null }).data, null);
  assert_equals(createEvent({ data: false }).data, false);
  assert_equals(createEvent({ data: true }).data, true);
  assert_equals(createEvent({ data: '' }).data, '');
  assert_equals(createEvent({ data: 'chocolate' }).data, 'chocolate');
  assert_equals(createEvent({ data: 12345 }).data, 12345);
  assert_equals(createEvent({ data: 18446744073709551615 }).data,
                            18446744073709552000);
  assert_equals(createEvent({ data: NaN }).data, NaN);
  // Note that valueOf() is not called, when the left hand side is
  // evaluated.
  assert_false(
      createEvent({ data: {
          valueOf: function() { return TEST_OBJECT; } } }).data ==
      TEST_OBJECT);
  assert_equals(createEvent({ get data(){ return 123; } }).data, 123);
  let thrown = { name: 'Error' };
  assert_throws_exactly(thrown, function() {
      createEvent({ get data() { throw thrown; } }); });
}, '`data` is specified');

test(function() {
  assert_equals(createEvent({ origin: 'melancholy' }).origin, 'melancholy');
  assert_equals(createEvent({ origin: '' }).origin, '');
  assert_equals(createEvent({ origin: null }).origin, 'null');
  assert_equals(createEvent({ origin: false }).origin, 'false');
  assert_equals(createEvent({ origin: true }).origin, 'true');
  assert_equals(createEvent({ origin: 12345 }).origin, '12345');
  assert_equals(
      createEvent({ origin: 18446744073709551615 }).origin,
      '18446744073709552000');
  assert_equals(createEvent({ origin: NaN }).origin, 'NaN');
  assert_equals(createEvent({ origin: [] }).origin, '');
  assert_equals(createEvent({ origin: [1, 2, 3] }).origin, '1,2,3');
  assert_equals(
      createEvent({ origin: { melancholy: 12345 } }).origin,
      '[object Object]');
  // Note that valueOf() is not called, when the left hand side is
  // evaluated.
  assert_equals(
      createEvent({ origin: {
          valueOf: function() { return 'melancholy'; } } }).origin,
      '[object Object]');
  assert_equals(
      createEvent({ get origin() { return 123; } }).origin, '123');
  let thrown = { name: 'Error' };
  assert_throws_exactly(thrown, function() {
      createEvent({ get origin() { throw thrown; } }); });
}, '`origin` is specified');

test(function() {
  assert_equals(
      createEvent({ lastEventId: 'melancholy' }).lastEventId, 'melancholy');
  assert_equals(createEvent({ lastEventId: '' }).lastEventId, '');
  assert_equals(createEvent({ lastEventId: null }).lastEventId, 'null');
  assert_equals(createEvent({ lastEventId: false }).lastEventId, 'false');
  assert_equals(createEvent({ lastEventId: true }).lastEventId, 'true');
  assert_equals(createEvent({ lastEventId: 12345 }).lastEventId, '12345');
  assert_equals(
      createEvent({ lastEventId: 18446744073709551615 }).lastEventId,
      '18446744073709552000');
  assert_equals(createEvent({ lastEventId: NaN }).lastEventId, 'NaN');
  assert_equals(createEvent({ lastEventId: [] }).lastEventId, '');
  assert_equals(
      createEvent({ lastEventId: [1, 2, 3] }).lastEventId, '1,2,3');
  assert_equals(
      createEvent({ lastEventId: { melancholy: 12345 } }).lastEventId,
      '[object Object]');
  // Note that valueOf() is not called, when the left hand side is
  // evaluated.
  assert_equals(
      createEvent({ lastEventId: {
          valueOf: function() { return 'melancholy'; } } }).lastEventId,
      '[object Object]');
  assert_equals(
      createEvent({ get lastEventId() { return 123; } }).lastEventId,
      '123');
  let thrown = { name: 'Error' };
  assert_throws_exactly(thrown, function() {
      createEvent({ get lastEventId() { throw thrown; } }); });
}, '`lastEventId` is specified');

test(function() {
  assert_equals(createEvent({ source: CHANNEL1.port1 }).source, CHANNEL1.port1);
  assert_equals(
      createEvent({ source: self.registration.active }).source,
      self.registration.active);
  assert_equals(
      createEvent({ source: CHANNEL1.port1 }).source, CHANNEL1.port1);
  assert_throws_js(
      TypeError, function() { createEvent({ source: this }); },
      'source should be Client or ServiceWorker or MessagePort');
}, '`source` is specified');

test(function() {
  // Valid message ports.
  var passed_ports = createEvent({ ports: PORTS}).ports;
  assert_equals(passed_ports[0], CHANNEL1.port1);
  assert_equals(passed_ports[1], CHANNEL1.port2);
  assert_equals(passed_ports[2], CHANNEL2.port1);
  assert_array_equals(createEvent({ ports: [] }).ports, []);
  assert_array_equals(createEvent({ ports: undefined }).ports, []);

  // Invalid message ports.
  assert_throws_js(TypeError,
      function() { createEvent({ ports: [1, 2, 3] }); });
  assert_throws_js(TypeError,
      function() { createEvent({ ports: TEST_OBJECT }); });
  assert_throws_js(TypeError,
      function() { createEvent({ ports: null }); });
  assert_throws_js(TypeError,
      function() { createEvent({ ports: this }); });
  assert_throws_js(TypeError,
      function() { createEvent({ ports: false }); });
  assert_throws_js(TypeError,
      function() { createEvent({ ports: true }); });
  assert_throws_js(TypeError,
      function() { createEvent({ ports: '' }); });
  assert_throws_js(TypeError,
      function() { createEvent({ ports: 'chocolate' }); });
  assert_throws_js(TypeError,
      function() { createEvent({ ports: 12345 }); });
  assert_throws_js(TypeError,
      function() { createEvent({ ports: 18446744073709551615 }); });
  assert_throws_js(TypeError,
      function() { createEvent({ ports: NaN }); });
  assert_throws_js(TypeError,
      function() { createEvent({ get ports() { return 123; } }); });
  let thrown = { name: 'Error' };
  assert_throws_exactly(thrown, function() {
      createEvent({ get ports() { throw thrown; } }); });
  // Note that valueOf() is not called, when the left hand side is
  // evaluated.
  var valueOf = function() { return PORTS; };
  assert_throws_js(TypeError, function() {
      createEvent({ ports: { valueOf: valueOf } }); });
}, '`ports` is specified');

test(function() {
  var initializers = {
      bubbles: true,
      cancelable: true,
      data: TEST_OBJECT,
      origin: 'wonderful',
      lastEventId: 'excellent',
      source: CHANNEL1.port1,
      ports: PORTS
  };
  assert_equals(createEvent(initializers).bubbles, true);
  assert_equals(createEvent(initializers).cancelable, true);
  assert_equals(createEvent(initializers).data, TEST_OBJECT);
  assert_equals(createEvent(initializers).origin, 'wonderful');
  assert_equals(createEvent(initializers).lastEventId, 'excellent');
  assert_equals(createEvent(initializers).source, CHANNEL1.port1);
  assert_equals(createEvent(initializers).ports[0], PORTS[0]);
  assert_equals(createEvent(initializers).ports[1], PORTS[1]);
  assert_equals(createEvent(initializers).ports[2], PORTS[2]);
}, 'all initial values are specified');
