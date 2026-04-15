// Flags: --expose-gc
'use strict';

require('../common');
const assert = require('assert');
const v8 = require('v8');

// Helper: find an externalBytes entry whose labels match a predicate.
function findExternal(profile, predicate) {
  if (!Array.isArray(profile.externalBytes)) return undefined;
  return profile.externalBytes.find(predicate);
}

// Helper: find an externalBytes entry by a single label key-value pair.
function findByLabel(profile, key, value) {
  return findExternal(profile, (e) => e.labels[key] === value);
}

// Test 1: Buffer.alloc() inside withHeapProfileLabels is attributed to the
// correct label in externalBytes.
{
  v8.startSamplingHeapProfiler(512 * 1024);  // 512KB interval (default)

  // Allocate 10MB Buffer inside a labeled context.
  const buf = v8.withHeapProfileLabels({ route: '/heavy' }, () => {
    const b = Buffer.alloc(10 * 1024 * 1024);
    // Keep buf alive.
    assert.strictEqual(b.length, 10 * 1024 * 1024);
    return b;
  });

  const profile = v8.getAllocationProfile();
  assert.ok(profile, 'profile should exist');

  assert.ok(Array.isArray(profile.externalBytes),
    'externalBytes should be an array (ProfilingArrayBufferAllocator active)');
  {
    const entry = findByLabel(profile, 'route', '/heavy');
    assert.ok(entry, 'Expected entry for route=/heavy in externalBytes');
    assert.ok(entry.bytes > 0,
      `Expected /heavy external bytes > 0, got ${entry.bytes}`);
    // The 10MB Buffer should show up (allow some tolerance for overhead).
    assert.ok(entry.bytes >= 9 * 1024 * 1024,
      `Expected /heavy >= 9MB, got ${entry.bytes}`);
  }

  // Keep buf alive until after profile is read.
  assert.ok(buf.length > 0);
  v8.stopSamplingHeapProfiler();
}

// Test 2: Buffer.alloc() outside any label context is not tracked.
{
  v8.startSamplingHeapProfiler(512 * 1024);

  // Allocate outside any label context.
  const buf = Buffer.alloc(5 * 1024 * 1024);
  assert.strictEqual(buf.length, 5 * 1024 * 1024);

  const profile = v8.getAllocationProfile();
  assert.ok(profile, 'profile should exist');

  // externalBytes should be empty or have zero bytes (no labeled allocations).
  if (Array.isArray(profile.externalBytes)) {
    const totalLabeled = profile.externalBytes
      .reduce((a, e) => a + e.bytes, 0);
    assert.strictEqual(totalLabeled, 0,
      `Expected 0 labeled external bytes, got ${totalLabeled}`);
  }

  v8.stopSamplingHeapProfiler();
}

// Test 3: After dropping Buffer references and forcing GC, per-label bytes
// decrease (Free is called).
{
  v8.startSamplingHeapProfiler(512 * 1024);

  let profile;

  v8.withHeapProfileLabels({ route: '/gc-test' }, () => {
    // Create a Buffer, then let it be GC'd.
    let buf = Buffer.alloc(8 * 1024 * 1024);
    assert.strictEqual(buf.length, 8 * 1024 * 1024);

    profile = v8.getAllocationProfile();
    assert.ok(Array.isArray(profile.externalBytes),
    'externalBytes should be an array (ProfilingArrayBufferAllocator active)');
  {
      const entry = findByLabel(profile, 'route', '/gc-test');
      if (entry) {
        assert.ok(entry.bytes >= 7 * 1024 * 1024,
          `Expected /gc-test >= 7MB before GC, got ${entry.bytes}`);
      }
    }

    // Drop reference and force GC.
    buf = null;
  });

  global.gc();
  global.gc();

  profile = v8.getAllocationProfile();
  // After GC, externalBytes may be absent if all labeled allocations were freed.
  {
    const entry = Array.isArray(profile.externalBytes)
      ? findByLabel(profile, 'route', '/gc-test') : undefined;
    const afterGC = entry ? entry.bytes : 0;
    // After GC, the buffer should be freed and the count should decrease.
    // It may not go to exactly 0 due to other small allocations.
    assert.ok(afterGC < 8 * 1024 * 1024,
      `Expected /gc-test < 8MB after GC, got ${afterGC}`);
  }

  v8.stopSamplingHeapProfiler();
}

