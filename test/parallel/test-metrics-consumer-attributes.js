'use strict';

require('../common');

const assert = require('assert');
const { metrics } = require('perf_hooks');

// Test groupBy attribute filtering
const m = metrics.create('test.attrs.groupby');

const consumer = metrics.createConsumer({
  'groupByAttributes': true,
  'test.attrs.groupby': {
    aggregation: 'sum',
    groupBy: ['method'],
  },
});

m.record(1, { method: 'GET', path: '/api', user: 'alice' });
m.record(2, { method: 'GET', path: '/users', user: 'bob' });
m.record(3, { method: 'POST', path: '/api', user: 'alice' });

const result = consumer.collect();
assert.strictEqual(result[0].dataPoints.length, 2);

const getData = result[0].dataPoints.find((dp) => dp.attributes.method === 'GET');
const postData = result[0].dataPoints.find((dp) => dp.attributes.method === 'POST');

// Only 'method' should be in attributes (groupBy filters)
assert.deepStrictEqual(getData.attributes, { method: 'GET' });
assert.deepStrictEqual(postData.attributes, { method: 'POST' });
assert.strictEqual(getData.sum, 3); // 1 + 2
assert.strictEqual(postData.sum, 3);

consumer.close();

// Test custom attributeKey function
const m2 = metrics.create('test.attrs.custom');

const consumer2 = metrics.createConsumer({
  'groupByAttributes': true,
  'test.attrs.custom': {
    aggregation: 'sum',
    attributeKey: (attrs) => attrs.region || 'unknown',
  },
});

m2.record(1, { region: 'us-east', zone: 'a' });
m2.record(2, { region: 'us-east', zone: 'b' });
m2.record(3, { region: 'eu-west', zone: 'a' });

const result2 = consumer2.collect();
assert.strictEqual(result2[0].dataPoints.length, 2);

consumer2.close();

// Test normalizeAttributes function
const m3 = metrics.create('test.attrs.normalize');

const consumer3 = metrics.createConsumer({
  'groupByAttributes': true,
  'test.attrs.normalize': {
    aggregation: 'sum',
    normalizeAttributes: (attrs) => ({
      method: attrs.method?.toUpperCase(),
    }),
  },
});

m3.record(1, { method: 'get', path: '/api' });
m3.record(2, { method: 'GET', path: '/users' });

const result3 = consumer3.collect();
// Both should be grouped under 'GET'
const dp = result3[0].dataPoints[0];
assert.strictEqual(dp.attributes.method, 'GET');
assert.strictEqual(dp.sum, 3);

consumer3.close();
