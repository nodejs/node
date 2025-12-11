'use strict';

const common = require('../common');

const assert = require('assert');
const dc = require('diagnostics_channel');
const { metrics } = require('perf_hooks');

// Test DiagnosticsChannel consumer
const m = metrics.create('test.dc', { unit: 'count' });

const received = [];
dc.subscribe('metrics:test.dc', common.mustCall((msg) => {
  received.push(msg);
}, 3));

// Create DC consumer
const dcConsumer = metrics.createDiagnosticsChannelConsumer();

// Record values
m.record(42, { tag: 'a' });
m.record(100, { tag: 'b' });
m.record(7);

// Verify DC received the values
assert.strictEqual(received.length, 3);

assert.strictEqual(received[0].value, 42);
assert.deepStrictEqual(received[0].attributes, { tag: 'a' });
assert.strictEqual(received[0].descriptor.name, 'test.dc');

assert.strictEqual(received[1].value, 100);
assert.deepStrictEqual(received[1].attributes, { tag: 'b' });

assert.strictEqual(received[2].value, 7);
assert.strictEqual(Object.keys(received[2].attributes).length, 0);

// Test DC consumer is singleton
const dcConsumer2 = metrics.createDiagnosticsChannelConsumer();
assert.strictEqual(dcConsumer, dcConsumer2);

// Close DC consumer
dcConsumer.close();

// Test DC consumer with observable metrics
let gaugeValue = 50;
metrics.create('test.dc.observable', {
  observable: (metric) => { metric.record(gaugeValue); },
});

const received2 = [];
dc.subscribe('metrics:test.dc.observable', (msg) => {
  received2.push(msg);
});

// Create new DC consumer
const dcConsumer3 = metrics.createDiagnosticsChannelConsumer();

// Observables are sampled on collect
dcConsumer3.collect();

assert.strictEqual(received2.length, 1);
assert.strictEqual(received2[0].value, 50);

gaugeValue = 75;
dcConsumer3.collect();

assert.strictEqual(received2.length, 2);
assert.strictEqual(received2[1].value, 75);

dcConsumer3.close();
