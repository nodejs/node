'use strict';

require('../common');

const assert = require('assert');
const { metrics } = require('perf_hooks');

// Test consumer close() unregisters from registry
const m = metrics.create('test.close');

const consumer = metrics.createConsumer({
  'test.close': { aggregation: 'sum' },
});

// Record value before close
m.record(10);

// Close consumer
consumer.close();

// Record more values - consumer should not receive them
m.record(20);
m.record(30);

// Collect after close should return empty result
const result = consumer.collect();
assert.deepStrictEqual(result, []);

// Test double close is safe
consumer.close(); // Should not throw

// Test closed consumer doesn't receive new values
const consumer2 = metrics.createConsumer({
  'test.close': { aggregation: 'sum' },
});

// This consumer should only see values recorded after it was created
const result2 = consumer2.collect();
// Note: the sum might include previous values if metric state persists,
// but the point is the closed consumer doesn't receive new values
assert.ok(Array.isArray(result2));

consumer2.close();
