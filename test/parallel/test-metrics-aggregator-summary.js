'use strict';

require('../common');

const assert = require('assert');
const { metrics } = require('perf_hooks');

// Test summary aggregator with custom quantiles
const m = metrics.create('test.summary', { unit: 'ms' });

const consumer = metrics.createConsumer({
  'test.summary': {
    aggregation: 'summary',
    quantiles: [0.5, 0.9, 0.99],
  },
});

// Record 100 values (1 to 100)
for (let i = 1; i <= 100; i++) {
  m.record(i);
}

const result = consumer.collect();
const summary = result[0].dataPoints[0];

assert.strictEqual(summary.count, 100);
assert.strictEqual(summary.sum, 5050); // Sum of 1 to 100
assert.strictEqual(summary.min, 1);
assert.strictEqual(summary.max, 100);

// Check quantiles (approximate values due to histogram-based calculation)
assert.ok(summary.quantiles['0.5'] >= 45 && summary.quantiles['0.5'] <= 55);
assert.ok(summary.quantiles['0.9'] >= 85 && summary.quantiles['0.9'] <= 95);
assert.ok(summary.quantiles['0.99'] >= 95 && summary.quantiles['0.99'] <= 100);

consumer.close();

// Test summary with default quantiles
const m2 = metrics.create('test.summary.default');

const consumer2 = metrics.createConsumer({
  'test.summary.default': { aggregation: 'summary' },
});

for (let i = 1; i <= 50; i++) {
  m2.record(i);
}

const result2 = consumer2.collect();
const summary2 = result2[0].dataPoints[0];

// Default quantiles should include 0.5, 0.9, 0.95, 0.99
assert.ok('0.5' in summary2.quantiles);
assert.ok('0.9' in summary2.quantiles);
assert.ok('0.95' in summary2.quantiles);
assert.ok('0.99' in summary2.quantiles);

consumer2.close();
