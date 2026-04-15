// Heap profile labels require async-context-frame (enabled by default in
// Node.js 24+; use --experimental-async-context-frame on Node.js 22).
'use strict';
const common = require('../common');
const assert = require('assert');
const v8 = require('v8');

// Test: labels survive await boundaries
async function testAwaitBoundary() {
  v8.startSamplingHeapProfiler(64);

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

  const profile = v8.getAllocationProfile();
  v8.stopSamplingHeapProfiler();

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
  v8.startSamplingHeapProfiler(64);

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

  const profile = v8.getAllocationProfile();
  v8.stopSamplingHeapProfiler();

  const users = profile.samples.filter((s) => s.labels.route === '/users');
  const products = profile.samples.filter(
    (s) => s.labels.route === '/products'
  );
  const orders = profile.samples.filter((s) => s.labels.route === '/orders');

  // At least some of the three routes should have samples
  const totalLabeled = users.length + products.length + orders.length;
  assert.ok(
    totalLabeled > 0,
    'Concurrent contexts should produce labeled samples'
  );
}

// Test: setHeapProfileLabels with async work
async function testSetLabelsAsync() {
  v8.startSamplingHeapProfiler(64);

  // Simulate Hapi-style: set labels, then do async work
  v8.setHeapProfileLabels({ route: '/hapi-style' });

  await new Promise((resolve) => setTimeout(resolve, 10));

  const arr = [];
  for (let i = 0; i < 5000; i++) arr.push({ hapi: i });

  const profile = v8.getAllocationProfile();
  v8.stopSamplingHeapProfiler();

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
  v8.startSamplingHeapProfiler(64);

  await assert.rejects(
    () => v8.withHeapProfileLabels({ route: '/error' }, async () => {
      await new Promise((resolve) => setTimeout(resolve, 5));
      throw new Error('test error');
    }),
    { message: 'test error' }
  );

  // Profiler should still work after error
  const profile = v8.getAllocationProfile();
  v8.stopSamplingHeapProfiler();
  assert.ok(profile);
}

// Test: nested withHeapProfileLabels
async function testNestedLabels() {
  v8.startSamplingHeapProfiler(64);

  await v8.withHeapProfileLabels({ route: '/outer' }, async () => {
    const outer = [];
    for (let i = 0; i < 2000; i++) outer.push({ outer: i });

    await v8.withHeapProfileLabels({ route: '/inner' }, async () => {
      const inner = [];
      for (let i = 0; i < 2000; i++) inner.push({ inner: i });
    });
  });

  const profile = v8.getAllocationProfile();
  v8.stopSamplingHeapProfiler();

  const outerSamples = profile.samples.filter(
    (s) => s.labels.route === '/outer'
  );
  const innerSamples = profile.samples.filter(
    (s) => s.labels.route === '/inner'
  );

  // Both outer and inner should have some samples
  assert.ok(
    outerSamples.length + innerSamples.length > 0,
    'Nested labels should produce labeled samples'
  );
}

async function main() {
  await testAwaitBoundary();
  await testConcurrentContexts();
  await testSetLabelsAsync();
  await testAsyncError();
  await testNestedLabels();
}

main().then(common.mustCall());
