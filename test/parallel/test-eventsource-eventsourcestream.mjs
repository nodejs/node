// Flags: --expose-internals
import {
  mustCall,
  mustNotCall,
} from '../common/index.mjs';

import assert from 'assert';
import eventSource from 'internal/event_source';

const EventSourceStream = eventSource.EventSourceStream;

{
  // Remove BOM from the beginning of the stream.
  const content = Buffer.from('\uFEFFdata: Hello\n\n', 'utf8');

  const stream = new EventSourceStream();

  stream.processEvent = mustCall(function(event) {
    assert.strictEqual(typeof event, 'object');
    assert.strictEqual(event.event, undefined);
    assert.strictEqual(event.data, 'Hello');
    assert.strictEqual(event.id, undefined);
    assert.strictEqual(event.retry, undefined);
  });

  for (let i = 0; i < content.length; i++) {
    stream.write(Buffer.from([content[i]]));
  }
}

{
  // Simple event with data field.
  const content = Buffer.from('data: Hello\n\n', 'utf8');

  const stream = new EventSourceStream();

  stream.processEvent = mustCall(function(event) {
    assert.strictEqual(typeof event, 'object');
    assert.strictEqual(event.event, undefined);
    assert.strictEqual(event.data, 'Hello');
    assert.strictEqual(event.id, undefined);
    assert.strictEqual(event.retry, undefined);
  });

  for (let i = 0; i < content.length; i++) {
    stream.write(Buffer.from([content[i]]));
  }
}

{
  // Ignore comments
  const content = Buffer.from(':data: Hello\n\n', 'utf8');

  const stream = new EventSourceStream();

  stream.processEvent = mustNotCall(function(event) {
    assert.fail('Should not be called');
  });

  for (let i = 0; i < content.length; i++) {
    stream.write(Buffer.from([content[i]]));
  }
}

{
  // Should fire two events.
  // @see https://html.spec.whatwg.org/multipage/server-sent-events.html#event-stream-interpretation
  const content = Buffer.from('data\n\ndata\ndata\n\ndata:', 'utf8');
  const stream = new EventSourceStream();

  stream.processEvent = mustCall(function(event) {
    assert.strictEqual(typeof event, 'object');
    assert.strictEqual(event.event, undefined);
    assert.strictEqual(event.data, undefined);
    assert.strictEqual(event.id, undefined);
    assert.strictEqual(event.retry, undefined);
  }, 2);

  for (let i = 0; i < content.length; i++) {
    stream.write(Buffer.from([content[i]]));
  }
}

{
  // Should fire two identical events.
  // @see https://html.spec.whatwg.org/multipage/server-sent-events.html#event-stream-interpretation
  const content = Buffer.from('data:test\n\ndata: test\n\n', 'utf8');
  const stream = new EventSourceStream();

  stream.processEvent = mustCall(function(event) {
    assert.strictEqual(typeof event, 'object');
    assert.strictEqual(event.event, undefined);
    assert.strictEqual(event.data, 'test');
    assert.strictEqual(event.id, undefined);
    assert.strictEqual(event.retry, undefined);
  }, 2);

  for (let i = 0; i < content.length; i++) {
    stream.write(Buffer.from([content[i]]));
  }
}
