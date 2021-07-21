// Flags: --expose-internals
'use strict';

const common = require('../common');

// Tests the functionality of the quic.EndpointConfig object, ensuring
// that validation of all of the properties is as expected.

if (!common.hasQuic)
  common.skip('quic support is not enabled');

const assert = require('assert');

const { Event } = require('internal/event_target');

const {
  DatagramEvent,
  SessionEvent,
  StreamEvent,
  createDatagramEvent,
  createSessionEvent,
  createStreamEvent,
} = require('internal/quic/common');

const {
  inspect,
} = require('util');

assert.throws(() => new DatagramEvent(), {
  code: 'ERR_ILLEGAL_CONSTRUCTOR',
});

assert.throws(() => new SessionEvent(), {
  code: 'ERR_ILLEGAL_CONSTRUCTOR',
});

assert.throws(() => new StreamEvent(), {
  code: 'ERR_ILLEGAL_CONSTRUCTOR',
});

{
  // The values are not validated so although these are incorrect
  // in reality, they're ok to use here.
  const session = {};
  const event = createDatagramEvent('a', false, session);
  assert('datagram' in event);
  assert('early' in event);
  assert('session' in event);
  assert.strictEqual(event.datagram, 'a');
  assert.strictEqual(event.early, false);
  assert.strictEqual(event.session, session);
  assert.match(inspect(event), /DatagramEvent {/);
  assert(event instanceof Event);
}

{
  const session = {};
  const event = new createSessionEvent(session);
  assert('session' in event);
  assert.strictEqual(event.session, session);
  assert.match(inspect(event), /SessionEvent {/);
  assert(event instanceof Event);
}

{
  const stream = { unidirectional: false };
  const event = createStreamEvent(stream);
  assert('stream' in event);
  assert('respondWith' in event);
  assert.strictEqual(event.stream, stream);
  assert.strictEqual(typeof event.respondWith, 'function');
  assert.match(inspect(event), /StreamEvent {/);
  assert(event instanceof Event);
}

{
  const stream = { unidirectional: true };
  const event = createStreamEvent(stream);
  assert('stream' in event);
  assert('respondWith' in event);
  assert.strictEqual(event.stream, stream);
  assert.strictEqual(event.respondWith, undefined);
  assert.match(inspect(event), /StreamEvent {/);
  assert(event instanceof Event);
}

assert.throws(
  () => Reflect.get(DatagramEvent.prototype, 'datagram', {}), {
    code: 'ERR_INVALID_THIS',
  });

assert.throws(
  () => Reflect.get(DatagramEvent.prototype, 'early', {}), {
    code: 'ERR_INVALID_THIS',
  });

assert.throws(
  () => Reflect.get(DatagramEvent.prototype, 'session', {}), {
    code: 'ERR_INVALID_THIS',
  });

assert.throws(
  () => Reflect.get(SessionEvent.prototype, 'session', {}), {
    code: 'ERR_INVALID_THIS',
  });

assert.throws(
  () => Reflect.get(StreamEvent.prototype, 'stream', {}), {
    code: 'ERR_INVALID_THIS',
  });

assert.throws(
  () => Reflect.get(StreamEvent.prototype, 'respondWith', {}), {
    code: 'ERR_INVALID_THIS',
  });
