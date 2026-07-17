'use strict';
require('../common');
const assert = require('node:assert');
const dc = require('node:diagnostics_channel');

// Test BoundedChannel exports
{
  assert.strictEqual(typeof dc.boundedChannel, 'function');
  assert.strictEqual(typeof dc.BoundedChannel, 'function');

  const wc = dc.boundedChannel('test-export');
  assert.ok(wc instanceof dc.BoundedChannel);
}

// Test basic BoundedChannel creation and properties
{
  const boundedChannel = dc.boundedChannel('test-window-basic');

  assert.ok(boundedChannel.start);
  assert.ok(boundedChannel.end);

  assert.strictEqual(boundedChannel.start.name, 'tracing:test-window-basic:start');
  assert.strictEqual(boundedChannel.end.name, 'tracing:test-window-basic:end');

  assert.strictEqual(boundedChannel.hasSubscribers, false);

  assert.strictEqual(typeof boundedChannel.subscribe, 'function');
  assert.strictEqual(typeof boundedChannel.unsubscribe, 'function');
  assert.strictEqual(typeof boundedChannel.run, 'function');
  assert.strictEqual(typeof boundedChannel.withScope, 'function');
}

// Test BoundedChannel with channel objects
{
  const startChannel = dc.channel('custom:start');
  const endChannel = dc.channel('custom:end');

  const boundedChannel = dc.boundedChannel({
    start: startChannel,
    end: endChannel,
  });

  assert.strictEqual(boundedChannel.start, startChannel);
  assert.strictEqual(boundedChannel.end, endChannel);
}

// Test subscribe/unsubscribe
{
  const boundedChannel = dc.boundedChannel('test-window-subscribe');
  const events = [];

  const handlers = {
    start(message) {
      events.push({ type: 'start', message });
    },
    end(message) {
      events.push({ type: 'end', message });
    },
  };

  assert.strictEqual(boundedChannel.hasSubscribers, false);

  boundedChannel.subscribe(handlers);

  assert.strictEqual(boundedChannel.hasSubscribers, true);

  // Test that events are received
  boundedChannel.start.publish({ test: 'start' });
  boundedChannel.end.publish({ test: 'end' });

  assert.strictEqual(events.length, 2);
  assert.strictEqual(events[0].type, 'start');
  assert.strictEqual(events[0].message.test, 'start');
  assert.strictEqual(events[1].type, 'end');
  assert.strictEqual(events[1].message.test, 'end');

  // Test unsubscribe
  const result = boundedChannel.unsubscribe(handlers);
  assert.strictEqual(result, true);
  assert.strictEqual(boundedChannel.hasSubscribers, false);

  // Test unsubscribe when not subscribed
  const result2 = boundedChannel.unsubscribe(handlers);
  assert.strictEqual(result2, false);
}

// Test partial subscription
{
  const boundedChannel = dc.boundedChannel('test-window-partial');
  const events = [];

  boundedChannel.subscribe({
    start(message) {
      events.push('start');
    },
  });

  assert.strictEqual(boundedChannel.hasSubscribers, true);

  boundedChannel.start.publish({});
  boundedChannel.end.publish({});

  assert.strictEqual(events.length, 1);
  assert.strictEqual(events[0], 'start');
}
