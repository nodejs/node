'use strict';

require('../common');

const assert = require('assert');
const { metrics } = require('perf_hooks');

// Test MetricDescriptor via metric creation
const m = metrics.create('test.descriptor', {
  unit: 'bytes',
  description: 'A test metric',
});

// Test descriptor getters
assert.strictEqual(m.descriptor.name, 'test.descriptor');
assert.strictEqual(m.descriptor.unit, 'bytes');
assert.strictEqual(m.descriptor.description, 'A test metric');
assert.strictEqual(m.descriptor.scope, undefined);

// Test toJSON
const json = m.descriptor.toJSON();
assert.deepStrictEqual(json, {
  name: 'test.descriptor',
  unit: 'bytes',
  description: 'A test metric',
  scope: undefined,
});

// Test with minimal options
const m2 = metrics.create('test.minimal');
assert.strictEqual(m2.descriptor.name, 'test.minimal');
assert.strictEqual(m2.descriptor.unit, undefined);
assert.strictEqual(m2.descriptor.description, undefined);

// Descriptor toJSON with undefined fields
const json2 = m2.descriptor.toJSON();
assert.deepStrictEqual(json2, {
  name: 'test.minimal',
  unit: undefined,
  description: undefined,
  scope: undefined,
});

// Test with InstrumentationScope
const scope = new metrics.InstrumentationScope('my-library', '1.0.0', 'https://example.com/schema');
const m3 = metrics.create('test.scoped', {
  unit: 'count',
  scope,
});

assert.strictEqual(m3.descriptor.scope, scope);
assert.strictEqual(m3.descriptor.scope.name, 'my-library');
assert.strictEqual(m3.descriptor.scope.version, '1.0.0');
assert.strictEqual(m3.descriptor.scope.schemaUrl, 'https://example.com/schema');

const json3 = m3.descriptor.toJSON();
assert.deepStrictEqual(json3.scope, {
  name: 'my-library',
  version: '1.0.0',
  schemaUrl: 'https://example.com/schema',
});