// Test 4: Multiple labels — allocate Buffers with different labels, verify
// externalBytes shows correct per-label totals.
{
  v8.startSamplingHeapProfiler(512 * 1024);

  const bufs = [];
  v8.withHeapProfileLabels({ route: '/api/users' }, () => {
    bufs.push(Buffer.alloc(4 * 1024 * 1024));
  });

  v8.withHeapProfileLabels({ route: '/api/orders' }, () => {
    bufs.push(Buffer.alloc(6 * 1024 * 1024));
  });

  const profile = v8.getAllocationProfile();
  assert.ok(profile, 'profile should exist');

  assert.ok(Array.isArray(profile.externalBytes),
    'externalBytes should be an array (ProfilingArrayBufferAllocator active)');
  {
    const usersEntry = findByLabel(profile, 'route', '/api/users');
    const ordersEntry = findByLabel(profile, 'route', '/api/orders');
    const usersBytes = usersEntry ? usersEntry.bytes : 0;
    const ordersBytes = ordersEntry ? ordersEntry.bytes : 0;
    assert.ok(usersBytes >= 3 * 1024 * 1024,
      `Expected /api/users >= 3MB, got ${usersBytes}`);
    assert.ok(ordersBytes >= 5 * 1024 * 1024,
      `Expected /api/orders >= 5MB, got ${ordersBytes}`);
    // Orders should have more external memory than users.
    assert.ok(ordersBytes > usersBytes,
      `Expected /api/orders (${ordersBytes}) > /api/users (${usersBytes})`);
  }

  // Keep bufs alive.
  assert.ok(bufs.length === 2);
  v8.stopSamplingHeapProfiler();
}

// Test 5: JSON serialization of the profile includes externalBytes.
{
  v8.startSamplingHeapProfiler(512 * 1024);

  const buf = v8.withHeapProfileLabels({ route: '/json-test' }, () => {
    return Buffer.alloc(2 * 1024 * 1024);
  });

  const profile = v8.getAllocationProfile();
  const json = JSON.stringify(profile);
  const parsed = JSON.parse(json);

  assert.ok(Array.isArray(parsed.samples), 'samples should be an array');
  if (Array.isArray(parsed.externalBytes)) {
    const entry = parsed.externalBytes.find(
      (e) => e.labels && e.labels.route === '/json-test'
    );
    assert.ok(entry, 'Expected /json-test in serialized externalBytes');
    assert.ok(entry.bytes > 0,
      `Expected /json-test bytes > 0, got ${entry.bytes}`);
  }

  // Keep buf alive.
  assert.ok(buf.length > 0);
  v8.stopSamplingHeapProfiler();
}

// Test 6: Multi-label context — both key-value pairs appear in externalBytes.
{
  v8.startSamplingHeapProfiler(512 * 1024);

  const buf = v8.withHeapProfileLabels(
    { route: '/foo', handler: 'bar' }, () => {
      return Buffer.alloc(3 * 1024 * 1024);
    });

  const profile = v8.getAllocationProfile();
  assert.ok(profile, 'profile should exist');

  assert.ok(Array.isArray(profile.externalBytes),
    'externalBytes should be an array (ProfilingArrayBufferAllocator active)');
  {
    const entry = findExternal(profile,
      (e) => e.labels.route === '/foo' && e.labels.handler === 'bar');
    assert.ok(entry,
      'Expected entry with both route=/foo and handler=bar');
    assert.ok(entry.bytes >= 2 * 1024 * 1024,
      `Expected multi-label entry >= 2MB, got ${entry.bytes}`);
    // Verify both keys are present.
    assert.strictEqual(entry.labels.route, '/foo');
    assert.strictEqual(entry.labels.handler, 'bar');
  }

  // Keep buf alive.
  assert.ok(buf.length > 0);
  v8.stopSamplingHeapProfiler();
}

// Test 7: externalBytes labels match heap sample labels for same context.
{
  v8.startSamplingHeapProfiler(64);  // Small interval to increase sample hits

  const buf = v8.withHeapProfileLabels({ route: '/match-test' }, () => {
    // Allocate both heap objects and a Buffer in the same label context.
    const arr = [];
    for (let i = 0; i < 1000; i++) {
      arr.push({ data: new Array(100).fill(i) });
    }
    const b = Buffer.alloc(5 * 1024 * 1024);
    // Keep arr alive.
    assert.ok(arr.length > 0);
    return b;
  });

  const profile = v8.getAllocationProfile();

  // Find heap samples with matching labels.
  const labeledSamples = profile.samples.filter(
    (s) => s.labels && s.labels.route === '/match-test'
  );

  // Find externalBytes entry with matching labels.
  assert.ok(Array.isArray(profile.externalBytes),
    'externalBytes should be an array (ProfilingArrayBufferAllocator active)');
  {
    const extEntry = findByLabel(profile, 'route', '/match-test');
    if (extEntry && labeledSamples.length > 0) {
      // The label keys should match between heap samples and externalBytes.
      const sampleLabelKeys = Object.keys(labeledSamples[0].labels).sort();
      const extLabelKeys = Object.keys(extEntry.labels).sort();
      assert.deepStrictEqual(extLabelKeys, sampleLabelKeys,
        'externalBytes label keys should match heap sample label keys');
    }
  }

  // Keep buf alive.
  assert.ok(buf.length > 0);
  v8.stopSamplingHeapProfiler();
}

console.log('All external memory tracking tests passed.');
