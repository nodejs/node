// Heap profile labels require async-context-frame (on by default).
'use strict';
const common = require('../common');
const assert = require('assert');
const v8 = require('v8');

// Test: labels survive await boundaries
async function testAwaitBoundary() {
  const handle = v8.startHeapProfile({ sampleInterval: 64, labels: true });

  await v8.withHeapProfileLabels({ route: '/async' }, async () => {
    // Allocate before await
    const before = [];
    for (let i = 0; i < 2000; i++) before.push({ pre: i });

    // Yield to event loop
    await new Promise((resolve) => setTimeout(resolve, 10));

    // Allocate after await — labels should still be active
    const after = [];
    for (let i = 0; i < 2000; i++) after.push({ post: i });
  });

  const profile = handle.getAllocationProfile();
  handle.stop();

  const labeled = profile.samples.filter(
    (s) => s.labels.route === '/async'
  );
  assert.ok(
    labeled.length > 0,
    'Labels should survive await boundaries'
  );
}

// Test: concurrent async contexts with different labels
async function testConcurrentContexts() {
  const handle = v8.startHeapProfile({ sampleInterval: 64, labels: true });

  const task = async (route, count) => {
    await v8.withHeapProfileLabels({ route }, async () => {
      await new Promise((resolve) => setTimeout(resolve, 5));
      const arr = [];
      for (let i = 0; i < count; i++) arr.push({ data: i, route });
    });
  };

  // Run multiple concurrent labeled tasks
  await Promise.all([
    task('/users', 5000),
    task('/products', 5000),
    task('/orders', 5000),
  ]);

  const profile = handle.getAllocationProfile();
  handle.stop();

  // Attribution-correctness: every sample that carries a route label must
  // carry one of the expected routes. A label-bleed bug would produce samples
  // with an unexpected route value.
  const expectedRoutes = new Set(['/users', '/products', '/orders']);
  const allLabeledSamples = profile.samples.filter(
    (s) => s.labels.route !== undefined
  );
  for (const sample of allLabeledSamples) {
    assert.ok(expectedRoutes.has(sample.labels.route),
      `Sample carries unexpected route: ${sample.labels.route}`);
  }
  // Weaker existence check: 5000 × 3 tasks at 64-byte interval must yield
  // at least some labeled samples.
  assert.ok(
    allLabeledSamples.length > 0,
    'Concurrent contexts should produce labeled samples'
  );
}

// Test: setHeapProfileLabels with async work
async function testSetLabelsAsync() {
  const handle = v8.startHeapProfile({ sampleInterval: 64, labels: true });

  // Simulate Hapi-style: set labels, then do async work
  v8.setHeapProfileLabels({ route: '/hapi-style' });

  await new Promise((resolve) => setTimeout(resolve, 10));

  const arr = [];
  for (let i = 0; i < 5000; i++) arr.push({ hapi: i });

  const profile = handle.getAllocationProfile();
  handle.stop();

  const labeled = profile.samples.filter(
    (s) => s.labels.route === '/hapi-style'
  );
  assert.ok(
    labeled.length > 0,
    'setHeapProfileLabels should work with async code'
  );
}

// Test: withHeapProfileLabels handles async errors
async function testAsyncError() {
  const handle = v8.startHeapProfile({ sampleInterval: 64, labels: true });

  await assert.rejects(
    () => v8.withHeapProfileLabels({ route: '/error' }, async () => {
      await new Promise((resolve) => setTimeout(resolve, 5));
      throw new Error('test error');
    }),
    { message: 'test error' }
  );

  // Profiler should still work after error
  const profile = handle.getAllocationProfile();
  handle.stop();
  assert.ok(profile);
}

// Test: nested withHeapProfileLabels
async function testNestedLabels() {
  const handle = v8.startHeapProfile({ sampleInterval: 64, labels: true });

  await v8.withHeapProfileLabels({ route: '/outer' }, async () => {
    // No pre-inner allocations: outer-labeled samples will only come from
    // the post-inner block, so their presence directly verifies reversion.
    await v8.withHeapProfileLabels({ route: '/inner' }, async () => {
      const inner = [];
      for (let i = 0; i < 2000; i++) inner.push({ inner: i });
    });

    // After inner exits the label must revert to '/outer'. These allocations
    // verify that nesting contract.
    const outer = [];
    for (let i = 0; i < 2000; i++) outer.push({ outer: i });
  });

  const profile = handle.getAllocationProfile();
  handle.stop();

  const outerSamples = profile.samples.filter(
    (s) => s.labels.route === '/outer'
  );
  const innerSamples = profile.samples.filter(
    (s) => s.labels.route === '/inner'
  );

  // Inner samples must exist in their own right (not merged with outer).
  assert.ok(innerSamples.length > 0,
    'Inner context must produce its own labeled samples');
  // Outer samples come from the post-inner block, proving label reversion.
  assert.ok(outerSamples.length > 0,
    'Label must revert to outer after inner exits');
}

async function main() {
  await testAwaitBoundary();
  await testConcurrentContexts();
  await testSetLabelsAsync();
  await testAsyncError();
  await testNestedLabels();
}

main().then(common.mustCall());
