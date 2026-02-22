'use strict';
require('../common');
const assert = require('node:assert');
const dc = require('node:diagnostics_channel');

// Test WindowChannel exports
{
  assert.strictEqual(typeof dc.windowChannel, 'function');
  assert.strictEqual(typeof dc.WindowChannel, 'function');

  const wc = dc.windowChannel('test-export');
  assert.ok(wc instanceof dc.WindowChannel);
}

// Test basic WindowChannel creation and properties
{
  const windowChannel = dc.windowChannel('test-window-basic');

  assert.ok(windowChannel.start);
  assert.ok(windowChannel.end);

  assert.strictEqual(windowChannel.start.name, 'tracing:test-window-basic:start');
  assert.strictEqual(windowChannel.end.name, 'tracing:test-window-basic:end');

  assert.strictEqual(windowChannel.hasSubscribers, false);

  assert.strictEqual(typeof windowChannel.subscribe, 'function');
  assert.strictEqual(typeof windowChannel.unsubscribe, 'function');
  assert.strictEqual(typeof windowChannel.run, 'function');
  assert.strictEqual(typeof windowChannel.withScope, 'function');
}

// Test WindowChannel with channel objects
{
  const startChannel = dc.channel('custom:start');
  const endChannel = dc.channel('custom:end');

  const windowChannel = dc.windowChannel({
    start: startChannel,
    end: endChannel,
  });

  assert.strictEqual(windowChannel.start, startChannel);
  assert.strictEqual(windowChannel.end, endChannel);
}

// Test subscribe/unsubscribe
{
  const windowChannel = dc.windowChannel('test-window-subscribe');
  const events = [];

  const handlers = {
    start(message) {
      events.push({ type: 'start', message });
    },
    end(message) {
      events.push({ type: 'end', message });
    },
  };

  assert.strictEqual(windowChannel.hasSubscribers, false);

  windowChannel.subscribe(handlers);

  assert.strictEqual(windowChannel.hasSubscribers, true);

  // Test that events are received
  windowChannel.start.publish({ test: 'start' });
  windowChannel.end.publish({ test: 'end' });

  assert.strictEqual(events.length, 2);
  assert.strictEqual(events[0].type, 'start');
  assert.strictEqual(events[0].message.test, 'start');
  assert.strictEqual(events[1].type, 'end');
  assert.strictEqual(events[1].message.test, 'end');

  // Test unsubscribe
  const result = windowChannel.unsubscribe(handlers);
  assert.strictEqual(result, true);
  assert.strictEqual(windowChannel.hasSubscribers, false);

  // Test unsubscribe when not subscribed
  const result2 = windowChannel.unsubscribe(handlers);
  assert.strictEqual(result2, false);
}

// Test partial subscription
{
  const windowChannel = dc.windowChannel('test-window-partial');
  const events = [];

  windowChannel.subscribe({
    start(message) {
      events.push('start');
    },
  });

  assert.strictEqual(windowChannel.hasSubscribers, true);

  windowChannel.start.publish({});
  windowChannel.end.publish({});

  assert.strictEqual(events.length, 1);
  assert.strictEqual(events[0], 'start');
}
