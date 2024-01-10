// Flags: --expose-internals
import '../common/index.mjs';

import assert from 'assert';
import eventTarget from 'internal/event_target';
import eventSource from 'internal/event_source';

const MessageEvent = eventSource.MessageEvent;

{
  // MessageEvent is a subclass of Event
  assert.strictEqual(new MessageEvent() instanceof eventTarget.Event, true);
}

{
  // default values
  assert.strictEqual(new MessageEvent().data, null);
  assert.strictEqual(new MessageEvent().origin, '');
  assert.strictEqual(new MessageEvent().lastEventId, '');
  assert.strictEqual(new MessageEvent().source, null);
  assert.deepStrictEqual(new MessageEvent().ports, []);
}

{
  // Ports is a frozen array
  assert.throws(() => {
    new MessageEvent('message', { ports: [] }).ports.push(null);
  }, TypeError);
}

{
  // The data attribute must return the value it was initialized to. It
  // represents the message being sent.
  assert.strictEqual(new MessageEvent('message', { data: null }).data, null);
  assert.strictEqual(new MessageEvent('message', { data: 1 }).data, 1);
  assert.strictEqual(new MessageEvent('message', { data: 'foo' }).data, 'foo');
  assert.deepStrictEqual(new MessageEvent('message', { data: {} }).data, {});
  assert.deepStrictEqual(new MessageEvent('message', { data: [] }).data, []);
  assert.strictEqual(new MessageEvent('message', { data: true }).data, true);
  assert.strictEqual(new MessageEvent('message', { data: false }).data, false);

  // The data attribute is a read-only attribute.
  assert.throws(() => {
    new MessageEvent('message', { data: null }).data = 1;
  }, TypeError);

  // The data attribute is non-writable
  const event = new MessageEvent('message', { data: 'dataValue' });
  delete event.data;
  assert.strictEqual(event.data, 'dataValue');
}

{
  // The origin attribute must return the value it was initialized to. It
  // represents the origin of the message being sent.

  assert.strictEqual(new MessageEvent('message', { origin: '' }).origin, '');
  assert.strictEqual(new MessageEvent('message', { origin: 'foo' }).origin, 'foo');

  // The origin attribute is a read-only attribute.
  assert.throws(() => {
    new MessageEvent('message', { origin: '' }).origin = 'foo';
  }, TypeError);

  // The origin attribute is non-writable
  const event = new MessageEvent('message', { origin: 'originValue' });
  delete event.origin;
  assert.strictEqual(event.origin, 'originValue');
}

{
  // The source attribute must return the value it was initialized to. It
  // represents the source of the message being sent.

  assert.strictEqual(new MessageEvent('message', { source: null }).source, null);
  assert.strictEqual(new MessageEvent('message', { source: 'foo' }).source, 'foo');

  // The source attribute is a read-only attribute.
  assert.throws(() => {
    new MessageEvent('message', { source: '' }).source = 'foo';
  }, TypeError);

  // The source attribute is non-writable
  const event = new MessageEvent('message', { source: 'sourceValue' });
  delete event.source;
  assert.strictEqual(event.source, 'sourceValue');
}

{
  // The ports attribute must return the value it was initialized to. It
  // represents the ports of the message being sent.

  assert.deepStrictEqual(new MessageEvent('message', { ports: [] }).ports, []);
  const target = new EventTarget();
  assert.deepStrictEqual(new MessageEvent('message', { ports: [target] }).ports, [target]);

  // The ports attribute is a read-only attribute.
  assert.throws(() => {
    new MessageEvent('message', { ports: [] }).ports = [];
  }, TypeError);

  // The ports attribute is non-writable
  const event = new MessageEvent('message', { ports: [target] });
  delete event.ports;
  assert.deepStrictEqual(event.ports, [target]);
}

{
  // The lastEventId attribute must return the value it was initialized to. It
  // represents the last event ID string of the message being sent.

  assert.strictEqual(new MessageEvent('message', { lastEventId: '' }).lastEventId, '');
  assert.strictEqual(new MessageEvent('message', { lastEventId: 'foo' }).lastEventId, 'foo');

  // The lastEventId attribute is a read-only attribute.
  assert.throws(() => {
    new MessageEvent('message', { lastEventId: '' }).lastEventId = 'foo';
  }, TypeError);

  // The lastEventId attribute is non-writable
  const event = new MessageEvent('message', { lastEventId: 'lastIdValue' });
  delete event.lastEventId;
  assert.strictEqual(event.lastEventId, 'lastIdValue');
}

{
  // initMessageEvent initializes the event in a manner analogous to the
  // similarly-named method in the DOM Events interfaces.

  const event = new MessageEvent();
  const eventTarget = new EventTarget();

  event.initMessageEvent('message');
  assert.strictEqual(event.type, 'message');
  assert.strictEqual(event.bubbles, false);
  assert.strictEqual(event.cancelable, false);
  assert.strictEqual(event.data, null);
  assert.strictEqual(event.origin, '');
  assert.strictEqual(event.lastEventId, '');
  assert.strictEqual(event.source, null);
  assert.deepStrictEqual(event.ports, []);

  event.initMessageEvent(
    'message',
    false,
    false,
    'dataValue',
    'originValue',
    'lastIdValue',
    'sourceValue',
    [eventTarget]
  );
  assert.strictEqual(event.type, 'message');
  assert.strictEqual(event.bubbles, false);
  assert.strictEqual(event.cancelable, false);
  assert.strictEqual(event.data, 'dataValue');
  assert.strictEqual(event.origin, 'originValue');
  assert.strictEqual(event.lastEventId, 'lastIdValue');
  assert.strictEqual(event.source, 'sourceValue');
  assert.deepStrictEqual(event.ports, [eventTarget]);

  event.initMessageEvent(
    'message',
    true,
    true,
    'dataValue',
    'originValue',
    'lastIdValue',
    'sourceValue',
    [eventTarget]
  );
  assert.strictEqual(event.bubbles, true);
  assert.strictEqual(event.cancelable, true);

  event.initMessageEvent(
    'message',
    true,
    false,
    'dataValue',
    'originValue',
    'lastIdValue',
    'sourceValue',
    [eventTarget]
  );
  assert.strictEqual(event.bubbles, true);
  assert.strictEqual(event.cancelable, false);

  event.initMessageEvent(
    'message',
    false,
    true,
    'dataValue',
    'originValue',
    'lastIdValue',
    'sourceValue',
    [eventTarget]
  );
  assert.strictEqual(event.bubbles, false);
  assert.strictEqual(event.cancelable, true);
}

{
  // toString returns [object MessageEvent]
  const event = new MessageEvent('message', {
    data: 'dataValue',
    origin: 'originValue',
    lastEventId: 'lastIdValue',
    source: 'sourceValue',
    ports: []
  });

  assert.strictEqual(event.toString(), '[object MessageEvent]');
}
