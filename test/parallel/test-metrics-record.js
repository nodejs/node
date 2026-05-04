'use strict';

require('../common');

const assert = require('assert');
const { metrics } = require('perf_hooks');

// Test recording values dispatches to consumers
const m = metrics.create('test.record', { unit: 'count' });

const consumer = metrics.createConsumer({
  'test.record': { aggregation: 'sum' },
});

// Record values
m.record(10);
m.record(20);
m.record(30);

// Collect and verify
const result = consumer.collect();
assert.strictEqual(result.length, 1);
assert.strictEqual(result[0].descriptor.name, 'test.record');
assert.strictEqual(result[0].dataPoints.length, 1);
assert.strictEqual(result[0].dataPoints[0].sum, 60);
assert.strictEqual(result[0].dataPoints[0].count, 3);

consumer.close();

// Test recording with attributes (groupByAttributes: true enables attribute differentiation)
const m2 = metrics.create('test.record.attrs');
const consumer2 = metrics.createConsumer({
  'groupByAttributes': true,
  'test.record.attrs': { aggregation: 'sum' },
});

m2.record(1, { method: 'GET' });
m2.record(2, { method: 'POST' });
m2.record(3, { method: 'GET' });

const result2 = consumer2.collect();
assert.strictEqual(result2[0].dataPoints.length, 2);

// Find GET and POST data points
const getData = result2[0].dataPoints.find((dp) => dp.attributes.method === 'GET');
const postData = result2[0].dataPoints.find((dp) => dp.attributes.method === 'POST');

assert.strictEqual(getData.sum, 4); // 1 + 3
assert.strictEqual(getData.count, 2);
assert.strictEqual(postData.sum, 2);
assert.strictEqual(postData.count, 1);

consumer2.close();

// Test validation
assert.throws(() => m.record('not a number'), {
  code: 'ERR_INVALID_ARG_TYPE',
});
