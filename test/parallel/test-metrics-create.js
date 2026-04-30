'use strict';

require('../common');

const assert = require('assert');
const { metrics } = require('perf_hooks');

// Test basic metric creation
const m1 = metrics.create('test.create');
assert.strictEqual(m1.descriptor.name, 'test.create');
assert.strictEqual(m1.isObservable, false);

// Test with all options
const m2 = metrics.create('test.full', {
  unit: 'ms',
  description: 'Full options test',
});
assert.strictEqual(m2.descriptor.name, 'test.full');
assert.strictEqual(m2.descriptor.unit, 'ms');
assert.strictEqual(m2.descriptor.description, 'Full options test');

// Test metrics.list()
const list = metrics.list();
assert.ok(Array.isArray(list));
assert.ok(list.length >= 2);
assert.ok(list.some((m) => m.descriptor.name === 'test.create'));
assert.ok(list.some((m) => m.descriptor.name === 'test.full'));

// Test metrics.get()
const retrieved = metrics.get('test.create');
assert.strictEqual(retrieved, m1);
assert.strictEqual(metrics.get('nonexistent'), undefined);

// Test validation
assert.throws(() => metrics.create(), {
  code: 'ERR_INVALID_ARG_TYPE',
});
assert.throws(() => metrics.create(123), {
  code: 'ERR_INVALID_ARG_TYPE',
});
