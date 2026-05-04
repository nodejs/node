'use strict';

require('../common');

const assert = require('assert');
const { metrics } = require('perf_hooks');

// Test bigint value support
const m = metrics.create('test.bigint');

const consumer = metrics.createConsumer({
  'test.bigint': { aggregation: 'sum' },
});

// Record bigint values
m.record(BigInt(Number.MAX_SAFE_INTEGER));
m.record(BigInt(Number.MAX_SAFE_INTEGER));
m.record(1n);

const result = consumer.collect();
const expected = BigInt(Number.MAX_SAFE_INTEGER) * 2n + 1n;
assert.strictEqual(result[0].dataPoints[0].sum, expected);
assert.strictEqual(result[0].dataPoints[0].count, 3);

consumer.close();

// Test mixed number and bigint (bigint converts sum to bigint)
const m2 = metrics.create('test.mixed');

const consumer2 = metrics.createConsumer({
  'test.mixed': { aggregation: 'sum' },
});

m2.record(10);
m2.record(20n);
m2.record(30);

const result2 = consumer2.collect();
assert.strictEqual(result2[0].dataPoints[0].sum, 60n);

consumer2.close();
