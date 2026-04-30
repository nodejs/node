'use strict';

require('../common');

const assert = require('assert');
const { metrics } = require('perf_hooks');

// Test lastValue aggregator
const m = metrics.create('test.lastvalue');

const consumer = metrics.createConsumer({
  'test.lastvalue': { aggregation: 'lastValue' },
});

m.record(10);
let result = consumer.collect();
assert.strictEqual(result[0].dataPoints[0].value, 10);
assert.ok(result[0].dataPoints[0].timestamp > 0);

m.record(42);
result = consumer.collect();
assert.strictEqual(result[0].dataPoints[0].value, 42);

m.record(0);
result = consumer.collect();
assert.strictEqual(result[0].dataPoints[0].value, 0);

consumer.close();

// Test lastValue with observable
let gaugeValue = 100;
metrics.create('test.lastvalue.observable', {
  observable: (metric) => { metric.record(gaugeValue); },
});

const consumer2 = metrics.createConsumer({
  'test.lastvalue.observable': { aggregation: 'lastValue' },
});

result = consumer2.collect();
assert.strictEqual(result[0].dataPoints[0].value, 100);

gaugeValue = 200;
result = consumer2.collect();
assert.strictEqual(result[0].dataPoints[0].value, 200);

consumer2.close();
