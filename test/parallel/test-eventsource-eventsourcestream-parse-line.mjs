// Flags: --expose-internals
import '../common/index.mjs';

import assert from 'assert';
import eventSource from 'internal/event_source';

const EventSourceStream = eventSource.EventSourceStream;

const defaultEventSourceState = {
  origin: 'example.com',
  reconnectionTime: 1000,
};

{
  // Should set the data field
  const stream = new EventSourceStream({
    eventSourceState: {
      ...defaultEventSourceState
    }
  });

  const event = {};

  stream.parseLine(Buffer.from('data: Hello', 'utf8'), event);

  assert.strictEqual(typeof event, 'object');
  assert.strictEqual(Object.keys(event).length, 1);
  assert.strictEqual(event.data, 'Hello');
  assert.strictEqual(event.id, undefined);
  assert.strictEqual(event.event, undefined);
  assert.strictEqual(event.retry, undefined);
}

{
  // Should set retry field
  const stream = new EventSourceStream({
    eventSourceState: {
      ...defaultEventSourceState
    }
  });

  const event = {};

  stream.parseLine(Buffer.from('retry: 1000', 'utf8'), event);

  assert.strictEqual(typeof event, 'object');
  assert.strictEqual(Object.keys(event).length, 1);
  assert.strictEqual(event.data, undefined);
  assert.strictEqual(event.id, undefined);
  assert.strictEqual(event.event, undefined);
  assert.strictEqual(event.retry, '1000');
}

{
  // Should set id field
  const stream = new EventSourceStream({
    eventSourceState: {
      ...defaultEventSourceState
    }
  });

  const event = {};

  stream.parseLine(Buffer.from('id: 1234', 'utf8'), event);

  assert.strictEqual(typeof event, 'object');
  assert.strictEqual(Object.keys(event).length, 1);
  assert.strictEqual(event.data, undefined);
  assert.strictEqual(event.id, '1234');
  assert.strictEqual(event.event, undefined);
  assert.strictEqual(event.retry, undefined);
}

{
  // Should set id field
  const stream = new EventSourceStream({
    eventSourceState: {
      ...defaultEventSourceState
    }
  });

  const event = {};

  stream.parseLine(Buffer.from('event: custom', 'utf8'), event);

  assert.strictEqual(typeof event, 'object');
  assert.strictEqual(Object.keys(event).length, 1);
  assert.strictEqual(event.data, undefined);
  assert.strictEqual(event.id, undefined);
  assert.strictEqual(event.event, 'custom');
  assert.strictEqual(event.retry, undefined);
}
