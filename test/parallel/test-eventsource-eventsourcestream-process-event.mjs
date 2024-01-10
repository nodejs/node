// Flags: --expose-internals
import {
  mustCall,
} from '../common/index.mjs';

import assert from 'assert';
import eventSource from 'internal/event_source';

const MessageEvent = eventSource.MessageEvent;
const EventSourceStream = eventSource.EventSourceStream;

const defaultEventSourceState = {
  origin: 'example.com',
  reconnectionTime: 1000,
};

{
  // Should set the defined origin as the origin of the MessageEvent
  const stream = new EventSourceStream({
    eventSourceState: {
      ...defaultEventSourceState
    }
  });

  stream.on('data', mustCall((event) => {
    assert.strictEqual(event instanceof MessageEvent, true);
    assert.strictEqual(event.data, null);
    assert.strictEqual(event.lastEventId, '');
    assert.strictEqual(event.type, 'message');
    assert.strictEqual(stream.state.reconnectionTime, 1000);
    assert.strictEqual(event.origin, 'example.com');
  }));

  stream.processEvent({});
}

{
  // Should set reconnectionTime to 4000 if event contains retry field
  const stream = new EventSourceStream({
    eventSourceState: {
      ...defaultEventSourceState
    }
  });

  stream.processEvent({
    retry: '4000',
  });

  assert.strictEqual(stream.state.reconnectionTime, 4000);
}

{
  // Dispatches a MessageEvent with data
  const stream = new EventSourceStream({
    eventSourceState: {
      ...defaultEventSourceState
    }
  });

  stream.on('data', mustCall((event) => {
    assert.strictEqual(event instanceof MessageEvent, true);
    assert.strictEqual(event.data, 'Hello');
    assert.strictEqual(event.lastEventId, '');
    assert.strictEqual(event.type, 'message');
    assert.strictEqual(event.origin, 'example.com');
    assert.strictEqual(stream.state.reconnectionTime, 1000);
  }));

  stream.processEvent({
    data: 'Hello',
  });
}

{
  // Dispatches a MessageEvent with lastEventId, when event contains id field
  const stream = new EventSourceStream({
    eventSourceState: {
      ...defaultEventSourceState,
    }
  });

  stream.on('data', mustCall((event) => {
    assert.strictEqual(event instanceof MessageEvent, true);
    assert.strictEqual(event.data, null);
    assert.strictEqual(event.lastEventId, '1234');
    assert.strictEqual(event.type, 'message');
    assert.strictEqual(event.origin, 'example.com');
    assert.strictEqual(stream.state.reconnectionTime, 1000);
  }));

  stream.processEvent({
    id: '1234',
  });
}

{
  // Dispatches a MessageEvent with lastEventId, reusing the persisted
  // lastEventId
  const stream = new EventSourceStream({
    eventSourceState: {
      ...defaultEventSourceState,
      lastEventId: '1234',
    }
  });

  stream.on('data', mustCall((event) => {
    assert.strictEqual(event instanceof MessageEvent, true);
    assert.strictEqual(event.data, null);
    assert.strictEqual(event.lastEventId, '1234');
    assert.strictEqual(event.type, 'message');
    assert.strictEqual(event.origin, 'example.com');
    assert.strictEqual(stream.state.reconnectionTime, 1000);
  }));

  stream.processEvent({});
}

{
  // Dispatches a MessageEvent with type custom, when event contains type field
  const stream = new EventSourceStream({
    eventSourceState: {
      ...defaultEventSourceState,
    }
  });

  stream.on('data', mustCall((event) => {
    assert.strictEqual(event instanceof MessageEvent, true);
    assert.strictEqual(event.data, null);
    assert.strictEqual(event.lastEventId, '');
    assert.strictEqual(event.type, 'custom');
    assert.strictEqual(event.origin, 'example.com');
    assert.strictEqual(stream.state.reconnectionTime, 1000);
  }));

  stream.processEvent({
    event: 'custom',
  });
}
