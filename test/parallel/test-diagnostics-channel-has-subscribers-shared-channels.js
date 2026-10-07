'use strict';

// Checks that TracingChannel and BoundedChannel report hasSubscribers
// correctly however their channels get or lose subscribers.

require('../common');
const assert = require('node:assert');
const { AsyncLocalStorage } = require('node:async_hooks');
const dc = require('node:diagnostics_channel');

function noop() {}

{
  // Each event channel counts, by itself.
  const tracing = dc.tracingChannel('test-each');
  for (const name of ['start', 'end', 'asyncStart', 'asyncEnd', 'error']) {
    assert.strictEqual(tracing.hasSubscribers, false);
    tracing[name].subscribe(noop);
    assert.strictEqual(tracing.hasSubscribers, true, name);
    tracing[name].unsubscribe(noop);
    assert.strictEqual(tracing.hasSubscribers, false, name);
  }
}

{
  // Subscribers added by channel name, before and after the
  // TracingChannel exists.
  dc.subscribe('tracing:test-name:end', noop);
  const tracing = dc.tracingChannel('test-name');
  assert.strictEqual(tracing.hasSubscribers, true);
  dc.subscribe('tracing:test-name:error', noop);
  dc.unsubscribe('tracing:test-name:end', noop);
  assert.strictEqual(tracing.hasSubscribers, true);
  dc.unsubscribe('tracing:test-name:error', noop);
  assert.strictEqual(tracing.hasSubscribers, false);
}

{
  // A bound store makes a channel active.
  const tracing = dc.tracingChannel('test-store');
  const store = new AsyncLocalStorage();
  tracing.start.bindStore(store);
  assert.strictEqual(tracing.hasSubscribers, true);
  tracing.start.unbindStore(store);
  assert.strictEqual(tracing.hasSubscribers, false);
}

{
  // Several subscribers on one channel, and TracingChannel objects made
  // for the same name, all see the same state.
  const first = dc.tracingChannel('test-same');
  const second = dc.tracingChannel('test-same');
  const handlers = { start: noop, asyncEnd: noop };
  first.subscribe(handlers);
  second.start.subscribe(noop);
  assert.strictEqual(first.hasSubscribers, true);
  assert.strictEqual(second.hasSubscribers, true);
  first.unsubscribe(handlers);
  assert.strictEqual(first.hasSubscribers, true);
  second.start.unsubscribe(noop);
  assert.strictEqual(first.hasSubscribers, false);
  assert.strictEqual(second.hasSubscribers, false);
}

{
  // The object form, with channels shared by other objects and with the
  // same channel used for more than one event.
  const shared = dc.channel('test-shared');
  const other = dc.channel('test-other');
  const tracing = dc.tracingChannel({
    start: shared,
    end: shared,
    asyncStart: other,
    asyncEnd: dc.channel('test-async-end'),
    error: dc.channel('test-error'),
  });
  const bounded = dc.boundedChannel({ start: shared, end: other });
  assert.strictEqual(tracing.hasSubscribers, false);
  assert.strictEqual(bounded.hasSubscribers, false);
  shared.subscribe(noop);
  assert.strictEqual(tracing.hasSubscribers, true);
  assert.strictEqual(bounded.hasSubscribers, true);
  shared.unsubscribe(noop);
  other.subscribe(noop);
  assert.strictEqual(tracing.hasSubscribers, true);
  assert.strictEqual(bounded.hasSubscribers, true);
  other.unsubscribe(noop);
  assert.strictEqual(tracing.hasSubscribers, false);
  assert.strictEqual(bounded.hasSubscribers, false);
}

{
  // A BoundedChannel and a TracingChannel for the same name share their
  // start and end channels.
  const tracing = dc.tracingChannel('test-bounded');
  const bounded = dc.boundedChannel('test-bounded');
  tracing.asyncEnd.subscribe(noop);
  assert.strictEqual(tracing.hasSubscribers, true);
  assert.strictEqual(bounded.hasSubscribers, false);
  bounded.end.subscribe(noop);
  tracing.asyncEnd.unsubscribe(noop);
  assert.strictEqual(tracing.hasSubscribers, true);
  assert.strictEqual(bounded.hasSubscribers, true);
  bounded.end.unsubscribe(noop);
  assert.strictEqual(tracing.hasSubscribers, false);
  assert.strictEqual(bounded.hasSubscribers, false);
}
