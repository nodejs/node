'use strict';

require('../common');

const assert = require('assert');
const { metrics } = require('perf_hooks');

// Test delta temporality includes startTime and timestamp
const m = metrics.create('test.delta.timestamps');

const consumer = metrics.createConsumer({
  'test.delta.timestamps': { aggregation: 'sum', temporality: 'delta' },
});

// Record some values
m.record(10);
m.record(20);

// First collect
const result1 = consumer.collect();
assert.strictEqual(result1.length, 1);
assert.strictEqual(result1[0].temporality, 'delta');
assert.ok(typeof result1[0].startTime === 'number', 'startTime should be a number');
assert.ok(typeof result1[0].timestamp === 'number', 'timestamp should be a number');
assert.ok(result1[0].startTime < result1[0].timestamp, 'startTime should be before timestamp');
assert.strictEqual(result1[0].dataPoints[0].sum, 30);

const firstEndTime = result1[0].timestamp;

// Record more values
m.record(5);

// Second collect
const result2 = consumer.collect();
assert.strictEqual(result2.length, 1);
// startTime of second collect should equal timestamp of first collect
assert.strictEqual(result2[0].startTime, firstEndTime);
assert.ok(result2[0].timestamp > result2[0].startTime);
assert.strictEqual(result2[0].dataPoints[0].sum, 5);

consumer.close();

// Test cumulative temporality does NOT include startTime
const m2 = metrics.create('test.cumulative.timestamps');

const consumer2 = metrics.createConsumer({
  'test.cumulative.timestamps': { aggregation: 'sum', temporality: 'cumulative' },
});

m2.record(10);

const result3 = consumer2.collect();
assert.strictEqual(result3[0].temporality, 'cumulative');
assert.ok(typeof result3[0].timestamp === 'number');
// Cumulative temporality should not have startTime
assert.strictEqual(result3[0].startTime, undefined);

consumer2.close();
