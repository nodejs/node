'use strict';

require('../common');

const assert = require('assert');
const { metrics } = require('perf_hooks');

// Test custom aggregator with non-standard state properties
// This tests the fix for custom aggregators that don't use 'count' or 'value'
const m = metrics.create('test.custom.aggregator');

const customAggregator = {
  createState() {
    return {
      total: 0,
      items: [],
    };
  },
  aggregate(state, value) {
    state.total += value;
    state.items.push(value);
  },
  finalize(state) {
    return {
      total: state.total,
      itemCount: state.items.length,
      average: state.items.length > 0 ? state.total / state.items.length : 0,
    };
  },
  resetState(state) {
    state.total = 0;
    state.items = [];
  },
};

const consumer = metrics.createConsumer({
  'test.custom.aggregator': {
    aggregation: customAggregator,
    temporality: 'delta',
  },
});

// Before recording, should have no data points
let result = consumer.collect();
assert.strictEqual(result.length, 0);

// Record some values
m.record(10);
m.record(20);
m.record(30);

// Collect and verify custom aggregator worked
result = consumer.collect();
assert.strictEqual(result.length, 1);
assert.strictEqual(result[0].dataPoints[0].total, 60);
assert.strictEqual(result[0].dataPoints[0].itemCount, 3);
assert.strictEqual(result[0].dataPoints[0].average, 20);

// After delta reset, should have no data points again
result = consumer.collect();
assert.strictEqual(result.length, 0);

// Record more and verify reset worked
m.record(5);
result = consumer.collect();
assert.strictEqual(result.length, 1);
assert.strictEqual(result[0].dataPoints[0].total, 5);
assert.strictEqual(result[0].dataPoints[0].itemCount, 1);

consumer.close();

// Test custom aggregator with groupByAttributes
const m2 = metrics.create('test.custom.aggregator.grouped');

const consumer2 = metrics.createConsumer({
  'groupByAttributes': true,
  'test.custom.aggregator.grouped': {
    aggregation: customAggregator,
    temporality: 'delta',
    groupBy: ['region'],
  },
});

// Before recording, should have no data
result = consumer2.collect();
assert.strictEqual(result.length, 0);

// Record values with different attributes
m2.record(10, { region: 'us-east' });
m2.record(20, { region: 'us-east' });
m2.record(30, { region: 'eu-west' });

result = consumer2.collect();
assert.strictEqual(result.length, 1);
assert.strictEqual(result[0].dataPoints.length, 2);

const usEast = result[0].dataPoints.find((dp) => dp.attributes.region === 'us-east');
const euWest = result[0].dataPoints.find((dp) => dp.attributes.region === 'eu-west');

assert.strictEqual(usEast.total, 30);
assert.strictEqual(usEast.itemCount, 2);
assert.strictEqual(euWest.total, 30);
assert.strictEqual(euWest.itemCount, 1);

// After delta reset, should have no data
result = consumer2.collect();
assert.strictEqual(result.length, 0);

consumer2.close();
