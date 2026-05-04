'use strict';
require('../common');
const assert = require('node:assert');
const { metrics } = require('node:perf_hooks');

// This test ensures that the singleton pattern works correctly for metrics.
// Note: Warning emission for mismatched options is verified separately in manual testing.

{
  // Test: Creating metric twice with same name returns same instance
  const metric1 = metrics.create('test.singleton.basic', {
    description: 'Test singleton',
    unit: 'count',
  });

  const metric2 = metrics.create('test.singleton.basic', {
    description: 'Test singleton',
    unit: 'count',
  });

  assert.strictEqual(metric1, metric2);
  metric1.close();
}

{
  // Test: No warning when options match
  const metric1 = metrics.create('test.singleton.no.warning', {
    description: 'Same options',
    unit: 'count',
  });

  const metric2 = metrics.create('test.singleton.no.warning', {
    description: 'Same options',
    unit: 'count',
  });

  // Should be same instance
  assert.strictEqual(metric1, metric2);
  metric1.close();
}

{
  // Test: Creating metric twice with different unit returns same instance (with warning)
  const metric1 = metrics.create('test.singleton.unit', {
    description: 'Test unit',
    unit: 'count',
  });

  const metric2 = metrics.create('test.singleton.unit', {
    description: 'Test unit',
    unit: 'bytes', // Different unit
  });

  // Still returns same instance despite different unit
  assert.strictEqual(metric1, metric2);
  // Original unit is preserved
  assert.strictEqual(metric1.descriptor.unit, 'count');
  metric1.close();
}

{
  // Test: Creating metric twice with different description returns same instance
  const metric1 = metrics.create('test.singleton.description', {
    description: 'First description',
    unit: 'count',
  });

  const metric2 = metrics.create('test.singleton.description', {
    description: 'Second description', // Different
    unit: 'count',
  });

  assert.strictEqual(metric1, metric2);
  // Original description is preserved
  assert.strictEqual(metric1.descriptor.description, 'First description');
  metric1.close();
}

{
  // Test: Creating metric twice with different observable status returns same instance
  const metric1 = metrics.create('test.singleton.observable', {
    description: 'Test observable',
  });

  const metric2 = metrics.create('test.singleton.observable', {
    description: 'Test observable',
    observable: (m) => { m.record(100); },
  });

  assert.strictEqual(metric1, metric2);
  // Original (non-observable) status is preserved
  assert.strictEqual(metric1.isObservable, false);
  metric1.close();
}

{
  // Test: Multiple option mismatches still return same instance
  const metric1 = metrics.create('test.singleton.multiple', {
    description: 'First',
    unit: 'count',
  });

  const metric2 = metrics.create('test.singleton.multiple', {
    description: 'Second',
    unit: 'bytes',
  });

  assert.strictEqual(metric1, metric2);
  // Original options are preserved
  assert.strictEqual(metric1.descriptor.description, 'First');
  assert.strictEqual(metric1.descriptor.unit, 'count');
  metric1.close();
}

{
  // Test: Singleton works after metric is closed and recreated
  const metric1 = metrics.create('test.singleton.recreate', {
    description: 'Original',
  });

  metric1.close();

  // Creating new metric with same name after close creates new instance
  const metric2 = metrics.create('test.singleton.recreate', {
    description: 'New',
  });

  assert.notStrictEqual(metric1, metric2);
  assert.strictEqual(metric2.descriptor.description, 'New');
  metric2.close();
}
